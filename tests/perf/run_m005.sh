#!/bin/sh
# run_m005.sh - M-005 Phase 13 soak driver. Builds probe_m005
# (production flavour, object-linked; Makefile untouched -
# the standalone runner is the entry point, see slice doc)
# and build/libmimiralloc.so via make lib, then runs the
# bounded soak: main leg timeout 900s, dl leg timeout 300s.
# Samtal :8787 sampled every ~2s for the whole run into
# /tmp/m005_samtal.txt; summary plus any non-200 lines are
# printed verbatim at the end, never hidden. free -g is
# recorded at start and end.
set -u
cd "$(dirname "$0")/../.." || exit 2
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
echo "=== M-005 recorded run ==="
date -Is; uname -m; git rev-parse HEAD
echo "--- free -g (start) ---"; free -g
echo "--- build libmimiralloc.so ---"
make lib > /tmp/m005_lib.log 2>&1; echo "make lib exit=$?"
nm -D --defined-only build/libmimiralloc.so | tee /tmp/m005_nm.txt
echo "nm -D defined count=$(wc -l < /tmp/m005_nm.txt)"
echo "--- build probe_m005 ---"
cc $CFLAGS tests/perf/probe_m005.c src/api/mimir_alloc.c src/core/mimir_core.c src/platform/mimir_platform.c -o /tmp/probe_m005 -ldl || exit 1
echo "build exit=0"
sha256sum tests/perf/probe_m005.c tests/perf/run_m005.sh /tmp/probe_m005 build/libmimiralloc.so
: > /tmp/m005_samtal.txt
( while :; do S=$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 http://127.0.0.1:8787/ 2>/dev/null); [ -z "$S" ] && S=000; printf '%s %s\n' "$(date +%H:%M:%S)" "$S" >> /tmp/m005_samtal.txt; sleep 2; done ) &
SAMP=$!
T0=$(date +%s)
echo "--- main leg (timeout 900) ---"
timeout 900 /tmp/probe_m005 main; RC1=$?
echo "main leg exit=$RC1"
echo "--- dl leg (timeout 300) ---"
timeout 300 /tmp/probe_m005 dl build/libmimiralloc.so; RC2=$?
echo "dl leg exit=$RC2"
T1=$(date +%s)
kill $SAMP 2>/dev/null; wait $SAMP 2>/dev/null
echo "soak_wall_s=$((T1-T0))"
awk '{n++; if($2!=200) bad++} END{printf "samtal samples=%d non200=%d\n", n, bad+0}' /tmp/m005_samtal.txt
echo "--- samtal non-200 samples (verbatim) ---"
awk '$2!=200' /tmp/m005_samtal.txt
echo "--- free -g (end) ---"; free -g
echo "--- run_m005 done rc1=$RC1 rc2=$RC2 ---"
[ "$RC1" = 0 ] && [ "$RC2" = 0 ]
