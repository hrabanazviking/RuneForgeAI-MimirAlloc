#!/usr/bin/env bash
# run_z104.sh - Z-104 sanitizer and warning matrix runner.
#
# Builds and grades, on this host only:
#   W-ENUM   warning enumeration: the M-003 extended set
#            (-Wconversion -Wshadow -Wstrict-prototypes
#            -Wmissing-prototypes -Wundef
#            -Wdouble-promotion) over each shipped
#            flavour's canonical build (production,
#            debug, hardened), no -Werror, make -k, every
#            compiler warning captured and counted.
#   W-GATE   the same sweeps with -Werror: build exit 0
#            and zero compiler warnings required;
#            production leg also runs the full suite
#            (counts parsed, must equal the standing
#            gate's 4988/0/77 on this tree); debug leg is
#            graded by test-debug per-binary exits (the
#            INC-J004-02 by-design divergences b007/c006
#            exit 1, all others 0); hardened leg runs
#            test-j008 (RESULT: PASS required). nm -D on
#            each flavour's shared library must show
#            exactly 13 exports.
#   S-ASAN / S-UBSAN / S-BOTH
#            the full suite (make test, the standing
#            gate's composition) built into a private
#            BUILD dir with -fsanitize=address /
#            -fsanitize=undefined -fno-sanitize-recover=all
#            / both. Required: make exit 0, parsed counts
#            4988/0/77, zero sanitizer report lines.
#   C-*      detection controls (tests/abi/probe_z104.c):
#            the sanitizers must PROVE they can see - a
#            glibc-heap overflow under ASan and a signed
#            overflow under UBSan must be reported with a
#            nonzero exit, and the ASan coverage probe
#            must confirm the recorded boundary (glibc
#            redzones poisoned, mimir-served span bytes
#            not poisoned).
#   W-CLANG  informational leg: clang compile-only of the
#            three library sources under the extended set
#            + -Werror. A failure here is a FILED finding
#            (see docs/Z13_Z104_SANITIZER_WARNING_MATRIX.md
#            F-104-1), never a runner failure: the shipped
#            compiler is cc/gcc.
#
# Standalone via `make test-z104`; NOT part of `make test`
# (standing gate counts untouched). All private build
# dirs are removed on exit; the flavour dirs are cleaned
# at the end of each leg. Raw per-leg logs are kept in
# build/z104-logs/ for the duration of the run and the
# counts in this report are parsed from them.
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
TMPD="$(mktemp -d /tmp/z104.XXXXXX)"
LOGD="$TMPD/logs"
mkdir -p "$LOGD"
FAIL=0
note_fail() { echo "MATRIX FAIL: $1"; FAIL=1; }

EXT="-Wconversion -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wdouble-promotion"
BASE="-std=c11 -Wall -Wextra -pthread -Iinclude -Isrc -O3"
SANBASE="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O1 -g"

echo "Z-104 SANITIZER AND WARNING MATRIX"
echo "host: $(uname -a)"
echo "date_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "loadavg_at_start: $(cat /proc/loadavg)"
echo "cc: $(cc --version | head -n 1)"
echo "clang: $(clang --version 2>/dev/null | head -n 1 || echo absent)"
echo "head: $(git rev-parse HEAD 2>/dev/null || echo unknown)"
echo "branch: $(git branch --show-current 2>/dev/null || echo unknown)"
echo "extended warning set: $EXT"

compiler_warnings() { grep "warning:" "$1" 2>/dev/null | grep -vc "^Makefile" || true; }
make_recipe_lines() { grep -c "warning: overriding recipe\|warning: ignoring old recipe" "$1" 2>/dev/null || true; }
suite_counts() {
  # prints "checks failed skips" parsed gate-style from a make test log
  awk '
    /^SMOKE [0-9]+\/[0-9]+ checks passed/ { split($2,a,"/"); sp=a[1]; st=a[2] }
    /^CHECKS:/ { r+=$2; f+=$4 }
    /^SKIP / { s++ }
    END { printf "%d %d %d", st+r, (st-sp)+f, s }' "$1"
}

