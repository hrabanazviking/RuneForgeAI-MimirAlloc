# Z-076 - Public statistics, coverage completed (Wave 9)

Roadmap: docs/MIMIRALLOC_Z13_ROADMAP.md Z-076. Gate: stats
API tests green; per-counter disposition table (S3 below).
Base: seat main 480459e (Z-001..Z-074 banked), merged into
the team-04 worktree as 6f70c57 (conflicts in Makefile,
DECISION_LEDGER, mimir_core.c/.h were all the anticipated
append unions; the seat side already carries the banked
Z-065 in composed form, so the merged tree is byte-equal
to the seat tree before this slice's edits).

Team: team-04 lead + 6 Mythic Engineering sub-agents
(public-API/ABI, small-free courses, contention pricing,
request-size pricing, test design, gate discipline),
spawned cleanly, no 429.

Machine: purpleraider (ASUS ROG Flow Z13, Strix Halo,
x86-64, 4096 B pages). Load at build/test time: loadavg
about 0.3, no cross-campaign benchmark running; the
pricing microbench (S4) ran in that quiet window and its
numbers are this-machine measurements, not baselines for
any other host.

## 1. What was built

The J-002 internal snapshot is promoted to a public,
versioned surface:

- include/mimir_alloc.h declares struct mimir_stats_v1
  (leading struct_version / struct_size pair,
  MIMIR_STATS_VERSION 1) and
  int mimir_stats_snapshot(struct mimir_stats_v1 *out).
  Versioning rule: future versions APPEND fields and bump
  the version; this v1 entry point always writes exactly
  the v1 layout, so a v1 caller is never overrun.
- src/api/mimir_alloc.c forwards: it takes the one locked
  internal snapshot and copies it field by field into the
  public struct (never a cross-boundary memcpy, so the
  public layout is an independent record). An E-005-shape
  audit counter (mimir_audit_call_stats_snapshot) counts
  calls at the L1 boundary; bound local in the .so.
- src/api/mimir_alloc.map exports mimir_stats_snapshot:
  the .so dynamic export list is exactly 7 on this base
  (6 through Z-074 + this one). BANKING NOTE: Z-075
  (usable size) banks before this slice and adds its own
  export; after both banks the list is 8, composed in
  slice order. The map's prose header count is composed
  at banking the same way.
- Two L-04 gaps are CLOSED in the core/platform (S2) and
  one is PRICED (S4). struct mimir_stats gains its Z-076
  appends at the END, in slice order after Z-070's
  poison_detections (the Z-064 append discipline):
  cross_thread_small_frees, small_frees_unrecorded,
  live_requested_bytes, live_requested_measured.
- docs/DECISION_LEDGER.md D-012 records the naming and
  versioning decision and the three dispositions.
- LIMITATIONS L-04 narrows accordingly: telemetry is now
  public; the register's L-04 text is composed at banking
  with the keeper (this slice does not rewrite the
  register, whose base predates Wave 9 entirely).

## 2. Gap closures

INC-J002-01 (small-path cross-thread frees) - CLOSED for
recorded blocks. Since J-002 was written, Z-029 gave the
small path what J-002 lacked: a per-object owner record
(the TLS owner table, block -> allocating thread + span).
mimir_core_free already routes a small free by that
record; Z-076 counts at the two recorded cross-thread
courses (non-shard and MIMIR_LOCK_SHARD forms): owner !=
freeing thread and the block proven a live span object,
under the lock, exactly once -> cross_thread_small_frees
moves, and the snapshot's cross_thread_frees is now the
sum over both paths. Residual, honestly bounded: a small
free the central core serves with NO owner record used
to classify it (TLS table probe exhaustion; every small
free in a MIMIR_DEBUG build, which bypasses the cache)
moves small_frees_unrecorded instead - never folded into
the cross-thread count. The true small cross-thread
total lies in [cross_thread_small_frees,
cross_thread_small_frees + small_frees_unrecorded].
Realloc's small-move retire is NOT counted (the banked
precedent: a realloc transfer retire is not a free).

INC-J002-02 (lock contention) - CLOSED.
mimir_platform_global_lock now probes with
pthread_mutex_trylock first: success is the uncontended
road; EBUSY is the one truthful contention event this
layer can observe, so it increments an L6 tally (single
relaxed atomic add, contended path only; the Z-039 shard
acquisition counters are the in-layer precedent) and then
takes the lock by the ordinary blocking call. The return
contract (OK / ERR_LOCK) is unchanged in every case: a
non-EBUSY trylock error falls through to the same
ERR_LOCK outcome the old code produced for a lock error.
lock_contention therefore counts ACQUISITIONS THAT FOUND
THE LOCK ALREADY HELD - never a duration, never an
estimate - and lock_contention_measured is 1. The probe
composes with the MIMIR_LOCK_HIST wrapper (which calls
the real L6 entry) and with MIMIR_LOCK_SHARD (whose
global acquisition calls the real L6 entry after the
shards; shard-lock contention itself is not this
counter). Measured probe price on this host (S4): the
trylock-first pair costs about +2 to +3 ns over the plain
lock/unlock pair per uncontended acquisition.

