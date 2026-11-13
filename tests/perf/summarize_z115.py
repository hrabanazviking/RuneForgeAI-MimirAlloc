#!/usr/bin/env python3
"""summarize_z115.py - Z-115 KV-cache-shaped summariser.
Reads the raw file written by run_z115.sh: CSV probe lines
  leg,shape,ops,elapsed_ns,ops_per_sec,ns_per_op,checksum
with legs mimiralloc, glibc (default) and glibc_tuned
(GLIBC_TUNABLES gcombo_cache, the Z-098/Z-107 tuned column
winner for large-block shapes). Prints median ns/op per
leg/shape and grades mimiralloc against BOTH glibc columns
on the house Z-004 5% band (ratio < 0.95 WIN, > 1.05 LOSS,
else TIE; lower ns/op is better). Hard FAIL on any missing
cell, any ops or checksum divergence within a shape (the
legs must do identical work), or rep count != expected.
Usage: summarize_z115.py <raw-file> [procs]
"""
import statistics
import sys
from collections import defaultdict

SHAPES = ["kv_append", "kv_blocks", "kv_mixed"]
LEGS = ["mimiralloc", "glibc", "glibc_tuned"]

def grade(r):
    if r < 0.95: return "WIN"
    if r > 1.05: return "LOSS"
    return "TIE"

def main():
    procs = int(sys.argv[2]) if len(sys.argv) > 2 else 5
    data = defaultdict(list)
    opss = defaultdict(set)
    chks = defaultdict(set)
    with open(sys.argv[1]) as f:
        for line in f:
            line = line.strip()
            if line.count(",") != 6:
                continue
            p = line.split(",")
            if p[0] not in LEGS or p[1] not in SHAPES:
                continue
            data[(p[0], p[1])].append(float(p[5]))
            opss[p[1]].add(int(p[2]))
            chks[p[1]].add(int(p[6]))
    bad = 0
    for sh in SHAPES:
        if len(opss[sh]) != 1:
            print(f"OPS DIVERGENCE {sh}: {sorted(opss[sh])}"); bad += 1
        if len(chks[sh]) != 1:
            print(f"CHECKSUM DIVERGENCE {sh}: {sorted(chks[sh])}"); bad += 1
        for leg in LEGS:
            n = len(data[(leg, sh)])
            if n != procs:
                print(f"MISSING/EXTRA CELL {leg} {sh}: n={n} expect {procs}"); bad += 1
    if bad:
        print(f"Z-115 RESULT: FAIL - {bad} cell/work problems")
        return 1
    med = {}
    print(f"{'leg':<12} {'shape':<10} {'med_ns/op':>10} {'min':>10} {'max':>10} n")
    for (leg, sh) in sorted(data):
        v = data[(leg, sh)]
        med[(leg, sh)] = statistics.median(v)
        print(f"{leg:<12} {sh:<10} {med[(leg, sh)]:>10.2f} {min(v):>10.2f} {max(v):>10.2f} {len(v)}")
    print()
    print("=== VERDICTS - mimiralloc vs both glibc columns (5% band) ===")
    for sh in SHAPES:
        m = med[("mimiralloc", sh)]
        for col in ("glibc", "glibc_tuned"):
            g = med[(col, sh)]
            r = m / g
            print(f"{sh}: mimir {m:.2f} vs {col} {g:.2f} ns/op | factor {r:.3f}x | {grade(r)}")
    print("Z-115 RESULT: PASS (all cells present, ops and checksums uniform)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
