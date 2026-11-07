#!/bin/sh
# run_l004.sh - L-004 Phase 12 load + metadata tracking runs.
#
# Builds tests/perf/probe_l004.c against the library objects'
# sources (the J-002 pattern: the stats snapshot entry point is
# internal, reachable only by linking the objects directly -
# the Makefile is NOT modified by this slice) and runs it 3
# times against the real TinyLlama GGUF. Then rebuilds and
# reruns the L-003-proven Aesir memory-refinements component
# (native aarch64 Mojo build) as the C2-side workload leg:
# MimirWell's ONE stdlib arena allocation + the tokenizer heap
# live inside that process; its own PASS lines are the result,
# and its peak RSS (child rusage) is a cross-check only.
#
# Everything is bounded: timeout on every run, no soak.
# Output is the raw evidence; the recorded run file adds the
# standard header (date, base, thermals, Samtal) around it.
set -u
cd "$(dirname "$0")/../.." || exit 2
MODEL=/mnt/yggdrasil/models/tinyllama/tinyllama-1.1b-chat-v1.0.Q4_0.gguf
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 -DMIMIR_DEBUG=1"
echo "--- build probe_l004 ---"
echo "cc $CFLAGS tests/perf/probe_l004.c src/api/mimir_alloc.c src/core/mimir_core.c src/platform/mimir_platform.c -o /tmp/probe_l004"
# shellcheck disable=SC2086
cc $CFLAGS tests/perf/probe_l004.c src/api/mimir_alloc.c \
    src/core/mimir_core.c src/platform/mimir_platform.c \
    -o /tmp/probe_l004 || exit 1
echo "build exit=0"
sha256sum tests/perf/probe_l004.c /tmp/probe_l004
for i in 1 2 3; do
    echo "--- probe rep $i: timeout 120 /tmp/probe_l004 $MODEL ---"
    timeout 120 /tmp/probe_l004 "$MODEL"
    echo "rep $i exit=$?"
done
echo "--- Aesir component leg (L-003 workload, rebuilt) ---"
AESIR=/home/pi/c2/aesir_engine
if command -v mojo >/dev/null 2>&1; then
    echo "mojo: $(mojo --version 2>&1 | head -1)"
    ( cd "$AESIR" && timeout 260 mojo build \
        tests/test_memory_refinements.mojo -o /tmp/l004_tmr )
    echo "mojo build exit=$?"
    sha256sum /tmp/l004_tmr
    python3 - <<'PYEOF'
import resource, subprocess
p = subprocess.run(["/tmp/l004_tmr"], capture_output=True,
                   text=True, timeout=120)
print(p.stdout, end="")
print("component exit=", p.returncode)
print("component child_ru_maxrss_kib=",
      resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)
PYEOF
    echo "component wrapper exit=$?"
else
    echo "mojo NOT FOUND - component leg UNAVAILABLE (owed)"
fi
echo "--- run_l004 done ---"
