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
Fold-in base: merge e1aa993 of seat main 457a739 (banked
Z-001..Z-062) into yrsa/mimiralloc-team-02 - clean merge, no
conflicts; the merge added exactly the banked Z-061 recorded
run and the Z-062 artifacts. Fold-in 2026-11-12 by the
team-02 fold-in lead (the six Mythic Engineering roles run
SEQUENTIALLY by the lead as reviewers - see the team note in
section 10). This verdict is amended, not rewritten, by the
fold-in commit on top of dae8009 (Z-036 / Z-045 precedent).

STATUS: FOLD-IN COMPLETE - FINAL for the whole wave.
Every Wave 7 slice (Z-055..Z-062) is banked at the seat and
is FINAL below, each traced to its banked recorded run.
The two slices OWED at build (Z-061, Z-062; section 6 as
built) have BANKED (Z-061 seat 76ef825, Z-062 seat 457a739)
and are folded in by this amendment - amended, not
rewritten, per the Z-036 / Z-045 precedent, from exactly
the recorded runs named in section 6: Z-061 as the new
section 3.3 and the section 5 ruling, Z-062 as the new
section 2.5 and the section 5 bearing. No Wave 7 slice
remains owed after this fold-in; the gaps that remain are
the non-slice gaps of record in section 8, unchanged in
kind.
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
Fold-in headline: Soak 2 (Z-061) reproduces Soak 1 curve by
curve on the post-Wave-6 allocator - no drift (TIE: mimir
first-30 vs last-30 RSS medians 146,888 -> 149,122 KiB,
+1.5%, flat), the resident-shape WIN holds (net_factor
median 1.948 vs glibc 2.637), drain retention unchanged
(36,872 KiB net) - so Z-057's F1, F2 and F4 STAND, and the
slow-half closure in section 5 is confirmed, not re-opened.
Reuse-distance (Z-062) grades reuse ORDER second-order
(fifo/lifo 0.886-1.278 across 18 cells) and the shipped
native policy a LOSS everywhere - not an order effect but
the full-churn span-release / re-carve cliff (native pays
269x-2745x the emulated-LIFO median, growing
superlinearly); its per-CCD L3 correction (32 MiB, not the
roadmap's 64 MiB aggregate) and proposals P1-P4 are filed
in section 2.5, with NO policy change adopted.

## Scope and honesty - read first

- Wave 7 (Z-055..Z-063) is fragmentation and long-run
  behaviour: KL-D's slow half (the Z-054 verdict chartered
  this wave on it). The fast half - the per-op standings -
  belongs to Z-054 and is restated, not re-measured, here.
- The roadmap's Z-063 text asks for: fragmentation + soak
  standings, both soaks, both allocators; KL-D's slow half
  restated. "Both soaks" is discharged in full after the
  fold-in: Soak 1 (Z-057, section 3.2) and Soak 2 (Z-061,
  section 3.3, folded in from its banked recorded run).
  At build time only Soak 1 had banked; the build said so
  in its STATUS rather than writing around the gap, and
  this amendment closes the gap from the banked run.
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
runs." This document is that verdict for Wave 7, complete
after the fold-in: every number cites its recorded run in
section 9.
Gate status: MET for every Wave 7 slice, Z-055..Z-062
(Z-055..Z-060 at build; Z-061 / Z-062 at fold-in, from
their banked recorded runs, seat 76ef825 / 457a739).
Standing correctness gate: scripts/z13/run_gate.sh GREEN
4988 checks / 0 failed / 77 by-design skips on the build's
final tree (log hidden_files/z063_gate.log, counts read
from the log), and GREEN 4988 / 0 / 77 again on the
fold-in's final tree (log hidden_files/z063_foldin_gate.log,
counts read from the log) - identical to the Z-001
baseline; this slice touches no code, so the delta is zero
by construction.

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
| Z-061 Soak 2: improved allocator | BANKED (fold-in) | 76ef825 | tests/perf/recorded_run_z13_z061.txt |
| Z-062 reuse-distance and L3 | BANKED (fold-in) | 457a739 | tests/perf/recorded_run_z13_z062.txt |
| Z-054 Wave 6 verdict (KL-D standing) | BANKED (Wave 6) | 0925d37 | docs/Z13_WAVE06_VERDICT.md + tests/perf/recorded_run_z13_z054.txt |
| Z-053 large-path fragmentation (context) | BANKED (Wave 6) | 8df68e8 | tests/perf/recorded_run_z13_z053.txt |

