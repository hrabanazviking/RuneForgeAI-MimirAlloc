#!/usr/bin/env python3
"""summarize_z053.py - Z-053 fragmentation summariser.

Reads run_z053.sh raw CKPT lines. Per process it takes
the BASE phase RSS as that process's own baseline, then
per phase computes:
  rss_factor  = Rss / live_bytes           (gross blow-up)
  net_factor  = (Rss - Rss_BASE) / live    (allocator-held
                resident bytes per live byte)
  win_factor  = window mapped bytes / live
  hole_frac   = window hole bytes / window span bytes
Headlines are medians across processes (Z-004); the
worst case is the maximum single-process net_factor.
Checksums must be identical across legs at every
(pattern, phase) - the workload-equivalence proof.
"""
import statistics
import sys
from collections import defaultdict

FIELDS = ["leg", "pattern", "phase", "live_count", "live_bytes",
          "vma_count", "vma_delta", "rss_kib", "pss_kib",
          "avma_count", "avma_kib", "win_hole_count", "win_hole_bytes",
          "win_largest_hole", "win_unmapped", "gap_bytes", "cksum"]


def main():
    path = sys.argv[1]
    procs = []          # list of dicts: (leg,pattern) -> rows
    cur = None
    ck_by = defaultdict(set)
    with open(path) as fh:
        for line in fh:
            line = line.strip()
            if line.startswith("PROC,"):
                _, leg, pattern = line.split(",")
                cur = {"leg": leg, "pattern": pattern, "rows": {}}
                procs.append(cur)
            elif line.startswith("CKPT,"):
                parts = line.split(",")
                row = dict(zip(FIELDS, parts[1:]))
                for k in FIELDS[3:]:
                    row[k] = int(row[k])
                cur["rows"][row["phase"]] = row
                ck_by[(row["pattern"], row["phase"])].add(row["cksum"])
    bad = {k: v for k, v in ck_by.items() if len(v) != 1}
    print("checksum proof: " + ("ALL (pattern,phase) checksums "
          "identical across legs/processes" if not bad else
          f"MISMATCH {bad}"))
    agg = defaultdict(list)
    for pr in procs:
        base = pr["rows"]["BASE"]
        for phase, row in pr["rows"].items():
            e = dict(row)
            e["net_rss_kib"] = row["rss_kib"] - base["rss_kib"]
            if row["live_bytes"] > 0:
                e["rss_factor"] = row["rss_kib"] * 1024 / row["live_bytes"]
                e["net_factor"] = e["net_rss_kib"] * 1024 / row["live_bytes"]
                e["win_factor"] = row["avma_kib"] * 1024 / row["live_bytes"]
                span = row["avma_kib"] * 1024 + row["win_hole_bytes"]
                e["hole_frac"] = row["win_hole_bytes"] / span if span else 0.0
            agg[(row["leg"], row["pattern"], phase)].append(e)
    order = ["checker", "uniform", "pinwheel", "churn"]
    phases = {}
    for (_, pat, ph) in agg:
        if ph not in phases.setdefault(pat, []):
            phases[pat].append(ph)
    ph_rank = {"BASE": 0, "FULL": 1, "OP0": 1, "HOLED": 2,
               "THIRD1": 2, "OP1500": 2, "REFILL": 3,
               "THIRD2": 3, "OP3000": 3, "OP4500": 4,
               "OP6000": 5, "DRAINED": 9}
    for pat in order:
        print(f"\n=== pattern {pat} ===")
        for leg in ("mimir", "glibc"):
            rows = [(ph, agg[(leg, pat, ph)]) for ph in
                    sorted(phases.get(pat, []),
                           key=lambda p: ph_rank.get(p, 5))
                    if (leg, pat, ph) in agg]
            for ph, es in rows:
                med = lambda k: statistics.median(e[k] for e in es)
                line = (f"{leg:6s} {ph:8s} n={len(es)} "
                        f"live={int(med('live_bytes')):>10d} "
                        f"vma_d={int(med('vma_delta')):>5d} "
                        f"rss={int(med('rss_kib')):>8d}KiB "
                        f"holes={int(med('win_hole_count')):>3d} "
                        f"holeB={int(med('win_hole_bytes')):>10d} "
                        f"gapB={int(med('gap_bytes')):>10d} unmapB={int(med('win_unmapped')):>10d}")
                if es[0]["live_bytes"] > 0:
                    line += (f" rss_f={med('rss_factor'):.3f} "
                             f"net_f={med('net_factor'):.3f} "
                             f"worst_net={max(e['net_factor'] for e in es):.3f} "
                             f"win_f={med('win_factor'):.3f} "
                             f"hole_frac={med('hole_frac'):.3f}")
                else:
                    line += f" retained_net={int(med('net_rss_kib'))}KiB"
                print(line)
    print("\n=== worst-case fragmentation factor per pattern "
          "(max over live phases of the median net_factor; "
          "absolute worst single process in brackets) ===")
    for pat in order:
        for leg in ("mimir", "glibc"):
            cand = [(ph, es) for (l2, p2, ph), es in agg.items()
                    if l2 == leg and p2 == pat and es[0]["live_bytes"] > 0]
            if not cand:
                continue
            meds = [(statistics.median(e["net_factor"] for e in es), ph)
                    for ph, es in cand]
            worst_med, wph = max(meds)
            worst_abs = max(e["net_factor"] for _, es in cand for e in es)
            print(f"{leg:6s} {pat:9s} worst median net_f="
                  f"{worst_med:.3f} at {wph} [worst process "
                  f"{worst_abs:.3f}]")


if __name__ == "__main__":
    main()
