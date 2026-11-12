#!/usr/bin/env python3
"""summarize_z044.py - Z-044 fork posture summarizer.

Fails closed on EVIDENCE COMPLETENESS only: every expected
phase line must be present, per-try line counts must equal
the try counts, and every EXECUTED proof phase (Q1, Q2,
E1, E2, B0b, B2) must read PASS in both mimir runs and in
the glibc leg where it applies. Busy-phase outcomes (B0a,
B1) are MEASUREMENTS: they are tabulated as counts, never
converted to rates, and never upgraded to support. A
BOTH_OK busy child is a child in which no hang was
observed within the 500 ms window on that attempt -
nothing more (see the recorded run's boundary section).
"""
import re
import sys

def load(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return [ln.rstrip("\n") for ln in f]

def find(lines, pat):
    rx = re.compile(pat)
    return [ln for ln in lines if rx.search(ln)]

def one(lines, pat, what, errs):
    hits = find(lines, pat)
    if len(hits) != 1:
        errs.append(f"{what}: expected exactly 1 line matching {pat!r}, found {len(hits)}")
        return None
    return hits[0]

def main():
    if len(sys.argv) != 4:
        print("usage: summarize_z044.py <mimir_a> <mimir_b> <glibc>")
        return 2
    runs = {"mimir A": load(sys.argv[1]), "mimir B": load(sys.argv[2]),
            "glibc": load(sys.argv[3])}
    errs = []
    out = []
    out.append("== Z-044 SUMMARY (counts only; no rates) ==")

    for name in ("mimir A", "mimir B"):
        lines = runs[name]
        one(lines, r"Z044,mimir,START,", f"{name} START", errs)
        end = one(lines, r"Z044,mimir,END,failures=0", f"{name} END", errs)
        if end is None:
            find_end = find(lines, r"Z044,mimir,END,")
            if find_end:
                errs.append(f"{name}: END line shows failures: {find_end[0]}")
        for ph in ("Q1", "Q2"):
            ln = one(lines, rf"Z044,mimir,{ph},result=PASS", f"{name} {ph} PASS", errs)
            if ln:
                out.append(f"{name} {ph}: PASS")
        child = one(lines, r"Z044,mimir,Q1,child,cached=(\d+) hit=(\d) verify=ok",
                    f"{name} Q1 child", errs)
        if child:
            m = re.search(r"cached=(\d+) hit=(\d)", child)
            out.append(f"{name} Q1 child: inherited-bin cached={m.group(1)} "
                       f"small-alloc hit={m.group(2)}")
        q1s = [l for l in lines if "Z044,mimir,Q1,childstats," in l]
        if q1s:
            out.append(f"{name} Q1 child end stats: "
                       f"{q1s[0].split('childstats,', 1)[1]}")
        q2c = one(lines, r"Z044,mimir,Q2,child,stats_ok=1 live_small_before=(\d+) "
                         r"live_small_after=(\d+) retained_before=(\d+) "
                         r"drain_released=(\d+) retained_after=(\d+)",
                  f"{name} Q2 child stranding line", errs)
        if q2c:
            m = re.search(r"live_small_before=(\d+) live_small_after=(\d+) "
                          r"retained_before=(\d+) drain_released=(\d+) "
                          r"retained_after=(\d+)", q2c)
            out.append(f"{name} Q2 child: live_small {m.group(1)} -> "
                       f"{m.group(2)} after freeing every inherited block; "
                       f"retained_before_drain={m.group(3)} "
                       f"drain_released={m.group(4)} "
                       f"retained_after_drain={m.group(5)} "
                       f"(retained_after > 0 = spans stranded by vanished "
                       f"threads' binned tally, INC-Z044-01)")
        q2p = [l for l in lines if "Z044,mimir,Q2,parent," in l]
        if q2p:
            out.append(f"{name} Q2 parent end stats: "
                       f"{q2p[0].split('parent,', 1)[1]}")
        for ph, tries in (("E1", 16), ("E2", 32), ("B2", 8)):
            ln = one(lines, rf"Z044,mimir,{ph},ok={tries} tries={tries} result=PASS",
                     f"{name} {ph}", errs)
            if ln:
                out.append(f"{name} {ph}: {tries}/{tries} PASS")
        b0b = one(lines, r"Z044,mimir,B0b,counts,both_ok=3 other=0 tries=3 result=PASS",
                  f"{name} B0b control", errs)
        if b0b:
            out.append(f"{name} B0b (lock released before fork): 3/3 BOTH_OK PASS")
        # B0a measurement tabulation
        b0a_tries = find(lines, r"Z044,mimir,B0a,try=")
        if len(b0a_tries) != 3:
            errs.append(f"{name} B0a: expected 3 try lines, found {len(b0a_tries)}")
        b0a = one(lines, r"Z044,mimir,B0a,counts,", f"{name} B0a counts", errs)
        if b0a:
            out.append(f"{name} B0a (lock provably HELD at fork): {b0a.split(',', 3)[-1]}")
        b1_tries = find(lines, r"Z044,mimir,B1,try=")
        if len(b1_tries) != 24:
            errs.append(f"{name} B1: expected 24 try lines, found {len(b1_tries)}")
        b1 = one(lines, r"Z044,mimir,B1,counts,", f"{name} B1 counts", errs)
        if b1:
            out.append(f"{name} B1 (fork-while-busy, 500 ms window): {b1.split(',', 3)[-1]}")
        busy = one(lines, r"Z044,mimir,BUSY,churn_iters_delta=(\d+)", f"{name} BUSY", errs)
        if busy:
            m = re.search(r"delta=(\d+)", busy)
            if int(m.group(1)) == 0:
                errs.append(f"{name}: churners made no progress - busy phases void")
            else:
                out.append(f"{name} churn progress during busy window: "
                           f"{m.group(1)} iterations per 100 ms sample")

    gl = runs["glibc"]
    one(gl, r"Z044,glibc,END,failures=0", "glibc END", errs)
    for ph, tries in (("E1", 16), ("E2", 32), ("B2", 8)):
        ln = one(gl, rf"Z044,glibc,{ph},ok={tries} tries={tries} result=PASS",
                 f"glibc {ph}", errs)
        if ln:
            out.append(f"glibc {ph}: {tries}/{tries} PASS (context leg)")
    gb1_tries = find(gl, r"Z044,glibc,B1,try=")
    if len(gb1_tries) != 24:
        errs.append(f"glibc B1: expected 24 try lines, found {len(gb1_tries)}")
    gb1 = one(gl, r"Z044,glibc,B1,counts,", "glibc B1 counts", errs)
    if gb1:
        out.append(f"glibc B1 (same shape, context only): {gb1.split(',', 3)[-1]}")

    out.append("== BOUNDARY (as evidenced above) ==")
    out.append("Fork supported: quiescent (Q1/Q2 PASS) and fork+exec "
               "(E1/E2 PASS, incl. under churn where the child only execs).")
    out.append("Fork-while-busy: MEASURED UNSUPPORTED. B0a stages the "
               "mechanism deterministically; B1 counts are observations "
               "in a 500 ms window, quoted as counts, never rates. A "
               "BOTH_OK busy child is not support, not a fix, not safety: "
               "no hang was observed in that child, nothing more.")
    out.append("Stranding: Q2 retained_after_drain counts are the "
               "INC-Z044-01 evidence (vanished threads' binned tally).")
    for ln in out:
        print(ln)
    if errs:
        print("SUMMARY: FAIL-CLOSED - evidence incomplete:")
        for e in errs:
            print(f"  - {e}")
        return 1
    print("SUMMARY: evidence complete; all EXECUTED proof phases PASS "
          "in both mimir runs; busy outcomes tabulated as counts.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
