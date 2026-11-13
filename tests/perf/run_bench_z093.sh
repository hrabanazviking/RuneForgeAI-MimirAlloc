#!/bin/sh
# run_bench_z093.sh - drive bench_z093 reps (K-007 runner shape).
# Usage: run_bench_z093.sh <raw-out> [reps]
set -eu
OUT=${1:?usage: run_bench_z093.sh <raw> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0");ROOT=$(cd "$DIR/../.." && pwd)
G="$ROOT/build/bench_z093_glibc";M="$ROOT/build/bench_z093_mimir"
[ -x "$G" ] && [ -x "$M" ] || { echo "build first: tests/perf/run_z093.sh" >&2; exit 1; }
KEYS="inter1:64 burst8:64 burst32:64 burst128:64 burst512:64 burst512fifo:64 inter1:1024 burst8:1024 burst32:1024 burst128:1024 burst512:1024 burst512fifo:1024 mixbatch512:0"
: > "$OUT";r=1
while [ "$r" -le "$REPS" ]; do
 for k in $KEYS; do "$G" "$k" >> "$OUT"; "$M" "$k" >> "$OUT"; done
 r=$((r+1)); done
echo "raw lines: $(wc -l < "$OUT") (expect $((REPS*26)))" >&2
