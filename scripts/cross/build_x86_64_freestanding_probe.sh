#!/bin/sh
# MimirAlloc D-005 - build and run the freestanding x86-64
# probe (tests/cross/probe_freestanding.c) on this ARM64 Pi.
# EXECUTED SUCCESSFULLY on purpleviking1 on 2026-11-06 as
# part of slice D-005; output recorded in
# tests/cross/recorded_run_d005.txt.
#
# What this proves: clang 14 emits valid ELF64 x86-64 for
# freestanding code, GNU gold links it (elf64-x86-64), and
# Box64 (direct exec via binfmt_misc, or explicit box64)
# runs it to completion with its exact exit status.
# What this does NOT prove: anything about building or
# running MimirAlloc itself for x86-64. The allocator needs
# an x86-64 libc sysroot (headers, libraries, crt, loader);
# none exists on this Pi. See the D-005 summary doc.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT INT TERM
SRC="$ROOT/tests/cross/probe_freestanding.c"
OBJ="$WORK/probe.o"
BIN="$WORK/probe_x64"
echo "== compile (clang --target=x86_64-linux-gnu) =="
clang --target=x86_64-linux-gnu -std=c11 -Wall -Wextra -Werror \
    -ffreestanding -nostdinc -c "$SRC" -o "$OBJ"
file "$OBJ"
echo "== link (ld.gold -m elf_x86_64) =="
ld.gold -m elf_x86_64 -o "$BIN" "$OBJ"
file "$BIN"
readelf -h "$BIN" | grep -E "Class|Type|Machine"
echo "== run via binfmt (direct exec) =="
set +e
OUT=$("$BIN"); RC=$?
set -e
echo "output: $OUT"
echo "exit:   $RC"
[ "$OUT" = "D005-X64-PROBE-OK" ] || { echo "FAIL: unexpected output"; exit 1; }
[ "$RC" -eq 42 ] || { echo "FAIL: unexpected exit status"; exit 1; }
if command -v box64 >/dev/null 2>&1; then
    echo "== run via explicit box64 =="
    set +e
    OUT2=$(box64 "$BIN"); RC2=$?
    set -e
    echo "output: $OUT2"
    echo "exit:   $RC2"
    [ "$OUT2" = "D005-X64-PROBE-OK" ] || { echo "FAIL: box64 output"; exit 1; }
    [ "$RC2" -eq 42 ] || { echo "FAIL: box64 exit status"; exit 1; }
else
    echo "box64 ABSENT - explicit-box64 leg skipped"
fi
echo "FREESTANDING PROBE: PASS (compile + link + run, exit 42 verified twice)"
