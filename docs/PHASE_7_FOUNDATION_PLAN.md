# Phase 7 - Foundation Plan: Mobile-Friendly Small Objects

Slice: **G-001 (team-01)**. Wave 7, Phase 7 foundation plan +
measured Phase 3 baseline.
Deliverables: `docs/PHASE_7_FOUNDATION_PLAN.md` (this file),
`tests/perf/probe_g001.c` (micro-probe source),
`tests/perf/recorded_run_g001.txt` (recorded runs), plus an
appended standalone `make test-g001` target (Makefile +21 lines,
insertions only; `make test` composition unchanged).
Base: `0372d94` (F-009 banked; Wave 6 COMPLETE 9 of 9) on branch
`yrsa/mimiralloc-01`, worktree `/home/pi/mimiralloc-team-01` on
Raspberry Pi `purpleviking1`. Governing text: Roadmap Section 8
Phase 7 ("Mobile-friendly small objects"), read with the banked
`docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md` (B-006),
`docs/PHASE_3_EXIT_EVIDENCE.md` (C-009 packet) and
`docs/PHASE_6_EXIT_EVIDENCE.md` (F-009). Wave 7 slice queue:
WAVE_PLAN.md in the coordinator checkout `~/mimiralloc-forge`
(G-001..G-009), read by this team on 2026-11-06. `README.md`
byte-untouched; public header and the 4-symbol ABI unchanged.
Work style: solo sequential work, no sub-agents. Pi load stayed
light; Samtal lives on this Pi and was not starved (`free -g`:
15 GiB total, 13 GiB available, before and after the runs).

> **PHASE 7 IS NOT EXITED, AND THIS PLAN EXITS NOTHING.**
> The Roadmap Phase 7 exit sentence (Section 8, Phase 7) is,
> verbatim: *"no regression in correctness; documented
> allocation latency, retained memory and responsiveness
> versus Phase 3 and the platform allocator."* At this base
> **no** Phase 7 design element exists yet: there are no size
> classes, no local reuse, no reclaim policy, no thread-exit
> handling and no tuning parameters in the banked allocator.
> What exists is the Phase 3 mapping-per-allocation design,
> whose measured baseline this slice records (Section 3) so
> that every later Wave 7 comparison has a banked "before".
>
> **CARRIED DEBT, STATED BEFORE ANYTHING ELSE:** Phase 6 exit
> is MET via the blocker-record alternative only; Clause A
> stays UNMET and X1 stays BLOCKED (F-009). Phase 3 exit and
> Phase 4 exit remain **UNMET** in the banked record (the
> x86-64 suite leg and the physical Android run are still
> OWED). Phase 1 exit also remains unmet. Nothing in this
> plan or in Wave 7 discharges any of that.

## 1. What this plan does

1. Records a **measured baseline** of the current banked
   allocator (the Phase 3 mapping-per-allocation design) on
   the Pi: allocation latency distribution for representative
   small sizes and retained memory after allocate/free cycles,
   from a small bounded micro-probe actually run and logged
   on the Pi (Section 3; `tests/perf/recorded_run_g001.txt`).
2. Maps every Phase 7 element to the Wave 7 slice
   (G-001..G-009) that will build or prove it (Section 4).
3. Gives the exit-evidence matrix for the Phase 7 exit
   sentence (Section 5).
4. Hands G-002 the measured size-class design inputs
   (Section 6).
5. Records every open question and inconsistency found as
   INC-G001-NN (Section 7). Claim labels throughout:
   **PROVEN on the Pi** (run or inspected by this slice),
   **documented** (a banked source says so; not re-run here),
   **OWED** (absent; named owner), or **BLOCKED**.

## 2. Evidence actually checked on the Pi in this slice

All checks were run by this team on 2026-11-06 (23:2x EST) in
`/home/pi/mimiralloc-team-01`, sequentially, load kept light.
Nothing was installed.

