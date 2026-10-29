# payments_handler

A C++20 payments microservice: ingest payment requests, validate payers against
a customer database, execute transactions through imaginary banking partners,
and emit confirmations into an outbound message queue — with multi/single-
threaded performance statistics.

## Status / Roadmap

- [x] **Phase 1 — Mock stream creator** (`src/payments/`)
  - `types.hpp` — `PaymentRequest` model + JSON wire (de)serialisation
  - `queue.hpp` — generic thread-safe `BlockingQueue<T>`
  - `stream.hpp` — `IRequestSource` interface + `MockStream` synthetic
    traffic generator (deterministic seeds, jittered rate, weighted method
    mix, cooperative stop, detached-thread producer via `start_async`)
- [ ] Phase 2 — Customer `DB` (valid payer/receiver lookup)
- [x] Phase 3 — Banking partners with confirmation
      (`partner.hpp`, `banks.hpp`: `IBankingPartner`, `BankingPartner` base,
      `SintaBank` / `RoRy` policies, method-aware `PartnerRouter`)
- [x] Phase 4 — Transaction engine + message sender queue
      (`processor.hpp`: `PaymentProcessor` full request flow;
      `reply.hpp`: statuses + JSON replies; `sender.hpp`: `MessageSender`
      outbound queue, delivery logic intentionally out of scope)
- [x] Phase 5 — `payments-cli`: interactive transactions + synthetic traffic
      (`runtime.hpp`: assembles DB/stream/workers/sender, foreground
      `submit()` runs next to the detached generator; commands: `pay`,
      `customers`, `rate`, `stop`, `stats`, `quit`)
- [ ] Phase 6 — Threaded worker pool vs single-threaded flow throughput/latency stats

- [x] Phase 6 — Single vs multi-threaded throughput/latency stats
      (`payments-bench`: identical seeded workload per run, per-request
      latency percentiles, ledger consistency)

## Dependencies

Deliberately minimal — two vendored third-party libraries fetched by CMake
`FetchContent` at configure time:

| Dependency | Purpose |
|---|---|
| [nlohmann/json](https://github.com/nlohmann/json) 3.11.3 | JSON wire format |
| [doctest](https://github.com/doctest/doctest) 2.4.12 | Unit test framework |

## Build & test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## CLI

```bash
./build/payments-cli --workers 4 --rate 50     # 4 workers, 50 synthetic req/s
./build/payments-cli --customers 25 --seed 42  # bigger registry, replayable stream
```

Interactive commands: `pay <payer> <payee> <amount> <method>`,
`customers`, `rate <rps>` (0 = stop stream), `stats`, `quit`.
Synthetic requests post their JSON replies continuously; `quit` drains.

## Benchmark

`payments-bench` drives the *same* seeded workload through either a single
threaded loop or K competing worker threads (Debug/unoptimized build unless
you rebuild with Release):

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -jK
./build-debug/payments-bench -n 20000        # --seed, --customers, -n
```

Example results (Debug build, 20 000 requests, 12 customers, seed 42,
8-core machine):

```
threads         req/s     mean_us    p50_us    p95_us    p99_us      max_us   approved  rejected
--------------------------------------------------------------------------------------------------------------
       1         25803        37.0      33.7      65.6      90.1       331.7      18029       1971
       2         20753        81.2      60.5     189.0     292.3      1262.6      18029       1971
       4         21345       163.1     127.2     397.5     669.4      3108.4      18029       1971
       8         23180       310.1     245.4     751.3    1338.7      5778.7      18029       1971
```

Reading it: approve/reject counts are identical across all runs (deterministic
workload) so the comparison is apples-to-apples. Throughput stays flat while
per-request latency was grows roughly linearly with worker count — the shared
`DB` mutex and the partner lock serialise the hot path, so this unoptimized
build shows contention-limited concurrency, not scaling. Under Release the
absolute numbers shift but the shape of the story is the same.

## Layout

```
src/payments/   core library headers/sources
apps/cli/       payments-cli (later phases)
tests/          doctest unit tests, one suite per component
```
