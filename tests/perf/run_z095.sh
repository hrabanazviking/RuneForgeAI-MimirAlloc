#!/bin/sh
# run_z095.sh - Z-095 trace-driven tuning runner (MimirAlloc Z13).
#
# Phases:
#   capture   build the capture shim and capture three real
#             traces on this host (python import storm + churn,
#             git churn in this worktree, an Ollama private
#             server generate), then convert each capture to a
#             replay trace + histogram stats.
#   measure   build the baseline (v0) and variant core objects
#             (v1: TLS bin 32 / batch 16; v2: TLS bin 8 /
#             batch 4; v3: respaced 20-class table, only when
#             V3_SIZES names 20 sizes), replay every trace
#             against every variant and glibc, run the
#             bench_k002 synthetic oracle per variant, and
#             summarise with the adoption bar.
#
# Z-004 protocol shape for every timed run: separate process
# per measured value, taskset -c 5, legs/variants interleaved
# round-robin, 5 measured reps, median taken by the
# summariser. Raw lines land in hidden_files/z095_replay.raw
# and hidden_files/z095_oracle.raw (variant-tagged).
#
# Usage: run_z095.sh capture|measure
set -u
PHASE=${1:?usage: run_z095.sh capture|measure}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
HF="$ROOT/hidden_files"
ZB="$ROOT/build/z95"
TR="$ROOT/tests/perf/traces_z095"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"
PIN=5
REPS=5
mkdir -p "$HF" "$ZB" "$TR"

if [ "$PHASE" = capture ]; then
    gcc $CFLAGS -shared -fPIC "$ROOT/tests/perf/trace_capture_z095.c" \
        -o "$ZB/trace_capture_z095.so" -ldl || exit 1
    echo "shim built: $ZB/trace_capture_z095.so"

    # --- python: import storm + allocation churn ---
    MIMIR_TRACE_OUT="$HF/z095_cap_python.bin" MIMIR_TRACE_MAX=2000000 \
    LD_PRELOAD="$ZB/trace_capture_z095.so" \
    python3 - <<'PYEOF'
import importlib, hashlib
mods = ("os sys json re math random collections typing dataclasses "
        "unittest.mock email.parser http.client urllib.request "
        "xml.etree.ElementTree sqlite3 logging argparse textwrap "
        "pathlib io struct array decimal fractions statistics "
        "hashlib hmac base64 binascii zlib gzip tarfile zipfile "
        "threading queue socket ssl csv configparser pickle "
        "datetime calendar timeit inspect ast dis tokenize").split()
for m in mods:
    importlib.import_module(m)
h = hashlib.sha256()
win = []
for i in range(300000):
    k = i % 5
    if k == 0: o = {"k": i, "v": [i, i + 1, str(i)]}
    elif k == 1: o = bytes((i % 251,)) * (i % 997 + 1)
    elif k == 2: o = "x" * (i % 513) + str(i)
    elif k == 3: o = [i] * (i % 129)
    else: o = bytearray(i % 2049 + 1)
    win.append(o)
    if len(win) > 4096: win.pop(0)
    if i % 50000 == 0: h.update(str(len(win)).encode())
