# Z13 Wave 7 Verdict

Slice: Z-063 - Wave 7 verdict (roadmap docs/MIMIRALLOC_Z13_ROADMAP.md).
Built 2026-11-12 by team-02 (fresh lead + the six Mythic
Engineering roles run SEQUENTIALLY by the lead as reviewers -
see the team note in section 9; no parallel sub-agents were
spawned this run, and that is recorded plainly here and in
CAMPAIGN_STATE.md).
Base: merge acb60f0 of seat main c0e19cd (banked Z-001..Z-060,
ls-remote verified by the keeper) into yrsa/mimiralloc-team-02 -
clean merge, no conflicts, merged tree byte-identical to seat
main.

STATUS: BUILT WITH OWED SECTIONS (the Z-036 / Z-045 pattern).
Every Wave 7 slice banked at the seat when this verdict was
built (Z-055..Z-060) is FINAL below, each traced to its banked
recorded run. Two Wave 7 slices were NOT yet banked at build
time and are carried as clearly-marked OWED sections
(section 6): Z-061 (Soak 2 - the one-hour soak was RUNNING
under a fresh team-01 lead at build time) and Z-062
(reuse-distance and L3 - team-06 finishing; its recorded run
had not banked). No number from either owed slice is cited
as fact anywhere in this document. The fold-in, when both
bank, is amended-not-rewritten per the Z-036 / Z-045
precedent, from exactly the recorded runs named in section 6.
The headline the banked evidence supports: KL-D's slow half
is DISCHARGED as a long-run fear - over a full hour of mixed
lifecycle the banked allocator does not drift, does not
bloat, and holds resident shape BETTER than glibc (soak
net_factor median 1.988 vs 2.500, Z-057) - while KL-D's fast
half, the per-operation wound, stands exactly where Z-054
left it (mixed_lifecycle LOSS 71.9x same-window), untouched
by any Wave 7 slice and restated at full size in section 5.
Fragmentation on this host is now mechanism-complete: it is
class-locked, page-granular, never healed inside a page
(Z-055, Z-058), and the one placement change that might
have softened it was tried and REJECTED on its own numbers
(Z-059, DECISION D-009 NO-ADOPTION).

## Scope and honesty - read first

- Wave 7 (Z-055..Z-063) is fragmentation and long-run
  behaviour: KL-D's slow half (the Z-054 verdict chartered
  this wave on it). The fast half - the per-op standings -
  belongs to Z-054 and is restated, not re-measured, here.
- The roadmap's Z-063 text asks for: fragmentation + soak
  standings, both soaks, both allocators; KL-D's slow half
  restated. "Both soaks" is discharged for Soak 1 (Z-057,
  banked, section 3) and OWED for Soak 2 (Z-061, section 6):
  at build time only one of the two soaks had banked, and
  this verdict says so in its STATUS rather than writing
  around the gap.
