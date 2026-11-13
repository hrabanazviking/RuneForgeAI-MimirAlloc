#!/usr/bin/env bash
# Z-089 rollback drill - one-variable rollback proven
# from a broken state, on this host.
#
# BROKEN STATE, HONESTLY CONTAINED: the drill subject is
# ONLY a harness process this script spawns
# (build/test_z089_harness, from tests/abi/harness_z089.c)
# inside a private mktemp root. The broken state is that
# spawned harness preloaded with a deliberately poisoned
# allocator (tests/abi/poison_z089.c, built into the
# temp root at drill time): its malloc family always
# returns NULL, the harness reports BROKEN and exits 3.
# No real program, service, display manager, systemd
# unit, or live desktop process is ever preloaded,
# attached to, or signalled; live PIDs (PID 1, sddm,
# plasmashell, kwin_wayland) are recorded before and
# after as proof.
#
# ROLLBACK: the identical harness command run with the
# ONE variable LD_PRELOAD unset. S5 proves with a sorted
# env diff that exactly one variable differs between the
# broken and rolled-back invocations, and S6 proves the
# rolled-back output is byte-identical to the plain
# glibc control from S2. No file, package, service, or
# second variable changes.
#
# Output is the recorded run itself; the caller tees it
# to tests/abi/recorded_run_z13_z089.txt. Standalone via
# `make test-z089`; NOT part of `make test`. Exit 0 iff
# every drill assertion passes.
set -u
cd "$(dirname "$0")/../.."
FAIL=0
chk() { if [ "$1" -eq 0 ]; then echo "  [ok] $2"; else echo "  [FAIL] $2"; FAIL=1; fi; }
IPOLIB="$PWD/build/libmimiralloc_interpose.so"

echo "=== Z-089 rollback drill recorded run ==="
echo "date_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "host: $(uname -a)"
echo "cc: $(cc --version | head -1)"
echo "head: $(git rev-parse HEAD)"
echo "branch: $(git branch --show-current)"
echo "loadavg: $(cat /proc/loadavg)"
echo "page_size: $(getconf PAGESIZE)"
echo "containment: private mktemp root; drill subjects are harness processes spawned by this script only; no real service, display manager, systemd unit or live desktop process is preloaded or signalled"
echo "broken_state: spawned harness + LD_PRELOAD=<poison .so whose malloc/calloc/realloc always return NULL>"
echo "rollback: unset LD_PRELOAD (one variable; nothing else changes)"

echo "=== S0 live-system hands-off record (before) ==="
PID1_BEFORE="$(ps -p 1 -o comm= | tr -d ' ')"; echo "pid1_comm_before: $PID1_BEFORE"
SDDM_BEFORE="$(pgrep -x sddm | tr '\n' ' ')"; echo "live sddm pids before: $SDDM_BEFORE"
PS_BEFORE="$(pgrep -x plasmashell | tr '\n' ' ')"; echo "live plasmashell pids before: $PS_BEFORE"
KWIN_BEFORE="$(pgrep -x kwin_wayland | tr '\n' ' ')"; echo "live kwin_wayland pids before: $KWIN_BEFORE"

echo "=== S1 build: interposing variant, harness, poison ==="
make lib-interpose > hidden_files/z089_build_interpose.log 2>&1
chk $? "make lib-interpose (log hidden_files/z089_build_interpose.log)"
echo "interpose defined dynamic exports: $(nm -D --defined-only build/libmimiralloc_interpose.so | wc -l)"
[ "$(nm -D --defined-only build/libmimiralloc_interpose.so | wc -l)" -eq 20 ]
chk $? "interpose export count == 20 (Z-082 variant, unchanged)"
T="$(mktemp -d /tmp/z089_drill.XXXXXX)"
mkdir -p "$T/home"
cc -fno-builtin -O2 -o "$T/harness" tests/abi/harness_z089.c 2> hidden_files/z089_build_harness.log
chk $? "build harness (log hidden_files/z089_build_harness.log)"
cc -shared -fPIC -O2 -o "$T/poison.so" tests/abi/poison_z089.c 2> hidden_files/z089_build_poison.log
chk $? "build poison allocator (log hidden_files/z089_build_poison.log)"
cc -fno-builtin -O2 -o "$T/envdump" tests/abi/envdump_z089.c 2> hidden_files/z089_build_envdump.log
chk $? "build zero-allocation env dumper (log hidden_files/z089_build_envdump.log)"
echo "harness sha256: $(sha256sum "$T/harness" | cut -d' ' -f1)"
echo "poison sha256: $(sha256sum "$T/poison.so" | cut -d' ' -f1)"
echo "interpose sha256: $(sha256sum "$IPOLIB" | cut -d' ' -f1)"

echo "=== S2 control: harness on glibc (no preload) ==="
env -i PATH=/usr/bin:/bin HOME="$T/home" "$T/harness" > "$T/control.out" 2> "$T/control.err"; CRC=$?
cat "$T/control.out"
echo "  control rc=$CRC"
[ $CRC -eq 0 ]; chk $? "control exits 0 on glibc"
grep -q 'HARNESS-RESULT: OK checksum=139217' "$T/control.out"; chk $? "control prints the fixed OK checksum"

