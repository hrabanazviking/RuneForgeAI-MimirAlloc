#!/usr/bin/env python3
"""summarize_z113.py - Z-113 GPU-shared UMA summariser.
Reads the raw file written by run_z113.sh: phase sections
marked by lines PHASE <name>, CSV probe lines per process.
Prints median ns/op per phase/leg/workload, the mimir vs
glibc ratio per phase, and the interference factor per leg
(loaded median / baseline median). Grades at the house 5%
bound: ratio in [0.95,1.05] TIE, below WIN, above LOSS.
For the interference factor the grade reads as: TIE = no
material interference, LOSS = co-tenant degrades the leg
past the bound, WIN = leg faster under load (noise-grade
curiosity, never claimed as a benefit). Checksums must be
uniform per workload across all phases and legs (per-pass
checksum x reps); any mismatch is reported, never hidden.
Usage: summarize_z113.py <raw-file>
"""
import statistics
import sys
from collections import defaultdict

def grade(r):
    if r < 0.95: return "WIN"
    if r > 1.05: return "LOSS"
    return "TIE"

def main():
    phase = None
    data = defaultdict(list)
    chks = defaultdict(set)
    with open(sys.argv[1]) as f:
        for line in f:
            line = line.strip()
            if line.startswith("PHASE "):
                phase = line.split()[1]
            elif line.count(",") == 6 and phase:
                p = line.split(",")
                leg, wl = p[0], p[1]
                data[(phase, leg, wl)].append(float(p[5]))
                chks[wl].add(int(p[6]))
    bad = {w: c for w, c in chks.items() if len(c) != 1}
    print("checksum uniformity:", "OK all workloads" if not bad else f"MISMATCH {bad}")
    wls = sorted({k[2] for k in data})
    med = {}
    for (ph, leg, wl), vals in sorted(data.items()):
        m = statistics.median(vals)
        med[(ph, leg, wl)] = m
        print(f"{ph} {leg} {wl}: median {m:.2f} ns/op  min {min(vals):.2f}  max {max(vals):.2f}  n={len(vals)}")
    print()
    for wl in wls:
        for ph in ("baseline", "gpuload"):
            r = med[(ph, "mimiralloc", wl)] / med[(ph, "glibc", wl)]
            print(f"{wl} {ph}: mimir/glibc {r:.3f} -> {grade(r)} (standing under this phase)")
        for leg in ("glibc", "mimiralloc"):
            f = med[("gpuload", leg, wl)] / med[("baseline", leg, wl)]
            print(f"{wl} {leg}: interference factor {f:.3f} -> {grade(f)}")
        print()
    return 1 if bad else 0

if __name__ == "__main__":
    sys.exit(main())
