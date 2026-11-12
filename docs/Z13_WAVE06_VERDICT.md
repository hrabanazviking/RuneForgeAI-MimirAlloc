# Z13 Wave 6 Verdict

Slice: Z-054 - Wave 6 verdict (roadmap docs/MIMIRALLOC_Z13_ROADMAP.md).
Built 2026-11-12 by team-08 (fresh restart lead + six Mythic
Engineering sub-agents spawned as reviewers - see the team note
in section 8; the previous team-08 lead was interrupted by a
timeout mid-slice with no commit, and this lead continued from
its landed files: the K-002 runner, the summariser, and run 1).
Base: merge db1cea1 of seat main 8df68e8 (banked Z-001..Z-053,
ls-remote verified by the keeper) into yrsa/mimiralloc-team-08.

STATUS: COMPLETE. Every Wave 6 slice (Z-046..Z-053) is BANKED
at the seat, and this slice adds the wave's own fresh standings:
two full 7-rep K-002 runs on the banked Wave 6 core, executed
for this verdict (tests/perf/recorded_run_z13_z054.txt). The
headline: KL-C's home workload flipped - large_batch is a WIN
against glibc in the same window for the first time on this
hardware line - and KL-D's wound did not move: mixed_lifecycle
stands at LOSS 71.9x in the same window, STILL LOST with its
reason named in section 3; and the closure's price is
stated at full size there too (the pool's bounded retention:
holed-pattern RSS factors up to 3.804 vs glibc 1.031,
Z-053). The cutoff of record is UNCHANGED
at 1024 bytes; section 4 justifies that from the wave's own
pricing.

## Scope and honesty - read first

- Wave 6 (Z-046..Z-054) is the large-path hunt: KL-C (a mapping
  per allocation past the 1024-byte cutoff) attacked on this
  machine only, against Z-009's baseline of record and glibc.
- The roadmap's Z-054 text asks for: large_batch +
  mixed_lifecycle standings vs Z-009 and glibc; KL-C restated;
  the cutoff of record (if changed) justified. All three are
  discharged here, each traced to a recorded run (section 7).
- Window discipline governs every grade in this doc. The
  Z-009 window ran under named Tensile contention (its own
  record says so); the Z-054 run of record ran in a quiet
  window (loadavg 0.09 before, 0.24 after). Cross-window
  factors therefore flatter everyone: they are printed as the
  wave-movement record with the glibc window gauge beside
  them, and the SAME-WINDOW grades against glibc govern the
  KL standings. Where a leg is DRIFT-REJECT under the Z-004
  8% split-half bound, its factor is INDICATIVE only and the
  text says so at the number, not in a footnote.
- large_batch is the mapping pool's home workload (32 live
  blocks, six cycling sizes, endpoint touch). Its WIN is a
  fact about repeated, cycling large traffic inside the
  pool's bounds (64 mappings / 32 MiB). It does not generalise
  to pool-miss traffic, and section 3 prices what a miss
  still costs. Nothing in this doc claims otherwise.
- No production code is changed by this slice. The standing
  gate is GREEN 4988 / 0 / 77 (section: Purpose and gate).

## Purpose and gate

Roadmap gate for Z-054: "verdict doc traceable to recorded
runs." This document is that verdict for Wave 6.
Gate status: MET. Every number below cites its recorded run
in section 7's index; the two standings runs this slice
itself executed are filed verbatim in
tests/perf/recorded_run_z13_z054.txt.
Standing correctness gate: scripts/z13/run_gate.sh GREEN
4988 checks / 0 failed / 77 by-design skips on the merged
branch (log hidden_files/gate_z054.log, counts read from the
log) - identical to the Z-001 baseline; this slice touches
no code, so the delta is zero by construction.

## Host of record

purpleraider (ASUS ROG Flow Z13, GZ302EA), AMD Ryzen AI MAX+
395 (Strix Halo): 16 cores / 32 threads, 2 CCDs, single NUMA
node (UMA), L3 64 MiB (2 x 32 MiB), 128 GB LPDDR5X, 4 KiB
base pages, THP madvise. Ubuntu 26.10, kernel
7.3.0-10-generic, glibc 2.44, governor performance. Full
header: scripts/z13/host_truth.sh (Z-002), annexed verbatim
in tests/perf/recorded_run_z13_z054.txt (captured
2026-11-12T13:27:56Z, in the run window).

