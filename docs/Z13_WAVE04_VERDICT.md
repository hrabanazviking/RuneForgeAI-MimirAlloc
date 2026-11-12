# Z13 Wave 4 Verdict

Slice: Z-036 - Wave 4 verdict
File: docs/Z13_WAVE04_VERDICT.md
Campaign: MimirAlloc Z13, purpleraider (ASUS ROG Flow Z13, Strix Halo)
Roadmap: docs/MIMIRALLOC_Z13_ROADMAP.md, Wave 4 (Z-028 to Z-036)
Seat base at build: main c84f021 (banked Z-001..Z-020), merged into
this branch as c18ef05 before drafting; team-05 HEAD before this
slice: 7dfffa9 (Z-028).
Fold-in (this amendment): seat main 35f7f55 (banked Z-001..Z-035)
merged into this branch as c28b998 before folding. The OWED
sections below are completed from the banked recorded runs in
that tree, each cited to its seat commit; the original FINAL
sections and structure are preserved, per the amendment rule.

## Scope and honesty - read first

This verdict grades only what has LANDED at drafting.

LANDED (banked at the seat, remote-verified): Z-001..Z-035 at
seat main 35f7f55 (fold-in merge c28b998). Every number in the
FINAL sections below is cited to a recorded run or a banked
doc in that tree.

Z-028, cited at drafting as built (team-05 commit 7dfffa9), is
now BANKED at the seat as b221eb5. Z-021..Z-027 (Wave 3) are
likewise banked, in roadmap order: dee30d6, f4730d7, 5e8f92c,
aa47083, 2958f30, 26cb368, 4b82c97.

OWED - NOT GRADED at drafting; FOLDED IN by this amendment:
Z-029..Z-035, each now banked (seat commits in section 1) and
graded below only from its own banked recorded run, read
first-hand from the merged tree by the team-05 lead and
cross-checked by six traceability auditors (one per folded
slice group). One honesty bound survives the fold-in and is
stated wherever it bites: no banked slice re-measured the
ASSEMBLED Wave 4 core (TLS cache in-tree, LTO build) on the
small path. Z-031 measured the banked pre-TLS core at
c84f021, Z-035 measured LTO on the pre-TLS core, and Z-030
priced its queue against the pre-TLS core; the post-TLS
small-path verdict run remains OWED (sections 6 and 9).

Amendment rule: this document is amended, not rewritten, as
each OWED slice lands - its section is folded in at banking,
per the Z-009 fold-in precedent and the honesty notes of
Z-027 (team-04 commit 189c65a, unbanked; used here only as the
discipline model for this skeleton, never as evidence) and
Z-028 section 0. Banking order: Z-036 queues behind
Z-021..Z-035; it does not leapfrog them. This fold-in is that
amendment, filed after Z-021..Z-035 had all banked, and it
completes the verdict as far as landed work carries it
(section 9 names what remains OWED beyond Wave 4).

## Purpose and gate

Roadmap Z-036: file the small-path standings - mimir
(original) vs mimir (wave 4) vs glibc, per workload, with the
remaining gap itemised. Gate: verdict doc traceable to
recorded runs. Wave 4 is the small-path speed hunt (KL-A):
its verdict workloads are small_churn and mixed_batch, the
two small-path workloads of the Z-009 baseline. large_batch
(KL-C, Wave 6) and mixed_lifecycle (KL-D, Wave 7) appear below
only as labelled context; no Wave 4 credit or penalty is
taken from them.

