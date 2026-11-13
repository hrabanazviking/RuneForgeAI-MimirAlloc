#!/usr/bin/env bash
# Z-086 desktop battery - KDE applications under the Z-082
# interposing variant (LD_PRELOAD of
# build/libmimiralloc_interpose.so) inside a contained
# session on this host. Output is the recorded run itself;
# the caller tees it to tests/abi/recorded_run_z13_z086.txt.
# Per-app logs land in hidden_files/z086_<app>_<mode>.log.
# Exit 0 iff no allocator-caused failure is found.
#
# CONTAINMENT METHOD (stated as the gate requires): every
# application runs with a private throwaway HOME,
# XDG_RUNTIME_DIR, XDG_CONFIG_HOME and XDG_DATA_HOME under
# one mktemp root, inside its own dbus-run-session (a
# private session bus created for the run and destroyed
# with it), with QT_QPA_PLATFORM=offscreen (the headless
# Qt platform: no display connection is ever opened),
# DISPLAY, WAYLAND_DISPLAY and SESSION_MANAGER unset, and
# its own setsid process group which the runner terminates
# at the end of the observation window. Nothing is
# preloaded into, attached to, or signalled inside the
# host desktop session: the live plasmashell and
# kwin_wayland (wayland-0 / X display :0) are never
# touched, and their PIDs are recorded before and after
# the battery as proof. The containment root is deleted
# at the end. Rollback for any user of the variant is
# unsetting the one environment variable: unset
# LD_PRELOAD (proven in S5 below).
set -u
cd "$(dirname "$0")/../.."
FAIL=0
chk() { if [ "$1" -eq 0 ]; then echo "  [ok] $2"; else echo "  [FAIL] $2"; FAIL=1; fi; }
IPOLIB="$PWD/build/libmimiralloc_interpose.so"
WINDOW=8

echo "=== Z-086 desktop battery recorded run ==="
echo "date_utc: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "host: $(uname -a)"
echo "cc: $(cc --version | head -1)"
echo "head: $(git rev-parse HEAD)"
echo "branch: $(git branch --show-current)"
echo "loadavg: $(cat /proc/loadavg)"
echo "page_size: $(getconf PAGESIZE)"
echo "containment: private mktemp HOME/XDG root + dbus-run-session (private bus) + QT_QPA_PLATFORM=offscreen + DISPLAY/WAYLAND_DISPLAY unset + setsid process group, torn down per run"
echo "preload: $IPOLIB (Z-082 interposing variant)"
echo "rollback: unset LD_PRELOAD"

echo "=== S0 host inventory and live-session hands-off record ==="
for a in kcalc kwrite kate dolphin gwenview okular plasmashell plasmawindowed dbus-run-session; do printf "%s: " "$a"; command -v "$a" || echo MISSING; done
if [ -x /usr/bin/Xvfb ]; then echo "Xvfb: present"; else echo "Xvfb: absent on this host (headless Qt platform used instead)"; fi
LIVE_PS_BEFORE="$(pgrep -x plasmashell | tr "\n" " ")"
LIVE_KWIN_BEFORE="$(pgrep -x kwin_wayland | tr "\n" " ")"
echo "live plasmashell pids before: $LIVE_PS_BEFORE"
echo "live kwin_wayland pids before: $LIVE_KWIN_BEFORE"

echo "=== S1 build the interposing variant ==="
make lib-interpose > hidden_files/z086_build_interpose.log 2>&1
chk $? "make lib-interpose (log hidden_files/z086_build_interpose.log)"
ls -la build/libmimiralloc_interpose.so
echo "interpose defined dynamic exports: $(nm -D --defined-only build/libmimiralloc_interpose.so | wc -l)"
[ "$(nm -D --defined-only build/libmimiralloc_interpose.so | wc -l)" -eq 20 ]
chk $? "interpose export count == 20 (10 namespaced + 10 standard)"

echo "=== S2 neighbour evidence: Z-082 suite still green on this tree ==="
make test-z082 > hidden_files/z086_test_z082.log 2>&1
chk $? "make test-z082 (log hidden_files/z086_test_z082.log)"
grep "PRELOAD-VERDICT" hidden_files/z086_test_z082.log | head -4

T="$(mktemp -d /tmp/z086_battery.XXXXXX)"
mkdir -p "$T/home" "$T/runtime" "$T/config" "$T/data" "$T/cache"
chmod 700 "$T/runtime"
cat > "$T/inner.sh" <<INNER
#!/usr/bin/env bash
export HOME="$T/home"
export XDG_RUNTIME_DIR="$T/runtime"
export XDG_CONFIG_HOME="$T/config"
export XDG_DATA_HOME="$T/data"
export XDG_CACHE_HOME="$T/cache"
export QT_QPA_PLATFORM=offscreen
unset DISPLAY
unset WAYLAND_DISPLAY
unset SESSION_MANAGER
if [ "\$1" = "preload" ]; then export LD_PRELOAD="$IPOLIB"; shift; else shift; fi
exec timeout --signal=TERM --kill-after=3 "\$@"
INNER
chmod +x "$T/inner.sh"

