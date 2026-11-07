# Phase 10 — Debug-Only Poisoning + Quarantine of Freed Blocks (J-005)

Slice J-005, Wave 10. Base: 1aab762 (J-002 banked).
Plan: `docs/PHASE_10_FOUNDATION_PLAN.md` (J-001) Section 5 —
S5.1 (one flag, separate build directory), S5.2 (what is
debug-only: "Poisoning of freed bytes" and "Quarantine of
freed blocks", both owned by J-005), S5.3 (the quarantine /
cached-bytes rule), S5.4 (what production keeps unchanged).
Exit item graded by this slice: X4 (J-009 grades finally).
Evidence: `tests/core/test_j005.c` (`make test-j005`),
`tests/core/recorded_run_j005.txt` (Pi-native run).

## 1. Scope

Under `MIMIR_DEBUG` only, a freed SMALL block (the G-003
size-class path, requests of 1..1024 bytes) is poisoned and
held in a bounded quarantine before it may be reused, so a
use-after-free write into it is detected while the block is
still allocator-held. The production build contains none of
this code: every J-005 line in `src/` is inside
`#ifdef MIMIR_DEBUG` or in the `#else`-preserved original
form, and the production suite result is unchanged
(4590 run, 0 failed; recorded run).

The large (mapping-per-allocation) path is out of scope:
INC-J005-01 records why.

## 2. Definitions (fixed before the run)

- Poison pattern: the single byte `0xDD`, written over the
  block's WHOLE class extent at free time (the small path
  records no per-object requested size; the class extent is
  the freed extent the allocator owns, S5.2 "freed extents").
- Quarantine: one global FIFO across classes, capacity
  exactly 32 blocks, fixed static storage (32 entries of
  two pointers = 512 bytes per process, no allocation).
- bytes_quarantined: the summed class extents of the held
  blocks — the S5.3 own-named quantity, exposed test-only
  as `mimir_debug_quarantined_bytes()`.
- A held block is in exactly one place: the quarantine
  table. It is NOT on its span's free list, so it cannot be
  handed out, and `find_span_live_locked` refuses it
  explicitly (a quarantined block is already freed; without
  the guard it would pass the free-list liveness walk).

## 3. Mechanism

Free (small path, debug): poison the extent, move the
production counters exactly as production does
(`live_count`, `mimir_live_small_objects`, the G-003 audit
counter — ownership left the caller at this instant;
quarantine is an allocator-internal hold, not caller
liveness), then insert into the quarantine. If the table
is full, the OLDEST entry is evicted first (below).

Verification points:

- Eviction (the only road back to reuse): the oldest
  block's bytes are compared against the pattern. Clean:
  it is threaded onto its span's free list (its first 8
  bytes become the link, exactly as a production free
  writes them) and may be handed out again. Corrupted:
  the fail-clearly path — a fixed message built in a stack
  buffer, `write(2)` to fd 2, then `abort()`. The path
  allocates nothing, uses no stdio, takes no lock (it runs
  under the global lock already held) and recurses into
  nothing (the Phase 10 rule).
- On demand: `mimir_debug_quarantine_verify()` returns how
  many held blocks are corrupted (0 = clean), -1 on lock
  failure. It never aborts, so detection is provable in a
  test parent; the abort shape is exercised only in
  disposable child processes (test Leg F).

Test-only surface (INC-E001-07 shape (b), J-002/J-004
precedent): `mimir_debug_quarantine_count`,
`mimir_debug_quarantined_bytes`,
`mimir_debug_quarantine_verify` — declared in
`src/core/mimir_core.h` under `MIMIR_DEBUG` and
re-declared in `tests/core/mimir_quarantine_j005.h`;
bound local in the shared library (nm -D exactly 4 in
both flavours, recorded run). No public ABI expansion:
`include/mimir_alloc.h` is byte-identical to base.

