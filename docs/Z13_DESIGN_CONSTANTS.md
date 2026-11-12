# Z13 Design Constants of Record

Slice: Z-018 - Design constants of record (Wave 2).
Status: verified 2026-11-12 by the team-04 lead and its six Mythic Engineering verifier sub-agents, every constant checked line-by-line against its recorded run on disk (seat + team-01/02/03/05 worktrees). RATIFIED numbers below are the only hardware numbers Waves
3-7 may assume on the MimirAlloc Z13 line. Every constant carries
its citation to a Wave 2 recorded run (or, where marked, a banked
Wave 1 run). No uncited constant is admitted: a row without a
landed recorded run is listed under Open Rows and is NOT a
constant of record.
Host of record: purpleraider, AMD RYZEN AI MAX+ 395 w/ Radeon
8060S (Strix Halo), x86-64, 128 GiB UMA, single NUMA node.
Citation rule: file paths are relative to the campaign repo root;
Wave 2 runs not yet banked live in the named team worktree until
the keeper banks them in roadmap order.
Load honesty: each section records the load state its source run
recorded. Numbers measured under contention are labelled as such
and are not quiet-machine baselines.

## 1. Host and topology constants

| Constant | Value | Citation |
|---|---|---|
| CPU | AMD RYZEN AI MAX+ 395 w/ Radeon 8060S | Z-002, tests/recorded_run_z002.txt |
| Logical CPUs | 32 (16 physical cores, SMT 2) | Z-002, tests/recorded_run_z002.txt; Z-012, tests/perf/recorded_run_z012.txt |
| CCD count | 2 | Z-010, tests/perf/recorded_run_z010.txt; Z-012, tests/perf/recorded_run_z012.txt |
| CCD0 CPUs | physical 0-7, SMT siblings 16-23 | Z-012, tests/perf/recorded_run_z012.txt |
| CCD1 CPUs | physical 8-15, SMT siblings 24-31 | Z-012, tests/perf/recorded_run_z012.txt |
| SMT sibling rule | sibling of logical n (n<16) is n+16 | Z-012, tests/perf/recorded_run_z012.txt |
| NUMA | single node, UMA | Z-011, tests/perf/recorded_run_z011.txt |
| Base page size | 4096 bytes | Z-002, tests/recorded_run_z002.txt |
| Kernel / glibc | Linux 7.3.0-10-generic x86_64 / glibc 2.44 | Z-002, tests/recorded_run_z002.txt |
| Governor / driver | performance / amd-pstate-epp | Z-002, tests/recorded_run_z002.txt |

## 2. Cache and latency constants (Z-010)

Source load state: Tensile lab ran throughout (load 2.00-4.85);
L1/L2 plateaus reproduce across both CCDs and both runs; DRAM and
L3-transition values are this-machine numbers under that stated
load, not quiet baselines.

| Constant | Value | Citation |
|---|---|---|
| Cache line size | 64 bytes (coherency_line_size 64) | Z-010, tests/perf/recorded_run_z010.txt |
| L1d per core | 48 KiB; plateau 4 KiB..48 KiB at 0.79-0.88 ns/access | Z-010, tests/perf/recorded_run_z010.txt |
| L2 per core | 1 MiB (1024K); plateau 64 KiB..384 KiB at 2.73-2.87 ns/access | Z-010, tests/perf/recorded_run_z010.txt |
| L3 per CCD | 32 MiB, NOT unified across CCDs; local plateau 2 MiB..12 MiB at 8.59-13.45 ns/access | Z-010, tests/perf/recorded_run_z010.txt |
| L3 total | 64 MiB (2 instances) | Z-002, tests/recorded_run_z002.txt |
| DRAM plateau | from 64 MiB; 107.7-170 ns/access at 64 MiB..512 MiB, 4 KiB pages, page-walk included, under recorded load | Z-010, tests/perf/recorded_run_z010.txt |
| Plateau boundaries | 48 KiB (L1d), 1 MiB (L2), 32 MiB (per-CCD L3), DRAM from 64 MiB; no step at 64 MiB for a second CCD L3 | Z-010, tests/perf/recorded_run_z010.txt |

