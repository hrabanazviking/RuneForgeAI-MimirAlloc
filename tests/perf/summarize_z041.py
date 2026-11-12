#!/usr/bin/env python3
"""summarize_z041.py - Z-041 SMT and pinning policy summariser.

Usage: summarize_z041.py <label>=<rawfile> [<label>=<rawfile> ...]
Labels name the leg (mimir / glibc). A label ending in '2' is the
second invocation of the base label (mimir2 -> mimir) and is used
only for the cross-invocation agreement table (Z-004 protocol:
5% noise bound for agreement, 8% drift bound).

Per cell (label, workload, threads, mode): n processes, median /
min / max aggregate ops/sec, spread % = (max-min)/median, median
migration count (floating mode). Checksum discipline: chk_each
must equal the K-002 ladder reference for the workload at the
ladder scale (small_churn 50000 iters -> 6371544; mixed_batch 200
batches -> 6627200; both identical in the banked Z-020 and Z-021
recorded runs) in every rep, or the cell is flagged BADCHK.
"""
import statistics
import sys
from collections import defaultdict

EXPECTED_CHK = {"small_churn": 6371544, "mixed_batch": 6627200}


def load(path):
    cells = defaultdict(list)
    badchk = defaultdict(int)
    with open(path) as f:
        for line in f:
            if not line.startswith("LADDER,"):
                continue
            parts = line.strip().split(",")
            # LADDER,workload,threads,mode,iters,ops_per_thread,
            # total_ops,elapsed_ns,agg,per_thread,chk_each,checksum,mig
            wl, threads, mode = parts[1], int(parts[2]), parts[3]
            agg, chk, mig = float(parts[8]), int(parts[10]), int(parts[12])
            key = (wl, threads, mode)
            cells[key].append(agg)
            if EXPECTED_CHK.get(wl) is not None and chk != EXPECTED_CHK[wl]:
                badchk[key] += 1
            cells[key + ("mig",)].append(mig)
    return cells, badchk


def med(vals):
    return statistics.median(vals)


def main():
    data = {}
    bad = {}
    for arg in sys.argv[1:]:
        label, path = arg.split("=", 1)
        data[label], bad[label] = load(path)
    print("=== Z-041 cells: median agg ops/sec (n, min, max, spread%, median mig) ===")
    for label in sorted(data):
        cells = data[label]
        keys = sorted({k[:3] for k in cells if len(k) == 3})
        for (wl, threads, mode) in keys:
            vals = cells[(wl, threads, mode)]
            migs = cells[(wl, threads, mode, "mig")]
            spread = (max(vals) - min(vals)) / med(vals) * 100.0
            flag = " BADCHK=%d" % bad[label][(wl, threads, mode)] if bad[label][(wl, threads, mode)] else ""
            print("%s %s t=%d %s: med=%.0f n=%d min=%.0f max=%.0f spread=%.1f%% mig_med=%d%s" % (
                label, wl, threads, mode, med(vals), len(vals), min(vals), max(vals), spread, int(med(migs)), flag))
    print()
    print("=== Z-041 comparisons (ratios of medians, invocation 1 labels only) ===")
    for label in sorted(data):
        if label.endswith("2"):
            continue
        cells = data[label]
        def m(wl, t, mode):
            v = cells.get((wl, t, mode))
            return med(v) if v else None
        for wl in ("small_churn", "mixed_batch"):
            rows = []
            a, b = m(wl, 16, "pinned"), m(wl, 32, "pinned")
            if a and b:
                rows.append("SMT pinned 32/16=%.3f" % (b / a))
            a, b = m(wl, 16, "floating"), m(wl, 32, "floating")
            if a and b:
                rows.append("SMT floating 32/16=%.3f" % (b / a))
            for t in (8, 16, 32):
                a, b = m(wl, t, "floating"), m(wl, t, "pinned")
                if a and b:
                    rows.append("pin/float t=%d=%.3f" % (t, b / a))
            a, b = m(wl, 8, "pinned"), m(wl, 16, "pinned")
            if a and b:
                rows.append("CCD pinned 16/8=%.3f" % (b / a))
            if rows:
                print("%s %s: %s" % (label, wl, "; ".join(rows)))
    print()
    print("=== Z-041 cross-invocation agreement (5% noise / 8% drift bounds) ===")
    any2 = False
    for label in sorted(data):
        if not label.endswith("2"):
            continue
        any2 = True
        base = label[:-1]
        if base not in data:
            print("%s: no base label %s" % (label, base))
            continue
        keys = sorted({k[:3] for k in data[base] if len(k) == 3})
        worst = 0.0
        for (wl, threads, mode) in keys:
            v1 = data[base].get((wl, threads, mode))
            v2 = data[label].get((wl, threads, mode))
            if not v1 or not v2:
                print("%s %s t=%d %s: MISSING in one invocation" % (base, wl, threads, mode))
                continue
            d = abs(med(v2) - med(v1)) / med(v1) * 100.0
            worst = max(worst, d)
            tier = "AGREE" if d <= 5.0 else ("DRIFT" if d <= 8.0 else "DISAGREE")
            print("%s %s t=%d %s: inv1=%.0f inv2=%.0f delta=%.2f%% %s" % (base, wl, threads, mode, med(v1), med(v2), d, tier))
        print("%s worst delta: %.2f%%" % (base, worst))
    if not any2:
        print("(no second-invocation labels given)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
