# Z-113 - GPU-shared UMA pool behaviour (Wave 13 reserve slice 5)

Slice: Z-113, Wave 13 (verdict-driven reserve), pool choice recorded in
the seat WAVE_PLAN.md before the slice started.
Team: team-05, branch yrsa/mimiralloc-team-05, base merge e6aa53e of
seat main 58af001 (banked Z-001..Z-111).
Host: purpleraider (ASUS ROG Flow Z13, AMD Strix Halo, x86-64, 4 KiB
pages, 128 GB UMA shared between CPU and the integrated Radeon).
Recorded run: tests/perf/recorded_run_z13_z113.txt.

## 1. Why this slice, and its success measure

The Wave 13 pool choice for Z-113 is GPU-shared UMA pool behaviour:
this machines defining trait is UMA shared with the graphics stack,
and the Z13 pool/THP standings price the CPU side only. Success
measure (stated in WAVE_PLAN.md before the slice started): allocator
behaviour measured with and without a GPU co-tenant load on this
host, interference quantified and filed, no claim beyond the run.
Gate: that measure plus the standing correctness gate.

## 2. Deliverables

- tests/perf/probe_z113.c - per-workload driver around the BANKED
  bench_k002.c harness, included UNMODIFIED with main renamed (the
  probe_z005 pattern). No workload code of its own.
- tests/perf/run_z113.sh - two-phase runner (below). Standalone via
  make test-z113; NOT part of make test.
- tests/perf/summarize_z113.py - medians, standings, interference
  factors, checksum-uniformity proof.
- tests/perf/recorded_run_z13_z113.txt - header, summariser output,
  and the raw runner output verbatim (incl. GPU samples).
- Makefile - additive test-z113 block only. No src/, include/, or
  README change.

## 3. Method

Phases, same battery both times: the four banked bench_k002
workloads (small_churn, mixed_batch, large_batch, mixed_lifecycle),
5 processes per leg per workload, legs interleaved process by
process, taskset -c 2, reps per workload as in run_perf_z005.sh.
Baseline: no GPU co-tenant - the ollama model is unloaded first and
gpu_busy_percent reads 0 before the phase. GPU phase: a co-tenant
loop of ollama generate requests against
qwen2.5:7b-instruct-q4_K_M (4.7 GB) on the integrated Radeon.
Engagement is proven, not assumed: the battery starts only after
gpu_busy_percent >= 50 is sampled; samples every 2 s through the
phase read 94-98 percent; MemAvailable fell about 6.3 GB under load
(the UMA sharing itself, in fact). The model is unloaded after the
phase (keep_alive 0) and gpu_busy returns to 0. Checksums are
uniform per workload across all phases and legs - the measured
work is the banked K-002 work in every cell.

Load honesty: SHARED HOST, not a verified quiet window. Load at
start 3.53 (decaying from pre-run ollama activity and sibling
campaign work on this host); GPU-phase load 4-6 includes the
co-tenant loop and llama-servers own CPU work. Medians are the
headline defence; mixed_lifecycle spread is wide in both legs and
both phases and its factors are read as parity, not precision.

## 4. Results (medians, ns/op; recorded run is the record)

Standings mimir/glibc are unchanged in direction by the co-tenant:
small_churn LOSS 8.81x baseline, 9.19x under load; mixed_batch LOSS
7.56x / 7.83x; large_batch WIN 0.167x / 0.133x; mixed_lifecycle
LOSS 58.6x / 58.9x.

Interference factors (loaded median / baseline median, 5% bound):
- small_churn: glibc 1.072 LOSS, mimiralloc 1.118 LOSS
- mixed_batch: glibc 1.083 LOSS, mimiralloc 1.122 LOSS
- large_batch: glibc 1.499 LOSS, mimiralloc 1.195 LOSS
- mixed_lifecycle: glibc 3.157 LOSS, mimiralloc 3.172 LOSS

Findings, no claim beyond this run: (1) a real GPU co-tenant on
this UMA host degrades BOTH allocators on every workload - the
interference is a property of the shared memory system, not of
either allocator. (2) On the small/mixed paths the two legs pay
nearly the same proportional price (about +7 to +12 percent).
(3) On large_batch, the fault-heavy path, glibc pays proportionally
more (+49.9 percent) than mimiralloc (+19.5 percent), and mimirs
large-path WIN over glibc widens under load. (4) mixed_lifecycle
pays about 3.2x in BOTH legs equally - parity under a wide-spread
workload; the likeliest mechanism is memory-bandwidth/latency
contention on the lifecycle path, named here as interpretation,
not as a measured cause.

## 5. Bugs and limits

No allocator bug discovered; no allocator code touched. Limits:
one co-tenant shape only (LLM generation: large resident weights,
sustained compute, steady UMA traffic) - not a graphics-desktop,
gaming, or video shape; one run of record on a shared host, not a
quiet window; no claim beyond this run, per the success measure.

## 6. Gate

Standing gate scripts/z13/run_gate.sh GREEN 4988 checks / 0 failed
/ 77 by-design skips on the slice branch (counts read from
hidden_files/z113_gate.log). Slice runner exit 0, summariser
exit 0, checksum uniformity OK.
