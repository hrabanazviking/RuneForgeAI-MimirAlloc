# Z13 TLS Cache Contract

Slice: Z-028 - TLS cache: contract before code (Wave 4 opener).
Status: contract written and RED-proven on the banked core,
2026-11-12, team-05 worktree. The implementation slice is
Z-029; this slice changes NO allocator code.
Artifacts: tests/contract/mimir_tls_cache_z028.h (the
surface), tests/contract/test_z028_tls_contract.c (the
suite), tests/contract/run_z028.sh (the runner),
tests/contract/recorded_run_z028.txt (the red evidence).

## 0. Position and the Z-026 honesty note

The roadmap writes this contract "against the Z-026
decision". At the hour this slice was built, the seat main
carried banked Z-001..Z-019; Z-020..Z-027 were not yet
banked, so no Z-026 dossier exists on disk to cite, and
this contract invents none. What the roadmap itself fixes,
and what this contract is written against, is the Wave 4/5
road split stated in the roadmap text: Wave 4 takes the TLS
freelist cache (Z-029: per-thread bins for the hot classes,
lock-free on cache hit, batched refill/drain to the central
core on miss) and Wave 5 takes arenas (Z-037). When the
Z-026 dossier banks, if its decision contradicts any clause
here, the dossier wins and this contract is amended in the
open, in the slice that notices. Every number cited below
comes from a banked recorded run named beside it.

## 1. Why a cache: the evidence the contract must serve

- The wound is serialized lock hold, not lock overhead.
  Z-019 (tests/perf/recorded_run_z13_z019.txt; SHARED-HOST
  run, load 2.50 -> 2.68, medians and drift-bounded shape
  are the claim, absolute cycles carry that label):
  uncontended wait sits at the measurement floor (mean
  ~40 cycles, ~13 ns); hold is small malloc 60 cycles /
  free 86 on small_churn, 143 / 601 on mixed_batch. Under
  contention, mean wait reaches 1597/1688 cycles at 4
  threads (~12x hold, p99 ~5 us) and 4190/4292 cycles at
  8 threads (~1.4 us, ~26x hold, p99 ~10 us). A cache hit
  that never takes the lock removes the hold, and with it
  the wait every queued thread pays for it. That is the
  whole reason this cache exists.
- The standing losses it answers (Z-018 S6, citing Z-003
  and Z-005): small_churn 93.43 vs glibc 9.02 ns/op
  (LOSS 10.4x), mixed_batch 398.42 vs 9.90 (LOSS 40.2x).
  This contract promises NO speed number. Speed is graded
  by Z-031's verdict run against Z-009 and glibc; a
  contract that promised nanoseconds would be unfalsifiable
  here and dishonest there.
- Classes: all 20 banked classes serve requests
  1..1024 bytes (16,32,48,64,80,96,112,128,160,192,224,
  256,320,384,448,512,640,768,896,1024); above 1024 the
  mapping path is out of scope for this cache (Z-018;
  src/core/mimir_core.c class table).
- Alignment (Z-017): both allocators return sub-16-aligned
  pointers 0 times in 163,840 blocks per leg; MimirAlloc
  class 48 splits cachelines 49.41% and class 64 100%
  (starts at ptr mod 64 = 32). The cache hands out span
  blocks unchanged, so it can neither repair nor worsen
  that geometry; clause TLS-ALIGN pins the 16-byte floor.
- False sharing (Z-016): one hot writer per 64-byte line;
  same-line packing costs 2.86x at 2 writers up to ~11x
  at 8-16. Section 5 applies the rule precisely: it binds
  shared-written state, not single-writer thread-local bins.
- Constants (Z-018): cache line 64 B; saturation at 16
  physical cores (threads 17-32 are ~0.2-core SMT helpers,
  Z-012), so per-thread cache bounds are sized against 32
  possible threads, not against hope.

## 2. The surface Z-029 must provide

Declared in tests/contract/mimir_tls_cache_z028.h,
test-only, INC-E001-07 shape (b) exactly like the drain /
trim / stats surfaces: defined in the core object, bound
local in the shared library (nm -D stays exactly 4), never
part of include/mimir_alloc.h, no ABI claimed (U-03).

