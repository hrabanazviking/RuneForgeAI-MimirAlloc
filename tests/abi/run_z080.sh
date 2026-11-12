#!/usr/bin/env bash
# Z-080 recorded run: SONAME, versioning, install (D-07).
#
# Proves, on this host and against the current ten-symbol
# surface: (S2) the versioned library file carries SONAME
# libmimiralloc.so.0 and exports exactly the ten public
# symbols; (S3) the banked unversioned library carries no
# SONAME and its consumers record the unversioned name --
# the pre-Z-080 state, kept as the contrast leg; (S4) a
# consumer linked against the versioned file records
# DT_NEEDED = libmimiralloc.so.0 and passes; (S5) `make
# install` into a staging prefix lays down the real file,
# both symlinks and the header, and does NOT touch the host
# linker cache; (S6) the staged ldconfig story: a staging
# root carries no ld.so.conf of its own, so the run gives
# it one naming /usr/local/lib, deletes the SONAME symlink,
# and shows `ldconfig -r` recreating it from the SONAME
# alone and entering the library in the staged cache;
# (S7) a consumer compiled against the staged prefix
# records DT_NEEDED = libmimiralloc.so.0, resolves into the
# staged prefix and passes; (S8) neighbour routes still
# pass. Output is the recorded run itself; the caller tees
# it to tests/abi/recorded_run_z13_z080.txt. Exit 0 iff
# every proof and consumer passes.
set -u
cd "$(dirname "$0")/../.."
FAIL=0
note() { echo "$*"; }
chk() { if [ "$1" -eq 0 ]; then echo "  [ok] $2"; else echo "  [FAIL] $2"; FAIL=1; fi; }

VER="$(cat VERSION)"
VLIB="build/libmimiralloc.so.$VER"

echo "=== Z-080 SONAME / versioning / install recorded run ==="
echo "date_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "host: $(uname -a)"
echo "cc: $(cc --version | head -1)"
echo "ldconfig: $(ldconfig --version | head -1)"
echo "head: $(git rev-parse HEAD)"
echo "branch: $(git branch --show-current)"
echo "loadavg: $(cat /proc/loadavg)"
echo "page_size: $(getconf PAGESIZE)"
echo "VERSION file: $VER"

echo "=== S1 build from clean ==="
make clean >/dev/null 2>&1
make lib lib-versioned build/test_z080_dyn build/test_z077_dyn > hidden_files/z080_build.log 2>&1
chk $? "make lib lib-versioned + z080/z077 consumers (log hidden_files/z080_build.log)"
ls -la build/libmimiralloc.so build/libmimiralloc.so.0 "$VLIB"

echo "=== S2 versioned library identity (SONAME + exports) ==="
readelf -d "$VLIB" | grep -E 'SONAME'
SONAME="$(readelf -d "$VLIB" | awk '/SONAME/ { gsub(/[][]/, "", $5); print $5 }')"
echo "parsed SONAME: $SONAME"
[ "$SONAME" = "libmimiralloc.so.0" ]
chk $? "SONAME is libmimiralloc.so.0 (ABI epoch 0, M-007 D2 rule)"
readlink build/libmimiralloc.so.0
[ "$(readlink build/libmimiralloc.so.0)" = "libmimiralloc.so.$VER" ]
chk $? "build-tree SONAME symlink points at the versioned file"
echo "-- exports (nm -D) - exactly the 10 public symbols --"
nm -D --defined-only "$VLIB"
echo "defined dynamic symbol count: $(nm -D --defined-only "$VLIB" | wc -l)"
[ "$(nm -D --defined-only "$VLIB" | wc -l)" -eq 10 ]
chk $? "count == 10"
for sym in mimir_malloc mimir_free mimir_calloc mimir_realloc mimir_aligned_alloc mimir_posix_memalign mimir_usable_size mimir_stats_snapshot mimir_trim mimir_reallocarray; do
  nm -D --defined-only "$VLIB" | grep -q " T $sym\$"
  chk $? "exported: $sym"
done
if nm -D --defined-only "$VLIB" | grep -E 'mimir_core_|mimir_platform_|mimir_audit_' ; then
  chk 1 "no mimir_core_/mimir_platform_/mimir_audit_ exports"
else
  chk 0 "no mimir_core_/mimir_platform_/mimir_audit_ exports"
fi

echo "=== S3 contrast leg: the banked unversioned library ==="
echo "SONAME entries in build/libmimiralloc.so: $(readelf -d build/libmimiralloc.so | grep -c SONAME)"
[ "$(readelf -d build/libmimiralloc.so | grep -c SONAME)" -eq 0 ]
chk $? "unversioned build library carries no SONAME (E-002 state, untouched)"
readelf -d build/test_z077_dyn | grep NEEDED
readelf -d build/test_z077_dyn | grep -q 'NEEDED.*\[libmimiralloc\.so\]'
chk $? "E-002-route consumer DT_NEEDED names the unversioned libmimiralloc.so"

echo "=== S4 build-tree consumer against the versioned file ==="
readelf -d build/test_z080_dyn | grep NEEDED
readelf -d build/test_z080_dyn | grep -q 'NEEDED.*\[libmimiralloc\.so\.0\]'
chk $? "test_z080_dyn DT_NEEDED names libmimiralloc.so.0 (the SONAME)"
if readelf -d build/test_z080_dyn | grep -q 'NEEDED.*\[libmimiralloc\.so\]'; then
  chk 1 "test_z080_dyn has no unversioned libmimiralloc.so DT_NEEDED"
