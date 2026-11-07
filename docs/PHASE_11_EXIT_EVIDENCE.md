# Phase 11 Exit Evidence Packet (Slice K-009)

Slice: **K-009 (team-09)**. Wave 11, Phase 11 (Performance
research). Deliverables: this packet and the fresh recorded
run `tests/perf/recorded_run_k009.txt` (742 lines, sha256
899e1bcf96a0c6089e11bafe9db9dfb3113c2e25cb21d18447d1ac836c379082).
Doc + recorded run ONLY: no `src/`, `Makefile`, `README.md`
or `include/mimir_alloc.h` change. Base:
`254ae72e7b257fe84afcd331f81207595e143a48` (Wave 11 = 7 of 9
banked) on branch `yrsa/mimiralloc-09`, worktree
`/home/pi/mimiralloc-team-09` on Raspberry Pi `purpleviking1`.

> **HEADLINE (plain first). PHASE 11 EXIT = PARTIAL.**
> Phase 11 produced a slower-but-measured allocator with
> every loss recorded. The banked MimirAlloc loses to the
> host glibc allocator on every workload measured in this
> phase (K-002 baseline: median factors about 5x to about
> 65x), and no slice in this wave found a workload where it
> wins. What Phase 11 did deliver is the evidence itself:
> a reproducible baseline, one profile-motivated shipped
> change (K-004 `-O2`), honest no-change and all-loss
> verdicts where the measurements said so (K-005, K-007),
> the scaling loss case quantified (K-006), and tails,
> footprint and whole-board energy measured (K-008,
> banking-parallel). No win is claimed anywhere in this
> packet that the banked evidence does not support.

Governing texts: roadmap Phase 11 exit sentence —
*publish reproducible results, loss cases, tail latency,
RSS/PSS, energy where measurable, and throughput with
confidence intervals or repeat distributions* — graded
through the exit items X-K1..X-K8 defined in
`docs/PHASE_11_FOUNDATION_PLAN.md` (K-001, Section 7),
plus the phase body rules: *profile before adding
complexity* and *a microbenchmark win is not a universal
win*. Precedent followed: H-009 / I-009 / J-009 (honest
MET / PARTIAL / UNMET grading; banking-parallel evidence
named as such, never as a run on this base).

Evidence map (all banked at this base unless marked):
K-001 methodology + foundation plan
(`docs/PHASE_11_FOUNDATION_PLAN.md`); K-002 baseline
(`docs/PHASE_11_BASELINE_BENCHMARKS.md`,
`tests/perf/recorded_run_k002.txt`); K-003 workload traces
(`docs/PHASE_11_WORKLOAD_TRACES.md`); K-004 `-O2` shipped
change (`docs/PHASE_11_PERFORMANCE_K004.md`,
`tests/core/recorded_run_k004.txt`); K-005 locality
no-change verdict (`docs/PHASE_11_LOCALITY_METADATA.md`,
`tests/perf/recorded_run_k005.txt`); K-006 scaling /
false-sharing (`docs/PHASE_11_FALSE_SHARING_SCALING.md`,
`tests/perf/recorded_run_k006.txt`); K-007 batching
(`docs/PHASE_11_BATCHING_K007.md`,
`tests/perf/recorded_run_k007.txt`); K-008 tails /
footprint / energy — **banking-parallel**: cited from its
commit `bbdc90e` on branch `yrsa/mimiralloc-08` (parent
`848f85b`), keeper-verified figures, NOT re-run on this
base and NOT banked at this base.

## 1. Fresh verification run (this base, this slice)

