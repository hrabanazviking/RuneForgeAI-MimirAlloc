#!/bin/sh
# run_z110.sh - Z-110 Quiet-window energy re-run (MimirAlloc Z13).
# DERIVED on-device from the banked tests/perf/run_z107.sh: the
# build, tunable engagement proof and energy leg below are the
# Z-107 lines verbatim (only hidden_files output names re-pointed
# z107->z110); the latency/counters/RSS legs are removed. Grading
# is by the BANKED tests/perf/summarize_z107.py energy mode,
# unmodified: median net nJ/op after idle subtraction, WIN <0.95x,
# TIE 0.95-1.05x, LOSS >1.05x; reference below idle-subtraction
# resolution claims no ratio. Control bounds NEVER weakened.
# Slice: WAVE_PLAN Wave 13 pool choice - re-run the Z-107 energy
# leg in a VERIFIED quiet window (Z-108 verdict S5.2: Z-107 ran in
# an Aesir co-tenant window), grades re-filed vs BOTH glibc columns.
# QUIET GATE (inserted before the energy leg): top second-sample
# CPU idle >= 90% and no heavy co-tenant (mojo, llama-server,
# ollama, probe_z*, bench_*, run_all, make test, run_gate) live;
# on failure the runner exits 3 and measures NOTHING.
set -u
cd "$(dirname "$0")/../.." || exit 2
ROOT=$PWD
HF="$ROOT/hidden_files"
mkdir -p "$HF" build
PIN_LAT=5
PIN=2
FAIL=0

echo "=== Z-110 Quiet-window energy re-run ==="
date -Is; uname -a; git rev-parse HEAD; git branch --show-current
cc --version | head -1
echo "--- load (start) ---"; uptime; cat /proc/loadavg
ps aux --sort=-%cpu | head -6
echo "--- host truth ---"
scripts/z13/host_truth.sh | tee "$HF/z110_host_truth.txt"

echo "--- build ---"
BASE="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc"
make bench-k002 > "$HF/z110_build.log" 2>&1 || { echo BUILD FAIL make bench-k002; exit 1; }
# latency binaries (run_z098.sh build shape: allocator -O3, bench TU -O2)
cc $BASE -O2 -DLEG_GLIBC tests/perf/bench_k002.c -o "$ROOT/build/bench_z107_glibc" >> "$HF/z110_build.log" 2>&1 || exit 1
cc $BASE -O3 -c src/api/mimir_alloc.c -o "$HF/z107_mimir_alloc.o" >> "$HF/z110_build.log" 2>&1 || exit 1
cc $BASE -O3 -c src/core/mimir_core.c -o "$HF/z107_mimir_core.o" >> "$HF/z110_build.log" 2>&1 || exit 1
cc $BASE -O3 -c src/platform/mimir_platform.c -o "$HF/z107_mimir_platform.o" >> "$HF/z110_build.log" 2>&1 || exit 1
cc $BASE -O2 -c tests/perf/bench_k002.c -o "$HF/z107_bench.o" >> "$HF/z110_build.log" 2>&1 || exit 1
cc $BASE -O3 "$HF/z107_mimir_alloc.o" "$HF/z107_mimir_core.o" "$HF/z107_mimir_platform.o" "$HF/z107_bench.o" -o "$ROOT/build/bench_z107_mimir" >> "$HF/z110_build.log" 2>&1 || exit 1
# counter / rss / energy probes (Z-005/Z-006/Z-007 build shape, production objects)
for P in z005 z006 z007; do
    cc $BASE -O2 -DLEG_GLIBC tests/perf/probe_$P.c -o "$ROOT/build/probe_z107_${P}_glibc" >> "$HF/z110_build.log" 2>&1 || exit 1
    cc $BASE -O2 tests/perf/probe_$P.c build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o -o "$ROOT/build/probe_z107_${P}_mimir" >> "$HF/z110_build.log" 2>&1 || exit 1
done
cc -std=c11 -Wall -Wextra -Werror -O2 tests/perf/probe_z098.c -o "$ROOT/build/probe_z107_z098" >> "$HF/z110_build.log" 2>&1 || exit 1
sha256sum tests/perf/bench_k002.c tests/perf/run_z107.sh tests/perf/summarize_z107.py tests/perf/run_rss_z107.py "$ROOT/build/bench_z107_glibc" "$ROOT/build/bench_z107_mimir" | tee "$HF/z110_sha.txt"

echo "--- tunable engagement proof (probe_z098) ---"
PROBE="$ROOT/build/probe_z107_z098"
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


tuned_for() {
    case "$1" in
        small_churn) echo "glibc.malloc.mmap_threshold=32768" ;;
        mixed_batch) echo "glibc.malloc.tcache_max=65536" ;;
        large_batch) echo "glibc.malloc.tcache_count=64:glibc.malloc.tcache_max=65536" ;;
        mixed_lifecycle) echo "glibc.malloc.tcache_count=64:glibc.malloc.tcache_max=65536" ;;
    esac
}

