#!/bin/sh
# run_z034.sh - Z-034 metadata diet runner (Z-019 pattern).
# Builds the production core and the trial core
# (-DMIMIR_OOL_META) into build/z34/ -- the production
# build/ objects are never touched -- plus the zero-cost
# proof, then runs probe_z034 in both legs:
#   table mode (header-cost audit, stats-verified),
#   pattern mode (out-of-line behavioural proof),
#   churn modes for classes 16/32/48/64 (trialled) and 128
#   (untrialled control), 7 alternating processes per leg,
#   single-thread phases pinned taskset -c 2 (Z-004 protocol:
#   3 warm-up processes per phase, discarded).
# Usage: run_z034.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_z034.sh <raw-output-file> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z34"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
# Production objects.
gcc $CFLAGS -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core_prod.o"
gcc $CFLAGS -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/alloc.o"
gcc $CFLAGS -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/plat.o"
gcc $CFLAGS "$ROOT/tests/perf/probe_z034.c" \
    "$ZB/alloc.o" "$ZB/core_prod.o" "$ZB/plat.o" \
    -o "$ZB/probe_prod"
# Trial objects.
gcc $CFLAGS -DMIMIR_OOL_META=1 -c "$ROOT/src/core/mimir_core.c" \
    -o "$ZB/core_ool.o"
gcc $CFLAGS "$ROOT/tests/perf/probe_z034.c" \
    "$ZB/alloc.o" "$ZB/core_ool.o" "$ZB/plat.o" \
    -o "$ZB/probe_ool"
{
    echo "=== ZERO-COST PROOF (production object, no MIMIR_OOL_META) ==="
    echo "prod core object bytes: $(wc -c < "$ZB/core_prod.o")"
    echo "ool  core object bytes: $(wc -c < "$ZB/core_ool.o")"
    echo "prod ool symbols (expect 0): $(nm "$ZB/core_prod.o" | grep -ci ool || true)"
    echo "ool  ool symbols (expect >0): $(nm "$ZB/core_ool.o" | grep -ci ool || true)"
    nm "$ZB/core_prod.o" | grep -i ool || echo "prod nm: no ool symbols"
} > "$OUT.proof"
: > "$OUT"
{
    echo "=== TABLE leg=prod ==="
    "$ZB/probe_prod" table
    echo "=== TABLE leg=ool ==="
    "$ZB/probe_ool" table
    echo "=== PATTERN leg=prod ==="
    for C in 16 32 48 64 128; do "$ZB/probe_prod" pattern $C; done
    echo "=== PATTERN leg=ool ==="
    for C in 16 32 48 64 128; do "$ZB/probe_ool" pattern $C; done
} >> "$OUT"
for C in 16 32 48 64 128; do
    i=0
    while [ "$i" -lt 3 ]; do
        taskset -c 2 "$ZB/probe_prod" churn $C > /dev/null
        taskset -c 2 "$ZB/probe_ool" churn $C > /dev/null
        i=$((i + 1))
    done
    r=0
    while [ "$r" -lt "$REPS" ]; do
        echo "=== CHURN class=$C leg=prod rep=$r ===" >> "$OUT"
        taskset -c 2 "$ZB/probe_prod" churn $C >> "$OUT"
        echo "=== CHURN class=$C leg=ool rep=$r ===" >> "$OUT"
        taskset -c 2 "$ZB/probe_ool" churn $C >> "$OUT"
        r=$((r + 1))
    done
    echo "phase done: churn $C" >&2
done
echo "raw written: $OUT (proof: $OUT.proof)" >&2
