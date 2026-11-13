# Z-110 - Quiet-window energy re-run: BLOCKED-CONTENTION record

Status: BLOCKED-CONTENTION (2026-11-13). No energy grade is filed by this slice. The Z-107 closing energy grades remain the grades of record; Z-108 verdict S5.2 and its quiet-window hand-off remain open.

## What was asked (WAVE_PLAN Wave 13 pool choice)
Re-run the Z-107 energy leg in a verified quiet window with load and co-tenants recorded; re-file the closing energy grades against both glibc columns (default and Z-098 tuned); control bounds never weakened.

## What was built
tests/perf/run_z110.sh - derived on-device from the banked tests/perf/run_z107.sh: its build, tunable engagement proof and energy leg verbatim (probe_z007 under sudo -n perf stat -a, RAPL power/energy-pkg/, legs glibc / glibc_tuned / mimiralloc, 5 procs per leg per workload, idle samples interleaved, reps 200/30/100/15, taskset -c 2), graded by the banked summarize_z107.py energy mode unmodified (median net nJ/op after idle subtraction; WIN < 0.95x, TIE 0.95-1.05x, LOSS > 1.05x; below-resolution references claim no ratio). A quiet gate was inserted before the energy leg: second top sample CPU idle >= 90% and no heavy co-tenant on the name screen, else exit 3 and measure nothing.

## Attempt 1 (~11:24 EST) - gate blocked, nothing measured
Engagement proof PASS. Idle 96.8%. The v1 name screen tripped on the resident ollama serve daemon (0.0% CPU - an idle daemon, part of this host's idle baseline in every recorded window) while the gate's top snapshot showed the real blocker: team-03's Z-111 probe_mimir at 99.8% CPU. Runner exited 3.
In-slice runner fix (gate v2, no bound weakened): the screen now excludes the ollama daemon by name and names the sibling binaries probe_mimir / probe_glibc. The 90% idle bound is unchanged.

## Attempt 2 (11:32:26-11:33:06 EST) - contended in fact, grades not adopted
Gate v2 passed at its instant (idle 96.8%, screen none), but the gate's own top snapshot seconds later shows team-03's v1 probe_mimir at 100% CPU - the sibling battery started inside the check window - and the leg-end snapshot shows it at 99.9% (0:47 CPU time). Idle samples across the leg: 23.0 / 23.5 / 20.9 / 27.7 / 24.0 W - not steady. The summariser's ENERGY RESULT: PASS is a measurement-validity pass (checksums true, idle baseline valid); its grades are preserved verbatim in tests/perf/recorded_run_z13_z110.txt and are NOT ADOPTED: this campaign does not grade energy from a contended run. They agree in direction with Z-107 (small_churn LOSS vs both columns, large_batch WIN vs default, mixed_batch and mixed_lifecycle adverse) - consistency, not a filing.

## Blocker and next step
Purely the shared host: team-03's Z-111 profile-guided trace battery ran continuously through this lead's whole window (11:15-11:35+ EST, load to 3.91, probes cycling v0/v1/wA over the z094 traces). No defect in the allocator, the suite, or the harness was found. When the host is verified quiet, one unchanged invocation of tests/perf/run_z110.sh plus the standing gate closes this slice and supersedes this record.
