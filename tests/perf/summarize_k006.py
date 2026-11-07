#!/usr/bin/env python3
"""summarize_k006.py - K-006 distribution summary.

Reads the raw CSV lines produced by probe_k006 (via
run_bench_k006.sh) and prints, per leg, workload and
thread count, the repetition count and the median / min /
max of ns_per_op, plus wall-based throughput (ops/s from
wall_ns). Lines starting with '#' (ADDR evidence) and
malformed lines are skipped. Medians are the headline
figure everywhere in docs/PHASE_11_FALSE_SHARING_SCALING.md;
a single repetition is never a verdict.
Usage: summarize_k006.py <raw-file>
"""
import statistics
import sys
from collections import defaultdict

rows = defaultdict(list)
with open(sys.argv[1], encoding="utf-8") as fh:
    for line in fh:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(",")
        if len(parts) != 9:
            continue
        leg, work, threads, ops, _total, ns_op, wall, fail, _sink = parts
        rows[(leg, work, int(threads))].append(
            (float(ns_op), int(ops) / (int(wall) / 1e9), int(fail)))

print(f"{'leg':<11} {'workload':<11} {'thr':>3} {'reps':>4} "
      f"{'med_ns_op':>10} {'min_ns_op':>10} {'max_ns_op':>10} "
      f"{'med_ops_s_wall':>15} {'fails':>5}")
for key in sorted(rows, key=lambda k: (k[1], k[0], k[2])):
    vals = rows[key]
    ns = sorted(v[0] for v in vals)
    ops_s = sorted(v[1] for v in vals)
    fails = sum(v[2] for v in vals)
    print(f"{key[0]:<11} {key[1]:<11} {key[2]:>3} {len(vals):>4} "
          f"{statistics.median(ns):>10.1f} {ns[0]:>10.1f} {ns[-1]:>10.1f} "
          f"{statistics.median(ops_s):>15.1f} {fails:>5}")
