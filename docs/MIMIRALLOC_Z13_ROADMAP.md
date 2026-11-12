# MimirAlloc Z13 Roadmap — a new hardware line for Strix Halo

Commissioned by Volmarr, 2026-11-12. Campaign home (banking seat):
`/home/volmarr/mimiralloc-z13-forge` on purpleraider. Slices Z-001…Z-117,
13 waves of 9, banked one slice per commit as `Z-NNN: <title>` through
Yrsa's GitHub identity only (@yrsafreydisdottir).

## Relation to the first campaign

The original MimirAlloc campaign is COMPLETE (117/117 slices) and closed
with an honest grade: it **loses to glibc on every workload measured**
(Phase 11: median factors about 5x to about 65x; small path 1,167 ns vs
glibc 56 ns; large path about 3.7 us vs 75/56 ns — see
docs/LIMITATIONS_AND_KNOWN_DEFECTS.md L-05/L-06). Those numbers were
measured on a 16 KiB-page aarch64 Raspberry Pi. They are this line's
*questions*, never its numbers. This roadmap is a separate hardware line
(the Aesir-Z13 vs Aesir-Hailo pattern): the old roadmap and the
production-and-beyond master roadmap stay history and parallel vision;
this line's one axis is **MimirAlloc running its best on this machine**.
It also discharges two debts this machine is the first able to pay:
O-01 (the portable suite has never run on x86-64) and O-06 (the suite
has never run on a 4 KiB-page host — every banked figure is a 16 KiB
figure).

## Hardware of record (measured on the seat host, 2026-11-12)

- ASUS ROG Flow Z13 (GZ302EA), AMD Ryzen AI MAX+ 395 (Strix Halo):
  16 cores / 32 threads, 2 CCDs, single NUMA node (UMA).
- L1d 48 KiB/core, L2 1 MiB/core, L3 64 MiB (2 x 32 MiB, one per CCD).
- 128 GB LPDDR5X unified memory, 121 GiB visible to Linux; no discrete
  VRAM split for system RAM. 4 KiB base pages; THP = madvise/madvise.
- Kubuntu 26.10, kernel 7.3.0-10-generic, glibc 2.44, gcc with the
  repo's own flags (-O2 -Wall -Wextra -Werror). CPU governor at study
  time: performance. Base commit: ea2a39f557109390e76aebb363ada57b9cca7ee7.

## Laws of this line

1. **Measurement first, this machine only.** No performance claim
   without numbers from the campaign oracle harness running on
   purpleraider. Numbers from any other machine (including the first
   campaign's Pi figures) are inadmissible as evidence here.
2. **Honest grading.** Every hunt ends in a recorded verdict: WIN,
   TIE, LOSS (with the size of the loss), or STILL LOST — REASON NAMED.
   A slice that proves a loss has succeeded as a slice. No document of
   this line may claim a universal performance win.
3. **Pinned methodology.** Every bench run records its environment
   header (Z-002), follows the interleave/repetition/median rules of
   Z-004, pattern-touches every block, and proves workload equivalence
   across legs by identical checksums (the K-002 discipline).
4. **Gates, not vibes.** A slice banks only with its named gate green:
   the standing correctness gate (Z-008) for code slices, the oracle
   numbers for measurement slices, the recorded verdict for hunts.
   Counts are read from log files on the seat, never from memory.
5. **The contract is not for sale.** No slice may trade away the
   first campaign's correctness posture (bounded behaviour, audit
   surfaces, no silent corruption) for speed without recording the
   trade as a decision in docs/DECISION_LEDGER.md.
6. **One heavy run at a time.** The seat is shared with other forges;
   heavy suites and soaks are serialised per WAVE_PLAN.md.
7. **README.md is never touched.** Banking is Yrsa-identity only.

## The known-loss classes (the hunts, from L-05/L-06 and the register)

- KL-A Single-thread small-op latency (small path vs glibc tcache).
- KL-B Multithread scaling under the one global lock (L-05).
- KL-C Large allocations: a mapping per allocation past the 1024-byte
  cutoff vs glibc's threshold-and-cache wisdom.
- KL-D Mixed lifecycle (the 65x wound: churn across classes + larges).
- KL-E Batching (K-007 lost everywhere it was measured; proposal P-1,
  register D-06, was never implemented).
