# Z13 TLS Cache Implementation

Slice: Z-029 - TLS cache: implementation (Wave 4).
Status: implemented on team-06 worktree, 2026-11-12.
Implements the contract of Z-028
(docs/Z13_TLS_CACHE_CONTRACT.md +
tests/contract/mimir_tls_cache_z028.h) over the banked
core. The Z-028 suite is GREEN unmodified on this core
and the standing gate (Z-008) is GREEN at the founding
counts; full evidence in
tests/contract/recorded_run_z029.txt.

Roadmap gate: Z-028 contract green; standing gate (Z-008)
green. Both halves are met (recorded run).

## 1. What changed

- src/core/mimir_core.c: the whole implementation (one
  Z-029 block plus small, named touch-points in the
  pre-existing locked machinery and entry points).
- src/core/mimir_core.h: the six-function surface
  declared (INC-E001-07 shape (b), exactly like the
  drain / trim / stats surfaces), with the snapshot
  struct forward-declared from the contract header so a
  signature drift is a compile error in the defining
  translation unit, which includes both headers.
- docs/Z13_TLS_CACHE_IMPLEMENTATION.md: this doc.
- tests/contract/recorded_run_z029.txt: the evidence.

No public header change (include/mimir_alloc.h still
declares exactly four entry points), no Makefile change,
no README touched. The shared library's dynamic symbols
stay exactly 4 (recorded run, nm proof): the TLS surface
is defined in the core object and bound local by the
E-002 version script, like every internal surface.

Dependency of record: this slice compiles against the
Z-028 contract header under tests/contract/; at banking
time Z-028 banks first (roadmap order), so the header is
at the seat before this slice merges. The Z-029 commit
deliberately does NOT carry copies of the Z-028 files.

## 2. Design

Bins. Static _Thread_local storage (struct tls_state,
cache-line aligned): bins[20][16] of block pointers, one
stack per banked class, plus the thread's counters and
its pending-accounting list. No allocation ever serves
the cache itself; first touch is zero-initialised static
TLS plus a call_once (C11 call_once / tss_create)
registration for the exit drain - never under the
central lock, never through a public entry point, so
initialisation is recursion-safe (contract S9).

Hit path. Pop the bin top; count the hit; record the
accounting transition as pending (S3 below). No lock is
taken and no shared-written state is touched besides the
pre-existing E-005 audit counter, which the banked core
already updates with the same relaxed atomic on every
small publication (and the L1 boundary does likewise,
lock-free, for every public call). Refill and drain take
the central lock once per batch and call only this
file's internal locked helpers - never a public entry
(the core lock is non-recursive).

Refill (miss, bin empty). Under one lock: take exactly
one batch (8) of FREE blocks of the class from the
central core - pop the serving span's free-list head or
carve, mirroring small_alloc_locked's span choice - bump
each taken block's binned tally and write its owner
record, then push the batch in reverse take order so the
first block taken (the span free-list head) is the bin
top: the handout observes central LIFO order (the Z-028
P7 identity depends on this). The handed-out block is
accounted LIVE under the same lock.

Free to bin. The owner record (S4) routes the free:
own block -> push the bin; the full-bin rule is contract
S3 verbatim - a free to a full bin first drains the
bottom batch (8, the oldest blocks) to the central core,
then caches the triggering block, so a bin never exceeds
16 and the triggering block is always cached.

calloc / realloc. A small calloc runs the same cache
path as malloc (hit or refill), zeroes the whole class
extent of the block it hands out (the banked core's
calloc extent), and counts calloc_served. realloc's
stay case is untouched; its move construction takes the
new block from the caller's bin when the new size is
small (refilling once if empty, falling back to the
locked dispatcher), and retires the old block into the
caller's bin when the owner record names the caller -
with the same S3 batch-drain rule - otherwise through
the banked small_free_locked. The realloc transfer
counts the E-005 audit pair exactly as the banked path
did; it does not move hits / misses / local_frees
(those count malloc / calloc / free events).

## 3. Accounting (contract S9 TLS-ACCT)

A block held in a bin is FREE at core level. Free-to-bin
and hits therefore move the core counters (span
live_count, mimir_live_small_objects, the J-002
bytes_requested / bytes_allocated pair) - but they do
so through a per-thread PENDING list of (span, delta)
transitions, flushed under the central lock at the
start of every locked core entry in the file: the
allocating entries, free's locked paths, the stats
snapshot, the metadata check, trim / drain / retained /
mapped queries, the tuning pair, drain_thread, the
realloc entry, and the thread-exit drain. What a thread
can observe through any locked surface is therefore
exact for its own transitions at every call, and exact
for every exited thread (the exit drain flushes first).

A locked per-span BINNED TALLY counts blocks held off
the span free lists. Refill increments it, drain and
batch-drain decrement it, and a flush converts pending
transitions between tally and live_count (a hit: tally
-1, live +1; a free-to-bin: live -1, tally +1). Two
invariants follow at every instant, for every span:

- free-list nodes + live_count + tally == carve_count
  (the metadata check now verifies exactly this sum);
- a span is releasable only when live_count == 0 AND
  tally == 0 (the release tests in
  empty_spans_of_class_locked and the trim walk carry
  the tally condition), so no span is ever released
  from under a bin or an unflushed transition: before
  its flush, a freed-to-bin block is still counted live
  and a hit block is still counted binned - the locked
  books always cover it one way or the other.

Deliberately unchanged views: the retained-span and
stats empty-span counts keep their banked definition
(live_count == 0), so pre-cache tests read the same
figures they always did after a free; what changed is
only that RELEASE additionally requires tally == 0.
mimir_core_drain_retained and the trim entry points
first return the CALLING thread's cached blocks to the
central core, so a drain or trim is never defeated by
the caller's own bins.

