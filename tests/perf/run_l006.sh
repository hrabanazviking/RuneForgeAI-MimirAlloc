#!/bin/sh
# run_l006.sh - L-006 Phase 12 prefill/decode/teardown profile runs.
#
# Builds tests/perf/probe_l006.c against the library objects'
# sources (the J-002 pattern, as run_l004.sh: the stats snapshot
# entry point is internal, reachable only by linking the objects
# directly - the Makefile is NOT modified by this slice) and runs
# it 3 times against the real TinyLlama GGUF. Then attempts the
# Aesir inference-path component leg (tests/test_kv_cache.mojo,
# native aarch64 Mojo build): on this Pi that build is BLOCKED by
# the banked INC-L003-02 stdlib skew, so the leg records its exact
# build exit and first error lines as OWED evidence and does NOT
# fail this driver - claiming it ran would be fabrication.
#
# Everything is bounded: timeout on every run, no soak.
# Output is the raw evidence; the recorded run file adds the
# standard header (date, base, thermals, Samtal) around it.
set -u
cd "$(dirname "$0")/../.." || exit 2
MODEL=/mnt/yggdrasil/models/tinyllama/tinyllama-1.1b-chat-v1.0.Q4_0.gguf
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 -DMIMIR_DEBUG=1"
echo "--- build probe_l006 ---"
echo "cc $CFLAGS tests/perf/probe_l006.c src/api/mimir_alloc.c src/core/mimir_core.c src/platform/mimir_platform.c -o /tmp/probe_l006"
# shellcheck disable=SC2086
cc $CFLAGS tests/perf/probe_l006.c src/api/mimir_alloc.c \
    src/core/mimir_core.c src/platform/mimir_platform.c \
    -o /tmp/probe_l006 || exit 1
echo "build exit=0"
sha256sum tests/perf/probe_l006.c /tmp/probe_l006
rc=0
for i in 1 2 3; do
    echo "--- probe rep $i: timeout 120 /tmp/probe_l006 $MODEL ---"
    timeout 120 /tmp/probe_l006 "$MODEL"
    e=$?
    echo "rep $i exit=$e"
    [ "$e" -eq 0 ] || rc=1
done
echo "--- Aesir KV-cache component leg (test_kv_cache.mojo) ---"
AESIR=/home/pi/c2/aesir_engine
if command -v mojo >/dev/null 2>&1; then
    echo "mojo: $(mojo --version 2>&1 | head -1)"
    ( cd "$AESIR" && timeout 260 mojo build \
        tests/test_kv_cache.mojo -o /tmp/l006_kv ) > /tmp/l006_kv_build.log 2>&1
    echo "kv component build exit=$?"
    tail -6 /tmp/l006_kv_build.log
    if [ -f /tmp/l006_kv ]; then
        file /tmp/l006_kv
        timeout 60 /tmp/l006_kv
        echo "kv component run exit=$?"
    else
        echo "kv component: NO BINARY - leg BLOCKED (INC-L003-02), recorded owed"
    fi
else
    echo "mojo NOT FOUND - component leg UNAVAILABLE (owed)"
fi
echo "--- run_l006 done rc=$rc ---"
exit $rc