- Every number below traces to a banked recorded run
  (section 8's index). This slice executed NO new benchmark;
  the only fresh run on the final tree is the standing
  correctness gate (Purpose and gate, below).
- Load honesty per source, stated where it matters: the
  graded quantities in Z-055 / Z-058 / Z-059 / Z-060 are
  exact allocator counters, address identities, or pattern
  checks at quiesced points - co-tenant load cannot change
  them. The soak latency batches (Z-056 / Z-057) are
  shared-machine samples, never baselines, and are graded
  as shape only. RSS / PSS are per-process smaps_rollup
  reads (resident shape, contention-insensitive).
- The soak legs are time-boxed, not op-boxed (Z-056's
  design, carried by Z-057): the legs share duration and
  workload distribution, never the operation sequence, so
  drift curves compare as resident-shape series, never as
  equal-work measurements. Throughput ratios are reported
  with that caveat attached, every time.
- No production code is changed by this slice. The standing
  gate is GREEN 4988 / 0 / 77 (Purpose and gate).

## Purpose and gate

Roadmap gate for Z-063: "verdict doc traceable to recorded
runs." This document is that verdict for Wave 7, for the
banked part of the wave; the owed part is named in
section 6 with its fold-in sources.
Gate status: MET for Z-055..Z-060 (every number cites its
recorded run in section 8); OWED for Z-061 / Z-062 exactly
as section 6 specifies.
Standing correctness gate: scripts/z13/run_gate.sh GREEN
4988 checks / 0 failed / 77 by-design skips on the final
tree (log hidden_files/z063_gate.log, counts read from the
log) - identical to the Z-001 baseline; this slice touches
no code, so the delta is zero by construction.

## Host of record

purpleraider (ASUS ROG Flow Z13, GZ302EA), AMD Ryzen AI MAX+
395 (Strix Halo): 16 cores / 32 threads, 2 CCDs, single NUMA
node (UMA), L3 64 MiB (2 x 32 MiB), 128 GB LPDDR5X, 4 KiB
base pages, THP madvise. Ubuntu 26.10, kernel
7.3.0-10-generic, glibc 2.44, governor performance. Full
header: scripts/z13/host_truth.sh (Z-002), annexed verbatim
in tests/perf/recorded_run_z13_z063.txt (captured
2026-11-12T16:18:13Z, at this build).

## 1. Evidence base - landed and owed

LANDED (banked at the seat; every recorded run read
first-hand from disk by this lead):
| Slice | State | Seat commit | Recorded run |
|---|---|---|---|
| Z-055 fragmentation oracle | BANKED | 7b49210 | tests/perf/recorded_run_z13_z055.txt |
| Z-056 soak harness (pilot) | BANKED | c2ed635 | tests/perf/recorded_run_z13_z056.txt |
| Z-057 Soak 1: banked allocator | BANKED | 3d9c267 | tests/perf/recorded_run_z13_z057.txt |
| Z-058 split and coalesce audit | BANKED | adb7f56 | tests/perf/recorded_run_z13_z058.txt |
| Z-059 placement policy trial | BANKED | f4ef75f | tests/perf/recorded_run_z13_z059.txt + docs/DECISION_LEDGER.md D-009 |
| Z-060 pressure cycles | BANKED | c0e19cd | tests/perf/recorded_run_z13_z060.txt + docs/Z13_PRESSURE_CYCLES.md |
| Z-054 Wave 6 verdict (KL-D standing) | BANKED (Wave 6) | 0925d37 | docs/Z13_WAVE06_VERDICT.md + tests/perf/recorded_run_z13_z054.txt |
| Z-053 large-path fragmentation (context) | BANKED (Wave 6) | 8df68e8 | tests/perf/recorded_run_z13_z053.txt |

OWED at this verdict (named, not buried; section 6 carries
exactly what folds in, from which recorded runs):
| Slice | State at build | Fold-in source (once banked) |
|---|---|---|
| Z-061 Soak 2: improved allocator | RUNNING (team-01, one-hour soak live) | tests/perf/recorded_run_z13_z061.txt |
| Z-062 Reuse-distance and L3 | FINISHING (team-06; recorded run + commit pending) | tests/perf/recorded_run_z13_z062.txt |
Nothing from either owed slice is cited as fact below.

## 2. Fragmentation standings - the oracle, the audit, the trial

### 2.1 Z-055 - the F-series on this host: PARTIAL filing

Gate MET as an F-series PARTIAL: F-1, F-2, F-3, F-4 and F-6
are filed with this host's header (x86-64, page 4096,
capacities 64->62 tail 32 / 256->15 tail 160 / 512->7 /
1024->3 tail 928); F-5 is ABSENT - NOT MEASURED, because no
budget enforcement exists at this base (INC-I006-01,
carried) - reported absent, never approximated. The filing
is never to be cited as a complete F-1..F-6 filing.
Oracle verdict: checks=120 failures=0, RESULT: PASS; both
F-6 children PASS (A 9/0, D 7/0); final reconciliation exact
(span_published=66 span_retired=66, small_alloc=527
small_free=527, map_published=2 map_retired=2).

The fragmented point, measured (quiesced counters):
- F-3 free-but-unusable = 32064 B (bearing-span free slots
  + Q-RETAINED, I-002 formula); R-EXT = F-3 / Q-MAPPED =
  0.260937 (Q-MAPPED 122880); R-INT = tails / Q-LIVESPAN =
  0.091146.
- F-1(b) span tails total 3360 B over the bearing spans;
  class-1024 tail 928 B. F-1(a) live term 15 x (1024-1000)
  = 360 B, workload-record exact (INC-I002-02).
- F-2 mapping overhead: 20000 req -> len 24576 (overhead
  4576); 40000 req -> len 45056 (overhead 5056); total
  9632 B on this host's 4 KiB arithmetic.
- Fragmented state per class (acquired / sweep-released /
  retained-empty / bearing / live): 64: 4/0/1/3/93;
  256: 4/0/1/3/23; 1024: 6/1/2/3/5.
- Cross-class proof: a class-128 allocation acquired a
  fresh span despite 32064 B of F-3 free bytes, while a
  class-64 allocation reused a hole with no new span -
  F-3 bytes are class-locked, exactly as the F-series
  definition states.
- F-4 reclaim: free-all past the bound released every
  bearing span past 2-per-class (6 retained spans,
  Q-RETAINED 24576); partial trim(2) released exactly 2;
  full trim released the remaining 4, the OS anonymous
  drop equalling the retained pages exactly (16384 B).
