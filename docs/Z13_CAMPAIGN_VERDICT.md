# Z13 Campaign Verdict

Slice: Z-108 - Campaign verdict (roadmap docs/MIMIRALLOC_Z13_ROADMAP.md).
Built 2026-11-13 by team-05. The six Mythic Engineering roles were run
SEQUENTIALLY by the lead itself (evidence audit, per-workload standings
review, loss-class review, non-latency standings review, pending/filings
review, gate/commit-scope review) - no sub-agents were spawned this
run, and that is recorded plainly here and in CAMPAIGN_STATE.md.
Base: merge e9d7247 of seat main 719b00d (banked Z-001..Z-105) into
yrsa/mimiralloc-team-05. The Makefile merge conflict was resolved by
taking the seat version whole (union discipline: the seat Makefile is
a superset carrying the Z-102/Z-104 blocks this branch lacked); the
merged tree was verified byte-identical to the seat before any Z-108
edit (git diff 719b00d HEAD empty). Baseline standing gate on the
merged tree before any Z-108 edit: GREEN 4988 checks / 0 failed / 77
by-design skips, exit 0 (hidden_files/z108_baseline_gate.log).
This slice derives no new measurement. Its only fresh executions are
the baseline and final standing gates.
Fold-in base: merge 1c6c51c of seat main 85b856f (banked
Z-001..Z-107) into the same branch, clean, no conflicts; the
merged tree was verified identical to the seat except this
document before any fold-in edit (git diff 85b856f HEAD lists
only docs/Z13_CAMPAIGN_VERDICT.md). CAMPAIGN_STATE.md was
preserved uncommitted and untouched by the merge. Baseline
standing gate on the fold-in merged tree: GREEN 4988 checks /
0 failed / 77 by-design skips, exit 0
(hidden_files/z108_foldin_baseline_gate.log); final gate on the
fold-in tree likewise GREEN (hidden_files/z108_foldin_gate.log).
The fold-in also derives no new measurement; its only fresh
executions are its baseline and final standing gates. The six
Mythic Engineering roles for the fold-in were likewise run
SEQUENTIALLY by the lead itself (merge and verification,
evidence re-read, standings re-grade, filings traceability,
gate, commit-scope) - no sub-agents were spawned.

STATUS: FINAL for every banked slice through Z-107, Z-106 and
Z-107 included, folded in under section 7.3. Sections resting
on Z-001..Z-105 are FINAL below, each traced to its banked
recorded run or banked wave verdict, re-read first-hand from
disk on the merged seat tree by this lead. Z-106
(documentation truth pass) banked at seat 9a199ba and Z-107
(the closing head-to-head) at seat 85b856f; their sections
(7.1 and 7.2) now read FINAL with their banked numbers and
grades, section 1 carries the Z-107 closing table, and no
pending marker for either slice remains anywhere in this
document. This slice banks last in Wave 12, after Z-106 and
Z-107; this fold-in commit completes the wave (9 of 9).

The headline the FINAL evidence supports: on this machine
(purpleraider, ASUS ROG Flow Z13, Strix Halo, x86-64, 4 KiB pages)
MimirAlloc ends the Z13 line still LOSING to glibc on raw speed in
almost every measured shape - and the campaign can now say exactly
where, by how much, and why, in numbers it measured itself. One
latency WIN is real and banked: repeated large allocations served
from the mapping pool beat default glibc roughly tenfold - and even
that WIN does not survive glibc with its cache raised (the fair fight, Z-098, confirmed at the close by Z-107). Against the best tuned glibc per workload the final
oracle standings are LOSS on all four workloads. What the campaign
won instead is everything around the speed: a public surface that
grew from 4 to 13 honest exports with contracts, budgets, statistics
and an ABI audit; corruption detection measured at scale with a 0.0%
false-positive rate; an eight-hour soak with no drift and no leak;
chaos runs whose every failure is a clean NULL or a named kernel
behaviour; a reproducible build proven byte-for-byte on this host;
and a preload variant that is correct everywhere it was tried and
honestly priced as slow. This document grades the campaign, not
the hope (roadmap, Z-108).

## 1. The plain answer - final standings per workload

The complete Wave-1 oracle was re-run at the close by Z-107
(the closing head-to-head, banked at seat 85b856f), and its
closing table supersedes Z-098's as the standings of record.
Run of record: 2026-11-13 09:57:33 to 09:58:59 EST, filed as
tests/perf/recorded_run_z13_z107.txt. The window was a
CO-TENANT window, recorded honestly in the run: an Aesir mojo
run_all was live throughout (loadavg 1.15 / 1.43 / 1.16 at
start, 2.34 / 1.67 / 1.27 at end); the identical-leg control
passed inside the Z-004 5% bound (worst 1.37%, large_batch)
and cross-invocation drift passed the 8% bound on every
headline column, so the run stands as the run of record under
that stated load. This-machine numbers only.

