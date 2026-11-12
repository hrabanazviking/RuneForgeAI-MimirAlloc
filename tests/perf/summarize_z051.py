#!/usr/bin/env python3
"""summarize_z051.py - Z-051 huge-size table builder.
Reads the cost CSV from run_z051.sh, prints per
(leg,size,touch) medians of processes (Z-004 protocol:
headline = median, min/max beside it, drift check by
split halves at the 8% bound), checksum unity per
(size,touch=1), and mimir-vs-glibc grades on the total
(alloc+touch+free) median at the 5% noise bound:
inside 5% TIE, mimir lower WIN, mimir higher LOSS.
Usage: summarize_z051.py <cost.csv>"""
import statistics
import sys
from collections import defaultdict

rows = []
with open(sys.argv[1]) as f:
    for line in f:
        parts = line.strip().split(",")
        if len(parts) != 15:
            continue
        rows.append(parts)

groups = defaultdict(list)
checksums = defaultdict(set)
for r in rows:
    leg, size, touch = r[0], int(r[1]), int(r[2])
    groups[(leg, size, touch)].append(r)
    if touch == 1 and r[14] == "OK":
        checksums[(size, touch)].add(int(r[13]))

def med(vals):
    return statistics.median(vals)

def drift(vals):
    if len(vals) < 4:
        return 0.0
    h = len(vals) // 2
    a = statistics.median(vals[:h])
    b = statistics.median(vals[h:])
    m = statistics.median(vals)
    return 100.0 * abs(b - a) / m if m else 0.0

print("size_mib touch leg        n  alloc_med_ns  touch_med_ns   free_med_ns  total_med_ns  total_min     total_max     drift_pct  minflt_med  rss_alloc_med  rss_touch_med  mapped_delta_med")
summary = {}
for (leg, size, touch) in sorted(groups, key=lambda k: (k[1], k[2], k[0])):
    rs = groups[(leg, size, touch)]
    alloc = [int(r[3]) for r in rs]
    tch = [int(r[4]) for r in rs]
    free = [int(r[5]) for r in rs]
    total = [a + t + f for a, t, f in zip(alloc, tch, free)]
    minflt = [int(r[6]) for r in rs]
    rssa = [int(r[8]) for r in rs]
    rsst = [int(r[9]) for r in rs]
    mapd = [int(r[12]) for r in rs]
    summary[(leg, size, touch)] = med(total)
    print(f"{size//1048576:8d} {touch:5d} {leg:10s} {len(rs):2d}  {med(alloc):13.0f}  {med(tch):13.0f}  {med(free):12.0f}  {med(total):13.0f}  {min(total):12d}  {max(total):12d}  {drift(total):9.2f}  {med(minflt):10.0f}  {med(rssa):14d}  {med(rsst):14d}  {med(mapd):16.0f}")

print()
bad = 0
for (size, touch), s in sorted(checksums.items()):
    status = "OK single checksum" if len(s) == 1 else f"FAIL {len(s)} distinct checksums"
    if len(s) != 1:
        bad = 1
    print(f"checksum size={size} touch={touch}: {sorted(s)} {status}")
print()
for touch in (0, 1):
    for size in sorted({k[1] for k in groups}):
        m = summary.get(("mimir", size, touch))
        g = summary.get(("glibc", size, touch))
        if m is None or g is None:
            continue
        ratio = m / g if g else float("inf")
        if abs(m - g) / ((m + g) / 2) * 100 <= 5:
            grade = "TIE"
        elif m < g:
            grade = "WIN"
        else:
            grade = "LOSS"
        print(f"grade size={size//1048576}MiB touch={touch}: mimir/glibc total ratio {ratio:.3f} -> {grade}")
sys.exit(bad)