Build wiring (this slice's own, S5.1 shape): debug
objects are compiled with `-DMIMIR_DEBUG=1` into the
separate directory `build-debug/` by the additive
Makefile block at the end of the Makefile (`make
test-j005`, `make lib-debug`; `make DEBUG=1 test-j005`
is accepted and identical — the flavour is carried by
the targets). Production targets are untouched.

## 4. The S5.3 rule, discharged

S5.3 offers two compliant shapes. This slice takes BOTH
readings literally:

- Quarantine is implemented INSIDE spans (no separate
  quarantine mappings are ever acquired), and
- a span holding quarantined blocks is never treated as
  EMPTY anywhere: the retention release in
  `small_free_locked`, the shared trim/drain walk
  (`trim_some_locked`), the retained walk
  (`retained_snapshot_locked`, Q-RETAINED) and the J-002
  statistics walk (span_empty_spans / bytes_cached) all
  skip it under `MIMIR_DEBUG`. D-CACHED therefore keeps
  its production definition — retention-cache EMPTY spans
  only — and quarantined bytes are reported as their own
  quantity (Section 2), never folded into D-CACHED.

Proof (test Leg A): two class-512 blocks are allocated and
freed into quarantine; bytes_quarantined reads 1024,
bytes_cached is byte-identical before and after,
Q-RETAINED reads 0, the drain releases 0 spans and mapped
bytes do not move. Production D-CACHED is untouched by
construction (no quarantine code is compiled there).

## 5. What the test proves (recorded run)

`make test-j005`: 81 checks, 0 failures.

- Leg A: S5.3 as above.
- Leg B: after `mimir_free`, all 64 user bytes of a
  class-64 block read back as poison 0xDD; on-demand
  verify reports every held block clean.
- Leg C: the bound is exact — 43 frees against a capacity
  of 32 never exceed 32 held; at the bound
  bytes_quarantined is exactly 32 x 224 for the class-224
  leg; a fresh allocation returns one of the evicted
  (verified) blocks and is fully writable; a double free
  of a quarantined block is inert (the banked
  non-destructive posture); at quiescence
  live_small_objects is 0.
- Leg D: defined-call outcomes match production (S5.2):
  calloc zeroes, realloc grow preserves min(old, new),
  large allocations never enter quarantine, zero-size
  posture NULLs stay posture NULLs.
- Leg E: overhead, measured (Section 6).
- Leg F (disposable children only): child A corrupts one
  byte of a quarantined block and the on-demand verify
  reports exactly 1 corrupted block; child B corrupts a
  block and churns frees until its eviction — the child
  dies of SIGABRT after the fd 2 report (the report line
  itself is in the recorded run), and the parent's
  quarantine is untouched by either child.

Production flavour (same recorded run): `make test`
4590 run, 0 failed; `nm -D --defined-only` exactly 4 on
both `build/libmimiralloc.so` and
`build-debug/libmimiralloc.so`; README.md and
`include/mimir_alloc.h` diff vs base is 0 lines.

## 6. Overhead (measured on this Pi, this run)

- Layout: 0 bytes per block. The span header stays 96
  bytes and every class extent is unchanged; quarantine
  adds no per-block metadata anywhere.
- Static: one 512-byte table (32 entries x 16 bytes).
- Held user bytes: at the bound, the summed class extents
  of the 32 held blocks (32 x 224 = 7168 bytes in Leg C;
  the absolute ceiling is 32 x 1024 = 32768 bytes if every
  held block were the largest class), inside spans that
  stay mapped until their blocks evict and normal
  retention/trim rules take over (INC-J005-02).
- Time: one debug-side measurement, printed in the
  recorded run — 20000 alloc/free cycles of 64 bytes took
  13,757,351 ns (687 ns/op) in the recorded run. A
  measurement, never an oracle. The production-vs-debug
  comparison table is J-008's X8 and is NOT claimed here
  (INC-J005-09); the production column for every J-005
  feature is zero/absent by construction.

## 7. Files

- `src/core/mimir_core.c` — J-005 block (poison, FIFO
  quarantine, verify, report; all `MIMIR_DEBUG`-only),
  the find_span_live_locked guard, the small_free_locked
  debug path (original preserved verbatim in `#else`),
  and the S5.3 guards in the EMPTY tests of
  empty_spans_of_class_locked / trim_some_locked /
  retained_snapshot_locked / the statistics walk. The
  only production-visible diff is those four condition
  lines gaining a compiled-out guard clause.
- `src/core/mimir_core.h` — the three test-only entry
  points, declared under `MIMIR_DEBUG` only.
- `tests/core/mimir_quarantine_j005.h` — test-only
  redeclaration (shape (b)).
- `tests/core/test_j005.c` — the evidence test above.
- `tests/core/recorded_run_j005.txt` — the recorded run.
- `Makefile` — additive debug block appended at the end
  (union-mergeable with J-003/J-004 appends).
- This doc. README.md untouched.

## 8. INC items (gaps, scope choices, reconciliation owed)

- INC-J005-01 (scope): the large mapping path is neither
  poisoned nor quarantined. A freed large block's mapping
  is unmapped whole at free; there are no freed bytes left
  to poison or verify, and quarantining large blocks would
  retain whole mappings (at least two pages each) per
  block — a different retention order than this slice's
  bound. If debug large-block quarantine is ever wanted it
  needs its own design decision and bound.
- INC-J005-02 (retention interaction, G-003): in a debug
  build the per-class empty-span retention bound is not
  enforced at the free that empties a span into quarantine
  (that span is guarded, never released there). Once its
  blocks have all evicted it is an ordinary EMPTY span
  again and converges through trim/drain or a later
  quarantine-free emptying free of its class. Worst-case
  retention beyond the production bound is bounded by the
  quarantine itself: at most 32 spans (one page each on
  top of the production bound's spans). D-CACHED never
  overstates meanwhile (Section 4).
- INC-J005-03 (RECONCILIATION OWED vs J-004, held at
  43be05d on team-04, NOT in this slice's base): both
  slices additively create the MIMIR_DEBUG distinction —
  the keeper must union-merge (a) the two Makefile debug
  blocks (this slice's DBUILD/DCFLAGS/lib-debug/test-j005
  vs J-004's wiring; one build-debug/ directory and one
  flag shape must survive), (b) the two sets of core
  `#ifdef MIMIR_DEBUG` blocks, and (c) most materially:
  J-004's red zones change the debug class stride/extent
  layout. This slice poisons and verifies span->class_size
  as defined WITHOUT red zones. After the merge, the
  poison extent and the verify walk must be re-stated
  against the red-zone layout — poison the user extent,
  never overwrite a red zone or canary, and compose with
  J-004's free-time canary verification instead of
  duplicating or masking it. Until that reconciliation
  runs, J-005's figures describe the no-red-zone debug
  layout only. The same union-merge note applies to
  J-003's additive Makefile appends (disjoint files
  otherwise; no J-003 file is touched here).
- INC-J005-04 (detection window): verification happens at
  eviction and on demand. A block that has been evicted
  onto a free list was verified clean at that instant;
  a later use-after-free write into the free-listed block
  is not re-verified at pop time (its first 8 bytes are
  the free-list link, as in production). Verifying the
  tail bytes at reuse is possible future work, not done.
- INC-J005-05 (layer deviation, deliberate): the report
  path in the core names `write(2)` directly (debug
  builds only). L6 publishes no diagnostic channel and
  B-006 S4 reserves OS facilities to it; J-007 owns the
  allocation-free production reporting channel, and this
  call is to be re-homed onto it when J-007 lands.
  Production contains no J-005 report path at all.
- INC-J005-06 (design choice): the quarantine is ONE
  global FIFO, not per-class. A flood of frees in one
  class evicts other classes' held blocks sooner; in
  exchange the retained-bytes ceiling is a single exact
  global bound (Section 6). Recorded as a choice, with
  the per-class alternative named.
- INC-J005-07 (unload): the H-001 drain deliberately does
  not release quarantine-held spans (same guard as live
  spans — there is no reporting channel at unload and the
  blocks are still allocator-held). A non-terminal drain
  leaves the quarantine fully intact and the allocator
  usable (Leg A proves counts and mapped bytes unmoved);
  at process exit the OS reclaims the pages as it does
  live spans'.
- INC-J005-08 (threading): the quarantine table is guarded
  by the one L6 global lock, like all span state; there
  is no new lock and no lock-free path. D-LOCK stays
  ABSENT exactly as J-002 recorded; nothing here measures
  contention.
- INC-J005-09 (evidence limit): the only timing figure is
  a single debug-side measurement on this Pi under its
  ordinary load (Samtal live, HTTP 200 before and after
  every run in the recorded run). No production-vs-debug
  overhead claim is made by this slice; that table is
  J-008's X8, fed by Sections 5-6 figures.

## 9. Honest limits

Poisoning detects writes that CHANGE a held byte. A
use-after-free write that rewrites the poison byte itself
(0xDD over 0xDD), a write into a block already evicted to
a free list (INC-J005-04), and any use-after-free READ are
outside what this mechanism can see, and nothing in this
doc claims otherwise. Use after free remains undefined
behaviour per the public header; J-005 is a debug-build
detection aid, not a defined recovery.
