# Phase 11 Workload Traces — Characterization (K-003)

Slice K-003, Wave 11, Phase 11 (Performance research). Base
5324991daadff6d84c3a1503ad9c54c53c6950a7 (Wave 10 COMPLETE).
Doc-only slice: no code, harness, Makefile, README or header change.
Every pattern below is extracted from a banked source or record at
this base, or is labelled MODELLED / OWED. Nothing is claimed as
measured that was not run and logged on the Pi in a banked record.

Evidence grades used in this document:
- BANKED-MEASURED: a banked recorded_run file logs an actual run.
- BANKED-AUTHORED: the pattern exists in banked test/harness source
  but the catalogue cites the source pattern, not a timing claim.
- MODELLED: composed from banked evidence; no run of the real
  workload exists. A modelled trace is a hypothesis for K-slices
  to test, never a result.

## 1. The roadmap demand

Roadmap Phase 11 (docs/MimirAlloc_Research_and_Development_Roadmap.md,
Phase 11 — Performance research): investigate direct fast paths, ARM
SIMD where applicable, better sizing, batching, TLB/cache locality,
metadata placement, false-sharing control and safe concurrency;
profile before adding complexity; optional rseq or per-CPU mode only
after platform proof, a tested fallback and a benchmarked gain.
Optimize BOTH compiler-build and Aesir traces; a microbenchmark win
is not a universal win. Exit: publish reproducible results, loss
cases, tail latency, RSS/PSS, energy where measurable, and
throughput with confidence intervals or repeat distributions.

Roadmap Section 11 names the two real-workload families Phase 11
must include: real compiler builds ( benchmark class 6: a real clean
Mojo build and compiler invocation, if integration is proven ) and
Aesir model-load/inference workloads ( class 7: Aesir load, prefill,
decode and unload on the same model/configuration ). Section 11
also fixes the method: fresh process runs, pinned versions, thermal
state recorded, median + p95/p99 + outliers, peak RSS/PSS, retained
memory after freeing, mapping count, startup time; raw data,
harness revision and negative results stored.

### The 8 benchmark classes (Section 11) and their signatures

| # | Class (roadmap wording, condensed) | Allocation-pattern signature |
|---|---|---|
| 1 | Single-threaded small alloc/free cycles, varied size distributions | Small size classes dominant; short lifetimes; high churn; same-thread free; latency-sensitive fast path |
| 2 | Producer/consumer and cross-thread free patterns | Allocation on one thread, free on another; ownership handoff; remote-free queue pressure |
| 3 | Thread churn and multiple CPU clusters (ARM big.LITTLE) | Repeated thread create/destroy; per-thread caches born and die; migration across clusters |
| 4 | Large allocations and mixed short/long lifetimes | Large/mmap-path sizes beside small; some blocks live for the whole run; realloc growth |
| 5 | Fragmentation, return-to-OS, repeated startup/shutdown | Interleaved alloc/free leaving holes; trim/release cycles; process-lifetime repetition |
| 6 | A real clean Mojo build and compiler invocation, if integration is proven | Many small short-lived plus medium objects; burst lifetimes per compilation unit; phase-bursts (parse, typecheck, codegen) |
| 7 | Aesir load, prefill, decode and unload, same model/configuration | Very large model-load allocations; KV-cache growth; per-token decode churn; full teardown release |
| 8 | Time-limited multitasking and thermal soak on the physical tablet | Any of the above under contention and heat; tail latency and responsiveness, not peak throughput |

Classes 1-5 and 8 can be served from banked synthetic patterns
(Section 2). Classes 6 and 7 are the named real workloads and are
treated in Sections 3 and 4 with strict honesty labels.

## 2. Trace catalogue — patterns actually present in the banked tree

### T-01 Smoke pair (contract entry pattern)
- Source: tests/contract/test_smoke.c; record tests/core/recorded_run.txt lineage via make test.
- Sizes: 1 byte and 1024 bytes, plus a SIZE_MAX refusal probe.
- Lifetime/ownership: allocate, write, read, free, same thread, immediate.
- Thread pattern: single-threaded. Serves class 1 (smallest leg).
- Consumer: K-002 baseline suite (held, see Section 5) and K-004 fast-path work.
- Grade: BANKED-MEASURED (runs inside every banked make test).

### T-02 Core live-set pattern
- Source: tests/core/test_core.c (29/0 at C-003 banking).
- Sizes: simultaneous live set {16, 500, 5000, 20000} bytes held together, pattern-filled and re-verified (non-overlap, data integrity).
- Lifetime: mixed — all four live concurrently, then released; repeated cycles return to baseline (Phase 3 exit doc, item 4 behavioural half).
- Thread pattern: single-threaded. Serves classes 1 and 4.
- Grade: BANKED-MEASURED (tests/core/recorded_run.txt).

### T-03 B-007 descriptor/span audit pattern
- Source: tests/core/test_b007.c (70/0); audit tests/core/AUDIT_B007_C004.md.
- Sizes: allocator size classes exercised through descriptor state checks (state LIVE, class index in range, span live-count).
- Lifetime: allocations held while metadata is inspected; correctness pattern, not a timing pattern.
- Thread pattern: single-threaded. Serves class 1 characterization of metadata placement (Phase 11 metadata-placement question).
- Grade: BANKED-MEASURED (recorded runs in make test).

