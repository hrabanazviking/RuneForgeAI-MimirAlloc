# Phase 13 Code Review + API/ABI Audit - M-002

Slice: M-002 (Wave 13, Phase 13 - security and production release)
Team: team-02, branch yrsa/mimiralloc-02
Base audited: 0b00f088ea6e522631b46c1b3abbdd5b3f9cf59d (Wave 12 COMPLETE head)
Host: Raspberry Pi Mimir (purpleviking1), aarch64, cc (Debian 12.2.0-14+deb12u1)
Date of run: 2026-11-07 (recorded run: tests/abi/recorded_run_m002.txt)
Method: full read of every file under src/ and include/ on the Pi,
checked against docs/contracts.md (A-008) and Roadmap Section 9.1;
a fresh make test and a fresh make lib + nm -D were run on the Pi
this tick and are recorded in the recorded run file. No measurement
is claimed that was not run on the Pi this tick. No src/, Makefile,
README or header file is changed by this slice: no genuine code
defect requiring a behaviour change was found; every gap found is
recorded as an INC-M002 item in Section 6.

## 1. Files reviewed (line counts from the Pi)

| File | Lines | Role |
|---|---|---|
| include/mimir_alloc.h | 171 | public header, 4 declarations |
| src/api/mimir_alloc.c | 144 | L1 boundary forwarders + E-005 call counters |
| src/api/mimir_alloc.map | 29 | E-002 version script (export contract) |
| src/core/mimir_core.h | 282 | internal core surface (drain/trim/stats/diag) |
| src/core/mimir_core.c | 2983 | L2/L4/L5 core: mapping path + span path |
| src/platform/mimir_platform.h | 104 | L6 published interface |
| src/platform/mimir_platform.c | 156 | L6 Linux/POSIX form, the one global lock |
| Total | 3869 | |

Build gate: the Makefile compiles everything with
-std=c11 -Wall -Wextra -Werror -pthread; the fresh make lib and
make test builds this tick produced 0 warnings (a warning would
fail the build under -Werror).

## 2. Code review

### 2.1 Checked arithmetic and alignment

Every size computation on an admission or layout path is checked
before it is evaluated (B-007 Section A discipline), verified by
reading each site:

- Page round-up: page_round_up_checked pre-tests the A5 addition
  form and the A6 multiply form before evaluating either
  (src/platform/mimir_platform.c:60-74). Page-size admission
  requires positive, size_t-fitting, power of two
  (mimir_platform.c:24-47).
- Mapping layout: total = usable_offset + size is pre-tested as
  size > SIZE_MAX - usable_offset (src/core/mimir_core.c:671);
  usable_size subtraction is gated (A8), the user-pointer range
  is checked before publication (A9), and both live_bytes and
  live_mappings counter additions are checked before publication
  (A10) - on any gate failure the fresh mapping is released and
  nothing is published.
- calloc: the product is refused as nmemb > SIZE_MAX / size
  before the multiplication is relied on (mimir_core.c:2651);
  overflow is exactly a NULL return, no wrapped allocation.
- Span path: capacity is computed by division from the header's
  own recorded mapping length; carving is bounded by
  span_can_serve (carve_count < capacity), so the carve offset
  product is bounded by one page by construction; span header
  relations (live <= carve <= capacity, usable_size exact) are
  revalidated on every use (span_header_is_live, J-006).
- realloc copy lengths are recorded sizes only: old
  requested_size on the mapping path, old class extent on the
  span path; in-bounds on both sides by the publication records.

Alignment: no numeric alignment value is promised in the public
header (U-15 open, include/mimir_alloc.h:101-103) and none is
needed for the claim made. By construction: mapping-path user
pointers are page base + exactly one system page (page-aligned);
span-path objects sit at page base + 96 + k * class_size, where
96 and all 20 class sizes (16..1024, mimir_core.c class table)
are multiples of 16, so every small object is 16-byte aligned -
the fundamental alignment on AArch64. The MIMIR_DEBUG stride
(class + 32, guards of 16) preserves this. Verdict: SOUND.

