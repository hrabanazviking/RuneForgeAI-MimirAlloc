#!/usr/bin/env bash
# Z-060 pressure-cycle runner: in-process cycle/cap/madv
# legs (make test-z060 once, cycles repeated to 3 attempts
# in the Z-050 style), the ceiling leg inside a systemd
# user scope (MemoryMax/MemorySwapMax: a cgroup ceiling
# scoped to this slice's own test process only), and the
# hog containment leg under a tight scope. Host + load +
# live PSI filed in the header. No other process is ever
# placed in these scopes.
set -u
cd "$(dirname "$0")/../.."
export XDG_RUNTIME_DIR=/run/user/1000
export DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus
echo "run_z060_version: 1"
echo "date_local: $(date '+%Y-%m-%dT%H:%M:%S%z')"
echo "host: $(uname -a)"
echo "loadavg_start: $(cat /proc/loadavg)"
echo "pressure_memory_live: $(tr '\n' ' ' < /proc/pressure/memory)"
echo "runner_cgroup: $(cat /proc/self/cgroup)"
make -s test-z060 2>&1
echo "test_z060_exit: $?"
for i in 2 3; do
  echo "--- cycles attempt $i ---"
  ./build/probe_z060 cycles
  echo "cycles_exit: $?"
done
echo "--- ceiling leg (systemd user scope, MemoryMax=512M) ---"
systemd-run --user --scope -p MemoryMax=512M -p MemorySwapMax=0 \
  ./build/probe_z060 ceiling 2>&1
echo "ceiling_exit: $?"
echo "--- hog leg (systemd user scope, MemoryMax=256M) ---"
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  ./build/probe_z060 hog 2>&1
echo "hog_exit: $?"
echo "--- hogfree leg (scope MemoryMax=256M, MADV_FREE reclaim) ---"
systemd-run --user --scope -p MemoryMax=256M -p MemorySwapMax=0 \
  ./build/probe_z060 hogfree 2>&1
echo "hogfree_exit: $?"
echo "host_alive_after_hog: $(cat /proc/loadavg)"
echo "loadavg_end: $(cat /proc/loadavg)"
