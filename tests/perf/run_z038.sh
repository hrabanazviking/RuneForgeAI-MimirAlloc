#!/bin/sh
# run_z038.sh - Z-038 Arena ladder runner (MimirAlloc Z13).
# One session, one probe source (tests/perf/probe_z038.c), the
# frozen Z-020/Z-021 ladder shape, three legs:
#   banked - current banked core objects (src/core/mimir_core.c,
#            src/api/mimir_alloc.c, src/platform/mimir_platform.c
#            built fresh under build/z38/, production objects
#            elsewhere untouched), probe built default.
#   glibc  - the same probe built with -DLEG_GLIBC
#            (bench_k002.c's own leg switch); no allocator
#            objects.
#   arena  - the Z-037 per-CCD arena variant. Detection: any
#            MIMIR_ARENA marker in Makefile / src / include. If
#            no marker exists, the leg is OWED: the runner still
#            attempts the -DLEG_ARENA build, whose #error (in
#            probe_z038.c) refuses to measure the banked core
#            under the arena name, and records the attempt's
#            failure lines plus an ARENA_OWED marker in the
#            arena raw. If a marker exists, the build is
#            built for real under the landed Z-037 contract
#            (core+platform -DMIMIR_ARENA_CCD=1, shared API
#            object - the run_z037.sh pattern) and laddered
#            like the other legs; any failure is shouted.
# Protocol (docs/Z13_MEASUREMENT_PROTOCOL.md, Z-004), identical
# to Z-020/Z-021: per-point load comments in each leg raw, one
# discarded warm-up PROCESS per ladder point at 1/10 scale,
# REPS separate processes per point (default 5), 2 s pauses
# between points, taskset -c 0-31 wrapper (per-thread pinning
# happens inside the probe), per-thread timing window (Z-021).
# Usage: run_z038.sh [reps]
set -eu
cd "$(dirname "$0")/../.." || exit 2
REPS=${1:-5}
ZB=build/z38
mkdir -p "$ZB"
RAWB=/tmp/z038_banked.raw
RAWG=/tmp/z038_glibc.raw
RAWA=/tmp/z038_arena.raw
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
echo "=== Z-038 Arena ladder (banked / arena / glibc) ==="
date -Is; uname -a; git rev-parse HEAD
echo "--- host truth (scripts/z13/host_truth.sh, Z-002) ---"
sh scripts/z13/host_truth.sh
echo "--- load (start) ---"; uptime; cat /proc/loadavg; ps aux --sort=-%cpu | head -6
echo "--- topology (sysfs) ---"
for c in 0 8 16 24; do
  echo "cpu$c core_id=$(cat /sys/devices/system/cpu/cpu$c/topology/core_id) siblings=$(cat /sys/devices/system/cpu/cpu$c/topology/thread_siblings_list) L3=$(cat /sys/devices/system/cpu/cpu$c/cache/index3/size) L3_shared=$(cat /sys/devices/system/cpu/cpu$c/cache/index3/shared_cpu_list)"
done
echo "governor cpu0: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
echo "--- build banked leg ---"
cc $CFLAGS -c src/core/mimir_core.c -o "$ZB/mimir_core.o"
cc $CFLAGS -c src/api/mimir_alloc.c -o "$ZB/mimir_alloc.o"
cc $CFLAGS -c src/platform/mimir_platform.c -o "$ZB/mimir_platform.o"
cc $CFLAGS tests/perf/probe_z038.c \
    "$ZB/mimir_alloc.o" "$ZB/mimir_core.o" "$ZB/mimir_platform.o" \
    -o "$ZB/probe_z038_banked"
