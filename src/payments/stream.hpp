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

/// Knobs for synthetic traffic generation.
struct StreamConfig {
  std::size_t customer_count = 20;
  double requests_per_second = 50.0;
  std::int64_t min_amount_cents = 100;
  std::int64_t max_amount_cents = 50000;
  std::vector<std::pair<double, PaymentMethod>> method_mix = {
      {0.45, PaymentMethod::debit_card},
      {0.25, PaymentMethod::credit_card},
      {0.20, PaymentMethod::bank_transfer},
      {0.10, PaymentMethod::wallet},
  };
  /// Fixed seed gives deterministic replay; random_device in production.
  std::uint64_t seed = std::random_device{}();
};

/// Sources of payment requests. `MockStream` implements this; real
/// HTTP/Kafka ingestion would too.
class IRequestSource {
 public:
  virtual ~IRequestSource() = default;

  /// Generate the next request; cadence is the caller's choice.
  virtual PaymentRequest next() = 0;

  virtual std::uint64_t produced() const = 0;
};

/// Generates synthetic payment requests at a configurable, jittered rate.
class MockStream final : public IRequestSource {
 public:
  explicit MockStream(StreamConfig config = {});

  PaymentRequest next() override;
  std::uint64_t produced() const override;

  /// Publish `count` requests, then close the sink.
  void run(BlockingQueue<PaymentRequest>& sink, std::uint64_t count,
           bool close_sink = true);
  /// Publish indefinitely in a detached thread until stop().
  void start_async(BlockingQueue<PaymentRequest>& sink,
                   std::uint64_t rate_limit = 0);

  void stop();

 private:
  PaymentMethod next_method();
  std::int64_t next_amount();

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
