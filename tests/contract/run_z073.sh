#!/usr/bin/env bash
# Z-073 aligned-allocation contract runner.
# Builds the contract suite against the production library
# OBJECTS (test-only static link, the shape every internal
# surface in this tree uses) and runs it. NOT part of
# `make test`; the standing gate (scripts/z13/run_gate.sh)
# is untouched by this slice.
# Expected on the banked pre-Z-074 core: RED by design -
# CHECKS: 130 run, 125 failed, RESULT: FAIL, exit 1
# (the 5 P7 public-regression checks pass; every aligned-
# surface check fails because the surface does not exist).
# Expected on the Z-074 core: CHECKS: 130 run, 0 failed,
# RESULT: PASS, exit 0, with this file and the suite
# unmodified.
set -u
cd "$(dirname "$0")/../.."
mkdir -p build hidden_files
LOG=hidden_files/z073_contract_run.log
{
  echo "# Z-073 contract run: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "# host: $(uname -a)"
  echo "# head: $(git rev-parse HEAD)"
  echo "# loadavg: $(cat /proc/loadavg)"
} > "$LOG"
make build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o >> "$LOG" 2>&1 || { echo "BUILD FAILED (library objects)" | tee -a "$LOG"; exit 2; }
cc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 -c tests/contract/test_z073_aligned_contract.c -o build/test_z073_aligned_contract.o >> "$LOG" 2>&1 || { echo "BUILD FAILED (contract suite)" | tee -a "$LOG"; exit 2; }
cc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o build/test_z073_aligned_contract.o -o build/test_z073_aligned_contract >> "$LOG" 2>&1 || { echo "BUILD FAILED (link)" | tee -a "$LOG"; exit 2; }
./build/test_z073_aligned_contract >> "$LOG" 2>&1
RC=$?
echo "# contract exit code: $RC" >> "$LOG"
cat "$LOG"
exit $RC
