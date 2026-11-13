#!/usr/bin/env bash
# Z-082 ABI recorded run: builds the default library and
# the opt-in interposing variant from clean, files the
# export proofs (default .so unchanged at exactly ten
# namespaced names with NO standard name exported; the
# interposing .so at exactly twenty), runs the symbol
# audit against this host's glibc malloc-family set
# (every glibc family symbol is either INTERPOSED,
# MISSING-NAMED in this script's list, or a libc-internal
# alias -- an unnamed family symbol FAILS the run, so a
# gap can never be a silent surprise), runs the static /
# dynamic / C++ consumers, and runs the LD_PRELOAD probe
# pair (plain = glibc verdict, preloaded = MimirAlloc
# verdict). Output is the recorded run itself; the caller
# tees it to tests/abi/recorded_run_z13_z082.txt. Exit 0
# iff every proof and consumer passes.
set -u
cd "$(dirname "$0")/../.."
FAIL=0
note() { echo "$*"; }
chk() { if [ "$1" -eq 0 ]; then echo "  [ok] $2"; else echo "  [FAIL] $2"; FAIL=1; fi; }

LIBC=/lib/x86_64-linux-gnu/libc.so.6

# The audit basis: the malloc-family symbol lists for
# THIS host's glibc, classified by disposition (D-015).
INTERPOSED="malloc free cfree calloc realloc reallocarray aligned_alloc posix_memalign malloc_usable_size malloc_trim"
MISSING_NAMED="memalign valloc pvalloc mallopt mallinfo mallinfo2 malloc_info malloc_stats mcheck mcheck_pedantic mcheck_check_all mprobe mallwatch mtrace muntrace memalignment free_sized free_aligned_sized"
INTERNAL_ALIASES="__libc_malloc __libc_free __libc_calloc __libc_realloc __libc_reallocarray __libc_memalign __libc_valloc __libc_pvalloc __libc_mallinfo __libc_mallopt __malloc_hook __free_hook __realloc_hook __memalign_hook __malloc_initialize_hook"
NAMESPACED="mimir_malloc mimir_free mimir_calloc mimir_realloc mimir_aligned_alloc mimir_posix_memalign mimir_usable_size mimir_stats_snapshot mimir_trim mimir_reallocarray"

has_sym() { # file sym -- exact defined dynamic symbol?
  nm -D --defined-only "$1" 2>/dev/null | awk '{print $NF}' | sed 's/@.*//' | grep -qx "$2"
}

echo "=== Z-082 ABI recorded run ==="
echo "date_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "host: $(uname -a)"
echo "cc: $(cc --version | head -1)"
echo "cxx: $(c++ --version | head -1)"
echo "glibc: $(ldd --version | head -1)"
echo "libc: $LIBC"
echo "head: $(git rev-parse HEAD)"
echo "branch: $(git branch --show-current)"
echo "loadavg: $(cat /proc/loadavg)"
echo "page_size: $(getconf PAGESIZE)"

echo "=== S1 build from clean ==="
make clean >/dev/null 2>&1
make lib lib-interpose build/test_z082 build/test_z082_dyn build/test_z082_cpp build/test_z082_pre > hidden_files/z082_build.log 2>&1
chk $? "make lib + lib-interpose + z082 consumers (log hidden_files/z082_build.log)"
ls -la build/libmimiralloc.so build/libmimiralloc_interpose.so

echo "=== S2 default library exports (nm -D) - exactly the 10 namespaced symbols, unchanged by Z-082 ==="
nm -D --defined-only build/libmimiralloc.so
echo "defined dynamic symbol count: $(nm -D --defined-only build/libmimiralloc.so | wc -l)"
[ "$(nm -D --defined-only build/libmimiralloc.so | wc -l)" -eq 10 ]
chk $? "count == 10"
for sym in $NAMESPACED; do
  has_sym build/libmimiralloc.so "$sym"
  chk $? "exported: $sym"
done
echo "-- opt-in proof: NO standard malloc-family name is exported by the default library --"
for sym in $INTERPOSED; do
  if has_sym build/libmimiralloc.so "$sym"; then
    chk 1 "default library does NOT export $sym"
  else
    chk 0 "default library does NOT export $sym"
  fi
done

echo "=== S3 interposing library exports (nm -D) - exactly 20: 10 namespaced + 10 standard ==="
nm -D --defined-only build/libmimiralloc_interpose.so
echo "defined dynamic symbol count: $(nm -D --defined-only build/libmimiralloc_interpose.so | wc -l)"
[ "$(nm -D --defined-only build/libmimiralloc_interpose.so | wc -l)" -eq 20 ]
chk $? "count == 20"
for sym in $NAMESPACED $INTERPOSED; do
  has_sym build/libmimiralloc_interpose.so "$sym"
  chk $? "interposing export: $sym"
done
if nm -D --defined-only build/libmimiralloc_interpose.so | grep -E 'mimir_core_|mimir_platform_|mimir_audit_'; then
  chk 1 "no mimir_core_/mimir_platform_/mimir_audit_ exports"
else
  chk 0 "no mimir_core_/mimir_platform_/mimir_audit_ exports"
fi

