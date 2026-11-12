#!/bin/sh
# run_z047.sh - Z-047 mapping-cache design probe runner.
# Host probe only (raw mmap in MimirAlloc's large-path
# geometry; no allocator code is linked). Protocol per
# docs/Z13_MEASUREMENT_PROTOCOL.md (Z-004), adapted like
# Z-015/Z-046: every timed process pinned taskset -c 2
# (CCD0 physical core); 3 warm-up PROCESSES per group
# discarded; 7 measured PROCESSES per group; groups
# interleaved round-robin within each round; headline =
# median of processes (summarize_z047.py). The census leg
# is deterministic (VMA counts) plus RSS readings, run in
# 3 fresh processes.
# Usage: run_z047.sh <raw-output-file> <census-output-file> [reps]
set -eu
OUT=${1:?usage: run_z047.sh <raw> <census> [reps]}
CEN=${2:?usage: run_z047.sh <raw> <census> [reps]}
REPS=${3:-7}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z47"
mkdir -p "$ZB"
cc -std=c11 -Wall -Wextra -Werror -O2 \
    "$ROOT/tests/perf/probe_z047.c" -o "$ZB/probe_z047"
BIN="$ZB/probe_z047"
sha256sum "$ROOT/tests/perf/probe_z047.c" "$ROOT/tests/perf/run_z047.sh" \
    "$ROOT/tests/perf/summarize_z047.py" "$BIN" >&2

iters_for() {
    case $1 in
        2048|8192) echo 5000 ;;
        65536) echo 3000 ;;
        262144) echo 1500 ;;
        1048576) echo 500 ;;
        4194304) echo 150 ;;
        16777216) echo 40 ;;
        33554432) echo 25 ;;
    esac
}
dense_for() {
    case $1 in
        1048576) echo 60 ;;
        8388608) echo 12 ;;
    esac
}

: > "$OUT"
SIZES="2048 8192 65536 262144 1048576 4194304 16777216 33554432"
for S in $SIZES; do
    IT=$(iters_for "$S")
    for LEG in fresh poolhit; do
        i=0
        while [ "$i" -lt 3 ]; do
            taskset -c 2 "$BIN" op "$LEG" "$S" "$IT" > /dev/null
            i=$((i + 1))
        done
    done
    r=0
    while [ "$r" -lt "$REPS" ]; do
        for LEG in fresh poolhit; do
            taskset -c 2 "$BIN" op "$LEG" "$S" "$IT" >> "$OUT"
        done
        r=$((r + 1))
    done
    echo "sparse size $S done" >&2
done

for S in 1048576 8388608; do
    IT=$(dense_for "$S")
    for LEG in freshdense pooldense pooldontneed; do
        i=0
        while [ "$i" -lt 3 ]; do
            taskset -c 2 "$BIN" op "$LEG" "$S" "$IT" > /dev/null
            i=$((i + 1))
        done
    done
    r=0
    while [ "$r" -lt "$REPS" ]; do
        for LEG in freshdense pooldense pooldontneed; do
            taskset -c 2 "$BIN" op "$LEG" "$S" "$IT" >> "$OUT"
        done
        r=$((r + 1))
    done
    echo "dense size $S done" >&2
done

: > "$CEN"
i=0
while [ "$i" -lt 3 ]; do
    taskset -c 2 "$BIN" census >> "$CEN"
    i=$((i + 1))
done
echo "raw lines: $(wc -l < "$OUT") (expect $((REPS * (8 * 2 + 2 * 3))))" >&2
echo "census lines: $(wc -l < "$CEN") (expect 15)" >&2
