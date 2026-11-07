# Phase 8 Stress — H-007

Slice H-007 (Wave 8). Governing: `docs/PHASE_8_FOUNDATION_PLAN_H002.md`
rows S6/S7 and carried O-05 — test at small thread counts AND under
oversubscription, with stress runs and independent data-pattern checks.
H-002 baseline already covered 1/2/4/8 threads (on this 4-core host,
8 threads is 2x oversubscription). H-007 widens repetitions and mixes,
and adds one bounded oversubscription shape above 8: 12 threads (3x).
No claim is made beyond 12 threads: nothing above 12 was run.

## Harness

`tests/perf/probe_h007.c`, standalone via `make test-h007` (NOT part
of `make test`). Per rep, every worker runs three mixes:

- A — cross-thread free ring: allocate 256 blocks (sizes cycle
  64/256/512 span path, 2048 mapping path), pattern-fill, barrier,
  then verify + free the next worker ring-neighbour blocks.
- B — own churn: 256 alloc/fill/verify/free iterations under
  contention (64/256 span, every 10th 2048 mapping).
- C — calloc/realloc mix: 128 calloc blocks (zero verified before
  fill), realloc grow to 2048 (mapping) or shrink/grow to 256,
  preserved prefix min(old,new) verified byte-for-byte, then free.

Independent pattern checks (H-002 S7 method): the verifying thread
re-derives each expected byte from (owner, index, phase) itself;
the allocator is never asked to vouch for its own blocks. Audit
counters (E-005/G-003 test-only counters, linked via LIBOBJS) are
read on the main thread at quiescent points only (all workers
joined): live small and live mappings must return exactly to the
baseline delta after every rep. Timings are printed, never judged.
Samtal spot-checks: a monitor thread issued 5 HTTP GETs to
127.0.0.1:8787 during each 12-thread rep — a probe, not a bound.

## Scenario table (recorded run, 2026-11-07, Pi aarch64, 4 cores)

Per rep ops = threads x (256 cross + 256 churn + 128 calloc/realloc).
All 15 scenarios: pattern_fail=0, null_fail=0, zero_fail=0,
realloc_fail=0, live_small_delta=0, live_map_delta=0, audit=OK.

| threads | reps | cross/churn/calloc-realloc ops per rep | failed reps |
|---|---|---|---|
| 1 | 3 | 256 / 256 / 128 | 0 |
| 2 | 3 | 512 / 512 / 256 | 0 |
| 4 | 3 | 1024 / 1024 / 512 | 0 |
| 8 | 3 | 2048 / 2048 / 1024 | 0 |
| 12 | 3 | 3072 / 3072 / 1536 | 0 |

Totals across the recorded run: 20,736 cross-thread verify+free
ops, 20,736 own churn ops, 10,368 calloc/realloc ops — 51,840
operations, 0 failures of any counted kind. Full per-scenario
lines: `tests/perf/recorded_run_h007.txt` (make test exit 0,
4590 checks 0 failed; make test-h007 exit 0; nm -D exactly 4).

## Oversubscription result

12 threads on 4 cores (3x oversubscription), 3 reps: all scenarios
OK, audit reconciled exactly after every rep, wall ~0.25 s per rep
in the recorded run. Samtal answered HTTP 200 on all 15 spot-check
GETs (5 per rep x 3 reps) issued during the 12-thread legs; Samtal
also 200 before and after the recorded run. This extends the
stress claim exactly as far as it was run: 12 threads. No claim
beyond 12 threads is made.

## Verdict and incidents

Correctness verdict, from the recorded counts only: no corrupted
live allocation and no lost ownership in any scenario — no pattern,
null, calloc-zero or realloc-preservation failure, and no audit
drift at any quiescent point. No allocator gap was found, so no
src/ change was made and no fix was needed.

INC-H007 list: none. (INC-H007-01 was reserved for the first gap
if one appeared; none did. A harness-only build fix during
development — missing sys/time.h / POSIX usleep in the probe
itself — was corrected in the probe before the recorded run;
it touched no allocator code and is not an allocator incident.)

## Phase 8 exit status — honest

Phase 8 is NOT exited by this slice. H-007 supplies the S6/S7
stress evidence (small counts + oversubscription, widened mixes,
repeated reps) toward exit row E1. H-008 (repeated-stress archive)
and H-009 (exit packet and grading) remain; the exit claim itself
is H-009 to grade on the full wave evidence.

## Boundaries held

README.md and include/mimir_alloc.h byte-identical to base
46110f1 (verified by git diff, empty). Makefile additive only
(one H-007 block). No src/ change. Pi load stayed light: the
whole probe finishes in seconds, memory free ~13 GiB throughout,
Samtal live before, during and after.
