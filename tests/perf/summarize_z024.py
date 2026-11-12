#!/usr/bin/env python3
"""summarize_z024.py - Z-024 free-path anatomy summariser.
Reads RESULT lines written by run_z024.sh (one per measured
process) and prints, per (leg, phase, size): the PROTOCOL
HEADLINE - median across processes of ns_per_op - with
min..max spread, split-half drift, and checksum status
(exactly one checksum per group is required).
Usage: summarize_z024.py <raw-file>
"""
import statistics
import sys
from collections import defaultdict

def main():
    if len(sys.argv) < 2:
        print("usage: summarize_z024.py <raw-file>", file=sys.stderr)
        return 2
    groups = defaultdict(list)
    chks = defaultdict(set)
    with open(sys.argv[1]) as f:
        for line in f:
            line = line.strip()
            if not line.startswith("RESULT,"):
                continue
            p = line.split(",")
            key = (p[1], p[2], int(p[3]))
            groups[key].append(float(p[6]))
            chks[key].add(int(p[7]))
    print("Z-024 free-path summary (ns per free; headline = median across processes)")
    bad = 0
    for key in sorted(groups):
        vals = groups[key]
        med = statistics.median(vals)
        half = len(vals) // 2
        drift = None
        if half >= 2:
            a = statistics.median(vals[:half])
            b = statistics.median(vals[half:])
            drift = 100.0 * abs(a - b) / ((a + b) / 2.0)
        ok = len(chks[key]) == 1
        if not ok:
            bad += 1
        dstr = "n/a" if drift is None else "%.2f%%" % drift
        cstr = "OK" if ok else "FAIL " + str(sorted(chks[key]))
        print("%6s %-28s size=%7d procs=%d median_ns_op=%10.3f min=%10.3f max=%10.3f drift=%s checksum=%s" % (key[0], key[1], key[2], len(vals), med, min(vals), max(vals), dstr, cstr))
    if bad:
        print("CHECKSUM FAILURES: %d group(s)" % bad, file=sys.stderr)
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