- int mimir_tls_cache_snapshot(out): the CALLING thread's
  counters and gauges (per-class cached counts, cached
  blocks/bytes, hits, misses, refills, refill_blocks,
  drains, drain_blocks, local_frees, local_frees_to_core,
  remote_frees_out, calloc_served). Snapshot must not
  allocate, must not take the central lock, and must not
  itself move any counter or initialise the cache: counters
  are thread-local and single-writer, so a plain read is
  coherent for the calling thread, which is the only
  consumer. Cross-thread and global figures other than S6's
  exit counters are deliberately NOT in the surface: a sum
  over other threads' bins cannot be made coherent without
  the lock, and no clause needs it.
- size_t mimir_tls_cache_bin_capacity(class_index) and
  mimir_tls_cache_batch(class_index): the S3 constants,
  0 for any out-of-range index.
- void mimir_tls_cache_drain_thread(void): return every
  cached-but-free block of the calling thread to the
  central core, exactly once; live blocks untouched;
  idempotent; NOT counted as an exit drain.
- uint64_t mimir_tls_cache_exit_drains(void) and
  mimir_tls_cache_exit_drain_blocks(void): process-global
  monotone counters of TLS-teardown drains (S6), written
  under the central lock during drain, readable at
  quiescent points.

Ownership vocabulary (fixes a hole the current core names
itself: the J-002 stats note records that no per-object
owner record exists for small blocks, so small cross-
thread frees are not even counted today): a cached block
is owned by exactly one thread's bin; a live block's owner
is the thread that allocated it. Z-029 must add the
per-block owner record that makes the second sentence
checkable (its layout is Z-029's choice; its existence is
this contract's demand). Blocks whose allocating thread
has exited are ownerless: any later free routes them to
the central core and does NOT count as remote (there is no
live owner to be remote from).

## 3. Contract constants (decisions, not measurements)

- CAPACITY: 16 cached blocks per class per thread.
  Ground: the banked mixed_batch workload holds 256 live
  blocks cycling all 20 classes (~13 per class); 16 is
  the smallest power of two holding one such free burst
  per class. A bound chosen for testability, not a claim
  that 16 is optimal; Z-031/Z-032 may bring evidence to
  amend it, by amending this contract in the open.
- BATCH: refill and drain move exactly 8 blocks (half a
  bin) per central-core transaction. Free-to-full-bin
  drains one batch BEFORE the triggering block is cached,
  so the bin never exceeds capacity and the triggering
  block is always cached (the suite's P3 sequence depends
  on exactly this rule).
- Bounds that follow: at most 320 cached blocks and
  16 * 6400 = 102,400 cached free bytes per thread; at 32
  threads at most 3,276,800 bytes process-wide, all inside
  spans whose retention stays governed by the unchanged
  G-007 bound (default 2 empty spans per class, 0..16).
  The cache adds no second retention policy.
- Latitude considered and declined: leaving batch size or
  capacity as implementation freedom would make the
  hit/miss/refill clauses untestable deterministically.
  The contract fixes them; Z-029's freedom is layout,
  owner-record design, and lock mechanics, not the numbers
  the suite counts.

## 4. The clauses, each traced to its test

Suite: tests/contract/test_z028_tls_contract.c, 119 checks
in every world (surface present or absent), phases P0..P10.
No clause is asserted by timing, sleep, or probability.

- TLS-SURFACE (P0, 7): all six surface symbols exist and
  resolve in a test-only static link.
- TLS-CONST (P1, 42): capacity(class)=16 and batch(class)=8
  for all 20 classes; 0 out of range.
- TLS-HITMISS (P2, 9): from an empty bin, an alloc is a
  miss, refills exactly once with exactly one batch, and
  leaves batch-1 blocks cached; a same-thread free enters
  the bin (local_frees); the next alloc is a hit and the
  bin is LIFO (the just-freed block is handed back first).
- TLS-CAP (P3, 7): a bin never exceeds capacity; the free
  past capacity bypasses to the core (local_frees_to_core)
  after a batch drain; a full bin serves 16 hits with no
  refill; cached_bytes is exact class accounting
  (16 blocks of class 1024 = 16,384 bytes).
- TLS-BYPASS (P4, 8): requests above 1024 never touch any
  cache counter or bin; mimir_malloc(0) stays the U-09
  posture null and mimir_free(NULL) the defined no-op, and
  neither moves a cache counter.
- TLS-CALLOC (P5, 5): calloc is routed through the cache
  path (calloc_served) and a calloc served a dirty cached
  block returns it zeroed over the requested extent;
  calloc takes the same LIFO bin head as malloc; realloc
  grow preserves the prefix across the cache path, and
  frees after realloc count as ordinary own frees.
- TLS-DRAIN (P10, 4): drain_thread returns exactly the
  cached blocks (drain_blocks), leaves the bin at 0, is
  idempotent, and is not an exit drain.
- TLS-ISO (P6, 4): a new thread's cache starts empty with
  zeroed counters (no state inherited through a recycled
  thread identity); one thread's churn moves no other
  thread's counters or bins.
- TLS-XFREE (P7, 5): a block allocated by another (even
  already-exited) thread stays live and intact; freeing
  a block whose owner is a different LIVE thread counts
  remote_frees_out, never local_frees, and the block does
  NOT enter the freer's bin - it routes to the central
  core, from where a miss on an empty bin returns it
  (central span LIFO head). The per-owner remote-free
  QUEUE is Z-030's design; Z-029's duty is only that the
  block reaches the core exactly once, uncorrupted, and
  is accounted. Geometries beyond one-producer/one-
  consumer (many-to-one storms) are Z-030/Z-043's.
- TLS-EXIT (P8, 5): at thread exit the cache drains
  exactly once (S6): exit counters move by the drain and
  by exactly the blocks cached at exit; live blocks of
  the exited thread survive intact and stay freeable;
  live-object accounting returns to baseline and
  mimir_core_metadata_check() reports 0.
- TLS-ALIGN + regression (P9, 23): every class returns
  16-byte-aligned pointers (Z-017 floor); simultaneously
  live blocks are pairwise distinct; stats reconcile and
  metadata is clean. P9 uses only the public API and the
  banked stats/integrity surfaces, passes on the old core
  today, and exists so Z-029 cannot trade correctness
  for the cache: it is a regression guard, not evidence
  of a cache.

## 5. False sharing: Z-016 applied, not pasted

Z-016's rule binds fields WRITTEN BY MORE THAN ONE THREAD.
Per-thread bins are single-writer (owner thread only, and
on the Z-029 road the freer of a remote block writes only
the central core under its lock, never the owner's bin),
so bin heads need no padding: padding them would spend
cache lines to protect against a writer that cannot exist.
The two global exit counters ARE shared-written and are
therefore written only under the central lock, in the same
critical sections that already move span state. If Z-030
adds any per-owner queue another thread writes, that
queue's head/tail fields fall under Z-016 in full (own
64-byte lines) and Z-040 owns the audit.

## 6. Thread-exit drain (with the drain review)

On thread exit, a TLS destructor (or pthread-key
destructor) drains all cached-but-free blocks to the
central core exactly once per block; after it, the
thread's bins are empty and are never dereferenced again.
Drain is idempotent across destructor re-runs and later
explicit drain calls. It allocates nothing, runs no user
code, and takes no lock other than the central lock in
established order. Live blocks are never reclaimed,
moved, or corrupted by it, and no live block's metadata
may live solely in the exiting thread's TLS. Per-thread
retention after exit is 0 bytes, unconditionally; global
retention after any number of create/exit cycles stays
bounded by the G-007 empty-span bound alone, and repeated
cycles reach a steady state (Z-042 owns the 10,000-thread
storm built on this clause). Main-thread exit drains the
main cache before the unload drain releases retained
spans. If the process dies without destructors (_exit,
abort, fatal signal), no drain is promised; process
teardown reclaims everything.

## 7. What is NOT promised

- No speed, latency, or throughput figure (S1).
- No fork posture: fork from a multithreaded allocating
  process is outside this contract (the public header
  claims none; Z-044 owns fork proofs). The only supported
  posture stays single-threaded fork immediately followed
  by exec, without allocating in between.
- No async-signal safety (unchanged, MA-6.3 posture).
- No realloc pointer identity, no in-place growth promise
  (B-008 S4.4 posture unchanged).
- No errno channel (U-10), no settlement of U-09/U-12;
  the cache must not move any posture case.
- dlclose with threads still allocating through the
  library is not promised; with all such threads joined
  first, S6's ordering (TLS drains, then unload drain)
  is the promise.
- Z-030's remote-free queue, Z-037's arenas, and any
  change to the 1024-byte cutoff are outside this slice.

## 8. Green-by-design review (the roadmap gate's second half)

Reviewed by the team lead with the six Mythic Engineering
review roles (ownership/API, cross-thread routing,
thread-exit drain, evidence audit, gate planning, risk)
before the recorded run was taken. Findings:

- Predetermined totals: the suite prints CHECKS: 119 run
  in BOTH worlds. Surface absent: 96 failed (P0's 7 symbol
  checks individually, plus every planned check of the
  nine surface phases, recorded by fail_planned at full
  count - never collapsed to a single failure), 23 passed
  (P9 regression only), RESULT: FAIL, exit 1. Surface
  present and correct: 119 run, 0 failed, RESULT: PASS,
  exit 0. Any other total on either core is itself a
  failure of determinism.
- Absence proven from symbols, not from a missing file:
  the recorded run carries nm output showing the six
  surface symbols as weak (w) in the suite binary and
  absent from every library object.
- RED mechanism, chosen over a plain link failure: the
  suite references the surface through weak symbols, so
  on the old core it links, runs, and fails per clause,
  and its public-API phases execute for real. A link-red
  suite could not show which clauses fail, could not run
  P9 at all, and would let a future build break masquerade
  as contract-red. The header itself declares the surface
  strongly, so a Z-029 signature drift is a compile error
  in the suite, never a silent weak miss.
- No vacuous green: no check passes on the old core except
  P9's, and P9 is labelled regression, in the suite header
  and in S4, as proving nothing about a cache. Every
  surface phase is guarded by surface_present() and fails
  at full planned count when it is absent.
- No timing anywhere: the suite contains no clock, sleep,
  spin, or retry; thread phases join; the three recorded
  repetitions print identical counts. Assertions about
  exit drains use the suite's quiesced geometry (counters
  read after join, one exiting thread at a time), which
  is the only geometry in which an exact delta is a
  contract property at all.
