#!/bin/sh
# run_z025.sh - Z-025 calloc and realloc anatomy runner.
#
# Z-004 protocol (docs/Z13_MEASUREMENT_PROTOCOL.md): both
# legs built by `make test-z025` (production LIBOBJS for
# the mimir leg); measured processes are separate
# processes, interleaved A,B (mimir,glibc), pinned
# taskset -c 2 (CCD0 physical core); 3 warm-up processes
# per leg discarded, 7 measured processes per leg.
# Raw CSV lands in hidden_files/z025_raw/ (team-local);
# summarize_z025.py prints the median tables. Load state
# is recorded before and after; this host is shared
# (Aesir forge, Lifthrasir forge, Tensile lab), so any
# contention present is named, never hidden.
set -u
cd "$(dirname "$0")/../.." || exit 2
ROOT=$(pwd)
RAW=hidden_files/z025_raw
mkdir -p "$RAW"
echo "=== Z-025 calloc and realloc anatomy ==="
date -Is; uname -a; git rev-parse HEAD
scripts/z13/host_truth.sh
echo "--- load (start) ---"; uptime
ps aux --sort=-%cpu | head -6
echo "--- build (make test-z025 compiles the legs) ---"
make test-z025 >/dev/null 2>&1 || { echo "build FAILED"; exit 1; }
ls -l build/probe_z025_glibc build/probe_z025_mimir
sha256sum tests/perf/probe_z025.c tests/perf/run_z025.sh \
          tests/perf/summarize_z025.py
rm -f "$RAW"/leg_*.csv
echo "--- warm-ups (3 per leg, discarded) ---"
for w in 1 2 3; do
  taskset -c 2 ./build/probe_z025_mimir >/dev/null 2>&1
  taskset -c 2 ./build/probe_z025_glibc >/dev/null 2>&1
  echo "warmup $w done"
done
echo "--- measured (7 per leg, interleaved) ---"
fail=0
for r in 1 2 3 4 5 6 7; do
  taskset -c 2 ./build/probe_z025_mimir >> "$RAW/leg_mimir.csv" 2>>"$RAW/err.log" || fail=1
  taskset -c 2 ./build/probe_z025_glibc >> "$RAW/leg_glibc.csv" 2>>"$RAW/err.log" || fail=1
  echo "rep $r done (load: $(cut -d' ' -f1-3 /proc/loadavg))"
done
echo "probe exit fail flag: $fail"
echo "--- load (end) ---"; uptime
echo "--- summary ---"
python3 tests/perf/summarize_z025.py "$RAW/leg_mimir.csv" "$RAW/leg_glibc.csv"
echo "summary exit=$?"
echo "--- run_z025 done ---"
