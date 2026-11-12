#!/usr/bin/env python3
"""Z-043 summariser: per-phase medians across the 7 measured
processes, split-half drift on ns_per_op, checksum agreement,
and the worst-case latency distribution (per-process max_ns
reported as min / median / max - a single pooled max hides
its own fragility, so the spread is printed, never averaged
away).

Reads RESULT lines written by probe_z043.c:
  RESULT phase=<p> ns_per_op=<f> ops=<n> checksum=<n>
         max_ns=<n> p99_ns=<n> timer_floor_ns=<n> [...]
Exactly one checksum per phase and exactly 7 processes per
phase are required (exit 2 otherwise).
"""
import statistics
import sys
from collections import defaultdict


def load(path):
    rows = defaultdict(list)
    checksums = defaultdict(set)
    with open(path) as fh:
        for line in fh:
            if not line.startswith("RESULT "):
                continue
            fields = dict(
                kv.split("=", 1) for kv in line.split()[1:] if "=" in kv
            )
            rows[fields["phase"]].append(fields)
            checksums[fields["phase"]].add(fields["checksum"])
    return rows, checksums


def report(path, label):
    rows, checksums = load(path)
    bad = False
    for phase, vals in sorted(rows.items()):
        if len(vals) != 7:
            print(f"BAD COUNT {phase}: {len(vals)} processes (want 7)")
            bad = True
        if len(checksums[phase]) != 1:
            print(f"BAD CHECKSUM {phase}: {sorted(checksums[phase])}")
            bad = True
    if bad:
        return 2
    print(f"== {label}: {path}")
    print(f"{'phase':16s} {'median':>12s} {'drift%':>7s} "
          f"{'p99_med':>10s} {'max_min':>10s} {'max_med':>10s} "
          f"{'max_max':>10s}")
    for phase in sorted(rows):
        vals = rows[phase]
        ns = [float(v["ns_per_op"]) for v in vals]
        med = statistics.median(ns)
        half = len(ns) // 2
        a = statistics.median(ns[:half])
        b = statistics.median(ns[half:])
        drift = abs(a - b) / ((a + b) / 2) * 100 if (a + b) else 0.0
        p99s = [int(v["p99_ns"]) for v in vals]
        maxs = [int(v["max_ns"]) for v in vals]
        extra = ""
        if "bypassed" in vals[0]:
            extra = (f"  bypassed={vals[0]['bypassed']}"
                     f" queued={vals[0].get('queued', '?')}"
                     f" drain_calls={vals[0].get('drain_calls', '?')}")
        print(f"{phase:16s} {med:12.1f} {drift:7.2f} "
              f"{statistics.median(p99s):10.0f} {min(maxs):10d} "
              f"{statistics.median(maxs):10.0f} {max(maxs):10d}{extra}")
    return 0


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: summarize_z043.py <raw.csv> [glibc.csv]",
              file=sys.stderr)
        return 2
    rc = report(sys.argv[1], "mimir")
    if rc != 0:
        return rc
    if len(sys.argv) > 2:
        return report(sys.argv[2], "glibc comparator")
    return 0


if __name__ == "__main__":
    sys.exit(main())
