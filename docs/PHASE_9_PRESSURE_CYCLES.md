# Phase 9 Repeated Pressure Cycles + Responsiveness - Slice I-008

Status: complete on Linux (Raspberry Pi Mimir, aarch64,
16 KiB pages). Evidence: tests/core/test_i008.c, recorded
run tests/core/recorded_run_i008.txt (`make test-i008`,
standalone, NOT part of `make test`). Zero src/ change.

## 1. What this slice proves (exit item X9)

X9 asks for repeated pressure cycles as a designed
experiment: across a stated cycle count, retained and
live figures return to the same bound each cycle (no
ratchet), failure behaviour is stable across cycles, and
responsiveness under pressure is reported without a bound
claim. Until this slice that experiment had never been
run (PHASE_9_FOUNDATION_PLAN.md Section 2.8 / X9 row:
OWED at base 8f0e8ec).

DESIGN (stated before the run): CYCLES = 10, all inside
ONE forked child so any per-cycle ratchet would accumulate
in a single process and be seen. The child uses the I-005
form, strengthened the same way: it calibrates a
per-process RLIMIT_AS from its own measured VmSize
baseline (recorded run: baseline 2,637,824 B) plus a
headroom of exactly 128 pages (limit 4,734,976 B), and
the parent enforces a 30 s wall bound (2 ms poll
granularity, SIGKILL on expiry). Per-process limit ONLY -
no system-wide limit was ever set. Samtal
(GET http://127.0.0.1:8787/, the G-008 house form) was
checked before and after the run (HTTP 200 both times)
and sampled during the cycles window by a dedicated
sampler process (Section 4). Free memory was 13 GiB
available before and after; the run's total VA is ~2 MiB
per child, a light load by construction.

Page arithmetic (measured on this host, asserted in the
test; identical to I-005): page 16384 B; a small span
costs 1 page of VA; class 64 holds 254 objects per span
(I-006 geometry), so 1200 x 64 B publishes exactly 5
spans; a large 4096 B block costs 2 pages of VA.

ONE CYCLE (identical script every cycle): quiescent
snapshot -> allocate exactly 1200 x 64 B with patterns ->
free all (retention bound raised to 16 first, test-only
tune, the I-005 Leg D setup) -> large-fill 4096 B blocks
to the clean NULL -> pressure-point checks -> respon-
siveness probe DURING pressure -> free all large -> trim
-> quiescent snapshot. The retention figure of 5 spans is
the workload's own span count under that stated bound;
the X9 claim tested is that the figure RETURNS to it
every cycle, not that 5 is universal (at the banked
default bound of 2 the retained count would be lower by
the same mechanism I-003/I-005 recorded).

## 2. Per-cycle measured results (recorded run)

Every cycle produced the identical tuple - the drift
check in the child compares all 10 tuples field-by-field
against cycle 1 and passed:

| cycle | small ok | span dPub | retained spans/bytes | large ok before NULL | further large | trim released | patterns | overlap | probe alloc ns | health ns |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 1200 | 5 | 5 / 81920 | 61 | NULL | 5 | intact | none | 1093 | 44944 |
| 2 | 1200 | 5 | 5 / 81920 | 61 | NULL | 5 | intact | none | 592 | 41981 |
| 3 | 1200 | 5 | 5 / 81920 | 61 | NULL | 5 | intact | none | 593 | 35536 |
| 4 | 1200 | 5 | 5 / 81920 | 61 | NULL | 5 | intact | none | 648 | 34056 |
| 5 | 1200 | 5 | 5 / 81920 | 61 | NULL | 5 | intact | none | 704 | 44111 |
| 6 | 1200 | 5 | 5 / 81920 | 61 | NULL | 5 | intact | none | 685 | 52203 |
| 7 | 1200 | 5 | 5 / 81920 | 61 | NULL | 5 | intact | none | 686 | 35444 |
| 8 | 1200 | 5 | 5 / 81920 | 61 | NULL | 5 | intact | none | 593 | 31685 |
| 9 | 1200 | 5 | 5 / 81920 | 61 | NULL | 5 | intact | none | 518 | 34426 |
| 10 | 1200 | 5 | 5 / 81920 | 61 | NULL | 5 | intact | none | 629 | 28204 |

Reading the table:

- Exact counts, no drift: 1200 small successes and 61
  large successes in every cycle - 61 is exactly
  floor((128 - 5) / 2), the I-005 Leg D figure at the
  same setup, now reproduced 10 times in one process.
  A further large request at the pressure point returned
  exactly NULL in every cycle (failure behaviour stable).
- Quiescence reconciles exactly at every cycle start
  and end (20 snapshots in the recorded run): Q-MAPPED
  == 0, Q-RETAINED == 0, retained spans 0, span
  published == span retired, mapping published ==
  mapping retired (cumulative counters reach 50 spans
  and 610 mappings each way by cycle 10, always equal).
  At the retained point of each cycle Q-MAPPED is
  exactly 5 x page = 81,920 B.
- Mapping-published delta per cycle == 61 == the
  mapping-retired delta on release; span-retired delta
  == the trim count (5) exactly.
- Child totals: 213 checks, 0 failures; parent totals
  10 checks, 0 failures; child wall time approx 40 ms
  against the 30 s bound (poll granularity 2 ms, so the
  figure is approximate - INC-I008-03). Full `make test`
  exit 0; nm -D on libmimiralloc.so exactly 4; README.md
  and include/mimir_alloc.h sha256-identical to base.

## 3. Responsiveness under pressure (reported, no bound claimed)

Two readings, both taken while each cycle sat at its
pressure point (61 large blocks live, VA headroom
exhausted for large requests):

- In-process probe (every cycle, 10/10): a small 64 B
  allocation - served from a retained class-64 span,
  consuming no fresh VA - completed, its pattern
  verified, and it freed back to exactly 5 retained
  spans; and a Samtal-independent local health check
  (read + parse /proc/self/status, getpid self-match)
  completed. Times (CLOCK_MONOTONIC, recorded in the
  table): probe alloc 518-1,093 ns, health check
  28,204-52,203 ns across the 10 cycles. The only
  assertion made on these times is the stated 5 s
  process-liveness bound (the allocator and process
  stay live and answer); the times themselves are
  timing-never-an-oracle - printed, never asserted as
  a performance bound, per X9's own text.
- Household witness (Samtal): HTTP 200 before the run
  and after it; during the cycles window the sampler
  process recorded 2 samples in the recorded run, both
  HTTP 200, 0.461 ms and 4.336 ms (development trials
  of the same harness observed 3-75 samples, all 200,
  0.254-40.748 ms). No responsiveness bound is claimed
  from a shared home host - the samples witness that
  Samtal stayed up and answering while the cycles ran;
  they do not grade its latency (INC-I008-02 gives the
  sampling-density limit honestly).

## 4. Evidence class - read this before citing

The NULL at each cycle's pressure point is a REAL OS
failure (kernel refuses mmap under RLIMIT_AS), the
I-005 class, kept separate from I-004's injected
failures. The Samtal sampler is a separate forked
process using a direct persistent loopback socket
(HTTP/1.1, Content-Length framing) - it only READS
Samtal's public GET / and never touches the allocator.
The counters read (E-005 audit counters, G-003 span
counters, I-003 Q-MAPPED / Q-RETAINED snapshots) are
test-only shape-(b) surfaces reached by linking the
library objects, exactly as in I-003..I-006; the public
ABI is unchanged.

## 5. INC-I008 register

- INC-I008-01 (build hygiene, fixed in-slice): the
  test's first builds failed under -Werror on
  -Wmisleading-indentation at three sites (single-line
  if-pairs in the sampling and parsing code). Split
  into separate lines; no production code was affected
  and no check was weakened.
- INC-I008-02 (measurement-instrument limit, design
  revised in-slice): sampling Samtal DURING the cycles
  is density-limited by physics, not by the allocator:
  the whole 10-cycle window lasts only ~40-50 ms and
  one Samtal GET / takes ~0.3-57 ms on this shared
  host. Two harness forms under-sampled it during
  development - parent-polled curl (1 sample: the
  parent's own curl is slower than the window) and a
  persistent curl process (its pipe buffering held
  completed samples back; 1 usable sample). The final
  form is the direct-socket sampler process of
  Section 4 (2 samples in the recorded run, 3-75 in
  development trials, all HTTP 200). Any future slice
  needing denser during-window sampling must lengthen
  the window itself; it cannot be fixed by sampling
  harder.
- INC-I008-03 (bound granularity): the child's wall
  time is reported as APPROX 40 ms - it is accumulated
  at the parent's 2 ms poll granularity and excludes
  sampler time. It evidences completion far inside
  the 30 s bound and nothing finer.
- INC-I008-04 (probe scope, NOT probed): the
  responsiveness probe's small allocation is served
  from a retained span, so it proves the allocator
  answers and recycles under pressure without fresh
  VA. A small-class request needing a FRESH span (a
  class with no retained span) at the same pressure
  point was not probed; by the VA arithmetic it would
  return NULL exactly like the large request, but
  that is arithmetic, not a measurement, and this
  slice does not claim it.

## 6. Honest grading of X9, and what is NOT proven

X9 on its Linux scope: MET by this slice - 10 stated
cycles, retained/live figures returning to the same
bound every cycle with exact counter reconciliation,
failure behaviour identical across cycles, and
responsiveness under pressure reported with no bound
claimed. The PHASE_9_FOUNDATION_PLAN.md X9 row update
is the coordinator's wave-bank edit, not this slice's.

NOT proven, and not claimed anywhere in this slice:

- Android anything. The standing caveat (plan item
  XC) holds in full: no Android run exists, and
  nothing here claims the allocator can prevent,
  survive, or gracefully recover from an Android
  low-memory kill.
- Any latency or responsiveness BOUND - for the
  allocator, for this process, or for Samtal. All
  times are single-host observations on a shared
  home Pi, reported as timing-never-an-oracle.
- Behaviour at other configurations: one headroom
  (128 pages), one workload shape (1200 x 64 B +
  61 x 4096 B), one retention bound (16), single-
  threaded cycles. Multi-threaded pressure, other
  size mixes, and longer soaks (hundreds of cycles)
  are unmeasured.
- Process-wide memory truth (RSS, file-backed,
  driver memory): that is I-002's definitions and
  I-007's telemetry-separation scope, not this
  slice's; this slice's figures are allocator
  counters and exact VA arithmetic only.
- No allocator defect was found by this slice: every
  predicted count was met exactly in all 10 cycles.