Recorded in `tests/perf/recorded_run_k009.txt`, run on
the Pi 2026-11-07 at base `254ae72`: `make clean && make
test` exit 0 (4590 checks, 0 failed); `make bench-k002`,
`make bench-k004`, `make bench-k005`, `make bench-k006`
and `make bench-k007` each exit 0; `nm -D
--defined-only` on `build/libmimiralloc.so` = exactly 4
(mimir_malloc, mimir_free, mimir_calloc, mimir_realloc).
`README.md`, `include/mimir_alloc.h` and `Makefile`
byte-identical to base (sha256-verified in the recorded
run: README `bd7f6614`, header `55107f05`, Makefile
`10bd916f`, worktree == HEAD for all three). Samtal HTTP
200 before/after on port 8787; free 13 GiB available
before/after. One blemish is recorded in the run itself,
not hidden: the first `nm` attempt used a wrong filename
and failed; the re-run against the built library follows
it in the same file (see INC-K009-01).

## 2. Grading of the Phase 11 exit items (X-K1 to X-K8)

Items are quoted in substance from
`docs/PHASE_11_FOUNDATION_PLAN.md` (K-001, Section 7).
Each grade rests on banked evidence only. K-008 is
cited as banking-parallel throughout: it was banked
after this packet's base as commit `c7e148a` on main
(Wave 11 = 8 of 9 at the time of writing), and its
figures here are the keeper-verified figures from
its banked packet
`docs/PHASE_11_TAIL_RSS_ENERGY_K008.md`, not a re-run
on this base.

- **X-K1 Reproducible results — MET.** The baseline
  and every optimisation claim reproduce from
  committed harnesses and recorded runs on this
  device, under the K-001 method. K-002's baseline
  harness and recorded run, K-004's before/after
  legs, and the K-005, K-006 and K-007 bench
  harnesses are all committed with recorded runs.
  This slice's fresh run (Section 1) re-ran
  `make test` (4590 checks, 0 failed) and the
  bench-k002, bench-k004, bench-k005, bench-k006
  and bench-k007 targets, each exit 0, at this base.

- **X-K2 Loss cases published — MET.** Every
  workload where MimirAlloc loses is recorded with
  figures. K-002's all-loss baseline: median factors
  about 5x to about 65x against glibc across its
  workloads (large batch about 5.4x; mixed lifecycle
  about 65.2x). No later slice hid a loss: K-005
  published a no-change verdict, K-006 quantified
  the scaling loss, K-007 published batching losses
  on every pattern, and K-008 published the tail,
  footprint and energy losses in Section 2 below.

- **X-K3 Tail latency — MET (K-008,
  banking-parallel).** Per-op p50/p95/p99/max from
  the K-008 instrument, median of 5 fresh-process
  repetitions: burst512 free p99 for MimirAlloc
  1167 ns against glibc 56 ns; large-path alloc
  p99 3852 ns and free p99 3686 ns (about 3.8 and
  3.7 microseconds) against glibc 75 ns and 56 ns;
  interleaved small-op p99 93 ns against glibc
  56 ns. Extreme outliers are reported, not
  attributed (INC-K008-03). This discharges the
  per-op tail evidence K-007 owed (INC-K007-03).

- **X-K4 RSS/PSS — PARTIAL.** K-008 measured
  footprint under the same workloads, in-process
  from `/proc/self/status` and
  `/proc/self/smaps_rollup`: peak RSS (VmHWM),
  RSS and PSS deltas, and retained memory after
  freeing are all reported. Small hold: MimirAlloc
  delta RSS/PSS 1360 KiB for 1250 KiB requested,
  retained 128 KiB; glibc delta 1232 KiB, retained
  1232 KiB. Large hold: MimirAlloc delta 5952 KiB,
  retained 0; PSS equalled RSS in every delta
  (private anonymous pages). Correction to the
  pre-writing grade note: PSS was measured, not
  missing. The item stays PARTIAL because virtual
  size and mapping counts, which Section 7 also
  names, are not reported in the K-008 packet,
  and because the footprint deltas are sequential
  in-process marginals over allocator-specific
  baselines (INC-K008-05), not absolute
  fresh-process footprints.

