#!/usr/bin/env python3
# probe_g002_sizes.py -- MimirAlloc slice G-002 bounded
# size-distribution probe (Phase 7 size-class table design).
#
# What this probe is: a READ-ONLY census plus arithmetic.
# It never allocates through MimirAlloc and never modifies
# any source. It answers three questions for the G-002
# design doc (docs/PHASE_7_SIZE_CLASSES_G002.md):
#
#   Part A (MEASURED FROM SOURCES): which request sizes do
#   the banked test suites and sources actually name at
#   mimir_* call sites, and in size-ladder initialisers?
#   Two sub-censuses, both exact and reproducible:
#     A1: integer-literal arguments at mimir_malloc /
#         mimir_calloc (product) / mimir_realloc (new size)
#         call sites. Non-literal arguments are counted
#         separately as DYNAMIC call sites (their sizes are
#         computed at run time and are NOT guessed here).
#     A2: integer elements of array initialisers whose
#         declarator name contains "size" (the exercised
#         size ladders, e.g. tests/compat/test_d008.c).
#         Each element counts once per array.
#   Part B (SYNTHETIC, labelled): a stated representative
#   small-object mix, invented by this slice as a workload
#   HYPOTHESIS (not measured anywhere), used only to stress
#   the class table where the Part A census is a correctness
#   distribution rather than a workload. Every Part B line
#   is printed under a SYNTHETIC banner.
#   Part C (ARITHMETIC): for the proposed class table in
#   TABLE below: per-request internal fragmentation, span
#   packing at both admitted system page sizes (4096 and
#   16384, roadmap/D-003 facts), amortised span-header
#   cost, and the current mapping-per-allocation fallback
#   cost for contrast. Arithmetic only -- no timing, no
#   allocator run, no performance claim.
#
# The class table below is the table proposed by G-002;
# it is duplicated in the design doc, and the doc's
# fragmentation figures are this script's output.
#
# Run from the repository root:
#   python3 tests/core/probe_g002_sizes.py
# Exit status: 0 when the census completes.

import glob
import re
import sys
from collections import Counter

# ----------------------------------------------------------------
# Proposed G-002 class table (bytes). Every entry is a
# multiple of 16 (the D-008 alignment floor). Requests of
# 1..CUTOFF bytes map to the smallest class >= request;
# requests above CUTOFF take the large path (the existing
# mapping-per-allocation fallback, unchanged).
# ----------------------------------------------------------------
TABLE = [
    16, 32, 48, 64, 80, 96, 112, 128,
    160, 192, 224, 256,
    320, 384, 448, 512,
    640, 768, 896, 1024,
]
CUTOFF = TABLE[-1]
SPAN_HEADER = 96  # bytes; B-006 S7.3 descriptor size, reused
PAGE_SIZES = [4096, 16384]  # admitted targets (D-003)

INT = r"(?:0[xX][0-9a-fA-F]+|\d+)[uUlL]*"
CALL_RE = re.compile(
    r"mimir_(malloc|calloc|realloc)\s*\(([^;()]*?(?:\([^()]*\)[^;()]*?)?)\)",
    re.S,
)
LADDER_RE = re.compile(
    r"\b\w*size\w*\s*(?:\[[^\]]*\])?\s*=\s*\{([^{}]*)\}",
    re.S | re.I,
)
PURE_INT_RE = re.compile(r"^\s*(" + INT + r")\s*$")
INT_TOKEN_RE = re.compile(INT)


def parse_int(tok):
    tok = tok.rstrip("uUlL")
    return int(tok, 0)


def split_args(text):
    args, depth, cur = [], 0, ""
    for ch in text:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            args.append(cur)
            cur = ""
        else:
            cur += ch
    args.append(cur)
    return args


def class_of(req):
    if req <= 0:
        return None  # zero-size: unresolved posture (U-09)
    for c in TABLE:
        if req <= c:
            return c
    return "LARGE"


