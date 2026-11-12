#!/usr/bin/env python3
"""summarize_z014.py - Z-014 fault-cost curve summariser.
Reads the probe CSV raw file, groups by (size, mode),
reports median ns/page (headline, protocol section 6),
min/max spread, first/second-half drift (section 8),
median minor faults per page, and the recycled-vs-fresh
ratios per size plus the headline ratio across sizes.
Usage: summarize_z014.py <raw-file>"""
import statistics
import sys
from collections import defaultdict

def main():
    rows = defaultdict(list)
    cks = defaultdict(set)
    with open(sys.argv[1]) as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            f = line.split(",")
            size, mode = int(f[0]), f[1]
            rows[(size, mode)].append({
                "pages": int(f[2]), "touch_ns": int(f[3]),
                "nspp": float(f[4]), "map_ns": int(f[5]),
                "aux_ns": int(f[6]), "unmap_ns": int(f[7]),
                "minflt": int(f[8])})
            cks[size].add(int(f[9]))
    bad_ck = [s for s, v in cks.items() if len(v) != 1]
    print(f"checksum status: {'OK - one checksum per size across all modes/processes' if not bad_ck else 'FAIL sizes ' + str(bad_ck)}")
    sizes = sorted({s for s, _ in rows})
    med = {}
    print(f"{'size':>11} {'mode':>11} {'n':>2} {'med ns/page':>11} {'med ns total':>13} {'min':>8} {'max':>8} {'spread%':>8} {'drift%':>7} {'flt/page':>8} {'med map ns':>11} {'med unmap ns':>13}")
    for s in sizes:
        for m in ("fresh", "recyc_map", "recyc_keep"):
            rs = rows.get((s, m), [])
            if not rs:
                print(f"{s:>11} {m:>11} MISSING")
                continue
            vals = [r["nspp"] for r in rs]
            md = statistics.median(vals)
            med[(s, m)] = md
            half = len(vals) // 2
            d = 0.0
            if half >= 2:
                a = statistics.median(vals[:half])
                b = statistics.median(vals[half:])
                d = 100.0 * abs(b - a) / md if md else 0.0
            spread = 100.0 * (max(vals) - min(vals)) / md if md else 0.0
            flt = statistics.median([r["minflt"] / r["pages"] for r in rs])
            print(f"{s:>11} {m:>11} {len(rs):>2} {md:>11.2f} {statistics.median([r['touch_ns'] for r in rs]):>13.0f} {min(vals):>8.2f} {max(vals):>8.2f} {spread:>8.1f} {d:>7.1f} {flt:>8.3f} {statistics.median([r['map_ns'] for r in rs]):>11.0f} {statistics.median([r['unmap_ns'] for r in rs]):>13.0f}")
    print()
    print("recycled-vs-fresh ratio (median ns/page, recycled / fresh; <1 = recycled cheaper):")
    print(f"{'size':>11} {'recyc_map/fresh':>16} {'recyc_keep/fresh':>17}")
    ratios_m, ratios_k = [], []
    for s in sizes:
        if (s, "fresh") in med:
            rm = med[(s, "recyc_map")] / med[(s, "fresh")]
            rk = med[(s, "recyc_keep")] / med[(s, "fresh")]
            ratios_m.append(rm); ratios_k.append(rk)
            print(f"{s:>11} {rm:>16.3f} {rk:>17.3f}")
    if ratios_m:
        print(f"headline (median across sizes): recyc_map {statistics.median(ratios_m):.3f}, recyc_keep {statistics.median(ratios_k):.3f}")
        big = [(s, med[(s, 'recyc_map')] / med[(s, 'fresh')], med[(s, 'recyc_keep')] / med[(s, 'fresh')]) for s in sizes if s >= 16777216]
        if big:
            print(f"large sizes >=16 MiB (median): recyc_map {statistics.median([b[1] for b in big]):.3f}, recyc_keep {statistics.median([b[2] for b in big]):.3f}")
    return 1 if bad_ck else 0

if __name__ == "__main__":
    sys.exit(main())
