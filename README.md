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
- [x] Phase 2 — Customer `DB` (valid payer/receiver lookup, balances)
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
- [x] Phase 6 — Single vs multi-threaded throughput/latency stats
      (`payments-bench`: identical seeded workload per run, per-request
      latency percentiles, ledger consistency)
- [x] Phase 7 — Shard-ready account storage
      (`store.hpp`: `AccountStore` interface the processor depends on;
      `sharded.hpp`: `ShardedDB` hash-partitioned over per-shard `DB`s;
      `--shards` in CLI and bench)

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
./build/payments-cli --workers 4 --rate 50           # 4 workers, 50 synthetic req/s
./build/payments-cli --customers 25 --seed 42        # bigger registry, replayable stream
./build/payments-cli --shards 8 --workers 8          # partitioned store
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
./build-debug/payments-bench -n 20000        # --seed, --customers, -n, --shards
```

Example results (Debug build, 20 000 requests, 12 customers, seed 42,
8-core machine):

```
workload: 20000 requests, 12 customers, shards=1, seed=42

threads         req/s     mean_us    p50_us    p95_us    p99_us      max_us   approved  rejected
       1         23237        41.1      35.4      72.5      97.5       306.5      18029       1971
       2         20748        81.3      61.0     188.7     271.2      2924.0      18029       1971
       4         23034       151.5     126.7     346.2     516.9      1582.6      18029       1971
       8         23650       303.8     245.7     717.2    1198.2      4735.6      18029       1971

workload: 20000 requests, 12 customers, shards=8, seed=42

threads         req/s     mean_us    p50_us    p95_us    p99_us      max_us   approved  rejected
       1         24841        38.4      33.2      70.3      96.8       513.2      18029       1971
       2         27126        58.5      42.7     137.2     218.6      2197.8      18029       1971
       4         32650        93.0      63.7     239.1     405.2      3042.7      18029       1971
       8         38532       154.2      94.1     443.0     782.2      4064.6      18029       1971
```

Reading it: approve/reject counts are identical across all runs (deterministic
workload) so the comparison is apples-to-apples.

- **1 shard**: throughput flat while latency multiplies with workers — one
  store-wide mutex serialises the hot path.
- **8 shards**: 8 workers now sustain 38.5k req/s (+63% over 1 thread) and
  mean latency drops from 304µs to 154µs: accounts hash across independent
  per-shard mutexes, so most requests stop contending with each other. The
  remaining ceiling is the partner lock (one shared RNG per partner instance).

`--shards` chooses the partitioning at construction; the processor only sees
the `AccountStore` interface.

## Layout

```
src/payments/   core library headers/sources
apps/cli/       payments-cli
apps/bench/     payments-bench
tests/          doctest unit tests, one suite per component
```
