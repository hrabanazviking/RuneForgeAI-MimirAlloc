#!/bin/sh
# run_energy_z007.sh - Z-007 energy runner (RAPL package energy).
#
# Measures RAPL package energy per workload for both legs
# (glibc, mimiralloc) around the Z-007 probe (tests/perf/
# probe_z007.c, which wraps the UNMODIFIED banked bench_k002.c
# workloads, one workload per process), and appends raw blocks
# to the output file named by $1:
#   === <leg> <workload> proc=<n> reps=<r> wall_ns=<w> ===
#   <probe CSV line>
#   <perf stat -a -x lines: count|unit|event|time|pct||>
# Idle blocks are recorded the same way with leg=idle,
# workload=sleep (perf around `sleep 3`): one idle sample
# before each workload block and one at the end, so machine
# drift lands on the idle estimate as well as on both legs.
# Legs alternate process by process (glibc, mimiralloc) per
# workload, house pattern from run_bench_k002.sh.
# Default 5 measured processes per leg per workload.
#
# ENERGY ACCESS ON THIS HOST (measured at Z-007 stand-up):
# - /sys/class/powercap/intel-rapl:0 (package-0) and :0:0
#   (core) EXIST on this AMD Strix Halo host; the kernel
#   exposes them through the intel_rapl_msr / rapl modules.
#   energy_uj is root-only (unprivileged read: Permission
#   denied); max_energy_range_uj = 65532610987 uJ.
# - perf events power/energy-pkg/ and power_core/energy-core/
#   EXIST but are SYSTEM-WIDE only: per-process perf stat
#   reports <not supported> (proven at stand-up). They count
#   only with `perf stat -a`.
# - Unprivileged perf_event_open is blocked entirely
#   (perf_event_paranoid = 4, same proof as Z-005).
# The runner therefore uses `sudo -n perf stat -a`
# (passwordless sudo already granted on this host); it does
# NOT change any sysctl or host setting. If sudo/perf/energy
# is unavailable the runner fails loudly and records nothing.
#
# METHOD (idle-baseline subtraction, stated per the roadmap):
#   idle_W   = median over idle samples of (idle_J / wall_s)
#   net_J    = gross_pkg_J - idle_W * wall_s   (per process)
#   J/op     = net_J / total_ops               (per process)
# Headline per (leg, workload) is the MEDIAN of per-process
# net J/op values (Z-004 protocol: median of processes).
# perf -a measures the WHOLE package (all co-tenants), never
# the process alone; the idle subtraction removes the idle
# floor, not co-tenant load - the run header records load.
#
# Fix (Z-007 resume, 2026-11-12): idle_sample must truncate
# $OUT.perf before capturing and use 2> not 2>> - the original
# append form bled the previous block perf lines into every
# idle block after the first (seen in the smoke raw).
# Pinning: workload processes are pinned to CPU 2 (CCD0) per
# Z-004 protocol section 3, identical pin for both legs.
# Usage: run_energy_z007.sh <raw-output-file> [procs]
set -eu
OUT=${1:?usage: run_energy_z007.sh <raw-output-file> [procs]}
PROCS=${2:-5}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
GBIN="$ROOT/build/probe_z007_glibc"
MBIN="$ROOT/build/probe_z007_mimir"
EVENTS="power/energy-pkg/,power_core/energy-core/"
PIN=2
# Timed passes per probe process, per workload. Chosen so
# every process runs long enough for the RAPL counter
# (~1 ms update, package ~8-30 W) to integrate a measurable
# signal on BOTH legs: the fastest leg (glibc) reaches
# ~0.06-1.2 s and the slowest (mimiralloc) ~2-5 s. The
# 1/10-scale warm-up inside each process is <= 0.7% of the
# timed ops at these rep counts (stated in the recorded run).
reps_for() {
    case "$1" in
        small_churn) echo 200 ;;
        mixed_batch) echo 30 ;;
        large_batch) echo 100 ;;
        mixed_lifecycle) echo 15 ;;
    esac
}
if [ ! -x "$GBIN" ] || [ ! -x "$MBIN" ]; then
    cd "$ROOT"
    [ -f build/mimir_alloc.o ] || make bench-k002 > /dev/null
    gcc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 \
        -DLEG_GLIBC tests/perf/probe_z007.c -o "$GBIN"
    gcc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 \
        tests/perf/probe_z007.c build/mimir_alloc.o build/mimir_core.o \
        build/mimir_platform.o -o "$MBIN"
fi
sudo -n perf stat -a -e power/energy-pkg/ -- sleep 1 > /dev/null 2>&1 || {
    echo "perf stat -a energy via sudo -n unavailable - no energy recorded" >&2
    exit 1
}
idle_sample() {
    N=$1
    W0=$(date +%s%N)
    echo "=== idle sleep sample=$N reps=0 ===" >> "$OUT"
    : > "$OUT.perf"
    sudo -n perf stat -a -x '|' -e "$EVENTS" -- sleep 3 \
        >> "$OUT" 2> "$OUT.perf"
    cat "$OUT.perf" >> "$OUT"
    : > "$OUT.perf"
    W1=$(date +%s%N)
    echo "idle_wall_ns=$((W1 - W0))" >> "$OUT"
}
: > "$OUT"
S=1
for W in small_churn mixed_batch large_batch mixed_lifecycle; do
    idle_sample "$S"; S=$((S + 1))
    R=$(reps_for "$W")
    p=1
    while [ "$p" -le "$PROCS" ]; do
        for LEG in glibc mimiralloc; do
            if [ "$LEG" = glibc ]; then BIN="$GBIN"; else BIN="$MBIN"; fi
            W0=$(date +%s%N)
            sudo -n perf stat -a -x '|' -e "$EVENTS" -- \
                taskset -c "$PIN" "$BIN" "$W" "$R" \
                > "$OUT.probe" 2> "$OUT.perf"
            W1=$(date +%s%N)
            echo "=== $LEG $W proc=$p reps=$R wall_ns=$((W1 - W0)) ===" >> "$OUT"
            cat "$OUT.probe" >> "$OUT"
            cat "$OUT.perf" >> "$OUT"
        done
        p=$((p + 1))
    done
done
idle_sample "$S"
rm -f "$OUT.perf" "$OUT.probe"
echo "raw blocks: $(grep -c '^=== ' "$OUT") (expect $((PROCS * 8 + 5)))" >&2