- Carried-in defects: D-01/D-02 (lock-failure observability), D-12
  (INC-J006 silent corruption classes), D-09 (fuzz coverage limits),
  L-02 (no aligned/usable-size/stats/trim public surface), L-03
  (budgets designed, never enforced), L-04 (telemetry gaps), L-07
  (diagnostics default-on ruling owed), L-09 (zero-size/errno
  contract), D-07 (no SONAME/install story), D-10 (contracts.md gap).

## Wave 1 — The oracle stands (measurement foundation)

### Z-001 — Campaign baseline: the full suite on x86-64 / 4 KiB pages
Run the unmodified `make test` on the seat host and record counts,
host facts and page size as the campaign's first evidence run.
Gate: full-suite counts in a recorded run file; O-01 and O-06 formally
discharged in the run header or every failure triaged to a named cause.

### Z-002 — Host-truth capture
Script `scripts/z13/host_truth.sh` printing the environment header
(CPU model, governor, frequency, cache topology, THP state, page size,
glibc and kernel versions) that every later bench run embeds verbatim.
Gate: header output for the seat host committed as the reference sample.

### Z-003 — The K-002 oracle, first on-machine loss table
Run the banked tests/perf/bench_k002 harness unmodified, both legs,
7 alternating processes per leg, checksums proven identical.
Gate: recorded run + summary table (4 workloads x 2 legs, medians and
spread) filed under tests/perf/ as recorded_run_z13_k002.txt.

### Z-004 — Measurement law: pinning, affinity, repetition
Write the campaign measurement protocol (governor/frequency recording,
CCD pinning via taskset, warm-up, interleave, median-of-processes,
drift rejection) and add the harness switches that implement it.
Gate: protocol doc + a demonstration run whose two identical legs
agree within the protocol's own noise bound.

### Z-005 — Perf-counter leg
Extend the oracle with perf stat counters (cycles, instructions,
cache-misses, branch-misses) normalised per op for both legs.
Gate: counter table for the 4 baseline workloads, both legs, recorded.

### Z-006 — RSS/PSS leg (discharges the O-09 RSS remainder here)
Smaps-rollup based resident-memory measurement for both allocators
across the baseline workloads, sampled at defined points.
Gate: RSS/PSS table vs Z-003's workloads, methodology notes included.

### Z-007 — Energy leg (discharges the O-09 energy remainder here)
RAPL package-energy measurement per workload for both legs, with the
idle-baseline subtraction method stated.
Gate: joules/op table for the baseline workloads or a recorded proof
that RAPL is not exposed on this host, with the fallback method named.

### Z-008 — The standing correctness gate
Wire contract + core + concurrency + failure suites into one gate
script `scripts/z13/run_gate.sh` that logs counts to a file and fails
loudly; this is the gate every later code slice must pass.
Gate: gate script green on the unmodified tree; counts in the log.

### Z-009 — Wave 1 verdict: the baseline of record
File docs/Z13_WAVE01_BASELINE.md: the on-machine table every later
wave is judged against (latency, counters, RSS, energy; both legs).
Gate: the document's every number traceable to a recorded run in-tree.

## Wave 2 — Hardware truth (the UMA the allocator must fit)

### Z-010 — Latency ladder
Pointer-chase latency by working-set size, pinned per CCD: L1/L2/L3/
DRAM plateaus measured on this silicon, not quoted from datasheets.
Gate: latency table 4 KiB..512 MiB with plateau boundaries named.

### Z-011 — Bandwidth saturation
STREAM-style triad at 1/2/4/8/16/32 threads: where UMA bandwidth
saturates and what one core can pull alone.
Gate: bandwidth curve + the saturation thread count recorded.

### Z-012 — CCD and SMT behaviour
Intra-CCD vs cross-CCD ping-pong latency and shared-line transfer
cost; SMT sibling vs physical-core throughput for memory work.
Gate: CCD/SMT cost table feeding the arena design constants.

### Z-013 — THP under madvise
MADV_HUGEPAGE on/off for large mappings: fault latency, RSS honesty,
and bandwidth deltas on 2 MiB-backed regions.
Gate: THP verdict per size decade, recorded with the Z-002 header.

