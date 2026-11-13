#!/bin/sh
# run_z107.sh - Z-107 The closing head-to-head runner (MimirAlloc Z13).
# House perf pattern: standalone, Makefile untouched.
#
# Roadmap gate (Z-107): the complete Wave-1 oracle (latency,
# counters, RSS, energy) re-run on the final allocator, against
# Z-009's baseline and BOTH glibc columns (default and Z-098
# tuned). Gate: the closing table filed as recorded runs +
# summary (tests/perf/recorded_run_z13_z107.txt, assembled by
# this runner from the four leg summaries).
#
# Legs (every leg runs the banked tests/perf/bench_k002.c
# workloads, byte-identical workload code; checksums must
# equal the Z-003 per-pass checksums x reps in every block -
# the summariser hard-fails otherwise):
#   latency   bench_k002 binaries under run-order configs:
#             gdefault, gdefault_dup (identical-leg control),
#             gmm32k, gtmax64k, gcombo_cache (the Z-098 winning
#             configurations; the tuned column per workload is
#             FIXED by Z-098 invocation 1: small_churn=gmm32k,
#             mixed_batch=gtmax64k, large_batch=gcombo_cache,
#             mixed_lifecycle=gcombo_cache), mimir.
#             Method per docs/Z13_MEASUREMENT_PROTOCOL.md
#             (Z-004) and run_z098.sh: taskset -c 5, 3 warm-up
#             rounds discarded, 9 measured rounds interleaved
#             round-robin, TWO invocations; control bound 5%,
#             cross-invocation drift bound 8%.
#   counters  probe_z005.c under sudo -n perf stat (cycles,
#             instructions, cache-misses, branch-misses),
#             legs glibc / glibc_tuned (per-workload Z-098
#             winner via GLIBC_TUNABLES) / mimiralloc, 5 procs
#             per leg per workload, taskset -c 2, reps per
#             workload as run_perf_z005.sh.
#   rss       probe_z006.c driven by run_rss_z107.py (the
#             Z-006 smaps-rollup phase protocol), same 3 legs,
#             5 procs per leg per workload, taskset -c 2.
#   energy    probe_z007.c under sudo -n perf stat -a (RAPL
#             power/energy-pkg/), same 3 legs, 5 procs per leg
#             per workload, idle samples interleaved, reps per
#             workload as run_energy_z007.sh, taskset -c 2.
#
# The MimirAlloc legs link the allocator objects built with
# the shipped flag set (-O3, the Z-096 adoption, carried in
# the Makefile CFLAGS / LIBOBJS); bench/probe TUs at -O2
# (K-004 style, identical for every binary).
#
# Tunable engagement is proven FIRST (probe_z098 pattern,
# as run_z098.sh): if the pattern is absent the tunable
# channel is not live and the run FAILS before any number
# is taken.
#
# Raw data: hidden_files/z107_lat_raw1.txt / _raw2.txt,
# z107_counters_raw.txt, z107_rss.raw.csv (+ .header),
# z107_energy_raw.txt (team bookkeeping, never banked).
# Summaries: hidden_files/z107_sum_*.txt. Load state is
# recorded before, between and after the legs; this host is
# shared, so contention present is named, never hidden.
#
# Exit 0 only if the engagement proof holds, every leg's
# summariser passes (checksums, control, drift), and the
# recorded run is assembled.
set -u
cd "$(dirname "$0")/../.." || exit 2
ROOT=$PWD
HF="$ROOT/hidden_files"
mkdir -p "$HF" build
PIN_LAT=5
PIN=2
FAIL=0

echo "=== Z-107 The closing head-to-head ==="
date -Is; uname -a; git rev-parse HEAD; git branch --show-current
cc --version | head -1
echo "--- load (start) ---"; uptime; cat /proc/loadavg
ps aux --sort=-%cpu | head -6
echo "--- host truth ---"
scripts/z13/host_truth.sh | tee "$HF/z107_host_truth.txt"

