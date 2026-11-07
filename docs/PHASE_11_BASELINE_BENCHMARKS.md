# Phase 11 - Baseline Benchmarks (Slice K-002)

Slice: **K-002 (team-02)**. Wave 11, Phase 11 (Performance
research). Deliverables: the harness
`tests/perf/bench_k002.c`, the runner
`tests/perf/run_bench_k002.sh`, the summariser
`tests/perf/summarize_k002.py`, the recorded run
`tests/perf/recorded_run_k002.txt`, and this document.
Base: `5324991` (Wave 10 COMPLETE) on branch
`yrsa/mimiralloc-02`, worktree `/home/pi/mimiralloc-team-02`
on Raspberry Pi `purpleviking1`. `README.md` was not
touched; the public header and the 4-symbol ABI are
unchanged; the Makefile change is one additive
`bench-k002` block at end of file (standalone, NOT part
of `make test`).

> **VERDICT (plain first).** On this Pi, in this suite,
> the banked MimirAlloc **loses to the host glibc
> allocator on every workload measured** - by a median
> factor of about 5x on the large-allocation workload
> and up to about 65x on the mixed lifecycle workload.
> There is no workload in this baseline where MimirAlloc
> wins. That is the honest Phase 11 starting point: the
> banked allocator was built for contract correctness,
> auditability and bounded behaviour across Phases 3-10,
> not for speed, and this suite now puts reproducible
> numbers on the gap so later Phase 11 work can be
> measured against them. No universal claim is made in
> either direction (Section 6).

## 1. Methodology

K-001's methodology document had **not** landed in the
forge main docs when this slice started (checked
`/home/pi/mimiralloc-forge/docs/` for `PHASE_11*` and
K-001 material: absent). The methodology below is
therefore stated here in full, and reconciliation with
K-001 when it lands is owed as **INC-K002-01**.

- **One source, two binaries.** `bench_k002.c` is
  compiled twice. The workload code is byte-identical;
  only the allocator function pointers differ: the glibc
  leg passes stdlib `malloc`/`free`, the MimirAlloc leg
  passes `mimir_malloc`/`mimir_free` from the banked
  production objects (exactly the objects `make test`
  builds). Workload equivalence is checkable from the
  recorded run itself: the per-workload checksums are
  identical across legs in all 56 raw lines.
- **An op** is one allocation plus its matching free.
  Every block is pattern-touched (first and last byte)
  while live, so the work cannot be optimised away.
- **Workloads** (sizes drawn from the banked size-class
  table, 20 classes 16..1024, G-002/banked core):
  - `small_churn`: 100,000 ops, one block at a time,
    sizes cycling 16..256.
  - `mixed_batch`: 256,000 ops, batches of 256 live
    blocks spanning all 20 classes, freed in reverse.
  - `large_batch`: 9,600 ops, batches of 32 live blocks
    at 2048..1048576 (above the 1024 cutoff; the
    mapping-per-allocation path for MimirAlloc).
  - `mixed_lifecycle`: 307,200 ops, rounds of 512 live
    mixed small+large blocks; odd slots freed and
    refilled, then all freed.
- **Warm-up + repetitions.** Each process runs every
  workload once at 1/10 scale untimed, then once timed
  with `CLOCK_MONOTONIC`. The runner executes each leg
  as **7 separate processes** (contract minimum 5),
  alternating legs (glibc, mimiralloc, glibc, ...) so
  slow machine drift lands on both legs.
- **Reporting.** Headline figures are **medians** of the
  7 repetitions, with min/max printed as the spread
  (`summarize_k002.py`). A single wall time is never a
  verdict anywhere in this slice.
- **Machine discipline.** Runs are short and bounded
  (about 11 s per full 7-repetition suite). Load
  average, CPU governor (`ondemand`), SoC temperature
  (57.85 C at run start), free memory (13 G available)
  and the Samtal health check (HTTP 200 on
  127.0.0.1:8787, before and after) are recorded in the
  recorded run. Nothing else was stopped or pinned; the
  Pi was under its normal home load (load average about
  1.3), which is part of the recorded conditions, not
  hidden from them.
- **Build caveat (material).** The harness driver is
  compiled `-O2` for both legs (bench_j008 precedent).
  The MimirAlloc objects are the banked production
  build, whose `CFLAGS` carry **no `-O` level** (the
  compiler default, `-O0`); glibc is the distribution's
  optimised build. This suite therefore measures **the
  banked build as shipped**, not an optimised MimirAlloc
  that does not exist yet. See INC-K002-02.

## 2. Results (recorded run, medians of 7)

| workload | glibc ops/s | MimirAlloc ops/s | glibc ns/op | MimirAlloc ns/op | glibc faster by |
|---|---|---|---|---|---|
| small_churn | 47,358,934 | 2,602,344 | 21.12 | 384.27 | 18.2x |
| mixed_batch | 22,918,489 | 1,545,828 | 43.63 | 646.90 | 14.8x |
| large_batch | 694,603 | 128,248 | 1439.67 | 7797.39 | 5.4x |
| mixed_lifecycle | 18,984,171 | 291,092 | 52.68 | 3435.34 | 65.2x |