### Z-014 — Page-fault cost curve
Fresh-mapping fault cost by size (4 KiB..1 GiB), first-touch vs
recycled: the number the large path (KL-C) lives or dies by.
Gate: fault-cost curve filed; recycled-vs-fresh ratio stated.

### Z-015 — Syscall and VMA economics
mmap/munmap/mremap cost ladder; VMA split/merge behaviour and counts
under allocation churn (from /proc/self/maps sampling).
Gate: syscall cost table + VMA behaviour notes for the trim design.

### Z-016 — False-sharing ladder
Contended cache-line microbench: same-line vs neighbouring-line vs
padded counters at 2..32 threads on this machine's 64-byte lines.
Gate: contention cost table; the padding rule of record stated.

### Z-017 — Alignment audit
Returned-pointer alignment distribution per size class for both
allocators; cacheline-split rates for the hot classes.
Gate: alignment table; any class whose blocks routinely split lines
is named for Wave 4.

### Z-018 — Design constants of record
File docs/Z13_DESIGN_CONSTANTS.md ratifying the numbers Waves 3-7
may assume (line size, page policy, CCD count, saturation point,
fault costs), each cited to its wave-2 run.
Gate: constants doc complete; no uncited constant admitted.

## Wave 3 — The single-lock autopsy (KL-A/KL-B root cause)

### Z-019 — Lock hold and wait histograms
Debug-build-only rdtsc instrumentation of core lock hold/wait per
public entry point; zero cost when not compiled in.
Gate: hold/wait histograms for the baseline workloads, recorded.

### Z-020 — The contention ladder (banked allocator)
1..32 threads on small_churn and mixed_batch: per-thread throughput
and the exact thread count where scaling inverts.
Gate: ladder table + inversion point named.

### Z-021 — The contention ladder (glibc comparator)
The identical ladder against glibc, same protocol, same header.
Gate: glibc ladder table side by side with Z-020.

### Z-022 — Small-path anatomy
perf record/report of the mimir leg on small_churn: the top hot
spots by cycles, each mapped to a source line region.
Gate: annotated hot-spot list (top 10) filed for Wave 4 to burn down.

### Z-023 — Large-path anatomy
Profile of large_batch: what share of the large-path cost is the
mmap/munmap pair, what share is bookkeeping, what share is faults.
Gate: cost split percentages recorded; KL-C's wound sized exactly.

### Z-024 — Free-path anatomy
Cross-thread free and remote-free costs on the banked core;
mapping-path free cost separated from small-path free cost.
Gate: free-cost table per path and thread geometry.

### Z-025 — calloc and realloc anatomy
Zeroing cost by size; realloc growth ladders (copy vs extend)
against glibc at the class boundaries and the 1024 cutoff.
Gate: calloc/realloc tables; any cliff named with its size.

### Z-026 — Redesign dossier
Candidate architectures (TLS freelist cache; per-CCD arenas;
per-thread arenas; sharded central locks) scored against the
Z-018 constants and Z-019..Z-025 evidence. Decision recorded in
docs/DECISION_LEDGER.md. No production code in this slice.
Gate: dossier + ledger decision naming Wave 4/5's chosen roads.

### Z-027 — Autopsy verdict
File the ranked loss table: where the nanoseconds go, per workload,
and which wave owns burning each down.
Gate: every Z-009 baseline gap assigned to a named later slice.

## Wave 4 — Small-path speed hunt (KL-A: the 1,167 ns wound)

### Z-028 — TLS cache: contract before code
Contract tests for a per-thread freelist cache over the banked size
classes (ownership, cross-thread free routing, thread-exit drain),
written against the Z-026 decision and failing on the current core.
Gate: contract suite exists, red on the old core, green-by-design
review recorded.

### Z-029 — TLS cache: implementation
Per-thread bins for the hot classes, lock-free on cache hit, batched
refill/drain to the central core on miss.
Gate: Z-028 contract green; standing gate (Z-008) green.

### Z-030 — Cross-thread frees into TLS-owned blocks
Remote-free queue per owner thread with bounded drain; concurrency
suite extended for producer/consumer geometries.
Gate: concurrency gate green incl. the new geometries; Z-024's
remote-free cost re-measured.

