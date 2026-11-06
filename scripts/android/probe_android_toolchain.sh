#!/bin/sh
# MimirAlloc D-006 - Android toolchain readiness probe.
# EXECUTED on Raspberry Pi purpleviking1 on 2026-11-06 as
# part of slice D-006; full output recorded in
# tests/cross/recorded_run_d006.txt. Read-only: this script
# installs nothing, builds nothing, and changes no state.
# It reports PRESENT or ABSENT for every component of an
# Android NDK build road on this host. Findings are data:
# the script exits 0 when the report completes, whatever
# the findings are.
set -u
have() { if command -v "$1" >/dev/null 2>&1; then echo "PRESENT: $1 ($(command -v "$1"))"; else echo "ABSENT:  $1"; fi; }
pathchk() { if [ -e "$1" ]; then echo "PRESENT: $1"; else echo "ABSENT:  $1"; fi; }
echo "== host =="
uname -a
uname -m
head -n 1 /etc/os-release 2>/dev/null
getconf PAGESIZE 2>/dev/null
nproc 2>/dev/null
free -g | head -n 2
echo "== android environment variables =="
echo "ANDROID_NDK_HOME=[${ANDROID_NDK_HOME:-}]"
echo "ANDROID_HOME=[${ANDROID_HOME:-}]"
echo "ANDROID_SDK_ROOT=[${ANDROID_SDK_ROOT:-}]"
echo "PREFIX (Termux)=[${PREFIX:-}]"
env | grep -i android || echo "(no ANDROID_* variables in environment)"
echo "== NDK / SDK commands =="
have ndk-build
have sdkmanager
have aarch64-linux-android21-clang
have aarch64-linux-android-clang
have adb
echo "== NDK / SDK paths =="
pathchk /home/pi/Android
pathchk /home/pi/Android/Sdk
pathchk /home/pi/android-ndk
pathchk /opt/android-sdk
pathchk /opt/android-ndk
pathchk /opt/android
pathchk /usr/lib/android-sdk
pathchk /usr/lib/android-ndk
pathchk /home/pi/.android
pathchk /usr/aarch64-linux-android
echo "== native compilers (for contrast) =="
have cc; have gcc; have clang
cc --version 2>/dev/null | head -n 1
clang --version 2>/dev/null | head -n 2
echo "== clang AArch64 backend (NOT an Android toolchain) =="
clang --print-targets 2>/dev/null | grep -E "aarch64|arm64" || echo "ABSENT:  clang aarch64 backend"
echo "== Termux / proot presence (on this Pi host) =="
have proot
have proot-distro
have termux-info
pathchk /data/data/com.termux
pathchk /data/data/com.termux/files/usr
pathchk /system/bin
echo "== debian packages mentioning android/ndk/adb (read-only) =="
dpkg -l 2>/dev/null | grep -i -E "android|ndk|adb|proot|termux" || echo "(no dpkg matches)"
echo "== name search, bounded depth (unrelated hits stay labelled) =="
find /home/pi /opt /usr/local -maxdepth 3 \( -iname "*ndk*" -o -iname "android-ndk*" -o -iname "ndk-build" \) 2>/dev/null | head -n 20
echo "(end name search - a name match is not an NDK; only an NDK tree with toolchains/ and ndk-build counts)"
echo "== probe complete =="
exit 0
