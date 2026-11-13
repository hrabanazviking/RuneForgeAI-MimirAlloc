# Z-116 - Energy-proportional idle behaviour

Status: COMPLETE (2026-11-13). Run of record: tests/perf/recorded_run_z13_z116.txt (12:44:59-12:50:12 EST, light window, load recorded). Standing correctness gate GREEN 4988/0/77 - counts in team CAMPAIGN_STATE.md.

## What was asked (WAVE_PLAN Wave 13 pool choice)
Energy-proportional idle behaviour. Reason: roadmap pool; the Z-007/Z-107 idle-subtraction method gives a proven idle baseline on this host. Success measure: post-load idle retention/trim behaviour measured over time, idle cost quantified, any busy-retention defect fixed in scope or filed. Boundary: the Z-110 quiet-window energy re-run hand-off remains OPEN and is not this slice; no Z-107 energy grade is re-filed here.

## What was built
tests/perf/probe_z116.c (one source, mimir + glibc legs), tests/perf/run_z116.sh, tests/perf/summarize_z116.py. Per leg: baseline sample, a verified 832-block load (49.7 MiB live), free-all, then a 120 s idle window sampled every 10 s with no allocator traffic, an explicit full trim, post-trim samples, and a sanity phase. Package power by the Z-007 method (perf stat -a, RAPL power/energy-pkg/): pre-load, twice inside the retained-idle window, post-trim, post-glibc. Summariser PASS; both probe exits 0; cross-leg DONE checksum identical (326368).

## Result
Retention after free-all is bounded and small: retained empty spans 229376 B, mapped 2727936 B total, RSS +4456 KiB over baseline on a 49.7 MiB live peak (glibc context: 8.58 MiB arena-held, RSS +9068 KiB). Across the whole 120 s idle window the retained footprint is exactly flat in both legs (drift +0 B cached, +0 KiB RSS over 12 samples): no idle growth, no busy-retention, no autonomous return. One explicit mimir_trim(0) releases everything (cached and mapped to 0, VMA back to baseline 26, RSS -2600 KiB); the post-trim residual RSS is non-allocator process memory, proven by mapped=0 in the same sample. Idle power cost: package watts during the retained-idle window (7.23 / 6.62 W) sit below the pre-load sample (14.27 W) - holding the retained footprint adds no measurable package power over the host idle baseline in this window; the priced cost of untrimmed idle is the resident memory itself (0.22 MiB spans, 2.60 MiB mapped), held until a trim or a Z-050 policy tick. Verdict in roadmap words: idle behaviour is STABLE and TRIM-RESPONSIVE (WIN on bounded retention vs the glibc leg shape), and NOT autonomous - energy-proportional idle is opt-in by the Z-050 caller-driven design.

## Limits, stated plainly
One load shape, one 120 s window at 10 s resolution, one process per leg: the flat verdict is exact for this shape and window; Z-100 already prices retention stability at 8 h. The power comparison is package-wide (no per-process attribution exists on this host) and the pre-load sample ran higher than every later sample, so the watt delta is reported as below resolution rather than graded. No quiet-window claim is made, and nothing here discharges Z-110.

## Defects
None found in the allocator, the suite, or the harness. Filed observation with owner: an application that never calls mimir_trim and never ticks the Z-050 trim policy holds the drained residue for its whole life - by the Z-050 design of record (caller-driven, no background thread). Owner: API consumers; any autonomous idle-return change is a future design slice, not a fix owed here. Harness transfer note: run_z116.sh arrived byte-exact after repair of one re-emission slip in a build-fail label, sha256-verified before the run.
