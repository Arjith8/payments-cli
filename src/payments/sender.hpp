#pragma once

#include "payments/queue.hpp"
#include "payments/reply.hpp"

#include <cstdint>
#include <optional>
#include <utility>

#include <atomic>

namespace payments {

/// Outbound queue: `post` mimics an external-service call while delivery
/// logic stays out of scope. Nothing leaves the service in this phase.
class MessageSender {
 public:
  using Queue = BlockingQueue<PaymentReply>;

  /// Blocks until delivered.
  void post(PaymentReply reply) {
    ++posted_;
    queue_.push(std::move(reply));
  }

  /// Drains by delivery workers; nullopt after close-and-drain.
  std::optional<PaymentReply> take() { return queue_.pop(); }

  void close() { queue_.close(); }
  std::size_t pending() const { return queue_.size(); }
  std::uint64_t posted_total() const { return posted_.load(); }
  bool is_closed() const { return queue_.closed(); }

 private:
  Queue queue_;
  std::atomic<std::uint64_t> posted_{0};
};

}  // namespace payments
