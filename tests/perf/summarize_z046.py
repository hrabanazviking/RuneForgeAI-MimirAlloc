#!/usr/bin/env python3
"""summarize_z046.py - Z-046 threshold study summariser.
Reads the raw CSV written by run_z046.sh (cost lines) and
the threshold CSV (T1/TD/TS/TM lines), prints:
  - per-process medians by (phase, leg, size) with spread
    and drift (Z-004 s.8, bound 8%: above = DRIFT-REJECT,
    indicative only)
  - checksum discipline (exactly one checksum per
    endpoint-touch family per size; raw/notouch == 0;
    opfull family one per size)
  - the per-size cost table and per-decade summary
  - cutoff-option pricing under two reference mixes,
    modelling a size as reuse-served (glibc arena-forced
    proxy) when size <= candidate cutoff, else at the
    measured MimirAlloc mapping-path cost
  - the threshold tables verbatim
Usage: summarize_z046.py <raw-file> <thresh-file>
"""
import statistics
import sys
from collections import defaultdict

def median(xs):
    return statistics.median(xs)

def drift_pct(vals):
    if len(vals) < 4:
        return 0.0
    half = len(vals) // 2
    a, b = vals[:half], vals[half:]
    med = median(vals)
    if med == 0:
        return 0.0
    return 100.0 * abs(median(b) - median(a)) / med

