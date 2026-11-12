#!/bin/sh
# run_z054.sh - Z-054 Wave 6 verdict standings runner.
#
# Fresh K-002 standings for the CURRENT banked core (seat
# main at the Z-054 merge: Z-001..Z-053, i.e. the Wave 6
# core with the Z-048 mapping cache, the Z-050 trim policy
# and the Z-052 mremap growth all live) against glibc, on
# the unmodified tests/perf/bench_k002.c harness - the
# exact harness Z-003/Z-009 graded, so the standings are
# directly comparable to the Wave 1 baseline of record.
#
# Z-004 protocol (docs/Z13_MEASUREMENT_PROTOCOL.md):
#   - every measured value is a separate PROCESS pinned
#     taskset -c 2 (CCD0 physical core)
#   - 3 warm-up processes per leg, discarded
#   - 7 measured processes per leg, headline = MEDIAN
#     (summarize_z054.py)
#   - legs interleaved round-robin within each round
#     (glibc, mimiralloc, ...), so drift lands on both
#     legs together
# The mimir leg is built from this tree's production
# sources with the shipped flags (-O2 -flto, Makefile
# CFLAGS); the bench source itself is unmodified.
#
# Usage: run_z054.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_z054.sh <raw-output-file> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z54"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2 -flto"

gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" \
    -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" \
    -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/alloc.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" \
    -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/plat.o"
gcc $CFLAGS -I"$ROOT/include" \
    -c "$ROOT/tests/perf/bench_k002.c" -o "$ZB/bench_mimir.o"
gcc $CFLAGS "$ZB/bench_mimir.o" "$ZB/alloc.o" "$ZB/core.o" \
    "$ZB/plat.o" -o "$ZB/bench_mimir"
gcc $CFLAGS -DLEG_GLIBC \
    "$ROOT/tests/perf/bench_k002.c" -o "$ZB/bench_glibc"

: > "$OUT"
# warm-ups: 3 processes per leg, discarded
i=0
while [ "$i" -lt 3 ]; do
    taskset -c 2 "$ZB/bench_glibc" > /dev/null
    taskset -c 2 "$ZB/bench_mimir" > /dev/null
    i=$((i + 1))
done
# measured: REPS rounds, legs interleaved within a round
r=0
while [ "$r" -lt "$REPS" ]; do
    taskset -c 2 "$ZB/bench_glibc" >> "$OUT"
    taskset -c 2 "$ZB/bench_mimir" >> "$OUT"
    r=$((r + 1))
done
echo "run_z054: legs=glibc,mimiralloc reps=$REPS raw=$OUT" >&2
