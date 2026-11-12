# Z13 Wave 1 Baseline of Record

Slice: Z-009 - Wave 1 verdict: the baseline of record
File: docs/Z13_WAVE01_BASELINE.md
Campaign: MimirAlloc Z13, purpleraider (ASUS ROG Flow Z13, Strix Halo)
Roadmap: docs/MIMIRALLOC_Z13_ROADMAP.md, Wave 1 (Z-001 to Z-009)
Base commit: 5ad882b0006578732c85b9bcda68977dd644b009 (founding)

## Purpose and gate

This document is the on-machine table every later wave is judged
against: latency, counters, RSS and energy, both legs (mimiralloc
and glibc), across the four K-002 baseline workloads. Gate (roadmap
Z-009): every number in this document is traceable to a recorded
run in-tree. Each table below names its recorded run file beside
it; a number with no such citation does not belong in this file.
Fold-in note (keeper, at banking 2026-11-12): sections 5 and 6 were filed OWED by team-09 and are folded in below from the banked Z-006 and Z-007 recorded runs (verdict blocks verbatim, cited to their files); section 7 is updated for the banked Z-008. No number was added from any other source.

Laws that govern this baseline (roadmap, Laws of this line):
Law 1 - measurement first, this machine only. No number from any
other machine is evidence here. First-campaign Pi figures are
history and questions, never numbers in these tables.
Law 2 - honest grading. Every verdict is WIN, TIE, LOSS (with the
size of the loss), or STILL LOST - REASON NAMED. The Z-003 losses
stand exactly as recorded.
Law 3 - pinned methodology. Runs embed the Z-002 host header,
follow the Z-004 protocol, pattern-touch every block, and prove
workload equivalence by identical checksums.

## Host and environment of record

Source: tests/recorded_run_z002.txt (Z-002 host-truth capture,
scripts/z13/host_truth.sh, exit 0, 2026-11-12T06:57:53Z) and the
headers embedded in the Z-001, Z-003 and Z-005 recorded runs.

| Field | Value | Source |
|---|---|---|
| Hostname | purpleraider | Z-002 |
| Machine | ASUS ROG Flow Z13 (GZ302EA) | roadmap hardware of record; Z-001/Z-003 headers |
| CPU | AMD RYZEN AI MAX+ 395 w/ Radeon 8060S, 32 threads (nproc 32) | Z-002 cpu_model, cpu_count |
| Kernel | Linux 7.3.0-10-generic x86_64 | Z-002 |
| OS | Ubuntu 26.10 | Z-002 |
| glibc | ldd (Ubuntu GLIBC 2.44-1ubuntu2) 2.44 | Z-002 |
| Page size | 4096 bytes | Z-002 page_size_bytes; Z-001 getconf PAGESIZE |
| Cache | L1d 768 KiB (16), L1i 512 KiB (16), L2 16 MiB (16), L3 64 MiB (2) | Z-002 cache_topology |
| Governor | performance (amd-pstate-epp, min 2000000 kHz, max 5187500 kHz) | Z-002 |
| THP | enabled madvise, defrag madvise, shmem never | Z-002 |
| Compiler | gcc (Ubuntu 15.3.0-4ubuntu1) 15.3.0, flags -std=c11 -Wall -Wextra -Werror -pthread -O2 | Z-001, Z-003 |
| Base commit | 5ad882b0006578732c85b9bcda68977dd644b009 (founding) | Z-001, Z-002, Z-003, Z-005 headers |

Volatile Z-002 sample values at capture (date_utc, scaling_cur_freq
1643848 kHz, cpu_scaling_mhz_pct 37 percent, loadavg 4.78 4.29 2.29)
are capture-time samples in the Z-002 file, not constants. Each
measured run below records its own load state; see the load-state
column in the traceability index (section 8).

## 1. Correctness baseline

Source: tests/recorded_run_z001.txt (Z-001, 2026-11-12T01:38:58-05:00,
make clean and make test, unmodified tree).

