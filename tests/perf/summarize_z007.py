#!/usr/bin/env python3
"""summarize_z007.py - Z-007 energy summariser.

Reads the raw file produced by run_energy_z007.sh and prints:
  - idle package power samples and their median (the idle
    baseline of the idle-baseline subtraction method)
  - per (leg, workload): per-process gross package J, wall s,
    idle-subtracted net J, net J/op and gross J/op; medians
  - verdicts: median net J/op ratio mimiralloc/glibc with the
    energy grade band WIN < 0.95, TIE 0.95..1.05, LOSS > 1.05
    (band stated here and in the recorded run; energy noise on
    a shared package is larger than timing noise, so a band
    narrower than the Z-004 5% timing bound would be fiction)
  - checksum proof: each process's total checksum must equal
    reps x the Z-003 recorded per-pass checksum, proving the
    workloads are the banked ones and equivalent across legs.

Usage: summarize_z007.py <raw-file>
"""
import re
import statistics
import sys

EXPECTED_CHK = {
    "small_churn": 12756656,
    "mixed_batch": 34727552,
    "large_batch": 1106008,
    "mixed_lifecycle": 40359936,
}

blocks = []  # (leg, workload, proc, reps, wall_ns, csv, events)
cur = None
idle_wall = {}
with open(sys.argv[1]) as f:
    for line in f:
        line = line.rstrip("\n")
        m = re.match(r"=== (\S+) (\S+) proc=(\d+) reps=(\d+) wall_ns=(\d+) ===", line)
        if m:
            cur = {"leg": m.group(1), "workload": m.group(2),
                   "proc": int(m.group(3)), "reps": int(m.group(4)),
                   "wall_ns": int(m.group(5)), "csv": None, "events": {}}
            blocks.append(cur)
            continue
        m = re.match(r"=== idle sleep sample=(\d+) reps=0 ===", line)
        if m:
            cur = {"leg": "idle", "workload": "sleep",
                   "proc": int(m.group(1)), "reps": 0,
                   "wall_ns": None, "csv": None, "events": {}}
            blocks.append(cur)
            continue
        m = re.match(r"idle_wall_ns=(\d+)", line)
        if m and cur is not None:
            cur["wall_ns"] = int(m.group(1))
            continue
        if cur is None:
            continue
        if "," in line and line.split(",")[0] in ("glibc", "mimiralloc"):
            parts = line.split(",")
            cur["csv"] = {"ops": int(parts[2]), "elapsed_ns": int(parts[3]),
                          "chk": int(parts[6])}
            continue
        if "|" in line:
            parts = line.split("|")
            if len(parts) >= 3 and parts[0] not in ("<not supported>",):
                try:
                    cur["events"][parts[2]] = float(parts[0])
                except ValueError:
                    pass
            elif parts[0] == "<not supported>":
                cur["events"][parts[2]] = None

idle_w = []
for b in blocks:
    if b["leg"] == "idle":
        j = b["events"].get("power/energy-pkg/")
        if j is None or not b["wall_ns"]:
            print(f"IDLE sample {b['proc']}: UNSUPPORTED/missing - INVALID")
            continue
        w = j / (b["wall_ns"] / 1e9)
        idle_w.append(w)
        print(f"IDLE sample {b['proc']}: {j:.3f} J over {b['wall_ns']/1e9:.3f} s = {w:.3f} W")
if not idle_w:
    print("NO VALID IDLE SAMPLES - cannot subtract baseline")
    sys.exit(1)
idle_med = statistics.median(idle_w)
print(f"IDLE baseline: median {idle_med:.3f} W "
      f"(min {min(idle_w):.3f}, max {max(idle_w):.3f}, n={len(idle_w)})")
print()

rows = {}
bad_chk = 0
for b in blocks:
    if b["leg"] == "idle":
        continue
    key = (b["leg"], b["workload"])
    gross = b["events"].get("power/energy-pkg/")
    core = b["events"].get("power_core/energy-core/")
    if gross is None or b["csv"] is None:
        print(f"BAD BLOCK {key} proc={b['proc']}: missing energy or CSV")
        bad_chk += 1
        continue
    wall_s = b["wall_ns"] / 1e9
    net = gross - idle_med * wall_s
    exp = EXPECTED_CHK[b["workload"]] * b["reps"]
    ok = "OK" if b["csv"]["chk"] == exp else "MISMATCH"
    if ok != "OK":
        bad_chk += 1
    print(f"{b['leg']:10s} {b['workload']:16s} proc={b['proc']} "
          f"gross={gross:9.3f} J core={core if core is not None else float('nan'):9.3f} J "
          f"wall={wall_s:7.3f} s net={net:9.3f} J "
          f"net_J/op={net / b['csv']['ops'] * 1e9:12.2f} nJ "
          f"gross_J/op={gross / b['csv']['ops'] * 1e9:12.2f} nJ chk={ok}")
    rows.setdefault(key, []).append(
        (net / b["csv"]["ops"] * 1e9, gross / b["csv"]["ops"] * 1e9, net, gross))
print()
print(f"{'leg':10s} {'workload':16s} {'n':>2s} {'med_net_nJ/op':>14s} "
      f"{'min':>10s} {'max':>10s} {'med_gross_nJ/op':>16s}")
med = {}
for key in sorted(rows):
    vals = [r[0] for r in rows[key]]
    gvals = [r[1] for r in rows[key]]
    med[key] = statistics.median(vals)
    print(f"{key[0]:10s} {key[1]:16s} {len(vals):2d} "
          f"{statistics.median(vals):14.2f} {min(vals):10.2f} {max(vals):10.2f} "
          f"{statistics.median(gvals):16.2f}")
print()
print("=== VERDICTS (median net J/op, mimiralloc vs glibc; "
      "WIN < 0.95x, TIE 0.95-1.05x, LOSS > 1.05x) ===")
for w in ("small_churn", "mixed_batch", "large_batch", "mixed_lifecycle"):
    g = med.get(("glibc", w))
    m = med.get(("mimiralloc", w))
    if g is None or m is None:
        print(f"{w}: MISSING LEG - NO VERDICT")
        continue
    if g <= 0:
        print(f"{w}: glibc median net J/op <= 0 ({g:.2f} nJ) - BELOW IDLE-"
              f"SUBTRACTION RESOLUTION, no ratio claimed")
        continue
    ratio = m / g
    grade = "WIN" if ratio < 0.95 else ("TIE" if ratio <= 1.05 else "LOSS")
    print(f"{w:16s} glibc median {g:12.2f} nJ/op | mimiralloc median "
          f"{m:12.2f} nJ/op | factor {ratio:7.2f}x | {grade}")
print()
print(f"checksum mismatches / bad blocks: {bad_chk} (must be 0)")
