#pragma once

#include "payments/banks.hpp"
#include "payments/db.hpp"
#include "payments/reply.hpp"
#include "payments/sender.hpp"
#include "payments/types.hpp"

namespace payments {

/// Owns the full payment flow: validate -> balance check -> partner
/// confirmation -> settle -> post reply. Stateless per call; the DB is
/// shared and mutex-guarded, safe for one processor across many threads.
class PaymentProcessor {
 public:
  PaymentProcessor(DB& db, PartnerRouter& router, MessageSender& sender);

  /// Runs one request to completion and returns the same reply that gets
  /// posted to the sender queue.
  PaymentReply process(const PaymentRequest& request);

 private:
  DB& db_;
  PartnerRouter& router_;
  MessageSender& sender_;
};

}  // namespace payments