## 3. Bandwidth and saturation constants (Z-011)

Source load state: quiet window (load 0.67-0.73, no process above
0.8% CPU); STREAM-style triad, pinned, best of 5 reps.

| Constant | Value | Citation |
|---|---|---|
| One core alone | 38.2 GB/s (38.25 GB/s primary) | Z-011, tests/perf/recorded_run_z011.txt |
| Single-CCD plateau | 61.7-61.8 GB/s, reached at 4 threads | Z-011, tests/perf/recorded_run_z011.txt |
| Saturation point (machine) | 16 threads (all physical cores, both CCDs) | Z-011, tests/perf/recorded_run_z011.txt |
| Peak bandwidth at saturation | 105.89-112.76 GB/s across 3 runs; peak 112.76 GB/s | Z-011, tests/perf/recorded_run_z011.txt |
| Saturation per CCD | 4 streaming threads per CCD on the way to 16 | Z-011, tests/perf/recorded_run_z011.txt |
| SMT bandwidth verdict | 32 threads: 97.66-101.80 GB/s, 4-10% BELOW 16 threads; SMT buys nothing for streaming | Z-011, tests/perf/recorded_run_z011.txt |
| Curve (primary, GB/s) | 1T 38.25, 2T 60.24, 4T 61.68, 8T 61.79, 16T 105.89, 32T 97.66 | Z-011, tests/perf/recorded_run_z011.txt |

## 4. CCD and SMT constants (Z-012)

Source load state: quietest window recorded (load 0.27/1.43/2.08);
near-baseline quality, residual contention named in the run.

| Constant | Value | Citation |
|---|---|---|
| Intra-CCD shared-line transfer | 32.5/32.2 ns median (CCD0/CCD1) | Z-012, tests/perf/recorded_run_z012.txt |
| Intra-CCD far (0<->7) | 37.0 ns median, ~1.14x adjacent | Z-012, tests/perf/recorded_run_z012.txt |
| Cross-CCD shared-line transfer | 103.0/103.7 ns median | Z-012, tests/perf/recorded_run_z012.txt |
| CCD penalty factor | 3.2x (cross-CCD / intra-CCD) | Z-012, tests/perf/recorded_run_z012.txt |
| SMT sibling shared-line | 33.4 ns median, ~1.03x intra-CCD | Z-012, tests/perf/recorded_run_z012.txt |
| SMT scaling factor | 0.18 (sibling adds ~18% streaming throughput; a sibling is ~0.2 of a core for memory work) | Z-012, tests/perf/recorded_run_z012.txt |
| Streaming pair scaling | intra-CCD pair 1.66x, cross-CCD pair 1.84x vs single 34.2 GB/s | Z-012, tests/perf/recorded_run_z012.txt |
| Arena rule | ARENA_PER_CCD = YES; CROSS_CCD_SHARING = AVOID on hot paths; PREFER_INTRA_CCD = YES | Z-012, tests/perf/recorded_run_z012.txt |
| Thread sizing rule | size thread-count-sensitive structures for 16 physical cores; threads 17-32 are ~0.2-core helpers | Z-012, tests/perf/recorded_run_z012.txt |

## 5. Page policy and THP constants (Z-002, Z-013)

Source load state (Z-013): quiet machine (the run records 1-min load stayed <=0.4 across all run windows);
3 consecutive executions, REPS=5 each; synthesis across all 3.