CLOSING TABLE - latency (Z-107, median ns/op, invocation 1):
Against DEFAULT glibc - LOSS on three of four:
- small_churn: mimir 29.43 vs glibc 3.35 - LOSS 8.79x.
- mixed_batch: mimir 29.12 vs glibc 3.72 - LOSS 7.83x.
- large_batch: mimir 98.45 vs glibc 710.47 - WIN 0.14x.
- mixed_lifecycle: mimir 531.01 vs glibc 7.12 - LOSS 74.58x.
Against the BEST TUNED glibc per workload - LOSS on all four:
- small_churn: mimir 29.43 vs tuned 3.35 - LOSS 8.79x.
- mixed_batch: mimir 29.12 vs tuned 3.71 - LOSS 7.85x.
- large_batch: mimir 98.45 vs tuned 9.08 - LOSS 10.84x - with
  its cache raised, tuned glibc runs this workload about 11x
  faster than MimirAlloc; the default-glibc WIN above does not
  survive the fair fight at the close either.
- mixed_lifecycle: mimir 531.01 vs tuned 3.16 - LOSS 168.04x.
Against its own Z-009 baseline the final core improved on
every workload (-68.5% small_churn, -92.7% mixed_batch,
-96.8% large_batch, -40.9% mixed_lifecycle) and still loses
three of four to default glibc. Counters grade the same
pattern (the Z-107 run, section 7.2): large_batch cycles and
instructions WIN 0.15x against default glibc and LOSS 4.39x /
4.30x against tuned; every other counter cell is a LOSS
(small_churn cycles 6.58x / 6.84x, instructions 6.40x /
6.40x; mixed_batch cycles 5.34x / 5.74x, instructions 5.62x /
5.62x; mixed_lifecycle cycles 51.48x / 103.94x, instructions
44.83x / 95.86x; default column first, tuned second).

The previous full re-run of record (Z-098, the fair-fight leg,
banked 1cfc4b8, quiet window) is superseded by the table above
and retained here as the arc's penultimate point: against
default glibc, large_batch WIN (98.07 vs 685.65 ns/op, 0.14x)
with the other three LOSS; against best tuned glibc, LOSS on
all four (small_churn 29.27 vs 3.32, 8.82x; mixed_batch 29.35
vs 3.71, 7.91x; large_batch 98.07 vs 8.92, 10.99x;
mixed_lifecycle 517.89 vs 3.01, 172.06x). Tuning itself graded
at Z-098: TUNING WIN on large_batch (-98.70%) and
mixed_lifecycle (-57.90%), TIE on small_churn and mixed_batch.

The campaign arc on the same four workloads (medians, ns/op):
- Founding baseline (Z-003/Z-009, contended window): LOSS on all
  four - small_churn 93.43 vs 9.02 (10.4x), mixed_batch 398.42 vs
  9.90 (40.2x), large_batch 3064.69 vs 1295.68 (2.4x),
  mixed_lifecycle 898.76 vs 13.38 (67.2x).
- Wave 6 standings (Z-054 run 2, quiet window): small_churn 28.52
  vs 3.32 (LOSS 8.59x), mixed_batch 27.15 vs 3.68 (LOSS 7.38x),
  large_batch 63.48 vs 658.29 (WIN, factor indicative ~0.10x, grade
  proven three ways), mixed_lifecycle 505.65 vs 7.03 (LOSS 71.93x).
- Every mimir absolute improved across the campaign against its
  founding value (93.43 -> 28.52 -> 29.43 small; 398.42 -> 27.15
  -> 29.12 mixed_batch; 3064.69 -> 63.48 -> 98.45 large; 898.76
  -> 505.65 -> 531.01 lifecycle; the middle point is Z-054 run 2
  and the last is the Z-107 closing run; cross-window movement,
  not grades). No workload except large_batch ever changed
  grade, and large_batch's WIN holds only against default glibc.

Real-program standings (not the synthetic oracle):
- Real traces (Z-094): the three canonical traces (python import
  storm, git object churn, Ollama session) replay MATCH on BOTH
  legs - 2,987,619 ops, 0 failures: as correctness artefacts a TIE.
  On replay speed MimirAlloc stands at 343.63 / 558.04 / 2907.94
  ns/op (git / ollama / python) against glibc 31.58 / 16.66 /
  12.37 (Z-095) - the same small-path LOSS, restated so no reader
  mistakes trace correctness for trace speed.
- Preload on real programs (Z-085, quiet window): Python total
  LOSS 5.01x (0.2772 s plain vs 1.3876 s preloaded), Python peak
  RSS LOSS +11.5%; Ollama model load LOSS ~22-24x per cold load;
  Ollama generation TIE. Correctness PASS everywhere tried
  (section 5.5).
- Short-lived processes (Z-097): the interposition tax is a LOSS
  on every shape: +16.98% for a process that never allocates,
  +26.72% for one allocation, +71.08% for eight, +130.25% (2.30x)
  for a 1000-cycle working process.
- Batch shapes (Z-093): batching LOSS at depth (section 2, KL-E).

## 2. Final standings per loss class

KL-A - single-thread small-op latency (small path vs glibc tcache).
STILL LOST - REASON NAMED. Same-window standings: LOSS 8.59x
(Z-054 run 2: 28.52 vs 3.32 ns/op) and LOSS 8.82x against tuned
glibc (Z-098: 29.27 vs 3.32). The closing run confirms the
grade unmoved (Z-107: small_churn LOSS 8.79x against both
glibc columns, 29.43 vs 3.35 ns/op; counters at the close
still price the wound - instructions LOSS 6.40x against both
columns, mimir 520.27 vs glibc 81.34 instructions/op). The autopsy (Z-027) named the wound:
instruction count, not cache behaviour - 861.97 vs 82.99
instructions/op and 175.65 vs 17.83 cycles/op at baseline (Z-005),
with free the dearer small entry (Z-019 holds: free 85.9 cycles vs
malloc 60). Wave 4's TLS cache and the adopted LTO (Z-035) and -O3
(Z-096) builds cut the absolute from 93.43 to ~29 ns/op; no banked
run ever brought the grade inside the tie band. Two geometry
defects named at Z-017 remain open: the 48 B class splits
cachelines on 49.41% of blocks and the 64 B class on 100% - a
carve-offset (span-header) item no slice took (Z-036 section 7).