- **X-K5 Energy where measurable — PARTIAL.**
  Energy is measurable on this Pi only as
  whole-board power through the Raspberry Pi 5
  PMIC (`vcgencmd pmic_read_adc`); no powercap or
  IIO energy counter exists (checks recorded in
  K-008). Whole-board means, median of 2 passes:
  idle 3.037 W, glibc under load 3.926 W,
  MimirAlloc under load 4.066 W. Per-op figures
  (about 0.22 microjoules per op for glibc,
  about 0.51 for MimirAlloc) are DERIVED from
  pass energy divided by ops and are labelled
  derived in K-008, never presented as measured
  per-op energy. No allocator-isolated energy
  measurement exists on this device
  (INC-K008-04), so the item is PARTIAL, not MET.

- **X-K6 Throughput with distributions — MET.**
  Throughput figures carry repeat distributions,
  never a single timing: K-002 reports medians
  of 7 repetitions with min/max spreads; K-004
  reports medians with ranges; K-006 reports
  median wall throughput across thread counts
  (MimirAlloc flat at about 0.8 M ops/s: 0.96,
  0.76, 0.79, 0.82 at 1/2/4/8 threads, against
  glibc rising 2.90 to 7.65 M ops/s); K-008
  reports the median across 5 repetitions of
  each per-op percentile.

- **X-K7 Profile-before-complexity — MET.** The
  one landed optimisation names its profile and
  its before/after measurement: K-004 shipped
  the `-O2` CFLAGS change on profile evidence,
  small64 168 to 86 ns/op, burst256 1488 to
  661 ns/op, large4096 flat. Where the profile
  gave no case for a change, no change was made:
  K-005 locality/metadata returned an honest
  NO-CHANGE verdict, and K-007 recorded its
  batching proposal P-1 without implementing it
  (see the carried-owed register). No rseq or
  per-CPU design was landed in this phase, so
  no unproven gate claim arises.

- **X-K8 No universal claims — MET.** No result
  in the phase packets is stated beyond its
  workloads, this device and its conditions.
  K-002's all-loss factors, K-006's scaling
  shape (per-thread cost rising about 6.2x from
  1 to 8 threads, matching the H-002 shape, with
  wall throughput flat — the signature of the
  one global lock serialising the allocator),
  and K-007's batching losses (64 B burst512
  7.01x slower than interleaved; FIFO-512 6.57x
  at 1024 B) are each tied to their harness and
  workloads. The shipped-build caveat is
  retired honestly: K-004 landed the optimised
  (`-O2`) leg, and K-005's `-O0`-base figures
  are carried in its own packet with that
  caveat stated, not silently generalised.

### Tally for Section 2

| Item | Grade |
|---|---|
| X-K1 Reproducible results | MET |
| X-K2 Loss cases published | MET |
| X-K3 Tail latency | MET |
| X-K4 RSS/PSS | PARTIAL |
| X-K5 Energy where measurable | PARTIAL |
| X-K6 Throughput with distributions | MET |
| X-K7 Profile-before-complexity | MET |
| X-K8 No universal claims | MET |

**6 MET / 2 PARTIAL / 0 UNMET.** Two PARTIAL
items, each with a named instrument or method
limit rather than missing work, are why the
overall verdict is **PHASE 11 EXIT = PARTIAL**,
matching the headline of this packet.

## 3. Carried-owed register

Nothing below is closed by this packet. Each
item is carried forward explicitly so Phase 11
does not pretend it closed:

- **Phase 3 exit — UNMET.** The x86-64 suite
  run owed in full since Phase 3 remains owed
  (INC-K001-01). No Phase 11 figure may be
  stated for x86-64.
- **Phase 4 exit — UNMET.** The physical
  Android device run owed since Phase 4 remains
  owed (INC-K001-02); the benchmark classes
  blocked on it are recorded as owed, not
  simulated on the Pi.
- **INC-I001-14.** Budget enforcement was not
  adopted in Phase 9 and remains not adopted;
  no Phase 11 pressure claim assumes it.
