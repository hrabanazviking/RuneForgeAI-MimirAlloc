#!/usr/bin/env bash
# Z-088 failure drills under preload: corruption and kill
# drills inside processes preloaded with the Z-082
# interposing variant (build/libmimiralloc_interpose.so).
# The victim (tests/abi/drill_z088.c) is an ordinary
# glibc consumer linked against no MimirAlloc artifact;
# every drill runs inside a victim process THIS runner
# spawns, and the allocator's own words are the evidence:
# the J-007 diag lines it writes to fd 2 (emission gated
# by MIMIRALLOC_DIAG=1, decision D-011) and the counters
# of the public statistics snapshot, read back by the
# victim through dlsym.
#
# SAFETY LAW (this slice, recorded in CAMPAIGN_STATE.md):
# drills run only inside processes this runner itself
# spawns from this worktree; no fault is injected into
# any running user or system process; the kill drills
# signal only PIDs this runner itself spawned for the
# drill (its own children, held in shell variables).
#
# A drill PASSes iff every fact it was predicted to
# produce is present in its own logs:
#   LOUD drills (doublefree, doublefree-calloc, poison,
#   overflow, huge): the predicted diag line(s) on the
#   victim's stderr, the predicted counter movement in
#   its STATS lines, the victim alive at the end of its
#   script (exit 0), and post-drill allocations working.
#   SILENT drills (badfree-int, badfree-foreign,
#   badfree-large): exactly zero diag lines and zero
#   counter movement, PLUS the consistency proof --
#   live-object counts showing the bad free changed no
#   state, the original block (where one exists) still
#   freeing properly afterwards, and a 256-block
#   pattern-verified churn completing clean. A silent
#   drill that corrupted anything would fail here, not
#   pass quietly: silence is the classified outcome only
#   when non-corruption is proven in the same run. Each
#   silent path is filed as a defect with an owner slice
#   in S16 (the roadmap gate's second clause).
#   KILL drills: the churn victim, spawned by this
#   runner, is observed heartbeating, signalled by this
#   runner, and reaped with the signal's wait status
#   (137 = SIGKILL, 143 = SIGTERM); the PID is gone
#   afterwards and a fresh control drill passes, proving
#   no cross-process contamination (the allocator keeps
#   no shared or persistent state to corrupt).
# Per-drill full logs land in hidden_files/z088_<name>.log
# (stdout, stderr, exit code, load state). Output is the
# recorded run itself; the caller tees it to
# tests/abi/recorded_run_z13_z088.txt. Exit 0 iff every
# drill PASSes; any failed expectation is a [FAIL] line
# in the run and exit 1, never hidden.
set -u
cd "$(dirname "$0")/../.."
FAIL=0
TOTAL=0
OKN=0
note() { echo "$*"; }
chk() { if [ "$1" -eq 0 ]; then echo "  [ok] $2"; else echo "  [FAIL] $2"; FAIL=1; fi; }
sec_begin() { SEC_START=$FAIL; }
sec_end() { TOTAL=$((TOTAL + 1)); if [ "$FAIL" -eq "$SEC_START" ]; then OKN=$((OKN + 1)); fi; }

SO="$PWD/build/libmimiralloc_interpose.so"
VICTIM="./build/drill_z088"
TMPD="$(mktemp -d /tmp/z088_drills.XXXXXX)"
trap 'rm -rf "$TMPD"' EXIT

echo "=== Z-088 failure drills under preload ==="
echo "date_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "host: $(uname -a)"
echo "cc: $(cc --version | head -1)"
echo "glibc: $(ldd --version | head -1)"
echo "head: $(git rev-parse HEAD)"
echo "branch: $(git branch --show-current)"
echo "loadavg: $(cat /proc/loadavg)"
echo "page_size: $(getconf PAGESIZE)"
echo "preload_target: build/libmimiralloc_interpose.so (Z-082 interposing variant)"
echo "victim: build/drill_z088 (tests/abi/drill_z088.c; links no MimirAlloc artifact; -ldl only)"
echo "safety: every victim is spawned by this runner from this worktree; kill drills signal only runner-spawned PIDs"

