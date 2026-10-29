#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

namespace payments {

/// Thread-safe blocking queue: producers push, consumers pop and block until
/// an element is available or the queue is closed and drained.
template <typename T>
class BlockingQueue {
 public:
  void push(T value) {
    {
      std::lock_guard lock(mutex_);
      items_.push_back(std::move(value));
    }
    ready_.notify_one();
  }

  /// Blocks while empty; nullopt once closed and drained.
  std::optional<T> pop() {
    std::unique_lock lock(mutex_);
    ready_.wait(lock, [this] { return !items_.empty() || closed_; });
    if (items_.empty()) return std::nullopt;
    T value = std::move(items_.front());
    items_.pop_front();
    return value;
  }

  /// Wake all consumers; drainable pop()s keep delivering, then nullopt.
  void close() {
    {
      std::lock_guard lock(mutex_);
      closed_ = true;
    }
    ready_.notify_all();
  }

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
