# Z13 Wave 5 Verdict

Slice: Z-045 - Wave 5 verdict (roadmap docs/MIMIRALLOC_Z13_ROADMAP.md).
Built 2026-11-12 by team-05 (lead + the six Mythic Engineering roles
run sequentially by the lead - see the team note in section 7; the
platform depth available to this lead did not permit six parallel
device-working sub-agents, so the roles were executed in sequence
and are recorded plainly here and in CAMPAIGN_STATE.md).
Base: merge d4343e1 of seat main d0185ab (banked Z-001..Z-037 +
Z-033 closure + Z-029 fix, ls-remote verified by the keeper) into
yrsa/mimiralloc-team-05.

STATUS: BUILT WITH OWED SECTIONS (the Z-036 pattern). Every number
in the FINAL sections is traced to a recorded run that has LANDED
(committed on its team branch or banked at the seat). Two Wave 5
slices had not landed at build time - Z-038 (arena ladder) and
Z-043 (producer/consumer free storm) - and every section that
depends on them is marked OWED, with the reason named. This doc is
amended, not rewritten, when they land; the keeper re-musters this
team for a fold-in commit before Z-045 banks. No arena number,
no free-storm number, and no other unlanded figure is invented,
estimated, or paraphrased anywhere in this document. A staged,
uncommitted Z-038 draft exists in team-06's index; it is NOT
evidence and none of its contents are used here.

## Scope and honesty - read first

- Wave 5 (Z-037..Z-045) is the thread-scaling hunt: KL-B (multi-
  thread scaling under the one global lock, L-05) attacked at
  16 cores / 32 threads on this machine.
- The roadmap's Z-045 text asks for: scaling curves 1..32 threads,
  three allocator variants, per workload; KL-B's standing restated
  in numbers. The three variants of record (per Z-038's gate) are
  the banked core, the arena variant, and glibc. The arena curve
  is OWED (Z-038); the banked-core and glibc curves are FINAL from
  landed runs, and the pre-Wave-4 original core's curve (Z-020) is
  carried as the wave's before-picture, FINAL.
- Load regimes differ per source and are stated per section:
  Z-020 ran on a CONTENDED shared host (its internal shape is the
  claim); Z-039's legs ran on a QUIET machine (its numbers may be
  cited as this-machine numbers); Z-040's primary run started
  quiet and was contended by a sibling suite mid-session (its
  verdicts rest on within-run ratios); Z-041 ran in a near-quiet
  window; Z-042 in a light shared window; Z-044 under recorded
  shared load (a sibling team's Z-043 storm was the top CPU
  consumer at its start). Cross-session comparisons between
  ladders are SHAPE evidence only, never verdicts - the
  discipline Z-031 and Z-039 themselves apply.
- glibc ladder medians on this host are drift-noisy by the
  campaign's own repeated finding (Z-021: 17 of 24 rungs
  DRIFT-REJECT; Z-039: most glibc rungs flagged). Where this doc
  quotes a glibc median it carries the drift flag of its source;
  the claims that rest on glibc are the ORDER of the gap and the
  directions that hold in every instance, never an exact factor.

## Purpose and gate

Roadmap gate for Z-045: "verdict doc traceable to recorded runs."
This document is that verdict for the landed part of Wave 5.
Gate status: MET for every landed slice (sections 2-5 FINAL);
OWED for the arena curve (Z-038), the Z-039 geometry decision
that Z-038 triggers, and the free storm (Z-043) - section 7
lists the exact fold-in owed before this slice banks.
Standing correctness gate at this build: scripts/z13/run_gate.sh
GREEN 4988 checks / 0 failed / 77 by-design skips on the merged
branch (log hidden_files/z045_gate.log) - identical to the
Z-001 baseline; this slice touches no code.

## Host of record

purpleraider (ASUS ROG Flow Z13, GZ302EA), AMD Ryzen AI MAX+ 395
(Strix Halo): 16 cores / 32 threads, 2 CCDs (CCD0 = CPUs 0-7 +
siblings 16-23; CCD1 = CPUs 8-15 + siblings 24-31, per Z-012),
single NUMA node (UMA), L3 64 MiB (2 x 32 MiB), 128 GB LPDDR5X,
4 KiB base pages, THP madvise. Kubuntu 26.10, kernel
7.3.0-10-generic, glibc 2.44, governor performance. Full header:
scripts/z13/host_truth.sh (Z-002), embedded verbatim in every
recorded run cited.

