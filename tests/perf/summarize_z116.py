#!/usr/bin/env python3
"""summarize_z116.py - Z-116 idle-behaviour summariser.

Reads the two probe raw files (mimir, glibc) produced by
run_z116.sh plus the power file (perf stat -a RAPL blocks
written by the runner: pre / during1 / during2 / post
package samples, Z-007 idle-subtraction method).

Checks (hard FAIL): each leg has the full sample schedule
(baseline, peak, drained, N idle, post_trim, 2
post_trim_idle, final), DONE integrity_ok=1, and the TRIM
line is present. Reports per leg: drained retention over
baseline (RSS KiB and cached bytes), idle drift (first
vs last idle sample), trim effect (cached and RSS drop
at the explicit trim), post-trim level vs baseline, and
the package idle watts: pre-load baseline, during the
retained-idle window (mimir leg), and post-trim. The
idle cost headline is the during-minus-pre package W
delta with the co-tenant caveat carried by the runner's
recorded load state, plus the retained MiB held across
the idle window (the resident cost itself).
"""
import statistics
import sys


def parse_probe(path):
    samples = []
    trim = None
    done = None
    hdr = None
    for line in open(path):
        line = line.strip()
        p = line.split(",")
        if p[0] == "IDLEPROBE":
            hdr = (p[1], int(p[2]), int(p[3]))
        elif p[0] == "SAMPLE":
            samples.append({
                "leg": p[1], "phase": p[2], "t_ms": int(p[3]),
                "rss": int(p[4]), "pss": int(p[5]),
                "vma": int(p[6]), "cached": int(p[7]),
                "mapped": int(p[8]), "live": int(p[9])})
        elif p[0] == "TRIM":
            trim = {"leg": p[1], "ret": int(p[2]),
                    "cb": int(p[3]), "ca": int(p[4])}
        elif p[0] == "DONE":
            done = {"leg": p[1], "n": int(p[2]),
                    "ck": int(p[3]), "ok": int(p[4])}
    return hdr, samples, trim, done


def parse_power(path):
    out = {}
    cur = None
    for line in open(path):
        line = line.strip()
        if line.startswith("=== power "):
            cur = line.split()[2]
            out[cur] = {}
            continue
        if cur and "|" in line:
            q = line.split("|")
            if len(q) >= 3 and q[0] != "<not supported>":
                try:
                    out[cur][q[2].strip()] = float(q[0])
                except ValueError:
                    pass
        if cur and line.startswith("power_wall_s="):
            out[cur]["wall_s"] = float(line.split("=")[1])
    watts = {}
    for k, v in out.items():
        j = v.get("power/energy-pkg/")
        w = v.get("wall_s")
        if j is not None and w:
            watts[k] = j / w
    return watts


def leg_report(name, hdr, samples, trim, done, fail):
    if hdr is None or done is None or trim is None:
        print(f"{name}: FAIL - missing header/DONE/TRIM")
        return False
    idle_s, sample_s = hdr[1], hdr[2]
    want_idle = idle_s // sample_s
    by = {}
    for s in samples:
        by.setdefault(s["phase"], []).append(s)
    need = {"baseline": 1, "peak": 1, "drained": 1,
            "idle": want_idle, "post_trim": 1,
            "post_trim_idle": 2, "final": 1}
    ok = True
    for ph, n in need.items():
        got = len(by.get(ph, []))
        if got != n:
            print(f"{name}: FAIL - phase {ph} samples {got} != {n}")
            ok = False
    if done["ok"] != 1:
        print(f"{name}: FAIL - DONE integrity_ok=0")
        ok = False
    if done["n"] != len(samples):
        print(f"{name}: FAIL - DONE samples {done['n']} != {len(samples)}")
        ok = False
    if not ok:
        return False
    base = by["baseline"][0]
    peak = by["peak"][0]
    dr = by["drained"][0]
    idle = by["idle"]
    pt = by["post_trim"][0]
    fin = by["final"][0]
    d_rss = dr["rss"] - base["rss"]
    i_first, i_last = idle[0], idle[-1]
    drift_rss = i_last["rss"] - i_first["rss"]
    drift_cached = i_last["cached"] - i_first["cached"]
    trim_rss = pt["rss"] - dr["rss"]
    print(f"=== leg {name} (idle {idle_s}s, sample {sample_s}s) ===")
    print(f"baseline: rss={base['rss']} KiB cached={base['cached']} B "
          f"mapped={base['mapped']} B vma={base['vma']}")
    print(f"peak: rss={peak['rss']} KiB live={peak['live']} B "
          f"cached={peak['cached']} B")
    print(f"drained: rss={dr['rss']} KiB (retained over baseline "
          f"{d_rss:+d} KiB) cached={dr['cached']} B "
          f"mapped={dr['mapped']} B vma={dr['vma']}")
    print(f"idle window: {len(idle)} samples, rss first={i_first['rss']} "
          f"last={i_last['rss']} drift={drift_rss:+d} KiB; cached "
          f"first={i_first['cached']} last={i_last['cached']} "
          f"drift={drift_cached:+d} B")
    stable = all(s["cached"] == dr["cached"] for s in idle)
    print(f"idle cached constant across window: {stable}")
    print(f"trim(0): ret={trim['ret']} cached {trim['cb']} -> "
          f"{trim['ca']} B; rss drained->post_trim {trim_rss:+d} KiB; "
          f"post_trim rss={pt['rss']} KiB cached={pt['cached']} B "
          f"vma={pt['vma']}")
    print(f"final: rss={fin['rss']} KiB cached={fin['cached']} B "
          f"(over baseline rss {fin['rss']-base['rss']:+d} KiB)")
    print(f"retained MiB held during idle (drained cached): "
          f"{dr['cached']/1048576.0:.2f}")
    return True


def main():
    mimir_p, glibc_p, power_p = sys.argv[1], sys.argv[2], sys.argv[3]
    ok = True
    for name, path in (("mimir", mimir_p), ("glibc", glibc_p)):
        hdr, samples, trim, done = parse_probe(path)
        if not leg_report(name, hdr, samples, trim, done, None):
            ok = False
    watts = parse_power(power_p)
    print("=== package idle power (RAPL power/energy-pkg/, "
          "perf stat -a, Z-007 method) ===")
    for k in ("pre", "during1", "during2", "post", "post_glibc"):
        if k in watts:
            print(f"{k}: {watts[k]:.2f} W")
    if "pre" in watts and "during1" in watts and "during2" in watts:
        during = statistics.median([watts["during1"], watts["during2"]])
        print(f"during retained-idle median: {during:.2f} W; "
              f"delta vs pre: {during - watts['pre']:+.2f} W")
    print("Z-116 RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
