#!/bin/sh
# run_z011.sh - Z-011 Bandwidth saturation runner (MimirAlloc Z13).
# Builds tests/perf/probe_z011.c standalone (Makefile untouched,
# house perf pattern) and runs the STREAM-style triad thread sweep
# 1/2/4/8/16/32. Load state is recorded before and after; this host
# is shared (Aesir forge, Lifthrasir forge, Tensile lab), so any
# contention present during the run is named in the output.
set -u
cd "$(dirname "$0")/../.." || exit 2
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
echo "=== Z-011 bandwidth saturation ==="
date -Is; uname -a; git rev-parse HEAD
echo "--- load (start) ---"; uptime; ps aux --sort=-%cpu | head -6
echo "--- topology (sysfs) ---"
for c in 0 8; do
  echo "cpu$c L3 shared_cpu_list: $(cat /sys/devices/system/cpu/cpu$c/cache/index3/shared_cpu_list) L3 size: $(cat /sys/devices/system/cpu/cpu$c/cache/index3/size)"
done
echo "governor cpu0: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
echo "--- build probe_z011 ---"
cc $CFLAGS tests/perf/probe_z011.c -o /tmp/probe_z011 || exit 1
echo "build exit=0"
sha256sum tests/perf/probe_z011.c tests/perf/run_z011.sh /tmp/probe_z011
echo "--- triad sweep (timeout 600) ---"
timeout 600 /tmp/probe_z011; echo "probe exit=$?"
echo "--- load (end) ---"; uptime
echo "--- run_z011 done ---"