KL-B - multithread scaling under the one global lock.
STILL LOST - REASON NAMED, SHAPE CHANGED. Before: inversion at 2
threads, scaling 0.043 / 0.033 of solo at 32 threads (Z-020).
After the TLS cache: no 2-thread inversion, honest scaling to one
CCD, then a cross-CCD cliff at 12 threads (scaling 0.412 small /
0.726 mixed) with partial SMT recovery (Z-045). Against glibc:
LOSS at every rung 1..32 on both ladder workloads (Z-021; Z-039
same-run order-of-gap about 94x / 134x at 32 threads, indicative
medians). The per-CCD arena partition is TIE vs the banked core at
21 of 24 rungs with a top-end mixed_batch WIN only (Z-038); the
lock-sharding follow-up was chosen by Z-039's R2 rule and banked
(afb89f5) - no banked verdict run re-graded the assembled scaling
curves after it, and none is claimed here. The cross-free flood
prices the surviving wound: direct remote free LOSES to glibc at
every percentile with a ~1.07 s worst case at 32 threads (Z-043);
the Z-030 remote-free queue was proven and priced (freer-side WIN,
pair total LOSS) and was never wired into mimir_free.

KL-C - large allocations: a mapping per allocation past the
1024-byte cutoff. DISCHARGED for repeated traffic; STILL LOST on
miss traffic - REASON NAMED. Z-048's bounded exact-length mapping
pool (64 mappings / 32 MiB) removed 97.6% of the mapping-path price
on cycling traffic (pool 64.95 vs pre-pool 2747.67 ns/op), and
large_batch stands at WIN ~0.10x vs default glibc (Z-054, factor
indicative, grade proven) / 0.14x in Z-098's window, and the closing run confirms the
WIN against default glibc (Z-107: 98.45 vs 710.47 ns/op,
0.14x) - but LOSS 10.84x vs tuned glibc at the close (Z-107:
98.45 vs 9.08; Z-098's 10.99x figure superseded as the
closing number). On a pool miss (first, unique, or
over-bound sizes) the full fresh-mapping price stands, the ~80x
cliff at 1025 B stands with it, and untouched huge alloc/free pairs
stand at LOSS 1.10-1.22x (Z-051). The cutoff of record never moved:
1024 bytes, justified by the wave's own pricing (Z-054 section 4).
Supporting standings: THP ADOPTED on pooled mappings at usable
sizes >= 2 MiB, rejected below (Z-049); huge allocations TIE
0.995..1.018 fully touched, 256 MiB..8 GiB (Z-051); realloc growth
by mremap TIE 1.004x vs glibc, from 3.77x LOSS (Z-052); retention
cost of the pool is a bounded LOSS on holed RSS patterns
(Z-053: checker holed net factor 3.804 vs glibc 1.031, capped by
the pool's own bounds).

KL-D - mixed lifecycle. STILL LOST - REASON NAMED (fast half);
slow half DISCHARGED. Fast half: LOSS 71.93x same-window (Z-054
run 2: 505.65 vs 7.03 ns/op) and 172.06x against tuned glibc
(Z-098: 517.89 vs 3.01). The closing run confirms the grade
unmoved and supersedes the closing figures (Z-107: LOSS
74.58x against default glibc, 531.01 vs 7.12 ns/op, and LOSS
168.04x against tuned glibc, 531.01 vs 3.16) - the deepest
standing wound of the line.
It exercises every path at once, and the paths it exercises most
(the small path, KL-A; lifecycle bookkeeping across classes) were
never rebuilt to glibc's per-op price. Slow half: an hour (Z-057)
and then eight hours (Z-100) of its own workload show no drift, no
leak, no fragmentation growth - resident shape is a WIN
(net_factor median 1.988 vs glibc 2.500 at one hour, Z-057; 1.895
vs 2.125 at eight hours, Z-100), retention bounded and returned
to exactly 0 under pressure cycles (Z-060). Placement cannot
soften the shape: best-fit LOSS, first-fit TIE, DECISION D-009
NO-ADOPTION (Z-059). The fragmentation mechanism is architectural
- class-locked free bytes, no split/coalesce below the page,
whole-page release the only healing (Z-055, Z-058) - present from
the first fragmented moment and proven not to worsen.

KL-E - batching. CLOSED as a LOSS - REASON NAMED. The dominant
factor is the live-set metadata walk: per-op cost grows with live
set because every alloc and free pays linear registry/span walks
(Z-091 F-4; walk priced at ~82-92% of free cost at deep live
sets). Caller-side batching at burst512 loses 11.94x (64 B) and
84.66x (1024 B) against its own interleaved shape, 33.8x / 120.2x
against glibc (Z-093); the Z-092 batch API prototype TIES the
per-op loop and therefore LOSES as a batching answer (it removes
no walk). Proposal P-1 is CLOSED, reaffirmed by two slices. The
only road left standing is Z-091's brief - span-grouped processing
with one registry pass per span - unbuilt by any banked slice.

## 3. The wave record, Z-001..Z-105 (all FINAL)

Wave 1 (Z-001..Z-009, verdict Z-009): the oracle stands. Full suite
green on x86-64 / 4 KiB pages at 4988 / 0 / 77 (Z-001; O-01 and
O-06 discharged), measurement law proven (Z-004 PROTOCOL GATE
PASS, identical legs agree within 0.85%), and the founding loss
table filed: LOSS on all four workloads (section 1).
Wave 2 (Z-010..Z-018, close Z-018): the machine priced - latency
ladder, bandwidth saturation at 16 threads (peak 112.76 GB/s),
cross-CCD ping-pong 3.2x intra-CCD, THP behaviour, fault cost
~345-505 ns/page with recycling proven NOT to cheapen faults at
>= 16 MiB (Z-014), and the design constants of record.
Wave 3 (Z-019..Z-027, verdict Z-027): the autopsy. The lock is not
slow; the work under it is. Free is the dearest small entry;
contention inverts to wait ~26x hold at 8 threads; every Z-009 gap
assigned a named owner slice.
Wave 4 (Z-028..Z-036, verdict Z-036): the TLS cache lands
(contract-first, Z-028/Z-029), the cross-thread free queue is
proven and priced but not wired in (Z-030), spacing NO-CHANGE by
measurement (Z-032), metadata REJECT-as-default (Z-034), LTO
adopted (Z-035: +18.46% / +33.68% on the pre-TLS core). The
pre-TLS verdict run stands STILL LOST 10.30x / 60.20x (Z-031).
Wave 5 (Z-037..Z-045, verdict Z-045): scaling's shape changes
(section 2, KL-B); false sharing purged from lock-free hot fields
(Z-040); affinity posture proven (Z-041); churn teardown
byte-exact (Z-042); the free storm finds and fixes two core
defects (Z-043); fork posture proven (Z-044).
Wave 6 (Z-046..Z-054, verdict Z-054): the large path flips
(section 2, KL-C) - threshold priced, mapping pool built, THP
adopted where it pays, trim under pressure, mremap TIE.
Wave 7 (Z-055..Z-063, verdict Z-063): KL-D's slow half discharged
(section 2); the fragmentation oracle and mechanism audit;
placement NO-ADOPTION (D-009); Soak 1 and Soak 2 agree.
Wave 8 (Z-064..Z-072, verdict Z-072): the allocator stops failing
quietly where the campaign could afford to hear it.
Observability counters at zero success-path cost (Z-064, Z-065,
D-010); corruption standings complete (section 5.3); canary
economics - one group adopt-candidate, three refused (Z-069);
double-free guard ADOPTED as capped, paying +0.9..+4.6 ns/pair
(Z-070); diagnostics OFF by default by ruling D-011 (Z-071).
Wave 9 (Z-073..Z-081, close Z-081): the public surface is built -
aligned allocation contract-first (Z-073/Z-074), usable size
(Z-075, D-012), public statistics (Z-076), trim + reallocarray
(Z-077), the zero-size/errno contract closed as DECISION D-013
(Z-078), budget enforcement adopted as D-014 (Z-079), SONAME /
versioning / install (Z-080), and a full ABI audit proving the
export contract three ways (Z-081).
Wave 10 (Z-082..Z-090, verdict Z-090): interposition (section 5.5).
Wave 11 (Z-091..Z-099, verdict Z-099): batching closed as a LOSS
(KL-E); real traces canonical (Z-094); trace tuning adopted
NOTHING - every candidate that helped the traces broke the
oracle (Z-095); -O3 adopted, -march=native and -flto refused
(Z-096); the per-process tax priced (Z-097); the fair fight filed
(Z-098, section 1).
Wave 12 (Z-100..Z-107 FINAL plus this verdict, section 7): the
eight-hour soak (Z-100: no drift either leg, drain retention
stable across 120 s / 1 h / 8 h - mimir retained net 36,400 KiB;
one both-leg host anomaly at t=21600/25200 recorded honestly, full
recovery); the combined storm battery GREEN in all 6 reps with a
byte-exact close, retained 20,480 B after the churn storm and 0 B
final (Z-101); memory-pressure chaos - every at-the-wall failure
either a journal-corroborated kernel OOM hitting both allocators
identically or a clean NULL + exactly one diag line with full
recovery, no allocator defect found (Z-102); corruption chaos at
scale (Z-103, section 5.3); the sanitizer/warning matrix clean
with one filed portability LOSS (Z-104, section 6); release
engineering - D-017 decides the next cut is 0.2.0, reproducible
build proven byte-identical on this host (Z-105, section 6).
The documentation truth pass (Z-106, seat 9a199ba, section
7.1) filed the Z13 line into the platform, target and
limitations registers without moving any grade those
registers did not already carry. The closing head-to-head
(Z-107, seat 85b856f, section 7.2) re-ran the complete
Wave-1 oracle on the final allocator and filed the closing
table of section 1. This verdict (Z-108, built on the
Z-001..Z-105 base and folded in over the banked Z-106/Z-107
per section 7.3) completes the wave: Wave 12 stands 9 of 9
FINAL.

## 4. Correctness standings (the contract was not sold)

The standing gate (Z-008) never moved: 4988 checks / 0 failed /
77 by-design skips at the founding baseline (Z-001) and at every
banked slice through Z-107, including under ASan, UBSan and
ASan+UBSan with zero sanitizer reports (Z-104). Roadmap Law 5
held: no speed slice traded away bounded behaviour, audit
surfaces, or the no-silent-corruption posture without a recorded
decision; the trades that were made are decisions D-009..D-017 in
docs/DECISION_LEDGER.md, each with its evidence.

## 5. Non-latency standings

5.1 Resident memory. Founding PEAK standings (Z-006): TIE on
small_churn (-4.3% Pss) and large_batch (+2.1%), LOSS on
mixed_batch (+23.3%) and mixed_lifecycle (+40.2%); retention
after free-all a WIN on three of four (small -15.3%, large
-39.9%, lifecycle -64.4%; mixed_batch the exception, LOSS
+16.3%). The soak line then proved the long-run shape: resident
WIN on the lifecycle workload at one hour and eight hours
(section 2, KL-D), retention bounded by named caps (span
retention 36,864 B in the storm battery, Z-101; pool bounded at
64 mappings / 32 MiB, Z-048) and returned to exactly 0 under
pressure (Z-060) and by trim in the battery's close (Z-101).
Closing RSS standings (Z-107) MOVE the oracle-workload half
of this section, and the move is named plainly: on the final
allocator the founding after-free standing is SUPERSEDED.
Z-107's RSS leg (median Pss, 5% band; Pss_Anon grades mirror
Pss in the recorded run) grades PEAK as LOSS on small_churn
(+333.0% against both glibc columns), mixed_batch (+542.5% /
+531.6%) and mixed_lifecycle (+269.3% / +272.7%), with
large_batch PEAK the only TIE (+3.5% / +3.2%); and
AFTER_FREE_ALL as LOSS nearly everywhere - small_churn
+1008.3% against both columns, mixed_batch +542.5% /
+534.3%, mixed_lifecycle +268.4% / +271.8%, large_batch
+2519.9% against default glibc and TIE +3.2% against tuned.
The Z-006/Z-009 after-free WIN on three of four workloads
(the founding figures above) does NOT survive to the final
core: retained bytes after free-all are, in Z-107's own
words, the honest regression of this campaign close. The
soak resident-shape WIN of section 2 (KL-D slow half, a
different workload and method) is not moved by this leg and
stands as recorded there.
5.2 Energy. The closing energy leg is Z-107's, and it
supersedes the founding leg (Z-007: LOSS in direction on all
four workloads, proven net factor 4.42x on large_batch) as
the standing of record for every cell it grades. Z-107 ran
in a co-tenant window (not a quiet window) with idle
baseline median 31.635 W. Graded cells: small_churn LOSS -
mimir 319.73 net nJ/op vs glibc 112.41 (2.84x) and vs tuned
98.20 (3.26x); large_batch WIN against default glibc -
mimir 500.94 vs 2513.57 net nJ/op (0.20x). Ungraded cells:
mixed_batch, large_batch against tuned, and
mixed_lifecycle - the reference net figure is below
idle-subtraction resolution, so no ratio is claimed; the
gross medians run adverse (mixed_batch mimir 1128.91 vs
glibc 173.18 / tuned 156.25; large_batch vs tuned mimir
4270.83 vs 541.67; mixed_lifecycle mimir 21503.91 vs glibc
360.24 / tuned 186.63 gross nJ/op). The pattern is the
latency pattern in energy dress: energy follows the
instruction-count wound (Z-005) everywhere except the pooled
large path against default glibc. A quiet-window energy
re-run remains a hand-off item (section 8).
5.3 Corruption and integrity. Wave 8 standings (Z-072): 19 of 25
J-006 probes DETECTED, 3 silences proven undetectable in this
design and closed with controls, 2 wild-pointer classes
crash-expected; the double-free guard adopted as capped (Z-070).
At scale (Z-103, 520 trials under churn load): false-positive
rate 0.0% (0/120 clean trials); descriptor flips DETECTED 96.7%
single-bit and 100% consistent-pair; span flips 0 silent among
completed trials (89/89), with 29 crash + 2 hang outcomes all in
the pointer fields - the crash-expected classes; the by-design
reserve-scratch family 100% silent, confirming Z-068's record
(it feeds no allocator decision). Failure drills under preload
(Z-088): loud failure through the diag channel for the filed
classes, victim survives, kill drills contaminate nothing; two
silent paths filed (F-Z088-1 interior/foreign-pointer frees
silent-safe; F-Z088-2 large-block double-free silent).
5.4 Public surface and contracts. Exports grew 4 (0.1.0 cut) to
13 namespaced on the default library (aligned pair Z-074, usable
size Z-075, stats Z-076, trim pair Z-077, memalign family Z-083),
ABI epoch unchanged at 0, every addition compatible (Z-105 S1).
Zero-size/errno is a decided contract (D-013), budgets are
enforced (D-014), diagnostics default OFF by ruling (D-011).
The surface remains Proposed: contracts.md S5.5 promotion unmet,
U-03 unresolved - 1.0 is withheld (Z-105 D-017).
Z-106's documentation truth pass re-checked the platform,
target and limitations registers against the banked wave
verdicts (section 7.1) and moved none of these standings.
5.5 Preload / interposition. The opt-in variant (20 exports,
default library untouched) is CORRECT everywhere tried: 13/13
smoke subjects byte-parity (Z-084), Python import storm + churn
exact checksum equality (Z-085), Ollama generation byte-identical
(Z-085), contained KDE applications pass (Z-086), Aesir engine
correctness TIE with identical token streams (Z-087). It is SLOW
where graded (section 1) and has exactly one known compatibility
break: the D-013 zero-size contract vs glibc anchored regcomp
(T-084-1 - a decided contract meeting a caller that treats NULL
as OOM; not heap corruption). Rollback is proven one-variable
from a broken state, and the nine-entry never-preload list
(systemd, display manager, banking tooling, live desktop,
package/repair tooling, sshd, long-lived services, backup
tooling, performance-critical model loading) is ops law (Z-089).