### Z-031 — Small-path verdict run
small_churn + mixed_batch re-run under the full oracle protocol.
Gate: verdict vs Z-009 and glibc recorded per workload (Law 2).

### Z-032 — Size-class spacing study
The 20 classes (16..1024) audited on-machine: internal fragmentation
bytes, cache behaviour per class (Z-017 splits), request histogram
fit. Propose a retuned table only if the evidence argues for one.
Gate: class-by-class table + retune proposal or a recorded no-change
verdict with reasons.

### Z-033 — Size-class retune (conditional)
Implement Z-032's adopted table; fragmentation suite + oracle legs.
Gate: standing gate green; fragmentation and latency deltas recorded
(either sign — an adopted-then-reverted table is a valid outcome if
the numbers say so, with the revert itself banked and explained).

### Z-034 — Metadata diet
Per-block header cost audit; trial out-of-line metadata for the
smallest classes behind a build flag; measure both ways.
Gate: header cost table + adopt/reject verdict per class group.

### Z-035 — Build-flag honesty round
-O3, -march=native and LTO trials on the small path, K-004 style:
each flag's delta measured, only evidence-backed flags adopted.
Gate: flag matrix recorded; Makefile changes only for adopted flags.

### Z-036 — Wave 4 verdict
File the small-path standings: mimir (original) vs mimir (wave 4)
vs glibc, per workload, with the remaining gap itemised.
Gate: verdict doc traceable to recorded runs.

## Wave 5 — Thread-scaling hunt (KL-B at 16 cores / 32 threads)

### Z-037 — Arena prototype behind a flag
Per-CCD arena variant (2 arenas, Z-012 constants) selectable at
build time, sharing the Wave-4 cache machinery.
Gate: prototype builds; standing gate green with the flag on.

### Z-038 — Arena ladder
The Z-020/Z-021 ladders re-run for the arena variant.
Gate: three-way ladder table (banked / arena / glibc) recorded.

### Z-039 — Second arena geometry (evidence-conditional)
Per-thread arenas if Z-038 argues for them, else lock-sharding of
the central core; the road not taken is recorded with its reason.
Gate: chosen geometry's ladder table + decision in the ledger.

### Z-040 — False-sharing purge
Apply Z-016's padding rule to every shared structure the profiles
name (caches, stats, free queues); measure before/after.
Gate: contention deltas recorded per structure touched.

### Z-041 — SMT and pinning policy
16 vs 32 threads, pinned vs floating, for allocator-heavy loads:
the campaign's recommended affinity posture for this machine.
Gate: policy table + recommendation doc; no code change implied.

### Z-042 — Thread-churn storm
10,000 short-lived allocating threads: TLS/arena teardown
correctness, drain cost, and leak accounting to the byte.
Gate: storm green; retained-bytes-after-storm figure recorded.

### Z-043 — Producer/consumer free storm
32-thread cross-free patterns at rates the banked core never faced;
remote-free queue behaviour under flood.
Gate: storm green; worst-case remote-free latency recorded.

### Z-044 — Fork posture proof on this host
Fork-quiescent and fork+exec proofs re-run on x86-64 (Phase 8
posture), plus a fork-while-busy measurement that states the
posture's boundary honestly (L-08).
Gate: posture proof filed for this host; boundary behaviour recorded.

### Z-045 — Wave 5 verdict
Scaling curves 1..32 threads, three allocator variants, per
workload; KL-B's standing restated in numbers.
Gate: verdict doc traceable to recorded runs.

## Wave 6 — Large-path hunt (KL-C: a mapping per allocation)

### Z-046 — The threshold study
Measure glibc's dynamic mmap-threshold behaviour on this host and
cost MimirAlloc's fixed 1024-byte cutoff per size decade
(1 KiB..32 MiB): where the mapping path starts paying, and where
it starts bleeding.
Gate: per-decade cost table; the campaign's cutoff options priced.

### Z-047 — Mapping-cache design
Design for cached dead mappings (size-segregated reuse pool,
bounded by bytes and count, Z-015 VMA economics applied).
Gate: design doc + the invariants the implementation must keep.

### Z-048 — Mapping-cache implementation
Reuse pool live for the large path; munmap only on pool overflow
or trim. Standing gate extended with large-path churn cases.
Gate: gate green; large_batch re-measured vs Z-009 and glibc.

