#!/bin/sh
# run_perf_z005.sh - Z-005 perf-counter runner.
# Runs the Z-005 probe (tests/perf/probe_z005.c, which wraps
# the UNMODIFIED banked bench_k002.c workloads, one workload
# per process) under `perf stat` for both legs and appends
# raw blocks to the output file named by $1:
#   === <leg> <workload> proc=<n> reps=<r> ===
#   <probe CSV line>
#   <perf stat -x lines: count||event|time|pct||>
# Legs alternate process by process (glibc, mimiralloc) per
# workload, house pattern from run_bench_k002.sh, so machine
# drift lands on both legs. Default 5 measured processes per
# leg per workload.
#
# PERF ACCESS ON THIS HOST: unprivileged perf_event_open is
# blocked (perf_event_paranoid = 4, proven in the recorded
# run). The runner therefore uses `sudo -n perf stat`
# (passwordless sudo already granted on this host); it does
# NOT change any sysctl or host setting. If sudo/perf is
# unavailable the runner fails loudly and records nothing.
# Usage: run_perf_z005.sh <raw-output-file> [procs]
set -eu
OUT=${1:?usage: run_perf_z005.sh <raw-output-file> [procs]}
PROCS=${2:-5}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
GBIN="$ROOT/build/probe_z005_glibc"
MBIN="$ROOT/build/probe_z005_mimir"
EVENTS="cycles,instructions,cache-misses,branch-misses"
# Timed passes per probe process, per workload: keeps the
# 1/10-scale warm-up share of executed ops <= 2% and the
# timed window long enough to amortise process start-up.
reps_for() {
    case "$1" in
        small_churn) echo 20 ;;
        mixed_batch) echo 5 ;;
        large_batch) echo 20 ;;
        mixed_lifecycle) echo 10 ;;
    esac
}
if [ ! -x "$GBIN" ] || [ ! -x "$MBIN" ]; then
    cd "$ROOT"
    [ -f build/mimir_alloc.o ] || make bench-k002 > /dev/null
    gcc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 \
        -DLEG_GLIBC tests/perf/probe_z005.c -o "$GBIN"
    gcc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 \
        tests/perf/probe_z005.c build/mimir_alloc.o build/mimir_core.o \
        build/mimir_platform.o -o "$MBIN"
fi
sudo -n perf stat -e cycles -- true > /dev/null 2>&1 || {
    echo "perf stat via sudo -n unavailable - no counters recorded" >&2
    exit 1
}
: > "$OUT"
for W in small_churn mixed_batch large_batch mixed_lifecycle; do
    R=$(reps_for "$W")
    p=1
    while [ "$p" -le "$PROCS" ]; do
        for LEG in glibc mimiralloc; do
            if [ "$LEG" = glibc ]; then BIN="$GBIN"; else BIN="$MBIN"; fi
            echo "=== $LEG $W proc=$p reps=$R ===" >> "$OUT"
            sudo -n perf stat -x '|' -e "$EVENTS" -- "$BIN" "$W" "$R" \
                >> "$OUT" 2>> "$OUT.perf"
            cat "$OUT.perf" >> "$OUT"
            : > "$OUT.perf"
        done
        p=$((p + 1))
    done
done
rm -f "$OUT.perf"
echo "raw blocks: $(grep -c '^=== ' "$OUT") (expect $((PROCS * 8)))" >&2