Gate status: FOLDED IN. The traceability half of the gate is
MET for every FINAL section (each number names its recorded
run). The standings half is now filed from Z-031's banked
recorded run (section 6): both verdict workloads carry Law 2
grades with the reason named. The gate closes on the
standings Z-031 actually measured - the pre-TLS banked core
(the scope header's honesty bound); the post-TLS re-run is
named in section 9 as remaining work, not silently assumed.

Laws that govern this verdict (roadmap, Laws of this line):
Law 1 - measurement first, this machine only. No number from
any other machine is evidence here.
Law 2 - honest grading. Verdict words are WIN, TIE, LOSS
(with the size of the loss), or STILL LOST - REASON NAMED.
No document of this line may claim a universal win.
Law 3 - pinned methodology. Cited runs embed the Z-002 host
header, follow the Z-004 protocol, pattern-touch every block,
and prove workload equivalence by identical checksums.
Law 4 - gates, not vibes. Counts are read from log files on
this host, never from memory.

## Host of record

Source: docs/Z13_WAVE01_BASELINE.md (Z-009), host table cited
there to tests/recorded_run_z002.txt (Z-002). Machine: ASUS
ROG Flow Z13 (GZ302EA), AMD Ryzen AI MAX+ 395, 32 threads;
kernel Linux 7.3.0-10-generic x86_64; glibc 2.44; page size
4096 bytes; THP madvise; governor performance; compiler gcc
15.3.0, flags -std=c11 -Wall -Wextra -Werror -pthread -O2.
Each measured run cited below records its own load state;
load regimes are never mixed silently (see section 8).

## 1. Evidence base - landed vs OWED

LANDED and read from disk by the team-05 lead before drafting:

| Input | Seat commit | Evidence used |
|---|---|---|
| Z-009 baseline of record | 2028f3d | docs/Z13_WAVE01_BASELINE.md (latency, counters, RSS, energy) |
| Z-017 alignment audit | 7974cc4 | tests/perf/recorded_run_z13_z017.txt (class splits named for Wave 4) |
| Z-018 design constants | 0b8abc0 | docs/Z13_DESIGN_CONSTANTS.md (losses of record, class table) |
| Z-019 lock histograms | 0b6c20f | tests/perf/recorded_run_z13_z019.txt (hold/wait per entry point) |
| Z-020 contention ladder (mimir) | c84f021 | tests/perf/recorded_run_z13_z020.txt (scaling LOSS 2..32; no glibc verdict claimed there) |
| Z-028 TLS contract | seat b221eb5 (built team-05 7dfffa9) | docs/Z13_TLS_CACHE_CONTRACT.md; tests/contract/recorded_run_z028.txt |
| Z-021..Z-025 Wave 3 ladders/anatomy | dee30d6, f4730d7, 5e8f92c, aa47083, 2958f30 | tests/perf/recorded_run_z13_z021..z025.txt |
| Z-026 redesign dossier | 26cb368 | docs/Z13_REDESIGN_DOSSIER.md (reconciliation, section 4) |
| Z-027 autopsy verdict | 4b82c97 | docs/Z13_WAVE03_AUTOPSY.md |
| Z-029 TLS implementation | 84e860a | tests/contract/recorded_run_z029.txt; docs/Z13_TLS_CACHE_IMPLEMENTATION.md |
| Z-030 remote-free queue | 9eebc2e | tests/perf/recorded_run_z13_z030.txt |
| Z-031 small-path verdict run | 98b2343 | tests/perf/recorded_run_z13_z031.txt |
| Z-032 spacing study | 8fbe7af | tests/perf/recorded_run_z13_z032.txt |
| Z-033 retune harness + BEFORE | 6c156d0 | tests/perf/recorded_run_z13_z033.txt |
| Z-034 metadata diet | 0da54c1 | tests/perf/recorded_run_z13_z034.txt |
| Z-035 build-flag honesty | 35f7f55 | tests/perf/recorded_run_z13_z035.txt |

OWED at filing, FOLDED IN by this amendment - every row below
was discharged by its banked recorded run, read first-hand:

| Slice | Banked seat | What it discharged |
|---|---|---|
| Z-021 glibc contention ladder | dee30d6 | side-by-side glibc ladder for Z-020 |
| Z-022..Z-027 (Wave 3 remainder) | f4730d7, 5e8f92c, aa47083, 2958f30, 26cb368, 4b82c97 | hot-spot, anatomy and dossier inputs (section 4 reconciliation) |
| Z-029 TLS implementation | 84e860a | the cache itself; Z-028 contract flip to green (section 5) |
| Z-030 cross-thread frees | 9eebc2e | remote-free queue priced vs Z-024 (section 5) |
| Z-031 small-path verdict run | 98b2343 | the Wave 4 standings of section 6 (pre-TLS core) |
| Z-032 spacing study | 8fbe7af | class-by-class table; NO-CHANGE verdict (section 5) |
| Z-033 retune (conditional) | 6c156d0 (part 1) | harness + BEFORE legs; closure by construction from Z-032 (section 5) |
| Z-034 metadata diet | 0da54c1 | header cost table; REJECT-as-default per group (section 5) |
| Z-035 build-flag honesty | 35f7f55 | flag matrix; LTO adopted (section 5) |

## 2. Baseline standings - FINAL (pre-Wave-4, of record)

Source: Z-009 section 2, from tests/perf/
recorded_run_z13_k002.txt (Z-003; medians of 7 alternating
processes per leg; harness bench_k002.c unmodified, sha256
30dd0d9a8da12b03aa511fd35a4a098cf593b79c0e9ad352026ecb11eacc7d37).
Load state: CONTENDED, not quiet - load average 5.82/4.42/2.28
at bench build, 5.29 (1-min) at run end; the Tensile lab
shared the host. Legs alternate process-by-process and
medians are the headline. Checksum proof: exactly one
checksum per workload across both legs - small_churn
12756656, mixed_batch 34727552, large_batch 1106008,
mixed_lifecycle 40359936 (fingerprints of workload
equivalence, not speed figures).

Wave 4 verdict workloads (in scope):

| Workload | glibc ns/op | mimir (original) ns/op | Excess ns/op | Verdict |
|---|---|---|---|---|
| small_churn | 9.02 | 93.43 | 84.41 | LOSS 10.4x |
| mixed_batch | 9.90 | 398.42 | 388.52 | LOSS 40.2x |

Context only (owned by later waves; no Wave 4 standing):

| Workload | glibc ns/op | mimir (original) ns/op | Verdict | Owner |
|---|---|---|---|---|---|
| large_batch | 1295.68 | 3064.69 | LOSS 2.4x | Wave 6 (KL-C) |
| mixed_lifecycle | 13.38 | 898.76 | LOSS 67.2x | Wave 7 (KL-D) |

These are the standings Wave 4 must move. They are baseline
figures, not Wave 4 results, and are never presented as such
anywhere in this document.

## 3. Why the gap exists - landed attribution - FINAL

Attribution only; nothing here re-grades section 2.

Lock level (Z-019, tests/perf/recorded_run_z13_z019.txt;
SHARED-HOST run, load 2.50 -> 2.68; instrumented debug build;
2.9944 cycles/ns): single-thread WAIT sits at the measurement
floor on every entry point (mean about 40 cycles, about
13 ns; p50 exactly 30 cycles, one TSC tick; p99 60) - nobody
waits for a free lock, so the lock mechanism is not the
wound; the work done UNDER the lock is. HOLD is the path
fingerprint: small_churn malloc 60 cycles (20 ns) / free 86
(29 ns); mixed_batch malloc 143 (48 ns, bimodal - span-refill
tail to p99 about 2926 cycles) / free 601 (201 ns). Free is
the more expensive small entry on both workloads. Under
contention, mean WAIT reaches 1597/1688 cycles at 4 threads
(about 12x hold, p99 about 5 us) and 4190/4292 at 8 threads
(about 1.4 us, about 26x hold, p99 about 10 us): every cycle
of hold removed is paid back once per queued thread. That is
the whole reason the Wave 4 cache exists. The banked Z-020
ladder adds the scaling shape: multithreaded throughput is a
LOSS at every thread count 2..32 against mimir's own 1-thread
peak, inversion at 2 threads (9 of 24 points re-taken;
mixed_batch 20/32 remain INDICATIVE after two re-takes). No
glibc scaling verdict is claimed - that comparison is
Z-021's. (Fold-in: Z-021 has since banked at seat dee30d6
and is listed in section 1; this section's attribution is
unchanged and takes no number from it.)

