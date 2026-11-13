#!/bin/sh
# Z-109 fuzz runner: API fuzz at scale over the full public
# surface. Usage:
#   run_fuzz_z109.sh [plain-binary] [inject-binary]
# Legs (each bounded by timeout; any FINDING line, nonzero
# exit, timeout (124) or FAIL result fails the whole run):
#   1. export audit: nm -D on the built shared library must
#      list exactly the 13 public mimir_* entry points the
#      harness fuzzes (the surface of record, Z-105/Z-106).
#   2. plain seeded streams: seeds 10901..10910, OPS each.
#   3. determinism: seed 10901 run twice, outputs diffed
#      byte-identical (same seed -> same stream -> same
#      checks; the M-004 reproducibility discipline).
#   4. inject seeded streams (mmap-refusal armed through
#      the I-004 shim): seeds 10921..10926, OPS each; every
#      seed must show FAULT injected >= 1, else the D-09
#      coverage is not proven and the run FAILS.
#   5. multithread leg: 4 threads x MTOPS stream ops with
#      cross-thread inbox exchange; final public-stats live
#      count must be exactly 0.
#   6. corpus replay: 16 generated corpus files replayed
#      through the plain binary's corpus mode.
#   7. libFuzzer leg (clang only; a named SKIP otherwise):
#      the harness built with -DZ109_LIBFUZZER
#      -fsanitize=fuzzer,address,undefined runs LF_RUNS
#      bounded executions over the corpus directory.
# Scale knobs (env): Z109_OPS (default 25000),
# Z109_MTOPS (default 20000), Z109_LF_RUNS (default 300).
set -u
PLAIN=${1:-./build/fuzz_z109}
INJECT=${2:-./build/fuzz_z109_inject}
OPS=${Z109_OPS:-25000}
MTOPS=${Z109_MTOPS:-20000}
LF_RUNS=${Z109_LF_RUNS:-300}
FAIL=0
TMPD=$(mktemp -d /tmp/z109run.XXXXXX)
trap 'rm -rf "$TMPD"' EXIT

echo "Z109-RUNNER plain=$PLAIN inject=$INJECT ops=$OPS mtops=$MTOPS lf_runs=$LF_RUNS"
echo "Z109-HOST $(uname -a)"
echo "Z109-LOAD $(cat /proc/loadavg)"

# ---- leg 1: export audit ----
if [ -f build/libmimiralloc.so ]; then
  NEXP=$(nm -D --defined-only build/libmimiralloc.so | grep -c " T mimir_")
  echo "EXPORTS defined_mimir_T=$NEXP expected=13"
  if [ "$NEXP" != "13" ]; then echo "LEG exports FAIL"; FAIL=1;
  else echo "LEG exports PASS"; fi
else
  echo "LEG exports SKIP (build/libmimiralloc.so absent; run make lib)"
fi

run_seed_leg() {
  # $1 binary $2 seed $3 ops $4 tag
  OUT="$TMPD/$4_$2.out"
  timeout 300 "$1" seed "$2" "$3" > "$OUT" 2>&1
  RC=$?
  cat "$OUT"
  if [ $RC -ne 0 ]; then echo "LEG $4 seed=$2 FAIL rc=$RC"; FAIL=1; return; fi
  if grep -q "FINDING" "$OUT"; then echo "LEG $4 seed=$2 FAIL finding"; FAIL=1; return; fi
  if ! grep -q "RESULT: PASS" "$OUT"; then echo "LEG $4 seed=$2 FAIL no-pass"; FAIL=1; return; fi
  echo "LEG $4 seed=$2 PASS"
}

# ---- leg 2: plain seeds ----
for S in 10901 10902 10903 10904 10905 10906 10907 10908 10909 10910; do
  run_seed_leg "$PLAIN" "$S" "$OPS" plain
done

# ---- leg 3: determinism ----
timeout 300 "$PLAIN" seed 10901 "$OPS" > "$TMPD/det1.out" 2>&1
timeout 300 "$PLAIN" seed 10901 "$OPS" > "$TMPD/det2.out" 2>&1
if diff -q "$TMPD/det1.out" "$TMPD/det2.out" > /dev/null 2>&1; then
  echo "LEG determinism PASS (seed 10901 twice, byte-identical)"
else
  echo "LEG determinism FAIL"; FAIL=1
fi

