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
Fold-in base: merge 4f018df of seat main 8aec32a (banked
Z-001..Z-039 part 1, including Z-038 at seat 7bc5473) into
yrsa/mimiralloc-team-05, on top of build commit b55b782.
Fold-in 2026-11-12 by a fresh team-05 lead (the six Mythic
Engineering roles run sequentially by the lead - see the
fold-in note in section 7).

STATUS: FOLD-IN - the two slices OWED at build have LANDED
and are folded in below, amended not rewritten (the Z-036
pattern). Z-038 (arena ladder) is BANKED at the seat as
7bc5473; Z-043 (producer/consumer free storm) is committed
on team-04 as a72e2db, its recorded run read first-hand from
team-04's worktree (it banks in roadmap order behind
Z-040..Z-042). Every number folded in is traced to those
recorded runs, verbatim at source precision. What remains
OWED after this fold-in is named in section 7: the Z-039
geometry-decision follow-up (being built by team-08 in
parallel, not landed at fold-in time), the Z-041-on-arena
and Z-042-on-arena re-runs, and the assembled-final-core
ladder re-run. The staged, uncommitted Z-038 draft noted at
build is superseded by the banked completion run of record;
it was never used as evidence.

## Scope and honesty - read first

- Wave 5 (Z-037..Z-045) is the thread-scaling hunt: KL-B (multi-
  thread scaling under the one global lock, L-05) attacked at
  16 cores / 32 threads on this machine.
- The roadmap's Z-045 text asks for: scaling curves 1..32 threads,
  three allocator variants, per workload; KL-B's standing restated
  in numbers. The three variants of record (per Z-038's gate) are
  the banked core, the arena variant, and glibc. The arena curve
  is FINAL from the banked Z-038 completion run (fold-in); the
  banked-core and glibc curves are FINAL from landed runs, and
  the pre-Wave-4 original core's curve (Z-020) is carried as the
  wave's before-picture, FINAL. Session honesty: the ARENA
  column is Z-038's session, not the Z-039 session of the
  CURRENT/glibc columns beside it - see section 2's session
  note; arena verdicts rest on Z-038's own same-session legs.
- Load regimes differ per source and are stated per section:
  Z-020 ran on a CONTENDED shared host (its internal shape is the
  claim); Z-039's legs ran on a QUIET machine (its numbers may be
  cited as this-machine numbers); Z-040's primary run started
  quiet and was contended by a sibling suite mid-session (its
  verdicts rest on within-run ratios); Z-041 ran in a near-quiet
  window; Z-042 in a light shared window; Z-044 under recorded
  shared load (a sibling team's Z-043 storm was the top CPU
  consumer at its start). Fold-in sources: Z-038 ran in a quiet
  shared-host window (runner start loadavg 0.27/0.72/1.09,
  per-point 0.3..2.4 - its record calls it a quiet window on a
  still-shared machine, so its numbers are this-session leg
  comparisons, not quiet-machine baselines); Z-043's canonical
  attempts required 1-min loadavg < 1.0 at launch (attempt 1
  launched at 0.62, sampler peak 0.99; attempt 2 launched at
  0.20, sampler peak 0.94). Cross-session comparisons between
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
This document is that verdict for Wave 5.
Gate status after fold-in: MET for every Wave 5 slice's
evidence (sections 2-5 FINAL, including the Z-038 arena curve
and the Z-043 free storm folded in from their landed recorded
runs). The Z-039 geometry decision that Z-038 triggers
remains OWED to its follow-up (section 7) - it is a decision
and build slice, not verdict evidence, and this doc does not
pre-empt it.
Standing correctness gate at this build: scripts/z13/run_gate.sh
GREEN 4988 checks / 0 failed / 77 by-design skips on the merged
branch (log hidden_files/z045_gate.log) - identical to the
Z-001 baseline; this slice touches no code.
Standing correctness gate at fold-in: scripts/z13/run_gate.sh
GREEN 4988 checks / 0 failed / 77 by-design skips on the
fold-in merged branch (log hidden_files/z045_foldin_gate.log)
- identical to the Z-001 baseline; this fold-in touches no
code.

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

