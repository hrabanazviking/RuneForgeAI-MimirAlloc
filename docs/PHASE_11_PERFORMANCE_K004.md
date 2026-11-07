# Phase 11 Performance Research — K-004

Slice K-004 (Wave 11), team-04, base
5324991daadff6d84c3a1503ad9c54c53c6950a7. Roadmap Phase 11 +
Section 11: profile before adding complexity; publish
reproducible results and loss cases. K-001/K-002/K-003 are
held/banking on parallel branches, NOT banked evidence here;
K-002's held baseline (team-02, read-only) reported the
shipped build losing to glibc 5.4x–65x and recorded in
INC-K002-02 that its -O0-vs-optimised comparison was unfair
and owed. This slice owns the honest version of that
question: same objects, same workload, only -O level differs.

## 1. Fast-path profile (SOURCE ANALYSIS, not measurement)

Common small allocation (1..1024 B), from the banked sources:

1. L1 boundary (src/api/mimir_alloc.c): one relaxed atomic
   fetch_add (E-005 audit counter) per call, then forwards.
2. L2/L4 (src/core/mimir_core.c mimir_core_alloc): zero-size
   posture gate, then the ONE global L6 lock — every alloc
   and free serialises on it; no thread-local cache exists
   (INC-G003-04). The lock is plausibly the largest fixed
   per-op cost and the only one that grows with contention.
3. alloc_any_locked dispatches size<=1024 to
   small_alloc_locked: class_index_for is a LINEAR scan of
   the 20-entry class table (size 64 = index 3, so 4 compares;
   size 1000 = 19 compares).
4. Cached-span check: mimir_class_active[class] plus
   span_header_is_live + span_can_serve (J-006 integrity:
   span_can_serve re-validates the free head via
   span_free_node_valid — division/modulo shape checks).
5. Reuse: free-list pop after span_free_node_valid on the
   head; else sequential carve at offset 96 + carve_count*class.
6. Publication bookkeeping under the lock: J-002 stats
   (bytes_requested += size, bytes_allocated += class_size),
   live counters, one relaxed atomic audit fetch_add.
7. Free (mimir_core_free): lock, then find_candidate_locked
   WALKS THE WHOLE MAPPING REGISTRY first (bounded by
   mimir_live_mappings, J-006) before the span registry is
   consulted — a small free pays O(live large mappings).
   Then find_span_live_locked walks spans (called twice on
   the small path: once in free, once in small_free_locked),
   and small_free_locked pushes the free list, decrements
   counters, and may count empty spans of the class.

Plausible cost ranking (labelled inference, unmeasured per
item): global lock > free-path registry walks (workload-
dependent) > J-006 validation + J-002 bookkeeping > linear
class scan (worst at large small sizes) > L1 atomic. The
-O0 build additionally pays full unoptimised codegen on
every one of these steps — which Section 2 measures.

## 2. Build-flavour comparison (MEASURED)

Harness tests/core/bench_k004.c (recorded run:
tests/core/recorded_run_k004.txt). Workloads 1–2 reuse
bench_j008's shape exactly; workload 3 (burst256: 200 rounds
of 256 live small64, then free-all) is new. Bench TU -O2 in
both legs; allocator objects differ ONLY by -O0 vs -O2.
5 reps per leg, fresh process per rep, interleaved.
Sink identical (73200) in all reps = identical workload.

| workload | -O0 median (range) | -O2 median (range) | ratio |
|---|---|---|---|
| small64 | 168 (168–173) | 86 (85–87) | 1.95x |
| large4096 | 5856 (5754–6260) | 5665 (5552–5832) | 1.03x |
| burst256 | 1488 (1465–1571) | 661 (651–678) | 2.25x |

(ns/op. Absolute values differ from J-008's recorded run —
different day/load; only the within-run comparison is claimed.)

What this proves: the shipped -O0 codegen roughly DOUBLES
the cost of the small-object path and the burst path on this
Pi. What it does NOT prove: anything about glibc — no glibc
leg was run here, so K-002's held loss factors are neither
confirmed nor revised by this slice; an -O2 MimirAlloc may
still lose to glibc, and that comparison must be re-run
against the NEW shipped build before any claim (INC-K004-02).
LOSS/FLAT CASE, plainly: large4096 does not improve — the
large path is one mmap/munmap pair per op (alloc_locked),
and compiler optimisation cannot move OS cost.

## 3. The change made (measured, minimal)

Makefile: shipped CFLAGS gain exactly one flag, -O2.
No source change, no ABI change, no other flag change.
Post-change shipped build, same bench, 5 reps: small64
median 87, burst256 657, large4096 5601 — the -O2 leg,
now shipped. This discharges INC-K002-02's comparison leg
by making the optimised flavour the shipped flavour.
No other fast-path change earned its keep in this slice:
the remaining costs in Section 1 (lock, free-path walks,
J-006/J-002 checks) are correctness/contract structures;
changing them needs its own measured slice, not a ride-along.

## 4. Sizing: class table and rounding waste (computed exactly)

Class table as banked (src/core/mimir_core.c,
mimir_class_sizes, 20 classes): 16, 32, 48, 64, 80, 96,
112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 640,
768, 896, 1024. Span = one page, 96-byte header; capacity
at 16384-byte pages: class 16 → 1018 objects, class 64 →
254, class 1024 → 15 (at 4096-byte pages class 64 → 62).

Representative rounding (request → class, waste, waste%
of class): 1→16 (15, 93.8%); 17→32 (15, 46.9%); 24→32
(8, 25.0%); 33→48 (15, 31.2%); 65→80 (15, 18.8%); 100→112
(12, 10.7%); 129→160 (31, 19.4%); 200→224 (24, 10.7%);
257→320 (63, 19.7%); 400→448 (48, 10.7%); 513→640 (127,
19.8%); 777→896 (119, 13.3%); 1000→1024 (24, 2.3%).
Exact class sizes waste 0. Worst absolute waste in range:
127 bytes (513→640 and 897→1024). Worst percentage above
the first class: 19.8%. No table change is proposed: the
table is the banked G-002 design, waste is bounded and
known, and no measurement here indicts it.

## 5. Verification

Clean rebuild after the change: make test exit 0, failures
0; test-j002/j003/j005/j006/j007/j008 + bench-j008 exit 0;
DEBUG=1 test-j004 exit 0; nm -D = exactly 4 dynamic symbols
in both production and hardened .so; README.md and
include/mimir_alloc.h byte-identical to base; Samtal
HTTP 200 before and after. Full numbers in the recorded run.

## 6. INC-K004 register

- INC-K004-01: mimir_core_free walks the mapping registry
  before the span registry and calls find_span_live_locked
  twice on the small path — O(live mappings) per small free.
  Candidate fast-path slice: span-first owner check or a
  page-aligned span lookup, ONLY with measured gain and
  J-006 integrity posture preserved. Commissioned follow-up,
  not fixed here (out of this slice's minimal-change scope).
- INC-K004-02: K-002's glibc loss factors were measured
  against -O0; they must be re-measured against the new
  -O2 shipped build before any Phase 11 claim cites them.
  Commissioned follow-up for the next baseline slice.
- INC-K004-03: class_index_for linear scan costs up to 19
  compares at the top of the small range; a branch/table
  form is unmeasured — no change without a bench that
  isolates it. Recorded, not commissioned.
- INC-K004-04: burst256 per-op cost (~1488 ns at -O0) is
  ~9x small64's, driven by carve + free bookkeeping at 256
  live objects; not decomposed further here. Recorded for
  the Phase 11 profiling queue.
