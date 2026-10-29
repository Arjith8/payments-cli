#include "payments/db.hpp"

#include <stdexcept>
#include <utility>

namespace payments {

void to_json(nlohmann::json& j, const Customer& c) {
  j = nlohmann::json{{"id", c.id},
                     {"name", c.name},
                     {"balance_cents", c.balance_cents}};
}

void from_json(const nlohmann::json& j, Customer& c) {
  j.at("id").get_to(c.id);
  j.at("name").get_to(c.name);
  j.at("balance_cents").get_to(c.balance_cents);
}

DB::DB(std::vector<Customer> customers) {
  for (auto& customer : customers) {
    if (customer.balance_cents < 0) {
      throw std::invalid_argument("negative balance for " + customer.id);
    }
    if (!customers_.emplace(customer.id, std::move(customer)).second) {
      throw std::invalid_argument("duplicate customer id");
    }
  }
}

DB::DB(const std::string& json_text)
    : DB(nlohmann::json::parse(json_text).get<std::vector<Customer>>()) {}

bool DB::exists(const std::string& id) const {
  std::lock_guard lock(mutex_);
  return customers_.count(id) != 0;
}

std::optional<Customer> DB::find(const std::string& id) const {
  std::lock_guard lock(mutex_);
  auto it = customers_.find(id);
  if (it == customers_.end()) return std::nullopt;
  return it->second;
}

bool DB::has_balance(const std::string& id, std::int64_t amount_cents) const {
  std::lock_guard lock(mutex_);
  auto it = customers_.find(id);
  return it != customers_.end() && it->second.balance_cents >= amount_cents;
}

void DB::debit(const std::string& id, std::int64_t amount_cents) {
  std::lock_guard lock(mutex_);
  if (locate(id).balance_cents < amount_cents) {
    throw std::invalid_argument("insufficient balance for " + id);
  }
  locate(id).balance_cents -= amount_cents;
}

void DB::credit(const std::string& id, std::int64_t amount_cents) {
  std::lock_guard lock(mutex_);
  locate(id).balance_cents += amount_cents;
}

std::size_t DB::size() const {
  std::lock_guard lock(mutex_);
  return customers_.size();
}

const Customer& DB::locate(const std::string& id) const {
  auto it = customers_.find(id);
  if (it == customers_.end()) throw std::out_of_range("unknown customer: " + id);
  return it->second;
}

Customer& DB::locate(const std::string& id) {
  auto it = customers_.find(id);
  if (it == customers_.end()) throw std::out_of_range("unknown customer: " + id);
  return it->second;
}

}  // namespace payments
