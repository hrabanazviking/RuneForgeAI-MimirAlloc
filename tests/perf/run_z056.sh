#!/bin/sh
# run_z056.sh - Z-056 soak harness runner.
#
# Detached-run discipline (start, poll, collect) per the
# shared-machine law: a soak is NEVER run in the
# foreground of a session. `start` builds both legs and
# launches them detached (setsid + nohup), each pinned to
# its own core (mimir: taskset -c 2, glibc: taskset -c 3),
# running CONCURRENTLY (separate processes; RSS/PSS are
# per-process smaps_rollup reads, so concurrency does not
# contaminate resident-shape samples; the latency batches
# of the two legs share the host and are recorded as
# shared-machine samples, never quiet-machine baselines).
# `poll` reports liveness and samples so far without
# blocking. `collect` refuses while a leg is still
# running; once both legs are done it verifies exit codes
# and runs the summariser over both raw files.
#
# Usage:
#   run_z056.sh start   <statedir> [seconds] [interval_s]
#   run_z056.sh poll    <statedir>
#   run_z056.sh collect <statedir>
# statedir holds: z056_mimir.raw, z056_glibc.raw,
# z056_mimir.pid, z056_glibc.pid, z056_mimir.exit,
# z056_glibc.exit, z056_start.txt (parameters + load).
# Defaults: seconds=3600 (the full Z-057-class hour),
# interval_s=60. The Z-056 gate pilot uses 600 / 10.
set -eu
CMD=${1:?usage: run_z056.sh start|poll|collect <statedir> [seconds] [interval_s]}
ST=${2:?usage: run_z056.sh start|poll|collect <statedir> [seconds] [interval_s]}
DIR=$(dirname "$0")
ROOT=$(cd "$DIR/../.." && pwd)
ZB="$ROOT/build/z56"
CFLAGS="-std=c11 -Wall -Wextra -Werror -pthread -O2"

build_legs() {
    mkdir -p "$ZB"
    gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/core/mimir_core.c" -o "$ZB/core.o"
    gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/api/mimir_alloc.c" -o "$ZB/alloc.o"
    gcc $CFLAGS -I"$ROOT/include" -I"$ROOT/src" -c "$ROOT/src/platform/mimir_platform.c" -o "$ZB/plat.o"
    gcc $CFLAGS -I"$ROOT/include" "$ROOT/tests/perf/probe_z056.c" "$ZB/alloc.o" "$ZB/core.o" "$ZB/plat.o" -o "$ZB/probe_mimir"
    gcc $CFLAGS -I"$ROOT/include" -DLEG_GLIBC "$ROOT/tests/perf/probe_z056.c" -o "$ZB/probe_glibc"
}

case "$CMD" in
start)
    SECONDS=${3:-3600}
    INTERVAL=${4:-60}
    mkdir -p "$ST"
    build_legs
    {
        echo "seconds=$SECONDS interval_s=$INTERVAL"
        echo "started_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
        echo "loadavg_at_start=$(cat /proc/loadavg)"
        echo "host=$(uname -a)"
    } > "$ST/z056_start.txt"
    setsid nohup sh -c "taskset -c 2 \"$ZB/probe_mimir\" $SECONDS $INTERVAL > \"$ST/z056_mimir.raw\" 2> \"$ST/z056_mimir.err\"; echo \$? > \"$ST/z056_mimir.exit\"" >/dev/null 2>&1 &
    echo $! > "$ST/z056_mimir.pid"
    setsid nohup sh -c "taskset -c 3 \"$ZB/probe_glibc\" $SECONDS $INTERVAL > \"$ST/z056_glibc.raw\" 2> \"$ST/z056_glibc.err\"; echo \$? > \"$ST/z056_glibc.exit\"" >/dev/null 2>&1 &
        echo $! > "$ST/z056_glibc.pid"
    echo "run_z056: started seconds=$SECONDS interval_s=$INTERVAL statedir=$ST" >&2
    ;;
poll)
    for leg in mimir glibc; do
        raw="$ST/z056_$leg.raw"
        n=0
        [ -f "$raw" ] && n=$(grep -c '^SAMPLE,' "$raw" || true)
        st=missing
        if [ -f "$ST/z056_$leg.exit" ]; then
            st="done exit=$(cat "$ST/z056_$leg.exit")"
        elif pgrep -f "probe_$leg" >/dev/null 2>&1; then
            st=running
        fi
        echo "run_z056 poll: leg=$leg state=$st samples=$n"
    done
    echo "run_z056 poll: loadavg=$(cat /proc/loadavg)"
    ;;
collect)
    for leg in mimir glibc; do
        if [ ! -f "$ST/z056_$leg.exit" ]; then
            echo "run_z056 collect: leg $leg not finished - poll again later" >&2
            exit 3
        fi
        ec=$(cat "$ST/z056_$leg.exit")
        if [ "$ec" != "0" ]; then
            echo "run_z056 collect: leg $leg FAILED exit=$ec (see $ST/z056_$leg.err)" >&2
            exit 1
        fi
    done
    echo "loadavg_at_collect=$(cat /proc/loadavg)"
    python3 "$ROOT/tests/perf/summarize_z056.py" "$ST/z056_mimir.raw" "$ST/z056_glibc.raw"
    ;;
*)
    echo "usage: run_z056.sh start|poll|collect <statedir> [seconds] [interval_s]" >&2
    exit 2
    ;;
esac
