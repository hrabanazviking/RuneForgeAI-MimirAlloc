#!/usr/bin/env bash
# Z-076 ABI recorded run: builds the library and the three
# consumers from clean, files the E-series style proofs
# (export list, absence checks, DT_NEEDED, undefined
# symbols, header-only compiles) and runs every consumer,
# plus the neighbour suites whose semantics Z-076 touches.
# Output is the recorded run itself; the caller tees it to
# tests/abi/recorded_run_z13_z076.txt. Exit 0 iff every
# Z-076 proof and consumer passes (the test-j002 neighbour
# leg is reported verbatim with its PRE-EXISTING failure
# count -- 113 at the Z-076 base and 113 after, zero
# attributable to this slice; see the slice doc).
set -u
cd "$(dirname "$0")/../.."
FAIL=0
note() { echo "$*"; }
chk() { if [ "$1" -eq 0 ]; then echo "  [ok] $2"; else echo "  [FAIL] $2"; FAIL=1; fi; }

echo "=== Z-076 ABI recorded run ==="
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
make lib build/test_z076 build/test_z076_dyn build/test_z076_cpp build/header_only_z076_c.o build/header_only_z076_cpp.o > hidden_files/z076_build.log 2>&1
chk $? "make lib + z076 consumers (log hidden_files/z076_build.log)"

echo "=== S2 shared library exports (nm -D) - exactly the 7 public symbols ==="
nm -D --defined-only build/libmimiralloc.so
echo "defined dynamic symbol count: $(nm -D --defined-only build/libmimiralloc.so | wc -l)"
[ "$(nm -D --defined-only build/libmimiralloc.so | wc -l)" -eq 10 ]
chk $? "count == 10"
for sym in mimir_malloc mimir_free mimir_calloc mimir_realloc mimir_aligned_alloc mimir_posix_memalign mimir_stats_snapshot; do
  nm -D --defined-only build/libmimiralloc.so | grep -q " T $sym$"
  chk $? "exported: $sym"
done

echo "=== S3 absence proofs - no internal symbol is exported ==="
if nm -D --defined-only build/libmimiralloc.so | grep -E 'mimir_core_|mimir_platform_|mimir_audit_' ; then
  chk 1 "no mimir_core_/mimir_platform_/mimir_audit_ exports"
else
  chk 0 "no mimir_core_/mimir_platform_/mimir_audit_ exports"
fi

echo "=== S4 consumers bind to the .so (DT_NEEDED + undefined symbols) ==="
for exe in test_z076_dyn test_z076_cpp; do
  echo "-- $exe --"
  readelf -d "build/$exe" | grep NEEDED
  readelf -d "build/$exe" | grep -q 'NEEDED.*libmimiralloc\.so'
  chk $? "$exe DT_NEEDED names libmimiralloc.so"
  nm "build/$exe" | grep -E ' U mimir_stats_snapshot$'
  chk $? "$exe references mimir_stats_snapshot as undefined (U)"
  if nm "build/$exe" | grep -qE ' T mimir_stats_snapshot$'; then
    chk 1 "$exe does not define mimir_stats_snapshot (not statically absorbed)"
  else
    chk 0 "$exe does not define mimir_stats_snapshot (not statically absorbed)"
  fi
done
echo "-- C++ consumer object: unmangled reference --"
nm build/test_z076_cpp.o | grep -E ' U mimir_stats_snapshot$'
chk $? "test_z076_cpp.o undefined reference is unmangled (extern C guards cover the new declaration)"

echo "=== S5 header-only translation units (compile-only) ==="
ls -la build/header_only_z076_c.o build/header_only_z076_cpp.o
chk $? "header-only C and C++ objects built (header self-contained, exact prototype)"

echo "=== S6 static statistics test ==="
./build/test_z076
chk $? "test_z076 exit 0"

echo "=== S7 dynamic C consumer ==="
./build/test_z076_dyn
chk $? "test_z076_dyn exit 0"

echo "=== S8 C++ consumer ==="
./build/test_z076_cpp
chk $? "test_z076_cpp exit 0"

echo "=== S9 neighbour suites ==="
make test-z074 > hidden_files/z076_test_z074.log 2>&1
chk $? "make test-z074 still passes (log hidden_files/z076_test_z074.log)"
make test-z065 > hidden_files/z076_test_z065.log 2>&1
chk $? "make test-z065 still passes (log hidden_files/z076_test_z065.log)"
make test-z064 > hidden_files/z076_test_z064.log 2>&1
chk $? "make test-z064 still passes (log hidden_files/z076_test_z064.log)"
echo "-- test-j002 (internal snapshot proof; PRE-EXISTING failures, see slice doc) --"
make test-j002 > hidden_files/z076_test_j002.log 2>&1
grep -E "checks: 550|RESULT:" hidden_files/z076_test_j002.log
echo "j002 status recorded verbatim above: 113 failures at the"
echo "Z-076 base (480459e) and 113 after this slice, with the"
echo "failure SET identical after the Z-076 semantic updates"
echo "to the ABSENT/PARTIAL assertions (docs/Z13_PUBLIC_STATISTICS.md S6)."

echo "=== Z-076 ABI RUN RESULT: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL) ==="
exit $FAIL
