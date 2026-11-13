#!/usr/bin/env python3
"""summarize_z107.py - Z-107 closing head-to-head summariser.

Modes (each reads one leg's raw data produced by run_z107.sh
/ run_rss_z107.py and prints that leg's closing tables):

  latency  raw1 raw2 reps
      bench_k002 CSV (config,workload,ops,elapsed_ns,
      ops_per_sec,ns_per_op,checksum), configs gdefault,
      gdefault_dup (identical-leg control), gmm32k,
      gtmax64k, gcombo_cache, mimir. Prints the invocation-1
      matrix, the control verdict (Z-004 5% bound), per-
      workload verdicts of MimirAlloc vs default glibc AND
      vs the tuned column, and the CLOSING TABLE against
      the Z-009 baseline (Z-003 medians). The tuned column
      per workload is FIXED by Z-098 invocation 1 of record:
      small_churn=gmm32k, mixed_batch=gtmax64k,
      large_batch=gcombo_cache, mixed_lifecycle=
      gcombo_cache. Hard FAIL (exit 1) on any missing cell,
      wrong rep count, ops/checksum mismatch, control
      breach, or cross-invocation drift beyond 8% on the
      headline columns (default / tuned / mimir).
  counters raw
      probe_z005 perf-stat blocks (=== leg workload proc=
      reps= ===), legs glibc / glibc_tuned / mimiralloc.
      Median counters per executed op (warm-up + timed,
      the Z-005 normalisation) + verdicts on cycles/op and
      instructions/op vs both glibc columns (5% band).
  rss raw.csv
      run_rss_z107.py output (Z-006 phase protocol).
      Median Pss / Pss_Anon tables at PEAK and
      AFTER_FREE_ALL + verdicts vs both glibc columns
      (5% band, lower is better), checksum + drift proofs.
  energy raw
      probe_z007 RAPL blocks (Z-007 idle-subtraction
      method). Median net J/op per leg + verdicts vs both
      glibc columns (WIN < 0.95x, TIE 0.95-1.05x, LOSS
      > 1.05x on median net J/op); a reference median at
      or below zero is BELOW IDLE-SUBTRACTION RESOLUTION
      and claims no ratio (the Z-007 honesty rule), with
      the gross medians reported for direction.

Grades everywhere: WIN / TIE / LOSS from MimirAlloc's side
on the Z-004 5% band unless a mode states its own band.
Baseline context numbers are the Z-009 document's
(docs/Z13_WAVE01_BASELINE.md), cited per mode; they are
context, never re-graded - the Z-003/Z-005/Z-006/Z-007
windows carried their own recorded load states.
"""
import csv
import re
import statistics
import sys
from collections import defaultdict

WORKLOADS = ["small_churn", "mixed_batch", "large_batch", "mixed_lifecycle"]
Z003_CHK = {"small_churn": 12756656, "mixed_batch": 34727552,
            "large_batch": 1106008, "mixed_lifecycle": 40359936}
PASS_OPS = {"small_churn": 100000, "mixed_batch": 256000,
            "large_batch": 9600, "mixed_lifecycle": 307200}
WARM_OPS = {"small_churn": 10000, "mixed_batch": 25600,
            "large_batch": 960, "mixed_lifecycle": 30720}


def grade(m, g):
    """(factor, verdict) for lower-is-better m vs reference g, 5% band."""
    if g <= 0:
        return None, "NO GRADE (reference <= 0)"
    f = m / g
    v = "WIN" if f < 0.95 else ("TIE" if f <= 1.05 else "LOSS")
    return f, v


# ---------------- latency ----------------
LAT_CONFIGS = [
    ("gdefault", ""),
    ("gdefault_dup", ""),
    ("gmm32k", "glibc.malloc.mmap_threshold=32768"),
    ("gtmax64k", "glibc.malloc.tcache_max=65536"),
    ("gcombo_cache", "glibc.malloc.tcache_count=64:glibc.malloc.tcache_max=65536"),
    ("mimir", "(MimirAlloc leg; GLIBC_TUNABLES not applied)"),
]
LAT_TUNED_FOR = {"small_churn": "gmm32k", "mixed_batch": "gtmax64k",
                 "large_batch": "gcombo_cache",
                 "mixed_lifecycle": "gcombo_cache"}
