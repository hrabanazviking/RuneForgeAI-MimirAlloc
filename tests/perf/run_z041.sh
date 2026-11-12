#!/bin/sh
# run_z041.sh - Z-041 SMT and pinning policy runner (MimirAlloc Z13).
# Builds probe_z041 twice (production core = mimir leg; -DLEG_GLIBC
# = glibc leg) and runs the policy matrix under the Z-004 protocol:
#   cells: leg x workload (small_churn 50000/thread, mixed_batch
#     200 batches/thread) x threads (8, 16, 32) x mode (pinned,
#     floating); REPS separate processes per cell (default 5),
#     headline = median of processes (summarize_z041.py); one
#     discarded warm-up process per cell at 1/10 scale plus the
#     probe in-process 1/10 warm-up; 2 s pauses between cells.
#   pinned: worker t pins itself to CPU t inside the probe
#     (identity order 0-15 physical, 16-31 SMT siblings), verified
#     before and after the timed pass. floating: no affinity call;
#     the taskset -c 0-31 wrapper is an outer bound only, identical
#     for both modes, so placement is the scheduler own choice.
# Usage: run_z041.sh <tag> [reps]   (tag e.g. inv1 / inv2; raw
#   files land in hidden_files/z041_<tag>_<leg>.raw)
set -eu
cd "$(dirname "$0")/../.." || exit 2
TAG=${1:?usage: run_z041.sh <tag> [reps]}
REPS=${2:-5}
mkdir -p hidden_files build/z41
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
echo "=== Z-041 SMT and pinning policy (tag=$TAG reps=$REPS) ==="
date -Is; uname -a; git rev-parse HEAD; git branch --show-current
echo "--- host truth (scripts/z13/host_truth.sh, Z-002) ---"
sh scripts/z13/host_truth.sh
echo "--- load (start) ---"; uptime; cat /proc/loadavg; ps aux --sort=-%cpu | head -6
echo "--- topology (sysfs) ---"
for c in 0 8 16 24; do
  echo "cpu$c core_id=$(cat /sys/devices/system/cpu/cpu$c/topology/core_id) siblings=$(cat /sys/devices/system/cpu/cpu$c/topology/thread_siblings_list) L3=$(cat /sys/devices/system/cpu/cpu$c/cache/index3/size) L3_shared=$(cat /sys/devices/system/cpu/cpu$c/cache/index3/shared_cpu_list)"
done
echo "smt active: $(cat /sys/devices/system/cpu/smt/active)"
echo "governor cpu0: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
echo "--- build probes ---"
gcc $CFLAGS -Iinclude -Isrc -c src/core/mimir_core.c -o build/z41/mimir_core.o
gcc $CFLAGS -Iinclude -Isrc -c src/api/mimir_alloc.c -o build/z41/mimir_alloc.o
gcc $CFLAGS -Iinclude -Isrc -c src/platform/mimir_platform.c -o build/z41/mimir_platform.o
gcc $CFLAGS -Iinclude -Isrc tests/perf/probe_z041.c build/z41/mimir_alloc.o build/z41/mimir_core.o build/z41/mimir_platform.o -o build/z41/probe_z041_mimir
gcc $CFLAGS -DLEG_GLIBC tests/perf/probe_z041.c -o build/z41/probe_z041_glibc
echo "build exit=0"
sha256sum tests/perf/probe_z041.c tests/perf/run_z041.sh tests/perf/summarize_z041.py build/z41/probe_z041_mimir build/z41/probe_z041_glibc
run_leg() {
    LEG=$1; BIN=$2; RAW="hidden_files/z041_${TAG}_${LEG}.raw"
    : > "$RAW"
    echo "--- selfcheck $LEG ---"
    taskset -c 0-31 "$BIN" selfcheck | tee "$RAW"
    for WL in small_churn mixed_batch; do
        if [ "$WL" = small_churn ]; then ITERS=50000; else ITERS=200; fi
        for T in 8 16 32; do
            for MODE in pinned floating; do
                echo "# point leg=$LEG workload=$WL threads=$T mode=$MODE loadavg=$(cat /proc/loadavg)" >> "$RAW"
                taskset -c 0-31 "$BIN" ladder "$WL" "$T" "$MODE" $((ITERS / 10)) > /dev/null
                r=0
                while [ "$r" -lt "$REPS" ]; do
                    taskset -c 0-31 "$BIN" ladder "$WL" "$T" "$MODE" "$ITERS" >> "$RAW"
                    r=$((r + 1))
                done
                echo "point done: $LEG $WL t=$T $MODE"
                sleep 2
            done
        done
    done
    echo "raw LADDER lines $LEG: $(grep -c '^LADDER,' "$RAW")"
}
run_leg mimir build/z41/probe_z041_mimir
run_leg glibc build/z41/probe_z041_glibc
echo "--- summary (tag=$TAG) ---"
python3 tests/perf/summarize_z041.py "mimir=hidden_files/z041_${TAG}_mimir.raw" "glibc=hidden_files/z041_${TAG}_glibc.raw"
echo "summarize exit=$?"
echo "--- load (end) ---"; uptime; cat /proc/loadavg
echo "--- run_z041 $TAG done ---"
