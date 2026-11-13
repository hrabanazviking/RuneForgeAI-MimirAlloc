#!/usr/bin/env bash
# Z-092 batch API prototype contract runner.
# Builds the contract suite against the production library
# OBJECTS (test-only static link, the shape every internal
# surface in this tree uses), runs it, and proves the
# roadmap gate's second clause: the prototype is NOT a
# public symbol (nm -D on the shared library shows exactly
# the 13 public exports and no mimir_batch_* name).
# NOT part of `make test`; the standing gate
# (scripts/z13/run_gate.sh) is untouched by this slice.
# Expected on the Z-092 tree: CHECKS: 62 run, 0 failed,
# RESULT: PASS, exit 0, NM-PROOF: PASS.
set -u
cd "$(dirname "$0")/../.."
mkdir -p build hidden_files
LOG=hidden_files/z092_contract_run.log
{
  echo "# Z-092 contract run: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "# host: $(uname -a)"
  echo "# head: $(git rev-parse HEAD)"
  echo "# loadavg: $(cat /proc/loadavg)"
} > "$LOG"
make build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o >> "$LOG" 2>&1 || { echo "BUILD FAILED (library objects)" | tee -a "$LOG"; exit 2; }
cc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -Itests/contract -O2 -c tests/contract/test_z092_batch_contract.c -o build/test_z092_batch_contract.o >> "$LOG" 2>&1 || { echo "BUILD FAILED (contract suite)" | tee -a "$LOG"; exit 2; }
cc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o build/test_z092_batch_contract.o -o build/test_z092_batch_contract >> "$LOG" 2>&1 || { echo "BUILD FAILED (link)" | tee -a "$LOG"; exit 2; }
./build/test_z092_batch_contract >> "$LOG" 2>&1
RC=$?
echo "# contract exit code: $RC" >> "$LOG"
make lib >> "$LOG" 2>&1 || { echo "BUILD FAILED (shared library)" | tee -a "$LOG"; exit 2; }
EXPORTS=$(nm -D --defined-only build/libmimiralloc.so | grep -c ' T ')
BATCHEXP=$(nm -D --defined-only build/libmimiralloc.so | grep -c mimir_batch || true)
echo "# nm -D defined T exports: $EXPORTS (expected 13)" >> "$LOG"
echo "# nm -D mimir_batch exports: $BATCHEXP (expected 0)" >> "$LOG"
if [ "$EXPORTS" -eq 13 ] && [ "$BATCHEXP" -eq 0 ]; then
  echo "NM-PROOF: PASS (prototype is not a public symbol)" >> "$LOG"
  NMRC=0
else
  echo "NM-PROOF: FAIL" >> "$LOG"
  NMRC=1
fi
cat "$LOG"
if [ "$RC" -ne 0 ]; then exit $RC; fi
exit $NMRC
