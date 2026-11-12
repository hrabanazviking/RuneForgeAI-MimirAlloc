#!/bin/sh
# run_probe_z017.sh - Z-017 alignment audit runner.
# Runs both built legs (glibc, then mimir) once and appends
# their output to the file named by $1.
# Usage: run_probe_z017.sh <output-file>
set -eu
OUT=${1:?usage: run_probe_z017.sh <output-file>}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
for leg in glibc mimir; do
    BIN="$ROOT/build/probe_z017_$leg"
    [ -x "$BIN" ] || { echo "build first: make test-z017" >&2; exit 1; }
    "$BIN" >> "$OUT"
done
echo "probe lines: $(wc -l < "$OUT")" >&2
