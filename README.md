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

## Layout

```
src/payments/   core library headers/sources
apps/cli/       payments-cli (later phases)
tests/          doctest unit tests, one suite per component
```
