#!/usr/bin/env python3
# workload_z085.py - Z-085 Python workload (MimirAlloc Z13).
# A real CPython process doing the two things real programs do:
#   Phase A: an import storm (interpreter + stdlib module load,
#     thousands of small allocations through the C allocator for
#     module objects, code objects, interned strings).
#   Phase B: allocation churn (seeded pseudo-random mix of small
#     dict/list/str/bytes objects in a rolling window, plus large
#     bytearray blocks that are touched page by page and freed).
# Correctness oracle: a checksum accumulated over every object
# created; the runner compares the checksum across plain and
# LD_PRELOAD runs - any allocator corruption changes it or kills
# the process. Exit 0 only if all phases complete.
# Output: one PHASE line per phase with wall seconds, one PEAK
# line with VmHWM KiB from /proc/self/status, one CHECKSUM line.
import hashlib
import importlib
import sys
import time

IMPORTS = [
    "argparse", "ast", "asyncio", "collections", "concurrent.futures",
    "csv", "ctypes", "dataclasses", "datetime", "decimal", "email",
    "encodings", "fractions", "functools", "hashlib", "html", "http",
    "importlib", "inspect", "io", "itertools", "json", "logging",
    "multiprocessing", "os", "pathlib", "pickle", "pprint", "re",
    "sqlite3", "statistics", "string", "subprocess", "tarfile",
    "tempfile", "textwrap", "threading", "typing", "unittest",
    "urllib", "uuid", "xml.etree.ElementTree", "zipfile",
]

def phase_imports():
    t0 = time.perf_counter()
    for name in IMPORTS:
        importlib.import_module(name)
    return time.perf_counter() - t0

def phase_churn():
    t0 = time.perf_counter()
    h = hashlib.sha256()
    state = 0x2545F491
    def rnd():
        nonlocal state
        state = (1103515245 * state + 12345) & 0x7FFFFFFF
        return state
    window = []
    for i in range(300000):
        r = rnd()
        kind = r % 4
        if kind == 0:
            obj = {"k%d" % (r % 997): [r, str(r), float(r) / 7.0]}
            h.update(str(obj["k%d" % (r % 997)][0]).encode())
        elif kind == 1:
            obj = bytes([(r >> 3) & 0xFF]) * (16 + r % 4080)
            h.update(obj[:8])
        elif kind == 2:
            obj = "s%d-" % r + "x" * (r % 512)
            h.update(obj[:16].encode())
        else:
            obj = [(r + j) * 2654435761 & 0xFFFFFFFF for j in range(r % 64)]
            h.update(str(obj[-1]).encode())
        window.append(obj)
        if len(window) > 4096:
            del window[:2048]
    del window
    # Large-block leg: 24 x ~4 MiB bytearrays, touched per page.
    blocks = []
    for i in range(24):
        b = bytearray(4 * 1024 * 1024 + (i % 5) * 65536)
        for off in range(0, len(b), 4096):
            b[off] = (i + off) & 0xFF
        h.update(bytes(b[::1048576]))
        blocks.append(b)
        if len(blocks) > 6:
            blocks.pop(0)
    del blocks
    return time.perf_counter() - t0, h.hexdigest()

def main():
    ta = phase_imports()
    print("PHASE imports %.4f" % ta, flush=True)
    tb, digest = phase_churn()
    print("PHASE churn %.4f" % tb, flush=True)
    with open("/proc/self/status") as f:
        for line in f:
            if line.startswith("VmHWM:"):
                print("PEAK " + line.split()[1], flush=True)
    print("CHECKSUM " + digest, flush=True)
    print("TOTAL %.4f" % (ta + tb), flush=True)
    return 0

if __name__ == "__main__":
    sys.exit(main())
