#!/usr/bin/env python3
# summarize_z093.py - median/min/max per (leg, shape, size)
# over bench_z093 CSV rows (same 10-field format as K-007).
import sys,statistics,collections
rows=collections.defaultdict(list);ck=collections.defaultdict(set)
flt=collections.defaultdict(list);rss=collections.defaultdict(list)
for line in open(sys.argv[1]):
 p=line.strip().split(',')
 if len(p)!=10: continue
 k=(p[0],p[1],p[3])
 rows[k].append(int(p[6]));ck[k].add(p[7]);flt[k].append(int(p[8]));rss[k].append(int(p[9]))
for k in sorted(rows):
 v=sorted(rows[k])
 print(f"{k[0]},{k[1]},size={k[2]}: n={len(v)} ns/op median={statistics.median(v):.2f} min={v[0]} max={v[-1]} all={v} ck={sorted(ck[k])} minflt_median={statistics.median(flt[k]):.0f} maxrss_delta_kb_median={statistics.median(rss[k]):.0f}")