### 2.2 Lock discipline (single global lock)

- Exactly ONE lock object exists in the tree: the statically
  initialised pthread mutex at src/platform/mimir_platform.c:140
  (PTHREAD_MUTEX_INITIALIZER; no allocation, no constructor).
  A grep of src/ and include/ this tick found no other mutex,
  spinlock, rwlock, semaphore or atomic-flag lock.
- All allocator metadata (both registries, all counters, the
  retention bound, the quarantine table) is touched only inside
  sections bracketed by mimir_platform_global_lock/unlock;
  core entry points take the lock once and release it before
  every return - including realloc's same-size and shrink early
  returns (mimir_core.c:2789, 2794). The lock is never held
  across a return to the caller and never guards user bytes
  (the calloc zeroing and realloc transfer copy are the two
  documented construction/transfer exceptions).
- The lock IS held across L6 map acquire/release by the banked
  S9.4 decision, making mapping + publication + registry
  insertion one locked step.
- The only state moved without the lock is counter state that
  is atomic by construction: the four L1 E-005 call counters
  (src/api/mimir_alloc.c:113-116, __atomic_fetch_add relaxed)
  and the J-002 failure/posture counters, read back under the
  lock in the snapshot. No decision reads any of them.
- Lock-ACQUIRE failure is handled everywhere: allocation entry
  points return NULL with no ownership passing and count +
  report the failure (J-002/J-007); free leaves the block LIVE;
  drain/trim release nothing. Lock-RELEASE (unlock) failure is
  a different story: see INC-M002-01.

### 2.3 Error paths

- Page-size discovery failure: NULL, no fallback constant, no
  mapping attempted (alloc_locked, span_acquire_locked).
- Backing (mmap) failure: NULL; nothing published; registry and
  counters unchanged; a small request is never silently
  re-routed to a private mapping (G-002 R-1).
- Release failure at retire: F-06 full rollback in both forms -
  retire_locked and span_release_locked restore the descriptor
  / span to LIVE, re-link it and restore counters, so the
  bookkeeping still describes actual memory.
- Invalid free / realloc pointer: non-destructive refusal -
  nothing unmapped, unlinked or counted; validation walks only
  allocator-owned registry links and never dereferences memory
  derived from the unvalidated pointer (registry walks and the
  span free-list walk are bounded, J-006).
- Diagnostics (J-007) fire only on genuine failure NULLs, after
  unlock, from an allocation-free, recursion-guarded channel;
  posture NULLs (U-09/U-12) stay silent and are counted
  separately from genuine failures.

### 2.4 H-001 destructor / drain

- The image destructor (src/core/mimir_core.c:2526-2530) does
  exactly one thing: call mimir_core_drain_retained. No
  constructor is registered; initialisation stays clean by
  construction (static storage + statically initialised lock).
- The drain (mimir_core.c:2154) takes the one global lock and
  runs the shared trim walk with no span limit: it releases
  only spans with live_count == 0 (and, in debug builds, no
  quarantined blocks). Live spans, live small objects and live
  large mappings are never touched; large mappings retain
  nothing after free, so there is nothing else to drain.
- It is non-terminal and idempotent: registries, counters,
  tuning state and the lock are left fully valid, the
  allocator stays usable afterwards, and a call with nothing
  retained returns 0. On lock failure it releases nothing and
  returns 0 (safe; the ambiguity of that 0 is INC-M002-02).
- The destructor allocates nothing and reports nothing; its
  return count is deliberately discarded. Freeing a block
  after its image has unmapped remains the consumer's
  documented duty (free before dlclose), unchanged.

## 3. Roadmap Section 9.1 invariants - graded one by one

