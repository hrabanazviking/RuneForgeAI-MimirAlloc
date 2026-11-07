#!/bin/sh
# run_bench_k008.sh - K-008 tail-latency + RSS/PSS + energy runner.
# Distribution/footprint phase: REPS fresh processes per leg,
# alternating leg order (drift lands on both legs). Energy
# phase: 2 passes per condition (idle sleep, glibc, mimiralloc);
# each pass runs the workload ~6 s in the background while this
# script samples the Pi 5 PMIC via `vcgencmd pmic_read_adc`
# (~100 ms interval) and sums V*A over every rail that reports
# both a current and a voltage of the same name. That is a
# WHOLE-BOARD figure (Samtal and the rest of the Pi included),
# never an allocator-only figure. Usage: run_bench_k008.sh <out> [reps]
set -eu
OUT=${1:?usage: run_bench_k008.sh <raw-output-file> [reps]}
REPS=${2:-5}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
GBIN="$ROOT/build/bench_k008_glibc"
MBIN="$ROOT/build/bench_k008_mimir"
[ -x "$GBIN" ] && [ -x "$MBIN" ] || { echo "build first: make bench-k008" >&2; exit 1; }
: > "$OUT"
r=1
while [ "$r" -le "$REPS" ]; do
    "$GBIN" >> "$OUT"
    "$MBIN" >> "$OUT"
    r=$((r + 1))
done
sample_pmic() {
    # $1 = command..., runs it in background, samples PMIC until it exits
    "$@" > /tmp/k008_erun.txt 2>&1 &
    pid=$!
    python3 - "$pid" >> "$OUT" <<'PYEOF'
import re, subprocess, sys, time
pid = int(sys.argv[1])
amps, volts, samples = {}, {}, []
t0 = time.time()
while True:
    try: out = subprocess.run(["vcgencmd", "pmic_read_adc"], capture_output=True, text=True, timeout=5).stdout
    except Exception: out = ""
    a, v = {}, {}
    for line in out.splitlines():
        m = re.search(r"(\S+)_A current\(\d+\)=([\d.]+)A", line)
        if m: a[m.group(1)] = float(m.group(2))
        m = re.search(r"(\S+)_V volt\(\d+\)=([\d.]+)V", line)
        if m: v[m.group(1)] = float(m.group(2))
    tot = sum(a[k] * v[k] for k in a if k in v)
    if tot > 0: samples.append(tot)
    try:
        with open(f"/proc/{pid}/stat") as fh: alive = fh.read().split()[2] != "Z"
    except OSError: alive = False
    if not alive: break
    time.sleep(0.1)
dur = time.time() - t0
if samples:
    mean = sum(samples) / len(samples)
    print(f"ENERGY,COND,{len(samples)},{mean:.3f},{min(samples):.3f},{max(samples):.3f},{dur:.2f},{mean*dur:.2f}")
PYEOF
    wait $pid || true
    sed -i "s/^ENERGY,COND/ENERGY,$COND/" "$OUT"
    cat /tmp/k008_erun.txt >> "$OUT"
}
p=1
while [ "$p" -le 2 ]; do
    COND=idle;      sample_pmic sleep 6
    COND=glibc;     sample_pmic "$GBIN" energy
    COND=mimiralloc; sample_pmic "$MBIN" energy
    p=$((p + 1))
done
echo "raw lines: $(wc -l < "$OUT")" >&2
