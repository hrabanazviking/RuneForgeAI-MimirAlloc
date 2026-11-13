#!/bin/sh
# run_z114.sh - Z-114 Perf-per-watt tuning at the 70 W envelope.
# DERIVED on-device from the banked tests/perf/run_z110.sh /
# run_z107.sh energy leg: build and tunable engagement proof
# verbatim in shape (output names re-pointed z107->z114), the
# energy measurement is probe_z007 under sudo -n perf stat -a
# (RAPL power/energy-pkg/) with the Z-007 idle-subtraction
# method, graded by tests/perf/summarize_z114.py.
# Slice: WAVE_PLAN Wave 13 pool choice - perf-per-watt at the
# 70 W envelope vs the untuned baseline on the oracle workloads.
# ENVELOPE METHOD (recorded plainly): this host exposes RAPL
# for MEASUREMENT only - /sys/class/powercap/intel-rapl:0 has
# NO constraint_* files, so no PPT/powercap cap can be set;
# the EPP sysfs files refuse writes (EBUSY, probed 2026-11-13).
# The one working power lever is the amd-pstate frequency cap
# (cpupower frequency-set -u). Calibration on this host
# (28-proc large_batch batch, 2026-11-13): untuned 89.1 J /
# 1.08 s = 82.7 W gross package; cap 4.0 GHz 134.0 J / 1.95 s
# = 68.8 W; cap 3.0 GHz 185.9 J / 2.69 s = 69.2 W. The tuned
# leg therefore runs at cap 3.5 GHz and the runner records
# per-block gross package W so envelope compliance is read
# from the run itself, never asserted. The cap is applied
# ONLY around tuned batches and the original scaling_max_freq
# is restored by trap on any exit (verified after the run).
# BLOCKS: P=14 parallel probe processes per block (one per
# physical core, even CPUs), 5 blocks per leg per workload,
# legs interleaved per block (mimir_untuned, mimir_tuned70,
# glibc untuned context), idle samples interleaved per
# workload, reps 200/30/100/15 as the banked energy leg.
# Control bounds NEVER weakened.
set -u
cd "$(dirname "$0")/../.." || exit 2
ROOT=$PWD
HF="$ROOT/hidden_files"
mkdir -p "$HF" build
PIN_LAT=5
FAIL=0
CAP_KHZ=3500000
P=14
echo "=== Z-114 Perf-per-watt tuning at the 70 W envelope ==="
date -Is; uname -a; git rev-parse HEAD; git branch --show-current
cc --version | head -1
echo "--- load (start) ---"; uptime; cat /proc/loadavg
ps aux --sort=-%cpu | head -6
echo "--- host truth ---"
scripts/z13/host_truth.sh | tee "$HF/z114_host_truth.txt"
echo "--- envelope method probe ---"
ls /sys/class/powercap/intel-rapl:0/
echo "powercap constraint files: $(ls /sys/class/powercap/intel-rapl:0/ | grep -c constraint) (0 = RAPL is measurement-only on this host)"
ORIG_MAX=$(cat /sys/devices/system/cpu/cpu2/cpufreq/scaling_max_freq)
echo "scaling_max_freq at start: $ORIG_MAX kHz; tuned cap: $CAP_KHZ kHz; EPP: $(cat /sys/devices/system/cpu/cpu2/cpufreq/energy_performance_preference)"
restore_cap() { sudo -n cpupower frequency-set -u $ORIG_MAX > /dev/null 2>&1 || true; }
trap restore_cap EXIT INT TERM
echo "--- build ---"
BASE="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc"
make bench-k002 > "$HF/z114_build.log" 2>&1 || { echo BUILD FAIL make bench-k002; exit 1; }
for P7 in z007; do
    cc $BASE -O2 -DLEG_GLIBC tests/perf/probe_$P7.c -o "$ROOT/build/probe_z114_${P7}_glibc" >> "$HF/z114_build.log" 2>&1 || exit 1
    cc $BASE -O2 tests/perf/probe_$P7.c build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o -o "$ROOT/build/probe_z114_${P7}_mimir" >> "$HF/z114_build.log" 2>&1 || exit 1
