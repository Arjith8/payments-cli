#pragma once

#include "payments/store.hpp"

#include <map>
#include <mutex>
#include <string>

namespace payments {

/// Mono store: one mutex over one map. The AccountStore implementation
/// reference; sharding can beat it once hot-account contention shows up.
class DB : public AccountStore {
 public:
  /// Build from a JSON array of customers; throws on duplicate ids,
  /// negative balances, or malformed entries.
  explicit DB(std::vector<Customer> customers);
  /// Parse from JSON text and delegate.
  explicit DB(const std::string& json_text);

  DB(DB&& other);
  DB& operator=(DB&& other);
  DB(const DB&) = delete;
  DB& operator=(const DB&) = delete;

  bool exists(const std::string& id) const override;
  std::optional<Customer> find(const std::string& id) const override;

  bool has_balance(const std::string& id,
                   std::int64_t amount_cents) const override;

  bool try_debit(const std::string& id,
                 std::int64_t amount_cents) override;

  void credit(const std::string& id, std::int64_t amount_cents) override;

  /// Throws out_of_range for unknown ids; invalid_argument when the balance
  /// would go negative. DB-specific convenience beyond the store interface.
  void debit(const std::string& id, std::int64_t amount_cents);

  std::size_t size() const override;
  std::vector<std::string> ids() const override;

 private:
  const Customer& locate(const std::string& id) const;
  Customer& locate(const std::string& id);

  mutable std::mutex mutex_;
  std::map<std::string, Customer> customers_;
};

}  // namespace payments
