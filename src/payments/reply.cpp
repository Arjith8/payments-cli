#include "payments/reply.hpp"

namespace payments {

const char* to_string(TransactionStatus status) {
  switch (status) {
    case TransactionStatus::approved:
      return "approved";
    case TransactionStatus::rejected:
      return "rejected";
    case TransactionStatus::failed:
      return "failed";
  }
  return "unknown";
}

void to_json(nlohmann::json& j, const PaymentReply& r) {
  j = nlohmann::json{{"request_id", r.request_id},
                     {"status", to_string(r.status)},
                     {"partner", r.partner},
                     {"confirm_ref", r.confirm_ref},
                     {"reason", r.reason}};
}

}  // namespace payments
