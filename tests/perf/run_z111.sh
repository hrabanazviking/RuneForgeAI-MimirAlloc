#!/bin/sh
# run_z111.sh - Z-111 profile-guided class table v2 runner
# (MimirAlloc Z13). Successor to run_z095.sh: same Z-004
# measurement shape (separate process per value, taskset
# -c 5, legs/variants interleaved round-robin, 5 measured
# reps, medians by summarize_z111.py), two trace suites:
# the BANKED canonical Z-094 traces (converted on the fly
# by convert_z111.py into build/z111/traces/, never
# committed) and Z-095's own captured traces (continuity).
# Oracle: bench_k002 per variant.
#
# Variants (per-class TLS bin cap / batch, class order =
# banked G-002 table 16..1024; see patch_z111.py):
#   v0  baseline 16/8 everywhere (pristine sources)
#   v1  Z-095 near-miss reference: 32/16 everywhere
#   wA  isolation: cap 32 / batch 8 everywhere
#   wB  isolation: cap 16 / batch 16 everywhere
#   v2  per-class profile-guided candidate; its tables are
#       the CAPS_V2 / BATCH_V2 literals below, chosen from
#       the stage-1 isolation results + the trace profiles
#       and recorded in the recorded run before stage 2.
#
# Phases:
#   stage1   oracle only for v0 v1 wA wB (isolation)
#   measure  full suites for $VARIANTS (default "v0 v1 v2")
# Raw lines land in hidden_files/z111_replay.raw and
# hidden_files/z111_oracle.raw (variant-tagged); stage1 raw
# in hidden_files/z111_stage1_oracle.raw.
set -u
PHASE=${1:?usage: run_z111.sh stage1|measure}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
HF="$ROOT/hidden_files"
ZB="$ROOT/build/z111"
TRC="$ZB/traces"
TR95="$ROOT/tests/perf/traces_z095"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
PIN=5
REPS=5
mkdir -p "$HF" "$ZB" "$TRC"

U16="16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16"
U32="32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32,32"
U8="8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8"
U16B="16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16"
# v2 tables: stage-1 isolation convicted batch 16 (wB +4.77%
# small_churn, wA -0.80%); capacity 32 carries the lifecycle
# win. v2 = cap 32 on the trace-hot union classes (Z-095
# histograms: ollama 16-96, git 16-160, python 512-1024),
# cap 16 on the cold middle (112,192-448), batch 8 everywhere.
CAPS_V2="32,32,32,32,32,32,16,32,32,16,16,16,16,16,16,32,32,32,32,32"
BATCH_V2="$U8"

caps_for() {
    case $1 in
        v1) echo "$U32" ;;
        wA) echo "$U32" ;;
        wB) echo "$U16" ;;
        v2) echo "$CAPS_V2" ;;
    esac
}
batch_for() {
    case $1 in
        v1) echo "$U16B" ;;
        wA) echo "$U8" ;;
        wB) echo "$U16B" ;;
        v2) echo "$BATCH_V2" ;;
    esac
}

build_variant() {
    v=$1
    src="$ZB/$v"
    rm -rf "$src"
    mkdir -p "$src/tests/contract"
    cp -a "$ROOT/src" "$src/src"
    cp -a "$ROOT/include" "$src/include"
    cp "$ROOT/tests/contract/mimir_tls_cache_z028.h" "$src/tests/contract/"
    if [ "$v" != v0 ]; then
        python3 "$ROOT/tests/perf/patch_z111.py" \
            "$src/src/core/mimir_core.c" \
            "$src/tests/contract/mimir_tls_cache_z028.h" \
            --caps "$(caps_for "$v")" --batches "$(batch_for "$v")" \
            || return 1
    fi
    gcc $CFLAGS -I"$src/include" -I"$src/src" -c "$src/src/core/mimir_core.c" -o "$src/core.o" || return 1
    gcc $CFLAGS -I"$src/include" -I"$src/src" -c "$src/src/api/mimir_alloc.c" -o "$src/alloc.o" || return 1
    gcc $CFLAGS -I"$src/include" -I"$src/src" -c "$src/src/platform/mimir_platform.c" -o "$src/plat.o" || return 1
    gcc $CFLAGS -I"$src/include" -I"$src/src" "$ROOT/tests/perf/probe_z095.c" \
        "$src/core.o" "$src/alloc.o" "$src/plat.o" -o "$src/probe_mimir" || return 1
    gcc $CFLAGS -I"$src/include" -I"$src/src" -c "$ROOT/tests/perf/bench_k002.c" -o "$src/bench_o.o" || return 1
    gcc $CFLAGS "$src/bench_o.o" "$src/core.o" "$src/alloc.o" "$src/plat.o" -o "$src/bench_mimir" || return 1
    echo "built variant $v"
}