LANDED (committed; read first-hand from disk by this lead;
fold-in rows marked):
| Slice | State at build | Commit | Recorded run |
|---|---|---|---|
| Z-037 arena prototype | BANKED at seat | 25322cd (seat) | tests/perf/recorded_run_z13_z037.txt |
| Z-038 arena ladder | BANKED at seat (fold-in) | 7bc5473 (seat) | tests/perf/recorded_run_z13_z038.txt |
| Z-039 second geometry | part 1 BANKED at seat (fold-in) | 8aec32a (seat; team-08 e5bf321) | tests/perf/recorded_run_z13_z039.txt |
| Z-043 free storm | committed, team-04 (fold-in) | a72e2db | tests/perf/recorded_run_z13_z043.txt (team-04 worktree) |
| Z-040 false-sharing purge | committed, team-09 | 8925723 | tests/perf/recorded_run_z13_z040.txt |
| Z-041 SMT + pinning | committed, team-01 | 48f602b | tests/perf/recorded_run_z13_z041.txt + docs/Z13_SMT_PINNING_POLICY.md |
| Z-042 thread-churn storm | committed, team-03 | 4490903 | tests/perf/recorded_run_z13_z042.txt |
| Z-044 fork posture | committed, team-02 | 07e2c20 | tests/perf/recorded_run_z13_z044.txt |
| Z-020 original ladder | BANKED (Wave 3) | c84f021 (seat) | tests/perf/recorded_run_z13_z020.txt |
| Z-021 glibc ladder | BANKED (Wave 3) | dee30d6 (seat) | tests/perf/recorded_run_z13_z021.txt |
| Z-019 lock histograms | BANKED (Wave 3) | 0b6c20f (seat) | tests/perf/recorded_run_z13_z019.txt |

OWED (not landed at fold-in; nothing from these is cited as fact):
| Slice | State at fold-in | What is owed |
|---|---|---|
| Z-039 decision follow-up | being built by team-08 in parallel; NOT landed (no decision commit; team-08 HEAD at finish is merge 9e86b51 of seat 8aec32a, verified first-hand) | applying Z-039's pre-registered rules R1-R3 to Z-038's banked table, building the chosen geometry, DECISION_LEDGER entry |
| Z-041 on arena | not run; policy scoped PROVISIONAL for the arena variant | the Z-041 affinity matrix re-run on the arena variant |
| Z-042 on arena | not run | the Z-042 churn storm on the arena teardown surface |
| Assembled-final-core ladder | never run as a verdict (Z-040 end-to-end indicative only) | a ladder re-run of the assembled post-Z-040 core |

## 2. The scaling curves, 1..32 threads - FINAL (arena folded in)

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
ARENA = the Z-037 per-CCD variant as measured by the banked
Z-038 completion run (fold-in; seat 7bc5473). SESSION NOTE:
the ARENA column is Z-038's session (quiet shared-host
window, runner start loadavg 0.27/0.72/1.09); the CURRENT
and glibc columns beside it are Z-039's session. The source
reports arena medians in Mops/s to three decimals; the
column below is that value x 1,000,000, verbatim at source
precision. Arena small_churn t=1 is DRIFT-REJECT in its
source (12.82% > 8.0% bound; median indicative only) and is
flagged (b). Arena-vs-banked grades in this verdict rest
ONLY on Z-038's own same-session three-way table (quoted in
the reading below), never on arithmetic between the ARENA
and CURRENT columns.

