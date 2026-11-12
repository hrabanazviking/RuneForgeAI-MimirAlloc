#!/bin/sh
# run_z024.sh - Z-024 Free-path anatomy runner (MimirAlloc Z13).
# Builds probe_z024.c twice (mimir leg linked with production
# objects built fresh into build/z24, glibc leg -DLEG_GLIBC)
# and runs every phase under the Z-004 protocol: separate
# PROCESSES per repetition, 3 warm-up processes per phase
# discarded, 7 measured processes per phase (override $2),
# headline = median of processes (summarize_z024.py).
# Local phases pin to CPU 2 (taskset + in-probe pin); remote
# and swap threads pin themselves to the exact CPUs named in
# the phase (CCD0 = 0-7, CCD1 = 8-15) under a wide mask.
# Usage: run_z024.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_z024.sh <raw-output-file> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z24"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
gcc $CFLAGS -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core.o"
gcc $CFLAGS -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/mimir_alloc.o"
gcc $CFLAGS -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/mimir_platform.o"
gcc $CFLAGS "$ROOT/tests/perf/probe_z024.c" "$ZB/mimir_alloc.o" "$ZB/mimir_core.o" "$ZB/mimir_platform.o" -o "$ZB/probe_z024_mimir"
gcc $CFLAGS -DLEG_GLIBC "$ROOT/tests/perf/probe_z024.c" -o "$ZB/probe_z024_glibc"
: > "$OUT"
run_phase() {
    BIN=$1; PIN=$2; shift 2
    i=0
    while [ "$i" -lt 3 ]; do
        taskset -c "$PIN" "$BIN" "$@" > /dev/null
        i=$((i + 1))
    done
    r=0
    while [ "$r" -lt "$REPS" ]; do
        taskset -c "$PIN" "$BIN" "$@" >> "$OUT"
        r=$((r + 1))
    done
    echo "phase done: $BIN $*" >&2
}
M="$ZB/probe_z024_mimir"
G="$ZB/probe_z024_glibc"
SMALL_N=8192
LARGE_N=2048
BIG_N=512
run_phase "$M" 2    local 64  $SMALL_N 0 2
run_phase "$M" 2    local 64  $SMALL_N 1 2
run_phase "$M" 2    local 512 $SMALL_N 0 2
run_phase "$M" 2    local 512 $SMALL_N 1 2
run_phase "$M" 2    localrev 64  $SMALL_N 2
run_phase "$M" 2    localrev 512 $SMALL_N 2
run_phase "$M" 2    localrev 8192 $LARGE_N 2
run_phase "$M" 0-15 remote 64 $SMALL_N 0 2 3  remote_sameCCD_steady
run_phase "$M" 0-15 remote 64 $SMALL_N 1 2 3  remote_sameCCD_emptying
run_phase "$M" 0-15 remote 64 $SMALL_N 0 2 10 remote_crossCCD_steady
run_phase "$M" 0-15 remote 64 $SMALL_N 1 2 10 remote_crossCCD_emptying
run_phase "$M" 0-15 remote 64 $SMALL_N 0 10 2 remote_crossCCD_rev_steady
run_phase "$M" 2    local 8192   $LARGE_N 1 2
run_phase "$M" 0-15 remote 8192   $LARGE_N 1 2 3  remote_sameCCD_emptying
run_phase "$M" 0-15 remote 8192   $LARGE_N 1 2 10 remote_crossCCD_emptying
run_phase "$M" 2    local 262144 $BIG_N 1 2
run_phase "$M" 0-15 remote 262144 $BIG_N 1 2 10 remote_crossCCD_emptying
run_phase "$M" 0-15 swap 64 4096 swap_sameCCD_x4  2 3 4 5
run_phase "$M" 0-15 swap 64 4096 swap_crossCCD_x4 2 10 3 11
run_phase "$G" 2    local 64  $SMALL_N 0 2
run_phase "$G" 2    local 64  $SMALL_N 1 2
run_phase "$G" 0-15 remote 64 $SMALL_N 0 2 3  remote_sameCCD_steady
run_phase "$G" 0-15 remote 64 $SMALL_N 0 2 10 remote_crossCCD_steady
run_phase "$G" 2    local 8192   $LARGE_N 1 2
run_phase "$G" 0-15 remote 8192   $LARGE_N 1 2 10 remote_crossCCD_emptying
echo "raw RESULT lines: $(grep -c '^RESULT,' "$OUT") (expect $((REPS * 25)))" >&2