OWED at build, FOLDED IN by this amendment (section 6
carries the fold-in record): Z-061 and Z-062 banked at the
seat (76ef825 / 457a739) and their recorded runs were read
first-hand from the merged tree for this fold-in. Nothing
in sections 1-5 or 7-10 now rests on an unbanked source.

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
### 2.5 Z-062 - reuse-distance and L3: order is second-order, the native churn path is the cliff (fold-in)

Gate MET (fold-in; banked 457a739): reuse-policy table of
record + per-class proposals with their numbers -
proposals only, NO core change made or adopted by Z-062.
Method (Z-004 protocol): one process per measured value,
pinned taskset -c 5 (CCD0); 5 measured processes per cell,
legs interleaved round-robin, headline median. Classes
64 / 256 / 1024 at footprints 65536, 2097152, 16777216,
33554432, 50331648, 100663296 B, plus large class 8192 at
2097152 / 33554432 / 100663296. Policies: native (the
allocator's own reuse order - MimirAlloc LIFO end-to-end:
TLS bin capacity 16 over central span free lists, both
LIFO; glibc tcache LIFO at these sizes) and emulated
lifo / fifo over a probe-held pool allocated once from
the same allocator (the lifo emulation is the control for
fifo: same blocks, only the order differs). Checksum
proof OK (one checksum per cell across all legs);
identical-leg agreement within 5% at 17 of 18 cells (sole
DISAGREE: class 256 / 32 MiB lifo parity, 10.1%); 12
DRIFT-REJECT split-half flags, concentrated in the glibc
fifo and class-256 / 1024 mid cells under shared-host
load - carried, never smoothed away; no headline verdict
rests on a flagged cell except the 256 / 32 MiB fifo WIN,
labelled INDICATIVE wherever cited. Load: quiet window
at main-sweep start (loadavg 0.10 / 0.38 / 1.11); the one
diagnostic cell completed by Z-062's finishing lead ran
contended (loadavg ~2.3-3.4, the Z-061 soak live on
cores 2/3) and is an n=1 indicative diagnostic only.
Headline medians are this-machine measurements under the
recorded load, not quiet-machine baselines.

TOPOLOGY CORRECTION, of record: the roadmap's "64 MiB L3"
is the lscpu AGGREGATE of two per-CCD caches. Per-CCD L3
is 32 MiB (Z-010 facts of record: L3 ONE-SIZE 32M /
ALL-SIZE 64M; the Z-010 pointer chase found the L3
boundary AT 32 MiB and expressly NO step at 64 MiB). The
probe is pinned to one CCD0 core, so the L3 that governs
every Z-062 number is the 32 MiB per-CCD L3: footprint
33554432 sits AT that boundary; 50331648 and 100663296
are BEYOND one CCD's L3. Footprints 65536 cross L1d
(48 KiB/core), 2097152 cross L2 (1 MiB/core), 16777216
sits inside per-CCD L3.

Reuse-policy table of record (emulated pair, mimir pool;
medians ns/block; grades on the 5% noise bound, ratio in
[0.95, 1.05] = TIE):
| Class | Footprint | lifo | fifo | fifo/lifo | Grade |
|---|---|---|---|---|---|
| 64 | 65536 | 1.366 | 1.396 | 1.022 | TIE |
| 64 | 2097152 | 1.441 | 1.432 | 0.994 | TIE |
| 64 | 16777216 | 2.792 | 3.568 | 1.278 | LOSS (fifo slower) |
| 64 | 33554432 | 6.016 | 6.442 | 1.071 | LOSS |
| 64 | 50331648 | 6.718 | 6.804 | 1.013 | TIE |
| 64 | 100663296 | 7.154 | 7.121 | 0.995 | TIE |
| 256 | 65536 | 2.641 | 2.866 | 1.085 | LOSS |
| 256 | 2097152 | 3.327 | 3.276 | 0.985 | TIE |
| 256 | 16777216 | 4.753 | 5.225 | 1.099 | LOSS |
| 256 | 33554432 | 16.796 | 15.742 | 0.937 | WIN (fifo) *INDICATIVE |
| 256 | 50331648 | 20.956 | 19.040 | 0.909 | WIN (fifo) |
| 256 | 100663296 | 23.328 | 20.658 | 0.886 | WIN (fifo) |
| 1024 | 65536 | 7.571 | 8.141 | 1.075 | LOSS |
| 1024 | 2097152 | 11.829 | 12.352 | 1.044 | TIE |
| 1024 | 16777216 | 20.733 | 23.961 | 1.156 | LOSS |
| 1024 | 33554432 | 65.892 | 69.698 | 1.058 | LOSS |
| 1024 | 50331648 | 70.689 | 77.867 | 1.102 | LOSS |
| 1024 | 100663296 | 79.218 | 82.140 | 1.037 | TIE |
*INDICATIVE: the 256 / 32 MiB cell carries a DRIFT-REJECT
on both emulated legs (fifo 9.8%, lifo 16.1%) and the
slice's only identical-leg DISAGREE (lifo parity 10.1%).