- F-6 trajectory filed at every phase AND by the
  cross-slice leg: O-08 / INC-I006-02 is DISCHARGED for
  x86-64. Child A (large-fill, 64-page headroom) served
  exactly 32 large blocks before NULL; after free-all the
  32 mappings sat pooled byte-exact (262144 B, Z-048) until
  trim released the pool. Child D (trim under pressure,
  128-page headroom) held 2 retained spans (8192 B), served
  63 large blocks before NULL, trim released exactly those
  2 spans, and a large request was then served under the
  same limit.

### 2.2 Z-058 - split and coalesce: the mechanism audit

Gate MET: pattern-by-pattern table (checks=69 failures=0,
RESULT: PASS), worst case named. The audit asked one
question of every freed space - can it be SPLIT for a
smaller request, or COALESCED toward a larger one? The
answer, at every granularity, is no, with exactly one
exception:

- Split exists in exactly one form: span carving at birth
  (P1 - a fresh page carved once into fixed class slots,
  one-way, one class per span for its life). No free block,
  span slot, pooled mapping, or realloc tail is ever split
  (P3 cross-class split NO; P6 pooled-mapping split NO;
  P9 realloc shrink split NO - the shrink stays in place,
  live_mapping_bytes 45056 -> 45056, its 25056 B tail -
  55.6% of the mapping - dead until free).
- Coalesce: none, at any granularity below the page.
  Adjacent free slots never merge (P2); freed slots of one
  class never serve another (P3); pooled mappings never
  merge (P7). The only coalescing-like act is whole-page
  release past the retention bound (P10: after free-all +
  full trim, mapped returns to baseline 0 exactly).
- Reuse is exact-size only, everywhere: a freed class slot
  serves only its class (P2; P5 - free-order reuse is
  exact-slot but NOT strict LIFO and NOT prompt: in the
  run of record the freed slots reappear at batch indices
  62 and 189 of the next 256 allocations, TLS-batch
  timing); a pooled mapping serves only its exact length
  (P8, at the same address).
- WORST CASE, named per the gate: P4, the pin trap. One
  live 64 B block pins its whole 4096 B span page: 4032 B
  of that page (98.44%) is free-but-unusable - it cannot be
  split, cannot coalesce, cannot be released until the
  last block frees. Measured at the pinned point: 28672 B
  stood mapped behind exactly 1 live class-64 object, with
  24576 B of retained empty spans beside it. This is
  Z-055's F-3 reduced to its atomic cause: in this
  allocator fragmentation is never healed inside a page,
  only waited out at page granularity.

### 2.3 Z-059 - placement trial: DECISION D-009, NO-ADOPTION

Gate MET: policy comparison table + ledger decision.
Control first: all three builds (current / first-fit /
best-fit) reproduce Z-055's fragmented state EXACTLY in
leg A (F-3 32064, tails 3360, R-EXT 0.260937, R-INT
0.091146, Q-MAPPED 122880, Q-RETAINED 16384, per-class
counts identical), so the trial is proven against the
oracle before any decision-leg number is read. Each policy
ran 3 runs, checks=49 failures=0 each, METRIC lines
identical across runs; the trial code is flag-gated and
the flag-off core object's .text is byte-identical to the
seat core's.

Decision leg (leg B, 24 follow-on allocations per class
from the fragmented point):
| Class | Policy | Touched spans | New spans | Empty total |
|---|---|---|---|---|
| 64 | current | 1 | 0 | 3 |
| 64 | first-fit | 1 | 0 | 3 |
| 64 | best-fit | 1 | 0 | 3 |
| 256 | current | 2 | 0 | 2 |
| 256 | first-fit | 2 | 0 | 2 |
| 256 | best-fit | 3 | 0 | 1 |

Grades: current vs first-fit TIE on every measured
quantity (the active-span shortcut costs nothing measurable
here - itself a filed result). Best-fit LOSS to current on
the decision leg at class 256 (touched 3 vs 2, empty_total
1 vs 2), TIE at class 64 and on f3class. The mechanism is
in the numbers: at class 256 the holes per span are
14 / 7 / 13; tightest-first consumes the small holes first
(7, then 13), still owes 4 of the 24 follow-ons, and is
forced into the emptiest span - the span best-fit exists to
spare is exactly the span it ends up occupying.
Pre-registered clauses (b) and (c) failed, so the decision
needed no discretion: DECISION D-009 - NO-ADOPTION; the
current policy stays; the trial flags
(MIMIR_PLACEMENT_FIRSTFIT / MIMIR_PLACEMENT_BESTFIT) remain
available. Re-open condition, named in the run: a burst-size
sweep at follow-on volumes that fit inside the tight holes
(4..16 at class 256) - the one regime this trial does not
measure.

