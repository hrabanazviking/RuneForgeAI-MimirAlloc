#!/usr/bin/env python3
"""summarize_z031.py - Z-031 small-path verdict summariser.

Reads the three measured runs produced by run_z031.sh through
scripts/z13/run_measured.sh (Z-004 protocol), each a prefix with
.legA.raw / .legB.raw files in the K-002 harness CSV format
(leg,workload,ops,elapsed_ns,ops_per_sec,ns_per_op,checksum):

  main : A = mimiralloc (banked core), B = glibc   (9 reps)
  gg   : A = glibc,      B = glibc  (identical-leg control, 7 reps)
  mm   : A = mimiralloc, B = mimiralloc (identical control, 7 reps)

For every workload it prints, per leg: median ns/op (the protocol
section 6 headline), min..max spread, and split-half drift %
(section 8, bound 8%). For the identical-leg controls it prints
the agreement % (section 7, bound 5%). Checksums must be exactly
one distinct value per workload across both legs of every run
(section 9, the K-002 discipline).

The Z-031 gate workloads are small_churn and mixed_batch; the
other two K-002 workloads ride along in the raw (the banked
harness runs all four) and are printed as context only.

Verdicts for the focus workloads are graded against glibc and
against the Z-009 baseline of record (Z-003 medians, banked in
docs/Z13_WAVE01_BASELINE.md: small_churn glibc 9.02 / mimir
93.43 ns/op, mixed_batch glibc 9.90 / mimir 398.42 ns/op) in
the roadmap's grades: WIN / TIE / LOSS, and STILL LOST with the
reason named when the loss stands (Law 2). A median difference
inside the 5% noise bound is WITHIN NOISE and graded TIE.

Usage: summarize_z031.py <main-prefix> <gg-prefix> <mm-prefix>
Exit: 0 iff checksums clean, all groups complete, both controls
agree within the noise bound on the focus workloads, and neither
focus workload is drift-rejected in the main run.
"""
import statistics
import sys
from collections import defaultdict

NOISE_PCT = 5.0
DRIFT_PCT = 8.0
FOCUS = ["small_churn", "mixed_batch"]
ALL_WL = ["small_churn", "mixed_batch", "large_batch", "mixed_lifecycle"]
Z009 = {  # medians of record, ns/op (leg glibc, leg mimiralloc)
    "small_churn": (9.02, 93.43),
    "mixed_batch": (9.90, 398.42),
    "large_batch": (1295.68, 3064.69),
    "mixed_lifecycle": (13.38, 898.76),
}


def load(path):
    rows = defaultdict(list)
    chks = defaultdict(set)
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            parts = line.strip().split(",")
            if len(parts) != 7:
                continue
            _leg, work, _ops, _el, _ops_s, ns_op, chk = parts
            rows[work].append(float(ns_op))
            chks[work].add(chk)
    return rows, chks


def drift_pct(vals):
    if len(vals) < 4:
        return 0.0
    half = len(vals) // 2
    first = statistics.median(vals[:half])
    second = statistics.median(vals[half:])
    med = statistics.median(vals)
    return 100.0 * abs(second - first) / med if med else 0.0


def stats(vals):
    return statistics.median(vals), min(vals), max(vals), drift_pct(vals)


