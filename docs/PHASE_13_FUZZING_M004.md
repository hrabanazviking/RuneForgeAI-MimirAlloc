# Phase 13 Fuzzing Harness - M-004

Slice: M-004 (Wave 13, Phase 13 - security and production release)
Team: team-04, branch yrsa/mimiralloc-04
Base: 721e518eea707ed6f790e2ea98704628fb037e8c (M-002 bank, Wave 13 = 2 of 9)
Host: Raspberry Pi Mimir (purpleviking1), aarch64, cc (Debian 12.2.0-14+deb12u1)
Date: 2026-11-07. Recorded run: tests/fuzz/recorded_run_m004.txt
Roadmap Section 10 (verbatim requirement): "Fuzz sequences such as
allocate, write, reallocate, transfer, free, trim, and thread exit
against a simple ownership model. Save seeds and version every
trace. Use differential comparison of defined observable behavior,
not identical pointer addresses or timing."
Baseline before building: make test exit 0, final suite 4590 run /
0 failed (full suite list in the recorded run). No measurement is
claimed that was not run on the Pi this tick.

## 1. Deliverables
- tests/fuzz/fuzz_m004.c - the harness (v1, trace version 1).
- tests/fuzz/run_fuzz_m004.sh (100755) - runner; timeout 120 cap.
- tests/fuzz/recorded_run_m004.txt - the recorded run below.
- Makefile: test-fuzz-m004 target appended ADDITIVELY at the end
  (standalone; make test composition unchanged). The harness links
  the banked LIBOBJS directly, the same route as the banked
  test-visible tests, so no new public surface is created.
- No src/, README or public-header change.

## 2. The ownership model and the observables
Independent model: a 128-slot table of live blocks, each holding
the returned pointer, the REQUESTED size, the owner thread id and
a per-block pattern byte derived from a serial number. Model and
allocator are checked against each other after every batch of 16
operations and at seed end:
1. Pattern integrity: every byte of every live block still equals
   its model pattern byte.
2. No overlap: the requested extents [ptr, ptr+size) of live
   blocks are pairwise disjoint (pointers sorted, extents compared).
3. Live count: model live count equals the allocator-visible live
   count. The lawful observable used is the banked test-visible
   statistics route: live_small_objects + live_mappings from
   mimir_core_stats_snapshot, re-declared for tests only in
   tests/core/mimir_stats_j002.h (J-002, INC-E001-07 shape (b),
   reachable only by linking the library objects directly; bound
   local in the shared library, nm -D stays exactly 4 - verified
   in the recorded run). Trim calls use the same-shape test route
   tests/core/mimir_trim_i003.h (I-003). Audit counters are NOT
   read through any other channel.
Differential statement: comparisons are of defined observable
behaviour only - pattern integrity, calloc zero-fill, the live
count above, preserved-prefix bytes on realloc, and success /
failure SHAPE (lawful sizes succeed; failure-shaped sizes return
exactly NULL with the original block intact). Pointer identity,
pointer addresses (used only for the overlap sort, never compared
against an expected value) and timing are never oracles.