### 2.4 Relation to the large path (Z-053, Wave 6 context)

Wave 7's oracle is the SMALL path's fragmentation record.
The large path's record stands from Z-053 (banked, Wave 6):
worst-phase median net_factor checker 3.804 (glibc 1.031),
uniform 1.559 (1.043), pinwheel 2.537 (1.077), churn 1.634
(1.480) - the holed-phase blow-up there is the Z-048 pool's
bounded retention price. The two records compose, they do
not compete: small-path fragmentation is class-locked and
page-pinned (Z-055 / Z-058); large-path fragmentation is
pool-retention-shaped and capped by the pool's own bounds
(Z-053, re-proven under pressure by Z-060, section 4).
No same-window glibc fragmentation grade exists for the
small path in any banked Wave 7 run, and none is claimed:
the oracle has no glibc leg. The allocator-vs-glibc
fragmentation comparison the banked evidence DOES carry is
the soak's resident shape (section 3) and Z-053's factors.

## 3. Soak standings - pilot and Soak 1, both allocators

### 3.1 Z-056 - the harness and its pilot (600 s)

Gate MET: harness proven on a 10-minute pilot with complete
samples. Both legs COMPLETE - 61/61 scheduled samples each
(t = 0..600 step 10), DRAINED + DONE present, both exit 0,
summariser verdict ALL LEGS COMPLETE, collect exit 0.
- mimir: 43,181,056 ops; peak RSS 174,744 KiB; net_factor
  median 2.016 / worst 2.638; drained retained net
  36,100 KiB / +83 VMA.
- glibc: 271,437,568 ops (6.29x - time-boxed caveat,
  Scope); peak RSS 198,468 KiB; net_factor median 2.522 /
  worst 3.275; drained retained net 194,444 KiB
  (in-process arena retention; exit returns all).
- Latency samples (shared machine, NOT baselines): small
  median 238 vs 44 ns/op; large 2290 vs 135 ns/op.
- Integrity: 61 samples x 2 legs, zero stored-byte /
  calloc / realloc verification failures.
Pilot reading (Z-056 F2): on the continuous mixed
lifecycle, mimir holds LESS resident memory per live byte
than glibc - resident shape WIN on this workload - which
does not contradict Z-053's holed-phase LOSS (a single
adversarial free pattern at a frozen phase is a different
workload).

### 3.2 Z-057 - Soak 1: the banked allocator (3600 s)

Gate MET: soak report (drift curves, worst fragmentation,
tail RSS). Both legs COMPLETE - 61/61 scheduled samples
each (t = 0, 60, ..., 3600), DRAINED + DONE present, both
probes exit 0, ALL LEGS COMPLETE, collect exit 0. Soak
target of record: the banked allocator at seat main
c2ed635 (the current banked evolution of the allocator
whose baseline of record was filed at Z-009), built by the
banked Z-056 harness, probe binaries byte-identical to the
Z-056 recorded builds.

| Quantity (one hour) | mimir | glibc |
|---|---|---|
| Ops total (time-boxed) | 256,120,832 | 1,628,749,056 (6.36x) |
| Peak RSS | 179,672 KiB (at t=720) | 204,084 KiB |
| Live bytes, peak | 105,892,768 B | 98,299,712 B |
| net_factor median / worst | 1.988 / 2.469 | 2.500 / 3.168 |
| DRAINED RSS / retained net | 38,772 KiB / 36,696 KiB (+73 VMA) | 186,480 KiB / 184,392 KiB (+0 VMA) |
| Latency small, median (worst) | 253 ns/op (329) | 44 ns/op (70) |
| Latency large, median (worst) | 2423 ns/op (2915) | 134 ns/op (191) |

Findings of record (Z-057 F1-F5):
- F1 - NO DRIFT over the hour, either allocator. mimir RSS
  median over the first 30 live samples 151,822 KiB vs
  148,730 KiB over the last 30 - flat to slightly DOWN;
  the final live sample (133,816 KiB) sits below the hour
  median (150,812 KiB) and peak RSS occurs early (t=720),
  not at the tail. glibc likewise flat (190,914 ->
  187,950 KiB half medians). Neither allocator degrades or
  bloats across 256M / 1.63B lifecycle operations.
- F2 - Resident shape: the pilot result HOLDS at 6x
  duration. net_factor median 1.988 / worst 2.469 (mimir)
  vs 2.500 / 3.168 (glibc), on near-equal live peaks.
  Grade: WIN on resident shape (this workload, this
  machine, this shared-host window).
