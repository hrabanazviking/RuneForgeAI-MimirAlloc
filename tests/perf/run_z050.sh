#!/usr/bin/env bash
# Z-050 pressure-cycle runner: unit/policy suite once, RSS
# probe three times, host + load + live PSI filed in header.
set -u
cd "$(dirname "$0")/../.."
echo "run_z050_version: 1"
echo "date_local: $(date '+%Y-%m-%dT%H:%M:%S%z')"
echo "host: $(uname -a)"
echo "loadavg_start: $(cat /proc/loadavg)"
echo "pressure_memory_live: $(tr '\n' ' ' < /proc/pressure/memory)"
make -s test-z050 2>&1
echo "unit_exit: $?"
make -s probe-z050 2>&1
for i in 1 2 3; do
  echo "--- probe attempt $i ---"
  ./build/probe_z050
  echo "probe_exit: $?"
done
echo "loadavg_end: $(cat /proc/loadavg)"
