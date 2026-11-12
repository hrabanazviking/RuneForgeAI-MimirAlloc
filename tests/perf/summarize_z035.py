#!/usr/bin/env python3
"""summarize_z035.py - Z-035 build-flag honesty verdicts.

Reads the raw file written by tests/perf/run_z035.sh (one CSV
line per workload per process:
variant,workload,ops,elapsed_ns,ops_per_sec,ns_per_op,checksum)
and applies docs/Z13_MEASUREMENT_PROTOCOL.md:

  - headline per (variant, workload): median of per-process
    ns_per_op; min/max printed as the spread (section 6);
  - delta vs the base (-O2) median, in percent (positive =
    faster than base);
  - split-half drift per (variant, workload) vs the 8% drift
    bound (section 8): above it the row reads DRIFT-REJECT;
  - checksum equivalence: exactly one distinct checksum per
    workload across ALL variants (section 9 / K-002 discipline);
  - per-workload verdict per variant: WIN if delta > +5% (the
    noise bound) with drift inside bounds, LOSS if delta < -5%,
    else TIE (inside noise).

Adoption rule (stated before measuring, Z-035): a flag is
adopted only if its isolated variant beats base by more than
the 5% noise bound on BOTH small-path workloads (small_churn,
mixed_batch), with drift inside bounds and checksums identical,
and no other workload regresses beyond the noise bound.

Usage: summarize_z035.py <raw-file> [--noise-pct P] [--drift-pct P]
Exit 0 iff checksums are identical for every workload AND no
small-path row is DRIFT-REJECT.
"""
import statistics
import sys
from collections import defaultdict

NOISE = 5.0
DRIFT = 8.0
args = sys.argv[1:]
positional = []
i = 0
while i < len(args):
    if args[i] == "--noise-pct":
        NOISE = float(args[i + 1]); i += 2
    elif args[i] == "--drift-pct":
        DRIFT = float(args[i + 1]); i += 2
    else:
        positional.append(args[i]); i += 1
if len(positional) != 1:
    print("usage: summarize_z035.py <raw-file>", file=sys.stderr)
    sys.exit(2)

VARIANT_ORDER = ["base", "o3", "march", "o3march", "lto", "full"]
SMALL_PATH = ["small_churn", "mixed_batch"]

rows = defaultdict(lambda: defaultdict(list))
checks = defaultdict(set)
with open(positional[0], encoding="utf-8") as fh:
    for line in fh:
        parts = line.strip().split(",")
        if len(parts) != 7:
            continue
        var, work, _ops, _el, _ops_s, ns_op, chk = parts
        rows[var][work].append(float(ns_op))
        checks[work].add(chk)

workloads = sorted({w for v in rows.values() for w in v})


def drift_pct(values):
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


print(f"noise bound: {NOISE:.1f}%   drift bound: {DRIFT:.1f}%")
print("checksums: " + "; ".join(
    f"{w}={'OK(1 distinct)' if len(checks[w]) == 1 else 'MISMATCH(' + str(len(checks[w])) + ' distinct)'}"
    for w in workloads))
checksum_ok = all(len(checks[w]) == 1 for w in workloads)

print(f"\n{'variant':<9} {'workload':<16} {'median':>10} {'min':>10} "
      f"{'max':>10} {'drift%':>7} {'delta%':>8}  verdict")
gate_ok = checksum_ok
summary = {}
for var in VARIANT_ORDER:
    if var not in rows:
        continue
    summary[var] = {}
    for work in workloads:
        vals = rows[var][work]
        med = statistics.median(vals)
        dr = drift_pct(vals)
        if var == "base":
            delta = 0.0
            verdict = "BASE"
        else:
            bmed = statistics.median(rows["base"][work])
            delta = 100.0 * (bmed - med) / bmed
            if dr > DRIFT:
                verdict = "DRIFT-REJECT"
            elif delta > NOISE:
                verdict = "WIN"
            elif delta < -NOISE:
                verdict = "LOSS"
            else:
                verdict = "TIE"
        if dr > DRIFT and work in SMALL_PATH:
            gate_ok = False
            if var == "base":
                verdict = "DRIFT-REJECT"
        summary[var][work] = (med, delta, dr, verdict)
        print(f"{var:<9} {work:<16} {med:>10.2f} {min(vals):>10.2f} "
              f"{max(vals):>10.2f} {dr:>7.2f} {delta:>8.2f}  {verdict}")

print("\nadoption rule: isolated flag adopted only if WIN on BOTH "
      "small-path workloads, drift inside bounds, checksums "
      "identical, and no other workload LOSS.")
for var, label in [("o3", "-O3"), ("march", "-march=native"),
                   ("lto", "LTO (-flto)")]:
    if var not in summary:
        continue
    small = [summary[var][w][3] for w in SMALL_PATH]
    others = [summary[var][w][3] for w in workloads if w not in SMALL_PATH]
    adopt = checksum_ok and all(v == "WIN" for v in small) \
        and all(v != "LOSS" for v in others)
    print(f"  {label:<16} small-path={small} others={others} "
          f"-> {'ADOPT' if adopt else 'DO NOT ADOPT'}")
for var, label in [("o3march", "-O3 + -march=native"),
                   ("full", "-O3 + -march=native + LTO")]:
    if var not in summary:
        continue
    small = [summary[var][w][3] for w in SMALL_PATH]
    others = [summary[var][w][3] for w in workloads if w not in SMALL_PATH]
    print(f"  combo {label:<26} small-path={small} others={others} "
          f"(informational; adoption judges isolated flags first)")

print(f"\nZ035 SUMMARY GATE: {'PASS' if gate_ok else 'FAIL'} "
      f"(checksums {'identical' if checksum_ok else 'MISMATCH'})")
sys.exit(0 if gate_ok else 1)
