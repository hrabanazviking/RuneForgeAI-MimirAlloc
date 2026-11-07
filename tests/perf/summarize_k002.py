#!/usr/bin/env python3
"""summarize_k002.py - K-002 distribution summary.

Reads the raw CSV lines produced by bench_k002 (via
run_bench_k002.sh) and prints, per leg and workload, the
repetition count and the median / min / max of
ops_per_sec and ns_per_op. Medians are the headline
figure everywhere in docs/PHASE_11_BASELINE_BENCHMARKS.md;
min/max are the spread, printed so no single wall time is
ever quoted as a verdict. Usage: summarize_k002.py <raw>
"""
import statistics
import sys
from collections import defaultdict

rows = defaultdict(list)
with open(sys.argv[1], encoding="utf-8") as fh:
    for line in fh:
        parts = line.strip().split(",")
        if len(parts) != 7:
            continue
        leg, work, ops, elapsed, ops_s, ns_op, _chk = parts
        rows[(leg, work)].append((float(ops_s), float(ns_op), int(ops)))

print(f"{'leg':<11} {'workload':<16} {'reps':>4} "
      f"{'med_ops_s':>14} {'min_ops_s':>14} {'max_ops_s':>14} "
      f"{'med_ns_op':>10} {'min_ns_op':>10} {'max_ns_op':>10}")
for (leg, work) in sorted(rows):
    vals = rows[(leg, work)]
    ops_s = sorted(v[0] for v in vals)
    ns_op = sorted(v[1] for v in vals)
    print(f"{leg:<11} {work:<16} {len(vals):>4} "
          f"{statistics.median(ops_s):>14.1f} {ops_s[0]:>14.1f} "
          f"{ops_s[-1]:>14.1f} {statistics.median(ns_op):>10.2f} "
          f"{ns_op[0]:>10.2f} {ns_op[-1]:>10.2f}")