build_glibc() {
    gcc $CFLAGS "$ROOT/tests/perf/probe_z095.c" -DLEG_GLIBC -o "$ZB/probe_glibc" || exit 1
    gcc $CFLAGS -DLEG_GLIBC "$ROOT/tests/perf/bench_k002.c" -o "$ZB/bench_glibc" || exit 1
}

convert_traces() {
    for t in "$ROOT"/tests/perf/traces/z094_*.trace.gz; do
        name=$(basename "$t" .trace.gz)
        python3 "$ROOT/tests/perf/convert_z111.py" "$t" "$TRC/$name.trace" \
            >> "$HF/z111_convert.log" 2>&1 \
            || { echo "CONVERT FAILED $name"; exit 1; }
        echo "converted $name: $(wc -l < "$TRC/$name.trace") ops"
    done
}

oracle_run() {
    # $1 = variants list, $2 = raw file
    : > "$2"
    echo "# loadavg at oracle start: $(cat /proc/loadavg)" >> "$2"
    r=1
    while [ $r -le $REPS ]; do
        for v in $1; do
            taskset -c $PIN "$ZB/$v/bench_mimir" > "$ZB/one.csv" \
                || { echo "ORACLE FAILED $v"; exit 1; }
            sed "s/^/$v,/" "$ZB/one.csv" >> "$2"
        done
        taskset -c $PIN "$ZB/bench_glibc" > "$ZB/one.csv" \
            || { echo "ORACLE FAILED glibc"; exit 1; }
        sed "s/^/v0,/" "$ZB/one.csv" >> "$2"
        r=$((r + 1))
    done
    echo "# loadavg at oracle end: $(cat /proc/loadavg)" >> "$2"
}

if [ "$PHASE" = stage1 ]; then
    for v in v0 v1 wA wB; do build_variant "$v" || { echo "BUILD FAILED $v"; exit 1; }; done
    build_glibc
    oracle_run "v0 v1 wA wB" "$HF/z111_stage1_oracle.raw"
    python3 - "$HF/z111_stage1_oracle.raw" <<'PYEOF'
import statistics, sys
from collections import defaultdict
oracle = defaultdict(list)
for ln in open(sys.argv[1]):
    ln = ln.strip()
    if not ln or ln.startswith("#"):
        continue
    p = ln.split(",")
    if len(p) != 8:
        continue
    oracle[(p[0], p[1], p[2])].append(float(p[6]))
wls = sorted({w for (_, _, w) in oracle})
base = {w: statistics.median(oracle[("v0", "mimiralloc", w)]) for w in wls}
for var in ("v0", "v1", "wA", "wB"):
    row = []
    for w in wls:
        m = statistics.median(oracle[(var, "mimiralloc", w)])
        row.append(f"{w}={m:.2f}" if var == "v0" else
                   f"{w}={m:.2f} ({100.0*(m-base[w])/base[w]:+.2f}%)")
    print(f"{var:4} " + "  ".join(row))
PYEOF
    exit 0
fi

# ================= measure =================
VARIANTS=${VARIANTS:-"v0 v1 v2"}
for v in $VARIANTS; do build_variant "$v" || { echo "BUILD FAILED $v"; exit 1; }; done
build_glibc
: > "$HF/z111_convert.log"
convert_traces

: > "$HF/z111_replay.raw"
echo "# loadavg at measure start: $(cat /proc/loadavg)" >> "$HF/z111_replay.raw"
for trace in "$TRC"/*.trace "$TR95"/*.trace; do
    [ -s "$trace" ] || continue
    ops=$(wc -l < "$trace")
    if [ "$ops" -lt 1000 ]; then echo "trace $trace too small ($ops ops) - skipped"; continue; fi
    r=1
    while [ $r -le $REPS ]; do
        for v in $VARIANTS; do
            taskset -c $PIN "$ZB/$v/probe_mimir" "$trace" > "$ZB/one.csv" \
                || { echo "REPLAY FAILED $v $trace"; exit 1; }
            sed "s/^/$v,/" "$ZB/one.csv" >> "$HF/z111_replay.raw"
        done
        taskset -c $PIN "$ZB/probe_glibc" "$trace" > "$ZB/one.csv" \
            || { echo "REPLAY FAILED glibc $trace"; exit 1; }
        sed "s/^/v0,/" "$ZB/one.csv" >> "$HF/z111_replay.raw"
        r=$((r + 1))
    done
    echo "replayed $trace ($ops ops)"
done

oracle_run "$VARIANTS" "$HF/z111_oracle.raw"
echo "# loadavg at measure end: $(cat /proc/loadavg)" >> "$HF/z111_replay.raw"

python3 "$ROOT/tests/perf/summarize_z111.py" "$HF/z111_replay.raw" "$HF/z111_oracle.raw" \
    | tee "$HF/z111_summary.txt"
