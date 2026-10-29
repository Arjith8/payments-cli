#include "payments/processor.hpp"

#include <doctest/doctest.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <thread>
#include <vector>

namespace {

struct Fixture {
  payments::DB db{R"([
      {"id": "cust_rich", "name": "Rich", "balance_cents": 1000000},
      {"id": "cust_poor", "name": "Poor", "balance_cents": 10},
      {"id": "cust_dest", "name": "Dest", "balance_cents": 0}
  ])"};
  payments::SintaBank sinta{7};
  payments::RoRy rory{7};
  payments::PartnerRouter router{rory, sinta};
  payments::MessageSender sender;
  payments::PaymentProcessor processor{db, router, sender};

  static payments::PaymentRequest request(std::string payer = "cust_rich",
                                          std::string payee = "cust_dest",
                                          std::int64_t amount = 500) {
    return {.request_id = "req-1",
            .amount_cents = amount,
            .currency = "INR",
            .payer = std::move(payer),
            .payee = std::move(payee),
            .method = payments::PaymentMethod::debit_card};
  }
};

}  // namespace

TEST_SUITE_BEGIN("payment flow");

TEST_CASE_FIXTURE(Fixture, "happy path: approval settles and posts a reply") {
  auto reply = processor.process(request());
  CHECK(reply.status == payments::TransactionStatus::approved);
  CHECK(reply.partner == "RoRy");
  CHECK_FALSE(reply.confirm_ref.empty());
  CHECK(reply.reason.empty());

  CHECK(db.find("cust_rich")->balance_cents == 999500);
  CHECK(db.find("cust_dest")->balance_cents == 500);
}

TEST_CASE_FIXTURE(Fixture, "every processed request lands in the sender queue") {
  processor.process(request());
  auto reply = sender.take();
  REQUIRE(reply.has_value());
  CHECK(reply->request_id == "req-1");
  CHECK(reply->status == payments::TransactionStatus::approved);
  CHECK(sender.pending() == 0);
}

TEST_CASE_FIXTURE(Fixture, "invalid requests are failed before consulting a partner") {
  struct {
    payments::PaymentRequest request;
    std::string expected_reason;
  } cases[] = {
      {request("cust_rich", "cust_rich"), "differ"},
      {request("cust_rich", "ghost"), "unknown payee"},
      {request("ghost", "cust_dest"), "unknown payer"},
      {request("cust_rich", "cust_dest", 0), "positive"},
      {request("cust_poor", "cust_dest", 999), "insufficient"},
  };

  for (auto& c : cases) {
    auto reply = processor.process(c.request);
    CHECK_MESSAGE(reply.status == payments::TransactionStatus::failed,
                  c.expected_reason);
    CHECK_MESSAGE(reply.reason.find(c.expected_reason) != std::string::npos,
                  c.expected_reason);
    CHECK_MESSAGE(reply.partner.empty(), c.expected_reason);
  }
  CHECK(db.find("cust_poor")->balance_cents == 10);  // poverty untouched

  // The queue saw exactly the failed requests.
  sender.close();
  std::size_t failed = 0;
  while (auto r = sender.take()) {
    CHECK(r->status == payments::TransactionStatus::failed);
    ++failed;
  }
  CHECK(failed == std::size(cases));
}

TEST_CASE_FIXTURE(Fixture, "partner rejections fail the request without settlement") {
  // Find a seed whose first RoRy verdict is a processing error.
  std::optional<payments::RoRy> failing;
  for (std::uint64_t seed = 1; !failing && seed < 1000; ++seed) {
    payments::RoRy candidate{seed};
    if (!candidate.charge(request()).approved) failing.emplace(seed);
  }
  REQUIRE(failing.has_value());

  payments::PartnerRouter failing_router{*failing, sinta};
  payments::MessageSender failing_sender;
  payments::PaymentProcessor failing_processor{db, failing_router, failing_sender};

  auto reply = failing_processor.process(request());
  CHECK(reply.status == payments::TransactionStatus::rejected);
  CHECK(reply.partner == failing->name());
  CHECK(reply.reason == "processing error");
  CHECK(reply.confirm_ref.empty());

  // No money moved.
  CHECK(db.find("cust_rich")->balance_cents == 1000000);
  CHECK(db.find("cust_dest")->balance_cents == 0);

  failing_sender.close();
  std::size_t rejected = 0;
  while (auto r = failing_sender.take()) {
    CHECK(r->status == payments::TransactionStatus::rejected);
    ++rejected;
  }
  CHECK(rejected == 1);
}

TEST_CASE_FIXTURE(Fixture, "concurrent processing settles every transfer exactly once") {
  const int kTransfers = 200;
  const std::int64_t kAmount = 100;  // total 20_000 of Rich's 1_000_000

  std::vector<std::thread> workers;
  std::atomic<int> approved{0};
  std::atomic<int> processed{0};
  for (int w = 0; w < 4; ++w) {
    workers.emplace_back([&, w] {
      for (int i = w; i < kTransfers; i += 4) {
        auto reply = processor.process(request("cust_rich", "cust_dest", kAmount));
        ++processed;
        if (reply.status == payments::TransactionStatus::approved) ++approved;
      }
    });
  }
  for (auto& t : workers) t.join();

  // RoRy's 8% failure rate means some transfer attempts were rejected, so
  // check the ledger is consistent with whatever got approved.
  const auto rich = db.find("cust_rich")->balance_cents;
  const auto dest = db.find("cust_dest")->balance_cents;
  const auto settled = (1000000 - rich) / kAmount;
  CHECK(rich + dest == 1000000);
  CHECK(settled == dest / kAmount);
  CHECK(settled == approved.load());

  sender.close();
  std::size_t replies = 0;
  while (auto r = sender.take()) ++replies;
  CHECK(processed.load() == kTransfers);
  CHECK(sender.posted_total() == replies);
  CHECK(replies == std::size_t(kTransfers));
}

TEST_SUITE_END();

