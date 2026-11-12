#!/bin/sh
# run_z031.sh - Z-031 small-path verdict runner.
#
# Re-runs the banked K-002 oracle harness (tests/perf/bench_k002.c,
# unmodified - the exact workload code behind the Z-003/Z-009
# baseline) under the full Z-004 oracle protocol
# (docs/Z13_MEASUREMENT_PROTOCOL.md) via the protocol's own
# harness scripts/z13/run_measured.sh:
#
#   main : mimiralloc vs glibc,      9 reps (headline-of-record
#          runs use 9, protocol section 6), 3 warm-up processes
#          per leg, legs interleaved A,B,A,B, every process
#          pinned taskset -c 2 (CCD0 physical core, section 3)
#   gg   : glibc vs glibc,           7 reps (identical-leg
#          control, section 7 noise bound 5%)
#   mm   : mimiralloc vs mimiralloc, 7 reps (identical-leg
#          control, section 7)
#
# Both leg binaries are built into build/z31/ (the production
# build/ tree is never touched): the allocator objects are the
# production build (Makefile CFLAGS, exactly as `make test`
# builds them) and the bench TU is compiled -O2, per the
# Makefile's bench-k002 rule.
#
# The Z-031 gate workloads are small_churn and mixed_batch; the
# harness also runs large_batch and mixed_lifecycle, which ride
# along in the raw files as context only.
#
# Usage: run_z031.sh <out-dir>
#   writes <out-dir>/z031_main.*, z031_gg.*, z031_mm.*
#   (.header/.legA.raw/.legB.raw per run) and prints the
#   summarize_z031.py verdict table on stdout.
set -eu
OUTDIR=${1:?usage: run_z031.sh <out-dir>}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z31"
mkdir -p "$ZB" "$OUTDIR"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
gcc $CFLAGS -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core.o"
gcc $CFLAGS -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/mimir_alloc.o"
gcc $CFLAGS -c "$ROOT/src/platform/mimir_platform.c" \
    -o "$ZB/mimir_platform.o"
gcc $CFLAGS -O2 -DLEG_GLIBC -c "$ROOT/tests/perf/bench_k002.c" \
    -o "$ZB/bench_k002_glibc.o"
gcc $CFLAGS -O2 "$ZB/bench_k002_glibc.o" -o "$ZB/bench_k002_glibc"
gcc $CFLAGS -O2 -c "$ROOT/tests/perf/bench_k002.c" \
    -o "$ZB/bench_k002_mimir.o"
gcc $CFLAGS "$ZB/mimir_alloc.o" "$ZB/mimir_core.o" \
    "$ZB/mimir_platform.o" "$ZB/bench_k002_mimir.o" \
    -o "$ZB/bench_k002_mimir"
GBIN="$ZB/bench_k002_glibc"
MBIN="$ZB/bench_k002_mimir"
MEAS="$ROOT/scripts/z13/run_measured.sh"

sh "$MEAS" --leg-a "$MBIN" --leg-b "$GBIN" \
    --out "$OUTDIR/z031_main" --cpus 2 --reps 9 --warmups 3
echo "main run done" >&2
sh "$MEAS" --leg-a "$GBIN" --leg-b "$GBIN" \
    --out "$OUTDIR/z031_gg" --cpus 2 --reps 7 --warmups 3
echo "gg control done" >&2
sh "$MEAS" --leg-a "$MBIN" --leg-b "$MBIN" \
    --out "$OUTDIR/z031_mm" --cpus 2 --reps 7 --warmups 3
echo "mm control done" >&2

python3 "$DIR/summarize_z031.py" \
    "$OUTDIR/z031_main" "$OUTDIR/z031_gg" "$OUTDIR/z031_mm"
