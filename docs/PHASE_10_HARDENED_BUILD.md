# Phase 10 - J-008: Hardened Build, Overhead Comparison, Overflow Audit

Slice J-008 (Wave 10, Phase 10), base 1aab762 (J-002).
Owns roadmap exit items X7 (hardened configuration +
integer-overflow audit) and X8 (debug-vs-production
overhead comparison), and the flavour grading carried
from J-004 as INC-J004-02. Evidence:
tests/core/recorded_run_j008.txt (Pi-native, AArch64,
gcc 12.2.0, glibc 2.36, page size 16384). No src/ file is
changed by this slice; the Makefile delta is one additive
block at the end; README.md and include/mimir_alloc.h are
byte-identical to base.

## 1. Hardened build configuration (X7, first half)

The hardened flavour lives in its own build directory,
build-hardened/, selected by `make lib-hardened` /
`make test-j008` / `make bench-j008` - the J-001 S5.1
rule (flavours distinguished by directory, never by a
runtime switch) carried to hardening. Nothing in the
default production build changes: no default target,
flag or composition is touched, and the full production
suite passes unchanged (make test exit 0; test-j002
550/0; recorded run). HCFLAGS = banked CFLAGS plus -O2
and the flags below; the shared library links with
-Wl,-z,relro,-z,now,-z,noexecstack and the banked
version script; the test executable links -fPIE -pie.

