#!/bin/sh
# run_z113.sh - Z-113 GPU-shared UMA pool behaviour runner.
# Runs the Z-113 probe (banked bench_k002 workloads, one
# workload per process, legs interleaved, taskset -c 2) in
# two phases on this host:
#   baseline - no GPU co-tenant (desktop idle; ollama model
#              unloaded before the phase starts)
#   gpuload  - a GPU co-tenant sharing the UMA: ollama
#              qwen2.5:7b-instruct-q4_K_M generation looped
#              on the integrated Radeon (Strix Halo iGPU).
#              Engagement is PROVEN, not assumed: the phase
#              starts only after gpu_busy_percent >= 50 is
#              sampled, samples are taken throughout, and
#              the model is unloaded afterwards.
# If the GPU load cannot be engaged the runner writes
# GPU_LOAD_FAILED into the raw file and exits 3, so the
# slice records the limitation instead of inventing a load.
# Usage: run_z113.sh <raw-output-file> [procs]
set -u
OUT=${1:?usage: run_z113.sh <raw-output-file> [procs]}
PROCS=${2:-5}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
GBIN="$ROOT/build/probe_z113_glibc"
MBIN="$ROOT/build/probe_z113_mimir"
GPUDEV=/sys/class/drm/card1/device/gpu_busy_percent
MODEL=qwen2.5:7b-instruct-q4_K_M
FLAG=/tmp/z113_gpu_load_on
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
    gcc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 -DLEG_GLIBC tests/perf/probe_z113.c -o "$GBIN"
    gcc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 tests/perf/probe_z113.c build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o -o "$MBIN"
fi
battery() {
    for W in small_churn mixed_batch large_batch mixed_lifecycle; do
        R=$(reps_for "$W")
        p=1
        while [ "$p" -le "$PROCS" ]; do
            for LEG in glibc mimiralloc; do
                if [ "$LEG" = glibc ]; then BIN="$GBIN"; else BIN="$MBIN"; fi
                taskset -c 2 "$BIN" "$W" "$R" >> "$OUT" 2>> "$OUT.err"
            done
            p=$((p + 1))
        done
    done
}
memavail() { awk "/MemAvailable/ {print \$2}" /proc/meminfo; }
: > "$OUT"; : > "$OUT.err"
echo "host: $(uname -a)" >> "$OUT"
echo "load_at_start: $(cat /proc/loadavg)" >> "$OUT"
curl -s http://127.0.0.1:11434/api/generate -d "{\"model\":\"$MODEL\",\"prompt\":\"x\",\"stream\":false,\"keep_alive\":0,\"options\":{\"num_predict\":1}}" > /dev/null 2>&1
sleep 3
echo "gpu_busy_before_baseline: $(cat $GPUDEV)" >> "$OUT"
echo "memavail_kb_before_baseline: $(memavail)" >> "$OUT"
echo "PHASE baseline" >> "$OUT"
battery
echo "gpu_busy_after_baseline: $(cat $GPUDEV)" >> "$OUT"
echo "load_after_baseline: $(cat /proc/loadavg)" >> "$OUT"
# --- GPU co-tenant phase ---
: > "$FLAG"
( while [ -f "$FLAG" ]; do curl -s http://127.0.0.1:11434/api/generate -d "{\"model\":\"$MODEL\",\"prompt\":\"Write a long detailed essay about oceans, tides, ships, storms, harbours, lighthouses and sailors.\",\"stream\":false,\"options\":{\"num_predict\":900}}" > /dev/null 2>&1; done ) &
LOADPID=$!
( while [ -f "$FLAG" ]; do echo "sample gpu_busy=$(cat $GPUDEV) memavail_kb=$(memavail) load=$(cat /proc/loadavg)" >> "$OUT.gpu"; sleep 2; done ) &
SAMPID=$!
ENGAGED=0; i=0
while [ "$i" -lt 45 ]; do B=$(cat $GPUDEV); if [ "$B" -ge 50 ]; then ENGAGED=1; break; fi; sleep 2; i=$((i + 1)); done
echo "gpu_engaged: $ENGAGED busy_at_engage: $(cat $GPUDEV)" >> "$OUT"
if [ "$ENGAGED" -ne 1 ]; then echo "GPU_LOAD_FAILED" >> "$OUT"; rm -f "$FLAG"; wait $LOADPID 2>/dev/null; wait $SAMPID 2>/dev/null; cat "$OUT.gpu" >> "$OUT" 2>/dev/null; exit 3; fi
echo "memavail_kb_under_load: $(memavail)" >> "$OUT"
echo "PHASE gpuload" >> "$OUT"
battery
rm -f "$FLAG"; wait $LOADPID 2>/dev/null; wait $SAMPID 2>/dev/null
cat "$OUT.gpu" >> "$OUT" 2>/dev/null; rm -f "$OUT.gpu"
curl -s http://127.0.0.1:11434/api/generate -d "{\"model\":\"$MODEL\",\"prompt\":\"x\",\"stream\":false,\"keep_alive\":0,\"options\":{\"num_predict\":1}}" > /dev/null 2>&1
echo "gpu_busy_after_unload: $(cat $GPUDEV)" >> "$OUT"
echo "load_at_end: $(cat /proc/loadavg)" >> "$OUT"
echo "probe_stderr_bytes: $(wc -c < "$OUT.err")" >> "$OUT"
rm -f "$OUT.err"
echo "raw lines: $(wc -l < "$OUT")" >&2
