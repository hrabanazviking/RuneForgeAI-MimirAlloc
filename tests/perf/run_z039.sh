#!/bin/sh
# run_z039.sh - Z-039 Second arena geometry (evidence-conditional)
# runner: the condition-independent legs of the Z-038 three-way
# ladder, measured on the current banked core. Legs:
#   mimir - probe_z039.c (banked probe_z020.c body) against the
#           production sources built with the SHIPPED flags
#           (-O2 -flto per Z-035; TLS cache Z-029 included),
#           objects under build/z39/ (production build/ tree
#           untouched)
#   glibc - banked probe_z021.c UNMODIFIED, built -DLEG_GLIBC
#           exactly as run_z021.sh builds it (its per-thread
#           timing deviation from Z-020 is Z-021's own, kept)
# Protocol identical to Z-020/Z-021: rungs 1 2 3 4 6 8 12 16 20
# 24 28 32; fixed per-thread scales (small_churn 50000 iters,
# mixed_batch 200 batches); per-thread pinning inside the
# probes; REPS separate processes per point (default 5), one
# discarded warm-up process per point at 1/10 scale; 2 s pauses;
# per-point loadavg comment in the raw file.
# Usage: run_z039.sh <mimir|glibc> [reps]
set -eu
LEG=${1:?usage: run_z039.sh <mimir|glibc> [reps]}
REPS=${2:-5}
cd "$(dirname "$0")/../.." || exit 2
RAW=/tmp/z039_${LEG}.raw
ZB=build/z39
mkdir -p "$ZB"
echo "=== Z-039 ladder leg: $LEG (reps=$REPS) ==="
date -Is; uname -a; git rev-parse HEAD
echo "--- host truth (scripts/z13/host_truth.sh, Z-002) ---"
sh scripts/z13/host_truth.sh
echo "--- load (start) ---"; uptime; cat /proc/loadavg
echo "governor cpu0: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
if [ "$LEG" = mimir ]; then
    CF="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 -flto"
    cc $CF -c src/core/mimir_core.c -o "$ZB/mimir_core.o"
    cc $CF -c src/api/mimir_alloc.c -o "$ZB/mimir_alloc.o"
    cc $CF -c src/platform/mimir_platform.c -o "$ZB/mimir_platform.o"
    cc $CF tests/perf/probe_z039.c "$ZB/mimir_alloc.o" "$ZB/mimir_core.o" "$ZB/mimir_platform.o" -o "$ZB/probe_z039"
    PROBE="$ZB/probe_z039"
    sha256sum tests/perf/probe_z039.c tests/perf/run_z039.sh tests/perf/summarize_z039.py "$PROBE"
elif [ "$LEG" = glibc ]; then
    CF="-std=c11 -Wall -Wextra -Werror -pthread -O2"
    cc $CF -DLEG_GLIBC tests/perf/probe_z021.c -o "$ZB/probe_z039_glibc"
    PROBE="$ZB/probe_z039_glibc"
    sha256sum tests/perf/probe_z021.c tests/perf/run_z039.sh tests/perf/summarize_z039.py "$PROBE"
else
    echo "unknown leg: $LEG" >&2; exit 2
fi
echo "build exit=0"
echo "--- selfcheck ---"
taskset -c 0-31 "$PROBE" selfcheck | tee "$RAW"
run_point() {
    echo "# point leg=$LEG workload=$1 threads=$2 iters=$3 loadavg=$(cat /proc/loadavg)" >> "$RAW"
    taskset -c 0-31 "$PROBE" ladder "$1" "$2" $(($3 / 10)) > /dev/null
    r=0
    while [ "$r" -lt "$REPS" ]; do
        taskset -c 0-31 "$PROBE" ladder "$1" "$2" "$3" >> "$RAW"
        r=$((r + 1))
    done
    echo "point done: $LEG $1 t=$2"
    sleep 2
}
for T in 1 2 3 4 6 8 12 16 20 24 28 32; do
    run_point small_churn "$T" 50000
done
for T in 1 2 3 4 6 8 12 16 20 24 28 32; do
    run_point mixed_batch "$T" 200
done
echo "raw LADDER lines: $(grep -c '^LADDER,' "$RAW")"
echo "--- summary (summarize_z039.py $LEG) ---"
python3 tests/perf/summarize_z039.py "$RAW" "$LEG"; echo "summarize exit=$?"
echo "--- load (end) ---"; uptime; cat /proc/loadavg
echo "--- run_z039 $LEG done ---"
