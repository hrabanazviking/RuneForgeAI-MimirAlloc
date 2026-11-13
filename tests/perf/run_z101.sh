#!/bin/sh
# run_z101.sh - Z-101 storm battery, final form runner.
# Builds probe_z101.c twice into build/z101/ (the Z-034 /
# Z-042 pattern: production objects built fresh here, the
# production build/ tree is never touched, and there is NO
# Makefile target for this slice, so the standing gate's
# counts cannot move):
#   probe_z101_mimir - the combined battery against the
#                      near-final core, with the full
#                      accounting phases.
#   probe_z101_glibc - the identical shapes against
#                      malloc/free (-DUSE_GLIBC), wall-time
#                      comparator only.
# Protocol (Z-004): one warm-up process per leg, discarded;
# then 3 recorded processes per leg, alternating legs.
# Every probe process runs, in order: calibration, the
# COMBINED storm (16 churn loopers + 16 ring cross-free
# workers + 256 fork+exec children, overlapping), the
# 10,000-thread churn storm (Z-042 scale), the 32-thread
# ring free-storm (Z-043 S1 scale, 196,608 cross frees),
# the fork storm (16 quiescent fork+alloc + 500 fork+exec),
# and - mimir leg - the byte-exact trim close. A probe
# that exits nonzero marks its REP section and the runner
# exits nonzero at the end, with all raw output kept.
# Usage: run_z101.sh <raw-output-file>
set -u
OUT=${1:?usage: run_z101.sh <raw-output-file>}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z101"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
gcc $CFLAGS -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core.o" || exit 1
gcc $CFLAGS -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/mimir_alloc.o" || exit 1
gcc $CFLAGS -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/mimir_platform.o" || exit 1
gcc $CFLAGS "$ROOT/tests/perf/probe_z101.c" \
    "$ZB/mimir_alloc.o" "$ZB/mimir_core.o" "$ZB/mimir_platform.o" \
    -o "$ZB/probe_z101_mimir" || exit 1
gcc $CFLAGS -DUSE_GLIBC "$ROOT/tests/perf/probe_z101.c" \
    -o "$ZB/probe_z101_glibc" || exit 1
M="$ZB/probe_z101_mimir"
G="$ZB/probe_z101_glibc"
echo "warm-up mimir (discarded)" >&2
timeout 600 "$M" > "$OUT.warm_mimir" || { echo "WARM-UP MIMIR FAILED" >&2; exit 1; }
echo "warm-up glibc (discarded)" >&2
timeout 600 "$G" > "$OUT.warm_glibc" || { echo "WARM-UP GLIBC FAILED" >&2; exit 1; }
: > "$OUT"
FAIL=0
i=1
while [ "$i" -le 3 ]; do
    echo "=== REP leg=mimir i=$i ===" >> "$OUT"
    timeout 600 "$M" >> "$OUT" 2>&1
    rc=$?
    echo "PROBE EXIT: $rc" >> "$OUT"
    [ "$rc" -eq 0 ] || FAIL=1
    echo "=== REP leg=glibc i=$i ===" >> "$OUT"
    timeout 600 "$G" >> "$OUT" 2>&1
    rc=$?
    echo "PROBE EXIT: $rc" >> "$OUT"
    [ "$rc" -eq 0 ] || FAIL=1
    echo "rep $i done" >&2
    i=$((i + 1))
done
echo "raw written: $OUT (fail=$FAIL)" >&2
exit $FAIL
