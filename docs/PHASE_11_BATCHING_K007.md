# Phase 11 Batching Experiments (K-007)

Status: measured on the Pi, slice K-007, base
3378fe0c59ec36cd19bf0d0f51c3918a2b35c51c (Wave 11, 5 of 9
banked at authoring). Shipped build flavour is **-O2**
(CFLAGS, per K-004); the harness TU is also compiled -O2
(bench_k002 precedent). This is a MEASUREMENT slice: no
src, API, ABI or shipped-flag change. Evidence:
`tests/perf/recorded_run_k007.txt` (raw + summary);
harness `tests/perf/bench_k007.c`, runner
`tests/perf/run_bench_k007.sh`, summariser
`tests/perf/summarize_k007.py`; standalone via
`make bench-k007` (NOT part of `make test`).

## 1. Question and design

Per `docs/PHASE_11_FOUNDATION_PLAN.md` S5 (K-001): batched
vs unbatched under the K-001 methodology contract, loss
cases recorded honestly, fragmentation / return-to-OS
legs building on Phase 9 evidence. Batch shapes come
from the K-003 trace catalogue (T-02 live-set + mass
free; compiler phase-bursts, S3): burst allocation
followed by burst free is the shape real callers produce
at compilation-unit end.

Patterns, at matched totals per leg (every leg performs
the same number of allocations, full-block memsets and
frees; only the grouping differs):

| Pattern | Shape |
|---|---|
| inter1 | alloc / touch / free, one at a time (batch 1, the unbatched baseline) |
| burst8/32/128/512 | alloc B blocks, touch all, free all (LIFO order) |
| burst512fifo | batch 512, free in allocation (FIFO) order |

Sizes 64 B (small-object path) and 1024 B. Totals:
120,000 alloc/free pairs per 64 B leg, 30,000 per
1024 B leg. Control leg: the identical harness built
against glibc malloc/free (-DLEG_GLIBC).

## 2. Method (K-001 contract)

- M-1/M-2: 7 repetitions per leg, each repetition a
  fresh process per leg (one leg per process, argv key),
  glibc and MimirAlloc alternating; medians reported
  with min/max and every raw repetition in the recorded
  run. Timing is a measurement, never an oracle.
- M-6: untimed warm-up pass at 1/10 scale per process.
- Whole-pass CLOCK_MONOTONIC; ns/op = pass time / pairs.
- Equivalence: the checksum depends only on the global
  op sequence. All patterns at one size print identical
  ck on both allocators (64 B: 3900000; 1024 B: 974616)
  — the workloads are the same work, differently grouped.
- Footprint per leg: minor-fault delta and peak-RSS
  (ru_maxrss) delta over warm-up + timed pass, per-leg
  processes so neither is contaminated by other legs.
- Environment (recorded run header): kernel
  6.12.112-v8-16k-hailo+, 16 KiB pages, governor
  ondemand, 57,300 mC at run start, 13 GiB available,
  Samtal HTTP 200 before and after.

## 3. Results (medians of 7, ns per alloc/free pair)

MimirAlloc, 64 B (interleaved baseline 93 ns/op):

| Pattern | ns/op | vs inter1 | minflt | RSS delta |
|---|---|---|---|---|
| inter1 | 93 | 1.00x | 3 | 16 KiB |
| burst8 | 102 | 1.10x | 3 | 16 KiB |
| burst32 | 153 | 1.65x | 3 | 16 KiB |
| burst128 | 360 | 3.87x | 3 | 16 KiB |
| burst512 | 652 | 7.01x | 261 | 16 KiB |
| burst512fifo | 654 | 7.03x | 261 | 16 KiB |

MimirAlloc, 1024 B (baseline 129 ns/op):

| Pattern | ns/op | vs inter1 | minflt | RSS delta |
|---|---|---|---|---|
| inter1 | 129 | 1.00x | 3 | 16 KiB |
| burst8 | 140 | 1.09x | 3 | 16 KiB |
| burst32 | 313 | 2.43x | 1034 | 16 KiB |
| burst128 | 425 | 3.29x | 1807 | 0 KiB |
| burst512 | 527 | 4.09x | 2130 | 512 KiB |
| burst512fifo | 847 | 6.57x | 2130 | 528 KiB |

glibc control, same workloads:

| Size | inter1 | burst8 | burst32 | burst128 | burst512 | fifo512 |
|---|---|---|---|---|---|---|
| 64 B | 26 | 29 | 33 | 33 | 33 | 33 |
| 1024 B | 49 | 55 | 84 | 103 | 114 | 140 |

