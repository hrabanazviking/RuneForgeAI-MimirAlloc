#!/bin/sh
# run_bench_k002.sh - K-002 baseline benchmark runner.
# Runs each leg binary REPS times as separate processes
# (default 7, minimum 5 per the slice contract), in
# alternating leg order (glibc, mimiralloc, glibc, ...)
# so slow machine drift lands on both legs, and appends
# every raw CSV line to the output file named by $1.
# Usage: run_bench_k002.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_bench_k002.sh <raw-output-file> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
GBIN="$ROOT/build/bench_k002_glibc"
MBIN="$ROOT/build/bench_k002_mimir"
[ -x "$GBIN" ] && [ -x "$MBIN" ] || { echo "build first: make bench-k002" >&2; exit 1; }
: > "$OUT"
r=1
while [ "$r" -le "$REPS" ]; do
    "$GBIN" >> "$OUT"
    "$MBIN" >> "$OUT"
    r=$((r + 1))
done
echo "raw lines: $(wc -l < "$OUT") (expect $((REPS * 8)))" >&2
