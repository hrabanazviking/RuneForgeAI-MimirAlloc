#!/usr/bin/env python3
"""Z-094 trace summariser: raw per-process audit-hook
streams (tests/perf/mimir_trace_z094.c) -> one canonical
logical trace.

Canonicalisation rules (the replay contract):
- Per-process shim ids are folded into one global logical
  id space; process streams are concatenated in the file
  order given on the command line (each stream is a whole
  process lifetime segment - lifetimes that overlapped in
  wall time replay as a sequence, which can only RAISE
  the replayed live set, never corrupt it: every free
  still names its own allocation).
- A realloc move (R old new size, new != old, new != 0)
  keeps ONE logical identity: later references to the new
  shim id resolve to the old logical id, and the canonical
  record is R <id> <id> <size>. An in-place realloc is
  already in that form.
- The retire form R <old> 0 0 and the failure form
  E <old> <size> pass through (ids translated).
- F 0 (foreign pointer) passes through; replay is a no-op
  for it, exactly as capture forwarded it.
- R with old id 0 (realloc of a foreign pointer) becomes
  M for the new pointer.
- A capped stream (a T record from the shim) is a hard
  error: a truncated trace never poses as complete.

The canonical op stream is hashed two ways over the exact
emitted op bytes: FNV-1a 64 (the replay checksum the
roadmap gate names) and SHA-256 (ops_sha256). The header
records counts, peak/final live bytes in REQUESTED sizes,
and the source command; the .gz is written with mtime 0
so the artefact is byte-reproducible from its raw streams.

Usage: summarize_z094.py --workload NAME --command STR
         --out OUT.trace.gz RAW [RAW ...]
Exit: 0 ok; 2 on any inconsistency (unbalanced free of a
live id twice, realloc of an unknown live id with a
recorded size change, capped stream, malformed record).
"""
import argparse
import gzip
import hashlib
import io
import sys

FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
MASK64 = (1 << 64) - 1


