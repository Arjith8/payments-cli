#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>

namespace payments {

/// How the customer chose to pay. Methods are only as real as our imaginary
/// banking partners make them — see BankingPartner for provider specifics.
enum class PaymentMethod {
  debit_card,
  credit_card,
  bank_transfer,
  wallet,
};

/// Render an enum to the canonical wire-format string.
/// Kept as free functions so tests stream code points naturally.
const char* to_string(PaymentMethod method);
/// Parse a wire-format method string; throws std::invalid_argument on unknown.
PaymentMethod payment_method_from_string(const std::string& value);

/// A payment as it arrives on the wire.
struct PaymentRequest {
  std::string request_id;   ///< Unique id, minted by our stream / client.
  std::int64_t amount_cents{};  ///< Amount in minor units (e.g. paise).
  std::string currency;     ///< ISO-4217 style, always populated by the stream.
  std::string payer;        ///< Account id of the customer paying.
  std::string payee;        ///< Account id of the customer receiving.
  PaymentMethod method{PaymentMethod::bank_transfer};
};

/// JSON (de)serialisation for PaymentRequest.
void to_json(nlohmann::json& j, const PaymentRequest& r);
void from_json(const nlohmann::json& j, PaymentRequest& r);

/// Parse a request from JSON text; throws on malformed input.
PaymentRequest parse_request(const std::string& text);

}  // namespace payments
