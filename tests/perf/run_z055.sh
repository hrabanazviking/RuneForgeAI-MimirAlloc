#!/bin/sh
# run_z055.sh - Z-055 fragmentation oracle runner.
#
# Builds the ported Phase-9 F-series oracle
# (tests/perf/probe_z055.c) against the library OBJECTS
# (the test_i006 link shape: the probe reads the G-003 /
# E-005 audit counters and the I-003 trim surface, which
# only exist when linking the objects directly) and runs
# it ONCE: every graded quantity in the probe is an exact
# counter value at a quiesced point, not a timing sample,
# so one clean run is the run of record (wall times in
# the probe are timing-never-an-oracle and are reported,
# never asserted). The F-6 children fork from the probe
# process itself.
#
# Usage: run_z055.sh <raw-output-file>
set -eu
OUT=${1:?usage: run_z055.sh <raw-output-file>}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z55"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"

gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/alloc.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/plat.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -I"$ROOT/tests/core" "$ROOT/tests/perf/probe_z055.c" "$ZB/alloc.o" "$ZB/core.o" "$ZB/plat.o" -o "$ZB/probe_z055"

"$ZB/probe_z055" > "$OUT" 2>&1 || true
echo "run_z055: raw=$OUT" >&2
tail -3 "$OUT" >&2