LAT_TUNABLES = dict(LAT_CONFIGS)
# Z-009 baseline of record, section 2 (Z-003 medians, ns/op).
BASE_LAT = {
    "glibc": {"small_churn": 9.02, "mixed_batch": 9.90,
              "large_batch": 1295.68, "mixed_lifecycle": 13.38},
    "mimiralloc": {"small_churn": 93.43, "mixed_batch": 398.42,
                   "large_batch": 3064.69, "mixed_lifecycle": 898.76},
}


def lat_parse(path):
    cells = defaultdict(list)
    ops_seen = {}
    chk_seen = defaultdict(set)
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or "," not in line:
                continue
            p = line.split(",")
            if len(p) != 7 or p[0] not in LAT_TUNABLES:
                continue
            cfg, w = p[0], p[1]
            cells[(cfg, w)].append(float(p[5]))
            ops_seen.setdefault(w, set()).add(int(p[2]))
            chk_seen[w].add(int(p[6]))
    return cells, ops_seen, chk_seen


def lat_medians(cells):
    return {k: statistics.median(v) for k, v in cells.items()}


def mode_latency(raw1, raw2, reps):
    fail = []
    c1, ops1, chk1 = lat_parse(raw1)
    c2, ops2, chk2 = lat_parse(raw2)
    for cells, ops, chk, tag in ((c1, ops1, chk1, "inv1"),
                                 (c2, ops2, chk2, "inv2")):
        for cfg, _ in LAT_CONFIGS:
            for w in WORKLOADS:
                n = len(cells.get((cfg, w), []))
                if n != reps:
                    fail.append(f"{tag} {cfg} {w}: {n} reps != {reps}")
        for w in WORKLOADS:
            if len(ops.get(w, set())) != 1:
                fail.append(f"{tag} {w}: ops mismatch {ops.get(w)}")
            if chk.get(w) != {Z003_CHK[w]}:
                fail.append(f"{tag} {w}: checksums {chk.get(w)} != "
                            f"{{{Z003_CHK[w]}}}")
    m1, m2 = lat_medians(c1), lat_medians(c2)
    print("=== Z-107 LATENCY MATRIX (invocation 1) - median ns/op, "
          "delta % vs default glibc ===")
    hdr = f"{'config':<14}" + "".join(f"{w:>18}" for w in WORKLOADS)
    print(hdr)
    for cfg, _ in LAT_CONFIGS:
        row = f"{cfg:<14}"
        for w in WORKLOADS:
            v = m1.get((cfg, w))
            d = m1.get(("gdefault", w))
            if v is None or not d:
                row += f"{'MISSING':>18}"
            else:
                row += f"{v:>9.2f} ({(v / d - 1) * 100:+6.2f}%)"
        print(row)
    print()
    print("=== IDENTICAL-LEG CONTROL (Z-004 bound: 5%) ===")
    worst = 0.0
    for w in WORKLOADS:
        a, b = m1[("gdefault", w)], m1[("gdefault_dup", w)]
        ag = abs(b / a - 1) * 100
        worst = max(worst, ag)
        print(f"{w:<16} default={a:10.2f} dup={b:10.2f} agreement={ag:.2f}%")
    print(f"worst agreement: {worst:.2f}% (bound 5.00%) - "
          f"{'PASS' if worst <= 5.0 else 'FAIL'}")
    if worst > 5.0:
        fail.append(f"control breach {worst:.2f}% > 5%")
    print()
    print("=== CROSS-INVOCATION AGREEMENT (drift bound: 8%, "
          "headline columns) ===")
    for w in WORKLOADS:
        for cfg in ("gdefault", LAT_TUNED_FOR[w], "mimir"):
            a, b = m1[(cfg, w)], m2[(cfg, w)]
            d = abs(b / a - 1) * 100
            ok = "OK" if d <= 8.0 else "DRIFT-FAIL"
            if d > 8.0:
                fail.append(f"drift {cfg} {w}: {d:.2f}% > 8%")
            print(f"{w:<16} {cfg:<14} inv1={a:10.2f} inv2={b:10.2f} "
                  f"drift={d:5.2f}% {ok}")
    print()
    print("=== CLOSING TABLE - LATENCY (median ns/op, invocation 1) ===")
    print("Z-009 baseline: docs/Z13_WAVE01_BASELINE.md section 2 "
          "(Z-003, contended window - context, not re-graded).")
    for w in WORKLOADS:
        tun = LAT_TUNED_FOR[w]
        gd, gt, mm = m1[("gdefault", w)], m1[(tun, w)], m1[("mimir", w)]
        fd, vd = grade(mm, gd)
        ft, vt = grade(mm, gt)
        ftu, vtu = grade(gt, gd)
        chg = (mm / BASE_LAT["mimiralloc"][w] - 1) * 100
        print(f"{w}:")
        print(f"  Z-009 baseline   glibc {BASE_LAT['glibc'][w]:10.2f} | "
              f"mimir {BASE_LAT['mimiralloc'][w]:10.2f}")
        print(f"  Z-107 default    glibc {gd:10.2f}")
        print(f"  Z-107 tuned      glibc {gt:10.2f} [{tun}: "
              f"{LAT_TUNABLES[tun]}] tuning factor {ftu:.3f}x {vtu} "
              f"for glibc")
        print(f"  Z-107 final      mimir {mm:10.2f} "
              f"(vs its Z-009 baseline {chg:+.1f}%)")
        print(f"  VERDICT vs default: {fd:.2f}x {vd} | "
              f"vs tuned: {ft:.2f}x {vt}")
    print()
    if fail:
        print("LATENCY RESULT: FAIL - " + "; ".join(fail))
        return 1
    print("LATENCY RESULT: PASS (checksums, control, drift all green)")
    return 0