| Constant | Value | Citation |
|---|---|---|
| THP host mode | enabled=madvise, defrag=madvise, shmem=never | Z-002, tests/recorded_run_z002.txt |
| PMD (huge page) size | 2 MiB (2097152 bytes) | Z-013, tests/perf/recorded_run_z13_thp.txt |
| Default page policy | 4 KiB base pages unless MADV_HUGEPAGE is applied; no assumption of always/never THP | Z-002, tests/recorded_run_z002.txt; Z-013, tests/perf/recorded_run_z13_thp.txt |
| THP at 512 KiB | no effect (below one PMD): minflt 128 both modes, AnonHugePages 0 | Z-013, tests/perf/recorded_run_z13_thp.txt |
| THP at 2 MiB | WIN: fault improve 88.8-90.6%; minflt 512 -> 1; write +10.6..14.6% | Z-013, tests/perf/recorded_run_z13_thp.txt |
| THP at 20 MiB | WIN: fault improve 90.1-90.6%; minflt 5120 -> 10; write +39.5..61.5% | Z-013, tests/perf/recorded_run_z13_thp.txt |
| THP at 200 MiB | WIN-fault / bandwidth-UNSTABLE: fault improve 81.4-84.6%; minflt 51200 -> 100; write -16.7..+0.3% across runs | Z-013, tests/perf/recorded_run_z13_thp.txt |
| First-touch fault cost, 4 KiB pages (Z-013 run 1 medians) | 350 ns/page at 2 MiB, 363 ns/page at 20 MiB, 476 ns/page at 200 MiB | Z-013, tests/perf/recorded_run_z13_thp.txt |
| First-touch fault cost, THP on (Z-013 run 1 medians) | 33 ns/page at 2 MiB, 36 ns/page at 20 MiB, 86 ns/page at 200 MiB (per 4 KiB page equivalent) | Z-013, tests/perf/recorded_run_z13_thp.txt |
| RSS honesty under THP | RSS equals touched bytes exactly in both modes at every size; 3 MiB non-multiple backs 2048 KiB huge + 1024 KiB base, RSS exactly 3072 KiB, no round-up waste | Z-013, tests/perf/recorded_run_z13_thp.txt |
| THP adoption rule of record | Adopt MADV_HUGEPAGE at the 2 MiB and 20 MiB decades; at 200 MiB the pooled-mapping adoption question stays OPEN (see Open Rows, section 11) | Z-013, tests/perf/recorded_run_z13_thp.txt |

## 6. Baseline context for Waves 3-7 (Wave 1, not Wave 2 constants)

These are workload baselines, not hardware constants; they are
included only so Waves 3-7 cite the same loss table.

| Constant | Value | Citation |
|---|---|---|
| K-002 loss, small_churn | mimir 93.43 vs glibc 9.02 ns/op, LOSS 10.4x | Z-003, tests/perf/recorded_run_z13_k002.txt |
| K-002 loss, mixed_batch | mimir 398.42 vs glibc 9.90 ns/op, LOSS 40.2x | Z-003, tests/perf/recorded_run_z13_k002.txt |
| K-002 loss, large_batch | mimir 3064.69 vs glibc 1295.68 ns/op, LOSS 2.4x | Z-003, tests/perf/recorded_run_z13_k002.txt |
| K-002 loss, mixed_lifecycle | mimir 898.76 vs glibc 13.38 ns/op, LOSS 67.2x | Z-003, tests/perf/recorded_run_z13_k002.txt |
| Instruction excess (Z-005) | mimir executes 3.6x-46x more instructions/op than glibc across the 4 workloads (small 10.4x, mixed_batch 31.1x, large 3.6x, lifecycle 45.9x) | Z-005, tests/perf/recorded_run_z13_z005.txt |
| Load note for Z-003/Z-005 | Z-003 under Tensile contention (load 5.29-6.14); Z-005 quiet window (load 0.61-0.85), counter ratios are the portable result | Z-003 / Z-005 recorded runs as above |

## 7. Fault-cost constants (Z-014)

Source: team-05 commit 09b8e55, tests/perf/recorded_run_z13_z014.txt.
Load: quiet window (1-min load 1.05-1.29); pinned taskset -c 2;
median of 7 processes; MADV_NOHUGEPAGE on every leg (pure 4 KiB
fault cost, no THP confound); two full runs.

