# Phase 8 - Exit Evidence Packet

Slice: **H-009 (team-09)**. Wave 8, final slice. Phase 8 exit
assembly: this packet originates no design and no
implementation; it tallies the Wave 8 evidence
(H-001..H-008) against the exit rows the H-002 foundation
plan defined (S1-S7, E1-E2), re-confirms it with one fresh
recorded run at this base, re-grades the Phase 7 rows the
G-009 packet left conditional on H-001, and carries every
still-open item forward by name.
Base: `de53575` (H-007 banked; Wave 8 = 7 of 9 banked,
H-008 built on its branch, banking owed - Section 3) on
branch `yrsa/mimiralloc-09`, worktree
`/home/pi/mimiralloc-team-09` on Raspberry Pi
`purpleviking1` (aarch64, 4 cores, page size 16384,
Samtal live on port 8787 throughout; `free -g` 15 GiB
total, 13 GiB available before and after the fresh run).
Governing texts: Roadmap Section 8 Phase 8;
`docs/PHASE_8_FOUNDATION_PLAN_H002.md` (H-002, rows
S1-S7 and exit rows E1/E2); the banked slice docs
H-001..H-007 with their INC lists; the built H-008 slice
doc on branch `yrsa/mimiralloc-08` (commit `2856ccf`,
read on that worktree, not present in this tree);
`docs/PHASE_7_EXIT_EVIDENCE.md` (G-009 packet, carried
register); WAVE_PLAN.md in the coordinator checkout.
Fresh run: `tests/core/recorded_run_h009.txt` (this
slice, this base). `README.md` and
`include/mimir_alloc.h` byte-identical to base (sha256
in the recorded run); no Makefile or `src/` change in
this slice.

> **BOTTOM LINE, STATED FIRST.** The Roadmap Phase 8 exit
> sentence is: *"no corrupted live allocations or lost*
> *ownership across repeated stress scenarios; latency*
> *and memory reports archived."* This packet grades it
> **PARTIAL - not a clean exit, for one banking reason
> and one posture reason, both named.** The correctness
> half (E1) is **MET**: across the whole wave and this
> packet's fresh run, no live allocation was corrupted
> and no ownership was lost in any supported scenario -
> cross-thread frees, owner death, deferred cancellation,
> teardown, and repeated stress to 12 threads (3x
> oversubscription) all reconcile exactly. The archive
> half (E2) is **PARTIAL, banking OWED**: the latency and
> memory reports exist in full - H-008, commit `2856ccf`
> on `yrsa/mimiralloc-08`, 4 files / +693 - but they are
> not yet banked into the main tree at this base, so at
> this base the archive is incomplete (INC-H009-01).
> When H-008 banks, E2 can be re-graded on that banking
> and the exit declared. Separately, the cancellation /
> teardown row (S5) is **PARTIAL**: deferred cancellation
> and teardown are proven, while asynchronous
> cancellation and fork-while-busy are **measured
> unsupported** - decided contract postures (H-006), not
> corruption, and not hidden. The commissioned Phase 7
> fix at the head of this wave, **H-001, is banked and
> its discharge is graded in Section 6**: INC-G008-02 is
> fixed (0 B growth per cycle, fresh-reproduced), and
> Phase 7 X4/X6 are re-graded MET.

## 1. Verdict vocabulary and reading rules

