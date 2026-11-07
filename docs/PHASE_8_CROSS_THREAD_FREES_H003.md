# Phase 8 — Cross-Thread Frees: Verify-and-Harden (Slice H-003)

Status: VERIFIED on this host (Raspberry Pi, aarch64, 4 cores,
16 KiB pages) in `tests/core/recorded_run_h003.txt` (real run,
2026-11-07, base 7c31df9, branch yrsa/mimiralloc-03).
Plain verdict: **cross-thread frees are correct at this base on
both allocator paths, under counter-exact reconciliation, and
no hardening was needed — this slice changes no `src/` code.**
The H-002 baseline probe (67,500 cross-thread frees, 0 failures,
no counter reconciliation) is now backed by a regression
evidence test (`tests/core/test_h003.c`, standalone
`make test-h003`) that reconciles every audit counter exactly
after every phase: **205 checks, 0 failures**, 12,180 frees in
the cross-thread phases with 0 pattern failures, 0 null
failures, 0 calloc-zero failures and 0 stay-pointer failures.

Companions: `docs/PHASE_8_FOUNDATION_PLAN_H002.md` (governing
plan; item S1 and INC-H002-03 own this slice),
`docs/MimirAlloc_Research_and_Development_Roadmap.md` Phase 8,
`docs/PHASE_7_UNLOAD_DRAIN_H001.md` (the H-001 drain whose
interplay Phase D exercises).

## 1. What was verified (test_h003, all counts from the recorded run)

Thread shapes: 2, 4 and 8 threads (8 is 2x oversubscription on
this 4-core host), 3 rounds per configuration, four phases per
round. Every block is filled with a seeded byte pattern by its
allocating thread and verified byte-for-byte by a DIFFERENT
thread before its free; the freeing thread re-derives the seed
from (owner, index) itself — the allocator is never asked to
vouch for its own blocks (H-002 S7 method).

- **Phase A — cross-thread free, both paths.** Each thread
  allocates 234 blocks cycling over six small classes
  (16/64/128/256/512/1024, span path) and three large sizes
  (2048/4096/8192, mapping path); after a barrier each thread
  verifies and frees the NEXT thread's blocks (ring).
  Per round: 468 / 936 / 1,872 frees at 2 / 4 / 8 threads;
  9,828 Phase A frees across the run.
- **Phase B — cross-thread calloc.** Each thread callocs 24
  blocks alternating small (16x16 = 256) and large
  (256x16 = 4096); the owner verifies the calloc zero promise
  byte-for-byte, fills a pattern, and the next thread verifies
  and frees. 1,008 calloc blocks across the run, 0 zero
  failures.
- **Phase C — cross-thread realloc**, each block realloc'd by
  a different thread than its allocator, three shapes:
  C1 grow small -> large (256 -> 2048, must move; preserved
  prefix verified by the reallocating thread; owner verifies
  the full new pattern and frees); C2 grow large -> large
  (2048 -> 8192, same shape); C3 stay within class
  (100 -> 112, class 112: the banked span rule returns the
  SAME pointer with no counter movement — verified explicitly,
  0 stay-pointer failures). 1,344 reallocs and 1,344 frees
  across the run.
- **Phase D — retained spans + the H-001 unload drain.**
  After all rounds: live small objects == 0 and live mappings
  == 0 while retained EMPTY spans remain. The main thread
  allocates one block in each of four small classes
  (32/256/768/1024); a single worker thread verifies and frees
  all four cross-thread; 13 retained empty spans were live at
  that point and `mimir_core_drain_retained()` released
  exactly 13, bringing span published == retired; a second
  drain returned 0 (idempotent) and the allocator stayed fully
  usable afterwards.

Reconciliation (E-005 discipline, the part the H-002 probe did
not have): after EVERY phase of every round, the deltas of the
L1 call counters, the E-005 mapping counters and the G-003
small-object counters equal the exact counts the test computes
from the operations it performed — all 27 phase reconciliations
passed — and the cumulative totals reconcile exactly:
malloc 11,172, calloc 1,008, realloc 1,344, free 12,180;
small alloc == small free; mapping published == retired.

