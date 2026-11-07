#!/bin/bash
# F-007 upstream-test runnability probe (Phase 6, task T5).
#
# Re-checks, cheaply, what F-007 established by execution (see
# tests/mojo/recorded_run_f007.txt and
# docs/PHASE_6_UPSTREAM_TESTS_F007.md): which relevant upstream
# Mojo test suites exist on this Pi and which of them the
# installed toolchain can actually execute. Precedent shape: the
# D-006 Android toolchain probe (make test-d006) - it reports
# PRESENT/ABSENT per component and exits 0 when the probe itself
# completes; the verdicts are the deliverable.
#
# F-007 findings this probe re-verifies:
#  - the installed published package ships NO test files;
#  - Mojo 1.1.0 has no `mojo test` subcommand, but `mojo run` on
#    a test file discovers and runs its test_ units, and the
#    documented -I flag supplies the test_utils package the
#    Bazel harness otherwise provides;
#  - a relevant subset of the 1.2.0.dev stdlib tests from the
#    operator-local checkout therefore RUNS against the
#    installed 1.1.0 stdlib (112 tests, all passed at F-007),
#    while the rest of the subset is compile-blocked by version
#    skew (1.2-era language/stdlib surface), and the lit and
#    C++ unit suites cannot run at all (no lit, no built tools).
# If a verdict flips (CHANGED lines), T5 must be re-opened.
#
# Exit: 0 = probe completed (any verdict). Non-zero only if the
# probe itself could not complete (e.g. mojo binary missing).
set -u
cd "$(dirname "$0")/../.." || exit 2

ENV_LIB=/home/pi/mojo-dev/.pixi/envs/default/lib/mojo
SRC=/home/pi/mojo-build/mojo-src
STDLIB_TEST=$SRC/Mojo/stdlib/test
PROBE_FILE=$STDLIB_TEST/bit/test_bit.mojo

echo "F-007 UPSTREAM-TEST RUNNABILITY PROBE"
echo "repo: $(pwd)"
echo "date: $(date -Iseconds)"

if ! command -v mojo >/dev/null 2>&1; then
  echo "PROBE-ERROR: mojo not on PATH - probe cannot complete"
  exit 2
fi
echo "mojo_version: $(mojo --version 2>&1)"

n_env=$(find $ENV_LIB -name 'test_*.mojo' 2>/dev/null | wc -l)
echo "env_shipped_test_mojo_files: $n_env"
if [ "$n_env" -eq 0 ]; then
  echo "VERDICT env_suite: ABSENT - the installed package ships no upstream test files (stdlib is the single compiled std.mojoc)"
else
  echo "CHANGED env_suite: $n_env test files now shipped - re-open T5"
fi

if mojo --help 2>&1 | grep -q '^        test '; then
  echo "CHANGED mojo_test_subcommand: PRESENT - re-open T5"
else
  echo "VERDICT mojo_test_subcommand: ABSENT - Mojo 1.1.0 has no test subcommand; mojo run is the test-file execution road"
fi

if [ -d "$SRC/.git" ]; then
  echo "checkout: $SRC"
  echo "checkout_head: $(git -C $SRC rev-parse HEAD 2>&1)"
  echo "checkout_dirty_entries: $(git -C $SRC status --porcelain 2>/dev/null | wc -l)"
  echo "checkout_stdlib_test_files: $(find $STDLIB_TEST -name 'test_*.mojo' 2>/dev/null | wc -l)"
else
  echo "CHANGED checkout: $SRC missing - re-derive the F-007 inventory"
fi

if [ -x "$SRC/bazel-bin/Mojo/tools/mojo/mojo" ]; then
  echo "CHANGED built_mojo_binary: PRESENT - re-open T5"
else
  echo "VERDICT built_mojo_binary: ABSENT - bazel-bin holds intermediates only; the source build failed (build.log), so the lit/C++ suites have no same-source toolchain"
fi

if command -v lit >/dev/null 2>&1 || python3 -c 'import lit' >/dev/null 2>&1; then
  echo "CHANGED lit_runner: PRESENT - re-open T5"
else
  echo "VERDICT lit_runner: ABSENT - no lit on PATH, python3 cannot import lit; the Mojo/test lit suites cannot run"
fi

out=$(timeout 60 mojo test "$PROBE_FILE" 2>&1)
rc=$?
echo "attempt_mojo_test: timeout 60 mojo test $PROBE_FILE"
echo "attempt_mojo_test_exit: $rc"
if [ $rc -eq 0 ]; then
  echo "CHANGED attempt_mojo_test: SUCCEEDED - re-open T5"
else
  echo "VERDICT attempt_mojo_test: FAILED (no such command) as recorded at F-007"
fi

if [ -f "$PROBE_FILE" ]; then
  out2=$(timeout 120 mojo run -I $STDLIB_TEST "$PROBE_FILE" 2>&1)
  rc2=$?
  echo "attempt_mojo_run: timeout 120 mojo run -I $STDLIB_TEST $PROBE_FILE"
  echo "attempt_mojo_run_exit: $rc2"
  echo "attempt_mojo_run_summary: $(echo "$out2" | grep -E 'Summary' | head -n 1)"
  if [ $rc2 -eq 0 ]; then
    echo "VERDICT attempt_mojo_run: RUNS - the F-007 runnable subset road (mojo run + -I) is live"
  else
    echo "CHANGED attempt_mojo_run: FAILED - the F-007 runnable road has closed; re-open T5"
  fi
fi

echo "F-007 PROBE RESULT: COMPLETE"
exit 0
