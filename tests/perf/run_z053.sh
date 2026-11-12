#!/bin/sh
# run_z053.sh - Z-053 large-path fragmentation runner.
#
# Z-004 protocol applied to a memory-shape (not timing)
# measurement: every value comes from a separate PROCESS
# pinned taskset -c 2 (CCD0 physical core); 1 warm-up
# process per leg per pattern, discarded; REPS measured
# processes per leg per pattern (default 3), legs
# interleaved within each round so machine drift lands
# on both legs. RSS/PSS come from the probe's own
# /proc/self/smaps_rollup reads at defined phases (the
# Z-006 method); VMA and hole figures from
# /proc/self/maps in the same instant.
#
# Usage: run_z053.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_z053.sh <raw-output-file> [reps]}
REPS=${2:-3}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z53"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"

gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/alloc.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/plat.o"
gcc $CFLAGS -I"$ROOT/include" "$ROOT/tests/perf/probe_z053.c" "$ZB/alloc.o" "$ZB/core.o" "$ZB/plat.o" -o "$ZB/probe_mimir"
gcc $CFLAGS -I"$ROOT/include" -DLEG_GLIBC "$ROOT/tests/perf/probe_z053.c" -o "$ZB/probe_glibc"

PATTERNS="checker uniform pinwheel churn"
: > "$OUT"
for P in $PATTERNS; do
    T0=$(date +%s)
    # warm-up: 1 process per leg, discarded
    taskset -c 2 "$ZB/probe_mimir" "$P" > /dev/null
    taskset -c 2 "$ZB/probe_glibc" "$P" > /dev/null
    r=0
    while [ "$r" -lt "$REPS" ]; do
        taskset -c 2 "$ZB/probe_mimir" "$P" >> "$OUT"
        taskset -c 2 "$ZB/probe_glibc" "$P" >> "$OUT"
        r=$((r + 1))
    done
    T1=$(date +%s)
    echo "run_z053: pattern=$P reps=$REPS wall_s=$((T1 - T0))" >&2
done
echo "run_z053: raw=$OUT" >&2