# ---------------- counters ----------------
EVENTS = ["cycles", "instructions", "cache-misses", "branch-misses"]
# Z-009 baseline of record, section 4 (Z-005 medians per op).
BASE_CNT = {
    "glibc": {"small_churn": (17.826, 82.990),
              "mixed_batch": (18.306, 100.611),
              "large_batch": (3512.486, 8820.614),
              "mixed_lifecycle": (35.280, 208.216)},
    "mimiralloc": {"small_churn": (175.653, 861.968),
                   "mixed_batch": (1173.776, 3125.603),
                   "large_batch": (14005.627, 32076.006),
                   "mixed_lifecycle": (4374.607, 9550.162)},
}


def mode_counters(path):
    blocks, cur = [], None
    with open(path) as f:
        for line in f:
            line = line.strip()
            if line.startswith("=== "):
                if cur:
                    blocks.append(cur)
                p = line.strip("= ").split()
                cur = {"leg": p[0], "workload": p[1],
                       "reps": int(p[3].split("=")[1]),
                       "counters": {}, "pct": {}}
            elif cur is not None and "|" in line:
                q = line.split("|")
                if len(q) >= 5 and q[2] in EVENTS:
                    raw = q[0].replace(",", "")
                    cur["counters"][q[2]] = (
                        None if raw in ("<not counted>",
                                        "<not supported>", "")
                        else int(raw))
                    cur["pct"][q[2]] = q[4]
            elif cur is not None and "," in line and line[0].isalpha():
                q = line.split(",")
                if len(q) == 7:
                    cur["ops"] = int(q[2])
                    cur["ns_op"] = float(q[5])
                    cur["checksum"] = int(q[6])
    if cur:
        blocks.append(cur)
    problems, groups = [], defaultdict(list)
    for b in blocks:
        key = (b["leg"], b["workload"])
        groups[key].append(b)
        if b.get("checksum") != b["reps"] * Z003_CHK[b["workload"]]:
            problems.append(f"checksum mismatch {key}")
        if b.get("ops") != b["reps"] * PASS_OPS[b["workload"]]:
            problems.append(f"ops mismatch {key}")
        for e in EVENTS:
            if b["counters"].get(e) is None:
                problems.append(f"counter missing {key} {e}")
            elif b["pct"].get(e) not in ("100.00", ""):
                problems.append(f"counter scaled {key} {e} "
                                f"{b['pct'].get(e)}%")
    print(f"blocks parsed: {len(blocks)} (expect 60)")
    med = {}
    print(f"{'leg':<11} {'workload':<16} {'cyc/op':>11} {'ins/op':>11} "
          f"{'cmiss/op':>10} {'bmiss/op':>10} {'ns/op':>10}")
    for leg in ("glibc", "glibc_tuned", "mimiralloc"):
        for w in WORKLOADS:
            bs = groups.get((leg, w), [])
            if not bs:
                problems.append(f"no blocks {leg} {w}")
                continue
            tot = [b["reps"] * (PASS_OPS[w] + WARM_OPS[w]) for b in bs]
            vals = {}
            for e in EVENTS:
                vals[e] = statistics.median(
                    [b["counters"][e] / t for b, t in zip(bs, tot)])
            ns = statistics.median([b["ns_op"] for b in bs])
            med[(leg, w)] = vals
            print(f"{leg:<11} {w:<16} {vals['cycles']:>11.3f} "
                  f"{vals['instructions']:>11.3f} "
                  f"{vals['cache-misses']:>10.5f} "
                  f"{vals['branch-misses']:>10.5f} {ns:>10.2f}")
    print()
    print("=== COUNTER VERDICTS (median per-op, 5% band) ===")
    print("Z-009 baseline (Z-005): " + "; ".join(
        f"{w} cyc gl {BASE_CNT['glibc'][w][0]:.2f} / mi "
        f"{BASE_CNT['mimiralloc'][w][0]:.2f}, ins gl "
        f"{BASE_CNT['glibc'][w][1]:.2f} / mi "
        f"{BASE_CNT['mimiralloc'][w][1]:.2f}" for w in WORKLOADS))
    for w in WORKLOADS:
        for ev in ("cycles", "instructions"):
            mm = med[("mimiralloc", w)][ev]
            fd, vd = grade(mm, med[("glibc", w)][ev])
            ft, vt = grade(mm, med[("glibc_tuned", w)][ev])
            print(f"{w:<16} {ev:<13} mimir {mm:>11.3f} | vs default "
                  f"{fd:.2f}x {vd} | vs tuned {ft:.2f}x {vt}")
    print()
    if problems:
        print("COUNTERS RESULT: FAIL - " + "; ".join(sorted(set(problems))))
        return 1
    print("COUNTERS RESULT: PASS (all blocks checksum-true, "
          "counters 100% running)")
    return 0


