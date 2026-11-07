# Phase 11 — Foundation Plan (Slice K-001)

Slice: **K-001 (team-01)**. Wave 11, Phase 11 (Performance research).
Deliverable: this plan only — one new file, no `src/`, `Makefile`,
`README.md` or `include/mimir_alloc.h` change. Base: `5324991`
(Wave 10 COMPLETE) on branch `yrsa/mimiralloc-01`, worktree
`/home/pi/mimiralloc-team-01` on Raspberry Pi `purpleviking1`.

> **Verdict (plain first).** Phase 11 starts from an honest,
> unfavourable baseline: the banked allocator was built across
> Phases 3–10 for contract correctness, auditability and bounded
> behaviour, not for speed, and the first reproducible baseline
> (K-002, built ahead of this plan — see Section 6) measures it
> losing to the host glibc allocator on every workload tried, by
> median factors from about 5x to about 65x. Phase 11's job is
> therefore not to decorate a win but to close a measured gap —
> profile first, change one thing at a time, and publish loss
> cases with the same care as wins. No claim in this wave is
> valid unless it follows the methodology contract in Section 4.

## 1. Phase 11 restated from the roadmap

Source: `docs/MimirAlloc_Research_and_Development_Roadmap.md`,
"Phase 11 — Performance research" (Section 8) and Section 11
"Benchmarking and success metrics". Restated in substance:

- Investigate: direct fast paths; ARM SIMD where applicable;
  better sizing; batching; TLB/cache locality; metadata
  placement; false-sharing control; safe concurrency.
- **Profile before adding complexity.** No optimisation lands
  on a guess; every change names the profile evidence that
  motivated it.
- Optional rseq or per-CPU mode comes **only** after platform
  proof, a tested fallback, and a benchmarked gain — all three.
