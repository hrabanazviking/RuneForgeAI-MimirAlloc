#!/usr/bin/env python3
"""summarize_z069.py -- Z-069 canary economics summariser.

Reads the raw CSV written by run_z069.sh (one line per process:
leg,class_size,blocks,rounds,elapsed_ns,ns_per_op,checksum,
canary_faults), groups by (class, leg), takes the MEDIAN
ns_per_op (Z-004 headline), and prints:

  1. per-class cost table: base median, identical-leg control
     delta (base_dup vs base, bound 5%), can16/can32 median
     delta in ns/op and percent vs base;
  2. exact byte economics per class from the banked span
     geometry (page 4096 B, span header 96 B, usable 4000 B):
     slots per page for the production stride (class), the
     tail-16 stride (class+16) and the full J-004 stride
     (class+32), with capacity loss percent;
  3. per-class-group verdicts under the slice's stated rule:
       ADOPT-CANDIDATE: canary delta <= max(1.0 ns/op, 2% of
         base) AND stride capacity loss <= 10%
       REJECT: delta > max(3.0 ns/op, 5% of base) OR capacity
         loss > 10%
       otherwise MARGINAL (between the bands).
     The large group (descriptor path) has no span stride;
     its byte cost is the tail bytes inside the mapping,
     graded on time only.

Checksum agreement across legs within a class is required;
any mismatch or nonzero canary_faults is shouted as INVALID.
Usage: summarize_z069.py <raw-file>
"""
import statistics
import sys
from collections import defaultdict

GROUPS = [
    ("G1 tiny (16-64)", [16, 32, 48, 64]),
    ("G2 small (80-256)", [80, 96, 112, 128, 160, 192, 224, 256]),
    ("G3 medium (320-1024)",
     [320, 384, 448, 512, 640, 768, 896, 1024]),
    ("G4 large (>1024, mapping path)", []),
]
MEASURED_GROUP = {}
for name, classes in GROUPS[:3]:
    for c in classes:
        MEASURED_GROUP[c] = name
LARGE_GROUP = GROUPS[3][0]

USABLE = 4096 - 96


def slots(stride):
    return USABLE // stride


def grade(delta_ns, base, cap_loss_pct):
    """Return verdict string for one (form, class)."""
    pct = 100.0 * delta_ns / base if base else 0.0
    if delta_ns <= max(1.0, 0.02 * base) and cap_loss_pct <= 10.0:
        return "ADOPT-CANDIDATE"
    if delta_ns > max(3.0, 0.05 * base) or cap_loss_pct > 10.0:
        return "REJECT"
    return "MARGINAL"


def main():
    if len(sys.argv) != 2:
        print("usage: summarize_z069.py <raw-file>", file=sys.stderr)
        return 2
    cells = defaultdict(list)
    checksums = defaultdict(set)
    faults = 0
    with open(sys.argv[1]) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split(",")
            leg, cls = parts[0], int(parts[1])
            cells[(cls, leg)].append(float(parts[5]))
            checksums[cls].add(parts[6])
            faults += int(parts[7])
    classes = sorted({c for c, _ in cells})
    invalid = False
    for c in classes:
        if len(checksums[c]) != 1:
            print(f"INVALID: checksum mismatch at class {c}: "
                  f"{checksums[c]}")
            invalid = True
    if faults:
        print(f"INVALID: {faults} canary faults recorded")
        invalid = True

    print("== per-class time cost (median ns/op, alloc+free pair) ==")
    print(f"{'class':>6} {'base':>9} {'dup%':>7} {'can16':>9} "
          f"{'d_ns':>7} {'d%':>7} {'can32':>9} {'d_ns':>7} "
          f"{'d%':>7}")
    med = {}
    for c in classes:
        row = {}
        for leg in ("base", "base_dup", "can16", "can32"):
            vals = cells.get((c, leg), [])
            row[leg] = statistics.median(vals) if vals else float("nan")
        med[c] = row
        dup_pct = 100.0 * (row["base_dup"] - row["base"]) / row["base"]
        if abs(dup_pct) > 5.0:
            print(f"CONTROL-FAIL class {c}: identical-leg delta "
                  f"{dup_pct:+.2f}% exceeds 5% bound")
            invalid = True
        print(f"{c:>6} {row['base']:>9.3f} {dup_pct:>+7.2f} "
              f"{row['can16']:>9.3f} "
              f"{row['can16'] - row['base']:>+7.3f} "
              f"{100.0 * (row['can16'] - row['base']) / row['base']:>+7.2f} "
              f"{row['can32']:>9.3f} "
              f"{row['can32'] - row['base']:>+7.3f} "
              f"{100.0 * (row['can32'] - row['base']) / row['base']:>+7.2f}")

    print()
    print("== exact byte economics (page 4096, header 96, "
          "usable 4000) ==")
    print(f"{'class':>6} {'slots0':>7} {'slots+16':>9} {'loss%':>7} "
          f"{'slots+32':>9} {'loss%':>7}")
    cap_loss = {}
    for c in classes:
        if c > 1024:
            print(f"{c:>6}  mapping path: no span stride; tail "
                  f"canary bytes live inside the mapping")
            cap_loss[c] = (0.0, 0.0)
            continue
        s0, s16, s32 = slots(c), slots(c + 16), slots(c + 32)
        l16 = 100.0 * (s0 - s16) / s0
        l32 = 100.0 * (s0 - s32) / s0
        cap_loss[c] = (l16, l32)
        print(f"{c:>6} {s0:>7} {s16:>9} {l16:>7.2f} {s32:>9} "
              f"{l32:>7.2f}")

    print()
    print("== per-class verdicts ==")
    for c in classes:
        row = med[c]
        l16, l32 = cap_loss[c]
        v16 = grade(row["can16"] - row["base"], row["base"], l16)
        v32 = grade(row["can32"] - row["base"], row["base"], l32)
        grp = MEASURED_GROUP.get(c, LARGE_GROUP)
        print(f"class {c:>6} [{grp}] tail16={v16} full32={v32}")

    print()
    print("== per-group verdicts (worst measured class governs) ==")
    order = {"ADOPT-CANDIDATE": 0, "MARGINAL": 1, "REJECT": 2}
    for name, gclasses in GROUPS:
        reps = [c for c in classes
                if (MEASURED_GROUP.get(c) == name or
                    (name == LARGE_GROUP and c > 1024))]
        if not reps:
            continue
        worst16 = max((grade(med[c]["can16"] - med[c]["base"],
                             med[c]["base"], cap_loss[c][0])
                       for c in reps), key=lambda v: order[v])
        worst32 = max((grade(med[c]["can32"] - med[c]["base"],
                             med[c]["base"], cap_loss[c][1])
                       for c in reps), key=lambda v: order[v])
        print(f"{name}: tail16={worst16} full32={worst32} "
              f"(classes {reps})")

    if invalid:
        print("RESULT: INVALID (see above)")
        return 1
    print("RESULT: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
