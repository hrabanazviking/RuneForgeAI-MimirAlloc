#!/bin/sh
# run_z071.sh - Z-071 diagnostics default-on packet runner.
#
# Z-004 protocol (docs/Z13_MEASUREMENT_PROTOCOL.md):
#   - every measured value is a separate PROCESS pinned
#     taskset -c 5 (the Z-062 house pin: CCD0 physical core;
#     avoids CPU 0 housekeeping and the soak cores 2/3)
#   - 1 warm-up process per (leg, cell), discarded (the
#     probe also runs an internal 1/10-scale warm-up pass)
#   - REPS (default 5) measured processes per (leg, cell),
#     legs interleaved round-robin within each round, leg
#     order rotated by round; headline = MEDIAN
#     (summarize_z071.py)
#   - the closed leg is run TWICE (closed, closed_dup):
#     the identical-leg agreement control, bound 5%.
# Legs (the diag gate posture is set by THIS runner via the
# process environment; the probe never touches it):
#   closed     MIMIRALLOC_DIAG unset
#   open       MIMIRALLOC_DIAG=1, stderr to /dev/null for
#              timed reps (emission counted exactly by the
#              probe's emitted_delta, never by timing)
#   closed_dup identical to closed
# Success cells: the 4 K-002 oracle workloads (bench_k002.c
# shapes and iteration counts). Their stderr is captured
# per rep to a file and totalled into the raw trailer as
# errbytes lines (exact byte counts; 0 expected everywhere,
# including gate-open: success paths make no report calls).
# Failure cells: 3 genuine-failure shapes, 2000 attempts per
# process. One extra gate-open CAPTURE rep per shape writes
# stderr to a file; the runner counts its lines and bytes
# into the raw trailer as a capture line. A capture file
# holds attempts + attempts/10 lines (the probe's internal
# warm-up failures emit too); bytes/line is computed from
# the capture totals in the summariser.
#
# Usage: run_z071.sh <raw-output-file> [reps]
set -eu
OUT=${1:?usage: run_z071.sh <raw-output-file> [reps]}
REPS=${2:-5}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z71"
CAP="$ZB/capture"
mkdir -p "$ZB" "$CAP"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
PIN=5
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/alloc.o"
gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/plat.o"
gcc $CFLAGS -I"$ROOT/include" "$ROOT/tests/perf/probe_z071.c" \
    "$ZB/alloc.o" "$ZB/core.o" "$ZB/plat.o" -o "$ZB/probe_z071"
PROBE="$ZB/probe_z071"
WORKLOADS="small_churn mixed_batch large_batch mixed_lifecycle"
FAILS="fail_malloc_huge fail_calloc_overflow fail_malloc_backing"
ATTEMPTS=2000
{
  echo "# Z-071 diag packet raw"
  echo "# date_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "# host: $(uname -a)"
  echo "# head: $(git -C "$ROOT" rev-parse HEAD)"
  echo "# loadavg_at_start: $(cat /proc/loadavg)"
  echo "# pin: taskset -c $PIN; reps: $REPS; attempts_fail: $ATTEMPTS"
} > "$OUT"
run_leg() { # leg cell count errfile
  case $1 in
    closed|closed_dup) env -u MIMIRALLOC_DIAG taskset -c $PIN "$PROBE" "$1" "$2" $3 2>"$4" ;;
    open) MIMIRALLOC_DIAG=1 taskset -c $PIN "$PROBE" "$1" "$2" $3 2>"$4" ;;
  esac
}
legs_for_round() { # round -> rotated leg order
  case $(($1 % 3)) in
    0) echo "closed open closed_dup" ;;
    1) echo "open closed_dup closed" ;;
    *) echo "closed_dup closed open" ;;
  esac
}
# warm-ups (discarded)
for cell in $WORKLOADS; do
  for leg in closed open closed_dup; do
    run_leg $leg $cell "" "$CAP/warm.err" >> "$ZB/warm.out"
  done
done
for cell in $FAILS; do
  for leg in closed open closed_dup; do
    run_leg $leg $cell $ATTEMPTS /dev/null >> "$ZB/warm.out"
  done
done
# measured rounds: success cells (stderr captured per rep)
r=1
while [ "$r" -le "$REPS" ]; do
  for cell in $WORKLOADS; do
    for leg in $(legs_for_round $r); do
      run_leg $leg $cell "" "$CAP/${leg}_${cell}_${r}.err" >> "$OUT"
    done
  done
  r=$((r + 1))
done
# measured rounds: failure cells (stderr to /dev/null)
r=1
while [ "$r" -le "$REPS" ]; do
  for cell in $FAILS; do
    for leg in $(legs_for_round $r); do
      run_leg $leg $cell $ATTEMPTS /dev/null >> "$OUT"
    done
  done
  r=$((r + 1))
done
# capture reps: one gate-open process per failure shape
for cell in $FAILS; do
  MIMIRALLOC_DIAG=1 taskset -c $PIN "$PROBE" open $cell $ATTEMPTS 2>"$CAP/capture_${cell}.err" >> "$OUT"
  lines=$(wc -l < "$CAP/capture_${cell}.err")
  bytes=$(wc -c < "$CAP/capture_${cell}.err")
  echo "capture,$cell,$lines,$bytes" >> "$OUT"
done
# success-leg stderr byte totals (exact)
for cell in $WORKLOADS; do
  for leg in closed open closed_dup; do
    total=0
    for f in "$CAP"/${leg}_${cell}_*.err; do
      b=$(wc -c < "$f"); total=$((total + b))
    done
    echo "errbytes,$leg,$cell,$total" >> "$OUT"
  done
done
echo "# loadavg_at_end: $(cat /proc/loadavg)" >> "$OUT"
echo "raw lines: $(wc -l < "$OUT")" >&2
