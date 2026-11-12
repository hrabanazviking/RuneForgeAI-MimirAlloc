#!/usr/bin/env python3
"""Summarize a Z-050 run log: verdict PASS iff the unit suite
is clean (0 failed) and all 3 probe attempts PASS."""
import re, sys
log = open(sys.argv[1]).read()
m = re.search(r"Z050 CHECKS: (\d+) run, (\d+) failed", log)
probes = log.count("PROBE_Z050: PASS")
fails = log.count("PROBE_Z050: FAIL")
print(f"unit_checks_run: {m.group(1) if m else 'MISSING'}")
print(f"unit_checks_failed: {m.group(2) if m else 'MISSING'}")
print(f"probe_pass: {probes}/3")
print(f"probe_fail_lines: {fails}")
ok = m and m.group(2) == "0" and probes == 3 and fails == 0
print("Z050 SUMMARY: " + ("PASS" if ok else "FAIL"))
sys.exit(0 if ok else 1)
