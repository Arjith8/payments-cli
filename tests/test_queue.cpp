#include "payments/queue.hpp"

#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

TEST_CASE("pop returns pushed items in fifo order") {
  payments::BlockingQueue<int> queue;
  queue.push(1);
  queue.push(2);

  auto first = queue.pop();
  auto second = queue.pop();
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  CHECK(*first == 1);
  CHECK(*second == 2);
}

TEST_CASE("pop blocks until an item is available") {
  payments::BlockingQueue<int> queue;
  std::atomic popped{false};

  std::thread consumer([&] {
    auto value = queue.pop();
    popped = value.has_value() && *value == 9;
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));  // let it block
  CHECK_FALSE(popped.load());

  queue.push(9);
  consumer.join();
  CHECK(popped.load());
}

TEST_CASE("pop after close-and-drain yields nullopt, waking all consumers") {
  payments::BlockingQueue<int> queue;
  queue.push(5);

  int woke_with_value = 0;
  std::thread a([&] { if (queue.pop()) ++woke_with_value; });
  std::thread b([&] { if (queue.pop()) ++woke_with_value; });

  queue.close();
  a.join();
  b.join();
  CHECK(woke_with_value == 1);  // exactly one consumer got the item
  CHECK_FALSE(queue.pop().has_value());
}