small_churn, median aggregate ops/s:
| thr | ORIGINAL (Z-020) | CURRENT (Z-039) | glibc (Z-039) | ARENA |
|---|---|---|---|---|
| 1 | 20,398,372 | 43,881,898 (a) | 288,078,173 + | 35,494,000 (b) |
| 2 | 6,177,253 | 45,230,326 | 254,365,549 + | 24,634,000 |
| 3 | 6,856,680 | 37,253,087 | 128,475,916 + | 28,504,000 |
| 4 | 5,474,234 | 35,796,099 | 841,559,578 | 29,135,000 |
| 6 | 3,512,483 | 34,377,949 | 1,694,608,884 + | 29,637,000 |
| 8 | 3,071,357 | 36,198,076 | 1,270,773,170 + | 30,529,000 |
| 12 | 1,087,270 | 18,070,209 | 967,476,660 + | 17,940,000 |
| 16 | 1,043,863 | 20,801,756 | 3,005,733,437 + | 19,752,000 |
| 20 | 930,308 | 25,856,640 | 3,525,956,328 + | 23,681,000 |
| 24 | 937,424 | 29,945,401 | 3,486,304,633 + | 28,554,000 |
| 28 | 878,069 | 31,012,748 | 2,849,704,547 + | 32,901,000 |
| 32 | 874,055 | 33,279,465 | 3,137,045,766 | 36,699,000 |
(a) CURRENT t=1 carries its source's drift note (10.6%, one slow
first process); the median stands, flagged in-table by Z-039.
(b) ARENA t=1 small_churn carries its source's DRIFT-REJECT
flag (drift 12.82%, bound 8.0%): the median stands as
INDICATIVE only, per Z-038. Every other arena rung is
drift-clean in its source (banked leg 0 of 24 rungs rejected;
arena leg 1 of 24 - this one).

mixed_batch, median aggregate ops/s:
| thr | ORIGINAL (Z-020) | CURRENT (Z-039) | glibc (Z-039) | ARENA |
|---|---|---|---|---|
| 1 | 4,086,641 | 35,058,840 | 288,579,142 + | 37,879,000 |
| 2 | 2,098,738 | 29,772,773 | 574,754,579 + | 23,088,000 |
| 3 | 1,506,812 | 36,837,889 | 121,991,415 + | 28,203,000 |
| 4 | 1,267,171 | 38,556,634 | 1,066,950,075 + | 28,756,000 |
| 6 | 973,316 | 41,842,748 | 962,143,777 + | 28,700,000 |
| 8 | 736,353 | 42,523,244 | 1,299,946,047 + | 28,917,000 |
| 12 | 375,033 | 25,468,606 | 1,781,980,075 + | 17,657,000 |
| 16 | 304,772 | 25,455,453 | 1,979,346,371 | 20,208,000 |
| 20 | 207,283 + | 29,889,470 | 2,789,225,527 + | 23,374,000 |
| 24 | 183,932 | 31,419,428 | 3,225,772,579 | 30,323,000 |
| 28 | 187,217 | 32,042,956 | 2,362,460,347 + | 34,978,000 |
| 32 | 136,253 + | 32,391,155 | 4,342,720,827 + | 35,617,000 |
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
- ARENA (fold-in; Z-038's own same-session three-way table):
  arena vs banked = TIE at 21 of 24 rungs - small_churn median
  ratio 0.976..1.009 across all 12 rungs (every rung within
  +/-2.4%); mixed_batch 0.969..1.016 at 1..20 threads - and
  arena WIN at mixed_batch 24 / 28 / 32 threads only (ratios
  1.131 / 1.161 / 1.081, +13.1% / +16.1% / +8.1% on medians,
  drift-clean on both legs at those rungs). The per-CCD split
  does NOT relieve the central contention shape: the 12-thread
  dip (arena 17,940,000 small / 17,657,000 mixed in the column
  above) and the ~3% per-thread efficiency at 32 threads are
  present in both variants almost identically (Z-038).
- ARENA vs glibc (fold-in; Z-038 session): LOSS at every rung
  1..32 on both workloads, direction PROVEN at rep level -
  full separation at all 24 rungs against both mimir legs
  (the slowest glibc rep beats the fastest mimir rep at every
  rung; worst separation 4.54x, mixed_batch 1 thread vs
  arena; up to 126.27x at the top rungs). glibc medians are
  DRIFT-REJECT at 18 of 24 rungs in Z-038, so median factors
  there are INDICATIVE ONLY and no exact factor is claimed.
- Cross-ladder CURRENT-vs-ORIGINAL deltas mix TLS + LTO +
  window effects (contended vs quiet) and are cited as shape
  only: the inversion's removal and the cliff's appearance are
  the shape facts; no exact improvement factor is claimed.
  The same discipline applies to the ARENA column (Z-038
  session) read beside CURRENT (Z-039 session).

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
- The lock convoy's tail is now priced directly (fold-in,
  Z-043): a single cross-thread mimir_free in the 32-thread
  ring measured a worst case of 1,074,081,376 ns (~1.07 s,
  attempt 2 per-process max median; per-process maxima across
  attempts 0.86-1.25 s) against a median of 8.1-9.2 us and
  p99 of 35-72 us - the one-global-lock convoy at storm scale
  (section 4, Z-043).
