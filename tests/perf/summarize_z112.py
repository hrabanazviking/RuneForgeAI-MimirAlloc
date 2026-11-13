#!/usr/bin/env python3
"""summarize_z112.py - Z-112 synthesis across executions.

Reads raw probe outputs (one file per execution), takes
median-of-medians per (size, leg), and grades under the
slice's PRE-REGISTERED rule (fixed in WAVE_PLAN.md before
the slice started, stated here before any recorded run):

  Success measure: the variant is measured against the
  banked pool on pool-miss shapes; ADOPTED only on a
  WIN/TIE with no regression elsewhere, else NO-ADOPTION.

  Miss-shape grade per size >= 2 MiB: WIN iff huge backing
  actually formed (AnonHugePages > 0, variant leg) AND
  touched-total improvement >= 10% AND no miss metric
  (pair, total) regresses > 10%. TIE iff every miss metric
  is within +/-5%. Otherwise LOSS / MIXED as named.
  Regression elsewhere: (a) the untouched pair at any
  size regressing > 10%; (b) the pool-hit witness - if
  replacing the banked pool with the huge-page-only pool
  makes sub-PMD cycling traffic pay the fresh price
  (variant_fresh > banked_pool_hit by > 10%), that is a
  regression elsewhere BY CONSTRUCTION and adoption as a
  replacement fails regardless of the miss grades.
  Overall decision: ADOPT iff every miss shape is WIN or
  TIE, huge backing formed, and no regression elsewhere;
  otherwise NO-ADOPTION, with the miss grades banked as
  priced evidence and the blocking regression named.
"""
import statistics
import sys

SIZES = [2097152, 4194304, 20971520, 41943040]
LABELS = {2097152: "2MiB", 4194304: "4MiB",
          20971520: "20MiB", 41943040: "40MiB-overbound"}


def parse(path):
    med = {}
    wit = {}
    verdicts = []
    for line in open(path):
        line = line.strip()
        if line.startswith("median,size="):
            kv = {}
            for part in line.split(",")[1:]:
                if "=" in part:
                    k, v = part.split("=", 1)
                    kv[k] = v
            med[(int(kv["size"]), kv["leg"])] = kv
        elif line.startswith("median,witness_size="):
            kv = {}
            for part in line.split(",")[1:]:
                if "=" in part:
                    k, v = part.split("=", 1)
                    kv[k] = v
            wit[int(kv["witness_size"])] = kv
        elif line.startswith("verdict,") or line.startswith("RESULT"):
            verdicts.append(line)
    return med, wit, verdicts


def main():
    runs, wits, allv = [], [], []
    for path in sys.argv[1:]:
        m, w, v = parse(path)
        runs.append(m); wits.append(w); allv.append(v)
    print("synthesis: median-of-medians across %d executions"
          % len(runs))
    def med(size, leg, field):
        vals = []
        for m in runs:
            kv = m.get((size, leg))
            if kv is not None and field in kv:
                vals.append(float(kv[field]))
        return statistics.median(vals) if vals else None
    def imp(off, on):
        if off in (None, 0) or on is None:
            return None
        return (off - on) / off * 100.0
    miss_ok = True
    huge_ok = True
    pair_reg = False
    for size in SIZES:
        tb, tv = med(size, "banked", "total_ns"), med(size, "variant", "total_ns")
        pb, pv = med(size, "banked", "pair_ns"), med(size, "variant", "pair_ns")
        mb, mv = med(size, "banked", "minflt"), med(size, "variant", "minflt")
        hg = med(size, "variant", "huge_kib")
        ti, pi = imp(tb, tv), imp(pb, pv)
        huge = (hg or 0) > 0
        huge_ok = huge_ok and huge
        if pi is not None and pi < -10:
            pair_reg = True
        if huge and ti is not None and ti >= 10 and (pi is None or pi >= -10):
            grade = "WIN"
        elif ti is not None and abs(ti) <= 5 and (pi is None or abs(pi) <= 5):
            grade = "TIE"
        elif ti is not None and ti < -10:
            grade = "LOSS"
        else:
            grade = "MIXED"
        if grade not in ("WIN", "TIE"):
            miss_ok = False
        print("size=%d label=%s total banked=%s variant=%s improve=%s; pair banked=%s variant=%s improve=%s; minflt banked=%s variant=%s; anon_huge_variant=%s KiB; grade=%s"
              % (size, LABELS[size], tb, tv, None if ti is None else round(ti, 2), pb, pv, None if pi is None else round(pi, 2), mb, mv, hg, grade))
    wit_reg = False
    for w in wits:
        for size, kv in sorted(w.items()):
            hit = float(kv["banked_pool_hit_ns"]); fresh = float(kv["variant_fresh_ns"])
            ratio = fresh / hit if hit else None
            reg = ratio is not None and ratio > 1.10
            wit_reg = wit_reg or reg
            print("witness size=%d banked_pool_hit_ns=%s variant_fresh_ns=%d ratio=%s regression=%s"
                  % (size, hit, fresh, None if ratio is None else round(ratio, 2), reg))
    print("control size=65536: variant does not serve sub-PMD sizes by construction; banked pool unchanged there (no-regression witness is the witness lines above, not this size)")
    adopt = miss_ok and huge_ok and not pair_reg and not wit_reg
    print("pair_regression=%s witness_regression=%s huge_backing_all=%s miss_all_win_or_tie=%s" % (pair_reg, wit_reg, huge_ok, miss_ok))
    print("DECISION: %s" % ("ADOPT" if adopt else "NO-ADOPTION"))
    if not adopt and miss_ok and wit_reg:
        print("reason: miss shapes WIN/TIE, but adoption as a replacement for the banked pool regresses sub-PMD pool-hit traffic by construction (witness); NO-ADOPTION as a replacement, miss-shape WIN banked as evidence")
    for i, v in enumerate(allv, 1):
        for line in v:
            print("  run%d %s" % (i, line))


if __name__ == "__main__":
    main()
