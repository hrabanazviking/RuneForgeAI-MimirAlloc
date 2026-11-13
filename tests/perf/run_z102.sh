#!/usr/bin/env bash
# run_z102.sh - Z-102 memory-pressure chaos runner
# (MimirAlloc Z13, Wave 12).
#
# Roadmap gate (Z-102): cgroup ceilings (4 GiB, 16 GiB)
# over allocation storms: behaviour at the wall,
# failure cleanliness, recovery. Gate: chaos report;
# every failure mode is a clean NULL + diag line or a
# filed defect.
#
# Every storm leg runs inside its OWN systemd user
# scope (systemd-run --user --scope, MemoryMax +
# MemorySwapMax=0) containing only the probe process
# the runner itself spawns -- the Z-060 safety form.
# No host, system, or other-campaign limit is set or
# touched; no other process is placed in a scope,
# limited, or signalled. OOM kills are contained
# inside their scopes (exit 137, or exit 143 when
# systemd tears the scope down with SIGTERM after the
# kernel OOM kill -- 143 is classified OOM_KILL only
# with journal 'oom-kill' evidence for the leg's own
# scope unit, proven on this host 2026-11-13); the
# runner shell,
# the desktop, and every co-tenant stay outside.
#
# Legs per ceiling (both ceilings unless noted):
#   dense   mimir + glibc - pure-cgroup dense-touch
#           large-chunk storm. Classified outcome:
#           OOM_KILL (exit 137) or CLEAN_NULL (probe
#           survival path PASS). Any other exit is a
#           runner FAIL (139/134/other = a defect
#           signal, never reclassified).
#   poll    mimir + glibc - approach to 92% of the
#           ceiling, free-half, re-approach, free-all,
#           health. Must PASS.
#   small   mimir + glibc, 4 GiB ceiling only -
#           pure-cgroup small-block storm, classified
#           like dense.
#   budget  mimir only - Z-079 budget at half the
#           ceiling inside the scope. Must PASS.
#   heritage mimir + glibc - Z-060 RLIMIT_AS form
#           inside the scope: clean NULL at the
#           allocator, diag lines counted for mimir.
#           Must PASS (SURVIVED_CLEAN_NULL).
# Order per ceiling: dense first (the kill, if it
# comes), then poll in a FRESH scope -- so the poll
# leg is itself the post-kill recovery evidence.
#
# The runner's stdout is the recorded run. Per-leg
# stderr lands in hidden_files/z102_<leg>_<ceil>.err
# (team bookkeeping, never banked); the mimir dense
# and heritage legs run with MIMIRALLOC_DIAG=1 and
# their diag lines are counted here.
#
# Exit 0 iff every must-pass leg passed and every
# classified leg produced a classified outcome.
set -u
cd "$(dirname "$0")/../.."
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/1000}"
export DBUS_SESSION_BUS_ADDRESS="${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/user/1000/bus}"
mkdir -p hidden_files

FAILURES=0
START_TS="$(date '+%Y-%m-%d %H:%M:%S')"

echo "run_z102_version: 1"
echo "slice: Z-102 - Memory-pressure chaos (Wave 12)"
date '+date_local: %Y-%m-%dT%H:%M:%S%z'
echo "date_utc: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
echo "host: $(uname -a)"
echo "head: $(git rev-parse HEAD 2>/dev/null)"
echo "branch: $(git branch --show-current 2>/dev/null)"
echo "loadavg_start: $(cat /proc/loadavg)"
echo "meminfo_start: $(grep -E 'MemTotal|MemAvailable|SwapTotal|SwapFree' /proc/meminfo | tr '\n' ' ')"
WITNESS_PID="$(pgrep -n plasmashell || echo none)"
echo "witness_pid_start: plasmashell=$WITNESS_PID"
echo "cotenant_top_start:"
ps -eo pid,pcpu,rss,comm --sort=-pcpu | head -n 6
echo "=== HOST TRUTH (scripts/z13/host_truth.sh) ==="
bash scripts/z13/host_truth.sh 2>/dev/null || echo "host_truth unavailable"

make -s probe-z102 2>&1
echo "probe_build_exit: $?"

run_scope() {
    # $1 ceiling, $2 leg tag, $3 diag(0/1), rest: command
    local ceil="$1" tag="$2" diag="$3"
    shift 3
    local errf="hidden_files/z102_${tag}_${ceil}.err"
    local outf="hidden_files/z102_${tag}_${ceil}.out"
    echo "--- leg $tag ceiling=$ceil ---"
    echo "loadavg_before: $(cat /proc/loadavg)"
    if [ "$diag" = "1" ]; then
        MIMIRALLOC_DIAG=1 systemd-run --user --scope \
            -p MemoryMax="$ceil" -p MemorySwapMax=0 \
            timeout 900 "$@" > "$outf" 2> "$errf"
    else
        systemd-run --user --scope \
            -p MemoryMax="$ceil" -p MemorySwapMax=0 \
            timeout 900 "$@" > "$outf" 2> "$errf"
    fi
    local rc=$?
    cat "$outf"
    echo "leg_${tag}_exit: $rc (ceiling $ceil)"
    echo "leg_${tag}_stderr_head:"
    head -n 3 "$errf"
    if grep -q "mimiralloc: error" "$errf"; then
        echo "leg_${tag}_diag_lines: $(grep -c 'mimiralloc: error' "$errf")"
        echo "leg_${tag}_diag_cause_backing: $(grep -c 'cause=backing' "$errf")"
    fi
    echo "loadavg_after: $(cat /proc/loadavg)"
    LAST_UNIT="$(grep -o 'run-[A-Za-z0-9-]*\.scope' "$errf" | head -n 1)"
    LAST_RC=$rc
    LAST_OUT="$outf"
    LAST_TAG="$tag"
}

