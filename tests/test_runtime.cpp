#include "payments/runtime.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <thread>

TEST_CASE("submit settles interactively and counters track") {
  payments::Runtime runtime(payments::make_default_db(4));

  auto reply = runtime.submit({"m-1", 1000, "INR", "cust_0", "cust_1",
                               payments::PaymentMethod::debit_card});
  CHECK(reply.status == payments::TransactionStatus::approved);

  auto stats = runtime.stats();
  CHECK(stats.processed == 1);
  CHECK(stats.approved == 1);
  CHECK(stats.failed == 0);
  CHECK(runtime.store().find("cust_0")->balance_cents == 1'000'000 - 1000);

  runtime.stop();  // deliverer drains the queued reply on close
  CHECK(runtime.pending_replies() == 0);
}

TEST_CASE("synthetic traffic flows through workers into stats") {
  payments::Runtime runtime(
      payments::make_default_db(8),
      payments::RuntimeOptions{.worker_count = 2, .synthetic_rate = 0});
  runtime.start();

  // flood: a huge rate works as an unthrottled source
  runtime.restart_synthetic(1'000'000);
  while (runtime.stats().processed < 50) {
    std::this_thread::yield();
  }
  runtime.stop();

  const auto stats = runtime.stats();
  CHECK(stats.processed >= 50);
  CHECK(stats.processed == stats.approved + stats.rejected + stats.failed);

  // sender drained to completion by the deliverer thread
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  CHECK(runtime.pending_replies() == 0);
}

TEST_CASE("zero-rate start produces no synthetic traffic") {
  std::uint64_t stream_seed = 5;
  payments::RuntimeOptions options{.worker_count = 1,
                                   .synthetic_rate = 0,
                                   .stream_seed = stream_seed};
  payments::Runtime runtime(payments::make_default_db(4), options);
  runtime.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  const auto stats = runtime.stats();
  CHECK(stats.processed == 0);
  runtime.stop();
}

TEST_CASE("restart_synthetic turns the stream on and off") {
  payments::Runtime runtime(
      payments::make_default_db(6),
      payments::RuntimeOptions{.worker_count = 1, .synthetic_rate = 0});
  runtime.start();

  runtime.restart_synthetic(1'000'000);
  while (runtime.stats().processed < 5) {
    std::this_thread::yield();
  }
  runtime.restart_synthetic(0);  // off

  std::uint64_t at_off = runtime.stats().processed;
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  CHECK(runtime.stats().processed <= at_off + 2);
  runtime.stop();
}
