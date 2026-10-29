#include "payments/sharded.hpp"

#include <unordered_set>
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <stdexcept>

namespace payments {

std::size_t ShardedDB::shard_for(const std::string& id,
                                 std::size_t shard_count) {
  return std::hash<std::string>{}(id) % shard_count;
}

ShardedDB::ShardedDB(std::vector<Customer> customers, std::size_t shard_count) {
  if (shard_count == 0) {
    throw std::invalid_argument("shard_count must be positive");
  }

  std::unordered_set<std::string> seen;
  seen.reserve(customers.size());
  for (const auto& customer : customers) {
    if (!seen.insert(customer.id).second) {
      throw std::invalid_argument("duplicate customer id " + customer.id);
    }
  }

  std::vector<std::vector<Customer>> partitions(shard_count);
  for (auto& customer : customers) {
    partitions[shard_for(customer.id, shard_count)].push_back(
        std::move(customer));
  }

  shards_.reserve(shard_count);
  for (auto& partition : partitions) {
    shards_.push_back(std::make_unique<DB>(std::move(partition)));
  }
  total_size_ = seen.size();
}

ShardedDB::ShardedDB(const std::string& json_text, std::size_t shard_hint) {
  auto customers =
      nlohmann::json::parse(json_text).get<std::vector<Customer>>();
  const std::size_t shards =
      shard_hint > 0 ? shard_hint : std::min<std::size_t>(
                                       customers.size() / 50 + 1, 16);
  *this = ShardedDB(std::move(customers), shards);
}

bool ShardedDB::exists(const std::string& id) const {
  return shard(id).exists(id);
}

std::optional<Customer> ShardedDB::find(const std::string& id) const {
  return shard(id).find(id);
}

bool ShardedDB::has_balance(const std::string& id,
                            std::int64_t amount_cents) const {
  return shard(id).has_balance(id, amount_cents);
}

bool ShardedDB::try_debit(const std::string& id, std::int64_t amount_cents) {
  return shard(id).try_debit(id, amount_cents);
}

void ShardedDB::credit(const std::string& id, std::int64_t amount_cents) {
  shard(id).credit(id, amount_cents);
}

std::size_t ShardedDB::size() const { return total_size_; }

std::vector<std::string> ShardedDB::ids() const {
  std::vector<std::string> result;
  result.reserve(total_size_);
  for (const auto& one : shards_) {
    for (const auto& id : one->ids()) result.push_back(id);
  }
  std::sort(result.begin(), result.end());
  return result;
}

DB& ShardedDB::shard(const std::string& id) {
  return *shards_[shard_for(id, shards_.size())];
}

const DB& ShardedDB::shard(const std::string& id) const {
  return *shards_[shard_for(id, shards_.size())];
}

std::unique_ptr<AccountStore> make_default_db(std::size_t customer_count,
                                              std::size_t shard_count) {
  std::vector<Customer> customers;
  customers.reserve(customer_count);
  for (std::size_t i = 0; i < customer_count; ++i) {
    customers.push_back({"cust_" + std::to_string(i),
                         "Customer " + std::to_string(static_cast<int>(i)),
                         1'000'000});
  }
  if (shard_count <= 1) {
    return std::make_unique<DB>(std::move(customers));
  }
  return std::make_unique<ShardedDB>(std::move(customers), shard_count);
}

}  // namespace payments