print("python workload checksum:", h.hexdigest())
PYEOF
    echo "python capture exit: $? size: $(stat -c%s "$HF/z095_cap_python.bin" 2>/dev/null || echo MISSING)"

    # --- git: object churn in this worktree (read-only ops) ---
    ( cd "$ROOT" && \
      MIMIR_TRACE_OUT="$HF/z095_cap_git.bin" MIMIR_TRACE_MAX=2000000 \
      LD_PRELOAD="$ZB/trace_capture_z095.so" \
      sh -c 'git --no-optional-locks log --oneline -3000 > /dev/null; \
             git --no-optional-locks status > /dev/null; \
             git --no-optional-locks diff --stat HEAD~5..HEAD > /dev/null; \
             git --no-optional-locks log --stat -200 > /dev/null' )
    echo "git capture exit: $? size: $(stat -c%s "$HF/z095_cap_git.bin" 2>/dev/null || echo MISSING)"

    # --- ollama: private server, one short generate ---
    OMODELS=$(systemctl show ollama -p Environment 2>/dev/null | tr ' ' '\n' | grep '^OLLAMA_MODELS=' | cut -d= -f2-)
    [ -n "$OMODELS" ] || OMODELS=/mnt/vault/models/ollama
    rm -f "$HF/z095_cap_ollama.bin"
    OLLAMA_HOST=127.0.0.1:11499 OLLAMA_MODELS="$OMODELS" \
    MIMIR_TRACE_OUT="$HF/z095_cap_ollama.bin" MIMIR_TRACE_MAX=4000000 \
    LD_PRELOAD="$ZB/trace_capture_z095.so" \
        ollama serve > "$HF/z095_ollama_serve.log" 2>&1 &
    OPID=$!
    ok=0
    i=0
    while [ $i -lt 60 ]; do
        if curl -s http://127.0.0.1:11499/api/tags > /dev/null 2>&1; then ok=1; break; fi
        sleep 1; i=$((i + 1))
    done
    if [ $ok = 1 ]; then
        curl -s http://127.0.0.1:11499/api/generate -d '{"model":"qwen2.5:0.5b","prompt":"The capital of France is","stream":false,"options":{"temperature":0,"num_predict":16}}' \
            > "$HF/z095_ollama_gen.json" 2>&1 || true
        echo "ollama generate done: $(head -c 200 "$HF/z095_ollama_gen.json" 2>/dev/null)"
    else
        echo "OLLAMA_CAPTURE_FAILED: private server never became ready"
    fi
    kill $OPID 2>/dev/null; wait $OPID 2>/dev/null
    sleep 1
    echo "ollama capture size: $(stat -c%s "$HF/z095_cap_ollama.bin" 2>/dev/null || echo MISSING)"

    # --- convert ---
    for name in python git ollama; do
        if [ -s "$HF/z095_cap_$name.bin" ]; then
            python3 "$ROOT/tests/perf/convert_z095.py" \
                "$HF/z095_cap_$name.bin" "$TR/$name.trace" 300000 \
                > "$HF/z095_stats_$name.txt" 2>&1 \
                && echo "converted $name: $(wc -l < "$TR/$name.trace") replay ops" \
                || echo "CONVERT FAILED for $name (see hidden_files/z095_stats_$name.txt)"
        else
            echo "no capture for $name - trace skipped"
        fi
    done
    exit 0
fi

# ================= measure =================
VARIANTS="v0 v1 v2"
[ -n "${V3_SIZES:-}" ] && VARIANTS="$VARIANTS v3"

build_variant() {
    v=$1
    src="$ZB/$v"
    rm -rf "$src"
    mkdir -p "$src/tests/contract"
    cp -a "$ROOT/src" "$src/src"
    cp -a "$ROOT/include" "$src/include"
    cp "$ROOT/tests/contract/mimir_tls_cache_z028.h" "$src/tests/contract/"
    hdr="$src/tests/contract/mimir_tls_cache_z028.h"
    core="$src/src/core/mimir_core.c"
    case $v in
        v1) sed -i 's/#define MIMIR_TLS_BIN_CAPACITY ((size_t)16)/#define MIMIR_TLS_BIN_CAPACITY ((size_t)32)/; s/#define MIMIR_TLS_BATCH ((size_t)8)/#define MIMIR_TLS_BATCH ((size_t)16)/' "$hdr" ;;
        v2) sed -i 's/#define MIMIR_TLS_BIN_CAPACITY ((size_t)16)/#define MIMIR_TLS_BIN_CAPACITY ((size_t)8)/; s/#define MIMIR_TLS_BATCH ((size_t)8)/#define MIMIR_TLS_BATCH ((size_t)4)/' "$hdr" ;;
        v3) V3_SIZES="$V3_SIZES" python3 - "$core" <<'PYEOF'
import os, re, sys
sizes = [s.strip() for s in os.environ["V3_SIZES"].split(",") if s.strip()]
assert len(sizes) == 20, f"V3 needs exactly 20 classes, got {len(sizes)}"
path = sys.argv[1]
text = open(path).read()
line1 = ", ".join(sizes[:10])
line2 = ", ".join(sizes[10:])
new = ("static const size_t mimir_class_sizes[MIMIR_SMALL_CLASS_COUNT] = {\n"
       f"    {line1},\n    {line2}\n}};")
