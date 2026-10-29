#pragma once

#include "payments/banks.hpp"
#include "payments/processor.hpp"
#include "payments/sender.hpp"
#include "payments/sharded.hpp"
#include "payments/stream.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <random>
#include <thread>
#include <vector>

#include <optional>

namespace payments {

/// Live counters for monitoring and later benchmark output.
struct RuntimeStats {
  std::uint64_t processed = 0;
  std::uint64_t approved = 0;
  std::uint64_t rejected = 0;
  std::uint64_t failed = 0;
};

struct RuntimeOptions {
  /// Request-processing workers; 1 until the threading phase.
  std::size_t worker_count = 1;
  /// Synthetic traffic rate (0 = no synthetic traffic at all).
  std::uint64_t synthetic_rate = 0;
  /// Deterministic stream seed.
  std::uint64_t stream_seed = std::random_device{}();
};

/// Assembles the whole pipeline and owns its threads.
///
///     MockStream ──▶ inbound_ ──▶ worker(s) ──▶ PaymentProcessor
///                                 (AccountStore, partners)  │
///                                                 replies   ▼
///                                           deliverer ◀── MessageSender
///
/// `submit()` bypasses the inbound queue for synchronous, foreground
/// requests (the CLI user's own payments).
class Runtime {
 public:
  explicit Runtime(std::unique_ptr<AccountStore> store,
                   RuntimeOptions options = {});
  ~Runtime();

  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

  /// Spawns the stream source, request workers, and reply deliverer.
  void start();

  /// Runs one foreground request and returns its reply.
  PaymentReply submit(PaymentRequest request);

  /// Stops synthetic traffic, drains, joins threads. Idempotent.
  void stop();

  RuntimeStats stats() const;
  const AccountStore& store() const { return *store_; }
  std::size_t pending_replies() const { return sender_.pending(); }
  /// Set synthetic rate live; 0 stops the stream.
  void restart_synthetic(std::uint64_t rate);

 private:
  void worker_loop();
  void deliver_loop();
  void account(const PaymentReply& reply);

  std::unique_ptr<AccountStore> store_;
  RuntimeOptions options_;
  SintaBank sinta_;
  RoRy rory_;
  PartnerRouter router_;
  MessageSender sender_;
  PaymentProcessor processor_;
  MockStream stream_;
  BlockingQueue<PaymentRequest> inbound_;

  std::vector<std::jthread> workers_;
  std::optional<std::jthread> deliverer_;
  std::atomic<bool> running_{false};
  std::atomic<std::uint64_t> processed_{0};
  std::atomic<std::uint64_t> approved_{0};
  std::atomic<std::uint64_t> rejected_{0};
  std::atomic<std::uint64_t> failed_{0};
};

}  // namespace payments