## 1. Evidence base - landed

LANDED (banked at the seat; every recorded run read first-hand
from disk by this lead):
| Slice | State | Seat commit | Recorded run |
|---|---|---|---|
| Z-046 threshold study | BANKED | 517bbb1 | tests/perf/recorded_run_z13_z046.txt |
| Z-047 mapping-cache design | BANKED | f82bfaa | tests/perf/recorded_run_z13_z047.txt + docs/Z13_MAPPING_CACHE_DESIGN.md |
| Z-048 mapping-cache implementation | BANKED | 7b8ebd4 | tests/perf/recorded_run_z13_z048.txt |
| Z-049 THP for cached mappings | BANKED | f8ee668 | tests/perf/recorded_run_z13_z049.txt |
| Z-050 trim policy under pressure | BANKED | 7780501 | tests/perf/recorded_run_z13_z050.txt + docs/Z13_TRIM_POLICY.md |
| Z-051 huge allocations | BANKED | 95d72d1 | tests/perf/recorded_run_z13_z051.txt |
| Z-052 mremap growth | BANKED | 2e4e8f1 | tests/perf/recorded_run_z13_z052.txt + docs/Z13_MREMAP_GROWTH.md |
| Z-053 large-path fragmentation | BANKED | 8df68e8 | tests/perf/recorded_run_z13_z053.txt |
| Z-009 baseline of record | BANKED (Wave 1) | - | docs/Z13_WAVE01_BASELINE.md (Z-003 medians) |
| Z-054 standings (this slice) | THIS COMMIT | - | tests/perf/recorded_run_z13_z054.txt (runs 1+2; raw CSVs hidden_files/z054_raw.csv, z054_raw_run2.csv, team-local) |

OWED at this verdict (named, not buried; section 6 carries
the full list): the Z-050 mapping-pool pressure-cycle leg,
OWED by Z-050 itself to Z-060; the Z-047/Z-048 invariant
reconciliation item Z-048 recorded at its build; and the
Phase-era standalone-battery defect family INC-Z048-01.
Nothing from any owed item is cited as fact anywhere below.

## 2. The standings - fresh K-002 on the banked Wave 6 core

Harness: tests/perf/bench_k002.c UNMODIFIED (sha256
30dd0d9a... - byte-identical to the Z-003/Z-009 harness of
record). Protocol (Z-004): separate processes pinned
taskset -c 2, 3 warm-ups discarded, 7 measured processes per
leg interleaved round-robin, headline = median ns/op, drift
bound 8%, tie band +/-5%. Checksums prove identical work on
every leg of both runs (12756656 / 34727552 / 1106008 /
40359936 - the K-002 checksums of record).
Run 2 is the run OF RECORD (quiet window, load-stamped;
raw hidden_files/z054_raw_run2.csv). Run 1 (previous lead,
load not captured) is the corroborating run: the mimir
medians of the two runs agree within 1.0% on all four
workloads, and within 2.3% of Z-048's pool median on
large_batch.

Same-window standings of record (run 2 medians; run 1 ratio
in brackets):
| Workload | glibc ns/op | mimir ns/op | Factor | Grade |
|---|---|---|---|---|
| small_churn | 3.32 | 28.52 | 8.590x [8.433x] | LOSS |
| mixed_batch | 3.68 | 27.15 | 7.378x [7.356x] | LOSS |
| large_batch | 658.29 + | 63.48 | 0.096x [0.116x] | WIN (factor INDICATIVE) |
| mixed_lifecycle | 7.03 | 505.65 | 71.927x [71.452x] | LOSS |
(+) glibc large_batch is DRIFT-REJECT in both runs (9.88%
run 2, 11.73% run 1; bound 8%): the leg is BIMODAL - each
run's seven reps split five-and-two between a ~540 and a
~660 ns/op band (rep sequences verbatim in the recorded
run), so its median lands in whichever band holds five
(547.70 run 1, 658.29 run 2) and the split-half statistic
trips by construction. The rejection retires the glibc
median as a precise quantity, not the comparison. The
mimir leg is drift-clean in both runs (0.23% / 1.93%).
The tie band (+/-5%) plays no role in any grade in this
table: every same-window ratio sits far outside it.

