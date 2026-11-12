#!/usr/bin/env python3
# summarize_z070.py - Z-070 cost table from the raw
# runner output. Medians per leg/workload (Z-004:
# medians, never means), A/A drift from the off/off2
# identical-leg control, checksum equality across legs,
# deltas vs off, and verdicts against the pre-registered
# adoption bars (fixed before measurement):
#   pair cost, classes 16/64/256 (churn1/churnN/churnF):
#     adopt iff delta_full <= max(3 ns, 3% of off)
#   pair cost, class 1024 (poison slope):
#     adopt iff delta_full <= max(3 + 0.04*1016 ns, 5%)
#   membership at depth (churnF class 16):
#     adopt iff delta_member <= max(3 ns, 5% of off)
#   xfree is attribution, not a bar: the J-006 walk is
#     present in every leg; delta_full there is the
#     poison fill on the central course alone.
import re
import statistics
import sys

raw = open(sys.argv[1]).read()
runs = {}  # (mode, class, n) -> leg -> [ns]
checksums = {}  # (mode, class, n) -> set
hdr = None
for line in raw.splitlines():
    m = re.match(r"=== RUN leg=(\w+) mode=(\w+) class=(\d+) n=(\d+)", line)
    if m:
        hdr = (m.group(1), m.group(2), int(m.group(3)), int(m.group(4)))
        continue
    m = re.match(
        r"RESULT mode=(\w+) class=(\d+) n=(\d+) pairs=(\d+)"
        r" ns_per_pair=([\d.]+) checksum=(\d+)", line)
    if m and hdr:
        leg = hdr[0]
        key = (m.group(1), int(m.group(2)), int(m.group(3)))
        runs.setdefault(key, {}).setdefault(leg, []).append(float(m.group(5)))
        checksums.setdefault(key, set()).add(int(m.group(6)))

def med(key, leg):
    vals = runs.get(key, {}).get(leg, [])
    return statistics.median(vals) if vals else None

print("Z-070 COST TABLE (medians of 7 processes/leg, ns per alloc+free pair)")
print("%-8s %5s %5s %9s %9s %9s %9s %9s %7s %7s %7s %s" % (
    "mode", "class", "n", "off", "off2", "member", "poison",
    "full", "AA-drift%", "d-full", "d-full%", "checksum"))
order = ["churn1", "churnN", "churnF", "xfree"]
for key in sorted(runs, key=lambda k: (order.index(k[0]), k[1], k[2])):
    o, o2 = med(key, "off"), med(key, "off2")
    mb, po, fu = med(key, "member"), med(key, "poison"), med(key, "full")
    drift = abs(o - o2) / o2 * 100 if o and o2 else float("nan")
    d = fu - o if fu is not None and o is not None else float("nan")
    dp = d / o * 100 if o else float("nan")
    cs = "EQUAL" if len(checksums.get(key, set())) == 1 else "DIFFER"
    print("%-8s %5d %5d %9.3f %9.3f %9.3f %9.3f %9.3f %7.2f %+7.3f %+7.2f %s" % (
        key[0], key[1], key[2], o, o2, mb, po, fu, drift, d, dp, cs))

print()
print("FACTORIAL DELTAS (ns/pair vs off): member-only, poison-only,"
      " full, additivity residual full-(member+poison)")
for key in sorted(runs, key=lambda k: (order.index(k[0]), k[1], k[2])):
    o = med(key, "off")
    dm = med(key, "member") - o
    dp = med(key, "poison") - o
    df = med(key, "full") - o
    print("%-8s class=%-5d n=%-4d member=%+7.3f poison=%+7.3f"
          " full=%+7.3f residual=%+7.3f" % (
              key[0], key[1], key[2], dm, dp, df, df - (dm + dp)))

print()
print("ADOPTION BARS (pre-registered)")
def bar(key, leg, ns_cap, pct_cap):
    o = med(key, "off")
    v = med(key, leg)
    if o is None or v is None:
        print("  MISSING DATA for", key, leg)
        return
    d = v - o
    cap = max(ns_cap, o * pct_cap / 100.0)
    print("  %-22s delta=%+7.3f ns cap=%.3f ns -> %s" % (
        "%s/%d/%s" % (key[0], key[1], leg), d, cap,
        "PASS" if d <= cap else "FAIL"))

for cls in (16, 64, 256):
    bar(("churn1", cls, 1), "full", 3.0, 3.0)
for cls in (16, 64):
    n = 250 if cls == 16 else 62
    bar(("churnN", cls, n), "full", 3.0, 3.0)
    bar(("churnF", cls, n), "full", 3.0, 3.0)
bar(("churn1", 1024, 1), "full", 3.0 + 0.04 * 1016, 5.0)
bar(("churnF", 16, 250), "member", 3.0, 5.0)
print("  xfree rows: attribution only (J-006 walk in every leg).")
