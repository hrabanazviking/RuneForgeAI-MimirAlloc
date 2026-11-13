#!/usr/bin/env bash
# Z-094 capture driver: runs the three real workloads
# under the audit-hook tracer on THIS host and summarises
# each into its canonical in-tree trace. Capture is a
# one-time, recorded act (the traces are the artefact);
# tests/perf/run_z094.sh verifies the banked traces by
# replay and never re-captures.
#
# Usage: bash tests/perf/capture_z094.sh [WORKDIR]
#   WORKDIR defaults to a mktemp dir under /tmp; raw
#   streams and per-workload logs land there and in
#   hidden_files/z094_capture.log. Canonical traces land
#   in tests/perf/traces/.
set -u
cd "$(dirname "$0")/../.."
REPO="$PWD"
WORK="${1:-$(mktemp -d /tmp/z094_capture.XXXXXX)}"
mkdir -p "$WORK" "$REPO/tests/perf/traces" hidden_files
LOG=hidden_files/z094_capture.log
SHIM="$REPO/build/libmimirtrace_z094.so"
{
  echo "# Z-094 capture: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "# host: $(uname -a)"
  echo "# head: $(git rev-parse HEAD)"
  echo "# loadavg_start: $(cat /proc/loadavg)"
  echo "# workdir: $WORK"
} > "$LOG"

make trace-lib-z094 >> "$LOG" 2>&1 || { echo "SHIM BUILD FAILED" | tee -a "$LOG"; exit 2; }

CAPTURED="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
LOADAVG="$(cat /proc/loadavg)"

# ---- workload 1: python import storm ----
PYRAW="$WORK/python_raw"; mkdir -p "$PYRAW"
MIMIR_TRACE_DIR="$PYRAW" LD_PRELOAD="$SHIM" \
  python3 -c '
import json, csv, xml.etree.ElementTree, sqlite3, gzip, zipfile
import http.client, urllib.request, email, logging, unittest
import decimal, fractions, hashlib, hmac, base64, binascii
import collections, itertools, functools, dataclasses
import asyncio, threading, multiprocessing, subprocess
import re, random, statistics, textwrap, difflib, pprint
data = [{"k": i, "v": [j for j in range(20)]} for i in range(2000)]
s = json.dumps(data); back = json.loads(s)
assert len(back) == 2000
import io
buf = io.BytesIO()
with gzip.GzipFile(fileobj=buf, mode="wb") as g:
    g.write(s.encode() * 4)
blob = gzip.decompress(buf.getvalue())
assert len(blob) == len(s) * 4
con = sqlite3.connect(":memory:")
con.execute("create table t (a integer, b text)")
con.executemany("insert into t values (?, ?)",
                [(i, "row-%d" % i) for i in range(5000)])
n = con.execute("select count(*) from t").fetchone()[0]
assert n == 5000
print("PYWORKLOAD OK", len(s), n)
' >> "$LOG" 2>&1
echo "# python exit: $?" >> "$LOG"
python3 tests/perf/summarize_z094.py --workload python_import_storm \
  --command "python3 -c <stdlib import storm + json/gzip/sqlite work>" \
  --captured "$CAPTURED" --loadavg "$LOADAVG" \
  --out tests/perf/traces/z094_python_import.trace.gz \
  "$PYRAW"/trace.*.raw >> "$LOG" 2>&1 || { echo "PY SUMMARIZE FAILED" | tee -a "$LOG"; exit 2; }

# ---- workload 2: git object churn ----
GITRAW="$WORK/git_raw"; mkdir -p "$GITRAW"
GREPO="$WORK/git_repo"; mkdir -p "$GREPO"
(
  export MIMIR_TRACE_DIR="$GITRAW" LD_PRELOAD="$SHIM"
  cd "$GREPO" || exit 2
  git init -q -b main . || exit 2
  git config user.email z094@example.invalid
  git config user.name "Z-094 trace capture"
  python3 - <<'PYEOF'
import subprocess, pathlib
root = pathlib.Path(".")
for i in range(150):
    (root / "mod_a.txt").write_text(f"alpha {i}\n" + "x" * (i % 50) + "\n")
    (root / "mod_b.txt").write_text(f"beta {i}\n" + "y" * (i % 70) + "\n")
    if i % 10 == 0:
        (root / f"obj_{i:03d}.txt").write_text(f"object {i}\n" * (i + 1))
    subprocess.run(["git", "add", "-A"], check=True)
    subprocess.run(["git", "commit", "-q", "-m", f"churn {i}"], check=True)
print("GITWORKLOAD commits done")
PYEOF
  git log --oneline | head -5
  git gc --aggressive -q || exit 2
  git repack -a -d -q || exit 2
  git fsck --strict || exit 2
  git count-objects -v
) >> "$LOG" 2>&1
echo "# git exit: $?" >> "$LOG"
python3 tests/perf/summarize_z094.py --workload git_object_churn \
  --command "git init + 150 commits + gc --aggressive + repack + fsck (traced process tree)" \
  --captured "$CAPTURED" --loadavg "$LOADAVG" \
  --out tests/perf/traces/z094_git_churn.trace.gz \
  "$GITRAW"/trace.*.raw >> "$LOG" 2>&1 || { echo "GIT SUMMARIZE FAILED" | tee -a "$LOG"; exit 2; }

