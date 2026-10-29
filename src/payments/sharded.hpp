#pragma once

#include "payments/db.hpp"
#include "payments/store.hpp"

#include <memory>
#include <string>
#include <vector>

namespace payments {

/// Hash-partitioned AccountStore: one inner DB (with its own mutex) per
/// shard. Owning a ready-made store is intentional — swap dependency-free.
class ShardedDB : public AccountStore {
 public:
  /// Distributes customers over `shard_count` shards; throws on duplicate
  /// ids (across shards too), negative balances, or shard_count == 0.
  ShardedDB(std::vector<Customer> customers, std::size_t shard_count);
  /// Parse from JSON text and delegate; auto-size shards.
  explicit ShardedDB(const std::string& json_text,
                     std::size_t shard_hint = 0);

  bool exists(const std::string& id) const override;
  std::optional<Customer> find(const std::string& id) const override;
  bool has_balance(const std::string& id,
                   std::int64_t amount_cents) const override;
  bool try_debit(const std::string& id, std::int64_t amount_cents) override;
  void credit(const std::string& id, std::int64_t amount_cents) override;
  std::size_t size() const override;
  std::vector<std::string> ids() const override;

  /// Number of independent partitions.
  std::size_t shard_count() const { return shards_.size(); }

  static std::size_t shard_for(const std::string& id,
                               std::size_t shard_count);

 private:
  DB& shard(const std::string& id);
  const DB& shard(const std::string& id) const;

  std::vector<std::unique_ptr<DB>> shards_;
  std::size_t total_size_ = 0;
};

/// Default registry split across `shard_count` partitions.
std::unique_ptr<AccountStore> make_default_db(std::size_t customer_count,
                                              std::size_t shard_count = 1);

}  // namespace payments
