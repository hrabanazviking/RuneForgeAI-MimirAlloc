#!/usr/bin/env python3
"""Summarize a Z-034 raw run: medians per leg/class/workload,
checksum equality across legs, deltas in percent."""
import re
import statistics
import sys

raw = open(sys.argv[1]).read()
data = {}
checks = {}
leg = None
for line in raw.splitlines():
    m = re.match(r"=== CHURN class=(\d+) leg=(\w+) rep=", line)
    if m:
        leg = m.group(2)
        continue
    m = re.match(r"(churn1|churn256) class=(\d+) ns_per_op=([\d.]+) "
                 r"checksum=(\d+)", line)
    if m and leg:
        w = m.group(1); c = int(m.group(2))
        ns = float(m.group(3)); chk = int(m.group(4))
        data.setdefault((w, c, leg), []).append(ns)
        checks.setdefault((w, c), {})[leg] = chk
print(f"{'workload':9} {'class':>5} {'prod_med':>9} {'ool_med':>9} "
      f"{'delta%':>7}  checksums")
for (w, c) in sorted(checks, key=lambda t: (t[1], t[0])):
    pm = statistics.median(data[(w, c, "prod")])
    om = statistics.median(data[(w, c, "ool")])
    d = 100.0 * (om - pm) / pm
    eq = "EQUAL" if checks[(w, c)]["prod"] == checks[(w, c)]["ool"] else "DIFFER"
    print(f"{w:9} {c:>5} {pm:>9.2f} {om:>9.2f} {d:>+7.2f}  {eq} "
          f"({checks[(w, c)]['prod']})")
