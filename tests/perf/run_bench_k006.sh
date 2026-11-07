#!/bin/sh
# run_bench_k006.sh - K-006 false-sharing + scaling runner.
# Every repetition is a separate process (K-001 M-1).
# Within a repetition the legs alternate (glibc, mimiralloc)
# so slow machine drift lands on both legs; thread counts
# run 1, 2, 4, 8 in order. Raw CSV and ADDR lines are
# appended to the output file named by $1.
# Usage: run_bench_k006.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_bench_k006.sh <raw-output-file> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
GBIN="$ROOT/build/bench_k006_glibc"
MBIN="$ROOT/build/bench_k006_mimir"
[ -x "$GBIN" ] && [ -x "$MBIN" ] || { echo "build first: make bench-k006" >&2; exit 1; }
: > "$OUT"
r=1
while [ "$r" -le "$REPS" ]; do
    for T in 1 2 4 8; do
        "$GBIN" scale "$T" >> "$OUT"
        "$MBIN" scale "$T" >> "$OUT"
    done
    for T in 1 2 4 8; do
        "$MBIN" fs_packed "$T" >> "$OUT"
        "$MBIN" fs_padded "$T" >> "$OUT"
    done
    for T in 1 2 4 8; do
        "$GBIN" fs_alloc16 "$T" >> "$OUT"
        "$MBIN" fs_alloc16 "$T" >> "$OUT"
        "$GBIN" fs_alloc128 "$T" >> "$OUT"
        "$MBIN" fs_alloc128 "$T" >> "$OUT"
    done
    r=$((r + 1))
done
echo "raw lines: $(wc -l < "$OUT")" >&2