# ---- leg 4: inject seeds ----
for S in 10921 10922 10923 10924 10925 10926; do
  run_seed_leg "$INJECT" "$S" "$OPS" inject
  INJ=$(grep "^FAULT " "$TMPD/inject_$S.out" | sed -n "s/.*injected=\([0-9]*\).*/\1/p")
  echo "INJECT-PROOF seed=$S shim_injected=${INJ:-none}"
  if [ -z "${INJ:-}" ] || [ "$INJ" -lt 1 ]; then
    echo "LEG inject seed=$S FAIL no-injection-observed"; FAIL=1
  fi
done

# ---- leg 5: multithread ----
timeout 300 "$PLAIN" mt 20901 "$MTOPS" > "$TMPD/mt.out" 2>&1
RC=$?
cat "$TMPD/mt.out"
if [ $RC -ne 0 ] || grep -q "FINDING" "$TMPD/mt.out" \
   || ! grep -q "MT-FINAL live=0 PASS" "$TMPD/mt.out"; then
  echo "LEG mt FAIL rc=$RC"; FAIL=1
else
  echo "LEG mt PASS"
fi

# ---- leg 6: corpus replay ----
CDIR="$TMPD/corpus"
mkdir -p "$CDIR"
CFAIL=0
for S in 31001 31002 31003 31004 31005 31006 31007 31008 \
         31009 31010 31011 31012 31013 31014 31015 31016; do
  "$PLAIN" gen "$S" 4096 "$CDIR/c$S" || CFAIL=1
  timeout 120 "$PLAIN" corpus "$CDIR/c$S" > "$TMPD/corpus_$S.out" 2>&1
  RC=$?
  cat "$TMPD/corpus_$S.out"
  if [ $RC -ne 0 ] || grep -q "FINDING" "$TMPD/corpus_$S.out"; then
    echo "LEG corpus seed=$S FAIL rc=$RC"; CFAIL=1
  fi
done
if [ $CFAIL -ne 0 ]; then FAIL=1; else echo "LEG corpus PASS (16 files)"; fi

# ---- leg 7: libFuzzer ----
if command -v clang > /dev/null 2>&1 \
   && [ -f build/mimir_alloc.o ] && [ -f build/mimir_core.o ] \
   && [ -f build/mimir_platform.o ]; then
  if clang -std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc \
       -O1 -g -DZ109_LIBFUZZER -fsanitize=fuzzer,address,undefined \
       tests/fuzz/fuzz_z109.c build/mimir_alloc.o build/mimir_core.o \
       build/mimir_platform.o -o "$TMPD/fuzz_z109_lf" 2> "$TMPD/lf_build.log"; then
    timeout 600 "$TMPD/fuzz_z109_lf" "$CDIR" -runs="$LF_RUNS" \
      -max_len=512 > "$TMPD/lf.out" 2>&1
    RC=$?
    tail -n 5 "$TMPD/lf.out"
    if [ $RC -ne 0 ] || ! grep -q "Done $LF_RUNS runs" "$TMPD/lf.out"; then
      echo "LEG libfuzzer FAIL rc=$RC"; FAIL=1
    else
      echo "LEG libfuzzer PASS ($LF_RUNS runs, ASan+UBSan)"
    fi
  else
    cat "$TMPD/lf_build.log"
    echo "LEG libfuzzer FAIL build"; FAIL=1
  fi
else
  echo "LEG libfuzzer SKIP (clang or library objects absent)"
fi

# ---- totals ----
cat "$TMPD"/*.out > "$TMPD/all.out" 2>/dev/null || true
TOT_CHECKS=$(grep -o "checks=[0-9]*" "$TMPD/all.out" | awk -F= "{s+=\$2} END {print s+0}")
TOT_FAILS=$(grep -o "fails=[0-9]*" "$TMPD/all.out" | awk -F= "{s+=\$2} END {print s+0}")
TOT_OPS=$(grep "^SEED " "$TMPD/all.out" | awk "{for(i=1;i<=NF;i++) if(\$i ~ /^ops=/){split(\$i,a,\"=\"); s+=a[2]}} END {print s+0}")
echo "Z109-TOTALS stream_ops=$TOT_OPS checks=$TOT_CHECKS fails=$TOT_FAILS"
echo "Z109-LOAD-END $(cat /proc/loadavg)"
if [ "$FAIL" -ne 0 ] || [ "$TOT_FAILS" != "0" ]; then
  echo "Z109-RESULT: FAIL"
  exit 1
fi
echo "Z109-RESULT: PASS"
exit 0
