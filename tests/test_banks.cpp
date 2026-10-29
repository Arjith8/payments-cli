#include "payments/banks.hpp"

#include <doctest/doctest.h>

#include <unordered_set>

namespace {

payments::PaymentRequest with_method(payments::PaymentMethod method,
                                     std::int64_t amount = 1000) {
  return {.request_id = "req-x",
          .amount_cents = amount,
          .currency = "INR",
          .payer = "cust_0",
          .payee = "cust_1",
          .method = method};
}

}  // namespace

TEST_CASE("approved charges carry a partner-branded confirmation ref") {
  payments::SintaBank sinta{42};
  payments::RoRy rory{42};

  auto sinta_hit = sinta.charge(with_method(payments::PaymentMethod::bank_transfer));
  REQUIRE(sinta_hit.approved);
  CHECK(sinta_hit.partner == "SintaBank");
  CHECK(sinta_hit.confirm_ref.starts_with("SintaBank-"));

  auto rory_hit = rory.charge(with_method(payments::PaymentMethod::wallet));
  REQUIRE(rory_hit.approved);
  CHECK(rory_hit.confirm_ref.starts_with("RoRy-"));
}

TEST_CASE("confirmation refs are unique") {
  payments::SintaBank sinta{1};
  std::unordered_set<std::string> refs;
  for (int i = 0; i < 30; ++i) {
    auto result = sinta.charge(with_method(payments::PaymentMethod::bank_transfer));
    if (result.approved) refs.insert(result.confirm_ref);
  }
  CHECK(refs.size() > 25);
}

TEST_CASE("SintaBank policy") {
  payments::SintaBank sinta{7};

  auto wallet = sinta.charge(with_method(payments::PaymentMethod::wallet));
  CHECK_FALSE(wallet.approved);
  CHECK(wallet.confirm_ref.empty());
  CHECK(wallet.reason.find("wallet") != std::string::npos);

  auto credit_over = sinta.charge(with_method(payments::PaymentMethod::credit_card, 50'000));
  CHECK_FALSE(credit_over.approved);
  CHECK(credit_over.reason.find("ceiling") != std::string::npos);

  auto credit_under = sinta.charge(with_method(payments::PaymentMethod::credit_card, 40'000));
  CHECK(credit_under.approved);
}

TEST_CASE("partners with the same seed replay identically") {
  payments::RoRy left{1234};
  payments::RoRy right{1234};
  const auto request = with_method(payments::PaymentMethod::debit_card);

  for (int i = 0; i < 50; ++i) {
    CHECK(left.charge(request).approved == right.charge(request).approved);
  }
}

TEST_CASE("router prefers RoRy lightly and SintaBank formally") {
  payments::SintaBank sinta{3};
  payments::RoRy rory{3};
  payments::PartnerRouter router{rory, sinta};

  CHECK(&router.pick(with_method(payments::PaymentMethod::wallet)) == &rory);
  CHECK(&router.pick(with_method(payments::PaymentMethod::debit_card)) == &rory);
  CHECK(&router.pick(with_method(payments::PaymentMethod::credit_card)) == &sinta);
  CHECK(&router.pick(with_method(payments::PaymentMethod::bank_transfer)) == &sinta);
}

TEST_CASE("rejection refs never leak a confirmation") {
  payments::SintaBank sinta{99};
  for (int i = 0; i < 40; ++i) {
    auto result = sinta.charge(with_method(payments::PaymentMethod::wallet));
    CHECK_FALSE(result.approved);
    CHECK(result.confirm_ref.empty());
    CHECK_FALSE(result.reason.empty());
  }
}
