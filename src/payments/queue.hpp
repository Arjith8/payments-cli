#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

namespace payments {

/// A minimal thread-safe blocking queue.
///
/// Producers push; consumers pop and block until either an element is
/// available or the producer has closed the queue. This is the spine of the
/// mock stream: the generator produces on one thread, workers consume on
/// theirs. Kept generic so the same queue later carries confirmation messages
/// to the message sender as well.
template <typename T>
class BlockingQueue {
 public:
  /// Enqueue one element. O(1).
  void push(T value) {
    {
      std::lock_guard lock(mutex_);
      items_.push_back(std::move(value));
    }
    ready_.notify_one();
  }

  /// Dequeue one element, blocking while the queue is empty.
  /// Returns std::nullopt once the queue is closed and drained.
  std::optional<T> pop() {
    std::unique_lock lock(mutex_);
    ready_.wait(lock, [this] { return !items_.empty() || closed_; });
    if (items_.empty()) return std::nullopt;
    T value = std::move(items_.front());
    items_.pop_front();
    return value;
  }

  /// Close the queue: wake all waiting consumers and make pop() return
  /// nullopt after draining whatever is left. Idempotent.
  void close() {
    {
      std::lock_guard lock(mutex_);
      closed_ = true;
    }
    ready_.notify_all();
  }

  /// Non-blocking snapshot count, mostly for tests and stats.
  std::size_t size() const {
    std::lock_guard lock(mutex_);
    return items_.size();
  }

  bool closed() const {
    std::lock_guard lock(mutex_);
    return closed_;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<T> items_;
  bool closed_ = false;
};

}  // namespace payments