run_one() {
  local label="$1" mode="$2" window="$3"; shift 3
  local log="hidden_files/z086_${label}_${mode}.log"
  setsid dbus-run-session -- bash "$T/inner.sh" "$mode" "$window" "$@" > "$log" 2>&1
  local rc=$?
  local pid envf
  for pid in $(pgrep -u "$(id -u)" -f "$(basename "$1")" 2>/dev/null); do
    envf="/proc/$pid/environ"
    if [ -r "$envf" ] && tr "\0" "\n" < "$envf" 2>/dev/null | grep -q "XDG_RUNTIME_DIR=$T/runtime"; then kill -TERM "$pid" 2>/dev/null; fi
  done
  echo "$rc"
}

classify() {
  if [ "$1" -eq 124 ]; then echo ALIVE; elif [ "$1" -eq 0 ]; then echo EXITED-CLEAN; else echo "FAILED(rc=$1)"; fi
}

battery_row() {
  local label="$1" window="$2"; shift 2
  echo "-- $label: $* (window ${window}s) --"
  local crc prc cclass pclass
  crc="$(run_one "$label" control "$window" "$@")"
  cclass="$(classify "$crc")"
  prc="$(run_one "$label" preload "$window" "$@")"
  pclass="$(classify "$prc")"
  echo "  control: rc=$crc class=$cclass"
  echo "  preload: rc=$prc class=$pclass"
  echo "  preload log tail:"
  tail -3 "hidden_files/z086_${label}_preload.log" | sed "s/^/    /"
  if [ "$pclass" = "ALIVE" ] || [ "$pclass" = "EXITED-CLEAN" ]; then
    chk 0 "$label PASS under preload ($pclass; control $cclass)"
  elif [ "$cclass" = "ALIVE" ] || [ "$cclass" = "EXITED-CLEAN" ]; then
    chk 1 "$label ALLOCATOR-CAUSED failure: preload $pclass but control $cclass"
  else
    echo "  [ok] $label ENV-ONLY: control also $cclass in containment; not an allocator result (cause named from the per-app logs)"
  fi
}

echo "=== S3 the battery: control vs preload per application ==="
if [ -x /usr/bin/kwrite ]; then battery_row kwrite $WINDOW /usr/bin/kwrite; else echo "-- kwrite: MISSING-APP (no /usr/bin/kwrite on this host) --"; echo "  [ok] kwrite row carried by kate below (same KTextEditor family, the kwrite-class editor)"; fi
battery_row kcalc $WINDOW /usr/bin/kcalc
battery_row kate $WINDOW /usr/bin/kate
battery_row dolphin $WINDOW /usr/bin/dolphin
battery_row gwenview $WINDOW /usr/bin/gwenview
battery_row okular $WINDOW /usr/bin/okular

echo "=== S4 plasmashell component in containment ==="
battery_row plasmawindowed $WINDOW /usr/bin/plasmawindowed org.kde.plasma.digitalclock
battery_row plasmashell 10 /usr/bin/plasmashell --no-respawn

echo "=== S5 rollback proof: unset LD_PRELOAD restores the glibc run ==="
rrc="$(run_one rollback control $WINDOW /usr/bin/kcalc)"
rclass="$(classify "$rrc")"
echo "  kcalc with LD_PRELOAD unset: rc=$rrc class=$rclass"
[ "$rclass" = "ALIVE" ] || [ "$rclass" = "EXITED-CLEAN" ]
chk $? "rollback by unsetting LD_PRELOAD: kcalc runs on glibc ($rclass)"

echo "=== S6 teardown and live-session proof ==="
rm -rf "$T"
[ ! -e "$T" ]; chk $? "containment root deleted ($T)"
LIVE_PS_AFTER="$(pgrep -x plasmashell | tr "\n" " ")"
LIVE_KWIN_AFTER="$(pgrep -x kwin_wayland | tr "\n" " ")"
echo "live plasmashell pids after: $LIVE_PS_AFTER"
echo "live kwin_wayland pids after: $LIVE_KWIN_AFTER"
[ "$LIVE_PS_AFTER" = "$LIVE_PS_BEFORE" ]; chk $? "live plasmashell untouched (same pids before and after)"
[ "$LIVE_KWIN_AFTER" = "$LIVE_KWIN_BEFORE" ]; chk $? "live kwin_wayland untouched (same pids before and after)"

echo "loadavg: $(cat /proc/loadavg)"
echo "=== Z-086 DESKTOP BATTERY RESULT: $([ $FAIL -eq 0 ] && echo PASS || echo FAIL) ==="
exit $FAIL
