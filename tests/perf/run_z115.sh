#!/bin/sh
# run_z115.sh - Z-115 Aesir KV-cache-shaped allocation
# specialisation runner. WAVE_PLAN Wave 13 pool choice:
# KV-cache-shaped allocation pattern measured on this
# allocator vs both glibc columns (default and tuned, per
# Z-098/Z-107 practice), Aesir tree READ-ONLY throughout,
# Z-087 blockers B-087-1/2/3 re-checked against the run.
# The pattern is SYNTHETIC (tests/perf/probe_z115.c): the
# blockers still stand, so no Aesir run is claimed. The
# tuned column is the Z-098 gcombo_cache string
# (tcache_count=64:tcache_max=65536), the tuned winner for
# the large-block oracle shapes at Z-098/Z-107; its channel
# is proven live by the banked probe_z098 engagement proof
# before any number is taken. Legs interleaved per round,
# taskset -c 5, 5 rounds. Boundary: aesir git status and
# blocker artifact hashes are snapshotted before and after
# from this runner's own reads; nothing writes there.
# Usage: run_z115.sh <raw-output-file> [rounds]
set -u
OUT=${1:?usage: run_z115.sh <raw-output-file> [rounds]}
ROUNDS=${2:-5}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
HF="$ROOT/hidden_files"
mkdir -p "$HF" "$ROOT/build"
GBIN="$ROOT/build/probe_z115_glibc"
MBIN="$ROOT/build/probe_z115_mimir"
PBIN="$ROOT/build/probe_z115_z098"
TUNED="glibc.malloc.tcache_count=64:glibc.malloc.tcache_max=65536"
FAIL=0
cd "$ROOT"
echo "=== Z-115 KV-cache-shaped allocation specialisation ==="
date -Is; uname -a; git rev-parse HEAD; git branch --show-current
cc --version | head -1
echo "--- load (start) ---"; uptime; cat /proc/loadavg
ps aux --sort=-%cpu | head -6
scripts/z13/host_truth.sh | tee "$HF/z115_host_truth.txt"
echo "--- build ---"
BASE="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc"
[ -f build/mimir_alloc.o ] || make bench-k002 > "$HF/z115_build.log" 2>&1
cc $BASE -O2 -DLEG_GLIBC tests/perf/probe_z115.c -o "$GBIN" >> "$HF/z115_build.log" 2>&1 || { echo BUILD FAIL glibc; exit 1; }
cc $BASE -O2 tests/perf/probe_z115.c build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o -o "$MBIN" >> "$HF/z115_build.log" 2>&1 || { echo BUILD FAIL mimir; exit 1; }
cc -std=c11 -Wall -Wextra -Werror -O2 tests/perf/probe_z098.c -o "$PBIN" >> "$HF/z115_build.log" 2>&1 || { echo BUILD FAIL z098; exit 1; }
sha256sum tests/perf/probe_z115.c tests/perf/run_z115.sh tests/perf/summarize_z115.py "$GBIN" "$MBIN" | tee "$HF/z115_sha.txt"
echo "--- tunable engagement proof (probe_z098) ---"
hblk_of() { echo "$1" | sed -n 's/.*hblkhd_delta=\(-*[0-9]*\).*/\1/p'; }
OUT_DEF=$(env -u GLIBC_TUNABLES taskset -c 5 "$PBIN") || FAIL=1
OUT_MM4K=$(GLIBC_TUNABLES="glibc.malloc.mmap_threshold=4096" taskset -c 5 "$PBIN") || FAIL=1
OUT_MM1M=$(GLIBC_TUNABLES="glibc.malloc.mmap_threshold=1048576" taskset -c 5 "$PBIN") || FAIL=1
echo "default: $OUT_DEF"; echo "mm4k: $OUT_MM4K"; echo "mm1m: $OUT_MM1M"
H_DEF=$(hblk_of "$OUT_DEF"); H_4K=$(hblk_of "$OUT_MM4K"); H_1M=$(hblk_of "$OUT_MM1M")
echo "hblkhd deltas: default=$H_DEF mm4k=$H_4K mm1m=$H_1M"
[ "$H_DEF" = "0" ] || { echo "PROOF FAIL default"; FAIL=1; }
[ "$H_1M" = "0" ] || { echo "PROOF FAIL mm1m"; FAIL=1; }
[ "${H_4K:-0}" -ge 60000 ] || { echo "PROOF FAIL mm4k"; FAIL=1; }
[ $FAIL -eq 0 ] && echo "engagement proof: PASS (tunable channel live)" || echo "engagement proof: FAIL"
[ $FAIL -eq 0 ] || exit 1
echo "--- Aesir boundary + blocker re-check (READ-ONLY) ---"
AESIR=/home/volmarr/aesir-forge
git -C "$AESIR" status --porcelain > "$HF/z115_aesir_status_runner_before.txt" 2>&1
echo "aesir head: $(git -C $AESIR rev-parse HEAD)"
echo "aesir status lines before: $(wc -l < $HF/z115_aesir_status_runner_before.txt)"
sha256sum "$AESIR/aesir_main" "$AESIR/aesir_engine/aesir_main" | tee "$HF/z115_blocker_hashes.txt"
ls -la "$AESIR/aesir_main" "$AESIR/aesir_engine/aesir_main" "$AESIR/aesir_engine/model.gguf" | tee -a "$HF/z115_blocker_hashes.txt"
echo "fixture: $(ls -la $AESIR/hidden_files/fixtures/Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf)"
echo "--- KV battery (legs interleaved, taskset -c 5) ---"
: > "$OUT"; : > "$OUT.err"
reps_for() { case "$1" in kv_append) echo 5 ;; kv_blocks) echo 20 ;; kv_mixed) echo 5 ;; esac; }
for SH in kv_append kv_blocks kv_mixed; do
    R=$(reps_for "$SH")
    p=1
    while [ "$p" -le "$ROUNDS" ]; do
        env -u GLIBC_TUNABLES taskset -c 5 "$MBIN" "$SH" "$R" >> "$OUT" 2>> "$OUT.err"
        env -u GLIBC_TUNABLES taskset -c 5 "$GBIN" "$SH" "$R" >> "$OUT" 2>> "$OUT.err"
        GLIBC_TUNABLES="$TUNED" taskset -c 5 "$GBIN" "$SH" "$R" | sed 's/^glibc,/glibc_tuned,/' >> "$OUT" 2>> "$OUT.err"
        p=$((p + 1))
    done
    echo "loadavg after $SH: $(cat /proc/loadavg)"
done
echo "probe stderr bytes: $(wc -c < $OUT.err)"; rm -f "$OUT.err"
python3 tests/perf/summarize_z115.py "$OUT" "$ROUNDS" || FAIL=1
echo "--- Aesir boundary after ---"
git -C "$AESIR" status --porcelain > "$HF/z115_aesir_status_runner_after.txt" 2>&1
if diff -q "$HF/z115_aesir_status_runner_before.txt" "$HF/z115_aesir_status_runner_after.txt" > /dev/null; then echo "aesir git status: UNCHANGED across the battery"; else echo "aesir git status: CHANGED (co-tenant campaign live; diff in hidden_files)"; diff "$HF/z115_aesir_status_runner_before.txt" "$HF/z115_aesir_status_runner_after.txt" | head -20; fi
sha256sum "$AESIR/aesir_main" "$AESIR/aesir_engine/aesir_main"
echo "--- load (end) ---"; uptime; cat /proc/loadavg
ps aux --sort=-%cpu | head -6
echo "Z-115 runner correctness: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL)"
echo "--- run_z115 done ---"
exit $FAIL