Counter level (Z-005, tests/perf/recorded_run_z13_z005.txt;
QUIET window, load 0.61 -> 0.85 - a different load regime
from Z-003, cited for ratios, never mixed with section 2
medians): small_churn cycles/op 175.65 vs glibc 17.83 (9.9x),
instructions/op 861.97 vs 82.99 (10.4x), cache-misses/op
near parity (0.01688 vs 0.01362) - the small path's loss is
instruction count, not cache behaviour; KL-A is a code-path-
length wound. mixed_batch cycles/op 1173.78 vs 18.31 (64.1x),
instructions/op 3125.60 vs 100.61 (31.1x), cache-misses/op
0.45442 vs 0.01806 (about 25x). Across all four baseline
workloads mimir executes 3.6x..46x more instructions per op
than glibc (Z-005 reading of record, via Z-009 section 4).

Geometry (Z-017, tests/perf/recorded_run_z13_z017.txt; no
timing measured, load cannot shift address distributions):
both allocators returned sub-16-aligned pointers 0 times in
163,840 blocks per leg. MimirAlloc class 48 splits cachelines
on 49.41 percent of blocks (4048/8192) and class 64 on 100
percent (8192/8192, every pointer at ptr mod 64 = 32); both
classes are named for Wave 4 by Z-017 itself. Judgement of
the full 20-class table is Z-032's (fold-in: banked at
8fbe7af, NO-CHANGE, folded in section 5).

## 4. The one Wave 4 slice with landed evidence: Z-028 - FINAL

Z-028 (team-05, commit 7dfffa9; BANKED at the seat as
b221eb5 - fold-in) filed the TLS cache contract: docs/Z13_TLS_CACHE_CONTRACT.md,
surface tests/contract/mimir_tls_cache_z028.h (test-only),
suite tests/contract/test_z028_tls_contract.c (119 checks,
phases P0..P10), runner tests/contract/run_z028.sh
(standalone; NOT wired into make test).

What is FINAL, each item cited to tests/contract/
recorded_run_z028.txt:
- The contract exists and fixes its constants as decisions:
  CAPACITY 16 cached blocks per class per thread, BATCH 8;
  clauses TLS-* traced to tests in contract section 4.
- The suite is RED on the old core, by design and
deterministically: 119 run, 96 failed, RESULT: FAIL, exit 1,
identical across 3 runs. The 23 passes are P9 public-API
regression only and are labelled as such in the contract;
they are not partial TLS success.
- Non-implementation is proven: nm shows the six surface
symbols absent from the core objects and weak in the suite
binary. The cache does not exist yet. No sentence in this
document may be read as claiming it does.
- The contract promises NO speed number. Speed is graded by
Z-031's verdict run against section 2 and glibc.
- Standing gate at the Z-028 build: scripts/z13/run_gate.sh
GREEN 4988 / 0 / 77 (team log hidden_files/z028_gate.log) -
the standing gate does not cover the contract suite.
- Honesty note of record (contract section 0): no Z-026
dossier was banked when the contract was written; it was
written against the roadmap's own Wave 4/5 road split, with
an amendment rule if the banked dossier contradicts it.
- Reconciliation - RESOLVED (fold-in, from the banked
docs): Z-026 has banked (seat 26cb368,
docs/Z13_REDESIGN_DOSSIER.md). Its section 4.1 ADOPTS the
TLS freelist cache as Wave 4's road, and its section 5
decision reads, in substance, exactly the contract's road:
Z-028 writes the contract against this decision (ownership,
cross-thread free routing, thread-exit drain), Z-029
implements per-thread bins lock-free on a hit with batched
refill/drain, and Z-030 routes cross-thread frees through
a bounded remote-free queue. The decision ledger records
it as D-007, Adopted. On constants: the dossier pins no
CAPACITY/BATCH number - it requires the contract to pin
them (dossier section 4.1, C6) - and the contract pins
CAPACITY 16 / BATCH 8 (contract section 3). No clause of
the contract is contradicted, so the contract section 0
amendment rule is not triggered. One residual, named not
hidden: the dossier's C7 expectation - that the cache's
refill carve, with Z-032 and Z-033, would give the 64 B
class cacheline-aligned starts and answer the 48 B split -
was NOT discharged by Wave 4. Z-032 proved the splits are
carve-offset geometry no spacing change can repair
(section 5), and the cache hands out span blocks
unchanged; the fixing lever (header/carve offset) is
span-layout work that remains OWED beyond Wave 4
(sections 7 and 9).