Verdicts of record (roadmap grades, Z-062 V1-V3):
- V1 - Reuse ORDER (LIFO vs FIFO), emulated pair:
  SECOND-ORDER. Across 18 cells the fifo/lifo ratio spans
  0.886-1.278; 7 TIE, 8 LOSS (fifo slower, worst +27.8%
  at 64 / 16 MiB), 3 WIN (fifo faster, all at class 256
  beyond the per-CCD L3 edge: -6.3% at 32 MiB INDICATIVE,
  -9.1% at 48 MiB, -11.4% at 96 MiB, the last two
  drift-clean). No footprint produces an order-driven
  cache cliff for either policy: the hypothesised LIFO
  hot-reuse advantage does not appear as a large effect
  at desktop working-set sizes, and neither does a FIFO
  collapse.
- V2 - Shipped native policy vs the emulated control:
  LOSS, and it is not an order effect. Mimir native pays
  269x-2745x the emulated-LIFO median in the main sweep
  (native/lifo 268.852x at the smallest cell, 64 / 64 KiB,
  growing superlinearly to 2745.239x at 1024 / 96 MiB),
  because a full free-all churn runs through span release
  and re-carve / re-map, not through the freelist order:
  the emulated legs reuse the same blocks at 1.4-82
  ns/block while native climbs superlinearly with
  footprint. The main sweep runs mimir native only where
  W <= 262144 blocks (rescoping disclosed in the run);
  above the cap, single-process diagnostics extend the
  same curve: 51437.049 and 50778.031 ns/block (64 /
  32 MiB, attempt 1, two full processes), 130593.008
  (64 / 48 MiB, n=1), 498637.968 (64 / 96 MiB, n=1),
  316759.294 (256 / 96 MiB, n=1). Against glibc native:
  mimir native LOSS at every measured cell (34.738x at
  64 / 64 KiB to 4219.487x at 1024 / 96 MiB); glibc
  native itself stays in a narrow band (10.4-13.4
  ns/block at class 64, 11.6-27.8 at 256, 25.2-51.5 at
  1024) at every footprint - glibc shows no such cliff.
  Large class 8192 (no lifo pair measured; emulated fifo
  is the control): mimir fifo 84.953 / 434.520 / 547.102
  vs mimir native 3154.074 / 14798.499 / 70135.099 vs
  glibc native 1179.961 / 1507.218 / 1862.206 ns/block at
  2 / 32 / 96 MiB - mimir native LOSS vs glibc native
  2.673x / 9.818x / 37.662x.
- V3 - Cache residency: hot reuse stays cache-resident
  on a gentle slope across L1 / L2 / per-CCD L3 for BOTH
  orders (e.g. class 1024 lifo 7.571 -> 11.829 -> 20.733
  -> 65.892 -> 70.689 -> 79.218 ns/block). The 32 MiB
  per-CCD boundary is visible as a slope change (class
  1024 lifo 20.733 -> 65.892 between 16 and 32 MiB),
  never as a step to DRAM-only cost, because the touch
  pattern streams the working set either way. The
  residency story is a slope, not a wall.

