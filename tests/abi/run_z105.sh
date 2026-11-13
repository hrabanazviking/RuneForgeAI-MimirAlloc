#!/usr/bin/env bash
# Z-105 release engineering: version-packet evidence,
# SBOM-refresh evidence, and the reproducible-build proof
# on THIS host (purpleraider, x86-64). Two full clean
# builds of the same tree state, staged outside the tree
# (M-007 custody pattern), compared hash by hash. Any
# differing artifact is NAMED with both hashes and fails
# the run - never averaged over, never retried silently
# (docs/REPRODUCIBLE_BUILD.md step 7 discipline).
#
# Scope of the reproducibility claim (M-007 D5, restated
# for this host): same sha256 for an artifact produced by
# two full clean builds of the same tree state ON THIS
# HOST, following this script exactly. Inside the claim:
# libmimiralloc.so.0.1.0, libmimiralloc-0.1.0.a (static),
# libmimiralloc_interpose.so, and the normalised source
# tarball mimiralloc-0.1.0.tar.gz. The header is a verbatim
# copy, hashed but not a build claim. NO cross-machine
# claim: the committed release/SHA256SUMS are the Pi-era
# (M-007) hashes of a different arch/compiler/flag tree;
# S7 records the contrast as EXPECTED-DIFFER, not a fail.
#
# Output is the evidence body of the recorded run; the
# caller tees it. Exit 0 iff every [ok] check passes.
set -u
cd "$(dirname "$0")/../.."
FAIL=0; TOTAL=0; OKN=0
chk() { TOTAL=$((TOTAL+1)); if [ "$1" -eq 0 ]; then OKN=$((OKN+1)); echo "  [ok] $2"; else echo "  [FAIL] $2"; FAIL=1; fi; }
note() { echo "$*"; }
SDE=1794577930
A=/tmp/z105-relA; B=/tmp/z105-relB; X=/tmp/z105-extract; I=/tmp/z105-install
ARTS="libmimiralloc.so.0.1.0 libmimiralloc-0.1.0.a libmimiralloc_interpose.so mimir_alloc.h mimiralloc-0.1.0.tar.gz"
note "=== S0 - host, toolchain, tree state ==="
scripts/z13/host_truth.sh || true
note "cc: $(cc --version | head -n1)"
note "make: $(make --version | head -n1)"
note "ld: $(ld --version | head -n1)"
note "head: $(git rev-parse HEAD)  branch: $(git branch --show-current)"
note "VERSION file: $(cat VERSION)"
note "CFLAGS line: $(grep -m1 '^CFLAGS' Makefile)"
note "SOURCE_DATE_EPOCH (tarball): $SDE (committer time of seat base c3424b2)"
note "loadavg at start: $(cat /proc/loadavg)"
build_one() {
  D="$1"; TAG="$2"
  rm -rf "$D"; mkdir -p "$D"
  make clean > "hidden_files/z105_build_${TAG}.log" 2>&1
  make lib lib-versioned static-lib lib-interpose >> "hidden_files/z105_build_${TAG}.log" 2>&1
  chk $? "clean build $TAG (log hidden_files/z105_build_${TAG}.log)"
  cp build/libmimiralloc.so.0.1.0 "$D/libmimiralloc.so.0.1.0"
  cp build/libmimiralloc.a "$D/libmimiralloc-0.1.0.a"
  cp build/libmimiralloc_interpose.so "$D/libmimiralloc_interpose.so"
  cp include/mimir_alloc.h "$D/mimir_alloc.h"
  { git ls-files; git ls-files --others --exclude-standard; } | grep -v -e '^release/SHA256SUMS$' -e '^tests/abi/recorded_run_z13_z105.txt$' | LC_ALL=C sort -u > "/tmp/z105-filelist-${TAG}.txt"
  LC_ALL=C TZ=UTC tar --sort=name --owner=0 --group=0 --numeric-owner --mtime=@$SDE -cf - -T "/tmp/z105-filelist-${TAG}.txt" | gzip -n > "$D/mimiralloc-0.1.0.tar.gz"
  ( cd "$D" && sha256sum $ARTS > SHA256SUMS )
  note "-- build $TAG staged at $D; file list $(wc -l < /tmp/z105-filelist-${TAG}.txt) files"
  cat "$D/SHA256SUMS" | sed 's/^/   /'
}
note "=== S1 - two full clean builds ==="
build_one "$A" A
build_one "$B" B
note "=== S2 - reproducibility comparison (A vs B, this host) ==="
for f in $ARTS; do
  if cmp -s "$A/$f" "$B/$f"; then R=0; else R=1; fi
  chk $R "identical: $f"
  if [ $R -ne 0 ]; then note "   A: $(sha256sum $A/$f)"; note "   B: $(sha256sum $B/$f)"; fi
