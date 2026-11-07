# Phase 8 — Owner Death Handling (Slice H-005)

Status: MEASURED on this host (Raspberry Pi, aarch64, 4 cores,
16 KiB pages) in `tests/core/recorded_run_h005.txt` (real run,
Sat Nov 7 03:42 EST 2026, base dacb5e6, branch yrsa/mimiralloc-05).
Plain verdict: **owner death under contention already works at
this base — no gap found, no hardening performed, no `src/`
change.** Across 3 rounds, 12 victim threads died mid-workload
(half by return, half by `pthread_exit`) while churners kept
allocating and freeing against the one global lock; all 180
blocks they died holding (144 span-path small + 36 mapping-path
large) survived with patterns intact, were verified and freed
by surviving threads under renewed contention, and the audit
counters reconciled exactly to zero live at the end of every
round. This extends G-006's calm-shape exit-while-holding proof
to the shape H-002 Section 5 item 2 named unexercised: death
mid-churn under contention, timed against other threads' frees
of the dead threads' blocks.

Companions: `docs/PHASE_8_FOUNDATION_PLAN_H002.md` (S4 + Section
5 item 2, this slice's governing items),
`docs/PHASE_7_THREAD_EXIT_G006.md` (calm-shape proof extended
here), `docs/PHASE_8_CROSS_THREAD_FREES_H003.md` (the
counter-reconciled evidence method followed here), roadmap
Phase 8 in `docs/MimirAlloc_Research_and_Development_Roadmap.md`.

## 1. What was run (test_h005, standalone `make test-h005`)

Per round (3 rounds), at most 7 concurrent threads (<= 8 bound):

- **D1 death under contention.** 3 churner threads each run 300
  alloc/fill/verify/free iterations (128 B / 4096 B) while 4
  victim threads allocate in two batches — 6 small, then 20 own
  alloc/free churn cycles, then 6 more small + 3 large — fill
  every kept block with a seeded pattern, and DIE still holding
  all of them. Victims 0/2 die by return, victims 1/3 by
  `pthread_exit`, identical shapes. Per victim: 8x64 B + 4x256 B
  (span path, <= 1024) + 3x2048 B (mapping path, > 1024) =
  12 small + 3 mapping blocks. Churners completed 900 ops per
  round with 0 failures — the deaths happened while the lock
  was genuinely contended, not at a quiescent edge.
- **D2 held-by-the-dead reconciliation (quiescent).** After all
  joins: live_small delta == 48 and live_mappings delta == 12
  exactly in every round, and the main thread verified all 60
  dead blocks' patterns (bad = 0). Both death forms are pinned
  to the same delta by the exact total plus per-victim pattern
  checks (identical shapes, as in G-006 T4).
- **D3 survivors free the dead, under renewed contention.** 2
  freer threads each independently re-derive every pattern of
  their victims' blocks, verify, then free — while 2 fresh
  churners run 300 iterations each. Freers freed exactly 60
  blocks per round, pattern-bad victims 0, churn failures 0.
  This is the death-timed-against-cross-thread-free shape:
  frees of a dead owner's blocks proceed concurrently with
  unrelated allocation traffic.
- **D4 reconciliation (quiescent).** Counters return exactly to
  baseline every round (live_small 0, live_mappings 0, live
  spans 3 <= 40 RP-5 bound). A lost block would leave a live
  remainder; a double free would corrupt a pattern or break
  the exact equality — neither occurred.
- **D5 final accounting.** Live small == 0, live mappings == 0,
  `small_alloc == small_free` (2515 == 2515) and mapping
  published == retired (2408 == 2408) exactly; a fresh small +
  large pair still allocates after all deaths.

Totals: **41 checks, 0 failures, RESULT: PASS**; 180 dead-owned
blocks (144 small + 36 mapping) freed by survivors; 5,400 churn
ops across both legs and all rounds, 0 failures.

Same recorded run: `make clean` exit 0, `make test` exit 0
(4,970 checks across the CHECKS-summed suites + SMOKE 18/18,
0 failed), `make lib` exit 0, `nm -D` on build/libmimiralloc.so
exactly 4 public symbols (mimir_malloc, mimir_free,
mimir_calloc, mimir_realloc), README.md and
include/mimir_alloc.h sha256-identical to HEAD, Samtal
HTTP 200 before and after.

Counter discipline (E-005, as G-006/H-003): audit counters are
read only on the main thread at quiescent points (every thread
of the leg joined). No counter was sampled mid-flight for a
verdict; contention is evidenced by the churners' completed
op counts and zero failures, not by mid-flight counter reads.

## 2. Verdict per H-002 foundation-plan item, for H-005

| Item | Verdict |
|---|---|
| S4 Include owner death (H-005) | **MET at this slice's scale.** Death mid-workload under contention, both death forms, both allocation paths, survivors freeing the dead threads' blocks cross-thread after death under renewed contention, exact reconciliation every round, 3 rounds. |
| Section 5 item 2 (owner death under load, death timed to another thread's free) | **Exercised and closed as a gap.** It was the last unrun owner-death shape; it now has a counter-reconciled regression test (`make test-h005`). |
| E1 No corrupted live allocations or lost ownership | **Leg strengthened, exit claim not graded here.** 0 corrupted blocks and 0 lost ownership at this scale; H-009 grades E1 on the full wave's evidence (H-006/H-007 still owed). |
| S5 Cancellation and teardown | Not H-005's. H-006 owns it; still unexercised. A dying thread here exits voluntarily (return / pthread_exit); no cancellation is claimed. |
| S6 Oversubscription / S7 stress + pattern method | Seeded only: <= 7 concurrent threads on a 4-core host is under 2x oversubscription. H-007 owns widening repetitions and mixes. |
| E2 Latency/memory reports | Not H-005's. H-008 owns; no latency or memory claim is made by this slice. |

Why death is a non-event here, restated on this slice's own
evidence: the core holds no per-thread state (H-002 Section 2,
grep-verified at that base and unchanged since — no `src/`
change in H-003/H-004/H-005), so a thread's death has nothing
to drain and no ownership record to orphan; live blocks sit in
the two process-global registries until any thread frees them.
H-005's contribution is proving that remains true when the
death lands mid-churn under lock contention and the frees land
under contention too — the two timings G-006 did not exercise.

## 3. Incidents recorded by this slice (INC-H005-NN)

- **INC-H005-01 (no allocator gap found — no hardening
  performed).** No `src/` change was needed or made: no pattern
  corruption, no lost or double-retired block, no counter
  drift, in any of the 3 rounds. Per the C-006/G-005/H-003
  proof-slice precedent, a proof slice with no src change is
  the valid outcome; inventing hardening work against this
  evidence would be dishonest. If a later slice (H-006
  cancellation, H-007 stress beyond 8 threads) exercises a
  shape this test does not, a gap found there reopens
  hardening on its own evidence.

## 4. Honest Phase 8 exit status

**Phase 8 is NOT exited.** H-005 completes only the S4
owner-death leg. Remaining in this wave: H-006 (cancellation,
teardown, fork posture O-03), H-007 (stress widening), H-008
(latency + memory reports), H-009 (exit packet grading E1/E2).
No claim beyond 7 concurrent threads is made; no cancellation,
fork, latency, or memory-under-contention claim is made by
this slice.

## 5. Slice discipline

No `src/` change; no header change; README untouched
(byte-identical, sha256-verified against HEAD in the recorded
run, as is include/mimir_alloc.h). Files: this doc,
`tests/core/test_h005.c`, `tests/core/recorded_run_h005.txt`,
and one additive Makefile block (`test-h005`, standalone,
`make test` composition unchanged). Public ABI unchanged:
`nm -D` exactly 4 (recorded). Banking order: H-005 banks
after H-004, per the keeper.
