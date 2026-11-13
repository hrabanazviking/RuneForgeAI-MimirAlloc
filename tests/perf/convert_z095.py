#!/usr/bin/env python3
"""convert_z095.py - Z-095 capture -> replay trace converter.

Reads a binary capture written by trace_capture_z095.c
(32-byte records: op byte, size, ptr, ptr2) and emits a
replay trace for probe_z095.c plus a statistics block.

Replay rules (each drop is counted, never silent):
- zero-size allocations are dropped (MimirAlloc's D-013
  contract refuses them by design; not replayable events),
  and their later frees/reallocs drop with them.
- allocations above 64 MiB are dropped with their frees
  (the replay probe's pattern writes would dominate the
  measurement; the histogram still counts them).
- a free of an unknown pointer (allocated before logging
  started or by an unhooked path) is dropped and counted.
- realloc of an unknown pointer is treated as a fresh
  allocation of the new size (that IS its semantics).
- realloc to size 0 drops as a free of the block (glibc
  semantics of the CAPTURED stream; the mimir leg never
  sees a zero-size event, matching the alloc rule above).
Replay stops at --max-ops ops (default 300000); statistics
(histogram) always cover the full capture.

Usage: convert_z095.py <capture.bin> <out.trace> [max_ops]
Prints the statistics block to stdout; exit 1 on a short /
malformed capture.
"""
import struct
import sys
from collections import Counter

REC = struct.Struct("<B7xQQQ")
CLASSES = [16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
           224, 256, 320, 384, 448, 512, 640, 768, 896, 1024]
MAX_KEEP = 64 * 1024 * 1024


def serving(size):
    for c in CLASSES:
        if size <= c:
            return c
    return None


def main():
    if len(sys.argv) < 3:
        print("usage: convert_z095.py <capture.bin> <out.trace> [max_ops]")
        return 1
    max_ops = int(sys.argv[3]) if len(sys.argv) > 3 else 300000
    data = open(sys.argv[1], "rb").read()
    if len(data) % REC.size != 0:
        print(f"CONVERT FAIL: capture size {len(data)} not a multiple of {REC.size}")
        return 1
    live = {}          # ptr -> replay idx (kept blocks only)
    dropped_ptrs = set()
    next_idx = 0
    out = []
    hist = Counter()   # serving class -> count (full capture)
    hist_large = 0
    hist_zero = 0
    total_reqs = Counter()
    drops = Counter()
    n_recs = 0
    for off in range(0, len(data), REC.size):
        op, size, ptr, ptr2 = REC.unpack_from(data, off)
        n_recs += 1
        op = chr(op)
        if op == 'A':
            total_reqs[size] += 1
            if size == 0:
                hist_zero += 1
                drops['zero_alloc'] += 1
                dropped_ptrs.add(ptr)
                continue
            c = serving(size)
            if c is None:
                hist_large += 1
            else:
                hist[c] += 1
            if size > MAX_KEEP:
                drops['huge_alloc'] += 1
                dropped_ptrs.add(ptr)
                continue
            idx = next_idx
            next_idx += 1
            live[ptr] = idx
            if len(out) < max_ops:
                out.append(f"A {size}")
        elif op == 'F':
            if ptr in live:
                idx = live.pop(ptr)
                if len(out) < max_ops:
                    out.append(f"F {idx}")
            else:
                drops['free_unknown_or_dropped'] += 1
                dropped_ptrs.discard(ptr)
        elif op == 'R':
            old, newp = ptr, ptr2
            if old == 0:
                # realloc(NULL, size) == allocation; the shim
                # logs it as R with ptr 0.
                total_reqs[size] += 1
                if size == 0:
                    hist_zero += 1
                    drops['zero_alloc'] += 1
                    dropped_ptrs.add(newp)
                    continue
                c = serving(size)
                if c is None:
                    hist_large += 1
                else:
                    hist[c] += 1
                if size > MAX_KEEP:
                    drops['huge_alloc'] += 1
                    dropped_ptrs.add(newp)
                    continue
                idx = next_idx
                next_idx += 1
                live[newp] = idx
                if len(out) < max_ops:
                    out.append(f"A {size}")
                continue
            if old in live:
                idx = live.pop(old)
                if size == 0:
                    if len(out) < max_ops:
                        out.append(f"F {idx}")
                    dropped_ptrs.add(newp)
                    continue
                total_reqs[size] += 1
                c = serving(size)
                if c is None:
                    hist_large += 1
                else:
                    hist[c] += 1
                if size > MAX_KEEP:
                    if len(out) < max_ops:
                        out.append(f"F {idx}")
                    drops['huge_realloc'] += 1
                    dropped_ptrs.add(newp)
                    continue
                live[newp] = idx
                if len(out) < max_ops:
                    out.append(f"R {idx} {size}")
            else:
                drops['realloc_unknown_or_dropped'] += 1
                if size and size <= MAX_KEEP:
                    idx = next_idx
                    next_idx += 1
                    live[newp] = idx
                    if len(out) < max_ops:
                        out.append(f"A {size}")
                else:
                    dropped_ptrs.add(newp)
    with open(sys.argv[2], "w") as fh:
        fh.write("\n".join(out) + "\n")
    total_small = sum(hist.values())
    total_all = total_small + hist_large + hist_zero
    print(f"records={n_recs} replay_ops={len(out)} live_at_end={len(live)}")
    print(f"requests_total={total_all} small={total_small} "
          f"large_over_1024={hist_large} zero={hist_zero}")
    for c in CLASSES:
        n = hist.get(c, 0)
        pct = (100.0 * n / total_small) if total_small else 0.0
        print(f"class {c}: {n} ({pct:.2f}% of small)")
    waste = 0
    req_bytes = 0
    for size, n in total_reqs.items():
        if size == 0 or size > 1024:
            continue
        c = serving(size)
        waste += (c - size) * n
        req_bytes += size * n
    if req_bytes:
        print(f"internal_waste_small: {waste} bytes over {req_bytes} "
              f"requested ({100.0 * waste / req_bytes:.2f}%)")
    for k in sorted(drops):
        print(f"drop {k}: {drops[k]}")
    # Hottest exact request sizes inside the small range.
    hot = [(s, n) for s, n in total_reqs.items() if 0 < s <= 1024]
    hot.sort(key=lambda kv: -kv[1])
    print("hottest_exact_sizes: " +
          ", ".join(f"{s}x{n}" for s, n in hot[:15]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
