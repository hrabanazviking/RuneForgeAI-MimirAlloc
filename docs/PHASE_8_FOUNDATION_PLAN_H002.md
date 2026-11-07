# Phase 8 — Foundation Plan + Measured Cross-Thread / Contention Baseline (Slice H-002)

Status: MEASURED BASELINE on this host (Raspberry Pi, aarch64, 4 cores,
16 KiB pages) in `tests/perf/recorded_run_h002.txt` (real run,
Sat Nov 7 02:06 EST 2026, base 3a93c11, branch yrsa/mimiralloc-02).
Plain verdict: **cross-thread free already works at this base and
nothing corrupts — the Phase 8 problem is contention cost and the
unexercised teardown cases, not cross-thread correctness.**
Across 3 recorded repetitions, 67,500 cross-thread frees (blocks
allocated by one thread, pattern-verified byte-for-byte and freed
by a different thread) produced **0 pattern failures and 0 null
failures**, and the contention leg shows the one-global-lock cost
plainly: mean alloc/free-pair cost rises from **4,184 ns/op at
1 thread to 25,778 ns/op at 8 threads** (medians of 3 reps;
~6.2x). H-002 changes **no `src/` code**: it is the plan +
measurement slice. H-001 (team-01, in parallel on the same base)
owns the only core change at the head of this wave, the
INC-G008-02 unload drain; every file in this slice is disjoint
from it.

Companions: `docs/MimirAlloc_Research_and_Development_Roadmap.md`
Phase 8 (governing text, quoted in Section 1),
`docs/PHASE_7_EXIT_EVIDENCE.md` (G-009 packet; carried register
O-03/O-04/O-05 restated in Section 4),
`docs/PHASE_7_THREAD_EXIT_G006.md` (G-006 thread-exit proof this
baseline builds on), `WAVE_PLAN.md` in the forge checkout (the
H-003..H-009 queue this plan maps onto).

## 1. The governing text, sentence by sentence

Roadmap Phase 8, verbatim: "Implement and verify cross-thread
frees and contention controls; then consider sharding or bounded
remote-free queues. Include owner death, thread cancellation and
teardown. Test at small thread counts and under
oversubscription. Verify correctness with stress runs and
independent data-pattern checks." Exit: "no corrupted live
allocations or lost ownership across repeated stress scenarios;
latency and memory reports archived."

| # | Phase 8 sentence / clause | Owning slice(s) | Baseline position (this slice) |
|---|---|---|---|
| S1 | Implement and verify cross-thread frees | **H-003** (verify/harden; see INC-H002-03) | Behaviour exists by construction (Section 2) and is measured correct at 1/2/4/8 threads (Section 3); H-003 turns the probe shape into a regression evidence test with counter reconciliation |
| S2 | Contention controls | **H-004** | Cost quantified (Section 3): per-op cost scales ~linearly with thread count under the single lock — the number H-004's controls must beat |
| S3 | Then consider sharding or bounded remote-free queues | **H-004** (evaluation + decision, then implementation if decided) | No sharding or queue exists (Section 2 greps); the baseline gives the before-picture; no design is pre-judged here |
| S4 | Include owner death | **H-005** | Unexercised as a distinct case: G-006 proved exit-while-holding, but a thread dying mid-workload under contention is not yet run (Section 5) |
| S5 | Thread cancellation and teardown | **H-006** (incl. fork posture O-03) | Never exercised (G-009 O-04); cancellation points, cleanup, and teardown ordering all unknown (Section 5) |
| S6 | Test at small thread counts and under oversubscription | **H-007** (stress), seeded here | This baseline covers 1/2/4/8; on this 4-core host 8 threads is already 2x oversubscription. H-007 widens repetitions and mixes, it does not start from zero |
| S7 | Verify correctness with stress runs and independent data-pattern checks | **H-007**, method set here | The probe's pattern check is independent in the required sense: the freeing thread re-derives the seed from (allocator thread, index) and verifies every byte itself before freeing; the allocator is never asked to vouch for its own blocks |
| E1 | Exit: no corrupted live allocations or lost ownership across repeated stress scenarios | **H-009** (exit packet), evidence from H-003/H-005/H-006/H-007 | Baseline leg MET at baseline scale (0 failures in 3 reps); the exit claim itself is H-009's to grade on the full wave's evidence |
| E2 | Exit: latency and memory reports archived | **H-008** | Latency baseline archived here (Section 3); memory-under-contention is NOT measured yet and is named unknown (Section 5) |

