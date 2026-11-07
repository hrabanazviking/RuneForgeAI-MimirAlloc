#!/bin/sh
set -eu
BIN=$(dirname "$0")/../../build/bench_k005
for w in seq64 rand64 seq1024 rand1024 mixed_allocorder mixed_grouped; do
 r=1; while [ "$r" -le 3 ]; do
  echo "perf workload=$w rep=$r"
  perf stat -e cycles:u,cache-misses:u,cache-references:u,dTLB-loads:u,dTLB-load-misses:u -x, "$BIN" "$w" 2>&1 | grep -E '^[0-9]'
  r=$((r+1)); done; done
