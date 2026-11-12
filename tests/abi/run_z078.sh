#!/usr/bin/env bash
# Z-078 ABI recorded run: builds the library and the
# three consumers from clean, files the E-series style
# proofs (export list unchanged at exactly ten, absence
# checks, DT_NEEDED, undefined symbols, header-only
# compiles) and runs every consumer. The slice adds NO
# public symbol: its proofs pin the export list as the
# same ten names Z-077 banked. Output is the recorded
# run itself; the caller tees it to
# tests/abi/recorded_run_z13_z078.txt. Exit 0 iff every
# proof and consumer passes.
set -u
cd "$(dirname "$0")/../.."
FAIL=0
note() { echo "$*"; }
chk() { if [ "$1" -eq 0 ]; then echo "  [ok] $2"; else echo "  [FAIL] $2"; FAIL=1; fi; }

echo "=== Z-078 ABI recorded run ==="
echo "date_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "host: $(uname -a)"
echo "cc: $(cc --version | head -1)"
echo "cxx: $(c++ --version | head -1)"
echo "head: $(git rev-parse HEAD)"
echo "branch: $(git branch --show-current)"
echo "loadavg: $(cat /proc/loadavg)"
echo "page_size: $(getconf PAGESIZE)"

echo "=== S1 build from clean ==="
make clean >/dev/null 2>&1
make lib build/test_z078 build/test_z078_dyn build/test_z078_cpp build/header_only_z078_c.o build/header_only_z078_cpp.o > hidden_files/z078_build.log 2>&1
chk $? "make lib + z078 consumers (log hidden_files/z078_build.log)"

echo "=== S2 shared library exports (nm -D) - exactly the 10 public symbols, unchanged by Z-078 ==="
nm -D --defined-only build/libmimiralloc.so
echo "defined dynamic symbol count: $(nm -D --defined-only build/libmimiralloc.so | wc -l)"
[ "$(nm -D --defined-only build/libmimiralloc.so | wc -l)" -eq 10 ]
chk $? "count == 10"
for sym in mimir_malloc mimir_free mimir_calloc mimir_realloc mimir_aligned_alloc mimir_posix_memalign mimir_usable_size mimir_stats_snapshot mimir_trim mimir_reallocarray; do
  nm -D --defined-only build/libmimiralloc.so | grep -q " T $sym\$"
  chk $? "exported: $sym"
done

echo "=== S3 absence proofs - no internal symbol is exported ==="
if nm -D --defined-only build/libmimiralloc.so | grep -E 'mimir_core_|mimir_platform_|mimir_audit_' ; then
  chk 1 "no mimir_core_/mimir_platform_/mimir_audit_ exports"
else
  chk 0 "no mimir_core_/mimir_platform_/mimir_audit_ exports"
fi

echo "=== S4 consumers bind to the .so (DT_NEEDED + undefined symbols) ==="
for exe in test_z078_dyn test_z078_cpp; do
  echo "-- $exe --"
  readelf -d "build/$exe" | grep NEEDED
  readelf -d "build/$exe" | grep -q 'NEEDED.*libmimiralloc\.so'
  chk $? "$exe DT_NEEDED names libmimiralloc.so"
  nm "build/$exe" | grep -E ' U (mimir_malloc|mimir_calloc|mimir_realloc|mimir_reallocarray|mimir_aligned_alloc|mimir_posix_memalign)$'
  chk $? "$exe references the allocation-family symbols as undefined (U)"
  if nm "build/$exe" | grep -qE ' T (mimir_malloc|mimir_posix_memalign)$'; then
    chk 1 "$exe does not define the family symbols (not statically absorbed)"
  else
    chk 0 "$exe does not define the family symbols (not statically absorbed)"
  fi
done
echo "-- C++ consumer object: unmangled references --"
nm build/test_z078_cpp.o | grep -E ' U (mimir_malloc|mimir_calloc|mimir_realloc|mimir_reallocarray|mimir_aligned_alloc|mimir_posix_memalign)$'
chk $? "test_z078_cpp.o undefined references are unmangled (extern C guards cover the declarations)"

echo "=== S5 header-only translation units (compile-only) ==="
ls -la build/header_only_z078_c.o build/header_only_z078_cpp.o
chk $? "header-only C and C++ objects built (header self-contained, exact prototypes)"

echo "=== S6 static contract test ==="
./build/test_z078
chk $? "test_z078 exit 0"

echo "=== S7 dynamic C consumer ==="
./build/test_z078_dyn
chk $? "test_z078_dyn exit 0"

echo "=== S8 C++ consumer ==="
./build/test_z078_cpp
chk $? "test_z078_cpp exit 0"

echo "=== S9 neighbour evidence: E-005 audit readers ==="
make test-e005 > hidden_files/z078_test_e005.log 2>&1
chk $? "make test-e005 still passes (log hidden_files/z078_test_e005.log)"

echo "=== Z-078 ABI RUN RESULT: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL) ==="
exit $FAIL
