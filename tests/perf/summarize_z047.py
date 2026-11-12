#!/usr/bin/env python3
"""summarize_z047.py - Z-047 mapping-cache design summariser.
Reads the raw OP CSV written by run_z047.sh and the census
CSV, prints:
  - per-process medians by (leg, size) with spread, split-
    half drift (Z-004 s.8, bound 8%: above = DRIFT-REJECT,
    indicative only) and mean minor faults per op
  - checksum discipline (exactly one checksum per leg/size)
  - the design pricing table: fresh (banked geometry price)
    vs poolhit raw floor, and the dense trio (freshdense /
    pooldense / pooldontneed) at 1 MiB and 8 MiB
  - the census table verbatim with VMA/RSS deltas vs base
Usage: summarize_z047.py <raw-file> <census-file>
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
    raw_path, cen_path = sys.argv[1], sys.argv[2]
    groups = defaultdict(list)
    faults = defaultdict(list)
    checks = defaultdict(set)
    with open(raw_path) as f:
        for line in f:
            parts = line.strip().split(",")
            if len(parts) != 7 or parts[0] != "OP":
                continue
            leg, size = parts[1], int(parts[2])
            nspo = float(parts[4])
            mfpo = float(parts[5]) / 1000.0
            chk = int(parts[6])
            groups[(leg, size)].append(nspo)
            faults[(leg, size)].append(mfpo)
            checks[(leg, size)].add(chk)
    med = {k: median(v) for k, v in groups.items()}

    print("Z-047 mapping-cache design summary (ns/op; headline = median of processes)")
    print("drift bound 8% (Z-004 s.8); DRIFT-REJECT = indicative only")
    print()
    print("=== per-process medians by group ===")
    for k in sorted(groups, key=lambda k: (k[1], k[0])):
        vals = groups[k]
        d = drift_pct(vals)
        flag = "  DRIFT-REJECT" if d > 8.0 else ""
        print("  op %-12s size=%8d  med=%10.2f  spread %.2f..%.2f  "
              "drift %.1f%%  n=%d  minflt/op=%.3f  checksums=%s%s"
              % (k[0], k[1], med[k], min(vals), max(vals), d,
                 len(vals), median(faults[k]),
                 sorted(checks[k]), flag))
    print()
    print("=== sparse pricing: fresh vs pool-hit floor ===")
    print("  (poolhit is the raw no-syscall floor: touch only;")
    print("   allocator bookkeeping is priced separately in the")
    print("   design doc from Z-046's residual, never added here)")
    for size in sorted({s for (l, s) in groups if l == "fresh"}):
        fr = med.get(("fresh", size))
        ph = med.get(("poolhit", size))
        if fr and ph:
            print("  size=%8d  fresh=%9.2f  poolhit=%8.2f  "
                  "syscall+fault share=%.1f%%  saved=%9.2f ns/op"
                  % (size, fr, ph, 100.0 * (fr - ph) / fr, fr - ph))
    print()
    print("=== dense pricing (full re-touch per op) ===")
    for size in sorted({s for (l, s) in groups if l == "freshdense"}):
        fd = med.get(("freshdense", size))
        pd = med.get(("pooldense", size))
        dn = med.get(("pooldontneed", size))
        print("  size=%8d  freshdense=%11.2f  pooldense=%11.2f  "
              "pooldontneed=%11.2f" % (size, fd, pd, dn))
        if fd and pd:
            print("             resident reuse saves %.1f%% vs fresh dense; "
                  "DONTNEED reuse saves %.1f%%"
                  % (100.0 * (fd - pd) / fd, 100.0 * (fd - dn) / fd))
    print()
    print("=== census (VMA counts deterministic; RSS KiB as read) ===")
    rows = []
    with open(cen_path) as f:
        for line in f:
            parts = line.strip().split(",")
            if len(parts) == 5 and parts[0] == "CENSUS":
                rows.append((parts[1], int(parts[2]), int(parts[3]),
                             parts[4]))
    by_stage = defaultdict(list)
    for stage, vmas, rss, detail in rows:
        by_stage[stage].append((vmas, rss, detail))
    for stage in ["base", "live8", "pooled8", "pooled8_dontneed", "evicted"]:
        entries = by_stage.get(stage, [])
        if not entries:
            continue
        vmas = [e[0] for e in entries]
        rss = [e[1] for e in entries]
        print("  %-18s vmas=%s  rss_kib=%s  (%s)"
              % (stage, vmas, rss, entries[0][2]))
    base_v = median([e[0] for e in by_stage["base"]]) if by_stage.get("base") else 0
    for stage in ["live8", "pooled8", "pooled8_dontneed", "evicted"]:
        if by_stage.get(stage):
            dv = median([e[0] for e in by_stage[stage]]) - base_v
            print("  delta vs base: %-18s %+.0f VMAs (median)" % (stage, dv))
    print()
    print("SUMMARIZER OK")


if __name__ == "__main__":
    main()
