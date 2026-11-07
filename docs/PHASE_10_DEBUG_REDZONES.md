# Phase 10 - Debug-only red zones + canaries (Slice J-004)

Roadmap Phase 10; governing plan docs/PHASE_10_FOUNDATION_PLAN.md
Section 5 (the debug/production distinction plan). Base:
1aab762 (J-002 banked). Wave 10 slice 3 of 9 in roadmap
order as banked by the keeper. All work on the Pi; recorded
run: tests/core/recorded_run_j004.txt.

## 1. What this slice builds

The debug build flavour of plan S5.1, plus the first two
rows of the S5.2 debug-only table that J-004 owns:

- Red zones around (small) allocations: written at
  publication, checked on free and on realloc.
- Canaries where the layout permits: the slot-0 pre-zone is
  the header-adjacent guard; on-demand verification also
  revalidates every span header (the metadata canary).

Everything lives under ONE compile-time macro, MIMIR_DEBUG,
selected by `make DEBUG=1`, which adds -DMIMIR_DEBUG=1 and
directs every object to build-debug/. Default make / make
test / make lib are the banked production build unchanged:
same BUILD=build, same CFLAGS, and every src change in this
slice sits inside #ifdef MIMIR_DEBUG (the production text of
src/core/mimir_core.c is the banked text plus guarded
insertions only). There is no runtime flag, environment
variable or mode word: a production binary does not contain
the debug code at all (S5.1). nm -D is exactly 4 in BOTH
flavours (proven in the recorded run).

## 2. Layout (debug builds only)

Small path only. A debug span keeps its banked form: one
system page, 96-byte header at offset 0, object area from
offset 96. Only the slot arithmetic changes:

    slot  = [ 16 B pre-zone ][ user extent: class_size B ]
            [ 16 B post-zone ]
    stride = class_size + 32
    user pointer = span + 96 + 16 + k * stride,  k = 0..

Guard size justification: 16 bytes is the allocator's own
alignment quantum (every class size is a multiple of 16),
so guard + class + guard keeps every user pointer 16-byte
aligned exactly as production (page base, 96 and 16 are all
multiples of 16, and the stride is a multiple of 16). 16
bytes catches the stated overwrite amounts with room to
spare (the proofs overwrite 1, 4 and 8 bytes) while costing
exactly 32 bytes of span space per object - the smallest
alignment-preserving guard that is a full cache-line
quarter on this host and an exact multiple of the largest
fundamental alignment. The pattern is the single byte 0xA5
repeated: distinct from zero (fresh pages), from the
free-list link (a pointer), and from both magics.

Guards are filled at every publication (fresh carve AND
free-list reuse; on reuse the old guards are verified first
- while a slot is free its guards are allocator-owned, so a
corruption that lands while the slot sits on the free list
is caught at the next allocation of that slot). Checks run:

- on free, BEFORE the free-list link overwrites the user
  extent (mimir_dbg_check_or_abort in small_free_locked);
- on realloc of a span block, BEFORE any case is decided
  (stay / shrink / move all verify first);
- on demand, via mimir_core_debug_verify_all() (internal,
  INC-E001-07 shape (b): declared in src/core/mimir_core.h
  under MIMIR_DEBUG, re-declared in
  tests/core/mimir_debug_j004.h, bound local in the .so).
  It walks every registered span under the one global
  lock, counts a span whose header fails
  span_header_is_live as one metadata corruption without
  trusting its object area, and checks both guards of
  every carved slot not on its span's free list. It
  returns the count found (0 = clean), -1 on lock failure,
  reports nothing and never aborts - the caller decides.

## 3. Detection posture (plan S9.2: fail clearly)

On a guard mismatch at free / realloc / reuse, the
allocator:

1. writes ONE fixed-format line to fd 2 -
   `MIMIR_DEBUG J-004 CORRUPTION red-zone mismatch at
   <where> obj=0x<hex>` - assembled byte-by-byte in a fixed
   static buffer and emitted with a single write(2): no
   allocation, no stdio, no lock recursion (the global lock
   is already held and write(2) never calls back), so the
   channel cannot recursively allocate, honouring J-007's
   coming reporting contract as far as J-001 S5 defines it;
2. abort()s: a defined stop in the calling thread. The
   corrupt block is NOT freed, NOT reused and NOT repaired;
   no counter is touched on the detection path, so J-001 /
   J-002 counter semantics cannot be redefined by a debug
   check (the S5.3 rule carried to red zones).

Debug checks never change which calls succeed: for any
defined call sequence the debug build returns the same
success/NULL outcome as production; the only divergence is
the S5.2 exception - actual corruption fails clearly, and
it is reported, never silent.

## 4. Overhead (measured / exact)

Per-allocation byte overhead is EXACT from the layout
arithmetic: 32 bytes of span space per live small object
(16 + 16 guards); the user extent, bytes_requested and
bytes_allocated are unchanged (test_j004 leg 2 proves the
J-002 deltas count class/usable bytes only, guards never
counted). Span capacity per class, page = 16384 on this
host, capacity = (page - 96) / stride, observed via the
J-002 snapshot in test_j004 leg 1 (expected == observed
for all 20 classes):

    class   prod cap   debug cap
       16       1018         339
       32        509         254
       48        339         203
       64        254         169
       80        203         145
       96        169         127
      112        145         113
      128        127         101
      160        101          84
      192         84          72
      224         72          63
      256         63          56
      320         50          46
      384         42          39
      448         36          33
      512         31          29
      640         25          24
      768         21          20
      896         18          17
     1024         15          15