### T-04 C-005 contract-pressure pattern
- Source: tests/core/test_c005.c (95/0); notes tests/core/NOTES_C005.md.
- Sizes: calloc product forms plus fixed 256-byte blocks; zero-size and overflow-boundary probes inherited from the contract.
- Lifetime: allocate/verify/free cycles under contract assertions; pressure comes from breadth of cases, not volume.
- Thread pattern: single-threaded. Serves class 1 (varied distributions leg).
- Grade: BANKED-MEASURED.

### T-05 I-series pressure and fragmentation legs (Phase 9)
- Sources: tests/core/test_i003.c (trim), test_i004.c (fault), test_i005.c (trim under pressure + raw-mmap fragmentation leg E), test_i006.c (fragmentation + reclaim overhead), test_i007.c, test_i008.c, test_i009.c exit packet.
- Sizes: I-006 builds a stated fragmented state from classes 64 / 256 / 1024, exactly 3 spans each, odd-indexed blocks freed; a class-128 request must take a fresh span while a class-64 request reuses a hole.
- Lifetime: long-lived survivors interleaved with freed holes; trim/release measured against the fragmented state (docs/PHASE_9_FRAGMENTATION_AND_RECLAIM.md).
- Thread pattern: single-threaded. Serves class 5 directly and class 4 (mixed lifetimes).
- Consumer: K-006 (fragmentation/trim) and K-004 sizing work.
- Grade: BANKED-MEASURED (recorded_run_i003..i009.txt).

### T-06 D-004 multithread phases A-D (the cross-thread trace)
- Source: tests/concurrency/test_d004.c; record tests/concurrency/recorded_run_d004.txt. 4 worker threads (Pi nproc), fixed iteration counts.
- Phase A private churn: 250 iterations/thread, 8-size rotation, calloc every 50 — same-thread alloc/free under concurrency.
- Phase B simultaneous live set: 16 live blocks/thread (64..1024 bytes by formula), barrier-held, cross-verified by other threads.
- Phase C cross-thread free: 50 blocks/thread published and freed by a different thread (hook C-009 / MA-6.2-054 by analogy).
- Phase D shared hammer: 200 iterations/thread of fixed-seed xorshift sizes 16..~1024 against the shared allocator, realloc every 25.
- Serves classes 2 and 3 — the ONLY banked cross-thread-free and thread-churn patterns in the tree.
- Consumer: K-005 (concurrency/false-sharing) and K-007.
- Grade: BANKED-MEASURED.

### T-07 J-008 bench pattern (the banked microbenchmark shape)
- Source: tests/core/bench_j008.c; record tests/core/recorded_run_j008.txt.
- Shape: small64 = 20000 x malloc(64)/memset/free; large4096 = 2000 x malloc(4096)/memset/free; optional stats snapshot every 1000 ops.
- Lifetime: immediate free, zero retention — pure fast-path latency shape. Serves class 1 and the small leg of class 4.
- Grade: BANKED-MEASURED (rows re-measured on the merged tree at J-008 banking).

### T-08 F-series Mojo compile runs (compiler-adjacent, NOT a traced build)
- Sources: tests/mojo/test_f003.mojo..test_f005.mojo, run_f006_compare.sh, run_f007_upstream_probe.sh; records recorded_run_f003..f009.txt.
- What they actually are: repeated mojo build invocations of a small representative program (F-006: timeout 180 mojo build, repeated numbered runs, with/without comparison) proving the Mojo toolchain runs and the allocator-adjacent probes link/run — NOT an allocation trace of a compiler build.
- Pattern extractable: process startup/shutdown repetition (class 5 leg) and real compiler-process existence on the Pi.
- Grade: BANKED-MEASURED for the compile runs themselves; BANKED-AUTHORED-ONLY as an allocation pattern (no allocation sizes or lifetimes were recorded from the compiler).

### T-09 Perf probes G-001 / H-002 / H-004 / H-007 / H-008
- Sources: tests/perf/probe_g001.c, probe_h002.c, probe_h004.c, probe_h007.c, probe_h008.c with recorded runs.
- H-002 is the Phase 8 cross-thread/contention baseline record (global-lock architecture greps recorded in-run).
- Serves classes 2/3 characterization alongside T-06; Grade: BANKED-MEASURED.

## 3. Compiler-build-like trace — MODELLED

Label: MODELLED. No banked record at this base measured a real
compiler build allocation trace. The F-series (T-08) proves mojo
build runs on the Pi; it recorded no allocation sizes, lifetimes
or thread behaviour from inside the compiler. Claiming otherwise
would be inventing a measurement.

Composed pattern (from T-01/T-02/T-04 shapes + the class-6
signature in Section 1):
- Many small short-lived objects (tens of bytes to ~256 B): symbol
  names, token strings, small AST nodes — alloc/free churn closest
  to T-07 small64 and T-04 varied-distribution legs.