else
  chk 0 "test_z080_dyn has no unversioned libmimiralloc.so DT_NEEDED"
fi
./build/test_z080_dyn
chk $? "test_z080_dyn exit 0 (run via \$ORIGIN rpath + SONAME symlink)"

echo "=== S5 staging-prefix install ==="
STAGE="$(mktemp -d /tmp/z080-stage.XXXXXX)"
echo "staging prefix root: $STAGE (DESTDIR=$STAGE, PREFIX=/usr/local)"
make install DESTDIR="$STAGE" PREFIX=/usr/local > hidden_files/z080_install.log 2>&1
chk $? "make install DESTDIR=stage (log hidden_files/z080_install.log)"
cat hidden_files/z080_install.log
find "$STAGE" | sort
SLIB="$STAGE/usr/local/lib"
[ -f "$SLIB/libmimiralloc.so.$VER" ] && [ ! -L "$SLIB/libmimiralloc.so.$VER" ]
chk $? "staged real file libmimiralloc.so.$VER present (not a symlink)"
[ "$(readlink "$SLIB/libmimiralloc.so.0")" = "libmimiralloc.so.$VER" ]
chk $? "staged libmimiralloc.so.0 -> libmimiralloc.so.$VER"
[ "$(readlink "$SLIB/libmimiralloc.so")" = "libmimiralloc.so.0" ]
chk $? "staged libmimiralloc.so (development symlink) -> libmimiralloc.so.0"
[ -f "$STAGE/usr/local/include/mimir_alloc.h" ]
chk $? "staged header usr/local/include/mimir_alloc.h present"
readelf -d "$SLIB/libmimiralloc.so.$VER" | grep SONAME
[ "$(nm -D --defined-only "$SLIB/libmimiralloc.so.$VER" | wc -l)" -eq 10 ]
chk $? "staged library exports exactly 10 symbols"
if ls /usr/local/lib/libmimiralloc.so* >/dev/null 2>&1; then
  chk 1 "host /usr/local/lib NOT polluted by the staged install"
else
  chk 0 "host /usr/local/lib NOT polluted by the staged install (DESTDIR set: install ran no host ldconfig)"
fi

echo "=== S6 the staged ldconfig story ==="
echo "A staging root has no /etc of its own: give it the one line a"
echo "packager's root carries (on this host /etc/ld.so.conf.d/libc.conf"
echo "already names /usr/local/lib), then delete the SONAME symlink"
echo "and let ldconfig -r recreate it from the SONAME alone."
mkdir -p "$STAGE/etc"
printf '/usr/local/lib\n' > "$STAGE/etc/ld.so.conf"
cat "$STAGE/etc/ld.so.conf"
rm "$SLIB/libmimiralloc.so.0"
ls "$SLIB"
ldconfig -r "$STAGE"
chk $? "ldconfig -r stage exit 0"
ls -la "$SLIB"
[ "$(readlink "$SLIB/libmimiralloc.so.0")" = "libmimiralloc.so.$VER" ]
chk $? "ldconfig recreated libmimiralloc.so.0 -> libmimiralloc.so.$VER from the SONAME"
ldconfig -r "$STAGE" -p | grep -i mimir
ldconfig -r "$STAGE" -p | grep -q 'libmimiralloc\.so\.0 .*=> /usr/local/lib/libmimiralloc\.so\.0'
chk $? "staged cache lists libmimiralloc.so.0 => /usr/local/lib/libmimiralloc.so.0"

echo "=== S7 consumer against the staged prefix ==="
cc -std=c11 -Wall -Wextra -Werror -O2 -pthread \
  -I"$STAGE/usr/local/include" tests/abi/test_z080_dyn.c \
  -L"$SLIB" -lmimiralloc -o "$STAGE/consumer_z080"
chk $? "consumer compiles against the staged include/lib"
readelf -d "$STAGE/consumer_z080" | grep NEEDED
readelf -d "$STAGE/consumer_z080" | grep -q 'NEEDED.*\[libmimiralloc\.so\.0\]'
chk $? "staged consumer DT_NEEDED names libmimiralloc.so.0"
echo "-- ldd (LD_LIBRARY_PATH stands in for the cache a real install refreshes) --"
LD_LIBRARY_PATH="$SLIB" ldd "$STAGE/consumer_z080" | grep -i mimir
LD_LIBRARY_PATH="$SLIB" ldd "$STAGE/consumer_z080" | grep -q "libmimiralloc\.so\.0 => $SLIB/libmimiralloc\.so\.0"
chk $? "loader resolves libmimiralloc.so.0 into the staged prefix"
LD_LIBRARY_PATH="$SLIB" "$STAGE/consumer_z080"
chk $? "staged consumer exit 0"

echo "=== S8 neighbour routes unregressed ==="
make test-z077 > hidden_files/z080_test_z077.log 2>&1
chk $? "make test-z077 still passes (log hidden_files/z080_test_z077.log)"
make test-e002 > hidden_files/z080_test_e002.log 2>&1
chk $? "make test-e002 still passes (log hidden_files/z080_test_e002.log)"
echo "staging root left in place for inspection: $STAGE"

echo "=== Z-080 RUN RESULT: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL) ==="
exit $FAIL