| Measure | Value |
|---|---|
| Targets run | 8 of 8 in the make test target |
| Checks run | 4988 total (18 smoke plus 4970 CHECKS across the other 7 targets) |
| Checks failed | 0 |
| Make exit code | 0 |
| Skips | 77 SKIP lines, all by-design SKIPPED/NOT-YET or BLOCKED rows (70 in test_c007, 7 in test_c008) |
| Per-target CHECKS | smoke 18/18; test_core 29/0; test_b007 69/0; test_c005 95/0; test_c006 23/0; test_c007 122/0; test_c008 42/0 with 7 skipped-not-yet; test_d004 4590/0 (threads=4, host_page_size=4096, allocs=2088 frees=2088) |

Formal discharges in the Z-001 run header: O-01 DISCHARGED (first
full portable-suite run on x86-64) and O-06 DISCHARGED (first full
run on a 4 KiB-page host; every prior banked figure was a 16 KiB
figure). Load note in that run: load average about 4.2 at run start;
a robustness soak and a Tensile bench shared the host; no
interference observed and the suite completed normally.

Team-09 re-verification on this branch (founding base 5ad882b):
full make test in the team-09 worktree, log hidden_files/
z009_make_test.log (team-local, not banked): 4988 checks
(18 smoke plus 4970 CHECKS), 0 failed, 77 by-design SKIP lines,
EXIT 0 - identical counts to the Z-001 baseline.

## 2. Latency baseline - the Z-003 loss table

Source: tests/perf/recorded_run_z13_k002.txt (Z-003, bench window
2026-11-12T01:57-05:00). Harness tests/perf/bench_k002.c UNMODIFIED
(sha256 30dd0d9a8da12b03aa511fd35a4a098cf593b79c0e9ad352026ecb11eacc7d37),
7 alternating processes per leg. Load state: load average 5.82 /
4.42 / 2.28 at bench build, 5.29 (1-min) at run end, 6.14 just
after - the Tensile lab bench shared the host; legs alternate
process-by-process and medians are the headline, so drift lands on
both legs. These are the campaign's first on-machine numbers,
recorded with their contention, not a quiet-machine baseline.

Checksum proof (Z-003): exactly ONE distinct checksum per workload
across both legs and all 7 repetitions each - workloads proven
equivalent across legs: small_churn 12756656, mixed_batch 34727552,
large_batch 1106008, mixed_lifecycle 40359936.

Summary (medians of 7 processes per leg, from summarize_k002.py
in the Z-003 recorded run):

| Workload | glibc med ns/op | glibc med ops/s | mimir med ns/op | mimir med ops/s | Factor | Verdict |
|---|---|---|---|---|---|---|
| small_churn | 9.02 | 110910978.4 | 93.43 | 10703676.8 | 10.4x | LOSS |
| mixed_batch | 9.90 | 101020783.4 | 398.42 | 2509888.8 | 40.2x | LOSS |
| large_batch | 1295.68 | 771797.1 | 3064.69 | 326297.2 | 2.4x | LOSS |
| mixed_lifecycle | 13.38 | 74712059.4 | 898.76 | 1112649.3 | 67.2x | LOSS |

Spread (min..max ns/op across the 7 processes, Z-003 summary):
glibc small_churn 3.92..10.46, mixed_batch 4.16..11.73,
large_batch 673.45..2156.88, mixed_lifecycle 6.96..22.80;
mimiralloc small_churn 38.24..95.19, mixed_batch 237.71..535.11,
large_batch 2969.81..4878.46, mixed_lifecycle 859.12..1946.98.

Verdict of record (Z-003): LOSS on all 4 workloads on this machine
- the first on-machine loss table. MimirAlloc's closest leg is
large_batch (~2.4x); the wounds match the known-loss classes
(KL-A small path ~10.4x, KL-E/KL-B mixed_batch ~40.2x, KL-D
mixed_lifecycle ~67.2x). A proven loss is a bankable result
(roadmap Law 2).

## 3. Measurement law - the Z-004 protocol proof