## 1. Evidence base - landed vs OWED

LANDED (committed; read first-hand from disk by this lead):
| Slice | State at build | Commit | Recorded run |
|---|---|---|---|
| Z-037 arena prototype | BANKED at seat | 25322cd (seat) | tests/perf/recorded_run_z13_z037.txt |
| Z-039 second geometry | committed, team-08 | e5bf321 | tests/perf/recorded_run_z13_z039.txt |
| Z-040 false-sharing purge | committed, team-09 | 8925723 | tests/perf/recorded_run_z13_z040.txt |
| Z-041 SMT + pinning | committed, team-01 | 48f602b | tests/perf/recorded_run_z13_z041.txt + docs/Z13_SMT_PINNING_POLICY.md |
| Z-042 thread-churn storm | committed, team-03 | 4490903 | tests/perf/recorded_run_z13_z042.txt |
| Z-044 fork posture | committed, team-02 | 07e2c20 | tests/perf/recorded_run_z13_z044.txt |
| Z-020 original ladder | BANKED (Wave 3) | c84f021 (seat) | tests/perf/recorded_run_z13_z020.txt |
| Z-021 glibc ladder | BANKED (Wave 3) | dee30d6 (seat) | tests/perf/recorded_run_z13_z021.txt |
| Z-019 lock histograms | BANKED (Wave 3) | 0b6c20f (seat) | tests/perf/recorded_run_z13_z019.txt |

OWED (not landed at build; nothing from these is cited as fact):
| Slice | State at build | What is owed |
|---|---|---|
| Z-038 arena ladder | in flight on team-06 on the fixed Z-029 core; artifacts staged, uncommitted | the three-way ladder table (banked / arena / glibc) - the arena scaling curve |
| Z-039 decision half | condition-pending by its own record | applying Z-039's pre-registered rules R1-R3 to Z-038's table, building the chosen geometry, ledger entry |
| Z-043 free storm | in flight on team-04; probe + runner on disk, core modified in its worktree, NO recorded run | storm verdict + worst-case remote-free latency |

## 2. The scaling curves, 1..32 threads - FINAL for two variants, arena OWED

All ladders share one shape (Z-020/Z-021/Z-039 protocol):
small_churn 50,000 iterations per thread and mixed_batch 200
batches (51,200 ops) per thread, bench_k002.c included
unmodified, worker t pinned to CPU t, headline = median
aggregate ops/s across 5 processes, checksums identical across
legs (small_churn 6371544, mixed_batch 6627200 per thread).
Variants: ORIGINAL = the pre-Wave-4 banked core (no TLS cache,
no LTO; Z-020, contended window, shape of record). CURRENT =
the banked core as it stands after Wave 4 (Z-029 TLS cache +
Z-035 LTO, shipped flags; Z-039 mimir leg, quiet machine).
GLIBC = Z-039 glibc leg, same run as CURRENT (dagger = the
source flags that rung DRIFT-REJECT, INDICATIVE median).
ARENA = OWED to Z-038 - the column is named, never filled.

small_churn, median aggregate ops/s:
| thr | ORIGINAL (Z-020) | CURRENT (Z-039) | glibc (Z-039) | ARENA |
|---|---|---|---|---|
| 1 | 20,398,372 | 43,881,898 (a) | 288,078,173 + | OWED |
| 2 | 6,177,253 | 45,230,326 | 254,365,549 + | OWED |
| 3 | 6,856,680 | 37,253,087 | 128,475,916 + | OWED |
| 4 | 5,474,234 | 35,796,099 | 841,559,578 | OWED |
| 6 | 3,512,483 | 34,377,949 | 1,694,608,884 + | OWED |
| 8 | 3,071,357 | 36,198,076 | 1,270,773,170 + | OWED |
| 12 | 1,087,270 | 18,070,209 | 967,476,660 + | OWED |
| 16 | 1,043,863 | 20,801,756 | 3,005,733,437 + | OWED |
| 20 | 930,308 | 25,856,640 | 3,525,956,328 + | OWED |
| 24 | 937,424 | 29,945,401 | 3,486,304,633 + | OWED |
| 28 | 878,069 | 31,012,748 | 2,849,704,547 + | OWED |
| 32 | 874,055 | 33,279,465 | 3,137,045,766 | OWED |
(a) CURRENT t=1 carries its source's drift note (10.6%, one slow
first process); the median stands, flagged in-table by Z-039.

