#!/bin/sh
# run_z037.sh - Z-037 arena prototype runner (Z-019/Z-034 pattern).
# Builds the production objects and the arena objects
# (-DMIMIR_ARENA_CCD=1) into build/z37/ -- the production
# build/ objects are never touched -- plus:
#   (1) ZERO-COST PROOF: flag-off core/platform/api objects
#       built from this tree must be byte-identical (sha256)
#       to the same objects built from the pristine seat
#       tree named by $SEAT (default the campaign seat).
#   (2) FLAG-ON SUITE: all eight make-test binaries rebuilt
#       against the arena core and run individually; their
#       CHECKS/SMOKE counts are summed by the caller from
#       this runner's raw output (Z-034 precedent).
#   (3) PROBE: place on CCD0 (cpu 2) and CCD1 (cpu 10) and
#       an SMT sibling (cpu 18 -> CCD0), crossfree, storm.
# Usage: run_z037.sh <raw-output-file>
set -eu
OUT=${1:?usage: run_z037.sh <raw-output-file>}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
SEAT=${SEAT:-/home/volmarr/mimiralloc-z13-forge}
ZB="$ROOT/build/z37"
mkdir -p "$ZB"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2"
cd "$ROOT"
# Production + arena objects (this tree).
gcc $CFLAGS -c src/core/mimir_core.c -o "$ZB/core_prod.o"
gcc $CFLAGS -DMIMIR_ARENA_CCD=1 -c src/core/mimir_core.c -o "$ZB/core_arena.o"
gcc $CFLAGS -c src/api/mimir_alloc.c -o "$ZB/alloc.o"
gcc $CFLAGS -c src/platform/mimir_platform.c -o "$ZB/plat_prod.o"
gcc $CFLAGS -DMIMIR_ARENA_CCD=1 -c src/platform/mimir_platform.c -o "$ZB/plat_arena.o"
# Pristine seat objects (flag off) for the byte-identity proof.
mkdir -p "$ZB/seat"
gcc -std=c11 -Wall -Wextra -Werror -pthread -I"$SEAT/include" -I"$SEAT/src" -O2 -c "$SEAT/src/core/mimir_core.c" -o "$ZB/seat/core.o"
gcc -std=c11 -Wall -Wextra -Werror -pthread -I"$SEAT/include" -I"$SEAT/src" -O2 -c "$SEAT/src/platform/mimir_platform.c" -o "$ZB/seat/plat.o"
gcc -std=c11 -Wall -Wextra -Werror -pthread -I"$SEAT/include" -I"$SEAT/src" -O2 -c "$SEAT/src/api/mimir_alloc.c" -o "$ZB/seat/alloc.o"
# Probe (arena leg only).
gcc $CFLAGS -DMIMIR_ARENA_CCD=1 tests/perf/probe_z037.c "$ZB/alloc.o" "$ZB/core_arena.o" "$ZB/plat_arena.o" -o "$ZB/probe_arena"
{
    echo "=== ZERO-COST PROOF (flag-off objects vs pristine seat tree) ==="
    for pair in "core_prod.o seat/core.o CORE" "plat_prod.o seat/plat.o PLATFORM" "alloc.o seat/alloc.o API"; do
        set -- $pair
        a=$(sha256sum "$ZB/$1" | cut -d' ' -f1)
        b=$(sha256sum "$ZB/$2" | cut -d' ' -f1)
        if [ "$a" = "$b" ]; then echo "$3 IDENTICAL $a"; else echo "$3 DIFFER a=$a b=$b"; fi
    done
    echo "prod arena symbols (expect 0): $(nm "$ZB/core_prod.o" | grep -ci arena || true)"
    echo "arena arena symbols (expect >0): $(nm "$ZB/core_arena.o" | grep -ci arena || true)"
    echo "prod core bytes: $(wc -c < "$ZB/core_prod.o") arena core bytes: $(wc -c < "$ZB/core_arena.o")"
} > "$OUT.proof"
: > "$OUT"
{
    echo "=== PROBE place cpu=2 (CCD0) ==="
    "$ZB/probe_arena" place 2 64 200
    echo "=== PROBE place cpu=10 (CCD1) ==="
    "$ZB/probe_arena" place 10 64 200
    echo "=== PROBE place cpu=18 (SMT sibling of core 2 -> CCD0) ==="
    "$ZB/probe_arena" place 18 128 120
    echo "=== PROBE place cpu=26 (SMT sibling of core 10 -> CCD1) ==="
    "$ZB/probe_arena" place 26 128 120
    echo "=== PROBE crossfree (alloc CCD0, free CCD1) ==="
    "$ZB/probe_arena" crossfree
    echo "=== PROBE crossfree-rev (alloc CCD1, free CCD0) ==="
    "$ZB/probe_arena" crossfree-rev
    echo "=== PROBE storm (16 threads, 8 per CCD) ==="
    "$ZB/probe_arena" storm
} >> "$OUT" 2>&1 || { echo "probe FAILED - see $OUT" >&2; exit 1; }
# Flag-on suite: eight binaries against the arena core.
: > "$OUT.suite"
{
    for t in contract/test_smoke core/test_core core/test_b007 core/test_c005 core/test_c006 contract/test_c007 core/test_c008 concurrency/test_d004; do
        name=$(basename "$t")
        gcc $CFLAGS -DMIMIR_ARENA_CCD=1 "tests/$t.c" "$ZB/alloc.o" "$ZB/core_arena.o" "$ZB/plat_arena.o" -o "$ZB/suite_$name"
        echo "=== SUITE $name (flag on) ==="
        "$ZB/suite_$name" || { echo "SUITE $name FAILED rc=$?"; exit 1; }
    done
} >> "$OUT.suite" 2>&1 || { echo "flag-on suite FAILED - see $OUT.suite" >&2; exit 1; }
echo "raw written: $OUT (proof: $OUT.proof, suite: $OUT.suite)" >&2
