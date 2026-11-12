#!/bin/sh
# run_z052.sh - Z-052 growth-ladder runner.
#
# Z-004 protocol (docs/Z13_MEASUREMENT_PROTOCOL.md):
#   - every measured value is a separate PROCESS pinned
#     taskset -c 2 (CCD0 physical core)
#   - 3 warm-up processes per leg, discarded
#   - 7 measured processes per leg, headline = MEDIAN
#     (summarize_z052.py)
#   - legs interleaved round-robin within each repetition
#     round, so drift lands on all legs together
# Legs: mimir_after (this tree, Z-052 live), mimir_before
# (the pre-Z-052 core from BASE_ROOT - the same-window A/B
# control), glibc (host allocator).
#
# Usage: run_z052.sh <raw-output-file> [reps]
#   BASE_ROOT: checkout of the pre-Z-052 tree (seat
#   f8ee668, banked Z-001..Z-049); default /tmp/z052base.
#   If BASE_ROOT is absent the before leg is skipped and
#   the summariser says so (the gate wants before/after,
#   so the recorded run always provides it).
set -eu
OUT=${1:?usage: run_z052.sh <raw-output-file> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
BASE_ROOT=${BASE_ROOT:-/tmp/z052base}
ZB="$ROOT/build/z52"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"

build_leg() {
    # $1 = source root, $2 = tag, $3 = extra defines; each
    # leg compiles against its OWN tree's headers.
    gcc $CFLAGS -I"$1/include" -I"$1/src" -c "$1/src/core/mimir_core.c" -o "$ZB/core_$2.o"
    gcc $CFLAGS -I"$1/include" -I"$1/src" -c "$1/src/api/mimir_alloc.c" -o "$ZB/alloc_$2.o"
    gcc $CFLAGS -I"$1/include" -I"$1/src" -c "$1/src/platform/mimir_platform.c" -o "$ZB/plat_$2.o"
    gcc $CFLAGS $3 -I"$1/include" "$ROOT/tests/perf/probe_z052.c" \
        "$ZB/alloc_$2.o" "$ZB/core_$2.o" "$ZB/plat_$2.o" \
        -o "$ZB/probe_$2"
}
build_leg "$ROOT" after "-DLEG_AFTER"
LEGS="after"
if [ -d "$BASE_ROOT/src/core" ]; then
    build_leg "$BASE_ROOT" before "-DLEG_BASE"
    LEGS="after before"
fi
gcc $CFLAGS -I"$ROOT/include" -DLEG_GLIBC "$ROOT/tests/perf/probe_z052.c" -o "$ZB/probe_glibc"
LEGS="$LEGS glibc"

bin_for() {
    case $1 in
        after) echo "$ZB/probe_after" ;;
        before) echo "$ZB/probe_before" ;;
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
echo "run_z052: legs=$LEGS reps=$REPS raw=$OUT" >&2