Reading the standings (each claim traced):
- large_batch WIN, factor indicative, grade proven. The
  factor against the rejected glibc median is ~0.10x and is
  quoted as INDICATIVE. The grade does not rest on that
  median: against glibc's fastest single rep in either run
  (535.49) the mimir median is 0.119x, against its slowest
  (680.50) 0.093x - an honest indicative range of
  0.09x-0.12x, roughly tenfold under every summary
  choice - and Z-048's drift-clean same-window A/B graded
  the pool 0.13x against glibc with the pre-pool core as its
  control (pool 64.95, base 2747.67, glibc 514.97 ns/op).
  Three windows, one direction, one rough size.
- mixed_lifecycle LOSS 71.9x is the wave's standing shame
  and it is reported at full size: the mimir absolute
  improved (898.76 at Z-009 to 505.65 now) and the factor
  still worsened against Z-009's 67.2x, because glibc's
  absolute improved more between the windows (13.38 to
  7.03). The same-window factor is the standing; the wound
  is KL-D's, and Wave 7 is its hunt (section 3).
- small_churn LOSS 8.6x and mixed_batch LOSS 7.4x are not
  Wave 6's hunts (KL-A / KL-E-KL-B), but the standings are
  this verdict's duty: both improved hugely against their
  Z-009 absolutes under Waves 4-5 work (93.43 to 28.52;
  398.42 to 27.15) and both still lose in the same window.

Cross-window vs Z-009 mimir of record (wave movement only;
glibc gauge = window comparability, printed per discipline):
| Workload | Z-009 mimir | Now (run 2) | Ratio | glibc gauge |
|---|---|---|---|---|
| small_churn | 93.43 | 28.52 | 0.305x WIN | 0.368x |
| mixed_batch | 398.42 | 27.15 | 0.068x WIN | 0.372x |
| large_batch | 3064.69 | 63.48 | 0.021x WIN | 0.508x |
| mixed_lifecycle | 898.76 | 505.65 | 0.563x WIN | 0.525x |

## 3. KL-C restated in numbers - and KL-D's standing

KL-C entered the wave as: large allocations - a mapping per
allocation past the 1024-byte cutoff vs glibc's
threshold-and-cache wisdom.

BEFORE (of record):
- large_batch LOSS 2.4x at Z-009 (mimir 3064.69 vs glibc
  1295.68 ns/op, docs/Z13_WAVE01_BASELINE.md s.2).
- Z-046 priced the posture per decade: the mapping price is
  nearly size-flat (op/mimir 1911 ns/op at 2048 rising to
  4011 at 33554432); one byte past the cutoff multiplies the
  price about 80x (23.98 ns/op at 1024 on the span path vs
  ~1939 at 1025 on the mapping path); the raw mmap/munmap
  pair alone is ~1.0 us at every size, faults and teardown
  are the growth term, bookkeeping residual 401-840 ns.
- glibc's wisdom, measured by Z-046 on this host: a dynamic
  mmap threshold that rises on free (to its cap) plus arena
  retention serves steady-state large sizes at ~8 ns/op;
  forced to map fresh every time (the glmap control) it pays
  the same class of price MimirAlloc paid.

AFTER (this wave):
- The wave did not move the cutoff; it removed the pair.
  Z-048's exact-length reuse pool (bounds 64 mappings /
  32 MiB, munmap only on overflow or trim) serves a cycling
  large allocation from a kept mapping. Same-window A/B at
  Z-048: pool 64.95 vs pre-pool base 2747.67 ns/op = 0.024x
  - the pool removes 97.6% of the mapping-path price on its
  home workload, more than Z-046's perfect-cache ceiling
  estimate for the pair+reserve share alone, because a hit
  also keeps the touched endpoints faulted in.
- Fresh standings (section 2): large_batch WIN ~0.10x vs
  glibc same-window (factor indicative, grade proven), and
  the mimir absolute moved 3064.69 (Z-009) to 63.48 ns/op
  (cross-window, gauge 0.508x - movement, not the grade).
