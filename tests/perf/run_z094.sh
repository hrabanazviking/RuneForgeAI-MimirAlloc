#!/usr/bin/env bash
# Z-094 verification runner (make test-z094): proves the
# gate - three traces in-tree with replay checksums
# matching - plus the capture pipeline's self-test.
#
#   S1 build the tracer shim and the namespaced library
#   S2 smoke: scripted consumer under the shim ->
#      summarise -> replay on glibc AND mimir legs, both
#      checksums must MATCH (pipeline truth, fresh)
#   S3 for each banked trace: gzip integrity, header
#      present, replay on glibc AND mimir legs, both
#      checksums must MATCH the header
# NOT part of `make test`; the standing gate is untouched.
set -u
cd "$(dirname "$0")/../.."
REPO="$PWD"
mkdir -p build hidden_files
LOG=hidden_files/z094_replay_run.log
SMOKEWORK="$(mktemp -d /tmp/z094_smoke.XXXXXX)"
FAIL=0
{
  echo "# Z-094 replay run: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "# host: $(uname -a)"
  echo "# head: $(git rev-parse HEAD)"
  echo "# loadavg: $(cat /proc/loadavg)"
} > "$LOG"

echo "S1 build" >> "$LOG"
make trace-lib-z094 >> "$LOG" 2>&1 || { echo "BUILD FAILED (shim)" | tee -a "$LOG"; exit 2; }
make lib >> "$LOG" 2>&1 || { echo "BUILD FAILED (lib)" | tee -a "$LOG"; exit 2; }
cc -std=c11 -Wall -Wextra -Werror -O2 -fno-builtin \
  tests/perf/test_z094_trace_smoke.c -o build/test_z094_trace_smoke \
  >> "$LOG" 2>&1 || { echo "BUILD FAILED (smoke)" | tee -a "$LOG"; exit 2; }

echo "S2 smoke pipeline" >> "$LOG"
mkdir -p "$SMOKEWORK/raw"
MIMIR_TRACE_DIR="$SMOKEWORK/raw" \
  LD_PRELOAD="$REPO/build/libmimirtrace_z094.so" \
  ./build/test_z094_trace_smoke >> "$LOG" 2>&1
SMOKERC=$?
echo "# smoke consumer exit: $SMOKERC" >> "$LOG"
[ "$SMOKERC" -eq 0 ] || FAIL=1
if python3 tests/perf/summarize_z094.py --workload smoke \
    --command "tests/perf/test_z094_trace_smoke.c under shim" \
    --captured "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
    --loadavg "$(cat /proc/loadavg)" \
    --out "$SMOKEWORK/smoke.trace.gz" \
    "$SMOKEWORK"/raw/trace.*.raw >> "$LOG" 2>&1; then
  for LEG in glibc mimir; do
    if python3 tests/perf/replay_z094.py "$SMOKEWORK/smoke.trace.gz" \
        --leg "$LEG" >> "$LOG" 2>&1; then
      echo "SMOKE REPLAY $LEG: MATCH" >> "$LOG"
    else
      echo "SMOKE REPLAY $LEG: MISMATCH" >> "$LOG"; FAIL=1
    fi
  done
else
  echo "SMOKE SUMMARIZE FAILED" >> "$LOG"; FAIL=1
fi

echo "S3 banked traces" >> "$LOG"
for T in tests/perf/traces/z094_python_import.trace.gz \
         tests/perf/traces/z094_git_churn.trace.gz \
         tests/perf/traces/z094_ollama_session.trace.gz; do
  if [ ! -f "$T" ]; then
    echo "TRACE MISSING: $T" >> "$LOG"; FAIL=1; continue
  fi
  gzip -t "$T" 2>> "$LOG" || { echo "TRACE CORRUPT: $T" >> "$LOG"; FAIL=1; continue; }
  echo "TRACE $T sha256=$(sha256sum "$T" | awk '{print $1}') bytes=$(stat -c%s "$T")" >> "$LOG"
  for LEG in glibc mimir; do
    if timeout 1200 python3 tests/perf/replay_z094.py "$T" \
        --leg "$LEG" >> "$LOG" 2>&1; then
      echo "TRACE REPLAY $T $LEG: MATCH" >> "$LOG"
    else
      echo "TRACE REPLAY $T $LEG: MISMATCH" >> "$LOG"; FAIL=1
    fi
  done
done

echo "# loadavg_end: $(cat /proc/loadavg)" >> "$LOG"
if [ "$FAIL" -eq 0 ]; then
  echo "Z-094 RESULT: PASS" >> "$LOG"
else
  echo "Z-094 RESULT: FAIL" >> "$LOG"
fi
cat "$LOG"
exit "$FAIL"