### Z-049 — THP for cached mappings
MADV_HUGEPAGE on pooled mappings per Z-013's verdict; measure
fault and bandwidth deltas on the large workloads.
Gate: adopt/reject per size decade, recorded with numbers.

### Z-050 — Trim policy under pressure
Conservative return-to-OS policy driven by pool idle time and
/proc/pressure/memory; RSS honesty proven by Z-006's leg.
Gate: pressure-cycle run shows RSS follows the policy's promise.

### Z-051 — Huge allocations (256 MiB..8 GiB)
UMA behaviour, overcommit posture, and failure cleanliness at
sizes this machine makes tempting; partial-failure paths audited.
Gate: huge-size table (success cost + failure behaviour) recorded.

### Z-052 — mremap growth
Audit whether large realloc grows in place; implement mremap
growth where the mapping cache allows, measure vs glibc.
Gate: growth-ladder table before/after; contract tests green.

### Z-053 — Large-path fragmentation
Interleaved large sizes over long runs: VMA count growth, hole
formation, worst-case RSS blow-up factor vs live bytes.
Gate: fragmentation factors recorded per pattern.

### Z-054 — Wave 6 verdict
large_batch + mixed_lifecycle standings vs Z-009 and glibc;
KL-C restated; the cutoff of record (if changed) justified.
Gate: verdict doc traceable to recorded runs.

## Wave 7 — Fragmentation and long-run behaviour (KL-D's slow half)

### Z-055 — The fragmentation oracle on this host
Port and run the Phase-9 F-series fragmentation legs here,
discharging O-08's owed F-6 cross-slice sampling for x86-64.
Gate: F-series results filed with this host's header.

### Z-056 — Soak harness
One-hour mixed-lifecycle soak runner with periodic RSS,
fragmentation and latency sampling; detached-run discipline
(start, poll, collect) per the shared-machine law.
Gate: harness proven on a 10-minute pilot with complete samples.

### Z-057 — Soak 1: the banked allocator
Full one-hour soak of the allocator as it stood at Z-009.
Gate: soak report (drift curves, worst fragmentation, tail RSS).

### Z-058 — Split and coalesce audit
Free-block splitting/coalescing behaviour in the core under
adversarial patterns constructed from Z-055's worst cases.
Gate: pattern-by-pattern behaviour table; worst case named.

### Z-059 — Placement policy trial
First-fit vs best-fit (vs current policy) measured on the
fragmentation oracle before any change is adopted.
Gate: policy comparison table + ledger decision on adoption.

### Z-060 — Pressure cycles on this host
Phase-9 pressure-cycle heritage re-run: cgroup ceilings, trim
response, recovery after pressure release (with Z-050's policy).
Gate: cycle report; no unbounded retention observed or the bound
that failed is named.

### Z-061 — Soak 2: the improved allocator
The Z-056 soak repeated on the post-wave-6 allocator.
Gate: soak comparison verdict vs Z-057, curve by curve.

### Z-062 — Reuse-distance and L3
Allocation reuse policy (LIFO/FIFO per class) measured against
the 64 MiB L3: does hot reuse stay cache-resident at desktop
working-set sizes?
Gate: reuse-policy table; any per-class policy change proposed
with its numbers.

### Z-063 — Wave 7 verdict
Fragmentation + soak standings, both soaks, both allocators;
KL-D's slow half restated.
Gate: verdict doc traceable to recorded runs.

## Wave 8 — Hardening and integrity (D-01, D-02, D-12, D-09, L-07)

### Z-064 — Lock-failure observability (D-01)
Post-unlock failure tally wired to the internal stats surface;
fault-injection proof (I-004 shim heritage) that a failed unlock
is now counted and reported, never silently wedging the process.
Gate: injection run shows the tally moving; standing gate green.

### Z-065 — Silent courses made distinguishable (D-02)
Drain/trim lock-failure outcomes made distinguishable from
nothing-retained (counter or sentinel, chosen by ledger decision);
free-on-lock-failure keeps its safe posture and gains a counter.
Gate: contract tests for each course; decision recorded.

### Z-066 — Silent corruption classes, round 1 (D-12)
Detection wiring for INC-J006 silent classes 1-2 (headed by
consistent length corruption, INC-J006-02) into the diag channel.
Gate: those probes flip from SILENT to DETECTED in the 25-probe
suite on this host.

### Z-067 — Silent corruption classes, round 2 (D-12)
Classes 3-4 (headed by free_head naming a live object,
INC-J006-04; live_count trusted by release paths).
Gate: probes flip or the undetectable-by-design reason is recorded
per class with its compensating control.

### Z-068 — Integrity standings
Remaining silent classes treated likewise; the full 25-probe
suite re-run on this host; fuzz refresh (D-09): failure-shaped
realloc and multithread legs at M-004's harness, longer budget.
Gate: detection count verdict vs the banked 16/25; fuzz report.

### Z-069 — Canary economics
Per-block canaries costed at this machine's scale (ns/op and
bytes/block per class group); adopt where the price is noise,
reject where it is not — by measurement, not taste.
Gate: cost table + adopt/reject verdict per class group.