Grades: PROVED = exercised by the fresh make test run this tick
(4590 checks, 0 failed) or by a measurement recorded this tick;
INSPECTED = established by reading every relevant code path;
BY DESIGN = holds by construction of the design, with the
failure leg proven; GAP = at least one leg unmet (INC raised).

| # | Invariant (short) | Grade |
|---|---|---|
| 1 | One owner, non-overlapping storage | PROVED |
| 2 | Metadata lifetime matches its memory | INSPECTED |
| 3 | Size/alignment cannot overflow | PROVED |
| 4 | free(NULL) per contract | PROVED |
| 5 | Failed realloc preserves original | PROVED |
| 6 | Cross-thread handoff preserves ownership | PROVED (see INC-M002-04) |
| 7 | Full cache / failed backing: bounded slow path | BY DESIGN |
| 8 | Statistics never drive unsafe decisions | INSPECTED |
| 9 | Sync failure reported or terminates safely | GAP (INC-M002-01, INC-M002-02) |
| 10 | Fallback preserves ownership or explicitly fails | INSPECTED |

Evidence per invariant:

1. PROVED. Mapping path: one private mapping per allocation
   (alloc_locked), so two live blocks cannot overlap. Span
   path: an object is handed out exactly once - carved fresh
   or popped from its span free list under the lock;
   find_span_live_locked refuses a pointer already on the
   free list, so a double free cannot re-publish a live
   object. Suite this tick: distinct-live-pointer checks and
   ALLOC/FREE BALANCE allocs=2088 frees=2088 ok.
2. INSPECTED. retire_locked / span_release_locked capture the
   exact base/length pair in locals before release and never
   read the descriptor/span after a successful release
   (mimir_core.c retire and span-release bodies); metadata
   lives either in static storage or inside the mapping it
   describes, destroyed only with that mapping.
3. PROVED. Section 2.1 lists every checked site; the suite's
   boundary/overflow legs (C-007/C-008 and contract tests, in
   the 4590) exercise calloc overflow, oversize and alignment
   boundaries against this build.
4. PROVED. mimir_core_free returns before any locking or
   descriptor access when ptr == NULL (mimir_core.c:2897-2899);
   the L1 forwarder passes NULL through unchanged; suite
   contract legs cover free(NULL).
5. PROVED. Grow constructs the new block completely before
   any transfer; construction failure returns NULL with the
   original LIVE and untouched; a transfer release failure
   rolls the new block back and retire_locked has already
   restored the original LIVE (F-06). Both registries follow
   the same sequence (mimir_core_realloc, mimir_core.c:2753).
   C-005 legs are inside the 4590 checks run this tick.
6. PROVED for behaviour. Ownership is registry-based, not
   thread-based: any thread may free any live block exactly
   once under the one lock, and no node can sit in two lists
   (single free list per span; single mapping registry).
   D-004 multithread and H-003 cross-thread legs are in the
   suite run this tick. The cross_thread_frees COUNTER is
   partial (mapping path only) - INC-M002-04.
7. BY DESIGN. There is no cache slow path to bound: retained
   memory is capped by the per-class empty-span retention
   bound (default 2, range 0..16; worst case 20 classes x 16
   spans x one page), and a failed backing allocation is a
   defined NULL failure with no re-routing. The failure leg
   is exercised by the suite's failure legs this tick.
8. INSPECTED. Every counter write site was read: counters are
   updated only at publication/release points or as failure
   tallies, and no counter is read by any allocation, free or
   release decision. The single decision-driving scalar is
   mimir_retention_bound, a tuning parameter (not a
   statistic), validated 0..16 before application. The J-007
   cause label is written before unlock and read only for
   the report text, never for control flow.
9. GAP. Safe leg holds: no sync failure ever returns
   overlapping or unpublished memory - acquire failure fails
   the operation with ownership unchanged. Reporting leg
   unmet for RELEASE: every unlock result in the core is
   discarded with a (void) cast (46 lock/unlock call mentions
   in mimir_core.c), and free/drain/trim lock failures are
   silent (INC-M002-01, INC-M002-02).