# run_drill NAME MODE DIAG -- DIAG is on, off, or plain.
# Fills $TMPD/NAME.out, $TMPD/NAME.err, $TMPD/NAME.rc and
# the per-drill log hidden_files/z088_NAME.log.
run_drill() {
  local name="$1" mode="$2" diag="$3"
  case "$diag" in
    on)    timeout 60 env MIMIRALLOC_DIAG=1 LD_PRELOAD="$SO" $VICTIM "$mode" > "$TMPD/$name.out" 2> "$TMPD/$name.err" ;;
    off)   timeout 60 env -u MIMIRALLOC_DIAG LD_PRELOAD="$SO" $VICTIM "$mode" > "$TMPD/$name.out" 2> "$TMPD/$name.err" ;;
    plain) timeout 60 env -u MIMIRALLOC_DIAG -u LD_PRELOAD $VICTIM "$mode" > "$TMPD/$name.out" 2> "$TMPD/$name.err" ;;
  esac
  echo "$?" > "$TMPD/$name.rc"
  {
    echo "# Z-088 drill log: $name (mode=$mode diag=$diag)"
    echo "# loadavg: $(cat /proc/loadavg)"
    echo "## stdout"
    cat "$TMPD/$name.out"
    echo "## stderr"
    cat "$TMPD/$name.err"
    echo "## rc"
    cat "$TMPD/$name.rc"
  } > "hidden_files/z088_${name}.log"
}
rc_of() { cat "$TMPD/$1.rc"; }
diag_count() { grep -c '^mimiralloc: error' "$TMPD/$1.err" || true; }
has_out() { grep -q "$2" "$TMPD/$1.out"; }
has_err() { grep -q "$2" "$TMPD/$1.err"; }
# stat_field NAME TAG FIELD -- numeric value of FIELD=
# on the victim's "STATS TAG" line.
stat_field() {
  grep "^STATS $2 " "$TMPD/$1.out" | sed -n "s/.* $3=\\([0-9][0-9]*\\).*/\\1/p" | head -1
}
expect_mimir() {
  has_out "$1" 'PRELOAD-VERDICT: MIMIR'; chk $? "$1: interposition proven (PRELOAD-VERDICT: MIMIR)"
  has_out "$1" 'snap=found'; chk $? "$1: public stats snapshot resolved in the preloaded process"
}

echo "=== S1 build from clean ==="
sec_begin
make clean >/dev/null 2>&1
make lib-interpose build/drill_z088 > hidden_files/z088_build.log 2>&1
chk $? "make lib-interpose + build/drill_z088 (log hidden_files/z088_build.log)"
ls -la build/libmimiralloc_interpose.so build/drill_z088
echo "interpose exports: $(nm -D --defined-only build/libmimiralloc_interpose.so | wc -l) (expect 20 = 10 namespaced + 10 standard, Z-082)"
[ "$(nm -D --defined-only build/libmimiralloc_interpose.so | wc -l)" -eq 20 ]
chk $? "interposing .so export set is exactly the Z-082 twenty"
sec_end

echo "=== S2 mechanism proof: plain leg is a glibc world ==="
sec_begin
run_drill plain_control control plain
[ "$(rc_of plain_control)" -eq 0 ]; chk $? "plain control exit 0 (rc=$(rc_of plain_control))"
has_out plain_control 'PRELOAD-VERDICT: GLIBC'; chk $? "plain control verdict is GLIBC (no preload, no MIMIR facts claimed)"
has_out plain_control 'snap=absent'; chk $? "plain control finds no mimir_stats_snapshot (contrast leg is honest)"
sec_end

echo "=== S3 control drill under preload (diag on, no fault) ==="
sec_begin
run_drill control control on
[ "$(rc_of control)" -eq 0 ]; chk $? "control exit 0 (rc=$(rc_of control))"
expect_mimir control
[ "$(diag_count control)" -eq 0 ]; chk $? "control: zero diag lines (no failure, no report)"
DF="$(stat_field control after df)"; PO="$(stat_field control after poison)"
ME="$(stat_field control after meta)"; FA="$(stat_field control after fail)"
[ "$DF" = "0" ] && [ "$PO" = "0" ] && [ "$ME" = "0" ] && [ "$FA" = "0" ]
chk $? "control: detection/failure counters all zero (df=$DF poison=$PO meta=$ME fail=$FA)"
SZ="$(stat_field control after size)"; SOZ="$(stat_field control after sizeof)"
[ -n "$SZ" ] && [ "$SZ" = "$SOZ" ]; chk $? "control: stats struct self-validates (struct_size=$SZ sizeof=$SOZ)"
has_out control 'DRILL-RESULT control SURVIVED'; chk $? "control: victim completed its script"
sec_end