mixed_batch, median aggregate ops/s:
| thr | ORIGINAL (Z-020) | CURRENT (Z-039) | glibc (Z-039) | ARENA |
|---|---|---|---|---|
| 1 | 4,086,641 | 35,058,840 | 288,579,142 + | OWED |
| 2 | 2,098,738 | 29,772,773 | 574,754,579 + | OWED |
| 3 | 1,506,812 | 36,837,889 | 121,991,415 + | OWED |
| 4 | 1,267,171 | 38,556,634 | 1,066,950,075 + | OWED |
| 6 | 973,316 | 41,842,748 | 962,143,777 + | OWED |
| 8 | 736,353 | 42,523,244 | 1,299,946,047 + | OWED |
| 12 | 375,033 | 25,468,606 | 1,781,980,075 + | OWED |
| 16 | 304,772 | 25,455,453 | 1,979,346,371 | OWED |
| 20 | 207,283 + | 29,889,470 | 2,789,225,527 + | OWED |
| 24 | 183,932 | 31,419,428 | 3,225,772,579 | OWED |
| 28 | 187,217 | 32,042,956 | 2,362,460,347 + | OWED |
| 32 | 136,253 + | 32,391,155 | 4,342,720,827 + | OWED |
ORIGINAL t=20 and t=32 are DRIFT-REJECT in Z-020 after two
re-takes and are INDICATIVE there and here.

Reading the curves (each claim traced to its table):
- ORIGINAL peaks at ONE thread on both workloads and inverts at
  2 (small -69.7%, mixed -48.6%): aggregate falls monotonically
  to scaling 0.043 (small) and 0.033 (mixed) of solo at 32
  threads. Per-thread at 32: 27,314 and 4,258 ops/s.
- CURRENT peaks at 2 threads on small_churn (45,230,326) and at
  8 on mixed_batch (42,523,244, scaling 1.213). Scaling holds
  0.78-0.83 (small) and rises to 1.213 (mixed) through 8
  threads - the Wave 4 TLS cache removed the per-op lock from
  the hit path, and the 2-thread inversion is GONE.
- CURRENT then falls off a CLIFF at 12 threads on both
  workloads (small 18,070,209, scaling 0.412; mixed 25,468,606,
  scaling 0.726) and partially recovers through the SMT range
  to 33,279,465 (0.758) and 32,391,155 (0.924) at 32. The cliff
  sits exactly where threads span both CCDs and the remaining
  central transactions (TLS refill/drain, span supply, registry
  walks) start paying the Z-012 cross-CCD line price.
- glibc is ahead at EVERY rung of both workloads in the same
  run: on small_churn by about 6.6x at 1 thread widening to
  about 94x at 32; on mixed_batch by about 8.2x at 1 thread to
  about 134x at 32 (Z-039's own wording; glibc medians are
  drift-indicative at most rungs, so the ORDER of the gap, not
  its exact size, is the claim). Z-021's independent ladder
  graded the same comparison LOSS at every rung 1..32.
- Cross-ladder CURRENT-vs-ORIGINAL deltas mix TLS + LTO +
  window effects (contended vs quiet) and are cited as shape
  only: the inversion's removal and the cliff's appearance are
  the shape facts; no exact improvement factor is claimed.

## 3. KL-B restated in numbers - FINAL

KL-B entered the wave as: multithread scaling under the one
global lock (L-05). Its numbers, before and after:

BEFORE (the Wave 3 autopsy form, banked Z-019/Z-020):
- Uncontended lock WAIT sits at the measurement floor on every
  entry (mean about 40 cycles, p50 = 30 cycles = one TSC tick).
  The lock was never slow; the work under it was the cost.
- Contended WAIT: mean 1,597/1,688 cycles at 4 threads; 4,190 /
  4,292 cycles at 8 threads (about 1.4 us, about 26x the hold,
  p99 about 10 us). Z-020's specimen extends it: WAIT 51 ->
  57,208 cycles (about 19.1 us) at 32 threads, about 90x hold.
- Aggregate consequence: inversion at 2 threads, scaling 0.043 /
  0.033 of solo at 32 threads (section 2, ORIGINAL).