## 6. Open filings carried forward (honest, with owners)

- F-Z105-1 (Z-105, surface gap, NOT fixed): the interposing
  variant still carries the Z-082-era 10-name namespaced set - it
  does not carry mimir_memalign / mimir_valloc / mimir_pvalloc
  (Z-083), and its standard-name set has no memalign / valloc /
  pvalloc compositions either (proven by Z-105's recorded nm
  listing: exactly 20 exports, the trio absent from both sets).
  An LD_PRELOAD user of the interposing library gets the
  pre-Z-083 surface. Changing it is an export-contract decision
  (D-015's contract proves "exactly 20") needing its own slice
  and decision entry. Owner per Z-105: the next gap-closure slice
  after Z-104, or this hand-off (section 8) if none is
  commissioned. Z-106 carried the same filing as register
  item Z13-N1 with the same owner; no gap-closure slice was
  commissioned, so at this fold-in the owner resolves to
  this hand-off (section 8), where the item is carried
  traceably.
- F-Z105-2 (Z-105, stale release record, discharged only at the
  cut): docs/REPRODUCIBLE_BUILD.md's pinned inputs and
  release/SHA256SUMS describe the Pi 0.1.0 tree state and cannot
  verify any current artifact on any host. They stand as that
  cut's honest record; D-017's cut checklist regenerates both at
  the 0.2.0 cut. No silent rewrite was made.
- D-017 execution note (Z-105): the next release cut is 0.2.0 -
  decided, NOT executed. VERSION stays 0.1.0 in-tree until the
  cut; cutting a public release is Volmarr's act (M-007
  precedent); Z-106 and Z-107 have since landed
  (section 7), so the cut can carry the closing table. Signing remains deferred-by-absence
  (owner Volmarr).