Per-flag verdicts (probe + artefact evidence in the
recorded run; checksec is not installed on the Pi, so
all artefact checks use the Pi's own readelf/nm):

| Flag | Supported | Applied | Verified from artefact |
|---|---|---|---|
| -fstack-protector-strong | YES (probe compiles) | YES, all hardened objects | YES: build-hardened/libmimiralloc.so carries U __stack_chk_fail and U __stack_chk_guard - at least one allocator function is instrumented |
| -D_FORTIFY_SOURCE=3 (with -O2) | YES (gcc 12 / glibc 2.36 accept =3; =2 also accepted) | YES | NO artefact check exists for these sources, stated plainly: no *_chk fortified call sites appear in the .so, because every memcpy/memset here has a runtime (not compile-time-known) object size. The flag is armed for any future fortifiable call, and proves nothing today |
| -fstack-clash-protection | YES (probe compiles, AArch64) | YES | NO artefact check exists, stated plainly: probing sequences appear only in large-frame prologues (disassembly), and no allocator frame is large enough to require one. Compile-acceptance is the only evidence |
| -Wformat -Wformat-security -Werror=format-security | YES, but ONLY as a set: -Wformat-security alone is REJECTED by this gcc ("ignored without -Wformat" becomes an error under -Werror=format-security; probe in recorded run) | YES, as the set | Compile-time only by nature; no artefact check exists. Effect proven negatively: the hardened build compiles the whole tree warning-free under it |
| -fPIC / -shared (library PIE equivalent) | banked (E-002) | YES, as banked, plus hardening link flags | YES: .so Type DYN; nm -D --defined-only exactly 4 |
| -fPIE -pie (test executable) | YES | YES, test_j008 only | YES: readelf Type: DYN (Position-Independent Executable) |
| -Wl,-z,relro | YES | YES | PARTIAL honesty: GNU_RELRO segment is present in the hardened .so - but it is also present in the production .so (linker default on this toolchain), so its presence alone proves nothing about this slice |
| -Wl,-z,now | YES | YES | YES: dynamic section carries FLAGS BIND_NOW and FLAGS_1 NOW in the hardened .so; the production .so carries neither (grep count 0). This is the verified hardening delta of the relro pair |
| -Wl,-z,noexecstack | YES | YES | WEAK, stated plainly: GNU_STACK is RW (not RWE) in the hardened .so - but RW is also this toolchain's default (production .so identical), so the flag confirms rather than creates the posture |

Byte effect: hardened .so 73,344 B vs production
74,248 B on disk (text 9,788 vs 12,828 - the -O2 code
generation, not a hardening cost; reported as a fact,
not a saving claim). Per-allocation layout overhead of
the hardened flavour: zero - no source changes, layouts
identical; test-j008 Leg A proves functional identity
on the hardened objects.

## 2. Overhead comparison (X8)

Method (S11 discipline): fresh process per repetition,
5 repetitions per flavour, CLOCK_MONOTONIC, the banked
J-004/J-005 micro-bench shape (20,000 x
malloc(64)/memset/free) plus 2,000 x malloc(4096)/free
for the large path, driven by tests/core/bench_j008.c.
Timing is a measurement, never an oracle. The Pi was
under its ordinary load (Samtal live, HTTP 200 before
and after every run). Every repetition is in the
recorded run; medians and ranges below are computed
from it. NOTE: absolute figures are NOT comparable to
J-004's bench (728 ns/op production) - that figure came
from J-004's own test binary; cross-bench comparison
would be a comparator artefact.

### 2a. Measured on THIS base (merged-tree figures)

| Flavour | small64 median (range) | large4096 median (range) |
|---|---|---|
| production (banked -O0 objects) | 121 ns/op (120-136) | 6,192 ns/op (5,869-6,735) |
| production + J-002 snapshot per 1,000 ops | 121 ns/op (120-123) | 6,083 ns/op (5,764-8,229) |
| hardened (S1) | 73 ns/op (72-114) | 5,644 ns/op (5,588-5,722) |
| hardened + snapshot per 1,000 ops | 73 ns/op (73-98) | 5,803 ns/op (5,603-5,965) |
| control: -O2, NO hardening flags | 71 ns/op (71-74) | 5,717 ns/op (5,607-6,390) |

Readings, honestly bounded:
- The statistics/diagnostics surface present at this
  base (the J-002 locked snapshot, read once per 1,000
  operations) costs nothing measurable at this bench's
  resolution: medians identical in both flavours. The
  per-snapshot cost amortised is under ~1 ns/op.
- The hardened flavour is FASTER than production on
  this bench, and the control row shows why: -O2, not
  hardening, is the difference. Against the -O2 control,
  the hardening flags' isolated cost is ~+2 ns/op on
  the small path (73 vs 71 median, inside run spread)
  and not measurable on the large path (5,644 vs 5,717).
  No stronger claim is made.

### 2b. OWED-TO-BANKING-RECONCILIATION: the debug flavour

The S5 debug flavour (J-004 red zones/canaries, J-005
poison/quarantine) is NOT in this base - those slices
sit on sibling branches yrsa/mimiralloc-04 / -05, held
for banking. The X8 table's debug rows therefore CANNOT
be completed here, and no sibling figure is presented
as a merged-tree measurement. What the sibling slices
recorded on their own branches (cited, not claimed):
J-004: exact layout cost 32 B of span space per live
small object; span capacities per class (its doc S4
table, e.g. class 64: 254 production -> 169 debug);
one bench sample set, production 728 / debug
1,068-1,156 ns/op in ITS bench shape. J-005: static
quarantine table 512 B; held user bytes at bound
7,168 B in its Leg C (absolute ceiling 32,768 B); one
debug-side timing, 687 ns/op in its recorded run;
production column zero/absent by construction.

The keeper fills the debug rows on the MERGED tree at
Wave 10 banking, with exactly these commands (run in
/home/pi/mimiralloc-forge after J-004 and J-005 bank):
  make test        (expect 4590/0, production column)
  make test-j004   (debug flavour: red zones/canaries)
  make test-j005   (debug flavour: poison/quarantine)
  make bench-j008  (production + stats rows, this slice)
  sed -n '1,999p' tests/core/bench_j008.c  (shape ref)
  # debug bench: rebuild the bench driver against the
  # merged debug objects per J-004's Makefile DEBUG=1
  # wiring, same source, 5 fresh-process reps:
  make DEBUG=1 bench-j008-equivalent  (keeper: wire the
    bench_j008 source to the merged tree's debug build
    directory exactly as test-j004 is wired, run 5 reps
    plain, record medians beside the S2a table)
Until those runs exist, X8 grades PARTIAL: production,
hardened, statistics and control rows measured; debug
rows owed, with the exact filler commands above.

## 3. Integer-overflow protection audit (X7, second half)

Scope: every size computation in the banked sources at
this base (src/api/mimir_alloc.c, src/core/mimir_core.c,
src/platform/mimir_platform.c, both headers - the
headers contain no runtime arithmetic). Method: full
read of all three .c files plus a grep sweep for
SIZE_MAX / arithmetic operators; every site graded
CHECKED (a check evaluated before the arithmetic is
relied on), SAFE-BY-CONSTRUCTION (a named invariant
bounds the value), or UNCHECKED. Line numbers are this
base's (1aab762).

CHECKED (10 sites):
 C1 core:431 total = usable_offset + size, gated by
    size > SIZE_MAX - usable_offset (B-007 A3/A4).
 C2 core: usable_size = mapping_length - usable_offset,
    gated by usable_offset > mapping_length refusal (A8).
 C3 core: base + usable_offset pointer range, gated by
    usable_offset > UINTPTR_MAX - base refusal (A9).
 C4 core:482-483 live counters before publication:
    mapping_length > SIZE_MAX - mimir_live_bytes and
    mimir_live_mappings == SIZE_MAX (A10, both halves).
 C5 core:815-816 the span pair of C4 for
    mimir_live_span_bytes / mimir_live_spans.
 C6 platform:65 page round-up addition form (A5).
 C7 platform:69 page round-up multiply form (A6).
 C8 core:1564 calloc product: nmemb > SIZE_MAX / size
    before nmemb * size is evaluated (A6 form).
 C9 platform:39 sysconf page size: (uintmax_t)queried >
    SIZE_MAX rejected before the cast; power-of-two and
    positivity also gated at admission.
 C10 core span_acquire: page_size <= 96 and
    page_size - 96 < class_size refused before any span
    arithmetic relies on the difference.

SAFE-BY-CONSTRUCTION (groups, invariant named):
 S1 span_capacity (mapping_length - 96) / class_size:
    mapping_length is an acquired page (>= 4096,
    equality-checked against page_size at acquisition),
    class_size is a table constant or a value validated
    equal to the table entry (span_header_is_live).
 S2 carve offset 96 + carve_count * class_size: the
    carve branch runs only when span_can_serve held
    with free_head == NULL, i.e. carve_count < capacity,
    so the product is < usable extent by construction.
 S3 find_span_live_locked base + mapping_length and
    offset/index arithmetic: mapping_length is one real
    page of a live mapping (cannot wrap the address
    space); index is checked < carve_count before use.
 S4 find_candidate_locked mapping_base + usable_offset:
    both fields were A9-checked (C3) at publication.
 S5 live-byte SUMS (core mapped_bytes, peak sampler,
    retained_snapshot bytes, snapshot bytes_cached):
    each term is the length of a distinct live OS
    mapping; disjoint mappings' lengths cannot sum past
    the address space they occupy.
 S6 count increments (live_count, carve_count,
    live_small_objects, registry-walk counts, trim
    released): each increment names a distinct real
    object/span in mapped memory; reaching SIZE_MAX
    would need SIZE_MAX live objects in real pages.
 S7 realloc copy lengths (old requested_size, old class
    extent): recorded publication extents of live
    blocks, in-bounds on both sides by construction.
 S8 cumulative counters (J-002 statistics accumulators,
    E-005 audit counters, L1 call counters): unchecked
    adds BY CONTRACT - J-001 S4 states the posture
    deliberately: increments are modulo 2^64, wrap is
    unreachable at any physically runnable scale on
    this host, and no counter may ever refuse or reroute
    an allocation (Invariant 8). Live state, the kind of
    quantity whose wrap would corrupt bookkeeping, is
    the CHECKED class above - the difference is the
    design, re-verified here, not an oversight.

UNCHECKED defects found: NONE. No fix was needed; no
src/ file changes in this slice.

New proof (test_j008 Leg B, hardened objects, 25/0):
B2/B3 the C1 gate at its exact boundary
(SIZE_MAX - page + 1 refused by the gate;
SIZE_MAX - page admitted arithmetically, refused by
the mapping), B5 the C8 product with factors swapped,
B6/B7 the product == SIZE_MAX edge (passes C8, refused
by C1), B9-B11 realloc grow to SIZE_MAX refused with
the original block preserved and freeable. B1/B4 are
labelled re-verifications of banked proofs (test_core /
C-005), not new claims. A NULL return is the proof
shape throughout: had any size wrapped, the wrapped
small request would have been admitted and mapped.

## 4. INC-J004-02 flavour grading (carried)

The three banked checks J-004 named - test_b007 C1.4
span form, test_c006 B-007 C1.4 (shared g003_span_clean
stride identity), and test_j002 legA/legE capacity
expectations - encode production layout arithmetic.
What THIS base proves: all three pass in the production
flavour here (make test exit 0 includes b007 and c006;
test-j002 550/0; recorded run). The debug flavour is
not in this base, so the debug half is graded
OWED-TO-BANKING-RECONCILIATION, with J-004's recorded
observations cited (b007 68/69, c006 22/23, j002
failures confined to the 2 capacity lines, debug
capacities 113/339 matching J-004 arithmetic exactly -
sibling-branch evidence, not a merged-tree run). The
grading rule J-008 hands to J-009/the keeper: these
three checks are PRODUCTION-LAYOUT checks; under
DEBUG=1 their divergence is the S5.2-permitted layout
change reported as J-004 overhead, NOT a failure - and
any OTHER check failing under DEBUG=1, or any of these
three failing in production, is a defect. Keeper rerun
on the merged tree (after J-004 banks):
  make test && make test-j002        (production: pass)
  make DEBUG=1 test-j004             (debug suite)
  # plus the three binaries under the merged DEBUG=1
  # wiring, expecting exactly the divergences above.

## 5. Reconciliation items (INC-J008-xx)

- INC-J008-01: X8 debug rows owed to banking
  reconciliation (S2b) - J-004/J-005 not in this base;
  exact filler commands recorded there.
- INC-J008-02: INC-J004-02 debug half owed (S4);
  production half proven at this base.
- INC-J008-03: FORTIFY_SOURCE=3 and stack-clash have no
  artefact-level proof for these sources (S1) - the
  flags are applied and supported, their effect is
  armed-but-unexercised; a future slice adding a
  fortifiable call or a large frame must re-verify.
- INC-J008-04: -z relro and -z noexecstack merely
  confirm this toolchain's defaults (S1); the verified
  link hardening delta of this slice is -z now alone.
  Stated so no later reader over-claims the pair.
- INC-J008-05: J-003/J-006/J-007 interactions: none of
  their surfaces change under the hardened flavour
  observed here (same sources, no ABI change, nm -D 4
  in production, hardened and test flavours alike);
  their slices' own runs were on production objects and
  are unaffected by an additive flavour they do not use.

## 6. Files

- Makefile - additive J-008 block only (+74 lines:
  lib-hardened, test-j008, bench-j008; build-hardened/).
- docs/PHASE_10_HARDENED_BUILD.md - this record.
- tests/core/test_j008.c - hardened-flavour evidence
  test (make test-j008; 25 checks / 0 failures).
- tests/core/bench_j008.c - overhead bench driver.
- tests/core/recorded_run_j008.txt - the real Pi runs.