echo "=== S4 corruption drill: double free, small block (diag on) ==="
sec_begin
run_drill doublefree doublefree on
[ "$(rc_of doublefree)" -eq 0 ]; chk $? "doublefree exit 0 (rc=$(rc_of doublefree)) -- the refusal is not a crash"
expect_mimir doublefree
[ "$(diag_count doublefree)" -eq 1 ]; chk $? "doublefree: exactly one diag line"
has_err doublefree 'mimiralloc: error op=free cause=double-free'; chk $? "doublefree: diag line is op=free cause=double-free (LOUD)"
[ "$(stat_field doublefree after df)" = "1" ]; chk $? "doublefree: double_free_detections=1"
has_out doublefree 'second free returned'; chk $? "doublefree: second free was refused and returned (block not freed twice)"
has_out doublefree 'post malloc ok'; chk $? "doublefree: allocator healthy after the refusal"
sec_end

echo "=== S5 double free with the diag gate closed (D-011 posture) ==="
sec_begin
run_drill doublefree_off doublefree off
[ "$(rc_of doublefree_off)" -eq 0 ]; chk $? "doublefree (diag off) exit 0 (rc=$(rc_of doublefree_off))"
expect_mimir doublefree_off
[ "$(diag_count doublefree_off)" -eq 0 ]; chk $? "doublefree (diag off): zero diag lines -- emission is the D-011 opt-in"
[ "$(stat_field doublefree_off after df)" = "1" ]; chk $? "doublefree (diag off): detection still tallied (df=1) -- detection is always-on, loudness is gated"
sec_end

echo "=== S6 corruption drill: double free, calloc block (diag on) ==="
sec_begin
run_drill dfcalloc doublefree-calloc on
[ "$(rc_of dfcalloc)" -eq 0 ]; chk $? "doublefree-calloc exit 0 (rc=$(rc_of dfcalloc))"
expect_mimir dfcalloc
has_err dfcalloc 'mimiralloc: error op=free cause=double-free'; chk $? "doublefree-calloc: diag line is op=free cause=double-free (LOUD)"
[ "$(stat_field dfcalloc after df)" = "1" ]; chk $? "doublefree-calloc: double_free_detections=1"
sec_end

echo "=== S7 corruption drill: use-after-free write into the poison range (diag on) ==="
sec_begin
run_drill poison poison on
[ "$(rc_of poison)" -eq 0 ]; chk $? "poison exit 0 (rc=$(rc_of poison))"
expect_mimir poison
has_out poison 'same=1'; chk $? "poison: the class was re-served from the very block that was poisoned"
has_err poison 'mimiralloc: error op=malloc cause=poison'; chk $? "poison: diag line is op=malloc cause=poison at handout (LOUD)"
[ "$(stat_field poison after poison)" = "1" ]; chk $? "poison: poison_detections=1"
sec_end

echo "=== S8 corruption drill: overflow from a live block into a freed neighbour (diag on) ==="
sec_begin
run_drill overflow overflow on
RC_OV="$(rc_of overflow)"
if [ "$RC_OV" -eq 4 ]; then
  echo "  [FAIL] overflow: victim found no adjacent pair in this process (INCONCLUSIVE -- layout fact in hidden_files/z088_overflow.log; not counted as evidence either way)"
  FAIL=1
else
  [ "$RC_OV" -eq 0 ]; chk $? "overflow exit 0 (rc=$RC_OV)"
  expect_mimir overflow
  grep -q 'adjacent_lo=[0-9]' "$TMPD/overflow.out"; chk $? "overflow: an adjacent live pair was located at run time (layout facts in the drill log)"
  has_out overflow 'neighbour=1'; chk $? "overflow: the class was re-served from the overflowed neighbour"
  has_err overflow 'mimiralloc: error op=malloc cause=poison'; chk $? "overflow: diag line is op=malloc cause=poison at handout (LOUD)"
  [ "$(stat_field overflow after poison)" = "1" ]; chk $? "overflow: poison_detections=1"
