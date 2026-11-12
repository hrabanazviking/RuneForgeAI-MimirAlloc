#!/usr/bin/env python3
"""summarize_z039f.py - Z-039 follow-up ladder (banked vs sharded).

Reads the per-leg ladder raw files produced by run_z039f.sh,
given as LEG=PATH arguments (e.g. banked=/tmp/z038_banked.raw
glibc=/tmp/z038_glibc.raw arena=/tmp/z038_arena.raw). A leg
whose raw carries an OWED marker and no LADDER lines is
reported as OWED - never as zero, never dropped, never
averaged over.

Per measured leg and (workload, threads): median aggregate
ops/sec across the rep processes (Z-004 protocol section 6:
repetitions are separate processes, headline is the median),
min/max spread, scaling vs the 1-thread median, parallel
efficiency, drift % across reps (section 8, bound 8%:
first-half vs second-half medians; a drift-rejected median is
INDICATIVE ONLY), and the inversion point (peak rung; first
later rung falling more than the 5% noise bound below its
predecessor). Checksum discipline per leg: chk_each must be a
single value per workload across every point and rep, and
must equal that leg's own selfcheck reference for the
workload's ladder scale (the two measured legs' references
must also agree with each other - identical work, rung for
rung).

Finally prints the TWO-WAY table the follow-up gate names:
per workload and rung, the median aggregate Mops/s of every
leg side by side (OWED where a leg is owed), with the
sharded/banked median ratio and the 5% verdict band
(WIN/TIE/LOSS for the sharded leg) where both legs measured.
Usage: summarize_z038.py LEG=RAW [LEG=RAW ...]
"""
import statistics
import sys

NOISE_PCT = 5.0
DRIFT_PCT = 8.0
RUNGS = [1, 2, 3, 4, 6, 8, 12, 16, 20, 24, 28, 32]
WORKLOADS = ["small_churn", "mixed_batch"]


def drift_pct(values):
    if len(values) < 4:
        return 0.0
    half = len(values) // 2
    first = statistics.median(values[:half])
    second = statistics.median(values[half:])
    return 100.0 * abs(second - first) / statistics.median(values)


def parse_leg(path):
    cells = {}
    refs = {}
    selfcheck_ok = False
    owed = False
    with open(path) as f:
        for line in f:
            line = line.strip()
            if "ARENA_OWED" in line or "LEG_OWED" in line:
                owed = True
            elif line.startswith("SELFCHECK mixed_batch_replica"):
                if line.endswith("MATCH"):
                    selfcheck_ok = True
            elif line.startswith("SELFCHECK small_churn_ref"):
                refs["small_churn"] = int(
                    line.rsplit("checksum=", 1)[1])
            elif line.startswith("SELFCHECK mixed_batch_ref"):
                refs["mixed_batch"] = int(
                    line.rsplit("checksum=", 1)[1])
            elif line.startswith("LADDER,"):
                p = line.split(",")
                wl, n = p[1], int(p[2])
                agg = float(p[7])
                chk_each = int(p[9])
                total_chk = int(p[10])
                cells.setdefault((wl, n), []).append(
                    (agg, chk_each, total_chk))
    if not cells:
        return {"owed": True, "cells": {}, "refs": refs,
                "selfcheck_ok": selfcheck_ok}
    return {"owed": owed and not cells, "cells": cells,
            "refs": refs, "selfcheck_ok": selfcheck_ok}