- F3 - Worst fragmentation (mimir allocation-VMA window):
  median 108 holes carrying a median 52,664,320 hole
  bytes; worst total hole bytes 76,947,456 B at t=2280;
  worst hole count 123; worst single largest hole
  24,289,280 B; worst net_factor sample 2.469 at t=1320.
  glibc's window shows 0 holes at every sample - its
  fragmentation cost appears as the higher net_factor of
  F2. Address holes and resident holes remain different
  quantities (the Z-053 F2 distinction, holding).
- F4 - Tail RSS and drain retention: mimir DRAINED RSS
  38,772 KiB (retained net 36,696 KiB) - essentially
  unchanged from the 600 s pilot's 36,100 KiB net:
  retention does NOT grow with soak duration. glibc
  DRAINED 186,480 KiB equals its final live sample
  (in-process arena retention, returned at exit). Process
  exit returns everything for both legs.
- F5 - Throughput and latency (shared-machine samples,
  NOT baselines): glibc executed 6.36x the operations in
  the same hour (the legs are not equal-work measurements);
  latency batches flat all hour with no degradation trend,
  at the pilot's shape: small LOSS ~5.7x, large LOSS ~18x
  on batch medians. The latency wound is steady, not
  growing - which is precisely the slow-half question.

## 4. Pressure and retention - Z-060 discharges the owed leg

Gate MET: cycle report; no unbounded retention observed,
and the bound that failed is named. Verdict: PASS -
cycles 3/3 attempts x 5 cycles, tuples identical in all
15 cycles; caps PASS; MADV legs PASS; ceiling PASS inside
its scope; hog contained at exit 137; hogfree PASS.

- NO UNBOUNDED RETENTION: span retention 393,216 B at
  bound 16; the mapping pool hard-capped at 64 units /
  33,554,432 B (32 MiB); BOTH return to exactly 0 under
  one FULL policy evaluation in every cycle, RSS following
  to the KiB (drop exactly 7200 KiB =
  (393216 + 6979584) / 1024).
- This discharges the mapping-pool pressure-cycle leg the
  Wave 6 verdict carried as OWED to Z-060 (Z-054 section 6,
  item 3; Z-050's own condition-pending clauses CP-1..CP-5
  are the table Z-060 rules on, in docs/Z13_PRESSURE_CYCLES.md):
  CP-1 DISCHARGED-PROVEN. CP-4 DISCHARGED-WITH-CORRECTION
  (the byte cap is 32 MiB, not 64 MiB; no <=16 MiB
  eligibility line exists - a 20,975,616 B mapping pools
  whole). CP-5 DISCHARGED BY DECISION (MADV_FREE priced
  both ways and REJECTED as a trim mechanism).
- THE BOUND THAT FAILED IS NAMED (a policy bound, not a
  retention bound): Z-050 CP-3's PARTIAL DONTNEED course
  does not exist on the banked implementation - PARTIAL
  released 0 pooled bytes in all 15 cycles
  (partial_pool_delta=0). CP-2 (LRU-per-bucket eviction
  order) is also NOT DISCHARGED: single-list LIFO is the
  implementation. Both stand as named gaps in section 7.
- Neighbouring standalone suites on Z-060's final tree:
  test-z048 57/0 PASS, test-z050 60/0 PASS, test-z052
  35/0 PASS; standing gate GREEN 4988/0/77.

## 5. KL-D restated - the fast half stands, the slow half closes

KL-D entered this line as mixed_lifecycle's standing:
the lifecycle workload churns across small classes and
larges through every path at once, and it loses.

THE FAST HALF (of record, Z-054, NOT re-measured by any
Wave 7 slice): mixed_lifecycle LOSS 71.927x same-window
(run 2 of record: mimir 505.65 vs glibc 7.03 ns/op) -
STILL LOST, reason named at Z-054: the paths the workload
exercises most (the small path's per-op latency, KL-A;
lifecycle bookkeeping across classes) are the ones Wave 6
did not rebuild. Its mimir absolute improved 898.76 (Z-009)
to 505.65 ns/op while the factor worsened, because glibc's
absolute improved more between windows. The soak's batch
latencies (Z-057 F5) are the same wound seen from the side
- small ~5.7x, large ~18x on shared-machine medians - and
are samples, not a new standing. NO Wave 7 number may be
read as progress against the 71.9x standing; no Wave 7
slice re-ran the K-002 standings, so 71.927x remains the
factor of record entering Wave 8.

THE SLOW HALF (this wave's hunt - what the loss does over
TIME, under fragmentation and pressure):
- It does not grow. One hour of the exact mixed lifecycle
  that names KL-D: no upward drift in RSS or net_factor,
  peak RSS early not late, latency batches flat all hour
  (Z-057 F1, F5). The 600 s pilot and the 3600 s soak agree
  (Z-056, Z-057).