done
cc -std=c11 -Wall -Wextra -Werror -O2 tests/perf/probe_z098.c -o "$ROOT/build/probe_z114_z098" >> "$HF/z114_build.log" 2>&1 || exit 1
sha256sum tests/perf/bench_k002.c tests/perf/run_z114.sh tests/perf/summarize_z114.py tests/perf/batch_z114.sh "$ROOT/build/probe_z114_z007_glibc" "$ROOT/build/probe_z114_z007_mimir" | tee "$HF/z114_sha.txt"
echo "--- tunable engagement proof (probe_z098) ---"
PROBE="$ROOT/build/probe_z114_z098"
hblk_of() { echo "$1" | sed -n 's/.*hblkhd_delta=\(-*[0-9]*\).*/\1/p'; }
OUT_DEF=$(env -u GLIBC_TUNABLES taskset -c $PIN_LAT "$PROBE") || FAIL=1
echo "default:   $OUT_DEF"
OUT_MM4K=$(GLIBC_TUNABLES="glibc.malloc.mmap_threshold=4096" taskset -c $PIN_LAT "$PROBE") || FAIL=1
echo "mm4k:      $OUT_MM4K"
OUT_MM1M=$(GLIBC_TUNABLES="glibc.malloc.mmap_threshold=1048576" taskset -c $PIN_LAT "$PROBE") || FAIL=1
echo "mm1m:      $OUT_MM1M"
H_DEF=$(hblk_of "$OUT_DEF"); H_4K=$(hblk_of "$OUT_MM4K"); H_1M=$(hblk_of "$OUT_MM1M")
echo "hblkhd deltas: default=$H_DEF mm4k=$H_4K mm1m=$H_1M"
[ "$H_DEF" = "0" ] || { echo "PROOF FAIL: default 64 KiB block not on heap"; FAIL=1; }
[ "$H_1M" = "0" ] || { echo "PROOF FAIL: mm1m 64 KiB block not on heap"; FAIL=1; }
[ "${H_4K:-0}" -ge 60000 ] || { echo "PROOF FAIL: mm4k 64 KiB block not mmap-served"; FAIL=1; }
[ $FAIL -eq 0 ] && echo "engagement proof: PASS (mmap_threshold channel live)" || echo "engagement proof: FAIL"
[ $FAIL -eq 0 ] || exit 1
echo "--- perf-per-watt leg (probe_z007 parallel batches + perf stat -a RAPL) ---"
reps7_for() {
    case "$1" in
        small_churn) echo 200 ;;
        mixed_batch) echo 30 ;;
        large_batch) echo 100 ;;
        mixed_lifecycle) echo 15 ;;
    esac
}
EOUT="$HF/z114_energy_raw.txt"
: > "$EOUT"
sudo -n perf stat -a -e power/energy-pkg/ -- sleep 1 > /dev/null 2>&1 || { echo "perf stat -a energy via sudo -n unavailable - FAIL"; FAIL=1; }
EPERF="$EOUT.perf"; ECSV="$EOUT.csv"
idle_sample() {
    N=$1; W0=$(date +%s%N)
    echo "=== idle sleep sample=$N reps=0 ===" >> "$EOUT"
    : > "$EPERF"
    sudo -n perf stat -a -x '|' -e "power/energy-pkg/,power_core/energy-core/" -- sleep 3 >> "$EOUT" 2> "$EPERF"
    cat "$EPERF" >> "$EOUT"; : > "$EPERF"
    W1=$(date +%s%N); echo "idle_wall_ns=$((W1 - W0))" >> "$EOUT"
}
run_batch() {
    LEG=$1; BIN=$2; W=$3; R=$4; BLK=$5
    if [ "$LEG" = mimir_tuned70 ]; then
        sudo -n cpupower frequency-set -u $CAP_KHZ > /dev/null 2>&1 || { echo "CAP SET FAIL"; FAIL=1; }
    fi
    W0=$(date +%s%N)
    sudo -n perf stat -a -x '|' -e "power/energy-pkg/,power_core/energy-core/" -- sh tests/perf/batch_z114.sh $P "$BIN" "$W" "$R" "$HF/z114_parts" > "$ECSV" 2> "$EPERF"
    W1=$(date +%s%N)
    if [ "$LEG" = mimir_tuned70 ]; then restore_cap; fi
    echo "=== $LEG $W proc=$BLK reps=$R procs=$P wall_ns=$((W1 - W0)) ===" >> "$EOUT"
    cat "$ECSV" >> "$EOUT"; cat "$EPERF" >> "$EOUT"
}
if [ $FAIL -eq 0 ]; then
    S=1
    for W in small_churn mixed_batch large_batch mixed_lifecycle; do
        idle_sample "$S"; S=$((S + 1))
        R=$(reps7_for "$W")
        p=1
        while [ "$p" -le 5 ]; do
            run_batch mimir_untuned "$ROOT/build/probe_z114_z007_mimir" "$W" "$R" "$p"
            run_batch mimir_tuned70 "$ROOT/build/probe_z114_z007_mimir" "$W" "$R" "$p"
            run_batch glibc "$ROOT/build/probe_z114_z007_glibc" "$W" "$R" "$p"
            p=$((p + 1))
        done
        echo "loadavg after $W: $(cat /proc/loadavg)"
    done
    idle_sample "$S"
    rm -f "$EPERF" "$ECSV"
    echo "energy raw blocks: $(grep -c '^=== ' "$EOUT") (expect 65)"
    python3 tests/perf/summarize_z114.py "$EOUT" > "$HF/z114_sum.txt" 2>&1 || FAIL=1
    cat "$HF/z114_sum.txt"
fi
restore_cap
echo "scaling_max_freq after restore: $(cat /sys/devices/system/cpu/cpu2/cpufreq/scaling_max_freq) kHz (expect $ORIG_MAX)"
echo "--- load (end) ---"; uptime; cat /proc/loadavg
ps aux --sort=-%cpu | head -6
echo "Z-114 runner correctness: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL)"
echo "--- run_z114 done ---"
exit $FAIL
