#!/bin/sh
# run_z116.sh - Z-116 energy-proportional idle behaviour.
# Builds probe_z116 (mimir + glibc legs), measures the
# pre-load package idle power (Z-007 idle-subtraction
# method: sudo -n perf stat -a, RAPL power/energy-pkg/),
# runs the mimir probe (load -> free-all -> 120 s idle
# window sampled every 10 s -> explicit trim -> post-trim
# samples) while two 10 s package-power samples are taken
# inside its retained-idle window, then a post sample,
# then the glibc leg (retention shape only), then a final
# package sample. Load state and top processes are
# recorded at every stage; NO quiet-window claim is made
# unless the recorded snapshots support it.
# Run detached (setsid/nohup) per the shared-machine law;
# total wall about 6 minutes.
set -u
cd "$(dirname "$0")/../.." || exit 2
ROOT=$PWD
HF="$ROOT/hidden_files"
mkdir -p "$HF" build
FAIL=0
echo "=== Z-116 Energy-proportional idle behaviour ==="
date -Is; uname -a; git rev-parse HEAD; git branch --show-current
cc --version | head -1
echo "--- load (start) ---"; uptime; cat /proc/loadavg
ps aux --sort=-%cpu | head -6
echo "--- host truth ---"
scripts/z13/host_truth.sh | tee "$HF/z116_host_truth.txt"
echo "--- build ---"
BASE="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc"
make bench-k002 > "$HF/z116_build.log" 2>&1 || { echo BUILD FAIL; exit 1; }
cc $BASE -O2 tests/perf/probe_z116.c build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o -o "$ROOT/build/probe_z116_mimir" >> "$HF/z116_build.log" 2>&1 || { echo BUILD FAIL mimir probe; exit 1; }
cc $BASE -O2 -DLEG_GLIBC tests/perf/probe_z116.c -o "$ROOT/build/probe_z116_glibc" >> "$HF/z116_build.log" 2>&1 || { echo BUILD FAIL glibc probe; exit 1; }
sha256sum tests/perf/probe_z116.c tests/perf/run_z116.sh tests/perf/summarize_z116.py "$ROOT/build/probe_z116_mimir" "$ROOT/build/probe_z116_glibc" | tee "$HF/z116_sha.txt"
sudo -n perf stat -a -e power/energy-pkg/ -- sleep 1 > /dev/null 2>&1 || { echo "perf stat -a energy via sudo -n unavailable - FAIL"; exit 1; }
PWR="$HF/z116_power_raw.txt"
: > "$PWR"
power_sample() {
    NAME=$1; DUR=$2
    W0=$(date +%s%N)
    echo "=== power $NAME ===" >> "$PWR"
    sudo -n perf stat -a -x '|' -e "power/energy-pkg/,power_core/energy-core/" -- sleep $DUR >> "$PWR" 2>&1
    W1=$(date +%s%N)
    echo "power_wall_s=$(( (W1 - W0) / 1000000000 ))" >> "$PWR"
    echo "loadavg at $NAME: $(cat /proc/loadavg)" >> "$PWR"
    ps aux --sort=-%cpu | head -4 >> "$PWR"
}
echo "--- power pre (host idle, before any probe) ---"
power_sample pre 10
echo "--- mimir leg (background probe, power sampled in its idle window) ---"
"$ROOT/build/probe_z116_mimir" 120 10 > "$HF/z116_mimir.raw" 2> "$HF/z116_mimir.err" &
MPID=$!
# wait for the drained sample (idle window start), max 60 s
N=0
while [ $N -lt 60 ]; do
    grep -q "SAMPLE,mimir,drained" "$HF/z116_mimir.raw" 2>/dev/null && break
    kill -0 $MPID 2>/dev/null || break
    sleep 1; N=$((N + 1))
done
echo "drained seen after ${N}s; loadavg: $(cat /proc/loadavg)"
sleep 5
power_sample during1 10
sleep 40
power_sample during2 10
wait $MPID; MEXIT=$?
echo "mimir probe exit=$MEXIT"
cat "$HF/z116_mimir.err"
echo "--- power post (after mimir trim + exit) ---"
power_sample post 10
echo "--- glibc leg ---"
"$ROOT/build/probe_z116_glibc" 120 10 > "$HF/z116_glibc.raw" 2> "$HF/z116_glibc.err"
GEXIT=$?
echo "glibc probe exit=$GEXIT"
cat "$HF/z116_glibc.err"
power_sample post_glibc 10
echo "--- summarise ---"
python3 tests/perf/summarize_z116.py "$HF/z116_mimir.raw" "$HF/z116_glibc.raw" "$PWR" | tee "$HF/z116_sum.txt" || FAIL=1
[ $MEXIT -eq 0 ] || FAIL=1
[ $GEXIT -eq 0 ] || FAIL=1
echo "--- load (end) ---"; uptime; cat /proc/loadavg
ps aux --sort=-%cpu | head -6
echo "Z-116 runner correctness: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL)"
echo "--- run_z116 done ---"
exit $FAIL
