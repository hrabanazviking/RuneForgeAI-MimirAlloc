#!/usr/bin/env python3
"""summarize_z023.py - Z-023 large-path anatomy summariser.

Reads the raw CSV written by run_z023.sh (one line per
process: phase,leg,size,live,iters,total_ns,ns_per_op,
minflt_delta,checksum) and prints the decomposition the
roadmap gate names: per size and for the large_batch shape,
what share of the large-path cost is the mmap/munmap pair,
what share is bookkeeping, what share is faults - plus the
KL-C wound (mimir vs glibc) per size and in aggregate.

Headline = median of per-process ns_per_op (Z-004 protocol
section 6). Drift = split-half drift of per-process values
in run order (section 8, bound 8%). Checksums: exactly one
value per comparable family or the family is flagged
(section 9): op/mimir, op/glibc and raw/touch share the
endpoint-touch pattern per (size, iters) and must agree;
batch and batchraw/touch must equal the K-002 large_batch
checksum 1106008.
"""
import statistics
import sys
from collections import defaultdict

K002_LARGE_BATCH_CHECKSUM = 1106008

rows = []
with open(sys.argv[1]) as fh:
    for line in fh:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        f = line.split(",")
        rows.append({
            "phase": f[0], "leg": f[1], "size": int(f[2]),
            "live": int(f[3]), "iters": int(f[4]),
            "total_ns": int(f[5]), "ns_per_op": float(f[6]),
            "minflt": int(f[7]), "checksum": int(f[8]),
        })

groups = defaultdict(list)
checks = defaultdict(set)
faults = {}
for r in rows:
    key = (r["phase"], r["leg"], r["size"], r["live"])
    groups[key].append(r["ns_per_op"])
    checks[key].add(r["checksum"])
    if r["iters"] > 0 and r["minflt"] >= 0:
        faults.setdefault(key, []).append(
            r["minflt"] / r["iters"])


def med(key):
    vals = groups.get(key)
    return statistics.median(vals) if vals else None


def spread(key):
    vals = groups.get(key)
    return (min(vals), max(vals)) if vals else (None, None)


def drift(key):
    vals = groups.get(key)
    if not vals or len(vals) < 4:
        return None
    half = len(vals) // 2
    a = statistics.median(vals[:half])
    b = statistics.median(vals[half:])
    m = statistics.median(vals)
    return 100.0 * abs(b - a) / m if m else None


def fault_per_op(key):
    vals = faults.get(key)
    return statistics.median(vals) if vals else None


print("Z-023 large-path anatomy summary (ns/op; headline = "
      "median of processes)")
print("checksum sets per group must hold exactly one value; "
      "drift bound 8% (Z-004 s.8)")
print()
SIZES = [2048, 8192, 16384, 65536, 262144, 1048576]
print("=== per-process medians by group ===")
for key in sorted(groups):
    lo, hi = spread(key)
    d = drift(key)
    ck = sorted(checks[key])
    print(f"{key[0]:>8} {key[1]:<9} size={key[2]:>7} "
          f"live={key[3]:>4}  med={med(key):10.2f}  "
          f"spread {lo:.2f}..{hi:.2f}  "
          f"drift {'n/a' if d is None else format(d, '.1f') + '%'}  "
          f"n={len(groups[key])}  checksums={ck}")
print()
print("=== checksum discipline ===")
bad = 0
for size in SIZES:
    fams = [(("op", "mimir", size, 1)),
            (("op", "glibc", size, 1)),
            (("raw", "touch", size, 1))]
    vals = set()
    for k in fams:
        vals |= checks.get(k, set())
    ok = len(vals) == 1
    if not ok:
        bad += 1
    print(f"size {size}: endpoint-touch family checksums "
          f"{sorted(vals)} -> {'OK' if ok else 'FAIL'}")
for key in [("batch", "mimir", 0, 32), ("batch", "glibc", 0, 32),
            ("batchraw", "touch", 0, 32)]:
    vals = checks.get(key, set())
    ok = vals == {K002_LARGE_BATCH_CHECKSUM}
    if not ok:
        bad += 1
    print(f"{key}: checksums {sorted(vals)} vs K-002 "
          f"{K002_LARGE_BATCH_CHECKSUM} -> "
          f"{'OK' if ok else 'FAIL'}")
nt = checks.get(("batchraw", "notouch", 0, 32), set())
print(f"batchraw/notouch checksums {sorted(nt)} "
      f"(expect {{0}}: no touch, no checksum work)")
if nt != {0}:
    bad += 1
print("checksum verdict:", "ALL OK" if bad == 0 else
      f"{bad} FAMILY FAILURES")
print()
print("=== DECOMPOSITION per size (single block live) ===")
print("total = op/mimir; pair = raw/notouch; faults = "
      "raw/touch - raw/notouch;")
print("bookkeeping = residual op/mimir - raw/touch (lock, "
      "registry, descriptor,")
