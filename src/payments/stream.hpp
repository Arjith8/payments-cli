#pragma once

#include "payments/queue.hpp"
#include "payments/types.hpp"

#include <random>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace payments {

namespace detail {
/// Small helper so MockStream's pools read cleanly in the constructor.
std::vector<std::string> make_customer_pool(std::size_t count,
                                            const std::string& prefix);
}  // namespace detail

/// Knobs for the synthetic traffic generator. Defaults imitate plausible
/// retail traffic: strong skew toward big customers plus a long tail.
struct StreamConfig {
  /// Distinct simulated customers; balances get attached to these ids later.
  std::size_t customer_count = 20;
  /// Requests produced per second (roughly; jittered so 0 == as fast as possible).
  double requests_per_second = 50.0;
  /// Amount range in minor units.
  std::int64_t min_amount_cents = 100;
  std::int64_t max_amount_cents = 50000;
  /// Weighted method mix; probabilities are used as relative weights.
  std::vector<std::pair<double, PaymentMethod>> method_mix = {
      {0.45, PaymentMethod::debit_card},
      {0.25, PaymentMethod::credit_card},
      {0.20, PaymentMethod::bank_transfer},
      {0.10, PaymentMethod::wallet},
  };
  /// Seed for deterministic replay in tests; random_device in production.
  std::uint64_t seed = std::random_device{}();
};

/// Anything that can feed payment requests into the pipeline. MockStream
/// implements this today; a real HTTP/Kafka ingestion source would too.
class IRequestSource {
 public:
  virtual ~IRequestSource() = default;

  /// Generate the next payment request. Purely synchronous — callers choose
  /// their own cadence (inline loop, detached thread, worker pool, ...).
  virtual PaymentRequest next() = 0;

  /// How many requests have been generated so far (mostly for stats).
  virtual std::uint64_t produced() const = 0;
};

/// Synthetic request generator.
///
/// `run()` loops `next()` at a jittered rate and publishes to the queue until
/// the limit is reached or `stop()` is called — the caller decides its thread
/// (inline for tests, a detached thread via `start_async()` for the app).
class MockStream final : public IRequestSource {
 public:
  explicit MockStream(StreamConfig config = {});

  PaymentRequest next() override;
  std::uint64_t produced() const override;

  /// Publish exactly `count` requests to `sink`, then closed if `close_sink`.
  void run(BlockingQueue<PaymentRequest>& sink, std::uint64_t count,
           bool close_sink = true);
  /// Publish until stopped or closed; runs in its own detached thread.
  void start_async(BlockingQueue<PaymentRequest>& sink,
                   std::uint64_t rate_limit = 0);

  /// Request the async loop finish. Cooperative; returns immediately.
  void stop();

 private:
  PaymentMethod next_method();
  std::int64_t next_amount();

  StreamConfig config_;
  std::mt19937 rng_;
  std::discrete_distribution<std::size_t> method_distribution_;
  std::vector<PaymentMethod> method_values_;

  std::vector<std::string> customers_;
  std::uniform_int_distribution<std::size_t> payer_pick_;
  std::atomic<std::uint64_t> produced_{0};
  std::atomic<bool> stopping_{false};
};

}  // namespace payments
