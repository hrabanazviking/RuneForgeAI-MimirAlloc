#!/bin/sh
# run_lockhist_z019.sh - Z-019 lock hold/wait histogram runner.
# Builds the INSTRUMENTED core (-DMIMIR_LOCK_HIST=1) into
# build/z19/ (the production build/ objects are never
# touched by this runner) plus a production core object for
# the zero-cost proof, then runs probe_z019 phases under the
# Z-004 protocol (docs/Z13_MEASUREMENT_PROTOCOL.md, banked
# at the seat):
#   - single-thread phases pinned taskset -c 2 (CCD0 core)
#   - contended phases pinned taskset -c 0-7 (one whole CCD,
#     one thread per physical core; threads = 4 and 8)
#   - 3 warm-up processes per phase, discarded
#   - measured repetitions are separate PROCESSES
#     (default 7 per phase), headline = median of processes
#     (summarize_z019.py), histograms merged across processes
# Usage: run_lockhist_z019.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_lockhist_z019.sh <raw-output-file> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z19"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
# Instrumented objects.
gcc $CFLAGS -DMIMIR_LOCK_HIST=1 -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core_hist.o"
gcc $CFLAGS -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/mimir_alloc.o"
gcc $CFLAGS -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/mimir_platform.o"
gcc $CFLAGS -DMIMIR_LOCK_HIST=1 "$ROOT/tests/perf/probe_z019.c" \
    "$ZB/mimir_alloc.o" "$ZB/mimir_core_hist.o" "$ZB/mimir_platform.o" \
    -o "$ZB/probe_z019"
# Production object for the zero-cost proof (no macro).
gcc $CFLAGS -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core_prod.o"
{
    echo "=== ZERO-COST PROOF (production object, no MIMIR_LOCK_HIST) ==="
    echo "prod core object bytes: $(wc -c < "$ZB/mimir_core_prod.o")"
    echo "hist core object bytes: $(wc -c < "$ZB/mimir_core_hist.o")"
    echo "prod z19/lock_hist symbols (expect 0): $(nm "$ZB/mimir_core_prod.o" | grep -ci lock_hist || true)"
    echo "hist z19/lock_hist symbols (expect >0): $(nm "$ZB/mimir_core_hist.o" | grep -ci lock_hist || true)"
    nm "$ZB/mimir_core_prod.o" | grep -i lock_hist || echo "prod nm: no lock_hist symbols"
    nm "$ZB/mimir_core_hist.o" | grep -i lock_hist || echo "hist nm: no lock_hist symbols"
} > "$OUT.proof"
run_phase() {
    # $1 = pin list, rest = probe args; phase key in $OUT lines
    PIN=$1; shift
    i=0
    while [ "$i" -lt 3 ]; do
        taskset -c "$PIN" "$ZB/probe_z019" "$@" > /dev/null
        i=$((i + 1))
    done
    r=0
    while [ "$r" -lt "$REPS" ]; do
        taskset -c "$PIN" "$ZB/probe_z019" "$@" >> "$OUT"
        r=$((r + 1))
    done
    echo "phase done: $*" >&2
}
: > "$OUT"
run_phase 2 overhead
for W in small_churn mixed_batch large_batch mixed_lifecycle; do
    run_phase 2 bench "$W" 1
done
run_phase 2 calloc_churn 100000
run_phase 2 realloc_cycle 20000
run_phase 0-7 contended 4 50000
run_phase 0-7 contended 8 50000
echo "raw HIST lines: $(grep -c '^HIST,' "$OUT") (expect $((REPS * 9 * 10)))" >&2
