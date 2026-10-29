#include "payments/sharded.hpp"
#include "payments/processor.hpp"
#include "payments/runtime.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <thread>
#include <unordered_set>

namespace {

std::vector<payments::Customer> demo_customers(std::size_t n,
                                               std::int64_t balance) {
  std::vector<payments::Customer> out;
  out.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    out.push_back({"cust_" + std::to_string(i), "C" + std::to_string(i),
                   balance});
  }
  return out;
}

}  // namespace

TEST_CASE("sharded store routes every id to one partition") {
  payments::ShardedDB db(demo_customers(40, 1000), 4);
  CHECK(db.size() == 40);
  CHECK(db.shard_count() == 4);
  CHECK(db.has_balance("cust_3", 1000));
  CHECK(db.has_balance("cust_39", 1000));
  CHECK_FALSE(db.has_balance("ghost", 1));
}

TEST_CASE("stable routing: same id always hits the same shard") {
  const auto assignment =
      payments::ShardedDB::shard_for("cust_7", 4);
  for (int i = 0; i < 10; ++i) {
    CHECK(assignment == payments::ShardedDB::shard_for("cust_7", 4));
  }
}

TEST_CASE("dequeues balances correctly through the partition boundary") {
  payments::ShardedDB db(demo_customers(20, 1000), 3);

  // Same-amount pairings: credit in one shard, debit in another.
  db.try_debit("cust_2", 700);
  db.credit("cust_2", 700);
  CHECK(db.find("cust_2")->balance_cents == 1000);

  CHECK(db.try_debit("cust_9", 1001) == false);
  CHECK(db.try_debit("cust_9", 500) == true);
  CHECK(db.find("cust_9")->balance_cents == 500);

  // This case exercises a partition whose permutation differs from cust_9's.
  for (std::size_t i = 0; i < 20; ++i) {
    CHECK_FALSE(db.has_balance("cust_" + std::to_string(i), 1001));
    CHECK(db.has_balance("cust_" + std::to_string(i), 500));
  }
}

TEST_CASE("ids() reassembles the full registry sorted") {
  payments::ShardedDB db(demo_customers(30, 1), 5);
  const auto ids = db.ids();
  REQUIRE(ids.size() == 30);
  CHECK(std::is_sorted(ids.begin(), ids.end()));
  std::unordered_set<std::string> unique(ids.begin(), ids.end());
  CHECK(unique.size() == 30);
}

TEST_CASE("duplicate ids across shards are rejected") {
  auto duplicates = demo_customers(10, 100);
  duplicates.push_back({"cust_0", "double", 100});
  CHECK_THROWS_AS(payments::ShardedDB(std::move(duplicates), 3),
                  std::invalid_argument);

  auto negative = demo_customers(2, 100);
  negative[1].balance_cents = -5;
  CHECK_THROWS_AS(payments::ShardedDB(std::move(negative), 3),
                  std::invalid_argument);

  CHECK_THROWS_AS(payments::ShardedDB(demo_customers(5, 1), 0),
                  std::invalid_argument);
}

TEST_CASE("processor drives a sharded store end to end") {
  payments::ShardedDB db(demo_customers(6, 100'000), 2);
  payments::SintaBank sinta{10};
  payments::RoRy rory{10};
  payments::PartnerRouter router{rory, sinta};
  payments::MessageSender sender;
  payments::PaymentProcessor processor{db, router, sender};

  auto reply = processor.process({"req-1", 1000, "INR", "cust_0", "cust_5",
                                  payments::PaymentMethod::debit_card});
  CHECK(reply.status == payments::TransactionStatus::approved);
  CHECK(db.find("cust_0")->balance_cents == 99'000);
  CHECK(db.find("cust_5")->balance_cents == 101'000);

  sender.close();
  std::size_t replies = 0;
  while (sender.take()) ++replies;
  CHECK(replies == 1);
}

TEST_CASE("concurrent cross-shard debits all settle exactly") {
  payments::ShardedDB db(demo_customers(8, 100'000), 4);
  const std::int64_t amount = 100;

  std::vector<std::thread> workers;
  for (int w = 0; w < 4; ++w) {
    workers.emplace_back([&, w] {
      for (int i = w; i < 400; i += 4) {
        const std::string payer = "cust_" + std::to_string(i % 8);
        CHECK(db.try_debit(payer, amount));
      }
    });
  }
  for (auto& t : workers) t.join();

  std::int64_t total = 0;
  for (const auto& id : db.ids()) {
    total += db.find(id)->balance_cents;
  }
  CHECK(total == 8 * 100'000 - 400 * amount);
}

TEST_CASE("sharded store drop-in for the runtime") {
  auto store = payments::make_default_db(8, 4);
  payments::Runtime runtime(std::move(store),
                            payments::RuntimeOptions{.worker_count = 2,
                                                     .synthetic_rate = 0});
  auto reply = runtime.submit({"m-9", 500, "INR", "cust_1", "cust_6",
                               payments::PaymentMethod::debit_card});
  CHECK(reply.status == payments::TransactionStatus::approved);
  CHECK(runtime.store().find("cust_1")->balance_cents == 999'500);
  runtime.stop();
}