Source: tests/perf/recorded_run_z13_z004.txt (Z-004, record
assembled 2026-11-12T02:39-05:00; passing demo run 02:38 EST).
Protocol: docs/Z13_MEASUREMENT_PROTOCOL.md; harness switches
scripts/z13/run_measured.sh and scripts/z13/summarize_measured.py.
Demonstration legs: IDENTICAL (build/bench_k002_glibc vs itself,
sha256 30ab120e50c22a552a4945638d2e5b9198816a9bfbdda1c3a2420404b1ca3986),
pinned taskset -c 2, 15 measured processes per leg, 3 discarded
warm-up processes per leg, interleaved A,B,A,B. Bounds: noise
5 percent, drift 8 percent, one checksum per workload.
Load state: loadavg 0.64 / 1.38 / 1.60 at demo start and end;
no known co-tenant heavy load during the passing attempt.

| Workload | medA ns | medB ns | agree % | driftA % | driftB % | Verdict |
|---|---|---|---|---|---|---|
| large_batch | 660.66 | 656.62 | 0.61 | 0.07 | 1.55 | AGREE |
| mixed_batch | 3.70 | 3.69 | 0.27 | 0.27 | 0.00 | AGREE |
| mixed_lifecycle | 7.01 | 7.07 | 0.85 | 2.28 | 1.27 | AGREE |
| small_churn | 3.31 | 3.31 | 0.00 | 0.30 | 0.00 | AGREE |

PROTOCOL GATE: PASS (Z-004). Two identical legs agree within
0.85 percent worst case against the 5 percent noise bound, drift
at most 2.28 percent against the 8 percent bound, exactly one
checksum per workload across both legs (12756656, 34727552,
1106008, 40359936 - the Z-003 checksums). These identical-legs
medians are a measurement-law proof, not an allocator verdict;
the Z-003 loss table stands as the allocator comparison of
record. The Z-004 attempts record (9 attempts, 8 voided by the
drift rule and kept, never deleted) is part of that recorded run.

## 4. Counter baseline - the Z-005 perf-counter leg

Source: tests/perf/recorded_run_z13_z005.txt (Z-005, perf run
window 2026-11-12T02:16-05:00). Method: probe_z005.c wraps the
UNMODIFIED bench_k002.c (same sha256 as Z-003), one workload per
process, 5 measured processes per leg per workload, legs
alternating. Counters via sudo -n perf stat (cycles, instructions,
cache-misses, branch-misses); unprivileged perf_event_open is
blocked on this host (perf_event_paranoid = 4) and no host setting
was changed - the blocker proof is recorded in the Z-005 run.
Every event in every block reports 100.00 percent running time.
Checksum proof: every one of the 40 blocks has checksum == reps x
the Z-003 recorded per-pass checksum. Load state: load average
0.61 / 2.25 / 2.43 at first check (02:11), 0.85 / 1.56 / 2.05 at
perf-run end (02:16) - a QUIET window (contrast Z-003 at load
5.29-6.14 under Tensile contention). That is why this run's ns/op
medians are faster than Z-003's for both legs; the counter ratios,
not the absolute ns/op, are the portable result here.

Counter table (medians per op, summarize_z005.py in the Z-005 run):

| Leg | Workload | cyc/op | ins/op | cmiss/op | bmiss/op | ns/op |
|---|---|---|---|---|---|---|
| glibc | small_churn | 17.826 | 82.990 | 0.01362 | 0.00472 | 3.87 |
| glibc | mixed_batch | 18.306 | 100.611 | 0.01806 | 0.01468 | 3.59 |
| glibc | large_batch | 3512.486 | 8820.614 | 4.64864 | 6.25547 | 698.76 |
| glibc | mixed_lifecycle | 35.280 | 208.216 | 0.01048 | 0.02965 | 6.79 |
| mimiralloc | small_churn | 175.653 | 861.968 | 0.01688 | 0.00613 | 34.49 |
| mimiralloc | mixed_batch | 1173.776 | 3125.603 | 0.45442 | 2.73256 | 228.81 |
| mimiralloc | large_batch | 14005.627 | 32076.006 | 13.06845 | 23.79151 | 2751.60 |
| mimiralloc | mixed_lifecycle | 4374.607 | 9550.162 | 4.90669 | 8.34941 | 852.87 |

