#!/usr/bin/env bash
# MimirAlloc Z13 - Z-008: the standing correctness gate.
#
# Wires the suites as the Makefile organises them into ONE gate:
#   contract    : build/test_smoke (tests/contract/test_smoke.c)
#                 build/test_c007  (tests/contract/test_c007.c)
#   core        : build/test_core, build/test_b007, build/test_c005,
#                 build/test_c006  (tests/core/)
#   failure     : build/test_c006 (section 6.3 startup / error-path
#                 audit) and build/test_c008 (tests/core/test_c008.c,
#                 B-008 / roadmap S9.3 controlled-failure tests),
#                 both inside the same `make test` invocation; the
#                 fault-injection rows of test_c007 report as
#                 by-design SKIP lines.
#   concurrency : build/test_d004  (tests/concurrency/test_d004.c)
# All eight binaries are built and run by the single `make test`
# target; this script is the standing wrapper every later code
# slice must pass. The standalone Python suite tests/failure/
# test_failure_capture.py is NOT part of `make test` (no Makefile
# target runs it) and is therefore NOT wired here; it keeps its
# own standalone road.
#
# Usage: scripts/z13/run_gate.sh [LOGFILE]
#   LOGFILE defaults to hidden_files/gate_<UTC timestamp>.log under
#   the repo root; a relative LOGFILE is resolved against the repo
#   root. The full suite output plus a header and a counts footer
#   are written to the log.
#
# Exit: 0 = gate GREEN; 1 = gate RED (reasons shouted on stderr
# and recorded in the log footer). Native bash + coreutils only;
# no Docker, no new dependencies.

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

