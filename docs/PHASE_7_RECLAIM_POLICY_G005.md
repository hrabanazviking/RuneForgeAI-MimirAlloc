# Phase 7 — Reclaim Policy + Idle-Thread Retained Memory (Slice G-005)

Status: POLICY DEFINED and MEASURED on this host (Raspberry Pi,
16 KiB pages) in `tests/core/recorded_run_g005.txt` (real run,
Sat Nov 7 12:47:44 AM EST 2026, base 81edaae, branch
yrsa/mimiralloc-05). Plain verdict: **the reclaim policy G-003
implemented holds exactly as designed under measurement** — after
allocate/free churn that forces 3 spans in every one of the 20
size classes, exactly 2 empty spans per class are retained and
the third is unmapped at free time: 40 spans, 655,360 mapped
bytes, worst case reached exactly and never exceeded. Four idle
threads that had allocated and freed add **zero** retention:
the allocator keeps no thread-local cache, so idle-thread
retained memory is the shared global bound, already counted.
Phase 7 is NOT exited by this slice; the exit assembly remains
G-009's.

Companions: `docs/PHASE_7_FOUNDATION_PLAN.md` (G-001),
`docs/PHASE_7_SIZE_CLASSES_G002.md` (G-002),
`docs/PHASE_7_SMALL_OBJECT_PATH_G003.md` (G-003, the
implementation this slice's policy describes).

## 1. The reclaim policy (RP-1..RP-5)

The policy is defined here and was already implemented by
G-003 in `src/core/mimir_core.c` (`small_free_locked`,
`span_release_locked`, `empty_spans_of_class_locked`,
`MIMIR_MAX_EMPTY_SPANS_PER_CLASS`). G-005 changes **no core
source**: the measured behaviour matched the policy as written,
so this slice's code deliverable is the evidence test
(`tests/core/test_g005.c`, standalone `make test-g005`) that
pins the policy, plus this definition. Any future change to
the bound or the trigger is a policy change against this
section, not an implementation detail.

- **RP-1 (when a span becomes reclaimable).** A span whose
  `live_count` returns to 0 is EMPTY. Only empty spans are
  ever reclaimed; a span with any live object is never
  released, in whole or in part. The large (mapping) path is
  untouched by this policy: a mapping is still released whole
  at the free of its single block, exactly as banked in
  Phase 3 (retained-after-free 0 there).
- **RP-2 (what is retained).** At most **2 empty spans per
  size class** are retained, kept mapped and on the span
  registry, ready to serve their class again (bounded local
  reuse). Retention is counted per class, not globally: one
  class's churn can never evict or consume another class's
  retained spans.
- **RP-3 (what is returned, and when).** The empty span that
  would exceed its class's bound is **released whole to the
  central layer (L6 `mimir_platform_map_release`, i.e.
  unmapped) synchronously, inside the same locked free
  operation that emptied it**. Reclaim is never deferred: no
  background reaper, no lazy sweep on a later allocation, no
  waiting for thread exit. On release failure the core's
  F-06-style rollback keeps the span retained and LIVE (the
  object stays freed either way); that path was not exercised
  by this slice's runs (no L6 failure was induced).
- **RP-4 (idle threads).** Retention is **global per class,
  never per thread**. The small-object path holds no
  thread-local state (verified by source inspection: no
  `_Thread_local`, `__thread`, or pthread-key state exists in
  `src/core/mimir_core.c` or `src/api/`). An idle thread
  therefore retains nothing of its own; whatever its earlier
  frees left behind is the shared per-class bound of RP-2,
  counted once. Per-class accounting of the retained total
  is the table in section 2: each class contributes exactly
  2 spans x 1 page = 32,768 bytes on this host.
- **RP-5 (the bound).** Retained-after-free memory of the
  small-object path is at most 20 classes x 2 spans x one
  system page: **655,360 bytes (640 KiB) on this 16 KiB-page
  host**, 163,840 bytes at 4 KiB pages. Live spans beyond the
  retained set are bounded by live demand only.

## 2. Measured evidence (recorded run, exact counts)

Method, stated honestly: span and object counts are **exact**
audit-counter deltas (G-003 counters via
`tests/core/mimir_small_g003.h`, INC-E001-07 shape (b));
retained mapped bytes are live spans x the runtime page size
(each span is exactly one page, G-002 C-2). VmRSS from
`/proc/self/status` is a **process-level cross-check only** —
it includes the test harness's own libc allocations and
thread stacks and is never a pass/fail check. Warm-up: one
large (2048, mapping-path) alloc/touch/free before measuring,
chosen so the warm-up itself leaves no span state behind.
All runs are small and bounded; the largest single phase
holds 2,037 blocks of class 16.

### P1 — per-class bound (all 20 classes)

For each class: hold 2*capacity+1 blocks (forces exactly 3
spans), free all. Observed for **every** class, without
exception: 3 spans published in the phase, 1 span retired in
the phase, retained delta exactly **+2 spans**, live small
objects back to 0. Capacity = (16384 - 96) / class size.

| class | cap | blocks held | retained after (cumulative spans) |
|------:|----:|------------:|----------------------------------:|
| 16 | 1018 | 2037 | 2 |
| 32 | 509 | 1019 | 4 |
| 48 | 339 | 679 | 6 |
| 64 | 254 | 509 | 8 |
| 80 | 203 | 407 | 10 |
| 96 | 169 | 339 | 12 |
| 112 | 145 | 291 | 14 |
| 128 | 127 | 255 | 16 |
| 160 | 101 | 203 | 18 |
| 192 | 84 | 169 | 20 |
| 224 | 72 | 145 | 22 |
| 256 | 63 | 127 | 24 |
| 320 | 50 | 101 | 26 |
| 384 | 42 | 85 | 28 |
| 448 | 36 | 73 | 30 |
| 512 | 31 | 63 | 32 |
| 640 | 25 | 51 | 34 |
| 768 | 21 | 43 | 36 |
| 896 | 18 | 37 | 38 |
| 1024 | 15 | 31 | 40 |

P1 end state: **live spans 40, retained mapped bytes
655,360** — the RP-5 worst case, reached exactly.

### P2 — churn stability

10 rounds of alloc/free of 3*capacity (381) blocks of class
128. After **every** round: live spans == 40, live small
objects == 0. Retention does not grow under churn. RSS at
P2 end: 2064 kB (process-level fact).

### P3 — idle-thread case

4 worker threads each allocate 200 blocks of size 256,
touch them, free them all, then sit idle (blocked on a
condition variable, holding nothing) while the main thread
measures. While idle: **live spans 40, live small objects 0,
RSS 2128 kB** — the idle threads add no retention beyond the
global bound state P1 ended in. Threads then released and
joined: live spans still **40**. Thread exit reclaimed
nothing further, because reclaim already happened at free
time (RP-3) and no per-thread state exists to drain (RP-4).

### P4 — final accounting

Live small objects 0; retained spans 40; retained mapped
bytes 655,360. RSS: start (after warm-up) 1408 kB, end
2320 kB, delta **+912 kB** — larger than the 640 KiB of
retained span pages, because RSS also carries the harness's
own allocations and the P3 thread stacks; see INC-G005-02.
Test totals: **97 checks, 0 failures, RESULT: PASS**;
`make test-g005` exit 0. Full suite in the same recorded
run: `make clean && make test` exit 0 (final accounting
4590 checks, 0 failed); `make test-g003` exit 0 (54 checks,
0 failures); `make lib` exit 0 with **nm -D exactly 4**
(mimir_malloc, mimir_free, mimir_calloc, mimir_realloc).
Public header and README untouched (git diff: Makefile only,
+26 additive lines for the standalone test-g005 target).

## 3. Comparison

- **Against the G-001 Phase 3 baseline.** The banked
  mapping-per-allocation allocator retained **0** after free
  and mapped 32 KiB per live allocation on this host. The
  Phase 7 small-object path deliberately trades that 0 for a
  **bounded, exactly measured 655,360 bytes** of retention —
  the "deliberate, bounded, reclaimable deviation from 0"
  the G-001 plan predicted, now sized and proven. In
  exchange, G-003 measured small-allocation p50 at 148–253 ns
  against the baseline's 4889–5167 ns. Nothing about the
  large path's 0-retention behaviour changed.
- **Against G-003's stated retention behaviour.** G-003's
  doc states the bound (2 per class; worst case 40 pages =
  640 KiB on this host) from the implementation. G-005
  confirms it **by measurement in all 20 classes
  individually**, under churn, and in the multi-thread idle
  case G-003 did not exercise. No discrepancy was found.
- **Against the platform allocator (G-001 libc leg).**
  G-001 observed libc arena retention without an exact
  bound. MimirAlloc's retention is smaller in kind: a fixed
  40-page ceiling, independent of how many threads ran or
  how much churn passed — proven here for 4 idle threads;
  the ceiling is thread-count-independent by construction
  (RP-4), not merely untested at higher counts.

## 4. Gaps, surprises, and owed work (INC-G005-NN)

- **INC-G005-01 (no core change — deliberate).** The reclaim
  policy needed no implementation work in this slice: G-003
  already implements RP-1..RP-5. G-005's deliverable is the
  policy definition, the pinning test, and the measurements.
  Recorded so the absence of a src/ diff is read as a
  finding, not an omission.
- **INC-G005-02 (RSS is not retention).** The process RSS
  delta over the test (+912 kB) exceeds the retained span
  bytes (640 KiB): RSS counts harness libc allocations,
  thread stacks, and every touched page. Any future slice
  quoting RSS as allocator retention would be wrong; the
  audit counters are the exact instrument. The G-001 probe's
  RSS-based retained readings inherit this caveat (they were
  single-threaded with no harness churn, so the comparison
  there stands, but the method note carries forward).
- **INC-G005-03 (no idle-time reaper — by design).** Once a
  class is warmed, its 2 retained spans stay mapped for the
  life of the process; nothing reclaims them after an idle
  period. This is the deliberate price of the reuse latency
  G-003 measured, and it is bounded by RP-5 — but a
  long-lived process that touches all 20 classes once pays
  640 KiB permanently on this host. Recorded for G-008
  (responsiveness record) and G-009 (exit packet) to carry
  as a stated property, not a defect.
- **INC-G005-04 (G-006 scope shrinks).** Thread-exit
  handling (G-006) has, in the current design, nothing
  thread-local to drain at exit — P3 proves exit reclaims
  nothing because nothing per-thread exists. G-006's honest
  scope is to verify and document this posture (or, if it
  introduces thread-local caching deliberately, to reopen
  this slice's measurement for the new retained total).
- **INC-G005-05 (G-004 dependency, owed).** INC-G003-02's
  re-teach of `test-e005-dyn` for span pointers is G-004
  work, **not present in this slice's base (81edaae)** and
  not run here; `make test` does not include the e005-dyn
  target, so the full-suite exit 0 above neither proves nor
  contradicts it. The keeper reconciles at banking after
  G-004.
- **INC-G005-06 (release-failure rollback unexercised).**
  RP-3's rollback branch (L6 release failure leaves the span
  retained) was not exercised: no failure-injection hook
  exists at L6 in this base, and adding one is outside this
  slice's scope. The branch is covered by code inspection
  only; a fault-injection slice could prove it by
  execution.