Z-029's flip test, fixed by the contract: run_z028.sh
unmodified returning 119 / 0 PASS, plus the standing gate
green. FOLDED IN: that run is recorded and green - see
section 5, Z-029 row (seat 84e860a; tests/contract/
recorded_run_z029.txt: 119 run, 0 failed, RESULT: PASS).
The contract of this section is therefore discharged; the
cache it defined exists in-tree.

## 5. Wave 4 change log, Z-029 to Z-035 - FOLDED IN

Each row was OWED at filing and is folded in at the
slice's banking, from its banked recorded run read
first-hand from the merged tree (seat main 35f7f55,
fold-in merge c28b998). Grades are the slices' own, in
the roadmap's words (Law 2); no row is re-graded here.

- Z-029 TLS cache: implementation - FOLDED IN (seat
  84e860a; tests/contract/recorded_run_z029.txt;
  docs/Z13_TLS_CACHE_IMPLEMENTATION.md). The contract
  of section 4 is flipped GREEN, unmodified: CHECKS:
  119 run, 0 failed, RESULT: PASS, exit 0, identical
  across 3 runs (a 4th run taken for the record, same
  result); it was RED BY DESIGN at 119 run, 96 failed
  on the pre-Z-029 core. Standing gate GREEN
  4988 / 0 / 77, identical to baseline. The cache as
  built: static _Thread_local bins, 20 classes x
  CAPACITY 16 blocks per class per thread, refill and
  drain in batches of exactly 8 under one central lock,
  lock-free on a hit; nm proof in the recorded run: the
  six TLS surface symbols defined in the core object
  and exactly 4 dynamic symbols in the shared library.
  Load: quiet window (loadavg 0.19 0.22 0.58 at gate
  start, 0.26 0.23 0.59 at contract end); both suites
  are deterministic correctness, no timing asserted.
  Tension recorded in the open (implementation doc
  section 4): contract doc S2 says an exited owner's
  free does NOT count remote; the suite's P7 requires
  remote +1 for exactly that case; the implementation
  follows the SUITE (the gate-named form of the
  contract). The cache is bypassed under MIMIR_DEBUG
  by design, and the debug flavour's two exits
  (test_b007, test_c006) are pre-existing on the banked
  core - a control run on the unmodified core shows the
  identical pattern; not a Z-029 regression.
