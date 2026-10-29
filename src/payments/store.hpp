#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace payments {

/// A customer as stored in an account store.
struct Customer {
  std::string id;
  std::string name;
  std::int64_t balance_cents;
};

void to_json(nlohmann::json& j, const Customer& c);
void from_json(const nlohmann::json& j, Customer& c);

/// Balance/existence operations. Implementations range from a single
/// mutex-guarded map (DB) to hash-partitioned stores (ShardedDB); the
/// PaymentProcessor only knows this interface.
///
/// All mutating calls must be safe to call concurrently.
class AccountStore {
 public:
  virtual ~AccountStore() = default;

  virtual bool exists(const std::string& id) const = 0;
  virtual std::optional<Customer> find(const std::string& id) const = 0;
  virtual bool has_balance(const std::string& id,
                           std::int64_t amount_cents) const = 0;

  /// Atomically checks and removes amount; false when unknown id or the
  /// balance would go negative.
  virtual bool try_debit(const std::string& id, std::int64_t amount_cents) = 0;

  /// Throws out_of_range for unknown ids.
  virtual void credit(const std::string& id, std::int64_t amount_cents) = 0;

  virtual std::size_t size() const = 0;
  /// All registered customer ids, sorted by id.
  virtual std::vector<std::string> ids() const = 0;
};

}  // namespace payments