Per-class policy proposals (Z-062 P1-P4; proposals only -
NO adoption in Z-062, and none is adopted by this
verdict):
- P1 - Class 256, footprints beyond the per-CCD L3 edge
  (> 32 MiB): PROPOSE FIFO (or FIFO-leaning) reuse for
  that regime. Numbers: fifo/lifo 0.937 (32 MiB,
  INDICATIVE - drift + parity flags), 0.909 (48 MiB),
  0.886 (96 MiB). Expected gain at most ~11%, and only
  in the beyond-L3 regime; below L3 the same class grades
  LOSS / TIE for fifo (1.085 at 64 KiB, 0.985 at 2 MiB,
  1.099 at 16 MiB), so the proposal is regime-scoped, not
  class-wide.
- P2 - Classes 64 and 1024: PROPOSE KEEPING LIFO (no
  change). Numbers: class 64 fifo grades LOSS at 16 MiB
  (1.278) and 32 MiB (1.071), TIE elsewhere; class 1024
  fifo grades LOSS at 4 of 6 footprints (worst 1.156 at
  16 MiB), TIE at the other two. No measured case for
  switching these classes.
- P3 - All classes, the dominant term: PROPOSE a future
  slice investigate the native full-churn cliff (span
  retention / reacquire path under free-all churn) BEFORE
  any order change is adopted: native / emulated ratios
  of 269x-2745x (main sweep) dwarf every order effect in
  the table (<= 1.278x). Any LIFO / FIFO adoption taken
  before the cliff is understood would be tuning the
  second-order term.
- P4 - Large class 8192: PROPOSE NO CHANGE. No lifo /
  fifo pair was measured for this class (fifo-only
  control), so no order claim is made; its native-vs-
  glibc LOSS (2.673x-37.662x) is the same cliff as V2,
  referred to P3.

Reconciliation with Z-058's P5 (required by section 6.2
as built): the two records describe different layers and
do not conflict. Z-058's P5 is an observation of the
small path's delivered free-order behaviour: reuse is
exact-slot but NOT strict LIFO and NOT prompt - the freed
slots reappeared at batch indices 62 and 189 of the next
256 allocations, on TLS-batch timing. Z-062's emulated
lifo / fifo legs are a probe-held pool order, not a claim
about that native batch timing, and Z-062's native leg is
graded as a whole path, not as an order claim: its LOSS
(V2) is priced to span release and re-carve under full
churn, a layer below both records' order questions.
Read together: at the slot level reuse is exact-size and
batch-timed (Z-058 P5); at the pool level order itself is
second-order (Z-062 V1); at the native-path level the
cost is the churn cliff, not the order (Z-062 V2 / P3).

## 3. Soak standings - pilot, Soak 1 and Soak 2, both allocators

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
### 3.3 Z-061 - Soak 2: the improved allocator (3600 s) (fold-in)

Gate MET (fold-in; banked 76ef825): soak report with the
curve-by-curve comparison verdict vs Z-057 - the roadmap
gate for this slice. Soak target: the CURRENT banked
allocator at seat main c0e19cd (the post-Wave-6 allocator
plus banked Wave 7 slices Z-055..Z-060), built by the
banked Z-056 harness from the merged tree; the harness is
unchanged (probe_z056.c sha256 887cb230..., run_z056.sh
d1e831f4..., summarize_z056.py 0bf0f302...) and the probe
binaries are byte-identical to the Z-056 / Z-057 recorded
builds (probe_mimir 500816e1..., probe_glibc 20f0cb0a...).
This slice banks ONLY its recorded run; no production
code changed. Window 2026-11-12T16:16:59Z to ~17:17Z,
detached (setsid + nohup); the soak was found LIVE and
healthy by Z-061's finishing lead and was never restarted.
Completeness: both legs COMPLETE - 61/61 scheduled
samples each (t = 0, 60, ..., 3600), DRAINED + DONE
present, both probes exit 0, both .err files empty,
summariser verdict ALL LEGS COMPLETE, collect exit 0.
Load (honest): SHARED HOST - loadavg 1.91 / 1.85 / 1.56
at soak start (team-06 Z-062 probe co-tenant on core 5),
2.55 / 2.77 / 3.02 at collect, 2.42 / 2.67 / 2.97 at
gate; legs pinned mimir -c 2 / glibc -c 3. RSS / PSS are
per-process smaps_rollup reads; latency batches are
shared-machine samples, NOT baselines. Time-boxed, not
op-boxed (Scope caveat applies to every ratio below).

