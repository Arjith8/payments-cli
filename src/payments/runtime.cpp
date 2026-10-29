#include "payments/runtime.hpp"

#include <iostream>

#include <stdexcept>

namespace payments {

DB make_default_db(std::size_t customer_count) {
  std::vector<Customer> customers;
  customers.reserve(customer_count);
  for (std::size_t i = 0; i < customer_count; ++i) {
    customers.push_back({"cust_" + std::to_string(i),
                         "Customer " + std::to_string(static_cast<int>(i)),
                         1'000'000});
  }
  return DB(std::move(customers));
}

Runtime::Runtime(DB db, RuntimeOptions options)
    : db_(std::move(db)),
      options_(options),
      router_(rory_, sinta_),
      processor_(db_, router_, sender_),
      stream_(StreamConfig{.customer_count = db_.size(),
                           .requests_per_second = 0,
                           .seed = options_.stream_seed,
                           .customer_ids = db_.ids()}) {}

Runtime::~Runtime() { stop(); }

void Runtime::start() {
  if (running_.exchange(true)) return;
  workers_.reserve(options_.worker_count);
  for (std::size_t id = 0; id < options_.worker_count; ++id) {
    workers_.emplace_back([this] { worker_loop(); });
  }
  deliverer_.emplace([this] { deliver_loop(); });
  if (options_.synthetic_rate > 0) {
    stream_.start_async(inbound_, options_.synthetic_rate);
  }
}

void Runtime::worker_loop() {
  while (auto request = inbound_.pop()) {
    account(processor_.process(*request));
  }
}

void Runtime::deliver_loop() {
  while (auto reply = sender_.take()) {
    std::cout << nlohmann::json(*reply).dump() << '\n';
    std::cout.flush();
  }
}

void Runtime::account(const PaymentReply& reply) {
  ++processed_;
  switch (reply.status) {
    case TransactionStatus::approved:
      ++approved_;
      break;
    case TransactionStatus::rejected:
      ++rejected_;
      break;
    case TransactionStatus::failed:
      ++failed_;
      break;
  }
}

PaymentReply Runtime::submit(PaymentRequest request) {
  if (!running_.load()) start();
  const auto reply = processor_.process(request);
  account(reply);
  return reply;
}

void Runtime::stop() {
  if (!running_.exchange(false)) return;
  stream_.stop();
  inbound_.close();
  workers_.clear();
  sender_.close();
  if (deliverer_) {
    deliverer_->join();
    deliverer_.reset();
  }
}

void Runtime::restart_synthetic(std::uint64_t rate) {
  stream_.stop();
  options_.synthetic_rate = rate;
  if (running_.load() && rate > 0) stream_.start_async(inbound_, rate);
}

RuntimeStats Runtime::stats() const {
  return {processed_.load(), approved_.load(), rejected_.load(), failed_.load()};
}

}  // namespace payments
