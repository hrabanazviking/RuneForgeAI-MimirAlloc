# Phase 10 — Statistics Counters Implementation (Slice J-002)

Wave 10, Phase 10 (Diagnostics and hardened builds). Base:
0dd6f7a (Wave 9 complete). Branch: yrsa/mimiralloc-02.
Team: TEAM-02, solo lead, sequential.

Governing material: roadmap Phase 10 ("Expose counters:
requested, allocated, cached, mapped, released and peak
bytes; allocation failures; span occupancy; cross-thread
activity; optional lock contention. Define exactly what
each counter means"); the I-002 quantity definitions in
docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md Sections 3.2
and 4.1 (this slice reuses that vocabulary and does not
redefine any existing quantity); the E-005 six internal
counters and the I-003 locked Q-MAPPED / Q-RETAINED
snapshots as the surface precedent.

J-001 (the Phase 10 foundation plan and counter-definition
contract, TEAM-01) is being built in parallel from the
same base; its contract document is NOT in this base.
Every definition choice this slice had to make on its own
is recorded as an INC-J002 item in Section 6 for
reconciliation at banking time.

## 1. What was built

One new internal entry point, in the INC-E001-07 shape (b)
precedent of E-005 / G-003 / I-003 — reachable only by
linking the library objects directly, hidden from the
shared library (nm -D stays exactly 4), declared in the
internal header src/core/mimir_core.h and re-declared for
tests only in tests/core/mimir_stats_j002.h:

    int mimir_core_stats_snapshot(struct mimir_stats *out);

All statistics state is fixed static storage in
src/core/mimir_core.c below L1, updated at the exact
publication / release / entry points named in Section 2,
and read out as ONE locked snapshot (Section 3). No
public symbol, header, or behaviour changes:
include/mimir_alloc.h and README.md are byte-identical
to base, and the full pre-existing suite is unregressed
(Section 5). Counter updates never drive an allocation
decision (Invariant 8) and never change a return value:
every update point is a counter increment beside an
already-decided outcome.

Where each counter is fed (all in src/core/mimir_core.c):

- bytes_requested / bytes_allocated: at the two
  publication points — alloc_locked (mapping path) and
  small_alloc_locked (span path).
- peak_mapped: a sampler called at exactly those two
  publication points (the only points where Q-MAPPED
  grows), inside the same locked sections.
- bytes_released: at the two successful-release points —
  retire_locked and span_release_locked — after the L6
  release has succeeded (a rolled-back F-06 release
  counts nothing).
- alloc_failures / alloc_posture_nulls: at the four core
  entry points (alloc / calloc / realloc; free returns
  nothing and has no NULL outcome), on the paths itemised
  in Section 2. These two are also incremented outside
  the lock (posture gates and lock failure itself), so
  they use __atomic_fetch_add (relaxed), exactly like the
  E-005 audit counters; every other J-002 counter is
  touched only under the one global lock.
- cross_thread_frees: in mimir_core_free only, after the
  descriptor is validated LIVE and before retire_locked,
  comparing the freeing thread against the owner recorded
  at publication (Section 2, PARTIAL).
- bytes_cached / span occupancy / live totals: computed
  from the registries during the snapshot's own locked
  walk — never maintained as separate counters that could
  drift from the registries they describe.

## 2. Definitions — exactly what each counter means

| Counter | Status | Exact definition |
|---|---|---|
| bytes_requested | EXISTS | Cumulative sum of caller-requested sizes of successful NEW block publications: mimir_malloc(size), mimir_calloc's checked product, mimir_realloc(NULL, n) (counted once, through mimir_core_alloc), and the new size of a realloc grow/move construction. A staying, same-size or shrink realloc publishes nothing and adds nothing. Monotone; never decremented by frees. |
| bytes_allocated | EXISTS | Cumulative sum of usable bytes actually granted at those same publications: for a span-served block, its class size; for a mapping-path block, usable_size = mapping_length - one system page. Always >= the same publication's requested bytes; the cumulative difference from bytes_requested is exactly the total rounding slack granted. Monotone. |
| bytes_cached | EXISTS | Q-RETAINED (I-002 S3.2), at the snapshot instant: sum of mapping_length over registered spans with live_count == 0, computed by registry walk under the lock. The only cache in this allocator; the mapping path retains nothing. |
| bytes_mapped | EXISTS | Q-MAPPED (I-002 S3.2), at the snapshot instant: mimir_live_bytes + mimir_live_span_bytes (live mapping-path bytes + all registered span bytes, live and retained-empty). Identical to the I-003 mimir_core_mapped_bytes() value at the same instant — the proof test cross-reads both at every quiescent point. |
| bytes_released | EXISTS | Q-RELEASED (I-002 S3.2): cumulative sum of mapping_length over SUCCESSFUL releases of PUBLISHED mappings and spans (retire_locked and span_release_locked successes; trim and drain releases pass through span_release_locked and are counted). Failed-publication cleanup releases (a mapping acquired and released inside one failed alloc_locked) are NOT counted: nothing was ever published, per the I-002 wording. Monotone. |
| peak_mapped | EXISTS | Q-PEAKMAPPED (I-002 S3.2): the maximum Q-MAPPED sampled at the two publication points under the lock. Because Q-MAPPED grows only at publications, sampling exactly there is exhaustive, not approximate. peak_mapped >= bytes_mapped at every snapshot. |
| alloc_failures | EXISTS | Q-FAILED-OS (I-002 S3.2): allocation entry-point calls that returned NULL for a genuine reason — a backing acquisition, span acquisition, page-size discovery or lock failure, a release failure at a realloc transfer step, or a pre-admission arithmetic rejection (calloc product overflow, oversize/unrepresentable request). Exactly one increment per failed entry-point call: internal delegation (realloc(NULL, n) -> core alloc) counts once, at the delegating implementation's single counting point. |
| alloc_posture_nulls | EXISTS | The disjoint companion count: entry-point calls that returned NULL purely as an implementation posture — mimir_malloc(0) and calloc with a zero factor (U-09), mimir_realloc(ptr, 0) with the original preserved (U-12). Posture NULLs are never counted in alloc_failures: they are not failures, no backing was needed or attempted, and folding them in would make the failure count depend on unresolved contract cases (the I-002 S3.2 / I-004 FC-6 separation). Why a separate counter rather than silence: Phase 10 exit needs statistics to balance under tests, and a balanced ledger needs the posture calls visible somewhere — they are observable NULL returns, so the snapshot accounts for them explicitly instead of letting them vanish. |
| span_occupancy | EXISTS | At the snapshot instant, per size class (the banked G-002 table of 20 classes, echoed in span_class_size[]): span_live_objects[] (sum of live_count over the class's spans), span_total_spans[], span_empty_spans[] (live_count == 0), span_capacity_objects[] (sum of per-span capacities (page - 96) / class_size). Plus the totals live_small_objects, live_spans, live_mappings, live_mapping_bytes, live_span_bytes. |
| cross_thread_frees | PARTIAL | Count of mimir_free calls that retired a MAPPING-path block whose allocating thread differs from the freeing thread. The owner identity (ISO C11 thrd_t, thrd_current()/thrd_equal() — standard C, no OS facility named in the core layer) is recorded at publication in the mapping's reserve page at offset 96, just past the 96-byte descriptor (INC-J002-01). Small-object frees are NOT counted — see INC-J002-01. The realloc transfer retire is not a free and is never counted. A free whose release then fails at L6 (F-06 rollback) IS counted: the cross-thread free happened; the block merely stays live. |
| lock_contention | ABSENT | No counter exists. The snapshot returns lock_contention == 0 with lock_contention_measured == 0 in every snapshot — an explicit ABSENT marker, not a measured zero. See INC-J002-02. |

Quantities deliberately NOT in the snapshot:
Q-LIVEREQ (live requested bytes) is not provided: its
small-object half remains underivable at this base
(INC-I002-02 — the span header retains the class size,
not each object's request). bytes_requested is the
cumulative publication total defined above, not a
rename of Q-LIVEREQ. Recorded as INC-J002-05.

## 3. Snapshot semantics

mimir_core_stats_snapshot takes the one global lock
once, copies the scalar counters, walks the span
registry once (cached bytes + per-class occupancy), and
releases the lock before returning. Every field of one
returned struct therefore describes the same instant;
no field is read from a second lock acquisition (the
I-003 queries each take their own lock — the cross-reads
in the proof test agree only because its reading points
are quiescent). Page size is discovered through L6
BEFORE the lock is taken (it needs no lock) and reported
as page_size_bytes (0 only if discovery itself failed).
On a NULL out or a lock failure the function returns -1
with *out untouched (the G-007 getter posture carried to
a struct). The snapshot allocates nothing, so reading
statistics can never recurse into the allocator
(roadmap Phase 10 exit concern, L7 form).

## 4. Reconciliation identities handed to J-003

At any quiescent point (single thread, nothing in
flight), with E-005 / G-003 counters read at the same
point:

1. bytes_mapped == live_mapping_bytes + live_span_bytes
   == mimir_core_mapped_bytes() (I-003 Q-MAPPED).
2. bytes_cached == mimir_core_retained_bytes()
   (I-003 Q-RETAINED), and bytes_cached <=
   live_span_bytes.
3. peak_mapped >= bytes_mapped; equality iff the peak
   is now.
4. Released/mapped conservation: bytes_released +
   bytes_mapped == total bytes ever published (sum of
   mapping-path mapping_lengths + span pages) MINUS any
   failed-publication cleanup releases (uncounted, S2).
   In a run with no failed publication after acquisition
   the identity is exact as stated; the proof test's
   final state (nothing mapped) has bytes_released ==
   every published byte, checked against the E-005 /
   G-003 publication counters times their byte sizes.
5. Call accounting: for malloc + calloc + realloc entry
   calls, every call either succeeds, or increments
   exactly one of alloc_posture_nulls (U-09 / U-12
   postures) and alloc_failures (genuine). The single
   exception is realloc of an invalid pointer (undefined
   behaviour, MA-6.1-REALLOC-05): it returns NULL and
   increments NEITHER — no counter may be moved on
   suspicion (the S8.3 posture the whole codebase takes
   for undefined calls). INC-J002-04.
6. Occupancy: sum of span_live_objects[] ==
   live_small_objects == mimir_audit_small_alloc -
   mimir_audit_small_free (G-003); sum of
   span_total_spans[] == live_spans ==
   mimir_audit_span_published - mimir_audit_span_retired;
   live_mappings == mimir_audit_alloc_published -
   mimir_audit_mapping_retired (E-005).
7. bytes_allocated - bytes_requested == total rounding
   slack granted at publications (class slack on the
   small path, page slack on the large path); both
   counters move together at exactly the same points.
8. cross_thread_frees <= total mapping-path frees;
   small-object frees contribute 0 until a per-object
   owner record exists (INC-J002-01).

Under concurrency the equalities in (1), (2) and (6)
hold INSIDE one snapshot by construction (one lock, one
walk); cross-snapshot equalities against the separately
locked I-003 / audit reads are quiescent-point claims
only. The proof test asserts the in-snapshot invariants
on 100 snapshots taken during a churn workload.

## 5. Evidence (all run on the Pi in this slice)

- make test-j002: 550 checks / 0 failures, RESULT: PASS
  (tests/core/recorded_run_j002.txt). Legs: zero
  baseline; stated small+large workloads with exact
  predicted counter movements (definitions written
  before the run); calloc + staying/grow realloc;
  failure leg through the reused I-004 --wrap shim
  (injected acquire, page-size and lock failures each
  count exactly one genuine failure; posture NULLs
  count separately; calloc overflow and oversize count
  as genuine with no mapping attempted); cross-thread
  legs (+1 worker-frees-main's large block, +0
  same-thread, +0 small cross-thread per the PARTIAL
  definition, +1 main-frees-worker's large block);
  per-class occupancy; 100-snapshot invariant sweep
  under churn; final identities of Section 4.
- make test: PASS, 4590 checks / 0 failures (unchanged
  composition).
- make test-i003: 72/0. test-i004: 248/0. test-i005:
  PASS. test-i006: 120/0. test-i007: 63/0. test-i008:
  PASS. (The Phase 9 proofs are unregressed by the core
  change.)
- nm -D --defined-only on build/libmimiralloc.so:
  exactly 4 (mimir_malloc, mimir_free, mimir_calloc,
  mimir_realloc); no statistics symbol is exported.
- README.md and include/mimir_alloc.h byte-identical
  to base 0dd6f7a (sha256 verified at commit time).

A defect was found and fixed inside this slice: the
first implementation recorded the owner thread in the
descriptor's reserved[24], which broke the B-007 C1.4
clean-init audit (test_b007: 7 failures — flags and
reserved are zero-on-publication, B-006 S7.3). The
record moved to the reserve page at offset 96, past the
descriptor (INC-J002-01); test_b007 and the whole suite
are green with the record there, and the C1.4 audit now
also stands as the regression proof that the record
never touches the descriptor.

## 6. Inconsistencies and unknowns (INC-J002-NN)

- INC-J002-01 (cross_thread_frees is PARTIAL): thread
  identity was not tracked anywhere at base. The
  mapping path gained an exact owner record (thrd_t in
  the reserve page at offset 96 — NOT in the descriptor:
  its flags/reserved are audited zero, B-007 C1.4;
  first implementation placed it there and test_b007
  caught it, S5). The span path has no per-object
  storage to hold an owner: a span serves objects of
  one class for many threads, objects carry no metadata
  while live, and the 96-byte span header is fully
  assigned (G-002 S7). Counting small cross-thread
  frees truthfully requires a per-object owner record
  (a layout change) or a side table (unbounded state
  the allocator would itself have to allocate); neither
  is this slice's to invent silently. Small cross-thread
  frees therefore contribute 0 and the counter is
  graded PARTIAL.
- INC-J002-02 (lock_contention is ABSENT): the one
  global lock is an L6 pthread mutex consumed only as
  lock/unlock; it exposes no contention signal. A count
  would require trylock probing or a second atomic on
  the hot path of every entry point — a behaviour and
  timing change to the allocator to measure a quantity
  the roadmap marks optional, and any number so derived
  (e.g. failed trylocks) would measure the probe as
  much as the contention. No truthful cheap measure
  exists under the current lock design, so the field is
  the explicit ABSENT pair 0 / measured=0, never a
  fabricated figure. Revisit if L6 ever exposes a real
  contention signal.
- INC-J002-03 (test header includes the internal
  header): earlier test-only headers re-declare the
  entry points they use; a struct cannot be re-declared
  that way without a second, drifting layout copy.
  tests/core/mimir_stats_j002.h therefore includes
  src/core/mimir_core.h by relative path, keeping one
  layout definition in the tree. The surface stays
  shape (b): the header is under tests/, nothing is
  installed, and the .so exports nothing new.
- INC-J002-04 (definition choices awaiting J-001
  reconciliation): (a) bytes_requested / bytes_allocated
  are CUMULATIVE publication totals, not live sums;
  (b) alloc_failures adopts the I-002 Q-FAILED-OS scope
  verbatim, including pre-admission arithmetic
  rejections (calloc overflow, oversize) that I-004's
  FC-6 separates from OS-reached failures — both are
  genuine NULLs, and the shim's attempt counters remain
  the way to tell them apart in a test; (c) posture
  NULLs get their own counter instead of being left
  uncounted (I-002 left them uncounted by both failure
  counters; this slice counts them, separately, for
  ledger balance — no I-002 quantity is redefined);
  (d) realloc of an invalid pointer counts nothing
  (S4 identity 5); (e) a realloc transfer-step release
  failure counts one genuine failure although a new
  block was briefly published (its requested/allocated
  and release accounting still reconcile through the
  rollback retire). If J-001's contract rules
  differently on any of these, the change is a counting
  -point change only; no field needs to move.
- INC-J002-05 (Q-LIVEREQ not provided): live requested
  bytes remain unprovided for the reason INC-I002-02
  records (small-object requests are not retained).
  J-003's identities are stated without it (S4).
- INC-J002-06 (J-001 contract not in base): J-001's
  Phase 10 foundation plan / counter-definition
  contract was built in parallel and is not in base
  0dd6f7a; this slice implemented to the roadmap text
  plus I-002. The banking coordinator owes a
  definition-by-definition reconciliation of S2 against
  J-001's contract; INC-J002-04 lists the exact choices
  to check first.

## 7. Scope kept

Core only (src/core/mimir_core.c, src/core/mimir_core.h),
one test-only header, one proof test, one Makefile
additive block, this doc. No public header, README, API
or platform change; no debug/red-zone/canary work
(that is J-004+ territory in Phase 10 order); no budget
enforcement (INC-I001-14 stands, untouched).
