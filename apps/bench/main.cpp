#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include <thread>

#include "payments/banks.hpp"
#include "payments/db.hpp"
#include "payments/processor.hpp"
#include "payments/runtime.hpp"
#include "payments/stream.hpp"

#include <atomic>

namespace {

using Clock = std::chrono::steady_clock;

struct BenchResult {
  std::size_t threads = 0;
  std::uint64_t requests = 0;
  double wall_seconds = 0.0;
  double mean_us = 0.0;
  double p50_us = 0.0, p95_us = 0.0, p99_us = 0.0, max_us = 0.0;
  std::uint64_t approved = 0, rejected = 0, failed = 0;
};

double us_of(std::chrono::steady_clock::duration d) {
  return std::chrono::duration<double, std::micro>(d).count();
}

/// Per-request latency around processor.process; walltime from first to last.
struct Latencies {
  std::vector<std::chrono::steady_clock::duration> samples;
  Clock::time_point first, last;

  void record(Clock::time_point start, Clock::time_point end) {
    if (samples.empty()) first = start;
    samples.push_back(end - start);
    last = end;
  }
};

BenchResult bench_sequential(
    const std::vector<payments::PaymentRequest>& workload,
    payments::AccountStore& store, payments::RoRy& rory,
    payments::PartnerRouter& router, payments::MessageSender& sender) {
  payments::PaymentProcessor processor{store, router, sender};
  Latencies latencies;
  const auto overall_start = Clock::now();

  BenchResult out;
  out.requests = workload.size();
  out.threads = 1;
  for (const auto& request : workload) {
    const auto t0 = Clock::now();
    const auto reply = processor.process(request);
    const auto t1 = Clock::now();
    latencies.record(t0, t1);
    switch (reply.status) {
      case payments::TransactionStatus::approved: ++out.approved; break;
      case payments::TransactionStatus::rejected: ++out.rejected; break;
      case payments::TransactionStatus::failed: ++out.failed; break;
    }
  }
  out.wall_seconds =
      std::chrono::duration<double>(Clock::now() - overall_start).count();

  auto sorted = latencies.samples;
  std::sort(sorted.begin(), sorted.end());
  if (!sorted.empty()) {
    out.mean_us =
        us_of(std::accumulate(sorted.begin(), sorted.end(),
                              std::chrono::steady_clock::duration{})) /
        static_cast<double>(sorted.size());
    out.p50_us = us_of(sorted[sorted.size() * 50 / 100]);
    out.p95_us = us_of(sorted[sorted.size() * 95 / 100]);
    out.p99_us = us_of(sorted[sorted.size() * 99 / 100]);
    out.max_us = us_of(sorted.back());
  }
  return out;
}

BenchResult bench_threaded(
    std::size_t threads, const std::vector<payments::PaymentRequest>& workload,
    payments::AccountStore& store, payments::RoRy& rory,
    payments::PartnerRouter& router, payments::MessageSender& sender) {
  payments::BlockingQueue<payments::PaymentRequest> queue;
  for (const auto& request : workload) queue.push(request);
  queue.close();

  payments::PaymentProcessor processor{store, router, sender};
  std::vector<Latencies> per_thread(threads);
  std::atomic<std::uint64_t> approved{0}, rejected{0}, failed{0};
  const auto overall_start = Clock::now();

  std::vector<std::thread> workers;
  workers.reserve(threads);
  for (std::size_t w = 0; w < threads; ++w) {
    workers.emplace_back([&, w] {
      auto& lat = per_thread[w];
      while (auto request = queue.pop()) {
        const auto t0 = Clock::now();
        const auto reply = processor.process(*request);
        const auto t1 = Clock::now();
        lat.record(t0, t1);
        switch (reply.status) {
          case payments::TransactionStatus::approved: ++approved; break;
          case payments::TransactionStatus::rejected: ++rejected; break;
          case payments::TransactionStatus::failed: ++failed; break;
        }
      }
    });
  }
  for (auto& t : workers) t.join();
  const auto overall_end = Clock::now();

  BenchResult out;
  out.requests = workload.size();
  out.threads = threads;
  out.wall_seconds =
      std::chrono::duration<double>(overall_end - overall_start).count();
  out.approved = approved.load();
  out.rejected = rejected.load();
  out.failed = failed.load();

  Latencies merged;
  for (const auto& part : per_thread) {
    merged.samples.insert(merged.samples.end(), part.samples.begin(),
                          part.samples.end());
  }
  auto sorted = merged.samples;
  std::sort(sorted.begin(), sorted.end());
  if (!sorted.empty()) {
    out.mean_us =
        us_of(std::accumulate(sorted.begin(), sorted.end(),
                              std::chrono::steady_clock::duration{})) /
        static_cast<double>(sorted.size());
    out.p50_us = us_of(sorted[sorted.size() * 50 / 100]);
    out.p95_us = us_of(sorted[sorted.size() * 95 / 100]);
    out.p99_us = us_of(sorted[sorted.size() * 99 / 100]);
    out.max_us = us_of(sorted.back());
  }
  return out;
}

void print_row(const BenchResult& r) {
  const double throughput = r.wall_seconds > 0 ? r.requests / r.wall_seconds : 0;
  std::cout << std::setw(8) << r.threads << std::setw(14)
            << std::fixed << std::setprecision(0) << throughput << std::setw(12)
            << std::setprecision(1) << r.mean_us << std::setw(10) << r.p50_us
            << std::setw(10) << r.p95_us << std::setw(10) << r.p99_us
            << std::setw(12) << r.max_us << std::setw(11) << r.approved << "  "
            << std::setw(9) << r.rejected << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::uint64_t request_count = 20000;
  std::uint64_t seed = 42;
  std::size_t customers = 12;
  std::size_t shards = 1;
  std::vector<std::size_t> thread_counts{1, 2, 4, 8};

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto value = [&](std::uint64_t fallback) {
      return i + 1 < argc ? std::stoull(argv[i + 1]) : fallback;
    };
    if (arg == "-n") {
      request_count = value(request_count);
      ++i;
    } else if (arg == "--seed") {
      seed = value(seed);
      ++i;
    } else if (arg == "--shards") {
      shards = value(shards);
      ++i;
    } else if (arg == "--customers") {
      customers = value(customers);
      ++i;
    } else {
      std::cerr << "usage: payments-bench [-n requests] [--seed s] "
                   "[--customers c]\n";
      return 2;
    }
  }

