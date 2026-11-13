#!/bin/sh
# run_z112.sh - Z-112 huge-page-only large-pool variant
# runner. Standalone host probe (Z-049 pattern): compiles
# tests/perf/probe_z112.c directly into build/z112/ with
# the repo flags - no Makefile target, NOT part of make
# test, the production build/ tree is never touched.
# One execution per invocation; the recorded run is three
# consecutive executions assembled by the lead.
# Usage: run_z112.sh
set -eu
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z112"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
gcc $CFLAGS "$ROOT/tests/perf/probe_z112.c" -o "$ZB/probe_z112"
sha256sum "$ROOT/tests/perf/probe_z112.c" \
    "$ROOT/tests/perf/run_z112.sh" \
    "$ROOT/tests/perf/summarize_z112.py" \
    "$ZB/probe_z112" >&2
taskset -c 5 "$ZB/probe_z112"
