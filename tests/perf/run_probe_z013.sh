#!/bin/sh
# run_probe_z013.sh - Z-013 THP-under-madvise probe runner.
# Runs the built probe once (the probe itself does REPS=5
# per config and prints raw + median + verdict lines) and
# appends its output to the file named by $1.
# Usage: run_probe_z013.sh <output-file>
set -eu
OUT=${1:?usage: run_probe_z013.sh <output-file>}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
BIN="$ROOT/build/probe_z013"
[ -x "$BIN" ] || { echo "build first: make test-z013" >&2; exit 1; }
"$BIN" >> "$OUT"
echo "probe lines: $(wc -l < "$OUT")" >&2
