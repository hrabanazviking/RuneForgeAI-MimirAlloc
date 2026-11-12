#!/usr/bin/env python3
"""summarize_z040.py - Z-040 per-structure contention delta table.

Reads one or two probe_z040 raw files (primary + corroborating),
pairs packed vs padded medians per (pattern, threads), and prints
the delta table: padded/packed ratio and percent change. A ratio
below 1.0 means the padded layout is FASTER (the purge wins);
the Z-004 noise bound is 5%: ratios within [0.95, 1.05] are TIE.
Also checks cross-run agreement on the primary file's cells when
two files are given (worst deviation printed).
"""
import re
import sys

LINE = re.compile(
    r"Z040 pattern=(\S+) layout=(\S+) threads=(\d+) "
    r"median_ns_per_op=([0-9.]+)")

def parse(path):
    cells = {}
    with open(path) as f:
        for line in f:
            m = LINE.search(line)
            if m:
                cells[(m.group(1), m.group(2), int(m.group(3)))] = float(m.group(4))
    return cells

def main():
    if len(sys.argv) < 2:
        print("usage: summarize_z040.py <raw1> [raw2]")
        return 2
    primary = parse(sys.argv[1])
    corro = parse(sys.argv[2]) if len(sys.argv) > 2 else {}
    if not primary:
        print("no Z040 lines parsed")
        return 1
    patterns = []
    for (pat, _lay, _t) in primary:
        if pat not in patterns:
            patterns.append(pat)
    print("pattern   threads  packed_ns  padded_ns   ratio  delta%   grade")
    for pat in patterns:
        for t in (2, 8, 16, 32):
            pk = primary.get((pat, "packed", t))
            pd = primary.get((pat, "padded", t))
            if pk is None or pd is None:
                continue
            ratio = pd / pk
            delta = (ratio - 1.0) * 100.0
            if pat == "l1same":
                grade = "CONTROL"
            elif ratio < 0.95:
                grade = "WIN"
            elif ratio > 1.05:
                grade = "LOSS"
            else:
                grade = "TIE"
            print(f"{pat:<9} {t:>7}  {pk:>9.3f}  {pd:>9.3f}  {ratio:>6.3f}  {delta:>+6.1f}  {grade}")
    if corro:
        worst = 0.0
        worst_key = None
        for k, v in primary.items():
            if k in corro and v > 0:
                dev = abs(corro[k] - v) / v
                if dev > worst:
                    worst = dev
                    worst_key = k
        print(f"cross-run worst median deviation: {worst * 100:.1f}% at {worst_key}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