Reading of record (Z-005): small_churn cycles/op 175.65 vs 17.83
(9.9x), instructions/op 861.97 vs 82.99 (10.4x), cache-miss/op
near-parity (0.0169 vs 0.0136) - the small path's loss is
instruction count, not cache behaviour; KL-A is a code-path-length
wound. mixed_batch cycles/op 1173.78 vs 18.31 (64.1x),
instructions/op 3125.60 vs 100.61 (31.1x), cache-misses/op 0.454
vs 0.018 (25x). large_batch cycles/op 14005.63 vs 3512.49 (4.0x),
instructions/op 32076.01 vs 8820.61 (3.6x); both legs are
cache-miss heavy per op (13.07 vs 4.65). mixed_lifecycle cycles/op
4374.61 vs 35.28 (124.0x), instructions/op 9550.16 vs 208.22
(45.9x), cache-misses/op 4.91 vs 0.010 - the deepest counter wound
on this machine. Overall (Z-005): MimirAlloc executes 3.6x..46x
more instructions per op than glibc on every baseline workload.

## 5. RSS / PSS baseline - Z-006

Folded in at banking by the keeper (see Purpose note). Source:
tests/perf/recorded_run_z13_z006.txt (banked with Z-006):
smaps-rollup resident-memory measurement, both legs, Z-003
workloads, phases START/WARM/STEADY/PEAK/AFTER_FREE/
AFTER_FREE_ALL; medians of 5 processes per leg, taskset CPU 2.
Load state: shared machine, loadavg 2.96/1.89/1.59 at run
start, 2.51/1.89/1.60 at end (Tensile lab and Lifthrasir forge
co-tenants). Verdict lines verbatim from the recorded run:

small_churn PEAK: live=851968 B | Pss glibc 1239 kB, mimir 1186 kB -> TIE (-4.3%) | Pss_Anon glibc 1192 kB, mimir 1132 kB -> WIN (-5.0%)
small_churn AFTER_FREE_ALL: live=851968 B | Pss glibc 419 kB, mimir 355 kB -> WIN (-15.3%) | Pss_Anon glibc 372 kB, mimir 300 kB -> WIN (-19.4%)
mixed_batch PEAK: live=79872 B | Pss glibc 245 kB, mimir 302 kB -> LOSS (+23.3%) | Pss_Anon glibc 196 kB, mimir 244 kB -> LOSS (+24.5%)
mixed_batch AFTER_FREE_ALL: live=79872 B | Pss glibc 245 kB, mimir 285 kB -> LOSS (+16.3%) | Pss_Anon glibc 196 kB, mimir 228 kB -> LOSS (+16.3%)
large_batch PEAK: live=7024640 B | Pss glibc 7021 kB, mimir 7169 kB -> TIE (+2.1%) | Pss_Anon glibc 6976 kB, mimir 7116 kB -> TIE (+2.0%)
large_batch AFTER_FREE_ALL: live=7024640 B | Pss glibc 281 kB, mimir 169 kB -> WIN (-39.9%) | Pss_Anon glibc 244 kB, mimir 116 kB -> WIN (-52.5%)
mixed_lifecycle PEAK: live=703936 B | Pss glibc 883 kB, mimir 1238 kB -> LOSS (+40.2%) | Pss_Anon glibc 832 kB, mimir 1180 kB -> LOSS (+41.8%)
mixed_lifecycle AFTER_FREE_ALL: live=703936 B | Pss glibc 885 kB, mimir 315 kB -> WIN (-64.4%) | Pss_Anon glibc 832 kB, mimir 256 kB -> WIN (-69.2%)

