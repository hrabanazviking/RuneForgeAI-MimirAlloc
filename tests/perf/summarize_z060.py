#!/usr/bin/env python3
"""Summarize a Z-060 run log: PASS iff the in-process legs
are clean (test-z060 exit 0, 3 cycle attempts PASS, caps
and madv PASS), the ceiling leg PASSes inside its scope,
and the hog leg shows cgroup containment (scope OOM kill,
exit 137, or a clean MMAP_NULL) with the host alive after.
"""
import re, sys
log = open(sys.argv[1]).read()
cycles = log.count("PROBE_Z060_CYCLES: PASS")
caps = log.count("PROBE_Z060_CAPS: PASS")
madv = log.count("PROBE_Z060_MADV: PASS")
ceiling = log.count("PROBE_Z060_CEILING: PASS")
fails = len(re.findall(r"PROBE_Z060.*: FAIL", log))
m = re.search(r"test_z060_exit: (\d+)", log)
test_exit = m.group(1) if m else "MISSING"
m = re.search(r"ceiling_exit: (\d+)", log)
ceiling_exit = m.group(1) if m else "MISSING"
m = re.search(r"hog_exit: (\d+)", log)
hog_exit = m.group(1) if m else "MISSING"
hogfree = log.count("PROBE_Z060_HOGFREE: PASS")
hog_null = "hog_result=MMAP_NULL" in log
hog_contained = hog_exit == "137" or hog_null
host_alive = "host_alive_after_hog:" in log
print(f"cycles_pass: {cycles}/3")
print(f"caps_pass: {caps}")
print(f"madv_pass: {madv}")
print(f"ceiling_pass: {ceiling}")
print(f"test_z060_exit: {test_exit}")
print(f"ceiling_exit: {ceiling_exit}")
print(f"hogfree_pass: {hogfree}")
print(f"hog_exit: {hog_exit}")
print(f"hog_contained: {hog_contained}")
print(f"host_alive_after_hog: {host_alive}")
print(f"fail_lines: {fails}")
ok = (cycles == 3 and caps >= 1 and madv >= 1 and ceiling == 1
      and test_exit == "0" and ceiling_exit == "0"
      and hog_contained and hogfree >= 1 and host_alive and fails == 0)
print("Z060 SUMMARY: " + ("PASS" if ok else "FAIL"))
sys.exit(0 if ok else 1)