- The supporting hunts landed around it: THP ADOPTED on
  pooled mappings at usable sizes >= 2 MiB (Z-049: fresh
  fault/page falls ~84-90% at 20/200 MiB when huge-backed;
  REJECTED below 2 MiB as a no-op); a pressure-driven trim
  policy proven on its span-retention leg (Z-050: RSS fell
  by exactly the retained bytes under injected HIGH PSI,
  3/3 attempts; its pool leg is owed to Z-060); huge
  allocations at parity when touched (Z-051: TIE
  0.995..1.018 fully touched, 256 MiB..8 GiB); realloc
  growth by mremap (Z-052: ladder 0.266x vs before, 1.004x
  TIE vs glibc, from 3.77x LOSS).

WHAT KL-C STILL IS - the restatement, honestly bounded:
KL-C is CLOSED for repeated large traffic whose lengths the
pool holds: there, a mapping per allocation is gone, and the
standing is a same-window WIN. KL-C is UNCHANGED on a pool
miss: a first, unique, or over-bound large allocation still
pays one mmap, its faults, and one munmap at the Z-046
price, and untouched huge alloc/free pairs still stand at
LOSS 1.10-1.22x (Z-051, magnitudes indicative). And the
closure is bought with bounded retention whose price is
measured, not hidden: under holed free patterns the pool's
kept pages hold resident bytes glibc returns (Z-053:
checker holed net factor 3.804 vs glibc 1.031; churn capped
by the pool's own 32 MiB / 64-VMA bounds, DRAINED retention
28956 KiB / 45 VMAs). The verdict in roadmap words:
large-path repeated traffic - WIN, KL-C discharged for
that traffic; large-path miss traffic - STILL LOST at the
Z-046 price, reason named (no reuse to amortise the pair;
the ~80x cliff at 1025 B stands on every miss); retention
cost - LOSS on holed RSS factors, bounded by design.

KL-D's standing (reported by this verdict; not Wave 6's
hunt): mixed_lifecycle LOSS 71.927x same-window (run 2;
505.65 vs 7.03 ns/op) - STILL LOST, reason named: the
lifecycle workload churns across small classes and larges
through every path at once, and the paths it exercises most
(the small path's per-op latency, KL-A; lifecycle
bookkeeping across classes) are the ones Wave 6 did not
rebuild. Its mimir absolute improved 1.78x against Z-009
(898.76 to 505.65, cross-window) without moving the
standing. Wave 7 (Z-055..) is chartered on its slow half.

## 4. The cutoff of record - 1024 bytes, UNCHANGED, justified

The cutoff of record remains MIMIR_SMALL_CUTOFF = 1024
bytes (src/core/mimir_core.c:1245 in this tree, verified
first-hand by this lead). No Wave 6 slice changed it: Z-047's
design states it outright ("It does not raise the cutoff"),
Z-048 implements reuse above it, and the dispatcher still
routes size <= 1024 to the class spans, above it to the
mapping path. The roadmap asks the cutoff (if changed) to
be justified; it was not changed, and the wave's evidence
justifies leaving it:
1. The wound was never the number. Z-046 showed the price
   of the large path is the uncached mmap/munmap pair
   (~1.0 us of the ~1.9-4.0 us) plus re-faults - costs a
   raised cutoff does not remove for sizes still above any
   cutoff, and costs the pool demonstrably did remove for
   cycling sizes (97.6% of the path price, Z-048 A/B).
2. Raising it was priced, not ignored. Z-046's options
   table (ideal retention-reuse proxy): cutoff 131072 worth
   37.2% uniform / 61.3% large_batch-mix; with bookkeeping
   charged at +500 ns/op, ~29% / ~48%. Against that model
   the built cache delivered large_batch at 63.48 ns/op
   where the status-quo model priced the mix at 2506.61 -
   the cache road beat the priced cutoff road on the
   workload of record, without touching span geometry.
3. A raised cutoff is not a constant change. The span
   machinery serves one page per span, so no cutoff past
   4096 is reachable without multi-page spans - new
   geometry, new fragmentation questions (Z-046, Z-047).
   Wave 6 bought its win with no geometry risk; the priced
   option remains open and unbuilt, and the two roads
   compose rather than substitute (Z-047 s.6, rejected
   alternatives): a future span-geometry slice may
   still raise it on its own evidence.