# ---------------- rss ----------------
RSS_FIELDS = ["Rss", "Pss", "Pss_Anon", "Pss_File", "Private_Clean",
              "Private_Dirty", "Shared_Clean", "Shared_Dirty", "Swap"]
RSS_PHASES = ["START", "WARM", "STEADY", "PEAK", "AFTER_FREE",
              "AFTER_FREE_ALL"]
RSS_REPS = {"small_churn": 20, "mixed_batch": 5,
            "large_batch": 20, "mixed_lifecycle": 10}


def grade_pct(m, g):
    d = (m / g - 1) * 100 if g else 0.0
    v = "WIN" if d < -5.0 else ("TIE" if d <= 5.0 else "LOSS")
    return v, d


def mode_rss(path):
    rows = list(csv.DictReader(open(path)))
    data, live, problems = defaultdict(list), {}, []
    for r in rows:
        if r["phase"] == "LIVEBYTES":
            live[(r["leg"], r["workload"])] = int(r["Rss"])
        elif r["phase"] == "CSV":
            chk = int(r["Shared_Clean"])
            reps = RSS_REPS[r["workload"]]
            if chk % reps or chk // reps != Z003_CHK[r["workload"]]:
                problems.append(f"checksum mismatch {r['leg']} "
                                f"{r['workload']}")
        else:
            data[(r["leg"], r["workload"], r["phase"])].append(
                {k: int(r[k]) for k in RSS_FIELDS})
    med = {k: {f: int(statistics.median([x[f] for x in rs]))
               for f in RSS_FIELDS} for k, rs in data.items()}
    for leg in ("glibc", "glibc_tuned", "mimiralloc"):
        for w in WORKLOADS:
            rs = data.get((leg, w, "PEAK"), [])
            if len(rs) >= 4:
                half = len(rs) // 2
                a = statistics.median([x["Pss"] for x in rs[:half]])
                b = statistics.median([x["Pss"] for x in rs[half:]])
                if a and abs(b - a) / a * 100 > 8.0:
                    problems.append(f"DRIFT-REJECT {leg} {w} PEAK Pss "
                                    f"{abs(b - a) / a * 100:.1f}% > 8%")
    print("=== RSS MEDIAN TABLE (kB, medians of 5 processes; "
          "PEAK and AFTER_FREE_ALL) ===")
    for w in WORKLOADS:
        for leg in ("glibc", "glibc_tuned", "mimiralloc"):
            for ph in ("PEAK", "AFTER_FREE_ALL"):
                m = med.get((leg, w, ph))
                if not m:
                    problems.append(f"missing {leg} {w} {ph}")
                    continue
                lb = live.get((leg, w), 0) if ph == "PEAK" else ""
                print(f"{leg:<11} {w:<16} {ph:<15} live={str(lb):>9} "
                      f"Pss={m['Pss']:>7} Pss_Anon={m['Pss_Anon']:>7} "
                      f"Rss={m['Rss']:>7}")
    print()
    print("=== RSS VERDICTS (median Pss and Pss_Anon, 5% band, "
          "lower is better) ===")
    for w in WORKLOADS:
        lb = live.get(("glibc", w), 0)
        for ph in ("PEAK", "AFTER_FREE_ALL"):
            for field in ("Pss", "Pss_Anon"):
                mm = med[("mimiralloc", w, ph)][field]
                gd = med[("glibc", w, ph)][field]
                gt = med[("glibc_tuned", w, ph)][field]
                vd, dd = grade_pct(mm, gd)
                vt, dt = grade_pct(mm, gt)
                print(f"{w} {ph} {field}: live={lb} B | default "
                      f"{gd} kB, tuned {gt} kB, mimir {mm} kB -> "
                      f"vs default {vd} ({dd:+.1f}%) | vs tuned "
                      f"{vt} ({dt:+.1f}%)")
    print()
    print("Z-009 baseline (Z-006, docs/Z13_WAVE01_BASELINE.md s.5): "
          "PEAK TIE small_churn / large_batch, LOSS mixed_batch "
          "(+23.3% Pss) / mixed_lifecycle (+40.2%); AFTER_FREE_ALL "
          "WIN on 3 of 4, mixed_batch LOSS at both points.")
    if problems:
        print("RSS RESULT: FAIL - " + "; ".join(sorted(set(problems))))
        return 1
    print("RSS RESULT: PASS (checksums true, no drift-reject)")
    return 0


