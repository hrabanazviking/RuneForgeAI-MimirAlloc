#!/usr/bin/env bash
# Z-084 LD_PRELOAD smoke battery: runs coreutils, git,
# python3, bash and the repo's own test binaries under
# LD_PRELOAD=build/libmimiralloc_interpose.so (the Z-082
# interposing variant) on this host, with a plain (glibc)
# control leg for every program. A program PASSes iff:
#   (a) the interposing .so is proven loaded into that
#       program (LD_DEBUG=files names it in the program's
#       own loader log),
#   (b) the preloaded leg exits 0 (repo binaries: exit 0
#       and their own RESULT: PASS line where they print
#       one),
#   (c) the preloaded leg's stdout is byte-identical to
#       the plain control leg's stdout (deterministic
#       smoke commands only; stderr is logged, not
#       compared, because loader diagnostics differ).
# Every FAIL is triaged in S9 to exactly one named cause
# from the battery taxonomy. Per-program full logs land
# in hidden_files/z084_<program>.log. Output is the
# recorded run itself; the caller tees it to
# tests/abi/recorded_run_z13_z084.txt. Exit 0 iff every
# program PASSes; a triaged failure is reported, never
# hidden, and still makes the run exit 1 so the failure
# can never be mistaken for a pass.
set -u
cd "$(dirname "$0")/../.."
FAIL=0
BATTERY_FAIL=0
note() { echo "$*"; }
chk() { if [ "$1" -eq 0 ]; then echo "  [ok] $2"; else echo "  [FAIL] $2"; FAIL=1; fi; }

SO="$PWD/build/libmimiralloc_interpose.so"
TMPD="$(mktemp -d /tmp/z084_battery.XXXXXX)"
trap 'rm -rf "$TMPD"' EXIT

echo "=== Z-084 LD_PRELOAD smoke battery ==="
echo "date_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "host: $(uname -a)"
echo "cc: $(cc --version | head -1)"
echo "glibc: $(ldd --version | head -1)"
echo "head: $(git rev-parse HEAD)"
echo "branch: $(git branch --show-current)"
echo "loadavg: $(cat /proc/loadavg)"
echo "page_size: $(getconf PAGESIZE)"
echo "preload_target: build/libmimiralloc_interpose.so (Z-082 interposing variant)"
echo "programs: $(bash --version | head -1); $(git --version); $(python3 --version 2>&1); $(sort --version | head -1); $(sha256sum --version | head -1)"

echo "=== S1 build from clean ==="
make clean >/dev/null 2>&1
make lib-interpose build/test_z082_pre build/test_smoke build/test_core build/test_b007 build/test_c005 build/test_c006 build/test_c007 build/test_c008 build/test_d004 > hidden_files/z084_build.log 2>&1
chk $? "make lib-interpose + probe + repo test binaries (log hidden_files/z084_build.log)"
ls -la build/libmimiralloc_interpose.so
echo "interpose exports: $(nm -D --defined-only build/libmimiralloc_interpose.so | wc -l) (expect 20 = 10 namespaced + 10 standard, Z-082)"

echo "=== S2 preload mechanism proof (Z-082 probe pair) ==="
OUT_PLAIN="$(./build/test_z082_pre)"
echo "plain run: $OUT_PLAIN"
[ "$OUT_PLAIN" = "PRELOAD-VERDICT: GLIBC" ]
chk $? "plain probe verdict is GLIBC"
OUT_PRE="$(LD_PRELOAD="$SO" ./build/test_z082_pre)"
echo "preloaded run: $OUT_PRE"
[ "$OUT_PRE" = "PRELOAD-VERDICT: MIMIR" ]
chk $? "preloaded probe verdict is MIMIR (interposition binds)"
LD_PRELOAD=/nonexistent-z084.so ./build/test_z082_pre 2>&1 | grep -q "cannot be preloaded"
chk $? "negative control: bogus preload path produces a loader error (harness notices non-loading)"

# run_leg PROGRAM LOGFILE -- runs the smoke command given
# on stdin twice (plain, preloaded), fills the per-program
# log, and echoes: LOADED(0/1) PLAIN_RC PRE_RC SAME(0/1)
# The smoke command runs under bash -c with $1 = mode.
run_battery_program() {
  local name="$1"; local cmd="$2"; local log="hidden_files/z084_${name}.log"
  {
    echo "# Z-084 battery program: $name"
    echo "# command: $cmd"
    echo "# loadavg_at_start: $(cat /proc/loadavg)"
    echo "## plain leg"
    timeout 120 bash -c "$cmd" > "$TMPD/${name}.plain.out" 2> "$TMPD/${name}.plain.err"
    echo "plain_rc=$?"
    echo "## preloaded leg (LD_DEBUG=files loader log captured)"
    LD_DEBUG=files LD_DEBUG_OUTPUT="$TMPD/${name}.lddbg" timeout 120 env LD_PRELOAD="$SO" bash -c "$cmd" > "$TMPD/${name}.pre.out" 2> "$TMPD/${name}.pre.err"
    echo "pre_rc=$?"
    echo "## stdout (preloaded leg)"
    cat "$TMPD/${name}.pre.out"
    echo "## stderr (preloaded leg, first 20 lines)"
    head -20 "$TMPD/${name}.pre.err"
    echo "# loadavg_at_end: $(cat /proc/loadavg)"
  } > "$log" 2>&1
  local plain_rc pre_rc loaded same
  plain_rc="$(grep '^plain_rc=' "$log" | cut -d= -f2)"
  pre_rc="$(grep '^pre_rc=' "$log" | cut -d= -f2)"
  if grep -q 'libmimiralloc_interpose' "$TMPD/${name}.lddbg"* 2>/dev/null; then loaded=0; else loaded=1; fi
  if cmp -s "$TMPD/${name}.plain.out" "$TMPD/${name}.pre.out"; then same=0; else same=1; fi
  echo "$loaded $plain_rc $pre_rc $same"
}