- F-104-1 (Z-104, FILED PORTABILITY LOSS, owner: the next
  core-touching portability slice, PBR-053 line): clang 22.1.8
  cannot compile mimir_core.c (122 __atomic call sites); the
  shipped gcc build is unaffected and the platform file passes
  clang. The matrix otherwise stands clean: full suite
  4988/0/77 under ASan, UBSan and both, zero warnings on all
  three shipped flavours, and the measured coverage boundary
  that ASan does not redzone mimir-served spans.
- Carried rigour items from earlier verdicts, still open at
  Z-105: the carve-offset geometry item (KL-A, section 2); the
  Z-034 free-list liveness walk (O(carve_count) in
  find_span_live_locked) named as a component of the batch
  excess; Z-095's carried re-validation of its tuning candidates
  against the now-banked Z-094 canonical traces (cited with
  Z-093 per Z-095's own direction; no adoption is pre-judged);
  Z-043's realloc-retire residual (strict finder can return NULL
  in the D2 transient window, original preserved, retryable);
  INC-Z048-01's standalone-battery repairs (i003/h001/g003/
  h003/j006 red at the pristine baseline, outside the standing
  gate's composition); Wave 8 section 8's suite-modernisation
  and G3 canary implementation follow-on (an approved flag-gated
  MIMIR_CANARY stride variant for classes 320-1024, unbuilt);
  D-09's mmap-refusal fuzz-injection coverage.

