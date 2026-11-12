#!/usr/bin/env python3
"""summarize_z032.py - Z-032 size-class spacing study summariser.

Reads the raw probe output (run_z032.sh), verifies stability
across executions, and derives every number in the recorded
run's tables FROM THE MEASURED request->class map (never from
the source table, which it cross-checks). All fragmentation
figures beyond the measured map are arithmetic on stated
models, labelled as such in the output.

Usage: summarize_z032.py <raw-file>
Prints the tables section; exit 1 on any instability/mismatch.
"""
import sys
from fractions import Fraction

CLASSES = [16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
           224, 256, 320, 384, 448, 512, 640, 768, 896, 1024]
PAGE, HDR = 4096, 96
USABLE = PAGE - HDR


def serving(table, r):
    for c in table:
        if r <= c:
            return c
    return None


def objs(c):
    return USABLE // c


def mapped_per_obj(c):
    return Fraction(PAGE, objs(c))


def fail(msg):
    print(f"SUMMARIZER FAIL: {msg}")
    sys.exit(1)


def main():
    raw = open(sys.argv[1]).read().splitlines()
    maps, spans, caches = [], {}, []
    cur_map, cur_cache = None, None
    for ln in raw:
        if ln.startswith("EXEC map"):
            cur_map = {}
            maps.append(cur_map)
        elif ln.startswith("EXEC cache"):
            cur_cache = {}
            caches.append(cur_cache)
        elif ln.startswith("MAP ") and not ln.startswith("MAPDONE"):
            _, r, c, s16 = ln.split()
            cur_map[int(r)] = (int(c), int(s16.split("=")[1]))
        elif ln.startswith("SPAN "):
            parts = ln.split()
            c = int(parts[1])
            d = dict(kv.split("=") for kv in parts[2:] if "=" in kv)
            spans[c] = (int(d["objs"]), int(d["first_off"]), int(d["sub16"]))
        elif ln.startswith("TOTAL "):
            parts = ln.replace(":", "").split()
            c = int(parts[2])
            d = dict(kv.split("=") for kv in parts[3:] if "=" in kv)
            cur_cache[c] = (int(d["mod64_0"]), int(d["mod64_16"]),
                            int(d["mod64_32"]), int(d["mod64_48"]),
                            int(d["sub16"]), int(d["split"]),
                            float(d["split_pct"]), int(d["min_align"]))

    # ---- stability + cross-checks -------------------------------------
    if len(maps) != 3 or len(caches) != 3 or len(spans) != 20:
        fail(f"executions: maps={len(maps)} caches={len(caches)} spans={len(spans)}")
    for m in maps:
        if len(m) != 1024:
            fail(f"map has {len(m)} entries")
        for r in range(1, 1025):
            c, s16 = m[r]
            if c != serving(CLASSES, r):
                fail(f"measured map r={r} -> {c}, source table says {serving(CLASSES, r)}")
            if s16 != 0:
                fail(f"sub-16 pointer at r={r}")
    if not (maps[0] == maps[1] == maps[2]):
        fail("request->class maps disagree across executions")
    for ch in caches:
        if len(ch) != 20:
            fail("cache execution incomplete")
    for c in CLASSES:
        if not (caches[0][c] == caches[1][c] == caches[2][c]):
            fail(f"cache distribution for class {c} disagrees across executions")
        o, off, s16 = spans[c]
        if o != objs(c):
            fail(f"measured objs/span class {c}: {o}, geometry says {objs(c)}")
        if off != HDR:
            fail(f"first-object offset class {c}: {off}, header says {HDR}")
        if s16 != 0 or caches[0][c][4] != 0:
            fail(f"sub-16 pointer in class {c}")

    out = []
    out.append("=== MEASURED BASIS (probe_z032, 3 executions, all identical) ===")
    out.append("Request->class map: measured for EVERY r in 1..1024 (6 blocks per")
    out.append("request, serving class = minimum absolute consecutive-pointer stride).")
    out.append("The measured map is a step function identical in all 3 executions and")
    out.append("identical to the mimir_class_sizes table in src/core/mimir_core.c.")
    out.append("Objects per span: measured in 20 fresh processes (first-page run);")
    out.append(f"equal to floor({USABLE}/class) for every class; first-object offset")
    out.append(f"measured {HDR} (= span header size) in every class.")
    out.append("Sub-16-aligned pointers: 0 in map, span and cache modes, all executions.")
    out.append("")
    out.append("=== SECTION 1 - CLASS-BY-CLASS TABLE (measured map + span geometry) ===")
    out.append("band = requests served by the class; waste over a band assumes requests")
    out.append("uniform WITHIN the band (arithmetic on the measured map, labelled model);")
    out.append("mapped/obj = 4096 / objs-per-span (span-full effective mapped bytes);")
    out.append("overhead at exact class = mapped/obj - class (header + span tail share).")
    out.append("maxW% = max waste as a percentage OF THE CLASS (allocated) size.")
    out.append("cls | band      | width | maxW | maxW% | meanW | objs | tail | hdr/obj | mapped/obj | ovhd@cls")
    prev = 0
    for c in CLASSES:
        lo, hi = prev + 1, c
        width = hi - lo + 1
        maxw = c - lo
        meanw = Fraction(sum(c - r for r in range(lo, hi + 1)), width)
        o = objs(c)
        tail = USABLE - o * c
        out.append(f"{c:4d} | {lo:4d}..{hi:<4d} | {width:5d} | {maxw:4d} | "
                   f"{100.0 * maxw / c:5.2f} | {float(meanw):5.2f} | {o:4d} | "
                   f"{tail:4d} | {HDR / o:7.3f} | {float(mapped_per_obj(c)):10.2f} | "
                   f"{float(mapped_per_obj(c) - c):8.2f}")
        prev = c
    out.append("Worst internal waste: 15 B for every band up to 128; 31 B to 256;")
    out.append("63 B to 512; 127 B to 1024. Worst waste fraction at a band bottom:")
    worst = max((Fraction(c - (p + 1), c), c, p + 1)
                for c, p in zip(CLASSES[1:], CLASSES[:-1]))
    out.append(f"{100 * float(worst[0]):.2f}% of the class (request {worst[2]} -> class "
               f"{worst[1]}), the largest band-bottom waste fraction above the")
    out.append("first band; in the top octave it is 19.84% (request 513 -> class 640).")
    out.append("First band separately: request 1 -> 16 wastes 15 B = 93.75% of the")
    out.append("class but only 15 B absolute,")
    out.append("the 16 B minimum-class floor every 16-byte-aligned allocator pays).")
    out.append("Largest span overhead at exact class: class 1024 - 3 objects per span,")
    out.append("928 B span tail, mapped 1365.33 B/object (+341.33 over class). This is")
    out.append("span GEOMETRY (one 4096 B page, 96 B header), not spacing: no class")
    out.append("table changes floor(4000/1024) = 3.")
    out.append("")
    out.append("=== SECTION 2 - CACHE BEHAVIOUR PER CLASS (probe cache mode; Z-017 splits) ===")
    out.append("mod64 = TOTAL pointer counts over 4096 blocks/class (2 rounds x 2048),")
    out.append("identical in all 3 executions of this probe and consistent with the")
    out.append("banked Z-017 audit (8192/class, same distributions). Split is graded")
    out.append("only for req <= 64 (classes that can fit one 64 B line); above 64 a")
    out.append("block spans lines by size alone.")
    out.append("cls | mod64 0/16/32/48 | split | split% | min_align | graded reading")
    for c in CLASSES:
        m0, m16, m32, m48, _s16, split, pct, minal = caches[0][c]
        if c <= 64:
            reading = "GRADED (Z-017 named)" if c in (48, 64) else "GRADED clean"
        else:
            reading = "spans by size (not graded)"
        out.append(f"{c:4d} | {m0}/{m16}/{m32}/{m48} | {split:5d} | {pct:6.2f} | "
                   f"{minal:9d} | {reading}")
    out.append("Structural fact (both audits): every class that is a multiple of 64")
    out.append("returns pointers exclusively at ptr mod 64 = 32 - carve starts at page")
    out.append("offset 96 (the span header) and strides by a multiple of 64, so starts")
    out.append("stay at 96 mod 64 = 32 forever. Class 64 therefore splits 100% and")
    out.append("class 48 splits ~half (offsets 32/48 straddle). This is fixed by the")
    out.append("HEADER/CARVE OFFSET, not by which class sizes exist: any table keeps")
    out.append("offset 96. Spacing cannot repair it; see the verdict.")
    out.append("")
    out.append("=== SECTION 3 - REQUEST HISTOGRAM FIT ===")
    out.append("H1 - the campaign's own workload cycles (tests/perf/bench_k002.c,")
    out.append("landed): small_churn cycles {16,32,48,64,96,128,192,256}; mixed_batch")
    out.append("and mixed_lifecycle cycle the 20 class sizes exactly; large_batch is")
    out.append("above the cutoff. Fit of the current table to H1, from the measured")
    h1_sizes = [16, 32, 48, 64, 96, 128, 192, 256] + CLASSES
    h1_waste = sum(serving(CLASSES, r) - r for r in h1_sizes)
    out.append(f"map: internal waste {h1_waste} B over {len(h1_sizes)} cycled sizes (0.00 B/request).")
    out.append("CAVEAT (circularity): those cycles were DRAWN FROM this table, so a")
    out.append("zero is guaranteed by construction and proves fit to the campaign's")
    out.append("synthetic workloads only - never to any real application. No real-world")
    out.append("request trace has been measured or landed on this host in this campaign.")
    out.append("H2 - uniform model over 1..1024 (ARITHMETIC MODEL, not a measurement):")
    uw = Fraction(sum(serving(CLASSES, r) - r for r in range(1, 1025)), 1024)
    um = sum((mapped_per_obj(serving(CLASSES, r)) for r in range(1, 1025)),
             Fraction(0)) / 1024
    out.append(f"current table: mean internal waste {float(uw):.2f} B/request; mean")
    out.append(f"span-effective mapped {float(um):.2f} B/request (spans full).")
    out.append("")
    out.append("=== SECTION 4 - CANDIDATE TABLES PRICED (same geometry, same models) ===")
    out.append("Every candidate is priced with the IDENTICAL span geometry (4096 B")
    out.append("page, 96 B header) and the IDENTICAL models (H1 cycles, H2 uniform).")
    out.append("retained = worst-case empty-span retention = classes x 2 spans x 4096 B")
    out.append("(the per-class retention bound of record, G-007 default 2).")
    cands = {
        "C0 current (20)": CLASSES,
        "C1 powers-of-two (7)": [16, 32, 64, 128, 256, 512, 1024],
        "C2 step-16 (64)": list(range(16, 1025, 16)),
        "C3 +midpoints >256 (28)": sorted(CLASSES + [288, 352, 416, 480, 576, 704, 832, 960]),
        "C4 +midpoints >128 (32)": sorted(CLASSES + [
            (a + b) // 2 for a, b in zip(CLASSES, CLASSES[1:]) if a >= 128]),
    }
    out.append("cand | classes | H2 mean waste | H2 mean mapped | vs C0 mapped | H1 waste | retention max")
    base_mapped = None
    rows = {}
    for name, tab in cands.items():
        w = Fraction(sum(serving(tab, r) - r for r in range(1, 1025)), 1024)
        mp = sum((mapped_per_obj(serving(tab, r)) for r in range(1, 1025)),
                 Fraction(0)) / 1024
        h1 = sum(serving(tab, r) - r for r in h1_sizes)
        rows[name] = (len(tab), w, mp, h1, len(tab) * 2 * PAGE)
        if base_mapped is None:
            base_mapped = mp
    for name, (n, w, mp, h1, ret) in rows.items():
        delta = 100 * float((mp - base_mapped) / base_mapped)
        out.append(f"{name} | {n} | {float(w):.2f} B | {float(mp):.2f} B | "
                   f"{delta:+.2f}% | {h1} B | {ret} B")
    out.append("Reading: internal-waste savings shrink once span geometry is priced")
    out.append("in (mapped column): a finer table's new classes carry their own span")
    out.append("tails and headers, and every added class adds its own live spans to")
    out.append("the registry Z-024 proved the free path walks linearly, twice.")
    print("\n".join(out))
    print("SUMMARIZER OK")


if __name__ == "__main__":
    main()
