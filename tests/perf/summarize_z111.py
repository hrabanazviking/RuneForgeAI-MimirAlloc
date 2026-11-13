#!/usr/bin/env python3
"""summarize_z111.py - Z-111 tuning summariser.

Reads the tagged raw CSV produced by run_z111.sh (same row
shapes as Z-095: replay rows variant,leg,trace,ops,
elapsed_ns,ns_per_op,checksum,peak_live_bytes; oracle rows
variant,leg,workload,ops,elapsed_ns,ops_per_sec,ns_per_op,
checksum from bench_k002) and prints, per trace suite:

  CANONICAL  - the banked Z-094 traces, converted by
               convert_z111.py (the Z-111 suite of record)
  Z095-OWN   - Z-095's own captured traces (continuity)

plus the bench_k002 synthetic oracle table, and grades
every variant against Z-095's pre-registered adoption bars
APPLIED UNCHANGED:

  (a) trace suite: median ns/op improves >= 3% over the v0
      baseline on at least 2 of 3 traces AND regresses no
      trace by more than 2%;
  (b) oracle: regresses no workload's median ns/op by more
      than 2%.

The ADOPTABLE verdict is printed for the CANONICAL suite
(the Z-111 success measure names the banked canonical
traces); the Z095-OWN bar outcome is printed alongside for
the record. Any checksum disagreement on a trace across
legs/variants is a hard FAIL (the streams are identical
work).

Usage: summarize_z111.py <replay.raw> <oracle.raw>
Exit 1 on checksum disagreement or missing cells.
"""
import statistics
import sys
from collections import defaultdict


def med(v):
    return statistics.median(v)


def bar(replay, oracle, variants, traces, wls, base, obase, suite):
    print(f"=== ADOPTION BAR (Z-095 bars unchanged) - {suite} ===")
    verdicts = {}
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
        worst_oracle = float("-inf")
        worst_wl = None
        for w in wls:
            d = 100.0 * (med(oracle[(var, "mimiralloc", w)]) - obase[w]) / obase[w]
            if d > worst_oracle:
                worst_oracle = d
                worst_wl = w
        ok = improved >= 2 and worst_trace <= 2.0 and worst_oracle <= 2.0
        verdicts[var] = ok
        print(f"{var}: traces_improved_ge3pct={improved}/{len(traces)} "
              f"worst_trace_delta={worst_trace:+.2f}% "
              f"worst_oracle_delta={worst_oracle:+.2f}% ({worst_wl}) -> "
              f"{'ADOPTABLE' if ok else 'NOT ADOPTED (bar not met)'}")
    return verdicts


def main():
    if len(sys.argv) != 3:
        print("usage: summarize_z111.py <replay.raw> <oracle.raw>")
        return 1
    replay = defaultdict(list)
    checks = defaultdict(set)
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
    oracle = defaultdict(list)
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

    all_traces = sorted({t for (_, _, t) in replay})
    canon = [t for t in all_traces if "z094_" in t]
    own = [t for t in all_traces if "z094_" not in t]
    variants = sorted({v for (v, _, _) in replay} |
                      {v for (v, _, _) in oracle})
    if "v0" not in variants:
        print("FAIL: no v0 baseline")
        return 1

    def table(traces, title):
        print(f"=== TRACE SUITE {title} (median ns/op, MimirAlloc leg) ===")
        base = {}
        for var in variants:
            row = []
            for t in traces:
                v = replay.get((var, "mimiralloc", t))
                if not v:
                    continue
                m = med(v)
                name = t.split("/")[-1]
                if var == "v0":
                    base[t] = m
                    row.append(f"{name}={m:.2f}")
                else:
                    d = 100.0 * (m - base[t]) / base[t]
                    row.append(f"{name}={m:.2f} ({d:+.2f}%)")
            if row:
                print(f"{var:8} " + "  ".join(row))
        for t in traces:
            v = replay.get(("v0", "glibc", t))
            if v:
                print(f"glibc    {t.split('/')[-1]}: {med(v):.2f} ns/op "
                      f"(peak_live_bytes {peaks.get(t)})")
        print()
        return base

    cbase = table(canon, "CANONICAL (banked Z-094 traces)")
    obase_t = table(own, "Z095-OWN (Z-095 captured traces)") if own else {}

    wls = sorted({w for (_, _, w) in oracle})
    print("=== SYNTHETIC ORACLE bench_k002 (median ns/op, MimirAlloc leg) ===")
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

    bar(replay, oracle, variants, canon, wls, cbase, obase,
        "CANONICAL suite (verdict of record)")
    if own:
        bar(replay, oracle, variants, own, wls, obase_t, obase,
            "Z095-OWN suite (continuity)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