fi
sec_end

echo "=== S9 bad-free drill: interior pointer (diag on) ==="
sec_begin
run_drill badfree_int badfree-int on
[ "$(rc_of badfree_int)" -eq 0 ]; chk $? "badfree-int exit 0 (rc=$(rc_of badfree_int))"
expect_mimir badfree_int
[ "$(diag_count badfree_int)" -eq 0 ]; chk $? "badfree-int: zero diag lines (SILENT -- classified below, filed F-Z088-1)"
LA="$(stat_field badfree_int after-alloc live_small)"
LI="$(stat_field badfree_int after-interior live_small)"
LP="$(stat_field badfree_int after-proper live_small)"
[ "$LA" = "1" ] && [ "$LI" = "1" ] && [ "$LP" = "0" ]
chk $? "badfree-int: live_small 1 -> 1 -> 0 across alloc/interior-free/proper-free (live_small=$LA/$LI/$LP): the interior free freed nothing and broke nothing"
[ "$(stat_field badfree_int after-proper df)" = "0" ]; chk $? "badfree-int: the proper free afterwards is a clean first free (df=0, no false double-free)"
has_out badfree_int 'churn verify OK'; chk $? "badfree-int: 256-block pattern churn clean after the drill (no corruption)"
echo "  CLASSIFICATION badfree-int: SILENT-SAFE -- silently ignored, state proven consistent in this run; loudness gap filed as F-Z088-1, owner Z-090"
sec_end

echo "=== S10 bad-free drill: foreign pointer (diag on) ==="
sec_begin
run_drill badfree_foreign badfree-foreign on
[ "$(rc_of badfree_foreign)" -eq 0 ]; chk $? "badfree-foreign exit 0 (rc=$(rc_of badfree_foreign))"
expect_mimir badfree_foreign
[ "$(diag_count badfree_foreign)" -eq 0 ]; chk $? "badfree-foreign: zero diag lines (SILENT -- classified below, filed F-Z088-1)"
has_out badfree_foreign 'churn verify OK'; chk $? "badfree-foreign: 256-block pattern churn clean after the drill (no corruption)"
echo "  CLASSIFICATION badfree-foreign: SILENT-SAFE -- silently ignored, state proven consistent in this run; loudness gap filed as F-Z088-1, owner Z-090"
sec_end

echo "=== S11 bad-free drill: double free, large mapping-path block (diag on) ==="
sec_begin
run_drill badfree_large badfree-large on
[ "$(rc_of badfree_large)" -eq 0 ]; chk $? "badfree-large exit 0 (rc=$(rc_of badfree_large))"
expect_mimir badfree_large
[ "$(diag_count badfree_large)" -eq 0 ]; chk $? "badfree-large: zero diag lines (SILENT -- classified below, filed F-Z088-2)"
MA="$(stat_field badfree_large after-alloc live_map)"
M1="$(stat_field badfree_large after-free1 live_map)"
M2="$(stat_field badfree_large after-free2 live_map)"
[ -n "$MA" ] && [ -n "$M1" ] && [ "$M1" = "$M2" ] && [ "$MA" -gt "$M1" ]
chk $? "badfree-large: live_map alloc=$MA free1=$M1 free2=$M2 -- first free retired the mapping, second free changed no state"
[ "$(stat_field badfree_large after-free2 df)" = "0" ]; chk $? "badfree-large: no double-free tally on the large course (df=0; detection is small-path only by Z-070's design)"
has_out badfree_large 'churn verify OK'; chk $? "badfree-large: 256-block pattern churn clean after the drill (no corruption)"
echo "  CLASSIFICATION badfree-large: SILENT-BY-DESIGN -- the Z-070 scope exclusion (large mapping-path blocks carry no membership/poison guard) behaving as banked; restated for the verdict as F-Z088-2, owner Z-090"
sec_end

