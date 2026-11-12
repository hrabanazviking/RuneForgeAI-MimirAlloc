#!/usr/bin/env python3
"""summarize_z048.py - Z-048 large_batch summariser.

Reads the raw CSV lines written by run_z048.sh
(leg,large_batch,ops,elapsed_ns,ops_per_sec,ns_per_op,checksum)
and prints medians per leg (Z-004 protocol headline), the
checksum proof, and the graded comparisons:
  mimir_pool vs mimir_base (same-window A/B, if present)
  mimir_pool vs glibc      (this window)
  mimir_pool vs Z-009      (the banked Wave 1 baseline of
                            record, docs/Z13_WAVE01_BASELINE.md:
                            mimir 3064.69 ns/op, glibc 1295.68)
Grades use the roadmap's words: WIN / TIE / LOSS, with the
tie band at +/-5% on the median ratio.
"""
import statistics
import sys
from collections import defaultdict

Z009_MIMIR = 3064.69
Z009_GLIBC = 1295.68
EXPECT_CHK = 1106008  # K-002 large_batch checksum, 300 batches


def grade(ratio):
    if ratio < 0.95:
        return "WIN"
    if ratio > 1.05:
        return "LOSS"
    return "TIE"


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "-"
    data = defaultdict(list)
    chks = defaultdict(set)
    src = sys.stdin if path == "-" else open(path)
    for line in src:
        line = line.strip()
        if not line or "," not in line:
            continue
        parts = line.split(",")
        if len(parts) != 7 or parts[1] != "large_batch":
            continue
        leg = parts[0]
        data[leg].append(float(parts[5]))
        chks[leg].add(int(parts[6]))
    legs = sorted(data)
    print("Z-048 large_batch summary (median of processes, Z-004 protocol)")
    med = {}
    for leg in legs:
        vals = data[leg]
        med[leg] = statistics.median(vals)
        print(f"  {leg:12s} n={len(vals)} median {med[leg]:10.2f} ns/op "
              f"min {min(vals):10.2f} max {max(vals):10.2f} "
              f"checksums {sorted(chks[leg])}")
    allchk = set()
    for leg in legs:
        allchk |= chks[leg]
    print(f"  checksum proof: distinct across legs = {sorted(allchk)} "
          f"(K-002 of record: {EXPECT_CHK}) -> "
          f"{'OK' if allchk == {EXPECT_CHK} else 'MISMATCH'}")
    if "mimir_pool" in med and "mimir_base" in med:
        r = med["mimir_pool"] / med["mimir_base"]
        print(f"  pool vs base (same window): {r:.3f}x -> "
              f"{'WIN' if r < 0.95 else 'TIE' if r <= 1.05 else 'LOSS'} "
              f"for the pool (saves {(1 - r) * 100:.1f}% of the base price)"
              if r < 1 else
              f"  pool vs base (same window): {r:.3f}x")
    if "mimir_pool" in med and "glibc" in med:
        r = med["mimir_pool"] / med["glibc"]
        print(f"  pool vs glibc (this window): {r:.2f}x -> {grade(r)} "
              f"(roadmap grade vs glibc)")
    if "mimir_pool" in med:
        r = med["mimir_pool"] / Z009_MIMIR
        print(f"  pool vs Z-009 mimir ({Z009_MIMIR} ns/op): {r:.3f}x -> "
              f"{'WIN' if r < 0.95 else 'TIE' if r <= 1.05 else 'LOSS'} "
              f"against the banked baseline")
    if "glibc" in med:
        r = med["glibc"] / Z009_GLIBC
        print(f"  glibc cross-check vs Z-009 glibc ({Z009_GLIBC} ns/op): "
              f"{r:.3f}x (window comparability gauge, not a grade)")


if __name__ == "__main__":
    main()