def frag_stats(counter, label):
    """Print per-distribution fragmentation over TABLE."""
    total = sum(counter.values())
    small = sum(n for r, n in counter.items() if 0 < r <= CUTOFF)
    large = sum(n for r, n in counter.items() if r > CUTOFF)
    zero = sum(n for r, n in counter.items() if r == 0)
    wsum = 0
    for r, n in counter.items():
        c = class_of(r)
        if isinstance(c, int):
            wsum += n * (c - r)
    mean_frag = (wsum / small) if small else 0.0
    print(f"  [{label}] total observations: {total}")
    print(f"  [{label}] small (1..{CUTOFF}): {small}; "
          f"large (>{CUTOFF}): {large}; zero: {zero}")
    if small:
        print(f"  [{label}] weighted mean internal fragmentation "
              f"over small requests: {mean_frag:.2f} bytes/request")
    return total, small, large


def main():
    files = sorted(
        glob.glob("tests/**/*.c", recursive=True)
        + glob.glob("src/**/*.c", recursive=True)
        + glob.glob("tests/**/*.mojo", recursive=True)
    )
    a1 = Counter()       # literal request sizes, by call kind
    a1_by_kind = {}
    dynamic = Counter()  # non-literal call sites, by kind
    a2 = Counter()       # ladder elements
    per_file_a1 = {}

    for path in files:
        try:
            with open(path, "r", errors="replace") as fh:
                text = fh.read()
        except OSError:
            continue
        file_counter = Counter()
        for m in CALL_RE.finditer(text):
            kind, argtext = m.group(1), m.group(2)
            args = split_args(argtext)
            size = None
            if kind == "malloc" and len(args) == 1:
                mm = PURE_INT_RE.match(args[0])
                if mm:
                    size = parse_int(mm.group(1))
            elif kind == "calloc" and len(args) == 2:
                ma, mb = PURE_INT_RE.match(args[0]), PURE_INT_RE.match(args[1])
                if ma and mb:
                    size = parse_int(ma.group(1)) * parse_int(mb.group(1))
            elif kind == "realloc" and len(args) == 2:
                mm = PURE_INT_RE.match(args[1])
                if mm:
                    size = parse_int(mm.group(1))
            if size is None:
                dynamic[kind] += 1
            else:
                a1[size] += 1
                a1_by_kind.setdefault(kind, Counter())[size] += 1
                file_counter[size] += 1
        if file_counter:
            per_file_a1[path] = file_counter
        for m in LADDER_RE.finditer(text):
            body = m.group(1)
            for tok in INT_TOKEN_RE.findall(body):
                try:
                    a2[parse_int(tok)] += 1
                except ValueError:
                    pass

    print("=" * 68)
    print("G-002 PROBE PART A1 -- literal mimir_* call-site sizes")
    print("  (MEASURED FROM SOURCES: exact count of integer-")
    print("   literal request sizes in tests/ and src/)")
    print("=" * 68)
    print(f"  files scanned: {len(files)}")
    print(f"  literal call sites counted: {sum(a1.values())}")
    print(f"  dynamic (non-literal) call sites, sizes NOT "
          f"guessed: {sum(dynamic.values())} "
          f"(malloc {dynamic['malloc']}, calloc {dynamic['calloc']}, "
          f"realloc {dynamic['realloc']})")
    print("  distinct literal sizes (size: call-site count):")
    for size in sorted(a1):
        print(f"    {size:>8} : {a1[size]}")
    frag_stats(a1, "A1")
    print("  per-file literal call-site counts:")
    for path in sorted(per_file_a1):
        fc = per_file_a1[path]
        sizes = ",".join(str(s) for s in sorted(fc))
        print(f"    {path}: {sum(fc.values())} sites, sizes [{sizes}]")

    print()
    print("=" * 68)
    print("G-002 PROBE PART A2 -- size-ladder elements")
    print("  (MEASURED FROM SOURCES: elements of *size* array")
    print("   initialisers in tests/ and src/; one count per")
    print("   element per array -- the exercised boundary ladder)")
    print("=" * 68)
    print(f"  ladder elements counted: {sum(a2.values())}")
    print("  distinct ladder sizes (size: element count):")
    for size in sorted(a2):
        print(f"    {size:>8} : {a2[size]}")
    frag_stats(a2, "A2")

    combined = a1 + a2
    print()
    print("  A1+A2 combined (call sites + ladder elements):")
    frag_stats(combined, "A1+A2")

    print()
    print("=" * 68)
    print("G-002 PROBE PART B -- SYNTHETIC representative mix")
    print("  SYNTHETIC -- INVENTED by this slice as a workload")
    print("  HYPOTHESIS. NOT measured on any host or workload.")
    print("  Stated mix: 10000 requests, small-object heavy,")
    print("  tail to the 1024 cutoff. Used only to stress the")
    print("  table where Part A is a correctness distribution.")
    print("=" * 68)
    synth = Counter({
        8: 900, 16: 1400, 24: 1100, 32: 1200, 48: 1000,
        64: 900, 80: 600, 96: 500, 128: 600, 160: 400,
        192: 300, 256: 400, 384: 250, 512: 200, 768: 100,
        1024: 80, 1536: 40, 2048: 30,
    })
    assert sum(synth.values()) == 10000
    for size in sorted(synth):
        c = class_of(size)
        print(f"    req {size:>6} : {synth[size]:>5} requests -> "
              f"class {c}")
    frag_stats(synth, "B-SYNTHETIC")

    print()
    print("=" * 68)
    print("G-002 PROBE PART C -- class-table arithmetic")
    print("  (ARITHMETIC ONLY: no timing, no allocator run,")
    print("   no performance claim. Span = one system page,")
    print(f"   span header {SPAN_HEADER} bytes at span start,")
    print("   objects at class stride from the header end;")
    print("   header end is 16-aligned because 96 % 16 == 0.)")
    print("=" * 68)
    print("  class | worst-req | worst-frag | worst-frag% | "
          "objs/span@4K | tail@4K | objs/span@16K | tail@16K | "
          "hdr/obj@16K")
    prev = 0
    for c in TABLE:
        worst_req = prev + 1
        worst_frag = c - worst_req
        worst_pct = 100.0 * worst_frag / c
        row = [f"  {c:>5} | {worst_req:>9} | {worst_frag:>10} | "
               f"{worst_pct:>10.1f}% |"]
        for p in PAGE_SIZES:
            objs = (p - SPAN_HEADER) // c
            tail = (p - SPAN_HEADER) - objs * c
            row.append(f" {objs:>12} | {tail:>7} |" if p == 4096
                       else f" {objs:>13} | {tail:>8} |")
        hdr = SPAN_HEADER / ((PAGE_SIZES[1] - SPAN_HEADER) // c)
        row.append(f" {hdr:>10.2f}")
        print("".join(row))
        prev = c
    print()
    print("  Fallback contrast (ARITHMETIC): the current")
    print("  mapping-per-allocation path maps reserve page +")
    print("  round_up(request, page) per allocation. Mapped")
    print("  bytes per request r at page P: P + round_up(r,P).")
    for p in PAGE_SIZES:
        for r in (16, 32, 128, 512, 1024, 4096):
            mapped = p + ((r + p - 1) // p) * p
            print(f"    P={p:>6} req={r:>6}: mapped {mapped:>7} "
                  f"bytes ({mapped / r:.1f}x the request)")
    print()
    print("  Span contrast (ARITHMETIC): a full span of class")
    print("  c holds floor((P-96)/c) objects in P bytes, so")
    print("  mapped bytes per object = P / floor((P-96)/c).")
    for p in PAGE_SIZES:
        for c in (16, 32, 128, 512, 1024):
            objs = (p - SPAN_HEADER) // c
            print(f"    P={p:>6} class={c:>5}: {objs:>3} objs/span, "
                  f"{p / objs:.1f} mapped bytes/object")
    print()
    print("PROBE COMPLETE")
    return 0


if __name__ == "__main__":
    sys.exit(main())
