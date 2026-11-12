#!/bin/sh
# run_z046.sh - Z-046 the threshold study runner.
# Builds the production core objects into build/z46/ (the
# production build/ tree is never touched) and runs
# probe_z046 under the Z-004 protocol
# (docs/Z13_MEASUREMENT_PROTOCOL.md):
#   - every cost process pinned taskset -c 2 (CCD0)
#   - 3 warm-up PROCESSES per leg, discarded
#   - measured repetitions are separate PROCESSES
#     (default 7 per leg), headline = median of processes
#     (summarize_z046.py)
#   - legs interleaved round-robin within each round so
#     drift lands on all legs of a size together
# Threshold legs are single-shot fresh processes by
# design (one verdict per process; repetition cannot
# pollute them because each process starts virgin).
# Usage: run_z046.sh <raw-output-file> <thresh-output-file> [reps]
set -eu
OUT=${1:?usage: run_z046.sh <raw> <thresh> [reps]}
THR=${2:?usage: run_z046.sh <raw> <thresh> [reps]}
REPS=${3:-7}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z46"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
gcc $CFLAGS -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core.o"
gcc $CFLAGS -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/mimir_alloc.o"
gcc $CFLAGS -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/mimir_platform.o"
gcc $CFLAGS "$ROOT/tests/perf/probe_z046.c" \
    "$ZB/mimir_alloc.o" "$ZB/mimir_core.o" "$ZB/mimir_platform.o" \
    -o "$ZB/probe_z046"
BIN="$ZB/probe_z046"
sha256sum "$ROOT/tests/perf/probe_z046.c" "$ROOT/tests/perf/run_z046.sh" \
    "$ROOT/tests/perf/summarize_z046.py" "$BIN" >&2

iters_for() {
    case $1 in
        1024|2048|4096|8192) echo 20000 ;;
        16384|32768|65536) echo 10000 ;;
        131072|262144) echo 5000 ;;
        524288) echo 3000 ;;
        1048576) echo 2000 ;;
        2097152) echo 1000 ;;
        4194304) echo 600 ;;
        8388608) echo 300 ;;
        16777216) echo 150 ;;
        33554432) echo 80 ;;
    esac
}
full_for() {
    case $1 in
        1048576) echo 100 ;;
        8388608) echo 20 ;;
        33554432) echo 6 ;;
    esac
}

: > "$OUT"
SIZES="1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608 16777216 33554432"
for S in $SIZES; do
    IT=$(iters_for "$S")
    for LEG in "op mimir" "op glibc" "op glmap" "op glarena" "op gltrim" "raw notouch" "raw touch"; do
        i=0
        while [ "$i" -lt 3 ]; do
            taskset -c 2 "$BIN" $LEG "$S" "$IT" > /dev/null
            i=$((i + 1))
        done
    done
    r=0
    while [ "$r" -lt "$REPS" ]; do
        for LEG in "op mimir" "op glibc" "op glmap" "op glarena" "op gltrim" "raw notouch" "raw touch"; do
            taskset -c 2 "$BIN" $LEG "$S" "$IT" >> "$OUT"
        done
        r=$((r + 1))
    done
    echo "size $S done" >&2
done

# dense-touch legs at three decade tops
for S in 1048576 8388608 33554432; do
    IT=$(full_for "$S")
    for LEG in "opfull mimir" "opfull glibc"; do
        i=0
        while [ "$i" -lt 3 ]; do
            taskset -c 2 "$BIN" $LEG "$S" "$IT" > /dev/null
            i=$((i + 1))
        done
    done
    r=0
    while [ "$r" -lt "$REPS" ]; do
        for LEG in "opfull mimir" "opfull glibc"; do
            taskset -c 2 "$BIN" $LEG "$S" "$IT" >> "$OUT"
        done
        r=$((r + 1))
    done
    echo "opfull size $S done" >&2
done

# threshold study: fresh process per verdict
: > "$THR"
for S in 1024 4096 16384 65536 98304 114688 126976 129024 130048 130560 130944 131040 131048 131049 131056 131063 131064 131072 131136 132096 262144 1048576 33554432; do
    taskset -c 2 "$BIN" threshone "$S" >> "$THR"
done
for PAIR in "262144 131072" "262144 262144" "262144 524288" "1048576 524288" "1048576 1048576" "1048576 2097152" "8388608 4194304" "8388608 8388608" "8388608 16777216" "33554400 16777216" "33554400 33554400" "33554400 33554432" "33554432 16777216" "67108864 16777216"; do
    taskset -c 2 "$BIN" threshdyn $PAIR >> "$THR"
done
taskset -c 2 "$BIN" threshstay 1048576 524288 >> "$THR"
taskset -c 2 "$BIN" threshmallopt 131072 8388608 4194304 >> "$THR"
taskset -c 2 "$BIN" threshmallopt 67108864 1048576 1048576 >> "$THR"
echo "raw lines: $(wc -l < "$OUT") (expect $((REPS * (16 * 7 + 3 * 2))))" >&2
echo "thresh lines: $(wc -l < "$THR") (expect 40)" >&2