H-008 also archives the repeated-stress record; H-009 assembles
the packet and grades E1/E2 honestly, exactly as G-009 did for
Phase 7 (PARTIAL is an available verdict and was used there).

## 2. Current architecture, as verified (not assumed)

All claims below are from the greps recorded verbatim in
`tests/perf/recorded_run_h002.txt` at this base:

- **One global lock.** The only mutex in `src/` or `include/` is
  `src/platform/mimir_platform.c:140`
  `static pthread_mutex_t mimir_global_lock = PTHREAD_MUTEX_INITIALIZER;`
  Every allocator entry point serialises on it (G-006 Section 1,
  re-verified here). There is no second lock, no rwlock, no
  lock-free path.
- **Two global registries.** Live state is the mapping registry
  (`mimir_registry_head`, `src/core/mimir_core.c:192`) and the
  span registry (G-003, same file), plus global counters.
  Ownership is process-global; no entry records its allocating
  thread.
- **No per-thread state — exhaustive negative search.** The
  recorded grep for `_Thread_local`, `__thread`,
  `pthread_key_create`, `pthread_setspecific` over `src/` and
  `include/` exits 1 (no match). There is no per-thread cache,
  no remote-free queue, no shard.
- **Consequence for Phase 8:** cross-thread free is not a
  feature to add — the free path cannot tell threads apart.
  What Phase 8 actually has to build is (a) proof at stress
  scale, (b) a contention answer to a measured ~6.2x per-op
  scaling, and (c) the teardown cases (death, cancellation,
  fork) that the current design has never been exercised
  against. Any sharding/queue design in H-004 introduces the
  first per-thread or per-shard state this allocator has ever
  had, and with it the first real thread-exit/ownership
  questions — G-006's "exit is a non-event" proof rests on
  there being nothing per-thread to drain, and H-004 must
  re-prove or amend it if that changes.

## 3. Measured baseline (probe_h002, 3 recorded reps)

Method: `tests/perf/probe_h002.c` (standalone `make test-h002`,
NOT in `make test`). Per configuration (1/2/4/8 threads):
Phase A allocates 1,500 blocks per thread (sizes cycling
64/256 span path, 2048 mapping path), fills each with a seeded
byte pattern, barrier, then each thread verifies and frees the
blocks of the next thread (ring); Phase B is a pure contention
leg — every thread alloc/fill/verify/free its own blocks
(64 B, every 10th 2048 B) against the global lock. Full per-rep
lines are in the recorded run; medians of the 3 reps:

| Threads | Cross-thread frees per rep | Pattern fail | Null fail | Contention ns/op (median) | vs 1 thread |
|---|---|---|---|---|---|
| 1 | 1,500 (self-free control) | 0 | 0 | 4,184 | 1.0x |
| 2 | 3,000 | 0 | 0 | 8,275 | 2.0x |
| 4 | 6,000 | 0 | 0 | 16,436 | 3.9x |
| 8 | 12,000 | 0 | 0 | 25,778 | 6.2x |

Totals across the 3 reps: **67,500 cross-thread frees verified,
0 pattern failures, 0 null failures, every rep RESULT: PASS,
probe exit 0.** Same recorded run: `make test` exit 0
(4,590 checks, 0 failed), `nm -D` exactly 4 public symbols,
Samtal HTTP 200 before and after.

Reading the numbers honestly:
- The contention scaling is close to linear in thread count —
  the signature of one serialising lock with queueing, not of
  a correctness problem. This is the baseline H-004 must
  improve and H-008 must re-measure with the same probe shape.
- Phase A wall time is dominated by the mapping path (one
  third of cross blocks are 2048 B mapping allocations whose
  free unmaps), so cross-phase times are reported in the
  recorded run but NOT quoted here as a latency claim; the
  contention leg (Phase B) is the latency baseline.
