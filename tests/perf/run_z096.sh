#!/bin/sh
# run_z096.sh - Z-096 final build-flag matrix runner.
#
# The Z-035 flag study repeated on the near-final core: the core
# changed since Z-035 (Wave-5/6 large path + mapping cache,
# Wave-7/8 work, Wave-9/10/11 small-path, batch and trace tuning
# through Z-095), so Z-035's verdicts do not carry over
# unmeasured. Full factorial over the Z-035 flag axes:
#   opt level : -O2 | -O3
#   arch      : (none) | -march=native
#   lto       : (none) | -flto (link -flto too)
# Eight variants:
#   base     : -O2
#   o3       : -O3
#   march    : -O2 -march=native
#   o3march  : -O3 -march=native
#   lto      : -O2 -flto            (the shipped set today)
#   o3lto    : -O3 -flto
#   marchlto : -O2 -march=native -flto
#   full     : -O3 -march=native -flto
# The bench harness (tests/perf/bench_k002.c) is built ONCE at
# -O2 for every leg (K-004 style); the three allocator objects
# (src/api, src/core, src/platform) differ ONLY by the flags
# under trial. Common flags for every TU: -std=c11 -Wall -Wextra
# -Werror -pthread -Iinclude -Isrc (the shipped set).
#
# Methodology: docs/Z13_MEASUREMENT_PROTOCOL.md (Z-004) -
# every process pinned with taskset to ONE physical core,
# warm-up processes discarded, measured processes interleaved
# round-robin variant-by-variant so slow drift lands on all
# variants, repetitions are separate processes, headline is
# the median of per-process ns/op (summarize_z096.py).
#
# Usage: tests/perf/run_z096.sh <out-prefix> [reps] [warmups] [cpus]
#   reps default 9 (protocol min 5), warmups default 3,
#   cpus default 2 (CCD0 physical core, protocol section 3).
# Outputs: <prefix>.header (environment stamp + flag table),
#          <prefix>.raw (variant,workload,ops,elapsed_ns,
#          ops_per_sec,ns_per_op,checksum lines in run order),
#          <prefix>.bins/<variant>/ (the built binaries).
set -eu

OUT=${1:?usage: run_z096.sh <out-prefix> [reps] [warmups] [cpus]}
REPS=${2:-9}
WARMUPS=${3:-3}
CPUS=${4:-2}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
cd "$ROOT"

[ "$REPS" -ge 5 ] || { echo "reps must be >= 5 (protocol section 6)" >&2; exit 2; }
command -v taskset >/dev/null 2>&1 || { echo "taskset not found" >&2; exit 1; }
taskset -c "$CPUS" true || { echo "taskset refused CPU list: $CPUS" >&2; exit 1; }

BASE="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc"
VARIANTS="base o3 march o3march lto o3lto marchlto full"
opt_for() {
    case "$1" in
        base)     echo "-O2" ;;
        o3)       echo "-O3" ;;
        march)    echo "-O2 -march=native" ;;
        o3march)  echo "-O3 -march=native" ;;
        lto)      echo "-O2 -flto" ;;
        o3lto)    echo "-O3 -flto" ;;
        marchlto) echo "-O2 -march=native -flto" ;;
        full)     echo "-O3 -march=native -flto" ;;
    esac
}

BINDIR="$OUT.bins"
mkdir -p "$BINDIR" "$(dirname "$OUT")"

# ---- build (bench TU at -O2 for every leg, K-004 style) ----
for v in $VARIANTS; do
    vd="$BINDIR/$v"
    mkdir -p "$vd"
    OPT=$(opt_for "$v")
    # shellcheck disable=SC2086
    cc $BASE $OPT -c src/api/mimir_alloc.c      -o "$vd/mimir_alloc.o"
    # shellcheck disable=SC2086
    cc $BASE $OPT -c src/core/mimir_core.c      -o "$vd/mimir_core.o"
    # shellcheck disable=SC2086
    cc $BASE $OPT -c src/platform/mimir_platform.c -o "$vd/mimir_platform.o"
    # shellcheck disable=SC2086
    cc $BASE -O2 -c tests/perf/bench_k002.c     -o "$vd/bench_k002.o"
    # shellcheck disable=SC2086
    cc $BASE $OPT "$vd/mimir_alloc.o" "$vd/mimir_core.o" \
       "$vd/mimir_platform.o" "$vd/bench_k002.o" -o "$vd/bench_z096_$v"
    echo "built variant $v: $OPT"
done

# ---- environment header ----
{
    echo "run_z096_version: 1"
    echo "slice: Z-096 final build-flag matrix"
    echo "date_utc: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    echo "repo: $ROOT"
    echo "head: $(git rev-parse HEAD 2>/dev/null || echo unknown)"
    echo "branch: $(git branch --show-current 2>/dev/null || echo unknown)"
    echo "cc: $(cc --version | head -1)"
    echo "cpus_pin: $CPUS"
    echo "reps: $REPS"
    echo "warmups: $WARMUPS"
    echo "common_flags: $BASE"
    echo "bench_tu_flags: -O2 (identical for every leg, K-004 style)"
    echo "shipped_today: -O2 -flto (variant lto; Z-035 adoption)"
    for v in $VARIANTS; do echo "variant_$v: $(opt_for "$v")"; done
    echo "loadavg_start: $(awk '{print $1, $2, $3}' /proc/loadavg)"
    echo "co_tenant_top: $(ps -eo comm,%cpu --sort=-%cpu | head -4 | tr '\n' ';')"
    if [ -x "$ROOT/scripts/z13/host_truth.sh" ]; then
        echo "--- host_truth (scripts/z13/host_truth.sh, verbatim) ---"
        "$ROOT/scripts/z13/host_truth.sh"
        echo "--- end host_truth ---"
    fi
} > "$OUT.header"

# ---- warm-ups (interleaved, discarded) ----
w=1
while [ "$w" -le "$WARMUPS" ]; do
    for v in $VARIANTS; do
        taskset -c "$CPUS" "$BINDIR/$v/bench_z096_$v" > /dev/null
    done
    w=$((w + 1))
done

# ---- measured processes (round-robin interleave) ----
: > "$OUT.raw"
r=1
while [ "$r" -le "$REPS" ]; do
    for v in $VARIANTS; do
        taskset -c "$CPUS" "$BINDIR/$v/bench_z096_$v" | \
            awk -F, -v v="$v" 'BEGIN{OFS=","} {$1=v; print}' >> "$OUT.raw"
    done
    r=$((r + 1))
done

{
    echo "loadavg_end: $(awk '{print $1, $2, $3}' /proc/loadavg)"
    echo "raw_lines: $(wc -l < "$OUT.raw") (expect $((REPS * 8 * 4)))"
} >> "$OUT.header"
echo "raw lines: $(wc -l < "$OUT.raw") (expect $((REPS * 8 * 4)))"
