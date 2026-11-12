#!/bin/sh
# run_z023.sh - Z-023 large-path anatomy runner.
# Builds the production core objects into build/z23/ (the
# production build/ tree is never touched) and runs
# probe_z023 phases under the Z-004 protocol
# (docs/Z13_MEASUREMENT_PROTOCOL.md, banked at the seat):
#   - every phase pinned taskset -c 2 (CCD0 physical core)
#   - 3 warm-up PROCESSES per phase, discarded
#   - measured repetitions are separate PROCESSES
#     (default 7 per phase), headline = median of processes
#     (summarize_z023.py)
#   - legs interleaved round-robin within each repetition
#     round so drift lands on all legs of a size together
# Usage: run_z023.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_z023.sh <raw-output-file> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z23"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
gcc $CFLAGS -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core.o"
gcc $CFLAGS -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/mimir_alloc.o"
gcc $CFLAGS -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/mimir_platform.o"
gcc $CFLAGS "$ROOT/tests/perf/probe_z023.c" \
    "$ZB/mimir_alloc.o" "$ZB/mimir_core.o" "$ZB/mimir_platform.o" \
    -o "$ZB/probe_z023"
BIN="$ZB/probe_z023"

# iters per size for the single-size legs: sized so a timed
# section is a few hundred ms at this host's large-path cost
iters_for() {
    case $1 in
        2048) echo 30000 ;;
        8192) echo 30000 ;;
        16384) echo 20000 ;;
        65536) echo 20000 ;;
        262144) echo 10000 ;;
        1048576) echo 3000 ;;
    esac
}

: > "$OUT"
SIZES="2048 8192 16384 65536 262144 1048576"
for S in $SIZES; do
    IT=$(iters_for "$S")
    # warm-ups: 3 processes per leg, discarded
    for LEG in "op mimir" "op glibc" "raw notouch" "raw touch" "raw fulltouch"; do
        i=0
        while [ "$i" -lt 3 ]; do
            taskset -c 2 "$BIN" $LEG "$S" "$IT" > /dev/null
            i=$((i + 1))
        done
    done
    # measured: REPS rounds, legs interleaved within a round
    r=0
    while [ "$r" -lt "$REPS" ]; do
        for LEG in "op mimir" "op glibc" "raw notouch" "raw touch" "raw fulltouch"; do
            taskset -c 2 "$BIN" $LEG "$S" "$IT" >> "$OUT"
        done
        r=$((r + 1))
    done
    echo "size $S done" >&2
done

# batch-shape phases (no size argument)
for LEG in "batch mimir" "batch glibc" "batchraw notouch" "batchraw touch"; do
    i=0
    while [ "$i" -lt 3 ]; do
        taskset -c 2 "$BIN" $LEG > /dev/null
        i=$((i + 1))
    done
done
r=0
while [ "$r" -lt "$REPS" ]; do
    for LEG in "batch mimir" "batch glibc" "batchraw notouch" "batchraw touch"; do
        taskset -c 2 "$BIN" $LEG >> "$OUT"
    done
    r=$((r + 1))
done
echo "batch phases done" >&2

# registry-walk phases: live counts, 65536-byte blocks
for LIVE in 1 32 256 1024; do
    case $LIVE in
        1024) WREPS=3 ;;
        *) WREPS=5 ;;
    esac
    for LEG in mimir raw; do
        i=0
        while [ "$i" -lt 3 ]; do
            taskset -c 2 "$BIN" walk "$LEG" "$LIVE" "$WREPS" > /dev/null
            i=$((i + 1))
        done
    done
    r=0
    while [ "$r" -lt "$REPS" ]; do
        for LEG in mimir raw; do
            taskset -c 2 "$BIN" walk "$LEG" "$LIVE" "$WREPS" >> "$OUT"
        done
        r=$((r + 1))
    done
    echo "walk live=$LIVE done" >&2
done
echo "raw lines: $(wc -l < "$OUT") (expect $((REPS * (6 * 5 + 4 + 4 * 2))))" >&2