# ---------------- energy ----------------
def mode_energy(path):
    blocks, cur = [], None
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            m = re.match(r"=== (\S+) (\S+) proc=(\d+) reps=(\d+) "
                         r"wall_ns=(\d+) ===", line)
            if m:
                cur = {"leg": m.group(1), "workload": m.group(2),
                       "proc": int(m.group(3)), "reps": int(m.group(4)),
                       "wall_ns": int(m.group(5)), "csv": None,
                       "events": {}}
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
            if "," in line and line.split(",")[0] in ("glibc",
                                                      "mimiralloc"):
                p = line.split(",")
                cur["csv"] = {"ops": int(p[2]),
                              "elapsed_ns": int(p[3]),
                              "chk": int(p[6])}
                continue
            if "|" in line:
                p = line.split("|")
                if len(p) >= 3 and p[0] != "<not supported>":
                    try:
                        cur["events"][p[2]] = float(p[0])
                    except ValueError:
                        pass
                elif p[0] == "<not supported>":
                    cur["events"][p[2]] = None
    idle_w = []
    for b in blocks:
        if b["leg"] == "idle":
            j = b["events"].get("power/energy-pkg/")
            if j is None or not b["wall_ns"]:
                print(f"IDLE sample {b['proc']}: UNSUPPORTED/missing")
                continue
            idle_w.append(j / (b["wall_ns"] / 1e9))
    if not idle_w:
        print("ENERGY RESULT: FAIL - no valid idle samples")
        return 1
    idle_med = statistics.median(idle_w)
    print(f"IDLE baseline: median {idle_med:.3f} W (min "
          f"{min(idle_w):.3f}, max {max(idle_w):.3f}, n={len(idle_w)})")
    rows, bad = defaultdict(list), 0
    for b in blocks:
        if b["leg"] == "idle":
            continue
        gross = b["events"].get("power/energy-pkg/")
        if gross is None or b["csv"] is None:
            print(f"BAD BLOCK {(b['leg'], b['workload'])} "
                  f"proc={b['proc']}")
            bad += 1
            continue
        if b["csv"]["chk"] != Z003_CHK[b["workload"]] * b["reps"]:
            bad += 1
        wall_s = b["wall_ns"] / 1e9
        net = gross - idle_med * wall_s
        rows[(b["leg"], b["workload"])].append(
            (net / b["csv"]["ops"] * 1e9, gross / b["csv"]["ops"] * 1e9))
    med, gmed = {}, {}
    print(f"{'leg':<11} {'workload':<16} {'med_net_nJ/op':>14} "
          f"{'min':>10} {'max':>10} {'med_gross_nJ/op':>16}")
    for key in sorted(rows):
        vals = [r[0] for r in rows[key]]
        gvals = [r[1] for r in rows[key]]
        med[key] = statistics.median(vals)
        gmed[key] = statistics.median(gvals)
        print(f"{key[0]:<11} {key[1]:<16} {med[key]:>14.2f} "
              f"{min(vals):>10.2f} {max(vals):>10.2f} {gmed[key]:>16.2f}")
    print()
    print("=== ENERGY VERDICTS (median net J/op; WIN < 0.95x, "
          "TIE 0.95-1.05x, LOSS > 1.05x) ===")
    for w in WORKLOADS:
        mm = med[("mimiralloc", w)]
        for ref in ("glibc", "glibc_tuned"):
            g = med[(ref, w)]
            if g <= 0:
                print(f"{w}: mimir net {mm:.2f} nJ/op vs {ref} "
                      f"{g:.2f} nJ/op - BELOW IDLE-SUBTRACTION "
                      f"RESOLUTION for the reference, no ratio "
                      f"claimed (gross medians: mimir "
                      f"{gmed[('mimiralloc', w)]:.2f} vs {ref} "
                      f"{gmed[(ref, w)]:.2f} nJ/op)")
                continue
            f, v = grade(mm, g)
            print(f"{w}: mimir net {mm:.2f} nJ/op vs {ref} "
                  f"{g:.2f} nJ/op | factor {f:.2f}x | {v}")
    print()
    print("Z-009 baseline (Z-007): LOSS in direction on all four; "
          "proven net factor large_batch 4.42x; small_churn factor "
          "indicative only; mixed_batch / mixed_lifecycle net "
          "factors below resolution in that contended window.")
    if bad:
        print(f"ENERGY RESULT: FAIL - {bad} bad/checksum blocks")
        return 1
    print("ENERGY RESULT: PASS (checksums true, idle baseline valid)")
    return 0


def main():
    mode = sys.argv[1]
    if mode == "latency":
        return mode_latency(sys.argv[2], sys.argv[3], int(sys.argv[4]))
    if mode == "counters":
        return mode_counters(sys.argv[2])
    if mode == "rss":
        return mode_rss(sys.argv[2])
    if mode == "energy":
        return mode_energy(sys.argv[2])
    print(f"unknown mode {mode}")
    return 2


if __name__ == "__main__":
    sys.exit(main())
