# Z-115 - Aesir KV-cache-shaped allocation specialisation: LOSS on all shapes

Status: COMPLETE (2026-11-13). Run of record: tests/perf/recorded_run_z13_z115.txt (2026-11-13T12:40:53-05:00, low-load window, recorded as such). Standing correctness gate GREEN - counts in team CAMPAIGN_STATE.md.

## What was asked (WAVE_PLAN Wave 13 pool choice)
KV-cache-shaped allocation pattern measured on this allocator vs both glibc columns (default and tuned, per Z-098/Z-107 practice), Aesir tree read-only throughout, Z-087 blockers B-087-1/2/3 re-checked against the run. Reason: roadmap pool; the Z-087 boundary trial filed the blockers.

## What was built
tests/perf/probe_z115.c (synthetic KV pattern: kv_append realloc growth 32 seqs x 64 x 4 KiB; kv_blocks resident 256 x 64 KiB pool + 4 KiB FIFO-ring churn; kv_mixed resident 128 x 256 KiB + append growth + eviction rotation), tests/perf/run_z115.sh (build, banked probe_z098 tunable engagement proof, blocker identity re-check, interleaved battery, Aesir boundary snapshots), tests/perf/summarize_z115.py (ops/checksum uniformity asserted per shape, median ns/op, 5% grades vs both columns), Makefile test-z115 (standalone, gate counts untouched). Tuned column: GLIBC_TUNABLES gcombo_cache (tcache_count=64:tcache_max=65536), the Z-098/Z-107 tuned winner for large-block shapes.

## Result
LOSS on every shape against both columns (median ns/op): kv_append mimir 2496.52 vs default 545.94 (4.573x) and tuned 508.88 (4.906x); kv_blocks 4020.76 vs 15.41 (260.919x) and 10.16 (395.744x); kv_mixed 4068.22 vs 1056.25 (3.852x) and 1056.74 (3.850x). No specialisation is adopted: the pool item asked for the measure and the measure is a loss. The kv_blocks factor carries the shape's arithmetic (ops count only the churn allocs; elapsed also carries the resident build) - filed as measured in the recorded run, never smoothed. The closest shape is the append path at ~4.6-4.9x, the starting point for any future KV work.

## Aesir boundary and blockers
No Aesir code ran and none is claimed. All three Z-087 blockers STILL STAND on unchanged artifacts (identity reads only): B-087-1 root aesir_main 645904 B, 2026-11-11 mtime, sha256 713f7a76...; B-087-2 aesir_engine/aesir_main 227456 B, same mtime, sha256 9b6bd709...; B-087-3 aesir_engine/model.gguf still 24 B. The fixture of record is byte-identical to Z-087 (sha256 7b064f58... verified). Aesir git status was UNCHANGED across the battery; head movement during the slice's wider window is the Aesir campaign's own live work, named in the recorded run.

## Limits, stated plainly
Synthetic shapes modelled on KV-cache behaviour, not a trace of a real serving run; sizes are one plausible mid-model geometry. The battery ran at loadavg 0.22 - a low-load window, not a Z-110-grade verified quiet window. One process per shape, single-threaded: no multi-sequence concurrency contention is priced. Defects: none found.