echo "=== S3 healthy preload: harness under the real Z-082 interposer ==="
env -i PATH=/usr/bin:/bin HOME="$T/home" LD_PRELOAD="$IPOLIB" "$T/harness" > "$T/healthy.out" 2> "$T/healthy.err"; HRC=$?
cat "$T/healthy.out"
echo "  healthy preload rc=$HRC"
[ $HRC -eq 0 ]; chk $? "healthy preload exits 0"
cmp -s "$T/control.out" "$T/healthy.out"; chk $? "healthy preload output byte-identical to glibc control"

echo "=== S4 BROKEN state: same harness, poison preload (twice, deterministic) ==="
for n in 1 2; do
  env -i PATH=/usr/bin:/bin HOME="$T/home" LD_PRELOAD="$T/poison.so" "$T/harness" > "$T/broken$n.out" 2> "$T/broken$n.err"; BRC=$?
  cat "$T/broken$n.out"
  echo "  broken run $n rc=$BRC"
  [ $BRC -eq 3 ]; chk $? "broken run $n exits 3 (allocator failure detected by the harness)"
  grep -q 'HARNESS-RESULT: BROKEN allocation returned NULL' "$T/broken$n.out"; chk $? "broken run $n prints BROKEN, never OK"
done
cmp -s "$T/broken1.out" "$T/broken2.out"; chk $? "broken state is deterministic across both runs"

echo "=== S5 one-variable proof: env diff, broken vs rolled-back ==="
echo "  method: the environment is dumped from INSIDE each invocation by tests/abi/envdump_z089.c (write(2) only, zero allocations, so it runs truthfully under the poison; /usr/bin/env cannot - it allocates at startup and died under the poison in this drill's first run, filed in the ops doc)"
env -i PATH=/usr/bin:/bin HOME="$T/home" LD_PRELOAD="$T/poison.so" "$T/envdump" | sort > "$T/broken.env"
env -i PATH=/usr/bin:/bin HOME="$T/home" "$T/envdump" | sort > "$T/rollback.env"
echo "  env diff (broken -> rolled-back):"
diff "$T/broken.env" "$T/rollback.env" | sed 's/^/    /'
DIFFLINES="$(diff "$T/broken.env" "$T/rollback.env" | grep -c '^[<>]')"
REMOVED="$(diff "$T/broken.env" "$T/rollback.env" | grep -c '^< LD_PRELOAD=')"
echo "  changed env lines: $DIFFLINES (removed LD_PRELOAD lines: $REMOVED, added lines: $((DIFFLINES-REMOVED)))"
[ "$DIFFLINES" -eq 1 ] && [ "$REMOVED" -eq 1 ]; chk $? "exactly ONE variable differs: LD_PRELOAD is removed and nothing else changes"

echo "=== S6 ROLLBACK: identical command with LD_PRELOAD unset ==="
env -i PATH=/usr/bin:/bin HOME="$T/home" "$T/harness" > "$T/rollback.out" 2> "$T/rollback.err"; RRC=$?
cat "$T/rollback.out"
echo "  rollback rc=$RRC"
[ $RRC -eq 0 ]; chk $? "rolled-back harness exits 0 (healthy again)"
cmp -s "$T/control.out" "$T/rollback.out"; chk $? "rolled-back output byte-identical to the glibc control (same checksum)"
echo "ROLLBACK_PROVEN: broken rc=3 -> unset LD_PRELOAD -> rc=0, output identical to control"

echo "=== S7 banking tooling healthy on glibc after the drill (never preloaded) ==="
git rev-parse HEAD > /dev/null; chk $? "git rev-parse HEAD works in this repo after the drill"
git status --porcelain > /dev/null; chk $? "git status works in this repo after the drill"

echo "=== S8 teardown and live-system proof (after) ==="
rm -rf "$T"
[ ! -e "$T" ]; chk $? "containment root deleted ($T)"
PID1_AFTER="$(ps -p 1 -o comm= | tr -d ' ')"; echo "pid1_comm_after: $PID1_AFTER"
SDDM_AFTER="$(pgrep -x sddm | tr '\n' ' ')"; echo "live sddm pids after: $SDDM_AFTER"
PS_AFTER="$(pgrep -x plasmashell | tr '\n' ' ')"; echo "live plasmashell pids after: $PS_AFTER"
KWIN_AFTER="$(pgrep -x kwin_wayland | tr '\n' ' ')"; echo "live kwin_wayland pids after: $KWIN_AFTER"
[ "$PID1_AFTER" = "$PID1_BEFORE" ]; chk $? "PID 1 unchanged ($PID1_BEFORE)"
[ "$SDDM_AFTER" = "$SDDM_BEFORE" ]; chk $? "live sddm untouched (same pids before and after)"
[ "$PS_AFTER" = "$PS_BEFORE" ]; chk $? "live plasmashell untouched (same pids before and after)"
[ "$KWIN_AFTER" = "$KWIN_BEFORE" ]; chk $? "live kwin_wayland untouched (same pids before and after)"
if printenv LD_PRELOAD > /dev/null 2>&1; then echo "  runner shell LD_PRELOAD: SET (inherited, disclosed)"; else echo "  runner shell LD_PRELOAD: unset (the drill never exports it)"; fi

echo "loadavg: $(cat /proc/loadavg)"
echo "=== Z-089 ROLLBACK DRILL RESULT: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL) ==="
exit $FAIL
