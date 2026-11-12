#!/bin/sh
# run_z044.sh - Z-044 Fork posture proof runner.
#
# Builds both probe legs fresh (stale outputs deleted
# first), runs the mimir leg TWICE (run A + corroborating
# run B) and the glibc comparator leg ONCE, all under the
# H-006 disposable-child discipline the probe implements
# itself. FAILS CLOSED: any build failure or nonzero probe
# exit (a failed EXECUTED proof phase) exits nonzero
# BEFORE the summary; hung busy children are measurements
# the probe classifies, never runner failures.
#
# Containment (shared host): ulimit -c 0 (no core dumps),
# the whole probe wrapped in timeout(1) as a backstop, the
# probe runs niced and confined to CPUs 2-5 (CCD0) so the
# desktop and other campaigns keep the rest; churners are
# bounded alloc/free loops inside the probe. The runner
# never detaches. Load is recorded start/end and a post-
# run orphan sweep must find no probe process left.
set -u
cd "$(dirname "$0")/../.." || exit 2
ROOT=$(pwd)
RAW=hidden_files/z044_raw
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
ulimit -c 0
echo "=== Z-044 Fork posture proof on this host ==="
date -Is; uname -a; git rev-parse HEAD
scripts/z13/host_truth.sh
echo "--- load (start) ---"; uptime
ps aux --sort=-%cpu | head -6
echo "--- build (fresh; stale outputs deleted first) ---"
rm -f /tmp/probe_z044_mimir /tmp/probe_z044_glibc
rm -rf "$RAW"; mkdir -p "$RAW"
make build/libmimiralloc.a >/dev/null 2>&1 || { echo "lib build FAILED"; exit 1; }
cc $CFLAGS -DLEG_GLIBC tests/perf/probe_z044.c -o /tmp/probe_z044_glibc || { echo "glibc leg build FAILED"; exit 1; }
echo "glibc leg build exit=0"
cc $CFLAGS tests/perf/probe_z044.c build/libmimiralloc.a -o /tmp/probe_z044_mimir || { echo "mimir leg build FAILED"; exit 1; }
echo "mimir leg build exit=0"
ls -l /tmp/probe_z044_mimir /tmp/probe_z044_glibc
sha256sum tests/perf/probe_z044.c tests/perf/run_z044.sh tests/perf/summarize_z044.py
echo "--- measured runs (mimir A, mimir B, glibc) ---"
fail=0
nice -n 10 taskset -c 2-5 timeout --kill-after=5s 300 /tmp/probe_z044_mimir > "$RAW/leg_mimir_a.txt" 2>>"$RAW/err.log" || fail=1
echo "mimir run A exit=$([ $fail -eq 0 ] && echo 0 || echo nonzero) (load: $(cut -d' ' -f1-3 /proc/loadavg))"
nice -n 10 taskset -c 2-5 timeout --kill-after=5s 300 /tmp/probe_z044_mimir > "$RAW/leg_mimir_b.txt" 2>>"$RAW/err.log" || fail=1
echo "mimir run B done (load: $(cut -d' ' -f1-3 /proc/loadavg))"
nice -n 10 taskset -c 2-5 timeout --kill-after=5s 300 /tmp/probe_z044_glibc > "$RAW/leg_glibc.txt" 2>>"$RAW/err.log" || fail=1
echo "glibc run done (load: $(cut -d' ' -f1-3 /proc/loadavg))"
cat "$RAW/leg_mimir_a.txt" "$RAW/leg_mimir_b.txt" "$RAW/leg_glibc.txt"
echo "probe exit fail flag: $fail"
if [ "$fail" -ne 0 ]; then echo "PROBE FAILED - no summary"; exit 1; fi
echo "--- orphan sweep (must be empty) ---"
pgrep -a -f probe_z044 || echo "orphan sweep: no probe processes remain"
echo "--- load (end) ---"; uptime
echo "--- summary ---"
python3 tests/perf/summarize_z044.py "$RAW/leg_mimir_a.txt" "$RAW/leg_mimir_b.txt" "$RAW/leg_glibc.txt"
echo "summary exit=$?"
echo "--- run_z044 done ---"
