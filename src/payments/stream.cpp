#include "payments/stream.hpp"


#include <algorithm>
#include <chrono>
#include <thread>
#include <limits>
#include <stdexcept>
#include <utility>

namespace payments {

namespace detail {

std::vector<std::string> make_customer_pool(std::size_t count,
                                            const std::string& prefix) {
  std::vector<std::string> pool;
  pool.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    pool.push_back(prefix + std::to_string(i));
  }
  return pool;
}

}  // namespace detail

MockStream::MockStream(StreamConfig config)
    : config_(std::move(config)),
      rng_(config_.seed),
      customers_(detail::make_customer_pool(config_.customer_count, "cust_")) {
  if (customers_.size() < 2) {
    throw std::invalid_argument("StreamConfig needs at least 2 customers: "
                                "a payment requires payer != payee");
  }
  payer_pick_ = std::uniform_int_distribution<std::size_t>(0, customers_.size() - 1);

  std::vector<double> weights;
  weights.reserve(config_.method_mix.size());
  for (const auto& [weight, method] : config_.method_mix) {
    if (weight <= 0) continue;
    weights.push_back(weight);
    method_values_.push_back(method);
  }
  if (method_values_.empty()) {
    throw std::invalid_argument("StreamConfig.method_mix has no positive weights");
  }
  method_distribution_ = std::discrete_distribution<std::size_t>(
      weights.begin(), weights.end());
}

PaymentRequest MockStream::next() {
  // Payer and payee must differ; rejection-sample until they do.
  auto payer = payer_pick_(rng_);
  auto payee = payer_pick_(rng_);
  while (payee == payer) {
    payee = payer_pick_(rng_);
  }

  PaymentRequest request;
  request.request_id = "req-" + std::to_string(produced_.load());
  request.amount_cents = next_amount();
  request.currency = "INR";
  request.payer = customers_[payer];
  request.payee = customers_[payee];
  request.method = next_method();

  ++produced_;
  return request;
}

std::uint64_t MockStream::produced() const { return produced_.load(); }

PaymentMethod MockStream::next_method() {
  return method_values_[method_distribution_(rng_)];
}

std::int64_t MockStream::next_amount() {
  std::uniform_int_distribution<std::int64_t> amount(config_.min_amount_cents,
                                                     config_.max_amount_cents);
  return amount(rng_);
}

void MockStream::run(BlockingQueue<PaymentRequest>& sink, std::uint64_t count,
                     bool close_sink) {
  const auto tick = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::duration<double>(
          config_.requests_per_second > 0 ? 1.0 / config_.requests_per_second
                                          : 0.0));

  std::uniform_real_distribution<double> jitter(0.5, 1.5);

  for (std::uint64_t i = 0; i < count && !stopping_.load(); ++i) {
    if (tick.count() > 0) {
      std::this_thread::sleep_for(tick * jitter(rng_));
    }
    sink.push(next());
  }
  if (close_sink) sink.close();
}

void MockStream::start_async(BlockingQueue<PaymentRequest>& sink,
                             std::uint64_t rate_limit) {
  if (rate_limit > 0) {
    config_.requests_per_second = static_cast<double>(rate_limit);
  }
  // Detached by design: stops via stop() / queue close, never joined.
  std::thread([this, &sink]() mutable {
    run(sink, std::numeric_limits<std::uint64_t>::max(), /*close_sink=*/false);
    sink.close();
  }).detach();
}

void MockStream::stop() { stopping_ = true; }

}  // namespace payments