## 3. Per-counter disposition table (the gate artifact)

| Counter (public v1 field) | Disposition | Exact meaning / note |
|---|---|---|
| bytes_requested | PROMOTED | J-002 definition unchanged (cumulative requested bytes at successful new publications). |
| bytes_allocated | PROMOTED | J-002 definition unchanged (cumulative usable bytes granted at the same publications). |
| bytes_cached | PROMOTED | Q-RETAINED at the snapshot instant (registry walk under the lock). |
| bytes_mapped | PROMOTED | Q-MAPPED at the snapshot instant (live mapping + pooled + live span bytes, Z-048 accounting). |
| bytes_released | PROMOTED | Q-RELEASED cumulative (bytes actually returned to the OS, Z-048 accounting). |
| peak_mapped | PROMOTED | Q-PEAKMAPPED, sampled at the two publication points. |
| alloc_failures | PROMOTED | Q-FAILED-OS; posture NULLs disjoint, below. |
| alloc_posture_nulls | PROMOTED | U-09 / U-12 posture NULLs, counted separately since J-002. |
| cross_thread_frees | CLOSED + PROMOTED | Sum over BOTH paths since Z-076 (was mapping-only PARTIAL). See S2 for the recorded-block scope and the unrecorded bracket. |
| cross_thread_small_frees | NEW (Z-076 append) | The small-path half of cross_thread_frees: recorded-owner cross-thread small frees, exactly once each. |
| small_frees_unrecorded | NEW (Z-076 append) | Small frees served centrally with no owner record used to classify them; thread relation UNKNOWN; bounds the cross-thread residual. |
| lock_contention | CLOSED + PROMOTED | Central-lock acquisitions that found the lock already held (L6 trylock probe). Count of events, never a duration. |
| lock_contention_measured | CLOSED + PROMOTED | 1 in every snapshot since Z-076 (was the ABSENT marker 0). |
| live_mappings / live_spans / live_small_objects / live_mapping_bytes / live_span_bytes | PROMOTED | Occupancy at the snapshot instant, J-002 definitions. |
| page_size_bytes | PROMOTED | Host page size discovered through L6 (4096 on this host). |
| span_class_size / span_live_objects / span_total_spans / span_empty_spans / span_capacity_objects [20] | PROMOTED | Per-class occupancy at the snapshot instant, J-002 definitions. |
| unlock_failures | PROMOTED | Z-064 (D-01) central unlock failure tally. |
| drain_lock_failures / trim_lock_failures / free_lock_failures | PROMOTED | Z-065 (D-02) silent-course lock-failure tallies. |
| metadata_detections / metadata_collateral_refusals | PROMOTED | Z-066 (D-12) consistent-length detection tallies. |
| live_release_detections / live_release_collateral_refusals | PROMOTED | Z-067 (D-12) live_count release-path tallies. |
| double_free_detections / poison_detections | PROMOTED | Z-070 detection tallies. |
| live_requested_bytes | PRICED (INC-J002-05) | Q-LIVEREQ. Always 0 with live_requested_measured 0: no truthful value exists while small objects carry no per-object requested-size record (INC-G003-03). Closure costs measured in S4. |
| live_requested_measured | PRICED (INC-J002-05) | 0 - the explicit not-measured marker, carried in the struct so the gap is visible in the API, not silent. |

## 4. INC-J002-05 priced (measured on purpleraider)

Prototype microbench (hidden_files working copy;
gcc -O2 -pthread, quiet window, two runs, ranges given):

- lock/unlock pair 2.37 / 4.25 ns per op; trylock-first
  pair 5.59 / 6.47 ns (the INC-J002-02 probe price above:
  about +2 to +3 ns per uncontended acquisition);
  contended trylock 2.06 / 2.49 ns per attempt.
- Owner-table full walk (65536 entries, atomic loads):
  23.2 / 22.3 us per walk. A snapshot-derived
  live-requested sum over the owner table would hold the
  central lock for at least this long per snapshot and
  still cover only recorded blocks.
- Owner lookup (hash + probe + atomic load): 0.60 / 0.49
  ns per op - the per-op lookup is NOT the blocker; the
  blockers are coverage and exactness, below.

The three closure roads, priced:

(a) Inline per-object header (8 B requested-size word on
    every small object): breaks the G-002 layout (objects
    are exactly class-sized, user pointer at the object
    start) and costs, per class of the banked table:
    16 B +50.0%, 32 +25.0%, 48 +16.7%, 64 +12.5%,
    80 +10.0%, 96 +8.3%, 112 +7.1%, 128 +6.25%,
    160 +5.0%, 192 +4.2%, 224 +3.6%, 256 +3.1%,
    320 +2.5%, 384 +2.1%, 448 +1.8%, 512 +1.6%,
    640 +1.25%, 768 +1.0%, 896 +0.9%, 1024 +0.8% -
    memory inflation on every live small object, worst
    exactly where the allocator is most used.
(b) Owner-table record (the table's 64 B entries carry
    40 B of Z-040 pad, so a size word fits at 0 B growth):
    coverage-capped - the table holds at most 65536
    blocks with a 256-entry probe, while live small
    objects are unbounded (one 4096 B span page alone
    holds (4096-96)/16 = 250 objects of the 16 B class,
    and spans are unbounded in count); a MIMIR_DEBUG
    build bypasses the table entirely. A running counter
    fed by this road drifts by exactly the unrecorded
    frees - a wrong number, worse than an honest marker.
(c) Snapshot walk derivation: (a)'s or (b)'s record plus
    at least the measured 22-23 us of lock-held walk per
    snapshot over the table alone - and still inexact
    for the same coverage reason.

Verdict: NOT ADOPTED at any of these prices. The gap is
re-documented as priced: the struct carries the explicit
0 / measured-0 pair, and this section is the price tag.
Re-open when a per-object record exists for another
reason (a layout change owned by its own slice), which
collapses road (b)'s marginal cost to the lookup above.

## 5. Tests

make test-z076 (standalone; NOT part of make test):
- test_z076 (static, LIBOBJS route): 181 checks / 0
  failed. Legs: NULL posture; version pair; public ==
  internal agreement on every mirrored field at a
  quiescent point; L1 audit counter (+1 per call);
  same-thread small +0; cross-thread small +1 exactly
  (total and small half; unrecorded +0); cross-thread
  large +1 total / +0 small; same-thread large +0;
  deterministic contention (main holds the L6 lock, a
  worker's acquisition counts exactly +1, cross-read
  against the L6 tally); churn contention > 0 through
  the public surface; residual invariants (small half <=
  total; lock-free getters agree); exact small byte
  accounting (100 requested -> 112 allocated, class 112).
  Repeated 5x: PASS every run.
- test_z076_dyn (.so only): 22 / 0. test_z076_cpp (.so
  only): 8 / 0. Header-only C and C++ TUs compile.
- tests/abi/run_z076.sh files the ABI recorded run
  (exports exactly 7, absence proofs, DT_NEEDED,
  undefined-symbol and header-only proofs, consumers,
  neighbours): tests/abi/recorded_run_z13_z076.txt.
- Variant compiles of the patched core: MIMIR_LOCK_SHARD,
  MIMIR_DEBUG, MIMIR_ARENA_CCD, MIMIR_LOCK_HIST,
  MIMIR_OOL_META - all -Werror clean.
- Neighbours: test-z064 19/0 PASS; test-z065 561/0 PASS;
  test-z066 legs PASS; test-z067 PASS; test-z070 PASS;
  test-z074 (make test-z076's predecessor surface) PASS.
- Standing gate on the final tree: see the recorded run
  (scripts/z13/run_gate.sh; counts read from its log).

## 6. The test-j002 reconciliation (named, not absorbed)

make test-j002 FAILS 550 checks / 113 failures on the
PRISTINE Z-076 base (480459e) and 550 / 113 on this
slice's final tree, with the failure set identical: the
113 are pre-existing drift between the J-002 scripted
expectations and the banked Z-048 mapping-pool byte
accounting (bytes_released counts only bytes actually
returned to the OS; bytes_mapped includes pooled bytes),
left unreconciled by every slice since Z-048. Z-076
changes NONE of them and absorbs NONE of them.
What Z-076 did change in that suite: its ABSENT/PARTIAL
assertions, which this slice's closures falsify by
design - the expect_all contention check now asserts
measured == 1, the legF invariant likewise, and legD3
expects the small cross-thread free COUNTED (+1). With
those updates, the Z-076-attributable failure count is
exactly 0 (33 Z-076-semantic failures before the update,
0 after; total stays 113 before and after the slice).
Reconciling the 113 pool-drift expectations is a
J-002/Z-048 bookkeeping repair, recorded here as the
immediate named follow-on; it is not this slice's scope
and faking it green was not an option.

## 7. Bugs found

None in repo code. The stale ABSENT/PARTIAL assertions
in tests/core/test_j002.c were the slice's own semantic
wake and were updated in scope (S6); the 113 pre-existing
test-j002 failures are recorded above as the named
follow-on, per the never-leave-a-bug-unrecorded law.
