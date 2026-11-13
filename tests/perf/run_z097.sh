#!/bin/sh
# run_z097.sh - Z-097 short-lived process economics runner
# (MimirAlloc Z13). House perf pattern: standalone,
# Makefile untouched.
#
# Builds the Z-082 interposing library, the payload
# (tests/perf/child_z097.c, an ordinary glibc consumer)
# and the storm driver (tests/perf/probe_z097.c), proves
# the two allocator worlds apart with the payload's
# verdict mode (the Z-082 two-probe check: plain must
# print GLIBC, preloaded must print MIMIR), sanity-runs
# every timed mode in both worlds, then runs the
# fork+exec storm:
#   shapes   none / first / multi / churn (child_z097.c)
#   worlds   plain (glibc) vs preload (LD_PRELOAD of
#            build/libmimiralloc_interpose.so)
#   counts   200 children per storm (100 for churn)
#   reps     7 storms per (world, shape), worlds
#            interleaved rep by rep (Z-004 protocol),
#            TWO full invocations; the driver is pinned
#            with taskset -c 5 and the children inherit
#            the pin.
# Warm-up: 2 discarded storms per (world, shape) before
# the first invocation. Raw CSV lands in
# hidden_files/z097_raw1.txt / z097_raw2.txt, summaries
# in hidden_files/z097_sum1.txt / z097_sum2.txt (team
# bookkeeping, never banked); the recorded run is
# assembled separately. Load state is recorded before,
# between and after; this host is shared (Aesir forge,
# Lifthrasir forge, Tensile lab), so contention present
# during the run is named in the output, never hidden.
#
# Exit 0 only if the verdict pair is coherent, every
# sanity run and storm child exits 0, and both
# summariser invocations pass.
set -u
cd "$(dirname "$0")/../.." || exit 2
ROOT=$PWD
HF="$ROOT/hidden_files"
mkdir -p "$HF" build
PIN=5
REPS=7
FAIL=0
IPOLIB="$ROOT/build/libmimiralloc_interpose.so"
CHILD="$ROOT/build/child_z097"
PROBE="$ROOT/build/probe_z097"

echo "=== Z-097 Short-lived process economics ==="
date -Is; uname -a; git rev-parse HEAD
cc --version | head -1
echo "--- load (start) ---"; uptime; cat /proc/loadavg
ps aux --sort=-%cpu | head -6

echo "--- build ---"
make lib-interpose > "$HF/z097_build.log" 2>&1 || exit 1
cc -std=c11 -Wall -Wextra -Werror -O2 -fno-builtin \
    tests/perf/child_z097.c -o "$CHILD" >> "$HF/z097_build.log" 2>&1 || exit 1
cc -std=c11 -Wall -Wextra -Werror -O2 \
    tests/perf/probe_z097.c -o "$PROBE" >> "$HF/z097_build.log" 2>&1 || exit 1
ls -l build/libmimiralloc_interpose.so "$CHILD" "$PROBE"
sha256sum tests/perf/child_z097.c tests/perf/probe_z097.c \
    tests/perf/run_z097.sh tests/perf/summarize_z097.py \
    build/libmimiralloc_interpose.so

count_for() {
    if [ "$1" = churn ]; then echo 100; else echo 200; fi
}

echo "--- verdict pair (Z-082 two-probe world check) ---"
OUT_PLAIN=$(taskset -c $PIN "$CHILD" verdict)
echo "plain verdict: $OUT_PLAIN"
[ "$OUT_PLAIN" = "PRELOAD-VERDICT: GLIBC" ] || FAIL=1
OUT_PRE=$(LD_PRELOAD="$IPOLIB" taskset -c $PIN "$CHILD" verdict)
echo "preload verdict: $OUT_PRE"
[ "$OUT_PRE" = "PRELOAD-VERDICT: MIMIR" ] || FAIL=1

echo "--- sanity: every timed mode, both worlds ---"
for mode in none first multi churn; do
    taskset -c $PIN "$CHILD" "$mode" || FAIL=1
    LD_PRELOAD="$IPOLIB" taskset -c $PIN "$CHILD" "$mode" || FAIL=1
    echo "sanity $mode: plain+preload exit 0 expected"
done

storm_invocation() {
    raw=$1
    : > "$raw"
    rep=1
    while [ $rep -le $REPS ]; do
        for mode in none first multi churn; do
            n=$(count_for "$mode")
            taskset -c $PIN "$PROBE" "$CHILD" "$mode" plain \
                "$n" - >> "$raw" || FAIL=1
            taskset -c $PIN "$PROBE" "$CHILD" "$mode" preload \
                "$n" "$IPOLIB" >> "$raw" || FAIL=1
        done
        rep=$((rep + 1))
    done
    echo "raw lines: $(wc -l < "$raw") (expect $((REPS * 8)))"
}

echo "--- warm-up storms (discarded) ---"
for mode in none first multi churn; do
    n=$(count_for "$mode")
    taskset -c $PIN "$PROBE" "$CHILD" "$mode" plain "$n" - > /dev/null || FAIL=1
    taskset -c $PIN "$PROBE" "$CHILD" "$mode" preload "$n" "$IPOLIB" > /dev/null || FAIL=1
    taskset -c $PIN "$PROBE" "$CHILD" "$mode" plain "$n" - > /dev/null || FAIL=1
    taskset -c $PIN "$PROBE" "$CHILD" "$mode" preload "$n" "$IPOLIB" > /dev/null || FAIL=1
done
echo "warm-ups done"
echo "loadavg before invocation 1: $(cat /proc/loadavg)"

echo "--- invocation 1 ---"
storm_invocation "$HF/z097_raw1.txt"
python3 tests/perf/summarize_z097.py "$HF/z097_raw1.txt" \
    | tee "$HF/z097_sum1.txt" || FAIL=1
echo "loadavg mid: $(cat /proc/loadavg)"

echo "--- invocation 2 ---"
storm_invocation "$HF/z097_raw2.txt"
python3 tests/perf/summarize_z097.py "$HF/z097_raw2.txt" \
    | tee "$HF/z097_sum2.txt" || FAIL=1
echo "--- load (end) ---"; uptime; cat /proc/loadavg

echo "Z-097 runner correctness: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL)"
echo "--- run_z097 done ---"
exit $FAIL
