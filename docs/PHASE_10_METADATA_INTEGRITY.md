# Phase 10 - Metadata Integrity Checks + Corrupted-Metadata Posture (Slice J-006)

Slice: **J-006 (team-06)**. Wave 10, Phase 10 (roadmap:
"Protect against integer overflow and corrupted
metadata; fail clearly rather than pretending to repair").
Base: `1aab76258ccc8af8e29bb482c674484c47d8f117` (J-002)
on branch `yrsa/mimiralloc-06`, worktree
`/home/pi/mimiralloc-team-06` on Raspberry Pi
`purpleviking1` (aarch64, system page size 16384).
Governing texts: `docs/PHASE_10_FOUNDATION_PLAN.md`
(J-001), in particular S2.9 (validation that already
exists) and S5.2 (J-006 owns the "extra metadata
consistency walks" debug half of P10-9) and exit item
X5; the Wave 10 queue in the coordinator's
`WAVE_PLAN.md`. Host state: `free -g` 15 GiB total,
13 GiB available at muster; Samtal HTTP 200 on port
8787 before and after. Sibling slices J-003 / J-004 /
J-005 are built on branches from this same base and
are NOT in it: no `MIMIR_DEBUG` build flavour exists
here (see INC-J006-08).

> **BOTTOM LINE, STATED FIRST.** Every metadata
> structure the allocator keeps was inventoried from
> the sources and probed, in disposable child
> processes only, field by field (25 probes,
> `tests/core/test_j006.c`). Before this slice, the
> banked lookup validation (J-001 S2.9) caught
> corruption of a descriptor's or span's magic, state,
> self-base and (for spans) class agreement, and
> refused non-destructively; corruption of mapping
> lengths, requested sizes, carve/live counts and
> free-list links was trusted blindly and produced
> wrong-length `munmap`s, out-of-mapping handouts,
> wild dereferences and unbounded loops. This slice
> adds, at the same revalidation points and with no
> behaviour change for valid metadata: exact layout-
> relation checks in both `is_live` validations,
> counter-bounded registry walks on both ownership
> paths, a range-checked, count-bounded free-list
> walk in span free, shape validation before a
> free-list node is ever handed out, and a test-only
> integrity walk (`mimir_core_metadata_check`,
> shape (b)) that cross-sums both registries against
> the static counters. Of the 25 probes: **1 clean
> baseline, 16 DETECTED** (refused at lookup and/or
> flagged by the walk, no wrong memory, no crash),
> **6 SILENT** (proven misbehaviour, recorded as INC
> items with smallest remedies, not repaired by
> invention here), and **2 CRASH** (a registry
> `list_next` forged to unmapped memory faults the
> walk that follows it -- no in-process check can
> validate a pointer without reading it;
> INC-J006-01). Production: `make test` **4590/0**
> unchanged, `nm -D` exactly **4**, public header and
> README byte-identical, valid-path cost **+~120
> ns/op on the small alloc/free pair** (+9..17%
> across two interleaved A/B sessions, S6), large
> pair within noise. No run in this
> slice claims repair of arbitrary corruption: the
> posture is *detect what is detectable, refuse
> non-destructively, never hand out memory the
> validated fields do not describe*.

## 1. Method

1. Inventory from the banked sources only
   (`src/core/mimir_core.c/.h`, `src/platform/`,
   `src/api/`), field by field (S2).
2. For each field: what its corruption looks like,
   what the code did before this slice, what it does
   now -- the "before" column is source-read, the
   "now" column is probe-proven in a child process.
3. Probes: each child builds fresh allocator state,
   corrupts one field of one live structure in
   place, exercises the allocator, and exits with a
   class code (10 DETECTED / 42 SILENT / signal).
   The parent grades exit code or signal under a
   10 s deadline, so a hang is a recorded outcome.
   Corruption never happens in the parent, in a
   long-lived process, or outside the child's own
   allocator metadata.

## 2. Inventory and per-structure posture

### 2.1 The 96-byte in-mapping descriptor (`struct mimir_descriptor`)

At the base of every large mapping's reserve page
(B-006 S7.3). User pointer = `mapping_base +
usable_offset`. Reached by free/realloc through
`find_candidate_locked` + `descriptor_is_live`.

| Field (offset) | Corruption looks like | Before J-006 | After J-006 (probe) |
|---|---|---|---|
| magic (0) | any byte flipped | detected: `is_live` fails, free refuses, block leaks | unchanged (desc-magic: DETECTED) |
| mapping_base (8) | != descriptor's own address | candidate pointer computation misses or `is_live` fails; refusal | unchanged (desc-base: DETECTED) |
| state (48) | != LIVE | detected, refusal | unchanged (desc-state: DETECTED) |
| usable_offset (24) | 0, small, huge | candidate miss (refusal) for most values; not itself validated | also relation-checked (`!= 0`, `<= mapping_length`) (desc-uoff: DETECTED) |
| mapping_length (16) | changed alone | **silent**: `is_live` passed; `retire_locked` munmapped the wrong length | relation `usable_size == length - usable_offset` fails: refusal (desc-len-incons: DETECTED) |
| mapping_length + usable_size (16, 40) | changed consistently | silent wrong-length release | **still silent operationally** (free credited Q-RELEASED 4 pages for a 3-page mapping); checker byte-sum flags it (desc-len-consist: SILENT, INC-J006-02) |
| requested_size (32) | inflated past usable extent | **silent**: realloc shrink/grow arithmetic and transfer copy used it as truth | relation `0 < requested <= usable_size` fails: realloc refuses NULL, original live (desc-requested: DETECTED) |
| usable_size (40) | changed alone | only read at calloc construction; post-publication corruption inert until a relation partner moved | relation-checked as above |
| flags (52) / reserved (72, 24 B) | non-zero | inert (nothing reads them), C1.4 zero-duty silently broken | still inert operationally **by design**; the integrity walk flags them (desc-flags-reserved: DETECTED by walk, INC-J006-09) |
| list_next / list_prev (56, 64) | cycle | **unbounded loop** in `find_candidate_locked` | walk bounded by `mimir_live_mappings`: refusal, no hang (desc-next-cycle: DETECTED) |
| list_next (56) | wild (unmapped) | SIGSEGV in the walk | **unchanged: SIGSEGV** (desc-next-wild: CRASH, INC-J006-01) |
| list_prev (64) | wrong predecessor | `registry_remove` writes through it on retire | walk cross-checks `list_prev` against walk order (walk-detected only) |
| owner-thread scratch (reserve page offset 96, NOT in the descriptor; J-002) | bytes flipped | `cross_thread_frees` miscounts | **unchanged: statistics lie, memory unaffected** (desc-owner-scratch: SILENT, INC-J006-03) |

### 2.2 The 96-byte span header (`struct mimir_span_header`)

At offset 0 of every small-object span's single page
(G-002 S7). Object area = page bytes [96, page size).
Reached through `find_span_live_locked` +
`span_header_is_live`; served from by
`small_alloc_locked`.

| Field (offset) | Corruption | Before J-006 | After J-006 (probe) |
|---|---|---|---|
| magic (0) / state (48) / mapping_base (8) / usable_offset (24) | any | detected: span refused for free, bypassed for alloc, objects leak | unchanged (span-magic: DETECTED) |
| class_size (32) alone | disagrees with class_index | detected (agreement check) | unchanged (span-class-mismatch: DETECTED) |
| class_index (52) + class_size (32) | changed consistently to another class | **silent**: stride arithmetic misreads live objects (offset-32 object becomes "interior") | **operationally still silent** (one block freed under the false class, the next unfreeable); walk detects only via the `class_active` cross-check (span-class-consist: SILENT, INC-J006-05) |
| mapping_length (16) alone | halved/doubled | capacity and release arithmetic trusted it | relation `usable_size == length - 96` fails: refusal (span-len-incons: DETECTED) |
| mapping_length + usable_size | changed consistently | **silent**: capacity doubles, carving hands out objects past the real page end | **still silent operationally** (probe carved an object at the real page end; no write attempted); walk byte-sum flags it (span-len-consist: SILENT, INC-J006-02 kin) |
| live_count (80) | set to 0 with objects live | **dangerous**: trim/drain/retention treat the span as EMPTY and release it under live blocks | unchanged operationally (drain probe: span unmapped under live blocks); walk cross-sum (`sum live_count == live_small_objects`) flags it (span-live-zero: SILENT, INC-J006-06) |
| live_count (80) | inflated | span never empties: leak only | relation `live <= carve` catches inflation past carve_count; within-carve inflation stays a leak |
| carve_count (88) | reduced below live_count | **silent**: re-carving could re-hand offsets of live objects (overlap) once the free list drained | relation `live <= carve` fails: span refused/bypassed (span-carve-reduced: DETECTED) |
| carve_count (88) | inflated past capacity | free-list membership arithmetic trusted it | relation `carve <= capacity` fails: refusal (span-carve-inflated: DETECTED) |
| carve_count (88) | reduced while span EMPTY with a non-empty free list (analysis, not probed) | re-carve duplicates free-list offsets after the list drains: overlap among *free-then-realloc'd* objects | relations still pass (`live == 0`); recorded as analysis under INC-J006-04's remedy family |
| free_head (72) | wild / misaligned / uncarved | **wild handout or crash**: alloc popped it as an object; free's walk dereferenced it | never followed: alloc shape-validates the head before popping and otherwise carves; free's walk range-checks each node before reading it (span-freehead-wild: DETECTED) |
| free_head (72) | points at a LIVE object of the same span | **silent overlap**: the live object is handed out again | **unchanged: silent overlap, proven in probe** (write via the new pointer visible via the old); shape and counts cannot tell live from free (span-freehead-live: SILENT, INC-J006-04) |
| in-object free-list link (first 8 bytes of a freed object) | cycle | **unbounded loop** in free's already-freed walk | walk bounded by `carve_count`: refusal (span-freelist-cycle: DETECTED) |
| in-object free-list link | wild | dereferenced: crash | range-checked before any read: refusal (same probe family as free_head wild) |
| list_next (56) | cycle | **unbounded loop** in span walks | ownership walk bounded by `mimir_live_spans`: refusal (span-next-cycle: DETECTED); non-ownership walks still unbounded (INC-J006-07) |
| list_next (56) | wild | SIGSEGV | **unchanged: SIGSEGV** (span-next-wild: CRASH, INC-J006-01) |

### 2.3 Registry anchors, class_active hints, static counters (.bss)

`mimir_registry_head`, `mimir_span_head`,
`mimir_class_active[20]`, the live counters and the
J-002 statistics state are fixed static storage, not
in any mapping a user block abuts: corrupting them
needs an arbitrary write, not a heap overflow. What
this slice can honestly say: the integrity walk
cross-validates every counter they feed (walked
count/bytes/live sums vs the statics, and each
`class_active` hint against the walked registry),
so *inconsistent* corruption of them is walk-
detectable; corruption performed consistently
across a counter and its registry is not detectable
from inside. No probe is claimed for .bss fields.

## 3. What this slice implements (all in `src/core/mimir_core.c`)

1. **`descriptor_is_live` relations** (J-006):
   `usable_offset != 0`, `usable_offset <=
   mapping_length`, `usable_size == mapping_length -
   usable_offset`, `0 < requested_size <=
   usable_size` -- exactly the relations
   `alloc_locked` publication writes (a realloc
   shrink only lowers `requested_size`).
2. **`span_header_is_live` relations**: mapping
   holds header + at least one object,
   `usable_size == mapping_length - 96`,
   `live_count <= carve_count <= span_capacity` --
   exactly what `span_acquire_locked` writes and the
   carve/free discipline maintains.
3. **Bounded ownership walks**: `find_candidate_
   locked` stops after `mimir_live_mappings` nodes;
   `find_span_live_locked` after `mimir_live_spans`
   spans. A valid registry holds exactly the counted
   nodes, so no valid walk is ever cut.
4. **Free-list walk hardening** in
   `find_span_live_locked`: each node is range-
   checked against the span's mapped object area
   (bounds hoisted to locals; the full 8-byte link
   read must lie inside the page) before it is read,
   and the walk is bounded by `carve_count`.
5. **Handout validation** in `small_alloc_locked`
   (+ `span_can_serve`): a `free_head` is popped
   only if `span_free_node_valid` proves it a carved
   object start of this span; otherwise the span
   serves by carving and the corrupt head is left
   in place for the walk to flag.
6. **`mimir_core_metadata_check`** (the J-001 S5.2
   extra consistency walk): one locked pass over
   both registries verifying every header against
   its strengthened validation, `list_prev` against
   walk order, walked counts/byte sums/live sums
   against the static counters, each span's free
   list (full shape validation; exactly
   `carve_count - live_count` nodes), each
   `class_active` hint against the walked registry,
   and descriptor flags/reserved still zero
   (B-007 C1.4). Returns violation count (0 clean)
   or -1 on lock failure. Declared in
   `src/core/mimir_core.h`, re-declared test-only in
   `tests/core/mimir_integrity_j006.h` (shape (b));
   no production entry point calls it.

Deliberately NOT done: no field was added to either
96-byte header (both are fully assigned); no repair
of any corruption is attempted anywhere; no new
failure is *reported loudly* -- refusal is still
silent because the reporting channel is J-007's
slice (the INC-J001-04 gap stands, named there).

## 4. Probe evidence (tests/core/test_j006.c, 25/25)

Baseline valid metadata: walk 0 before/after, all
frees clean. DETECTED (16): desc magic, state, base,
length-inconsistent, requested_size, usable_offset,
list_next cycle, flags/reserved (walk); span magic,
class mismatch, carve reduced, carve inflated,
length-inconsistent, free_head wild, free-list
cycle, list_next cycle. SILENT (6): desc
length+usable consistent, desc owner scratch, span
class consistent, span live_count zero, span
length+usable consistent, span free_head naming a
live object. CRASH (2, expected SIGSEGV): desc and
span list_next wild. No probe timed out in the
final run; the parent's 10 s deadline is the hang
oracle, and it fired exactly once during
development -- on the *statistics snapshot's*
unbounded span walk, not on a bounded ownership
walk (INC-J006-07).

## 5. What did not change

- `include/mimir_alloc.h`, `README.md`:
  byte-identical to base (git diff empty).
- `nm -D --defined-only` on the production shared
  library: exactly 4 (`mimir_malloc`, `mimir_free`,
  `mimir_calloc`, `mimir_realloc`); the new walk is
  bound local like every shape-(b) surface.
- `make test`: 4590 checks, 0 failed -- the same
  count as base; test-j002 (statistics) still PASS.
- No defined call sequence changes outcome: the
  added checks reject only states publication
  cannot produce (each relation is an invariant of
  the construction code, cited per check).

## 6. Measurement (valid-path cost of the always-on checks)

Bench: 256 small (64 B) or 64 large (4096 B)
alloc/free pairs per round, 2000 rounds, interleaved
A/B against a base-core binary built from
`1aab762` sources in the same minutes (Pi under
sibling-team load; large path is syscall-dominated
and noisy):

| pair | base (3 rounds) | J-006 (3 rounds) |
|---|---|---|
| small alloc+free | 1397 / 1352 / 1344 ns | 1461 / 1485 / 1504 ns |
| large alloc+free | 6356 / 6140 / 6050 ns | 6155 / 8560 / 5823 ns |

Small-path delta **+~120 ns/op (~+9%)** in the
first A/B session, and +180..+280 ns/op in the
second session recorded in
`tests/core/recorded_run_j006.txt` as sibling-team
load on the Pi rose (base rounds were stable at
1354-1400 ns in both) -- call it **+9..17%** under
load, the price of the two `is_live` relation sets
(two capacity divisions) and the pop validation;
large path within run-to-run noise in both
sessions. An earlier form of this
slice validated every free-list node with full
shape arithmetic on the free walk and measured
**+54%** on the small pair; that is why the hot
walk uses hoisted range bounds and full validation
lives only at handout and in the integrity walk.
The integrity walk itself runs only when a test or
debug caller asks; production never calls it.

## 7. INC items (honestly owed, not repaired here)

- **INC-J006-01 (wild registry link faults).** A
  `list_next` forged to unmapped memory SIGSEGVs
  whichever walk follows it first -- ownership
  walks, the integrity walk, statistics -- proven
  by probes desc-next-wild and span-next-wild.
  No in-process check can validate a pointer
  without dereferencing it. Smallest remedy: a
  static extent index of live mappings/spans
  against which links are range-checked before
  being followed (a design change: unbounded
  mapping count vs fixed static storage must be
  reconciled first). Until then this class is
  named UNDETECTABLE in-process; a disposable-
  process checker is the containment.
- **INC-J006-02 (consistent length corruption).**
  `mapping_length`/`usable_size` (descriptor) or
  `mapping_length`/`usable_size` (span) corrupted
  *consistently* satisfy every operational
  relation: the descriptor form released a
  never-mapped length and credited Q-RELEASED for
  it; the span form carved objects past the real
  page end. The integrity walk's byte-sum catches
  both. Smallest remedy: a redundant record of
  each mapping's true length outside the mapping
  (static side table, as INC-J006-01) or a header
  checksum field -- the headers have no spare
  bytes except descriptor `reserved`, whose use
  needs a B-006/B-007 amendment.
- **INC-J006-03 (owner-thread scratch).** Flipping
  the J-002 owner record at reserve-page offset 96
  falsifies `cross_thread_frees` by +1 on an
  ordinary same-thread free. Statistics only; no
  allocator decision reads it (Invariant 8).
  Remedy: none cheap; accepted as diagnostics-grade
  trust, recorded here so no later slice mistakes
  the counter for evidence about memory safety.
- **INC-J006-04 (free_head naming a live object).**
  Proven silent overlap: the next allocation
  returned a still-live object and writes crossed.
  Shape and count invariants cannot distinguish a
  live object from a free node. The analysis-only
  kin case -- `carve_count` reduced on an EMPTY
  span with a populated free list -- re-carves
  free-list offsets after the list drains, with
  the same root cause. Smallest remedy: a per-span
  allocation bitmap (or J-005-style quarantine
  bookkeeping) kept OUTSIDE the 96-byte header,
  e.g. a static side bitmap indexed by span -- a
  design slice, not this one.
- **INC-J006-05 (consistent class corruption).**
  class_index + class_size changed together pass
  the agreement check; operational paths then
  mis-stride the span (one block freed under the
  false class, the next judged interior and
  unfreeable). The walk catches it only indirectly,
  via the `class_active` hint disagreeing with the
  header. Smallest remedy: as INC-J006-02 (side
  record of each span's class), or have the walk's
  cross-check promoted to the free path -- the
  latter costs a registry walk per free and was
  rejected on the S6 measurement.
- **INC-J006-06 (live_count trusted by release
  paths).** `live_count == 0` is the sole emptiness
  test for retention overflow, trim and drain; a
  corrupted 0 released a span under two live
  blocks (probe, mincore-proven). The walk's
  cross-sum detects the inconsistency but nothing
  operational consults it. Smallest remedy: have
  trim/drain first verify `sum(live_count) ==
  mimir_live_small_objects` under the same lock
  and refuse to release anything on mismatch
  (one extra walk on cold paths only) -- a small
  follow-up slice; deliberately not folded in here
  because it changes drain semantics J-003's
  balance proof grades.
- **INC-J006-07 (unbounded non-ownership walks).**
  The statistics snapshot's span walk,
  `retained_snapshot_locked`, `trim_some_locked`,
  `empty_spans_of_class_locked` and the span search
  inside `small_alloc_locked` still follow
  `list_next` without a bound: a cycling link
  hangs them. Demonstrated during this slice's
  development, when an early span-next-cycle probe
  hung in `mimir_core_stats_snapshot` and was
  killed by the parent deadline (the shipped probe
  restores the link before snapshotting, and says
  so). Smallest remedy: apply this slice's
  counter-bound pattern to those five walks --
  mechanical, no semantics change.
- **INC-J006-08 (J-004 / J-005 reconciliation,
  banking-time).** No `MIMIR_DEBUG` flavour exists
  at this base, so the S5.2 "debug half" walk is
  compiled always (shape-(b) internal, exactly the
  statistics/drain precedent) and invoked by
  nothing in production. When J-004 (43be05d)
  banks its `DEBUG=1` build, the coordinator
  should gate any *automatic* invocation of
  `mimir_core_metadata_check` under that flavour;
  the function itself needs no change. J-005
  (86ce5eb) poisons freed extents in debug builds:
  if its quarantine ever threads a block onto a
  span free list while poisoned, the poison pattern
  occupies the link's 8 bytes -- union-merge must
  confirm J-005's design keeps quarantined blocks
  off the free list until reuse (its doc says
  quarantine precedes reuse), in which case the
  two compose: J-006 validates links only on
  blocks that have actually been freed onto a
  list. Makefile: J-006's block is appended at the
  very end, additive, no line shared with the
  J-003/J-004/J-005 blocks.
- **INC-J006-09 (flags/reserved: walk-only).**
  Descriptor flags/reserved corruption is flagged
  only by the walk, never by an operational path,
  because nothing reads those fields and adding a
  free-path failure for them would punish a state
  with no operational meaning. If a future slice
  assigns flags a meaning, its validation joins
  `descriptor_is_live` in that slice.

- **INC-J006-10 (banking fix: free-node shape
  is layout-relative).** INC-J006-08 predicted
  `span_free_node_valid` would need no change
  when J-004/J-005 banked; the union run
  proved that wrong for exactly one function.
  J-004 carves debug objects at `first +
  MIMIR_DBG_GUARD_SIZE + k *
  mimir_dbg_stride(class_size)`, so the
  production shape arithmetic (`(addr - first)
  % class_size == 0`, index by `class_size`)
  rejected every legitimate debug free node:
  on the verified union tree, `DEBUG=1
  test-j005` failed deterministically at
  `reuse returns an evicted (verified) block`
  (81 checks / 1 failure) while every other
  leg of the merged battery stayed green.
  Fix (banking-fix commit on top of 9920686):
  `span_free_node_valid` now validates
  against the placement of the layout it is
  compiled for -- under `MIMIR_DEBUG`, first
  object at header + guard, stride
  `mimir_dbg_stride(class_size)`, bound
  `carve_count`, exactly as J-004 defines
  them; the production branch is the shipped
  arithmetic, unchanged. Strength is kept in
  both layouts: guard bytes, slot interiors,
  uncarved slots and wild values still fail.
  The sibling J-006 checks needed no change:
  the `find_span_live_locked` free-list walk
  is range-and-count only (layout-agnostic),
  `span_header_is_live`'s carve bound goes
  through `span_capacity`, which J-004
  already made debug-aware, and the large
  (descriptor) path carries no debug layout
  (INC-J004-01). Re-verified on the union
  tree with the fixed file (sha256 in
  tests/core/recorded_run_j006.txt):
  `make test` 4590/0, test-j002 550/0,
  test-j003 2155/0, test-j006 25/0,
  `DEBUG=1` test-j004 23/0, `DEBUG=1`
  test-j005 81/0, `nm -D` exactly 4 in both
  flavours, Samtal HTTP 200 before/after.

## 8. Posture, in one paragraph

For every corruption class this slice could make
detectable without changing valid behaviour, the
allocator now **fails clearly within today's
vocabulary**: it refuses the operation
non-destructively -- nothing unmapped, unlinked,
counted or handed out on suspicion -- and the
integrity walk names the inconsistency to any test
or debug caller. It never pretends to repair
corrupted metadata, never hands out memory the
validated fields do not describe, and its two
remaining crash/silent families (INC-J006-01,
-02, -04) are named with their proving probes and
smallest remedies rather than smoothed over. Loud
*reporting* of these refusals awaits J-007's
allocation-free channel, exactly as J-001 planned.
