# Phase 11 Tail Latency, RSS/PSS and Energy — K-008

Slice K-008, Wave 11, Phase 11 (Performance research). Base
848f85b1a5557586b534d6846ee56f674f8c3d52 (Wave 11 = 6 of 9
banked). Shipped build flavour is -O2 (K-004): CFLAGS =
-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2,
read from the Makefile in the recorded run. Measurement-only
slice: no src, header or README change. This packet supplies
the Phase 11 exit inputs the roadmap names — tail latency,
RSS/PSS, energy where measurable — and discharges
INC-K007-03 (K-007 reported whole-pass medians only; per-op
p95/p99 were owed to K-008).

Evidence: tests/perf/recorded_run_k008.txt, a real run on
the Pi (aarch64, 4 cores, temp 57.6 C, throttled=0x0, free
13G available, Samtal HTTP 200 before and after). Every
number below is in that record. Harness:
tests/perf/probe_k008.c (one source, two binaries, the
bench_k002 pattern: -DLEG_GLIBC vs the banked LIBOBJS),
runner tests/perf/run_bench_k008.sh, summarizer
tests/perf/summarize_k008.py; standalone via
`make bench-k008`, NOT part of `make test`.

## 1. Method (K-001 contract)

- Distributions, not single timings: each alloc call and
  each free call is timed individually (CLOCK_MONOTONIC
  around the single call), all samples collected, sorted,
  and reported as p50/p95/p99/max (nearest-rank,
  idx = floor(p*n/100) clamped) plus mean.
- 5 fresh-process repetitions per leg, alternating leg
  order; the table reports the median across repetitions
  of each percentile, and max_max = the largest single-op
  maximum seen in any repetition.
- Patterns (K-003 catalogue / K-007 shapes, identical
  workloads both legs, checksums equal per pattern):
  inter_small = 64 B alloc/touch/free, batch 1, n=50000;
  burst512 = 64 B alloc-all/touch-all/free-all LIFO,
  batch 512, n=51200; large_inter = sizes cycling
  2048, 8192, 16384, 65536, 262144, 1048576 B,
  alloc/touch/free, n=6000. Touch = first+last byte.
  Warm-up 1/10 scale untimed (K-001 M-6).
- Timer floor: a timer_floor pattern times back-to-back
  clock_gettime pairs, n=5000. Both legs: p50 37 ns,
  p95 37 ns, p99 38 ns. Every latency below includes this
  ~37 ns floor; glibc's small-op p50s sit AT the floor,
  i.e. at/below timer resolution — read them as "<= floor",
  not as exact costs. MimirAlloc's p50s sit clearly above
  the floor, so its comparisons at p95/p99 are real.

## 2. Tail latency (ns per call, median of 5 reps)

| leg | pattern | kind | p50 | p95 | p99 | max_max | mean |
|---|---|---|---|---|---|---|---|
| glibc | inter_small | alloc | 37 | 56 | 56 | 61334 | 40 |
| glibc | inter_small | free | 37 | 56 | 56 | 96463 | 45 |
| mimiralloc | inter_small | alloc | 74 | 93 | 93 | 3263329 | 77 |
| mimiralloc | inter_small | free | 92 | 93 | 93 | 29055 | 88 |
| glibc | burst512 | alloc | 37 | 92 | 93 | 65667 | 46 |
| glibc | burst512 | free | 55 | 56 | 56 | 188870 | 47 |
| mimiralloc | burst512 | alloc | 74 | 92 | 93 | 446629 | 81 |
| mimiralloc | burst512 | free | 630 | 1111 | 1167 | 206592 | 655 |
| glibc | large_inter | alloc | 56 | 74 | 75 | 24537 | 62 |
| glibc | large_inter | free | 55 | 56 | 56 | 889 | 52 |
| mimiralloc | large_inter | alloc | 2519 | 2944 | 3852 | 1163146 | 2787 |
| mimiralloc | large_inter | free | 2278 | 3093 | 3686 | 1326683 | 2400 |

Findings:
- F-1 Interleaved small (the common case): MimirAlloc p99
  93 ns vs glibc 56 ns for both alloc and free (~1.7x).
  A consistent, modest tail cost — not a blowout.
- F-2 Burst free is the worst small-size tail in the
  packet (LOSS): MimirAlloc burst512 free p50 630 ns,
  p99 1167 ns vs glibc 56 ns p99 (~21x at p99), while
  burst alloc p99 ties glibc at 93 ns. Freeing a burst
  walks the span-release path K-007 indicted (its L-3:
  pages/spans released on free are re-faulted and
  re-processed per burst cycle); the per-op view here
  confirms the cost sits in free, not alloc.
- F-3 Large path (LOSS): MimirAlloc large_inter p50
  2519 ns alloc / 2278 ns free, p99 3852 / 3686 ns, vs
  glibc p99 75 / 56 ns (~51x / ~66x at p99). Every large
  alloc/free pays microseconds of mapping-path work
  where glibc pays nanoseconds. This is the single
  largest latency gap measured in Phase 11 so far.
- F-4 Rare extreme outliers exist in both legs (inter
  alloc max_max 61334 ns glibc, 3263329 ns MimirAlloc;
  large free max_max 1326683 ns MimirAlloc). They are
  single samples in 50000-51200, consistent with
  scheduling/interrupt noise on a shared home Pi
  (Samtal live); they are reported, not attributed
  (INC-K008-03).

