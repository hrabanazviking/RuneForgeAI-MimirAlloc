#!/bin/sh
# run_z051.sh - Z-051 huge allocations runner.
# Builds the production core objects into build/z51/ (the
# production build/ tree is never touched) and runs
# probe_z051 under the Z-004 protocol
# (docs/Z13_MEASUREMENT_PROTOCOL.md):
#   - every process pinned taskset -c 2 (CCD0)
#   - 1 warm-up PROCESS per (leg,size,touch), discarded
#     (huge sizes make the usual 3 wasteful; deviation
#     recorded in the recorded run)
#   - measured repetitions are separate PROCESSES
#     (default 5 per leg, the protocol minimum), headline
#     = median of processes (summarize_z051.py)
#   - legs interleaved round by round so drift lands on
#     both legs together
# Failure-audit cases run once each (they are verdicts,
# not timings), except sequence which runs twice.
# Usage: run_z051.sh <cost-out> <fail-out> [reps]
set -eu
OUT=${1:?usage: run_z051.sh <cost> <fail> [reps]}
FAIL=${2:?usage: run_z051.sh <cost> <fail> [reps]}
REPS=${3:-5}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z51"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
gcc $CFLAGS -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core.o"
gcc $CFLAGS -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/mimir_alloc.o"
gcc $CFLAGS -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/mimir_platform.o"
gcc $CFLAGS "$ROOT/tests/perf/probe_z051.c" \
    "$ZB/mimir_alloc.o" "$ZB/mimir_core.o" "$ZB/mimir_platform.o" \
    -o "$ZB/probe_z051"
BIN="$ZB/probe_z051"
sha256sum "$ROOT/tests/perf/probe_z051.c" "$ROOT/tests/perf/run_z051.sh" \
    "$ROOT/tests/perf/summarize_z051.py" "$BIN" >&2

SIZES="268435456 536870912 1073741824 2147483648 4294967296 8589934592"
: > "$OUT"
for S in $SIZES; do
    for T in 0 1; do
        for LEG in mimir glibc; do
            taskset -c 2 "$BIN" cost "$LEG" "$S" "$T" > /dev/null
        done
        r=0
        while [ "$r" -lt "$REPS" ]; do
            for LEG in mimir glibc; do
                taskset -c 2 "$BIN" cost "$LEG" "$S" "$T" >> "$OUT"
            done
            r=$((r + 1))
        done
    done
    echo "size $S done" >&2
done

: > "$FAIL"
for CASE in overflow beyondva overcommit calloczero reallocfail sequence pair; do
    echo "case $CASE rc follows" >> "$FAIL"
    taskset -c 2 "$BIN" fail "$CASE" >> "$FAIL" 2>&1 || echo "CASE_RC_NONZERO $CASE" >> "$FAIL"
done
# sequence repeated once: restoration must be repeatable
taskset -c 2 "$BIN" fail sequence >> "$FAIL" 2>&1 || echo "CASE_RC_NONZERO sequence2" >> "$FAIL"
echo "fail audit done" >&2
