#!/bin/sh
# run_anatomy_z022.sh - Z-022 small-path anatomy runner.
#
# Roadmap gate (Z-022): perf record/report of the mimir leg
# on small_churn - top hot spots by cycles, each mapped to
# a source line region. This runner builds the anatomy
# probe into build/z22/ (production build/ objects are
# never touched) from the PRODUCTION core sources with the
# production flags plus -g (debug info only; the .text
# identity proof below shows -g changes no code), then:
#
#   TIMING legs (Z-004 protocol, docs/Z13_MEASUREMENT_
#   PROTOCOL.md): pinned taskset -c 2 (CCD0 physical core),
#   3 warm-up processes discarded, 7 measured processes of
#   2,000,000 ops each; headline = median ns/op across
#   processes (summarize_z022.py).
#   PERF legs: 3 separate `perf record` processes (separate
#   processes per the protocol; shares are the median
#   across the 3 reports), pinned taskset -c 2, cycles
#   event at -F 4000, 50,000,000 ops each. perf needs root
#   on this host (perf_event_paranoid=4, Z-005 precedent):
#   invoked via sudo -n; no host setting is changed.
#   Reports: flat by symbol AND by source line (srcline),
#   children excluded (flat profile: a cycle is charged to
#   the function/line executing it).
#
# Only the mimir leg exists in this slice (the roadmap
# scopes Z-022 to the mimir leg), so there is no leg
# interleave; drift is bounded by the timing legs' spread
# and the cross-report share stability, both printed by
# the summariser.
#
# Usage: run_anatomy_z022.sh <raw-output-file>
set -eu
OUT=${1:?usage: run_anatomy_z022.sh <raw-output-file>}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z22"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"

# -g build (anatomy) + no-g build (.text identity proof).
gcc $CFLAGS -g -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core_g.o"
gcc $CFLAGS -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core_nog.o"
gcc $CFLAGS -g -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/mimir_alloc.o"
gcc $CFLAGS -g -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/mimir_platform.o"
gcc $CFLAGS -g "$ROOT/tests/perf/probe_z022.c" \
    "$ZB/mimir_alloc.o" "$ZB/mimir_core_g.o" "$ZB/mimir_platform.o" \
    -o "$ZB/probe_z022"

TEXT_G=$(objcopy --dump-section .text=/dev/stdout "$ZB/mimir_core_g.o" 2>/dev/null | sha256sum | cut -d' ' -f1)
TEXT_NOG=$(objcopy --dump-section .text=/dev/stdout "$ZB/mimir_core_nog.o" 2>/dev/null | sha256sum | cut -d' ' -f1)

: > "$OUT"
{
    echo "=== Z22 BUILD ==="
    echo "core object (-g) bytes: $(wc -c < "$ZB/mimir_core_g.o")"
    echo "core object (no -g) bytes: $(wc -c < "$ZB/mimir_core_nog.o")"
    echo "core .text sha256 (-g):    $TEXT_G"
    echo "core .text sha256 (no -g): $TEXT_NOG"
    if [ "$TEXT_G" = "$TEXT_NOG" ]; then
        echo "text_identity: IDENTICAL (-g changes no code)"
    else
        echo "text_identity: DIFFER - investigate before trusting"
    fi
    echo "=== Z22 TIMING (taskset -c 2, 3 warmups discarded, 7 x 2000000 ops) ==="
} >> "$OUT"

i=0
while [ "$i" -lt 3 ]; do
    taskset -c 2 "$ZB/probe_z022" 2000000 > /dev/null
    i=$((i + 1))
done
r=0
while [ "$r" -lt 7 ]; do
    taskset -c 2 "$ZB/probe_z022" 2000000 >> "$OUT"
    r=$((r + 1))
done

echo "=== Z22 PERF (sudo -n perf record -F 4000 cycles, taskset -c 2, 3 x 50000000 ops) ===" >> "$OUT"
p=1
while [ "$p" -le 3 ]; do
    PD="$ZB/perf$p.data"
    sudo -n perf record -F 4000 -o "$PD" -- \
        taskset -c 2 "$ZB/probe_z022" 50000000 > "$ZB/perf$p.probe.log" 2>&1
    {
        echo "--- perf rep $p probe line ---"
        cat "$ZB/perf$p.probe.log"
        echo "--- perf rep $p report by symbol ---"
        sudo -n perf report -i "$PD" --stdio --no-children \
            --percent-limit 0.3 --sort symbol,dso 2>/dev/null
        echo "--- perf rep $p report by srcline ---"
        sudo -n perf report -i "$PD" --stdio --no-children \
            --percent-limit 0.3 --sort srcline 2>/dev/null
    } >> "$OUT"
    echo "perf rep $p done" >&2
    p=$((p + 1))
done
echo "raw sections: $(grep -c '^=== Z22' "$OUT") (expect 3)" >&2
