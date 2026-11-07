#!/bin/sh
# run_l005.sh - L-005 Phase 12 tokenizer + KV profile runs.
# Builds probe_l005.c against the library sources (J-002 pattern,
# Makefile NOT modified) and runs it 3x against the real TinyLlama
# GGUF. Then rebuilds the L-003-proven Aesir memory-refinements
# component (native aarch64 Mojo): its tokenizer leg is the REAL
# RuneWeaver BPE code (loader/tokenizer.mojo) running on the
# stdlib heap (route C2); its own PASS lines are the result and
# child peak RSS is a cross-check only. Bounded throughout.
set -u
cd "$(dirname "$0")/../.." || exit 2
MODEL=/mnt/yggdrasil/models/tinyllama/tinyllama-1.1b-chat-v1.0.Q4_0.gguf
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 -DMIMIR_DEBUG=1"
echo "--- build probe_l005 ---"
echo "cc $CFLAGS tests/perf/probe_l005.c src/api/mimir_alloc.c src/core/mimir_core.c src/platform/mimir_platform.c -o /tmp/probe_l005"
# shellcheck disable=SC2086
cc $CFLAGS tests/perf/probe_l005.c src/api/mimir_alloc.c \
    src/core/mimir_core.c src/platform/mimir_platform.c \
    -o /tmp/probe_l005 || exit 1
echo "build exit=0"
sha256sum tests/perf/probe_l005.c /tmp/probe_l005
for i in 1 2 3; do
    echo "--- probe rep $i: timeout 120 /tmp/probe_l005 $MODEL ---"
    timeout 120 /tmp/probe_l005 "$MODEL"
    echo "rep $i exit=$?"
done
echo "--- Aesir component leg (L-003 workload, rebuilt) ---"
AESIR=/home/pi/c2/aesir_engine
if command -v mojo >/dev/null 2>&1; then
    echo "mojo: $(mojo --version 2>&1 | head -1)"
    ( cd "$AESIR" && timeout 260 mojo build \
        tests/test_memory_refinements.mojo -o /tmp/l005_tmr )
    echo "mojo build exit=$?"
    sha256sum /tmp/l005_tmr
    python3 - <<'PYEOF'
import resource, subprocess
p = subprocess.run(["/tmp/l005_tmr"], capture_output=True,
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
echo "--- run_l005 done ---"