echo "--- quiet-window verification (gate before energy leg) ---"
IDLE=$(top -bn2 -d 1 | grep Cpu | tail -1 | sed -n 's/.* \([0-9.]*\) id.*/\1/p')
echo "cpu_idle_pct(second top sample): $IDLE"
ps aux > "$HF/z110_ps_check.txt"
HEAVY=$(grep -E 'mojo|llama-server|probe_z|probe_mimir|probe_glibc|bench_z|run_all|run_gate|make test' "$HF/z110_ps_check.txt" | grep -v grep | grep -v ollama | grep -v run_z110 || true)
echo "heavy co-tenants detected:"; echo "${HEAVY:-none}"
ps aux --sort=-%cpu | head -6
QUIET=1
python3 -c "import sys; sys.exit(0 if float('${IDLE:-0}') >= 90.0 else 1)" || QUIET=0
[ -z "$HEAVY" ] || QUIET=0
if [ $QUIET -ne 1 ]; then
    echo "QUIET CHECK: FAIL - window is not quiet; no measurement taken"
    echo "Z-110 runner correctness: BLOCKED-CONTENTION"
    exit 3
fi
echo "QUIET CHECK: PASS (idle >= 90%, no heavy co-tenant live)"

# ---------------- energy leg ----------------
echo "--- energy leg (probe_z007 + perf stat -a RAPL) ---"
reps7_for() {
    case "$1" in
        small_churn) echo 200 ;;
        mixed_batch) echo 30 ;;
        large_batch) echo 100 ;;
        mixed_lifecycle) echo 15 ;;
    esac
}
EOUT="$HF/z110_energy_raw.txt"
: > "$EOUT"
if [ $FAIL -eq 0 ]; then
    sudo -n perf stat -a -e power/energy-pkg/ -- sleep 1 > /dev/null 2>&1 || { echo "perf stat -a energy via sudo -n unavailable - energy FAIL"; FAIL=1; }
fi
if [ $FAIL -eq 0 ]; then
    EPERF="$EOUT.perf"; EPROBE="$EOUT.probe"
    idle_sample() {
        N=$1
        W0=$(date +%s%N)
        echo "=== idle sleep sample=$N reps=0 ===" >> "$EOUT"
        : > "$EPERF"
        sudo -n perf stat -a -x '|' -e "power/energy-pkg/,power_core/energy-core/" -- sleep 3 >> "$EOUT" 2> "$EPERF"
        cat "$EPERF" >> "$EOUT"; : > "$EPERF"
        W1=$(date +%s%N)
        echo "idle_wall_ns=$((W1 - W0))" >> "$EOUT"
    }
    S=1
    for W in small_churn mixed_batch large_batch mixed_lifecycle; do
        idle_sample "$S"; S=$((S + 1))
        R=$(reps7_for "$W"); TUN=$(tuned_for "$W")
        p=1
        while [ "$p" -le 5 ]; do
            for LEG in glibc glibc_tuned mimiralloc; do
                W0=$(date +%s%N)
                if [ "$LEG" = glibc ]; then
                    sudo -n perf stat -a -x '|' -e "power/energy-pkg/,power_core/energy-core/" -- taskset -c $PIN env -u GLIBC_TUNABLES "$ROOT/build/probe_z107_z007_glibc" "$W" "$R" > "$EPROBE" 2> "$EPERF"
                elif [ "$LEG" = glibc_tuned ]; then
                    sudo -n perf stat -a -x '|' -e "power/energy-pkg/,power_core/energy-core/" -- taskset -c $PIN env GLIBC_TUNABLES="$TUN" "$ROOT/build/probe_z107_z007_glibc" "$W" "$R" > "$EPROBE" 2> "$EPERF"
                else
                    sudo -n perf stat -a -x '|' -e "power/energy-pkg/,power_core/energy-core/" -- taskset -c $PIN env -u GLIBC_TUNABLES "$ROOT/build/probe_z107_z007_mimir" "$W" "$R" > "$EPROBE" 2> "$EPERF"
                fi
                W1=$(date +%s%N)
                echo "=== $LEG $W proc=$p reps=$R wall_ns=$((W1 - W0)) ===" >> "$EOUT"
                cat "$EPROBE" >> "$EOUT"; cat "$EPERF" >> "$EOUT"
            done
            p=$((p + 1))
        done
    done
    idle_sample "$S"
    rm -f "$EPERF" "$EPROBE"
    echo "energy raw blocks: $(grep -c '^=== ' "$EOUT") (expect 65)"
    python3 tests/perf/summarize_z107.py energy "$EOUT" > "$HF/z110_sum_energy.txt" 2>&1 || FAIL=1
    cat "$HF/z110_sum_energy.txt"
fi
echo "--- load (end) ---"; uptime; cat /proc/loadavg
ps aux --sort=-%cpu | head -6

echo "Z-110 runner correctness: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL)"
echo "--- run_z110 done ---"
exit $FAIL
