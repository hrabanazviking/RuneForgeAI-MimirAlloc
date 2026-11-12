#!/bin/sh
# run_z048.sh - Z-048 large_batch re-measurement runner.
#
# Z-004 protocol (docs/Z13_MEASUREMENT_PROTOCOL.md):
#   - every measured value is a separate PROCESS pinned
#     taskset -c 2 (CCD0 physical core)
#   - 3 warm-up processes per leg, discarded
#   - 7 measured processes per leg, headline = MEDIAN
#     (summarize_z048.py)
#   - legs interleaved round-robin within each repetition
#     round, so drift lands on all legs together
# Legs: mimir_pool (this tree's core objects, Z-048 live),
# mimir_base (the pre-Z-048 core from BASE_ROOT - the
# same-window A/B control), glibc (host allocator).
#
# Usage: run_z048.sh <raw-output-file> [reps]
#   BASE_ROOT: checkout of the pre-Z-048 tree (seat
#   517bbb1) used for the mimir_base leg; default
#   /tmp/z048base. If BASE_ROOT is absent the base leg is
#   skipped and the summariser grades against the banked
#   Z-009 baseline alone (named in its output).
set -eu
OUT=${1:?usage: run_z048.sh <raw-output-file> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
BASE_ROOT=${BASE_ROOT:-/tmp/z048base}
ZB="$ROOT/build/z48"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"

build_leg() {
    # $1 = source root, $2 = tag, $3 = extra defines; each
    # leg compiles against its OWN tree's headers.
    gcc $CFLAGS -I"$1/include" -I"$1/src" -c "$1/src/core/mimir_core.c" -o "$ZB/core_$2.o"
    gcc $CFLAGS -I"$1/include" -I"$1/src" -c "$1/src/api/mimir_alloc.c" -o "$ZB/alloc_$2.o"
    gcc $CFLAGS -I"$1/include" -I"$1/src" -c "$1/src/platform/mimir_platform.c" -o "$ZB/plat_$2.o"
    gcc $CFLAGS $3 -I"$1/include" "$ROOT/tests/perf/probe_z048.c" \
        "$ZB/alloc_$2.o" "$ZB/core_$2.o" "$ZB/plat_$2.o" \
        -o "$ZB/probe_$2"
}
build_leg "$ROOT" pool ""
LEGS="pool"
if [ -d "$BASE_ROOT/src/core" ]; then
    build_leg "$BASE_ROOT" base "-DLEG_BASE"
    LEGS="pool base"
fi
gcc $CFLAGS -I"$ROOT/include" -DLEG_GLIBC "$ROOT/tests/perf/probe_z048.c" -o "$ZB/probe_glibc"
LEGS="$LEGS glibc"

bin_for() {
    case $1 in
        pool) echo "$ZB/probe_pool" ;;
        base) echo "$ZB/probe_base" ;;
        glibc) echo "$ZB/probe_glibc" ;;
    esac
}

: > "$OUT"
# warm-ups: 3 processes per leg, discarded
for L in $LEGS; do
    i=0
    while [ "$i" -lt 3 ]; do
        taskset -c 2 "$(bin_for "$L")" > /dev/null
        i=$((i + 1))
    done
done
# measured: REPS rounds, legs interleaved within a round
r=0
while [ "$r" -lt "$REPS" ]; do
    for L in $LEGS; do
        taskset -c 2 "$(bin_for "$L")" >> "$OUT"
    done
    r=$((r + 1))
done
echo "run_z048: legs=$LEGS reps=$REPS raw=$OUT" >&2
