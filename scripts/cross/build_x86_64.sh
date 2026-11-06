#!/bin/sh
# MimirAlloc D-005 - full x86-64 cross build + suite run.
#
# STATUS: PROPOSED - BUILD PATH UNEXECUTED (2026-11-06).
# On the Pi today this script stops at its preflight: the
# x86-64 cross compiler it needs is ABSENT (verified by
# slice D-005; the preflight refusal itself was run and is
# recorded in tests/cross/recorded_run_d005.txt, exit 3).
# The build/run path below has never executed on the Pi.
# It becomes runnable after the smallest next step in
# docs/PHASE_4_CROSS_COMPILATION_AND_CI.md (install the
# Debian packages gcc-x86-64-linux-gnu and, for the run
# leg, qemu-user - or rely on the already-present Box64).
#
# When a compiler IS present, this script:
#   1. rebuilds the allocator + portable suite for
#      x86_64-linux-gnu into build-x86_64/ (never touching
#      the native build/ tree),
#   2. verifies every produced test binary is ELF64 x86-64
#      with `file` (a silent native build does not pass),
#   3. runs the suite under the first available runner -
#      qemu-x86_64, then box64 - and records the run to
#      tests/cross/recorded_run_x86_64_suite.txt.
# A cross-compile without step 3 is NOT a suite run and
# must never be reported as one (roadmap Phase 4 gate).
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"
CROSS_CC=${CROSS_CC:-x86_64-linux-gnu-gcc}
MISSING=""
command -v "$CROSS_CC" >/dev/null 2>&1 || MISSING="$MISSING compiler:$CROSS_CC"
[ -d /usr/x86_64-linux-gnu ] || MISSING="$MISSING sysroot:/usr/x86_64-linux-gnu"
if [ -n "$MISSING" ]; then
    echo "BLOCKED - x86-64 cross build cannot run on this host. Missing:$MISSING"
    echo "Smallest next step: install gcc-x86-64-linux-gnu (Debian bookworm) - see docs/PHASE_4_CROSS_COMPILATION_AND_CI.md"
    exit 3
fi
RUNNER=""
if command -v qemu-x86_64 >/dev/null 2>&1; then RUNNER="qemu-x86_64";
elif command -v box64 >/dev/null 2>&1; then RUNNER="box64"; fi
if [ -z "$RUNNER" ]; then
    echo "BLOCKED - compiler present but no x86-64 runner (qemu-x86_64 or box64). Refusing to call a compile-only result a suite run."
    exit 3
fi
echo "compiler: $CROSS_CC ($($CROSS_CC --version | head -n 1))"
echo "runner:   $RUNNER"
B=build-x86_64
rm -rf "$B"; mkdir -p "$B"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc"
for s in src/api/mimir_alloc.c src/core/mimir_core.c src/platform/mimir_platform.c; do
    o="$B/$(basename "$s" .c).o"
    "$CROSS_CC" $CFLAGS -c "$s" -o "$o"
done
for t in tests/contract/test_smoke.c tests/core/test_core.c tests/core/test_b007.c tests/core/test_c005.c tests/core/test_c006.c tests/contract/test_c007.c tests/core/test_c008.c; do
    n=$(basename "$t" .c)
    "$CROSS_CC" $CFLAGS -c "$t" -o "$B/$n.o"
    "$CROSS_CC" $CFLAGS "$B/mimir_alloc.o" "$B/mimir_core.o" "$B/mimir_platform.o" "$B/$n.o" -o "$B/$n"
    file "$B/$n" | grep -q "x86-64" || { echo "FAIL: $B/$n is not x86-64"; exit 1; }
done
REC=tests/cross/recorded_run_x86_64_suite.txt
{
    echo "MimirAlloc x86-64 suite run via $RUNNER (script: scripts/cross/build_x86_64.sh)"
    date -Is; uname -a
    for n in test_smoke test_core test_b007 test_c005 test_c006 test_c007 test_c008; do
        echo "== $n =="; "$RUNNER" "$B/$n"
    done
} 2>&1 | tee "$REC"
echo "X86-64 SUITE RUN COMPLETE - recorded in $REC"
