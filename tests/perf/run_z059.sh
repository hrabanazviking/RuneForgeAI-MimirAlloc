#!/bin/sh
# run_z059.sh - Z-059 placement policy trial runner.
#
# Builds the Z-059 probe (tests/perf/probe_z059.c) against
# the library OBJECTS three times, once per placement
# policy (current = no flag, first-fit =
# -DMIMIR_PLACEMENT_FIRSTFIT, best-fit =
# -DMIMIR_PLACEMENT_BESTFIT), each build from scratch in
# its own directory, and runs each binary THREE times:
# the graded quantities are exact counters at quiesced
# points, so the three runs of one policy must agree
# bit-for-bit on every METRIC line (determinism proof,
# checked by tests/perf/summarize_z059.py). sha256 of
# each policy's core object and probe binary is recorded
# in hashes.txt so the three builds are provably distinct
# objects. Load averages are recorded before and after
# every run (shared host; the graded counters are exact
# and load-invariant, the load lines are the honest
# record, not a quiet-machine claim).
#
# Usage: run_z059.sh <output-dir>
# Exit: 0 iff every probe run exited 0.
set -eu
OUT=${1:?usage: run_z059.sh <output-dir>}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
mkdir -p "$OUT"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
FAIL=0
: > "$OUT/hashes.txt"
for policy in current first best; do
  case $policy in
    current) DEF="" ;;
    first)   DEF="-DMIMIR_PLACEMENT_FIRSTFIT" ;;
    best)    DEF="-DMIMIR_PLACEMENT_BESTFIT" ;;
  esac
  ZB="$ROOT/build/z59/$policy"
  rm -rf "$ZB"
  mkdir -p "$ZB"
  gcc $CFLAGS $DEF -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core.o"
  gcc $CFLAGS $DEF -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/alloc.o"
  gcc $CFLAGS $DEF -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/plat.o"
  gcc $CFLAGS $DEF -I"$ROOT/include" -I"$ROOT/src" -I"$ROOT/tests/core" "$ROOT/tests/perf/probe_z059.c" "$ZB/alloc.o" "$ZB/core.o" "$ZB/plat.o" -o "$ZB/probe_z059"
  sha256sum "$ZB/core.o" "$ZB/probe_z059" >> "$OUT/hashes.txt"
  run=1
  while [ $run -le 3 ]; do
    RAW="$OUT/raw_${policy}_run${run}.txt"
    {
      echo "# policy_build=$policy def='${DEF}'"
      echo "# loadavg_before: $(cat /proc/loadavg)"
    } > "$RAW"
    if "$ZB/probe_z059" >> "$RAW" 2>&1; then
      echo "# probe_exit=0" >> "$RAW"
    else
      echo "# probe_exit=$?" >> "$RAW"
      FAIL=1
    fi
    echo "# loadavg_after: $(cat /proc/loadavg)" >> "$RAW"
    run=$((run + 1))
  done
done
echo "run_z059: out=$OUT fail=$FAIL" >&2
[ "$FAIL" -eq 0 ]
