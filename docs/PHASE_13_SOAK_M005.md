# Phase 13 — Long Soak + Repeated Cycles (M-005)

Slice M-005, Wave 13, Phase 13 (SRC-07 soak evidence). Team-05 worktree
on Mimir (purpleviking1, aarch64), base
721e518eea707ed6f790e2ea98704628fb037e8c (M-002 bank). Recorded run:
tests/perf/recorded_run_m005.txt (2026-11-07T15:42:29-05:00).

## Baseline before the soak

`make test` on this base, run on the Pi before the soak:
**CHECKS: 4590 run, 0 failed**, exit 0 (log /tmp/m005_maketest.log).
`nm -D --defined-only build/libmimiralloc.so` = exactly **4**:
mimir_calloc, mimir_free, mimir_malloc, mimir_realloc.

## Entry point

Makefile untouched. The runner tests/perf/run_m005.sh (100755) is the
entry point: a Makefile target append was not purely additive-safe
against the M-003/M-004 parallel lines, so the slice ships standalone,
in the L-004..L-008 pattern.

## Design (bound stated in advance)

Main leg (probe_m005.c mode `main`, object-linked, production flavour
so no debug quarantine pins spans): 10 cycles x 20,000 mixed
iterations through the public mimir_ API only (malloc / calloc /
realloc / free, explicit adoption), 256-slot live set, small classes
(16..2048 B table) + calloc leg + large path (8192..131072 B),
10 s light pacing between cycles, runner timeout 900 s; design bound
total soak wall-clock <= 20 minutes. Dlopen leg (mode `dl`): 6
load/unload cycles against build/libmimiralloc.so, public symbols via
dlsym only, G-008 workload shape (32x256 B + 8x1024 B + 1x8192 B
calloc + one realloc grow 256->512 per cycle).

Observables (banked test-visible route, named exactly):
mimir_core_stats_snapshot fields bytes_cached, bytes_mapped,
live_small_objects, live_mappings, live_spans, peak_mapped,
alloc_failures (tests/core/mimir_stats_j002.h);
mimir_core_retained_spans / mimir_core_retained_bytes /
mimir_core_trim_retained (tests/core/mimir_trim_i003.h);
process VmRSS from /proc/self/status.

Bounded-memory band (derived from cycles 1-2, never widened):
post-trim retained bytes == 0 every cycle; post-cycle bytes_mapped
<= max(c1,c2) + 1 page; post-cycle RSS <= max(c1,c2) + 512 kB.
Derived values in the run: page = 16384 B (16K-page kernel), base
mapped 0 -> band 16384; base RSS 1392 kB -> band 1904 kB.

## Results (from the recorded run, verbatim figures)

Main leg — **PASS**, all 10 cycles within band:

- Per-cycle ops 15,878..16,018 API calls (20,000 iterations; pure
  verify iterations are checks, not ops); checks 19,583..19,899 per
  cycle; live high-water 2,606,544..3,308,560 B per cycle.
- Every cycle boundary: full live-set pattern verification passed,
  all blocks freed, trim released 19..22 retained spans
  (311,296..360,448 B pre-trim), post-trim retained spans/bytes =
  0/0, post bytes_cached = 0, post bytes_mapped = 0, live counts 0.
- RSS flat at 1392 kB in all 10 cycles; peak_mapped plateaued at
  6,963,200 B from cycle 4 and never grew again.
- Accounting: total_ops=159,476, total_checks=197,219,
  total_bytes_checked=1,589,149,744, failures=0, alloc_failures=0,
  final state clean (live 0, retained 0).

Dlopen leg — **PASS**, 6/6 cycles: image mappings after each dlclose
= 0; anonymous rw bytes after close = 49,152 every cycle, growth
0 B/cycle vs the cycle-0 close level (H-001 evidence shape;
INC-H001-01 stale-growth context not regressed). total_ops=498,
total_checks=246, failures=0, library unloaded after each cycle.

Soak wall-clock: 94 s total (90 s designed pacing + ~4 s aggregate
churn work — the churn is fast; the bound and cycle structure were
run exactly as stated in advance, and the pacing is what makes the
load Samtal-light). Samtal :8787 sampled every ~2 s by the runner
for the whole run: **47 samples, 0 non-200** (no non-200 lines to
report). free -g: 13 G available at start and at end; Samtal 200
before and after.

## Incidents

- INC-M005-01 (minor, closed in-slice): the first compile of
  probe_m005.c failed under -Werror=misleading-indentation in the
  dlopen leg (a statement sharing the line of a for-loop). Fixed by
  splitting the statement; the shipped probe builds warning-free.
  Probe-only; no library code involved.
- No other incidents. No band was widened; no failure was hidden;
  the failure count is zero because the recorded run shows zero.

README.md and include/mimir_alloc.h are byte-identical to the base;
this slice adds only the four m005 files.