| Quantity (one hour) | Z-061 mimir | Z-061 glibc | Z-057 mimir | Z-057 glibc |
|---|---|---|---|---|
| Ops total (time-boxed) | 253,541,376 | 1,598,629,888 (6.30x) | 256,120,832 | 1,628,749,056 (6.36x) |
| Peak RSS | 168,968 KiB (t=1200) | 196,964 KiB (t=960) | 179,672 KiB (t=720) | 204,084 KiB |
| Live bytes, peak | 95,730,553 B (t=1260) | 93,119,314 B (t=2220) | 105,892,768 B | 98,299,712 B |
| RSS first / median / final live | 2,120 / 147,868 / 144,360 KiB | 2,088 / 188,816 / 195,972 KiB | - / - / 133,816 KiB | - / - / - |
| net_factor median / worst | 1.948 / 2.515 (worst t=3240) | 2.637 / 3.357 (worst t=3120) | 1.988 / 2.469 (worst t=1320) | 2.500 / 3.168 |
| net_factor final | 1.920 | 3.237 | - | - |
| Holes median count / bytes | 106 / 48,533,504 B | 0 / 0 | 108 / 52,664,320 B | 0 / 0 |
| Worst hole bytes / count / single | 77,832,192 B (t=2700) / 122 (t=2280) / 21,606,400 B (t=2760) | 0 / 0 / 0 | 76,947,456 B (t=2280) / 123 / 24,289,280 B | 0 / 0 / 0 |
| DRAINED RSS / retained net | 38,992 KiB / 36,872 KiB (+78 VMA) | 195,972 KiB / 193,884 KiB (+0 VMA) | 38,772 KiB / 36,696 KiB (+73 VMA) | 186,480 KiB / 184,392 KiB (+0 VMA) |
| Latency small, median (worst) | 241 ns/op (456, t=840) | 45 ns/op (103, t=840) | 253 ns/op (329) | 44 ns/op (70) |
| Latency large, median (worst) | 2253 ns/op (5191, t=840) | 143 ns/op (311, t=840) | 2423 ns/op (2915) | 134 ns/op (191) |

Findings of record (Z-061 F1-F6, each against Z-057):
- F1 - Drift: TIE. mimir first-30 live-sample RSS median
  146,888 KiB vs last-30 median 149,122 KiB (+1.5%) -
  flat, NO upward drift; Z-057 mimir 151,822 -> 148,730
  KiB (-2.0%), also flat. glibc 188,526 -> 188,972 KiB
  (+0.2%), flat (Z-057: 190,914 -> 187,950). mimir final
  live sample 144,360 KiB sits below its hour median
  (147,868 KiB); peak RSS occurs mid-hour (t=1200), not
  at the tail (Z-057: final 133,816, peak at t=720).
- F2 - Resident shape: WIN holds. mimir net_factor
  median 1.948 / worst 2.515 vs glibc 2.637 / 3.357, on
  near-equal live peaks; mimir marginally better than
  Z-057 (1.988), glibc marginally worse (2.500) - the
  mimir-vs-glibc gap is unchanged in shape. Grade in
  roadmap words: WIN on resident shape (this workload,
  this machine, this shared-host window).
- F3 - Worst fragmentation: TIE. Median 106 holes
  carrying a median 48,533,504 hole bytes (Z-057: 108 /
  52,664,320); worst total hole bytes 77,832,192 B at
  t=2700 (Z-057: 76,947,456 at t=2280); worst count 122
  (Z-057: 123); worst single 21,606,400 B (Z-057:
  24,289,280 B). The fragmentation curve is the Z-057
  curve within run-to-run variation; no regression and
  no improvement claimed. glibc's window shows 0 holes
  at every sample in both runs.
- F4 - Tail RSS and drain retention: mimir DRAINED RSS
  38,992 KiB, retained net 36,872 KiB (+78 VMA) -
  essentially identical to Z-057 (38,772 / 36,696 / +73)
  and to the 600 s pilot posture: retention does NOT
  grow with duration and did not change between the two
  soaks. glibc DRAINED 195,972 KiB equals its final live
  sample (retained net 193,884 KiB, +0 VMA; Z-057:
  186,480 / 184,392 / 0) - in-process arena retention,
  returned at exit. Process exit returns everything for
  both legs.
