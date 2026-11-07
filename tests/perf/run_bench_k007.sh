#!/bin/sh
set -eu
OUT=${1:?usage: run_bench_k007.sh <raw> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0");ROOT=$(cd "$DIR/../.." && pwd)
G="$ROOT/build/bench_k007_glibc";M="$ROOT/build/bench_k007_mimir"
[ -x "$G" ] && [ -x "$M" ] || { echo "build first: make bench-k007" >&2; exit 1; }
KEYS="inter1:64 burst8:64 burst32:64 burst128:64 burst512:64 burst512fifo:64 inter1:1024 burst8:1024 burst32:1024 burst128:1024 burst512:1024 burst512fifo:1024"
: > "$OUT";r=1
while [ "$r" -le "$REPS" ]; do
 for k in $KEYS; do "$G" "$k" >> "$OUT"; "$M" "$k" >> "$OUT"; done
 r=$((r+1)); done
echo "raw lines: $(wc -l < "$OUT") (expect $((REPS*24)))" >&2