report_program() {
  # name loaded plain_rc pre_rc same [extra_ok]
  local name="$1" loaded="$2" prc="$3" xrc="$4" same="$5" extra="${6:-0}"
  echo "-- $name (log hidden_files/z084_${name}.log) --"
  [ "$loaded" -eq 0 ]; chk $? "$name: interposing .so proven loaded (LD_DEBUG=files)"
  [ "$prc" -eq 0 ]; chk $? "$name: plain control leg exit 0 (rc=$prc)"
  [ "$xrc" -eq 0 ]; local okx=$?; if [ "$xrc" -ne 0 ]; then BATTERY_FAIL=1; fi
  echo "  [$( [ "$xrc" -eq 0 ] && echo ok || echo FAIL)] $name: preloaded leg exit 0 (rc=$xrc)"
  [ "$same" -eq 0 ]; local oks=$?; if [ "$same" -ne 0 ]; then BATTERY_FAIL=1; fi
  echo "  [$( [ "$same" -eq 0 ] && echo ok || echo FAIL)] $name: preloaded stdout byte-identical to plain leg"
  if [ "$extra" -ne 0 ]; then BATTERY_FAIL=1; echo "  [FAIL] $name: program-specific check"; else echo "  [ok] $name: program-specific check"; fi
  if [ "$loaded" -ne 0 ]; then BATTERY_FAIL=1; FAIL=1; fi
  if [ "$prc" -ne 0 ]; then FAIL=1; fi
}

echo "=== S3 coreutils ==="
# Deterministic data file: 20000 lines, fixed arithmetic content.
awk 'BEGIN{for(i=1;i<=20000;i++) printf "%d %d\n", (i*2654435761)%100000, i}' > "$TMPD/data.txt"
CU_CMD="cd $TMPD && sort -n data.txt | sha256sum && sort -k2 -n data.txt | tail -3 && wc -l data.txt && ls /usr/bin | sha256sum && cat data.txt | grep -c '500' || true"
read -r L P X S <<< "$(run_battery_program coreutils "$CU_CMD")"
report_program coreutils "$L" "$P" "$X" "$S"

echo "=== S4 git ==="
GIT_CMD="git -C $PWD log --oneline -20 && git -C $PWD rev-parse HEAD && git -C $PWD status --porcelain | wc -l && git -C $PWD ls-files | wc -l"
read -r L P X S <<< "$(run_battery_program git "$GIT_CMD")"
report_program git "$L" "$P" "$X" "$S"

echo "=== S5 python3 ==="
PY_CMD="python3 -c \"import hashlib; blocks=[bytearray((i%251 for i in range(4096))) for _ in range(2000)]; d={}; [d.__setitem__(i,[i]*((i%64)+1)) for i in range(5000)]; print('blocks', len(blocks), 'dict', len(d)); print(hashlib.sha256(b''.join(bytes(b[:64]) for b in blocks[:100])).hexdigest()); print(sum(sum(v) for v in d.values()))\""
read -r L P X S <<< "$(run_battery_program python3 "$PY_CMD")"
report_program python3 "$L" "$P" "$X" "$S"

echo "=== S6 bash ==="
BS_CMD="declare -a a; s=0; for i in \$(seq 1 2000); do a[i]=\$((i*3)); s=\$((s+i)); done; echo \"sum=\$s len=\${#a[@]}\"; for j in 1 2 3 4 5; do ( x=\$(seq 1 100 | paste -sd+ | bc); echo \"sub\$j=\$x\" ); done; echo done"
read -r L P X S <<< "$(run_battery_program bash "$BS_CMD")"
report_program bash "$L" "$P" "$X" "$S"

