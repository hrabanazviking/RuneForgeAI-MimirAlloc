#!/bin/sh
# run_z040.sh - Z-040 False-sharing purge runner (MimirAlloc Z13).
# Builds tests/perf/probe_z040.c standalone (house perf pattern),
# prints the Z-002 host-truth header, records load before/after,
# and runs the probe TWICE (primary + corroborating, Z-016
# discipline). Each run writes its own raw file:
#   run_z040.sh <raw1> <raw2>
# Usage from repo root or anywhere; paths resolved to repo root.
set -u
cd "$(dirname "$0")/../.." || exit 2
RAW1=${1:?usage: run_z040.sh <raw1> <raw2>}
RAW2=${2:?usage: run_z040.sh <raw1> <raw2>}
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
echo "=== Z-040 False-sharing purge ==="
date -Is; uname -a; git rev-parse HEAD
echo "--- host truth (scripts/z13/host_truth.sh, Z-002) ---"
sh scripts/z13/host_truth.sh
echo "--- load (start) ---"; uptime; ps aux --sort=-%cpu | head -6
echo "--- build probe_z040 ---"
cc $CFLAGS tests/perf/probe_z040.c -o /tmp/probe_z040 || exit 1
echo "build exit=0"
sha256sum tests/perf/probe_z040.c tests/perf/run_z040.sh /tmp/probe_z040
echo "--- probe run 1 (primary) ---"
timeout 900 /tmp/probe_z040 > "$RAW1"; echo "probe1 exit=$?"
echo "--- load (mid) ---"; uptime
echo "--- probe run 2 (corroborating) ---"
timeout 900 /tmp/probe_z040 > "$RAW2"; echo "probe2 exit=$?"
echo "--- load (end) ---"; uptime
echo "--- run_z040 done ---"