- Against glibc the standing is unchanged in kind: LOSS at
  every rung 1..32 on both workloads (Z-021 of record; Z-039
  same-run order-of-gap about 94x / 134x at 32 threads).

RESTATED: KL-B is no longer "every op queues on one lock." It
is now "the cache-miss and span-supply path queues on one lock,
and the queue is priced cross-CCD from 12 threads up." The
wave's chosen remedy class is now MEASURED on its partition
arm (fold-in, Z-038): per-CCD arena partition is TIE vs the
banked core at 21 of 24 rungs, with a top-end mixed_batch WIN
at 24-32 threads only - partition as prototyped does NOT move
KL-B's shape (the 12-thread dip survives in both variants),
because the surviving contention cost is NOT primarily
cross-CCD span ownership (Z-038's input to Z-039, folded into
section 4). The lock-sharding arm remains UNMEASURED, and the
choice between the arms is Z-039's R1-R3 decision, still OWED
to the team-08 follow-up (section 7). Arena results may now
be quoted - from Z-038's recorded run only, at its session's
precision.

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

### Z-038 - Arena ladder - FINAL (banked 7bc5473; fold-in)
Completion run of record on the fixed Z-029 core (base
d0185ab): all three legs measured fresh in ONE session
(2026-11-12T06:00:21..06:02:54-05:00) under the frozen
Z-020/Z-021 protocol, checksums identical on all legs
(small_churn 6371544, mixed_batch 6627200). The first Z-038
session's numbers are SUPERSEDED by its own record - the
banked leg could not be certified unaffected by the Z-029
defects (on the fixed tree, banked mixed_batch moved ~4.4x
at 1 thread) - and none are carried here. Load: quiet
shared-host window (runner start 0.27/0.72/1.09, per-point
0.3..2.4); its numbers are this-session leg comparisons, not
quiet-machine baselines. Standing gate GREEN 4988/0/77 on
the merged tree (33fad05).
Roadmap gate MET IN FULL, in its own words: the three-way
ladder table (banked / arena / glibc) is recorded, and
section 2's ARENA column is filled from it. Grades, in its
own words: banked core LOSS at every tested rung
(1,2,3,4,6,8,12,16,20,24,28,32) on both workloads vs glibc,
and arena variant LOSS at every tested rung on both
workloads - direction PROVEN at rep level (full separation
at all 24 rungs against both mimir legs; worst separation
4.54x). Arena vs banked: TIE at 21 of 24 rungs; arena WIN at
mixed_batch 24, 28 and 32 threads (top-end only, +8..+16%).
Drift discipline in its record: banked 0 of 24 rungs
rejected; arena 1 of 24 (small_churn 1 thread, 12.82%);
glibc 18 of 24 - every median factor on a rejected glibc
rung is INDICATIVE ONLY.
Input to Z-039 (recorded by Z-038, decided neither there nor
here): (a) the arena geometry as prototyped in Z-037 does
not move the ladder shape - the surviving contention cost is
NOT primarily cross-CCD span ownership, since splitting
spans per CCD changed almost nothing at <=20 threads; (b)
the one place arena helps is mixed_batch at SMT scale
(+8..+16%); (c) the broken-core mixed_batch collapse
recorded in the first session is RETRACTED - on the fixed
core mixed_batch dips and recovers like small_churn.
Applying R1-R3 to this table is the Z-039 follow-up's work
(section 7, still OWED). Whether the arena pays is therefore
ANSWERED - as a partition it does not, beyond the SMT
top-end - and the geometry decision it triggers is what
remains open.

