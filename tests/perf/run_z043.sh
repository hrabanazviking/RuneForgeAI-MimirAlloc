#!/usr/bin/env bash
# Z-043 runner: free-storm correctness suite + perf probe.
# Builds the Z-043 storm suite and probe against the
# production library OBJECTS (test-only static link, the
# shape every internal surface in this tree uses), runs the
# suite (green build plus BOTH red-path proofs, which MUST
# fail: the Z-030 lossy queue mutant -DMRQ_MUTANT_LOSSY and
# this slice's dropped-free mutant -DZ043_MUTANT_DROP_FREE),
# then runs the perf probe per the Z-004 protocol: 3 warm-up
# processes discarded + 7 measured per phase, one phase per
# process. A glibc comparator build runs the two latency
# phases only. NOT part of `make test`; the standing gate
# is untouched.
# Usage: tests/perf/run_z043.sh [raw_csv]
set -u
cd "$(dirname "$0")/../.."
RAW="${1:-hidden_files/z043_raw.csv}"
mkdir -p build hidden_files
LOG=hidden_files/z043_suite.log
{
  echo "# Z-043 suite: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "# host: $(uname -a)"
  echo "# head: $(git rev-parse HEAD)"
  echo "# loadavg: $(cat /proc/loadavg)"
} > "$LOG"
make build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o >> "$LOG" 2>&1 || { echo "BUILD FAILED (library objects)"; exit 2; }
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -Itests/concurrency -O2"
cc $CFLAGS -c tests/concurrency/test_z043_free_storm.c -o build/test_z043_free_storm.o >> "$LOG" 2>&1 || { echo "BUILD FAILED (suite)"; exit 2; }
cc $CFLAGS build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o build/test_z043_free_storm.o -o build/test_z043_free_storm >> "$LOG" 2>&1 || { echo "BUILD FAILED (suite link)"; exit 2; }
./build/test_z043_free_storm >> "$LOG" 2>&1
SUITE_RC=$?
echo "# suite exit code: $SUITE_RC" >> "$LOG"
cat "$LOG"
if [ "$SUITE_RC" -ne 0 ]; then
  echo "SUITE FAILED"
  exit 1
fi
# Red-path proof 1: the lossy queue mutant MUST fail.
MLOG=hidden_files/z043_mutant_queue.log
cc $CFLAGS -DMRQ_MUTANT_LOSSY -c tests/concurrency/test_z043_free_storm.c -o build/test_z043_mutant_queue.o >> "$MLOG" 2>&1 || { echo "BUILD FAILED (queue mutant)"; exit 2; }
cc $CFLAGS build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o build/test_z043_mutant_queue.o -o build/test_z043_mutant_queue >> "$MLOG" 2>&1 || { echo "BUILD FAILED (queue mutant link)"; exit 2; }
./build/test_z043_mutant_queue >> "$MLOG" 2>&1
MUTANT_RC=$?
echo "# queue mutant exit code: $MUTANT_RC" >> "$MLOG"
if [ "$MUTANT_RC" -eq 0 ]; then
  echo "QUEUE MUTANT DID NOT FAIL - suite proves nothing"
  cat "$MLOG"
  exit 1
fi
echo "red-path proof 1: queue mutant exit $MUTANT_RC (expected nonzero); log $MLOG"
# Red-path proof 2: the dropped-free mutant MUST fail.
DLOG=hidden_files/z043_mutant_drop.log
cc $CFLAGS -DZ043_MUTANT_DROP_FREE -c tests/concurrency/test_z043_free_storm.c -o build/test_z043_mutant_drop.o >> "$DLOG" 2>&1 || { echo "BUILD FAILED (drop mutant)"; exit 2; }
cc $CFLAGS build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o build/test_z043_mutant_drop.o -o build/test_z043_mutant_drop >> "$DLOG" 2>&1 || { echo "BUILD FAILED (drop mutant link)"; exit 2; }
./build/test_z043_mutant_drop >> "$DLOG" 2>&1
DROP_RC=$?
echo "# drop mutant exit code: $DROP_RC" >> "$DLOG"
if [ "$DROP_RC" -eq 0 ]; then
  echo "DROP MUTANT DID NOT FAIL - suite proves nothing"
  cat "$DLOG"
  exit 1
fi
echo "red-path proof 2: drop mutant exit $DROP_RC (expected nonzero); log $DLOG"
# Perf probe (mimir build + glibc comparator build).
cc $CFLAGS -c tests/perf/probe_z043.c -o build/probe_z043.o 2> hidden_files/z043_probe_build.log || { echo "BUILD FAILED (probe)"; exit 2; }
cc $CFLAGS build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o build/probe_z043.o -o build/probe_z043 >> hidden_files/z043_probe_build.log 2>&1 || { echo "BUILD FAILED (probe link)"; exit 2; }
cc $CFLAGS -DLEG_GLIBC -c tests/perf/probe_z043.c -o build/probe_z043_glibc.o 2>> hidden_files/z043_probe_build.log || { echo "BUILD FAILED (probe glibc)"; exit 2; }
cc $CFLAGS build/probe_z043_glibc.o -o build/probe_z043_glibc >> hidden_files/z043_probe_build.log 2>&1 || { echo "BUILD FAILED (probe glibc link)"; exit 2; }
# Load sampler for the perf window (Z-043 load discipline:
# loadavg + top co-tenant snapshots every 5 s, background).
SAMP=hidden_files/z043_load_samples.log
: > "$SAMP"
( while :; do
    echo "$(date -u +%H:%M:%S) $(cat /proc/loadavg)" >> "$SAMP"
    sleep 5
  done ) &
SAMP_PID=$!
trap 'kill $SAMP_PID 2>/dev/null' EXIT
PHASES="local32_wall ring32_wall ring32_lat pc16x16_lat queue31_enq queue31_total"
: > "$RAW"
for phase in $PHASES; do
  for rep in 1 2 3; do
    ./build/probe_z043 "$phase" > /dev/null 2>&1 || { echo "WARMUP FAILED $phase"; exit 2; }
  done
  for rep in 1 2 3 4 5 6 7; do
    ./build/probe_z043 "$phase" >> "$RAW" 2> hidden_files/z043_probe_err.log || { echo "PROBE FAILED $phase rep $rep"; cat hidden_files/z043_probe_err.log; exit 2; }
  done
done
# glibc comparator: latency phases only, separate raw file.
GRAW=hidden_files/z043_raw_glibc.csv
: > "$GRAW"
for phase in ring32_lat pc16x16_lat; do
  for rep in 1 2 3; do
    ./build/probe_z043_glibc "$phase" > /dev/null 2>&1 || { echo "WARMUP FAILED glibc $phase"; exit 2; }
  done
  for rep in 1 2 3 4 5 6 7; do
    ./build/probe_z043_glibc "$phase" >> "$GRAW" 2> hidden_files/z043_probe_err.log || { echo "PROBE FAILED glibc $phase rep $rep"; cat hidden_files/z043_probe_err.log; exit 2; }
  done
done
kill $SAMP_PID 2>/dev/null
trap - EXIT
echo "raw RESULT lines: $(grep -c '^RESULT' "$RAW") -> $RAW"
echo "glibc RESULT lines: $(grep -c '^RESULT' "$GRAW") -> $GRAW"
