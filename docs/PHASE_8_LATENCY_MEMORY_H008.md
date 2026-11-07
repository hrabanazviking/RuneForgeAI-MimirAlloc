# Phase 8 Latency and Memory Reports Under Contention — H-008

Slice H-008 (Wave 8). Governing: roadmap Phase 8 exit row E2
("latency and memory reports archived") via
`docs/PHASE_8_FOUNDATION_PLAN_H002.md`, which named this
slice as owner of the two unmeasured items in its Section 5:
item 5 (latency distribution — only whole-leg means existed)
and item 1 (memory behaviour under contention). H-004's
decision stands untouched: the single global lock is
RETAINED and sharding is deferred; this slice re-measures
that retained design, it does not re-open the question.
All numbers below are from the real recorded run in
`tests/perf/recorded_run_h008.txt` (Sat Nov 7 04:14 EST
2026, Pi aarch64, 4 cores, 16 KiB pages, base de53575).
Timing is recorded as measured on a shared home host —
never a bound, never a guarantee.

## Harness and method

`tests/perf/probe_h008.c`, standalone via `make test-h008`
(NOT part of `make test`). Two legs, at 1/2/4/8 threads:

- Latency leg — the H-002 Phase B shape exactly, so the
  comparison is honest: every worker alloc/fill/verify/free
  its own blocks against the one global lock, sizes 64 B
  with every 10th 2048 B (mapping path), 2,000 iterations
  per thread (H-002 used 1,500). The difference is the
  timing: each `mimir_malloc` and each `mimir_free` call is
  timed individually with CLOCK_MONOTONIC and the fill /
  pattern-verify between them is NOT timed. Samples from
  all threads are pooled, sorted, and reported as mean /
  p50 / p90 / p99 / max for malloc, free, and the pair.
  Two extra clock reads per op are part of the measured
  number. Pattern checks follow the H-002 S7 method: the
  worker re-derives each expected byte itself; the
  allocator never vouches for its own blocks.
- Memory leg — each worker allocates a held set of 128
  blocks (sizes cycle 64/256/512 span path, 2048 mapping
  path), fills them, and waits at a barrier whose count
  includes the main thread. While all workers are blocked
  at the second barrier (no allocator call in flight — a
  true quiescent point), the main thread reads the E-005 /
  G-003 audit counters and VmRSS and reconciles them
  against the deterministic held set. Workers then verify
  and free the ring-neighbour's blocks cross-thread, and
  the counters are re-read at the next quiescent point.

Byte accounting: requested bytes are the workload sum.
Mapped bytes are derived per live mapping — one header
page plus payload pages (the S7.1 construction); every
mapping in this workload carries a 2048 B request, so
each occupies exactly 2 pages = 32,768 B. Span bytes are
live spans x page size (one span is exactly one system
page, G-003). Both derivations are stated here so the
tables can be re-derived from the raw counter lines in
the recorded run.

## Latency results (medians of the 3 recorded reps, ns)

Alloc/free pair (malloc call + free call, fill excluded):

| threads | n | mean | p50 | p90 | p99 | max (median) |
|---|---|---|---|---|---|---|
| 1 | 2,000 | 746 | 260 | 4,722 | 5,388 | 11,666 |
| 2 | 4,000 | 1,764 | 370 | 6,907 | 12,166 | 109,019 |
| 4 | 8,000 | 5,366 | 352 | 7,944 | 35,278 | 3,332,143 |
| 8 | 16,000 | 12,564 | 277 | 5,871 | 56,574 | 7,467,934 |

Malloc alone, median p50 / p99: 111 / 3,074 (1T), 129 /
6,129 (2T), 129 / 14,482 (4T), 111 / 22,815 (8T). Free
alone, median p50 / p99: 149 / 2,444 (1T), 204 / 8,019
(2T), 186 / 21,482 (4T), 167 / 34,945 (8T). Full per-rep
lines for every statistic are in the recorded run.

Reading the distribution honestly:

- The median pair latency does not move with thread count
  (260–370 ns at 1–8 threads). The contention cost the
  single lock charges lives in the MEAN and the TAIL:
  the pair mean rises ~17x from 1 to 8 threads and the
  p99 rises ~10x. Most operations are span-path freelist
  hits that win the lock quickly; the cost concentrates
  in the mapping-path operations (every 10th op, visible
  as the p90 step at ~4.6–7.9 µs) and in unlucky queueing
  behind them.
- Single-sample maxima reach ~11.9 ms at 8 threads. This
  host is a shared home Pi (Samtal and the household
  services run on it); the maxima are recorded as
  measured — scheduler preemption included — and are not
  an allocator bound in either direction.
- Comparison with H-002: H-002's whole-leg means (4,184
  ns/op at 1T rising to 25,778 at 8T, ~6.2x) INCLUDE the
  per-op fill and byte-verify work; H-008's pair means
  time the two calls only. The absolute numbers are
  therefore not the same quantity and no improvement is
  claimed (INC-H008-03). What is comparable is the shape:
  both measures scale steeply with thread count under the
  one retained lock, and H-008 adds where in the
  distribution the scaling sits — the tail, not the
  median.

