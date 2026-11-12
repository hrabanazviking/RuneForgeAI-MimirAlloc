#!/usr/bin/env python3
"""summarize_z033.py - Z-033 BEFORE-leg summary. Fails closed:
missing/empty/malformed CSV, wrong process count (!=7 distinct
PIDs per leg), TABLECHECK not OK in every mimir process,
checksum disagreement across legs/processes, or FRAGCLASS
rows disagreeing with arithmetic recomputed from the TABLE
line all exit nonzero."""
import sys
from statistics import median

def load(path):
    rows = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line:
                rows.append(line.split(","))
    if not rows:
        sys.exit(f"FAIL: {path} empty")
    return rows

def main():
    if len(sys.argv) != 3:
        sys.exit("usage: summarize_z033.py <mimir.csv> <glibc.csv>")
    data = {"mimir": load(sys.argv[1]), "glibc": load(sys.argv[2])}
    for leg, rows in data.items():
        pids = {r[2] for r in rows if r[1] == "PID"}
        if len(pids) != 7:
            sys.exit(f"FAIL: {leg} has {len(pids)} distinct PIDs, want 7")
    # TABLE line must agree across legs and processes
    tables = {tuple(r[2:]) for rows in data.values() for r in rows if r[1] == "TABLE"}
    if len(tables) != 1:
        sys.exit("FAIL: TABLE lines disagree")
    classes = [int(x) for x in next(iter(tables))]
    # TABLECHECK: mimir OK x7, glibc SKIP x7
    for leg, want in (("mimir", "OK"), ("glibc", "SKIP")):
        got = [r[2] for r in data[leg] if r[1] == "TABLECHECK"]
        if got != [want] * 7:
            sys.exit(f"FAIL: {leg} TABLECHECK rows {got}, want 7x {want}")
    print("TABLECHECK: mimir OK in 7/7 processes; glibc SKIP (own classes)")
    # FRAGCLASS cross-check (mimir leg, 7 identical copies)
    frag = {}
    for r in data["mimir"]:
        if r[1] == "FRAGCLASS":
            frag.setdefault(int(r[2]), []).append(r)
    if len(frag) != len(classes):
        sys.exit("FAIL: FRAGCLASS class count mismatch")
    print("\nFRAGMENTATION (current table, exact arithmetic; BEFORE)")
    print("class lo hi count worst mean pct_at_lo")
    tot_count = tot_worst_mean = 0
    for i, c in enumerate(classes):
        lo = 1 if i == 0 else classes[i - 1] + 1
        worst = c - lo
        # wastes over [lo, c] are exactly 0..(c-lo), each once,
        # so the uniform mean waste is (c-lo)/2; x100 integer:
        mean_x100 = (c - lo) * 50
        pct_x100 = worst * 10000 // c
        want = [str(c), str(lo), str(c), str(c - lo + 1), str(worst),
                str(mean_x100), str(pct_x100)]
        for r in frag[c]:
            if r[2:] != want:
                sys.exit(f"FAIL: FRAGCLASS mismatch class {c}: {r[2:]} vs {want}")
        if len(frag[c]) != 7:
            sys.exit(f"FAIL: FRAGCLASS class {c} seen {len(frag[c])}x, want 7")
        tot_count += c - lo + 1
        tot_worst_mean += sum(range(0, c - lo + 1))
        print(f"{c} {lo} {c} {c - lo + 1} {worst} {mean_x100 / 100:.2f} {pct_x100 / 100:.2f}")
    print(f"uniform mean waste over 1..{classes[-1]}: {tot_worst_mean / tot_count:.4f} bytes/request")
    print("waste % denominators: pct_at_lo = worst waste as % of CLASS size;")
    print("mean waste is per-request over the class's own range [lo, class].")
    # LAT medians + checksum agreement + spread flag
    lat = {}
    for leg, rows in data.items():
        for r in rows:
            if r[1] == "LAT":
                key = (leg, r[2], int(r[3]))
                lat.setdefault(key, []).append((int(r[4]), int(r[5])))
    checks = {}
    for (leg, work, size), vals in lat.items():
        if len(vals) != 7:
            sys.exit(f"FAIL: LAT {leg} {work} {size} has {len(vals)} rows, want 7")
        cs = {v[1] for v in vals}
        if len(cs) != 1:
            sys.exit(f"FAIL: checksum disagreement {leg} {work} {size}: {cs}")
        checks.setdefault((work, size), set()).update(cs)
    for k, cs in checks.items():
        if len(cs) != 1:
            sys.exit(f"FAIL: cross-leg checksum disagreement {k}: {cs}")
    print("\nLATENCY ns/op (median of 7 processes; BEFORE; spread = (max-min)/median)")
    print("work size mimir glibc ratio mimir_spread glibc_spread flag")
    for work in ("CHURN", "LIVESET"):
        sizes = sorted({s for (l, w, s) in lat if w == work})
        for s in sizes:
            m = [v[0] for v in lat[("mimir", work, s)]]
            g = [v[0] for v in lat[("glibc", work, s)]]
            mm, gm = median(m), median(g)
            ms = (max(m) - min(m)) / mm if mm else 0
            gs = (max(g) - min(g)) / gm if gm else 0
            flag = "DRIFT-FLAG" if (ms > 0.25 or gs > 0.25) else ""
            print(f"{work} {s} {mm:.1f} {gm:.1f} {mm / gm:.2f}x {ms:.2f} {gs:.2f} {flag}")
    print("\nSUMMARY OK: all checksums agree; TABLECHECK OK; FRAGCLASS verified")

if __name__ == "__main__":
    main()
