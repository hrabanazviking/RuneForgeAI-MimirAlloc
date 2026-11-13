#!/usr/bin/env python3
"""summarize_z098.py - Z-098 fair-fight summariser.

Reads the raw CSV produced by tests/perf/run_z098.sh
driving tests/perf/bench_k002.c:

  config,workload,ops,elapsed_ns,ops_per_sec,ns_per_op,checksum

config is one of the glibc tunable configurations in
CONFIGS below (label -> GLIBC_TUNABLES string, "" =
unset / glibc defaults), the identical-leg control
gdefault_dup, or mimir (the MimirAlloc leg of the same
oracle harness, built with the shipped -O3 set).

Prints, per invocation:
  1. The full tunables matrix: median ns/op of every
     config on every workload, with delta % vs the
     default-glibc median of the same workload.
  2. The identical-leg control verdict: gdefault vs
     gdefault_dup median agreement per workload against
     the Z-004 protocol's 5% noise bound.
  3. Per workload: the best tuned glibc (lowest median
     among the tuned configs), its tunable string, its
     gain vs default glibc graded on the 5% band
     (TUNING WIN beyond -5%, TIE inside the band), and
     MimirAlloc's standing against BOTH default glibc
     and the best tuned glibc (WIN / TIE / LOSS on the
     same band, from MimirAlloc's side).
  4. The Z-107 raw-data block: machine-readable lines
     carrying the default-glibc, tuned-glibc (per
     workload best) and mimiralloc medians, so the
     Z-107 closing table's tuned-glibc column is built
     from this slice's recorded raw data.

Hard FAIL (exit 1) on: any missing (config, workload)
cell, any rep count other than the expected one, any
ops mismatch within a workload, any checksum mismatch
within a workload (the legs must do identical work),
or an identical-leg control breach beyond 5% (a
breached control means the window was too noisy for
any cell to be a verdict).

With a second raw file, an AGREEMENT section compares
the two invocations: best-tuned config per workload
must be the same config, or the two configs' medians
within the 5% noise band of each other in BOTH
invocations (a statistical tie at the top is reported
as such, never silently resolved); default, best-tuned
and mimir medians must agree within the protocol's
8% drift bound.

Usage: summarize_z098.py <raw1> [raw2] [reps]
"""
import statistics
import sys
from collections import defaultdict

WORKLOADS = ["small_churn", "mixed_batch", "large_batch", "mixed_lifecycle"]

# label -> GLIBC_TUNABLES string ("" = unset). Must match run_z098.sh.
CONFIGS = [
    ("gdefault", ""),
    ("gdefault_dup", ""),
    ("gtc0", "glibc.malloc.tcache_count=0"),
    ("gtc1", "glibc.malloc.tcache_count=1"),
    ("gtc16", "glibc.malloc.tcache_count=16"),
    ("gtc64", "glibc.malloc.tcache_count=64"),
    ("gtmax512", "glibc.malloc.tcache_max=512"),
    ("gtmax4k", "glibc.malloc.tcache_max=4096"),
    ("gtmax64k", "glibc.malloc.tcache_max=65536"),
    ("gmm4k", "glibc.malloc.mmap_threshold=4096"),
    ("gmm32k", "glibc.malloc.mmap_threshold=32768"),
    ("gmm1m", "glibc.malloc.mmap_threshold=1048576"),
    ("gcombo_cache", "glibc.malloc.tcache_count=64:glibc.malloc.tcache_max=65536"),
    ("gcombo_fair",
     "glibc.malloc.tcache_count=64:glibc.malloc.tcache_max=65536:"
     "glibc.malloc.mmap_threshold=1048576"),
    ("mimir", "(MimirAlloc leg; GLIBC_TUNABLES not applied)"),
]
TUNED = [label for label, _ in CONFIGS
         if label not in ("gdefault", "gdefault_dup", "mimir")]
NOISE = 5.0   # Z-004 identical-leg / tie band, percent
DRIFT = 8.0   # Z-004 cross-invocation drift bound, percent


def med(v):
    return statistics.median(v)


def load(path, reps):
    cells = defaultdict(list)
    ops = {}
    chks = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split(",")
            if len(parts) != 7:
                print(f"FAIL: malformed raw line: {line}")
                sys.exit(1)
            cfg, wl, o, _el, _ops, nspo, chk = parts
            cells[(cfg, wl)].append(float(nspo))
            if wl in ops and ops[wl] != int(o):
                print(f"FAIL: ops mismatch on {wl}: {ops[wl]} vs {o}")
                sys.exit(1)
            ops[wl] = int(o)
            if wl in chks and chks[wl] != chk:
                print(f"FAIL: checksum mismatch on {wl}: "
                      f"{chks[wl]} vs {chk} (legs did different work)")
                sys.exit(1)
            chks[wl] = chk
    labels = [label for label, _ in CONFIGS]
    for cfg in labels:
        for wl in WORKLOADS:
            got = len(cells.get((cfg, wl), []))
            if got != reps:
                print(f"FAIL: cell {cfg}/{wl} has {got} reps, expect {reps}")
                sys.exit(1)
    return {k: med(v) for k, v in cells.items()}


def pct(delta_base, base):
    return 100.0 * delta_base / base


def grade_mimir(mimir_ns, other_ns):
    d = pct(mimir_ns - other_ns, other_ns)
    if d > NOISE:
        return "LOSS"
    if d < -NOISE:
        return "WIN"
    return "TIE"