PEAK resident cost is a split result: TIE on small_churn (Pss -4.3%) and large_batch (+2.1%), LOSS on mixed_batch (+23.3% Pss, +24.5% Pss_Anon) and a clear LOSS on mixed_lifecycle (+40.2% Pss, +41.8% Pss_Anon) - at peak, mimiralloc holds materially more resident memory than glibc on the mixed workloads.
Retention after free-all reverses on three of four: WIN small_churn (-15.3% Pss), WIN large_batch (-39.9%), WIN mixed_lifecycle (-64.4% - glibc still holds 885 kB resident with nothing live where mimiralloc is back to 315 kB); the exception is mixed_batch, LOSS at both points (+16.3% after free-all) - the mixed-size path of mimiralloc both peaks higher and retains more. A proven loss is a bankable result (roadmap Law 2): the mixed_batch resident overhead is now a measured, named wound for the optimisation waves.

## 6. Energy baseline - Z-007

Folded in at banking by the keeper (see Purpose note). Source:
tests/perf/recorded_run_z13_z007.txt (banked with Z-007):
RAPL package energy per workload for both legs, measured via
sudo -n perf stat -a (system-wide; energy_uj is root-only on
this host, no host setting changed), idle-baseline subtraction
with idle median 35.95 W. Load state: contended window,
loadavg 8.65/3.39/2.15 at run start decaying to
3.67/3.12/2.17; NOT a quiet baseline. Verbatim from the run:

RAPL IS exposed on this host, so the roadmap gate is discharged with the joules/op table below, not with the not-exposed proof.

small_churn      glibc median        13.28 nJ/op | mimiralloc median       236.69 nJ/op | factor   17.82x | LOSS
mixed_batch: glibc median net J/op <= 0 (-12.25 nJ) - BELOW IDLE-SUBTRACTION RESOLUTION, no ratio claimed
large_batch      glibc median     11861.56 nJ/op | mimiralloc median     52394.16 nJ/op | factor    4.42x | LOSS
mixed_lifecycle  glibc median        10.71 nJ/op | mimiralloc median      6490.07 nJ/op | factor  606.10x | LOSS

large_batch      LOSS, factor 4.42x on median net J/op (glibc 11861.56 nJ/op, mimiralloc 52394.16 nJ/op) - the strongest row in the table: both legs have long windows (glibc 0.59..0.81 s, mimiralloc 2.68..3.04 s), every per-process net is positive, and the gross medians corroborate (160166.67 vs 35010.42 nJ/op, ratio 4.57x). The per-process spread is wide (glibc net 1350.30..22434.21) because the package baseline drifted during the window, so 4.42x is a median estimate, not a precise constant - but the LOSS and its rough size are proven.
small_churn      LOSS - per-process net ranges are disjoint (mimiralloc 90.43..596.97 nJ/op vs glibc -5.85..42.47 nJ/op) and gross ranges are disjoint too (mimiralloc 1368.00..1864.50 vs glibc 152.50..200.00 nJ/op), so the direction is proven. The printed median factor 17.82x is INDICATIVE only: the glibc median (13.28 nJ/op) sits near the idle-subtraction floor (2 of 7 glibc nets are negative), so the true factor could be materially smaller or larger; what is proven is a large loss, consistent with the Z-005 counter leg (9.9x cycles/op, 10.4x instructions/op).
mixed_lifecycle  LOSS in direction; net factor NOT claimed. The summariser's printed 606.10x divides by a glibc median (10.71 nJ/op) that is inside its own per-process noise (-66.25..72.51 nJ/op, 3 of 7 negative) at 45..54 ms glibc windows - that factor is arithmetic, not measurement, and is not claimed here. The direction is proven by the gross counters: medians 40021.70 (mimiralloc) vs 384.11 (glibc) nJ/op with disjoint gross ranges (35900.61..48062.07 vs 355.90..425.35), and by mimiralloc's all-positive nets (3536.29..13277.16 nJ/op). This is also the workload where Z-005 found the deepest counter wound (124x cycles/op).
mixed_batch      NO NET RATIO - below idle-subtraction resolution (the summariser says so itself): the glibc median net is -12.25 nJ/op at 41..46 ms windows, and 2 of 7 mimiralloc nets are also negative because package power during those early blocks ran below the window's median idle level while the 03:08 co-tenant burst decayed. LOSS in direction by the gross counters: medians 10597.66 (mimiralloc) vs 184.90 (glibc) nJ/op, gross ranges disjoint (6497.40..12666.67 vs 117.19..210.94). A net factor for this workload requires a quieter window and is recorded as follow-up work, not invented here.
Overall: the energy leg confirms the time and counter legs - MimirAlloc burns far more package energy per op than glibc on every baseline workload on this machine (proven LOSS in direction on all four; proven net factor on large_batch, indicative on small_churn; net factor below resolution on the two sub-55 ms glibc workloads under this load). A proven loss, measured as deep as the shared machine honestly allows, is a bankable result (roadmap Law 2).