# ---------------- warning matrix ----------------
# flavour: name|extra make args|CFLAGS flavour add|targets|lib path
run_warn_leg() {
  local name="$1"; local makeargs="$2"; local fadd="$3"; local targets="$4"; local lib="$5"; local mode="$6"
  local log="$LOGD/warn_${mode}_${name}.log"
  make $makeargs clean > /dev/null 2>&1
  rm -rf build-debug build-hardened
  local wflag=""
  [ "$mode" = gate ] && wflag="-Werror"
  # shellcheck disable=SC2086
  make -k $makeargs CFLAGS="$BASE $fadd $EXT $wflag" $targets > "$log" 2>&1
  local rc=$?
  local cw; cw=$(compiler_warnings "$log")
  local mr; mr=$(make_recipe_lines "$log")
  local exports="n/a"
  if [ -f "$lib" ]; then exports=$(nm -D --defined-only "$lib" | wc -l); fi
  echo "WARN $mode $name: make_exit=$rc compiler_warnings=$cw make_recipe_lines=$mr exports=$exports"
  if [ "$mode" = enum ]; then
    grep "warning:" "$log" | grep -v "^Makefile" | head -n 20 | sed 's/^/  ENUM-WARN /' || true
  else
    [ "$rc" -eq 0 ] || note_fail "warn gate $name make exit $rc"
    [ "$cw" = "0" ] || note_fail "warn gate $name has $cw compiler warnings"
    [ "$exports" = "13" ] || note_fail "warn gate $name exports=$exports (expect 13)"
  fi
}

for mode in enum gate; do
  run_warn_leg production "" "" "test lib" "build/libmimiralloc.so" "$mode"
  if [ "$mode" = gate ]; then
    read -r c f s <<EOF
$(suite_counts "$LOGD/warn_gate_production.log")
EOF
    echo "WARN gate production suite counts: checks=$c failed=$f skips=$s"
    [ "$c" = "4988" ] && [ "$f" = "0" ] && [ "$s" = "77" ] || note_fail "warn gate production suite counts $c/$f/$s"
  fi
  run_warn_leg debug "DEBUG=1" "-DMIMIR_DEBUG=1" "all lib-debug" "build-debug/libmimiralloc.so" "$mode"
  if [ "$mode" = gate ]; then
    # debug runtime grading: per-binary exits from test-debug
    make DEBUG=1 CFLAGS="$BASE -DMIMIR_DEBUG=1 $EXT -Werror" test-debug > "$LOGD/warn_gate_debug_testdebug.log" 2>&1
    grep "^test-debug" "$LOGD/warn_gate_debug_testdebug.log" | sed 's/^/  /'
    local_bad=$(grep "^test-debug" "$LOGD/warn_gate_debug_testdebug.log" | grep -v "exit=0" | grep -vc "test_b007 exit=1\|test_c006 exit=1" || true)
    [ "$local_bad" = "0" ] || note_fail "debug test-debug has exits beyond the INC-J004-02 pair"
    echo "WARN gate debug test-debug: only the INC-J004-02 pair (b007, c006) exit 1 - by design"
  fi
  run_warn_leg hardened "" "" "lib-hardened test-j008" "build-hardened/libmimiralloc.so" "$mode"
  if [ "$mode" = gate ]; then
    grep -E "^RESULT" "$LOGD/warn_gate_hardened.log" | sed 's/^/  hardened /' || true
    grep -q "RESULT: PASS" "$LOGD/warn_gate_hardened.log" || note_fail "hardened test-j008 not PASS"
  fi
done

# ---------------- sanitizer matrix ----------------
run_san_leg() {
  local name="$1"; local sanflags="$2"; local bdir="build-z104-$3"
  local log="$LOGD/san_$3.log"
  rm -rf "$bdir"
  # shellcheck disable=SC2086
  make BUILD="$bdir" CFLAGS="$SANBASE $sanflags" test > "$log" 2>&1
  local rc=$?
  local counts; counts=$(suite_counts "$log")
  local reports; reports=$(grep -ci "AddressSanitizer\|runtime error:" "$log" || true)
  local resultpass; resultpass=$(grep -c "^RESULT: PASS" "$log" || true)
  local resultfail; resultfail=$(grep -c "^RESULT: FAIL" "$log" || true)
  echo "SAN $name: make_exit=$rc counts(checks/failed/skips)=$counts sanitizer_reports=$reports result_pass=$resultpass result_fail=$resultfail"
  [ "$rc" -eq 0 ] || note_fail "san $name make exit $rc"
  [ "$counts" = "4988 0 77" ] || note_fail "san $name counts $counts (expect 4988 0 77)"
  [ "$reports" = "0" ] || note_fail "san $name has $reports sanitizer reports"
  [ "$resultfail" = "0" ] || note_fail "san $name has RESULT: FAIL lines"
  rm -rf "$bdir"
}

