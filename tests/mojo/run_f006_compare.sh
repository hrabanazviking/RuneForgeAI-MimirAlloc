#!/bin/bash
# F-006 - representative program repeated-compile comparison
# harness (Phase 6, tasks T4+T6). Runs a bounded repeated
# series - F006_REPS builds + F006_REPS runs per leg
# (default 5) - for the two legs of the F-006 comparison on
# this tree's banked revision:
#
#   WITHOUT: tests/mojo/test_f004.mojo (banked F-004
#     baseline; stdlib List[Int] growth through the Mojo
#     runtime's default allocator; no mimir_* reference,
#     no -Xlinker argument).
#   WITH (B2+B1 boundary): tests/mojo/test_f005.mojo,
#     DYNAMIC leg only (banked F-005; linked against
#     build/libmimiralloc.so + the test-only observer
#     build/libf005probe.so; the program's own explicit
#     mimir_* calls only). The program warms up and settles
#     before its mapped-byte baseline (INC-F005-01
#     quiescence discipline, inside the program itself).
#
# Timing is RECORDED (wall milliseconds per build and run)
# but is never an oracle and never a performance claim:
# this Pi is shared (Samtal and other residents), so the
# numbers describe these runs only.
#
# Every repetition is recorded, pass or fail; a failed
# repetition is never deleted or hidden. The script exits
# 0 only if every build and run of both legs passed.
#
# Standalone via `make test-f006`; NOT part of `make test`.
# Binaries land in build/ (untracked) so the WITH leg's
# $ORIGIN rpath resolves the project libraries exactly as
# in F-003/F-005.

set -u
cd "$(dirname "$0")/../.." || exit 1

REPS="${F006_REPS:-5}"
FAILURES=0

now_ms() { date +%s%N | cut -c1-13; }

echo "==================================================================="
echo "F-006 RECORDED RUN - representative program repeated-compile"
echo "comparison, same revision with and without the replacement"
echo "Slice F-006 (Wave 6, Phase 6, tasks T4+T6), team-06."
echo "Worktree: $(pwd) (purpleviking1, Raspberry Pi)"
echo "==================================================================="
echo
echo "--- S0. Environment and base state ---"
echo "\$ date -Iseconds"
date -Iseconds
echo "\$ uname -a"
uname -a
echo "\$ git rev-parse HEAD"
git rev-parse HEAD
echo "\$ git branch --show-current"
git branch --show-current
echo "\$ git status --porcelain"
git status --porcelain
echo "\$ cc --version | head -n 1"
cc --version | head -n 1
echo "\$ cat /usr/local/bin/mojo"
cat /usr/local/bin/mojo
echo "\$ mojo --version"
mojo --version
echo "exit:$?"
echo "\$ getconf PAGESIZE"
getconf PAGESIZE
echo "\$ free -g"
free -g
echo "\$ curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:8787/ (Samtal)"
curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:8787/
echo
echo
echo "--- S1. Leg sources and the comparison shape ---"
echo "\$ sha256sum tests/mojo/test_f004.mojo tests/mojo/test_f005.mojo tests/mojo/f005_probe.c tests/mojo/run_f006_compare.sh"
sha256sum tests/mojo/test_f004.mojo tests/mojo/test_f005.mojo tests/mojo/f005_probe.c tests/mojo/run_f006_compare.sh
echo
echo "WITHOUT leg source: tests/mojo/test_f004.mojo - stdlib"
echo "List[Int] growth (192 entries) + mutation + independently"
echo "computed checksums; no mimir_* reference, no external_call,"
echo "no -Xlinker. Its allocations are the Mojo runtime default"
echo "allocator's (stdlib List storage)."
echo "WITH leg source: tests/mojo/test_f005.mojo DYNAMIC leg -"
echo "explicit mimir_malloc/calloc/realloc/free via Mojo FFI"
echo "(B1), build/libmimiralloc.so placed on the link line via"
echo "-Xlinker (B2), plus the test-only observer"
echo "build/libf005probe.so (descriptor validation + mapped-bytes"
echo "accounting; INC-F005-03: the observer is a second project"
echo "DT_NEEDED entry and holds no allocator state). The program"
echo "warms up and settles before baselining (INC-F005-01)."
echo "The legs are DIFFERENT programs by design (F-004 Section 3:"
echo "a B2-only build of the F-004 source would be presence, not"
echo "servicing), so stdout is never byte-compared across legs;"
echo "the comparison is verdict / exit-code / link-record level."
echo "Repetitions per leg: $REPS builds + $REPS runs."
echo
echo "--- S2. Project libraries for the WITH leg ---"
if [ ! -f build/libmimiralloc.so ]; then
  echo "\$ make build/libmimiralloc.so"
  make build/libmimiralloc.so
  echo "exit:$?"