AFTER Wave 4 + the landed Wave 5 measurement (Z-039, quiet):
- The per-op serialisation is off the hit path: small_churn no
  longer inverts at 2 threads; it peaks at 2 and holds scaling
  0.78-0.83 to 8 threads; mixed_batch scales to 1.213 at 8.
- Per-thread throughput at 32 threads is 1,039,983 (small) and
  1,012,224 (mixed) ops/s on CURRENT, against ORIGINAL's
  27,314 and 4,258 - quoted as shape across two windows, not
  as an exact factor (section 2 honesty note).
- The wound MOVED, it did not close. What remains of KL-B sits
  in the central transactions every cache miss still pays -
  refill/drain batches, span supply, the Z-024 registry walks -
  under the same one global lock, and it prices itself in the
  curves as (a) the 12-thread cross-CCD cliff (scaling 0.412 /
  0.726 at 12 threads) and (b) Z-041's proven dip: 16 pinned
  threads across both CCDs is the WORST pinned posture, 0.69-
  0.72x of the 8-thread one-CCD posture, both workloads, both
  invocations.
- Against glibc the standing is unchanged in kind: LOSS at
  every rung 1..32 on both workloads (Z-021 of record; Z-039
  same-run order-of-gap about 94x / 134x at 32 threads).

RESTATED: KL-B is no longer "every op queues on one lock." It
is now "the cache-miss and span-supply path queues on one lock,
and the queue is priced cross-CCD from 12 threads up." The
wave's chosen remedy class - partition the serving state
(arena) or shard the lock - is UNMEASURED: its ladder is Z-038,
OWED, and its decision is Z-039's R1-R3, pending that ladder.
No sentence in this verdict may be quoted as an arena result.

## 4. Wave 5 change log, Z-037 to Z-044

### Z-037 - Arena prototype behind a flag - FINAL (banked 25322cd)
Per-CCD arena variant (2 arenas, Z-012 constants) behind
MIMIR_ARENA_CCD, sharing the Wave-4 TLS machinery by
construction: span ownership fixed at creation in a 65,536-slot
side table (fail-clean ceiling), serving arena-restricted on
both central roads, frees routed by the block's span, retention
bound SPLIT across arenas, span header unchanged at 96 B.
Gate MET: prototype builds; flag-on full suite 4988/0/77
identical to baseline; flag-off objects BYTE-IDENTICAL to the
pristine seat tree (zero-cost proof, closed again at the linked
level under -O2 -flto). Placement proven exact on both CCDs
including SMT siblings; cross-CCD frees return live counts to
exactly 0; 16-thread storm clean. NO performance claim is made
by the slice: the one global lock is unchanged, migration rate
and remote-free ratio are explicitly deferred to Z-038, and
fork posture to Z-044. One accounting note carried, not hidden:
the Z-029 pending-tally residual (6 blocks after an un-quiesced
cross-free sequence) is identical on the banked core and the
arena build - banked semantics, neither fixed nor worsened.

### Z-038 - Arena ladder - OWED
Not landed at build time (in flight on team-06, on the fixed
Z-029 core; artifacts staged, uncommitted - not evidence).
Owed: the three-way ladder table (banked / arena / glibc) that
fills section 2's ARENA column, prices the prototype's
partition, and triggers Z-039's decision rule. Until it lands,
whether the arena pays is an OPEN question of this campaign,
and this verdict records it as one.

### Z-039 - Second arena geometry - FINAL as committed (e5bf321); decision OWED
STATUS in its own record: CONDITION-PENDING - no geometry
chosen, no production code changed, and the roadmap gate's
"chosen geometry" half expressly NOT MET. What landed is
everything that does not depend on Z-038: (1) the two
condition-independent ladder legs reproduced in section 2
(banked-current and glibc, quiet machine, checksums identical
to Z-020/Z-021); (2) the pre-registered decision rule - R1:
arena beats banked by >5% on BOTH workloads in the 8..16 band
-> build per-thread arenas (geometry A); R2: arena within the
5% TIE band at 8..16 on either workload, or wins only at 32
-> build lock-sharding (geometry B); R3: split result -> the
workload Z-036 named as priority decides, else R2; R4: the
chosen geometry's own ladder is its gate evidence and the road
not taken is recorded with the R-rule that closed it; (3)
paper designs for both geometries at buildable depth. Standing
gate GREEN 4988/0/77 in its worktree. The same-run Law 2 grade
on the synthetic churn shapes: LOSS (glibc ahead at every
rung; section 2). Dependency recorded for Z-040: while the
decision is pending, only structures both geometries keep are
condition-independent purge targets.

