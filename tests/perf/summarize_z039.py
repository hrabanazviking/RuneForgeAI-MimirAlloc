#!/usr/bin/env python3
"""summarize_z039.py - Z-039 ladder summariser (one leg).

Derived from the banked summarize_z020.py: identical parsing,
identical headline rules (median across processes, 5% noise
bound, 8% split-half drift bound, peak + inversion reporting,
checksum discipline). Differences: the HIST section never
fires (run_z039.sh records no hist lines), the title names
the leg (argv[2]: mimir = current banked core, shipped flags;
glibc = banked probe_z021.c comparator), and the gate line is
labelled for this slice. Usage: summarize_z039.py <raw> <leg>
Exit: 0 = checksums clean and every ladder group complete.
"""
import statistics
import sys
from collections import defaultdict

THREAD_LADDER = [1, 2, 3, 4, 6, 8, 12, 16, 20, 24, 28, 32]
WORKLOADS = ["small_churn", "mixed_batch"]
NOISE_PCT = 5.0
DRIFT_PCT = 8.0
FALLBACK_CYC_PER_NS = 2.9944  # Z-019 recorded calibration


def drift_pct(vals):
    if len(vals) < 4:
        return 0.0
    half = len(vals) // 2
    first = statistics.median(vals[:half])
    second = statistics.median(vals[half:])
    med = statistics.median(vals)
    return 100.0 * abs(second - first) / med if med else 0.0


def main():
    if len(sys.argv) < 2:
        print("usage: summarize_z020.py <raw-file>", file=sys.stderr)
        return 2
    self_lines = []
    ladder = defaultdict(list)   # (workload, threads) -> rows
    chk_each = defaultdict(set)  # workload -> {chk_each}
    hist = defaultdict(list)     # threads -> list of process dicts
    cur = None
    with open(sys.argv[1]) as f:
        for line in f:
            line = line.strip()
            if line.startswith("SELF,"):
                self_lines.append(line)
            elif line.startswith("LADDER,"):
                p = line.split(",")
                wl, th = p[1], int(p[2])
                ladder[(wl, th)].append({
                    "iters": int(p[3]), "ops_pt": int(p[4]),
                    "total_ops": int(p[5]), "elapsed_ns": int(p[6]),
                    "agg": float(p[7]), "per_thread": float(p[8]),
                    "chk_each": int(p[9]), "checksum": int(p[10]),
                })
                chk_each[wl].add(int(p[9]))
            elif line.startswith("META,ladder_hist,"):
                p = line.split(",")
                cur = {"threads": int(p[3]), "series": {}}
                hist[int(p[3])].append(cur)
            elif line.startswith("HIST,ladder_hist,") and cur is not None:
                p = line.split(",")
                count = int(p[4])
                total = int(p[5])
                cur["series"][(p[2], p[3])] = (count, total)
    ok = True
    leg = sys.argv[2] if len(sys.argv) > 2 else "?"
    print(f"Z-039 ladder summary - leg: {leg}")
    print("headline = median across processes; noise bound 5%; drift bound 8%")
    print()
    for line in self_lines:
        print("selfcheck: " + line)
        if "MISMATCH" in line:
            ok = False
    print()
    for wl in WORKLOADS:
        if len(chk_each.get(wl, set())) == 1:
            print(f"checksum {wl}: OK - exactly one chk_each "
                  f"value={next(iter(chk_each[wl]))} across all processes")
        else:
            print(f"checksum {wl}: FAIL - chk_each values "
                  f"{sorted(chk_each.get(wl, set()))}")
            ok = False
    print()
    for wl in WORKLOADS:
        print(f"=== ladder: {wl} ===")
        print("threads  processes  agg_ops_s_median  agg_min..agg_max"
              "     per_thread_median  scaling  efficiency  drift%")
        med = {}
        for th in THREAD_LADDER:
            rows = ladder.get((wl, th), [])
            if not rows:
                print(f"{th:7d}  MISSING")
                ok = False
                continue
            aggs = [r["agg"] for r in rows]
            med[th] = statistics.median(aggs)
            pt = statistics.median([r["per_thread"] for r in rows])
            base_agg = med.get(1)
            base_pt = None
            if (wl, 1) in ladder:
                base_pt = statistics.median(
                    [r["per_thread"] for r in ladder[(wl, 1)]])
            scaling = (med[th] / base_agg) if base_agg else 0.0
            eff = (pt / base_pt) if base_pt else 0.0
            d = drift_pct(aggs)
            flag = " DRIFT-REJECT" if d > DRIFT_PCT else ""
            if d > DRIFT_PCT:
                print(f"# note: {wl} t={th} drift {d:.1f}% exceeds "
                      f"{DRIFT_PCT}% bound{flag}")
            print(f"{th:7d}  {len(rows):9d}  {med[th]:16.1f}  "
                  f"{min(aggs):.1f}..{max(aggs):.1f}  {pt:17.1f}  "
                  f"{scaling:7.3f}  {eff:10.3f}  {d:6.1f}")
        if med:
            peak_th = max(med, key=lambda t: med[t])
            peak_val = med[peak_th]
            inversion = None
            for th in THREAD_LADDER:
                if th > peak_th and med.get(th) is not None and \
                        med[th] < peak_val * (1 - NOISE_PCT / 100.0):
                    inversion = th
                    break
            print(f"PEAK: {wl} aggregate peaks at {peak_th} thread(s) "
                  f"({peak_val:.1f} ops/s)")
            if inversion is not None:
                print(f"INVERSION: {wl} scaling inverts at {inversion} "
                      f"threads (median {med[inversion]:.1f} ops/s is "
                      f">5% below the {peak_th}-thread peak)")
            elif peak_th == THREAD_LADDER[-1]:
                print(f"INVERSION: {wl} none in ladder - aggregate "
                      f"still rising at {peak_th} threads")
            else:
                print(f"INVERSION: {wl} no post-peak point falls >5% "
                      f"below the peak within {THREAD_LADDER[-1]} "
                      f"threads (plateau, not inversion)")
        print()
    if hist:
        print("=== lock histogram specimen (instrumented build, "
              "small_churn shape) ===")
        print(f"cycles->ns at {FALLBACK_CYC_PER_NS} cyc/ns "
              "(Z-019 recorded TSC calibration; hist META carries "
              "no own calibration)")
        print("threads  processes  malloc_hold_mean_cyc  malloc_wait_mean_cyc"
              "  free_hold_mean_cyc  free_wait_mean_cyc")
        for th in sorted(hist):
            procs = hist[th]
            def med_mean(op, kind):
                vals = []
                for pr in procs:
                    cnt, tot = pr["series"].get((op, kind), (0, 0))
                    if cnt:
                        vals.append(tot / cnt)
                return statistics.median(vals) if vals else 0.0
            print(f"{th:7d}  {len(procs):9d}  "
                  f"{med_mean('malloc', 'hold'):20.1f}  "
                  f"{med_mean('malloc', 'wait'):20.1f}  "
                  f"{med_mean('free', 'hold'):18.1f}  "
                  f"{med_mean('free', 'wait'):18.1f}")
        print()
    print("Z-039 LEG GATE:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
