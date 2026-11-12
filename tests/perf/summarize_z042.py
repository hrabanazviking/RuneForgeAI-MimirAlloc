#!/usr/bin/env python3
"""summarize_z042.py - Z-042 thread-churn storm summariser.

Reads the raw file produced by run_z042.sh and prints, per
recorded REP: the leg's storm/null wall times, the mimir
leg's retained-bytes-after-storm figure and exit-drain
totals, every FAIL check line (verbatim), and the rep's
STORM RESULT. Ends with per-leg medians of the storm wall
and the list of retained figures across mimir reps (the
gate figure must be explainable in every rep, not just on
average). Exit 0 iff every rep in the file passed.
"""
import re
import statistics
import sys

def main() -> int:
    if len(sys.argv) != 2:
        print("usage: summarize_z042.py <raw-file>", file=sys.stderr)
        return 2
    text = open(sys.argv[1], encoding="utf-8").read()
    sections = re.split(r"^=== REP ", text, flags=re.M)[1:]
    ok = True
    walls = {"mimir": [], "glibc": []}
    retained = []
    for sec in sections:
        head = sec.splitlines()[0] if sec.splitlines() else "?"
        m = re.search(r"STORM threads=\d+ waves_of=\d+ "
                      r"null_wall_s=([\d.]+) storm_wall_s=([\d.]+) "
                      r"worst_wave_ms=([\d.]+) "
                      r"alloc_teardown_ns_per_thread=([\d.]+)", sec)
        r = re.search(r"RETAINED_AFTER_STORM_BYTES (\d+)", sec)
        d = re.search(r"DRAIN exit_blocks=(\d+)", sec)
        res = re.search(r"STORM RESULT: (PASS|FAIL)", sec)
        fails = [ln for ln in sec.splitlines()
                 if ln.startswith("CHECK ") and " FAIL " in ln]
        print(f"REP {head}")
        if m:
            print(f"  null_wall_s={m.group(1)} "
                  f"storm_wall_s={m.group(2)} "
                  f"worst_wave_ms={m.group(3)} "
                  f"alloc_teardown_ns_per_thread={m.group(4)}")
            leg = "glibc" if "glibc" in head else "mimir"
            walls[leg].append(float(m.group(2)))
        if r:
            print(f"  retained_after_storm_bytes={r.group(1)}")
            retained.append(int(r.group(1)))
        if d:
            print(f"  exit_blocks_delta={d.group(1)}")
        for ln in fails:
            print(f"  {ln}")
        print(f"  result={res.group(1) if res else 'MISSING'}")
        if not res or res.group(1) != "PASS":
            ok = False
    for leg, ws in walls.items():
        if ws:
            print(f"MEDIAN storm_wall_s leg={leg} "
                  f"{statistics.median(ws):.3f} (n={len(ws)})")
    if retained:
        print(f"RETAINED figures across mimir reps: {retained}")
    print("SUMMARY RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1

if __name__ == "__main__":
    sys.exit(main())