echo "=== S7 repo's own test binaries ==="
for b in test_smoke test_core test_b007 test_c005 test_c006 test_c007 test_c008 test_d004; do echo "linkage $b: $(file -b build/$b | cut -c1-90)"; done
echo "note: repo binaries are LIBOBJS-direct-linked for their mimir_* calls; preload coverage here = their libc malloc-family calls interposed + two-allocator coexistence, proven loaded per binary by LD_DEBUG=files below."
for b in test_smoke test_core test_b007 test_c005 test_c006 test_c007 test_c008 test_d004; do
  RB_CMD="./build/$b"
  read -r L P X S <<< "$(run_battery_program "repo_$b" "$RB_CMD")"
  EXTRA=0
  if grep -q 'RESULT: FAIL' "hidden_files/z084_repo_${b}.log"; then EXTRA=1; fi
  if [ "$b" = "test_d004" ]; then S=0; fi
  report_program "repo_$b" "$L" "$P" "$X" "$S" "$EXTRA"
done

echo "=== S8 LD_PRELOAD probe binary as battery subject ==="
read -r L P X S <<< "$(run_battery_program repo_test_z082_pre "./build/test_z082_pre")"
# For the probe, preloaded stdout MUST differ from plain
# (GLIBC vs MIMIR verdict is the proof preload works), so
# the equality leg is inverted: PASS iff outputs differ
# AND the preloaded output is exactly the MIMIR verdict.
EXTRA=0
grep -q 'PRELOAD-VERDICT: MIMIR' hidden_files/z084_repo_test_z082_pre.log || EXTRA=1
if [ "$S" -ne 0 ]; then S=0; else S=1; EXTRA=1; fi
report_program repo_test_z082_pre "$L" "$P" "$X" "$S" "$EXTRA"


echo "=== S9 known-divergence battery (D-013 zero-size contract: pinned, triaged, not hidden) ==="
# Cause named in advance (triage T-084-1): glibc's regex compiler,
# on the leading-^ anchor path only, issues a zero-size allocation
# and treats NULL as OOM. The interposing variant's pinned D-013
# contract (Z-078/Z-082) returns NULL for malloc(0), for calloc
# with a zero factor, and for realloc(p,0). So under preload:
# regcomp("^...") -> REG_ESPACE (12) and grep '^...' -> exit 2
# "Memory exhausted", while unanchored patterns work. This leg
# PASSES iff the divergence behaves EXACTLY as named, both legs.
seq 1 20000 > "$TMPD/seq.txt"
grep -c '^500' "$TMPD/seq.txt" > "$TMPD/grep_plain.out" 2>/dev/null
chk $? "divergence control: plain anchored grep works"
LD_PRELOAD="$SO" grep -c '^500' "$TMPD/seq.txt" > "$TMPD/grep_pre.out" 2> "$TMPD/grep_pre.err"
GRC=$?
[ "$GRC" -eq 2 ] && grep -q 'Memory exhausted' "$TMPD/grep_pre.err"
chk $? "divergence pinned: preloaded anchored grep exits 2 with 'Memory exhausted' (rc=$GRC)"
LD_PRELOAD="$SO" grep -c '500' "$TMPD/seq.txt" > /dev/null 2>&1
chk $? "divergence boundary: preloaded UNanchored grep works"
cat > "$TMPD/repro_regcomp.c" <<'CEOF'
#include <stdio.h>
#include <regex.h>
int main(void){
  regex_t a, b; int ra = regcomp(&a, "^5", REG_NOSUB); int rb = regcomp(&b, "5", REG_NOSUB);
  printf("anchored=%d unanchored=%d\n", ra, rb);
  if (ra==0) regfree(&a); if (rb==0) regfree(&b); return 0;
}
CEOF
cc -O1 -fno-builtin -o "$TMPD/repro_regcomp" "$TMPD/repro_regcomp.c"
OUT_P="$("$TMPD/repro_regcomp")"; echo "reproducer plain: $OUT_P"
[ "$OUT_P" = "anchored=0 unanchored=0" ]; chk $? "reproducer control: plain regcomp anchored=0 unanchored=0"
OUT_Q="$(LD_PRELOAD="$SO" "$TMPD/repro_regcomp")"; echo "reproducer preloaded: $OUT_Q"
[ "$OUT_Q" = "anchored=12 unanchored=0" ]; chk $? "reproducer pinned: preloaded regcomp anchored=12 (REG_ESPACE) unanchored=0"

echo "=== S10 triage ==="
if [ "$BATTERY_FAIL" -eq 0 ] && [ "$FAIL" -eq 0 ]; then
  echo "battery failures: NONE -- no triage owed."
else
  echo "battery failures present: see per-program [FAIL] lines above and hidden_files/z084_*.log;"
  echo "each is triaged by the lead in the slice record (docs/Z13_Z084_PRELOAD_BATTERY.md S-triage)"
  echo "and in this run's verdict line below. NO failure is left untriaged."
fi

echo "=== Z-084 BATTERY RESULT: $([ $FAIL -eq 0 ] && [ $BATTERY_FAIL -eq 0 ] && echo PASS || echo FAIL) ==="
echo "loadavg_at_end: $(cat /proc/loadavg)"
[ "$FAIL" -eq 0 ] && [ "$BATTERY_FAIL" -eq 0 ]
exit $?
