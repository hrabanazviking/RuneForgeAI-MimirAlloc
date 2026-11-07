# Phase 10 — Statistics Balance Proof (Slice J-003)

Wave 10, Phase 10 (Diagnostics and hardened builds). Base:
1aab762 (J-002 banked; Wave 10 = 2 of 9 at slice start).
Branch: yrsa/mimiralloc-03. Team: TEAM-03, solo lead,
sequential.

Governing material: the J-001 counter-definition
contract (docs/PHASE_10_FOUNDATION_PLAN.md S4, esp. the
reconciliation identities R1-R12 of S4.4); the J-002
statistics surface (docs/PHASE_10_STATISTICS.md:
mimir_core_stats_snapshot via
tests/core/mimir_stats_j002.h, the 8 handed identities,
INC-J002-01..06); the WAVE_PLAN.md banking
reconciliation for J-002, including its two disclosed
derivations (D-ACQ-CUM is not a named field; D-ALLOC-LIVE
is not a named field but is exactly derivable) and its
disclosed narrowing (a realloc transfer-retire is NOT
counted as a cross-thread free).

## 1. What was built

One proof test, tests/core/test_j003.c, standalone via
`make test-j003` (additive Makefile block after the
test-j002 block; NOT part of `make test`), plus its
recorded run (tests/core/recorded_run_j003.txt) and
this record. No src change, no public header change,
no README change: the J-002 surface reconciled exactly
as banked, and no counter defect was found (S5).

Method, in one paragraph: the test keeps its OWN script
state — cumulative requested / allocated / released /
failures / posture NULLs / cross-thread frees,
cumulative large published bytes, and the exact set of
live blocks with their request sizes — updated by
script arithmetic from the runtime page size, never
copied from a snapshot. At every quiescent point it
takes ONE locked snapshot and equates three independent
sources: the script, the J-002 snapshot (one locked
registry walk), and the E-005 / G-003 audit publication
counters. D-ALLOC-LIVE and D-ACQ-CUM enter through the
exact derivations the J-002 banking reconciliation
prescribes:

- D-ALLOC-LIVE = live_mapping_bytes - live_mappings x P
  + sum over classes of span_live_objects x
  span_class_size.
- D-ACQ-CUM = script cumulative large mapping_lengths
  + mimir_audit_span_published x P.

## 2. Identity-by-identity verdicts

