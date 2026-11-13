#!/bin/sh
# run_z093.sh - Z-093 slice runner: prototype probe, build,
# two full bench invocations (7 reps/leg each), checksum
# validation, summaries. Exit 0 only if both invocations
# complete with per-size checksums identical across legs
# and shapes. Logs land in hidden_files/ (team bookkeeping,
# never banked); the recorded run is assembled separately.
set -eu
DIR=$(dirname "$0");ROOT=$(cd "$DIR/../.." && pwd);cd "$ROOT"
HF="$ROOT/hidden_files";mkdir -p "$HF" build
echo "== prototype probe (Z-092 batch API presence) =="
echo "header batch hits: $(grep -c -i batch include/mimir_alloc.h || true)"
grep -i batch include/mimir_alloc.h || echo "(no batch declaration in public header)"
make -s lib >> "$HF/z093_build.log" 2>&1
echo "so batch exports: $(nm -D --defined-only build/libmimiralloc.so | grep -c -i batch || true)"
nm -D --defined-only build/libmimiralloc.so | grep -i batch || echo "(no batch symbol exported by libmimiralloc.so)"
echo "== build =="
make -s build/bench_k007_mimir >> "$HF/z093_build.log" 2>&1
cc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 -flto -DLEG_GLIBC -c tests/perf/bench_z093.c -o build/bench_z093_glibc.o
cc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 -flto -DLEG_GLIBC build/bench_z093_glibc.o -o build/bench_z093_glibc
cc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 -flto -c tests/perf/bench_z093.c -o build/bench_z093_mimir.o
cc -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 -flto build/mimir_alloc.o build/mimir_core.o build/mimir_platform.o build/bench_z093_mimir.o -o build/bench_z093_mimir
echo "build ok"
echo "loadavg before: $(cat /proc/loadavg)"
echo "== invocation 1 =="
./tests/perf/run_bench_z093.sh "$HF/z093_raw1.txt" 7
python3 tests/perf/summarize_z093.py "$HF/z093_raw1.txt" | tee "$HF/z093_sum1.txt"
echo "loadavg mid: $(cat /proc/loadavg)"
echo "== invocation 2 =="
./tests/perf/run_bench_z093.sh "$HF/z093_raw2.txt" 7
python3 tests/perf/summarize_z093.py "$HF/z093_raw2.txt" | tee "$HF/z093_sum2.txt"
echo "loadavg after: $(cat /proc/loadavg)"
echo "== checksum validation =="
python3 - "$HF/z093_raw1.txt" "$HF/z093_raw2.txt" <<'PYEOF'
import sys,collections
ok=True
for path in sys.argv[1:]:
    ck=collections.defaultdict(set)
    for line in open(path):
        p=line.strip().split(',')
        if len(p)!=10: continue
        ck[p[3]].add(p[7])
    for size,s in sorted(ck.items()):
        print(path, "size", size, "checksums", sorted(s))
        if len(s)!=1: ok=False
print("CHECKSUMS OK" if ok else "CHECKSUM MISMATCH")
sys.exit(0 if ok else 1)
PYEOF
echo "Z093 RUNNER PASS"
