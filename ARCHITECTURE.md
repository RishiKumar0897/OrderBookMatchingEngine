# Multi-Symbol Order Book Matching Engine — Architecture & Phased Workflow

## Goals
- Learn market microstructure through hands-on implementation
- Practice low-latency C++ (allocator design, cache-conscious layout, lock-free concurrency)
- Produce a portfolio piece demonstrating skills relevant to HFT/quant infra roles
- Multi-symbol, scalable to arbitrary feed size, config-switchable synthetic/real data
- Python/Jupyter front end for visualization and driving the engine

---

## System Architecture

```
                          ┌─────────────────────────────────────┐
                          │           Ingestion Thread            │
                          │  (synthetic generator OR LOBSTER      │
                          │   file parser — config-switchable)    │
                          └───────────────┬───────────────────────┘
                                          │ OrderEvent (add/cancel/modify)
                                          ▼
                          ┌─────────────────────────────────────┐
                          │         Symbol Router / Dispatcher    │
                          │  symbol -> shard_id (hash or static)  │
                          └───────┬───────────────┬───────────────┘
                                  │ SPSC queue    │ SPSC queue
                                  ▼               ▼
                     ┌─────────────────┐  ┌─────────────────┐
                     │  Shard Thread 0  │  │  Shard Thread 1  │  ... N shards
                     │  ┌────────────┐  │  │  ┌────────────┐  │
                     │  │ OrderBook  │  │  │  │ OrderBook  │  │
                     │  │  AAPL      │  │  │  │  GOOG      │  │
                     │  ├────────────┤  │  │  ├────────────┤  │
                     │  │ OrderBook  │  │  │  │ OrderBook  │  │
                     │  │  MSFT      │  │  │  │  AMZN      │  │
                     │  └────────────┘  │  │  └────────────┘  │
                     └────────┬─────────┘  └────────┬─────────┘
                              │ MatchEvent + latency sample     │
                              ▼                                  ▼
                          ┌─────────────────────────────────────┐
                          │       Binary Logger (append-only)     │
                          │  events.bin  latency_samples.bin      │
                          └───────────────┬───────────────────────┘
                                          │ np.fromfile
                                          ▼
                          ┌─────────────────────────────────────┐
                          │     Jupyter / Python Front End        │
                          │  - depth ladder animation              │
                          │  - trade tape + price chart             │
                          │  - latency percentile histograms        │
                          └─────────────────────────────────────┘
```

### Core components

| Component | Responsibility | Key constraint |
|---|---|---|
| `OrderEvent` | Common struct for add/cancel/modify/trade, tagged with symbol + timestamp | POD, no heap allocation |
| `SymbolRouter` | Maps symbol → shard/book index | O(1), resolved once at ingestion, not per-event lookup by string |
| `OrderBook` | One per symbol; bid/ask price levels, order queues | No allocation in hot path |
| `PriceLevel` | Price + intrusive FIFO list of resting orders | O(1) insert/remove given a node pointer |
| `OrderPool` | Preallocated arena of order nodes | Fixed capacity, recycled via free list |
| `MatchingEngine` | Owns a shard's books, applies events, emits fills | Pinned to a core, no syscalls in loop |
| `LatencyRecorder` | rdtsc-based timestamp capture at ingestion + at match | Preallocated sample buffer, dumped at end |
| `BinaryLogger` | Serializes events/fills/samples to disk | Fixed-width structs, no text formatting in hot path |
| `Python front end` | Parses binary logs with NumPy, renders visualizations | Fully decoupled from the engine — offline consumer |

### Directory structure (suggested)

```
matching-engine/
├── CMakeLists.txt
├── include/
│   ├── order.h                     # POD, header-only
│   ├── price_level.h               # hot-path, header-only (inline for compiler)
│   ├── order_pool.h                # hot-path, header-only (inline for compiler)
│   ├── symbol_router.h             # hot-path, header-only (inline for compiler)
│   ├── latency_recorder.h          # hot-path, header-only (inline for compiler)
│   ├── order_book.h                # declarations only, impl in src/
│   ├── matching_engine.h           # declarations only, impl in src/
│   ├── binary_logger.h             # declarations only, impl in src/
│   └── ingestion/
│       ├── event_source.h          # IEventSource interface, both sources implement this
│       ├── synthetic_generator.h
│       └── lobster_parser.h
├── src/
│   ├── order_book.cpp
│   ├── matching_engine.cpp
│   ├── binary_logger.cpp
│   ├── ingestion/
│   │   ├── synthetic_generator.cpp
│   │   └── lobster_parser.cpp
│   └── main.cpp
├── tests/
│   ├── test_order_book.cpp        # correctness (Catch2 or GoogleTest)
│   └── test_lobster_validation.cpp
├── bench/
│   └── latency_bench.cpp
├── python/
│   ├── parse_log.py                # struct/dtype definitions matching C++ layout
│   ├── depth_ladder.ipynb
│   ├── trade_tape.ipynb
│   └── latency_histograms.ipynb
└── data/
    └── lobster_samples/
```

---

## Phased Workflow

### Phase 0 — Foundations (setup, ~few days)
- CMake build, unit test framework (GoogleTest or Catch2) wired in from day one — you want tests in place before matching logic gets complex.
- Define `Order`, `OrderEvent`, `Fill` POD structs.
- **Deliverable:** builds, runs empty `main`, tests execute (even if trivial).

