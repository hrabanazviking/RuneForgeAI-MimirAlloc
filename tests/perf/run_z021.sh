#!/bin/sh
# run_z021.sh - Z-021 The contention ladder (glibc comparator)
# runner (MimirAlloc Z13). Mirrors team-06's run_z020.sh shape
# exactly (the pairing discipline): per-point load comments in
# the raw file, one discarded warm-up PROCESS per ladder point
# at 1/10 scale, REPS separate processes per point (default 5),
# 2 s pauses between points, taskset -c 0-31 wrapper (per-thread
# pinning happens inside the probe). Builds probe_z021 once
# with -DLEG_GLIBC (bench_k002.c's leg switch) - the glibc leg
# needs no allocator objects. Header sections (host truth,
# topology, load) follow the house perf pattern so the recorded
# run can be assembled from this log alone.
# Usage: run_z021.sh [reps]
set -eu
cd "$(dirname "$0")/../.." || exit 2
REPS=${1:-5}
OUT=/tmp/z021_ladder.raw
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
echo "=== Z-021 The contention ladder (glibc comparator) ==="
date -Is; uname -a; git rev-parse HEAD
echo "--- host truth (scripts/z13/host_truth.sh, Z-002) ---"
sh scripts/z13/host_truth.sh
echo "--- load (start) ---"; uptime; cat /proc/loadavg; ps aux --sort=-%cpu | head -6
echo "--- topology (sysfs) ---"
for c in 0 8 16 24; do
  echo "cpu$c core_id=$(cat /sys/devices/system/cpu/cpu$c/topology/core_id) siblings=$(cat /sys/devices/system/cpu/cpu$c/topology/thread_siblings_list) L3=$(cat /sys/devices/system/cpu/cpu$c/cache/index3/size) L3_shared=$(cat /sys/devices/system/cpu/cpu$c/cache/index3/shared_cpu_list)"
done
echo "governor cpu0: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
echo "--- build probe_z021 (-DLEG_GLIBC) ---"
cc $CFLAGS -DLEG_GLIBC tests/perf/probe_z021.c -o /tmp/probe_z021
echo "build exit=0"
sha256sum tests/perf/probe_z021.c tests/perf/run_z021.sh tests/perf/summarize_z021.py /tmp/probe_z021
echo "--- selfcheck ---"
taskset -c 0-31 /tmp/probe_z021 selfcheck | tee "$OUT"
echo "--- ladder (reps=$REPS processes per point) ---"
run_point() {
    # $1 workload, $2 threads, $3 iters per thread
    echo "# point workload=$1 threads=$2 iters=$3 loadavg=$(cat /proc/loadavg)" >> "$OUT"
    taskset -c 0-31 /tmp/probe_z021 ladder "$1" "$2" $(($3 / 10)) > /dev/null
    r=0
    while [ "$r" -lt "$REPS" ]; do
        taskset -c 0-31 /tmp/probe_z021 ladder "$1" "$2" "$3" >> "$OUT"
        r=$((r + 1))
    done
    echo "point done: $1 t=$2"
    sleep 2
}
for T in 1 2 3 4 6 8 12 16 20 24 28 32; do
    run_point small_churn "$T" 50000
done
for T in 1 2 3 4 6 8 12 16 20 24 28 32; do
    run_point mixed_batch "$T" 200
done
echo "raw LADDER lines: $(grep -c '^LADDER,' "$OUT")"
cat "$OUT"
echo "--- summary (summarize_z021.py) ---"
python3 tests/perf/summarize_z021.py "$OUT"; echo "summarize exit=$?"
echo "--- load (end) ---"; uptime; cat /proc/loadavg
echo "--- run_z021 done ---"