def fnv_update(h, data: bytes) -> int:
    for b in data:
        h ^= b
        h = (h * FNV_PRIME) & MASK64
    return h


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--workload", required=True)
    ap.add_argument("--command", required=True)
    ap.add_argument("--captured", required=True,
                    help="capture UTC timestamp string")
    ap.add_argument("--loadavg", default="unknown")
    ap.add_argument("--out", required=True)
    ap.add_argument("raw", nargs="+")
    args = ap.parse_args()

    ops = []  # canonical op lines (bytes, no newline)
    counts = {"M": 0, "C": 0, "A": 0, "R": 0, "E": 0, "F": 0}
    alloc_fail = 0
    live = {}  # logical id -> requested size
    live_bytes = 0
    peak_bytes = 0
    next_logical = 1
    procs = 0

    def die(msg):
        print(f"SUMMARIZE FAIL: {msg}", file=sys.stderr)
        return 2

    for rawpath in args.raw:
        procs += 1
        alias = {}  # shim id -> logical id (live pointers)
        with open(rawpath, "rb") as fh:
            raw_bytes = fh.read()
        if raw_bytes and not raw_bytes.endswith(b"\n"):
            return die(f"{rawpath}: torn final record "
                       f"(stream does not end at a record "
                       f"boundary)")
        with open(rawpath, "r", encoding="ascii") as fh:
            for lineno, line in enumerate(fh, 1):
                parts = line.split()
                if not parts:
                    continue
                op = parts[0]
                try:
                    vals = [int(x) for x in parts[1:]]
                except ValueError:
                    return die(f"{rawpath}:{lineno}: bad record {line!r}")
                if op == "T":
                    return die(f"{rawpath}:{lineno}: capped stream")
                if op == "M":
                    sid, size = vals
                    counts["M"] += 1
                    if sid == 0:
                        alloc_fail += 1
                        ops.append(f"M 0 {size}")
                    else:
                        lid = next_logical; next_logical += 1
                        alias[sid] = lid
                        live[lid] = size
                        live_bytes += size
                        peak_bytes = max(peak_bytes, live_bytes)
                        ops.append(f"M {lid} {size}")
                elif op == "C":
                    sid, nmemb, size = vals
                    counts["C"] += 1
                    if sid == 0:
                        alloc_fail += 1
                        ops.append(f"C 0 {nmemb} {size}")
                    else:
                        lid = next_logical; next_logical += 1
                        alias[sid] = lid
                        total = nmemb * size
                        live[lid] = total
                        live_bytes += total
                        peak_bytes = max(peak_bytes, live_bytes)
                        ops.append(f"C {lid} {nmemb} {size}")
                elif op == "A":
                    sid, align, size = vals
                    counts["A"] += 1
                    if sid == 0:
                        alloc_fail += 1
                        ops.append(f"A 0 {align} {size}")
                    else:
                        lid = next_logical; next_logical += 1
                        alias[sid] = lid
                        live[lid] = size
                        live_bytes += size
                        peak_bytes = max(peak_bytes, live_bytes)
                        ops.append(f"A {lid} {align} {size}")
                elif op == "R":
                    old, new, size = vals
                    counts["R"] += 1
                    if old == 0:
                        # realloc of a foreign pointer: the
                        # new pointer is a fresh allocation
                        if new == 0:
                            ops.append(f"M 0 {size}")
                            alloc_fail += 1
                        else:
                            lid = next_logical; next_logical += 1
                            alias[new] = lid
                            live[lid] = size
                            live_bytes += size
                            peak_bytes = max(peak_bytes, live_bytes)
                            ops.append(f"M {lid} {size}")
                        continue
                    lid = alias.get(old)
                    if lid is None:
                        return die(f"{rawpath}:{lineno}: realloc of "
                                   f"unknown shim id {old}")
                    if new == 0:
                        # retire form (size 0)
                        del alias[old]
                        live_bytes -= live.pop(lid)
                        ops.append(f"R {lid} 0 0")
                    else:
                        if new != old:
                            alias[new] = lid
                            del alias[old]
                        live_bytes += size - live[lid]
                        live[lid] = size
                        peak_bytes = max(peak_bytes, live_bytes)
                        ops.append(f"R {lid} {lid} {size}")
                elif op == "E":
                    old, size = vals
                    counts["E"] += 1
                    lid = alias.get(old)
                    if lid is None and old != 0:
                        return die(f"{rawpath}:{lineno}: realloc-fail "
                                   f"of unknown shim id {old}")
                    ops.append(f"E {lid if lid is not None else 0} {size}")
                elif op == "F":
                    (sid,) = vals
                    counts["F"] += 1
                    if sid == 0:
                        ops.append("F 0")
                    else:
                        lid = alias.pop(sid, None)
                        if lid is None:
                            return die(f"{rawpath}:{lineno}: double or "
                                       f"unknown free of shim id {sid}")
                        live_bytes -= live.pop(lid)
                        ops.append(f"F {lid}")
                else:
                    return die(f"{rawpath}:{lineno}: unknown op {op!r}")
        # pointers still live at this process's exit stay in
        # `live`: they are the trace's honest residual (real
        # programs exit with allocations outstanding; the
        # exit itself is the final free the OS performs).

    body = "".join(o + "\n" for o in ops).encode("ascii")
    fnv = fnv_update(FNV_OFF, body)
    sha = hashlib.sha256(body).hexdigest()
    header = (
        "# MIMIRALLOC Z-094 TRACE v1\n"
        f"# workload: {args.workload}\n"
        f"# command: {args.command}\n"
        f"# captured_utc: {args.captured}\n"
        f"# loadavg_at_capture: {args.loadavg}\n"
        f"# processes: {procs}\n"
        f"# ops_total: {len(ops)}\n"
        f"# ops_M: {counts['M']} ops_C: {counts['C']} "
        f"ops_A: {counts['A']} ops_R: {counts['R']} "
        f"ops_E: {counts['E']} ops_F: {counts['F']}\n"
        f"# alloc_failures: {alloc_fail}\n"
        f"# peak_live_bytes: {peak_bytes}\n"
        f"# final_live_count: {len(live)}\n"
        f"# final_live_bytes: {live_bytes}\n"
        f"# canonical_checksum_fnv1a64: {fnv:016x}\n"
        f"# ops_sha256: {sha}\n"
    )
    payload = header.encode("ascii") + body
    buf = io.BytesIO()
    with gzip.GzipFile(filename="", mode="wb", fileobj=buf,
                       mtime=0, compresslevel=9) as gz:
        gz.write(payload)
    with open(args.out, "wb") as fh:
        fh.write(buf.getvalue())
    print(f"SUMMARY workload={args.workload} procs={procs} "
          f"ops={len(ops)} M={counts['M']} C={counts['C']} "
          f"A={counts['A']} R={counts['R']} E={counts['E']} "
          f"F={counts['F']} alloc_failures={alloc_fail} "
          f"peak_live_bytes={peak_bytes} "
          f"final_live_count={len(live)} "
          f"final_live_bytes={live_bytes} "
          f"checksum_fnv1a64={fnv:016x} ops_sha256={sha} "
          f"gz_bytes={len(buf.getvalue())}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