### Phase 1 — Single-symbol, single-threaded, correctness first
- Implement `PriceLevel` (intrusive FIFO), `OrderBook` (price-time priority), `OrderPool` (arena allocator).
- Support: add limit, add market, cancel, modify.
- Unit tests: known sequences of orders → known resulting book state and fills. Test edge cases (crossing spread, partial fills, cancel of already-filled order, price-time tie-breaking).
- **Deliverable:** a correct single-symbol book, verified by unit tests, no performance work yet.

### Phase 2 — Latency instrumentation
- Add `LatencyRecorder`: rdtsc capture at event ingestion and at match completion, TSC calibration against `steady_clock`.
- Preallocated sample buffer, dumped to file at shutdown — no I/O during measurement.
- Pin thread to core (`pthread_setaffinity_np`), disable turbo/frequency scaling if your environment allows it (may be limited in Docker — note this as a caveat in your writeup).
- Synthetic load generator with configurable Poisson arrival rate.
- **Deliverable:** p50/p99/p99.9/max tick-to-trade latency numbers for single-symbol synthetic load. This is your first real benchmark result.

### Phase 3 — Multi-symbol (single-threaded)
- Add `SymbolRouter`: intern symbols to integer IDs at startup, `std::vector<OrderBook>` indexed by ID.
- Preallocate all books for a configured symbol universe.
- Re-run latency benchmark across N symbols on one thread to establish a throughput ceiling baseline (this number motivates Phase 5).
- **Deliverable:** engine handles a configurable symbol list, still single-threaded.

### Phase 4 — Real data: LOBSTER integration
- Write a LOBSTER message-file parser producing the same `OrderEvent` stream as the synthetic generator (shared interface — this is why Phase 1's event abstraction matters).
- Validate: replay LOBSTER's message file through your engine, compare your reconstructed book state against LOBSTER's provided order-book snapshot file at matching timestamps. This is your correctness gold standard beyond unit tests.
- **Deliverable:** engine reconstructs real historical book state correctly; documented validation methodology (useful to describe in interviews).

### Phase 5 — Sharded multi-threading
- Dedicated ingestion thread parses/generates events, hands off via lock-free SPSC queue per shard.
- N matching threads, each owning a disjoint subset of symbols — no cross-thread locking needed since books don't share state.
- Re-benchmark latency and aggregate throughput (events/sec) vs. Phase 3's single-threaded baseline — quantify the scaling improvement.
- **Deliverable:** documented before/after scaling numbers. This is your strongest "systems engineering" story for interviews.

### Phase 6 — Binary logging + Python/Jupyter visualization
- Define fixed-width structs for events/fills/latency samples; write `BinaryLogger`.
- Python: `struct`/NumPy `dtype` mirroring the C++ layout, parse via `np.fromfile`.
- Notebooks: depth ladder animation (matplotlib `FuncAnimation` or Plotly), trade tape + price chart, latency percentile histograms (log-scale x-axis).
- **Deliverable:** a run of the engine (synthetic or LOBSTER-replayed) fully visualizable end-to-end in Jupyter.

### Phase 7 — Polish / stretch (optional, time-permitting)
- README with architecture diagram, benchmark results, validation methodology — this doc is effectively your first draft.
- Config file (YAML/JSON) for symbol universe, thread count, synthetic load parameters.
- Optional: live streaming visualization via shared memory or a Unix domain socket instead of offline log replay.

---

## Header convention

Using `.h` throughout (no `.hpp`), split by whether the code sits on the per-event hot path:

| File | Style | Why |
|---|---|---|
| `order.h` | header-only | POD struct, nothing to implement |
| `price_level.h`, `order_pool.h`, `symbol_router.h`, `latency_recorder.h` | header-only, fully inline | Called per-event in the matching hot path. Implementation must be visible at the call site for the compiler to inline it — splitting into a `.cpp` blocks inlining across translation units (short of LTO) and directly costs you the latency you're trying to optimize for. |
| `order_book.h`, `matching_engine.h`, `binary_logger.h` | declare in `.h`, implement in `.cpp` | Larger, not called at per-event granularity in a way that needs cross-TU inlining; splitting keeps compile times sane and headers readable. |
| `ingestion/event_source.h` | interface only (pure virtual or CRTP), both `synthetic_generator` and `lobster_parser` implement it | Ingestion is not hot-path — it runs on a dedicated thread feeding queues — so the declare/implement split is fine here, and the shared interface is what makes Phase 4 (LOBSTER) a drop-in replacement for the synthetic source rather than a rewrite. |

Rule of thumb going forward: if a function gets called once per order event inside a matching thread, it belongs in a header, fully visible for inlining. If it's setup, ingestion, logging, or anything off the hot path, declare it in a `.h` and implement it in a `.cpp`.

## Notes
- Do not build multi-threading and multi-symbol correctness simultaneously — Phase 3 (multi-symbol, single-threaded) isolates symbol-routing bugs from concurrency bugs before Phase 5 introduces threading.
- Keep the `OrderEvent` interface identical across synthetic and LOBSTER sources from Phase 1 onward — this is what makes Phase 4 a drop-in rather than a rewrite.
- Docker note: CPU pinning and frequency-scaling control may be limited inside your dev container depending on host configuration — if so, note this as a known limitation in your latency writeup rather than presenting numbers as bare-metal-equivalent.
