#!/usr/bin/env python3
"""summarize_z022.py - Z-022 small-path anatomy summariser.
Reads the raw file from run_anatomy_z022.sh and prints:
the -g/.text identity proof; timing legs (per-process
ns/op, median/min/max, split-half drift vs the Z-004 8%
bound, checksum discipline); perf legs (flat symbol and
srcline shares per record process; headline = MEDIAN
share per symbol across the 3 reports, ranked - the
roadmap top-10 - plus median srcline leaders).
A symbol absent from a report counts 0.0 there.
"""
import re, statistics, sys

T = re.compile(r"^Z022,small_churn,(\d+),(\d+),([0-9.]+),(\d+)$", re.M)
SH = re.compile(r"^--- perf rep (\d+) report by symbol ---$", re.M)
LH = re.compile(r"^--- perf rep (\d+) report by srcline ---$", re.M)
SR = re.compile(r"^\s+([0-9.]+)%\s+\[\.\]\s+(.+?)\s{2,}\S+\s+-", re.M)
LR = re.compile(r"^\s+([0-9.]+)%\s+(\S+:\d+)\s*$", re.M)
SM = re.compile(r"# Samples: (\S+) of event")

def secs(text, head):
    p = head.split(text); out = {}
    for i in range(1, len(p) - 1, 2):
        out[int(p[i])] = p[i + 1]
    return out

def med_table(per_rep, limit):
    keys = set()
    for d in per_rep.values(): keys.update(d)
    med = {k: statistics.median([per_rep[r].get(k, 0.0)
           for r in (1, 2, 3)]) for k in keys}
    ranked = sorted(med.items(), key=lambda kv: -kv[1])
    for i, (k, v) in enumerate(ranked[:limit], 1):
        reps = "/".join(f"{per_rep[r].get(k, 0.0):.2f}" for r in (1, 2, 3))
        print(f"{i:>4}  {v:6.2f}  ({reps})  {k}")
    return ranked

def main():
    if len(sys.argv) != 2:
        print("usage: summarize_z022.py <raw-file>", file=sys.stderr)
        return 1
    text = open(sys.argv[1]).read()
    print("=== BUILD PROOF ===")
    for ln in text.splitlines():
        if "bytes:" in ln or ".text sha256" in ln or ln.startswith("text_identity:"):
            print(ln)
    print("\n=== TIMING (separate processes, taskset -c 2) ===")
    allrows = [(int(m.group(1)), float(m.group(3)), int(m.group(4)))
               for m in T.finditer(text)]
    rows = [r for r in allrows if r[0] == 2000000]
    perfrows = [r for r in allrows if r[0] == 50000000]
    if len(rows) != 7:
        print(f"TIMING ROWS {len(rows)} != 7 - run invalid"); return 1
    pcks = sorted({r[2] for r in perfrows})
    print(f"perf probe rows: {len(perfrows)}  checksums: {pcks} "
          f"({'OK - exactly one' if len(pcks) == 1 else 'MISMATCH'})")
    ns = [r[1] for r in rows]; cks = sorted({r[2] for r in rows})
    med = statistics.median(ns); half = len(ns) // 2; drift = 0.0
    if half:
        a = statistics.median(ns[:half]); b = statistics.median(ns[half:])
        drift = 100.0 * abs(a - b) / ((a + b) / 2.0)
    print(f"processes: {len(rows)}  ops each: {rows[0][0]}")
    print("ns/op per process: " + ", ".join(f"{v:.2f}" for v in ns))
    print(f"median ns/op: {med:.2f}  min: {min(ns):.2f}  max: {max(ns):.2f}")
    print(f"split-half drift: {drift:.2f}% (protocol bound 8%)")
    print(f"timing checksums: {cks} ({'OK - exactly one' if len(cks) == 1 else 'MISMATCH'})")
    print("\n=== PERF SYMBOL SHARES (flat, cycles; median of 3) ===")
    ss = secs(text, SH)
    if sorted(ss) != [1, 2, 3]:
        print(f"expected 3 symbol reports, found {sorted(ss)}"); return 1
    per = {}; smp = {}
    for rep, body in ss.items():
        per[rep] = {m.group(2): float(m.group(1)) for m in SR.finditer(body)}
        s = SM.search(body); smp[rep] = s.group(1) if s else "?"
    print(f"samples per rep: {smp}")
    print("rank  median%  (rep1/rep2/rep3)  symbol")
    ranked = med_table(per, 15)
    print(f"top-10 median share sum: {sum(v for _, v in ranked[:10]):.2f}%")
    print("\n=== PERF SOURCE-LINE SHARES (srcline; median of 3) ===")
    ls = secs(text, LH); perl = {}
    for rep, body in ls.items():
        perl[rep] = {m.group(2): float(m.group(1)) for m in LR.finditer(body)}
    print("rank  median%  (rep1/rep2/rep3)  source:line")
    med_table(perl, 20)
    return 0

if __name__ == "__main__":
    sys.exit(main())