echo "banked build exit=0"
echo "--- build glibc leg ---"
cc $CFLAGS -DLEG_GLIBC tests/perf/probe_z038.c -o "$ZB/probe_z038_glibc"
echo "glibc build exit=0"
sha256sum tests/perf/probe_z038.c tests/perf/run_z038.sh tests/perf/summarize_z038.py "$ZB/probe_z038_banked" "$ZB/probe_z038_glibc"
echo "--- arena leg detection (Z-037 prototype) ---"
ARENA_MARKERS=$(grep -rl MIMIR_ARENA Makefile src include 2>/dev/null | head -5 || true)
if [ -n "$ARENA_MARKERS" ]; then
  echo "arena markers found: $ARENA_MARKERS"
  echo "--- build arena leg (landed Z-037 contract: core+platform -DMIMIR_ARENA_CCD=1, shared API object, per run_z037.sh) ---"
  cc $CFLAGS -DMIMIR_ARENA_CCD=1 -c src/core/mimir_core.c -o "$ZB/mimir_core_arena.o"
  cc $CFLAGS -DMIMIR_ARENA_CCD=1 -c src/platform/mimir_platform.c -o "$ZB/mimir_platform_arena.o"
  cc $CFLAGS -DLEG_ARENA -DMIMIR_ARENA_CCD=1 tests/perf/probe_z038.c       "$ZB/mimir_alloc.o" "$ZB/mimir_core_arena.o" "$ZB/mimir_platform_arena.o"       -o "$ZB/probe_z038_arena"
  echo "arena build exit=0"
  sha256sum "$ZB/probe_z038_arena"
else
  echo "no MIMIR_ARENA marker in Makefile/src/include: Z-037 prototype NOT landed in this tree"
  echo "# Z-038 arena leg" > "$RAWA"
  echo "ARENA_OWED reason=Z-037-prototype-not-landed-in-this-tree" >> "$RAWA"
  echo "--- arena build attempt (-DLEG_ARENA, expected to fail loudly while Z-037 is unlanded) ---"
  if cc $CFLAGS -DLEG_ARENA tests/perf/probe_z038.c -o "$ZB/probe_z038_arena" 2> "$ZB/arena_build.err"; then
    echo "ARENA BUILD UNEXPECTEDLY SUCCEEDED - investigate before trusting any arena number" | tee -a "$RAWA"
  else
    echo "arena build failed as designed; compiler said:" 
    head -4 "$ZB/arena_build.err"
    sed 's/^/# arena-build: /' "$ZB/arena_build.err" | head -4 >> "$RAWA"
  fi
fi
run_leg() {
    # $1 binary, $2 raw file
    BIN=$1; OUT=$2
    : > "$OUT"
    echo "--- selfcheck ($BIN) ---"
    taskset -c 0-31 "$BIN" selfcheck | tee "$OUT"
    for T in 1 2 3 4 6 8 12 16 20 24 28 32; do
        echo "# point workload=small_churn threads=$T iters=50000 loadavg=$(cat /proc/loadavg)" >> "$OUT"
        taskset -c 0-31 "$BIN" ladder small_churn "$T" 5000 > /dev/null
        r=0
        while [ "$r" -lt "$REPS" ]; do
            taskset -c 0-31 "$BIN" ladder small_churn "$T" 50000 >> "$OUT"
            r=$((r + 1))
        done
        echo "point done: $BIN small_churn t=$T"
        sleep 2
    done
    for T in 1 2 3 4 6 8 12 16 20 24 28 32; do
        echo "# point workload=mixed_batch threads=$T iters=200 loadavg=$(cat /proc/loadavg)" >> "$OUT"
        taskset -c 0-31 "$BIN" ladder mixed_batch "$T" 20 > /dev/null
        r=0
        while [ "$r" -lt "$REPS" ]; do
            taskset -c 0-31 "$BIN" ladder mixed_batch "$T" 200 >> "$OUT"
            r=$((r + 1))
        done
        echo "point done: $BIN mixed_batch t=$T"
        sleep 2
    done
    echo "raw LADDER lines ($OUT): $(grep -c '^LADDER,' "$OUT")"
}
echo "--- ladder: banked leg ---"
run_leg "$ZB/probe_z038_banked" "$RAWB"
echo "--- ladder: glibc leg ---"
run_leg "$ZB/probe_z038_glibc" "$RAWG"
if [ -x "$ZB/probe_z038_arena" ]; then
  echo "--- ladder: arena leg ---"
  run_leg "$ZB/probe_z038_arena" "$RAWA"
fi
echo "--- summary (summarize_z038.py, three-way) ---"
python3 tests/perf/summarize_z038.py banked="$RAWB" glibc="$RAWG" arena="$RAWA"; echo "summarize exit=$?"
echo "--- load (end) ---"; uptime; cat /proc/loadavg
echo "--- run_z038 done ---"