- Host: `uname -a` = `Linux purpleviking1
  6.12.112-v8-16k-hailo+ #1 SMP PREEMPT Sat Oct 10 09:05:25
  EDT 2026 aarch64 GNU/Linux`; `getconf PAGESIZE` = `16384`;
  `cc --version` = `cc (Debian 12.2.0-14+deb12u1) 12.2.0`;
  `free -g` = 15 GiB total, 13 GiB available (before and
  after the probe runs; recorded in the run file).
- `git log --oneline -1` at slice start = `0372d94 F-009:
  Phase 6 exit evidence packet ...`; branch
  `yrsa/mimiralloc-01`; worktree clean except the untracked
  `hidden_files/` assignment directory (standing keeper
  pattern).
- Allocator shape (inspected, **PROVEN on the Pi**):
  `src/platform/mimir_platform.c` performs one `mmap` per
  allocation through the L6 platform layer; the core
  (`src/core/mimir_core.c`) keeps per-allocation metadata and
  a simple global lock (design: B-006, **documented**). The
  public surface is exactly the four symbols declared in
  `include/mimir_alloc.h`: `mimir_malloc`, `mimir_free`,
  `mimir_calloc`, `mimir_realloc`.

## 3. Measured Phase 3 baseline (micro-probe, not a benchmark suite)

Probe: `tests/perf/probe_g001.c`, built by the appended
`make test-g001` wiring (library objects + probe object,
`-std=c11 -Wall -Wextra -Werror -pthread`). Timing method:
per-op `clock_gettime(CLOCK_MONOTONIC)` around one
allocate + touch-first-byte + free triple; 2000 ops per size;
samples sorted, min/p50/p90/p99/max/mean printed. Retained
memory: `/proc/self/status` VmRSS read before a 256-block
batch, while live and fully touched, and after freeing the
batch; plus a 10-cycle churn of the same batch shape.
Bounds: at most 256 live blocks, under 100k ops total, each
run well under a second. Runs: **3 consecutive mimir runs +
1 companion libc run** (identical probe shape against the
platform allocator), all in
`tests/perf/recorded_run_g001.txt`, all exit 0, all
`fails=0`. The numbers below are quoted exactly as recorded.

### 3.1 Latency - Phase 3 allocator (mimir leg)

Across the 3 runs, for every probed size 8, 16, 32, 64, 128,
256, 512, 1024 and 4096 bytes:

- **p50 between 4889 ns and 5167 ns** (run 1 range across
  sizes: 4889-5018 ns; run 2: 4926-5037 ns; run 3:
  5000-5167 ns). Latency is **size-independent** within
  noise over 8..4096 bytes - the cost is the mapping
  syscalls and the lock, not the request size.
- p90 between 4982 ns and 5703 ns; p99 between 5278 ns and
  6907 ns in the body of the distribution, with rare
  scheduler outliers (single-sample maxima up to 858221 ns,
  run 2 size 128) on this shared, Samtal-carrying Pi. Tail
  claims beyond p99 are **not** made from this probe.
- Mean per size per run: 4966-6103 ns.

### 3.2 Retained memory - Phase 3 allocator (mimir leg)

Identical in all 3 runs, for batch sizes 16, 64, 256 and
1024 bytes x 256 blocks:

- `rss_before_kb=1408`, `rss_live_kb=9600`,
  **`live_delta_kb=8192` for every size**,
  `rss_after_free_kb=1408`, **`retained_after_free_kb=0`**.
- 8192 KiB / 256 blocks = **32 KiB mapped footprint per
  allocation** on this 16384-byte-page host (two system
  pages per allocation: metadata + user mapping, per the
  B-006 design shape). A 16-byte request and a 1024-byte
  request cost the process exactly the same live RSS.
- Churn (256 x 64 bytes, 10 cycles): RSS after every cycle
  exactly 1408 KiB - the mapping-per-allocation design
  retains **nothing** after free and does not grow under
  churn. That zero-retention property is the baseline any
  Phase 7 caching/reuse design must be judged against: it
  is what local reuse will deliberately trade away, in
  bounded amounts, for latency.

### 3.3 Companion leg - platform allocator, identical shape

One libc run, same probe, same bounds (recorded in full in
the run file):