- Z-030 Cross-thread frees into TLS-owned blocks -
  FOLDED IN (seat 9eebc2e; tests/perf/
  recorded_run_z13_z030.txt). The per-owner remote-free
  queue (bounded MPSC ring, capacity 256 pointers per
  owner, drain batch 8) is proven as a standalone,
  test-only surface: concurrency suite CHECKS: 86 run,
  0 failed, RESULT: PASS, exit 0, with a red-path proof
  (a lossy mutant fails at 86 run, 17 failed); standing
  gate GREEN 4988 / 0 / 77. Z-024's remote-free cost is
  re-measured (Z-004 protocol; SHARED HOST, LIGHT
  window - loadavg 0.33/0.35/0.88 at the first
  measurement invocation; the two full measurement
  invocations agree within 4.4% on every phase median;
  these are not quiet-machine baselines): freer-side
  cost WIN for the queue (enqueue 408.2 vs direct
  444.6 ns/free same-CCD; 444.4 vs 481.9 cross-CCD;
  about 20% less under swap contention, 1022.5 vs
  1280.4 and 1285.4 vs 1606.3); total system cost LOSS
  in the pair geometries (+35.9% same-CCD, 604.1 vs
  444.6; +60.9% cross-CCD, 775.4 vs 481.9); total
  system cost TIE under swap x4 (1274.4 vs 1280.4
  same-CCD, -0.5%; 1636.2 vs 1606.3 cross-CCD, +1.9%).
  Reason named by the slice itself: the owner still
  pays the same registry walks under the same central
  lock when it drains. Dependency honesty of record:
  this slice changed NO production code and mimir_free
  is NOT wired to the queue; the queue earns its place
  only as part of the assembled Z-029 design, and the
  integration (detecting a non-owner free inside
  mimir_free and routing it to the owner's queue) is
  recorded there as the immediate next work. No banked
  Wave 4 slice claims it done.
- Z-031 Small-path verdict run - FOLDED IN (seat
  98b2343; tests/perf/recorded_run_z13_z031.txt). The
  verdict run of section 6: small_churn STILL LOST
  10.30x and mixed_batch STILL LOST 60.20x, reasons
  named, on the pre-TLS banked core - full standings,
  method and load state in section 6.
- Z-032 Size-class spacing study - FOLDED IN (seat
  8fbe7af; tests/perf/recorded_run_z13_z032.txt).
  DECISION: NO-CHANGE - the current 20-class table
  (16..1024) remains in force and Z-033's conditional
  trigger is NOT met. Every candidate was priced under
  identical span geometry and models: the best finer
  candidate, C2 step-16 (64 classes), gains 8.74% on
  the uniform model's span-effective mapped figure
  (638.79 -> 582.99 B/request), failing the study's
  pre-set 10% adoption bar and tripling the class
  count; C3 and C4 gain only 4.29% / 4.47%; C1
  powers-of-two regresses +36.85% and is rejected
  outright. Fit to the landed workload cycles is 0 B
  waste over 28 cycled sizes, labelled circular by the
  slice itself (the cycles were drawn from this
  table); no real-world request trace exists on this
  host. The Z-017 splits are reproduced exactly (class
  48 at 49.41%, class 64 at 100.00%, every
  multiple-of-64 class at ptr mod 64 = 32) and proven
  to be carve-offset geometry - carve starts at the
  96 B span header, so no class table changes the
  residue; spacing cannot repair either class, and the
  lever is header/carve-offset span-layout work.
  No timing measured; probe executions in a quiet
  window (load 0.10 at start, 0.25 at end); standing
  gate GREEN 4988 / 0 / 77.
- Z-033 Size-class retune (conditional) - FOLDED IN
  as part 1 plus closure by construction (seat
  6c156d0, part 1: harness + BEFORE legs;
  tests/perf/recorded_run_z13_z033.txt). Part 1
  recorded, on the current table: TABLECHECK OK in
  7/7 processes over the full 1..1024 sweep;
  fragmentation (uniform model, computed) mean waste
  42.50 B/request, worst waste 127 B in each 128-step
  class; latency LOSS at every boundary size on both
  legs (CHURN mimir medians 97..1934 vs glibc 2..4
  ns/op; LIVESET LOSS at every size). Standing gate
  GREEN 4988 / 0 / 77; load quiet, 0.30 at runner start
  and end. Closure: Z-033's own recorded terms state
  that a Z-032 no-change verdict satisfies the
  condition by itself - the adopted table IS the
  current table and AFTER legs are identical to the
  BEFORE legs by construction - and banked Z-032
  (8fbe7af) is exactly that verdict; Z-032 likewise
  records that Z-033 is closable as a recorded no-op
  citing its run. The standing folded in here is
  therefore: no retune adopted, AFTER identical to
  BEFORE by construction, citing banked Z-032 plus
  part 1. The formal second Z-033 closure commit is
  being written in parallel by team-02; no retune
  delta of any kind is claimed in this document.
- Z-034 Metadata diet - FOLDED IN (seat 0da54c1;
  tests/perf/recorded_run_z13_z034.txt). Header cost
  table: the production small path's per-block header
  is 0 bytes for EVERY class - there is no per-block
  header to diet; the span header is 96 B per 4096 B
  page, amortised 0.384 B/block at class 16 (cap 250)
  rising to 32.000 B/block at class 1024 (cap 3); span
  efficiency 97.66% at 16/32/80/160 B, worst 75.00% at
  1024 B (928 B tail waste - a capacity effect, not a
  header effect). The out-of-line bitmap trial
  (-DMIMIR_OOL_META=1, one side page per trial-class
  span, 32 B of bitmap used) is graded per class group
  against a pre-registered bar (adopt only on a >=5%
  win with no measured regression): group 16/32 B
  REJECT as the default (churn1 +8.78% / +9.94% slower
  breaches the bar, despite churn256 -65.88% /
  -49.17%); group 48/64 B REJECT as the default
  (churn1 +8.78% / +9.61%; churn256 -27.87% / -4.73%);
  group 80..1024 B NOT TRIALLED (the roadmap's own
  scope) - verdict NO CHANGE, in-band retained. The
  flag stays OFF by default; the trial code is banked
  behind it at proven zero production cost (production
  object 21192 B with 0 trial symbols; trial object
  23008 B); the flag-on suite is identical at
  4988 / 0 / 77 and the standing gate on the final
  tree is GREEN 4988 / 0 / 77. Load: QUIET machine,
  loadavg 0.21-0.31 (1-min) across the windows. The
  slice's real finding, named as follow-up and NOT
  built there: the churn256 trial win is not a
  metadata-space effect - production
  find_span_live_locked proves liveness by walking the
  span's free list, O(carve_count) per free (up to 250
  nodes at 16 B), where the trial's bitmap answers in
  O(1); a per-span liveness bitmap as an index only is
  the promising hybrid for a future free-path slice.
