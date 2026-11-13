#!/usr/bin/env python3
"""summarize_z101.py - Z-101 storm battery summariser.

Reads the raw file produced by run_z101.sh and prints, per
recorded REP: the leg's phase wall times, fork counts,
cross-free total, the mimir leg's retained-bytes figures
(after combined / after churn storm / after ring / after
fork / pre-trim / final), every FAIL check line
(verbatim), and the rep's BATTERY RESULT. Ends with
per-leg medians of the churn-storm wall, the list of
retained figures across mimir reps (the gate figures must
be explainable in every rep, not just on average), and
the cross-leg workload-equivalence verdict: the churn
checksum and the ring checksum must be identical in
every rep of both legs (K-002 discipline - the two legs
did identical work). Exit 0 iff every rep passed and the
checksums agree.
"""
import re
import statistics
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: summarize_z101.py <raw-file>", file=sys.stderr)
        return 2
    text = open(sys.argv[1], encoding="utf-8").read()
    sections = re.split(r"^=== REP ", text, flags=re.M)[1:]
    ok = True
    walls = {"mimir": [], "glibc": []}
    retained = {"combined": [], "storm": [], "ring": [],
                "fork": [], "pre_trim": [], "final": []}
    checksums = set()
    ring_checksums = set()
    for sec in sections:
        head = sec.splitlines()[0] if sec.splitlines() else "?"
        leg = "glibc" if "glibc" in head else "mimir"
        m = re.search(
            r"STORM combined_wall_s=([\d.]+) churn_wall_s=([\d.]+) "
            r"worst_wave_ms=([\d.]+) ring_wall_s=([\d.]+) "
            r"fork_wall_s=([\d.]+) cmb_forks=(\d+) frk_alloc=(\d+) "
            r"frk_exec=(\d+) cross_total=(\d+) checksum=(\d+) "
            r"ring_checksum=(\d+) alloc_fail=(\d+) "
            r"pattern_fail=(\d+) create_fail=(\d+)", sec)
        res = re.search(r"BATTERY RESULT: (PASS|FAIL)", sec)
        fails = [ln for ln in sec.splitlines()
                 if ln.startswith("CHECK ") and " FAIL " in ln]
        print(f"REP {head}")
        if m:
            print(f"  combined_wall_s={m.group(1)} "
                  f"churn_wall_s={m.group(2)} "
                  f"worst_wave_ms={m.group(3)} "
                  f"ring_wall_s={m.group(4)} "
                  f"fork_wall_s={m.group(5)}")
            print(f"  cmb_forks={m.group(6)} frk_alloc={m.group(7)} "
                  f"frk_exec={m.group(8)} cross_total={m.group(9)} "
                  f"checksum={m.group(10)} "
                  f"ring_checksum={m.group(11)}")
            walls[leg].append(float(m.group(2)))
            checksums.add(int(m.group(10)))
            ring_checksums.add(int(m.group(11)))
            if m.group(12) != "0" or m.group(13) != "0" \
                    or m.group(14) != "0":
                print("  FAIL counters nonzero in STORM line")
                ok = False
        else:
            print("  STORM line MISSING")
            ok = False
        for key, pat in (
                ("combined", r"RETAINED_AFTER_COMBINED_BYTES (\d+)"),
                ("storm", r"RETAINED_AFTER_STORM_BYTES (\d+)"),
                ("ring", r"RETAINED_AFTER_RING_BYTES (\d+)"),
                ("fork", r"RETAINED_AFTER_FORK_BYTES (\d+)"),
                ("pre_trim", r"RETAINED_PRE_TRIM_BYTES (\d+)"),
                ("final", r"RETAINED_FINAL_BYTES (\d+)")):
            r = re.search(pat, sec)
            if r and leg == "mimir":
                print(f"  retained_{key}_bytes={r.group(1)}")
                retained[key].append(int(r.group(1)))
            elif leg == "mimir":
                print(f"  retained_{key}_bytes=MISSING")
                ok = False
        for ln in fails:
            print(f"  {ln}")
        print(f"  result={res.group(1) if res else 'MISSING'}")
        if not res or res.group(1) != "PASS":
            ok = False
    for leg, ws in walls.items():
        if ws:
            print(f"MEDIAN churn_wall_s leg={leg} "
                  f"{statistics.median(ws):.3f} (n={len(ws)})")
    for key, vals in retained.items():
        if vals:
            print(f"RETAINED {key} figures across mimir reps: {vals}")
    if len(checksums) == 1:
        print(f"CHURN CHECKSUM identical across all reps/legs: "
              f"{checksums.pop()}")
    else:
        print(f"CHURN CHECKSUM MISMATCH across reps/legs: "
              f"{sorted(checksums)}")
        ok = False
    if len(ring_checksums) == 1:
        print(f"RING CHECKSUM identical across all reps/legs: "
              f"{ring_checksums.pop()}")
    else:
        print(f"RING CHECKSUM MISMATCH across reps/legs: "
              f"{sorted(ring_checksums)}")
        ok = False
    finals = retained["final"]
    if finals and any(v != 0 for v in finals):
        print(f"RETAINED FINAL not zero in some rep: {finals}")
        ok = False
    print("SUMMARY RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