4. glibc parity of strategy, not of number: glibc answers
   with threshold-and-cache (dynamic threshold + retained
   arena, Z-046). MimirAlloc now answers with
   fixed-cutoff-and-cache (1024 + bounded exact-length
   pool). The strategies match; the constants differ
   because the small path they guard differs.

## 5. Wave 6 change log, Z-046 to Z-053

### Z-046 - The threshold study - FINAL (banked 517bbb1)
Gate MET: per-decade cost table filed; cutoff options
priced. glibc's dynamic threshold rises on free to its cap
and steady-state reuse serves large sizes at ~8 ns/op;
MimirAlloc's fixed cutoff costs the full mapping price from
1025 bytes up (cliff ~80x at the byte, section 3). The
gltrim control leg proved reuse only pays when freed memory
is RETAINED - the finding the pool's bounds are built on.

### Z-047 - Mapping-cache design - FINAL (banked f82bfaa)
Gate MET: docs/Z13_MAPPING_CACHE_DESIGN.md with invariants
MC-01..MC-16. Exact-length segregation, hard bounds (the
retention bound IS the price), resident-by-default pooling;
MADV_DONTNEED-on-free rejected by its own probe (repays
every fault, saves 0.2-1.4%); raising the cutoff deferred
as a composing road (s.6). No code changed.

### Z-048 - Mapping-cache implementation - FINAL (banked 7b8ebd4)
Gate MET: standing gate GREEN 4988/0/77 + standalone churn
battery 57/0. The pool is live: freed large mappings are
kept (state POOLED) up to 64 mappings / 32 MiB, reused
exact-length through the identical publication path,
munmap only on overflow or trim. large_batch A/B of
record: pool 64.95 vs base 2747.67 vs glibc 514.97 ns/op
(section 3). Recorded at build: Z-048 started before Z-047
landed and followed the roadmap + Z-046/Z-015; the
MC-invariant reconciliation is named in section 6, as is
its INC-Z048-01 battery finding.

### Z-049 - THP for cached mappings - FINAL (banked f8ee668)
Gate MET: adopt/reject per decade, recorded with numbers.
ADOPTED for usable sizes >= 2 MiB (2 MiB bimodal on PMD
alignment, named; 20 MiB fresh fault/page 372 -> 36 ns;
200 MiB 488 -> 80 ns, bandwidth TIE there); REJECTED below
2 MiB (no PMD window fits; zero huge backing in every rep).
Discharges Z-018's owed 200 MiB row. Gate GREEN 4988/0/77.

### Z-050 - Trim policy under pressure - FINAL (banked 7780501)
Gate MET on its span-retention leg: policy suite 60/0;
pressure-cycle probe PASS 3/3 - under an injected HIGH PSI
sample RSS fell by exactly the retained bytes (384 KiB =
393216 B) in every attempt; the CALM leg released 0.
Named at build and carried here: the mapping-pool clauses
were CONDITION-PENDING on Z-048's banking, and the
mapping-pool pressure-cycle leg is OWED to Z-060.

### Z-051 - Huge allocations (256 MiB..8 GiB) - FINAL (banked 95d72d1)
Gate MET: huge-size table (success cost + failure
behaviour) recorded. Fully touched: TIE at every size
(0.995..1.018 - the price is the kernel's first touch,
~486-496 ns/page in both legs). Untouched pairs: LOSS
1.10-1.22x on 5-10 us totals (magnitudes indicative).
Failure behaviour clean: arithmetic refusals are NULLs
before any mapping attempt and the allocator is proven
live after each. Posture difference named: calloc is NOT
lazy in this core (256 MiB calloc 46.7 ms, eager zeroing)
where glibc returns untouched zero pages.

