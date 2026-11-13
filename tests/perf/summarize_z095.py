#!/usr/bin/env python3
"""summarize_z095.py - Z-095 tuning summariser.

Reads tagged raw CSV produced by run_z095.sh:
  replay lines: variant,leg,trace,ops,elapsed_ns,ns_per_op,
                checksum,peak_live_bytes
  oracle lines: variant,leg,workload,ops,elapsed_ns,
                ops_per_sec,ns_per_op,checksum   (bench_k002)
and prints the before/after delta tables the Z-095 gate
requires, for BOTH suites, plus the adoption-bar verdict.

Adoption bar (registered before the verdict, Z-032 style):
a candidate variant is adoptable only if
  (a) on the trace suite its median ns/op improves >= 3%
      over baseline on at least 2 of the traces and
      regresses no trace by more than 2%, AND
  (b) on the synthetic oracle (bench_k002) it regresses no
      workload's median ns/op by more than 2%.
Any checksum disagreement on a trace (legs or variants) is
a hard FAIL: the replay streams must be identical work.

Usage: summarize_z095.py <replay.raw> <oracle.raw>
Exit 1 on checksum disagreement or missing cells.
"""
import statistics
import sys
from collections import defaultdict


def med(v):
    return statistics.median(v)


def main():
    if len(sys.argv) != 3:
        print("usage: summarize_z095.py <replay.raw> <oracle.raw>")
        return 1
    replay = defaultdict(list)     # (variant, leg, trace) -> [ns/op]
    checks = defaultdict(set)      # trace -> {checksums}
    peaks = {}
    for ln in open(sys.argv[1]):
        ln = ln.strip()
        if not ln or ln.startswith("#"):
            continue
        p = ln.split(",")
        if len(p) != 8:
            continue
        var, leg, trace, ops, el, ns, chk, peak = p
        replay[(var, leg, trace)].append(float(ns))
        checks[trace].add(chk)
        peaks[trace] = peak
    oracle = defaultdict(list)     # (variant, leg, workload) -> [ns/op]
    for ln in open(sys.argv[2]):
        ln = ln.strip()
        if not ln or ln.startswith("#"):
            continue
        p = ln.split(",")
        if len(p) != 8:
            continue
        var, leg, wl, ops, el, ops_s, ns, chk = p
        oracle[(var, leg, wl)].append(float(ns))

    bad = 0
    for trace, s in sorted(checks.items()):
        if len(s) != 1:
            print(f"FAIL: checksum disagreement on {trace}: {sorted(s)}")
            bad = 1
    if bad:
        return 1

    traces = sorted({t for (_, _, t) in replay})
    variants = sorted({v for (v, _, _) in replay})
    if "v0" not in variants:
        print("FAIL: no v0 baseline in replay raw")
        return 1
    print("=== TRACE SUITE (median ns/op, MimirAlloc leg) ===")
    print(f"{'variant':8} " + " ".join(f"{t.split('/')[-1]:>22}" for t in traces))
    base = {}
    for var in variants:
        row = []
        for t in traces:
            v = replay.get((var, "mimiralloc", t))
            if not v:
                print(f"FAIL: missing replay cell {var} {t}")
                return 1
            m = med(v)
            if var == "v0":
                base[t] = m
                row.append(f"{m:22.2f}")
            else:
                d = 100.0 * (m - base[t]) / base[t]
                row.append(f"{m:17.2f} ({d:+5.2f}%)")
        print(f"{var:8} " + " ".join(row))
    print("--- glibc leg (context, v0 build) ---")
    for t in traces:
        v = replay.get(("v0", "glibc", t))
        if v:
            print(f"glibc {t.split('/')[-1]:>22}: {med(v):10.2f} ns/op "
                  f"(peak_live_bytes {peaks.get(t)})")

    print()
    print("=== SYNTHETIC ORACLE bench_k002 (median ns/op, MimirAlloc leg) ===")
    wls = sorted({w for (_, _, w) in oracle})
    obase = {}
    for var in variants:
        row = []
        for w in wls:
            v = oracle.get((var, "mimiralloc", w))
            if not v:
                print(f"FAIL: missing oracle cell {var} {w}")
                return 1
            m = med(v)
            if var == "v0":
                obase[w] = m
                row.append(f"{w}={m:.2f}")
            else:
                d = 100.0 * (m - obase[w]) / obase[w]
                row.append(f"{w}={m:.2f} ({d:+.2f}%)")
        print(f"{var:8} " + "  ".join(row))

    print()
    print("=== ADOPTION BAR ===")
    for var in variants:
        if var == "v0":
            continue
        improved = 0
        worst_trace = 0.0
        for t in traces:
            d = 100.0 * (med(replay[(var, "mimiralloc", t)]) - base[t]) / base[t]
            if d <= -3.0:
                improved += 1
            worst_trace = max(worst_trace, d)
        worst_oracle = 0.0
        for w in wls:
            d = 100.0 * (med(oracle[(var, "mimiralloc", w)]) - obase[w]) / obase[w]
            worst_oracle = max(worst_oracle, d)
        ok = improved >= 2 and worst_trace <= 2.0 and worst_oracle <= 2.0
        print(f"{var}: traces_improved_ge3pct={improved}/{len(traces)} "
              f"worst_trace_delta={worst_trace:+.2f}% "
              f"worst_oracle_delta={worst_oracle:+.2f}% -> "
              f"{'ADOPTABLE' if ok else 'NOT ADOPTED (bar not met)'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
