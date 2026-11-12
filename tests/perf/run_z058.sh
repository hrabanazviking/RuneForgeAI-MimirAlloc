#!/bin/sh
# run_z058.sh - Z-058 split/coalesce audit runner (z055 shape).
set -eu
OUT=${1:?usage: run_z058.sh <raw-output-file>}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z58"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/alloc.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/plat.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -I"$ROOT/tests/core" "$ROOT/tests/perf/probe_z058.c" "$ZB/alloc.o" "$ZB/core.o" "$ZB/plat.o" -o "$ZB/probe_z058"
"$ZB/probe_z058" > "$OUT" 2>&1 || true
echo "run_z058: raw=$OUT" >&2
tail -3 "$OUT" >&2
