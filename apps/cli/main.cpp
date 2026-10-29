#include <cctype>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "payments/runtime.hpp"
#include "payments/types.hpp"

namespace {

const char* kBanner = R"(
payments_handler - imaginary payments microservice

commands:
  pay <payer> <payee> <amount> <method>   fund a transfer (method: debit_card,
                                          credit_card, bank_transfer, wallet)
  customers                               list registered customers
  rate <requests-per-second>              restart synthetic traffic at this rate
  stop                                    silence the synthetic traffic
  stats                                   print counters
  quit                                    drain and exit
)";

void print_reply(const payments::PaymentReply& reply) {
  std::cout << "  -> " << nlohmann::json(reply).dump() << "\n";
  std::cout.flush();
}

std::uint64_t next_id() {
  static std::uint64_t counter = 0;
  return ++counter;
}

}  // namespace

int main(int argc, char** argv) {
  std::signal(SIGPIPE, SIG_IGN);

  payments::RuntimeOptions options;

  payments::DB db = payments::make_default_db(10);
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto value = [&](std::uint64_t fallback) {
      return i + 1 < argc ? std::stoull(argv[i + 1]) : fallback;
    };
    if (arg == "--rate") {
      options.synthetic_rate = value(options.synthetic_rate);
      ++i;
    } else if (arg == "--workers") {
      options.worker_count = value(options.worker_count);
      ++i;
    } else if (arg == "--customers") {
      db = payments::make_default_db(value(10));
      ++i;
    } else if (arg == "--seed") {
      options.stream_seed = value(options.stream_seed);
      ++i;
    } else if (arg == "--customers-file") {
      if (i + 1 >= argc) {
        std::cerr << "--customers-file needs a path\n";
        return 2;
      }
      std::ifstream file(argv[i + 1]);
      if (!file) {
        std::cerr << "cannot open " << argv[i + 1] << "\n";
        return 2;
      }
      std::ostringstream buffer;
      buffer << file.rdbuf();
      try {
        db = payments::DB(buffer.str());
      } catch (const std::exception& e) {
        std::cerr << "bad customers file: " << e.what() << "\n";
        return 2;
      }
      ++i;
    } else {
      std::cerr << "unknown arg " << arg << "\n";
      return 2;
    }
  }

  payments::Runtime runtime(std::move(db), options);
  runtime.start();

  std::cout << kBanner << "database: " << runtime.db().size()
            << " customers, workers: " << options.worker_count << "\n";

  std::string line;
  while (std::cout << "payments> ", std::cout.flush(), std::getline(std::cin, line)) {
    std::istringstream in(line);
    std::string command;
    in >> command;
    if (command.empty()) continue;

    if (command == "quit" || command == "exit") {
      break;
    }
    if (command == "customers") {
      for (const auto& id : runtime.db().ids()) {
        const auto customer = runtime.db().find(id).value();
        std::cout << "  " << id << "  " << customer.name << "  "
                  << customer.balance_cents << " cents\n";
      }
      continue;
    }
    if (command == "stats") {
      const auto stats = runtime.stats();
      std::cout << "  processed=" << stats.processed << " approved=" << stats.approved
                << " rejected=" << stats.rejected << " failed=" << stats.failed
                << " sender_pending=" << runtime.pending_replies() << "\n";
      continue;
    }
    if (command == "rate") {
      std::uint64_t rate = 0;
      in >> rate;
      runtime.restart_synthetic(rate);
      std::cout << "  synthetic stream at rate=" << rate << "\n";
      continue;
    }
    if (command == "stop") {
      runtime.restart_synthetic(0);
      std::cout << "  synthetic stream stopped\n";
      continue;
    }
    if (command == "pay") {
      std::string payer, payee, method, amount_raw;
      if (!(in >> payer >> payee >> amount_raw >> method)) {
        std::cout << "  usage: pay <payer> <payee> <amount> <method>\n";
        continue;
      }
      for (auto& ch : method) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      try {
        const auto amount = static_cast<std::int64_t>(std::stoll(amount_raw)) * 100;
        auto reply = runtime.submit(payments::PaymentRequest{
            .request_id = "manual-" + std::to_string(next_id()),
            .amount_cents = amount,
            .currency = "INR",
            .payer = payer,
            .payee = payee,
            .method =
                payments::payment_method_from_string(method),
        });
        print_reply(reply);
      } catch (const std::exception& e) {
        std::cout << "  bad request: " << e.what() << "\n";
      }
      continue;
    }
    std::cout << "  unknown command: " << command << "\n";
  }

  runtime.stop();
  const auto start = runtime.stats();
  std::cout << "bye. processed=" << start.processed << " approved=" << start.approved
            << " rejected=" << start.rejected << " failed=" << start.failed << "\n";
}