- Traceability: S4 maps every clause to a phase and a
  check count; every check label in the suite carries its
  phase and clause name. Nothing is asserted in this doc
  that no check or S9 duty backs.
- The one exact-batch assertion (P2) is a contract
  decision (S3), not a smuggled measurement; S3 records
  the latitude that was considered and declined, and the
  amendment road.

## 9. Duties recorded for Z-029 (contract-level, untested here)

These bind Z-029's design review; several are not
runtime-testable deterministically and are therefore
reviewed there, not asserted here:

- TLS-ACCT: a block held in a bin is FREE at core level:
  free-to-bin decrements the span's live accounting at
  the moment of the free, refill moves blocks off the
  span free list, and drain moves them back. Stats
  (live_small_objects, per-class live counts) stay exact
  at every quiescent point with no cache-specific fudge;
  P8/P9 reconciliation is written against this semantics.
- Cache storage is never allocated through mimir_malloc
  itself: bins live in static _Thread_local storage (or
  platform allocation outside the allocator), first-touch
  initialisation is recursion-safe, and a hit after
  initialisation allocates nothing.
- The hit path takes no lock and touches no shared-written
  state (S5); refill/drain take the central lock once per
  batch, never per block.
- Debug builds: a cached-but-free block must stay
  consistent with the J-004/J-005 debug machinery (span
  walk, poison/quarantine posture) - Z-029 records how;
  a green production contract with a broken DEBUG=1 gate
  is not done.
