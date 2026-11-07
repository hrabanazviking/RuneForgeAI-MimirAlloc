#!/bin/sh
# run_l007.sh - L-007 protocol gate validation runs.
# Builds probe_l007.c (J-002 pattern, Makefile NOT modified)
# and exercises the protocol gates on synthetic headers:
#  case PASS: identical controls + identical outputs -> exit 0
#  case THREADS-DIFF: threads differ -> gate must FAIL (exit 1)
#  case THERMAL-DIFF: start temps 5C apart -> must FAIL
#  case OUTPUT-DIFF: outputs differ -> must FAIL
# A case that does not behave as labelled fails this script.
set -u
cd "$(dirname "$0")/../.." || exit 2
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -Iinclude -Isrc -O2 -DMIMIR_DEBUG=1"
echo "--- build probe_l007 ---"
cc $CFLAGS tests/perf/probe_l007.c -o /tmp/probe_l007 -lm || exit 1
echo "build exit=0"
sha256sum tests/perf/probe_l007.c /tmp/probe_l007
D=/tmp/l007_fix; rm -rf $D; mkdir -p $D
printf 'model_sha256=aaaa\nprompt_sha256=bbbb\nbackend_sha256=cccc\ncontext=2048\nthreads=4\ntemp_start_c=57.0\nthrottled=0\n' > $D/base.hdr
cp $D/base.hdr $D/comp.hdr
sed 's/threads=4/threads=8/' $D/base.hdr > $D/comp_threads.hdr
sed 's/temp_start_c=57.0/temp_start_c=62.0/' $D/base.hdr > $D/comp_thermal.hdr
printf 'the quick brown fox\n' > $D/out_base.txt
cp $D/out_base.txt $D/out_comp.txt
printf 'the quick brown FOX\n' > $D/out_diff.txt
fails=0
run_case() { # name expect_exit hdr2 out2
  echo "--- case $1 (expect exit $2) ---"
  timeout 30 /tmp/probe_l007 $D/base.hdr $3 $D/out_base.txt $4
  rc=$?
  echo "case $1 exit=$rc"
  [ "$rc" = "$2" ] || { echo "case $1 MISMATCH"; fails=1; }
}
run_case PASS 0 $D/comp.hdr $D/out_comp.txt
run_case THREADS-DIFF 1 $D/comp_threads.hdr $D/out_comp.txt
run_case THERMAL-DIFF 1 $D/comp_thermal.hdr $D/out_comp.txt
run_case OUTPUT-DIFF 1 $D/comp.hdr $D/out_diff.txt
echo "run_l007 RESULT: $([ $fails = 0 ] && echo PASS || echo FAIL)"
exit $fails