## 3. RSS/PSS footprint (KiB, median of 5 reps)

Measured in-process from /proc/self/status (VmRSS, VmHWM)
and /proc/self/smaps_rollup (Pss). Baseline is read in the
same process immediately before each hold; delta = hold
minus baseline; retained = after-free minus baseline.
These are PROCESS figures. The allocator-attributable
figure is stated only as delta versus requested bytes
(I-007 telemetry-separation precedent); requested bytes
are application data, not allocator overhead.

| leg | set | live | requested | dRSS | dPSS | retained RSS | retained PSS | HWM |
|---|---|---|---|---|---|---|---|---|
| glibc | small_hold | 20000x64B | 1250 | 1232 | 1232 | 1232 | 1232 | 3904 |
| mimiralloc | small_hold | 20000x64B | 1250 | 1360 | 1360 | 128 | 128 | 4016 |
| glibc | large_hold | 24 blocks | 5480 | 3920 | 3920 | -1424 | -1424 | 7824 |
| mimiralloc | large_hold | 24 blocks | 5480 | 5952 | 5952 | 0 | 0 | 8736 |

- Small hold: MimirAlloc delta 1360 KiB for 1250 KiB
  requested = +110 KiB over request (metadata/span
  granularity). glibc delta 1232 KiB, within 18 KiB of
  requested. After free, MimirAlloc returns to within
  128 KiB of baseline; glibc retains the full 1232 KiB
  in its arena (retained is not a defect per se — it is
  reuse-ready memory — but it is footprint a caller
  still pays).
- Large hold: MimirAlloc delta 5952 KiB = requested
  5480 + 472 KiB; after free it returns exactly to its
  (already small-hold-elevated) baseline, retained 0.
  glibc's large delta (3920 KiB) is BELOW requested:
  its baseline already included the 1232 KiB retained
  small-hold arena, and releasing the large set also
  trimmed that arena (retained -1424 KiB). So the two
  legs' large deltas are marginal growth over different
  baselines, NOT like-for-like absolute footprints;
  per-rep baselines are printed in the recorded run's
  raw MEM lines (INC-K008-05).
- PSS equals RSS in every delta here (private anonymous
  pages, no sharing), as expected for a single process.

## 4. Energy — MEASURABLE at whole-board level only

Checks run on the Pi (in the recorded run): no
/sys/class/powercap (no RAPL-style energy counter), no
/sys/bus/iio/devices (no INA-class sensor). A real source
DOES exist: the Raspberry Pi 5 PMIC is readable via
`vcgencmd pmic_read_adc` (per-rail current + voltage).
The runner samples it ~every 100 ms during ~6 s sustained
inter_small runs and sums V*A over every rail reporting
both a current and a voltage of the same name. That is a
WHOLE-BOARD figure — Samtal, the OS and all peripherals
included — never an allocator-only measurement.
2 passes per condition, medians:

| condition | mean power (W) | energy per ~6 s pass (J) | ops per pass |
|---|---|---|---|
| idle (sleep 6) | 3.037 | 18.35 | — |
| glibc | 3.926 | 23.77 | 106325003 / 107237615 |
| mimiralloc | 4.066 | 24.63 | 48105859 / 48497258 |

Above-idle mean power: glibc +0.889 W, MimirAlloc
+1.029 W. DERIVED whole-board energy per alloc/free
op (pass energy / ops, labelled derived, not measured
per-op): glibc ~0.22 uJ/op, MimirAlloc ~0.51 uJ/op —
the expected consequence of F-1/F-3: MimirAlloc does
about half the ops in the same window at similar board
power, so each op carries about 2.3x the board energy.
No finer energy attribution is possible on this Pi
(INC-K008-04).

## 5. INC-K008 register

- INC-K008-01 (burst free tail): burst512 free p99
  1167 ns vs alloc p99 93 ns in the same leg — the
  span-release-on-burst-free path (K-007 L-3) is the
  prime suspect. Any fix must re-run this probe and
  show burst free p99 moving without inter_small
  regressing.
- INC-K008-02 (large-path cost): large alloc/free p50
  ~2.5 us / ~2.3 us vs glibc ~56 ns — per-op
  mapping-path cost. Profiling the large path is the
  highest-value latency work this packet identifies.
- INC-K008-03 (extreme outliers unattributed): max
  samples up to 3263329 ns occur in both legs' tails;
  no isolation (CPU pinning / IRQ shielding) was
  attempted on the shared home Pi, so no cause is
  claimed.
- INC-K008-04 (energy attribution limit): energy is
  measurable only as whole-board PMIC power; no
  per-process or per-allocator energy counter exists
  on this Pi (powercap and IIO both absent, checks
  recorded). Per-op energy here is DERIVED only.
- INC-K008-05 (footprint method limit): footprint
  deltas are sequential in-process marginals over
  allocator-specific baselines (see Section 3 large
  hold); a fresh-process-per-set variant would give
  cleaner absolute footprints if a future slice needs
  them.
- DISCHARGED: INC-K007-03 — per-op p50/p95/p99/max
  for the K-007 patterns are in Section 2.

## 6. Reproduction

make bench-k008  (5 reps + energy phase; ~40 s on the Pi)
make test-k008   (1-rep smoke form)
make test        (4590 checks, 0 failed at this slice —
see recorded_run_k008.txt verification section)
