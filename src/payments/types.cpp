#include "payments/types.hpp"

#include <stdexcept>

namespace payments {

const char* to_string(PaymentMethod method) {
  switch (method) {
    case PaymentMethod::debit_card:
      return "debit_card";
    case PaymentMethod::credit_card:
      return "credit_card";
    case PaymentMethod::bank_transfer:
      return "bank_transfer";
    case PaymentMethod::wallet:
      return "wallet";
  }
  return "unknown";
}

PaymentMethod payment_method_from_string(const std::string& value) {
  if (value == "debit_card") return PaymentMethod::debit_card;
  if (value == "credit_card") return PaymentMethod::credit_card;
  if (value == "bank_transfer") return PaymentMethod::bank_transfer;
  if (value == "wallet") return PaymentMethod::wallet;
  throw std::invalid_argument("unknown payment method: " + value);
}

void to_json(nlohmann::json& j, const PaymentRequest& r) {
  j = nlohmann::json{{"request_id", r.request_id},
                     {"amount_cents", r.amount_cents},
                     {"currency", r.currency},
                     {"payer", r.payer},
                     {"payee", r.payee},
                     {"method", to_string(r.method)}};
}

void from_json(const nlohmann::json& j, PaymentRequest& r) {
  j.at("request_id").get_to(r.request_id);
  j.at("amount_cents").get_to(r.amount_cents);
  j.at("currency").get_to(r.currency);
  j.at("payer").get_to(r.payer);
  j.at("payee").get_to(r.payee);
  r.method = payment_method_from_string(j.at("method").get<std::string>());
}

PaymentRequest parse_request(const std::string& text) {
  return nlohmann::json::parse(text).get<PaymentRequest>();
}

}  // namespace payments
