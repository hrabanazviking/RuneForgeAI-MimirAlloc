#!/bin/sh
# MimirAlloc D-006 - Android NDK build of the allocator.
#
# STATUS: PROPOSED - BUILD PATH UNEXECUTED (2026-11-06).
# On the Pi today this script stops at its preflight: the
# Android NDK it needs is ABSENT (verified live by slice
# D-006; the preflight refusal itself was executed and is
# recorded in tests/cross/recorded_run_d006.txt, exit 3).
# The build path below has never executed anywhere in this
# campaign. It becomes runnable only after the smallest
# next step in docs/PHASE_4_ANDROID_BUILD_PLAN.md lands
# (an Android NDK installed by Volmarr's word - this slice
# installs nothing).
#
# When an NDK IS present, this script:
#   1. cross-builds the three library translation units
#      plus the contract smoke test for
#      aarch64-linux-android<API> into build-android/
#      (never touching the native build/ tree),
#   2. verifies the produced binary is ELF64 AArch64 with
#      `file` and lists its dynamic dependencies and the
#      four exported mimir_ symbols with the NDK's
#      llvm-readelf / llvm-nm (a silent native build or a
#      glibc-linked binary does not pass),
#   3. STOPS. Running the binary needs an Android device
#      road (adb or on-device shell), which does not exist
#      in this campaign today; a cross-compile alone is
#      NOT a device run and must never be reported as one
#      (roadmap Phase 4 gate, D-001 E5).
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$ROOT"
API=${ANDROID_API:-24}
NDK=${ANDROID_NDK_HOME:-${ANDROID_HOME:+$ANDROID_HOME/ndk}}
MISSING=""
[ -n "$NDK" ] || MISSING="$MISSING ndk-path(unset ANDROID_NDK_HOME/ANDROID_HOME)"
if [ -n "$NDK" ] && [ ! -d "$NDK/toolchains/llvm/prebuilt" ]; then
    MISSING="$MISSING ndk-toolchains:$NDK/toolchains/llvm/prebuilt"
fi
if [ -n "$MISSING" ]; then
    echo "BLOCKED - Android NDK build cannot run on this host. Missing:$MISSING"
    echo "Smallest next step: install an Android NDK (Volmarr's word required) - see docs/PHASE_4_ANDROID_BUILD_PLAN.md"
    exit 3
fi
TOOLCHAIN="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
# NOTE: on an AArch64 host the NDK prebuilt directory is
# linux-x86_64 in current NDK layouts only where Google
# ships it; if the host prebuilt differs, set NDK_HOST_TAG.
TOOLCHAIN=${NDK_TOOLCHAIN:-$TOOLCHAIN}
CC="$TOOLCHAIN/bin/aarch64-linux-android${API}-clang"
[ -x "$CC" ] || { echo "BLOCKED - NDK clang driver not executable: $CC"; exit 3; }
echo "compiler: $CC ($($CC --version | head -n 1))"
echo "api:      $API  target: aarch64-linux-android$API  abi: arm64-v8a"
B=build-android
rm -rf "$B"; mkdir -p "$B"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc"
for s in src/api/mimir_alloc.c src/core/mimir_core.c src/platform/mimir_platform.c; do
    o="$B/$(basename "$s" .c).o"
    "$CC" $CFLAGS -c "$s" -o "$o"
done
"$CC" $CFLAGS -c tests/contract/test_smoke.c -o "$B/test_smoke.o"
"$CC" $CFLAGS "$B/mimir_alloc.o" "$B/mimir_core.o" "$B/mimir_platform.o" "$B/test_smoke.o" -o "$B/test_smoke"
file "$B/test_smoke" | grep -q "aarch64" || { echo "FAIL: $B/test_smoke is not AArch64"; exit 1; }
"$TOOLCHAIN/bin/llvm-nm" -g --defined-only "$B/mimir_alloc.o"
"$TOOLCHAIN/bin/llvm-readelf" -d "$B/test_smoke" | head -n 20
echo "ANDROID CROSS-BUILD COMPLETE (compile only - NOT a device run; the Phase 4 gate needs the on-device run, still OWED)"
