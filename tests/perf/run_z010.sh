#!/bin/sh
# run_z010.sh - Z-010 Latency ladder runner (MimirAlloc Z13).
# Builds tests/perf/probe_z010.c standalone (Makefile untouched,
# house perf pattern) and runs it pinned to one physical core on
# each CCD: CPU 0 (CCD0) and CPU 8 (CCD1). Topology is printed
# from sysfs so the pinning claim is evidence, not assertion.
# Load state is recorded before and after; this host is shared
# (Aesir forge, Lifthrasir forge, Tensile lab), so contention
# present during the run is named in the output, never hidden.
set -u
cd "$(dirname "$0")/../.." || exit 2
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
echo "=== Z-010 latency ladder ==="
date -Is; uname -a; git rev-parse HEAD
echo "--- load (start) ---"; uptime; ps aux --sort=-%cpu | head -6
echo "--- topology (sysfs) ---"
for c in 0 8; do
  echo "cpu$c L3 shared_cpu_list: $(cat /sys/devices/system/cpu/cpu$c/cache/index3/shared_cpu_list) L3 size: $(cat /sys/devices/system/cpu/cpu$c/cache/index3/size)"
  echo "cpu$c L2 size: $(cat /sys/devices/system/cpu/cpu$c/cache/index2/size) L1d size: $(cat /sys/devices/system/cpu/cpu$c/cache/index0/size)"
done
echo "governor cpu0: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
echo "--- build probe_z010 ---"
cc $CFLAGS tests/perf/probe_z010.c -o /tmp/probe_z010 || exit 1
echo "build exit=0"
sha256sum tests/perf/probe_z010.c tests/perf/run_z010.sh /tmp/probe_z010
for cpu in 0 8; do
  echo "--- chase leg cpu=$cpu (timeout 600) ---"
  timeout 600 /tmp/probe_z010 $cpu; echo "leg cpu=$cpu exit=$?"
done
echo "--- load (end) ---"; uptime
echo "--- run_z010 done ---"