echo "=== S12 failure drill: unsatisfiable requests (diag on) ==="
sec_begin
run_drill huge huge on
[ "$(rc_of huge)" -eq 0 ]; chk $? "huge exit 0 (rc=$(rc_of huge)) -- failure NULLs, not crashes"
expect_mimir huge
[ "$(diag_count huge)" -eq 3 ]; chk $? "huge: exactly three diag lines, one per refused request"
has_err huge 'op=malloc cause=backing req=4611686018427387904'; chk $? "huge: malloc(2^62) refused LOUD (op=malloc cause=backing)"
has_err huge 'op=calloc cause=arithmetic req=18446744073709551615'; chk $? "huge: calloc(2^32,2^32) refused LOUD (op=calloc cause=arithmetic, req=SIZE_MAX)"
has_err huge 'op=realloc cause=arithmetic req=18446744073709551615'; chk $? "huge: reallocarray(2^33,2^33) refused LOUD (op=realloc cause=arithmetic, req=SIZE_MAX)"
has_out huge 'malloc(2^62)=(nil)'; chk $? "huge: malloc returned NULL"
has_out huge 'calloc(2^32,2^32)=(nil)'; chk $? "huge: calloc returned NULL (no wrapped allocation)"
has_out huge 'reallocarray(2^33,2^33)=(nil)'; chk $? "huge: reallocarray returned NULL (no wrapped allocation)"
[ "$(stat_field huge after fail)" = "3" ]; chk $? "huge: alloc_failures=3 (one per refusal; posture nulls are a separate counter)"
has_out huge 'post malloc ok'; chk $? "huge: allocator healthy after the refusals"
sec_end

echo "=== S13 kill drill: SIGKILL mid-churn ==="
sec_begin
MIMIRALLOC_DIAG=1 LD_PRELOAD="$SO" $VICTIM churn > "$TMPD/killkill.out" 2> "$TMPD/killkill.err" &
BPID=$!
HB=1
for i in $(seq 1 100); do
  if grep -q 'HEARTBEAT' "$TMPD/killkill.out" 2>/dev/null; then HB=0; break; fi
  kill -0 $BPID 2>/dev/null || break
  sleep 0.1
done
echo "victim pid=$BPID (spawned by this runner; no other process is signalled)"
grep -q 'CHURN-READY' "$TMPD/killkill.out"; chk $? "kill-sigkill: victim started under preload (CHURN-READY)"
grep -q 'PRELOAD-VERDICT: MIMIR' "$TMPD/killkill.out"; chk $? "kill-sigkill: interposition proven in the victim"
[ "$HB" -eq 0 ]; chk $? "kill-sigkill: victim observed heartbeating mid-churn before the signal"
kill -9 $BPID
wait $BPID; KRC=$?
[ "$KRC" -eq 137 ]; chk $? "kill-sigkill: wait status 137 = 128+SIGKILL (rc=$KRC)"
kill -0 $BPID 2>/dev/null; [ $? -ne 0 ]; chk $? "kill-sigkill: victim PID is gone after the reap"
{
  echo "# Z-088 drill log: kill_sigkill (mode=churn signal=SIGKILL)"
  echo "# victim pid=$BPID (runner-spawned) wait_rc=$KRC loadavg: $(cat /proc/loadavg)"
  echo "## stdout"; cat "$TMPD/killkill.out"
  echo "## stderr"; cat "$TMPD/killkill.err"
} > hidden_files/z088_kill_sigkill.log
sec_end

echo "=== S14 kill drill: SIGTERM mid-churn ==="
sec_begin
MIMIRALLOC_DIAG=1 LD_PRELOAD="$SO" $VICTIM churn > "$TMPD/killterm.out" 2> "$TMPD/killterm.err" &
BPID=$!
HB=1
for i in $(seq 1 100); do
  if grep -q 'HEARTBEAT' "$TMPD/killterm.out" 2>/dev/null; then HB=0; break; fi
  kill -0 $BPID 2>/dev/null || break
  sleep 0.1
