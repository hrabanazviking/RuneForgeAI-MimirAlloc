#!/usr/bin/env python3
"""summarize_z055.py - summarise the Z-055 oracle raw run.

Reads the raw probe output, verifies the run of record
(RESULT: PASS, zero FAIL lines, both F-6 children PASS),
and prints the F-series headline table exactly as filed
in the recorded run. Every number is taken from the raw
file; nothing is recomputed from assumptions here beyond
echoing the probe's own derived values.
"""
import re
import sys

def main(path):
    text = open(path).read()
    lines = text.splitlines()
    fails = [ln for ln in lines if "[FAIL]" in ln]
    result = [ln for ln in lines if ln.startswith("RESULT:")]
    checks = [ln for ln in lines if ln.startswith("checks=")]
    hdr = [ln for ln in lines
           if ln.startswith("page=") or
           ln.startswith("capacities:")]
    f_lines = [ln.strip() for ln in lines
               if re.search(r"F-2:|F-3 |F-1\(b\)|R-EXT|R-INT|"
                            r"F-1\(a\) live|HIT median|"
                            r"full-trim trials median|"
                            r"partial-trim trials median|"
                            r"F6-A large successes|"
                            r"F6-A pool after|"
                            r"F6-D large successes|"
                            r"F6-D retained spans|"
                            r"F6-D trim released|"
                            r"spans acquired|"
                            r"class 512 warm|"
                            r"leg spans published", ln)]
    traj = [ln.strip() for ln in lines if "TRAJ" in ln]
    totals = [ln.strip() for ln in lines
              if ln.strip().startswith("totals:")]
    child = [ln.strip() for ln in lines
             if "CHILD A checks=" in ln or
             "CHILD D checks=" in ln]
    print("Z-055 oracle summary (from %s)" % path)
    for ln in hdr:
        print("  " + ln)
    ok = (not fails and result == ["RESULT: PASS"]
          and len(child) == 2
          and all("failures=0" in c for c in child))
    print("  verdict: %s (fail lines=%d, %s, children=%s)"
          % ("PASS" if ok else "NOT-PASS", len(fails),
             result[0] if result else "no RESULT line",
             child))
    for ln in checks:
        print("  main " + ln)
    print("  -- F-series headlines --")
    for ln in f_lines:
        print("  " + ln)
    print("  -- F-6 trajectory (this probe + cross-slice "
          "children) --")
    for ln in traj:
        print("  " + ln)
    for ln in totals:
        print("  " + ln)
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