- **INC-I009-01.** `docs/SUPPORTED_TARGETS.md`
  freshness remains owed outside this phase.
- **K-007 proposal P-1 — recorded, not
  implemented.** Span retention as the candidate
  answer to burst re-faulting (INC-K007-01) and
  the FIFO free-order penalty (INC-K007-02)
  stay proposals. K-007's own evidence is that
  batching loses everywhere as built; P-1 may
  only be landed by a future slice that
  re-measures against it.
- **INC-K008-01.** Burst free tail: burst512
  free p99 1167 ns against alloc p99 93 ns in
  the same leg; the span-release-on-burst-free
  path is the prime suspect. A fix must re-run
  the K-008 probe and show the free p99 moving
  without interleaved small ops regressing.
- **INC-K008-02.** Large-path cost: large
  alloc/free p50 about 2.5 / 2.3 microseconds
  against glibc about 56 ns. Profiling the
  large path is the highest-value latency work
  this phase identifies.
- **INC-K008-03.** Extreme outliers (single
  samples up to 3263329 ns) are unattributed;
  no isolation was attempted on the shared
  home Pi, so no cause is claimed.
- **INC-K008-04.** Energy attribution limit:
  whole-board PMIC power only; per-op energy
  is derived, never measured (see X-K5).
- **INC-K008-05.** Footprint method limit:
  sequential in-process marginal deltas over
  allocator-specific baselines (see X-K4).
- **INC-K009-01 (new).** This slice's recorded
  run contains one blemish, recorded in-file
  and corrected in-file: the first `nm`
  attempt used a wrong filename and failed;
  the re-run against `build/libmimiralloc.so`
  follows it in the same file and shows
  exactly the 4 exported symbols. The blemish
  is kept in the record, not edited out.

## 4. Closing tally and what Phase 12 would need

Phase 11 exit: **PARTIAL — 6 MET / 2 PARTIAL /
0 UNMET across X-K1 to X-K8.** The phase met
its evidence purpose: reproducible results,
published loss cases, tail latency, throughput
with distributions, and profile-before-
complexity discipline are all in the banked
record. It did not produce a faster allocator:
MimirAlloc loses to glibc on every workload
measured in this phase, and this packet claims
no win the banked evidence does not support.

What Phase 12 would need, stated only as
recorded-owed pointers, with no new claims:

- An allocator-isolated energy instrument, or
  a fresh-process-per-set footprint method
  reporting virtual size and mapping counts,
  to lift X-K4 and X-K5 from PARTIAL (owed:
  INC-K008-04, INC-K008-05).
- A profiled answer to the two largest
  measured costs — the burst-free span-release
  path and the large mapping path — each
  re-measured with the K-008 per-op probe
  (owed: INC-K008-01, INC-K008-02; candidate
  recorded: K-007 proposal P-1).
- The still-owed platform evidence carried
  since earlier phases: the x86-64 suite run
  in full (Phase 3 exit) and the physical
  Android run (Phase 4 exit). Phase 12
  planning owns whether and when those are
  scheduled; this packet only records that
  they remain owed.

## 5. Reproduction

```
make test          # 4590 checks, 0 failed
make bench-k002    # K-002 baseline harness
make bench-k004    # K-004 -O2 before/after legs
make bench-k005    # K-005 locality legs
make bench-k006    # K-006 scaling legs
make bench-k007    # K-007 batching legs
make bench-k008    # K-008 per-op tails, footprint
                   # and energy (banking-parallel
                   # slice; on main from c7e148a)
```

Fresh run for this packet:
`tests/perf/recorded_run_k009.txt` (742 lines,
sha256
899e1bcf96a0c6089e11bafe9db9dfb3113c2e25cb21d18447d1ac836c379082).
Packet files only: this document and that
recorded run. No `src/`, `Makefile`,
`README.md` or `include/mimir_alloc.h` change;
all three named files verified byte-identical
to base `254ae72` in Section 1.
