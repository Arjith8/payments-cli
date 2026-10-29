#pragma once

#include "payments/partner.hpp"
#include "payments/types.hpp"

#include <random>

#include <string>

namespace payments {

namespace detail {
std::string mint_reference(const std::string& prefix, std::mt19937& rng);
}  // namespace detail

/// Shared plumbing for concrete partners: identity, seeded randomness,
/// reference minting. Subclasses define policy in charge().
class BankingPartner : public IBankingPartner {
 public:
  virtual ~BankingPartner() = default;

  std::string name() const override { return name_; }

 protected:
  BankingPartner(std::string name, std::uint64_t seed)
      : name_(std::move(name)), rng_(seed) {}

  /// Bernoulli flip with probability p (0..1).
  bool coin(double p) { return std::bernoulli_distribution(p)(rng_); }

  std::string make_ref() {
    return detail::mint_reference(name_ + "-" + std::to_string(++sequence_), rng_);
  }

 private:
  std::string name_;
  std::mt19937 rng_;
  std::uint64_t sequence_ = 0;
};

/// SintaBank: conservative. Refuses wallet and big credit-card spend,
/// slow (-ish) but reliable.
class SintaBank final : public BankingPartner {
 public:
  SintaBank() : BankingPartner("SintaBank", 0x517A) {}
  explicit SintaBank(std::uint64_t seed) : BankingPartner("SintaBank", seed) {}

  ChargeResult charge(const PaymentRequest& request) override;
};

/// RoRy: fast, permissive on methods, higher technical-failure rate.
class RoRy final : public BankingPartner {
 public:
  RoRy() : BankingPartner("RoRy", 0x4F52) {}
  explicit RoRy(std::uint64_t seed) : BankingPartner("RoRy", seed) {}

  ChargeResult charge(const PaymentRequest& request) override;
};

/// Static method -> partner affinity used by the router.
class PartnerRouter {
 public:
  /// Ownership stays with the router.
  explicit PartnerRouter(RoRy& rory, SintaBank& sinta);

  IBankingPartner& pick(const PaymentRequest& request) const;

 private:
  RoRy& rory_;
  SintaBank& sinta_;
};

}  // namespace payments