### Z-052 - mremap growth - FINAL (banked 2e4e8f1)
Gate MET: growth-ladder table before/after recorded;
contract tests green (test-z052 35/0 + standing gate).
Audit: large realloc NEVER grew in place before. In-place-
only mremap (flags=0) proven NOT IMPLEMENTABLE for fresh
mappings on this host (top-down packing; raw flags=0
mremap of a fresh 34 MiB mapping returns ENOMEM).
Implemented: MREMAP_MAYMOVE growth beyond the cache bound.
Ladder (34 MiB grown to 128 MiB): after 4345139.50 vs
before 16315428.00 ns/op medians = 0.266x WIN; after vs
glibc 4329033.00 = 1.004x TIE (before vs glibc was
3.769x LOSS).

### Z-053 - Large-path fragmentation - FINAL (banked 8df68e8)
Gate MET: fragmentation factors recorded per pattern
(median net_factor = resident bytes held per live byte,
worst live phase): checker mimir 3.804 / glibc 1.031;
uniform 1.559 / 1.043; pinwheel 2.537 / 1.077; churn
1.634 / 1.480 (gross factors in the recorded run). At
FULL phases both allocators sit at 1.006..1.037 -
fragmentation is a property of the FREE pattern. Findings
of record: the holed-phase blow-up is the pool's bounded
retention price (F1); address holes and RSS holes are
different quantities and the allocators trade them (F2);
refill is exact for MimirAlloc (F3); the long run is
capped by the pool bounds, not the workload (F4). The
address-vs-RSS hole distinction (F2) is inherited by
Z-058's split/coalesce audit, per Z-053's own record.
Standing gate GREEN 4988/0/77 on its final tree.

## 6. Remaining gap, itemised

1. KL-D (mixed_lifecycle LOSS 71.9x same-window) - the
   largest standing wound on this line. Not Wave 6's hunt;
   Wave 7 (Z-055..) opens on its slow half, and Z-056/Z-057's
   soak line inherits it. No Wave 6 number may be read as
   progress against its standing (its absolute improved;
   its grade did not).
2. Pool-miss large traffic at the Z-046 price - first,
   unique, and over-bound (single mapping > 32 MiB)
   allocations still pay the full fresh-mapping price, and
   untouched huge pairs stand at LOSS 1.10-1.22x (Z-051).
   No slice is yet chartered against the miss path itself.
3. Z-050's mapping-pool pressure-cycle leg - OWED to
   Z-060 by Z-050's own record (its span-retention leg is
   proven; the pool leg was condition-pending at build and
   the pool it now names has since banked at Z-048).
4. Z-047/Z-048 reconciliation - Z-048 was built before
   Z-047's design landed; its recorded run states the
   invariants it implemented so the reconciliation against
   MC-01..MC-16 is mechanical. It has not been executed as
   a slice; any divergence found is a defect to fix, not a
   disagreement to file.
