#include "payments/banks.hpp"

#include <cstdint>

namespace payments {

namespace detail {

std::string mint_reference(const std::string& prefix, std::mt19937& rng) {
  std::uniform_int_distribution<int> hex(0, 15);
  static constexpr char kDigits[] = "0123456789abcdef";
  std::string suffix(10, '0');
  for (auto& ch : suffix) {
    ch = kDigits[hex(rng)];
  }
  return prefix + "-" + suffix;
}

}  // namespace detail

ChargeResult SintaBank::charge(const PaymentRequest& request) {
  if (request.method == PaymentMethod::wallet) {
    return {.partner = name(),
            .approved = false,
            .reason = "wallet payments are not accepted"};
  }

  if (request.method == PaymentMethod::credit_card &&
      request.amount_cents > 40'000) {
    return {.partner = name(),
            .approved = false,
            .reason = "credit-card amount exceeds SintaBank ceiling"};
  }

  if (coin(0.02)) {
    return {.partner = name(), .approved = false, .reason = "processing error"};
  }

  return {.partner = name(), .confirm_ref = make_ref(), .approved = true};
}

ChargeResult RoRy::charge(const PaymentRequest& request) {
  if (coin(0.08)) {
    return {.partner = name(), .approved = false, .reason = "processing error"};
  }

  return {.partner = name(), .confirm_ref = make_ref(), .approved = true};
}

PartnerRouter::PartnerRouter(RoRy& rory, SintaBank& sinta)
    : rory_(rory), sinta_(sinta) {}

IBankingPartner& PartnerRouter::pick(const PaymentRequest& request) const {
  switch (request.method) {
    case PaymentMethod::credit_card:
    case PaymentMethod::bank_transfer:
      return sinta_;
    default:
      return rory_;
  }
}

}  // namespace payments