### Z-070 — Double-free detection economics
The cheap always-on subset (freed-block poison + freelist
membership) costed the same way; implemented if the numbers
support it, with the escape hatch documented.
Gate: cost table + implementation or a recorded rejection.

### Z-071 — The diagnostics default-on packet (L-07)
Measure diag-on cost across the oracle workloads; file the
evidence packet and record the campaign's ruling in
DECISION_LEDGER (INC-J007-01 discharged one way or the other);
wire the remaining detection sites (INC-J007-03) or list them.
Gate: packet + ledger ruling + site inventory in-tree.

### Z-072 — Wave 8 verdict
Integrity standings: detection coverage, hardening costs paid,
and the honest list of what still passes silently.
Gate: verdict doc traceable to recorded runs.

## Wave 9 — Public surface and contracts (L-02, L-03, L-04, L-09, D-07, D-10)

### Z-073 — Aligned allocation: contract first
U-series contract for mimir_aligned_alloc and a posix_memalign-
shaped entry (alignment domain, size domain, failure behaviour),
written before code.
Gate: contract text in docs/contracts.md style + red tests.

### Z-074 — Aligned allocation: implementation
The surface implemented over the core's class machinery;
ABI link proofs in E-series style (C and C++ consumers).
Gate: contract green; ABI recorded runs filed.

### Z-075 — Usable size
Naming decision (mimir_usable_size vs malloc_usable_size shape)
recorded; implementation exact per class, never a guess.
Gate: per-class exactness tests green; decision in the ledger.

### Z-076 — Public statistics, coverage completed
The internal snapshot promoted to a public versioned struct;
the L-04 gaps closed per counter: lock contention counted
(INC-J002-02), small-path cross-thread frees counted
(INC-J002-01), request sizes retained (INC-J002-05) — or each
gap's closure cost measured and the gap re-documented as priced.
Gate: stats API tests green; per-counter disposition table.

### Z-077 — Trim and reallocarray surface
Public mimir_trim over the I-003 machinery; mimir_reallocarray
with the overflow contract honoured exactly.
Gate: contract tests incl. overflow edges green.

### Z-078 — Zero-size and errno contract closure (L-09)
U-09/U-12/U-10 decided by recorded decision, implemented, and
stated in the public header as contract, not posture.
Gate: header text + tests match the decision, byte for byte in
behaviour.

### Z-079 — Budget enforcement adopted (L-03 / D-04)
Phase-9 soft budgets enforced at last: counters, failure
behaviour, and the budget-failure counter live (INC-I001-14
discharged).
Gate: enforcement tests green; over-budget behaviour proven.

### Z-080 — SONAME, versioning, install (D-07)
Versioned .so with SONAME, a real install target, and the
ldconfig story written down; DT_NEEDED names the versioned file.
Gate: install into a staging prefix + consumer link proof.

### Z-081 — Contracts appendix and ABI audit (D-10)
contracts.md Appendix A amended to inventory calloc/realloc
(and the wave-9 surface); M-002-style ABI audit re-run over the
widened surface on this host.
Gate: audit report filed; no undocumented public symbol remains.

## Wave 10 — Interposition and whole-machine integration (L-01's opt-in route)

### Z-082 — The interposing variant
An opt-in build whose library interposes the malloc family;
symbol-table audit against glibc's set, every missing symbol a
named list entry, never a surprise at runtime.
Gate: interposing .so builds; symbol diff filed.

