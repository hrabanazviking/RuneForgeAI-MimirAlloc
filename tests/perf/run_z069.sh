#!/bin/sh
# run_z069.sh - Z-069 canary economics runner.
#
# Z-004 protocol (docs/Z13_MEASUREMENT_PROTOCOL.md):
#   - every measured value is a separate PROCESS pinned
#     taskset -c 5 (CCD0 physical core; avoids CPU 0
#     housekeeping and cores 2/3, where campaign soaks run)
#   - 3 warm-up processes per leg at the smallest cell,
#     discarded
#   - REPS (default 5) measured processes per cell, legs
#     interleaved round-robin within each repetition round,
#     headline = MEDIAN (summarize_z069.py)
#   - the base leg is run TWICE (base and base_dup from the
#     same binary): the identical-leg agreement control,
#     bound 5%.
# Legs: base, base_dup, can16, can32.
# Cells (representative class per class group):
#   G1 tiny   (span classes 16/32/48/64):    16, 32, 64
#   G2 small  (span classes 80..256):       128, 256
#   G3 medium (span classes 320..1024):     512, 1024
#   G4 large  (descriptor/mapping path):    8192, 65536
# Blocks per process: 4096 for classes <= 1024 (footprint
# <= 4 MiB), 256 for the large classes. Rounds per process: 4.
#
# Usage: run_z069.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_z069.sh <raw-output-file> [reps]}
REPS=${2:-5}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z69"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
PIN=5

gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/alloc.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/plat.o"
gcc $CFLAGS -I"$ROOT/include" "$ROOT/tests/perf/probe_z069.c" \
    "$ZB/alloc.o" "$ZB/core.o" "$ZB/plat.o" -o "$ZB/probe_z069"

LEGS="base base_dup can16 can32"
SMALL_CLASSES="16 32 64 128 256 512 1024"
LARGE_CLASSES="8192 65536"

: > "$OUT"
# warm-ups: 3 processes per leg at the smallest cell, discarded
for L in $LEGS; do
    i=0
    while [ "$i" -lt 3 ]; do
        taskset -c $PIN "$ZB/probe_z069" "$L" 16 4096 4 > /dev/null
        i=$((i + 1))
    done
done
# measured cells, small/medium classes: reps rounds, legs
# interleaved per round
for C in $SMALL_CLASSES; do
    r=0
    while [ "$r" -lt "$REPS" ]; do
        for L in $LEGS; do
            taskset -c $PIN "$ZB/probe_z069" "$L" "$C" 4096 4 >> "$OUT"
        done
        r=$((r + 1))
    done
done
# measured cells, large classes
for C in $LARGE_CLASSES; do
    r=0
    while [ "$r" -lt "$REPS" ]; do
        for L in $LEGS; do
            taskset -c $PIN "$ZB/probe_z069" "$L" "$C" 256 4 >> "$OUT"
        done
        r=$((r + 1))
    done
done
echo "run_z069: reps=$REPS raw=$OUT" >&2