| Constant | Value | Citation |
|---|---|---|
| Fresh fault cost by size (median ns/page, run 1) | 4 KiB 401, 16 KiB 348, 64 KiB 344, 256 KiB 373, 1 MiB 361, 4 MiB 361, 16 MiB 379, 64 MiB 466, 256 MiB 485, 1 GiB 505 | Z-014, tests/perf/recorded_run_z13_z014.txt |
| Fault-cost shape | ~345-380 ns/page up to 16 MiB, rising to ~466-505 ns/page at 64 MiB..1 GiB | Z-014, tests/perf/recorded_run_z13_z014.txt |
| Minor faults per page | 1.000 in every leg and size (full re-fault always) | Z-014, tests/perf/recorded_run_z13_z014.txt |
| Recycled-vs-fresh ratio, headline | recyc_map 0.999, recyc_keep 0.979 (median across sizes, run 1); run 2: 0.988 / 0.985 | Z-014, tests/perf/recorded_run_z13_z014.txt |
| Recycled-vs-fresh at >=16 MiB | recyc_map 0.999, recyc_keep 0.988 (run 1); run 2: 0.999 / 0.997 - inside the 5% noise bound | Z-014, tests/perf/recorded_run_z13_z014.txt |
| Recycling verdict | Recycling does NOT make faults cheaper at large sizes on this machine; no fault-cost saving may be booked for pooled mappings | Z-014, tests/perf/recorded_run_z13_z014.txt |
| Mid-size recycled discount (only real discount) | 0.89-0.91x at 256 KiB and ~0.96x at 1 MiB, both runs (the run itself states 0.91x run 1 and 0.89x run 2 at 256 KiB, ~0.96x at 1 MiB) | Z-014, tests/perf/recorded_run_z13_z014.txt |
| munmap cost at large sizes | ~104 ns/page at 1 GiB (27.3 ms total), ~78 ns/page at 256 MiB | Z-014, tests/perf/recorded_run_z13_z014.txt |
| MADV_DONTNEED cost | 69 ns/page at 64 MiB, 74 ns/page at 256 MiB, 103 ns/page at 1 GiB | Z-014, tests/perf/recorded_run_z13_z014.txt |
| mmap cost | ~0.5-6.5 us at all sizes (noise range) | Z-014, tests/perf/recorded_run_z13_z014.txt |
| Pooling verdict | Pooling is worth it for syscall/VMA economics only (Z-015 territory), never for fault savings | Z-014, tests/perf/recorded_run_z13_z014.txt |

## 8. Syscall and VMA constants (Z-015)

Source: team-01 commit 211d4e5, tests/perf/recorded_run_z015.txt
(landed during the Z-018 build). Method: Z-004 protocol adapted
to a syscall microbench - pinned CPU 2, every op timed
individually, reps 101/31/15 by size, four process runs in two
invocations; untouched leg separates pure syscall cost from
fault cost; VMA counts from /proc/self/maps inside the probe.
Load honesty: CONTENDED per protocol section 10 (load
1.26-1.74; Tensile lab plus sibling-team probes) - the
microsecond medians are indicative under that stated load,
NOT quiet-machine baselines; spreads beyond the 5% noise
bound are reported as spreads. VMA counts are deterministic
and identical in all four runs.