Full raw repetition table and min/max spreads:
`tests/perf/recorded_run_k002.txt`. Spread quality: the
MimirAlloc medians are tight (e.g. small_churn min/max
2,462,723..2,612,368 ops/s); glibc large_batch spreads
wider (569,768..724,991), consistent with kernel mapping
costs varying under normal home load.

**Reproduction.** The suite was re-run once in full
after the recorded run. Second-run medians: glibc
47,020,764 / 22,845,013 / 709,025 / 19,054,363 ops/s;
MimirAlloc 2,596,648 / 1,544,888 / 118,690 / 318,545
ops/s (same workload order as the table). Every median
agrees with the recorded run within 9%, the ordering of
the legs and of the workloads is identical, and the
loss picture is unchanged. The recorded run is the one
committed; the reproduction run was verification only.

## 3. Where MimirAlloc loses (required reading)

All four workloads are loss cases; ranked worst first:

1. **mixed_lifecycle (65.2x slower).** The realistic
   shape - many live blocks, partial frees, refills -
   is where the gap is largest. Interleaved large blocks
   (every 8th slot) take a fresh mapping per allocation
   on the MimirAlloc side, and every operation passes
   the one global lock and the core's validation work.
2. **small_churn (18.2x slower).** The pure fast-path
   test. glibc's per-thread cache answers in about
   21 ns/op; the banked small path (span lookup, class
   bookkeeping, locked) costs about 384 ns/op.
3. **mixed_batch (14.8x slower).** Same story at batch
   scale across all 20 classes.
4. **large_batch (5.4x slower) - the narrowest gap.**
   Both sides pay real kernel mapping costs here;
   MimirAlloc still loses, because each large allocation
   is a fresh mapping plus header page plus bookkeeping,
   against glibc's arena reuse.

## 4. Where MimirAlloc wins

**Nowhere in this suite.** No workload, repetition, or
size band measured here has MimirAlloc ahead of glibc.
The closest approach is `large_batch` (5.4x). Any future
slice claiming a win must reproduce it with this suite
(or its K-001-reconciled successor) and quote medians,
not a single run.

## 5. What plausibly causes the gap (labelled)

Design facts (from the banked code, not measured
separately here): one global lock serialises all
operations; the production build is unoptimised (`-O0`,
Section 1 caveat); large allocations map fresh pages
per allocation with a header page per mapping; the core
performs validation and statistics bookkeeping on the
allocation path (Phases 9-10 surfaces). Hypotheses (not
established by this slice): the relative weight of lock,
optimisation level, mapping policy and bookkeeping in
the totals above. Apportioning them is later Phase 11
work, and this baseline exists so that work has a
before-picture.

## 6. What is NOT claimed

- No universal-win or universal-loss claim: these are
  Pi-only numbers (Raspberry Pi `purpleviking1`,
  AArch64, kernel 6.12.112-v8-16k, page size **16384**),
  against **glibc 2.36** (Debian 2.36-9+rpt2+deb12u14),
  gcc 12.2.0, governor `ondemand`, under normal home
  load with Samtal live.
- No claim about an optimised MimirAlloc build: none
  was measured (INC-K002-02).
- No multithreaded claim: this baseline is
  single-threaded (INC-K002-03). Phase 8 has contention
  probes, but they are not a baseline suite.
- No memory-footprint claim: this suite measures
  throughput and per-op time only, not RSS, retained
  bytes or fragmentation (INC-K002-04).
- No latency-distribution claim inside an op: timing is
  whole-pass, not per-op (the H-008 probe remains the
  per-op latency instrument).

## 7. Reproduce it

```
make bench-k002        # builds both legs, 7 reps each, prints summary
python3 tests/perf/summarize_k002.py <raw-file>
```

## 8. INC list

- **INC-K002-01** K-001 methodology doc not landed at
  this slice's start; this doc states its own
  methodology in full. Reconciliation owed when K-001
  banks (terminology and any differing repetition or
  reporting rules to be folded in by the banking
  coordinator or a follow-up slice).
- **INC-K002-02** The banked production `CFLAGS` carry
  no optimisation level, so the MimirAlloc leg is an
  `-O0` build against an optimised glibc. An
  optimised-flavour comparison (production code at
  `-O2`, artefact-verified like the J-008 hardened
  flavour) is owed before any general statement about
  the allocator's speed, as opposed to the shipped
  build's speed.
- **INC-K002-03** No multithreaded baseline in this
  suite (single-threaded processes only); a contention
  baseline in suite form is owed to Phase 11.
- **INC-K002-04** No footprint leg: RSS / retained /
  mapped bytes under the same workloads are unmeasured
  in baseline form; owed to Phase 11.
