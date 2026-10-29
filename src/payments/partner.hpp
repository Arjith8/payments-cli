#pragma once

#include "payments/types.hpp"

#include <string>

namespace payments {

/// Outcome of a single charge attempt against a banking partner.
struct ChargeResult {
  std::string partner;       ///< Which partner handled it.
  std::string confirm_ref;   ///< Partner-assigned reference (empty if rejected).
  bool approved = false;
  std::string reason;        ///< Rejection reason (empty if approved).
};

/// Interface all imaginary banking partners implement.
class IBankingPartner {
 public:
  virtual ~IBankingPartner() = default;

  virtual std::string name() const = 0;
  /// Synchronous attempt; latency and approval rates are simulated per partner.
  virtual ChargeResult charge(const PaymentRequest& request) = 0;
};

}  // namespace payments
