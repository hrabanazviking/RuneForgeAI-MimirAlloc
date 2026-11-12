#!/usr/bin/env python3
"""summarize_z025.py - Z-025 calloc/realloc anatomy summarizer.

Usage: summarize_z025.py <mimir.csv> <glibc.csv>

Per Z-004: headline = MEDIAN of per-process ns/op; min/max
printed as spread; split-half drift per (leg, workload,
size) with the 8% bound flagged; checksum equivalence -
exactly one REALLOC checksum per (from,to) pair across
both legs and all processes, and every VERIFY /
VERIFYBULK nonzero count must be 0. Grades per size,
mimir vs glibc: inside 5% = TIE (within noise), mimir
faster by >5% = WIN, slower by >5% = LOSS. Cliff scan:
adjacent-size per-byte cost (ns/byte) step > 25% in one
leg is printed as CLIFF-CANDIDATE with its size.
"""
import sys
from statistics import median

NOISE = 5.0
DRIFT = 8.0


def load(path):
    rows = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("leg:"):
                continue
            parts = line.split(",")
            rows.append(parts)
    return rows


def med(vals):
    return median(vals)


def drift_pct(vals):
    if len(vals) < 4:
        return 0.0
    h = len(vals) // 2
    a, b = vals[:h], vals[-h:]
    m = median(vals)
    if m == 0:
        return 0.0
    return 100.0 * abs(median(b) - median(a)) / m


def main():
    mimir = load(sys.argv[1])
    glibc = load(sys.argv[2])
    allrows = mimir + glibc
    bad = 0

    # ---- verification tallies ----
    nz = [r for r in allrows if r[0] in ("VERIFY", "VERIFYBULK")
          and int(r[3]) != 0]
    done = [r for r in allrows if r[0] == "DONE"]
    print(f"VERIFY lines: {sum(1 for r in allrows if r[0] in ('VERIFY','VERIFYBULK'))}"
          f"  nonzero-byte lines: {len(nz)}")
    for r in nz:
        print("  NONZERO:", ",".join(r))
        bad = 1
    for r in done:
        if r[2] != "0":
            print("  PROBE BAD FLAG:", ",".join(r))
            bad = 1

    # ---- timed tables ----
    for mode in ("CALLOC", "MALLOC", "MALLOCSET", "BULK"):
        print(f"\n=== {mode}: median ns/op (mimir vs glibc) ===")
        print(f"{'size':>8} {'mimir':>10} {'glibc':>10} {'ratio':>7} "
              f"{'verdict':>7} {'m_spread':>15} {'g_spread':>15} {'drift%':>7}")
        sizes = sorted({int(r[2]) for r in allrows if r[0] == mode})
        prev = {}
        for s in sizes:
            mv = [float(r[4]) for r in mimir if r[0] == mode and int(r[2]) == s]
            gv = [float(r[4]) for r in glibc if r[0] == mode and int(r[2]) == s]
            if not mv or not gv:
                print(f"{s:>8} MISSING LEG")
                bad = 1
                continue
            mm, gm = med(mv), med(gv)
            ratio = mm / gm if gm else 0.0
            diff = 100.0 * (mm - gm) / gm if gm else 0.0
            verdict = "TIE" if abs(diff) <= NOISE else ("WIN" if diff < 0 else "LOSS")
            d = max(drift_pct(mv), drift_pct(gv))
            dflag = " DRIFT-REJECT" if d > DRIFT else ""
            if dflag:
                bad = 1
            print(f"{s:>8} {mm:>10.2f} {gm:>10.2f} {ratio:>7.3f} "
                  f"{verdict:>7} {min(mv):>6.1f}..{max(mv):<8.1f} "
                  f"{min(gv):>6.1f}..{max(gv):<8.1f} {d:>7.1f}{dflag}")
            # cliff scan on per-byte cost, both legs
            for leg, val in (("mimir", mm), ("glibc", gm)):
                pb = val / s
                if leg in prev:
                    ps, ppb = prev[leg]
                    if ppb > 0 and abs(pb - ppb) / ppb > 0.25:
                        print(f"  CLIFF-CANDIDATE {mode} leg={leg} "
                              f"at size {s} (prev {ps}: {ppb:.4f} ns/B, "
                              f"now {pb:.4f} ns/B)")
                prev[leg] = (s, pb)
        prev = {}

    # calloc premium tables
    print("\n=== zeroing premium: CALLOC vs MALLOC and vs MALLOCSET "
          "(median ns/op, per leg) ===")
    for legname, rows in (("mimir", mimir), ("glibc", glibc)):
        print(f"-- {legname} --")
        print(f"{'size':>8} {'calloc':>10} {'malloc':>10} {'prem_vs_malloc':>14} "
              f"{'mallocset':>10} {'calloc_vs_set':>13}")
        sizes = sorted({int(r[2]) for r in rows if r[0] == "CALLOC"})
        for s in sizes:
            c = med([float(r[4]) for r in rows if r[0] == "CALLOC" and int(r[2]) == s])
            m = med([float(r[4]) for r in rows if r[0] == "MALLOC" and int(r[2]) == s])
            ms = med([float(r[4]) for r in rows if r[0] == "MALLOCSET" and int(r[2]) == s])
            print(f"{s:>8} {c:>10.2f} {m:>10.2f} {c - m:>14.2f} "
                  f"{ms:>10.2f} {c - ms:>13.2f}")

    # ---- realloc ladder ----
    print("\n=== REALLOC: median ns/op, moved fraction, checksum ===")
    print(f"{'from':>8} {'to':>8} {'mimir':>10} {'glibc':>10} {'verdict':>7} "
          f"{'m_moved':>8} {'g_moved':>8} {'cksum_set':>10} {'drift%':>7}")
    pairs = sorted({(int(r[2]), int(r[3])) for r in allrows if r[0] == "REALLOC"})
    for (fr, to) in pairs:
        mv = [r for r in mimir if r[0] == "REALLOC" and int(r[2]) == fr and int(r[3]) == to]
        gv = [r for r in glibc if r[0] == "REALLOC" and int(r[2]) == fr and int(r[3]) == to]
        if not mv or not gv:
            print(f"{fr:>8} {to:>8} MISSING LEG")
            bad = 1
            continue
        mm = med([float(r[5]) for r in mv])
        gm = med([float(r[5]) for r in gv])
        mfrac = sum(int(r[6]) for r in mv) / sum(int(r[4]) for r in mv)
        gfrac = sum(int(r[6]) for r in gv) / sum(int(r[4]) for r in gv)
        ck = {r[7] for r in mv + gv}
        diff = 100.0 * (mm - gm) / gm if gm else 0.0
        verdict = "TIE" if abs(diff) <= NOISE else ("WIN" if diff < 0 else "LOSS")
        d = max(drift_pct([float(r[5]) for r in mv]),
                drift_pct([float(r[5]) for r in gv]))
        dflag = " DRIFT-REJECT" if d > DRIFT else ""
        if len(ck) != 1:
            bad = 1
        if dflag:
            bad = 1
        print(f"{fr:>8} {to:>8} {mm:>10.2f} {gm:>10.2f} {verdict:>7} "
              f"{mfrac:>8.2f} {gfrac:>8.2f} {len(ck):>10} {d:>7.1f}{dflag}"
              f"{' CKSUM-MISMATCH' if len(ck) != 1 else ''}")

    print(f"\nPROTOCOL GATE: {'FAIL' if bad else 'PASS'}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