### Z-040 - False-sharing purge - FINAL (committed 8925723)
Z-016's padding rule applied to every shared structure the
profiles name, in production code (src/core/mimir_core.c,
src/api/mimir_alloc.c; layout only, types and semantics
unchanged; standing gate GREEN 4988/0/77; .so surface still
exactly 4 symbols). Purged to their own 64 B lines: the four L1
audit call counters, the two lock-free core audit counters
(small_alloc / small_free on the TLS hit paths), the J-002
pre-lock stats pair, and the TLS owner-table entry (24 B ->
64 B per entry). Structure-level verdicts under replicated
access patterns (within-run ratios, replicated across three
runs): L1-mix pattern WIN 0.26-0.35x per-op at 2-32 threads;
pre-lock pair WIN 0.36-0.50x; owner-table record op WIN
0.10-0.22x - the largest single-structure false-sharing cost
in the tree, on the cache's hot path. The central NEGATIVE
result: scattering the lock-held groups is a proven LOSS
(+11..+36% at 8-32 threads, locked7 pattern) - under one mutex
there is one writer at a time, so those groups stay packed and
their purge is isolation from lock-free fields. True-sharing
control unaffected (0.98-1.03), so no claim rests on beating
true sharing. Bill stated: core .bss +2,621,640 B, almost all
of it the owner table's virtual zero-page BSS. The assembled
allocator's end-to-end effect is recorded INDICATIVE ONLY
(unequal load windows; one -6.7% cell is the proof the windows
are not comparable); no assembled-gap claim is made.

### Z-041 - SMT and pinning policy - FINAL (committed 48f602b)
Policy slice, no code change: docs/Z13_SMT_PINNING_POLICY.md +
this evidence. Matrix leg x workload x threads (8/16/32) x
mode (pinned/floating), two full invocations, near-quiet
window. Every mimir PINNED cell agrees across invocations
within 2.41% (spreads at most 6.6%): the pinned table is
PROVEN. Findings of record: (1) 16 pinned threads across both
CCDs is the worst pinned posture for the banked core,
0.69-0.72x of 8 pinned on one CCD, identically in both
workloads and invocations - the remaining shared structures
pay the Z-012 cross-CCD price (about 103 ns vs about 32 ns)
as soon as the second CCD joins. (2) SMT is a real gain for
allocator-heavy loads on this machine: mimir pinned 32/16 =
1.46-1.88 PROVEN; glibc 32/16 direction 4/4 in both modes.
This does not contradict Z-011/Z-012 - bandwidth streaming
gains nothing from siblings; the allocator path is latency-
and serialisation-bound, and siblings hide exactly that
latency. (3) Pinning never loses beyond a tie band: at 8
threads it is worth 1.19-1.97x for mimir (floating placement
of few threads is bimodal; the slow cluster never appears
pinned); at 16-32 it ties floating on medians (0.95-1.22) but
removes the variance (floating spreads to 93.7%; a median
26-30 of 32 floating workers end on a different CPU than they
started). (4) Allocator class decides the thread-count answer:
glibc scales monotonically 8 -> 16 -> 32 in every pair while
mimir dips at 16 and recovers at 32 - no single machine-wide
cap serves both. Best aggregate mimir posture measured: 32
pinned (33.2-33.8M small_churn, 39.7-39.9M mixed_batch ops/s);
best per-thread efficiency: 8 pinned on one CCD (about 3.9M
ops/s per thread vs about 1.0M at 32 pinned). glibc magnitudes
are INDICATIVE (short passes, downward outliers); only its
directions are claimed. The policy is scoped PROVISIONAL for
the arena variant: an arena posture re-run of this matrix is
OWED (recorded in the policy doc) once an arena banks.