- Latency p50 = **56 ns** for sizes 8..1024 (74 ns at 4096);
  mean 58-80 ns. Ratio at p50 versus Section 3.1: the Phase 3
  allocator's allocate/free pair is roughly **two orders of
  magnitude slower** (~5000 ns vs ~56 ns) - the expected,
  documented cost of a correctness-first
  mapping-per-allocation prototype (Roadmap Phase 3: "do not
  ship it as a performance allocator").
- Retention: 256-block live deltas 0 KiB (16 B), 16 KiB
  (64 B), 64 KiB (256 B), 160 KiB (1024 B);
  retained-after-free equals the live delta in each case
  (arena retention: 0 / 16 / 64 / 160 KiB) - the platform
  allocator keeps freed small-object memory in-process,
  where the Phase 3 design returns all of it to the OS.

This companion leg is a **first same-shape reading**, not
the exit-grade platform comparison: the formal "versus the
platform allocator" evidence for the exit sentence is OWED
to the later Wave 7 slices under the matrix in Section 5.

## 4. Phase 7 element -> Wave 7 slice map

Queue verbatim in substance from WAVE_PLAN.md (coordinator
checkout, Wave 7 derived 2026-11-06 23:2x EST). Banked at
plan time: Wave 7 = 0 of 9.

| Phase 7 element (Roadmap S8 Phase 7) | Slice | Team | Deliverable in one line |
|---|---|---|---|
| Foundation plan + measured Phase 3 baseline + size-class design inputs | **G-001** | 01 | This plan; probe + recorded runs (Section 3) |
| Measured size classes (the class table itself) | **G-002** | 02 | Size-class table design with measured size-distribution justification |
| Bounded local reuse (+ size classes implemented) | **G-003** | 03 | Small-object path implementation (size classes + bounded local reuse) with central fallback to the Phase 3 core |
| Central fallback | **G-004** | 04 | Central fallback integration + full correctness re-run |
| Reclaim policies | **G-005** | 05 | Reclaim policy + idle-thread retained-memory measurement |
| Thread exit handling (avoid bloat when many threads idle) | **G-006** | 06 | Thread exit handling |
| Tuning parameters configurable, validated and visible | **G-007** | 07 | Tuning parameters slice |
| Device multitasking + repeated model-load/unload tests | **G-008** | 08 | Repeated load/unload + multitasking responsiveness record (bounded, on the Pi) |
| Exit assembly | **G-009** | 09 | Phase 7 exit evidence packet |

Dependency order is the queue order: G-002's class table
consumes G-001's Section 6 inputs; G-003 implements what
G-002 designs; G-005/G-006 bound the retention G-003's reuse
introduces; G-009 assembles, it does not originate, evidence.

## 5. Exit-evidence matrix

Exit sentence decomposed; every element names its proving
slice and its state at this base.

| # | Exit element | Proven by | State at G-001 base |
|---|---|---|---|
| X1 | No regression in correctness | G-004 full correctness re-run; re-confirmed by G-009 | OWED. Baseline correctness = banked `make test` suite at 0372d94 (F-009 fresh run: 8/8 PASS, 4590 checks, **documented**) |
| X2 | Documented allocation latency vs Phase 3 | G-001 baseline (this slice); Phase 7 readings OWED to G-003/G-005, assembled by G-009 | Phase 3 side **PROVEN on the Pi** (Section 3.1); Phase 7 side OWED |
| X3 | Documented allocation latency vs platform allocator | G-009 from same-shape probe legs | First same-shape libc reading recorded (Section 3.3); exit-grade comparison OWED |
| X4 | Documented retained memory vs Phase 3 | G-001 baseline; G-005 idle-thread measurement | Phase 3 side **PROVEN on the Pi** (Section 3.2: 32 KiB/allocation live, 0 retained); Phase 7 side OWED |
| X5 | Documented retained memory vs platform allocator | G-009 from same-shape probe legs | First reading recorded (Section 3.3); exit-grade comparison OWED |
| X6 | Documented responsiveness (multitasking, repeated load/unload) | G-008 | OWED; the metric itself is undefined - INC-G001-03 |
| X7 | Tuning parameters configurable, validated, visible | G-007 | OWED; the configuration channel is an open design question - INC-G001-05 |

## 6. Size-class design inputs for G-002

Measured inputs only; the class table is G-002's decision,
not this slice's.

1. **The baseline waste is total and measured.** At 32 KiB
   mapped per allocation regardless of request (Section
   3.2), a 64-byte request uses 0.2% of its mapping. Any
   size-class scheme that packs small objects into shared
   mappings improves live footprint by orders of magnitude
   at these sizes; the design question is the retention
   bound, not whether packing helps.
2. **Probed request range:** 8..4096 bytes, with retention
   batches at 16/64/256/1024. G-002's distribution
   justification must name its workload (INC-G001-01); this
   probe's size list is a measurement grid, not a workload
   distribution, and must not be cited as one.
3. **Host page size is 16384 bytes on this Pi** (and 4096
   elsewhere; D-003 portability record). Class geometry
   stated in system pages must say which, per MA-6.2-041
   (INC-G001-07).
4. **Latency target shape:** the Phase 3 pair costs ~5 us
   at p50 size-independently; the platform allocator costs
   ~56 ns at p50 in the same shape. G-002/G-003 success is
   movement toward the second number without correctness
   regression (X1), not parity claims from this probe.
5. **Retention budget shape:** the baseline retains 0 KiB
   after free. Every KiB a reuse cache retains is a
   deliberate, bounded, reclaimable deviation from 0, to be
   sized by G-002/G-003 and measured idle by G-005.

## 7. Open questions and inconsistencies (INC-G001-NN)

- **INC-G001-01** - "Measured size classes" (Roadmap Phase
  7) presupposes a size distribution, but no banked document
  names the workload whose distribution justifies the
  classes. G-002 must name its workload explicitly or record
  the classes as grid-derived, not distribution-derived.
- **INC-G001-02** - The exit sentence requires comparison
  "versus Phase 3 and the platform allocator", yet at this
  base no banked recorded run measures the platform
  allocator in any shape. G-001 records the first same-shape
  libc leg (Section 3.3); the exit-grade comparison remains
  OWED to G-009's packet.
- **INC-G001-03** - "Responsiveness" in the exit sentence
  has no metric anywhere in the roadmap or banked docs (no
  frame-time, load-time or scheduling-latency definition).
  G-008 must define the metric before measuring it; until
  then X6 cannot be graded.
- **INC-G001-04** - The banked allocator has no per-thread
  state at all (one global lock, **documented** B-006/C-003
  shape). "Bounded local reuse" and "thread exit handling"
  therefore introduce a new state dimension in G-003/G-006,
  not a modification of an existing cache; designs must say
  how that state is created, bounded and destroyed.
- **INC-G001-05** - "Tuning parameters configurable,
  validated and visible" has no channel in the frozen
  4-symbol ABI and the public header may not change: build
  flags, environment, and a side channel are all still open.
  G-007 must choose one without silent ABI expansion (the
  Phase 5 hard law).
- **INC-G001-06** - VmRSS is process-level: it includes the
  probe's own runtime pages (the flat 1408 KiB floor in
  every reading) and cannot attribute bytes to the
  allocator alone. This slice reports RSS deltas only and
  labels them process-level; allocator-attributable
  accounting exists in the E-005 audit counters and a
  future probe leg should pair the two.
- **INC-G001-07** - This Pi's system page size is 16384
  bytes; the 32 KiB-per-allocation footprint in Section 3.2
  is a two-system-page fact on THIS host and would be
  8 KiB on a 4096-byte-page host. No Phase 7 document may
  quote 32 KiB without the page-size qualification
  (MA-6.2-041).

## 8. What this slice does not claim

- No size class, reuse, reclaim, thread-exit or tuning
  design is decided here; Sections 4-6 allocate and inform
  that work, they do not perform it.
- The probe is a micro-probe: single-threaded, one host,
  one session, bounded ops. It is not a benchmark suite, not
  a Phase 11 performance result, and supports no claim
  beyond the recorded runs.
- No Phase 7 exit element is met by this slice; X2/X4 are
  half-proven (Phase 3 side only) and the rest are OWED as
  tabled in Section 5.
