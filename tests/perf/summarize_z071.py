#!/usr/bin/env python3
"""summarize_z071.py -- Z-071 diag packet summariser.

Reads the raw CSV written by run_z071.sh and prints the
packet tables. Success cells (the 4 K-002 oracle workloads):
per-leg MEDIAN ns/op over the first REPS rows per leg (any
later open row for a failure cell is the capture rep and is
reported separately), checksum identity across all legs and
reps (a mismatch = broken cell, exit 1), emitted deltas
(must be 0 on success cells in every leg), stderr byte
totals from the errbytes trailer (exact), the identical-leg
agreement |closed - closed_dup| and the gate delta
(open - closed), each as a percent of the closed median.
Verdict per workload: TIE when |gate delta| <= max(agreement,
5%) -- the Z-004 drift discipline.

Failure cells: per-leg median ns/attempt; emitted per
attempt must be exactly 0 (closed legs) and exactly 1
(open leg: one report per genuine failure); per-report
emission cost = median(open) - median(closed) ns with
stderr to /dev/null; capture lines/bytes from the capture
trailer give the exact emitted line size.
"""
import statistics
import sys
from collections import defaultdict

REPS = 5
WORKLOADS = ["small_churn", "mixed_batch", "large_batch", "mixed_lifecycle"]
FAILS = ["fail_malloc_huge", "fail_calloc_overflow", "fail_malloc_backing"]

def main(path):
    rows = defaultdict(list)   # (leg, cell) -> [(ns_per_op, chk, emitted, suppressed)]
    errbytes = {}              # (leg, cell) -> total bytes
    captures = {}              # cell -> (lines, bytes)
    broken = []
    for line in open(path):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(",")
        if parts[0] == "errbytes":
            errbytes[(parts[1], parts[2])] = int(parts[3])
            continue
        if parts[0] == "capture":
            captures[parts[1]] = (int(parts[2]), int(parts[3]))
            continue
        if parts[0] == "PROBE_ERROR":
            broken.append(line)
            continue
        leg, cell = parts[0], parts[1]
        rows[(leg, cell)].append((float(parts[4]), int(parts[5]),
                                  int(parts[6]), int(parts[7])))
    if broken:
        print("BROKEN probe errors:", broken)
        return 1
    print("== success cells: oracle workloads, gate closed vs open ==")
    print(f"{'workload':<16} {'closed':>9} {'open':>9} {'dup':>9} "
          f"{'agree%':>7} {'gate%':>7} {'emitted':>8} {'errB':>5}  verdict")
    rc = 0
    for cell in WORKLOADS:
        med = {}
        for leg in ("closed", "open", "closed_dup"):
            vals = rows.get((leg, cell), [])
            if len(vals) < REPS:
                print(f"BROKEN {cell}/{leg}: {len(vals)} rows < {REPS}")
                rc = 1
                continue
            med[leg] = statistics.median(v[0] for v in vals[:REPS])
        chks = {v[1] for leg in ("closed", "open", "closed_dup")
                for v in rows.get((leg, cell), [])}
        emitted = sum(v[2] for leg in ("closed", "open", "closed_dup")
                      for v in rows.get((leg, cell), []))
        errb = sum(errbytes.get((leg, cell), -1)
                   for leg in ("closed", "open", "closed_dup"))
        if len(chks) != 1:
            print(f"BROKEN {cell}: checksums disagree {sorted(chks)}")
            rc = 1
            continue
        if emitted != 0 or errb != 0:
            print(f"BROKEN {cell}: emitted={emitted} errbytes={errb} on success cell")
            rc = 1
            continue
        agree = abs(med["closed"] - med["closed_dup"]) / med["closed"] * 100.0
        gate = (med["open"] - med["closed"]) / med["closed"] * 100.0
        verdict = "TIE" if abs(gate) <= max(agree, 5.0) else ("LOSS" if gate > 0 else "WIN")
        print(f"{cell:<16} {med['closed']:>9.2f} {med['open']:>9.2f} "
              f"{med['closed_dup']:>9.2f} {agree:>7.2f} {gate:>7.2f} "
              f"{emitted:>8} {errb:>5}  {verdict}")
    print()
    print("== failure cells: genuine failures, per-attempt ns ==")
    print(f"{'shape':<22} {'closed':>9} {'open':>9} {'dup':>9} "
          f"{'emit/att':>8} {'ns/report':>10} {'line B':>7}  verdict")
    for cell in FAILS:
        med = {}
        emit_per = {}
        for leg in ("closed", "open", "closed_dup"):
            vals = rows.get((leg, cell), [])
            if len(vals) < REPS:
                print(f"BROKEN {cell}/{leg}: {len(vals)} rows < {REPS}")
                rc = 1
                continue
            med[leg] = statistics.median(v[0] for v in vals[:REPS])
            tot_att = REPS * 2000
            emit_per[leg] = sum(v[2] for v in vals[:REPS]) / tot_att
        if rc:
            continue
        ok = (emit_per["closed"] == 0.0 and emit_per["closed_dup"] == 0.0
              and emit_per["open"] == 1.0)
        lines, byt = captures.get(cell, (0, 0))
        line_b = byt / lines if lines else 0.0
        per_report = med["open"] - med["closed"]
        print(f"{cell:<22} {med['closed']:>9.2f} {med['open']:>9.2f} "
              f"{med['closed_dup']:>9.2f} {emit_per['open']:>8.2f} "
              f"{per_report:>10.2f} {line_b:>7.1f}  "
              f"{'EMISSION-EXACT' if ok else 'BROKEN-EMISSION'}")
        if not ok:
            rc = 1
    return rc

if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
