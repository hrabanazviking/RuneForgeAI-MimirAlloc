# Phase 4 - Multithread Run (Pi Half) - Slice D-004 (Wave 4)

Slice: **D-004 (team-04)**. Wave 4, Phase 4 exit evidence, Pi half.
Deliverables: `tests/concurrency/test_d004.c`,
`tests/concurrency/recorded_run_d004.txt`, additive `Makefile`
wiring, and this summary. Base: `f5965c1d8adf7b8951fe4975befd1f5c47e172f4`
(D-003 banked) on branch `yrsa/mimiralloc-04`, worktree
`/home/pi/mimiralloc-team-04` on Raspberry Pi `purpleviking1`.
`README.md` was not touched by this slice.

> **PHASE 4 EXIT STAYS UNMET.** This slice is the Pi
> (Linux AArch64 + glibc) half of the Phase 4 physical-device
> evidence only. The Android physical-device half remains
> OWED per the D-001 foundation plan: there is still no
> Android NDK build and no MimirAlloc run on any physical
> Android device in evidence. A Pi run does not pass the
> Phase 4 gate on its own, and no target is promoted to
> supported by this slice.

## 1. What was run

Program: `tests/concurrency/test_d004.c` (461 lines), built
with `-std=c11 -Wall -Wextra -Werror -pthread` against the
public `mimir_` API in `include/mimir_alloc.h`. Bounded by
design (assignment cap 8 threads): 4 worker threads (host
`nproc` is 4), fixed iteration counts, request sizes at
most 8192 bytes, seconds-scale runtime.

Phases, in order, all threads concurrent unless noted:

- **Phase A - private churn:** each thread does 250
  iterations of `mimir_malloc` / fill / verify / `mimir_free`
  with a per-thread, per-iteration pattern, plus a
  `mimir_calloc` zero-check every 50 iterations (5 per
  thread). No block is shared.
- **Phase B - simultaneous live set + cross-verification:**
  each thread allocates 16 blocks, fills each with a pattern
  unique to (thread, slot). After a barrier each thread
  verifies the NEXT thread's 16 live blocks read-only; after
  a second barrier it re-verifies and frees its own. The
  main thread then checks all 64 simultaneously-live recorded
  pointers were pairwise distinct. Every live block is
  pattern-checked before its free.
- **Phase C - cross-thread free:** each producer thread
  allocates 50 blocks, fills and verifies them, and publishes
  them; after a barrier the NEXT thread verifies each block
  again and frees it - blocks allocated on one thread are
  freed on another. Each consumer then makes one fresh
  allocation (fill / verify / free) to show subsequent
  allocations remain valid.
- **Phase D - shared concurrent phase:** all threads run
  200 further iterations against the shared allocator with
  per-thread fixed-seed xorshift sizes (16..4096 bytes);
  every 25th iteration is a `mimir_realloc` grow (128 -> 512)
  with the preserved prefix verified before the grown block
  is re-filled, verified, and freed.

Counting discipline: each worker accumulates checks,
failures, allocations and frees in its own result struct;
the main thread sums after `pthread_join`. No counter is
shared while threads run. Every loop bound, size and pattern
is a fixed function of (phase, thread, iteration), so the
printed totals are deterministic for a given build.

Commands recorded in `tests/concurrency/recorded_run_d004.txt`
(625 lines): full `make clean && make test` including
`test_d004`, plus a timed standalone `./build/test_d004`.
Host recorded there: `Linux purpleviking1 6.12.112-v8-16k-hailo+`
`aarch64 GNU/Linux`, `getconf PAGESIZE` 16384, `nproc` 4,
`cc` Debian 12.2.0, glibc 2.36.

Makefile wiring is additive only: `test_d004` added to
`all` and `test`, a `test-d004` standalone target, build
rules for `tests/concurrency/test_d004.c`, and `.PHONY`
updated. No existing target was removed or changed in
behaviour.

## 2. Results - recorded run and finisher re-run

Recorded run (2026-11-06, in the recorded-run file):

- Standalone `test_d004`: CONFIG threads=4 iter_a=250
  live_b=16 cross_c=50 iter_d=200 host_page_size=16384.
  Per thread: checks=1147 failures=0 allocs=522 frees=522
  (all four threads identical). Phase B distinct live
  pointers: ok (64 recorded). Alloc/free balance:
  2088 / 2088 ok. **CHECKS: 4590 run, 0 failed.
  ALLOCATIONS: 2088 successful, 2088 frees. RESULT: PASS.**
  Wall 0.029 s. The 2088 total includes 32 realloc grows
  (8 per thread in Phase D), each counted as one alloc
  and one free of the result.
