# Phase 7 — Small-Object Path (Slice G-003)

Status: IMPLEMENTED and banked as evidence in the recorded run
`tests/core/recorded_run_g003.txt` (real run on this host,
Raspberry Pi, 16 KiB pages). G-003 implements the G-002
contract: the 20-class size-class table, one-system-page spans
with the 96-byte S7.3-discipline header, routing rules
R-1..R-6, bounded local reuse, and the R-5 span counters.
Plain verdict: **small allocations no longer map a fresh
mapping per call** — measured on this host, the G-001 probe
shape reads p50 148-253 ns across sizes 8..1024 (G-001
baseline: 4889-5167 ns at every size), and 256 live blocks of
size 64 now cost 16 KiB of RSS delta (one page) where G-001
measured 8192 KiB. The large path is byte-for-byte the banked
code path, unchanged. Phase 7 is NOT exited by this slice;
the exit assembly remains G-009's (see section 6).

Companions: `docs/PHASE_7_FOUNDATION_PLAN.md` (G-001),
`docs/PHASE_7_SIZE_CLASSES_G002.md` (G-002, the contract this
slice implements), `docs/PHASE_3_CONTRACT.md` sections
5.6/5.9/B-006 S7.3/B-007/B-008, `docs/PHASE_5_AUDIT_HOOK_E005.md`.

## 1. What was built

All in `src/core/mimir_core.c` (no new translation unit, no
`core.h` change, public header byte-identical):

- **Class table** — exactly G-002 section 3's 20 classes:
  16, 32, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320,
  384, 448, 512, 640, 768, 896, 1024. `size_to_class` returns
  the smallest class >= request, or -1 above the 1024 cutoff.
- **Span header, 96 bytes, closing INC-G002-04.** The banked
  S7.3 field layout with the previously reserved fields
  assigned: offset 32 `requested_size` carries the span's
  CLASS SIZE (a span serves one class; the field's additive
  per-arena semantics, mirroring the mapping header's use of
  offset 32 for request size); offset 52 `flags` carries the
  CLASS INDEX; offsets 72/80/88 (reserved tail) carry
  `free_head` (offset within the span, 0 = none),
  `live_count`, `carve_count`. Magic is `MIMIRSPN`, distinct
  from the mapping magic `MIMIRALL`; `mapping_base` is the
  span's own address, `mapping_length` is exactly one runtime
  page, `usable_offset` 96, `usable_size` page - 96, state
  ACTIVE/RETIRED as S7.3. Compile-time asserts pin the header
  size (96) and every offset.
- **Span registry** beside the mapping registry (offsets
  56/64 list links, S7.3 discipline): link, publish, find by
  containment, retire — same publish-then-link / unlink-then-
  unmap publication discipline as Phase 3.
- **Object layout** — objects at span_base + 96 + k * class,
  k < (page - 96) / class; object starts are 16-aligned
  because 96 and every class are multiples of 16 (G-002
  section 4 proof obligations (a)-(d) hold by construction
  and are test-asserted for every class in test S2).
- **Carving and reuse** — a span serves its class first from
  its intrusive free list (link in the freed object's first
  8 bytes), then by carving fresh slots, then the class takes
  a new span. A span is ELIGIBLE when it has a free-list head
  or uncarved room. Lock-guarded throughout (L3 pass-through;
  see INC-G003-04).