## Memory under contention (identical in all 3 reps)

| threads | held small / map | requested B | mapped B | span B (spans) | RSS held / after (kB) |
|---|---|---|---|---|---|
| 1 | 96 / 32 | 92,160 | 1,048,576 | 65,536 (4) | 2,528 / 1,696 |
| 2 | 192 / 64 | 184,320 | 2,097,152 | 98,304 (6) | 3,792 / 1,728 |
| 4 | 384 / 128 | 368,640 | 4,194,304 | 147,456 (9) | 5,920 / 1,760 |
| 8 | 768 / 256 | 737,280 | 8,388,608 | 262,144 (16) | 10,192 / 1,776 |

- Reconciliation was exact at every quiescent point in
  every rep: live small objects and live mappings equal
  the held set precisely, and after the cross-thread
  frees both return to exactly delta 0.
- Mapping amplification is the dominant memory fact: a
  2048 B request occupies 32,768 B mapped (16x — one
  header page + one payload page on this 16 KiB-page
  host, by the S7.1 construction). At 8 threads the held
  set's 737,280 requested bytes correspond to 8,388,608
  mapped bytes + 262,144 span bytes (~11.7x overall).
- Retained spans: after all frees the process retains
  the warmed classes' spans (G-005: no idle reaper). The
  retained count settles at 6 spans = 98,304 B by the end
  of every recorded rep (FINAL line), and RSS returns to
  1,696–1,776 kB after each leg. Retention is bounded by
  the warmed size classes, not by the held-set size:
  quadrupling the held set from 2 to 8 threads leaves the
  end state at the same 6 retained spans.

## Repeated-stress archive (stability across repetitions)

The recorded run re-runs the banked stress evidence as
one archive: `make test` x2, `make test-h007` x3, and
`make test-h008` x3, all in the one session.

- make test: exit 0 in both reps, counts identical —
  4,590 checks run, 0 failed; 2,088 allocations, 2,088
  frees.
- test-h007: exit 0 in all 3 reps; all 15 scenario
  summaries (1/2/4/8/12 threads x 3 internal reps) OK in
  each — 51,840 operations per rep, 0 failures of any
  counted kind, audit reconciled every rep.
- test-h008: exit 0 in all 3 reps; every MEM line is
  byte-identical across reps (counters, byte figures and
  RSS all repeat exactly), and every LAT line reports 0
  pattern failures and 0 null failures.
- Stability verdict, in counts: every correctness count
  in the archive is identical across repetitions — zero
  failures, zero audit drift, zero counter variance.
  Timing is the only thing that varies: the 1-thread pair
  mean is stable within ~8% (736–800 ns), while the
  8-thread pair mean spreads 9,864–14,037 ns and its p99
  spreads 32,037–113,407 ns between reps. That variance
  is reported, not smoothed: on this shared host the tail
  is a distribution, not a number.

## Verdict and incidents

No allocator gap was found: no corrupted live allocation,
no lost ownership, no audit drift, at any thread count or
repetition. No `src/` change was made and no fix was
needed. Samtal answered HTTP 200 before and after the
recorded run; Pi memory stayed at ~13 GiB available
throughout.

- INC-H008-01 (the contention cost lives in the tail —
  design datum, not a defect). Median pair latency is
  flat at 1–8 threads while the mean rises ~17x and the
  p99 ~10x under the retained single lock. This refines
  INC-H002-01's whole-leg picture; it does not contradict
  it and it does not re-open H-004's decision — any future
  contention work must be judged on tail reduction, since
  the median has nothing left to gain.
- INC-H008-02 (mapping-path memory amplification, 16x at
  2048 B on 16 KiB pages — measured cost of the S7.1
  one-header-page construction, not a defect). Recorded
  here so the H-009 exit packet grades E2 with the number
  in hand. Small mapping-path requests are the expensive
  shape on large-page hosts; no change is proposed by
  this slice.
- INC-H008-03 (method note — H-002 and H-008 latency
  absolutes are different quantities). H-002 timed whole
  legs including fill + verify; H-008 times the two calls
  only. The slices compare on scaling and distribution
  shape only, and this doc claims no speedup.

## Phase 8 exit status — honest

Phase 8 is NOT exited by this slice. H-008 supplies the
E2 evidence (latency distribution + memory under
contention, archived with the repeated-stress record)
that H-002 left named as unknown. H-009 (exit packet and
grading) remains; the exit claim itself is H-009's to
grade on the full wave evidence.

## Boundaries held

README.md and include/mimir_alloc.h byte-identical to
base de53575 (git diff empty, recorded). No src/ change;
Makefile additive only (one H-008 block). Public ABI
exactly 4 symbols (nm -D, recorded). make test exit 0.
Pi load stayed light: the whole archive completes in
seconds, memory free ~13 GiB throughout, Samtal live
before and after.