- Full suite: `make clean && make test` exit 0, every
  binary PASS including `test_d004`.

Finisher re-run (2026-11-06, this slice's completion pass,
run on the same Pi in the same worktree):

- `make test` (after `rm -rf build`): **exit 0**. All
  suite RESULT lines PASS: test_core 29/0, test_b007 70/0,
  test_c005 95/0, test_c006 23/0 (suite-line counts as
  printed), test_c007 122/0, test_c008 42/0 with 7
  skipped-not-yet, and `test_d004` 4590/0.
- Standalone `./build/test_d004`: **exit 0, RESULT: PASS,
  CHECKS 4590 run / 0 failed, ALLOCATIONS 2088 / 2088,**
  per-thread 1147 / 0 / 522 / 522 - identical deterministic
  counts to the recorded run. Wall about 0.037 s in this
  re-run (recorded 0.029 s; wall time is a record, never
  an oracle - see INC-D004-03).

No count differed between the recorded run and the
finisher re-run, so the recorded-run file stands as-is
without correction.

Self-test / tester verdict: **PASS (documented self-test).**
Checklist: (1) landed file hashes re-verified in this pass
against the keeper-verified values; (2) spec re-read from
`hidden_files/ASSIGNMENT.md` and the source audited against
it (threads <= 8, pattern-verify before every free in all
phases, cross-thread phase, shared concurrent phase,
deterministic counts - all present); (3) full suite and
standalone test re-run by the finisher with exit 0 and
identical counts; (4) Makefile diff inspected - additive
D-004 wiring only. No separate tester worker was used;
this is a sequential finisher self-verification, stated
honestly as such.

## 3. Inconsistencies - INC-D004-*

- **INC-D004-01 - No direct contract for thread-safety.**
  The namespaced `mimir_` API has no direct contract rows
  (U-03 open; `docs/contracts.md` records that the Roadmap
  Section 6 text does not name core-function thread-safety).
  This test is therefore implementation evidence BY ANALOGY
  to MA-6.2-054 / hook C-009 (cross-thread ownership) and
  the Phase 3 C-007 cases ERR-211/ERR-212. It proves what
  THIS build did under concurrency on THIS host and settles
  no contract question. Thread-safety of this implementation
  rests on the one global lock consumed from L6 (B-006;
  `src/platform/mimir_platform.c`).
- **INC-D004-02 - One host, one page size measured.**
  All runs are on this Pi at page size 16384. No 4096-page
  host run of this test exists; that measurement remains
  OWED (same class as INC-D003-02). Page-size arithmetic
  portability is D-003 evidence, not D-004 evidence.
- **INC-D004-03 - Bounded scale; no timing or contention
  oracle.** 4 threads, fixed small counts, sub-second wall.
  This is a correctness-under-concurrency test, not a
  stress, soak, scalability or performance test. No timing
  value in either run is an oracle and no performance
  conclusion is drawn. Higher thread counts (up to the
  assignment cap of 8) and larger schedules were not run.
- **INC-D004-04 - Android half OWED; Phase 4 exit UNMET.**
  No Android build, no Android device run, and no bionic /
  musl / Termux / proot evidence exists for this test.
  Per D-001, the Android physical-device half of Phase 4
  remains OWED and Phase 4 exit stays UNMET until it exists.
  Nothing in this slice is worded as Android evidence.
- **INC-D004-05 - Original lead incomplete; finisher
  completion.** The original D-004 lead timed out with the
  test, recorded run and Makefile landed but uncommitted
  and this summary doc missing. This slice was completed
  by a finisher pass that re-verified hashes, re-ran the
  evidence, wrote this doc, and made the single commit.
  The recorded-run file is the original lead's run; the
  finisher re-run in Section 2 is separately identified
  and is not merged into that file.

## 4. Honest scope

Claimed: on 2026-11-06, on Raspberry Pi `purpleviking1`
(Linux AArch64, glibc 2.36, page size 16384), the banked
Phase 3 allocator build passed a bounded 4-thread
allocate / fill / verify / free test - private churn,
cross-verified simultaneous live set, cross-thread free,
and a shared concurrent hammer with realloc grows -
4590 checks, 0 failures, 2088 allocations and 2088 frees,
inside a full `make test` suite at exit 0. Both the
recorded run and an independent finisher re-run produced
those identical deterministic counts.

Not claimed: any Android result, any 4096-page host result,
any contract settlement on thread-safety, any performance,
stress or soak result, any fork / signal-handler safety,
and Phase 4 exit. Phase 4 exit stays **UNMET**.
