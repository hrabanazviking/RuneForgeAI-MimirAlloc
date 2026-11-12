#!/bin/sh
# run_z012.sh - Z-012 CCD and SMT behaviour runner (MimirAlloc Z13).
# Builds tests/perf/probe_z012.c standalone (house perf pattern),
# prints the Z-002 host-truth header + sysfs topology as evidence,
# records load before/after (shared host: Aesir forge, Lifthrasir
# forge, Tensile lab - contention is named, never hidden).
set -u
cd "$(dirname "$0")/../.." || exit 2
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
echo "=== Z-012 CCD and SMT behaviour ==="
date -Is; uname -a; git rev-parse HEAD
echo "--- host truth (scripts/z13/host_truth.sh, Z-002) ---"
sh scripts/z13/host_truth.sh
echo "--- load (start) ---"; uptime; ps aux --sort=-%cpu | head -6
echo "--- topology (sysfs) ---"
for c in 0 8 16 24; do
  echo "cpu$c core_id=$(cat /sys/devices/system/cpu/cpu$c/topology/core_id) siblings=$(cat /sys/devices/system/cpu/cpu$c/topology/thread_siblings_list) L3=$(cat /sys/devices/system/cpu/cpu$c/cache/index3/size) L3_shared=$(cat /sys/devices/system/cpu/cpu$c/cache/index3/shared_cpu_list)"
done
echo "governor cpu0: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
echo "--- build probe_z012 ---"
cc $CFLAGS tests/perf/probe_z012.c -o /tmp/probe_z012 || exit 1
echo "build exit=0"
sha256sum tests/perf/probe_z012.c tests/perf/run_z012.sh /tmp/probe_z012
echo "--- probe run (timeout 600) ---"
timeout 600 /tmp/probe_z012; echo "probe exit=$?"
echo "--- load (end) ---"; uptime
echo "--- run_z012 done ---"