Same recorded run: `make clean` exit 0, `make test` exit 0
(all prior suites PASS; 4,970 checks across the CHECKS-summed
suites, 0 failed), `make lib` exit 0, `nm -D` on
build/libmimiralloc.so exactly 4 public symbols, Samtal
HTTP 200 before and after.

## 2. Verdict per H-002 foundation-plan item, for H-003

| Item | Verdict |
|---|---|
| S1 Implement and verify cross-thread frees | **MET on the verify side; no implementation was needed.** Behaviour falls out of the process-global registries (H-002 Section 2) and now carries a counter-reconciled regression test at 2/4/8 threads x 3 rounds on both paths, plus cross-thread calloc/realloc and the retained-span/drain interplay. |
| S7 Independent data-pattern checks (method) | **Applied.** Every verification in this test is independent in the required sense (freeing thread re-derives the seed; counters are read against expectations the test computes itself). H-007 owns widening this to stress scale. |
| E1 No corrupted live allocations or lost ownership | **Baseline leg strengthened, exit claim not graded here.** 0 failures and exact reconciliation at this slice's scale; H-009 grades E1 on the full wave's evidence (H-005/H-006/H-007 still owed). |
| S2/S3 Contention controls, sharding/queue | Not H-003's. Untouched; H-004 owns them against the H-002 baseline. |
| S4/S5 Owner death; cancellation/teardown | Not H-003's. H-005 / H-006 own them; still unexercised. |
| S6 Oversubscription | Seeded only: 8 threads = 2x oversubscription here, as in H-002. H-007 widens. |
| E2 Latency/memory reports archived | Not H-003's. H-008 owns; memory-under-contention remains unmeasured. |

## 3. Incidents recorded by this slice (INC-H003-NN)

- **INC-H003-01 (the slice's one real failure was in the
  test's own expectation arithmetic, not in the allocator).**
  The first `make test-h003` run failed 10 of 205 checks: every
  Phase C mapping-delta check (9) plus the cumulative core
  reconciliation. The allocator's actual published delta was
  192 at 8 threads against the test's expected 128: the
  expectation had counted only the grows' new mappings and
  omitted the initial C2 large mallocs (2048 B), which publish
  one mapping each at allocation time. The expectation was
  corrected (initial large mallocs + grows), the comment
  arithmetic was fixed to match, and the rerun passes
  205/205 with the allocator untouched throughout. Recorded
  here because the failing-then-passing evidence is the
  slice's proof that the reconciliation has teeth: it caught
  a real miscount the moment one existed.
- **INC-H003-02 (no allocator gap found — no hardening
  performed).** No `src/` change was needed or made: no
  pattern corruption, no lost or double-retired block, no
  counter drift, no stay-case violation, and the drain
  interplay is exact. Per the C-006/G-005 precedent, a proof
  slice with no src change is the valid outcome; inventing
  hardening work against this evidence would be dishonest.
  If a later slice (H-005/H-006/H-007) exercises a shape this
  test does not — owner death mid-churn, cancellation,
  oversubscription beyond 8 threads — a gap found there
  reopens hardening on its own evidence.

## 4. Honest Phase 8 exit status

**Phase 8 is NOT exited.** H-003 completes only the S1
verify-and-harden leg. Remaining in this wave: H-004
(contention controls; sharding/queue decision), H-005 (owner
death), H-006 (cancellation, teardown, fork posture O-03),
H-007 (stress widening), H-008 (latency + memory reports),
H-009 (exit packet grading E1/E2). No claim beyond 8 threads
is made; no latency or memory-under-contention claim is made
by this slice.

## 5. Slice discipline

No `src/` change; no header change; README untouched
(byte-identical, sha256-verified against HEAD in the recorded
run, as is include/mimir_alloc.h). Files: this doc,
`tests/core/test_h003.c`, `tests/core/recorded_run_h003.txt`,
and one additive Makefile block (`test-h003`, standalone,
`make test` composition unchanged). Public ABI unchanged:
`nm -D` exactly 4 (recorded). Banking order: H-003 banks
after H-002, per the keeper.