- Z-035 Build-flag honesty round - FOLDED IN (seat
  35f7f55; tests/perf/recorded_run_z13_z035.txt). Flag
  matrix on the pre-TLS core (six variants, bench TU
  -O2 in every leg, 3 warm-ups, 9 reps, taskset -c 2;
  attempt 1 of record: quiet window, loadavg 0.22 ->
  0.56, drifts <= 1.60%, exactly one checksum per
  workload): -O3 TIE on all four workloads (deltas
  +0.01..+1.05%) - DO NOT ADOPT; -march=native TIE on
  all four (-2.22..+0.49%) - DO NOT ADOPT; the
  -O3 + -march combo TIE on all four. LTO (-flto):
  WIN small_churn 34.18 -> 27.87 ns/op (+18.46%), WIN
  mixed_batch 223.22 -> 148.03 ns/op (+33.68%), WIN
  mixed_lifecycle +11.22%, TIE large_batch -0.64%;
  the full combo is statistically the LTO leg again
  (+18.72% / +34.38%). Attempt 2 confirms the LTO WINs
  (+18.07% / +33.72% / +12.63%). ADOPTED: -flto only -
  shipped CFLAGS gain exactly that flag and the four
  .so link lines gain -flto; no source or ABI change.
  Final-tree standing gate GREEN 4988 / 0 / 77, and
  the shipped build reproduces the measured LTO
  medians (small_churn 28.08, mixed_batch 147.72
  ns/op). Provisional scope, stated by the slice
  itself: measured BEFORE the Wave 4 TLS-cache slices
  integrated, so the deltas are against the pre-TLS
  -O2 core of that tree; Z-096 re-trials the matrix on
  the near-final core; the deltas say nothing about
  glibc standings.

## 6. Wave 4 standings - FOLDED IN from Z-031

The three-way standings this slice exists to file,
filed from Z-031's banked recorded run (seat 98b2343;
tests/perf/recorded_run_z13_z031.txt) - the only
verdict run Wave 4 landed. Read the honesty bound
stated in this section before citing any column: Z-031
measured the banked production core at c84f021, so its
mimir column is the core BEFORE the Z-029 TLS cache
landed and before the Z-035 LTO adoption - it is the
pre-TLS verdict of record, in Z-031's own words, that
the Wave 4 changes must beat. No TLS, queue, retune,
metadata or LTO effect is included in it, because none
was in its tree.

Within-session standings (Z-031 main run; QUIET WINDOW,
loadavg 0.34/1.17/1.67 at main-run start and end;
taskset -c 2; 3 warm-up processes per leg discarded;
9 measured reps per leg, interleaved; identical-leg
controls agree within 3.95% worst case; main-run drift
<= 1.36% on every series; checksums exactly one per
workload, identical to the Z-003/Z-009 checksums of
record - small_churn 12756656, mixed_batch 34727552):

| Workload | glibc ns/op (Z-031) | mimir (pre-TLS core) ns/op (Z-031) | Verdict |
|---|---|---|---|
| small_churn | 3.36 | 34.62 | STILL LOST 10.30x |
| mixed_batch | 3.72 | 223.95 | STILL LOST 60.20x |

Three-way, with the baseline of record (section 2,
Z-003 session) beside Z-031's session. Cross-session
factors are INDICATIVE ONLY, per Z-031's own ruling
(Z-003 was unpinned, with no process warm-ups and wide
per-process spread, and session effects moved the two
legs asymmetrically - both legs ran faster in absolute
terms in Z-031's session: glibc -62.7% / -62.4%,
mimir -62.9% / -43.8%):

| Workload | glibc (Z-003) | mimir (original) (Z-003) | glibc (Z-031) | mimir (pre-TLS) (Z-031) | Factor |
|---|---|---|---|---|---|
| small_churn | 9.02 | 93.43, LOSS 10.4x | 3.36 | 34.62 | 10.4x -> 10.30x |
| mixed_batch | 9.90 | 398.42, LOSS 40.2x | 3.72 | 223.95 | 40.2x -> 60.20x |

Reasons named (Z-031's own verdict lines): the
measured core has no TLS freelist cache, so every
small op still pays the single global lock and the
full central-core path - the KL-A wound of record
(Z-005: 861.97 vs 82.99 instructions/op; Z-022's
hot-spot list names the same path). mixed_batch
additionally pays lock + central-core cost on every op
with no batched refill/drain (the Z-029 design point)
and no remote-free queue (Z-030, also not landed in
that tree). No production small-path code banked
between the Z-009 baseline and Z-031's run, so the
widened mixed_batch factor cannot be a code
regression; Z-031 attributes it to session/machine-
state effect and takes the within-session verdict as
the number of record.

What this discharges and what it does not: the OWED
standings table is discharged - the standings are
filed, per workload, with Law 2 grades and the reason
named. But the mimir (wave 4) column in its ASSEMBLED
sense - TLS cache live in-tree, LTO build shipped -
has never been measured by any banked run: Z-029
proved the cache correct (119 run, 0 failed) without
asserting any timing, and Z-035 proved LTO worth
+18.46% / +33.68% on the pre-TLS core. The post-TLS
verdict run therefore remains OWED (section 9): a
re-run of Z-031's harness on the assembled core (the
harness is repeatable as-is, tests/perf/run_z031.sh).
Any number entered for the assembled core before that
run lands would be fabrication, and none is entered.

## 7. Remaining gap, itemised

