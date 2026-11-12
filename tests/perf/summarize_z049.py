#!/usr/bin/env python3
"""summarize_z049.py - Z-049 synthesis across recorded executions.

Reads the raw probe outputs (one file per execution) named on
the command line, takes the median-of-medians for every
(size, mode, phase) metric, computes the THP-on vs THP-off
deltas, and prints the per-size-decade adopt/reject table
under the slice's pre-registered rule:

  ADOPT a size decade iff huge backing actually formed in the
  pooled shape (AnonHugePages > 0 in the on leg) AND at least
  one of fresh-fault / refault improvement >= 10% or write
  improvement >= 5% AND no measured metric regresses > 10%.
  REJECT iff no huge backing formed, or every improvement is
  < 5% with no regression (no benefit to buy), or a metric
  regresses > 10% while another wins (MIXED - named).
Grades use the roadmap words: WIN / TIE / LOSS / MIXED.
"""
import statistics
import sys

SIZES = [1048576, 2097152, 20971520, 209715200]
LABELS = {1048576: "1MiB-subPMD", 2097152: "2MiB",
          20971520: "20MiB", 209715200: "200MiB"}


def parse(path):
    med = {}
    verdicts = []
    for line in open(path):
        line = line.strip()
        if line.startswith("median,"):
            kv = {}
            for part in line.split(",")[1:]:
                if "=" in part:
                    k, v = part.split("=", 1)
                    kv[k] = v
            key = (int(kv["size"]), kv["mode"], kv["phase"])
            med[key] = kv
        elif line.startswith("verdict,"):
            verdicts.append(line)
    return med, verdicts


def main():
    runs = []
    all_verdicts = []
    for path in sys.argv[1:]:
        med, verdicts = parse(path)
        runs.append(med)
        all_verdicts.append(verdicts)
    print("synthesis: median-of-medians across %d executions"
          % len(runs))
    for size in SIZES:
        def med(mode, phase, field, cast=float):
            vals = []
            for med in runs:
                kv = med.get((size, mode, phase))
                if kv is not None and field in kv:
                    vals.append(cast(kv[field]))
            return statistics.median(vals) if vals else None

        f_off = med("off", "fresh", "fault_ns_per_page")
        f_on = med("on", "fresh", "fault_ns_per_page")
        w_off = med("off", "fresh", "write_mibs")
        w_on = med("on", "fresh", "write_mibs")
        r_off = med("off", "fresh", "read_mibs")
        r_on = med("on", "fresh", "read_mibs")
        mf_off = med("off", "fresh", "minflt_delta")
        mf_on = med("on", "fresh", "minflt_delta")
        hg_f = med("on", "fresh", "anon_huge_kib")
        rf_off = med("off", "dontneed", "refault_ns_per_page")
        rf_on = med("on", "dontneed", "refault_ns_per_page")
        rmf_off = med("off", "dontneed", "minflt_delta")
        rmf_on = med("on", "dontneed", "minflt_delta")
        hg_r = med("on", "dontneed", "anon_huge_kib")
        dn_off = med("off", "dontneed", "rss_after_dontneed_kib")
        dn_on = med("on", "dontneed", "rss_after_dontneed_kib")
        ru_off = med("off", "resident", "reuse_memset_ns")
        ru_on = med("on", "resident", "reuse_memset_ns")

        def imp(off, on, invert=False):
            if off in (None, 0) or on is None:
                return None
            if invert:
                return (off - on) / off * 100.0
            return (on - off) / off * 100.0

        fimp = imp(f_off, f_on, invert=True)
        wimp = imp(w_off, w_on)
        rimp = imp(r_off, r_on)
        rfimp = imp(rf_off, rf_on, invert=True)
        ruimp = imp(ru_off, ru_on, invert=True)

        def fmt(v, suffix=""):
            return "n/a" if v is None else "%.1f%s" % (v, suffix)

        print("size=%d label=%s" % (size, LABELS[size]))
        print("  fresh: fault/page off=%s on=%s improve=%s; "
              "minflt off=%s on=%s; write off=%s on=%s improve=%s; "
              "read off=%s on=%s improve=%s; anon_huge_on=%s KiB"
              % (fmt(f_off, "ns"), fmt(f_on, "ns"), fmt(fimp, "%"),
                 fmt(mf_off), fmt(mf_on),
                 fmt(w_off), fmt(w_on), fmt(wimp, "%"),
                 fmt(r_off), fmt(r_on), fmt(rimp, "%"),
                 fmt(hg_f)))
        print("  resident: reuse_memset off=%s on=%s improve=%s"
              % (fmt(ru_off, "ns"), fmt(ru_on, "ns"),
                 fmt(ruimp, "%")))
        print("  dontneed: rss_after off=%s on=%s KiB; refault/page "
              "off=%s on=%s improve=%s; minflt off=%s on=%s; "
              "anon_huge_on=%s KiB"
              % (fmt(dn_off), fmt(dn_on),
                 fmt(rf_off, "ns"), fmt(rf_on, "ns"),
                 fmt(rfimp, "%"), fmt(rmf_off), fmt(rmf_on),
                 fmt(hg_r)))
        gains = [v for v in (fimp, rfimp) if v is not None]
        regs = [v for v in (fimp, wimp, rimp, rfimp)
                if v is not None]
        huge = (hg_f or 0) > 0 or (hg_r or 0) > 0
        if not huge and all(abs(v) < 10 for v in regs):
            grade, decision = "TIE-no-huge-backing", "REJECT-no-effect"
        elif not huge:
            grade, decision = "LOSS-no-huge-backing", "REJECT"
        elif any(v <= -10 for v in regs) and \
                any(v >= 10 for v in gains + [wimp or 0]):
            grade, decision = "MIXED", "REJECT-mixed"
        elif (max(gains) >= 10 if gains else False) or \
                (wimp is not None and wimp >= 5):
            if all(v >= -10 for v in regs):
                grade, decision = "WIN", "ADOPT"
            else:
                grade, decision = "MIXED", "REJECT-mixed"
        elif all(-5 < v < 5 for v in regs if v is not None):
            grade, decision = "TIE", "REJECT-no-benefit"
        elif any(v <= -10 for v in regs):
            grade, decision = "LOSS", "REJECT"
        else:
            grade, decision = "TIE", "REJECT-no-benefit"
        print("  grade=%s decision=%s" % (grade, decision))
    print("per-execution probe verdicts:")
    for i, verdicts in enumerate(all_verdicts, 1):
        for v in verdicts:
            print("  run%d %s" % (i, v))


if __name__ == "__main__":
    main()