echo "--- build ---"
BASE="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc"
make bench-k002 > "$HF/z107_build.log" 2>&1 || { echo BUILD FAIL make bench-k002; exit 1; }
# latency binaries (run_z098.sh build shape: allocator -O3, bench TU -O2)
cc $BASE -O2 -DLEG_GLIBC tests/perf/bench_k002.c -o "$ROOT/build/bench_z107_glibc" >> "$HF/z107_build.log" 2>&1 || exit 1
cc $BASE -O3 -c src/api/mimir_alloc.c -o "$HF/z107_mimir_alloc.o" >> "$HF/z107_build.log" 2>&1 || exit 1
cc $BASE -O3 -c src/core/mimir_core.c -o "$HF/z107_mimir_core.o" >> "$HF/z107_build.log" 2>&1 || exit 1
cc $BASE -O3 -c src/platform/mimir_platform.c -o "$HF/z107_mimir_platform.o" >> "$HF/z107_build.log" 2>&1 || exit 1
cc $BASE -O2 -c tests/perf/bench_k002.c -o "$HF/z107_bench.o" >> "$HF/z107_build.log" 2>&1 || exit 1
cc $BASE -O3 "$HF/z107_mimir_alloc.o" "$HF/z107_mimir_core.o" "$HF/z107_mimir_platform.o" "$HF/z107_bench.o" -o "$ROOT/build/bench_z107_mimir" >> "$HF/z107_build.log" 2>&1 || exit 1
# counter / rss / energy probes (Z-005/Z-006/Z-007 build shape, production objects)
for P in z005 z006 z007; do
    cc $BASE -O2 -DLEG_GLIBC tests/perf/probe_$P.c -o "$ROOT/build/probe_z107_${P}_glibc" >> "$HF/z107_build.log" 2>&1 || exit 1
    cc $BASE -O2 tests/perf/probe_$P.c build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o -o "$ROOT/build/probe_z107_${P}_mimir" >> "$HF/z107_build.log" 2>&1 || exit 1
done
cc -std=c11 -Wall -Wextra -Werror -O2 tests/perf/probe_z098.c -o "$ROOT/build/probe_z107_z098" >> "$HF/z107_build.log" 2>&1 || exit 1
sha256sum tests/perf/bench_k002.c tests/perf/run_z107.sh tests/perf/summarize_z107.py tests/perf/run_rss_z107.py "$ROOT/build/bench_z107_glibc" "$ROOT/build/bench_z107_mimir" | tee "$HF/z107_sha.txt"

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

# ---------------- latency leg ----------------
echo "--- latency leg (bench_k002, Z-004/Z-098 method) ---"
BENCH_G="$ROOT/build/bench_z107_glibc"
BENCH_M="$ROOT/build/bench_z107_mimir"
CONFIGS="gdefault|
gdefault_dup|
gmm32k|glibc.malloc.mmap_threshold=32768
gtmax64k|glibc.malloc.tcache_max=65536
gcombo_cache|glibc.malloc.tcache_count=64:glibc.malloc.tcache_max=65536
mimir|"
REPS=9
WARM=3
run_leg() {
    if [ "$1" = mimir ]; then
        env -u GLIBC_TUNABLES taskset -c $PIN_LAT "$BENCH_M" | awk -F, -v v="$1" 'BEGIN{OFS=","} {$1=v; print}' >> "$3" || FAIL=1
    elif [ -z "$2" ]; then
        env -u GLIBC_TUNABLES taskset -c $PIN_LAT "$BENCH_G" | awk -F, -v v="$1" 'BEGIN{OFS=","} {$1=v; print}' >> "$3" || FAIL=1
    else
        GLIBC_TUNABLES="$2" taskset -c $PIN_LAT "$BENCH_G" | awk -F, -v v="$1" 'BEGIN{OFS=","} {$1=v; print}' >> "$3" || FAIL=1
    fi
}
one_round() {
    raw=$1
    echo "$CONFIGS" | while IFS='|' read -r label tun; do
        [ -n "$label" ] && run_leg "$label" "$tun" "$raw"
    done
}
w=1
while [ $w -le $WARM ]; do
    echo "$CONFIGS" | while IFS='|' read -r label tun; do
        if [ -n "$label" ]; then
            if [ "$label" = mimir ]; then
                env -u GLIBC_TUNABLES taskset -c $PIN_LAT "$BENCH_M" > /dev/null || FAIL=1
            elif [ -z "$tun" ]; then
                env -u GLIBC_TUNABLES taskset -c $PIN_LAT "$BENCH_G" > /dev/null || FAIL=1
            else
                GLIBC_TUNABLES="$tun" taskset -c $PIN_LAT "$BENCH_G" > /dev/null || FAIL=1
            fi
        fi
    done
    w=$((w + 1))
done
echo "warm-ups done; loadavg: $(cat /proc/loadavg)"
: > "$HF/z107_lat_raw1.txt"
rep=1; while [ $rep -le $REPS ]; do one_round "$HF/z107_lat_raw1.txt"; rep=$((rep + 1)); done
echo "invocation 1 raw lines: $(wc -l < "$HF/z107_lat_raw1.txt") (expect $((REPS * 6 * 4)))"
echo "loadavg mid-latency: $(cat /proc/loadavg)"; ps aux --sort=-%cpu | head -4
: > "$HF/z107_lat_raw2.txt"
rep=1; while [ $rep -le $REPS ]; do one_round "$HF/z107_lat_raw2.txt"; rep=$((rep + 1)); done
echo "invocation 2 raw lines: $(wc -l < "$HF/z107_lat_raw2.txt") (expect $((REPS * 6 * 4)))"
python3 tests/perf/summarize_z107.py latency "$HF/z107_lat_raw1.txt" "$HF/z107_lat_raw2.txt" $REPS > "$HF/z107_sum_latency.txt" 2>&1 || FAIL=1
cat "$HF/z107_sum_latency.txt"
echo "loadavg after latency: $(cat /proc/loadavg)"