def main():
    if len(sys.argv) < 2:
        print("usage: summarize_z039f.py LEG=RAW [LEG=RAW ...]")
        return 2
    legs = {}
    order = []
    for arg in sys.argv[1:]:
        leg, _, path = arg.partition("=")
        if not path:
            print(f"bad argument {arg!r}: want LEG=RAW")
            return 2
        legs[leg] = parse_leg(path)
        order.append(leg)
    status = 0
    med = {}    # (leg, wl, n) -> median agg ops/s
    drift = {}  # (leg, wl, n) -> drift %
    for leg in order:
        data = legs[leg]
        if data["owed"]:
            print(f"=== leg {leg}: OWED (no LADDER lines; "
                  f"see the raw file and the recorded run) ===")
            continue
        if not data["selfcheck_ok"]:
            print(f"SELFCHECK leg={leg}: replica/bench MISMATCH "
                  f"or missing - VOID")
            status = 1
        for wl in WORKLOADS:
            print(f"=== {wl} ({leg}) ===")
            print("threads  median_agg_Mops/s  min      max      "
                  "scaling  efficiency  per-thread_Mops/s  drift%  checksums")
            rungs = sorted(n for (w, n) in data["cells"] if w == wl)
            for n in rungs:
                rows = data["cells"][(wl, n)]
                vals = [a for (a, _, _) in rows]
                m = statistics.median(vals)
                med[(leg, wl, n)] = m
                d = drift_pct(vals)
                drift[(leg, wl, n)] = d
                chk_ok = (all(c == data["refs"].get(wl)
                              for (_, c, _) in rows)
                          and all(t == n * data["refs"].get(wl, 0)
                                  for (_, _, t) in rows))
                print(f"{n:7d}  {m / 1e6:17.3f}  {min(vals) / 1e6:7.3f}  "
                      f"{max(vals) / 1e6:7.3f}  "
                      f"{m / med[(leg, wl, rungs[0])]:7.2f}x "
                      f"{100.0 * (m / med[(leg, wl, rungs[0])]) / n:9.1f}%  "
                      f"{m / n / 1e6:17.3f}  {d:6.2f}  "
                      f"{'OK' if chk_ok else 'BAD'}")
                if not chk_ok:
                    status = 1
                if d > DRIFT_PCT:
                    print(f"  DRIFT-REJECT leg={leg} threads={n}: "
                          f"drift {d:.2f}% > bound {DRIFT_PCT}% - "
                          f"median indicative only")
            peak = max(rungs, key=lambda n: med[(leg, wl, n)])
            inv = None
            for a, b in zip(rungs, rungs[1:]):
                if med[(leg, wl, b)] < med[(leg, wl, a)] * (
                        1.0 - NOISE_PCT / 100.0):
                    inv = (a, b)
                    break
            print(f"INVERSION {wl} ({leg}): peak aggregate "
                  f"throughput at {peak} threads "
                  f"({med[(leg, wl, peak)] / 1e6:.3f} Mops/s)")
            if inv:
                print(f"INVERSION {wl} ({leg}): scaling inverts "
                      f"between {inv[0]} and {inv[1]} threads "
                      f"(first drop beyond the {NOISE_PCT}% "
                      f"noise bound)")
            else:
                print(f"INVERSION {wl} ({leg}): no inversion "
                      f"inside the ladder (no post-peak drop "
                      f"beyond the noise bound)")
    # Cross-leg checksum agreement: identical work, rung for rung.
    measured = [leg for leg in order if not legs[leg]["owed"]]
    for wl in WORKLOADS:
        refs = {legs[leg]["refs"].get(wl) for leg in measured}
        if len(refs) > 1:
            print(f"CHECKSUM REF DISAGREEMENT {wl}: {refs} - VOID")
            status = 1
    # The two-way table (the follow-up gate artefact).
    for wl in WORKLOADS:
        print(f"=== TWO-WAY {wl} (median agg Mops/s) ===")
        hdr = "threads"
        for leg in order:
            hdr += f"  {leg:>12s}"
        if "banked" in legs and "sharded" in legs:
            hdr += "  sharded/banked  verdict"
        print(hdr)
        for n in RUNGS:
            row = f"{n:7d}"
            for leg in order:
                if legs[leg]["owed"]:
                    row += f"  {'OWED':>12s}"
                elif (leg, wl, n) in med:
                    flag = ("R" if drift[(leg, wl, n)] > DRIFT_PCT
                            else " ")
                    row += f"  {med[(leg, wl, n)] / 1e6:11.3f}{flag}"
                else:
                    row += f"  {'--':>12s}"
            if ("banked" in legs and "sharded" in legs
                    and ("banked", wl, n) in med
                    and ("sharded", wl, n) in med):
                r = med[('sharded', wl, n)] / med[('banked', wl, n)]
                v = "WIN" if r > 1.05 else ("LOSS" if r < 0.95 else "TIE")
                row += (f"  {r:11.3f}x  {v}")
            print(row)
        print("(R = DRIFT-REJECT on that leg's rung: median "
              "indicative only; OWED = leg not landed, not measured)")
    print("SUMMARY DONE Z-039F")
    return status


if __name__ == "__main__":
    sys.exit(main())