- Its resident shape is a WIN, not a loss: net_factor
  median 1.988 vs glibc 2.500 over the hour; drain
  retention bounded, duration-independent (36,100 KiB net
  after 600 s; 36,696 KiB after 3600 s), and pressure
  cycles return both retention stores to exactly 0 under
  one FULL evaluation, every cycle (Z-057 F2/F4, Z-060).
- Its fragmentation mechanism is fully named and is
  architectural, not accretive: class-locked free bytes
  (F-3 32064 B, R-EXT 0.260937 at the oracle's fragmented
  point), no split and no coalesce below the page, the
  P4 pin trap as the atomic worst case (one 64 B block
  holding 4032 B hostage), whole-page release as the only
  healing (Z-055, Z-058). The shape is present from the
  first fragmented moment; the soak proves it does not
  worsen from there.
- The one softening lever tried this wave does not work:
  placement policy. Best-fit LOSS at class 256, first-fit
  TIE with current, DECISION D-009 NO-ADOPTION (Z-059).
  Whatever KL-D's slow half costs, span-choice order is
  not where it is paid.

THE RESTATEMENT, in roadmap words: KL-D is a STEADY-STATE
per-operation LOSS (71.9x, fast half, Z-054) whose slow
half - drift, bloat, fragmentation growth, retention
growth over long mixed-lifecycle runs - is DISCHARGED on
this host: no long-run degradation was found by any banked
Wave 7 measurement; the allocator ends an hour of KL-D's
own workload holding less resident memory per live byte
than glibc does. What remains of KL-D is the fast half
alone, and it belongs to the per-op hunts (KL-A and the
lifecycle bookkeeping), not to time.
OWED dependence inside this restatement: Soak 2 (Z-061)
repeats the soak on the post-Wave-6 allocator and rules
curve-by-curve against Z-057; if its curves contradict F1,
F2 or F4 above, this section's slow-half closure must be
re-opened at fold-in and the contradiction named, not
averaged away. Z-062's reuse-distance evidence bears on
the F3 hole shape's cache cost, not on the drift verdict.
Both dependencies are marked, per section 6.

## 6. OWED sections - exactly what folds in, from where

Fold-in discipline (Z-036 / Z-045 precedent): when each
slice banks at the seat, a fold-in commit amends this
document - amended, not rewritten - adding the landed
verdicts at the places named below, every number traced to
the banked recorded run at source precision. Until then
these sections are placeholders by design, and nothing in
sections 1-5 or 7-9 depends on an owed number.

### 6.1 OWED - Z-061, Soak 2: the improved allocator

State at build: the one-hour soak was RUNNING under a
fresh team-01 lead (the Z-056 harness repeated on the
post-Wave-6 allocator, per the roadmap). NOT banked; no
result of any kind is asserted here.
Folds in, from tests/perf/recorded_run_z13_z061.txt once
banked, into section 3 as a new section 3.3 and into
section 5's marked dependence:
- completeness (scheduled samples per leg, DRAINED + DONE,
  exit codes, summariser verdict);
- the curve-by-curve comparison vs Z-057 the roadmap gate
  names: per-leg ops totals, RSS first / median / peak /
  final, first-30 vs last-30 drift medians, net_factor
  median / worst / final, hole medians and worsts, latency
  batch medians and worsts (as shared-machine samples),
  DRAINED RSS and retained net - each Z-061 figure beside
  its Z-057 counterpart from section 3.2's table;
- the soak comparison verdict in roadmap grades
  (WIN / TIE / LOSS per curve family), and an explicit
  ruling on whether Z-057's F1 (no drift), F2 (resident
  shape WIN) and F4 (duration-independent retention) stand,
  move, or fall on the improved allocator.

### 6.2 OWED - Z-062, Reuse-distance and L3

State at build: team-06 finishing - probe, runner and
summariser on disk with raw + summary and a GREEN gate
reported by the keeper; the recorded run and the commit
had NOT landed. NOT banked; no result of any kind is
asserted here.
Folds in, from tests/perf/recorded_run_z13_z062.txt once
banked, into section 2 as a new section 2.5 and into
section 5's marked dependence:
- the reuse-policy table of record (LIFO / FIFO per class,
  as the slice defines its legs) against the 64 MiB L3,
  at the desktop working-set sizes the slice measured;
- the cache-residency verdict per class (does hot reuse
  stay cache-resident), in roadmap grades;
- any per-class policy change the slice proposes, with its
  numbers, or its explicit no-change ruling;
- the bearing, if any, on Z-058's P5 finding (reuse is
  exact-slot on TLS-batch timing, not strict LIFO) - the
  two records must be reconciled at fold-in, not left
  standing side by side unexamined.

## 7. Wave 7 change log, Z-055 to Z-060

