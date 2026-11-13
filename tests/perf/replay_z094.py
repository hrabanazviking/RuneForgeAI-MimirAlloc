#!/usr/bin/env python3
"""Z-094 replay harness: re-runs a canonical trace against
any allocator leg and proves the replay checksum matches.

Legs:
  glibc  - libc.so.6 malloc family via ctypes (the capture
           leg's allocator: an honest self-replay control)
  mimir  - build/libmimiralloc.so namespaced mimir_*
           entries via ctypes (the leg Z-095 tunes for)

The checksum is FNV-1a 64 (plus SHA-256) over the outcome
stream the replay itself emits, in the summariser's
canonical grammar. A MATCH certifies: every allocation,
realloc and free in the trace was serviced by this leg
with the same logical outcome (success / failure /
retire) the capture recorded, the live-set arithmetic
(requested bytes) tracks to the same peak and the same
final residual.

Deliberate divergences, counted and reported, never
allowed to falsify the checksum:
- Zero-size allocations (decision D-013): glibc returns a
  distinct freeable pointer, MimirAlloc returns NULL.
  Both are normalised to the traced id in the outcome
  stream; the count is printed as d013_zero_size.
- realloc(p, 0) (D-013 again): glibc frees and returns
  NULL; MimirAlloc returns NULL and leaves the block
  LIVE. The harness completes the traced retire on the
  mimir leg with an explicit free and counts
  d013_realloc_zero.
Any OTHER outcome difference (an allocation that fails
on this leg but succeeded in the trace, a realloc that
succeeds where the trace recorded failure, a calloc that
is not zero-filled, a free of an id the replay does not
hold live) increments failures and breaks the checksum:
the harness exits 1 and says so.

Every successful allocation is touched (first and last
byte written; calloc's first bytes verified zero) so the
leg really faults the pages in. Requested sizes are
honoured exactly, including the trace's large blocks.

Usage: replay_z094.py TRACE.gz --leg glibc|mimir
         [--mimir-so PATH] [--progress-every N]
Prints exactly one REPLAY line; exit 0 iff MATCH.
"""
import argparse
import ctypes
import gzip
import hashlib
import sys

FNV_OFF = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
MASK64 = (1 << 64) - 1


