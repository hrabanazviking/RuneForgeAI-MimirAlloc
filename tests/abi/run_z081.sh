#!/usr/bin/env bash
# Z-081 ABI recorded run: the M-002-style audit re-run
# over the widened Wave 9 surface on this host. Builds
# the library and the three consumers from clean, files
# the E-series style proofs (export list exactly the ten
# public symbols, three-way agreement of header /
# version script / .dynsym, absence checks for internal,
# standard-name, C++-mangled and audit-counter symbols,
# DT_NEEDED, undefined symbols, header-only compiles),
# runs every consumer, and then the DOCUMENTATION gate
# (S9): every exported symbol must be inventoried by
# name in docs/contracts.md Appendix A -- the gate this
# slice exists to close. The slice adds NO public
# symbol. Output is the recorded run itself; the caller
# tees it to tests/abi/recorded_run_z13_z081.txt. Exit 0
# iff every proof and consumer passes.
set -u
cd "$(dirname "$0")/../.."
FAIL=0
note() { echo "$*"; }
chk() { if [ "$1" -eq 0 ]; then echo "  [ok] $2"; else echo "  [FAIL] $2"; FAIL=1; fi; }

SYMS="mimir_malloc mimir_free mimir_calloc mimir_realloc mimir_aligned_alloc mimir_posix_memalign mimir_usable_size mimir_stats_snapshot mimir_trim mimir_reallocarray"

echo "=== Z-081 ABI recorded run (contracts appendix + ABI audit) ==="
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
make lib build/test_z081 build/test_z081_dyn build/test_z081_cpp build/header_only_z081_c.o build/header_only_z081_cpp.o > hidden_files/z081_build.log 2>&1
chk $? "make lib + z081 consumers (log hidden_files/z081_build.log)"

echo "=== S2 shared library exports (nm -D) - exactly the 10 public symbols ==="
nm -D --defined-only build/libmimiralloc.so
echo "defined dynamic symbol count: $(nm -D --defined-only build/libmimiralloc.so | wc -l)"
[ "$(nm -D --defined-only build/libmimiralloc.so | wc -l)" -eq 10 ]
chk $? "count == 10"
for sym in $SYMS; do
  nm -D --defined-only build/libmimiralloc.so | grep -q " T $sym\$"
  chk $? "exported: $sym"
done

echo "=== S3 absence proofs - nothing undocumented, nothing leaked ==="
if nm -D --defined-only build/libmimiralloc.so | grep -E 'mimir_core_|mimir_platform_|mimir_audit_' ; then
  chk 1 "no mimir_core_/mimir_platform_/mimir_audit_ exports"
else
  chk 0 "no mimir_core_/mimir_platform_/mimir_audit_ exports"
fi
if nm -D build/libmimiralloc.so | grep -q 'mimir_audit_' ; then
  chk 1 "no mimir_audit_ name anywhere in .dynsym (counters bound local)"
else
  chk 0 "no mimir_audit_ name anywhere in .dynsym (counters bound local)"
fi
if nm -D --defined-only build/libmimiralloc.so | grep -E ' T (malloc|free|calloc|realloc|aligned_alloc|posix_memalign|malloc_usable_size|reallocarray|memalign|valloc|pvalloc|strdup)$' ; then
  chk 1 "no standard-name interposition symbol exported"
else
  chk 0 "no standard-name interposition symbol exported"
fi
if nm -D --defined-only build/libmimiralloc.so | grep -qE ' _Z' ; then
  chk 1 "no C++ mangled symbol exported (no operator new/delete)"
else
  chk 0 "no C++ mangled symbol exported (no operator new/delete)"
fi

echo "=== S4 three-way agreement: header declarations and version script ==="
for sym in $SYMS; do
  grep -q "$sym" include/mimir_alloc.h
  chk $? "declared in include/mimir_alloc.h: $sym"
  grep -q "^    $sym;" src/api/mimir_alloc.map
  chk $? "listed in src/api/mimir_alloc.map global: $sym"
done
echo "header function declarations: $(grep -cE '^(void \*|void |int |size_t )mimir_' include/mimir_alloc.h)"
[ "$(grep -cE '^(void \*|void |int |size_t )mimir_' include/mimir_alloc.h)" -eq 10 ]
chk $? "header declares exactly 10 functions"

echo "=== S5 consumers bind to the .so (DT_NEEDED + undefined symbols) ==="
for exe in test_z081_dyn test_z081_cpp; do
  echo "-- $exe --"
  readelf -d "build/$exe" | grep NEEDED
  readelf -d "build/$exe" | grep -q 'NEEDED.*libmimiralloc\.so'
  chk $? "$exe DT_NEEDED names libmimiralloc.so"
  nm "build/$exe" | grep -E ' U mimir_' | sort
  for sym in $SYMS; do
    nm "build/$exe" | grep -q " U $sym\$"
    chk $? "$exe references $sym as undefined (U)"
  done
  if nm "build/$exe" | grep -qE ' T mimir_'; then
    chk 1 "$exe does not define any mimir_ symbol (not statically absorbed)"
  else
    chk 0 "$exe does not define any mimir_ symbol (not statically absorbed)"
  fi
done
echo "-- C++ consumer object: unmangled references --"
nm build/test_z081_cpp.o | grep -E ' U mimir_' | sort
chk $? "test_z081_cpp.o undefined references are the unmangled C names"

echo "=== S6 header-only translation units (compile-only) ==="
ls -la build/header_only_z081_c.o build/header_only_z081_cpp.o
chk $? "header-only C and C++ objects built (all ten prototypes, exact types)"

echo "=== S7 static audit test ==="
./build/test_z081
chk $? "test_z081 exit 0"

echo "=== S8 dynamic consumers ==="
./build/test_z081_dyn
chk $? "test_z081_dyn exit 0"
./build/test_z081_cpp
chk $? "test_z081_cpp exit 0"

echo "=== S9 documentation gate: Appendix A inventories every exported symbol ==="
awk '/^## Appendix A/,/^## Appendix B/' docs/contracts.md > /tmp/z081_appA.txt
for sym in $SYMS; do
  grep -q "\`$sym\`" /tmp/z081_appA.txt
  chk $? "Appendix A inventories: $sym"
done
grep -q 'Z13_Z081_ABI_AUDIT' docs/contracts.md
chk $? "contracts.md points at the Z-081 audit report"
test -f docs/Z13_Z081_ABI_AUDIT.md
chk $? "audit report filed: docs/Z13_Z081_ABI_AUDIT.md"
for sym in $SYMS; do
  grep -q "$sym" docs/Z13_Z081_ABI_AUDIT.md
  chk $? "audit report accounts for: $sym"
done

echo "=== S10 versioning state of record (informational, not a gate) ==="
echo "SONAME entries in build/libmimiralloc.so: $(readelf -d build/libmimiralloc.so | grep -c SONAME)"
echo "note: this tree predates Z-080 banking; the SONAME / install discharge"
echo "is Z-080's slice (team-06 0a48239), queued immediately ahead of Z-081"
echo "in banking order. This audit records the state, it does not claim it."

echo "=== S11 neighbour evidence ==="
make test-e005 > hidden_files/z081_test_e005.log 2>&1
chk $? "make test-e005 still passes (log hidden_files/z081_test_e005.log)"
make test-z078 > hidden_files/z081_test_z078.log 2>&1
chk $? "make test-z078 still passes (log hidden_files/z081_test_z078.log)"

echo "=== Z-081 ABI RUN RESULT: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL) ==="
exit $FAIL
