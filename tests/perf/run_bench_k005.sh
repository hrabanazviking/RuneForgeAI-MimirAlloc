#!/bin/sh
set -eu
OUT=${1:?usage: run_bench_k005.sh <raw> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0");ROOT=$(cd "$DIR/../.." && pwd)
BIN="$ROOT/build/bench_k005"
[ -x "$BIN" ] || { echo "build first: make bench-k005" >&2; exit 1; }
: > "$OUT";r=1
while [ "$r" -le "$REPS" ]; do "$BIN" >> "$OUT"; r=$((r+1)); done
echo "raw lines: $(wc -l < "$OUT") (expect $((REPS*8)))" >&2