FINAL now (the pre-Wave-4 gap, from landed evidence only):
the gap to close is small_churn 84.41 ns/op of excess
(LOSS 10.4x) and mixed_batch 388.52 ns/op of excess (LOSS
40.2x), attributed by section 3 to (i) code-path length
under one global lock - instruction excess 10.4x on
small_churn and 31.1x on mixed_batch (Z-005), (ii) hold
time on the small malloc and free entries, free the dearer
(Z-019), and (iii) on mixed_batch, span-refill frequency
and cache/branch wounds (Z-005, Z-019). Two fixable geometry
defects ride along: class 48 at 49.41 percent splits and
class 64 at 100 percent splits (Z-017).

FOLDED IN (the post-Wave-4 remainder, from the banked
runs of section 5): of the pre-Wave-4 gap above, Wave 4's
landed work closes the following, item by item:
- TLS cache: EXISTS and is contract-green (Z-029,
  119 run, 0 failed). Its speed contribution is NOT
  measured: Z-031's verdict run predates it (pre-TLS
  core), so neither workload's excess can be credited
  down by any landed number. Items (i) code-path length
  and (ii) lock hold of the FINAL paragraph above remain
  open as MEASURED gaps, with the fix now in-tree
  awaiting its verdict run.
- Cross-thread frees: the queue is proven and priced
  (Z-030: freer-side WIN, pair total LOSS, swap TIE, the
  loss attributed to the central registry walk at
  drain). The queue is NOT wired into mimir_free by any
  banked slice, so no workload's standing moves.
- Spacing: NO-CHANGE (Z-032), by measured decision - no
  candidate cleared the 10% adoption bar under identical
  geometry. The two geometry defects of section 3 remain
  OPEN: class 48 at 49.41% splits and class 64 at 100%
  splits are carve-offset geometry (the 96 B span
  header); the fixing lever (header/carve offset) is
  span-layout work no Wave 4 slice took - Z-034 audited
  the header and changed no layout. With it, the Z-026
  dossier's C7 alignment expectation remains
  undischarged by Wave 4 (section 4 reconciliation).
- Retune: none adopted (Z-033 part 1 banked 6c156d0;
  closure by construction from Z-032 - AFTER identical
  to BEFORE). No fragmentation or latency delta exists
  to credit against the gap.
- Metadata: REJECT-as-default (Z-034). The production
  per-block header was already 0 B, so there was no
  header excess to close. The slice's real finding names
  a NEW itemised component of the batch excess for a
  future free-path slice: the free path's O(carve_count)
  free-list liveness walk in find_span_live_locked (up
  to 250 nodes at 16 B; production churn256 232.94 ns/op
  at 16 B in Z-034's run, where the bitmap trial leg
  answered in O(1)) - not built in Wave 4.
- Build flags: LTO adopted (Z-035), worth +18.46%
  (small_churn) and +33.68% (mixed_batch) against the
  pre-TLS -O2 core in its own session - the one landed,
  measured reduction of small-path cost in Wave 4. It is
  a build-level reduction of the same path, not a glibc
  standing; Z-035 itself states its deltas say nothing
  about glibc standings. The shipped build is now the
  LTO build.
- Net remainder, stated plainly: as last MEASURED
  (Z-031, pre-TLS core, quiet window) the gap stands at
  small_churn 34.62 vs glibc 3.36 ns/op (STILL LOST
  10.30x) and mixed_batch 223.95 vs 3.72 ns/op (STILL
  LOST 60.20x), reasons named in section 6. Wave 4
  landed the cache, the queue, the adopted LTO build and
  the honest no-change / reject verdicts against that
  gap, but no banked run has yet priced the assembled
  result. The remaining gap is therefore exactly: the
  unmeasured assembled core, plus the open carve-offset
  geometry item, plus the free-list liveness walk Z-034
  named.

## 8. Traceability index