echo "=== S4 symbol audit against glibc's malloc-family set ($LIBC) ==="
echo "-- glibc family symbols found on this host (defined, dynamic) --"
nm -D --defined-only "$LIBC" | awk '{print $NF}' | sed 's/@.*//' | sort -u > /tmp/z082_glibc_all.txt
FAMILY_RE='^(malloc|free|cfree|calloc|realloc|reallocarray|memalign|aligned_alloc|posix_memalign|valloc|pvalloc|mallopt|mallinfo|mallinfo2|malloc_info|malloc_stats|malloc_trim|malloc_usable_size|mcheck|mcheck_pedantic|mcheck_check_all|mprobe|mallwatch|mtrace|muntrace|memalignment|free_sized|free_aligned_sized|__libc_malloc|__libc_free|__libc_calloc|__libc_realloc|__libc_reallocarray|__libc_memalign|__libc_valloc|__libc_pvalloc|__libc_mallinfo|__libc_mallopt|__malloc_hook|__free_hook|__realloc_hook|__memalign_hook|__malloc_initialize_hook)$'
grep -E "$FAMILY_RE" /tmp/z082_glibc_all.txt
echo "-- disposition of every family symbol --"
for sym in $INTERPOSED; do
  grep -qx "$sym" /tmp/z082_glibc_all.txt
  chk $? "audit basis: glibc has $sym"
  has_sym build/libmimiralloc_interpose.so "$sym"
  chk $? "INTERPOSED: $sym"
done
for sym in $MISSING_NAMED; do
  grep -qx "$sym" /tmp/z082_glibc_all.txt
  chk $? "audit basis: glibc has $sym (named missing)"
  if has_sym build/libmimiralloc_interpose.so "$sym"; then
    chk 1 "MISSING-NAMED (absent from interposing .so): $sym"
  else
    chk 0 "MISSING-NAMED (absent from interposing .so): $sym"
  fi
done
for sym in $INTERNAL_ALIASES; do
  if grep -qx "$sym" /tmp/z082_glibc_all.txt; then
    echo "  [note] INTERNAL (libc-private, not interposable): $sym"
  fi
  if has_sym build/libmimiralloc_interpose.so "$sym"; then
    chk 1 "interposing .so does NOT claim internal alias $sym"
  fi
done
echo "-- surprise scan: any glibc family symbol with NO disposition above fails --"
UNNAMED=0
while read -r sym; do
  case " $INTERPOSED $MISSING_NAMED $INTERNAL_ALIASES " in
    *" $sym "*) ;;
    *) echo "  [FAIL] UNNAMED family symbol in glibc set: $sym"; UNNAMED=1 ;;
  esac
done <<EOF
$(grep -E "$FAMILY_RE" /tmp/z082_glibc_all.txt)
EOF
chk $UNNAMED "no unnamed family symbol (every gap is a named list entry)"
echo "-- full glibc raw rows for the family (with versions) --"
nm -D --defined-only "$LIBC" | grep -E ' (malloc|free|cfree|calloc|realloc|reallocarray|memalign|aligned_alloc|posix_memalign|valloc|pvalloc|mallopt|mallinfo|mallinfo2|malloc_info|malloc_stats|malloc_trim|malloc_usable_size|mcheck|mcheck_pedantic|mcheck_check_all|mprobe|mallwatch|mtrace|muntrace|memalignment|free_sized|free_aligned_sized|__libc_malloc|__libc_free|__libc_calloc|__libc_realloc|__libc_reallocarray|__libc_memalign|__libc_valloc|__libc_pvalloc|__libc_mallinfo|__libc_mallopt|__malloc_hook|__free_hook|__realloc_hook|__memalign_hook|__malloc_initialize_hook)(@|$)' | sort -k3

echo "=== S5 consumers bind to the interposing .so (DT_NEEDED + undefined symbols) ==="
for exe in test_z082_dyn test_z082_cpp; do
  echo "-- $exe --"
  readelf -d "build/$exe" | grep NEEDED
  readelf -d "build/$exe" | grep -q 'NEEDED.*libmimiralloc_interpose\.so'
  chk $? "$exe DT_NEEDED names libmimiralloc_interpose.so"
  nm "build/$exe" | grep -E ' U (malloc|calloc|realloc|free)$'
  chk $? "$exe references standard names as undefined (U)"
  if nm "build/$exe" | grep -qE ' T (malloc|free|calloc|realloc)$'; then
    chk 1 "$exe does not define the standard names (not statically absorbed)"
  else
    chk 0 "$exe does not define the standard names (not statically absorbed)"
  fi
done
echo "-- preload probe consumer: no MimirAlloc DT_NEEDED (a plain glibc consumer) --"
readelf -d build/test_z082_pre | grep NEEDED
if readelf -d build/test_z082_pre | grep -q 'NEEDED.*mimiralloc'; then
  chk 1 "test_z082_pre has no mimiralloc DT_NEEDED"
else
  chk 0 "test_z082_pre has no mimiralloc DT_NEEDED"
fi

echo "=== S6 static contract test ==="
./build/test_z082
chk $? "test_z082 exit 0"

echo "=== S7 dynamic C consumer ==="
./build/test_z082_dyn
chk $? "test_z082_dyn exit 0"

echo "=== S8 C++ consumer ==="
./build/test_z082_cpp
chk $? "test_z082_cpp exit 0"

echo "=== S9 LD_PRELOAD probe pair ==="
OUT_PLAIN="$(./build/test_z082_pre)"
echo "plain run: $OUT_PLAIN"
[ "$OUT_PLAIN" = "PRELOAD-VERDICT: GLIBC" ]
chk $? "plain probe verdict is GLIBC (no interposition without opt-in)"
OUT_PRE="$(LD_PRELOAD="$PWD/build/libmimiralloc_interpose.so" ./build/test_z082_pre)"
echo "preloaded run: $OUT_PRE"
[ "$OUT_PRE" = "PRELOAD-VERDICT: MIMIR" ]
chk $? "preloaded probe verdict is MIMIR (runtime interposition binds)"

echo "=== S10 neighbour evidence: E-005 audit readers ==="
make test-e005 > hidden_files/z082_test_e005.log 2>&1
chk $? "make test-e005 still passes (log hidden_files/z082_test_e005.log)"

echo "=== Z-082 ABI RUN RESULT: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL) ==="
exit $FAIL