class Leg:
    def __init__(self, name, mimir_so):
        self.name = name
        if name == "glibc":
            lib = ctypes.CDLL("libc.so.6", use_errno=True)
            self.malloc = lib.malloc
            self.calloc = lib.calloc
            self.realloc = lib.realloc
            self.free = lib.free
            self.aligned = lib.aligned_alloc
        else:
            lib = ctypes.CDLL(mimir_so, use_errno=True)
            self.malloc = lib.mimir_malloc
            self.calloc = lib.mimir_calloc
            self.realloc = lib.mimir_realloc
            self.free = lib.mimir_free
            self.aligned = lib.mimir_aligned_alloc
        self.malloc.restype = ctypes.c_void_p
        self.malloc.argtypes = [ctypes.c_size_t]
        self.calloc.restype = ctypes.c_void_p
        self.calloc.argtypes = [ctypes.c_size_t, ctypes.c_size_t]
        self.aligned.restype = ctypes.c_void_p
        self.aligned.argtypes = [ctypes.c_size_t, ctypes.c_size_t]
        self.realloc.restype = ctypes.c_void_p
        self.realloc.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        self.free.restype = None
        self.free.argtypes = [ctypes.c_void_p]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("trace")
    ap.add_argument("--leg", required=True,
                    choices=["glibc", "mimir"])
    ap.add_argument("--mimir-so", default="build/libmimiralloc.so")
    args = ap.parse_args()

    with gzip.open(args.trace, "rt", encoding="ascii") as fh:
        text = fh.read()
    header = {}
    ops = []
    for line in text.splitlines():
        if line.startswith("#"):
            if ":" in line:
                k, v = line[1:].split(":", 1)
                header[k.strip()] = v.strip()
        elif line:
            ops.append(line)
    expected_fnv = header["canonical_checksum_fnv1a64"]
    expected_sha = header["ops_sha256"]
    expected_ops = int(header["ops_total"])
    expected_peak = int(header["peak_live_bytes"])
    expected_flc = int(header["final_live_count"])
    expected_flb = int(header["final_live_bytes"])

    leg = Leg(args.leg, args.mimir_so)
    ptrs = {}   # logical id -> address or None (zero-size)
    sizes = {}  # logical id -> requested bytes
    live_bytes = 0
    peak = 0
    failures = 0
    d013_zero = 0
    d013_rz = 0
    calloc_dirty = 0
    fnv = FNV_OFF
    sha = hashlib.sha256()

    def emit(line: str):
        nonlocal fnv
        data = (line + "\n").encode("ascii")
        sha.update(data)
        h = fnv
        for b in data:
            h ^= b
            h = (h * FNV_PRIME) & MASK64
        fnv = h

    def touch(addr, size):
        if addr and size:
            ctypes.c_char.from_address(addr).value = b"\xa5"
            if size > 1:
                ctypes.c_char.from_address(addr + size - 1).value = b"\xa5"

    for line in ops:
        p = line.split()
        op = p[0]
        if op == "M":
            lid, size = int(p[1]), int(p[2])
            if lid == 0:
                r = leg.malloc(size)
                if r:
                    leg.free(r)
                    failures += 1  # trace failed, leg succeeded
                emit(f"M 0 {size}")
            elif size == 0:
                r = leg.malloc(0)
                if not r and leg.name == "mimir":
                    d013_zero += 1  # D-013 NULL, glibc gave a ptr
                elif not r:
                    failures += 1
                ptrs[lid] = r
                sizes[lid] = 0
                emit(f"M {lid} 0")
            else:
                r = leg.malloc(size)
                if not r:
                    failures += 1
                    emit(f"M 0 {size}")
                else:
                    touch(r, size)
                    ptrs[lid] = r
                    sizes[lid] = size
                    live_bytes += size
                    peak = max(peak, live_bytes)
                    emit(f"M {lid} {size}")
        elif op == "C":
            lid, nmemb, size = int(p[1]), int(p[2]), int(p[3])
            total = nmemb * size
            if lid == 0:
                r = leg.calloc(nmemb, size)
                if r:
                    leg.free(r)
                    failures += 1
                emit(f"C 0 {nmemb} {size}")
            elif total == 0:
                r = leg.calloc(nmemb, size)
                if not r and leg.name == "mimir":
                    d013_zero += 1
                elif not r:
                    failures += 1
                ptrs[lid] = r
                sizes[lid] = 0
                emit(f"C {lid} {nmemb} {size}")
            else:
                r = leg.calloc(nmemb, size)
                if not r:
                    failures += 1
                    emit(f"C 0 {nmemb} {size}")
                else:
                    head = ctypes.string_at(r, min(total, 64))
                    if any(head):
                        calloc_dirty += 1
                        failures += 1
                    touch(r, total)
                    ptrs[lid] = r
                    sizes[lid] = total
                    live_bytes += total
                    peak = max(peak, live_bytes)
                    emit(f"C {lid} {nmemb} {size}")
        elif op == "A":
            lid, align, size = int(p[1]), int(p[2]), int(p[3])
            if lid == 0:
                r = leg.aligned(align, size)
                if r:
                    leg.free(r)
                    failures += 1
                emit(f"A 0 {align} {size}")
            elif size == 0:
                r = leg.aligned(align, 0)
                if not r and leg.name == "mimir":
                    d013_zero += 1
                elif not r:
                    failures += 1
                ptrs[lid] = r
                sizes[lid] = 0
                emit(f"A {lid} {align} 0")
            else:
                r = leg.aligned(align, size)
                if not r:
                    failures += 1
                    emit(f"A 0 {align} {size}")
                else:
                    if r % align:
                        failures += 1  # alignment not honoured
                    touch(r, size)
                    ptrs[lid] = r
                    sizes[lid] = size
                    live_bytes += size
                    peak = max(peak, live_bytes)
                    emit(f"A {lid} {align} {size}")
        elif op == "R":
            old, new, size = int(p[1]), int(p[2]), int(p[3])
            if new == 0:
                # traced retire: realloc(p, 0), glibc freed it
                addr = ptrs.pop(old, None)
                oldsz = sizes.pop(old, 0)
                if addr:
                    r = leg.realloc(addr, 0)
                    if leg.name == "mimir":
                        # D-013: block still live; complete
                        # the traced retire explicitly
                        leg.free(addr)
                        d013_rz += 1
                    elif r:
                        failures += 1
                    live_bytes -= oldsz
                emit(f"R {old} 0 0")
            else:
                addr = ptrs.get(old)
                oldsz = sizes.get(old, 0)
                r = leg.realloc(addr, size) if addr else \
                    leg.realloc(None, size)
                if not r and size:
                    failures += 1
                    emit(f"E {old} {size}")
                else:
                    if r:
                        touch(r, size)
                    ptrs[old] = r
                    sizes[old] = size
                    live_bytes += size - oldsz
                    peak = max(peak, live_bytes)
                    emit(f"R {old} {old} {size}")
        elif op == "E":
            old, size = int(p[1]), int(p[2])
            addr = ptrs.get(old)
            r = leg.realloc(addr, size) if addr else None
            if r:
                # trace recorded failure; this leg succeeded:
                # an outcome difference, honestly a failure
                # of equivalence. Shrink back to the traced
                # size so the run can continue.
                failures += 1
                r2 = leg.realloc(r, sizes.get(old, 0))
                if r2:
                    ptrs[old] = r2
                emit(f"R {old} {old} {sizes.get(old, 0)}")
            else:
                emit(f"E {old} {size}")
        elif op == "F":
            lid = int(p[1])
            if lid == 0:
                emit("F 0")
            else:
                addr = ptrs.pop(lid, None)
                oldsz = sizes.pop(lid, None)
                if oldsz is None:
                    failures += 1
                else:
                    live_bytes -= oldsz
                    if addr:
                        leg.free(addr)
                emit(f"F {lid}")
        else:
            print(f"REPLAY leg={args.leg} trace={args.trace} "
                  f"verdict=FAIL reason=unknown-op-{op}")
            return 1

    got_fnv = f"{fnv:016x}"
    got_sha = sha.hexdigest()
    match = (got_fnv == expected_fnv and got_sha == expected_sha
             and len(ops) == expected_ops and failures == 0
             and peak == expected_peak
             and len(ptrs) == expected_flc
             and live_bytes == expected_flb)
    print(f"REPLAY leg={args.leg} workload="
          f"{header.get('workload', '?')} ops={len(ops)} "
          f"checksum_fnv1a64={got_fnv} expected={expected_fnv} "
          f"ops_sha256_match={got_sha == expected_sha} "
          f"peak_live_bytes={peak} expected_peak={expected_peak} "
          f"final_live_count={len(ptrs)} expected={expected_flc} "
          f"final_live_bytes={live_bytes} expected={expected_flb} "
          f"failures={failures} calloc_dirty={calloc_dirty} "
          f"d013_zero_size={d013_zero} "
          f"d013_realloc_zero={d013_rz} "
          f"verdict={'MATCH' if match else 'MISMATCH'}")
    return 0 if match else 1


if __name__ == "__main__":
    sys.exit(main())