### Z-042 - Thread-churn storm - FINAL (committed 4490903)
10,000 short-lived allocating threads in waves of 32, no
production code changed. Gate MET: storm GREEN - 162 CHECK
lines, 0 FAIL across the recorded reps; drain exactly-once
proven (calibration thread: 46 cached blocks returned exactly;
256 pre-drained threads return 0 more at exit; 10,000 null
threads move no counter); storm exit drains exactly 10,000
and exit-drained blocks exactly 460,000 in every rep; the
20,000 blocks held live at exit are freed by main and every
counter settles. RETAINED-BYTES-AFTER-STORM figure of record:
20,480 B (5 empty spans of 4096 B) in every rep - legitimate
G-007 retention (bound 2 per class), not a leak, and the proof
is the same-process TRIM close: trim releases exactly the 9
retained spans / 36,864 B then present, and bytes_mapped
returns to the pristine 0 TO THE BYTE. Cost honesty: mimir
storm wall 1.163 s vs null-thread wall 0.118 s in-process
(about 104,574 ns per thread of alloc+free+teardown over
null; drain-inclusive upper bound 2,273 ns per exit-drained
block); glibc's same-shape wall is 0.115 s, a whole-shape
ratio of about 10.1x recorded here for this verdict - Z-042's
own gate claims no glibc comparison. Named follow-up, carried
to section 7: the storm ran on the TLS teardown surface; an
arena variant's teardown under churn is unmeasured.

### Z-043 - Producer/consumer free storm - OWED
Not landed at build time (in flight on team-04; probe,
runner and summariser on disk, production core modified in
its worktree, NO recorded run exists). Owed: the storm-green
verdict and the worst-case remote-free latency at 32-thread
cross-free rates the banked core never faced. Until it lands
this verdict says nothing about remote-free flood behaviour;
the only landed remote-free evidence remains Z-030 (banked:
freer-side enqueue WIN, pair total LOSS, swap TIE, queue not
wired into mimir_free) and Z-037's exact cross-CCD free
correctness. Any Wave 5 claim about cross-free storms is OWED
with this slice.

### Z-044 - Fork posture proof on this host - FINAL (committed 07e2c20)
No production code changed; standing gate GREEN 4988/0/77.
Posture RE-CONFIRMED AND EXTENDED on x86-64 for the current
Z-029/Z-030 core, runs A and B identical in every count:
fork-quiescent PROVEN (Q1 single-thread, Q2 multithread);
fork+exec PROVEN (E1 quiescent 16/16; E2 under churn 32/32,
churn progress proven by counter delta); fork-while-busy
MEASURED UNSUPPORTED - the staged mechanism (B0a: parent
holds the core's own global lock across fork) hangs the
child's first locked-path call 3/3 in both runs while its
TLS-bin small alloc completes lock-free, and under saturated
churn (B1) 24/24 busy children in both runs complete the
small alloc from the inherited bin and do not complete the
large alloc within 500 ms (counts, never rates). The control
B0b (lock released before fork) passes 3/3, and B2 proves fork
itself is never the problem (8/8). L-08 stands unchanged in
core and is EXTENDED: Clause 2 (INC-Z044-01, NEW) - in a
multithread-quiescent fork, spans whose binned tally belongs
to threads that do not exist in the child cannot be released
in the child even after every inherited block is freed and
the child's bins drained (Q2: 2 spans stranded,
drain_released=0, both runs); the child's memory floor is
bounded below by those spans for its whole life, with no
corruption observed. An accounting note rides with it: the
child's stats counters inherit only the flushed view (Q2:
16 read where 19 were live; a -1 residual after freeing all),
an accounting-visibility property, not corruption; the
parent settles exactly 0/0. glibc's 24/24 BOTH_OK on the B1
shape is context only (a different allocator, no single
global lock on that path) - not parity, not part of the
verdict. Practical rule, sharpened: a forked child may only
exec or _exit without allocator calls unless the fork was
quiescent - and "the small one worked" is not evidence of
safety, because the TLS hit path never takes the lock.

## 5. Remaining gap, itemised - FINAL as far as landed

- vs glibc: LOSS at every rung of both ladder workloads
  (section 2). The gap at 32 threads is the widest the
  campaign has recorded in relative terms (order of 94x /
  134x, indicative medians) because glibc keeps scaling into
  SMT while CURRENT is still recovering from its cliff.
- The 12-thread cliff and the 16-pinned dip are one wound in
  two instruments: central transactions priced cross-CCD.
  Z-040 removed the false-sharing tax on the lock-free fields
  around that path; it did not and could not remove the lock.
- The remedy question is OPEN: arena partition vs lock-
  sharding awaits Z-038's ladder and Z-039's R1-R3 decision.
  Wave 5 therefore ends, on landed evidence, with a diagnosis
  and a pre-registered decision procedure - not a fix.