  // One workload for every run: identical traffic, fair comparison.
  payments::MockStream stream(payments::StreamConfig{
      .customer_count = customers, .requests_per_second = 0, .seed = seed});
  std::vector<payments::PaymentRequest> workload;
  workload.reserve(request_count);
  for (std::uint64_t i = 0; i < request_count; ++i) {
    workload.push_back(stream.next());
  }

  std::cout << "workload: " << request_count << " requests, " << customers
            << " customers, shards=" << shards << ", seed=" << seed
            << "\n\n";

  std::cout << std::left << "threads" << std::right << std::setw(14)
            << "req/s" << std::setw(12) << "mean_us" << std::setw(10)
            << "p50_us" << std::setw(10) << "p95_us" << std::setw(10)
            << "p99_us" << std::setw(12) << "max_us" << std::setw(11)
            << "approved" << "  " << std::setw(9) << "rejected\n";
  std::cout << std::string(110, '-') << "\n";

  for (const std::size_t threads : thread_counts) {
    if (threads > std::thread::hardware_concurrency()) continue;

    std::vector<payments::Customer> bench_customers;
    bench_customers.reserve(customers);
    for (std::size_t i = 0; i < customers; ++i) {
      bench_customers.push_back({"cust_" + std::to_string(i),
                                 "Bench " + std::to_string(i), 1'000'000'000});
    }
    std::unique_ptr<payments::AccountStore> store;
    if (shards <= 1) {
      store = std::make_unique<payments::DB>(bench_customers);
    } else {
      store = std::make_unique<payments::ShardedDB>(bench_customers, shards);
    }
    payments::SintaBank sinta{seed};
    payments::RoRy rory{seed};
    payments::PartnerRouter router{rory, sinta};
    payments::MessageSender sender;

    const BenchResult result =
        threads == 1
            ? bench_sequential(workload, *store, rory, router, sender)
            : bench_threaded(threads, workload, *store, rory, router, sender);
    print_row(result);
    sender.close();
    while (sender.take()) {
    }
  }
}
