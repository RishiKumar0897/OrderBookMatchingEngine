# Implementation Workflow

Companion to ARCHITECTURE.md. That doc says *what* each phase delivers — this says *what to actually type, in what order*, so you never sit down without knowing the next concrete step.

General rule used throughout: **write the smallest piece, write its test, watch it pass, then move to the next piece.** Don't write `order_book.h` end to end and test at the end — you'll spend a day debugging which of five methods broke.

---

## Phase 0 — Build skeleton (do this first, don't skip)

1. `CMakeLists.txt` — C++20, one executable target, `FetchContent` for GoogleTest.
2. `tests/test_smoke.cpp` — one trivial `EXPECT_EQ(1, 1)` test.
3. Build, run the test, confirm the pipeline works (`cmake --build . && ctest`).

Do not write any engine code until this passes. If your test harness is broken, every phase after this gets harder to debug.

---

## Phase 1 — Single-symbol book, in build order

**1. `include/order.h`**
Define the `Order` struct: `order_id`, `symbol_id`, `side`, `price`, `qty`, `remaining_qty`, `timestamp`, plus `next`/`prev` raw pointers (for the intrusive list in step 3). Pure data, no methods needed beyond maybe a constructor.

**2. `include/order_pool.h`** — implement before anything else touches memory
- Preallocated `std::vector<Order>` of fixed capacity, plus a free list (stack of indices or pointers).
- `Order* acquire()` — pop from free list, or fail loudly (don't silently grow — that defeats the point).
- `void release(Order*)` — push back onto free list.
- **Test now, before moving on:** acquire N orders, release them, reacquire, confirm no order is handed out twice and fields aren't stale.

**3. `include/price_level.h`**
- `PriceLevel` holds `price`, `total_qty`, and an intrusive doubly-linked list (head/tail `Order*`).
- Methods: `push_back(Order*)`, `remove(Order*)` (O(1), since you have the node pointer), `front()`.
- **Test now:** push 3 orders, confirm FIFO order on `front()`/iteration; remove the middle one, confirm the list re-links correctly and `total_qty` updates.

**4. `include/order_book.h` + `src/order_book.cpp`** — build the methods in this exact order, testing after each:

  a. **Book storage.** Start with `std::map<Price, PriceLevel>` for bids and asks (descending/ascending comparators). Yes, this isn't the fast final structure — that optimization is explicitly deferred to a later pass (see note at the end of this phase). Correctness first.
  b. **`add_limit_order()` with no matching yet** — just inserts into the correct side/price level. Test: add several orders across price levels, confirm book state (best bid/ask, level ordering) is what you expect.
  c. **Matching logic** — `match_order()`: given an incoming order, walk the opposite side from best price while prices cross, generate `Fill` structs, decrement `remaining_qty`, remove fully-filled orders (return them to the pool). This is the method to spend the most test-writing effort on. Test cases specifically:
     - Full fill against one resting order
     - Partial fill (incoming order larger than resting order, and the reverse)
     - Fill across multiple price levels in one incoming order
     - No cross (incoming order rests instead of matching)
  d. **`add_market_order()`** — same as limit but skips the "does it cross" check and matches until filled or book side is empty. Test: market order against a thin book (partially unfilled — decide and test your policy here: reject remainder, or let it rest at last price, or drop it).
  e. **`cancel_order(order_id)`** — needs the `order_id -> {side, price, Order*}` hash map (add this alongside step (a)). Remove from `PriceLevel`, return to pool. Test: cancel a resting order, confirm book state and that a later match doesn't see it.
  f. **`modify_order(order_id, new_price, new_qty)`** — simplest correct implementation is cancel + re-add (which correctly loses time priority — that matches real exchange behavior for price/qty-increase modifies). Test: modify, confirm new time priority position. Note this as a known simplification in your README; a stricter implementation preserves priority on quantity-decrease-only modifies, which you can add later if you want the extra realism.

**5. `src/main.cpp`** — minimal harness: construct one `OrderBook`, feed a hardcoded sequence of orders inline in code, print resulting fills and final book state. This is a sanity-check step, not your test suite — eyeball it once to catch anything your unit tests didn't, then move on.

**End-of-phase checkpoint:** all of the above have passing tests, `main.cpp` runs and produces sane output. Do not proceed to Phase 2 until matching logic (4c) is genuinely solid — bugs here compound badly once threading and multi-symbol are layered on top.

---

## Phase 2 — Latency instrumentation, in build order

1. `include/latency_recorder.h` — wrapper around `__rdtsc()`, a calibration routine against `std::chrono::steady_clock` (sample both at startup, compute cycles-per-nanosecond), and a preallocated `std::vector<uint64_t>` for raw samples (reserve capacity up front — no reallocation during measurement).
2. Add ingestion-timestamp and match-completion-timestamp capture points inside `add_limit_order`/`match_order` from Phase 1 — this is a small, surgical edit, not a rewrite.
3. `src/ingestion/synthetic_generator.cpp` (+ header) — Poisson arrival process (use `std::exponential_distribution` for inter-arrival times), configurable order size/price distributions and cancel rate.
4. Wire the generator into `main.cpp`, run a batch, dump latency samples to a flat binary file at the end.
5. Quick Python one-liner (not a full notebook yet) to load the file with `np.fromfile` and print p50/p99/p99.9/max — this is your first real number. Write it down somewhere; you'll compare against it in Phase 5.

---

## Phase 3 — Multi-symbol, in build order

1. `include/symbol_router.h` — intern symbol strings to integer IDs at startup (a simple `std::unordered_map<std::string, int>` built once, not per-event), expose `vector<OrderBook>` indexed by that ID.
2. Refactor `main.cpp` to construct N books from a configured symbol list instead of one.
3. Update the synthetic generator to tag events with a symbol ID (round-robin or weighted across your symbol list is fine).
4. Re-run the Phase 2 latency measurement across N symbols on the same single thread — record this number as your "single-thread ceiling" baseline before Phase 5.

---

## Phase 4 — LOBSTER integration, in build order

1. `include/ingestion/event_source.h` — define the shared interface (e.g. `bool next(OrderEvent&)`) both sources will implement.
2. Retrofit `synthetic_generator` to implement this interface (small change if Phase 1–3 kept the event type consistent).
3. `src/ingestion/lobster_parser.cpp` (+ header) — parse LOBSTER's message file format into the same `OrderEvent` struct.
4. Download a LOBSTER sample file, replay it through your Phase 1 engine unmodified (this is the payoff of keeping the interface shared).
5. Write a small validation script comparing your engine's book state at given timestamps against LOBSTER's provided order-book snapshot file. Document any mismatches and root-cause them — this is genuinely the most valuable debugging you'll do in the whole project.

---

## Phase 5 — Sharded multi-threading, in build order

1. Write or vendor a lock-free SPSC queue (a ring buffer with atomic head/tail is enough — don't reach for a general lock-free MPMC structure, you don't need one here).
2. Spawn one ingestion thread (feeds events, pushes to per-shard queues) and N matching threads (each owns a disjoint subset of symbols, pinned via `pthread_setaffinity_np`).
3. Each matching thread's loop: pop from its queue, call into its owned `OrderBook`s — this reuses Phase 1–3 code untouched, which is the point of the earlier isolation.
4. Re-run the Phase 3 latency/throughput benchmark and compare directly against the single-thread baseline you recorded.

---

## Phase 6 — Logging + visualization, in build order

1. Define fixed-width `Fill`/`OrderEvent`/latency-sample structs with explicit types (`uint64_t`, `int32_t`, etc. — avoid anything platform-size-ambiguous).
2. `include/binary_logger.h` + `.cpp` — append-only writer, buffered (don't `write()` per event).
3. `python/parse_log.py` — NumPy `dtype` mirroring the C++ struct layout exactly (watch padding/alignment — consider `#pragma pack` or manually pad the struct to avoid mismatches).
4. Notebooks, in order: trade tape + price chart first (simplest), then depth ladder animation, then latency histograms (you already have the loading code from Phase 2's one-liner).

---

## What this workflow deliberately defers

- **Book storage optimization** (flat array near the touch instead of `std::map`) — do this as a dedicated optimization pass *after* Phase 1 tests are all green, with a benchmark before/after so you can quantify the improvement. Optimizing before you have correctness or a benchmark to compare against is wasted effort.
- **Strict modify-preserves-priority semantics** — cancel+re-add is correct enough to start; revisit only if you want the extra realism.
- Anything not listed above that shows up as a "nice to have" mid-phase — write it on a running TODO list and finish the current phase first.