### Z-083 — Gap closure for real programs
Shims for the symbol classes real programs actually call
(memalign/valloc/pvalloc family over Z-074; strdup-class needs
assessed honestly — libc-internal users cannot be interposed
and the doc must say so).
Gate: gap list shrunk to the provably-unreachable remainder.

### Z-084 — LD_PRELOAD smoke battery
coreutils, git, python3, bash and the repo's own test binaries
under preload on the seat host; per-program pass/fail with logs.
Gate: battery report; every failure triaged to a named cause.

### Z-085 — Python and Ollama-class workload
A real Python process (import storm + allocation churn) and a
short Ollama run under preload: correctness plus perf delta.
Gate: workload report; deltas vs unpreloaded recorded.

### Z-086 — Desktop battery
KDE application battery (kcalc/kwrite-class apps and a plasmashell
component where safely testable) under preload in a contained
session; rollback is unsetting one environment variable.
Gate: battery report with the containment method stated.

### Z-087 — Aesir boundary trial
The Aesir CPU engine on this host run against the interposing
build (Phase-12 boundary heritage): allocation behaviour on the
token path profiled, read-only toward the Aesir forge.
Gate: trial report; any Aesir-side blocker named, not fixed here.

### Z-088 — Failure drills under preload
Corruption and kill drills inside preloaded processes: the
allocator must fail loudly through the diag channel, never
corrupt silently (Wave-8 wiring under field conditions).
Gate: drill log shows loud failure or the silent path is filed
as a defect with an owner slice.

### Z-089 — Rollback and never-preload packet
One-variable rollback proven from a broken state; the
never-preload list (display manager, systemd, the banking
tooling itself) written as an ops doc with reasons.
Gate: rollback drill recorded; ops doc in-tree.

### Z-090 — Wave 10 verdict
Where preload is safe today, where it is not, and the evidence
for each — stated plainly enough for a user to decide.
Gate: verdict doc traceable to battery reports.

## Wave 11 — Batching's second trial and real traces (KL-E / D-06)

### Z-091 — Why batching lost: the K-007 post-mortem
Re-read proposal P-1 against this machine's constants: lock
amortisation vs locality vs API shape, priced per factor from
Waves 2-3 evidence.
Gate: post-mortem doc naming the dominant loss factor.

### Z-092 — Batch API prototype
Bulk same-class alloc/free API over the Wave-4 cache, shaped by
Z-091's dominant factor; contract tests first.
Gate: prototype green on its contract; not yet a public symbol.

### Z-093 — Batch verdict run
Prototype vs per-op loop vs glibc on batch-shaped workloads.
Gate: verdict recorded (Law 2) — including if batching loses
again, with the reason named and P-1 closed honestly.

### Z-094 — Real workload traces
Allocation traces captured from real programs on this host
(audit-hook route): python import storm, git object churn, an
Ollama session; a replay harness that re-runs a trace against
any allocator leg.
Gate: three traces in-tree with replay checksums matching.

### Z-095 — Trace-driven tuning
Class table and cache sizes re-tuned against the traces;
before/after measured on traces AND the synthetic oracle.
Gate: tuning deltas recorded for both suites; regressions in
either block adoption and are reported, not hidden.

### Z-096 — Final build-flag matrix
The flag study repeated on the near-final core (the core changed
since Z-035; its verdicts do not carry over unmeasured).
Gate: final flag matrix + adopted set in the Makefile.

### Z-097 — Short-lived process economics
Interposition .so load and first-allocation cost across a
fork+exec storm: the tax on shell scripts and build farms.
Gate: per-process tax table vs glibc recorded.

### Z-098 — The fair-fight leg
glibc measured under its own tunables (tcache size/count,
mmap threshold) at their best for each workload: MimirAlloc
faces tuned glibc, not only default glibc, in all final tables.
Gate: tunables matrix recorded; the tuned-glibc column exists
in the Z-107 closing table's raw data.

### Z-099 — Wave 11 verdict
Batching's final standing and the trace standings, stated
per workload with reasons.
Gate: verdict doc traceable to recorded runs.

## Wave 12 — Soak, chaos, and the closing table