## 3. Sequences, seeds and bounds
PRNG: xorshift64star seeded per seed; same seed -> same sequence
(proven: two fresh full runs diff identical, recorded run file).
Operation mix (per 100 draws): alloc 28 (sizes from an 18-entry
table, 1 byte .. 64 KiB, spanning the span and mapping paths),
write/verify 14, realloc 16 (grow/shrink/same-table sizes; 1 in 10
failure-shaped: realloc to SIZE_MAX must return NULL with the
original intact), calloc 10 (zero-fill checked; 1 in 10 overflow
calloc must return NULL), free 15 (plus free(NULL) 2), trim 4
(full batch check immediately after each trim), cross-thread
transfer+free 6 (a spawned thread verifies the pattern and frees
a block it did not allocate), thread-exit handoff 3 (a spawned
thread allocates 2 blocks, fills them and exits voluntarily with
them live; main adopts them into the model), full verify 2.
Seeds (FIXED, trace version 1): 4001, 4002, 4003 single-threaded
at 2000 operations each; 4004 multithreaded (below). Bounds:
2000 ops/seed, 128 live blocks max, runner timeout 120 s; the
whole recorded run took 2 s wall on the Pi. Open-ended soak is
M-005 territory and is NOT done here.
Multithread seed 4004: 3 threads (<= 4), barrier-phased so the
counts are deterministic. Phase 1: 300 seeded own-block ops per
thread (alloc/write/free). Phase 2: each thread transfers about
half its live blocks to the next thread (single writer per
inbox). Phase 3: adopt and verify inbox blocks, free every 3rd
adopted block (free by non-owner), keep the rest. Phase 4:
threads exit voluntarily with the rest live, handed to main,
which adopts them into the model, batch-checks (patterns,
overlap, stats live count == handed count), frees all and trims.
Postures per H-005/H-006 evidence: voluntary return exit only;
no async cancellation mid-call; no fork legs (fork stays
BLOCKED-by-U-17..U-21, exactly as the banked suites record).
The MT leg has no realloc class (realloc is covered by the three
single-thread seeds); its line reports realloc=0 honestly.

## 4. Recorded results (recorded_run_m004.txt)
| Seed | Ops | Checks | Fails | Live at end |
|---|---|---|---|---|
| 4001 | 2000 (alloc 586, write 295, realloc 301, calloc 180, free 296, trim 80, xfer 121, handoff 59, verify 38, free-NULL 45) | 57687 | 0 | 0 |
| 4002 | 2000 (alloc 561, write 280, realloc 302, calloc 192, free 308, trim 88, xfer 122, handoff 59, verify 40, free-NULL 48) | 58217 | 0 | 0 |
| 4003 | 2000 (alloc 563, write 287, realloc 307, calloc 199, free 300, trim 72, xfer 134, handoff 59, verify 41, free-NULL 40) | 52266 | 0 | 0 |
| 4004 MT | 3 threads (alloc 420, write 206, free 290, xfer 44, handed 70) | 214 | 0 | 0 |
Totals: 168384 checks, 0 failed; every seed ends with the model
empty and the allocator live count at 0 after the final drain and
trim. Reproducibility: two fresh full runs byte-identical.
Single-seed reproducer modes: ./build/fuzz_m004 4001 2000 and
./build/fuzz_m004 mt 4004 (both exit 0 in the recorded run); the
op-count argument bisects a failing prefix - the minimisation
procedure had no failure to apply to this tick (Section 5).
Gates after the slice: make test exit 0 (4590 run / 0 failed in
the final suite), make lib exit 0, nm -D on libmimiralloc.so =
exactly 4 T symbols (mimir_malloc, mimir_free, mimir_calloc,
mimir_realloc). Samtal on the Pi answered HTTP 200 before and
after the work; free memory stayed at 13G available class.

## 5. Findings and INCs
No mismatch was found: no FINDING line was emitted by any seed,
so there is no failing prefix to minimise and no reproducer to
record this tick. The harness is deliberately not tuned to this
outcome: a mismatch prints FINDING seed/op/class verbatim, stops
that seed at the failing batch, and the bisection mode above is
the recorded minimisation road.
- INC-M004-01 (coverage limit, honest): failure-shaped realloc
  and calloc are exercised, but OS-level backing failure (mmap
  refusal) is not injected - no fault-injection shim exists on
  this base (the same gap the banked suites record as
  INC-C007-01). Fuzzing under injected backing failure is owed
  to whichever slice builds that shim.
- INC-M004-02 (coverage limit, honest): the MT leg is
  barrier-phased for deterministic counts, so it does not fuzz
  arbitrary interleavings; unsynchronised contention fuzzing
  belongs with the M-005 soak design, bounded there.
No other INC: no bug or inconsistency was discovered in the
allocator, the model routes, or the Makefile by this slice.
The one harness-side defect found and fixed during the build
(strict-C11 pthread_barrier visibility and -Werror indentation
warnings in the harness itself) was in the new test file only.
