#!/usr/bin/env bash
# Z-083 ABI recorded run: builds the library and the three
# consumers from clean, files the E-series style proofs
# (export list, absence checks, DT_NEEDED, undefined
# symbols, header-only compiles), runs every consumer,
# and files the host glibc allocation-symbol inventory
# the Z-083 gap list is built from. Output is the
# recorded run itself; the caller tees it to
# tests/abi/recorded_run_z13_z083.txt. Exit 0 iff every
# proof and consumer passes.
set -u
cd "$(dirname "$0")/../.."
FAIL=0
note() { echo "$*"; }
chk() { if [ "$1" -eq 0 ]; then echo "  [ok] $2"; else echo "  [FAIL] $2"; FAIL=1; fi; }

echo "=== Z-083 ABI recorded run ==="
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
make lib build/test_z083 build/test_z083_dyn build/test_z083_cpp build/header_only_z083_c.o build/header_only_z083_cpp.o > hidden_files/z083_build.log 2>&1
chk $? "make lib + z083 consumers (log hidden_files/z083_build.log)"

echo "=== S2 shared library exports (nm -D) - exactly the 13 public symbols ==="
nm -D --defined-only build/libmimiralloc.so
echo "defined dynamic symbol count: $(nm -D --defined-only build/libmimiralloc.so | wc -l)"
[ "$(nm -D --defined-only build/libmimiralloc.so | wc -l)" -eq 13 ]
chk $? "count == 13"
for sym in mimir_malloc mimir_free mimir_calloc mimir_realloc mimir_aligned_alloc mimir_posix_memalign mimir_usable_size mimir_stats_snapshot mimir_trim mimir_reallocarray mimir_memalign mimir_valloc mimir_pvalloc; do
  nm -D --defined-only build/libmimiralloc.so | grep -q " T $sym\$"
  chk $? "exported: $sym"
done

echo "=== S3 absence proofs - no internal or standard-name symbol is exported ==="
if nm -D --defined-only build/libmimiralloc.so | grep -E 'mimir_core_|mimir_platform_|mimir_audit_' ; then
  chk 1 "no mimir_core_/mimir_platform_/mimir_audit_ exports"
else
  chk 0 "no mimir_core_/mimir_platform_/mimir_audit_ exports"
fi
if nm -D --defined-only build/libmimiralloc.so | grep -E ' T (malloc|free|calloc|realloc|memalign|valloc|pvalloc|strdup)$' ; then
  chk 1 "no standard-name exports (they belong to the Z-082 opt-in variant)"
else
  chk 0 "no standard-name exports (they belong to the Z-082 opt-in variant)"
fi

echo "=== S4 consumers bind to the .so (DT_NEEDED + undefined symbols) ==="
for exe in test_z083_dyn test_z083_cpp; do
  echo "-- $exe --"
  readelf -d "build/$exe" | grep NEEDED
  readelf -d "build/$exe" | grep -q 'NEEDED.*libmimiralloc\.so'
  chk $? "$exe DT_NEEDED names libmimiralloc.so"
  nm "build/$exe" | grep -E ' U (mimir_memalign|mimir_valloc|mimir_pvalloc)$'
  chk $? "$exe references the three new symbols as undefined (U)"
  if nm "build/$exe" | grep -qE ' T (mimir_memalign|mimir_valloc|mimir_pvalloc)$'; then
    chk 1 "$exe does not define the new symbols (not statically absorbed)"
  else
    chk 0 "$exe does not define the new symbols (not statically absorbed)"
  fi
done
echo "-- C++ consumer object: unmangled references --"
nm build/test_z083_cpp.o | grep -E ' U (mimir_memalign|mimir_valloc|mimir_pvalloc)$'
chk $? "test_z083_cpp.o undefined references are unmangled (extern C guards cover the new declarations)"

echo "=== S5 header-only translation units (compile-only) ==="
ls -la build/header_only_z083_c.o build/header_only_z083_cpp.o
chk $? "header-only C and C++ objects built (header self-contained, exact prototypes)"

echo "=== S6 static contract test ==="
./build/test_z083
chk $? "test_z083 exit 0"

echo "=== S7 dynamic C consumer ==="
./build/test_z083_dyn
chk $? "test_z083_dyn exit 0"

echo "=== S8 C++ consumer ==="
./build/test_z083_cpp
chk $? "test_z083_cpp exit 0"

echo "=== S9 neighbour evidence: E-005 audit readers ==="
make test-e005 > hidden_files/z083_test_e005.log 2>&1
chk $? "make test-e005 still passes (log hidden_files/z083_test_e005.log)"

echo "=== S10 host glibc allocation-symbol inventory (gap-list evidence) ==="
echo "libc: $(ldd --version | head -1) at $(readlink -f /lib/x86_64-linux-gnu/libc.so.6)"
nm -D --defined-only /lib/x86_64-linux-gnu/libc.so.6 | awk '{print $NF}' | sed 's/@.*//' | sort -u | grep -E '^(malloc|free|calloc|realloc|reallocarray|memalign|valloc|pvalloc|posix_memalign|aligned_alloc|malloc_usable_size|memalignment|malloc_trim|mallopt|mallinfo|mallinfo2|malloc_stats|malloc_info|cfree|free_sized|free_aligned_sized|strdup|strndup|__libc_(malloc|free|calloc|realloc|memalign|valloc|pvalloc|reallocarray|mallinfo|mallopt))$'
chk $? "glibc inventory listed"

echo "=== Z-083 ABI RUN RESULT: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL) ==="
exit $FAIL
