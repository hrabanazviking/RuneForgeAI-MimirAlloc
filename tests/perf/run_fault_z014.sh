#!/bin/sh
# run_fault_z014.sh - Z-014 page-fault cost curve runner.
# Follows docs/Z13_MEASUREMENT_PROTOCOL.md (Z-004, banked):
# pinned taskset -c 2 (CCD0 physical core), measured values
# are separate PROCESSES (default 7 per size+mode), modes
# interleaved fresh,recyc_map,recyc_keep per rep so drift
# lands on all legs, warm-up processes discarded before
# measuring (3 per mode for sizes < 16 MiB, 1 for larger -
# the large-size processes are long and self-warming via
# the recycled legs internal cycles; deviation from the
# default 3 is stated here and in the recorded run).
# Usage: run_fault_z014.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_fault_z014.sh <raw-output-file> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
BIN="$ROOT/build/probe_z014"
if [ ! -x "$BIN" ]; then
    mkdir -p "$ROOT/build"
    gcc -std=c11 -Wall -Wextra -Werror -O2 \
        "$ROOT/tests/perf/probe_z014.c" -o "$BIN"
fi
SIZES="4096 16384 65536 262144 1048576 4194304 16777216 67108864 268435456 1073741824"
MODES="fresh recyc_map recyc_keep"
: > "$OUT"
for S in $SIZES; do
    W=1
    if [ "$S" -ge 16777216 ]; then W=1; else W=3; fi
    for M in $MODES; do
        i=0
        while [ "$i" -lt "$W" ]; do
            taskset -c 2 "$BIN" "$S" "$M" > /dev/null
            i=$((i + 1))
        done
    done
    r=1
    while [ "$r" -le "$REPS" ]; do
        for M in $MODES; do
            taskset -c 2 "$BIN" "$S" "$M" >> "$OUT"
        done
        r=$((r + 1))
    done
    echo "size $S done" >&2
done
echo "raw lines: $(wc -l < "$OUT") (expect $((REPS * 30)))" >&2