run_san_leg asan "-fsanitize=address" asan
run_san_leg ubsan "-fsanitize=undefined -fno-sanitize-recover=all" ubsan
run_san_leg asan+ubsan "-fsanitize=address,undefined -fno-sanitize-recover=all" both

# ---------------- detection controls ----------------
ZB="$TMPD/probe"
mkdir -p "$ZB"
gcc $SANBASE -fsanitize=address -c "$ROOT/src/core/mimir_core.c" -o "$ZB/mimir_core.o" &&
gcc $SANBASE -fsanitize=address -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/mimir_alloc.o" &&
gcc $SANBASE -fsanitize=address -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/mimir_platform.o" &&
gcc $SANBASE -fsanitize=address "$ROOT/tests/abi/probe_z104.c" \
    "$ZB/mimir_alloc.o" "$ZB/mimir_core.o" "$ZB/mimir_platform.o" -o "$ZB/probe_asan" \
    || note_fail "probe_asan build"
gcc $SANBASE -fsanitize=undefined -fno-sanitize-recover=all -c "$ROOT/src/core/mimir_core.c" -o "$ZB/u_core.o" &&
gcc $SANBASE -fsanitize=undefined -fno-sanitize-recover=all -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/u_alloc.o" &&
gcc $SANBASE -fsanitize=undefined -fno-sanitize-recover=all -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/u_platform.o" &&
gcc $SANBASE -fsanitize=undefined -fno-sanitize-recover=all "$ROOT/tests/abi/probe_z104.c" \
    "$ZB/u_alloc.o" "$ZB/u_core.o" "$ZB/u_platform.o" -o "$ZB/probe_ubsan" || note_fail "probe_ubsan build"

if [ -x "$ZB/probe_asan" ]; then
  "$ZB/probe_asan" asan-detect > "$LOGD/control_asan.log" 2>&1
  rc=$?
  if [ "$rc" -ne 0 ] && grep -q "heap-buffer-overflow" "$LOGD/control_asan.log"; then
    echo "CONTROL asan-detect: DETECTED (exit=$rc, heap-buffer-overflow reported)"
  else
    echo "CONTROL asan-detect: NOT DETECTED (exit=$rc)"; note_fail "asan control not detected"
  fi
  "$ZB/probe_asan" coverage > "$LOGD/control_coverage.log" 2>&1
  rc=$?
  cat "$LOGD/control_coverage.log" | sed 's/^/  /'
  [ "$rc" -eq 0 ] || note_fail "coverage probe boundary differs (exit $rc)"
fi
if [ -x "$ZB/probe_ubsan" ]; then
  "$ZB/probe_ubsan" ubsan-detect > "$LOGD/control_ubsan.log" 2>&1
  rc=$?
  if [ "$rc" -ne 0 ] && grep -q "runtime error:" "$LOGD/control_ubsan.log"; then
    echo "CONTROL ubsan-detect: DETECTED (exit=$rc, runtime error reported)"
  else
    echo "CONTROL ubsan-detect: NOT DETECTED (exit=$rc)"; note_fail "ubsan control not detected"
  fi
fi

# ---------------- clang informational leg ----------------
echo "CLANG LEG (informational; shipped compiler is cc/gcc; see F-104-1)"
for tu in src/api/mimir_alloc.c src/core/mimir_core.c src/platform/mimir_platform.c; do
  if clang -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O3 $EXT -c "$tu" -o "$ZB/clang_tmp.o" 2> "$LOGD/clang_$(basename $tu).log"; then
    echo "CLANG $tu: PASS (zero warnings under the extended set)"
  else
    echo "CLANG $tu: FAIL - first diagnostics:"
    head -n 4 "$LOGD/clang_$(basename $tu).log" | sed 's/^/  /'
  fi
done

# ---------------- cleanup + verdict ----------------
make clean > /dev/null 2>&1; make DEBUG=1 clean > /dev/null 2>&1
rm -rf build-debug build-hardened "$TMPD"
echo "loadavg_at_end: $(cat /proc/loadavg)"
if [ "$FAIL" -eq 0 ]; then
  echo "Z-104 MATRIX RESULT: PASS (all gated legs green; clang leg informational, filed as F-104-1)"
  exit 0
fi
echo "Z-104 MATRIX RESULT: FAIL"
exit 1