classify_storm() {
    # $1 = pass label expected in probe output on survival
    if [ "$LAST_RC" = "137" ]; then
        echo "classification_${LAST_TAG}: OOM_KILL (exit 137, contained in scope)"
    elif [ "$LAST_RC" = "143" ] && [ -n "${LAST_UNIT:-}" ] && journalctl --user --since "$START_TS" --no-pager 2>/dev/null | grep -Fq "${LAST_UNIT}: Failed with result 'oom-kill'"; then
        echo "classification_${LAST_TAG}: OOM_KILL (exit 143, systemd scope teardown SIGTERM after in-scope kernel OOM; journal oom-kill for ${LAST_UNIT} corroborates)"
    elif [ "$LAST_RC" = "0" ] && grep -q "$1" "$LAST_OUT"; then
        echo "classification_${LAST_TAG}: CLEAN_NULL (probe survival path completed)"
    else
        echo "classification_${LAST_TAG}: UNEXPECTED (exit $LAST_RC) - RUNNER FAIL"
        FAILURES=$((FAILURES + 1))
    fi
}

require_pass() {
    # $1 = pass label
    if [ "$LAST_RC" = "0" ] && grep -q "$1" "$LAST_OUT"; then
        echo "verdict_${LAST_TAG}: PASS"
    else
        echo "verdict_${LAST_TAG}: FAIL (exit $LAST_RC, label '$1' not found)"
        FAILURES=$((FAILURES + 1))
    fi
}

for CEIL in 4G 16G; do
    if [ "$CEIL" = "4G" ]; then
        DENSE_MAX=384; POLL_CHUNK=16; BUDGET=2147483648; SMALL_OK=1
    else
        DENSE_MAX=1280; POLL_CHUNK=16; BUDGET=8589934592; SMALL_OK=0
    fi
    echo "===== CEILING $CEIL ====="
    run_scope "$CEIL" dense_mimir 1 ./build/probe_z102_mimir dense 16 "$DENSE_MAX"
    classify_storm "PROBE_Z102_DENSE: SURVIVED_CLEAN_NULL"
    run_scope "$CEIL" dense_glibc 0 ./build/probe_z102_glibc dense 16 "$DENSE_MAX"
    classify_storm "PROBE_Z102_DENSE: SURVIVED_CLEAN_NULL"
    run_scope "$CEIL" poll_mimir 0 ./build/probe_z102_mimir poll 16 92
    require_pass "PROBE_Z102_POLL: PASS"
    run_scope "$CEIL" poll_glibc 0 ./build/probe_z102_glibc poll 16 92
    require_pass "PROBE_Z102_POLL: PASS"
    if [ "$SMALL_OK" = "1" ]; then
        echo "small-leg scope note: the small storm runs under a"
        echo "512 MiB scope, a NAMED deviation from the 4 GiB ceiling:"
        echo "the small path's per-allocation span walk (the Z-091/Z-093"
        echo "F-4 loss class) makes a 4 GiB small fill a multi-hour run"
        echo "(fill rate measured in the slice doc, collapsing with the"
        echo "live set); the wall mechanics graded here are the same"
        echo "kernel cgroup behaviour the dense legs grade at 4/16 GiB."
        run_scope "512M" small_mimir 1 ./build/probe_z102_mimir small 1024 6000000
        classify_storm "PROBE_Z102_SMALL: SURVIVED_CLEAN_NULL"
        run_scope "512M" small_glibc 0 ./build/probe_z102_glibc small 1024 6000000
        classify_storm "PROBE_Z102_SMALL: SURVIVED_CLEAN_NULL"
    fi
    run_scope "$CEIL" budget_mimir 1 ./build/probe_z102_mimir budget "$BUDGET" 16 4096
    require_pass "PROBE_Z102_BUDGET: PASS"
    run_scope "$CEIL" heritage_mimir 1 ./build/probe_z102_mimir heritage 1536 16 512
    require_pass "PROBE_Z102_HERITAGE: SURVIVED_CLEAN_NULL"
    run_scope "$CEIL" heritage_glibc 0 ./build/probe_z102_glibc heritage 1536 16 512
    require_pass "PROBE_Z102_HERITAGE: SURVIVED_CLEAN_NULL"
done

echo "===== JOURNAL OOM EVIDENCE (user journal since $START_TS) ====="
journalctl --user --since "$START_TS" --no-pager 2>/dev/null | grep -E "oom-kill|OOM killer" | head -n 20 || echo "journal unavailable"
echo "===== POST-CHAOS WITNESS ====="
WITNESS_PID_AFTER="$(pgrep -n plasmashell || echo none)"
echo "witness_pid_end: plasmashell=$WITNESS_PID_AFTER (start was $WITNESS_PID)"
echo "loadavg_end: $(cat /proc/loadavg)"
echo "meminfo_end: $(grep -E 'MemTotal|MemAvailable|SwapTotal|SwapFree' /proc/meminfo | tr '\n' ' ')"
echo "runner_failures: $FAILURES"
if [ "$FAILURES" = "0" ]; then
    echo "Z-102 RESULT: PASS (all must-pass legs green; storm legs classified)"
    exit 0
fi
echo "Z-102 RESULT: FAIL ($FAILURES runner failures)"
exit 1
