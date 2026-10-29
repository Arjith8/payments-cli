#include "payments/stream.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <thread>
#include <unordered_map>
#include <unordered_set>

TEST_CASE("produces well-formed, self-consistent requests") {
  payments::MockStream stream({.customer_count = 10,
                               .requests_per_second = 0,
                               .seed = 42});

  const auto request = stream.next();
  CHECK(request.request_id == "req-0");
  CHECK(request.currency == "INR");
  CHECK(request.payer != request.payee);
  CHECK(request.amount_cents >= 100);
  CHECK(request.amount_cents <= 50000);
  CHECK(stream.produced() == 1);
}

TEST_CASE("request ids are unique and increment") {
  payments::MockStream stream({.customer_count = 5,
                               .requests_per_second = 0,
                               .seed = 7});
  std::unordered_set<std::string> ids;
  for (int i = 0; i < 50; ++i) {
    ids.insert(stream.next().request_id);
  }
  CHECK(ids.size() == 50);
}

TEST_CASE("uses only configured customers") {
  payments::MockStream stream({.customer_count = 6,
                               .requests_per_second = 0,
                               .seed = 99});
  std::unordered_set<std::string> allowed;
  for (int i = 0; i < 6; ++i) allowed.insert("cust_" + std::to_string(i));

  std::unordered_set<std::string> seen;
  for (int i = 0; i < 60; ++i) {
    auto r = stream.next();
    CHECK(allowed.count(r.payer) == 1);
    CHECK(allowed.count(r.payee) == 1);
    seen.insert(r.payer);
    seen.insert(r.payee);
  }
  // With 60 draws over 6 customers we expect decent coverage; be lenient.
  CHECK(seen.size() >= 4);
}

TEST_CASE("method mix is honoured") {
  // 100% debit: every draw must be debit_card.
  payments::MockStream stream({.customer_count = 5,
                               .requests_per_second = 0,
                               .method_mix = {{1.0, payments::PaymentMethod::debit_card}},
                               .seed = 3});
  for (int i = 0; i < 20; ++i) {
    CHECK(stream.next().method == payments::PaymentMethod::debit_card);
  }
}

TEST_CASE("run publishes the exact count and closes the sink") {
  payments::MockStream stream({.customer_count = 8,
                               .requests_per_second = 0,
                               .seed = 11});
  payments::BlockingQueue<payments::PaymentRequest> queue;

  std::thread producer([&] { stream.run(queue, 25); });

  std::size_t drained = 0;
  while (auto request = queue.pop()) {
    ++drained;
  }
  producer.join();

  CHECK(drained == 25);
  CHECK(stream.produced() == 25);
}

TEST_CASE("async stream can be stopped cooperatively") {
  payments::MockStream stream({.customer_count = 8,
                               .requests_per_second = 0,
                               .seed = 13});
  payments::BlockingQueue<payments::PaymentRequest> queue;

  stream.start_async(queue);
  std::uint64_t before = 0;
  while (stream.produced() < 5) {
    std::this_thread::yield();
  }
  before = stream.produced();
  stream.stop();

  // After stop, produced() must stabilise.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  CHECK(stream.produced() <= before + 2);
  while (queue.pop())
    ;  // drain; queue never closes for an unbounded async stream
}

TEST_CASE("rejects degenerate configs") {
  CHECK_THROWS_AS(
      payments::MockStream(
          payments::StreamConfig{.customer_count = 1, .requests_per_second = 0, .seed = 1}),
      std::invalid_argument);

  CHECK_THROWS_AS(
      payments::MockStream(
          payments::StreamConfig{.customer_count = 5,
                                 .requests_per_second = 0,
                                 .method_mix = {},
                                 .seed = 1}),
      std::invalid_argument);
}

TEST_CASE("same seed replays identically (determinism)") {
  payments::MockStream a({.customer_count = 12, .requests_per_second = 0, .seed = 77});
  payments::MockStream b({.customer_count = 12, .requests_per_second = 0, .seed = 77});

  for (int i = 0; i < 100; ++i) {
    CHECK(a.next().amount_cents == b.next().amount_cents);
  }
}
