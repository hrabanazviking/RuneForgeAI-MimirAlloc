#!/usr/bin/env python3
"""summarize_z019.py - Z-019 lock hold/wait histogram summariser.

Reads the raw CSV written by run_lockhist_z019.sh (META + HIST
lines from probe_z019 processes) and prints, per phase:

  - checksum status (bench phases: exactly one checksum
    across all measured processes, the K-002 discipline)
  - per (op, kind): the PROTOCOL HEADLINE - median across
    processes of the per-process mean / p50 / p90 / p99 -
    with min..max spread of the per-process means, the
    split-half drift of the per-process means (bound 8%),
    and the POOLED histogram shape (buckets summed across
    processes, labelled pooled - shape only, never the
    headline), with percentile intervals from the pooled
    buckets.
  - bucket decode: idx < 64 -> [idx, idx+1) cycles (exact);
    idx >= 64 -> e = 6 + (idx-64)//4, sub = (idx-64) % 4,
    [2^e + sub*2^(e-2), +2^(e-2)) cycles (quarter-octave,
    max relative width 25%).

Percentile from buckets: the interval [lo, hi) containing
the rank-r sample, plus a uniform-in-bucket estimate.
Cycles -> ns uses the MEDIAN per-process cyc_per_ns from
the META lines (each process calibrates its own TSC
against CLOCK_MONOTONIC; the raw file keeps every value).
Usage: summarize_z019.py <raw-file>
"""
import statistics
import sys
from collections import defaultdict

OPS = ["malloc", "calloc", "realloc", "free", "other"]
KINDS = ["hold", "wait"]
NBUCKETS = 208


def bucket_lo_hi(idx):
    if idx < 64:
        return idx, idx + 1
    e = 6 + (idx - 64) // 4
    sub = (idx - 64) % 4
    lo = (1 << e) + sub * (1 << (e - 2))
    return lo, lo + (1 << (e - 2))


def pct_from_buckets(buckets, p):
    n = sum(buckets)
    if n == 0:
        return None
    rank = max(1, int(p * n + 0.999999))  # ceil(p*n)
    cum = 0
    for idx, c in enumerate(buckets):
        if c == 0:
            continue
        if cum + c >= rank:
            lo, hi = bucket_lo_hi(idx)
            est = lo + (rank - cum) / c * (hi - lo)
            return lo, hi, est
        cum += c
    lo, hi = bucket_lo_hi(len(buckets) - 1)
    return lo, hi, float(lo)


def mean_of_series(count, total):
    return total / count if count else 0.0


def main():
    if len(sys.argv) < 2:
        print("usage: summarize_z019.py <raw-file>", file=sys.stderr)
        return 2
    metas = []          # (phase_key, detail, threads, ops, chk, cpn)
    procs = []          # list of dicts: one per process
    cur = None
    with open(sys.argv[1]) as f:
        for line in f:
            line = line.strip()
            if line.startswith("META,"):
                p = line.split(",")
                cur = {"phase": p[1], "detail": p[2],
                       "threads": int(p[3]), "ops": int(p[4]),
                       "chk": int(p[5]), "cpn": float(p[6]),
                       "series": {}}
                procs.append(cur)
                metas.append(cur)
            elif line.startswith("HIST,") and cur is not None:
                p = line.split(",")
                key = (p[2], p[3])  # op, kind
                cur["series"][key] = {
                    "count": int(p[4]), "sum": int(p[5]),
                    "min": int(p[6]), "max": int(p[7]),
                    "lockfail": int(p[8]),
                    "buckets": [int(x) for x in p[9:9 + NBUCKETS]],
                }
    phases = []
    for pr in procs:
        k = (pr["phase"], pr["detail"], pr["threads"])
        if k not in phases:
            phases.append(k)
    print("Z-019 lock hold/wait summary (cycles; ns via median "
          "per-process TSC calibration)")
    print("headline = median across processes of per-process "
          "scalars; pooled = buckets summed across processes "
          "(shape only)")
    for pk in phases:
        group = [pr for pr in procs
                 if (pr["phase"], pr["detail"], pr["threads"]) == pk]
        cpns = [pr["cpn"] for pr in group]
        cpn = statistics.median(cpns)
        chks = sorted({pr["chk"] for pr in group})
        print()
        print(f"=== phase {pk[0]} detail={pk[1]} threads={pk[2]} "
              f"processes={len(group)} median_cyc_per_ns={cpn:.4f} "
              f"(min {min(cpns):.4f} max {max(cpns):.4f}) ===")
        if pk[0] == "bench":
            print(f"checksum status: "
                  f"{'OK - exactly one checksum' if len(chks) == 1 else 'FAIL - ' + str(len(chks)) + ' distinct checksums'} "
                  f"value={chks}")
        else:
            print(f"checksums across processes: {chks} "
                  f"(informational - phases differ per process "
                  f"only in contended chk aggregation)" if False else
                  f"checksum values: {chks}")
        lockfails = {pr["series"][("malloc", "hold")]["lockfail"]
                     for pr in group if ("malloc", "hold") in pr["series"]}
        print(f"lock_failures per process (set): {sorted(lockfails)}")
        for op in OPS:
            for kind in KINDS:
                series = [pr["series"].get((op, kind)) for pr in group]
                series = [s for s in series if s and s["count"] > 0]
                if not series:
                    continue
                means = [mean_of_series(s["count"], s["sum"])
                         for s in series]
                med_mean = statistics.median(means)
                # per-process percentiles, then median across
                pp = {}
                for p in (0.50, 0.90, 0.99):
                    ests = []
                    for s in series:
                        r = pct_from_buckets(s["buckets"], p)
                        if r:
                            ests.append(r[2])
                    pp[p] = statistics.median(ests) if ests else 0.0
                # drift on per-process means (split halves)
                half = len(means) // 2
                drift = 0.0
                if half >= 1 and len(means) >= 2:
                    a = statistics.median(means[:len(means) - half])
                    b = statistics.median(means[half:])
                    m = statistics.median(means)
                    drift = 100.0 * abs(b - a) / m if m else 0.0
                # pooled buckets
                pooled = [0] * NBUCKETS
                tot_n = 0
                tot_sum = 0
                pmax = 0
                for s in series:
                    tot_n += s["count"]
                    tot_sum += s["sum"]
                    pmax = max(pmax, s["max"])
                    for i, c in enumerate(s["buckets"]):
                        pooled[i] += c
                print(f"{op:8s} {kind:4s} n={tot_n} "
                      f"mean_cyc med={med_mean:.1f} "
                      f"({med_mean / cpn:.1f} ns) "
                      f"spread {min(means):.1f}..{max(means):.1f} "
                      f"drift {drift:.1f}% "
                      f"p50~{pp[0.50]:.0f} p90~{pp[0.90]:.0f} "
                      f"p99~{pp[0.99]:.0f} max={pmax}")
                nz = [(i, c) for i, c in enumerate(pooled) if c]
                top = sorted(nz, key=lambda t: -t[1])[:6]
                top.sort()
                desc = " ".join(
                    f"[{bucket_lo_hi(i)[0]},{bucket_lo_hi(i)[1]}):{c}"
                    for i, c in top)
                print(f"         pooled top buckets: {desc}")
                for p in (0.50, 0.99):
                    r = pct_from_buckets(pooled, p)
                    if r:
                        print(f"         pooled p{int(p * 100)} "
                              f"interval [{r[0]},{r[1]}) cyc "
                              f"est {r[2]:.0f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
