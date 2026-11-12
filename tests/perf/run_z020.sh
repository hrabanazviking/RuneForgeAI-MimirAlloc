#!/bin/sh
# run_z020.sh - Z-020 contention ladder runner (banked allocator).
# Builds probe_z020 twice under build/z20/ (production objects
# untouched elsewhere):
#   probe_z020      - production core, the ladder of record
#   probe_z020_hist - core with -DMIMIR_LOCK_HIST=1 (Z-019
#                     instrumentation), supplementary hold/wait
#                     specimen at 1/8/16/32 threads
# Protocol (docs/Z13_MEASUREMENT_PROTOCOL.md, Z-004):
#   - per-thread pinning inside the probe (thread t -> CPU t,
#     identity order: 0-15 physical, 16-31 SMT siblings),
#     verified by the probe with sched_getcpu()
#   - repetitions are separate PROCESSES (default 5 per ladder
#     point), headline = median of processes (summarize_z020.py)
#   - one discarded warm-up PROCESS per ladder point at 1/10
#     scale, plus the probe's own in-process 1/10 warm-up pass
#   - 2 s pauses between ladder points (thermal honesty,
#     protocol section 10); load recorded per point as a
#     #-comment in the raw file
# Usage: run_z020.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_z020.sh <raw-output-file> [reps]}
REPS=${2:-5}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z20"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
gcc $CFLAGS -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core.o"
gcc $CFLAGS -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/mimir_alloc.o"
gcc $CFLAGS -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/mimir_platform.o"
gcc $CFLAGS "$ROOT/tests/perf/probe_z020.c" \
    "$ZB/mimir_alloc.o" "$ZB/mimir_core.o" "$ZB/mimir_platform.o" \
    -o "$ZB/probe_z020"
gcc $CFLAGS -DMIMIR_LOCK_HIST=1 -c "$ROOT/src/core/mimir_core.c" \
    -o "$ZB/mimir_core_hist.o"
gcc $CFLAGS -DMIMIR_LOCK_HIST=1 "$ROOT/tests/perf/probe_z020.c" \
    "$ZB/mimir_alloc.o" "$ZB/mimir_core_hist.o" "$ZB/mimir_platform.o" \
    -o "$ZB/probe_z020_hist"
: > "$OUT"
taskset -c 0-31 "$ZB/probe_z020" selfcheck >> "$OUT"
echo "selfcheck done" >&2
run_point() {
    # $1 workload, $2 threads, $3 iters per thread
    echo "# point workload=$1 threads=$2 iters=$3 loadavg=$(cat /proc/loadavg)" >> "$OUT"
    taskset -c 0-31 "$ZB/probe_z020" ladder "$1" "$2" $(($3 / 10)) > /dev/null
    r=0
    while [ "$r" -lt "$REPS" ]; do
        taskset -c 0-31 "$ZB/probe_z020" ladder "$1" "$2" "$3" >> "$OUT"
        r=$((r + 1))
    done
    echo "point done: $1 t=$2" >&2
    sleep 2
}
for T in 1 2 3 4 6 8 12 16 20 24 28 32; do
    run_point small_churn "$T" 50000
done
for T in 1 2 3 4 6 8 12 16 20 24 28 32; do
    run_point mixed_batch "$T" 200
done
for T in 1 8 16 32; do
    echo "# hist point threads=$T loadavg=$(cat /proc/loadavg)" >> "$OUT"
    r=0
    while [ "$r" -lt 3 ]; do
        taskset -c 0-31 "$ZB/probe_z020_hist" hist "$T" 50000 >> "$OUT"
        r=$((r + 1))
    done
    echo "hist point done: t=$T" >&2
    sleep 2
done
echo "raw LADDER lines: $(grep -c '^LADDER,' "$OUT")" >&2
echo "raw HIST lines: $(grep -c '^HIST,' "$OUT")" >&2