def main():
    raw_path, thr_path = sys.argv[1], sys.argv[2]
    groups = defaultdict(list)
    checks = defaultdict(set)
    faults = {}
    with open(raw_path) as f:
        for line in f:
            parts = line.strip().split(",")
            if len(parts) != 9:
                continue
            phase, leg, size = parts[0], parts[1], int(parts[2])
            iters = int(parts[4])
            nspo = float(parts[6])
            minflt = int(parts[7])
            chk = int(parts[8])
            groups[(phase, leg, size)].append(nspo)
            checks[(phase, leg, size)].add(chk)
            faults.setdefault((phase, leg, size), []).append(
                minflt / iters if iters else 0.0)
    med = {k: median(v) for k, v in groups.items()}
    print("Z-046 threshold study summary (ns/op; headline = median of processes)")
    print("drift bound 8% (Z-004 s.8); DRIFT-REJECT = indicative only")
    print()
    print("=== per-process medians by group ===")
    for k in sorted(groups, key=lambda k: (k[0], k[2], k[1])):
        vals = groups[k]
        d = drift_pct(vals)
        flag = "  DRIFT-REJECT" if d > 8.0 else ""
        print(f"{k[0]:>7} {k[1]:<8} size={k[2]:>8}  med={med[k]:>12.2f}  "
              f"spread {min(vals):.2f}..{max(vals):.2f}  drift {d:.1f}%  "
              f"n={len(vals)}  checksums={sorted(checks[k])}{flag}")
    print()
    print("=== checksum discipline ===")
    ok = True
    sizes = sorted({k[2] for k in groups})
    for s in sizes:
        fam = set()
        for leg in ("mimir", "glibc", "glmap", "glarena", "gltrim"):
            fam |= checks.get(("op", leg, s), set())
        fam |= checks.get(("raw", "touch", s), set())
        status = "OK" if len(fam) == 1 else "FAIL"
        if len(fam) != 1:
            ok = False
        print(f"size {s}: endpoint-touch family {sorted(fam)} -> {status}")
        nt = checks.get(("raw", "notouch", s), set())
        if nt and nt != {0}:
            ok = False
            print(f"size {s}: raw/notouch checksums {sorted(nt)} != {{0}} -> FAIL")
    for s in sorted({k[2] for k in groups if k[0] == "opfull"}):
        fam = set()
        for leg in ("mimir", "glibc"):
            fam |= checks.get(("opfull", leg, s), set())
        status = "OK" if len(fam) == 1 else "FAIL"
        if len(fam) != 1:
            ok = False
        print(f"size {s}: opfull family {sorted(fam)} -> {status}")
    print("checksum verdict:", "ALL OK" if ok else "FAILED")
    print()
    print("=== PER-SIZE COST TABLE (endpoint touch, single block live) ===")
    print("pair = raw/notouch; faults = raw/touch - raw/notouch;")
    print("book = mimir - raw/touch (residual; at 1024 mimir is on")
    print("the span path, so the split is shown for contrast only);")
    print("waste = MimirAlloc mapping_length - request (reserve page")
    print("+ page round-up), from alloc_locked geometry.")
    hdr = (f"{'size':>9} {'mimir':>10} {'glibc':>9} {'glmap':>10} "
           f"{'glarena':>10} {'gltrim':>10} {'pair':>9} {'faults':>9} {'book':>9} "
           f"{'mim/gl':>8} {'mim/are':>8} {'flt/op':>7} {'wasteB':>8} {'waste%':>8}")
    print(hdr)
    def m(phase, leg, s):
        return med.get((phase, leg, s), float("nan"))
    for s in sizes:
        if ("op", "mimir", s) not in med:
            continue
        pair = m("raw", "notouch", s)
        rt = m("raw", "touch", s)
        waste = ((4096 + s + 4095) // 4096) * 4096 - s
        fl = median(faults.get(("op", "mimir", s), [0.0]))
        print(f"{s:>9} {m('op','mimir',s):>10.2f} {m('op','glibc',s):>9.2f} "
              f"{m('op','glmap',s):>10.2f} {m('op','glarena',s):>10.2f} "
              f"{m('op','gltrim',s):>10.2f} "
              f"{pair:>9.2f} {rt - pair:>9.2f} {m('op','mimir',s) - rt:>9.2f} "
              f"{m('op','mimir',s) / m('op','glibc',s):>8.2f} "
              f"{m('op','mimir',s) / m('op','glarena',s):>8.2f} "
              f"{fl:>7.3f} {waste:>8} {100.0 * waste / s:>8.2f}")
    print()
    print("=== PER-DECADE SUMMARY (mean of per-size medians, ns/op) ===")
    decades = [("1-8 KiB", 1024, 8192), ("16-64 KiB", 16384, 65536),
               ("128-512 KiB", 131072, 524288), ("1-4 MiB", 1048576, 4194304),
               ("8-32 MiB", 8388608, 33554432)]
    for name, lo, hi in decades:
        ss = [s for s in sizes if lo <= s <= hi and ("op", "mimir", s) in med]
        if not ss:
            continue
        row = []
        for leg in ("mimir", "glibc", "glmap", "glarena", "gltrim"):
            row.append(sum(m("op", leg, s) for s in ss) / len(ss))
        save = row[0] - row[3]
        print(f"{name:>12}: mimir {row[0]:>10.2f}  glibc {row[1]:>9.2f}  "
              f"glmap {row[2]:>10.2f}  glarena {row[3]:>10.2f}  "
              f"gltrim {row[4]:>10.2f}  "
              f"reuse saving vs mimir {save:>10.2f} ({100.0 * save / row[0]:.1f}%)")
    print()
    print("=== DENSE TOUCH (opfull: every user page written+read) ===")
    for s in sorted({k[2] for k in groups if k[0] == "opfull"}):
        mm, gg = m("opfull", "mimir", s), m("opfull", "glibc", s)
        fm = median(faults.get(("opfull", "mimir", s), [0.0]))
        fg = median(faults.get(("opfull", "glibc", s), [0.0]))
        print(f"size {s:>9}: mimir {mm:>12.2f}  glibc-steady {gg:>12.2f}  "
              f"ratio {mm / gg:>8.2f}x  faults/op mimir {fm:.1f} glibc {fg:.3f}")
    print()
    print("=== CUTOFF OPTIONS PRICED ===")
    print("model: size <= cutoff pays the glarena (reuse proxy)")
    print("median; size > cutoff pays the measured mimir median.")
    print("The proxy is glibc's arena, not a built MimirAlloc")
    print("reuse path: a real one pays MimirAlloc bookkeeping")
    print("on top. Direction and scale are the claim, not the")
    print("last digit.")
    ladder = [s for s in sizes if ("op", "mimir", s) in med]
    batch = [2048, 8192, 16384, 65536, 262144, 1048576]
    def mix_cost(cutoff, mix):
        tot = 0.0
        for s in mix:
            tot += m("op", "glarena", s) if s <= cutoff else m("op", "mimir", s)
        return tot / len(mix)
    base_u, base_b = mix_cost(1024, ladder), mix_cost(1024, batch)
    print(f"{'cutoff':>9} {'uniform-mix':>12} {'saving%':>8} {'large_batch-mix':>16} {'saving%':>8}")
    for c in (1024, 8192, 32768, 131072, 524288, 1048576, 4194304, 33554432):
        u, b = mix_cost(c, ladder), mix_cost(c, batch)
        print(f"{c:>9} {u:>12.2f} {100.0 * (base_u - u) / base_u:>8.1f} "
              f"{b:>16.2f} {100.0 * (base_b - b) / base_b:>8.1f}")
    print()
    print("=== GLIBC THRESHOLD TABLES (fresh process per verdict) ===")
    with open(thr_path) as f:
        for line in f:
            print(line.rstrip())

if __name__ == "__main__":
    main()