## 7. Closing sections - Z-106 and Z-107 (both FINAL)

7.1 Z-106 - Documentation truth pass: FINAL. Banked at seat
9a199ba (3 files, +245, documentation only), built on seat
719b00d (banked Z-001..Z-105). What it filed, re-read
first-hand from disk on the fold-in merged tree by this
lead: (1) docs/SUPPORTED_PLATFORMS.md gains the Z13 line
addendum - Linux, x86-64, glibc, 4 KiB pages on purpleraider
is VERIFIED-RUN, scoped to this host and this tree,
discharging the x86-64 section's earlier grade for this
scope only; the pre-Z13 sections are preserved as their own
record and the AArch64 rows rest on their own Pi evidence
(this campaign re-ran nothing on AArch64). Its performance
paragraph states the Z-098 tuned-glibc LOSS on all four
workloads without softening and claims no performance win
beyond the recorded cells. (2) docs/SUPPORTED_TARGETS.md
section 8 adds exactly one row - the same Z13 host,
VERIFIED-RUN - and changes no other grade; the 4 KiB
AArch64, musl, Android, Termux, proot and SM-X400 grades
stand exactly as the matrix records them.
(3) docs/LIMITATIONS_AND_KNOWN_DEFECTS.md Part (v) records
what this campaign changed and what it did not: discharged
or superseded on this line - L-02 (extended public surface,
13 default exports), L-03 and D-04 (budget enforcement,
D-014), L-09 (zero-size/errno contract, D-013), the L-07
gate ruled (D-011), D-01 and D-02 addressed (counters,
D-010), L-04 partially discharged (public statistics,
D-012), O-01 and O-06 discharged for the Z13 host scope,
L-06 superseded in figures with its verdict unchanged in
kind (still LOSS); standing unchanged - L-01 (no default
interposition), L-05 (the central lock is the shipped
default; lock-sharding is a build flag, not the default),
L-08 (fork posture), the Proposed surface with 1.0 withheld
and VERSION still 0.1.0, and every other platform row. New
register items: Z13-N1 (F-Z105-1) and Z13-N2 (F-104-1),
both filed with owners (section 6); Z13-N3 (corruption
detection bounded, measured at scale, Z-103) and Z13-N4
(soak honesty, Z-100). The fold-in re-checked every doc
claim this verdict makes against the banked Z-106 diff: no
claim in this verdict rests on a register entry the truth
pass moved without naming the move (sections 5.1 and 5.2
carry the Z-107 moves; section 6 carries the filings).
Z-106's own gates, per its addenda: its baseline and final
standing gates both read 4988 / 0 / 77.
7.2 Z-107 - The closing head-to-head: FINAL. Banked at seat
85b856f (4 files, +1182: the recorded run, the runner, the
RSS runner and the summariser; no src/, API, ABI or Makefile
change). The complete Wave-1 oracle (latency, counters, RSS,
energy) was re-run on the final allocator against the Z-009
baseline and BOTH glibc columns (default and Z-098 tuned);
run of record 2026-11-13 09:57:33 to 09:58:59 EST in a
co-tenant window (Aesir mojo run_all live throughout;
loadavg 1.15 / 1.43 / 1.16 at start to 2.34 / 1.67 / 1.27 at
end), identical-leg control PASS (worst 1.37%, large_batch,
inside the Z-004 5% bound), cross-invocation drift PASS on
every headline column, all four legs PASS - filed as
tests/perf/recorded_run_z13_z107.txt and re-read first-hand
from disk on the fold-in merged tree by this lead. Section
1 now carries its closing table at source precision:
latency LOSS on three of four against default glibc
(large_batch WIN 0.14x) and LOSS on all four against tuned
glibc; counters the same pattern; RSS LOSS nearly
everywhere with the founding after-free WIN superseded
(section 5.1); energy one graded WIN (large_batch vs
default), one graded LOSS (small_churn), the rest ungraded
below resolution with adverse gross direction (section
5.2). Sections 2 and 5 were re-graded only where this
evidence actually moves a standing, and each move is named
where it lands (KL-A confirmed unmoved; KL-C and KL-D
closing figures superseded; 5.1 after-free standing
superseded; 5.2 standing of record replaced).
7.3 Fold-in record (executed per the protocol filed in this
section at the original build - the Z-099 section 6
pattern): (1) seat main 85b856f, carrying the banked Z-106
(9a199ba) and Z-107 (85b856f) commits, was merged into this
branch as merge 1c6c51c; the merged tree was verified
identical to the seat except this document before any
fold-in edit (git diff 85b856f HEAD lists only
docs/Z13_CAMPAIGN_VERDICT.md, 522 lines); CAMPAIGN_STATE.md
was preserved uncommitted and untouched by the merge.
(2) The baseline standing gate on the merged tree read
GREEN 4988 checks / 0 failed / 77 by-design skips, exit 0,
from its log (hidden_files/z108_foldin_baseline_gate.log).
(3) The banked Z-106 diff and the Z-107 recorded run were
re-read first-hand from disk on the merged tree - no number
entered from any report. (4) Sections 7.1 and 7.2 stand
FINAL above; section 1's table is the Z-107 closing table;
sections 2 and 5 were re-graded only where Z-107's evidence
moves a standing, with the move named. (5) The final
standing gate on the fold-in tree read GREEN 4988 / 0 / 77,
exit 0, from its log (hidden_files/z108_foldin_gate.log).
(6) This fold-in commit carries only this document and
completes Wave 12. No number anywhere in this verdict rests
on an unbanked source.

