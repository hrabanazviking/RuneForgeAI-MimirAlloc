#!/usr/bin/env python3
"""summarize_z062.py -- Z-062 reuse-distance summariser.

Reads the raw CSV from run_z062.sh (one line per process:
leg,policy,class,footprint,blocks,rounds,elapsed_ns,
ns_per_block,checksum) and prints, per Z-004:
  - per (leg,class,footprint): n, median/min/max ns_per_block
  - checksum equivalence: exactly one checksum per
    (class,footprint) across all legs, else CHECKSUM-FAIL
  - identical-leg agreement: mimir_lifo vs mimir_lifo_dup
    median difference %, bound 5%, else DISAGREE
  - split-half drift per (leg,class,footprint): >8% flags
    DRIFT-REJECT
  - policy ratios per (class,footprint): fifo vs lifo (the
    emulated policy pair), native vs lifo (emulation control),
    mimir native vs glibc native
Grades use the 5% noise bound: ratio in [0.95,1.05] = TIE.
Usage: summarize_z062.py <raw-file>
"""
import sys
from statistics import median

def main():
    rows = []
    with open(sys.argv[1]) as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            f = line.split(",")
            rows.append({
                "legalloc": f[0], "policy": f[1],
                "leg": f[0] + "_" + f[1] if f[1] != "lifo"
                       else f[0] + "_lifo",
                "cls": int(f[2]), "fp": int(f[3]),
                "blocks": int(f[4]), "rounds": int(f[5]),
                "ns": float(f[7]), "ck": int(f[8]),
                "rawleg": f[0], "rawpol": f[1],
            })
    # The runner emits mimir_lifo twice under the same CSV leg
    # name; the raw file order distinguishes them: within each
    # rep round the leg order is fixed, so identical CSV rows
    # pairs are split by occurrence parity per cell below.
    cells = {}
    checksums = {}
    for r in rows:
        key = (r["rawleg"], r["rawpol"], r["cls"], r["fp"])
        cells.setdefault(key, []).append(r["ns"])
        checksums.setdefault((r["cls"], r["fp"]), set()).add(r["ck"])
    print("Z-062 reuse-distance summary (median of processes, "
          "Z-004 protocol)")
    keys = sorted(cells, key=lambda k: (k[2], k[3], k[0], k[1]))
    med = {}
    for k in keys:
        vals = cells[k]
        m = median(vals)
        med[k] = m
        print(f"  {k[0]:6s} {k[1]:6s} class={k[2]:5d} "
              f"fp={k[3]:9d} n={len(vals)} median {m:9.3f} "
              f"min {min(vals):9.3f} max {max(vals):9.3f} ns/block")
    bad = {k: v for k, v in checksums.items() if len(v) != 1}
    if bad:
        print("checksum proof: FAIL", bad)
    else:
        print("checksum proof: OK (one checksum per cell across "
              "all legs)")
    print("drift (split-half, bound 8%):")
    any_drift = False
    for k in keys:
        vals = cells[k]
        if len(vals) >= 4:
            h = len(vals) // 2
            d = abs(median(vals[h:]) - median(vals[:h])) / med[k] * 100
            if d > 8:
                print(f"  DRIFT-REJECT {k}: {d:.1f}%")
                any_drift = True
    if not any_drift:
        print("  none above 8%")
    def ratio(a, b):
        if a in med and b in med and med[b] > 0:
            return med[a] / med[b]
        return None
    def grade(r):
        if r is None:
            return "n/a"
        if r < 0.95:
            return "WIN (first-named faster)"
        if r > 1.05:
            return "LOSS (first-named slower)"
        return "TIE"
    print("policy ratios (median ns/block):")
    cell_keys = sorted({(k[2], k[3]) for k in keys})
    for (c, fpt) in cell_keys:
        fifo = ratio(("mimir", "fifo", c, fpt),
                     ("mimir", "lifo", c, fpt))
        # NOTE: mimir lifo cell holds BOTH lifo and lifo_dup
        # processes (same CSV name); its median is the pooled
        # lifo median and the agreement check is the split of
        # that cell's values by occurrence parity, printed by
        # the parity section below.
        nat = ratio(("mimir", "native", c, fpt),
                    ("mimir", "lifo", c, fpt))
        mg = ratio(("mimir", "native", c, fpt),
                   ("glibc", "native", c, fpt))
        def fmt(r):
            return "n/a" if r is None else f"{r:.3f} {grade(r)}"
        print(f"  class={c:5d} fp={fpt:9d} "
              f"fifo/lifo {fmt(fifo)} | native/lifo {fmt(nat)} "
              f"| mimir-native/glibc-native {fmt(mg)}")
    # identical-leg agreement via occurrence parity inside the
    # pooled mimir lifo cells: the runner interleaves
    # lifo,lifo_dup adjacently, so even/odd occurrences are
    # the two identical legs.
    print("identical-leg agreement (mimir lifo vs lifo_dup, "
          "bound 5%):")
    any_dis = False
    for k in keys:
        if k[0] == "mimir" and k[1] == "lifo":
            vals = cells[k]
            a = median(vals[0::2])
            b = median(vals[1::2])
            d = abs(a - b) / ((a + b) / 2) * 100
            flag = "" if d <= 5 else " DISAGREE"
            if d > 5:
                any_dis = True
            print(f"  class={k[2]:5d} fp={k[3]:9d} "
                  f"{a:.3f} vs {b:.3f} diff {d:.1f}%{flag}")
    if not any_dis:
        print("  all within 5%")

if __name__ == "__main__":
    main()
