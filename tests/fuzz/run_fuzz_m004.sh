#!/bin/sh
# M-004 fuzz runner: bounded, fixed seeds. Usage:
# run_fuzz_m004.sh [path-to-fuzz_m004-binary]
set -eu
BIN=${1:-./build/fuzz_m004}
timeout 120 "$BIN"
