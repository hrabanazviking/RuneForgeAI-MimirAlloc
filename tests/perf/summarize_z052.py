#!/usr/bin/env python3
"""summarize_z052.py - Z-052 growth-ladder summariser.

Reads the raw CSV lines written by run_z052.sh
(leg,growth_ladder,ops,elapsed_ns,ns_per_op,inplace,checksum)
and prints medians per leg (the Z-004 protocol headline),
the checksum proof (workload equivalence across legs), the
median in-place grow count per leg, and the graded
comparisons in the roadmap's words (WIN / TIE / LOSS, tie
band +/-5% on the median ratio):
  mimir_after vs mimir_before (the slice's before/after)
  mimir_after vs glibc        (the roadmap comparison)
  mimir_before vs glibc       (the audit's starting point)
"""
import statistics
import sys
from collections import defaultdict


def grade(ratio):
    if ratio < 0.95:
        return "WIN"
    if ratio > 1.05:
        return "LOSS"
    return "TIE"


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "-"
    data = defaultdict(list)
    inplace = defaultdict(list)
    chks = defaultdict(set)
    src = sys.stdin if path == "-" else open(path)
    for line in src:
        line = line.strip()
        if not line or "," not in line:
            continue
        parts = line.split(",")
        if len(parts) != 7 or parts[1] != "growth_ladder":
            continue
        leg = parts[0]
        data[leg].append(float(parts[4]))
        inplace[leg].append(int(parts[5]))
        chks[leg].add(int(parts[6]))
    legs = sorted(data)
    print("Z-052 growth-ladder summary (median of processes, Z-004 protocol)")
    med = {}
    for leg in legs:
        vals = data[leg]
        med[leg] = statistics.median(vals)
        print(f"  {leg:14s} n={len(vals)} median {med[leg]:12.2f} ns/op "
              f"min {min(vals):12.2f} max {max(vals):12.2f} "
              f"inplace median {statistics.median(inplace[leg]):.0f}/7 "
              f"checksums {sorted(chks[leg])}")
    allchk = set()
    for leg in legs:
        allchk |= chks[leg]
    print(f"  checksum proof: distinct across legs = {sorted(allchk)} -> "
          f"{'OK (one workload, all legs)' if len(allchk) == 1 and 0 not in allchk else 'MISMATCH'}")
    if "mimir_after" in med and "mimir_before" in med:
        r = med["mimir_after"] / med["mimir_before"]
        print(f"  after vs before (this slice): {r:.3f}x -> {grade(r)} "
              f"(saves {(1 - r) * 100:.1f}% of the before price)"
              if r < 1 else
              f"  after vs before (this slice): {r:.3f}x -> {grade(r)}")
    if "mimir_after" in med and "glibc" in med:
        r = med["mimir_after"] / med["glibc"]
        print(f"  after vs glibc (roadmap): {r:.3f}x -> {grade(r)}")
    if "mimir_before" in med and "glibc" in med:
        r = med["mimir_before"] / med["glibc"]
        print(f"  before vs glibc (audit start): {r:.3f}x -> {grade(r)}")


if __name__ == "__main__":
    main()