| Section | Numbers from | Recorded run / doc in-tree | Load state of the run |
|---|---|---|---|
| Host | Z-002 via Z-009 | tests/recorded_run_z002.txt; docs/Z13_WAVE01_BASELINE.md | loadavg 4.78 4.29 2.29 at capture |
| 2 Baseline latency | Z-003 via Z-009 S2 | tests/perf/recorded_run_z13_k002.txt | contended: 5.82/4.42/2.28 at build, 5.29 at end; Tensile shared host |
| 3 Lock attribution | Z-019 | tests/perf/recorded_run_z13_z019.txt | shared host: 2.50 -> 2.68 (1-min) |
| 3 Scaling shape | Z-020 | tests/perf/recorded_run_z13_z020.txt | as recorded in that run; 9 of 24 points re-taken |
| 3 Counters | Z-005 via Z-009 S4 | tests/perf/recorded_run_z13_z005.txt | quiet window: 0.61/2.25/2.43 first check, 0.85 at end |
| 3 Geometry | Z-017 | tests/perf/recorded_run_z13_z017.txt | no timing measured; address distributions only |
| 4 Contract + RED | Z-028 (banked b221eb5) | tests/contract/recorded_run_z028.txt; docs/Z13_TLS_CACHE_CONTRACT.md | shared host, loadavg 8.34 at run header; counts are load-independent (no timing assertions) |
| 5 Z-029 flip | Z-029 | tests/contract/recorded_run_z029.txt; docs/Z13_TLS_CACHE_IMPLEMENTATION.md | quiet window: 0.19/0.22/0.58 at gate start; counts load-independent |
| 5 Z-030 queue | Z-030 | tests/perf/recorded_run_z13_z030.txt | shared host, light window: 0.33/0.35/0.88 at first measurement; invocations agree within 4.4% |
| 5, 6, 7 Z-031 standings | Z-031 | tests/perf/recorded_run_z13_z031.txt | quiet window: 0.34/1.17/1.67; controls agree within 3.95% |
| 5 Z-032 spacing | Z-032 | tests/perf/recorded_run_z13_z032.txt | quiet window 0.10 -> 0.25 for the probe; no timing measured |
| 5 Z-033 BEFORE | Z-033 part 1 | tests/perf/recorded_run_z13_z033.txt | quiet window: 0.30 at start and at end |
| 5 Z-034 metadata | Z-034 | tests/perf/recorded_run_z13_z034.txt | quiet machine: loadavg 0.21-0.31 (1-min) |
| 5 Z-035 flags | Z-035 | tests/perf/recorded_run_z13_z035.txt | quiet window: 0.22 -> 0.56 (attempt 1 of record) |
| 4 Reconciliation | Z-026 vs Z-028 | docs/Z13_REDESIGN_DOSSIER.md S4.1/S5; docs/DECISION_LEDGER.md D-007; docs/Z13_TLS_CACHE_CONTRACT.md S0 | no timing; document comparison |
| Gate for this slice | Z-008 standing gate | team logs hidden_files/z036_gate.log (skeleton) and hidden_files/z036_foldin_gate.log (fold-in), team-local, not banked | loadavg 2.91 2.71 2.20 at skeleton gate start; fold-in gate on the merged tree, counts identical |

Standing gate for this slice: at the skeleton filing
(team-05 re-verification on the merged branch, head
c18ef05): scripts/z13/run_gate.sh GREEN - 4988 checks,
0 failed, 77 by-design skips, exit 0, counts read from
hidden_files/z036_gate.log. At this fold-in (merged
branch, head c28b998): scripts/z13/run_gate.sh GREEN -
4988 checks, 0 failed, 77 by-design skips, exit 0,
counts read from hidden_files/z036_foldin_gate.log.
Identical to the baseline at both filings, as a verdict
slice that touches no code must be.

## 9. Verdict - FOLDED IN, complete as far as Wave 4 landed

Wave 4 verdict, completed by this fold-in from the
banked recorded runs: the small path, as last measured
under the full oracle protocol (Z-031, pre-TLS core,
quiet window), stands at STILL LOST - REASON NAMED on
both verdict workloads: small_churn 10.30x (mimir
34.62 vs glibc 3.36 ns/op) and mixed_batch 60.20x
(mimir 223.95 vs glibc 3.72 ns/op), the reason being
the missing TLS cache in the measured core - every
small op paying the single global lock and the full
central-core path. Against the baseline of record the
factors moved 10.4x -> 10.30x and 40.2x -> 60.20x,
cross-session and INDICATIVE ONLY (section 6).

What Wave 4 landed against that standing, all banked
and cited in section 5: the TLS cache exists and is
contract-green (Z-029, 119 run, 0 failed, RESULT:
PASS; standing gate 4988 / 0 / 77); the remote-free
queue is proven and honestly priced (Z-030: freer-side
WIN, pair total LOSS, swap TIE, and NOT wired into
mimir_free); spacing is a measured NO-CHANGE (Z-032),
with the retune closed by construction (Z-033 part 1
banked 6c156d0; formal closure commit in parallel,
team-02); the metadata diet is a measured
REJECT-as-default (Z-034) over a production per-block
header that was already 0 B; and the build is now the
LTO build (Z-035: +18.46% / +33.68% on the pre-TLS
core, -O3 and -march=native TIE and rejected). The
Z-026 <-> Z-028 reconciliation is RESOLVED (section
4): the banked dossier adopts exactly the road the
contract was written against, and the decision ledger
records it as D-007, Adopted.

What remains OWED after Wave 4, named plainly - this
verdict is complete as a verdict on landed work, and
these items stand outside it, not hidden inside it:
1. The post-TLS small-path verdict run: no banked run
   has measured the assembled Wave 4 core (TLS cache
   in-tree, LTO build shipped) against glibc. Until it
   lands - a re-run of Z-031's harness on the assembled
   core, or a later wave's verdict slice - the assembled
   mimir (wave 4) standing is UNMEASURED, and no grade
   beyond Z-031's pre-TLS STILL LOST may be quoted
   for it.
2. The Z-030 queue integration into mimir_free
   (recorded by Z-030 itself as the immediate next work
   for a Z-029 integration slice).
3. The carve-offset geometry item: the class 48 / 64
   cacheline splits (Z-017, Z-032), whose lever is
   span-layout work, and with it the Z-026 dossier's C7
   alignment expectation.
4. The free-list liveness walk named by Z-034 (a
   per-span liveness bitmap as an index only) - future
   free-path work.
5. Z-035's own provisional scope: the flag matrix is
   re-trialled at Z-096 on the near-final core.

The honest one-line standing of Wave 4 is therefore:
cache built and contract-green, LTO adopted, the
honest no-change / reject verdicts banked - and the
small path STILL LOST on the last measured numbers,
with the assembled core's verdict run still owed.