### Z-039 - Second arena geometry - part 1 FINAL (banked 8aec32a); decision follow-up OWED
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
gate GREEN 4988/0/77 in its worktree. Fold-in state: part 1
is BANKED at the seat as 8aec32a; the re-open trigger in its
own record (recorded_run_z13_z038.txt banks at the seat) is
now SATISFIED, and the decision follow-up - apply R1-R3 to
the banked Z-038 table, build the chosen geometry as the
follow-up slice, record the road not taken, append the
decision to docs/DECISION_LEDGER.md - is being built by
team-08 in parallel and had NOT landed at fold-in time
(no decision commit on team-08; its HEAD at finish is
merge 9e86b51 of seat 8aec32a, verified first-hand). It
remains OWED (section 7); this verdict records Z-038's input
to it (above) and decides nothing. The same-run Law 2 grade
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

### Z-043 - Producer/consumer free storm - FINAL (committed a72e2db, team-04; fold-in)
Built on seat main 35f7f55 merged as 5c18608 in team-04;
runs 2026-11-12 10:54-11:03Z under canonical load discipline
(attempts require 1-min loadavg < 1.0 at launch; attempt 1
launched at 0.62, sampler peak 0.99; attempt 2 launched at
0.20, sampler peak 0.94). Correctness: 5 phases, 42 checks
(S1 ring32: 32 threads each cross-free the NEXT thread's
2048 x 64B batch x 3 rounds, 196,608 remote frees; S2
split16; S3 Z-030 MPSC queue flood; S4 32 queues
cross-flooded; S5 ring32 mixed classes). Roadmap gate MET:
storm GREEN (42/42; both red paths fail as designed -
DMRQ_MUTANT_LOSSY FAIL, 8 checks; DZ043_MUTANT_DROP_FREE
FAIL, 6 checks, freed short by exactly 32) and worst-case
remote-free latency RECORDED.
Worst case, in its own words: 1,074,081,376 ns (~1.07 s,
attempt 2 per-process max median; per-process maxima across
attempts 0.86-1.25 s) for a single cross-thread mimir_free
in the 32-thread ring, against ring32_lat med 8136 ns /
9248 ns and p99 34.8/72.2 us across the two attempts
(max_med 0.99 s / 1.07 s). The tail is the one-global-lock
convoy of the banked core at storm scale, amplified by
descheduling inside the timed call; it is reported as
measured, not explained away.
Verdicts, in its own words: ring32 wall 9691.6 / 9874.7
ns/op is 2.9x the local32 control (3361.6 / 3354.4 ns/op).
Against glibc the direct remote free LOSES at every
percentile (median ~300x glibc's 30 ns tcache free; worst
case ~4 orders; glibc ring32_lat med 30 ns, p99 50 ns,
max_med 38-64 us; glibc pc16x16_lat med 40 ns, max_med
41-844 us). pc16x16_lat med 2034 / 2304 ns, p99 734/484 us,
max_med 5.8 ms / 3.1 ms. Queue under flood: bypassed 95.1% /
95.4% confirms the saturation regime (pre-registered >90%);
queue31_enq push med 72 / 59 ns, p99 3.7/4.2 us, max_med
19.7 / 22.2 us - freer-side tail WIN, by ~45,000x (queue
enqueue per-process max ~20 us vs direct free max ~1 s);
queue e2e max_med 1.79 / 1.34 ms also beats the direct max.
queue31_total wall 77.6 / 62.0 ns/op is labelled UNSTABLE by
its source (cross-attempt wall delta 22% > 10% band) and is
NOT a like-for-like allocator win. The Z-030 pair-geometry
total-cost LOSS (+35.9%/+60.9%) stands uncontradicted: the
queue remains test-only harness (NOT wired into mimir_free);
production remote frees still take the locked central path
ring32 measures. Z-030's landed grades therefore stand as
amended, not replaced: freer-side enqueue WIN, pair total
LOSS, swap TIE - now joined by a measured production-path
tail the queue was never wired to fix.
DEFECTS FOUND BY THE STORM, both fixed in this slice in
src/core/mimir_core.c (the storm failed until they were):
D1 - span released from under live bin-published blocks.
small_free_locked's retention-release trigger required only
live_count == 0 plus other empty spans over the bound; it
did not require THIS span's binned tally to be 0, unlike the
retention sweep and the trim path, which both carry that
guard. Fix, in its own words: the trigger now also requires
tls_binned_count_locked(span) == 0.
D2 - pending-accounting wrap refuses live spans on the free
path. A bin-hit publication's +1 is pending in the
allocating thread until its next flush; a cross-thread free
applies its -1 immediately, so live_count transiently wraps
(measured live_count = 2^64-1, carve_count = 62, class 64B)
and the J-006 relation live_count <= carve_count in
span_header_is_live() refused the span: instrumented runs
showed 41-92 of ~391k cross frees taking the owner-remote
route, failing only that one relation, and silently leaking
the block. Fix, in its own words: the free path
(small_free_locked and the two mimir_core_free pre-checks)
now uses find_span_for_free_locked /
span_header_is_live_for_free - every J-006 relation except
the live_count bound; allocation-side callers keep the
strict form.
Regression evidence post-fix, in its own record: storm
suite 42/42 on three consecutive runs and inside both
recorded attempts; Z-028 TLS contract 119/119; Z-030 queue
suite 86/86; standing gate GREEN 4988/0/77 with both defect
fixes in the tree. Named residual (not exercised by this
storm, carried to section 5): the realloc retire lookup at
the old-block span check keeps the strict finder; in the
same transient window it returns NULL with the original
block preserved - a visible, retryable failure, not a
silent leak. Banking note: a72e2db is committed on team-04
and queues in roadmap order behind Z-040..Z-042; its two
core fixes bank with the slice, not before it. This verdict
cites its recorded run as committed evidence, the same
discipline the original build applied to Z-039..Z-044.

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
  (section 2). Z-038's independent session grades the same
  standing LOSS for both the banked core and the arena
  variant at every rung (rep-level proven; fold-in). The
  gap at 32 threads is the widest the campaign has recorded in relative terms (order of 94x /
  134x, indicative medians) because glibc keeps scaling into
  SMT while CURRENT is still recovering from its cliff.
