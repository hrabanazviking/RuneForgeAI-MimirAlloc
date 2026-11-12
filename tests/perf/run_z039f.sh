#!/bin/sh
# run_z039f.sh - Z-039 follow-up runner (MimirAlloc Z13).
# The geometry-B (lock-sharding) evidence session, one probe
# source (tests/perf/probe_z039f.c), the frozen Z-020/Z-021/
# Z-038 ladder shape, two legs:
#   banked  - this tree built flag-off. The zero-cost proof
#             below shows the flag-off core/platform/api
#             objects byte-identical to the pristine seat tree
#             (/home/volmarr/mimiralloc-z13-forge), so this
#             leg IS the banked allocator, re-measured fresh.
#   sharded - geometry B: core + platform built with
#             -DMIMIR_LOCK_SHARD=1, shared API object, probe
#             built -DLEG_SHARDED -DMIMIR_LOCK_SHARD=1 (the
#             probe #error refuses the sharded name without
#             the flag).
# Plus: the flag-on suite (the eight make-test binaries built
# against the sharded core) and the shardcheck correctness
# phase on the sharded probe.
# Protocol (docs/Z13_MEASUREMENT_PROTOCOL.md, Z-004), identical
# to Z-038: per-point load comments in each leg raw, one
# discarded warm-up PROCESS per ladder point at 1/10 scale,
# REPS separate processes per point (default 5), 2 s pauses
# between points, taskset -c 0-31 wrapper (per-thread pinning
# happens inside the probe), per-thread timing window (Z-021).
# Usage: run_z039f.sh [reps]
set -eu
cd "$(dirname "$0")/../.." || exit 2
REPS=${1:-5}
ZB=build/z39f
SEAT=/home/volmarr/mimiralloc-z13-forge
mkdir -p "$ZB/seat"
RAWB=/tmp/z039f_banked.raw
RAWS=/tmp/z039f_sharded.raw
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
echo "=== Z-039 follow-up (banked / sharded) ==="
date -Is; uname -a; git rev-parse HEAD
echo "--- host truth (scripts/z13/host_truth.sh, Z-002) ---"
sh scripts/z13/host_truth.sh
echo "--- load (start) ---"; uptime; cat /proc/loadavg; ps aux --sort=-%cpu | head -6
echo "--- topology (sysfs) ---"
for c in 0 8 16 24; do
  echo "cpu$c core_id=$(cat /sys/devices/system/cpu/cpu$c/topology/core_id) siblings=$(cat /sys/devices/system/cpu/cpu$c/topology/thread_siblings_list) L3=$(cat /sys/devices/system/cpu/cpu$c/cache/index3/size) L3_shared=$(cat /sys/devices/system/cpu/cpu$c/cache/index3/shared_cpu_list)"
done
echo "governor cpu0: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
echo "--- build banked leg (flag off) ---"
cc $CFLAGS -c src/core/mimir_core.c -o "$ZB/core_prod.o"
cc $CFLAGS -c src/api/mimir_alloc.c -o "$ZB/alloc.o"
cc $CFLAGS -c src/platform/mimir_platform.c -o "$ZB/plat_prod.o"
cc $CFLAGS tests/perf/probe_z039f.c \
    "$ZB/alloc.o" "$ZB/core_prod.o" "$ZB/plat_prod.o" \
    -o "$ZB/probe_z039f_banked"
echo "banked build exit=0"
echo "--- build sharded leg (MIMIR_LOCK_SHARD) ---"
cc $CFLAGS -DMIMIR_LOCK_SHARD=1 -c src/core/mimir_core.c -o "$ZB/core_shard.o"
cc $CFLAGS -DMIMIR_LOCK_SHARD=1 -c src/platform/mimir_platform.c -o "$ZB/plat_shard.o"
cc $CFLAGS -DLEG_SHARDED -DMIMIR_LOCK_SHARD=1 tests/perf/probe_z039f.c \
    "$ZB/alloc.o" "$ZB/core_shard.o" "$ZB/plat_shard.o" \
    -o "$ZB/probe_z039f_sharded"
echo "sharded build exit=0"
sha256sum tests/perf/probe_z039f.c tests/perf/run_z039f.sh tests/perf/summarize_z039f.py "$ZB/probe_z039f_banked" "$ZB/probe_z039f_sharded"
echo "--- zero-cost proof (flag-off objects vs pristine seat tree) ---"
gcc -std=c11 -Wall -Wextra -Werror -pthread -I"$SEAT/include" -I"$SEAT/src" -O2 -c "$SEAT/src/core/mimir_core.c" -o "$ZB/seat/core.o"
gcc -std=c11 -Wall -Wextra -Werror -pthread -I"$SEAT/include" -I"$SEAT/src" -O2 -c "$SEAT/src/platform/mimir_platform.c" -o "$ZB/seat/plat.o"
gcc -std=c11 -Wall -Wextra -Werror -pthread -I"$SEAT/include" -I"$SEAT/src" -O2 -c "$SEAT/src/api/mimir_alloc.c" -o "$ZB/seat/alloc.o"
{
    for pair in "core_prod.o seat/core.o CORE" "plat_prod.o seat/plat.o PLATFORM" "alloc.o seat/alloc.o API"; do
        set -- $pair
        a=$(sha256sum "$ZB/$1" | cut -d' ' -f1)
        b=$(sha256sum "$ZB/$2" | cut -d' ' -f1)
        if [ "$a" = "$b" ]; then echo "$3 IDENTICAL $a"; else echo "$3 DIFFER a=$a b=$b"; fi
    done
    echo "prod shard symbols (expect 0): $(nm "$ZB/core_prod.o" | grep -ci shard || true)"
    echo "shard shard symbols (expect >0): $(nm "$ZB/core_shard.o" | grep -ci shard || true)"
    echo "prod core bytes: $(wc -c < "$ZB/core_prod.o") shard core bytes: $(wc -c < "$ZB/core_shard.o")"
} | tee "$RAWB.proof"
echo "--- flag-on suite (eight make-test binaries, sharded core) ---"
: > "$RAWS.suite"
{
    for t in contract/test_smoke core/test_core core/test_b007 core/test_c005 core/test_c006 contract/test_c007 core/test_c008 concurrency/test_d004; do
        name=$(basename "$t")
        cc $CFLAGS -DMIMIR_LOCK_SHARD=1 "tests/$t.c" "$ZB/alloc.o" "$ZB/core_shard.o" "$ZB/plat_shard.o" -o "$ZB/suite_$name"
        echo "=== SUITE $name (flag on) ==="
        "$ZB/suite_$name" || { echo "SUITE $name FAILED rc=$?"; exit 1; }
    done
} >> "$RAWS.suite" 2>&1 || { echo "flag-on suite FAILED - see $RAWS.suite" >&2; exit 1; }
echo "flag-on suite done: $RAWS.suite"
echo "--- shardcheck (sharded probe) ---"
"$ZB/probe_z039f_sharded" shardcheck | tee "$RAWS.shardcheck"
run_leg() {
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
run_leg "$ZB/probe_z039f_banked" "$RAWB"
echo "--- ladder: sharded leg ---"
run_leg "$ZB/probe_z039f_sharded" "$RAWS"
echo "--- summary (summarize_z039f.py, two-way) ---"
python3 tests/perf/summarize_z039f.py banked="$RAWB" sharded="$RAWS"; echo "summarize exit=$?"
echo "--- load (end) ---"; uptime; cat /proc/loadavg
echo "--- run_z039f done ---"
