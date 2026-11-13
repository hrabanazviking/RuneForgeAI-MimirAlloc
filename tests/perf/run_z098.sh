#!/bin/sh
# run_z098.sh - Z-098 fair-fight leg runner (MimirAlloc Z13).
# House perf pattern: standalone, Makefile untouched.
#
# Roadmap gate (Z-098): glibc measured under its own
# tunables (tcache size/count, mmap threshold) at their
# best for each workload - MimirAlloc faces tuned glibc,
# not only default glibc, in all final tables; tunables
# matrix recorded; the tuned-glibc column exists in the
# Z-107 closing table's raw data (the summariser emits
# that block from this run's raw data).
#
# Harness: the Wave-1 oracle tests/perf/bench_k002.c,
# byte-identical workload code for every leg. The glibc
# binary (-DLEG_GLIBC) is run under each tunable
# configuration via GLIBC_TUNABLES in the process
# environment; the MimirAlloc binary links the three
# allocator objects built with the shipped flag set
# (-O3, the Z-096 adoption) and its bench TU at -O2
# (K-004 style, identical for both binaries).
#
# Configurations (label|GLIBC_TUNABLES, "" = unset):
#   gdefault      defaults (tcache_max 1032, tcache_count 7,
#                 mmap threshold dynamic from 128 KiB)
#   gdefault_dup  identical-leg control (Z-004 protocol)
#   gtc0/1/16/64  tcache_count axis
#   gtmax512/4k/64k  tcache_max axis (bytes)
#   gmm4k/32k/1m  mmap_threshold axis (bytes, fixed)
#   gcombo_cache  tcache_count=64 + tcache_max=65536
#   gcombo_fair   gcombo_cache + mmap_threshold=1048576
#   mimir         MimirAlloc leg (tunables not applied)
#
# Tunable-engagement proof comes FIRST (probe_z098.c):
# the runner asserts the mallinfo2 mmap pattern
# (64 KiB block: heap under default and under
# mmap_threshold=1 MiB, mmap-served under
# mmap_threshold=4096) and records the tcache_count=0
# timing signature. If the pattern is absent the
# tunable channel is not live and the run FAILS before
# any matrix number is taken.
#
# Method (docs/Z13_MEASUREMENT_PROTOCOL.md, Z-004):
# every process pinned taskset -c 5; 3 warm-up rounds
# over all legs discarded; 9 measured rounds, legs
# interleaved round-robin so drift lands on all legs;
# TWO full invocations; the summariser headlines
# invocation 1 and cross-checks invocation 2 (control
# bound 5%, drift bound 8%, best-config agreement).
# Raw CSV: hidden_files/z098_raw1.txt / z098_raw2.txt;
# summaries beside them (team bookkeeping, never
# banked). Load state recorded before, between and
# after; this host is shared (Aesir forge, Lifthrasir
# forge, Tensile lab), so contention present during
# the run is named, never hidden.
#
# Exit 0 only if the engagement proof holds, every
# bench process exits 0 with identical checksums per
# workload (the summariser hard-fails otherwise), and
# the summariser passes both invocations + agreement.
set -u
cd "$(dirname "$0")/../.." || exit 2
ROOT=$PWD
HF="$ROOT/hidden_files"
mkdir -p "$HF" build
PIN=5
REPS=9
WARM=3
FAIL=0
BENCH_G="$ROOT/build/bench_z098_glibc"
BENCH_M="$ROOT/build/bench_z098_mimir"
PROBE="$ROOT/build/probe_z098"

CONFIGS="gdefault|
gdefault_dup|
gtc0|glibc.malloc.tcache_count=0
gtc1|glibc.malloc.tcache_count=1
gtc16|glibc.malloc.tcache_count=16
gtc64|glibc.malloc.tcache_count=64
gtmax512|glibc.malloc.tcache_max=512
gtmax4k|glibc.malloc.tcache_max=4096
gtmax64k|glibc.malloc.tcache_max=65536
gmm4k|glibc.malloc.mmap_threshold=4096
gmm32k|glibc.malloc.mmap_threshold=32768
gmm1m|glibc.malloc.mmap_threshold=1048576
gcombo_cache|glibc.malloc.tcache_count=64:glibc.malloc.tcache_max=65536
gcombo_fair|glibc.malloc.tcache_count=64:glibc.malloc.tcache_max=65536:glibc.malloc.mmap_threshold=1048576
mimir|"

echo "=== Z-098 The fair-fight leg (tuned glibc matrix) ==="
date -Is; uname -a; git rev-parse HEAD
cc --version | head -1
echo "--- load (start) ---"; uptime; cat /proc/loadavg
ps aux --sort=-%cpu | head -6

echo "--- build ---"
BASE="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc"
cc $BASE -O2 -DLEG_GLIBC tests/perf/bench_k002.c -o "$BENCH_G" \
    > "$HF/z098_build.log" 2>&1 || exit 1
cc $BASE -O3 -c src/api/mimir_alloc.c -o "$HF/z098_mimir_alloc.o" \
    >> "$HF/z098_build.log" 2>&1 || exit 1
cc $BASE -O3 -c src/core/mimir_core.c -o "$HF/z098_mimir_core.o" \
    >> "$HF/z098_build.log" 2>&1 || exit 1
cc $BASE -O3 -c src/platform/mimir_platform.c -o "$HF/z098_mimir_platform.o" \
    >> "$HF/z098_build.log" 2>&1 || exit 1
cc $BASE -O2 -c tests/perf/bench_k002.c -o "$HF/z098_bench.o" \
    >> "$HF/z098_build.log" 2>&1 || exit 1
