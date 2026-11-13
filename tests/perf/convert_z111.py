#!/usr/bin/env python3
"""convert_z111.py - Z-111 canonical-trace converter.

Reads a banked Z-094 canonical trace (.trace.gz, grammar in
tests/perf/summarize_z094.py) and emits a Z-095-probe-format
trace (A <size> / F <idx> / R <idx> <size>, one op per line,
indices assigned in allocation order) that
tests/perf/probe_z095.c replays timed against any allocator
leg. This is the Z-111 route for applying Z-095's
pre-registered adoption bars to the BANKED canonical traces
(Z-095's OWED re-validation): the canonical streams are the
trace suite of record for Z-111.

Foldings, each dropped item counted and printed, never
silent (Z-095 converter precedent):
- C id n s        -> A (n*s)
- A id align size -> A size (alignment folded, counted)
- M id 0 / C/A total 0 -> dropped with its later F/R
  (MimirAlloc's D-013 contract returns NULL for zero-size;
  not a replayable event), counted
- R old 0 0 (retire form) -> F
- E old size (failed realloc) -> no replay op (the block is
  unchanged), counted
- F 0 (foreign pointer) -> no replay op, counted
Every other op is emitted. The converter verifies that the
emitted stream reproduces the header's peak_live_bytes and
final_live_bytes exactly (requested-byte accounting is
folding-invariant) and prints both checks; a mismatch is a
hard failure (exit 1).

Usage: convert_z111.py IN.trace.gz OUT.trace
Prints one summary line to stdout.
"""
import gzip
import hashlib
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: convert_z111.py IN.trace.gz OUT.trace")
        return 2
    header = {}
    ops = []
    with gzip.open(sys.argv[1], "rt", encoding="ascii") as fh:
        for line in fh:
            line = line.strip()
            if line.startswith("#"):
                if ":" in line:
                    k, v = line[1:].split(":", 1)
                    header[k.strip()] = v.strip()
            elif line:
                ops.append(line.split())

    live = {}          # canonical id -> probe idx
    live_bytes = {}    # canonical id -> requested size
    dropped = set()    # canonical ids dropped (zero-size)
    out = []
    counts = {"M": 0, "C": 0, "A": 0, "R": 0, "E": 0, "F": 0,
              "zero_alloc_dropped": 0, "zero_ref_dropped": 0,
              "aligned_folded": 0, "foreign_free": 0,
              "unknown_ref": 0}
    cur = 0
    peak = 0

    # Probe indices are allocation-order positions, built in
    # emission order during the pass itself because
    # allocations are never reordered.
    next_idx = 0
    emitted = []
    for p in ops:
        kind = p[0]
        if kind == "M":
            cid, size = int(p[1]), int(p[2])
            counts["M"] += 1
            if size == 0:
                dropped.add(cid)
                counts["zero_alloc_dropped"] += 1
                continue
            live[cid] = next_idx
            live_bytes[cid] = size
            next_idx += 1
            emitted.append(f"A {size}")
            cur += size
        elif kind == "C":
            cid, total = int(p[1]), int(p[2]) * int(p[3])
            counts["C"] += 1
            if total == 0:
                dropped.add(cid)
                counts["zero_alloc_dropped"] += 1
                continue
            live[cid] = next_idx
            live_bytes[cid] = total
            next_idx += 1
            emitted.append(f"A {total}")
            cur += total
        elif kind == "A":
            cid, size = int(p[1]), int(p[3])
            counts["A"] += 1
            counts["aligned_folded"] += 1
            if size == 0:
                dropped.add(cid)
                counts["zero_alloc_dropped"] += 1
                continue
            live[cid] = next_idx
            live_bytes[cid] = size
            next_idx += 1
            emitted.append(f"A {size}")
            cur += size
        elif kind == "R":
            old, new, size = int(p[1]), int(p[2]), int(p[3])
            counts["R"] += 1
            if old in dropped:
                counts["zero_ref_dropped"] += 1
                continue
            if old not in live:
                counts["unknown_ref"] += 1
                continue
            if new == 0 and size == 0:
                emitted.append(f"F {live[old]}")
                cur -= live_bytes[old]
                del live[old], live_bytes[old]
            else:
                emitted.append(f"R {live[old]} {size}")
                cur += size - live_bytes[old]
                live_bytes[old] = size
        elif kind == "E":
            counts["E"] += 1  # failed realloc: no state change
        elif kind == "F":
            cid = int(p[1])
            counts["F"] += 1
            if cid == 0:
                counts["foreign_free"] += 1
                continue
            if cid in dropped:
                counts["zero_ref_dropped"] += 1
                continue
            if cid not in live:
                counts["unknown_ref"] += 1
                continue
            emitted.append(f"F {live[cid]}")
            cur -= live_bytes[cid]
            del live[cid], live_bytes[cid]
        else:
            print(f"FAIL: unknown op {kind}")
            return 1
        if cur > peak:
            peak = cur

    final_bytes = sum(live_bytes.values())
    exp_peak = int(header["peak_live_bytes"])
    exp_final = int(header["final_live_bytes"])
    if peak != exp_peak or final_bytes != exp_final:
        print(f"FAIL: live-byte check peak {peak} vs header "
              f"{exp_peak}; final {final_bytes} vs {exp_final}")
        return 1
    if counts["unknown_ref"]:
        print(f"FAIL: {counts['unknown_ref']} unknown references")
        return 1

    blob = ("\n".join(emitted) + "\n").encode()
    with open(sys.argv[2], "wb") as fh:
        fh.write(blob)
    print(f"CONVERT {header.get('workload', '?')}: "
          f"in_ops={len(ops)} out_ops={len(emitted)} "
          f"M={counts['M']} C={counts['C']} A={counts['A']} "
          f"R={counts['R']} E={counts['E']} F={counts['F']} "
          f"zero_alloc_dropped={counts['zero_alloc_dropped']} "
          f"zero_ref_dropped={counts['zero_ref_dropped']} "
          f"aligned_folded={counts['aligned_folded']} "
          f"foreign_free={counts['foreign_free']} "
          f"peak_live_bytes={peak} final_live_bytes={final_bytes} "
          f"sha256={hashlib.sha256(blob).hexdigest()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
