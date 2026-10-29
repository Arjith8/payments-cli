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
- [ ] Phase 3 — Banking partners (`SintaBank`, `RoRy`, ...) with confirmation
- [ ] Phase 4 — Transaction engine + message-sender queue (interface only)
- [ ] Phase 5 — `payments-cli`: interactive transactions + synthetic traffic
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

## Layout

```
src/payments/   core library headers/sources
apps/cli/       payments-cli (later phases)
tests/          doctest unit tests, one suite per component
```
