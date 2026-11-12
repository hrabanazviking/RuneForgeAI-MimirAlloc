#!/bin/sh
# run_z062.sh - Z-062 reuse-distance runner.
#
# Z-004 protocol (docs/Z13_MEASUREMENT_PROTOCOL.md):
#   - every measured value is a separate PROCESS pinned
#     taskset -c 5 (CCD0 physical core; avoids CPU 0
#     housekeeping and cores 2/3, where the Z-057 soak ran)
#   - 3 warm-up processes per leg at the smallest cell,
#     discarded
#   - REPS (default 5) measured processes per cell, legs
#     interleaved round-robin within each repetition round,
#     headline = MEDIAN (summarize_z062.py)
#   - the mimir_lifo leg is run TWICE (mimir_lifo and
#     mimir_lifo_dup from the same binary): the identical-leg
#     agreement control, bound 5%.
# Legs: mimir_native, mimir_lifo, mimir_lifo_dup, mimir_fifo,
#       glibc_native, glibc_fifo  (small classes)
#       mimir_native, mimir_fifo, glibc_native  (large class)
# Cells A: classes 64/256/1024 x footprints
#   65536, 2097152, 16777216, 33554432, 50331648, 100663296
# Cells B: class 8192 x footprints 2097152, 33554432,
#   100663296
# Rounds per process: 4 timed cycles (plus 1 warm-up cycle
# inside the probe); 2 timed cycles when W > 200000 blocks.
#
# NATIVE CAP (attempt-2 rescoping, recorded in the slice's
# recorded run): attempt 1 measured mimir_native at class 64 /
# 32 MiB taking 51,437 ns/block (53.9 s for one process) -- a
# superlinear cliff (emulated LIFO: 5.9 ns/block). In the main
# sweep mimir_native runs only where W <= 262144 blocks;
# above the cap the other legs run at full reps and
# mimir_native is measured as a SINGLE-PROCESS DIAGNOSTIC
# (1 rep, 1 timed round) written to <raw>.diag -- n=1, never
# a median. Attempt 1's raw already holds one full native
# process at class 64 / 32 MiB, so that cell is not re-run.
#
# Usage: run_z062.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_z062.sh <raw-output-file> [reps]}
REPS=${2:-5}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z62"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
PIN=5

gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/alloc.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/plat.o"
gcc $CFLAGS -I"$ROOT/include" "$ROOT/tests/perf/probe_z062.c" \
    "$ZB/alloc.o" "$ZB/core.o" "$ZB/plat.o" -o "$ZB/probe_mimir"
gcc $CFLAGS -DLEG_GLIBC "$ROOT/tests/perf/probe_z062.c" -o "$ZB/probe_glibc"

bin_for() {
    case $1 in
        mimir_*) echo "$ZB/probe_mimir" ;;
        glibc_*) echo "$ZB/probe_glibc" ;;
    esac
}
pol_for() {
    case $1 in
        *_native) echo native ;;
        *_fifo) echo fifo ;;
        *_lifo|*_lifo_dup) echo lifo ;;
    esac
}
rounds_for() {
    # $1 = blocks W
    if [ "$1" -gt 200000 ]; then echo 2; else echo 4; fi
}

LEGS_A="mimir_native mimir_lifo mimir_lifo_dup mimir_fifo glibc_native glibc_fifo"
LEGS_B="mimir_native mimir_fifo glibc_native"
CLASSES_A="64 256 1024"
FPS_A="65536 2097152 16777216 33554432 50331648 100663296"
CLASS_B=8192
FPS_B="2097152 33554432 100663296"

: > "$OUT"
# warm-ups: 3 processes per leg at the smallest cell, discarded
for L in $LEGS_A; do
    i=0
    while [ "$i" -lt 3 ]; do
        taskset -c $PIN "$(bin_for "$L")" "$(pol_for "$L")" 256 65536 2 > /dev/null
        i=$((i + 1))
    done
done
# measured cells A: reps rounds, legs interleaved per round
for C in $CLASSES_A; do
    for F in $FPS_A; do
        W=$((F / C))
        R=$(rounds_for "$W")
        if [ "$W" -le 262144 ]; then
            CELL_LEGS="$LEGS_A"
        else
            CELL_LEGS="mimir_lifo mimir_lifo_dup mimir_fifo glibc_native glibc_fifo"
        fi
        r=0
        while [ "$r" -lt "$REPS" ]; do
            for L in $CELL_LEGS; do
                taskset -c $PIN "$(bin_for "$L")" "$(pol_for "$L")" "$C" "$F" "$R" >> "$OUT"
            done
            r=$((r + 1))
        done
    done
done
# measured cells B (large class)
for F in $FPS_B; do
    W=$((F / CLASS_B))
    R=$(rounds_for "$W")
    r=0
    while [ "$r" -lt "$REPS" ]; do
        for L in $LEGS_B; do
            taskset -c $PIN "$(bin_for "$L")" "$(pol_for "$L")" "$CLASS_B" "$F" "$R" >> "$OUT"
        done
        r=$((r + 1))
    done
done
# native diagnostics above the cap: 1 process, 1 timed round
DIAG="$OUT.diag"
: > "$DIAG"
for CELL in "64 50331648" "64 100663296" "256 100663296"; do
    set -- $CELL
    taskset -c $PIN "$ZB/probe_mimir" native "$1" "$2" 1 >> "$DIAG"
done
echo "run_z062: reps=$REPS raw=$OUT" >&2
