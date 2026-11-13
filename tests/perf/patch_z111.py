#!/usr/bin/env python3
"""patch_z111.py - Z-111 per-class TLS bin variant patcher.

Rewrites a SCRATCH copy of src/core/mimir_core.c and
tests/contract/mimir_tls_cache_z028.h (production sources in
the worktree are never touched by the harness) so the TLS
cache uses per-class bin capacities and refill/drain batches
from the tables given on the command line:

  patch_z111.py CORE HDR --caps c0,..,c19 --batches b0,..,b19

Class order is the banked table (G-002): 16,32,48,64,80,96,
112,128,160,192,224,256,320,384,448,512,640,768,896,1024.

Mechanics: the header's MIMIR_TLS_BIN_CAPACITY becomes
max(caps) and MIMIR_TLS_BATCH becomes max(batches) - those
macros then size the bin storage arrays only. A z111_cap /
z111_batch table pair plus accessors is injected before
mimir_tls_cache_bin_capacity, whose body (and
mimir_tls_cache_batch's) returns the per-class values. The
three full-bin checks, the refill loop bound and the drain
batch body are rewritten to the accessors. Every rewrite is
count-asserted; a missed site aborts the build.

Requires batch[i] <= caps[i] for every class (the drain
precondition) and caps[i] <= 32 (bin_len is uint8_t and the
storage is sized by the header macro).
"""
import argparse
import re
import sys


def subn(text, pat, rep, want, label, regex=False):
    if regex:
        text2, n = re.subn(pat, rep, text, count=0)
    else:
        n = text.count(pat)
        text2 = text.replace(pat, rep)
    if n != want:
        print(f"FAIL: {label}: {n} replacements, want {want}")
        sys.exit(1)
    return text2


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("core")
    ap.add_argument("hdr")
    ap.add_argument("--caps", required=True)
    ap.add_argument("--batches", required=True)
    args = ap.parse_args()
    caps = [int(x) for x in args.caps.split(",")]
    batches = [int(x) for x in args.batches.split(",")]
    assert len(caps) == 20 and len(batches) == 20, "need 20+20"
    for c, b in zip(caps, batches):
        assert 1 <= b <= c <= 32, f"bad pair cap={c} batch={b}"

    hdr = open(args.hdr).read()
    hdr = subn(hdr,
               "#define MIMIR_TLS_BIN_CAPACITY ((size_t)16)",
               f"#define MIMIR_TLS_BIN_CAPACITY ((size_t){max(caps)})",
               1, "hdr cap")
    hdr = subn(hdr,
               "#define MIMIR_TLS_BATCH ((size_t)8)",
               f"#define MIMIR_TLS_BATCH ((size_t){max(batches)})",
               1, "hdr batch")
    open(args.hdr, "w").write(hdr)

    src = open(args.core).read()
    tables = (
        "/* Z-111 variant tables (scratch build only, patched\n"
        " * in by tests/perf/patch_z111.py): per-class TLS bin\n"
        " * capacity and refill/drain batch, class order = the\n"
        " * banked G-002 table. */\n"
        "static const uint8_t z111_cap[20] = {"
        + ",".join(str(c) for c in caps) + "};\n"
        "static const uint8_t z111_batch[20] = {"
        + ",".join(str(b) for b in batches) + "};\n"
        "static size_t z111_cap_for(size_t i)\n"
        "{\n"
        "    return i < MIMIR_TLS_CLASS_COUNT\n"
        "               ? (size_t)z111_cap[i] : (size_t)0;\n"
        "}\n"
        "static size_t z111_batch_for(size_t i)\n"
        "{\n"
        "    return i < MIMIR_TLS_CLASS_COUNT\n"
        "               ? (size_t)z111_batch[i] : (size_t)0;\n"
        "}\n\n")
    src = subn(src, "size_t mimir_tls_cache_bin_capacity(size_t class_index)",
               tables + "size_t mimir_tls_cache_bin_capacity(size_t class_index)",
               1, "inject tables")
    src = subn(src, "? MIMIR_TLS_BIN_CAPACITY\n",
               "? z111_cap_for(class_index)\n", 1, "capacity fn")
    src = subn(src, "? MIMIR_TLS_BATCH\n",
               "? z111_batch_for(class_index)\n", 1, "batch fn")
    # Full-bin checks (retire + both free courses).
    src = subn(src,
               "tls.bin_len[class_index] >= MIMIR_TLS_BIN_CAPACITY",
               "tls.bin_len[class_index] >= z111_cap_for(class_index)",
               3, "full-bin checks")
    # Refill: storage to the max batch, loop to per-class.
    src = subn(src, "void *taken[MIMIR_TLS_BATCH];",
               "void *taken[32];", 1, "refill storage")
    src = subn(src, "while (moved < MIMIR_TLS_BATCH) {",
               "while (moved < z111_batch_for(class_index)) {",
               1, "refill bound")
    # Drain batch body: rewrite inside the function span only.
    start = src.index("static void tls_drain_batch_locked(size_t class_index)")
    end = src.index("\nstatic ", start + 10)
    body = src[start:end]
    body = subn(body,
                "struct mimir_span_header *touched[MIMIR_TLS_BATCH];",
                "struct mimir_span_header *touched[32];\n"
                "    size_t z111_b = z111_batch_for(class_index);",
                1, "drain storage")
    body = body.replace("MIMIR_TLS_BATCH", "z111_b")
    if "MIMIR_TLS_BATCH" in body:
        print("FAIL: drain body still references MIMIR_TLS_BATCH")
        return 1
    src = src[:start] + body + src[end:]
    open(args.core, "w").write(src)
    print(f"patched caps={caps} batches={batches}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