- Cross-free flood behaviour at 32 threads: UNKNOWN (Z-043
  OWED). Churn teardown: PROVEN for the TLS surface (Z-042),
  OWED for any arena surface. Fork: posture proven and
  bounded (Z-044), with INC-Z044-01 span stranding entered as
  named future design work in team-02's CAMPAIGN_STATE.md.
- An end-to-end ladder of the ASSEMBLED final Wave 5 core
  (current core + Z-040 layout) has never been run as a
  verdict: Z-040's e2e is indicative only. That re-run is
  owed rigour, named here so it is not silently assumed.

## 6. Traceability index

Every FINAL number above traces to exactly one landed file:
- Curves ORIGINAL: tests/perf/recorded_run_z13_z020.txt
  (banked seat c84f021).
- Curves CURRENT + glibc, Z-039 rule and designs:
  tests/perf/recorded_run_z13_z039.txt (team-08 e5bf321).
- KL-B before-form: tests/perf/recorded_run_z13_z019.txt
  (banked seat 0b6c20f) and Z-020 as above; glibc LOSS of
  record: tests/perf/recorded_run_z13_z021.txt (banked seat
  dee30d6).
- Z-037: tests/perf/recorded_run_z13_z037.txt (banked seat
  25322cd; merge d0185ab at this build's base).
- Z-040: tests/perf/recorded_run_z13_z040.txt (team-09
  8925723), raw hidden_files/z040_raw1..3.txt in team-09.
- Z-041: tests/perf/recorded_run_z13_z041.txt + docs/
  Z13_SMT_PINNING_POLICY.md (team-01 48f602b).
- Z-042: tests/perf/recorded_run_z13_z042.txt (team-03
  4490903).
- Z-044: tests/perf/recorded_run_z13_z044.txt (team-02
  07e2c20).
- OWED rows trace to absence, verified first-hand at build:
  no Z-038 commit on any branch (team-06 HEAD is its Z-029
  fix; Z-038 artifacts staged only) and no Z-043 recorded
  run or commit (team-04 worktree mid-build).

## 7. Verdict and the fold-in owed - PARTIAL by construction

Wave 5's landed verdict: the Wave 4 TLS cache changed KL-B's
shape, not its standing. The allocator no longer collapses at
the second thread; it scales honestly to one CCD, falls off a
cross-CCD cliff at 12 threads, and recovers only partially
through SMT - and it still loses to glibc at every thread
count on both workloads, by the widest relative margin at 32
threads. The wave also banked real, bounded wins around that
core fact: an exact, zero-cost arena prototype (Z-037);
false-sharing removed from every lock-free hot field with the
scattering of locked groups proven a loss and refused
(Z-040); a proven affinity posture (pin; 32 pinned for
aggregate, 8 pinned one-CCD for efficiency; never 16 pinned
across CCDs) (Z-041); byte-exact churn teardown (Z-042); and
a fork posture re-proven on this host with its boundary
sharpened by a new stranding clause (Z-044).

OWED before this slice can bank (the fold-in list):
1. Z-038's three-way ladder: fill section 2's ARENA column,
   complete the three-variant curve set the roadmap names,
   and restate section 3's KL-B close with the arena priced.
2. Z-039's decision: apply R1-R3 to that ladder; record the
   chosen geometry, the road not taken with its R-rule, and
   the docs/DECISION_LEDGER.md entry.
3. Z-043's free storm: storm verdict + worst-case remote-
   free latency into sections 4 and 5.
4. Dependent re-runs the landed slices themselves name:
   the Z-041 affinity matrix on the arena variant, and
   Z-042's churn storm on the arena teardown surface - both
   become possible only when an arena banks.
5. An assembled-final-core ladder re-run (post-Z-040 layout)
   if the keeper wants the indicative e2e note replaced by a
   verdict; until then it stands as named owed rigour.

Team note: this lead ran the six Mythic Engineering roles
sequentially (traceability audit; baseline fidelity; Wave 5
completeness; honesty and anti-fabrication; structure and
gate; KL-B synthesis) rather than as parallel sub-agents -
the platform depth available to this lead did not permit six
parallel device-working sub-agents. Every number in this
document was read first-hand from the recorded runs on disk
by the lead; sub-agent-style claims were never used as
evidence (the Z-027 lesson). Bugs discovered: none. No
allocator code was touched by this slice.
