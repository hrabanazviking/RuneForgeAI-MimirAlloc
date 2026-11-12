#!/bin/sh
# run_z070.sh - Z-070 double-free detection economics
# runner (Z-004 protocol, Z-034 runner pattern).
# Builds the core five ways into build/z70/ (production
# build/ objects never touched):
#   off     -DMIMIR_DFREE_GUARD=0  (the escape hatch)
#   off2    same flags as off      (A/A identical-leg
#                                  control for drift)
#   member  -DMIMIR_DFREE_POISON=0 (membership only)
#   poison  -DMIMIR_DFREE_MEMBER=0 (poison only)
#   full    default                (the adopted build)
# plus the escape-hatch evidence (object bytes + dfree
# symbol counts), then runs probe_z070 pinned to core 2
# (taskset -c 2), 3 warm-up processes per workload
# discarded, 7 recorded processes per leg, legs
# interleaved in rotating order within each round.
# Usage: run_z070.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_z070.sh <raw-output-file> [reps]}
REPS=${2:-7}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z70"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
gcc $CFLAGS -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/alloc.o"
gcc $CFLAGS -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/plat.o"
gcc $CFLAGS -DMIMIR_DFREE_GUARD=0 -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core_off.o"
gcc $CFLAGS -DMIMIR_DFREE_GUARD=0 -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core_off2.o"
gcc $CFLAGS -DMIMIR_DFREE_POISON=0 -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core_member.o"
gcc $CFLAGS -DMIMIR_DFREE_MEMBER=0 -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core_poison.o"
gcc $CFLAGS -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core_full.o"
for LEG in off off2 member poison full; do
    gcc $CFLAGS "$ROOT/tests/perf/probe_z070.c" \
        "$ZB/alloc.o" "$ZB/core_$LEG.o" "$ZB/plat.o" \
        -o "$ZB/probe_$LEG"
done
{
    echo "=== ESCAPE-HATCH EVIDENCE (core objects) ==="
    for LEG in off member poison full; do
        echo "core_$LEG bytes: $(wc -c < "$ZB/core_$LEG.o")"
        echo "core_$LEG dfree symbols: $(nm "$ZB/core_$LEG.o" | grep -ci dfree || true)"
        echo "core_$LEG poison symbols: $(nm "$ZB/core_$LEG.o" | grep -ci poison || true)"
    done
    echo "public dynamic symbols (expect 4):"
    echo "(checked separately against the built library;"
    echo " the probe links objects directly)"
} > "$OUT.proof"
: > "$OUT"
# workload list: mode class n rounds
WORKLOADS="
churn1 16 100000 0
churn1 64 100000 0
churn1 256 100000 0
churn1 1024 100000 0
churnN 16 250 2000
churnN 64 62 4000
churnF 16 250 2000
churnF 64 62 4000
xfree 16 16 2000
xfree 16 62 1500
xfree 16 250 600
"
echo "$WORKLOADS" | while read -r MODE CLASS N ROUNDS; do
    [ -n "$MODE" ] || continue
    i=0
    while [ "$i" -lt 3 ]; do
        for LEG in off off2 member poison full; do
            if [ "$MODE" = churn1 ]; then
                taskset -c 2 "$ZB/probe_$LEG" churn1 "$CLASS" "$N" > /dev/null
            else
                taskset -c 2 "$ZB/probe_$LEG" "$MODE" "$CLASS" "$N" "$ROUNDS" > /dev/null
            fi
        done
        i=$((i + 1))
    done
    r=0
    while [ "$r" -lt "$REPS" ]; do
        # rotate leg order each round (interleaving)
        case $((r % 5)) in
            0) ORDER="off off2 member poison full" ;;
            1) ORDER="member poison full off off2" ;;
            2) ORDER="full off off2 member poison" ;;
            3) ORDER="poison full off off2 member" ;;
            *) ORDER="off2 member poison full off" ;;
        esac
        for LEG in $ORDER; do
            echo "=== RUN leg=$LEG mode=$MODE class=$CLASS n=$N rep=$r ===" >> "$OUT"
            if [ "$MODE" = churn1 ]; then
                taskset -c 2 "$ZB/probe_$LEG" churn1 "$CLASS" "$N" >> "$OUT"
            else
                taskset -c 2 "$ZB/probe_$LEG" "$MODE" "$CLASS" "$N" "$ROUNDS" >> "$OUT"
            fi
        done
        r=$((r + 1))
    done
    echo "phase done: $MODE $CLASS $N" >&2
done
echo "raw written: $OUT (proof: $OUT.proof)" >&2