# ---------------- counters leg ----------------
echo "--- counters leg (probe_z005 + perf stat) ---"
sudo -n perf stat -e cycles -- true > /dev/null 2>&1 || { echo "perf stat via sudo -n unavailable - counters FAIL"; FAIL=1; }
tuned_for() {
    case "$1" in
        small_churn) echo "glibc.malloc.mmap_threshold=32768" ;;
        mixed_batch) echo "glibc.malloc.tcache_max=65536" ;;
        large_batch) echo "glibc.malloc.tcache_count=64:glibc.malloc.tcache_max=65536" ;;
        mixed_lifecycle) echo "glibc.malloc.tcache_count=64:glibc.malloc.tcache_max=65536" ;;
    esac
}
reps5_for() {
    case "$1" in
        small_churn) echo 20 ;;
        mixed_batch) echo 5 ;;
        large_batch) echo 20 ;;
        mixed_lifecycle) echo 10 ;;
    esac
}
COUT="$HF/z107_counters_raw.txt"
: > "$COUT"
if [ $FAIL -eq 0 ]; then
    for W in small_churn mixed_batch large_batch mixed_lifecycle; do
        R=$(reps5_for "$W"); TUN=$(tuned_for "$W")
        p=1
        while [ "$p" -le 5 ]; do
            for LEG in glibc glibc_tuned mimiralloc; do
                echo "=== $LEG $W proc=$p reps=$R ===" >> "$COUT"
                if [ "$LEG" = glibc ]; then
                    sudo -n perf stat -x '|' -e "cycles,instructions,cache-misses,branch-misses" -- taskset -c $PIN env -u GLIBC_TUNABLES "$ROOT/build/probe_z107_z005_glibc" "$W" "$R" >> "$COUT" 2>> "$COUT.perf"
                elif [ "$LEG" = glibc_tuned ]; then
                    sudo -n perf stat -x '|' -e "cycles,instructions,cache-misses,branch-misses" -- taskset -c $PIN env GLIBC_TUNABLES="$TUN" "$ROOT/build/probe_z107_z005_glibc" "$W" "$R" >> "$COUT" 2>> "$COUT.perf"
                else
                    sudo -n perf stat -x '|' -e "cycles,instructions,cache-misses,branch-misses" -- taskset -c $PIN env -u GLIBC_TUNABLES "$ROOT/build/probe_z107_z005_mimir" "$W" "$R" >> "$COUT" 2>> "$COUT.perf"
                fi
                cat "$COUT.perf" >> "$COUT"; : > "$COUT.perf"
            done
            p=$((p + 1))
        done
    done
    rm -f "$COUT.perf"
    echo "counter raw blocks: $(grep -c '^=== ' "$COUT") (expect 60)"
    python3 tests/perf/summarize_z107.py counters "$COUT" > "$HF/z107_sum_counters.txt" 2>&1 || FAIL=1
    cat "$HF/z107_sum_counters.txt"
fi
echo "loadavg after counters: $(cat /proc/loadavg)"

# ---------------- RSS leg ----------------
echo "--- RSS leg (probe_z006 + run_rss_z107.py) ---"
if [ $FAIL -eq 0 ]; then
    Z107_RSS_GBIN="$ROOT/build/probe_z107_z006_glibc" Z107_RSS_MBIN="$ROOT/build/probe_z107_z006_mimir" \
        python3 tests/perf/run_rss_z107.py "$HF/z107_rss" 5 > "$HF/z107_rss_driver.log" 2>&1 || FAIL=1
    cat "$HF/z107_rss_driver.log"
    python3 tests/perf/summarize_z107.py rss "$HF/z107_rss.raw.csv" > "$HF/z107_sum_rss.txt" 2>&1 || FAIL=1
    cat "$HF/z107_sum_rss.txt"
fi
echo "loadavg after RSS: $(cat /proc/loadavg)"

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
EOUT="$HF/z107_energy_raw.txt"
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
    python3 tests/perf/summarize_z107.py energy "$EOUT" > "$HF/z107_sum_energy.txt" 2>&1 || FAIL=1
    cat "$HF/z107_sum_energy.txt"
fi
echo "--- load (end) ---"; uptime; cat /proc/loadavg
ps aux --sort=-%cpu | head -6

echo "Z-107 runner correctness: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL)"
echo "--- run_z107 done ---"
exit $FAIL