### Z-055 - The fragmentation oracle on this host - FINAL (banked 7b49210)
Gate MET as a PARTIAL filing: F-1..F-4 + F-6 filed with
this host's header, oracle 120/0 PASS, F-6 DISCHARGED for
x86-64; F-5 absent (no budget enforcement, INC-I006-01
carried). F-3 32064 B class-locked; R-EXT 0.260937,
R-INT 0.091146. Standing gate GREEN 4988/0/77.

### Z-056 - Soak harness - FINAL (banked c2ed635)
Gate MET: 10-minute pilot COMPLETE 61/61 samples both
legs; mimir 43.18M ops, peak RSS 174,744 KiB, resident
shape WIN on the pilot workload (net_factor median 2.016
vs 2.522); glibc 6.29x throughput (time-boxed caveat).
Harness defaults 3600 s / 60 s handed to Z-057. Gate GREEN.

### Z-057 - Soak 1: the banked allocator - FINAL (banked 3d9c267)
Gate MET: full one-hour soak, both legs COMPLETE 61/61;
NO drift either allocator; resident-shape WIN holds at 6x
duration (net_factor 1.988 vs 2.500); drain retention
36,696 KiB net, duration-independent; latency LOSS shape
steady. Banks ONLY its recorded run (harness unchanged).
Gate GREEN 4988/0/77.

### Z-058 - Split and coalesce audit - FINAL (banked adb7f56)
Gate MET: pattern table P1-P10, checks 69/0 PASS; worst
case NAMED - P4 pin trap (one 64 B block pins 4096 B,
4032 B / 98.44% free-but-unusable). Split = carve-at-birth
only; coalesce = none below the page; reuse = exact-size
only. No production code changed. Gate GREEN.

### Z-059 - Placement policy trial - FINAL (banked f4ef75f)
Gate MET: comparison table + DECISION D-009 NO-ADOPTION.
Leg A control reproduces Z-055 exactly in all three
builds; leg B: first-fit TIE with current everywhere,
best-fit LOSS at class 256 (touched 3 vs 2, empty 1 vs 2).
Trial code flag-gated, flag-off .text byte-identical.
Gate GREEN 4988/0/77.

### Z-060 - Pressure cycles on this host - FINAL (banked c0e19cd)
Gate MET: PASS 3/3 attempts x 5 cycles, identical tuples;
NO unbounded retention (span 393,216 B at bound 16; pool
64 / 32 MiB; both to exactly 0 under one FULL evaluation
every cycle). Discharges Z-050's owed pool leg:
CP-1 proven, CP-4 corrected, CP-5 decided; the failed
bound NAMED - CP-3's PARTIAL DONTNEED course does not
exist (0 pooled bytes in all 15 cycles); CP-2 not
discharged. Gate GREEN; neighbours z048 57/0, z050 60/0,
z052 35/0.

## 8. Remaining gap, itemised

1. Z-061 and Z-062 are OWED (section 6). Until they bank
   and fold in, this verdict is the wave's verdict for
   Z-055..Z-060 plus two named placeholders - the Z-045
   pattern, stated in the STATUS, not a completion claim
   for the whole wave.
2. KL-D's fast half: mixed_lifecycle LOSS 71.927x
   same-window (Z-054) - the largest standing wound on
   this line, untouched by Wave 7 by charter. Its slow
   half is discharged (section 5); the per-op hunt belongs
   to KL-A / lifecycle bookkeeping and no later wave is
   yet chartered against it by name in this document.
3. F-5 remains unmeasured: no budget enforcement exists at
   this base (INC-I006-01, carried through Z-055). The
   F-series filing stays PARTIAL until an enforcement
   slice exists.
4. Z-050 CP-3 / CP-2: the PARTIAL DONTNEED trim course
   does not exist on the banked implementation and the
   pool's eviction is single-list LIFO, not LRU-per-bucket
   (Z-060). Named policy gaps; Z-060's PASS stands because
   its gate asked for the failed bound to be named, and
   it is.
5. Placement: D-009's re-open regime (burst sizes 4..16 at
   class 256, scan cost priced) is unmeasured. Until such
   a run exists, NO-ADOPTION stands and the current policy
   is the policy of record.
6. INC-Z048-01 (carried from the Z-054 verdict, section 6
   item 5): the Phase-era standalone batteries outside the
   standing gate's composition (i003 / h001 / g003 / h003 /
   j006 RED at baseline; the i004 / j002 / j003 fault shim
   inert under -flto). No banked Wave 7 run re-proves or
   retires it; it is carried here unchanged, as a gap of
   record, and the standing gate remains GREEN and
   unaffected.
7. The pin trap (Z-058 P4) has no mitigation on record:
   one long-lived small block holds its page's remaining
   98.44% hostage for the block's whole life. It is the
   design's price (one class per span, page-granular
   release), measured and named - not a defect, and not
   yet a chartered fix.

