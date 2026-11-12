#!/usr/bin/env bash
# Z-030 runner: concurrency suite + perf probe.
# Builds the Z-030 suite and probe against the production
# library OBJECTS (test-only static link, the shape every
# internal surface in this tree uses), runs the suite
# (green build and, for the red-path proof, the
# MRQ_MUTANT_LOSSY build, which MUST fail), then runs the
# perf probe per the Z-004 protocol: 3 warm-up processes
# discarded + 7 measured per phase, one phase per process.
# NOT part of `make test`; the standing gate is untouched.
# Usage: tests/perf/run_z030.sh [raw_csv]
set -u
cd "$(dirname "$0")/../.."
RAW="${1:-hidden_files/z030_raw.csv}"
mkdir -p build hidden_files
LOG=hidden_files/z030_suite.log
{
  echo "# Z-030 suite: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "# host: $(uname -a)"
  echo "# head: $(git rev-parse HEAD)"
  echo "# loadavg: $(cat /proc/loadavg)"
} > "$LOG"
make build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o >> "$LOG" 2>&1 || { echo "BUILD FAILED (library objects)"; exit 2; }
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -Itests/concurrency -O2"
cc $CFLAGS -c tests/concurrency/test_z030_remote_queue.c -o build/test_z030_remote_queue.o >> "$LOG" 2>&1 || { echo "BUILD FAILED (suite)"; exit 2; }
cc $CFLAGS build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o build/test_z030_remote_queue.o -o build/test_z030_remote_queue >> "$LOG" 2>&1 || { echo "BUILD FAILED (suite link)"; exit 2; }
./build/test_z030_remote_queue >> "$LOG" 2>&1
SUITE_RC=$?
echo "# suite exit code: $SUITE_RC" >> "$LOG"
cat "$LOG"
if [ "$SUITE_RC" -ne 0 ]; then
  echo "SUITE FAILED"
  exit 1
fi
# Red-path proof: the lossy mutant MUST fail the same suite.
MLOG=hidden_files/z030_mutant.log
cc $CFLAGS -DMRQ_MUTANT_LOSSY -c tests/concurrency/test_z030_remote_queue.c -o build/test_z030_mutant.o >> "$MLOG" 2>&1 || { echo "BUILD FAILED (mutant)"; exit 2; }
cc $CFLAGS build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o build/test_z030_mutant.o -o build/test_z030_mutant >> "$MLOG" 2>&1 || { echo "BUILD FAILED (mutant link)"; exit 2; }
./build/test_z030_mutant >> "$MLOG" 2>&1
MUTANT_RC=$?
echo "# mutant exit code: $MUTANT_RC" >> "$MLOG"
if [ "$MUTANT_RC" -eq 0 ]; then
  echo "MUTANT DID NOT FAIL - suite proves nothing"
  cat "$MLOG"
  exit 1
fi
echo "red-path proof: mutant exit $MUTANT_RC (expected nonzero); log $MLOG"
# Perf probe.
cc $CFLAGS -c tests/perf/probe_z030.c -o build/probe_z030.o 2> hidden_files/z030_probe_build.log || { echo "BUILD FAILED (probe)"; exit 2; }
cc $CFLAGS build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o build/probe_z030.o -o build/probe_z030 >> hidden_files/z030_probe_build.log 2>&1 || { echo "BUILD FAILED (probe link)"; exit 2; }
PHASES="local_steady remote_same_direct remote_cross_direct remote_same_enq remote_cross_enq remote_same_total remote_cross_total swap_same_direct swap_cross_direct swap_same_enq swap_cross_enq swap_same_total swap_cross_total"
: > "$RAW"
for phase in $PHASES; do
  for rep in 1 2 3; do
    ./build/probe_z030 "$phase" > /dev/null 2>&1 || { echo "WARMUP FAILED $phase"; exit 2; }
  done
  for rep in 1 2 3 4 5 6 7; do
    ./build/probe_z030 "$phase" >> "$RAW" 2> hidden_files/z030_probe_err.log || { echo "PROBE FAILED $phase rep $rep"; cat hidden_files/z030_probe_err.log; exit 2; }
  done
done
echo "raw RESULT lines: $(grep -c '^RESULT' "$RAW") -> $RAW"