One honest limit: a stats snapshot or metadata check
taken by one thread while ANOTHER thread is mid-churn
reads that thread's transitions only up to its last
flush. The locked books stay internally consistent at
every instant (the invariants above are not
quiescence-relative); only the attribution of the other
thread's in-flight transitions lags, and it settles at
that thread's next locked entry or at its exit. The
contract's own reading discipline is quiescent points
(S2 / S4), which is exactly the geometry in which this
is exact.

## 4. Ownership (contract S2)

A fixed open-addressing table (65,536 slots, static
storage) maps each live or binned small block to its
allocating thread (thrd_t bits) and its span. Updates
use only __atomic builtins; no lock is taken to read or
write a record. Records are written at refill (for the
whole batch) and at locked small publications (the
realloc fallback construction); cleared when a block
returns to the central core through a remote free.
A block's record is stable from publication until its
free - the allocator writes it before the block can be
handed to another thread - so the free path can read it
lock-free.

Free routing. Owner == caller: local free (bin). Owner
!= caller: the block returns to the central core under
the lock (small_free_locked), the record is cleared,
and the freer's remote_frees_out counts it. Owner
record absent (or a record the span registry disagrees
with): the legacy two-registry path decides, and a
small block freed that way counts remote_frees_out as
fallback accounting (S7) - it reached the core, and it
was not a bin free.

The S2 / suite tension, recorded in the open. Contract
doc S2 says a block whose allocating thread has exited
is ownerless and a later free does NOT count as remote.
The Z-028 suite's P7, however, allocates in a worker,
joins it (the thread has exited), frees from main, and
requires remote_frees_out + 1. The suite is the
gate-named form of the contract ("Z-028 contract
green"), so this implementation follows the SUITE: any
recorded owner other than the freer counts remote,
including an exited owner; the routing half of S2 (the
block reaches the central core exactly once) is kept
verbatim. If the contract doc is amended later, the
change here is one comparison in mimir_core_free.

## 5. Thread-exit drain (contract S6)

The tss destructor flushes the exiting thread's pending
accounting, returns every binned block to the central
core exactly once, and moves the process-global exit
counters under the central lock. An exit_drained flag
is set before the drain, so destructor re-invocation
(the tss value stays non-NULL) and the unload hook can
never double-drain or double-count. Live blocks are
never touched, no user code runs, and the only lock
taken is the central one. The main thread is covered
by the banked image destructor, extended for this
slice: it drains the calling thread's cache (counted
as an exit drain) BEFORE mimir_core_drain_retained
releases retained spans - the S6 ordering. Fork,
signal and dlclose-with-live-threads postures are
unchanged (contract S7: unpromised, not widened).

## 6. Counter semantics fixed by this slice

Where the contract header names a counter without
pinning its exact event, this slice fixes the reading
and records it here:

- local_frees_to_core counts free EVENTS that found a
  full bin (each such event batch-drains 8 blocks to
  the core, which drains / drain_blocks also record).
  Z-028 P3 requires only >= 1 for its sequence; the
  event reading matches the header's wording ("own
  frees bypassing a full bin") closest while S3's
  drain-before-caching rule is implemented literally.
- drains / drain_blocks count only drains that return
  at least one block; an empty drain_thread is a true
  no-op (P10's idempotence reading).
- exit_drains counts a thread once, when its cache is
  drained at exit, even if it held 0 blocks at exit; a
  thread that never touched the allocator never
  registers and never counts.
- A refill that moves k > 0 blocks counts one refill
  and k refill_blocks; a refill that can move nothing
  is an allocation failure (the J-002 failure note and
  the J-007 report fire exactly as on the banked core)
  and moves no cache counter except through that path.

## 7. Limits and fallbacks (honest bounds)

- Owner table: 65,536 slots, 256 probes per record. A
  block whose record cannot be placed (probe
  exhaustion - past 65k live + binned small blocks
  with adversarial hashing) is still allocated and
  freed correctly; only its free's local / remote
  attribution degrades to the S4 fallback. No suite or
  gate geometry comes near this.
- Binned tally: 4,096 span slots. Beyond 4,096 spans
  simultaneously holding binned blocks, an increment
  can be lost and a span could look releasable early;
  bins hold at most 320 blocks per thread, so this
  needs a dozen threads simultaneously pinning
  thousands of distinct spans each - recorded as the
  design's outer bound, not a suite-reachable state.
- Pending list: 64 distinct spans per thread between
  flushes; on overflow the hit / free accounts directly
  under the lock (a cold path that keeps the books
  exact at the price of one lock on that operation).
- False sharing (contract S5): bins are single-writer
  thread-local state and carry no padding, per S5's own
  ruling; the two shared-written exit counters are
  written only under the central lock inside the drain
  critical sections, as S5 prescribes. The owner table
  is written on refill / free paths only, never on the
  hit path's read of the caller's own record.
- MIMIR_DEBUG: the cache is bypassed entirely (the S9
  road recorded there): small operations take the
  banked locked core paths, so J-004 red zones and the
  J-005 poison / quarantine transitions are the banked
  ones unchanged; the surface links with exact
  constants and zero counters. Verified: the DEBUG
  flavour's test-debug exit pattern on this core is
  identical to the banked core's (the same two
  pre-existing exits, test_b007 / test_c006, that the
  unmodified tree shows - the debug flavour is partial
  in this tree by the J-006 record; no new failure).
- MIMIR_LOCK_HIST builds still compile (checked); the
  hit path takes no lock and so records no sample,
  which is the honest statement about a lock-free hit.