### Z-100 — The long soak
Eight-hour mixed-lifecycle soak (detached, sampled hourly) on
the near-final allocator, scheduled under the shared-machine law.
Gate: complete soak report; any drift or leak quantified.

### Z-101 — Storm battery, final form
Thread-churn (Z-042) + free-storm (Z-043) + fork storm combined
at machine scale against the near-final core.
Gate: battery green; retained-bytes figures recorded.

### Z-102 — Memory-pressure chaos
Cgroup ceilings (4 GiB, 16 GiB) over allocation storms:
behaviour at the wall, failure cleanliness, recovery.
Gate: chaos report; every failure mode is a clean NULL + diag
line or a filed defect.

### Z-103 — Corruption chaos
Randomised metadata flips under load: Wave-8 detection measured
at scale, false-positive rate on clean runs measured too.
Gate: detection and false-positive rates recorded.

### Z-104 — Sanitizer and warning matrix
ASan/UBSan builds of the full suite plus the warning gates
(M-003 heritage) on x86-64.
Gate: matrix report; every finding fixed or filed with an owner.

### Z-105 — Release engineering
Version-bump decision packet, SBOM refresh, reproducible-build
proof on this host (two clean builds, identical hashes).
Gate: packet + reproducibility proof in-tree.

### Z-106 — Documentation truth pass
SUPPORTED_PLATFORMS/SUPPORTED_TARGETS gain the Z13 line's truth
(D-05's pattern, this line's files); LIMITATIONS register
addendum records what this campaign changed and what it did not.
Gate: docs diff reviewed against the wave verdicts; no claim in
any doc exceeds its recorded evidence.

### Z-107 — The closing head-to-head
The complete Wave-1 oracle (latency, counters, RSS, energy)
re-run on the final allocator, against Z-009's baseline and BOTH
glibc columns (default and Z-098 tuned).
Gate: the closing table filed as recorded runs + summary.

### Z-108 — Campaign verdict
File docs/Z13_CAMPAIGN_VERDICT.md: per workload and per loss
class, WIN / TIE / LOSS with the numbers; what remains lost and
why, in plain language; hand-off notes to the production-and-
beyond roadmap. This document grades the campaign, not the hope.
Gate: every claim traceable; the keeper's stand-down checklist
attached.

## Wave 13 — Verdict-driven reserve (chosen by evidence, not by hope)

The final wave's slices are drawn from the pool below by the
Wave-12 evidence, one pool item per slice, the choice and its
reason recorded in WAVE_PLAN.md before the slice starts. Pool:
per-CCD arena refinement; GPU-shared UMA pool behaviour (the
graphics stack shares this memory — measure the interference);
perf-per-watt tuning at the 70 W envelope; profile-guided class
table v2; preload long-soak of a full desktop session; Aesir
KV-cache-shaped allocation specialisation; API fuzz at scale
(libFuzzer-style harness over the public surface); huge-page-only
large-pool variant; energy-proportional idle behaviour.

### Z-109 — Reserve slice 1 (pool choice recorded in WAVE_PLAN)
Gate: the pool item's own success measure, stated before it starts.
### Z-110 — Reserve slice 2 (pool choice recorded in WAVE_PLAN)
Gate: as Z-109.
### Z-111 — Reserve slice 3 (pool choice recorded in WAVE_PLAN)
Gate: as Z-109.
### Z-112 — Reserve slice 4 (pool choice recorded in WAVE_PLAN)
Gate: as Z-109.
### Z-113 — Reserve slice 5 (pool choice recorded in WAVE_PLAN)
Gate: as Z-109.
### Z-114 — Reserve slice 6 (pool choice recorded in WAVE_PLAN)
Gate: as Z-109.
### Z-115 — Reserve slice 7 (pool choice recorded in WAVE_PLAN)
Gate: as Z-109.
### Z-116 — Reserve slice 8 (pool choice recorded in WAVE_PLAN)
Gate: as Z-109.

### Z-117 — Campaign closure
Final evidence-ledger sweep for the Z line, PROJECT_MEMORY_MAP
completed, keeper stand-down executed, and a closing summary
addressed to Volmarr in plain language: what was won, what was
tied, what remains lost, and what it would take to change that.
Gate: closure packet complete; banked count from git = 117/117
plus the founding commit, verified by ls-remote.
