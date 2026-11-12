#!/bin/sh
# run_z033.sh - Z-033 Size-class retune (conditional) runner.
#
# Condition-independent harness run: Z-032 has not landed,
# so this measures the CURRENT size-class table (BEFORE
# legs) under the Z-004 protocol: legs built fresh, stale
# outputs deleted first, 3 warm-up processes per leg
# discarded, 7 measured processes per leg interleaved,
# pinned taskset -c 2 (CCD0). FAILS CLOSED: any build or
# probe failure exits nonzero BEFORE the summary; no
# stale binary or CSV can pass. Load recorded start/end;
# this host is shared (Aesir forge, Lifthrasir forge,
# Tensile lab) - contention is named, never hidden.
set -u
cd "$(dirname "$0")/../.." || exit 2
ROOT=$(pwd)
RAW=hidden_files/z033_raw
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
echo "=== Z-033 Size-class retune (conditional) - BEFORE legs ==="
date -Is; uname -a; git rev-parse HEAD
scripts/z13/host_truth.sh
echo "--- load (start) ---"; uptime
ps aux --sort=-%cpu | head -6
echo "--- build (fresh; stale outputs deleted first) ---"
rm -f /tmp/probe_z033_mimir /tmp/probe_z033_glibc
rm -rf "$RAW"; mkdir -p "$RAW"
make build/libmimiralloc.a >/dev/null 2>&1 || { echo "lib build FAILED"; exit 1; }
cc $CFLAGS -DLEG_GLIBC tests/perf/probe_z033.c -o /tmp/probe_z033_glibc || { echo "glibc leg build FAILED"; exit 1; }
echo "glibc leg build exit=0"
cc $CFLAGS tests/perf/probe_z033.c build/libmimiralloc.a -o /tmp/probe_z033_mimir || { echo "mimir leg build FAILED"; exit 1; }
echo "mimir leg build exit=0"
ls -l /tmp/probe_z033_mimir /tmp/probe_z033_glibc
sha256sum tests/perf/probe_z033.c tests/perf/run_z033.sh tests/perf/summarize_z033.py
echo "--- warm-ups (3 per leg, discarded) ---"
for w in 1 2 3; do
  taskset -c 2 /tmp/probe_z033_mimir >/dev/null 2>&1 || { echo "warmup mimir FAILED"; exit 1; }
  taskset -c 2 /tmp/probe_z033_glibc >/dev/null 2>&1 || { echo "warmup glibc FAILED"; exit 1; }
  echo "warmup $w done"
done
echo "--- measured (7 per leg, interleaved) ---"
fail=0
for r in 1 2 3 4 5 6 7; do
  taskset -c 2 /tmp/probe_z033_mimir >> "$RAW/leg_mimir.csv" 2>>"$RAW/err.log" || fail=1
  taskset -c 2 /tmp/probe_z033_glibc >> "$RAW/leg_glibc.csv" 2>>"$RAW/err.log" || fail=1
  echo "rep $r done (load: $(cut -d' ' -f1-3 /proc/loadavg))"
done
echo "probe exit fail flag: $fail"
if [ "$fail" -ne 0 ]; then echo "PROBE FAILED - no summary"; exit 1; fi
echo "--- load (end) ---"; uptime
echo "--- summary ---"
python3 tests/perf/summarize_z033.py "$RAW/leg_mimir.csv" "$RAW/leg_glibc.csv"
echo "summary exit=$?"
echo "--- run_z033 done ---"