def main():
    if len(sys.argv) != 4:
        print("usage: summarize_z031.py <main> <gg> <mm>", file=sys.stderr)
        return 2
    runs = {}
    ok = True
    for name, prefix in zip(["main", "gg", "mm"], sys.argv[1:4]):
        a_rows, a_chk = load(prefix + ".legA.raw")
        b_rows, b_chk = load(prefix + ".legB.raw")
        runs[name] = (a_rows, b_rows)
        for wl in ALL_WL:
            if not a_rows.get(wl) or not b_rows.get(wl):
                print(f"MISSING GROUP: run={name} workload={wl}")
                ok = False
            union = a_chk.get(wl, set()) | b_chk.get(wl, set())
            if len(union) != 1:
                print(f"CHECKSUM FAILURE: run={name} workload={wl} "
                      f"distinct={sorted(union)}")
                ok = False
    print("CHECKSUMS: exactly one distinct value per workload in "
          "every run" if ok else "CHECKSUM/GROUP PROBLEMS ABOVE")

    def agree(name, wl):
        a_rows, b_rows = runs[name]
        ma = statistics.median(a_rows[wl])
        mb = statistics.median(b_rows[wl])
        return 100.0 * abs(ma - mb) / ((ma + mb) / 2.0), ma, mb

    print("\n== Identical-leg controls (protocol section 7, "
          f"bound {NOISE_PCT:.0f}%) ==")
    for name, leg in [("gg", "glibc"), ("mm", "mimiralloc")]:
        for wl in ALL_WL:
            ag, ma, mb = agree(name, wl)
            verdict = "AGREE" if ag <= NOISE_PCT else "DISAGREE"
            if wl in FOCUS and ag > NOISE_PCT:
                ok = False
            print(f"{leg:10s} {wl:16s} A median {ma:10.2f}  "
                  f"B median {mb:10.2f}  agreement {ag:5.2f}%  {verdict}")

    a_rows, b_rows = runs["main"]  # A mimiralloc, B glibc
    print("\n== Main run: mimiralloc vs glibc (median ns/op, "
          "protocol section 6) ==")
    print(f"{'workload':16s} {'glibc':>10s} {'mimir':>10s} {'factor':>8s} "
          f"{'grade':>6s}  spreads and drift")
    verdicts = {}
    for wl in ALL_WL:
        gmed, gmin, gmax, gdrift = stats(b_rows[wl])
        mmed, mmin, mmax, mdrift = stats(a_rows[wl])
        factor = mmed / gmed
        if factor <= 1.0 + NOISE_PCT / 100.0 and \
                factor >= 1.0 - NOISE_PCT / 100.0:
            grade = "TIE"
        elif factor < 1.0:
            grade = "WIN"
        else:
            grade = "LOSS"
        verdicts[wl] = (gmed, mmed, factor, grade)
        flag = ""
        if wl in FOCUS and (gdrift > DRIFT_PCT or mdrift > DRIFT_PCT):
            flag = " DRIFT-REJECT"
            ok = False
        print(f"{wl:16s} {gmed:10.2f} {mmed:10.2f} {factor:7.2f}x "
              f"{grade:>6s}  glibc {gmin:.2f}..{gmax:.2f} drift "
              f"{gdrift:.2f}% | mimir {mmin:.2f}..{mmax:.2f} drift "
              f"{mdrift:.2f}%{flag}")

    print("\n== Vs Z-009 baseline of record (Z-003 medians; "
          "cross-session, indicative) ==")
    for wl in FOCUS:
        gmed, mmed, factor, grade = verdicts[wl]
        g0, m0 = Z009[wl]
        print(f"{wl}: glibc {g0:.2f} -> {gmed:.2f} "
              f"({100.0 * (gmed - g0) / g0:+.1f}%), "
              f"mimir {m0:.2f} -> {mmed:.2f} "
              f"({100.0 * (mmed - m0) / m0:+.1f}%), "
              f"factor {m0 / g0:.1f}x -> {factor:.1f}x, grade {grade}")

    print("\n== Z-031 verdict lines (focus workloads) ==")
    for wl in FOCUS:
        gmed, mmed, factor, grade = verdicts[wl]
        if grade == "LOSS":
            print(f"{wl}: STILL LOST - {factor:.2f}x vs glibc "
                  f"(mimir {mmed:.2f} ns/op, glibc {gmed:.2f} ns/op)")
        else:
            print(f"{wl}: {grade} - {factor:.2f}x vs glibc "
                  f"(mimir {mmed:.2f} ns/op, glibc {gmed:.2f} ns/op)")

    print("\nZ031 PROTOCOL GATE:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