- **MET** - the evidence the item specifies exists, in
  the form it specifies, in the banked repo (or, for
  fresh-run items, in this packet's recorded run).
- **UNMET** - some evidence exists, but the item as
  written is not satisfied.
- **PARTIAL** - one named part of the item is met and
  another named part is discharged only in a weaker,
  explicitly recorded sense, or is owed.
- **OWED** - the evidence is wholly absent at this base;
  the row names exactly what is missing.
- Counts are copied from the recorded-run files named
  in each row, never restated from memory or from
  another slice's summary. Where this packet's fresh
  run reproduces a banked count, both are cited.
- H-008 is cited as **built-on-branch** evidence only:
  its counts are quoted from its commit message and its
  slice doc / recorded run as read on the team-08
  worktree (INC-H009-06). Nothing in this packet claims
  H-008 banked.
- Every Wave 8 claim is single-target (Pi-native
  AArch64 Linux/glibc, 16 KiB pages) unless a row says
  otherwise. No target is promoted to "supported"
  (Roadmap Section 5 sense) by anything in this packet.
- Claim labels follow the standing pattern: **PROVEN on
  the Pi** (run or inspected by a named slice),
  **documented** (a banked source says so), **OWED**,
  **BLOCKED**.

## 2. Fresh verification run for this packet (H-009)

One fresh run on the Pi at base `de53575`, recorded in
`tests/core/recorded_run_h009.txt` (1867 lines). Counts
below are this run's only; no banked number is copied
into this section.

| Step | Exit | Fresh result |
|---|---|---|
| `make clean` | 0 | - |
| `make test` | 0 | 8/8 suites PASS; final accounting **4590 checks run, 0 failed** (smoke 18/18, core 29, b007 69, c005 95, c006 23, c007 122, c008 42 with 7 skipped-not-yet, d004 final 4590) |
| `make test-h001` | 0 | **299 checks, 0 failures**; anonymous rw flat at 49,152 B, **0 B growth per cycle**, `img_after_close=0` throughout (dlopen leg 267 + direct leg 32) |
| `make test-h002` | 0 | Probe PASS all configs: cross-thread 1,500 / 3,000 / 6,000 / 12,000 ops at 1/2/4/8 threads, pattern_fail=0, null_fail=0 throughout; this run's contention means 4,158 / 8,236 / 19,451 / 30,081 ns/op (single pass; banked medians in Section 3) |
| `make test-h003` | 0 | **205 checks, 0 failures**; Phase D drain released exactly 13 retained spans, counters reconcile exactly |
| `make test-h004` | 0 | Probe PASS all configs; this run's V0 real 1,863 / 3,065 / 5,622 / 9,995 ns/pair at 1/2/4/8 threads; queue fallbacks recorded per rep (0 at 1T, rising with thread count, INC-H004-02 shape) |
| `make test-h005` | 0 | **41 checks, 0 failures**; final small_alloc == small_free == 2515, mappings published == retired == 2408, live 0/0 |
| `make test-h006` | 0 | **48 checks, 0 failures**; this run's hazard probes: D3 fork-mid-churn tries=1 ok=0 hung=1; D4 async-cancel ok=1 hung=2 of 3 (ratios vary by run, INC-H009-04) |
| `make test-h007` | 0 | All 15 scenarios OK (1/2/4/8/12 threads x 3 reps): **51,840 ops, 0 failures** of any counted kind, audit reconciled every rep, Samtal 200 x15 during the 12-thread legs |
| `make test-g008` | **2** | **214 checks, 1 failure - expected and recorded, not hidden**: the single FAIL is the banked harness's stale growth PASS-check asserting the pre-fix orphan defect, inverted by H-001 (INC-H001-01, Section 6). Every other check passes, including the orphan probe's pattern-intact readings under the decided INC-G008-03 consumer-duty posture. `make test-h001` supersedes this target for unload behaviour |
| `make test-g005` | 0 | **97 checks, 0 failures**; retained spans 40 = 655,360 B (RP-5 worst case, reached exactly) against the drain-bearing core |
| `make lib` + `nm -D` | 0 | Exactly 4 defined dynamic symbols: `mimir_malloc`, `mimir_free`, `mimir_calloc`, `mimir_realloc` |

Integrity, same run: `include/mimir_alloc.h` and
`README.md` sha256-identical to HEAD `de53575`
(header 55107f05..., README bd7f6614..., both matching
`git show HEAD:` in the recorded run); Samtal `GET /`
HTTP 200 before and after the run and 15/15 during the
test-h007 12-thread legs; `free -g` unchanged (13 GiB
available). `make test-h008` does not exist at this
base - the H-008 target and files live only on branch
`yrsa/mimiralloc-08` (Section 3); that absence is the
E2 banking gap, recorded as INC-H009-01.

## 3. Evidence inventory (H-001..H-008)

| Slice | State | Deliverable in one line | Headline evidence |
|---|---|---|---|
| H-001 | **BANKED** `1af7307` | Commissioned fix for INC-G008-02: image destructor + internal `mimir_core_drain_retained` under the existing global lock releases only retained EMPTY spans at unload; live blocks untouched (INC-G008-03 consumer-duty posture DECIDED); drain non-terminal and idempotent (INC-G006-03 re-examined) | test_h001 **299/0**: anonymous rw growth exactly **0 B per cycle** (flat 49,152 B, `img_after_close=0` x50 cycles in the banked run); live-at-unload probe +1 page, pattern intact; `make test` 4590/0, test-g005 97/0; nm -D exactly 4; INC-H001-01..06 |
| H-002 | **BANKED** `7c31df9` | Phase 8 foundation plan + measured cross-thread / contention baseline; rows S1-S7, E1/E2 defined; architecture grep-verified (exactly one mutex, two global registries, no per-thread state) | 3 reps: **67,500 cross-thread frees, 0 pattern failures, 0 null failures**; contention medians (whole-leg) 4,184 ns/op at 1T to 25,778 at 8T (~6.2x, near-linear single-lock scaling); INC-H002-01..03 |
| H-003 | **BANKED** `42ecdb6` | Cross-thread frees verify-and-harden: counter-reconciled regression evidence test, both paths, cross calloc/realloc, drain interplay (Phase D) | test_h003 **205/0**: 12,180 cross-thread frees, 27 counter reconciliations exact, drain released exactly 13 retained spans; **no allocator gap, no src change** (INC-H003-02); INC-H003-01 (the one failure seen in development was the test's own expectation arithmetic, corrected) |
| H-004 | **BANKED** `dacb5e6` | Contention controls: evaluation + measured decision. **Single global lock RETAINED; sharding DEFERRED (reopen trigger: end-to-end sharded prototype beats the real allocator by 2x at 8 threads with the RP-5 bound intact); bounded remote-free queue REJECTED as a standalone control** (62-65% of burst cross frees overflow cap 512 to the direct locked fallback, INC-H004-02), hybrid deferred | Probe models at 1/2/4/8 threads with pattern + count reconciliation, 0 failures; V0/V1/V2 within-run comparisons only (INC-H004-01: absolutes not comparable across slices); sharding would multiply the RP-5 bound per shard (INC-H004-03); no src change |
| H-005 | **BANKED** `bec8aa2` | Owner death handling: counter-reconciled evidence test, death mid-churn under contention, both death forms (return / `pthread_exit`) | test_h005 **41/0**: 12 victim threads across 3 rounds died holding 180 blocks (144 small + 36 mapping); all patterns intact, freed by survivors under renewed contention, counters to baseline every round; **no gap** (INC-H005-01); no src change |
| H-006 | **BANKED** `46110f1` | Thread cancellation + teardown evidence; fork posture DECIDED | test_h006 **48/0**: deferred cancellation between calls proven (Phase A), consumer cleanup handlers freeing allocator blocks proven 30/30 (Phase B), teardown + drain exact (Phase C/E); hazards MEASURED in disposable children: mid-churn fork hung **2 of 3**, async cancellation lock-died **5 of 6** (combined banked runs); fork DECISION: quiescent or fork+exec only; async cancellation UNSUPPORTED posture; INC-H006-01..05; no src change |
| H-007 | **BANKED** `de53575` | Stress at small thread counts + oversubscription, widened mixes (cross-ring / churn / calloc-realloc), independent pattern checks, audit reconciled at quiescent points | probe_h007 at 1/2/4/8/12 threads x 3 reps: **51,840 ops, 0 failures**, audit OK every rep, Samtal 200 x15 during 12-thread legs; no claim beyond 12 threads; no src change; no INC-H007 items (none arose) |
| H-008 | **BUILT, NOT BANKED** `2856ccf` on `yrsa/mimiralloc-08`, parent `de53575` | Latency and memory reports under contention + repeated-stress archive. 4 files / +693: `docs/PHASE_8_LATENCY_MEMORY_H008.md`, `tests/perf/probe_h008.c`, `tests/perf/recorded_run_h008.txt`, Makefile +19 (additive `test-h008`). No src change. **Banking OWED** - the keeper banks in roadmap order; this packet does not claim it banked | Per its commit message and recorded run as read on team-08: per-op percentiles at 1/2/4/8 threads - pair mean 746 ns at 1T rising to 12,564 ns at 8T while pair p50 stays 260-370 ns (**the retained single-lock cost lives in the tail, INC-H008-01**); held-set memory reconciled exactly at quiescent points, live deltas return to 0, a 2048 B request occupies 32,768 B mapped on 16 KiB pages (INC-H008-02), retained spans settle at 6 (98,304 B); archive: `make test` x2 (4590/0 both), test-h007 x3 (51,840 ops 0 failures each), test-h008 x3 (MEM lines byte-identical), all stable, 0 failures; INC-H008-03 (H-002 whole-leg means and H-008 call means are different quantities; no speedup claimed) |

## 4. Exit tally against the H-002 matrix (S1-S7, E1-E2)

| # | Item (H-002 Sections 1/5) | Grade | Evidence in one line |
|---|---|---|---|
| S1 | Implement and verify cross-thread frees | **MET** | H-002 baseline 67,500 frees 0 failures; H-003 counter-reconciled 205/0 (12,180 frees, both paths, calloc/realloc, drain interplay); fresh run re-confirms 205/0 and the h002 probe PASS |
| S2 | Contention controls | **MET** | H-004 executed the roadmap's own order - controls evaluated against the measured baseline and a decision taken and recorded: single lock retained as the control, with the cost re-measured since (H-008: the cost lives in the tail, INC-H008-01). The retained cost is a design datum (INC-H002-01), not an unexamined gap |
| S3 | Then consider sharding or bounded remote-free queues | **MET** | Considered with measurements, not by assertion: sharding DEFERRED with a numeric reopen trigger (2x at 8T, RP-5 intact); queue REJECTED standalone on measured 62-65% burst fallback (INC-H004-02), hybrid deferred with its own trigger |
| S4 | Include owner death | **MET** | H-005 41/0: death mid-churn under contention, both death forms, 180 dead-owned blocks pattern-intact and survivor-freed, counters exact every round; fresh run re-confirms 41/0 |
| S5 | Include thread cancellation and teardown | **PARTIAL** | Deferred cancellation MET (H-006 Phases A/B: between calls, and cleanup handlers freeing blocks 30/30); teardown MET (Phases C/E: leftovers freed from any thread, H-001 drain exact, dlclose unmaps every cycle). PARTIAL because two named shapes are measured **unsupported** and stand as decided postures, not supported behaviour: asynchronous cancellation (5 of 6 lock deaths, INC-H006-02) and fork-while-busy (2 of 3 children hung, INC-H006-01; fork supported quiescent / fork+exec only) |
| S6 | Test at small thread counts and under oversubscription | **MET** | H-007: 1/2/4/8/12 threads x 3 reps on this 4-core host (12 = 3x oversubscription), widened mixes; fresh run reproduces 51,840 ops 0 failures. No claim beyond 12 threads is made anywhere |
| S7 | Verify correctness with stress runs and independent data-pattern checks | **MET** | The H-002 S7 method (verifier re-derives every expected byte itself; the allocator never vouches for its own blocks) is used by H-003/H-005/H-006/H-007 and the H-008 archive; audit counters reconciled at quiescent points in every leg |
| E1 | Exit: no corrupted live allocations or lost ownership across repeated stress scenarios | **MET** | Zero pattern / null / zero / realloc failures and zero counter drift across H-002 (67,500 frees), H-003 (205/0), H-005 (41/0), H-006 supported shapes (48/0), H-007 (51,840 ops), the H-008 archive (make test x2, h007 x3, h008 x3, all stable), and this packet's fresh run. Scope, stated so the grade is not over-read (INC-H009-05): MET covers the supported shapes; the two S5 unsupported shapes fail by hanging (lock death in a child that never returns), never by corrupting a live allocation or losing ownership, and they are excluded by decided posture, not silently untested |
| E2 | Exit: latency and memory reports archived | **PARTIAL - banking OWED** | The reports exist in full: H-002's banked latency baseline (in tree) plus H-008's latency distribution and memory-under-contention reports with the repeated-stress archive. But H-008 is **built on its branch (`2856ccf`), not banked** at this base - its doc, probe, recorded run and `test-h008` target are absent from this tree (verified: Section 2). "Archived" in the banked repo is therefore not yet true of the complete report set (INC-H009-01). When H-008 banks, this row re-grades MET on that banking alone; no further evidence work is owed for it |

**Tally: S1-S4, S6, S7, E1 MET; S5 PARTIAL (decided
unsupported postures); E2 PARTIAL (H-008 banking owed).
Phase 8 exit: PARTIAL until H-008 banks.**

## 5. The two PARTIAL grades, stated in full

### 5.1 E2 - the archive is one banking short

Nothing about H-008's evidence is in doubt in this
packet: its commit (`2856ccf464f69c78390d9e4c29aaa1547295053e`,
parent `de53575`, 4 files / +693, no src change) was
read on the team-08 worktree, its recorded run is 196
lines, and its headline numbers (Section 3) are quoted
from that record. The grade is about where the evidence
*lives*: the exit sentence says reports are "archived",
and in this campaign archived means banked in the main
tree - the standard every prior packet applied (G-009
graded on banked evidence and named its one unbanked
dependency as a commission). At base `de53575` the
tree contains H-002's baseline report only. H-008 banks
next in roadmap order by the keeper; this packet
records the debt as register item R-01, first entry,
and grades E2 PARTIAL until it lands.

### 5.2 S5 - cancellation is proven exactly where POSIX supports it

H-006 executed what G-006/G-009 had left inspection-
only (register O-04). Deferred cancellation - the
POSIX default - is fully proven: cancellation between
allocator calls leaves blocks live and intact for any
thread to free (Phase A), and consumer cleanup handlers
that free allocator blocks during unwinding reconcile
exactly (Phase B, 30/30). Teardown is orderly and
proven (Phases C/E, resting on the H-001 drain). The
two unsupported shapes are not unexamined: both were
measured in disposable children under parent timeouts,
asynchronous cancellation killing the allocator lock
in 5 of 6 children and mid-churn fork hanging 2 of 3
(banked combined runs; this packet's fresh run shows
the same direction with its own smaller samples,
INC-H009-04). H-006 decided both as contract postures
(async cancellation must not be enabled on allocator
threads; fork only quiescent or fork+exec) and made no
code change, because no lock-based allocator survives
either shape and a masking hack would change semantics
for every caller. The row is PARTIAL because
"cancellation" unqualified includes a mode this
allocator does not support - by decision, on measured
evidence, with the decision recorded where a consumer
can find it (H-006 Sections 3-4). No corruption or
lost ownership occurs in either shape (E1, Section 4),
so the PARTIAL is confined to S5.

## 6. Phase 7 re-grade on H-001 (INC-G008-02 discharged)

The G-009 packet graded Phase 7 X4/X6 PARTIAL on one
finding - INC-G008-02, `dlclose` orphaning retained
spans at exactly 49,152 B per cycle - and commissioned
the remedy as H-001 with its evidence test defined:
re-run the G-008 harness shape and record anonymous rw
growth of exactly 0 B per cycle. INC-H001-06 left the
re-grade itself to "the next exit packet's" business.
This is that packet, and the re-grade is:

- **The fix (H-001, banked `1af7307`).** One image
  destructor calls the internal
  `mimir_core_drain_retained()` under the existing
  global lock; it releases only retained EMPTY spans
  through the banked release path. Live blocks are
  deliberately untouched: **INC-G008-03 is DECIDED as
  consumer duty** (a block live at unload stays mapped
  and pattern-intact, +1 page in the probe, with no
  defined free path - the consumer must free before
  unload). INC-G006-03's destructor-order surface was
  re-examined in H-001 Section 4 (interleave cases
  inspection-covered, INC-H001-02, carried at R-04).
- **The evidence.** Banked test_h001 299/0: growth
  exactly 0 B per cycle across 50 cycles, flat 49,152 B,
  `img_after_close=0` throughout, VmRSS flat; drain
  non-terminal and idempotent. This packet's fresh run
  reproduces it at the Wave 8 exit base: **test-h001
  299/0, 0 B growth per cycle**, and test-g005 97/0
  against the drain-bearing core.
- **The stale harness, noted not hidden
  (INC-H001-01).** The banked G-008 harness was written
  to prove the defect's presence byte-exactly; its one
  growth PASS-check now asserts a fixed defect and
  fails - banked 214 checks / 1 failure, reproduced
  exactly by this packet's fresh run (`make test-g008`
  exit 2, single FAIL line: "anonymous rw bytes grow
  across cycles"). `test_g008.c` is deliberately
  unedited (banked evidence); `make test-h001`
  supersedes it for unload behaviour. Re-pointing the
  G-008 target at the new expectations is an
  evidence-policy decision for the keeper or a future
  slice, carried at R-02 - it is not done here, and
  the fresh run's exit 2 is reported in Section 2, not
  smoothed into the PASS column.
- **Re-grades.** Phase 7 **X4 (retained memory vs
  Phase 3): MET** - retention is now bounded in every
  dimension the row covers: in-process RP-5 ceiling
  (G-005, fresh-confirmed 40 spans / 655,360 B) and
  across unload exactly 0 B per cycle (H-001). Phase 7
  **X6 (responsiveness incl. repeated load/unload):
  MET** - the load/unload half that carried the defect
  now cycles cleanly (H-001; G-008 responsiveness
  record itself was never in doubt). **X5 (retained
  memory vs platform allocator) stays PARTIAL** for
  its own recorded reasons, which H-001 does not touch:
  the platform side rests on process-level RSS with no
  allocator-attributable instrument (G-009 O-14,
  carried at R-09). Phase 7's exit therefore stands
  PARTIAL on X5 alone; that remainder is a measurement-
  instrument debt, not a defect, and it does not bear
  on any Phase 8 row.

## 7. Contention story (summary)

One global lock is the design, and Wave 8 priced it
three times without ever finding a correctness failure
attached to it. H-002's whole-leg baseline: mean pair
cost ~linear in thread count, 4,184 ns/op at 1T to
25,778 at 8T (~6.2x). H-004 judged the alternatives as
models (lock cost only, honestly labelled): striped
locks and a deferred-free queue both cut model lock
cost sharply, and both were declined on measured
grounds - the queue overflows its bound on 62-65% of
burst cross frees, and sharding multiplies the RP-5
retention bound and voids the no-per-thread-state
proofs it would have to re-earn; the decision carries
numeric reopen triggers instead of a closed door.
H-008 (built) then located the retained cost inside
the distribution: pair p50 flat at 260-370 ns from 1 to
8 threads while the mean rises ~17x and p99 ~10x - the
single lock charges the tail, not the median, and any
future contention work must be judged on tail
reduction (INC-H008-01). Absolute numbers are never
compared across slices (INC-H004-01; this packet's
fresh h002/h004 absolutes differ from the banked
medians in the expected direction, INC-H009-03).

## 8. Memory story (summary)

Under contention the allocator's memory behaviour is
counter-exact at every quiescent point the wave
measured: H-003's reconciliations after every phase,
H-005's return to baseline after every death round,
H-007's audit after every one of 15 scenarios, and
H-008's (built) held-set leg - live deltas return to
0, retained spans settle at 6 (98,304 B) regardless of
held-set size, bounded by warmed classes under RP-5,
never by workload size. Two measured costs stand
recorded, neither a defect: mapping-path amplification
on this 16 KiB-page host (a 2048 B request occupies
32,768 B mapped, INC-H008-02 - the S7.1 one-header-page
construction, Phase 11 fodder at R-07), and the
in-process retention ceiling itself (655,360 B worst
case, G-005), which across unload is now fully drained
(H-001, Section 6). No idle-time reaper exists; that
remains a stated Phase 9 property (G-009 O-12, R-08).

## 9. Carried-owed register

Nothing in this register is silently dropped. R-01
heads it: it is the only item standing between this
packet and a clean Phase 8 exit. "Closed here" means
this packet itself discharges the item, with the
reason stated. G-009 register numbers (O-NN) are cited
where an item continues one.

| # | Owed / carried item | Source | Future home / trigger |
|---|---|---|---|
| **R-01** | **H-008 BANKING OWED.** H-008 (latency/memory reports + repeated-stress archive) is built as commit `2856ccf` on `yrsa/mimiralloc-08` (parent `de53575`, 4 files / +693) and not yet banked into main. Until it banks, E2 stays PARTIAL and Phase 8 stays unexited. No evidence work is owed - banking only, in roadmap order, by the keeper | This packet, Sections 3/5.1 | **The keeper, next banking in order (ahead of this packet)** |
| R-02 | test-g008 stale PASS-check: the banked harness asserts the pre-fix orphan growth and now fails 214/1 by design of the fix (fresh-reproduced, Section 6). Re-pointing the target at the post-fix expectations is an evidence-policy decision; `test_g008.c` stays byte-identical until someone with that authority decides | INC-H001-01 | The keeper, or a future evidence-policy slice |
| R-03 | Sharding DEFERRED, not closed: reopen only when an end-to-end sharded prototype beats the real allocator by 2x at 8 threads with the RP-5 bound intact (per-shard bounds would otherwise multiply RP-5, INC-H004-03). Queue hybrid DEFERRED with its own trigger: an interleaved (non-burst) workload measurement with queue occupancy below cap and < 10% fallbacks across 3 recorded runs, plus thread-exit and H-001 drain designs for queued blocks | INC-H002-01; INC-H004-01..03 | A future contention slice meeting a trigger; H-008's tail finding (INC-H008-01) says any such work is judged on tail reduction |
| R-04 | Destructor-order interleave (consumer / TLS destructors, atexit handlers vs the H-001 drain) rests on inspection plus the drain's executed idempotence; no executed interleave harness exists | INC-H001-02 (continuing G-009 O-02, re-examined in H-001) | A future teardown slice, if ever commissioned |
| R-05 | Fork support beyond the decided shapes (quiescent / fork+exec) would need a `pthread_atfork` design H-006 judged unsound to bolt on; src/ still registers no atfork handler. Async cancellation on allocator threads remains an unsupported posture, permanently, unless the locking discipline is redesigned | INC-H006-01..03 (closing G-009 O-03/O-04 as DECISIONS in H-006) | A future contract slice that wants fork-while-busy promoted; the public header still states no fork claim (U-17..U-21) |
| R-06 | Thread evidence scale bound is now 12 concurrent threads (H-007) and the H-005/H-006 shapes at <= 7 / <= 4 workers; no claim beyond 12 threads exists or is implied | INC-H006-05; G-009 O-05 as widened by H-007 | Any future stress slice that runs beyond 12 |
| R-07 | Mapping-path memory amplification on large-page hosts (2048 B request = 32,768 B mapped) is a measured cost of the S7.1 construction, recorded for performance research; no change proposed | INC-H008-02 | Roadmap Phase 11, with evidence |
| R-08 | No idle-time reaper (in-process face): a warmed class keeps its retained spans for the life of the process, bounded by RP-5. Stated property by design; its unload face is fixed (H-001) | INC-G005-03 (G-009 O-12) | Roadmap Phase 9 (trimming and cache release are named there) |
| R-09 | Exit-grade retained-memory comparison vs the platform allocator (Phase 7 X5) still needs an allocator-attributable instrument for the platform side; RSS-only readings are what exist. Phase 7 exit stands PARTIAL on X5 alone (Section 6) | INC-G001-06; INC-G005-02 (G-009 O-14) | A future measurement slice; Phase 10 counters are the natural instrument on the mimir side |
| R-10 | Fault-injection debts unchanged: drain lock-failure branch (INC-H001-03), tuning lock-failure paths, RP-3 rollback branch and the R-1 backing-failure sub-case all rest on inspection; no injection hook exists | INC-H001-03; INC-G007-05 (G-009 O-08) | A fault-injection slice, discharging them together |
| R-11 | Multithreaded tuning (setting the retention bound while other threads allocate) still unmeasured; race-free by lock construction only | INC-G007-06 (G-009 O-09) | A measurement leg in a future tuning or responsiveness slice |
| R-12 | Public tuning ABI and public shutdown surface both remain proposals for a future contract slice settling U-03; neither was promoted by Wave 8 | INC-G007-01; INC-H001-06 (G-009 O-07) | Future contract slice |
| R-13 | Two cosmetic tidies verified still present at this base by this slice: `tests/core/test_g005.c` line 18 and the G-005 doc still name the retired constant `MIMIR_MAX_EMPTY_SPANS_PER_CLASS` (INC-G007-08, G-009 O-10); `tests/core/test_g004.c` line 63's stale INC citation (INC-G004-06, G-009 O-11). This packet may not fix them: its scope is exactly two files and editing banked evidence files is outside it (INC-H009-07 pattern, as INC-G009-03 before it) | INC-G007-08; INC-G004-06 | The keeper, or the next slice that touches those files for cause |
| R-14 | Runtime size histogram still unmeasured; the class table stays census/grid-derived. B-008 "extent" wording reconciliation, alignment-floor D-008 legs (musl BLOCKED, bionic OWED), and the signal-context posture all stand exactly as G-009 carried them (O-15..O-18, O-21) | G-009 register | Their own recorded resumption points |
| R-15 | **Phase 3 exit UNMET**: the x86-64 suite run is still OWED | C-009 packet, carried by G-009 O-22 and every packet since | An x86-64 sysroot + suite run; a 4096-page environment run |
| R-16 | **Phase 4 exit UNMET**: physical-device allocate/fill/free + multithread tests on Android still OWED | E-009 O-16, carried by G-009 O-23 | An Android NDK (install reserved for Volmarr's word), an NDK build, and a physical device run |
| R-17 | **Phase 1 exit unmet; Phase 6 Clause A UNMET / X1 BLOCKED** (blocker-record alternative only) - restated by G-001/G-009 and carried unchanged; nothing in Wave 8 discharges any of it. Phase 7 exit stands PARTIAL on X5 only (Section 6) | G-009 O-24 | Their own recorded resumption points |

**Closed here:** the G-009 register's head commission
(H-001, the unload drain) is DISCHARGED - banked as
`1af7307`, graded in Section 6. G-009 O-01 (live blocks
at `dlclose`) is closed as a DECISION (consumer duty,
H-001). G-009 O-03 (fork) and O-04 (cancellation) are
closed as DECISIONS by H-006 (R-05 carries only the
future-promotion question). G-009 O-06 (tuning cap 16)
was already closed as decided by G-009 itself.

## 10. Bottom line

Phase 8 built what the roadmap asked it to build, and
- at the head of the wave - repaired what Phase 7 had
left open. The commissioned unload drain (H-001)
landed first and is proven to zero growth. Cross-
thread frees, which the baseline showed needed proof
rather than implementation, got counter-exact proof
(H-002, H-003). Contention controls were evaluated
with measurements and decided, not gestured at: the
single lock stays, its price is known three ways, and
the alternatives carry numeric reopen triggers
(H-004, H-008). Owner death (H-005), cancellation and
teardown (H-006), and stress to 3x oversubscription
(H-007) all executed the shapes earlier phases had
only read about, with independent pattern checks and
exact counter reconciliation throughout - and this
packet's fresh run re-confirms every banked count at
the exit base (Section 2).

The exit is nevertheless **PARTIAL, not clean**, for
the two named reasons: **E2's archive is one banking
short** (H-008 built, not banked, R-01) and **S5's
cancellation row carries two measured-unsupported
shapes as decided postures** (Section 5.2). Neither is
a correctness failure: across every supported,
repeated stress scenario in the wave, no live
allocation was corrupted and no ownership was lost
(E1 MET). The roadmap sentence offers no blocker-
record alternative for Phase 8, and this packet does
not invent one. The honest verdict is: **Phase 8 exit
PARTIAL - E1 MET, E2 PARTIAL on banking owed, S5
PARTIAL on decided postures - with the exit declarable
on H-008's banking, which is the keeper's next step
in roadmap order.** Phase 7's conditional rows are
re-graded in Section 6 (X4/X6 MET on H-001; X5 the
sole remaining Phase 7 PARTIAL, an instrument debt).
Carried debts from earlier phases stand exactly as
before (R-15..R-17): Phase 3 exit UNMET, Phase 4 exit
UNMET, Phase 1 exit unmet, Phase 6 met via the
blocker-record alternative only.

## 11. Inconsistencies recorded by this slice (INC-H009-NN)

- **INC-H009-01 (the E2 grade is a banking-state
  grade, stated openly).** H-008's evidence is
  complete and was read in full on its branch; E2 is
  PARTIAL solely because that evidence is not yet
  banked into the tree this packet grades. A reader
  who counts built-on-branch evidence as archived
  would grade E2 MET on the same record; this packet
  applies the campaign's banked-evidence standard
  (G-009's), names the difference, and notes that the
  keeper's H-008 banking removes the divergence with
  no further evidence work.
- **INC-H009-02 (the fresh run's one red target is a
  stale assertion, reproduced and reported, not
  hidden).** `make test-g008` exits 2 in the fresh run
  (214 checks, 1 failure) solely on the growth
  PASS-check INC-H001-01 recorded when H-001 banked.
  The packet reports the exit in Section 2's table
  alongside the passes. The harness is banked evidence
  and stays unedited (R-02); `make test-h001` (299/0
  fresh) is the live unload evidence.
- **INC-H009-03 (fresh single-run timing absolutes
  differ from banked medians, by design of the
  evidence).** The fresh h002 contention means
  (4,158 / 8,236 / 19,451 / 30,081 ns/op) and h004 V0
  medians (1,863 / 3,065 / 5,622 / 9,995 ns/pair) differ
  from the banked H-002/H-004 medians while the scaling
  shapes agree. Per INC-H004-01, absolutes on this
  shared host are not comparable across runs or
  slices; this packet cites each run's numbers only
  for that run and claims agreement in shape only.
- **INC-H009-04 (hazard ratios vary by run).** H-006's
  banked combined ratios (fork-mid-churn hung 2 of 3;
  async cancel lock-died 5 of 6) and this packet's
  fresh samples (D3: 1 hung of 1 try; D4: 2 hung of 3)
  differ as ratios while agreeing in direction and in
  the underlying mechanism (a child inheriting a locked
  mutex; a lock left held by async cancellation).
  Per INC-H006-05 the ratios are observations on a
  shared host, not probabilities; the postures rest on
  the mechanism, which every run reproduces.
- **INC-H009-05 (E1's MET is scoped, recorded so it
  is not over-read).** E1 grades the supported shapes.
  The two unsupported shapes (S5) fail by hanging, in
  disposable children, without corrupting any live
  allocation or losing ownership - which is why they
  grade inside S5 and do not lower E1. A reader who
  scopes the exit sentence to every conceivable
  threading shape would grade E1 PARTIAL on the same
  evidence; this packet records that the difference is
  a scoping reading and that H-006's decided postures
  are the campaign's answer to it.
- **INC-H009-06 (H-008's counts are quoted from its
  branch record, a weaker citation than every other
  row's).** All other rows cite files in this tree or
  this packet's fresh log. H-008's numbers are quoted
  from commit `2856ccf`'s message and its doc/recorded
  run as read on `/home/pi/mimiralloc-team-08`. They
  are labelled built-on-branch wherever used, and the
  keeper's banking verification (byte-identical files,
  its own re-run) is the point at which they gain the
  same standing as the banked rows.
- **INC-H009-07 (two tidies owed toward this packet
  could not be discharged by it).** INC-G007-08 and
  INC-G004-06 both allow a future packet as a home,
  but this packet's scope is exactly two files with no
  test-file edits; both items are verified still
  present at this base (Section 9, R-13) and carried
  rather than fixed out of scope or silently dropped
  (the INC-G009-03 pattern).

## 12. Files of this slice

- `docs/PHASE_8_EXIT_EVIDENCE.md` - this packet (new).
- `tests/core/recorded_run_h009.txt` - the fresh run
  (new, 1867 lines).

Exactly two files, one commit on `yrsa/mimiralloc-09`
with parent `de53575`. No Makefile change, no `src/`
change, no test change. `README.md` and
`include/mimir_alloc.h` byte-identical to base,
verified in the recorded run. No push, no merge - the
keeper banks H-008 first, then this packet last, and
completes Wave 8.
