#pragma once

#include "payments/types.hpp"

#include <string>

namespace payments {

/// How a request finally resolved.
enum class TransactionStatus {
  approved,
  rejected,   ///< Valid request the partner refused.
  failed,     ///< Invalid request (unknown customer, bad amount, ...).
};

const char* to_string(TransactionStatus status);

/// Final reply returned to the requester (and posted to the outbound queue).
struct PaymentReply {
  std::string request_id;
  TransactionStatus status = TransactionStatus::failed;
  std::string partner;      ///< Empty when no partner was consulted.
  std::string confirm_ref;  ///< Partner reference; empty unless approved.
  std::string reason;       ///< Human-readable detail for rejected/failed.
};

void to_json(nlohmann::json& j, const PaymentReply& r);

}  // namespace payments
