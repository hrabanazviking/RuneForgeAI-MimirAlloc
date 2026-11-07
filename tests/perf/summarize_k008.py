#!/usr/bin/env python3
"""summarize_k008.py - K-008 summary: medians across fresh-
process repetitions of the per-op percentiles printed by
probe_k008, median footprint deltas, and PMIC energy passes.
Usage: summarize_k008.py <raw>"""
import statistics, sys
from collections import defaultdict
lat = defaultdict(list); mem = defaultdict(list); en = defaultdict(list)
with open(sys.argv[1], encoding="utf-8") as fh:
    for line in fh:
        p = line.strip().split(",")
        if len(p) < 3: continue
        if p[0] == "LAT" and len(p) == 11:
            _, leg, pat, kind, n, p50, p95, p99, mx, mean, _ck = p
            lat[(leg, pat, kind)].append((int(n), int(p50), int(p95), int(p99), int(mx), int(mean)))
        elif p[0] == "MEM" and len(p) == 15:
            _, leg, s, live, req, br, bp, hr, hp, dr, dp, ar, ap, hwm, _ck = p
            mem[(leg, s)].append((int(req), int(dr), int(dp), int(ar), int(ap), int(hwm)))
        elif p[0] == "ENERGY" and len(p) == 8:
            _, leg, samples, meanw, minw, maxw, dur, ej = p
            en[leg].append((int(samples), float(meanw), float(minw), float(maxw), float(dur), float(ej)))
print(f"{'leg':<11} {'pattern':<12} {'kind':<6} {'reps':>4} {'n':>6} {'med_p50':>8} {'med_p95':>8} {'med_p99':>8} {'max_max':>9} {'med_mean':>9}")
for k in sorted(lat):
    v = lat[k]
    print(f"{k[0]:<11} {k[1]:<12} {k[2]:<6} {len(v):>4} {v[0][0]:>6} "
          f"{statistics.median(x[1] for x in v):>8.0f} {statistics.median(x[2] for x in v):>8.0f} "
          f"{statistics.median(x[3] for x in v):>8.0f} {max(x[4] for x in v):>9} "
          f"{statistics.median(x[5] for x in v):>9.0f}")
print(f"{'leg':<11} {'set':<12} {'reps':>4} {'req_kB':>7} {'med_dRSS':>9} {'med_dPSS':>9} {'med_retRSS':>11} {'med_retPSS':>11} {'med_HWM':>8}")
for k in sorted(mem):
    v = mem[k]
    print(f"{k[0]:<11} {k[1]:<12} {len(v):>4} {v[0][0]:>7} "
          f"{statistics.median(x[1] for x in v):>9.0f} {statistics.median(x[2] for x in v):>9.0f} "
          f"{statistics.median(x[3] for x in v):>11.0f} {statistics.median(x[4] for x in v):>11.0f} "
          f"{statistics.median(x[5] for x in v):>8.0f}")
print(f"{'leg':<11} {'passes':>6} {'med_mean_W':>11} {'min_W':>8} {'max_W':>8} {'med_energy_J':>13}")
for leg in sorted(en):
    v = en[leg]
    print(f"{leg:<11} {len(v):>6} {statistics.median(x[1] for x in v):>11.3f} "
          f"{min(x[2] for x in v):>8.3f} {max(x[3] for x in v):>8.3f} "
          f"{statistics.median(x[5] for x in v):>13.2f}")