## 8. Hand-off notes to the production-and-beyond roadmap

- The successor line already exists in-tree:
  docs/POST_Z13_PRODUCTION_EXECUTION_ROADMAP_2026-11-13.md
  (164 gated work packages, PBR-001..164). This verdict is its
  evidence base for the Z13 line; where the two disagree, the
  recorded runs govern.
- The one architectural road to a small-path or batching WIN
  left standing by measurement is span-grouped processing with
  one registry pass per span (Z-091's brief, section 2 KL-E);
  the same walk prices part of KL-A/KL-D. Nothing else tried
  moved the small path's grade: cache (necessary, not
  sufficient), arenas (TIE), placement (NO-ADOPTION), tuning
  (NOTHING adopted), flags (-O3 adopted; the gain is real and
  the grade is unmoved).
- The fair fight is the honest target: any future speed claim
  must be graded against tuned glibc (Z-098's columns), not only
  default glibc, or it repeats the large_batch lesson.
- Open filings with owners are listed in section 6; two are
  carried here traceably by name, per Z-106's register:
  F-Z105-1 (section 6; register item Z13-N1) - the
  interposing variant's pre-Z-083 surface. Z-106 assigned it
  to the next gap-closure slice after Z-104 or the Z-108
  hand-off; no gap-closure slice was commissioned, so this
  hand-off is its owner of record: it passes to the
  production roadmap queue for a gap-closure slice with its
  own decision entry on D-015's exactly-20 contract.
  Carried, not discharged. F-104-1 (section 6; register
  item Z13-N2) - the clang portability LOSS on
  mimir_core.c; owner remains the next core-touching
  portability slice (PBR-053 line). Carried, not
  discharged. The release cut (D-017, 0.2.0) is Volmarr's
  act and carries F-Z105-2's regeneration with it.
- Wave 13 (Z-109..Z-116) is verdict-driven reserve: its pool
  choices are drawn from this verdict's evidence and recorded
  in WAVE_PLAN.md before each slice starts, per the roadmap.
  Candidates this verdict's evidence speaks to directly:
  per-CCD arena refinement (priced TIE at Z-038 - choose only
  with new evidence), API fuzz at scale (D-09 coverage gap,
  section 6), huge-page-only large-pool variant (the pool-miss
  and PMD-alignment items, sections 2 and 6), profile-guided
  class table v2 (Z-095's per-class bin near-miss, section 6),
  and the quiet-window energy re-run (section 5.2 -
  Z-107's closing energy leg ran in a co-tenant window, so
  the quiet-window run remains unrun).

## 9. Keeper stand-down checklist (attached per the Z-108 gate)

For the keeper, when the Z-108 fold-in banks and Wave 12 closes:
[ ] Banked count from git (ls-remote + seat log) = Z-001..Z-108
    with no gaps, one slice per commit, subjects in house style.
[ ] Z-106 and Z-107 sections of this verdict read FINAL; the
    pending-marker grep over docs/Z13_CAMPAIGN_VERDICT.md
    returns zero for Z-106/Z-107; section 7 reads FINAL.
[ ] Standing gate GREEN 4988 / 0 / 77 on the seat tree after
    the fold-in bank, counts read from the seat gate log.
[ ] WAVE_PLAN.md and PROJECT_MEMORY_MAP.md carry the Z-108
    entry and the Wave 12 completion line.
[ ] Current State board banked count updated to 108/117.
[ ] Section 6's filings each carry a named owner into the
    Wave 13 pool choice or the production roadmap queue -
    none silently dropped.
[ ] The keeper itself stands down only at Z-117 (campaign
    closure), per the roadmap; this checklist closes Wave 12,
    not the campaign.

## 10. Traceability index

Every FINAL number above traces to a banked file on the merged
tree, re-read first-hand by this lead:
- Founding standings: docs/Z13_WAVE01_BASELINE.md (Z-009) over
  tests/perf/recorded_run_z13_k002.txt (Z-003),
  recorded_run_z13_z005.txt (counters), _z006 (RSS), _z007
  (energy).
- Autopsy: docs/Z13_WAVE03_AUTOPSY.md (Z-027) over
  recorded_run_z13_z019.txt.
- Waves 4-8: docs/Z13_WAVE04_VERDICT.md (Z-036),
  Z13_WAVE05_VERDICT.md (Z-045), Z13_WAVE06_VERDICT.md (Z-054,
  standings run recorded_run_z13_z054.txt), Z13_WAVE07_VERDICT.md
  (Z-063), Z13_WAVE08_VERDICT.md (Z-072) - each over its own
  index of banked recorded runs.
- Wave 9: docs/Z13_Z081_ABI_AUDIT.md, docs/Z13_BUDGET_Z079.md,
  docs/Z13_Z077_TRIM_REALLOCARRAY.md, docs/Z13_Z078_ZERO_SIZE_
  ERRNO.md and the slices' recorded runs z073..z081.
- Wave 10: docs/Z13_WAVE10_VERDICT.md (Z-090) over recorded runs
  z082..z089.
- Wave 11 + fair fight: docs/Z13_WAVE11_VERDICT.md (Z-099) over
  recorded runs z091..z098.
- Wave 12 FINAL slices: tests/perf/recorded_run_z13_z100.txt,
  _z101.txt, _z102.txt, _z103.txt; docs/Z13_Z104_SANITIZER_
  WARNING_MATRIX.md + tests/abi/recorded_run_z13_z104.txt;
  docs/Z13_Z105_RELEASE_ENGINEERING.md + tests/abi/
  recorded_run_z13_z105.txt.
- Wave 12 closing slices (Z-106/Z-107): the Z13 addendum
  of docs/SUPPORTED_PLATFORMS.md, section 8 of
  docs/SUPPORTED_TARGETS.md and Part (v) of
  docs/LIMITATIONS_AND_KNOWN_DEFECTS.md (Z-106, seat
  9a199ba); tests/perf/recorded_run_z13_z107.txt (Z-107,
  seat 85b856f) - re-read first-hand on the fold-in tree.

## 11. Correctness gate for this slice

Standing gate on the merged tree before any Z-108 edit (baseline)
and on the final slice tree: both GREEN 4988 checks / 0 failed /
77 by-design skips, exit 0 (hidden_files/z108_baseline_gate.log
and z108_final_gate.log). Fold-in gates: baseline on the merged
Z-001..Z-107 tree and final on the fold-in tree, both GREEN
4988 / 0 / 77, exit 0 (hidden_files/z108_foldin_baseline_gate.log
and z108_foldin_gate.log). This slice adds one document and no
code, no tests, and no Makefile change; the gate guards the tree
the verdict describes. Bugs discovered during this slice: none
in allocator code. The Makefile merge conflict at base was a
union composition (seat superset taken whole), verified by the
empty post-merge diff, not a defect.