5. INC-Z048-01 - the Phase-era standalone batteries:
   i003/h001/g003/h003/j006 are RED at the pristine seat
   baseline with failure sets byte-identical with the pool
   in (proven by Z-048 against a baseline clone), and the
   i004/j002/j003 fault shim is inert under -flto (LTO
   defeats the interposition it counts on), with j002/j003
   additionally encoding pre-pool accounting semantics.
   Filed by Z-048 as immediate next work for the seat;
   still open at this verdict. The standing gate (make
   test) is unaffected and GREEN; these are standalone
   targets outside its composition (Z-008's scope note).
6. Z-049's named follow-up - guaranteed 2 MiB THP coverage
   would need the usable extent PMD-aligned by construction
   (reserve + trim the head); whether that is worth its VMA
   cost was left to the implementation line and remains
   unbuilt. The 2 MiB decade's THP win is therefore
   bimodal by placement, exactly as Z-049 recorded.
7. Z-051's calloc posture - eager zeroing of huge callocs
   (46.7 ms at 256 MiB) vs glibc's lazy zero pages: a real
   behavioural difference, recorded, not yet a slice.

## 7. Traceability index

Every number above traces to exactly one landed file:
- Standings of record, both runs, drift analysis:
  tests/perf/recorded_run_z13_z054.txt (this commit; raw
  hidden_files/z054_raw.csv + z054_raw_run2.csv, summaries
  reproduced verbatim in the recorded run).
- Z-009 baseline (all Z-009 medians + checksums):
  docs/Z13_WAVE01_BASELINE.md s.2 (source:
  tests/perf/recorded_run_z13_k002.txt, Z-003).
- Z-046 prices, cliff, threshold behaviour, cutoff options
  table: tests/perf/recorded_run_z13_z046.txt (517bbb1).
- Z-047 design, invariants, DONTNEED rejection:
  docs/Z13_MAPPING_CACHE_DESIGN.md +
  tests/perf/recorded_run_z13_z047.txt (f82bfaa).
- Z-048 pool invariants, bounds, churn 57/0, A/B medians
  (64.95 / 2747.67 / 514.97), INC-Z048-01:
  tests/perf/recorded_run_z13_z048.txt (7b8ebd4).
- Z-049 per-decade adopt/reject + fault numbers:
  tests/perf/recorded_run_z13_z049.txt (f8ee668).
- Z-050 suite 60/0, pressure-cycle 3/3, 384 KiB figure,
  owed pool leg: tests/perf/recorded_run_z13_z050.txt
  (7780501).
- Z-051 TIE/LOSS bands, calloc figure, failure audit:
  tests/perf/recorded_run_z13_z051.txt (95d72d1).
- Z-052 audit, ENOMEM proof, ladder medians + verdicts:
  tests/perf/recorded_run_z13_z052.txt (2e4e8f1).
- Z-053 factors per pattern, F1-F5 findings:
  tests/perf/recorded_run_z13_z053.txt (8df68e8).
- Cutoff + pool bounds in the tree of record:
  src/core/mimir_core.c:1245 (MIMIR_SMALL_CUTOFF 1024),
  :486-487 (MIMIR_POOL_MAX_COUNT 64, MIMIR_POOL_MAX_BYTES
  32 MiB) - read first-hand at this build's base db1cea1.
- Standing gate: hidden_files/gate_z054.log (team-local):
  checks=4988 failed=0 skips=77, GATE RESULT: GREEN.

## 8. Verdict and what remains owed

Wave 6's landed verdict: the large path's defining wound
is closed where reuse can reach it, and the wave proved it
in the same window. A mapping per allocation past the
cutoff stood at LOSS 2.4x when the wave opened; the banked
core now serves cycling large traffic from a bounded
exact-length pool and stands at WIN ~0.10x against glibc
on large_batch (factor indicative on a drift-rejected
glibc leg; grade proven three ways), with THP adopted
where it pays, a pressure-honest trim policy, huge
allocations at touched parity, mremap growth at TIE with
glibc, and the retention price of it all measured to the
KiB and capped by the pool's own bounds. The cutoff of
record never moved: 1024 bytes, justified in section 4 by
the wave's own pricing - the wound was the uncached pair,
not the number. What the wave did not touch stands at full
size in this doc: mixed_lifecycle LOSS 71.9x (KL-D, Wave
7's hunt), pool-miss traffic at the old price, and the
small-path losses the earlier waves already owned.

REMAINS OWED after this verdict (section 6 carries each
with its source): the Z-060 mapping-pool pressure-cycle
leg; the Z-047/Z-048 MC-invariant reconciliation;
INC-Z048-01's standalone-battery repairs; the unbuilt
PMD-aligned 2 MiB construction; a charter, when Volmarr
gives it, for the pool-miss path and the huge-calloc
posture. None of these is a Wave 6 gate item; the wave's
gate - this doc, traceable - is MET.

Team note: this slice was executed by a fresh team-08 lead
after the previous lead's timeout interruption (restart
note in CAMPAIGN_STATE.md; run 1, the runner and the
summariser are its landed work, inherited and verified,
not redone silently - run 2 was taken because run 1's
glibc large_batch leg was drift-rejected and its load
uncaptured, and both runs are filed). The lead spawned
all six Mythic Engineering sub-agents as reviewers
(inspector, measurement reviewer, KL-C and cutoff analyst,
wave-completeness reviewer, failure and honesty analyst,
recorder-verifier); their must-fixes were folded before
commit. All device-side work was executed by the lead
alone, sequentially, in this worktree only. Bugs
discovered in the allocator: none. The one defect
encountered was documentary (run 1's missing load stamp)
and is repaired by run 2's stamped window, recorded in
tests/perf/recorded_run_z13_z054.txt.
