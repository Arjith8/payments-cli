#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>

namespace payments {

/// How the customer chose to pay.
enum class PaymentMethod {
  debit_card,
  credit_card,
  bank_transfer,
  wallet,
};

const char* to_string(PaymentMethod method);
PaymentMethod payment_method_from_string(const std::string& value);

/// A request as it arrives on the wire.
struct PaymentRequest {
  std::string request_id;
  std::int64_t amount_cents;
  std::string currency;
  std::string payer;
  std::string payee;
  PaymentMethod method;
};

/// JSON (de)serialisation.
void to_json(nlohmann::json& j, const PaymentRequest& r);
void from_json(const nlohmann::json& j, PaymentRequest& r);

/// Parse from JSON text; throws on malformed input.
PaymentRequest parse_request(const std::string& text);

}  // namespace payments