done
diff "$A/SHA256SUMS" "$B/SHA256SUMS" > /dev/null 2>&1; chk $? "SHA256SUMS of build A and build B are byte-identical"
note "=== S3 - surface proofs on build B ==="
NDEF=$(nm -D --defined-only "$B/libmimiralloc.so.0.1.0" | grep -c ' T ')
note "versioned .so defined dynamic symbols: $NDEF (expect 13)"; [ "$NDEF" = 13 ]; chk $? "versioned library exports exactly 13"
NUNV=$(nm -D --defined-only build/libmimiralloc.so | grep -c ' T ')
note "default .so defined dynamic symbols: $NUNV (expect 13)"; [ "$NUNV" = 13 ]; chk $? "default library exports exactly 13 (same tree state)"
NINT=$(nm -D --defined-only "$B/libmimiralloc_interpose.so" | grep -c ' T ')
note "interpose .so defined dynamic symbols: $NINT (expect 20 = 10 namespaced + 10 standard, Z-082 contract)"; [ "$NINT" = 20 ]; chk $? "interposing variant exports exactly 20 (Z-082 contract unchanged)"
SON=$(readelf -d "$B/libmimiralloc.so.0.1.0" | grep -c 'SONAME.*libmimiralloc.so.0]')
[ "$SON" = 1 ]; chk $? "SONAME of versioned library is libmimiralloc.so.0"
note "DT_NEEDED of versioned library:"; readelf -d "$B/libmimiralloc.so.0.1.0" | grep NEEDED | sed 's/^/   /'
NBATCH=$(nm -D --defined-only "$B/libmimiralloc.so.0.1.0" | grep -c mimir_batch || true)
[ "$NBATCH" = 0 ]; chk $? "Z-092 batch prototype is NOT a public symbol (0 mimir_batch_* exports)"
note "=== S4 - staged install + consumer proof (build B artifacts) ==="
rm -rf "$I"; mkdir -p "$I"
make install DESTDIR="$I" PREFIX=/usr/local > hidden_files/z105_install.log 2>&1; chk $? "make install DESTDIR staged (log hidden_files/z105_install.log)"
ls -l "$I/usr/local/lib" "$I/usr/local/include" | sed 's/^/   /'
rm -rf "$X"; mkdir -p "$X"
cc tests/abi/test_e004.c -I"$I/usr/local/include" -L"$I/usr/local/lib" -lmimiralloc -o "$X/consumer" 2> hidden_files/z105_consumer_build.log; chk $? "consumer (tests/abi/test_e004.c) compiles+links against staged install"
ND=$(readelf -d "$X/consumer" | grep -c 'NEEDED.*libmimiralloc.so.0]')
[ "$ND" = 1 ]; chk $? "consumer DT_NEEDED names the SONAME libmimiralloc.so.0"
LD_LIBRARY_PATH="$I/usr/local/lib" "$X/consumer" > hidden_files/z105_consumer_run.log 2>&1; chk $? "consumer runs against staged install (exit 0)"
note "=== S5 - checksum verification from clean extraction ==="
cp "$B/libmimiralloc.so.0.1.0" "$B/libmimiralloc-0.1.0.a" "$B/libmimiralloc_interpose.so" "$B/mimir_alloc.h" "$B/mimiralloc-0.1.0.tar.gz" "$B/SHA256SUMS" "$X/"
( cd "$X" && sha256sum -c SHA256SUMS ) | sed 's/^/   /'
( cd "$X" && sha256sum -c SHA256SUMS > /dev/null 2>&1 ); chk $? "sha256sum -c SHA256SUMS: every artifact OK in a fresh directory"
note "=== S6 - contrast with committed release/SHA256SUMS (Pi, M-007) ==="
note "committed (aarch64 Pi, gcc 12.2, pre-Z13 flags):"; cat release/SHA256SUMS | sed 's/^/   /'
note "this host build B (x86-64, gcc 15.3, -O3 Z-096 set):"; cat "$B/SHA256SUMS" | grep -v interpose | sed 's/^/   /'
note "verdict: hashes DIFFER across hosts, as M-007 D5 scopes the claim (same-host only). Not a failure."
note "=== S7 - verdict ==="
note "checks: total=$TOTAL ok=$OKN failed=$((TOTAL-OKN))"
if [ $FAIL -eq 0 ]; then note "Z-105 RESULT: PASS"; else note "Z-105 RESULT: FAIL"; fi
note "loadavg at end: $(cat /proc/loadavg)"
exit $FAIL