| Constant | Value | Citation |
|---|---|---|
| Untouched mmap cost | FLAT ~0.38-0.54 us (mins 0.37-0.43 us) from 4 KiB to 64 MiB - reserving address space costs the same at any size when nothing is faulted | Z-015, tests/perf/recorded_run_z015.txt |
| Untouched munmap cost | sub-microsecond and size-flat (medians 0.36-0.89 us, jitter-dominated under load; mins 0.35-0.66 us) - no per-page term for untouched space | Z-015, tests/perf/recorded_run_z015.txt |
| First-touch fault cost (Z-015 leg) | 0.33-0.41 us/page: ~333 ns/page at 256 KiB, ~353 at 1 MiB, ~367 at 4 MiB, ~372-409 at 16 MiB - mildly rising with size; cross-checks the Z-014 curve (section 7) | Z-015, tests/perf/recorded_run_z015.txt |
| Populated teardown (touched munmap) | ~128 ns/page at 64 KiB, falling to a flat ~63-65 ns/page from 1 MiB up - releasing populated memory pays per resident page | Z-015, tests/perf/recorded_run_z015.txt |
| mremap resize (untouched range) | grow ~0.95-2.0 us, shrink ~0.61-0.65 us, FLAT - pure page-table work independent of size; a grow does NOT fault the new tail | Z-015, tests/perf/recorded_run_z015.txt |
| Fault cost under contention | roughly 3x at 16 MiB under the recorded invocation-1 contention (medians 4.47-4.53 ms vs 1.53-1.68 ms uncontended-regime) - contention multiplies fault cost, kept as evidence, never averaged away | Z-015, tests/perf/recorded_run_z015.txt |
| VMA split rule | interior unmap (hole punch) SPLITS: exactly +1 VMA per hole, fragments linearly (16 holes -> 17 VMAs); edge unmap NEVER splits (edge shrink, no new VMA) | Z-015, tests/perf/recorded_run_z015.txt |
| VMA merge rule | exact-adjacent mappings with identical flags MERGE (count unchanged); holes never merge - every surviving gapped piece stays its own VMA; counts always return to base, no VMA leaks | Z-015, tests/perf/recorded_run_z015.txt |
| VMA ceiling | vm.max_map_count = 1048576 is NOT the binding constraint on this host - VMA walk cost and fragmentation are | Z-015, tests/perf/recorded_run_z015.txt |
| Trim design rules of record | reserve big, fault lazily (reservation is flat ~0.4 us at any size); prefer trimming cold/untouched tails (flat munmap) over populated pages (~64 ns/page teardown); prefer mremap shrink (~0.6 us flat) over unmap+remap when an arena must change size | Z-015, tests/perf/recorded_run_z015.txt |

## 9. False-sharing constants (Z-016)

Source: team-02 commit 4fdc2a9, tests/perf/recorded_run_z016.txt
(landed during the Z-018 build). Method: N threads x 2,000,000
relaxed atomic increments on private counters; only the counter
layout varies (same-line stride 8 B, neighbour stride 64 B,
padded 128 B, padded 256 B); 5 reps, median; Z-004 pinning.
Load honesty: both final runs were CONTENDED (a Tensile
CreateLibrary build ramping throughout; load 3.99-5.11 primary)
- absolute ns/op are indicative, NOT quiet-machine baselines.
The constants of record are the same-line penalty RATIOS at
fixed thread count, which the run proves load-stable (same-line
cells agree within 2.7% against quieter history runs).

| Constant | Value | Citation |
|---|---|---|
| Same-line penalty vs one-counter-per-line (aggregate throughput) | 2.86x at 2 threads, 5.98x at 4, 10.72x at 8, 11.12x at 16, 7.45x at 32 (9.5x at 32 in the quiet history run) | Z-016, tests/perf/recorded_run_z016.txt |
| Same-line cost shape | packing 8 counters into one 64 B line costs ~2.9x at 2 writers, ~6x at 4, ~11x at 8-16 writers; per-line contention saturates at 8 writers - writers-per-line is what matters, not total thread count | Z-016, tests/perf/recorded_run_z016.txt |
| Padding verdict | one counter per 64 B line captures the ENTIRE win: neighbour (64 B), padded128 and padded256 are within the 5% noise bound of each other at 8/16/32 threads; guard lines beyond line-per-counter buy nothing | Z-016, tests/perf/recorded_run_z016.txt |
| MIN_HOT_COUNTER_STRIDE | 64 B - one independently-written counter, stat, queue head or flag per 64 B cache line; no two hot writers share a line | Z-016, tests/perf/recorded_run_z016.txt |
| GUARD_LINES_NEEDED | 0 - pad to the line, not beyond it | Z-016, tests/perf/recorded_run_z016.txt |
| PADDING_RULE of record | every shared structure field written by more than one thread gets its own 64 B line (align + pad to 64 B); read-mostly and single-writer fields are exempt (applied per Z-040) | Z-016, tests/perf/recorded_run_z016.txt |