text2, n = re.subn(r"static const size_t mimir_class_sizes\[MIMIR_SMALL_CLASS_COUNT\] = \{[^}]*\};",
                   new, text, count=1)
assert n == 1, "class table not found"
open(path, "w").write(text2)
print("v3 class table installed:", sizes)
PYEOF
            ;;
    esac
    gcc $CFLAGS -I"$src/include" -I"$src/src" -c "$src/src/core/mimir_core.c" -o "$src/core.o" || return 1
    gcc $CFLAGS -I"$src/include" -I"$src/src" -c "$src/src/api/mimir_alloc.c" -o "$src/alloc.o" || return 1
    gcc $CFLAGS -I"$src/include" -I"$src/src" -c "$src/src/platform/mimir_platform.c" -o "$src/plat.o" || return 1
    gcc $CFLAGS -I"$src/include" -I"$src/src" "$ROOT/tests/perf/probe_z095.c" \
        "$src/core.o" "$src/alloc.o" "$src/plat.o" -o "$src/probe_mimir" || return 1
    gcc $CFLAGS -I"$src/include" -I"$src/src" -c "$ROOT/tests/perf/bench_k002.c" -o "$src/bench_o.o" || return 1
    gcc $CFLAGS "$src/bench_o.o" "$src/core.o" "$src/alloc.o" "$src/plat.o" -o "$src/bench_mimir" || return 1
    echo "built variant $v"
}

for v in $VARIANTS; do build_variant "$v" || { echo "BUILD FAILED $v"; exit 1; }; done
gcc $CFLAGS "$ROOT/tests/perf/probe_z095.c" -DLEG_GLIBC -o "$ZB/probe_glibc" || exit 1
gcc $CFLAGS -DLEG_GLIBC "$ROOT/tests/perf/bench_k002.c" -o "$ZB/bench_glibc" || exit 1

: > "$HF/z095_replay.raw"
: > "$HF/z095_oracle.raw"
echo "# loadavg at measure start: $(cat /proc/loadavg)" >> "$HF/z095_replay.raw"

# Replay suite: reps interleaved across variants and legs.
for trace in "$TR"/*.trace; do
    [ -s "$trace" ] || continue
    ops=$(wc -l < "$trace")
    if [ "$ops" -lt 1000 ]; then echo "trace $trace too small ($ops ops) - skipped"; continue; fi
    r=1
    while [ $r -le $REPS ]; do
        for v in $VARIANTS; do
            taskset -c $PIN "$ZB/$v/probe_mimir" "$trace" > "$ZB/one.csv" \
                || { echo "REPLAY FAILED $v $trace"; exit 1; }
            sed "s/^/$v,/" "$ZB/one.csv" >> "$HF/z095_replay.raw"
        done
        taskset -c $PIN "$ZB/probe_glibc" "$trace" > "$ZB/one.csv" \
            || { echo "REPLAY FAILED glibc $trace"; exit 1; }
        sed "s/^/v0,/" "$ZB/one.csv" >> "$HF/z095_replay.raw"
        r=$((r + 1))
    done
    echo "replayed $trace"
done

# Oracle suite: bench_k002 per variant + glibc, interleaved.
r=1
while [ $r -le $REPS ]; do
    for v in $VARIANTS; do
        taskset -c $PIN "$ZB/$v/bench_mimir" > "$ZB/one.csv" \
            || { echo "ORACLE FAILED $v"; exit 1; }
        sed "s/^/$v,/" "$ZB/one.csv" >> "$HF/z095_oracle.raw"
    done
    taskset -c $PIN "$ZB/bench_glibc" > "$ZB/one.csv" \
        || { echo "ORACLE FAILED glibc"; exit 1; }
    sed "s/^/v0,/" "$ZB/one.csv" >> "$HF/z095_oracle.raw"
    r=$((r + 1))
done
echo "# loadavg at measure end: $(cat /proc/loadavg)" >> "$HF/z095_replay.raw"

python3 "$ROOT/tests/perf/summarize_z095.py" "$HF/z095_replay.raw" "$HF/z095_oracle.raw" \
    | tee "$HF/z095_summary.txt"
