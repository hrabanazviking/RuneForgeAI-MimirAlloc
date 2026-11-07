#!/bin/sh
# run_l008.sh - L-008 tuning A/B driver. Builds probe_l008
# against library sources (L-004 pattern; internal tune +
# stats surfaces are object-link only; Makefile untouched).
# A/B: fresh-process reps per retention bound (2, 0, 16),
# order ROTATED per rep so no leg always runs first/cold.
# Debug flavour (-DMIMIR_DEBUG=1, the L-004..L-006 profile
# flavour): 5 reps. Production flavour (no debug): 3 reps -
# it isolates the retention effect from the J-005 debug
# quarantine, which pins spans in the debug flavour.
# Each process: 1 warm-up + 7 timed iters per workload,
# median printed. During the WHOLE sweep (the heaviest
# leg) Samtal :8787 is sampled by a sequential curl loop;
# density is whatever that loop achieves and is reported
# exactly - no bound claimed beyond the samples taken.
# Bounded: timeout on every run, no soak.
set -u
cd "$(dirname "$0")/../.." || exit 2
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
echo "--- build probe_l008 (debug flavour) ---"
cc $CFLAGS -DMIMIR_DEBUG=1 tests/perf/probe_l008.c src/api/mimir_alloc.c src/core/mimir_core.c src/platform/mimir_platform.c -o /tmp/probe_l008 || exit 1
echo "--- build probe_l008_prod (production flavour) ---"
cc $CFLAGS tests/perf/probe_l008.c src/api/mimir_alloc.c src/core/mimir_core.c src/platform/mimir_platform.c -o /tmp/probe_l008_prod || exit 1
echo "build exits=0"
sha256sum tests/perf/probe_l008.c /tmp/probe_l008 /tmp/probe_l008_prod
: > /tmp/l008_samtal.txt
( while :; do curl -s -o /dev/null -w "%{http_code} %{time_total}\n" --max-time 2 http://127.0.0.1:8787/ >> /tmp/l008_samtal.txt 2>/dev/null || echo "000 0" >> /tmp/l008_samtal.txt; done ) &
SAMP=$!
rc=0; rep=1
for order in "2 0 16" "0 16 2" "16 2 0" "2 0 16" "0 16 2"; do
 for b in $order; do echo "--- debug rep $rep bound $b ---"; timeout 60 /tmp/probe_l008 $b || rc=1; done; rep=$((rep+1))
done
rep=1
for order in "2 0 16" "0 16 2" "16 2 0"; do
 for b in $order; do echo "--- prod rep $rep bound $b ---"; timeout 60 /tmp/probe_l008_prod $b || rc=1; done; rep=$((rep+1))
done
kill $SAMP 2>/dev/null; wait $SAMP 2>/dev/null
awk "{n++; if(\$1!=200) bad++; s+=\$2; if(\$2>m) m=\$2} END{printf \"samtal samples=%d non200=%d mean_s=%.4f max_s=%.4f\\n\", n, bad+0, (n?s/n:0), m+0}" /tmp/l008_samtal.txt
echo "--- run_l008 done rc=$rc ---"
exit $rc