fi
if [ ! -f build/libf005probe.so ]; then
  echo "\$ make build/libf005probe.so"
  make build/libf005probe.so
  echo "exit:$?"
fi
echo "\$ sha256sum build/libmimiralloc.so build/libf005probe.so"
sha256sum build/libmimiralloc.so build/libf005probe.so
echo "\$ nm -D --defined-only build/libmimiralloc.so"
nm -D --defined-only build/libmimiralloc.so

run_leg() {
  leg="$1"; src="$2"; shift 2
  echo
  echo "--- $leg ---"
  leg_build_ok=0; leg_run_ok=0
  i=1
  while [ "$i" -le "$REPS" ]; do
    out="$1"; shift
    echo "--- repetition $i of $REPS ---"
    if [ "$leg" = "WITH" ]; then
      echo "\$ timeout 180 mojo build -o $out $src -Xlinker -Lbuild -Xlinker -lmimiralloc -Xlinker -lf005probe -Xlinker -rpath -Xlinker '\$ORIGIN'"
    else
      echo "\$ timeout 180 mojo build $src -o $out"
    fi
    b0=$(now_ms)
    if [ "$leg" = "WITH" ]; then
      timeout 180 mojo build -o "$out" "$src" -Xlinker -Lbuild -Xlinker -lmimiralloc -Xlinker -lf005probe -Xlinker -rpath -Xlinker '$ORIGIN'
    else
      timeout 180 mojo build "$src" -o "$out"
    fi
    bexit=$?
    b1=$(now_ms)
    echo "build_exit:$bexit build_wall_ms:$((b1-b0))"
    if [ "$bexit" -eq 0 ]; then leg_build_ok=$((leg_build_ok+1)); else FAILURES=$((FAILURES+1)); fi
    echo "\$ sha256sum $out"
    sha256sum "$out"
    echo "\$ ./$out"
    r0=$(now_ms)
    "./$out" > "/tmp/f006_stdout_$$.txt" 2>&1
    rexit=$?
    r1=$(now_ms)
    cat "/tmp/f006_stdout_$$.txt"
    echo "run_exit:$rexit run_wall_ms:$((r1-r0))"
    if [ "$rexit" -eq 0 ]; then leg_run_ok=$((leg_run_ok+1)); else FAILURES=$((FAILURES+1)); fi
    rm -f "/tmp/f006_stdout_$$.txt"
    i=$((i+1))
  done
  echo "$leg SUMMARY: builds_passed:$leg_build_ok/$REPS runs_passed:$leg_run_ok/$REPS"
}

echo
echo "--- S3. WITHOUT leg: $REPS builds + $REPS runs ---"
echo "Exact build command per repetition (only -o name varies):"
echo "  timeout 180 mojo build tests/mojo/test_f004.mojo -o build/test_f006_without_<n>"
run_leg "WITHOUT" "tests/mojo/test_f004.mojo" \
  build/test_f006_without_1 build/test_f006_without_2 build/test_f006_without_3 build/test_f006_without_4 build/test_f006_without_5

echo
echo "--- S4. WITH leg (B2+B1, dynamic): $REPS builds + $REPS runs ---"
echo "Exact build command per repetition (only -o name varies):"
echo "  timeout 180 mojo build -o build/test_f006_with_<n> tests/mojo/test_f005.mojo -Xlinker -Lbuild -Xlinker -lmimiralloc -Xlinker -lf005probe -Xlinker -rpath -Xlinker '\$ORIGIN'"
run_leg "WITH" "tests/mojo/test_f005.mojo" \
  build/test_f006_with_1 build/test_f006_with_2 build/test_f006_with_3 build/test_f006_with_4 build/test_f006_with_5

