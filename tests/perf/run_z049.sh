#!/bin/sh
# run_z049.sh - Z-049 THP for cached mappings runner.
# Standalone host probe (Z-013 pattern, Z-042 runner pattern):
# compiles tests/perf/probe_z049.c directly into build/z49/
# with the repo flags - no Makefile target, NOT part of
# make test, the production build/ tree is never touched.
# One execution per invocation; the recorded run is three
# consecutive executions assembled by the lead.
# Usage: run_z049.sh
set -eu
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z49"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
gcc $CFLAGS "$ROOT/tests/perf/probe_z049.c" -o "$ZB/probe_z049"
sha256sum "$ROOT/tests/perf/probe_z049.c" \
    "$ROOT/tests/perf/run_z049.sh" \
    "$ROOT/tests/perf/summarize_z049.py" \
    "$ZB/probe_z049" >&2
"$ZB/probe_z049"