- The 12-thread cliff and the 16-pinned dip are one wound in
  two instruments: central transactions priced cross-CCD.
  Z-040 removed the false-sharing tax on the lock-free fields
  around that path; it did not and could not remove the lock.
- The remedy question is HALF-ANSWERED (fold-in): the arena
  partition arm is measured - TIE vs banked at 21 of 24
  rungs, top-end mixed WIN only - and does not move the
  shape (Z-038). What remains OPEN is the decision:
  lock-sharding vs per-thread arenas under Z-039's R1-R3,
  owed to the team-08 follow-up. Wave 5 therefore ends, on
  landed evidence, with a diagnosis, one remedy arm priced,
  and a pre-registered decision procedure - not a fix.
- Cross-free flood behaviour at 32 threads: MEASURED
  (fold-in, Z-043) - direct remote free LOSES to glibc at
  every percentile with a ~1.07 s worst case (section 4);
  the queue's freer-side tail WIN does not change production
  behaviour (queue NOT wired into mimir_free; the Z-030
  total-cost LOSS stands). Z-043 also fixed two core defects
  the storm exposed (D1/D2, section 4); its named
  realloc-retire residual rides as future work. Churn
  teardown: PROVEN for the TLS surface (Z-042), OWED for
  any arena surface. Fork: posture proven and
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
  tests/perf/recorded_run_z13_z039.txt (banked seat 8aec32a;
  team-08 e5bf321).
