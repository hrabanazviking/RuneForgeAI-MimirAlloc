#!/usr/bin/env python3
"""summarize_z005.py - Z-005 perf-counter summariser.
Reads the raw blocks written by run_perf_z005.sh and prints
the counter table: median (min..max) of each perf counter
normalised per executed op, per leg per workload.
Executed ops = warm-up ops + timed ops: perf counters cover
the whole probe process, so the normalisation basis is the
work the counters actually measured. The warm-up share is
printed per workload and is <= 2% by runner design.
Also verifies, per process block, that the probe checksum
equals reps x the Z-003 recorded per-pass checksum - proof
the measured work is the banked K-002 work. Any mismatch or
missing/<not counted> counter is reported, never hidden.
Usage: summarize_z005.py <raw-file>
"""
import statistics
import sys

EVENTS = ["cycles", "instructions", "cache-misses", "branch-misses"]
# Per-pass timed ops and warm-up ops (1/10 scale), from the
# banked bench_k002.c workload table.
PASS_OPS = {"small_churn": 100000, "mixed_batch": 256000,
            "large_batch": 9600, "mixed_lifecycle": 307200}
WARM_OPS = {"small_churn": 10000, "mixed_batch": 25600,
            "large_batch": 960, "mixed_lifecycle": 30720}
# Z-003 recorded per-pass checksums (recorded_run_z13_k002.txt).
Z003_CHK = {"small_churn": 12756656, "mixed_batch": 34727552,
            "large_batch": 1106008, "mixed_lifecycle": 40359936}


def parse(path):
    blocks = []
    cur = None
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line.startswith("=== "):
                if cur:
                    blocks.append(cur)
                parts = line.strip("= ").split()
                cur = {"leg": parts[0], "workload": parts[1],
                       "reps": int(parts[3].split("=")[1]),
                       "counters": {}, "pct": {}}
            elif cur is not None and "|" in line:
                f_ = line.split("|")
                if len(f_) >= 5 and f_[2] in EVENTS:
                    raw = f_[0].replace(",", "")
                    if raw in ("<not counted>", "<not supported>", ""):
                        cur["counters"][f_[2]] = None
                    else:
                        cur["counters"][f_[2]] = int(raw)
                    cur["pct"][f_[2]] = f_[4]
            elif cur is not None and "," in line and line[0].isalpha():
                f_ = line.split(",")
                if len(f_) == 7:
                    cur["ops"] = int(f_[2])
                    cur["ns_op"] = float(f_[5])
                    cur["checksum"] = int(f_[6])
    if cur:
        blocks.append(cur)
    return blocks


def main():
    blocks = parse(sys.argv[1])
    problems = []
    groups = {}
    for b in blocks:
        key = (b["leg"], b["workload"])
        groups.setdefault(key, []).append(b)
        want = b["reps"] * Z003_CHK[b["workload"]]
        if b.get("checksum") != want:
            problems.append(f"checksum mismatch {key}: "
                            f"{b.get('checksum')} != {want}")
        if b.get("ops") != b["reps"] * PASS_OPS[b["workload"]]:
            problems.append(f"ops mismatch {key}: {b.get('ops')}")
        for e in EVENTS:
            if b["counters"].get(e) is None:
                problems.append(f"counter missing {key} {e}")
            elif b["pct"].get(e) not in ("100.00", ""):
                problems.append(f"counter scaled {key} {e}: "
                                f"running {b['pct'].get(e)}%")
    print(f"blocks parsed: {len(blocks)} (expect 40)")
    print("checksum proof: every block checksum == reps x Z-003 "
          "per-pass checksum" if not problems else
          "PROBLEMS: " + "; ".join(problems))
    print()
    hdr = (f"{'leg':<11} {'workload':<16} {'procs':>5} {'warm%':>6} "
           f"{'cyc/op med':>11} {'ins/op med':>11} "
           f"{'cmiss/op med':>13} {'bmiss/op med':>13} {'ns/op med':>10}")
    print(hdr)
    for leg in ("glibc", "mimiralloc"):
        for w in ("small_churn", "mixed_batch", "large_batch",
                  "mixed_lifecycle"):
            bs = groups.get((leg, w), [])
            if not bs:
                print(f"{leg:<11} {w:<16} MISSING")
                continue
            exec_ops = WARM_OPS[w] + bs[0]["reps"] * PASS_OPS[w]
            warm_pct = 100.0 * WARM_OPS[w] / exec_ops
            med = {}
            for e in EVENTS:
                vals = [b["counters"][e] / exec_ops for b in bs
                        if b["counters"].get(e) is not None]
                med[e] = statistics.median(vals) if vals else float("nan")
            ns = statistics.median([b["ns_op"] for b in bs])
            print(f"{leg:<11} {w:<16} {len(bs):>5} {warm_pct:>5.1f}% "
                  f"{med['cycles']:>11.3f} {med['instructions']:>11.3f} "
                  f"{med['cache-misses']:>13.5f} "
                  f"{med['branch-misses']:>13.5f} {ns:>10.2f}")
    print()
    print("per-op ranges (min..max across processes):")
    for leg in ("glibc", "mimiralloc"):
        for w in ("small_churn", "mixed_batch", "large_batch",
                  "mixed_lifecycle"):
            bs = groups.get((leg, w), [])
            if not bs:
                continue
            exec_ops = WARM_OPS[w] + bs[0]["reps"] * PASS_OPS[w]
            cells = []
            for e in EVENTS:
                vals = [b["counters"][e] / exec_ops for b in bs
                        if b["counters"].get(e) is not None]
                cells.append(f"{e}={min(vals):.4g}..{max(vals):.4g}"
                             if vals else f"{e}=NONE")
            print(f"  {leg} {w}: " + " | ".join(cells))


if __name__ == "__main__":
    main()