# ---- workload 3: an Ollama session ----
OLLRAW="$WORK/ollama_raw"; mkdir -p "$OLLRAW"
PORT=11499
# The model store is NOT the default ~/.ollama/models:
# the system service override (ollama.service.d) sets
# OLLAMA_MODELS=/mnt/vault/models/ollama, and a private
# serve without it reports every model not found (the
# first Z-094 capture attempt failed exactly there).
OLLAMA_MODELS_DIR=/mnt/vault/models/ollama
if (exec 3<>"/dev/tcp/127.0.0.1/$PORT") 2>/dev/null; then
  echo "OLLAMA PORT $PORT BUSY - capture refused" | tee -a "$LOG"; exit 2
fi
OLLAMA_HOST="127.0.0.1:$PORT" OLLAMA_MODELS="$OLLAMA_MODELS_DIR" MIMIR_TRACE_DIR="$OLLRAW" \
  LD_PRELOAD="$SHIM" ollama serve > "$WORK/ollama_serve.log" 2>&1 &
SERVE_PID=$!
READY=0
for i in $(seq 1 60); do
  if curl -s "http://127.0.0.1:$PORT/api/version" >> "$LOG" 2>&1; then
    READY=1; echo "" >> "$LOG"; break
  fi
  sleep 1
done
if [ "$READY" != 1 ]; then
  echo "OLLAMA SERVER DID NOT START" | tee -a "$LOG"
  kill "$SERVE_PID" 2>/dev/null; exit 2
fi
curl -s -X POST "http://127.0.0.1:$PORT/api/generate" -d '{
  "model": "qwen2.5:0.5b",
  "prompt": "Say one word about the sea.",
  "stream": false,
  "options": {"num_predict": 12}
}' > "$WORK/ollama_response.json" 2>> "$LOG"
echo "# ollama generate curl exit: $?" >> "$LOG"
python3 -c '
import json, sys
d = json.load(open(sys.argv[1]))
print("OLLAMA response:", repr(d.get("response", ""))[:120],
      "eval_count:", d.get("eval_count"), "done:", d.get("done_reason"))
' "$WORK/ollama_response.json" >> "$LOG" 2>&1
python3 -c '
import json, sys
d = json.load(open(sys.argv[1]))
ok = d.get("done") is True and (d.get("eval_count") or 0) > 0
print("OLLAMA session check:", "OK" if ok else "FAILED",
      d.get("done_reason"), d.get("eval_count"))
sys.exit(0 if ok else 1)
' "$WORK/ollama_response.json" >> "$LOG" 2>&1 || { echo "OLLAMA SESSION DID NOT GENERATE - capture refused" | tee -a "$LOG"; kill "$SERVE_PID" 2>/dev/null; exit 2; }
curl -s "http://127.0.0.1:$PORT/api/ps" >> "$LOG" 2>&1; echo "" >> "$LOG"
kill "$SERVE_PID" 2>/dev/null
for i in $(seq 1 30); do
  pgrep -f "llama-server" >/dev/null 2>&1 || break
  sleep 1
done
pkill -f "11499" 2>/dev/null
sleep 2
ls -la "$OLLRAW" >> "$LOG" 2>&1
python3 tests/perf/summarize_z094.py --workload ollama_session \
  --command "private ollama serve (127.0.0.1:11499) + POST /api/generate qwen2.5:0.5b num_predict=12 (server + llama-server runner streams)" \
  --captured "$CAPTURED" --loadavg "$LOADAVG" \
  --out tests/perf/traces/z094_ollama_session.trace.gz \
  "$OLLRAW"/trace.*.raw >> "$LOG" 2>&1 || { echo "OLLAMA SUMMARIZE FAILED" | tee -a "$LOG"; exit 2; }

{
  echo "# loadavg_end: $(cat /proc/loadavg)"
  echo "# capture workdir kept at: $WORK"
  echo "CAPTURE RESULT: DONE"
} >> "$LOG"
cat "$LOG"