TS="$(date -u +%Y%m%dT%H%M%SZ)"
if [ $# -ge 1 ]; then
  case "$1" in
    /*) LOG="$1" ;;
    *)  LOG="$REPO_ROOT/$1" ;;
  esac
else
  LOG="$REPO_ROOT/hidden_files/gate_$TS.log"
fi
mkdir -p "$(dirname "$LOG")"

LOADAVG="$(cat /proc/loadavg 2>/dev/null || echo unknown)"

# ---- header (into the log) ----
{
  echo "# GATE Z-008 standing correctness gate"
  echo "# script: scripts/z13/run_gate.sh"
  echo "# date_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "# host: $(uname -a)"
  echo "# repo: $REPO_ROOT"
  echo "# head: $(git rev-parse HEAD 2>/dev/null || echo unknown)"
  echo "# branch: $(git branch --show-current 2>/dev/null || echo unknown)"
  echo "# loadavg_at_start: $LOADAVG"
  echo "# command: make clean, then make test"
  echo "# suites: contract=test_smoke,test_c007 core=test_core,test_b007,test_c005,test_c006 failure=test_c006(error-path),test_c008(controlled-failure) concurrency=test_d004"
} > "$LOG"

# ---- run the suite (fresh build: no stale objects) ----
make clean >> "$LOG" 2>&1
make test >> "$LOG" 2>&1
MAKE_EXIT=$?
{
  echo "# make exit code: $MAKE_EXIT"
  echo "# loadavg_at_end: $(cat /proc/loadavg 2>/dev/null || echo unknown)"
} >> "$LOG"

# ---- parse counts from the log ----
# SMOKE line format:  SMOKE 18/18 checks passed
# CHECKS line format: CHECKS: 29 run, 0 failed
#                     CHECKS: 42 run, 0 failed, 7 skipped-not-yet
# SKIP lines begin with "SKIP " (by-design skips named by the suite).
SMOKE_LINES=$(grep -c '^SMOKE ' "$LOG" || true)
CHECKS_LINES=$(grep -c '^CHECKS:' "$LOG" || true)
RESULT_FAIL=$(grep -c '^RESULT: FAIL' "$LOG" || true)
SKIPS=$(grep -c '^SKIP ' "$LOG" || true)

read -r SMOKE_PASSED SMOKE_TOTAL <<EOF
$(awk '/^SMOKE [0-9]+\/[0-9]+ checks passed/ { split($2, a, "/"); p = a[1]; t = a[2] } END { print p + 0, t + 0 }' "$LOG")
EOF

read -r CHECKS_RUN CHECKS_FAILED <<EOF
$(awk '/^CHECKS:/ { r += $2; f += $4 } END { print r + 0, f + 0 }' "$LOG")
EOF

CHECKS=$((SMOKE_TOTAL + CHECKS_RUN))
FAILED=$((SMOKE_TOTAL - SMOKE_PASSED + CHECKS_FAILED))

# By-design skip baseline on the founding tree: 77 (70 test_c007 +
# 7 test_c008), named by the suite itself. A modestly different
# skip count is shouted as a warning only, because later slices
# may legitimately change the suite; a skip balloon far beyond
# the baseline turns the gate red (see SKIP_BALLOON_LIMIT below).
# Failed checks, a failing RESULT, a nonzero make exit, or a
# truncated / composition-changed / collapsed log DO turn the
# gate red.
EXPECTED_SMOKE_LINES=1
EXPECTED_CHECKS_LINES=7
BY_DESIGN_SKIPS=77
# Low-water mark: a run parsing far below the founding tree's
# 4988 checks is a collapsed or silently shrunk suite, not a pass.
# Re-baseline this constant in the same commit as any intentional
# suite change that legitimately moves the total.
LOW_WATER_CHECKS=4500
# A skip balloon (far beyond the by-design baseline) hides
# regressions behind silent skips; modest drift only warns.
SKIP_BALLOON_LIMIT=$((BY_DESIGN_SKIPS + 20))

RED_REASONS=""
if [ "$MAKE_EXIT" -ne 0 ]; then
  RED_REASONS="$RED_REASONS make-exited-$MAKE_EXIT;"
fi
if [ "$FAILED" -ne 0 ]; then
  RED_REASONS="$RED_REASONS $FAILED-checks-failed;"
fi
if [ "$RESULT_FAIL" -ne 0 ]; then
  RED_REASONS="$RED_REASONS $RESULT_FAIL-RESULT-FAIL-lines;"
fi
if [ "$SMOKE_LINES" -ne "$EXPECTED_SMOKE_LINES" ] || [ "$CHECKS_LINES" -ne "$EXPECTED_CHECKS_LINES" ]; then
  RED_REASONS="$RED_REASONS suite-composition-mismatch(smoke-lines=$SMOKE_LINES,checks-lines=$CHECKS_LINES);"
fi
if [ "$CHECKS" -eq 0 ]; then
  RED_REASONS="$RED_REASONS zero-checks-parsed(log-truncated-or-build-broken);"
fi
if [ "$CHECKS" -ne 0 ] && [ "$CHECKS" -lt "$LOW_WATER_CHECKS" ]; then
  RED_REASONS="$RED_REASONS checks-collapse($CHECKS-below-low-water-$LOW_WATER_CHECKS);"
fi
if [ "$SKIPS" -gt "$SKIP_BALLOON_LIMIT" ]; then
  RED_REASONS="$RED_REASONS skip-balloon($SKIPS-above-limit-$SKIP_BALLOON_LIMIT);"
fi

# ---- footer (into the log) + report ----
{
  echo "# GATE COUNTS: checks=$CHECKS failed=$FAILED skips=$SKIPS"
  echo "# GATE DETAIL: smoke=$SMOKE_PASSED/$SMOKE_TOTAL checks-lines=$CHECKS_LINES result-fail-lines=$RESULT_FAIL make-exit=$MAKE_EXIT"
  if [ "$SKIPS" -ne "$BY_DESIGN_SKIPS" ]; then
    echo "# GATE WARNING: skips=$SKIPS differs from the by-design baseline $BY_DESIGN_SKIPS on the founding tree"
  fi
} >> "$LOG"

echo "gate log: $LOG"
echo "GATE COUNTS: checks=$CHECKS failed=$FAILED skips=$SKIPS"

if [ -n "$RED_REASONS" ]; then
  echo "# GATE RESULT: RED ($RED_REASONS)" >> "$LOG"
  echo "==================================================" >&2
  echo "GATE RED: the standing correctness gate FAILED" >&2
  echo "reasons:$RED_REASONS" >&2
  echo "counts: checks=$CHECKS failed=$FAILED skips=$SKIPS" >&2
  echo "log: $LOG" >&2
  echo "==================================================" >&2
  exit 1
fi

if [ "$SKIPS" -ne "$BY_DESIGN_SKIPS" ]; then
  echo "GATE WARNING: skips=$SKIPS differs from the by-design baseline $BY_DESIGN_SKIPS" >&2
fi
echo "# GATE RESULT: GREEN" >> "$LOG"
echo "GATE RESULT: GREEN"
exit 0
