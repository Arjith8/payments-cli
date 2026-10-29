#include "payments/processor.hpp"

namespace payments {

PaymentProcessor::PaymentProcessor(DB& db, PartnerRouter& router,
                                   MessageSender& sender)
    : db_(db), router_(router), sender_(sender) {}

PaymentReply PaymentProcessor::process(const PaymentRequest& request) {
  auto post = [&](PaymentReply reply) {
    sender_.post(reply);
    return reply;
  };

  const auto invalid = [&](std::string reason) {
    return post(PaymentReply{.request_id = request.request_id,
                             .status = TransactionStatus::failed,
                             .partner = "",
                             .confirm_ref = "",
                             .reason = std::move(reason)});
  };

  if (request.amount_cents <= 0) {
    return invalid("amount must be positive");
  }
  if (request.payer == request.payee) {
    return invalid("payer and payee must differ");
  }
  if (!db_.exists(request.payer)) {
    return invalid("unknown payer " + request.payer);
  }
  if (!db_.exists(request.payee)) {
    return invalid("unknown payee " + request.payee);
  }
  auto& partner = router_.pick(request);
  const ChargeResult result = partner.charge(request);
  if (!result.approved) {
    return post(PaymentReply{.request_id = request.request_id,
                             .status = TransactionStatus::rejected,
                             .partner = result.partner,
                             .confirm_ref = "",
                             .reason = result.reason});
  }
  if (!db_.try_debit(request.payer, request.amount_cents)) {
    return invalid("insufficient balance for " + request.payer);
  }
  db_.credit(request.payee, request.amount_cents);

  return post(PaymentReply{.request_id = request.request_id,
                           .status = TransactionStatus::approved,
                           .partner = result.partner,
                           .confirm_ref = result.confirm_ref,
                           .reason = ""});
}

}  // namespace payments