Retained-bytes effect: a span is still exactly one page;
a debug workload needing more objects of a class than one
debug span holds acquires (and may retain, within the
banked per-class bound) proportionally more spans - the
capacity ratios above are the whole effect. D-CACHED keeps
its production definition (retained EMPTY spans) untouched:
no quarantine exists in this slice and no counter changed.

Wall time (timing-never-an-oracle, single samples from the
recorded run's bench leg, 20000 x malloc(64)/memset/free):
production 728 ns/op, debug 1068-1156 ns/op across runs of
this slice - the guard fill + verify cost is real, roughly
+50% on this micro-bench, and is reported as a distribution
fact for J-008 to grade properly, never as a verdict.

## 5. Proof (tests/core/test_j004.c, DEBUG=1: 23/0 PASS)

- Leg 1: capacities for all 20 classes == debug arithmetic.
- Leg 2: J-002 counter deltas exact (guards not counted).
- Legs 3-4: verify_all clean; detects a 1-byte post-zone
  overwrite; clean again after TEST-side restore (the
  allocator itself never repairs).
- Legs 5-7 (disposable children): post-zone +8 B on free,
  pre-zone -4 B on free, post-zone +1 B on realloc - each
  child dies by SIGABRT with the fixed fd-2 record naming
  the check point (free / realloc), parent-asserted.
- Leg 8: clean child (200 alloc/write/free cycles) exits 0.
- Leg 9: 500-block all-class workload, calloc zeroing,
  verify_all clean throughout - no false positives.
- Leg 10: B-007 C1.4 header clauses replicated in debug
  arithmetic - all hold (see INC-J004-02).
- Production flavour of the same binary: banked capacities,
  bench, PASS (3/0). Production make test: 4590/0 suite
  total with every binary at base counts; test-j002 550/0.

Debug flavour suite (make DEBUG=1, per binary): smoke
exit 0, core 29/0, c005 95/0, c007 122/0, c008 42/0,
d004 4590/0, test_j004 23/0 - plus exactly the three
INC-J004-02 divergences below. No run aborted anywhere:
no corruption false positive fired in any suite binary.

## 6. INC register

- INC-J004-01 (scope): large (mapping-path) allocations
  carry NO red zones in this slice. A mapping's usable
  region is page-granular and per-allocation; a guard page
  or tail canary there is a different mechanism (and the
  tail beyond usable_size is not writable-safely
  attributable). Small allocations - the overflow-prone
  path - are covered; large-path guards are OWED to a
  future slice if the roadmap's "where appropriate" is
  read to include them. Overwrites WITHIN a small block's
  class slack (requested < class_size) are likewise not
  detectable by any guard outside the class extent and
  are not claimed.
- INC-J004-02 (banked tests encode production layout):
  three banked checks compute production layout arithmetic
  exactly and therefore diverge under DEBUG=1 BY DESIGN
  (plan S5.2 permits debug layout change, reported as
  overhead): test_b007 C1.4 span form and test_c006 B-007
  C1.4 (their shared g003_span_clean stride identity
  (off-96) % class == 0; debug off is 112 mod stride), and
  test_j002 legA/legE capacity expectations (production
  formula; observed debug capacities 113 and 339 equal
  the J-004 arithmetic exactly). All other checks in
  those binaries pass under debug (b007 68/69, c006 22/23,
  j002 failures confined to the 2 capacity lines) and NO
  divergence is a corruption false positive. The banked
  tests were NOT edited in this slice; flavour-aware
  grading of these three lines is OWED to J-008 (the
  both-flavours diff slice). Leg 10 of test_j004 proves
  every C1.4 header-field clause holds in debug form.
- INC-J004-03 (layering, debug-only): the detection
  channel names write(2) / abort() inside src/core under
  MIMIR_DEBUG because L6 publishes no reporting primitive
  and the core may name no OS facility (B-006 S9.1).
  Production code names neither. J-007, which owns the
  production reporting channel, should lift a reporting
  primitive into L6 and re-point this debug channel at
  it; until then the debug-only exception stands,
  recorded here rather than hidden.
- INC-J004-04 (detection limits, honest): guards detect
  writes that LAND in the 32 guard bytes. An overwrite
  that jumps clean over a guard (stride-length or wild
  writes), a write into another live object's extent, and
  corruption of free-list links inside freed user extents
  are NOT detected by this slice (metadata walks are
  J-006). Use-after-free writes into a still-mapped span
  are caught only if they touch a guard or the block is
  re-verified before reuse.

## 7. NOT covered (explicit)

- Large-mapping red zones / guard pages (INC-J004-01).
- Poisoning and quarantine: J-005, untouched here.
- Metadata corruption probing beyond header revalidation
  in verify_all: J-006.
- Production error reporting: J-007 (this slice's channel
  is debug-only and pre-figures its contract only).
- Both-flavours observable-result diffing: J-008.
- Double-free / foreign-pointer behaviour: unchanged
  banked posture; guards add nothing there.
