#!/bin/sh
# run_z015.sh - Z-015 Syscall and VMA economics runner (MimirAlloc Z13).
# Builds tests/perf/probe_z015.c standalone (Makefile untouched, house
# perf pattern) and runs it twice pinned to CPU 2 (CCD0) via taskset,
# per the Z-004 measurement protocol: per-op samples, median + min
# reported. Load state is recorded before and after; this host is
# shared (Aesir forge, Lifthrasir forge, Tensile lab), so contention
# present during the run is named in the output, never hidden.
set -u
cd "$(dirname "$0")/../.." || exit 2
CFLAGS="-std=c11 -Wall -Wextra -Werror -O2"
echo "=== Z-015 syscall and VMA economics ==="
date -Is; uname -a; git rev-parse HEAD
echo "--- load (start) ---"; uptime; ps aux --sort=-%cpu | head -6
echo "pagesize: $(getconf PAGESIZE)"
echo "vm.max_map_count: $(cat /proc/sys/vm/max_map_count)"
echo "THP enabled: $(cat /sys/kernel/mm/transparent_hugepage/enabled)"
echo "governor cpu2: $(cat /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor)"
echo "cpu2 L3 shared_cpu_list: $(cat /sys/devices/system/cpu/cpu2/cache/index3/shared_cpu_list)"
echo "--- build probe_z015 ---"
cc $CFLAGS tests/perf/probe_z015.c -o /tmp/probe_z015 || exit 1
echo "build exit=0"
sha256sum tests/perf/probe_z015.c tests/perf/run_z015.sh /tmp/probe_z015
for r in 1 2; do echo "--- probe run $r (taskset -c 2) ---"; timeout 300 taskset -c 2 /tmp/probe_z015; echo "run $r exit=$?"; done
echo "--- load (end) ---"; uptime
echo "--- run_z015 done ---"