echo
echo "--- S5. Binary identity across repetitions ---"
echo "\$ sha256sum build/test_f006_without_* build/test_f006_with_*"
sha256sum build/test_f006_without_* build/test_f006_with_*
echo "\$ distinct WITHOUT binary hashes:"
sha256sum build/test_f006_without_* | awk '{print $1}' | sort -u | wc -l
echo "\$ distinct WITH binary hashes:"
sha256sum build/test_f006_with_* | awk '{print $1}' | sort -u | wc -l

echo
echo "--- S6. Link records (repetition-1 binaries) ---"
echo "\$ file build/test_f006_without_1 build/test_f006_with_1"
file build/test_f006_without_1 build/test_f006_with_1
echo "\$ readelf -d build/test_f006_without_1 | grep -E 'NEEDED|RUNPATH'"
readelf -d build/test_f006_without_1 | grep -E 'NEEDED|RUNPATH'
echo "pipeline_exit:$? (0 = at least one NEEDED/RUNPATH line)"
echo "\$ readelf -d build/test_f006_with_1 | grep -E 'NEEDED|RUNPATH'"
readelf -d build/test_f006_with_1 | grep -E 'NEEDED|RUNPATH'
echo "pipeline_exit:$? (0 = at least one NEEDED/RUNPATH line)"
echo "\$ nm -D build/test_f006_without_1 | grep -i mimir (expect: no lines)"
nm -D build/test_f006_without_1 | grep -i mimir
echo "grep_exit:$? (1 = no mimir symbol, as expected for WITHOUT)"
echo "\$ nm -D build/test_f006_with_1 | grep -E 'mimir|f005'"
nm -D build/test_f006_with_1 | grep -E 'mimir|f005'
echo "grep_exit:$?"
echo "\$ ldd build/test_f006_with_1 | grep -E 'mimir|f005'"
ldd build/test_f006_with_1 | grep -E 'mimir|f005'
echo "grep_exit:$?"

echo
echo "--- S7. WITH leg loader bindings (inspection run, not a repetition) ---"
echo "\$ LD_DEBUG=bindings ./build/test_f006_with_1 (stderr filtered to libmimiralloc.so binding lines)"
LD_DEBUG=bindings ./build/test_f006_with_1 > "/tmp/f006_bind_out_$$.txt" 2> "/tmp/f006_bind_err_$$.txt"
echo "bindings_run_exit:$?"
echo "\$ binding lines FROM the generated executable TO libmimiralloc.so (stderr log;"
echo "   F-005 S6 shape: source must be the executable itself, so the library's own"
echo "   bindings to libc, which also name libmimiralloc.so, are excluded):"
grep -E 'binding file ./build/test_f006_with_1 \[0\] to .*libmimiralloc\.so' "/tmp/f006_bind_err_$$.txt" || true
echo "\$ count of those binding lines:"
grep -cE 'binding file ./build/test_f006_with_1 \[0\] to .*libmimiralloc\.so' "/tmp/f006_bind_err_$$.txt" || true
echo "\$ program verdict line of the bindings run:"
grep "RESULT" "/tmp/f006_bind_out_$$.txt" || cat "/tmp/f006_bind_out_$$.txt"
rm -f "/tmp/f006_bind_out_$$.txt" "/tmp/f006_bind_err_$$.txt"

echo
echo "--- S8. Post-run Pi state ---"
echo "\$ free -g"
free -g
echo "\$ curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:8787/ (Samtal)"
curl -s -o /dev/null -w '%{http_code}' http://127.0.0.1:8787/
echo
echo
if [ "$FAILURES" -eq 0 ]; then
  echo "F-006 HARNESS RESULT: PASS (all builds and runs of both legs passed)"
else
  echo "F-006 HARNESS RESULT: FAIL ($FAILURES build/run failure(s) recorded above, none hidden)"
fi
echo "=== End of F-006 recorded run ==="
exit "$FAILURES"