10. INSPECTED. Every fallback in the tree was read: lock
    failure (fail closed, ownership unchanged), release
    failure (F-06 rollback restores ownership records),
    span-release failure at free (object stays freed, span
    stays retained and tracked), publication-gate failure
    (fresh mapping released, nothing published). There is no
    fallback to another allocator anywhere: absent facilities
    are absent at link time by design (header notes).

## 4. Public API / ABI audit against docs/contracts.md (A-008)

Status frame, stated plainly first: contracts.md assigns NO
ABI. Its Section 5 is PROPOSED in full - no ABI version is
assigned (identifier <unassigned>, Section 5.3), the final
exported symbol set is [UNRESOLVED: U-01], and the namespaced
mimir_* API is outside Section 6 scope [UNRESOLVED: U-03]
(contracts.md:75-76, 840, Appendix A). The public header says
exactly the same (include/mimir_alloc.h:5-7, 23-29: Proposed,
not a stable ABI, mappings by analogy only). This audit
therefore grades CONSISTENCY between header, implementation,
version script and the contract document - not stability,
which nobody claims.

### 4.1 Exported dynamic symbols - exactly the 4

Fresh make lib on the Pi this tick, then nm -D (full output in
the recorded run): the defined dynamic symbols of
build/libmimiralloc.so are exactly

  mimir_malloc, mimir_free, mimir_calloc, mimir_realloc

counted: nm -D --defined-only shows 4, all type T. Every other
name in .dynsym is an undefined import (libc/pthread) or a weak
loader symbol. The internal surface - mimir_core_* (including
drain, trim, stats, diag), mimir_platform_*, the E-005 audit
counters, the G-007 tuning pair and the J-004/J-005 debug
surface - is bound local by the anonymous version script
(src/api/mimir_alloc.map: global = the 4; local: *) plus the
hidden-visibility PIC objects, exactly as designed in E-002.
The anonymous node is deliberate: a named node would add a
version-name symbol and imply a stability promise the header
withholds.

### 4.2 Header declarations vs contracts

include/mimir_alloc.h declares exactly four functions
(lines 135, 140, 151, 165):

  void *mimir_malloc(size_t);
  void mimir_free(void *);
  void *mimir_calloc(size_t, size_t);
  void *mimir_realloc(void *, size_t);

These match, in shape and in documented behaviour, the
contracts Section 2 core-symbol rows for malloc / free /
calloc / realloc (contracts.md Sections 2.2-2.5) BY ANALOGY,
which is the strongest relation available: the namespaced
names have no direct contract rows (U-03). Signatures match
the L1 definitions (src/api/mimir_alloc.c:118-144) and the
version script. C++ consumers see them under extern "C"
guards (header lines 127-129, 167-169). One inventory
inconsistency in the contract document itself: INC-M002-03.

### 4.3 Owed items, recorded honestly

- No SONAME: readelf -d on the fresh .so this tick shows no
  SONAME entry (recorded run). No versioned filename: the
  artifact is the plain build/libmimiralloc.so. No install
  target exists in the Makefile. All three are consistent
  with the unassigned ABI version (contracts U-07 mechanism
  unresolved) and are OWED to the future ABI-freeze slice,
  not defects at Proposed status - INC-M002-05 tracks them
  for the Phase 13 exit criteria.
- Aligned-allocation surface: PROPOSED only. E-006 proposes
  mimir_aligned_alloc; nothing is declared in the header,
  nothing is exported (nm this tick confirms absence), and
  the standard-name candidates aligned_alloc / posix_memalign
  (contracts Sections 2.6/2.7) are likewise absent. Callers
  needing explicit alignment have no MimirAlloc entry point
  today - absence at link time, never a runtime refusal.