print("counters, dispatch + non-additivity between legs).")
hdr = (f"{'size':>8} {'total':>9} {'glibc':>9} {'pair':>9} "
       f"{'pair%':>6} {'faults':>9} {'flt%':>6} {'book':>9} "
       f"{'book%':>6} {'wound':>9} {'ratio':>6} "
       f"{'flt/op':>7} {'fullflt':>9}")
print(hdr)
agg = {"total": 0.0, "pair": 0.0, "faults": 0.0, "book": 0.0,
       "glibc": 0.0}
for size in SIZES:
    total = med(("op", "mimir", size, 1))
    glibc = med(("op", "glibc", size, 1))
    pair = med(("raw", "notouch", size, 1))
    touch = med(("raw", "touch", size, 1))
    full = med(("raw", "fulltouch", size, 1))
    flt = touch - pair
    book = total - touch
    for k, v in (("total", total), ("pair", pair),
                 ("faults", flt), ("book", book),
                 ("glibc", glibc)):
        agg[k] += v
    fpo = fault_per_op(("raw", "touch", size, 1))
    print(f"{size:>8} {total:9.2f} {glibc:9.2f} {pair:9.2f} "
          f"{100 * pair / total:6.1f} {flt:9.2f} "
          f"{100 * flt / total:6.1f} {book:9.2f} "
          f"{100 * book / total:6.1f} {total - glibc:9.2f} "
          f"{total / glibc:6.2f} "
          f"{'n/a' if fpo is None else format(fpo, '.3f'):>7} "
          f"{full - touch:9.2f}")
n = float(len(SIZES))
print(f"mean of sizes: total {agg['total'] / n:.2f}  "
      f"pair {agg['pair'] / n:.2f} "
      f"({100 * agg['pair'] / agg['total']:.1f}%)  "
      f"faults {agg['faults'] / n:.2f} "
      f"({100 * agg['faults'] / agg['total']:.1f}%)  "
      f"book {agg['book'] / n:.2f} "
      f"({100 * agg['book'] / agg['total']:.1f}%)  "
      f"glibc {agg['glibc'] / n:.2f}  "
      f"wound {(agg['total'] - agg['glibc']) / n:.2f} ns/op  "
      f"ratio {agg['total'] / agg['glibc']:.2f}x")
print("(fullflt = extra cost if every user page were touched "
      "= raw/fulltouch - raw/touch;")
print(" the bench touches endpoints only, so the headline "
      "fault share is the")
print(" sparse-touch share; fullflt prices the dense-touch "
      "world.)")
print()
print("=== BATCH SHAPE (large_batch, 32 live, forward free) ===")
bm = med(("batch", "mimir", 0, 32))
bg = med(("batch", "glibc", 0, 32))
bt = med(("batchraw", "touch", 0, 32))
bn = med(("batchraw", "notouch", 0, 32))
print(f"batch mimir    {bm:10.2f} ns/op")
print(f"batch glibc    {bg:10.2f} ns/op")
print(f"batchraw touch {bt:10.2f} ns/op (pair+faults, zero "
      f"bookkeeping)")
print(f"batchraw notouch {bn:10.2f} ns/op (pair only, batch "
      f"shape)")
print(f"KL-C wound (batch): {bm - bg:.2f} ns/op, ratio "
      f"{bm / bg:.2f}x")
print(f"batch shares of mimir total: pair "
      f"{100 * bn / bm:.1f}%  faults {100 * (bt - bn) / bm:.1f}%"
      f"  bookkeeping-residual {100 * (bm - bt) / bm:.1f}%")
fpo_b = fault_per_op(("batch", "mimir", 0, 32))
fpo_r = fault_per_op(("batchraw", "touch", 0, 32))
print(f"minor faults/op: batch mimir "
      f"{'n/a' if fpo_b is None else format(fpo_b, '.3f')}, "
      f"batchraw touch "
      f"{'n/a' if fpo_r is None else format(fpo_r, '.3f')}")
print()
print("=== REGISTRY WALK (65536-byte blocks, forward free) ===")
print(f"{'live':>6} {'mimir free':>11} {'raw munmap':>11} "
      f"{'walk+book':>10} {'per-step':>9}")
prev = None
for live in (1, 32, 256, 1024):
    m = med(("walk", "mimir", 65536, live))
    r = med(("walk", "raw", 65536, live))
    step = ""
    if prev is not None and live > prev[0]:
        step = f"{(m - prev[1]) / (live - prev[0]):9.3f}"
    print(f"{live:>6} {m:11.2f} {r:11.2f} {m - r:10.2f} "
          f"{step:>9}")
    prev = (live, m)
print("(per-step = added ns per free for each additional live "
      "mapping,")
print(" between successive live counts; the registry walk is "
      "O(live) with")
print(" oldest-first free order = deepest walk first.)")
