#!/usr/bin/env python3
"""summarize_z006.py - Z-006 RSS/PSS summariser.

Reads run_rss_z006.py's raw CSV, computes per-(leg, workload,
phase) medians across processes, verifies the probe CSV
checksums against the Z-003 recorded per-pass checksums,
checks drift on PEAK Pss (Z-004 protocol section 8, 8%),
and prints the RSS/PSS table plus verdicts at PEAK and
AFTER_FREE_ALL using the protocol's 5% noise band
(section 7): inside 5% is TIE / WITHIN NOISE.

Usage: summarize_z006.py <raw.csv>
"""
import csv
import statistics
import sys
from collections import defaultdict

FIELDS = ["Rss", "Pss", "Pss_Anon", "Pss_File", "Private_Clean",
          "Private_Dirty", "Shared_Clean", "Shared_Dirty", "Swap"]
PHASES = ["START", "WARM", "STEADY", "PEAK", "AFTER_FREE",
          "AFTER_FREE_ALL"]
# Z-003 recorded per-pass checksums (recorded_run_z13_k002.txt).
EXPECT_CHK = {"small_churn": 12756656, "mixed_batch": 34727552,
              "large_batch": 1106008, "mixed_lifecycle": 40359936}
REPS_FOR = {"small_churn": 20, "mixed_batch": 5,
            "large_batch": 20, "mixed_lifecycle": 10}


def grade(mimir, glibc):
    if glibc == 0:
        return "NO-BASE"
    d = 100.0 * (mimir - glibc) / glibc
    if abs(d) <= 5.0:
        return f"TIE ({d:+.1f}%)"
    return f"{'WIN' if d < 0 else 'LOSS'} ({d:+.1f}%)"


def main():
    rows = list(csv.DictReader(open(sys.argv[1])))
    data = defaultdict(list)   # (leg, workload, phase) -> [row]
    live = {}
    problems = []
    for r in rows:
        key = (r["leg"], r["workload"], r["phase"])
        if r["phase"] == "LIVEBYTES":
            live[(r["leg"], r["workload"])] = int(r["Rss"])
        elif r["phase"] == "CSV":
            # The runner embeds the probe CSV line comma-joined
            # into this row, so DictReader spreads its 7 fields
            # (leg,workload,ops,elapsed,ops/s,ns/op,checksum)
            # across Rss..Shared_Clean; checksum is Shared_Clean.
            chk = int(r["Shared_Clean"])
            per = chk // REPS_FOR[r["workload"]]
            if chk % REPS_FOR[r["workload"]] or per != EXPECT_CHK[r["workload"]]:
                problems.append(
                    f"checksum mismatch {r['leg']} {r['workload']}: "
                    f"total {chk}, expect per-pass {EXPECT_CHK[r['workload']]}")
        else:
            data[key].append({k: int(r[k]) for k in FIELDS})
    med = {}
    for key, rs in data.items():
        med[key] = {k: int(statistics.median([r[k] for r in rs]))
                    for k in FIELDS}
    # Drift check: PEAK Pss first half vs second half of procs.
    for leg in ("glibc", "mimiralloc"):
        for w in EXPECT_CHK:
            rs = data.get((leg, w, "PEAK"), [])
            if len(rs) >= 4:
                half = len(rs) // 2
                a = statistics.median([r["Pss"] for r in rs[:half]])
                b = statistics.median([r["Pss"] for r in rs[half:]])
                if a and abs(b - a) / a * 100 > 8.0:
                    problems.append(
                        f"DRIFT-REJECT {leg} {w} PEAK Pss drift "
                        f"{abs(b - a) / a * 100:.1f}% > 8%")
    print("=== MEDIAN TABLE (kB, medians across processes) ===")
    hdr = ("leg", "workload", "phase", "live_B") + tuple(FIELDS)
    print(" ".join(f"{x:>14}" if i > 2 else f"{x:<14}" for i, x in enumerate(hdr)))
    for w in EXPECT_CHK:
        for leg in ("glibc", "mimiralloc"):
            for ph in PHASES:
                m = med.get((leg, w, ph))
                if not m:
                    continue
                lb = live.get((leg, w), 0) if ph in ("PEAK",) else ""
                print(f"{leg:<14} {w:<14} {ph:<14} {str(lb):>14} " +
                      " ".join(f"{m[k]:>14}" for k in FIELDS))
    print()
    print("=== VERDICTS (median Pss and Pss_Anon, mimiralloc vs glibc; "
          "5% band = TIE per Z-004 protocol s.7) ===")
    for w in EXPECT_CHK:
        lb = live.get(("glibc", w), 0)
        for ph in ("PEAK", "AFTER_FREE_ALL"):
            g, m = med[("glibc", w, ph)], med[("mimiralloc", w, ph)]
            print(f"{w} {ph}: live={lb} B | Pss glibc {g['Pss']} kB, "
                  f"mimir {m['Pss']} kB -> {grade(m['Pss'], g['Pss'])} | "
                  f"Pss_Anon glibc {g['Pss_Anon']} kB, mimir "
                  f"{m['Pss_Anon']} kB -> {grade(m['Pss_Anon'], g['Pss_Anon'])}")
    print()
    if problems:
        print("=== PROBLEMS ===")
        for p in problems:
            print(p)
    else:
        print("checksums: all probe CSV lines match Z-003 per-pass "
              "checksums x reps; drift: no DRIFT-REJECT (8% band)")


if __name__ == "__main__":
    main()
