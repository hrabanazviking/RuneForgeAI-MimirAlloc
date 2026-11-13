# Z-114 - Perf-per-watt tuning at the 70 W envelope: WIN (tuned vs untuned)

Status: COMPLETE (2026-11-13). Run of record: tests/perf/recorded_run_z13_z114.txt (12:19:15-12:20:45 EST, co-tenant window, recorded as such). Standing correctness gate GREEN - counts in team CAMPAIGN_STATE.md.

## What was asked (WAVE_PLAN Wave 13 pool choice)
Perf-per-watt at the 70 W envelope vs the untuned baseline on the oracle workloads, graded WIN/TIE/LOSS with the envelope method recorded. Reason: roadmap pool over the Z-007/Z-107 energy evidence base.

## Envelope method (the honest version)
This host cannot take a power cap: RAPL under /sys/class/powercap/intel-rapl:0 is measurement-only (no constraint_* files exist), and the amd-pstate EPP files refuse writes (EBUSY, probed). The one working lever is the amd-pstate frequency cap (cpupower frequency-set -u). The tuned configuration caps scaling_max_freq at 3.5 GHz, applied only around tuned batches and restored after each one (post-run read-back 5187500 kHz, the pre-run value). Envelope compliance is read from the run's own per-block gross package watts (perf stat -a, RAPL power/energy-pkg/), never asserted from the setting.

## What was built
tests/perf/run_z114.sh (derived from the banked run_z110.sh/run_z107.sh energy leg: same build, same probe_z098 engagement proof, probe_z007 workloads and reps, Z-007 idle-subtraction method), tests/perf/batch_z114.sh (14 parallel probe processes per block, one per physical core), tests/perf/summarize_z114.py (per-block checksum proof against the Z-003 per-pass checksums, net nJ/op, ops-per-net-joule, per-block gross package W, 5% PPW band). Legs interleaved block by block: mimir_untuned, mimir_tuned70, glibc untuned (context). 65/65 blocks, summariser PASS.

## Result
Perf-per-watt (ops per net joule, tuned70 vs untuned): small_churn 1.914x WIN, mixed_batch 1.722x WIN, large_batch 2.255x WIN, mixed_lifecycle 1.948x WIN. Untuned batches exceeded the 70 W envelope on every workload (median gross 72.5-80.9 W); tuned batches held it on every workload (medians 28.0-38.2 W, max block 39.7 W). Cost: aggregate throughput fell 25-27% under the cap on every workload - the tuning is a ceiling trade, not a free win. Against glibc untuned (context, net nJ/op) tuning narrows the allocator's energy gap without closing it: LOSS 4.05x / 3.88x / 24.06x, large_batch WIN 0.06x.

## Limits, stated plainly
Co-tenant window (Aesir mojo run_all live throughout; load and confound recorded in the recorded run): the cap is host-wide, so tuned gross watts also reflect throttled co-tenants, and untuned watts carry co-tenant burn - perf -a has no per-process attribution on this host. The PPW factors are this-run, this-method numbers. No quiet-window claim is made; Z-110's quiet-window energy re-run remains open and is not discharged by this slice. Calibration on an oversubscribed 28-proc shape pointed the other way (caps cost more time than power there); the graded shape is the stated 14-proc one-per-core batch, and both are on record.

## Defects
None found in the allocator, the suite, or the harness. Host findings filed above: RAPL measurement-only on this host (no powercap constraint files) and EPP writes refused (EBUSY) - platform facts, recorded for any future power slice.
