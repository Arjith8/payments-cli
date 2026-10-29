# payments_handler

A C++20 payments microservice: ingest payment requests, validate the payer and
payee against a customer registry, execute the transaction through simulated
banking partners, and emit confirmations into an outbound message queue —
with benchmarked single-threaded vs multi-threaded throughput and latency
statistics.

## Features

- **Ingestion stream** — deterministic, seeded synthetic traffic generator
  (jittered rate limiting, weighted payment-method mix, cooperative stop),
  feeding a thread-safe blocking queue
- **Account store** — mutex-guarded registry with balance validation,
  atomic debit/credit; monolithic or hash-sharded with a one-line switch
- **Banking partners** — `IBankingPartner` interface with `SintaBank` /
  `RoRy` policy implementations (method restrictions, amount ceilings,
  failure rates) and a method-aware router
- **Transaction engine** — validate → partner confirmation → settle; every
  request produces a JSON reply posted to the outbound sender queue
- **Interactive CLI** — fund transfers from the terminal while a detached
  thread keeps synthetic traffic flowing
- **Benchmarks** — identical seeded workload across worker counts and shard
  counts; mean/p50/p95/p99/max latency and req/s per configuration
- **Unit tested** — 8 doctest suites (one per component, including
  concurrency), wired into `ctest`

## Architecture

```
                       ┌────────────────────────────────────────────────┐
 synthetic traffic ──▶ │ BlockingQueue<PaymentRequest> ─▶ workers (K)   │
 (MockStream, detached)│                                      │         │
                       │                          AccountStore │ DB /    │
                       │                          exists,     │ ShardedDB
   manual `pay` ──────▶│  submit() ──▶ processor  try_debit   │         │
                       │                │         credit      ▼         │
                       │                ▼                               │
                       │        PartnerRouter ─▶ SintaBank / RoRy       │
                       │                │ confirmations                 │
                       │                ▼                               │
                       │        MessageSender (outbound JSON replies)   │
                       └────────────────────────────────────────────────┘
```

Key submodules, in dependency order:

| Module | Role |
|---|---|
| `types.hpp` | `PaymentRequest` model + JSON wire (de)serialisation |
| `queue.hpp` | generic thread-safe `BlockingQueue<T>` (blocking pop, close-and-drain) |
| `stream.hpp` | `IRequestSource` + `MockStream` generator |
| `store.hpp`, `db.hpp` | `AccountStore` interface + mono `DB` (validated registry, atomic `try_debit`) |
| `sharded.hpp` | `ShardedDB`: stable hash partitioning over per-shard `DB`s |
| `partner.hpp`, `banks.hpp` | partner interface, seeded policy partners, `PartnerRouter` |
| `reply.hpp`, `sender.hpp` | reply model + outbound `MessageSender` queue |
| `processor.hpp` | `PaymentProcessor`: the full request flow |
| `runtime.hpp` | assembles the pipeline and owns its threads |

The processor depends only on the `AccountStore` and `IBankingPartner`
interfaces — monolithic vs sharded storage (or a future network-backed
store) is purely a construction-time choice.

## Requirements

- CMake ≥ 3.20
- A C++20 compiler (GCC 13+, tested with GCC 16)
- Network access on first configure (FetchContent downloads two headers-only
  libraries; nothing else)

## Quick start

```bash
git clone https://github.com/Arjith8/payments-cli.git && cd payment_handler

# build (defaults to Release)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# run the unit test suite (8 suites)
ctest --test-dir build --output-on-failure
```

### Interactive CLI

```
./build/payments-cli --workers 4 --rate 50           # 4 workers, 50 synthetic req/s
./build/payments-cli --customers 25 --seed 42        # bigger registry, replayable stream
./build/payments-cli --shards 8 --workers 8          # partitioned store
```

Flags: `--workers`, `--rate`, `--customers`, `--shards`, `--seed`,
`--customers-file <path>` (JSON array of customers).

Interactive commands:

| Command | Effect |
|---|---|
| `pay <payer> <payee> <amount> <method>` | funds a transfer synchronously and prints the JSON reply |
| `customers` | registry with balances |
| `rate <rps>` | turns the synthetic stream on at that rate (0 = off) |
| `stats` | processed / approved / rejected / failed counters |
| `quit` | drains the pipeline and exits |

Synthetic requests post their JSON replies continuously; e.g.

```json
{"confirm_ref":"RoRy-1-ff7f9ed8fe","partner":"RoRy","reason":"","request_id":"manual-1","status":"approved"}
```

## Benchmark

`payments-bench` drives the *same* seeded workload through either a
single-threaded loop or K competing worker threads. The numbers below are
from the **Debug (unoptimized) build** — compare Debug builds to isolate the
concurrency story from the optimizer's:

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j
./build-debug/payments-bench -n 20000 --shards 8
```

Results (20 000 requests, 12 customers, seed 42, 8-core machine):

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

Reading it (approve/reject counts are identical in every run — deterministic
workload — so the comparison is apples-to-apples):

- **1 shard**: throughput flat while latency multiplies with worker count;
  a store-wide mutex serialises the hot path, so threading adds contention,
  not capacity (mean latency 41 µs → 304 µs at 8 threads).
- **8 shards**: 8 workers sustain **38.5k req/s (+63% over 1 thread)** and
  mean latency drops from 304 µs to 154 µs — accounts hash across
  independent per-shard mutexes, so requests stop contending. The remaining
  ceiling is the per-partner RNG lock.

## Testing

```bash
ctest --test-dir [build dir] --output-on-failure   # or run each ./build-debug/test_* binary
```

Eight suites: stream (`deterministic replay, rate/mix, stop`), queue
(FIFO, blocking, close semantics), types (JSON round-trips), DB (validation,
balance edges, concurrent debit), banks (policies, seeded determinism,
routing), processor (full flow, every failure branch, 4-thread ledger
consistency), runtime (counters, synthetic on/off), sharded (routing
stability, cross-shard balance math, drop-in).

## Dependencies

Deliberately minimal — two header-only libraries fetched by CMake
`FetchContent` at configure time:

| Dependency | Purpose |
|---|---|
| [nlohmann/json](https://github.com/nlohmann/json) 3.11.3 | JSON wire format |
| [doctest](https://github.com/doctest/doctest) 2.4.12 | Unit test framework |

## Layout

```
src/payments/   core library (store, stream, banks, processor, runtime)
apps/cli/       payments-cli
apps/bench/     payments-bench
tests/          doctest suites, one per component
```