cc $BASE -O3 "$HF/z098_mimir_alloc.o" "$HF/z098_mimir_core.o" \
    "$HF/z098_mimir_platform.o" "$HF/z098_bench.o" -o "$BENCH_M" \
    >> "$HF/z098_build.log" 2>&1 || exit 1
cc -std=c11 -Wall -Wextra -Werror -O2 tests/perf/probe_z098.c -o "$PROBE" \
    >> "$HF/z098_build.log" 2>&1 || exit 1
ls -l "$BENCH_G" "$BENCH_M" "$PROBE"
sha256sum tests/perf/bench_k002.c tests/perf/probe_z098.c \
    tests/perf/run_z098.sh tests/perf/summarize_z098.py \
    "$BENCH_G" "$BENCH_M"

echo "--- tunable engagement proof (probe_z098) ---"
hblk_of() { echo "$1" | sed -n 's/.*hblkhd_delta=\(-*[0-9]*\).*/\1/p'; }
OUT_DEF=$(env -u GLIBC_TUNABLES taskset -c $PIN "$PROBE") || FAIL=1
echo "default:   $OUT_DEF"
OUT_MM4K=$(GLIBC_TUNABLES="glibc.malloc.mmap_threshold=4096" \
    taskset -c $PIN "$PROBE") || FAIL=1
echo "mm4k:      $OUT_MM4K"
OUT_MM1M=$(GLIBC_TUNABLES="glibc.malloc.mmap_threshold=1048576" \
    taskset -c $PIN "$PROBE") || FAIL=1
echo "mm1m:      $OUT_MM1M"
OUT_TC0=$(GLIBC_TUNABLES="glibc.malloc.tcache_count=0" \
    taskset -c $PIN "$PROBE") || FAIL=1
echo "tc0:       $OUT_TC0"
H_DEF=$(hblk_of "$OUT_DEF"); H_4K=$(hblk_of "$OUT_MM4K"); H_1M=$(hblk_of "$OUT_MM1M")
echo "hblkhd deltas: default=$H_DEF mm4k=$H_4K mm1m=$H_1M"
[ "$H_DEF" = "0" ] || { echo "PROOF FAIL: default 64 KiB block not on heap"; FAIL=1; }
[ "$H_1M" = "0" ] || { echo "PROOF FAIL: mm1m 64 KiB block not on heap"; FAIL=1; }
[ "${H_4K:-0}" -ge 60000 ] || { echo "PROOF FAIL: mm4k 64 KiB block not mmap-served"; FAIL=1; }
[ $FAIL -eq 0 ] && echo "engagement proof: PASS (mmap_threshold channel live)" \
    || echo "engagement proof: FAIL"

run_leg() {
    # $1 = label, $2 = tunables (may be empty); appends to $3 (raw file)
    if [ "$1" = mimir ]; then
        env -u GLIBC_TUNABLES taskset -c $PIN "$BENCH_M" \
            | awk -F, -v v="$1" 'BEGIN{OFS=","} {$1=v; print}' >> "$3" || FAIL=1
    elif [ -z "$2" ]; then
        env -u GLIBC_TUNABLES taskset -c $PIN "$BENCH_G" \
            | awk -F, -v v="$1" 'BEGIN{OFS=","} {$1=v; print}' >> "$3" || FAIL=1
    else
        GLIBC_TUNABLES="$2" taskset -c $PIN "$BENCH_G" \
            | awk -F, -v v="$1" 'BEGIN{OFS=","} {$1=v; print}' >> "$3" || FAIL=1
    fi
}

matrix_invocation() {
    raw=$1
    : > "$raw"
    rep=1
    while [ $rep -le $REPS ]; do
        echo "$CONFIGS" | while IFS='|' read -r label tun; do
            [ -n "$label" ] && run_leg "$label" "$tun" "$raw"
        done
        rep=$((rep + 1))
    done
    echo "raw lines: $(wc -l < "$raw") (expect $((REPS * 15 * 4)))"
}

echo "--- warm-up rounds (discarded, all legs) ---"
w=1
while [ $w -le $WARM ]; do
    echo "$CONFIGS" | while IFS='|' read -r label tun; do
        if [ -n "$label" ]; then
            if [ "$label" = mimir ]; then
                env -u GLIBC_TUNABLES taskset -c $PIN "$BENCH_M" > /dev/null || FAIL=1
            elif [ -z "$tun" ]; then
                env -u GLIBC_TUNABLES taskset -c $PIN "$BENCH_G" > /dev/null || FAIL=1
            else
                GLIBC_TUNABLES="$tun" taskset -c $PIN "$BENCH_G" > /dev/null || FAIL=1
            fi
        fi
    done
    w=$((w + 1))
done
echo "warm-ups done"
echo "loadavg before invocation 1: $(cat /proc/loadavg)"

echo "--- invocation 1 ---"
matrix_invocation "$HF/z098_raw1.txt"
echo "loadavg mid: $(cat /proc/loadavg)"
ps aux --sort=-%cpu | head -4

echo "--- invocation 2 ---"
matrix_invocation "$HF/z098_raw2.txt"
python3 tests/perf/summarize_z098.py "$HF/z098_raw1.txt" "$HF/z098_raw2.txt" $REPS \
    > "$HF/z098_sum.txt" 2>&1 || FAIL=1
cat "$HF/z098_sum.txt"
echo "--- load (end) ---"; uptime; cat /proc/loadavg

echo "Z-098 runner correctness: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL)"
echo "--- run_z098 done ---"
exit $FAIL