## 7. Standing correctness gate - Z-008

Z-008 banked before this document, in banking order: the gate
script scripts/z13/run_gate.sh is in-tree, with its green run
recorded in tests/recorded_run_z008.txt (checks 4988, failed
0, by-design skips 77, GATE RESULT: GREEN on the unmodified
tree; loadavg 5.41 3.78 2.61 with Tensile and Lifthrasir
contention noted in the run header). The Z-001 correctness
baseline (section 1) stands, re-verified identically by
Z-003, Z-004, Z-005 and team-09 make test for this slice
(4988 / 0 failed / 77 by-design skips, EXIT 0 in each case).

## 8. Traceability index

| Section | Numbers from | Recorded run in-tree | Load state of the run |
|---|---|---|---|
| Host | Z-002 | tests/recorded_run_z002.txt | loadavg 4.78 4.29 2.29 at capture |
| 1 Correctness | Z-001 | tests/recorded_run_z001.txt | load avg about 4.2 at start; soak + Tensile shared host |
| 2 Latency | Z-003 | tests/perf/recorded_run_z13_k002.txt | 5.82/4.42/2.28 at build; 5.29 at end; Tensile contention |
| 3 Measurement law | Z-004 | tests/perf/recorded_run_z13_z004.txt | 0.64/1.38/1.60 start and end; quiet |
| 4 Counters | Z-005 | tests/perf/recorded_run_z13_z005.txt | 0.61/2.25/2.43 first check; 0.85/1.56/2.05 at end; quiet window |
| 5 RSS/PSS | Z-006 | tests/perf/recorded_run_z13_z006.txt | 2.96/1.89/1.59 start; 2.51/1.89/1.60 end; shared host |
| 6 Energy | Z-007 | tests/perf/recorded_run_z13_z007.txt | contended: 8.65/3.39/2.15 start; 3.67/3.12/2.17 end |
| 7 Gate | Z-008 | tests/recorded_run_z008.txt | 5.41/3.78/2.61; Tensile + Lifthrasir contention |

## 9. Wave 1 verdict

On purpleraider, at the founding base, MimirAlloc LOSES to glibc
on all four baseline workloads (Z-003: 10.4x, 40.2x, 2.4x, 67.2x
by median ns/op), and the counter leg explains the losses at
instruction-count level (Z-005: 3.6x..46x more instructions per
op, plus cache/branch wounds on the mixed workloads). The
measurement law is proven (Z-004 PROTOCOL GATE PASS), the host
truth is captured (Z-002), and the correctness baseline is green
(Z-001: 4988 / 0 / 77, EXIT 0). RSS standings (section 5):
PEAK is split - TIE on small_churn and large_batch, LOSS on
mixed_batch (+23.3 percent Pss) and mixed_lifecycle
(+40.2 percent); retention after free-all is a WIN on three
of four workloads (mixed_batch the exception, LOSS at both
points). Energy standings (section 6): LOSS in direction on
all four workloads - proven net factor 4.42x on large_batch,
small_churn factor indicative only, net factors for the two
sub-55 ms glibc workloads below idle-subtraction resolution
under this load (quiet-window re-run recorded as follow-up
work in the Z-007 recorded run). Later waves are judged
against sections 1, 2 and 4 of this document, under the
Z-004 protocol, on this machine only.