- Destructor ordering: the exit drain runs before the
  unload drain and tolerates a never-initialised cache.
  Fork, signal, and dlclose-with-live-threads stay
  unpromised per S7; Z-029 must not silently widen them.
- Risk-review addendum (sixth review role, folded before
  commit): per-thread cache state lives in true
  thread-local storage, never in a tid-indexed shared
  array (a recycled tid must not alias a dead thread's
  bin; Z-016 also forbids the packed array outright).
  Any destructor/key registration happens through a
  pthread_once-style path that never runs inside the
  central lock and never calls a public mimir_* entry;
  the core lock is non-recursive, so refill/drain must
  use the core's internal locked helpers and never
  re-enter a public entry point while holding it. In
  debug builds the cached path must take the same
  poison/red-zone state transitions as a core free, or
  be disabled under MIMIR_DEBUG with that recorded in
  the Z-029 slice doc - a cached free that skips the
  poison is a J-005 regression, not an optimisation.
  A batch moved between a bin and the core is published
  by one atomic head exchange or under the central lock,
  never by a multi-store splice another thread can
  observe half-done. A deliberate release valve was
  considered - shipping Z-029 malloc/free-only with
  calloc/realloc bypassing - and declined for the
  contract as written: TLS-CALLOC stands, and any future
  bypass must amend S4 in the open, not reinterpret it.