- **Bounded retention** — when a span's live count reaches 0
  it is kept only while its class holds fewer than
  MIMIR_SPAN_EMPTY_RETAIN (2) empty spans; beyond that it is
  retired whole through L6 with the F-06-style rollback
  (alloc.c's retire helper, mirrored). Worst-case retained
  empty memory: 20 classes x 2 spans x 1 page = 40 pages
  (640 KiB on this 16 KiB-page host, 160 KiB at 4 KiB).
- **Counters (R-5)** — `mimir_audit_span_published`,
  `mimir_audit_span_retired`, `mimir_audit_small_alloc`,
  `mimir_audit_small_free`, INC-E001-07 shape (b), exposed
  test-only via `tests/core/mimir_small_g003.h`, bound local
  in the .so exactly like the E-005 counters. Mapping
  counters keep their banked meanings; a span allocation
  moves NO mapping counter.

## 2. Routing as implemented vs G-002 R-1..R-6

- **R-1 (route by size) — implemented as written.** One
  dispatcher (`alloc_any_locked`) under the core lock:
  request <= 1024 -> span path; larger -> the banked
  `alloc_locked` mapping path, unmodified. A span-path
  backing failure (L6 fails) returns NULL; the request is
  NEVER silently re-routed to the mapping path (which would
  also fail, and would corrupt the counter story).
- **R-2 (calloc) — implemented.** Small calloc takes a span
  object and zeroes the whole CLASS extent (the usable
  extent the class guarantees, >= the B-008 product);
  callocs above the cutoff take the banked mapping calloc,
  zeroing the whole mapping as banked. Product overflow
  still fails NULL before any routing.
- **R-3 (realloc) — implemented, with INC-G002-07 carried.**
  Span block, new size within the block's class: returned
  unchanged (same / shrink / grow-within-class). Span block,
  new size above the class (including above the cutoff):
  B-008 construct-copy-disable — fresh allocation (span or
  mapping per the new size), copy of the old CLASS extent
  (in-bounds on both sides: read bounded by the old class
  extent the block provably has, write bounded by the new
  object's class/mapping extent), old block retired only
  after the copy. Failure leaves the old block untouched and
  fully owned (failure-atomic, B-008). New size 0 returns
  NULL with the old block untouched (banked D-008 posture).
  Mapping block shrinking to <= 1024 STAYS on its mapping
  (banked B-008 shrink posture; the R-3 shrink-across note
  and INC-G002-07's churn question carry — INC-G003-07).
- **R-4 (two-registry free) — implemented.** `mimir_free`
  consults the mapping registry first, then the span
  registry (the free path itself lives in the core, called
  from alloc.c under the lock; alloc.c keeps the banked
  mapping helpers for its own registry walk). Span
  membership is computed from trusted span-header fields
  only: containment in [base+96, base+96+carve*class),
  pointer == base + 96 + k*class. Candidate-pointer bytes
  are never dereferenced. Freeing a span object threads it
  onto the span's free list; the span retires only under the
  retention bound. Test S5 posture probes (this
  implementation's posture, per G-002 section 5): interior
  pointer (block+8), the span-header pointer itself, and
  double free are all detected and ignored non-destructively
  — the double-free case by walking the span's own free list
  before accepting the object, so a second free cannot
  corrupt the chain (proven by re-allocation pattern checks
  after each probe).
- **R-5 (counters) — implemented** as section 1 above; the
  E-005 static proof is re-taught against the two registries
  in this slice (INC-G003-01); the dynamic leg is
  commissioned (INC-G003-02).
- **R-6 (init) — implemented.** Span state initialises with
  the banked lazy/explicit init; no allocation at init; the
  page size comes from the L6 cache (`plat_page_size`),
  never a literal; the existing suites' smoke checks (zero
  counters after explicit init) still pass.

## 3. Tests — tests/core/test_g003.c (54 checks, 0 failures)

Wired additively as `make test-g003` (standalone, G-001
pattern; `make test` membership unchanged). Sections:
S1 routing proven by counters (small moves only small
counters; 1025/4096/1 MiB move only mapping counters);
S2 alignment 16 asserted for all 20 classes and size 1;
S3 pattern write/verify and pairwise non-overlap across 48
live blocks; S4 reuse discipline — 500 alloc/free cycles
publish no new span; capacity+10 live blocks all distinct
and pattern-clean; 800 live blocks force extra spans and
freeing them all retires spans beyond the bound (2 span
retirements observed); live spans stay <= 40 after mass
free; S5 two-registry free routing + posture probes;
S6 calloc zeroing over the full class extent on REUSED
blocks, large calloc still mapping-served and zeroed,
product overflow NULL; S7 realloc — within-class stay,
cross-class move with exactly one construction + one retire
and contents preserved over the REQUESTED range,
small->large move, large shrink stays on its mapping,
SIZE_MAX failure atomic, realloc(NULL) as malloc; S8 large
path counters unchanged in shape; final accounting: small
allocs == small frees, live mappings 0.

## 4. Measured results (this host, recorded run)

From `tests/core/recorded_run_g003.txt` (same session,
shared host — Samtal village co-resident; treat as
micro-probe readings, not exit-grade, per INC-G003-10):

- `make clean && make test` — exit 0; all 8 suites PASS:
  smoke 18/18, core 29, b007 69, c005 95, c006 23, c007 122,
  c008 42 (7 skipped-not-yet), d004 4590 (4 threads).
- `make test-g003` — exit 0, 54 checks, 0 failures.
- `make test-e005` (static leg, re-taught) — exit 0,
  32 checks, 0 failures; final AUDIT line:
  published=0 retired=0 (mapping), span_published=7
  span_retired=0, small_alloc=10 small_free=10.
- Latency, G-001 probe shape (2000 iterations of
  malloc+memset-pattern+free per size), mimir leg:
  p50 148 ns (size 8), 148 (16), 166 (32), 167 (64),
  203 (128), 204 (256), 223 (512), 241 (1024) — against the
  G-001 banked baseline of 4889-5167 ns at every size; size
  4096 (large path) p50 5723 ns, the same mapping-path shape
  as banked. The libc leg of the same probe reads p50 ~56 ns
  at small sizes (context only, not a target claim). The
  test-g003 S9 leg in the same run reads p50 500 ns at size
  512 and 6685 ns at size 2048 (mapping path); its size-64
  first leg (2741 ns) is inconsistent with the identical-
  shape probe leg minutes later (167 ns) and is recorded as
  contention noise on the shared host, not attributed.
- Retention, G-001 probe shape, mimir leg: 256 live blocks —
  size 16: RSS live delta 0 KiB; size 64: 16 KiB (ONE page;
  G-001 baseline 8192 KiB); size 256: 64 KiB; size 1024:
  272 KiB; retained after free 16 KiB per exercised class
  (one span each; bound allows two). 10 churn cycles at size
  64: RSS flat at every cycle.
- `nm -D` on the rebuilt .so: exactly 4 defined symbols
  (mimir_malloc, mimir_calloc, mimir_realloc, mimir_free);
  the new counters are bound local. Public header and
  README.md byte-identical to HEAD (git diff --quiet exit 0
  for both, in the recorded run).

## 5. Inconsistencies and gaps — INC-G003

- **INC-G003-01 (resolved in-slice).** Banked test programs
  coupled to the retired accident G-002 section 9 disowns:
  test_b007.c and test_c006.c read a mapping descriptor at
  (user_ptr - page) for EVERY allocation including small
  sizes and assert page-modulus properties G-002 expressly
  retires; the first full-suite run SEGFAULTED in test_b007
  on its first small allocation. Both programs are now
  shape-aware: for span-served sizes they verify the span
  header's clean-init with the same field-by-field rigour
  (magic, self-base, length, usable offset, class size in
  the offset-32 field, class index in flags, and zero in
  the span header's two still-reserved fields at offsets
  20 and 84), and every original descriptor
  check still runs at full strength for every mapping-path
  allocation, including all large boundary sizes. No
  contract-level check changed: patterns, alignment,
  overflow, and health-cycle assertions are untouched. The
  E-005 STATIC proof is re-taught in the same spirit (its
  whole script is <= 1024; see section 2 R-5). Banked
  recorded runs for C-004/C-006/E-005 predate these
  amendments; the G-003 recorded run carries the fresh
  full-suite output.
- **INC-G003-02 (commissioned to G-004).** `make
  test-e005-dyn` SEGFAULTS on a Phase 7 build: the dynamic
  leg validates a mapping descriptor at (ptr - page) for
  span-served pointers (the same coupling as INC-G003-01,
  in the .so leg) and its mapped-bytes model expects one
  mapping per allocation (4 x malloc(64) == 4 mapping
  lengths; under spans it is one page total, and the
  retained-after-free expectation changes likewise). A
  faithful re-teach must rewrite that leg's accounting
  model span-aware (descriptor validation against the span
  header; mapped-bytes expectations in span pages including
  the retention bound). G-004 owns "central fallback + full
  correctness re-run" and is the named owner; the leg is NOT
  green in this slice and is not part of `make test`. No
  other standalone target was found broken: test-g001,
  test-e002, test-e006, test-d008 all PASS on this build
  (e006's page-modulus OBSERVATION line changes as G-002
  section 9 predicted; its checks pass).
- **INC-G003-03 (design note).** Realloc's move copy length
  is the old CLASS extent: the G-002 header layout carries
  no per-object requested size, and request <= class always
  holds, so the preserved range is fully copied and no byte
  outside either object's extent is read or written. A
  future per-object requested-size record would change copy
  cost, not semantics; not taken here.
- **INC-G003-04 (design note).** "Local reuse" in this slice
  is the lock-guarded per-class span free list (L3
  pass-through, G-002 C-5/C-6). No thread-local cache
  exists; G-006 owns thread-local reuse and fork/exit
  behaviour. Multithread correctness on the span path is
  covered by the banked d004 suite (its sizes route small;
  4590 checks, 0 failures).
- **INC-G003-05 (policy fixed here).** The empty-span
  retention bound G-002 deferred is fixed at 2 per class
  (compile-time constant MIMIR_SPAN_EMPTY_RETAIN). If G-007
  tuning wants it measured rather than chosen, the constant
  is the single knob.
- **INC-G003-06 (evaluated, not adopted).** INC-G002-06's
  two-page span for class 1024: not adopted; the one-page
  rule (G-002 section 3's mapping_length invariant) is kept
  and the 928-byte tail waste per class-1024 span stands,
  as G-002's defence recorded.
- **INC-G003-07 (carried).** Shrink-across-class keeps the
  block in its larger class (R-3); under churn this can hold
  memory in big classes. Unmeasured under churn in this
  slice; G-007 measurement territory, as G-002 recorded.
- **INC-G003-08 (cost note).** The free path proves a span
  object is not already freed by walking the span's free
  list: O(free-list length) per free in the worst case
  (bounded by the span's capacity, <= 1018 for class 16).
  Correctness-first per the campaign's posture rules; a
  per-object liveness bitmap is the known optimisation if
  G-007 measurements indict it.
- **INC-G003-09 (carried).** INC-G002-08's B-008 wording
  ("usable extent" vs requested size): R-2 as implemented
  zeroes the class extent, which satisfies either reading's
  safety direction; the wording reconciliation remains owed
  by a B-008-touching slice.
- **INC-G003-10 (evidence grade).** All latency/retention
  numbers here are single-session micro-probe readings on a
  shared host. They are real and recorded, and they are NOT
  the Phase 7 exit comparison; X2/X3 remain half-proven
  until G-009 assembles the exit evidence under a stated
  noise protocol.

## 6. Exit-evidence status (honest)

Phase 7 is NOT exited by G-003. Of the G-001 X-criteria:
X1 (full prior correctness re-run green) is supported by
this slice's recorded `make test` and will be re-confirmed
by G-004's full re-run including the re-taught dynamic leg
(INC-G003-02); X2 and X4 have their first Phase 7 readings
recorded here (latency two orders of magnitude under the
mapping baseline at small sizes; footprint per class down
from 8192 KiB to one page at size 64) but remain unproven at
exit grade until G-009. The C-1..C-8 conditions stand as
G-002 left them, with C-2's table now implemented and
C-5/C-6 in their L3 pass-through form.

## 7. Files changed by this slice

- `src/core/mimir_core.c` — span machinery, dispatcher,
  two-registry free path, calloc/realloc span handling,
  R-5 counters.
- `tests/core/test_g003.c`, `tests/core/mimir_small_g003.h`
  — new (54-check suite + test-only counter header).
- `tests/core/recorded_run_g003.txt` — new (recorded run).
- `tests/core/test_b007.c`, `tests/core/test_c006.c`,
  `tests/abi/test_e005.c` — INC-G003-01 shape-aware /
  two-registry amendments (no check weakened; see the INC).
- `Makefile` — additive `test-g003` target only.
- `docs/PHASE_7_SMALL_OBJECT_PATH_G003.md` — this file.
- `hidden_files/ASSIGNMENT.md` — rewritten for G-003
  (untracked working note, standing keeper pattern).

Public header `include/mimir_alloc.h`: byte-identical.
README.md: byte-identical.
