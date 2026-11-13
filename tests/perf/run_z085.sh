#!/bin/sh
# Ollama model store fix (Z-085 finishing run): the host ollama
# service runs as user ollama via systemd with drop-in override
# OLLAMA_MODELS=/mnt/vault/models/ollama, so ollama list (via the
# :11434 service) sees qwen2.5:0.5b while a private server with
# only the login environment defaults to the empty ~/.ollama/models
# and returns model-not-found on every leg. This runner discovers
# the service OLLAMA_MODELS and passes it to both private legs;
# the store is read-only for this runner, :11434 never touched.
# run_z085.sh - Z-085 Python and Ollama-class workload runner
# (MimirAlloc Z13). House perf pattern: standalone, Makefile
# untouched. Runs a real CPython workload (import storm +
# allocation churn, tests/perf/workload_z085.py) and a short
# Ollama generation (qwen2.5:0.5b on a dedicated server, port
# 11499, never the user's default 11434 instance), each plain
# (glibc) and under LD_PRELOAD of the Z-082 interposing build
# (build/libmimiralloc_interpose.so). Correctness: every Python
# rep must exit 0 with an identical CHECKSUM; every Ollama leg
# must return a well-formed generation. Load state is recorded
# before and after; this host is shared (Aesir forge, Lifthrasir
# forge, Tensile lab), so contention present during the run is
# named in the output, never hidden.
set -u
cd "$(dirname "$0")/../.." || exit 2
PORT=11499
MODEL=qwen2.5:0.5b
FAIL=0
echo "=== Z-085 Python and Ollama-class workload ==="
date -Is; uname -a; git rev-parse HEAD
python3 --version; ollama --version
echo "--- load (start) ---"; uptime; ps aux --sort=-%cpu | head -6
echo "--- build interpose library ---"
make lib-interpose || exit 1
ls -l build/libmimiralloc_interpose.so
sha256sum tests/perf/workload_z085.py tests/perf/run_z085.sh build/libmimiralloc_interpose.so
IPOLIB="$PWD/build/libmimiralloc_interpose.so"
MODELS_DIR="${OLLAMA_MODELS:-}"
if [ -z "$MODELS_DIR" ]; then
  MODELS_DIR=$(systemctl show ollama -p Environment 2>/dev/null | grep -o "OLLAMA_MODELS=[^ ]*" | head -1 | cut -d= -f2-)
fi
if [ -z "$MODELS_DIR" ] || [ ! -d "$MODELS_DIR" ]; then
  if [ -d /mnt/vault/models/ollama ]; then MODELS_DIR=/mnt/vault/models/ollama; else MODELS_DIR="$HOME/.ollama/models"; fi
fi
export OLLAMA_MODELS="$MODELS_DIR"
echo "ollama models dir: $OLLAMA_MODELS"
mkdir -p hidden_files/z085_reps
run_py() {
  mode=$1; rep=$2
  out="hidden_files/z085_reps/py_${mode}_${rep}.log"
  if [ "$mode" = preload ]; then
    LD_PRELOAD="$IPOLIB" python3 tests/perf/workload_z085.py > "$out" 2>&1
  else
    python3 tests/perf/workload_z085.py > "$out" 2>&1
  fi
  rc=$?
  echo "py $mode rep$rep exit=$rc"; cat "$out"
  [ "$rc" -eq 0 ] || FAIL=1
}
for r in 1 2 3; do run_py plain $r; done
for r in 1 2 3; do run_py preload $r; done
SUM=$(grep -h CHECKSUM hidden_files/z085_reps/py_*.log | sort -u | wc -l)
echo "distinct python checksums across 6 reps: $SUM (must be 1)"
[ "$SUM" -eq 1 ] || FAIL=1
grep -h "TOTAL" hidden_files/z085_reps/py_plain_*.log | awk '{print "py plain TOTAL", $2}'
grep -h "TOTAL" hidden_files/z085_reps/py_preload_*.log | awk '{print "py preload TOTAL", $2}'
grep -h "PEAK" hidden_files/z085_reps/py_plain_*.log | awk '{print "py plain PEAK_KiB", $2}'
grep -h "PEAK" hidden_files/z085_reps/py_preload_*.log | awk '{print "py preload PEAK_KiB", $2}'
ollama_leg() {
  mode=$1
  log="hidden_files/z085_reps/ollama_${mode}.log"
  resp="hidden_files/z085_reps/ollama_${mode}.json"
  if [ "$mode" = preload ]; then
    LD_PRELOAD="$IPOLIB" OLLAMA_MODELS="$MODELS_DIR" OLLAMA_HOST="127.0.0.1:$PORT" ollama serve > "$log" 2>&1 &
  else
    OLLAMA_MODELS="$MODELS_DIR" OLLAMA_HOST="127.0.0.1:$PORT" ollama serve > "$log" 2>&1 &
  fi
  srv=$!
  i=0
  while [ $i -lt 60 ]; do
    if curl -s "http://127.0.0.1:$PORT/api/tags" > /dev/null 2>&1; then break; fi
    i=$((i+1)); sleep 1
  done
  echo "ollama $mode server pid=$srv ready_after=${i}s"
  tags="hidden_files/z085_reps/ollama_${mode}_tags.json"
  curl -s "http://127.0.0.1:$PORT/api/tags" > "$tags" 2>/dev/null; cat "$tags"; echo
  grep -q "$MODEL" "$tags" || { echo "ollama $mode MODEL NOT VISIBLE: $MODEL"; FAIL=1; }
  t0=$(date +%s.%N)
  curl -s -X POST "http://127.0.0.1:$PORT/api/generate" -d "{\"model\":\"$MODEL\",\"prompt\":\"The capital of France is\",\"stream\":false,\"options\":{\"num_predict\":16,\"seed\":42,\"temperature\":0}}" > "$resp"
  rc=$?
  t1=$(date +%s.%N)
  echo "ollama $mode generate curl_exit=$rc wall=$(echo "$t1 $t0" | awk '{printf "%.3f", $1-$2}')s"
  cat "$resp"; echo
  rss=$(ps -o rss= -p "$srv" 2>/dev/null | tr -d ' ')
  crss=$(ps --ppid "$srv" -o rss= 2>/dev/null | awk '{s+=$1} END {print s+0}')
  echo "ollama $mode server_rss_KiB=${rss:-NA} children_rss_KiB=$crss"
  grep -q '"done":true' "$resp" || FAIL=1
  grep -q '"response":"' "$resp" || FAIL=1
  pkill -P "$srv" 2>/dev/null; kill "$srv" 2>/dev/null; wait "$srv" 2>/dev/null
  sleep 2
  if curl -s "http://127.0.0.1:$PORT/api/tags" > /dev/null 2>&1; then echo "ollama $mode WARNING port still answering"; else echo "ollama $mode server stopped, port free"; fi
}
ollama_leg plain
ollama_leg preload
echo "--- load (end) ---"; uptime
echo "Z-085 runner correctness: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL)"
echo "--- run_z085 done ---"
exit $FAIL