- These are means over whole legs on a shared home Pi, not
  percentile latencies. No p50/p99 claim is made; percentile
  methodology is H-008's to add.

## 4. Carried register from G-009 (Phase 7 exit packet)

- **O-03 (fork):** inspection-covered only, never executed; the
  single-global-lock fork hazard stands (a child may inherit a
  locked mutex); the header makes no fork claim (U-17..U-21
  open). Owned in this wave by **H-006** with an explicit
  contract decision.
- **O-04 (cancellation):** thread cancellation never exercised;
  POSIX reading only. Owned by **H-006**.
- **O-05 (thread scale):** Phase 7 evidence was bounded at
  <= 8 concurrent threads and ~1.8k allocations. This baseline
  already exceeds the allocation count (22,500 cross + 22,500
  contention ops per rep) at the same <= 8 thread bound;
  **H-007** owns widening repetitions and mixes, and any claim
  beyond 8 threads remains unmade until run.
- **INC-G008-02 / H-001:** the unload drain is H-001's
  commissioned fix, ahead of all Phase 8 content. Until it
  banks, Phase 7 X4/X6 stay PARTIAL; nothing in this slice
  touches that question or its files.

## 5. What is NOT yet known (named, not smoothed over)

1. Memory behaviour under contention: retained spans, RSS, and
   span-registry growth while many threads churn were not
   measured here. (H-008.)
2. Owner death under load: a thread exiting mid-churn while
   others allocate/free was proven in calm shapes by G-006 T2,
   but not at contention scale and not with death timed to
   another thread's free of the dead thread's blocks. (H-005.)
3. Cancellation: whether a thread cancelled between allocator
   calls — or, the dangerous case, with cleanup handlers in
   consumer code that free allocator blocks — behaves sanely
   is entirely unexercised. (H-006, O-04.)
4. Fork: unexecuted; hazard known, posture undecided. (H-006, O-03.)
5. Latency distribution: only whole-leg means exist; tail
   latency under contention is unmeasured. (H-008.)
6. Whether sharding or a bounded remote-free queue actually
   helps on this 4-core host — plausible, unproven, and the
   queue's bound/back-pressure behaviour is a design question,
   not a measurement yet. (H-004.)
7. Interaction of any future per-shard/per-thread state with
   thread exit and with the H-001 unload drain — the two
   places "nothing per-thread exists" is currently load-bearing.
   (H-004 with H-006.)

## 6. Incidents recorded by this slice (INC-H002-NN)

- **INC-H002-01 (contention cost is the measured price of the
  single lock — not a defect, a design datum).** Per-op cost
  scales ~linearly to 6.2x at 8 threads (Section 3). No
  correctness failure accompanies it. Commissioned, per the
  roadmap's own order, to H-004: contention controls first,
  sharding/queue consideration second, judged against this
  baseline with the same probe shape.
- **INC-H002-02 (cross-phase timing is not a latency
  measure).** Phase A mixes pattern verification and
  mapping-path unmaps into its wall time; quoting it as free
  latency would mislead. This plan quotes only Phase B as the
  latency baseline and leaves the raw Phase A lines in the
  recorded run, labelled by construction.
- **INC-H002-03 (H-003's "implement" is, on this evidence,
  "verify and harden").** Cross-thread frees need no
  implementation at this base — they fall out of the
  process-global registries (Section 2) and pass 67,500
  measured frees. H-003's honest work is the counter-reconciled
  regression evidence test and any gap it finds; if H-003
  finds a real gap, it becomes an implementation again. The
  WAVE_PLAN wording stands; this note records what the
  baseline says the work will actually be.

## 7. Slice discipline

No `src/` change; no header change; README untouched
(byte-identical, verified at commit). Files: this doc,
`tests/perf/probe_h002.c`, `tests/perf/recorded_run_h002.txt`,
and one additive Makefile block (`test-h002`, standalone,
`make test` composition unchanged). Public ABI unchanged:
`nm -D` exactly 4 (recorded). Banking order: H-001 first,
this slice second, per the keeper.