done
echo "victim pid=$BPID (spawned by this runner; no other process is signalled)"
grep -q 'CHURN-READY' "$TMPD/killterm.out"; chk $? "kill-sigterm: victim started under preload (CHURN-READY)"
grep -q 'PRELOAD-VERDICT: MIMIR' "$TMPD/killterm.out"; chk $? "kill-sigterm: interposition proven in the victim"
[ "$HB" -eq 0 ]; chk $? "kill-sigterm: victim observed heartbeating mid-churn before the signal"
kill -TERM $BPID
wait $BPID; KRC=$?
[ "$KRC" -eq 143 ]; chk $? "kill-sigterm: wait status 143 = 128+SIGTERM (rc=$KRC)"
kill -0 $BPID 2>/dev/null; [ $? -ne 0 ]; chk $? "kill-sigterm: victim PID is gone after the reap"
{
  echo "# Z-088 drill log: kill_sigterm (mode=churn signal=SIGTERM)"
  echo "# victim pid=$BPID (runner-spawned) wait_rc=$KRC loadavg: $(cat /proc/loadavg)"
  echo "## stdout"; cat "$TMPD/killterm.out"
  echo "## stderr"; cat "$TMPD/killterm.err"
} > hidden_files/z088_kill_sigterm.log
sec_end

echo "=== S15 post-kill control: a fresh preloaded process is uncontaminated ==="
sec_begin
run_drill postkill control on
[ "$(rc_of postkill)" -eq 0 ]; chk $? "post-kill control exit 0 (rc=$(rc_of postkill))"
expect_mimir postkill
[ "$(diag_count postkill)" -eq 0 ]; chk $? "post-kill control: zero diag lines"
DF="$(stat_field postkill after df)"; PO="$(stat_field postkill after poison)"
[ "$DF" = "0" ] && [ "$PO" = "0" ]; chk $? "post-kill control: fresh process counters clean (df=$DF poison=$PO) -- a killed peer leaves nothing behind"
sec_end

echo "=== S16 classification summary and defect filings ==="
echo "LOUD through the diag channel (MIMIRALLOC_DIAG=1), victim surviving, counters moving:"
echo "  doublefree        op=free cause=double-free    df 0->1      (S4)"
echo "  doublefree-calloc op=free cause=double-free    df 0->1      (S6)"
echo "  poison (UAF write) op=malloc cause=poison      poison 0->1  (S7)"
echo "  overflow (live->freed neighbour) op=malloc cause=poison    poison 0->1 (S8)"
echo "  huge malloc       op=malloc cause=backing      fail 0->1    (S12)"
echo "  huge calloc       op=calloc cause=arithmetic   fail 1->2    (S12)"
echo "  huge reallocarray op=realloc cause=arithmetic  fail 2->3    (S12)"
echo "DETECTION without loudness (by the D-011 ruling, not a drill failure):"
echo "  doublefree with MIMIRALLOC_DIAG unset: df 0->1, zero diag lines (S5)"
echo "SILENT paths (no diag line, no counter) -- each filed here with an owner slice:"
echo "  F-Z088-1 badfree-int + badfree-foreign: a free of a pointer the allocator"
echo "    cannot find (interior or foreign) returns silently. Non-corruption is"
echo "    PROVEN in this run (live counts unchanged; the original block frees"
echo "    properly afterwards; 256-block pattern churn clean). The public header"
echo "    already declares these calls undefined behaviour with no defined"
echo "    error, so the gap is loudness only: the roadmap's 'fail loudly'"
echo "    sentence does not hold for them. OWNER: Z-090 (Wave 10 verdict) --"
echo "    restate for preload users; any code change is a later hardening"
echo "    slice's to propose, not this slice's to smuggle in."
echo "  F-Z088-2 badfree-large: a second free of a large mapping-path block is"
echo "    silently ignored. This is Z-070's banked scope exclusion (membership"
echo "    + poison guard the small classes only) behaving as designed; the"
echo "    mapping is already retired, so the ignore is state-safe (live_map"
echo "    proof in S11). OWNER: Z-090 -- restate beside F-Z088-1."
echo "KILL drills: SIGKILL wait 137, SIGTERM wait 143, victims reaped, PIDs"
echo "  gone, post-kill control clean (S13-S15). The allocator holds no"
echo "  cross-process state, so a killed preloaded process contaminates"
echo "  nothing -- proven, not assumed."
echo "drills: total=$TOTAL ok=$OKN failed=$((TOTAL - OKN))"
echo "loadavg at end: $(cat /proc/loadavg)"
if [ "$FAIL" -ne 0 ]; then
  echo "Z-088 RESULT: FAIL"
  exit 1
fi
echo "Z-088 RESULT: PASS"
exit 0
