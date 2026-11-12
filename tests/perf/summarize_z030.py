#!/usr/bin/env python3
"""Z-030 summariser: median per phase across processes,
split-half drift, and the comparison table vs Z-024's
recorded direct-free values and across the queue legs.

Reads RESULT lines written by probe_z030.c:
  RESULT phase=<p> ns_per_op=<f> ops=<n> checksum=<n> ...
Exactly one checksum per phase is required (exit 2 if a
phase disagrees with itself), exactly 7 measured processes
per phase (exit 2 otherwise). Drift is the split-half
agreement of the 7 medians' halves, as in Z-024.
"""
import statistics
import sys
from collections import defaultdict

Z024 = {
    "local_steady": 444.1,
    "remote_same_direct": 456.1,
    "remote_cross_direct": 470.0,
    "swap_same_direct": 1128.2,
    "swap_cross_direct": 1356.8,
}

def main() -> int:
    if len(sys.argv) != 2:
        print("usage: summarize_z030.py <raw.csv>", file=sys.stderr)
        return 2
    rows = defaultdict(list)
    checksums = defaultdict(set)
    with open(sys.argv[1]) as fh:
        for line in fh:
            if not line.startswith("RESULT "):
                continue
            fields = dict(
                kv.split("=", 1) for kv in line.split()[1:] if "=" in kv
            )
            rows[fields["phase"]].append(float(fields["ns_per_op"]))
            checksums[fields["phase"]].add(fields["checksum"])
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
    med = {p: statistics.median(v) for p, v in rows.items()}
    print("phase                    median_ns  drift%   n")
    for phase in sorted(rows):
        vals = rows[phase]
        half = len(vals) // 2
        a = statistics.median(vals[:half])
        b = statistics.median(vals[half:])
        drift = abs(a - b) / ((a + b) / 2) * 100 if (a + b) else 0.0
        print(f"{phase:24s} {med[phase]:9.1f}  {drift:5.2f}  {len(vals)}")
    print()
    print("direct legs vs Z-024 recorded (same shape, banked core):")
    for phase, ref in Z024.items():
        if phase in med:
            delta = (med[phase] - ref) / ref * 100
            print(f"  {phase:24s} z030 {med[phase]:9.1f}  "
                  f"z024 {ref:9.1f}  delta {delta:+.1f}%")
    print()
    print("queue legs vs today's direct leg (same run):")
    for geo in ("remote_same", "remote_cross"):
        d = med.get(f"{geo}_direct")
        e = med.get(f"{geo}_enq")
        t = med.get(f"{geo}_total")
        if d and e and t:
            print(f"  {geo:12s} direct {d:9.1f}  enq-only {e:9.1f} "
                  f"({(e - d) / d * 100:+.1f}%)  total {t:9.1f} "
                  f"({(t - d) / d * 100:+.1f}%)")
    for geo in ("swap_same", "swap_cross"):
        d = med.get(f"{geo}_direct")
        e = med.get(f"{geo}_enq")
        t = med.get(f"{geo}_total")
        if d and t:
            extra = f" enq-only {e:9.1f} ({(e - d) / d * 100:+.1f}%)" if e else ""
            print(f"  {geo:12s} direct {d:9.1f}{extra}  total {t:9.1f} "
                  f"({(t - d) / d * 100:+.1f}%)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
