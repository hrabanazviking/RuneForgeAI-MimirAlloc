#!/bin/sh
# run_z032.sh - Z-032 size-class spacing study runner (MimirAlloc Z13).
# Builds probe_z032.c once (MimirAlloc leg, production objects
# built fresh into build/z32) and runs it in separate PROCESSES:
#   map    x 3 executions (the request->class map must be
#          identical in all three)
#   span   x 1 fresh process per class (20 processes; the first
#          span in a fresh process is a fresh carve, so the
#          first-page run is the true objects-per-span)
#   cache  x 3 executions (mod64 distributions must agree)
# No timing is measured anywhere in this slice (the Z-017
# precedent: distributions contention cannot shift, proven by
# exact agreement across executions under differing load).
# Usage: run_z032.sh <raw-output-file>
set -eu
OUT=${1:?usage: run_z032.sh <raw-output-file>}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z32"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
gcc $CFLAGS -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core.o"
gcc $CFLAGS -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/mimir_alloc.o"
gcc $CFLAGS -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/mimir_platform.o"
gcc $CFLAGS "$ROOT/tests/perf/probe_z032.c" "$ZB/mimir_alloc.o" "$ZB/mimir_core.o" "$ZB/mimir_platform.o" -o "$ZB/probe_z032_mimir"
M="$ZB/probe_z032_mimir"
: > "$OUT"
i=1
while [ "$i" -le 3 ]; do
    echo "EXEC map $i" >> "$OUT"
    "$M" map >> "$OUT"
    i=$((i + 1))
done
for c in 16 32 48 64 80 96 112 128 160 192 224 256 320 384 448 512 640 768 896 1024; do
    echo "EXEC span $c" >> "$OUT"
    "$M" span "$c" >> "$OUT"
done
i=1
while [ "$i" -le 3 ]; do
    echo "EXEC cache $i" >> "$OUT"
    "$M" cache >> "$OUT"
    i=$((i + 1))
done
echo "raw MAP lines: $(grep -c '^MAP ' "$OUT") (expect 3072)" >&2
echo "raw SPAN lines: $(grep -c '^SPAN ' "$OUT") (expect 20)" >&2
echo "raw TOTAL lines: $(grep -c '^TOTAL ' "$OUT") (expect 60)" >&2
