#!/bin/bash
# run_g008_probe.sh -- G-008 responsiveness probe.
#
# Samples two concurrent probes, first with no churn
# running (BASELINE), then while the bounded synthetic
# churn of test_g008 runs (CHURN: 2 threads, 12 s, the
# G-004 thread bound is 4):
#   P1 Samtal: curl http://127.0.0.1:8787/ -> HTTP code +
#      time_total per sample (15 samples per leg).
#   P2 trivial command: /bin/true fork+exec wall time
#      per sample (15 samples per leg), via date +%s%N.
# Prints every raw sample plus min/p50/max/mean per leg
# (awk). Timing is reported as observed fact only,
# never as a guaranteed bound. Samtal is only READ
# (GET /); nothing on the Pi is configured or stopped.
set -u
cd "$(dirname "$0")/../.." || exit 1
SAMPLES=15
CHURN_SECONDS=12
CHURN_THREADS=2
TMPD=$(mktemp -d)
trap 'rm -rf "$TMPD"' EXIT

stats() {
    # $1 = file of numeric samples (one per line, ms)
    awk '{a[NR]=$1; s+=$1} END {
        n=NR; if (n==0) {print "n=0"; exit}
        for (i=1;i<=n;i++) for (j=i+1;j<=n;j++)
            if (a[j]<a[i]) {t=a[i]; a[i]=a[j]; a[j]=t}
        printf "n=%d min_ms=%.3f p50_ms=%.3f max_ms=%.3f mean_ms=%.3f\n",
            n, a[1], a[int((n+1)/2)], a[n], s/n
    }' "$1"
}

probe_leg() {
    # $1 = label
    local label="$1" i code ttc ms t0 t1
    : > "$TMPD/samtal_$label.txt"
    : > "$TMPD/true_$label.txt"
    for i in $(seq 1 "$SAMPLES"); do
        code=$(curl -s -o /dev/null -w "%{http_code} %{time_total}" \
               --max-time 5 http://127.0.0.1:8787/ 2>/dev/null)
        ttc=$(echo "$code" | awk '{print $2}')
        ms=$(awk -v v="$ttc" 'BEGIN{printf "%.3f", v*1000}')
        echo "SAMPLE leg=$label probe=samtal i=$i http=${code%% *} ms=$ms"
        echo "$ms" >> "$TMPD/samtal_$label.txt"
        t0=$(date +%s%N)
        /bin/true
        t1=$(date +%s%N)
        ms=$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.3f", (b-a)/1000000}')
        echo "SAMPLE leg=$label probe=true i=$i ms=$ms"
        echo "$ms" >> "$TMPD/true_$label.txt"
        sleep 0.2
    done
    echo "STATS leg=$label probe=samtal $(stats "$TMPD/samtal_$label.txt")"
    echo "STATS leg=$label probe=true $(stats "$TMPD/true_$label.txt")"
}

echo "PROBE samples_per_leg=$SAMPLES churn_seconds=$CHURN_SECONDS churn_threads=$CHURN_THREADS"
echo "=== LEG baseline (no churn) ==="
probe_leg baseline
echo "=== LEG churn (synthetic churn running) ==="
./build/test_g008 build/libmimiralloc.so churn "$CHURN_SECONDS" "$CHURN_THREADS" \
    > "$TMPD/churn.txt" 2>&1 &
CHURN_PID=$!
sleep 1
probe_leg churn
wait "$CHURN_PID"
CHURN_EXIT=$?
echo "--- churn output ---"
cat "$TMPD/churn.txt"
echo "churn exit: $CHURN_EXIT"
echo "=== post-churn Samtal health ==="
curl -s -o /dev/null -w "samtal_after http=%{http_code} time_total=%{time_total}\n" \
    --max-time 5 http://127.0.0.1:8787/
echo "probe exit: 0"
exit 0