- Audit counters: internal / test-only, confirmed. The four
  L1 call counters and the core published/retired counters
  are declared in no installed header, are readable only by
  test-only static builds (E-005 shape (b)), and do not
  appear in the dynamic symbol table (nm this tick).
  Contract status per E-005: Proposed-internal, NOT part of
  the contract or the ABI.
- Also absent, as documented unsupported behaviour: standard
  malloc/free interposition names, malloc_usable_size,
  reallocarray, statistics/trim public entry points.

## 5. Test evidence (this tick, on the Pi)

- make test, run twice this tick from this worktree: exit 0
  both times; final suite line CHECKS: 4590 run, 0 failed;
  ALLOC/FREE BALANCE allocs=2088 frees=2088 ok; RESULT: PASS.
- make lib from a clean build directory: exit 0, 0 warnings.
- nm -D --defined-only on the fresh .so: exactly 4 symbols.
- readelf -d: no SONAME.
- Samtal on the same Pi (http://127.0.0.1:8787/): HTTP 200
  before and after the work; free memory 13G available
  throughout. Full outputs: tests/abi/recorded_run_m002.txt.

## 6. INC-M002 register

- INC-M002-01 (Invariant 9, GAP): unlock failure is never
  observed. Every mimir_platform_global_unlock call in the
  core discards its status with a (void) cast. The design
  comment at mimir_core_alloc states the reason (no report
  may be made while the lock is held, and the allocation is
  already soundly published), which covers the alloc path's
  reasoning but leaves a failed unlock - which would wedge
  every later call - with no counter, no report and no
  termination. Severity: low likelihood (a normal mutex
  unlock fails only on misuse/corruption), high opacity.
  Owed: a post-unlock failure tally in the J-002/J-007 shape,
  or a documented ruling that the posture is accepted.
- INC-M002-02 (Invariant 9, GAP, reporting only): silent
  lock-failure courses. mimir_core_free on lock failure
  returns silently with the block LIVE (safe, F-07), and
  mimir_core_drain_retained / the trim entry points return 0
  on lock failure, indistinguishable from nothing retained.
  No ownership is ever lost on these paths; only the
  reporting leg of Invariant 9 is unmet.
- INC-M002-03 (documentation inconsistency): contracts.md
  Appendix A (lines 919-921) inventories mimir_malloc and
  mimir_free as the namespaced U-03 entries but omits
  mimir_calloc and mimir_realloc, which have been declared
  and implemented since C-005. Documentation gap only; the
  header, version script and implementation agree with each
  other. Owed: an Appendix A amendment when U-03 is next
  touched. U-03 itself remains open by design.
- INC-M002-04 (Invariant 6, observability): the
  cross_thread_frees counter is PARTIAL - mapping-path
  blocks only; small-object frees carry no per-object owner
  record and realloc transfer retires are not counted. This
  confirms pre-existing INC-J002-01 (src/core/mimir_core.h
  lines 138-144). The invariant's behaviour is PROVED; only
  its measurement is partial.
- INC-M002-05 (ABI owed, confirmation): no SONAME, no
  versioned library filename, no install target, anonymous
  version node, ABI version <unassigned>. Consistent with
  Proposed status (contracts Section 5, U-01/U-07 open);
  recorded so the Phase 13 exit criteria carry the freeze
  work explicitly.

## 7. Verdict

The banked tree at 0b00f08 is internally consistent: header,
version script, implementation and dynamic symbol table agree
exactly on a four-function namespaced surface; the single-lock,
checked-arithmetic and rollback disciplines hold under a full
read; 8 of 10 Roadmap 9.1 invariants grade PROVED, INSPECTED
or BY DESIGN, and Invariant 9 grades GAP on its reporting leg
only (INC-M002-01/02) - its safety leg holds everywhere. No
code change is made by this slice. The ABI remains Proposed
by design; the owed freeze items are INC-M002-05 plus the
U-01/U-03/U-07 decisions in contracts.md, which are the
project owner's to take.
