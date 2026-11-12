#!/bin/sh
# run_measured.sh - Z13 measured-run harness (Z-004).
#
# Implements the Z13 measurement protocol
# (docs/Z13_MEASUREMENT_PROTOCOL.md) as harness switches:
# both legs are pinned with taskset to the identical CPU list,
# warm-up processes run first (discarded), measured processes
# interleave A,B,A,B,... as separate processes, and every run
# writes an environment header (Z-002 host_truth.sh output when
# that script is present, load at start and end, the switches).
#
# Usage:
#   scripts/z13/run_measured.sh --leg-a <path> --leg-b <path>
#       --out <prefix> [--cpus <list>] [--reps <n>]
#       [--warmups <n>] [--noise-pct <p>] [--drift-pct <p>]
# Every switch has an environment default: Z13_LEG_A, Z13_LEG_B,
# Z13_OUT, Z13_CPUS (default 2), Z13_REPS (default 7, min 5),
# Z13_WARMUPS (default 3), Z13_NOISE_PCT (default 5),
# Z13_DRIFT_PCT (default 8). Flags override the environment.
#
# Outputs: <prefix>.header, <prefix>.legA.raw, <prefix>.legB.raw
# (raw CSV lines in the harness's own format, run order kept).
# Verdicts: scripts/z13/summarize_measured.py on the two raw files.
set -eu

LEG_A=${Z13_LEG_A:-}
LEG_B=${Z13_LEG_B:-}
OUT=${Z13_OUT:-}
CPUS=${Z13_CPUS:-2}
REPS=${Z13_REPS:-7}
WARMUPS=${Z13_WARMUPS:-3}
NOISE_PCT=${Z13_NOISE_PCT:-5}
DRIFT_PCT=${Z13_DRIFT_PCT:-8}

while [ $# -gt 0 ]; do
    case "$1" in
        --leg-a) LEG_A=$2; shift 2 ;;
        --leg-b) LEG_B=$2; shift 2 ;;
        --out) OUT=$2; shift 2 ;;
        --cpus) CPUS=$2; shift 2 ;;
        --reps) REPS=$2; shift 2 ;;
        --warmups) WARMUPS=$2; shift 2 ;;
        --noise-pct) NOISE_PCT=$2; shift 2 ;;
        --drift-pct) DRIFT_PCT=$2; shift 2 ;;
        *) echo "unknown switch: $1" >&2; exit 2 ;;
    esac
done

[ -n "$LEG_A" ] && [ -n "$LEG_B" ] && [ -n "$OUT" ] || {
    echo "usage: run_measured.sh --leg-a <path> --leg-b <path> --out <prefix>" >&2
    exit 2
}
[ -x "$LEG_A" ] || { echo "leg-a not executable: $LEG_A" >&2; exit 1; }
[ -x "$LEG_B" ] || { echo "leg-b not executable: $LEG_B" >&2; exit 1; }
command -v taskset >/dev/null 2>&1 || {
    echo "taskset not found; pinning is mandatory (protocol section 3)" >&2
    exit 1
}
if [ "$REPS" -lt 5 ]; then
    echo "reps must be >= 5 (protocol section 6)" >&2
    exit 2
fi

# Verify the pin is accepted before any number is taken
# (protocol section 3 rule 5): a refused mask aborts the run.
taskset -c "$CPUS" true || {
    echo "taskset refused CPU list: $CPUS" >&2
    exit 1
}

DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)

{
    echo "run_measured_version: 1"
    echo "date_utc: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    echo "leg_a: $LEG_A"
    echo "leg_b: $LEG_B"
    echo "cpus: $CPUS"
    echo "reps: $REPS"
    echo "warmups: $WARMUPS"
    echo "noise_pct: $NOISE_PCT"
    echo "drift_pct: $DRIFT_PCT"
    echo "loadavg_start: $(awk '{print $1, $2, $3}' /proc/loadavg)"
    if [ -x "$ROOT/scripts/z13/host_truth.sh" ]; then
        echo "--- host_truth (scripts/z13/host_truth.sh, verbatim) ---"
        "$ROOT/scripts/z13/host_truth.sh"
        echo "--- end host_truth ---"
    else
        echo "host_truth: unavailable in this tree (Z-002 script not present); minimal stamp follows"
        echo "scaling_governor: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo unavailable)"
        echo "scaling_cur_freq_khz: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq 2>/dev/null || echo unavailable)"
        echo "kernel: $(uname -srm)"
    fi
} > "$OUT.header"

: > "$OUT.legA.raw"
: > "$OUT.legB.raw"

# Warm-up processes, interleaved like the measured ones,
# output discarded (protocol section 4).
w=1
while [ "$w" -le "$WARMUPS" ]; do
    taskset -c "$CPUS" "$LEG_A" > /dev/null
    taskset -c "$CPUS" "$LEG_B" > /dev/null
    w=$((w + 1))
done

# Measured processes: A,B,A,B,... (protocol section 5).
r=1
while [ "$r" -le "$REPS" ]; do
    taskset -c "$CPUS" "$LEG_A" >> "$OUT.legA.raw"
    taskset -c "$CPUS" "$LEG_B" >> "$OUT.legB.raw"
    r=$((r + 1))
done

echo "loadavg_end: $(awk '{print $1, $2, $3}' /proc/loadavg)" >> "$OUT.header"
echo "raw lines: A=$(wc -l < "$OUT.legA.raw") B=$(wc -l < "$OUT.legB.raw")" >&2
