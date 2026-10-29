#include "payments/db.hpp"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <thread>

namespace {

const std::string kThreeCustomers = R"([
  {"id": "cust_a", "name": "Ada",  "balance_cents": 100000},
  {"id": "cust_b", "name": "Bas",  "balance_cents": 500},
  {"id": "cust_c", "name": "Cai",  "balance_cents": 0}
])";

payments::DB small_db() { return payments::DB(kThreeCustomers); }

}  // namespace

TEST_CASE("loads customers from json and answers existence") {
  auto db = small_db();
  CHECK(db.size() == 3);
  CHECK(db.exists("cust_a"));
  CHECK_FALSE(db.exists("ghost"));
}

TEST_CASE("find returns full record") {
  auto db = small_db();
  auto ada = db.find("cust_a");
  REQUIRE(ada.has_value());
  CHECK(ada->name == "Ada");
  CHECK(ada->balance_cents == 100000);
  CHECK_FALSE(db.find("ghost").has_value());
}

TEST_CASE("has_balance is inclusive of exact amounts") {
  auto db = small_db();
  CHECK(db.has_balance("cust_b", 500));
  CHECK_FALSE(db.has_balance("cust_b", 501));
  CHECK_FALSE(db.has_balance("cust_c", 1));
  CHECK_FALSE(db.has_balance("ghost", 1));
}

TEST_CASE("at zero balance, only zero amounts pass") {
  auto db = small_db();
  CHECK(db.has_balance("cust_c", 0));
}

TEST_CASE("debit moves money out and preserves it") {
  auto db = small_db();
  db.debit("cust_b", 500);
  auto bas = db.find("cust_b");
  REQUIRE(bas.has_value());
  CHECK(bas->balance_cents == 0);
  CHECK_FALSE(db.has_balance("cust_b", 1));
}

TEST_CASE("debit below balance is rejected atomically") {
  auto db = small_db();
  CHECK_THROWS_AS(db.debit("cust_b", 501), std::invalid_argument);
  CHECK(db.find("cust_b")->balance_cents == 500);
  CHECK_THROWS_AS(db.debit("ghost", 1), std::out_of_range);
}

TEST_CASE("credit and debit compose") {
  auto db = small_db();
  db.credit("cust_c", 250);
  db.debit("cust_a", 300);
  CHECK(db.find("cust_c")->balance_cents == 250);
  CHECK(db.find("cust_a")->balance_cents == 99700);
}

TEST_CASE("customer json round-trip") {
  const payments::Customer ada{"cust_a", "Ada", 100000};
  CHECK(nlohmann::json(ada) == nlohmann::json::parse(R"({
    "id": "cust_a", "name": "Ada", "balance_cents": 100000
  })"));
}

TEST_CASE("rejects duplicate ids and negative balances") {
  CHECK_THROWS_AS(payments::DB(R"([
    {"id": "x", "name": "one", "balance_cents": 1},
    {"id": "x", "name": "two", "balance_cents": 2}
  ])"), std::invalid_argument);

  CHECK_THROWS_AS(payments::DB(R"([
    {"id": "x", "name": "debt", "balance_cents": -5}
  ])"), std::invalid_argument);
}

TEST_CASE("concurrent debit accounts total exactly once each") {
  auto db = small_db();

  std::thread a([&db] { db.debit("cust_a", 60000); });
  std::thread b([&db] { db.debit("cust_a", 40000); });
  a.join();
  b.join();

  CHECK(db.find("cust_a")->balance_cents == 0);
  CHECK_FALSE(db.has_balance("cust_a", 1));
}