## 9. Traceability index

Every number above traces to exactly one landed file:
- F-series filing, oracle 120/0, F-3 / R-EXT / R-INT,
  F-4 reclaim, F-6 discharge, host header capacities:
  tests/perf/recorded_run_z13_z055.txt (7b49210).
- Pilot completeness, ops, RSS, net_factor medians /
  worsts, drained retention, latency medians, integrity:
  tests/perf/recorded_run_z13_z056.txt (c2ed635).
- Soak 1 completeness, ops, peak / drift / net_factor /
  holes / DRAINED / latency series and F1-F5:
  tests/perf/recorded_run_z13_z057.txt (3d9c267).
- Pattern table P1-P10, pin-trap measurements, P5 batch
  indices, split / coalesce / reuse reading:
  tests/perf/recorded_run_z13_z058.txt (adb7f56).
- Leg A control identity, leg B table, decision clauses,
  D-009 grades, flag-off identity:
  tests/perf/recorded_run_z13_z059.txt (f4ef75f) +
  docs/DECISION_LEDGER.md entry D-009.
- Cycle PASS, retention bounds and exact-0 returns, RSS
  drop 7200 KiB, CP-1..CP-5 rulings, neighbour suites:
  tests/perf/recorded_run_z13_z060.txt (c0e19cd) +
  docs/Z13_PRESSURE_CYCLES.md.
- KL-D fast-half standing (71.927x; 505.65 vs 7.03 ns/op;
  Z-009 absolute 898.76) and the Z-060 leg as owed there:
  docs/Z13_WAVE06_VERDICT.md sections 2, 3, 6 +
  tests/perf/recorded_run_z13_z054.txt (0925d37).
- Large-path fragmentation factors (checker / uniform /
  pinwheel / churn, both allocators):
  tests/perf/recorded_run_z13_z053.txt (8df68e8).
- Standing gate on this slice's final tree:
  hidden_files/z063_gate.log (team-local): checks=4988
  failed=0 skips=77, GATE RESULT: GREEN.
- OWED fold-in sources (not yet banked, cited as sources
  only): tests/perf/recorded_run_z13_z061.txt,
  tests/perf/recorded_run_z13_z062.txt.

## 10. Verdict and what remains owed

Wave 7's landed verdict: the long run is not where this
allocator loses. The wave opened on KL-D's slow half -
the suspicion that mixed_lifecycle's 71.9x per-op loss
compounds over time into drift, bloat, and fragmentation
growth - and the banked evidence acquits the allocator on
every slow count: an hour of its own worst workload with
no drift and no degradation (Z-057), resident shape held
better than glibc throughout (net_factor 1.988 vs 2.500),
retention bounded by named caps and returned to exactly
zero under pressure, every cycle (Z-060), and a
fragmentation mechanism that is fully architectural -
class-locked, page-pinned, exact-size reuse, no split, no
coalesce - present from the first fragmented moment and
proven not to worsen from there (Z-055, Z-058, Z-057).
The wave also closed two honest negatives at full size:
placement cannot soften the shape (best-fit LOSS,
D-009 NO-ADOPTION, Z-059), and the F-series still cannot
be filed complete (F-5 absent, no budget enforcement).
And it discharged, with one correction and one named
failure, the pressure-cycle leg Wave 6 owed it (Z-060 on
Z-050's CP-1..CP-5). What the wave did not touch stands
at full size in section 5: KL-D's fast half, 71.9x,
exactly as Z-054 measured it.

REMAINS OWED after this verdict (section 6 carries each
with its fold-in source): Z-061's curve-by-curve soak
comparison and its ruling on Z-057's F1 / F2 / F4; Z-062's
reuse-policy table and its reconciliation with Z-058's
P5. When both have banked and folded in, this document
becomes the complete Wave 7 verdict by amendment. Until
then its STATUS line governs: BUILT WITH OWED SECTIONS.

Team note: this slice was executed by a fresh team-02 lead
(the team's previous slice, Z-056, was banked at c2ed635).
The lead ran the six Mythic Engineering roles SEQUENTIALLY
itself as reviewers - evidence audit (every number re-read
from the banked recorded runs on disk), fragmentation
review, soak review, KL-D restatement review, owed-scope
review, and gate / commit-scope review - because no
parallel sub-agents were spawned for this verdict slice;
their checks are folded above and recorded plainly here
and in CAMPAIGN_STATE.md, per the muster instruction. All
device-side work was executed by the lead alone, in this
worktree only. Bugs discovered in the allocator: none.
No new benchmark was run for this verdict; the only fresh
execution on the final tree is the standing gate.