- Curves ARENA, Z-038 grades and Z-039 input (fold-in):
  tests/perf/recorded_run_z13_z038.txt (banked seat 7bc5473).
- Z-043 storm, D1/D2 fixes, worst-case latency (fold-in):
  tests/perf/recorded_run_z13_z043.txt (team-04 a72e2db;
  read first-hand from team-04's worktree - the file banks
  with Z-043).
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
- OWED rows trace to absence, verified first-hand at
  fold-in: no Z-039 decision-follow-up commit on team-08
  (no decision commit; HEAD at finish is merge 9e86b51)
  and no landed Z-041-on-arena,
  Z-042-on-arena, or assembled-final-core ladder run on any
  branch.

## 7. Verdict and what remains owed - fold-in complete, decision owed

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
sharpened by a new stranding clause (Z-044). The fold-in
completes the wave's evidence: the arena partition is priced
(TIE overall, top-end mixed WIN only; Z-038) and the
cross-free flood is measured (direct remote free LOSS at
every percentile, ~1.07 s worst case, two core defects found
and fixed; Z-043).

FOLDED IN by this commit (the build's fold-in list,
discharged):
1. Z-038's three-way ladder: section 2's ARENA column
   filled, the three-variant curve set completed, and
   section 3's KL-B close restated with the arena priced.
2. Z-043's free storm: storm verdict, the D1/D2 defect
   fixes, and worst-case remote-free latency folded into
   sections 3, 4 and 5.

STILL OWED after this fold-in (each verified unlanded at
fold-in time):
1. Z-039's decision follow-up: apply R1-R3 to Z-038's banked
   table; record the chosen geometry, the road not taken
   with its R-rule, and the docs/DECISION_LEDGER.md entry;
   build the chosen geometry. Being built by team-08 in
   parallel; NOT landed at fold-in (no decision commit; team-08
   HEAD at finish is merge 9e86b51).
2. Dependent re-runs the landed slices themselves name:
   the Z-041 affinity matrix on the arena variant
   (Z-041-on-arena), and Z-042's churn storm on the arena
   teardown surface (Z-042-on-arena) - both still unrun.
3. An assembled-final-core ladder re-run (post-Z-040
   layout): Z-040's end-to-end effect remains indicative
   only; the re-run stands as named owed rigour.
4. Z-043's named realloc-retire residual: the realloc retire
   lookup keeps the strict finder and can return NULL
   (original block preserved, retryable) in the D2 transient
   window - future free-path work named by Z-043 itself.

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

Fold-in note: this fold-in was executed by a fresh team-05
lead running the six Mythic Engineering roles sequentially
(traceability audit of the banked Z-038 run and team-04's
Z-043 run, both read first-hand; arena-column fidelity at
source precision; Z-043 defect and verdict fidelity;
owed-list re-verification against team-08's HEAD; structure
and gate; KL-B synthesis) - the same platform-depth fallback
as the build, recorded plainly. Bugs discovered by the
fold-in: none. No allocator code was touched by this
fold-in.