- F5 - Throughput and latency: LOSS, same shape. glibc
  executed 6.30x the operations in the same hour
  (time-boxed caveat); mimir ops -1.0% vs Z-057, within
  shared-host variation - no throughput change claimed.
  Latency batches flat all hour with no degradation
  trend in either run, at the Z-057 shape (small 241 vs
  45, large 2253 vs 143 ns/op medians). The t=840 worsts
  in both legs coincide with a shared-host contention
  moment and are recorded as such, not as allocator
  events.
- F6 - Integrity: 61 samples x 2 legs, zero stored-byte /
  calloc / realloc verification failures; neither leg
  aborted; no allocator bug discovered, and no bug in
  the slice's tooling (the harness ran exactly as banked
  at Z-056).

OVERALL (Z-061's own verdict): the improved allocator
reproduces the Z-057 soak curves curve-by-curve - no
drift (TIE), resident-shape WIN vs glibc holds,
fragmentation TIE, drain retention unchanged, batch
latency LOSS vs glibc unchanged in shape. No regression
introduced by the Wave 7 slices banked between the two
soaks is visible in any curve, and no improvement is
claimed beyond the marginally lower mimir net_factor
median.
EXPLICIT RULING (required by section 6.1 as built) on
Z-057's findings: F1 (no drift) STANDS; F2 (resident
shape WIN) STANDS; F4 (duration-independent retention)
STANDS. None moves, none falls.

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
Fold-in ruling on this restatement's marked dependencies
(section 6 as built): DISCHARGED - the slow-half closure
is CONFIRMED, not re-opened. Soak 2 (Z-061, section 3.3)
rules curve-by-curve against Z-057 and contradicts none
of F1, F2 or F4: F1 (no drift) STANDS (mimir +1.5%
first-30 vs last-30 RSS medians, flat; grade TIE), F2
(resident-shape WIN) STANDS (net_factor median 1.948 vs
glibc 2.637), F4 (duration-independent retention) STANDS
(36,872 KiB net, unchanged between the soaks and the
pilot). No contradiction had to be named or averaged
away. Z-062's reuse-distance evidence (section 2.5)
bears as marked at build: it prices the reuse path
behind the F3 hole shape's cache cost - order
second-order, residency a gentle slope - and its native
LOSS is a full-churn span-release / re-carve cost, a
per-operation (fast-half) wound, not a drift, bloat or
retention effect; it does not touch the slow-half
verdict. The slow half therefore closes on the full
Wave 7 evidence, both soaks included.

## 6. Fold-in record - the sections OWED at build, discharged

Fold-in discipline (Z-036 / Z-045 precedent): when each
slice banks at the seat, a fold-in commit amends this
document - amended, not rewritten - adding the landed
verdicts at the places named below, every number traced
to the banked recorded run at source precision. At build
this section named two OWED sections, 6.1 (Z-061) and
6.2 (Z-062), as placeholders by design, and nothing in
sections 1-5 or 7-9 depended on an owed number. Both
have since banked and folded in; this section is now
their discharge record.

### 6.1 Z-061, Soak 2: the improved allocator - DISCHARGED

State at build: the one-hour soak was RUNNING under a
fresh team-01 lead (the Z-056 harness repeated on the
post-Wave-6 allocator, per the roadmap). NOT banked; no
result of any kind was asserted.
DISCHARGED by this fold-in: Z-061 banked at the seat as
76ef825; its recorded run
(tests/perf/recorded_run_z13_z061.txt) was read
first-hand from the merged tree and folded in at
exactly the places named at build: section 3.3 (the
full curve-by-curve comparison vs Z-057 - completeness,
per-leg ops totals, RSS first / median / peak / final,
first-30 vs last-30 drift medians, net_factor median /
worst / final, hole medians and worsts, latency batch
medians and worsts as shared-machine samples, DRAINED
RSS and retained net, each Z-061 figure beside its
Z-057 counterpart) and the section 5 ruling (Z-057's
F1, F2 and F4 STAND; the slow-half closure is
confirmed, not re-opened). The soak comparison verdict
in roadmap grades: drift TIE, resident shape WIN,
fragmentation TIE, drain retention unchanged, batch
latency LOSS in the Z-057 shape.

### 6.2 Z-062, Reuse-distance and L3 - DISCHARGED

