#!/bin/sh
# run_z042.sh - Z-042 thread-churn storm runner.
# Builds probe_z042.c twice into build/z42/ (the Z-034
# pattern: production objects built fresh here, the
# production build/ tree is never touched, and there is NO
# Makefile target for this slice, so the standing gate's
# counts cannot move):
#   probe_z042_mimir - the storm against the banked core
#                      (TLS cache of Z-029 included), with
#                      the full accounting phases.
#   probe_z042_glibc - the identical thread shape against
#                      malloc/free (-DUSE_GLIBC), wall-time
#                      comparator only.
# Protocol (Z-004): one warm-up process per leg, discarded;
# then 3 recorded processes per leg, alternating legs. Every
# probe process runs its own null-thread control (10,000
# allocator-free threads) and the full 10,000-thread storm.
# A probe that exits nonzero marks its REP section and the
# runner exits nonzero at the end, with all raw output kept.
# Usage: run_z042.sh <raw-output-file>
set -u
OUT=${1:?usage: run_z042.sh <raw-output-file>}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z42"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
gcc $CFLAGS -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core.o" || exit 1
gcc $CFLAGS -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/mimir_alloc.o" || exit 1
gcc $CFLAGS -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/mimir_platform.o" || exit 1
gcc $CFLAGS "$ROOT/tests/perf/probe_z042.c" \
    "$ZB/mimir_alloc.o" "$ZB/mimir_core.o" "$ZB/mimir_platform.o" \
    -o "$ZB/probe_z042_mimir" || exit 1
gcc $CFLAGS -DUSE_GLIBC "$ROOT/tests/perf/probe_z042.c" \
    -o "$ZB/probe_z042_glibc" || exit 1
M="$ZB/probe_z042_mimir"
G="$ZB/probe_z042_glibc"
echo "warm-up mimir (discarded)" >&2
"$M" > "$OUT.warm_mimir" || { echo "WARM-UP MIMIR FAILED" >&2; exit 1; }
echo "warm-up glibc (discarded)" >&2
"$G" > "$OUT.warm_glibc" || { echo "WARM-UP GLIBC FAILED" >&2; exit 1; }
: > "$OUT"
FAIL=0
i=1
while [ "$i" -le 3 ]; do
    echo "=== REP leg=mimir i=$i ===" >> "$OUT"
    "$M" >> "$OUT" 2>&1
    rc=$?
    echo "PROBE EXIT: $rc" >> "$OUT"
    [ "$rc" -eq 0 ] || FAIL=1
    echo "=== REP leg=glibc i=$i ===" >> "$OUT"
    "$G" >> "$OUT" 2>&1
    rc=$?
    echo "PROBE EXIT: $rc" >> "$OUT"
    [ "$rc" -eq 0 ] || FAIL=1
    echo "rep $i done" >&2
    i=$((i + 1))
done
echo "raw written: $OUT (fail=$FAIL)" >&2
exit $FAIL