## 10. Alignment constants (Z-017)

Source: team-03 commit 074ce43, tests/perf/recorded_run_z13_z017.txt
(landed during the Z-018 build). Method: all 20 small size classes
(16..1024), 8,192 returned pointers per class per leg, 3 executions
with bucket-identical totals (plus a fourth under a different load
state); address distributions only, no timing - the run was taken
under Tensile contention (load 6.99-7.71, ~760% CPU bench), which
the run proves cannot shift these distributions.

| Constant | Value | Citation |
|---|---|---|
| Alignment contract | sub-16-aligned pointers: 0 in BOTH allocators, every class, every execution (0 of 163,840 blocks per leg); the 16-byte guarantee holds everywhere on this host | Z-017, tests/perf/recorded_run_z13_z017.txt |
| Minimum alignment per class | 16 or 32 bytes for both MimirAlloc and glibc | Z-017, tests/perf/recorded_run_z13_z017.txt |
| Cacheline split, graded classes (req <= 64) - MimirAlloc | 16 B: 0.00%; 32 B: 0.00% (offsets 0/32 only); 48 B: 49.41% (4048/8192); 64 B: 100.00% (every pointer at ptr mod 64 = 32, 64-byte-aligned starts 0.00%) | Z-017, tests/perf/recorded_run_z13_z017.txt |
| Cacheline split, graded classes - glibc | 16 B: 0.00%; 32 B: 25.00%; 48 B: 100.00% (every pointer at offset 32); 64 B: 75.00% | Z-017, tests/perf/recorded_run_z13_z017.txt |
| Carve geometry of record | every MimirAlloc class that is a multiple of 64, from 64 B to 1024 B (12 classes), returns pointers EXCLUSIVELY at ptr mod 64 = 32 - zero cacheline-aligned starts; block starts rest half a line into a cacheline | Z-017, tests/perf/recorded_run_z13_z017.txt |
| Named for Wave 4 | MimirAlloc classes 48 B (49.41% split) and 64 B (100% split, fully avoidable by 64-byte-aligning the class); classes 16 B and 32 B are clean and not named; classes above 64 B span lines by size, not by misalignment, and are not alignment defects | Z-017, tests/perf/recorded_run_z13_z017.txt |
| glibc usable size | malloc_usable_size = request + 8 bytes in every class | Z-017, tests/perf/recorded_run_z13_z017.txt |


## 11. Open rows - OWED, NOT constants of record

Every Wave 2 source run (Z-010..Z-017) has now landed and is
folded in above - Z-015, Z-016 and Z-017 landed during this
Z-018 build itself (team-01 211d4e5, team-02 4fdc2a9,
team-03 074ce43) and were verified from disk by the team-04
lead before inclusion. One row remains OWED: it may not be
assumed by any wave until a recorded run lands and this doc
is amended with its citation. No value is estimated here.

| Row | Awaits | Citation needed |
|---|---|---|
| THP adoption at 200 MiB for pooled mappings | No landed run: Z-014 measured pure 4 KiB fault cost with MADV_NOHUGEPAGE on every leg and Z-015 never applied MADV_HUGEPAGE, so the pooled-THP measurement named by Z-013 has not been taken by any landed slice | a future recorded run |

Gate statement (roadmap Z-018 gate: constants doc complete;
no uncited constant admitted): every constant admitted above
carries its citation to a landed recorded run on this host,
verified line-by-line against the run files on disk on
2026-11-12. The one Open Row is admitted as OWED only - it
is not a constant of record and no wave may assume it. This
doc is amended, not rewritten, when that run lands.

Correctness gate for this slice: full make test on this tree,
log hidden_files/z018_make_test.log: 4,988 checks (18 smoke +
4,970 CHECKS) / 0 failed, 77 by-design skips, EXIT 0 -
identical to the Z-001 baseline.
