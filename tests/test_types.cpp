#include "payments/types.hpp"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

TEST_CASE("payment method round-trips through strings") {
  for (auto m : {payments::PaymentMethod::debit_card,
                 payments::PaymentMethod::credit_card,
                 payments::PaymentMethod::bank_transfer,
                 payments::PaymentMethod::wallet}) {
    CHECK(payments::payment_method_from_string(payments::to_string(m)) == m);
  }
}

TEST_CASE("unknown method string is rejected") {
  CHECK_THROWS_AS(
      payments::payment_method_from_string("carrier_pigeon"),
      std::invalid_argument);
}

TEST_CASE("request serialises to the full wire shape") {
  const payments::PaymentRequest request{
      .request_id = "req-1",
      .amount_cents = 42500,
      .currency = "INR",
      .payer = "cust_elim",
      .payee = "cust_jaunt",
      .method = payments::PaymentMethod::bank_transfer,
  };

  const auto j = nlohmann::json(request);
  CHECK(j == nlohmann::json::parse(R"({
    "request_id": "req-1",
    "amount_cents": 42500,
    "currency": "INR",
    "payer": "cust_elim",
    "payee": "cust_jaunt",
    "method": "bank_transfer"
  })"));
}

TEST_CASE("request round-trips through json and parse_request") {
  const payments::PaymentRequest request{
      .request_id = "req-2",
      .amount_cents = 10,
      .currency = "USD",
      .payer = "a",
      .payee = "b",
      .method = payments::PaymentMethod::wallet,
  };

  const auto parsed = payments::parse_request(nlohmann::json(request).dump());
  CHECK(parsed.request_id == request.request_id);
  CHECK(parsed.amount_cents == request.amount_cents);
  CHECK(parsed.currency == request.currency);
  CHECK(parsed.payer == request.payer);
  CHECK(parsed.payee == request.payee);
  CHECK(parsed.method == request.method);
}

TEST_CASE("malformed json throws") {
  CHECK_THROWS_AS(payments::parse_request("{ nope"), nlohmann::json::parse_error);
  CHECK_THROWS_AS(payments::parse_request(R"({"request_id": "x"})"),
                  nlohmann::json::out_of_range);
}