- Medium objects (~1 KiB..64 KiB): per-unit buffers and tables,
  living for one compilation unit, then freed in a burst at unit
  end — burst lifetime, approximated by T-02 live-set + mass free.
- A minority of long-lived objects (whole-build tables) — the
  mixed-lifetime leg of T-05.
- Mostly single-process, largely single-threaded per invocation
  in the banked F-series runs; parallel-build thread behaviour
  is NOT banked and is owed, not assumed.
- K-slice use: K-002 class-6 leg and K-004 fast-path sizing test
  this model; it is confirmed or refuted only when a real build
  is traced under a future slice with integration proven
  (roadmap class 6 carries that condition itself).

## 4. Aesir-like trace — MODELLED / OWED to Phase 12

Label: MODELLED, and mostly OWED. Aesir allocation behaviour is
Phase 12 territory by the roadmap itself (Phase 12 — Aesir
model-workload integration: track where Aesir actually allocates:
model loading, GGUF metadata, tokenizer, KV cache, prompt
prefill, decode and teardown; separate mmap weight residency,
CPU buffers and GPU allocations; compare Qwen3-1.7B Q4_K_M on
supported devices under identical model hash, prompt, context,
thread count, backend and thermal state).

What banked docs actually state at this base: only the roadmap
demand above and the comparator doc caution (docs/
ESTABLISHED_ALLOCATOR_COMPARATORS.md: its probes prove nothing
about Aesir; no Aesir consumer is proven at this base). No banked
record contains an Aesir allocation size, lifetime, or count.
This document invents none.

Characterization held for Phase 12 to measure, stated as shape
only: (a) load phase — very large, long-lived weight-adjacent
allocations, much of it mmap residency that is NOT allocator
traffic and must be separated per the roadmap; (b) prefill —
burst of medium/large activation and KV allocations; (c) decode
— repeated small-to-medium per-token churn beside a growing,
long-lived KV cache (mixed lifetimes, class 4 shape); (d)
teardown — near-total release, a class 5 startup/shutdown leg at
large scale. Every quantity in (a)-(d) is OWED to Phase 12
measurement; the shapes are hypotheses from the roadmap text,
not observations.

K-slice use: K-008 (Aesir-trace readiness) consumes this section
as its owed-evidence list; no Phase 11 optimization may cite an
Aesir number from this document, because there are none.

## 5. Gaps and the INC-K003 register

INC-K003-01 (held-slice evidence, not banked): the K-002 baseline
suite exists as commit cbb6c0f2d023572e83839bb394e4d4be3f2e6a96 on
branch yrsa/mimiralloc-02 (team-02 worktree, read-only citation).
It is built and recorded there but is NOT banked into this base
(5324991). Its 4 workloads (small_churn, mixed_batch, large_batch,
mixed_lifecycle; MimirAlloc losing all 4 at median in its record)
are citable as HELD-SLICE evidence only. Any Phase 11 document at
this base that calls them banked is wrong; they become banked
evidence only when the keeper banks K-002.

INC-K003-02 (no real compiler trace): class 6 has no banked
allocation trace; Section 3 is MODELLED. Closes only when a real
Mojo/compiler build is traced with integration proven.

INC-K003-03 (no Aesir trace): class 7 has no banked evidence at
all; Section 4 is MODELLED/OWED to Phase 12 by roadmap design.

INC-K003-04 (no tablet soak trace): class 8 has no banked pattern
(no physical-tablet run exists in the tree); carried with the
Phase 4 Android evidence debt below.

Carried-owed, restated unchanged:
- Phase 3 exit UNMET (docs/PHASE_3_EXIT_EVIDENCE.md): the x86-64
  leg of the portable suite is owed in full; trace claims in this
  document are ARM64-Pi claims only.
- Phase 4 exit UNMET (docs/PHASE_4_ANDROID_BUILD_PLAN.md and
  PHASE_4_ANDROID_DEVICE_PACKET.md): a physical Android run is
  owed; no trace here is an Android trace.
- INC-I001-14: soft-budget enforcement is not adopted anywhere
  in src/ or include/; no workload in this catalogue exercises a
  budget boundary, because none exists to exercise.
- INC-I009-01: docs/SUPPORTED_TARGETS.md freshness is owed; the
  target claims a trace reader might draw from it are stale-
  flagged there and in docs/PHASE_9_EXIT_EVIDENCE.md.

## 6. Who consumes which trace

| Trace | Primary K-slice consumer | Class(es) |
|---|---|---|
| T-01, T-04, T-07 | K-004 fast paths / sizing | 1 |
| T-06, T-09 | K-005 concurrency, false sharing | 2, 3 |
| T-02, T-05 | K-006 fragmentation, trim, mixed lifetimes | 4, 5 |
| T-03 | K-004/K-006 metadata placement | 1, 5 |
| T-08 + Section 3 | K-002 held suite, class-6 leg | 5, 6 |
| Section 4 | K-008 Aesir readiness (owed list) | 7 |
| (none banked) | K-007 tablet/thermal readiness | 8 |

End of K-003. One file, doc-only, against base 5324991.