| Identity | Verdict | Where proven |
|---|---|---|
| R1 alloc_published - mapping_retired == live_mappings == registry length | PROVEN | every quiescent point, all legs; under pressure in the Leg 7 child (delta form) |
| R2 span_published - span_retired == live_spans == registry length | PROVEN | every quiescent point; class rows sum to live_spans (R10 form) at the same points |
| R3 small_alloc - small_free == live_small_objects == D-OCC live | PROVEN | every quiescent point, all legs |
| R4 D-MAPPED == live bytes sum == independent Q-MAPPED read | PROVEN | every quiescent point (snapshot-internal form and the separately locked I-003 mimir_core_mapped_bytes cross-read agree because the points are quiescent, S4.3) |
| R5 D-MAPPED == D-ALLOC-LIVE + overhead, no remainder | PROVEN | term by term: large walk (live_mapping_bytes == script usable sum + live_mappings x P, the reserve-page term), span walk (live_span_bytes == live_spans x P), small walk (snapshot charged sum == script charged sum); the derived D-ALLOC-LIVE equals the script's live granted total exactly |
| R6 D-ACQ-CUM - D-REL-CUM == D-MAPPED | PROVEN | every quiescent point; D-ACQ-CUM via the derivation above, D-REL-CUM == bytes_released asserted against the script at the same points; closed under RLIMIT_AS pressure in the Leg 7 child (released delta == published delta at mapped == 0) |
| R7 D-CACHED == EMPTY x P == D-MAPPED - (live spans x P + Q-LIVEMAP); bounded | PROVEN with the reading fixed by INC-J003-01 | every quiescent point, incl. Leg 4's 6 retained spans and partial/full trims; bound asserted as cached <= retention_bound x 20 x P with the bound read from the G-007 tuning surface (2 at default) |
| R8 peak discipline | PROVEN | Leg 6: peak == mapped exactly at the driven high-water instant; unchanged by a free-only sequence and by a trim-only sequence (asserted exactly, not as >=); after free-all + drain, mapped == 0 while peak retains the high-water value exactly; every later prove_all asserts peak == that locked value, and the final snapshot confirms no later leg exceeded it |
| R9 per-entry-point call accounting | PROVEN | Leg 2: malloc 7 calls == 4 successes + 2 genuine failures + 1 posture NULL; calloc 4 == 2 + 1 + 1; realloc 5 == (2 publications + 1 stay completion) + 1 grow-construction failure + 1 posture NULL, the stay/shrink/grow split explicit in the leg script; each line cross-checked against the E-005 audit call counters. Stated exception, not exercised: realloc of an invalid pointer is undefined behaviour (MA-6.1-REALLOC-05) and counts NEITHER success, failure, nor posture (J-002 S4 identity 5); the proof does not invoke UB to grade it |
| R10 D-OCC aggregate == sum of the 20 class rows | PROVEN for live objects, total spans, empty spans (via R7's first form) and capacity (aggregate == sum of per-span capacities recomputed from the class table); the free-slots clause is NOT-PROVABLE (INC-J003-02) | every quiescent point |
| R11 D-XTHREAD | Tier 1 PROVEN (single-threaded legs read exactly 0 at every point; the two scripted Tier-1 cross-thread frees count exactly 2); the realloc transfer-retire narrowing is PROVEN-AS-DOCUMENTED-NARROWING (Leg 5 XT3: a worker grows main's large block by realloc and frees the new block — the counter moves by exactly 0 across the leg, asserted AS the documented behaviour of the J-002 banking reconciliation, not as a Tier-1 truth); Tier 2 (small path) exact count NOT-PROVABLE by J-002's recorded choice (b) — its documented contribution of exactly 0 is asserted (Leg 5 XT4) | Leg 5; 0-readings asserted in every earlier leg |
| R12 full-cycle closure | PROVEN | at the end of every leg (free-all + drain: R1-R3 read 0, D-MAPPED == 0, D-CACHED == 0, D-ACQ-CUM == D-REL-CUM i.e. released == all published bytes), over the whole run at the final point, and under pressure in the Leg 7 child |
| D-LOCK (contention count) | NOT-PROVABLE | ABSENT by INC-J002-02: no counter exists. The test asserts the ABSENT marker pair lock_contention == 0 / lock_contention_measured == 0 in EVERY snapshot (a fabricated count would break this), and claims no contention figure anywhere |
| D-FAIL-BUDGET | NOT-PROVABLE | 0-by-absence (INC-I001-14): no budget exists and no field exists in the snapshot struct to assert; graded here as its recorded status, exactly as S4.4 prescribes |
| D-REQ-LIVE | not an R-identity; NOT PROVIDED (INC-J002-05 / INC-I002-02) | no identity in this proof is stated with it |

Tally: 10 identities PROVEN outright (R1-R9, R12);
R10 PROVEN except its free-slots clause; R11 PROVEN
for Tier 1 with one PROVEN-AS-DOCUMENTED-NARROWING
case and Tier 2 NOT-PROVABLE; D-LOCK and D-FAIL-BUDGET
NOT-PROVABLE as recorded absences. Nothing was
silently skipped: every NOT-PROVABLE carries its
reason above and in S4.

## 3. Workloads (one leg per concern, all on the Pi)

- Leg 0: zero baseline of a fresh process image.
- Leg 1: mixed churn — 10 x 100 B + 5 x 1000 B small,
  5 large (2000/5000/3000), partial frees, 3 x calloc
  (4,50), a same-class staying realloc, a cross-class
  small realloc move (112 -> 224), a small-to-large
  grow-by-move realloc; identities at 7 quiescent
  points plus closure.
- Leg 2: R9 accounting (above), incl. posture NULLs
  (malloc(0), calloc zero factor, realloc(ptr,0)),
  pre-admission failures (calloc product overflow,
  oversize malloc) and an injected grow-construction
  failure with the original preserved.
- Leg 3: fault injection via the I-004 --wrap shim —
  injected acquire, page-size and lock failures each
  count exactly one D-FAIL-OS; the exact difference
  between D-FAIL-OS and the shim is stated and
  asserted: in this leg D-FAIL-OS delta 5, shim
  injected total 3, difference exactly the 2
  pre-admission failures that never reach L6. A
  release-failure leg arms the shim's release wrap:
  the free rolls back (F-06), the block stays LIVE,
  and R1/R6 and every cumulative figure reconcile
  with it live; the retry releases it and reconciles
  again.
- Leg 4: trim cycles in the I-003 shape — 2 spans in
  each of 3 classes (counts computed from runtime
  span capacities, page-size-agnostic), all 6 retained
  empty at the default bound, trim_retained_some(2),
  then trim_retained() for the rest; R7 asserted at
  every step.
- Leg 5: cross-thread frees (above): worker-frees-
  main's large (+1), main-frees-worker's large (+1),
  the transfer-retire narrowing case (+0, asserted as
  documented), small cross-thread free (+0, PARTIAL).
- Leg 6: peak discipline (above): the high-water is
  driven a scripted 6 pages past the previous peak
  with 5000-byte larges and locked; free-only and
  trim-only sequences must not move it.
- Leg 7: I-008-style pressure — a forked child sets
  RLIMIT_AS to its measured VmSize baseline + 96
  pages, allocates 200 x 64 B smalls, then 4096-byte
  larges until the kernel refuses; exactly 2
  caller-visible failures (the breaking NULL and one
  repeat) are asserted against the D-FAIL-OS delta,
  R1 holds at the pressure point, and after free-all
  + drain the child asserts R12 closure and R6 in
  deltas (released delta == its own published bytes).
  Parent waits with a 30 s wall bound; the parent's
  own counters are asserted untouched afterwards.
  The child's large count is read from its own run
  line in the recorded run, not assumed.
- Final: whole-run closure; released == all published
  bytes; peak == the Leg 6 high-water exactly.

## 4. INC-J003 register

- INC-J003-01 (R7's middle form: a contract reading
  had to be fixed): S4.4 R7 reads "D-CACHED ==
  D-MAPPED - (live spans x P + Q-LIVEMAP)". The
  snapshot's live_spans counts ALL registered spans
  (live-carrying and retained-empty alike — R2's form
  requires that reading), and under it the middle
  expression reduces to live_span_bytes - live_spans
  x P == 0 whenever any span is retained, contradicting
  R7's own first form (cached == EMPTY x P, which
  holds exactly). The two readings cannot both stand;
  this proof fixes "live spans" in R7's middle form
  as spans carrying live objects (total - EMPTY),
  under which all three forms agree term by term at
  every point, including 6 retained spans. Recommend
  J-009 grade R7 under this reading and the contract
  wording be annotated at the Phase 10 exit.
- INC-J003-02 (R10 free-slots clause NOT-PROVABLE):
  free slots == carved - live needs the carved count;
  the snapshot exposes capacity (an upper bound) but
  no carved field — the J-002 banking reconciliation
  already disclosed "the carved-objects component of
  D-OCC is not a named field". No exact derivation
  from the snapshot exists (carve granularity is an
  allocator internal), so the clause is graded
  NOT-PROVABLE rather than approximated by
  capacity - live, which would be a different,
  weaker statement.
- INC-J003-03 (derived, not named — status, not
  defect): D-ACQ-CUM and D-ALLOC-LIVE are proven
  through the banking reconciliation's prescribed
  derivations (S1). Both derivations are exact and
  are asserted against independent script state, so
  the identities close with the same force as named
  fields; the derivation dependency is recorded so
  J-009 grades what was actually proven.
- INC-J003-04 (the proof's teeth are test-side, and
  they bit the test, never the allocator): the first
  run of this test failed 20 checks, every one traced
  to the TEST's own script assumptions — span counts
  calibrated to 4 KiB pages on a 16 KiB-page host
  (the trim leg), an acquire-attempt baseline read
  one injection too early, and the R7 reading of
  INC-J003-01. Each was fixed in the test (runtime
  capacities; correctly placed baseline; the fixed
  reading); the allocator's counters reconciled in
  every case once the script stated the documented
  semantics. No counter defect was found in
  src/core; no src change was made or is owed.

## 5. Evidence (all run on the Pi in this slice)

- make test-j003: 2155 checks / 0 failures,
  RESULT: PASS (tests/core/recorded_run_j003.txt).
  Page size on this host: 16384 bytes; the proof is
  page-size-parametric throughout.
- make test: PASS, 4590 checks / 0 failures
  (composition unchanged).
- make test-j002: 550 checks / 0 failures.
  make test-i008: PASS.
- nm -D --defined-only on build/libmimiralloc.so:
  exactly 4 (mimir_malloc, mimir_free, mimir_calloc,
  mimir_realloc); no statistics symbol is exported.
- README.md and include/mimir_alloc.h byte-identical
  to base 1aab762 (sha256 verified at commit time).
- Samtal on the Pi answered HTTP 200 before and after
  the run; loads stayed light (single test binary,
  one short-lived child).

## 6. Scope kept

Tests and docs only: tests/core/test_j003.c,
tests/core/recorded_run_j003.txt, this doc, and one
additive Makefile block (standalone test-j003 target
after the test-j002 block; `make test` unchanged).
No src, header, README, API or platform change. The
J-004 files (debug red zones) are untouched; this
slice's files are disjoint from team-04's by
construction.