def tuning_grade(best_ns, default_ns):
    d = pct(best_ns - default_ns, default_ns)
    if d < -NOISE:
        return "TUNING WIN"
    if d > NOISE:
        return "TUNING LOSS (every tuned config slower)"
    return "TIE (no gain beyond noise)"


def best_tuned(medians, wl):
    cands = sorted(TUNED, key=lambda c: medians[(c, wl)])
    return cands[0], medians[(cands[0], wl)]


def report(medians, tag):
    print(f"=== Z-098 TUNABLES MATRIX ({tag}) - median ns/op, delta % vs default glibc ===")
    hdr = f"{'config':<14}" + "".join(f"{w:>26}" for w in WORKLOADS)
    print(hdr)
    for label, tun in CONFIGS:
        row = f"{label:<14}"
        for wl in WORKLOADS:
            m = medians[(label, wl)]
            d = medians[("gdefault", wl)]
            row += f"{m:>13.2f} ({pct(m - d, d):>+6.2f}%)   "
        print(row)
    print()
    print("=== IDENTICAL-LEG CONTROL (Z-004 bound: 5%) ===")
    worst = 0.0
    for wl in WORKLOADS:
        a = medians[("gdefault", wl)]
        b = medians[("gdefault_dup", wl)]
        ag = abs(pct(b - a, a))
        worst = max(worst, ag)
        print(f"{wl:<16} default={a:>10.2f} dup={b:>10.2f} agreement={ag:.2f}%")
    print(f"worst agreement: {worst:.2f}% (bound 5.00%) - "
          f"{'PASS' if worst <= NOISE else 'BREACH'}")
    if worst > NOISE:
        print("FAIL: identical-leg control breached; window too noisy")
        sys.exit(1)
    print()
    print("=== PER-WORKLOAD VERDICT: best tuned glibc + MimirAlloc standing ===")
    tun_map = dict(CONFIGS)
    for wl in WORKLOADS:
        d = medians[("gdefault", wl)]
        bl, bm = best_tuned(medians, wl)
        mm = medians[("mimir", wl)]
        print(f"{wl}:")
        print(f"  default glibc        {d:>10.2f} ns/op")
        print(f"  best tuned glibc     {bm:>10.2f} ns/op  [{bl}: "
              f"{tun_map[bl]}]  gain {pct(bm - d, d):+.2f}% - "
              f"{tuning_grade(bm, d)}")
        print(f"  mimiralloc           {mm:>10.2f} ns/op")
        print(f"  mimir vs default:    {mm / d:6.2f}x  {grade_mimir(mm, d)}"
              f"    mimir vs tuned: {mm / bm:6.2f}x  {grade_mimir(mm, bm)}")
    print()
    print("=== Z-107 RAW DATA (tuned-glibc column source) ===")
    for wl in WORKLOADS:
        d = medians[("gdefault", wl)]
        bl, bm = best_tuned(medians, wl)
        mm = medians[("mimir", wl)]
        print(f"z107,{wl},glibc_default,,{d:.2f}")
        print(f"z107,{wl},glibc_tuned,{bl}:{tun_map[bl]},{bm:.2f}")
        print(f"z107,{wl},mimiralloc,,{mm:.2f}")
    print()
    return medians


def agreement(m1, m2):
    print("=== CROSS-INVOCATION AGREEMENT (drift bound: 8%) ===")
    ok = True
    tun_map = dict(CONFIGS)
    for wl in WORKLOADS:
        b1, bm1 = best_tuned(m1, wl)
        b2, bm2 = best_tuned(m2, wl)
        same = (b1 == b2)
        tie = False
        if not same:
            # statistical tie at the top: each invocation's winner
            # must sit within the noise band of the other's winner
            # in BOTH invocations.
            tie = (abs(pct(m1[(b2, wl)] - bm1, bm1)) <= NOISE and
                   abs(pct(m2[(b1, wl)] - bm2, bm2)) <= NOISE)
        drift_best = abs(pct(bm2 - bm1, bm1))
        d_drift = abs(pct(m2[("gdefault", wl)] - m1[("gdefault", wl)],
                          m1[("gdefault", wl)]))
        m_drift = abs(pct(m2[("mimir", wl)] - m1[("mimir", wl)],
                          m1[("mimir", wl)]))
        worst = max(drift_best, d_drift, m_drift)
        stat = "OK" if (same or tie) and worst <= DRIFT else "DISAGREE"
        if stat != "OK":
            ok = False
        print(f"{wl:<16} best1={b1} best2={b2} "
              f"({'same' if same else 'tied-at-top' if tie else 'DIFFERENT'}) "
              f"drift default={d_drift:.2f}% best={drift_best:.2f}% "
              f"mimir={m_drift:.2f}% - {stat}")
        if tie and not same:
            print(f"  note: {b1} [{tun_map[b1]}] and {b2} [{tun_map[b2]}] "
                  f"are within noise of each other in both invocations; "
                  f"the tuned column takes invocation 1's winner and "
                  f"names the tie.")
    print(f"agreement: {'PASS' if ok else 'FAIL'}")
    if not ok:
        sys.exit(1)


def main():
    if len(sys.argv) < 2:
        print("usage: summarize_z098.py <raw1> [raw2] [reps]")
        return 2
    reps = int(sys.argv[3]) if len(sys.argv) > 3 else 7
    m1 = load(sys.argv[1], reps)
    report(m1, "invocation 1")
    if len(sys.argv) > 2 and sys.argv[2] != "-":
        m2 = load(sys.argv[2], reps)
        report(m2, "invocation 2")
        agreement(m1, m2)
    return 0


if __name__ == "__main__":
    sys.exit(main())
