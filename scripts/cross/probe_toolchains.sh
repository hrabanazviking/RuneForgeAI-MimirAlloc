#!/bin/sh
# MimirAlloc D-005 - cross-compilation toolchain probe.
# EXECUTED on Raspberry Pi purpleviking1 on 2026-11-06 as
# part of slice D-005; full output recorded in
# tests/cross/recorded_run_d005.txt. Read-only: this script
# installs nothing and builds nothing. It reports, for each
# component of an x86-64 cross road, PRESENT or ABSENT as
# found on this host. Findings are data: the script exits 0
# when the report completes, whatever the findings are.
set -u
have() { if command -v "$1" >/dev/null 2>&1; then echo "PRESENT: $1 ($(command -v "$1"))"; else echo "ABSENT:  $1"; fi; }
pathchk() { if [ -e "$1" ]; then echo "PRESENT: $1"; else echo "ABSENT:  $1"; fi; }
echo "== host =="
uname -a
uname -m
cat /etc/os-release 2>/dev/null | head -n 1
getconf PAGESIZE 2>/dev/null
echo "== native compilers =="
have cc; have gcc; have clang; have aarch64-linux-gnu-gcc
cc --version 2>/dev/null | head -n 1
clang --version 2>/dev/null | head -n 2
echo "== x86-64 cross compilers =="
have x86_64-linux-gnu-gcc
have x86_64-linux-musl-gcc
have zig
have tcc
tcc -version 2>/dev/null | head -n 1
echo "== clang x86-64 backend =="
clang --print-targets 2>/dev/null | grep -E "x86" || echo "ABSENT:  clang x86 backend"
echo "== sysroots and target headers =="
pathchk /usr/x86_64-linux-gnu
pathchk /usr/include/x86_64-linux-gnu
pathchk /usr/include/aarch64-linux-gnu
pathchk /usr/lib/musl
echo "== musl =="
have musl-gcc; have musl-clang
echo "== linkers =="
ld -V 2>&1 | head -n 4
have ld.gold; have ld.lld; have x86_64-linux-gnu-ld
ld.gold -V 2>/dev/null | grep -E "elf64-x86-64" || echo "ABSENT:  gold elf64-x86-64 target"
echo "== qemu =="
have qemu-x86_64; have qemu-x86_64-static; have qemu-system-x86_64; have qemu-aarch64
echo "== box64 / binfmt (x86-64 run road) =="
have box64; have box86; have wine
box64 --version 2>/dev/null | head -n 1
if [ -f /proc/sys/fs/binfmt_misc/box64 ]; then echo "binfmt box64 entry:"; cat /proc/sys/fs/binfmt_misc/box64; else echo "ABSENT:  /proc/sys/fs/binfmt_misc/box64"; fi
if [ -f /proc/sys/fs/binfmt_misc/status ]; then echo "binfmt status: $(cat /proc/sys/fs/binfmt_misc/status)"; fi
echo "== CI configuration (repo) =="
for f in .github/workflows .gitlab-ci.yml Jenkinsfile .circleci; do pathchk "$f"; done
have act; have gitlab-runner
echo "== apt candidates for the missing pieces (read-only) =="
if command -v apt-cache >/dev/null 2>&1; then
  for p in gcc-x86-64-linux-gnu libc6-dev-amd64-cross qemu-user qemu-user-static musl-tools; do
    echo "--- $p"; apt-cache policy "$p" 2>/dev/null | head -n 3
  done
else echo "ABSENT:  apt-cache"; fi
echo "== probe complete =="
exit 0
