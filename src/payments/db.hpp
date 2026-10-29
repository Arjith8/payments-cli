#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace payments {

/// A customer as stored in the registry.
struct Customer {
  std::string id;
  std::string name;
  std::int64_t balance_cents;
};

void to_json(nlohmann::json& j, const Customer& c);
void from_json(const nlohmann::json& j, Customer& c);

/// Customer registry: existence checks, balance validation, balance mutation.
/// Thread-safe; worker threads in later phases share one DB instance.
class DB {
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

  bool exists(const std::string& id) const;
  std::optional<Customer> find(const std::string& id) const;

  bool has_balance(const std::string& id, std::int64_t amount_cents) const;

  /// Both throw out_of_range for unknown ids; debit additionally throws
  /// invalid_argument when the balance would go negative.
  void debit(const std::string& id, std::int64_t amount_cents);
  void credit(const std::string& id, std::int64_t amount_cents);

  std::size_t size() const;

  /// All registered customer ids, in id order.
  std::vector<std::string> ids() const;

 private:
  const Customer& locate(const std::string& id) const;
  Customer& locate(const std::string& id);

  mutable std::mutex mutex_;
  std::map<std::string, Customer> customers_;
};

}  // namespace payments
