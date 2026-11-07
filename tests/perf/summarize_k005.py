#!/usr/bin/env python3
import sys,statistics,collections
rows=collections.defaultdict(list);ck={};flt=collections.defaultdict(list)
for line in open(sys.argv[1]):
 p=line.strip().split(',')
 if len(p)!=7: continue
 rows[p[1]].append(int(p[3])/int(p[2]));ck.setdefault(p[1],set()).add(p[5]);flt[p[1]].append(int(p[6]))
for k in sorted(rows):
 v=sorted(rows[k])
 print(f"{k}: n={len(v)} ns/access median={statistics.median(v):.2f} min={v[0]:.2f} max={v[-1]:.2f} all={[round(x,2) for x in v]} ck={sorted(ck[k])} build_minflt_median={statistics.median(flt[k]):.0f}")
