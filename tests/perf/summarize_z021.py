#!/usr/bin/env python3
"""summarize_z021.py - Z-021 contention ladder (glibc comparator).

Reads ONE run_z021 ladder raw file (selfcheck lines + LADDER
lines, one per measured process, default 5 reps per point).
Per (workload, threads): median aggregate ops/sec across the
rep processes (Z-004 protocol section 6: repetitions are
separate processes, headline is the median), min/max spread,
scaling vs the 1-thread median, parallel efficiency, drift %
across reps (section 8, bound 8%: first-half vs second-half
medians), and the inversion point (peak rung; first later rung
falling more than the 5% noise bound below its predecessor).
Checksum discipline: chk_each must be a single value per
workload across every point and rep, and must equal the
selfcheck reference for that workload's ladder scale.
Usage: summarize_z021.py LADDER.raw
"""
import statistics
import sys

NOISE_PCT = 5.0
DRIFT_PCT = 8.0


def drift_pct(values):
    if len(values) < 4:
        return 0.0
    half = len(values) // 2
    first = statistics.median(values[:half])
    second = statistics.median(values[half:])
    return 100.0 * abs(second - first) / statistics.median(values)


def main():
    if len(sys.argv) != 2:
        print("usage: summarize_z021.py LADDER.raw")
        return 2
    cells = {}
    refs = {}
    selfcheck_ok = False
    with open(sys.argv[1]) as f:
        for line in f:
            line = line.strip()
            if line.startswith("SELFCHECK mixed_batch_replica"):
                if line.endswith("MATCH"):
                    selfcheck_ok = True
            elif line.startswith("SELFCHECK small_churn_ref"):
                refs["small_churn"] = int(
                    line.rsplit("checksum=", 1)[1])
            elif line.startswith("SELFCHECK mixed_batch_ref"):
                refs["mixed_batch"] = int(
                    line.rsplit("checksum=", 1)[1])
            elif line.startswith("LADDER,"):
                p = line.split(",")
                wl, n = p[1], int(p[2])
                agg = float(p[7])
                chk_each = int(p[9])
                total_chk = int(p[10])
                cells.setdefault((wl, n), []).append(
                    (agg, chk_each, total_chk))
    if not selfcheck_ok:
        print("SELFCHECK: replica/bench MISMATCH or missing - VOID")
        return 1
    status = 0
    workloads = []
    for (wl, _) in cells:
        if wl not in workloads:
            workloads.append(wl)
    for wl in workloads:
        print(f"=== {wl} (glibc) ===")
        print("threads  median_agg_Mops/s  min      max      "
              "scaling  efficiency  per-thread_Mops/s  drift%  checksums")
        rungs = sorted(n for (w, n) in cells if w == wl)
        med = {}
        for n in rungs:
            rows = cells[(wl, n)]
            vals = [a for (a, _, _) in rows]
            m = statistics.median(vals)
            med[n] = m
            chk_ok = (all(c == refs.get(wl) for (_, c, _) in rows)
                      and all(t == n * refs.get(wl, 0)
                              for (_, _, t) in rows))
            d = drift_pct(vals)
            print(f"{n:7d}  {m / 1e6:17.3f}  {min(vals) / 1e6:7.3f}  "
                  f"{max(vals) / 1e6:7.3f}  {m / med[rungs[0]]:7.2f}x "
                  f"{100.0 * (m / med[rungs[0]]) / n:9.1f}%  "
                  f"{m / n / 1e6:17.3f}  {d:6.2f}  "
                  f"{'OK' if chk_ok else 'BAD'}")
            if not chk_ok:
                status = 1
            if d > DRIFT_PCT:
                print(f"  DRIFT-REJECT threads={n}: drift {d:.2f}% "
                      f"> bound {DRIFT_PCT}% - median indicative only")
        peak = max(rungs, key=lambda n: med[n])
        inv = None
        for a, b in zip(rungs, rungs[1:]):
            if med[b] < med[a] * (1.0 - NOISE_PCT / 100.0):
                inv = (a, b)
                break
        print(f"INVERSION {wl}: peak aggregate throughput at "
              f"{peak} threads ({med[peak] / 1e6:.3f} Mops/s)")
        if inv:
            print(f"INVERSION {wl}: scaling inverts between "
                  f"{inv[0]} and {inv[1]} threads (first drop "
                  f"beyond the {NOISE_PCT}% noise bound)")
        else:
            print(f"INVERSION {wl}: no inversion inside the ladder "
                  f"(no post-peak drop beyond the noise bound)")
    print("SUMMARY DONE Z-021")
    return status


if __name__ == "__main__":
    sys.exit(main())