- Optimise both compiler-build traces and Aesir traces
  (workload characterisation is K-003's slice).
- **A microbenchmark win is not a universal win.** No result
  generalises beyond the workloads, device and conditions it
  was measured under.

**Roadmap exit (verbatim substance):** publish reproducible
results, loss cases, tail latency, RSS/PSS, energy where
measurable, and throughput with confidence intervals or repeat
distributions.

### 1.1 Section 11 principles (binding on this wave)

- Compare platform libc allocator, a pinned tcmalloc build
  where supported, mimalloc, and MimirAlloc, using correct
  compatible build modes and equivalent settings.
- Verify each allocator was actually selected; record dynamic
  vs static linkage and exported-symbol evidence.
- Fresh-process runs; pin versions; record thermal state, core
  selection, clock policy, workload and repetitions.
- Report median, p95/p99 and outliers for allocation latency,
  throughput, peak RSS/PSS, virtual size, retained memory after
  freeing, mapping count and startup time.
- Measure CPU time, device responsiveness and energy/battery
  where reliable instrumentation exists.
- Include real compiler builds and Aesir load/prefill/decode/
  unload workloads; model output must remain equivalent.
- On tablet trials, measure with foreground tasks running,
  under cool and sustained conditions; never call swap RAM.
- A lower allocation-time number that increases memory
  pressure or UI stutter may not satisfy the objective.
- Store raw data, harness revision, scripts and negative
  results; flag noise rather than invent certainty.
- Targets are established **after** a device baseline; no
  fixed claims of superiority.

### 1.2 Section 11 benchmark classes (wave coverage)

1. Single-threaded small alloc/free cycles, varied sizes —
   K-002 (done, baseline), K-004.
2. Producer/consumer and cross-thread free patterns — K-006.
3. Thread churn and multiple CPU clusters (big.LITTLE) —
   K-006 (Pi has one cluster; big.LITTLE leg owed to a device
   that has one, recorded as owed, not faked).
4. Large allocations, mixed short/long lifetimes — K-002,
   K-007.
5. Fragmentation, return-to-OS, repeated startup/shutdown —
   K-007/K-008 (Phase 9 evidence is the starting point).
6. A real clean Mojo build / compiler invocation — K-003
   traces; a timed leg is owed only if integration proof
   (Phase 6) supports it.
7. Aesir load, prefill, decode, unload — Phase 12's home;
   Phase 11 supplies traces and method only (K-003).
8. Time-limited multitasking/thermal soak on the physical
   tablet — owed to the Android line; not runnable on the Pi.

## 2. Verified inventory: performance evidence already banked

Checked against the tree at base `5324991`, not docs alone.
"Banked evidence" = figures in a committed recorded run or in
`WAVE_PLAN.md` at banking. "Owed" = named but not yet measured
in a form Phase 11 may cite.

| Item | File(s) | What it measures | Status |
|---|---|---|---|
| J-008 overhead bench | `tests/core/bench_j008.c`, `tests/core/recorded_run_j008.txt` | Whole-pass ns/op: 20000x malloc(64)/memset/free + 2000x malloc(4096)/free, per build flavour, 5 fresh-process reps + median, optional J-002 stats snapshot per 1000 ops | Banked for production/hardened on the slice tree (small64 ~120-136 ns/op; large4096 ~5.8-6.7k ns/op per rep) |
| J-008 merged-tree rows | `/home/pi/mimiralloc-forge/WAVE_PLAN.md`, J-008 banking entry | Same bench on the merged tree: production small64 169 / large4096 5836 ns/op; production+stats 169/5826; hardened 77/5917; hardened+stats 78/5831 (coordinator single reps) | Banked in WAVE_PLAN only — the slice doc still reads "owed" (INC-J008-01); debug rows below likewise |
| J-008 debug overhead rows | WAVE_PLAN J-008 entry only | bench_j008 compiled `-DMIMIR_DEBUG=1` vs merged debug objects, 5 reps: small64 775/880/1099/784/795 (median 795) ns/op; large4096 6574/7021/9260/7102/6900 (median 7021) ns/op | Banked in WAVE_PLAN only; no recorded-run file in the tree |
| G-001 baseline probe | `tests/perf/probe_g001.c`, `recorded_run_g001.txt` | Phase 3 baseline micro-probe; explicitly NOT a benchmark suite | Banked probe figures; not citable as a suite baseline |
| H-002 measured baseline | `tests/perf/probe_h002.c`, `recorded_run_h002.txt` | Cross-thread free behaviour + contention latency at 1/2/4/8 threads, per-op latency method | Banked; the per-op latency instrument K-008 builds on |
| H-004 contention controls | `tests/perf/probe_h004.c`, `recorded_run_h004.txt` | Three contention-control variants, H-002 Phase B method | Banked |
| H-007 stress harness | `tests/perf/probe_h007.c`, `recorded_run_h007.txt` | Stress at small thread counts + oversubscription | Banked |
| H-008 latency + memory | `tests/perf/probe_h008.c`, `recorded_run_h008.txt` | Per-op latency + memory reports, H-002 shape | Banked; remains the per-op latency instrument (per K-002 S6) |
| D-004 multithread run | `tests/concurrency/test_d004.c`, `recorded_run_d004.txt` | Timed standalone multithread alloc/fill/free (4 threads, fixed config) | Banked correctness+timing run; not a scaling suite |
| Phase 9 pressure/fragmentation | `docs/PHASE_9_PRESSURE_CYCLES.md`, `docs/PHASE_9_FRAGMENTATION_AND_RECLAIM.md`, `tests/core/recorded_run_i003..i009.txt` | Pressure cycles, fragmentation, reclaim/trim behaviour and their byte accounting | Banked measurements; K-007/K-008 start here, not from zero |
| Phase 10 diagnostics overhead | J-004/J-005 recorded runs + J-008 above | Debug red-zone / poison / quarantine overhead, production distinguished | Banked (see J-008 rows) |
| F-006 Mojo compile comparison | `tests/mojo/run_f006_compare.sh`, `recorded_run_f006.txt` | Representative program repeated-compile comparison, same revision with/without the replacement | Banked single-program comparison; NOT a compiler-build benchmark class-6 leg |
| K-002 baseline suite | `tests/perf/bench_k002.c` etc. — on branch `yrsa/mimiralloc-02` only (commit `cbb6c0f`), not in this tree | MimirAlloc vs glibc, 4 workloads, 7 reps | Built ahead; banks after K-001; reconciled in Section 6 |

**Owed at wave start (no banked evidence exists):** any
optimised (`-O2`) MimirAlloc build comparison; any tcmalloc or
mimalloc leg on this device; multithreaded baseline in suite
form; RSS/PSS under benchmark workloads; per-op latency
distributions (p95/p99) for the baseline workloads; energy
measurement of any kind; Aesir trace timings (Phase 12).


## 3. Standing rules inherited by every K-slice

- Timing is a measurement, **never an oracle for
  correctness**. Correctness is proven by the test suites
  (`make test` and the slice's own checks); a fast wrong
  answer is a failure, not a result.
- The Pi is a working home (Samtal village, kiosk, Aurelia):
  runs stay short and bounded, `free -g` and the Samtal health
  check (HTTP 200 on `127.0.0.1:8787`) are recorded before and
  after any measured run, and nothing else is stopped or
  starved to make a number prettier. Contention from the
  normal home load is a recorded condition, not a hidden one.
- Every figure cited in a slice doc must trace to a recorded
  run committed in the same slice (or to a named earlier
  recorded run / WAVE_PLAN entry). No figure from memory, no
  figure from an uncommitted run.

## 4. Methodology contract for K-002..K-008

Each numbered clause is binding. A slice that cannot meet a
clause records an INC in its own register naming the clause,
the reason, and the owed follow-up — silence is not
compliance.

- **M-1 Fresh processes.** Every repetition is a separate
  process invocation. Legs alternate (A, B, A, B, ...) so
  slow machine drift lands on all legs.
- **M-2 Repetitions and reporting.** Minimum 7 repetitions
  per leg (5 only where a run is genuinely expensive, with
  the reason recorded). Report the **full repeat
  distribution**: every raw repetition, median, min/max, and
  — where the instrument measures per-op latency — p95/p99
  and outliers. A single timing is never a verdict. Where
  whole-pass timing is used, say so; do not dress a pass
  median as a latency distribution.
- **M-3 Allocator selection proven.** For each leg, record
  how the allocator under test was selected and the evidence:
  linkage (dynamic/static), the exact objects or library
  linked, and exported-symbol evidence (`nm -D` or
  equivalent) where a shared library is involved. Workload
  equivalence across legs must be checkable from the recorded
  run itself (e.g. identical per-workload checksums).
- **M-4 Build honesty.** Record the exact build flags of
  every leg. Comparing builds at different optimisation
  levels is permitted only when the difference is the stated
  subject or is flagged as a material caveat in the headline
  results — never silently. An optimised-flavour comparison
  (production code at `-O2`, artefact-verified like the J-008
  hardened flavour) is owed before any general statement
  about the allocator's speed as opposed to the shipped
  build's speed.
- **M-5 Conditions recorded.** Pin and record: harness
  revision (commit), compiler and libc versions, kernel,
  page size, CPU governor/clock policy, core selection or
  pinning (or explicitly none), SoC temperature at run start,
  free memory, system load, and workload parameters. Versions
  are never left as "current".
- **M-6 Warm-up.** Each process runs an untimed warm-up pass
  before its timed pass, or the slice records why not.
- **M-7 Raw data stored.** Raw repetition data, the runner
  and summariser scripts, and the recorded run are committed
  with the slice. Negative results and loss cases are stored
  and reported with the same prominence as wins.
- **M-8 Noise honesty.** Flag noisy legs (wide spreads,
  drift, thermal or load interference) as noisy. Never invent
  certainty: no confidence claim stronger than the data, and
  confidence intervals only when actually computed.
- **M-9 No fixed targets.** Targets are established after the
  device baseline exists. No slice promises a superiority
  factor in advance, and no slice claims a universal win or
  loss from Pi-only data.
- **M-10 The pressure clause.** A lower allocation-time
  number that worsens memory pressure, retained bytes,
  RSS/PSS or device responsiveness does **not** count as a
  win. Any claimed win names its memory-footprint leg or
  records the footprint leg as owed against the claim.
- **M-11 Profile before change.** K-004..K-007 changes name
  the profile or measurement that motivates them and measure
  the same workloads before and after with this contract.
  One change at a time per measured comparison.
- **M-12 rseq/per-CPU gate.** No rseq or per-CPU design lands
  without all three of: platform proof on the target, a
  tested fallback path, and a benchmarked gain under this
  contract.

## 5. Wave 11 slice map

| Slice | Team | Deliverable | Owed evidence |
|---|---|---|---|
| K-001 | 01 | This foundation plan | Roadmap restatement, banked-evidence inventory, this contract, K-002 reconciliation, exit items, INC register |
| K-002 | 02 | Reproducible baseline suite, MimirAlloc vs glibc on the Pi | **Built ahead** (`cbb6c0f`, parent `5324991`): harness + runner + summariser + recorded run + doc; 4 workloads x 7 reps; all-loss result recorded honestly. Divergences from this contract: Section 6 |
| K-003 | 03 | Workload-trace characterisation: compiler-build-like + Aesir-like allocation patterns, from banked evidence only | Trace shapes (size/lifetime/thread histograms) with each figure traced to banked evidence; no invented traces; gaps named as owed |
| K-004 | 04 | Fast-path + sizing investigation, profile-guided | Profile evidence first (M-11); before/after on K-002 workloads under this contract; optimised-flavour leg (M-4) or INC naming it owed |
| K-005 | 05 | Cache/TLB locality + metadata placement investigation | Locality measurements (incl. mapping counts, page-fault or TLB proxies available on the Pi); metadata placement costed in bytes and ns/op |
| K-006 | 06 | False-sharing + concurrency scaling measurement | Multithreaded baseline in suite form (closes INC-K002-03); scaling at 1/2/4 threads (+ oversubscription), per-op latency via the H-002/H-008 method; false-sharing legs with padding/control variants |
| K-007 | 07 | Batching experiments | Batched vs unbatched under this contract; **loss cases recorded honestly**; fragmentation/return-to-OS legs building on Phase 9 evidence |
| K-008 | 08 | Tail-latency / RSS-PSS / energy packet | p95/p99 + outliers per workload (closes the M-2 distribution gap for the baseline); peak RSS/PSS, virtual size, retained-after-free under the K-002 workloads (closes INC-K002-04); energy only where a reliable instrument exists on this Pi, otherwise recorded as not measurable here — never estimated |
| K-009 | 09 | Phase 11 exit evidence packet | Grades every X-item in Section 7 against banked evidence; carries all open INCs forward explicitly |

Banking order is roadmap order: K-001, then K-002, then
K-003, and so on, regardless of build order.


## 6. Reconciliation: K-002 against this contract

K-002 (team-02) built ahead of this plan: commit `cbb6c0f` on
`yrsa/mimiralloc-02`, parent `5324991`, delivering
`tests/perf/bench_k002.c`, `run_bench_k002.sh`,
`summarize_k002.py`, `tests/perf/recorded_run_k002.txt` and
`docs/PHASE_11_BASELINE_BENCHMARKS.md`. Its doc records
**INC-K002-01** (K-001 methodology not yet landed;
reconciliation owed at K-001 banking). This section is that
reconciliation, read from the K-002 doc and run as committed.
K-002's files are not edited here.

**Where K-002 complies:**

- M-1: 7 separate processes per leg, legs alternating so
  drift lands on both. Complies (exceeds the 5 minimum).
- M-3 (equivalence half): one source compiled twice,
  byte-identical workload code, only allocator function
  pointers differ; per-workload checksums identical across
  legs in all 56 raw lines — equivalence is checkable from
  the recorded run itself. Complies.
- M-5: governor (`ondemand`), SoC temperature at run start
  (57.85 C), free memory (13G available), load (~1.3), Samtal
  200 before and after, and full version pinning in its S6
  (kernel 6.12.112-v8-16k, page size 16384, glibc 2.36,
  gcc 12.2.0) are recorded. Complies.
- M-6: 1/10-scale untimed warm-up per process. Complies.
- M-7: raw data, runner, summariser and recorded run all
  committed; the all-loss result is the doc's headline, and a
  full second reproduction run (all medians within 9%,
  ordering identical) is reported. Complies.
- M-9: no target promised, no universal claim in either
  direction; results explicitly scoped to this Pi and the
  shipped build. Complies.
- Section 3: runs short (~11 s per suite), nothing stopped
  or pinned, home load treated as a recorded condition.
  Complies.

**Where K-002 diverges (each already an INC in its doc or
recorded here against it):**

- **M-2 distribution depth — DIVERGES (owed to K-008).**
  K-002 reports median + min/max over 7 whole-pass
  repetitions. It does not report p95/p99 or per-op latency
  distributions, and it computes no confidence intervals.
  Its own S6 states the whole-pass limitation plainly and
  names the H-008 probe as the per-op instrument. Under the
  roadmap exit ("confidence intervals or repeat
  distributions") the full raw repeat distribution it does
  publish is a partial satisfaction; the tail-latency half
  is owed and is assigned to K-008 in Section 5.
- **M-4 build parity — DIVERGES (its INC-K002-02).** The
  MimirAlloc leg is the banked production build, whose
  `CFLAGS` carry no `-O` level (compiler default, `-O0`);
  the glibc leg is the distribution's optimised build. The
  harness driver is `-O2` for both legs, but the allocator
  objects differ in optimisation level. K-002 flags this as
  a material caveat in its S1 and scopes every claim to "the
  banked build as shipped" — that honesty is why the baseline
  stands — but under M-4 an artefact-verified optimised
  (`-O2`) MimirAlloc comparison is owed before any general
  statement about the allocator's speed. Assigned to K-004.
- **M-3 selection evidence — PARTIAL.** K-002's legs call
  `glibc malloc/free` vs `mimir_malloc/mimir_free` from the
  banked production objects by direct linkage; there is no
  interposition ambiguity to resolve, but the recorded run
  carries no `nm`/linkage evidence table of the kind M-3
  asks for when a shared library is involved. Accepted for
  K-002 (direct linkage to named banked objects is stated in
  the doc); future legs that select via a shared library or
  preload must include the symbol evidence.
- **Benchmark class coverage — PARTIAL (its INC-K002-03 /
  INC-K002-04).** The suite is single-threaded only and
  measures throughput/per-op pass time only: no multithreaded
  leg, no RSS/PSS, retained or mapping footprint. Both gaps
  are recorded as INCs in K-002's own doc and are assigned:
  multithreaded baseline to K-006, footprint to K-008.

**Disposition.** INC-K002-01 is discharged by this section:
K-002's method stands as the wave baseline, its four INCs
are adopted into the slice map (Section 5) rather than left
as commentary, and no K-002 file needs to change for the
baseline to be citable — provided every citation carries the
M-4 shipped-build caveat until K-004's optimised leg lands.

## 7. Phase 11 exit items (for K-009 to grade)

Mirroring the roadmap Phase 11 exit text in substance. Each
item is graded MET / PARTIAL / UNMET from banked evidence
only.

- **X-K1 Reproducible results.** The baseline and every
  optimisation claim reproduce from committed harnesses and
  recorded runs under Section 4, on this device.
- **X-K2 Loss cases published.** Every workload where
  MimirAlloc loses — at baseline and after each change — is
  recorded with figures, not summarised away.
- **X-K3 Tail latency.** p95/p99 and outliers reported for
  the baseline workloads from a per-op instrument (K-008).
- **X-K4 RSS/PSS.** Peak RSS/PSS, virtual size, retained
  memory after freeing and mapping counts reported under the
  same workloads (K-008).
- **X-K5 Energy where measurable.** Energy/battery figures
  where a reliable instrument exists on the device, or an
  explicit "not measurable here" record — never an estimate
  presented as a measurement.
- **X-K6 Throughput with distributions.** Throughput figures
  carry confidence intervals or full repeat distributions
  (M-2), never a single timing.
- **X-K7 Profile-before-complexity.** Every landed
  optimisation names its motivating profile and its
  before/after measurement (M-11); the rseq/per-CPU gate
  (M-12) is either satisfied with all three proofs or the
  design is recorded as not landed.
- **X-K8 No universal claims.** No result is stated beyond
  its workloads, device and conditions; the shipped-build
  (`-O0`) caveat is either retired by K-004's optimised leg
  or carried verbatim into the exit packet.

## 8. INC-K001 register

Carried-owed (pre-existing, restated so Phase 11 does not
pretend they closed):

- **INC-K001-01 (Phase 3 exit UNMET).** The x86-64 suite run
  owed since Phase 3 remains owed; no Phase 11 figure may be
  stated for x86-64.
- **INC-K001-02 (Phase 4 exit UNMET).** The physical Android
  run owed since Phase 4 remains owed; benchmark classes 3
  (big.LITTLE) and 8 (tablet soak) are blocked on it and are
  recorded as owed, not simulated on the Pi.
- **INC-K001-03 (INC-I001-14).** Budget enforcement was not
  adopted in Phase 9 and remains not adopted; Phase 11
  pressure claims must not assume it.
- **INC-K001-04 (INC-I009-01).** `docs/SUPPORTED_TARGETS.md`
  is stale since Phase 3; a freshness slice is owed outside
  this wave's queue. Phase 11 target statements cite the
  recorded runs, not that file.

New in this slice:

- **INC-K001-05.** The J-008 debug overhead rows and the
  merged-tree production/hardened rows exist **only** in
  `WAVE_PLAN.md`'s J-008 banking entry; the banked slice doc
  still reads them as owed, and no recorded-run file in the
  tree carries them. Any Phase 11 citation of those rows must
  cite WAVE_PLAN explicitly until a reconciliation slice
  lands them in the tree.
- **INC-K001-06.** No tcmalloc or mimalloc build is banked
  or known present on the Pi, though Section 11 names both
  as comparators "where supported". K-002's glibc-only
  baseline is therefore a partial comparator set; adding a
  pinned comparator leg is owed to a later K-slice (K-004 or
  K-009 to schedule), and no slice may imply the comparator
  set is complete until then.
- **INC-K001-07.** K-002's shipped-build caveat (M-4 /
  INC-K002-02) applies to **every** Phase 11 speed statement
  made before K-004's optimised leg: until then, all figures
  describe the shipped `-O0` build against an optimised
  glibc, and docs must say so where they quote the numbers.

## 9. Verification owed at this slice's commit

`make test` exit 0 in this worktree (expect 4590/0);
`README.md` and `include/mimir_alloc.h` byte-identical to
base `5324991`; `nm -D --defined-only` on the built `.so`
exactly 4 symbols; Samtal HTTP 200 after the run. This slice
commits exactly one new file: this plan.