Distributions are tight (e.g. MimirAlloc burst32/64 B:
152..155 across all 7 reps; full raw rows in the
recorded run).

## 4. Loss cases (recorded, not hidden)

- **L-1 Batching always loses on MimirAlloc.** Every
  burst pattern is slower than interleaved at both
  sizes, monotonically in batch size: up to 7.01x at
  64 B / batch 512 and 4.09x at 1024 B / batch 512.
  There is no batch size at which caller-side batching
  wins on this design. Verdict: **no batching win
  exists for caller-side batching** — a measured
  negative result, complete as it stands.
- **L-2 Batching also loses on glibc, less steeply**
  (1024 B: 49 -> 114 ns/op, 2.33x; 64 B: 1.27x). Part
  of the effect is generic: interleaved alloc/free
  reuses one hot block, while a burst walks cold
  blocks, spans and pages. MimirAlloc amplifies the
  generic loss by ~3x at 64 B (7.01x vs 1.27x total).
- **L-3 Burst legs re-fault pages on MimirAlloc.**
  Minor faults per pass: interleaved 3 at both sizes;
  burst512 261 (64 B) and 2130 (1024 B); burst32 at
  1024 B already 1034. glibc pays 5..35 on the same
  LIFO legs. This is the Phase 9 return-to-OS behaviour
  seen from the caller side: spans/pages released on
  free are faulted back in on the next burst cycle, so
  a burst pays full fault cost every cycle instead of
  amortising it. (glibc's FIFO 1024 B leg also faults,
  1553 — free order matters there too.)
- **L-4 FIFO free order is a further loss at 1024 B.**
  MimirAlloc 847 vs 527 ns/op LIFO (+61%); glibc 140
  vs 114 (+23%). At 64 B FIFO and LIFO are indistinguishable
  (654 vs 652). No future batch-free facility may
  promise order-independence at larger sizes.
- **L-5 Peak RSS grows with batch, as constructed.**
  At batch 512 / 1024 B the live set is 512 KiB by
  construction; measured RSS delta 512 KiB (MimirAlloc)
  / 528 KiB (glibc). Batching trades footprint for
  nothing here: the larger live set buys no speed (L-1).

## 5. Interpretation and proposal (not implemented)

The interleaved baseline wins because reuse is
immediate and hot: one block, one span, pages resident.
A burst defeats every one of those: it forces span
walk / fresh-span setup across many blocks and, on
this design, re-faults released pages each cycle (L-3).
An allocator-internal batching optimisation would
therefore have to attack L-3, not the call pattern:

- **Proposal P-1 (recorded only):** span retention
  across burst frees — defer unmap/return of a span
  emptied by a burst so the next burst reuses resident
  pages. Motivation: the 261 / 2130 fault deltas and
  the 3.87x..7.01x burst penalties above. Any such
  change must be measured under this same contract,
  one change at a time (K-001 M-11), and weighed
  against the Phase 9 return-to-OS evidence that
  motivated releasing spans. NOT implemented here.
- **Rejected by evidence:** a caller-facing batch
  alloc/free API. L-1 shows the call pattern itself is
  not where time goes; batching the calls cannot win
  while L-3 stands.

## 6. INC register

- INC-K007-01: burst re-faulting (L-3) — spans/pages
  released on free are faulted back per burst cycle
  (minflt 261 @ 512x64 B, 2130 @ 512x1024 B vs 3
  interleaved). Proposal P-1 records the candidate
  direction; a fix is owed to a future slice, not
  this measurement slice.
- INC-K007-02: FIFO free-order penalty at 1024 B
  (+61% MimirAlloc, +23% glibc vs LIFO). Any future
  batch-free design must state free-order behaviour
  explicitly; order-independence is disproven here.
- INC-K007-03: whole-pass medians only; no per-op
  p95/p99 distribution (the K-001 M-2 gap carried by
  K-002/K-005). K-008 owns tail latency for Wave 11.

## 7. Verification (this worktree, on the Pi)

- `make bench-k007` exit 0; raw 168 lines = 7 reps x
  12 legs x 2 allocators, as expected.
- `make test` exit 0 — CHECKS: 4590 run, 0 failed.
- `nm -D --defined-only build/libmimiralloc.so` =
  exactly 4 (mimir_malloc, mimir_free, mimir_calloc,
  mimir_realloc).
- README.md and include/mimir_alloc.h byte-identical
  to base (empty git diff); Makefile delta is the
  additive bench-k007 block only.
- Samtal HTTP 200 before and after; free 13 GiB
  available throughout. Every number in S3/S4 appears
  in `tests/perf/recorded_run_k007.txt`.
