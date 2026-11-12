#!/usr/bin/env python3
"""summarize_measured.py - Z13 protocol verdicts (Z-004).

Reads the two raw files written by scripts/z13/run_measured.sh
(one CSV line per workload per process, in the K-002 harness
format: leg,workload,ops,elapsed_ns,ops_per_sec,ns_per_op,
checksum) and applies docs/Z13_MEASUREMENT_PROTOCOL.md:

  - headline figure per (leg, workload): median of per-process
    ns_per_op; min/max printed as the spread (section 6);
  - identical-legs agreement vs the noise bound (section 7);
  - split-half drift per (leg, workload) vs the drift bound,
    DRIFT-REJECT above it (section 8);
  - checksum equivalence: exactly one distinct checksum per
    workload across both legs (section 9).

Usage: summarize_measured.py <legA.raw> <legB.raw>
           [--noise-pct P] [--drift-pct P]
Exit code 0 iff PROTOCOL GATE: PASS.
"""
import statistics
import sys
from collections import defaultdict

noise_pct = 5.0
drift_pct = 8.0
positional = []
args = sys.argv[1:]
i = 0
while i < len(args):
    if args[i] == "--noise-pct":
        noise_pct = float(args[i + 1])
        i += 2
    elif args[i] == "--drift-pct":
        drift_pct = float(args[i + 1])
        i += 2
    else:
        positional.append(args[i])
        i += 1
if len(positional) != 2:
    print("usage: summarize_measured.py <legA.raw> <legB.raw> "
          "[--noise-pct P] [--drift-pct P]", file=sys.stderr)
    sys.exit(2)


def load(path):
    """Return {workload: [(ns_per_op, checksum), ...]} in run order."""
    rows = defaultdict(list)
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            parts = line.strip().split(",")
            if len(parts) != 7:
                continue
            _leg, work, _ops, _elapsed, _ops_s, ns_op, chk = parts
            rows[work].append((float(ns_op), chk))
    return rows


def split_half_drift(values):
    """Drift % between first-half and second-half medians."""
    n = len(values)
    half = n // 2
    if half == 0:
        return 0.0
    first = statistics.median(values[:half])
    second = statistics.median(values[n - half:])
    overall = statistics.median(values)
    if overall == 0:
        return 0.0
    return 100.0 * abs(second - first) / overall


leg_a = load(positional[0])
leg_b = load(positional[1])
workloads = sorted(set(leg_a) | set(leg_b))

print(f"noise bound: {noise_pct:.1f}%   drift bound: {drift_pct:.1f}%")
print(f"{'workload':<16} {'medA_ns':>10} {'medB_ns':>10} "
      f"{'agree%':>7} {'driftA%':>8} {'driftB%':>8}  verdict")
gate_ok = True
for work in workloads:
    a_vals = [v for v, _c in leg_a.get(work, [])]
    b_vals = [v for v, _c in leg_b.get(work, [])]
    if not a_vals or not b_vals:
        print(f"{work:<16} MISSING LEG DATA")
        gate_ok = False
        continue
    med_a = statistics.median(a_vals)
    med_b = statistics.median(b_vals)
    mean = (med_a + med_b) / 2.0
    agree = 100.0 * abs(med_a - med_b) / mean if mean else 0.0
    drift_a = split_half_drift(a_vals)
    drift_b = split_half_drift(b_vals)
    checksums = {c for _v, c in leg_a[work]} | {c for _v, c in leg_b[work]}
    notes = []
    if agree > noise_pct:
        notes.append("DISAGREE")
        gate_ok = False
    if drift_a > drift_pct or drift_b > drift_pct:
        notes.append("DRIFT-REJECT")
        gate_ok = False
    if len(checksums) != 1:
        notes.append("CHECKSUM-FAIL")
        gate_ok = False
    verdict = " ".join(notes) if notes else "AGREE"
    print(f"{work:<16} {med_a:>10.2f} {med_b:>10.2f} "
          f"{agree:>7.2f} {drift_a:>8.2f} {drift_b:>8.2f}  {verdict}")
    print(f"{'':<16} spreadA {min(a_vals):.2f}..{max(a_vals):.2f} "
          f"spreadB {min(b_vals):.2f}..{max(b_vals):.2f} "
          f"repsA {len(a_vals)} repsB {len(b_vals)} "
          f"checksum {sorted(checksums)}")

print("PROTOCOL GATE: " + ("PASS" if gate_ok else "FAIL"))
sys.exit(0 if gate_ok else 1)