State at build: team-06 finishing - probe, runner and
summariser on disk with raw + summary and a GREEN gate
reported by the keeper; the recorded run and the commit
had NOT landed. NOT banked; no result of any kind was
asserted.
DISCHARGED by this fold-in: Z-062 banked at the seat as
457a739; its recorded run
(tests/perf/recorded_run_z13_z062.txt) was read
first-hand from the merged tree and folded in at
exactly the places named at build: section 2.5 (the
reuse-policy table of record against the per-CCD L3 -
32 MiB, the roadmap's 64 MiB being the lscpu aggregate,
corrected in section 2.5; the cache-residency verdict
in roadmap grades; the per-class proposals P1-P4 with
their numbers, proposals only, no adoption) and the
section 5 bearing. The bearing on Z-058's P5 finding
is reconciled in section 2.5, as required at build:
the two records describe different layers and do not
conflict.

## 7. Wave 7 change log, Z-055 to Z-062

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

### Z-061 - Soak 2: the improved allocator - FINAL (banked 76ef825, fold-in)
Gate MET: full one-hour soak on the post-Wave-6
allocator, both legs COMPLETE 61/61; curve-by-curve vs
Z-057: drift TIE (+1.5% flat), resident-shape WIN holds
(net_factor 1.948 vs 2.637), fragmentation TIE, drain
retention 36,872 KiB net unchanged, latency LOSS shape
unchanged. Z-057's F1 / F2 / F4 STAND (section 3.3).
Banks ONLY its recorded run (harness unchanged). Gate
GREEN 4988/0/77.

### Z-062 - Reuse-distance and L3 - FINAL (banked 457a739, fold-in)
Gate MET: reuse-policy table of record + proposals
P1-P4, no adoption. Order SECOND-ORDER (fifo/lifo
0.886-1.278 across 18 cells); native policy LOSS
everywhere (269x-2745x the emulated medians - the
full-churn span-release / re-carve cliff, not an order
effect); per-CCD L3 is 32 MiB, not the roadmap's
64 MiB aggregate (section 2.5). No production code
changed. Gate GREEN 4988/0/77.

## 8. Remaining gap, itemised

1. The owed-slice gap is CLOSED by this fold-in:
   Z-061 and Z-062 banked (76ef825 / 457a739) and are
   folded in (sections 2.5, 3.3, 5, 6). No Wave 7 slice
   remains owed; the gaps below are the wave's
   non-slice gaps of record.
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
- Soak 2 completeness, ops, drift halves, net_factor /
  holes / DRAINED / latency series and F1-F6, and the
  comparison verdict vs Z-057:
  tests/perf/recorded_run_z13_z061.txt (76ef825).
- Reuse-policy table of record, V1-V3 verdicts, native
  diagnostics, residency slope, proposals P1-P4, L3
  topology reconciliation:
  tests/perf/recorded_run_z13_z062.txt (457a739).
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
- Fold-in gate on the amended tree:
  hidden_files/z063_foldin_gate.log (team-local):
  checks=4988 failed=0 skips=77, GATE RESULT: GREEN.

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

Fold-in completion: nothing remains owed. Z-061's
curve-by-curve soak comparison and its ruling on
Z-057's F1 / F2 / F4 are folded in at section 3.3 and
section 5 (F1, F2, F4 STAND; the slow-half closure is
confirmed); Z-062's reuse-policy table and its
reconciliation with Z-058's P5 are folded in at
section 2.5 (order second-order; the native LOSS is
the full-churn cliff; P1-P4 proposals only). With both
amendments this document is the complete Wave 7
verdict, and its STATUS line governs: FOLD-IN COMPLETE
- FINAL for the whole wave. The gaps that remain are
the non-slice gaps of section 8.

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

Fold-in team note: the fold-in was executed by team-02
fold-in leads running the six Mythic Engineering roles
SEQUENTIALLY themselves (no parallel sub-agents
spawned; recorded plainly per the muster instruction).
The first fold-in lead merged seat main 457a739
(e1aa993) and stalled after its start note; a fresh
fold-in lead took over from disk state, verified every
folded number first-hand against the banked recorded
runs in the merged tree, completed the amendment, and
ran the standing gate on the final tree. All work was
executed in this worktree only; nothing was pushed
from this worktree - banking happens at the seat by
the keeper.
