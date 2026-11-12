#!/usr/bin/env python3
"""summarize_z054.py - Z-054 Wave 6 standings summariser.

Reads the raw CSV written by run_z054.sh (bench_k002 lines:
leg,workload,ops,elapsed_ns,ops_per_sec,ns_per_op,checksum)
and prints, per workload:
  - per-leg medians (Z-004 protocol headline), min/max and
    split-half drift (protocol s.8, bound 8%; above =
    DRIFT-REJECT, indicative only)
  - the checksum proof (protocol s.9): exactly one
    distinct checksum per workload across both legs, equal
    to the K-002 checksum of record
  - graded comparisons in the roadmap's words (WIN / TIE /
    LOSS, tie band +/-5% on the median ratio, the
    Z-048/Z-052 house band):
      mimir vs glibc, same window
      mimir vs Z-009 mimir of record (cross-window; the
        glibc window gauge is printed beside it)
Z-009 of record (docs/Z13_WAVE01_BASELINE.md s.2, Z-003
medians): small_churn glibc 9.02 / mimir 93.43;
mixed_batch 9.90 / 398.42; large_batch 1295.68 / 3064.69;
mixed_lifecycle 13.38 / 898.76 ns/op.
"""
import statistics
import sys
from collections import defaultdict

WORKLOADS = ["small_churn", "mixed_batch", "large_batch",
             "mixed_lifecycle"]
Z009 = {
    "small_churn": (9.02, 93.43),
    "mixed_batch": (9.90, 398.42),
    "large_batch": (1295.68, 3064.69),
    "mixed_lifecycle": (13.38, 898.76),
}
EXPECT_CHK = {
    "small_churn": 12756656,
    "mixed_batch": 34727552,
    "large_batch": 1106008,
    "mixed_lifecycle": 40359936,
}


def grade(ratio):
    if ratio < 0.95:
        return "WIN"
    if ratio > 1.05:
        return "LOSS"
    return "TIE"


def drift_pct(vals):
    if len(vals) < 4:
        return 0.0
    half = len(vals) // 2
    a, b = vals[:half], vals[half:]
    med = statistics.median(vals)
    if med == 0:
        return 0.0
    return 100.0 * abs(statistics.median(b)
                       - statistics.median(a)) / med


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
        if len(parts) != 7:
            continue
        leg, work = parts[0], parts[1]
        data[(leg, work)].append(float(parts[5]))
        chks[work].add(int(parts[6]))
    print("Z-054 standings summary (ns/op; headline = median "
          "of processes, Z-004 protocol)")
    print("drift bound 8% (Z-004 s.8); DRIFT-REJECT = "
          "indicative only; tie band +/-5%")
    print()
    print("=== per-leg medians ===")
    med = {}
    for work in WORKLOADS:
        for leg in ("glibc", "mimiralloc"):
            vals = data.get((leg, work), [])
            if not vals:
                print(f"  {leg:11s} {work:16s} NO DATA")
                continue
            med[(leg, work)] = statistics.median(vals)
            d = drift_pct(vals)
            flag = "  DRIFT-REJECT" if d > 8.0 else ""
            print(f"  {leg:11s} {work:16s} n={len(vals)} "
                  f"median {med[(leg, work)]:10.2f} "
                  f"min {min(vals):10.2f} "
                  f"max {max(vals):10.2f} "
                  f"drift {d:5.2f}%{flag}")
    print()
    print("=== checksum proof (Z-004 s.9) ===")
    ok = True
    for work in WORKLOADS:
        got = sorted(chks.get(work, set()))
        want = EXPECT_CHK[work]
        good = got == [want]
        ok = ok and good
        print(f"  {work:16s} distinct={got} "
              f"expected=[{want}] "
              f"{'OK' if good else 'FAIL'}")
    print(f"  checksum proof: {'OK' if ok else 'FAIL'} - "
          "one checksum per workload across both legs")
    print()
    print("=== grades ===")
    for work in WORKLOADS:
        g = med.get(("glibc", work))
        m = med.get(("mimiralloc", work))
        if g is None or m is None:
            continue
        r = m / g
        zg, zm = Z009[work]
        print(f"  {work:16s} mimir/glibc {r:7.3f}x "
              f"-> {grade(r)} (same window)")
        print(f"  {work:16s} mimir now {m:10.2f} vs "
              f"Z-009 mimir {zm:9.2f}: ratio {m / zm:7.3f}x "
              f"-> {grade(m / zm)} (cross-window)")
        print(f"  {work:16s} glibc gauge: now {g:9.2f} vs "
              f"Z-009 glibc {zg:8.2f}: ratio {g / zg:7.3f}x "
              "(window comparability only)")


if __name__ == "__main__":
    main()
