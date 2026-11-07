# Phase 9 - Trimming and Cache-Release Implementation (Slice I-003)

Status: **IMPLEMENTED and measured on the Pi.** This slice
changes `src/core/mimir_core.c` and `src/core/mimir_core.h`,
adds the test-only header `tests/core/mimir_trim_i003.h`,
the evidence test `tests/core/test_i003.c` with its
recorded run `tests/core/recorded_run_i003.txt`, and one
additive Makefile target (`test-i003`). It makes **no**
change to `include/mimir_alloc.h` or `README.md` (both
byte-identical to base, sha256-verified in the recorded
run), and it adds **no public symbol**: `nm -D
--defined-only` on the built `libmimiralloc.so` is
exactly **4**, as required.

Base: `6b495be90c520fc38554aa9a98bfc674798ab74b` (I-002
banked; I-001 banked `ccc83d8` before it), branch
`yrsa/mimiralloc-03`, worktree reset `--hard` to base and
HEAD verified at muster.

Governing texts: `docs/PHASE_9_FOUNDATION_PLAN.md`
(I-001: two-path inventory, Section 2.2 retained bound,
Section 2.3 H-001 drain, I-003 evidence obligation in the
Wave 9 map, exit item X3) and
`docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md` (I-002:
Section 3.2 quantity definitions Q-MAPPED / Q-RETAINED /
Q-RELEASED, Section 5 the trim semantics this slice
implements, Section 5.3 the quiescent-point
reconciliation rule). Roadmap Phase 9 ("trimming and
cache release" beside soft budgets) is the charter.

## 0. What was actually verified on the Pi in this slice

- Base HEAD `6b495be` verified by `git rev-parse HEAD`
  after the reset. `getconf PAGESIZE` = **16384**;
  `uname -m` = **aarch64**; `free -g` at slice start:
  15 GiB total, 13 GiB available; Samtal on port 8787
  answered HTTP **200** before the work and **200** after
  (both readings inside the recorded run).
- Every number in Sections 3 and 6 comes from the
  recorded run `tests/core/recorded_run_i003.txt`
  (fresh run on the Pi in this slice: `make clean &&
  make test` exit 0, `make test-i003` 72 checks /
  0 failures, `make test-h001` 299 checks / 0 failures,
  `nm -D` count 4). No other measurement is claimed.

## 1. What was implemented

### 1.1 The trim walk (one release path)

`src/core/mimir_core.c` gains one locked helper,
`trim_some_locked(max_spans)`, which is now the single
walk behind **both** the H-001 drain and the new
on-demand trim - exactly the "one release path, not a
budget-flavoured variant" that I-002 Section 5.1
requires. With the global lock held it releases retained
EMPTY spans (`live_count == 0`) whole through the banked
`span_release_locked` (the L5/L6 release path with its
F-06 rollback), in class-fair round-robin passes: each
pass visits classes 0..19 in index order and releases at
most one span per class, stopping as soon as `max_spans`
spans have been released or a full pass releases nothing.
Within a class it releases the deepest EMPTY span of that
class in the registry (see INC-I003-01 for exactly how
that stands against I-002's oldest-empty-first rule).

The H-001 drain `mimir_core_drain_retained` is
re-implemented as this walk with no span limit. The set
of spans a full drain releases is unchanged (every
retained EMPTY span; live spans and live mappings never
touched); only the walk order changed, which cannot
change how many bytes a full release returns (I-002
Section 5.2 preamble). The drain's recorded evidence was
re-run fresh in this slice and is unregressed:
`make test-h001` = **299 checks / 0 failures**, the
banked H-001 counts exactly (INC-I003-06 records the
order change itself).

### 1.2 Entry points (all internal, shape (b))

Declared in `src/core/mimir_core.h` and re-declared for
tests only in `tests/core/mimir_trim_i003.h` - the exact
H-001 drain / G-007 pair pattern (INC-E001-07 shape (b)):
hidden visibility plus the version script bind every one
local in the shared library, so no `.so` consumer can
link against them and `nm -D` stays at 4.

| Entry point | Behaviour |
|---|---|
| `size_t mimir_core_trim_retained(void)` | Full on-demand trim: releases every retained EMPTY span; returns spans actually released. |
| `size_t mimir_core_trim_retained_some(size_t max_spans)` | Partial trim: the same walk stopped after at most `max_spans` releases (the I-002 S5.2.3 stop-at-need mechanism). `max_spans == 0` releases nothing. |
| `size_t mimir_core_retained_spans(void)` | Q-RETAINED span count, one locked registry snapshot. |
| `size_t mimir_core_retained_bytes(void)` | Q-RETAINED bytes: sum of the EMPTY spans' `mapping_length`s, one locked snapshot. |
| `size_t mimir_core_mapped_bytes(void)` | Q-MAPPED (I-002 S3.2): `mimir_live_bytes + mimir_live_span_bytes` read as one locked snapshot, never two torn reads. |

Bytes released by a trim are spans x page size exactly
(every span is one page), and the evidence test
reconciles that figure three independent ways at
quiescent points (Section 3). The trims are non-terminal
and idempotent exactly as the drain is: nothing retained
returns 0, a lock failure releases nothing and returns
0, and the allocator stays fully usable afterwards. The
three queries return `(size_t)-1` only on lock failure
(the G-007 getter posture) rather than a torn figure.

### 1.3 The evidence test

`tests/core/test_i003.c`, standalone via `make test-i003`
(additive Makefile target; `make test` composition
unchanged). Single-threaded, so every reading is at a
quiescent point by construction (I-002 S5.3). Script:
keep five small blocks (request 100, class 112) and one
8192-byte mapping live throughout; warm classes 256 /
512 / 1024 past one span each (span capacity + 1 blocks)
and free them all; then partial trims, class-fairness
reuse probes, a full trim with an OS-side reading, no-op
trims, a bound re-verification, the drain regression leg,
and final counter reconciliation. The OS-side figure is
the sum of anonymous `rw-p` bytes in `/proc/self/maps`,
read with raw `open`/`read` into a static buffer so no
heap or stdio activity of the test itself can move the
figure across a trim.

## 2. Semantics vs I-002 Section 5, point by point

| I-002 rule | I-003 standing |
|---|---|
| S5.1 Trim = release EMPTY retained spans, whole, to L6, the drain's operation on demand | **Implemented exactly.** Same `span_release_locked` path, same walk as the drain (S1.1). |
| S5.1 Trim returns a count; bytes = count x page size | **Implemented exactly**, and strengthened: Q-RETAINED before/after is directly readable (`mimir_core_retained_bytes`), so bytes released are reconciled, not merely derived (S3). |
| S5.1 No silent ABI expansion | **Held.** Internal-only surface (S4); `nm -D` exactly 4 in the recorded run. |
| S5.2.1 EMPTY spans only, ever; live allocations never released | **Implemented exactly and proven**: live small blocks and the live mapping are pattern-verified byte-for-byte after every trim leg, and the span accounting shows only the one live span surviving the full trim. |
| S5.2.2 Class-fair round-robin, one span per class per pass | **Implemented exactly and proven** by reuse probes that distinguish the fair walk from a greedy one (S3). |
| S5.2.2 Within a class, earliest-EMPTIED span first | **DEVIATION - INC-I003-01.** Implemented as earliest-ACQUIRED (deepest in the registry). No became-empty order is recorded anywhere and the 96-byte span header is fully assigned (G-002 S7), so the assumed order is not computable from existing state. A full trim is unaffected (same set, same bytes); a partial trim may pick a different span of the same class than the assumed rule would. |
| S5.2.3 Stop exactly at need (partial trim) | **Mechanism implemented exactly**: `mimir_core_trim_retained_some` stops at the asked count mid-walk. There is no budget to drive it yet (INC-I003-02). |
| S5.2.4 Release failure: span stays retained, walk continues, count reflects only actual releases | **Implemented by inheritance** from `span_release_locked`'s banked rollback (a failed release restores the span LIVE and re-linked; the walk does not count it and moves on). **Not fault-injected in this slice** (INC-I003-03): fault injection is I-004's charter. |
| S5.3 Quiescent-point reconciliation against E-005/G-003 counters | **Implemented exactly**: every identity asserted in the test holds exactly at quiescent points, including the final published == retired reconciliation of all three counter pairs. |

## 3. Measured figures (recorded run only)

Page size 16384 B throughout. Warm counts: 64 x 256 B,
32 x 512 B, 16 x 1024 B (span capacity + 1 per class).

- After warm + free: Q-RETAINED = **6 spans =
  98,304 B** (2 per class, the default bound), Q-MAPPED
  reconciled exactly as live span (1 page) + retained
  (6 pages) + the live mapping's `mapping_length`
  (32,768 B). Counter identities at that point: 7
  live+retained spans, 5 live small objects, 1 live
  mapping.
- `trim_retained_some(0)` released **0**;
  `trim_retained_some(2)` released **2 spans =
  32,768 B** (span_retired delta 2, Q-RETAINED 4 pages);
  reuse probes then showed classes 256 and 512 each
  still holding a retained span (0 new spans published),
  which a class-greedy walk would have falsified;
  `trim_retained_some(1)` released **1 span =
  16,384 B**, after which a 256 probe published exactly
  1 fresh span - the lowest class was exhausted first,
  as the fair walk prescribes.
- Full trim released **4 spans = 65,536 B**: the
  returned count, the Q-RETAINED drop (4 pages to 0),
  the `span_retired` delta (4) and the OS-side figure
  moved together byte-exactly - anonymous `rw-p` bytes
  **688,128 -> 622,592**, a drop of exactly **65,536 B**.
  Live blocks byte-identical afterwards (all pattern
  checks passed); exactly 1 span (the live one) remained.
- Trim again and a partial trim on the empty cache each
  returned **0** (no-op, idempotent). Re-warming 256
  re-retained exactly **2** spans (bound re-verified, no
  ratchet) and a trim released exactly those 2. The
  drain leg released exactly 2, then 0.
- Final totals after the closing drain: span
  published / retired **12 / 12**, small alloc / free
  **216 / 216**, mapping published / retired **1 / 1**;
  Q-RETAINED 0, Q-MAPPED 0.
- Whole-suite standing: `make test` exit **0** with the
  banked composition unchanged; `make test-h001`
  **299 / 0** (drain unregressed); `make test-i003`
  **72 checks / 0 failures**; `nm -D --defined-only` =
  **4** (`mimir_malloc`, `mimir_free`, `mimir_calloc`,
  `mimir_realloc`); trim symbols in the `.so` dynamic
  table: **0**.

## 4. Public-surface decision (recorded, binding on this slice)

**Decision: internal-only.** The trim and its queries are
INC-E001-07 shape (b) internal entry points, declared
only in `src/core/mimir_core.h` and the test-only
`tests/core/mimir_trim_i003.h`, bound local in the shared
library. The public ABI stays at exactly the four banked
symbols and the public header is byte-identical to base.

Reasons, in the banked terms: I-002 Section 3.6 already
fixed this pattern for Phase 9 surfaces (the G-007
tuning pair is the proven precedent: internal now,
promotion is a future contract slice's business, with
the re-proof obligations promotion carries - header,
contracts, version script, nm proof); the namespaced
surface itself is [UNRESOLVED: U-03], so no Phase 9
slice may silently settle its ABI; and the Wave 9 queue
line for I-003 says the same ("public surface decision
recorded, no silent ABI expansion"). A **public trim API
is therefore OWED to a later ABI/contract slice**, which
must decide its name, signature (span count vs bytes vs
a result struct), and error posture, and re-prove the
surface when it moves. Nothing in this slice pre-decides
that choice beyond providing the internal operation such
a slice would promote.

## 5. The budget-enforcement gap (INC-I001-14) - what this slice does and does not adopt

INC-I001-14 stands **exactly as recorded**: soft-budget
enforcement is chartered to no slice, and this slice
does **not** implement it. There is still no budget
scalar, no setter/getter, no at/over-budget decision
procedure (I-002 S3.3), and no budget refusal anywhere
in `src/`: a grep for budget in the core finds only
comments pointing at this section.

The one piece of the I-002 design this slice honestly
adopts is the budget's **step-3 tool itself**: the
partial-trim stop-at-need mechanism
(`mimir_core_trim_retained_some`, I-002 S5.2.3) now
exists with the exact class-fair order a future budget
path must spend, and the Q-MAPPED / Q-RETAINED locked
snapshots (I-002 S3.6's query discipline) exist for its
step-2 projection to read. When a later slice implements
the budget, it calls these; nothing here calls them on
its own - no allocation path changed behaviour, which
the unchanged `make test` totals and the byte-identical
live-block evidence confirm.

## 6. Exit item X3 (I-001 grading) after this slice

X3 asked for: an on-demand operation releasing retained
empty spans to the OS, shown by counters and an OS
figure moving together, live blocks untouched, the
retention bound re-verified, and the public-surface
decision recorded. On this slice's evidence X3's
substance is **MET** for the internal surface: release
on demand exists and is measured (S3), the drain
behaviour it extends is unregressed (299/0), and the
surface decision is recorded (S4). The formal exit
grading belongs to I-009's packet, not to this slice;
the two honest caveats it must carry are INC-I003-01
(the within-class order proxy) and the internal-only
scope of the surface (a `.so` consumer still has no
caller-reachable trim until the owed ABI slice lands).

## 7. Inconsistencies and deviations (INC-I003-NN)

- **INC-I003-01 (deviation from I-002 S5.2.2):**
  within-class release order is earliest-ACQUIRED, not
  earliest-EMPTIED. The assumed order is uncomputable
  from existing state (no became-empty record; the
  96-byte header is fully assigned). Fixing it means
  spending state the design has not budgeted - e.g. a
  per-span sequence field needs a header change with
  its own slice and re-proofs. Recorded, not hidden;
  full-trim byte totals are unaffected.
- **INC-I003-02 (carried gap):** budget enforcement
  remains absent (INC-I001-14 stands). I-003 ships the
  step-3 mechanism and the locked quantity snapshots
  only; S3.3's procedure, the budget scalar and its
  setter/getter remain owed to whichever slice the
  keeper charters for them.
- **INC-I003-03 (unproven posture):** the S5.2.4
  release-failure continuation is inherited from the
  banked rollback, not exercised: no fault injection
  exists at this base to fail an L6 release on demand.
  I-004 (Nth-mapping fault injection) is the chartered
  place to prove it.
- **INC-I003-04 (evidence shape):** per-class retained
  counts have no counter; class-fairness is proven by
  reuse probes (published-count deltas) at the tested
  shapes, not by a per-class readout. A per-class query
  would be new internal surface; not added here because
  the probes decide the fair-vs-greedy question the
  design actually asks.
- **INC-I003-05 (owed counter):** Q-RELEASED as a
  cumulative byte accumulator (I-002 S3.2,
  Proposed-internal) was not added. Released bytes are
  reconciled per event (returned count x page, checked
  against Q-RETAINED and counter deltas and the OS
  figure). The cumulative accumulator belongs with the
  telemetry work (I-007 / Phase 10), which owns the
  counter set as a whole.
- **INC-I003-06 (deliberate change, verified):** the
  H-001 drain's walk order changed from registry order
  to the shared class-fair walk. The released set and
  counts are unchanged; H-001's full recorded evidence
  was re-run fresh (299/0, including the 50-cycle
  0 B-growth dlopen leg) rather than assumed.

## 8. Files in this slice

- `src/core/mimir_core.c` - `trim_some_locked`,
  `retained_snapshot_locked`, drain re-implemented on
  the shared walk, five I-003 entry points.
- `src/core/mimir_core.h` - declarations for the five
  entry points (internal header; not installed).
- `tests/core/mimir_trim_i003.h` - test-only
  redeclaration (shape (b)).
- `tests/core/test_i003.c` - evidence test, 72 checks.
- `tests/core/recorded_run_i003.txt` - recorded run
  (723 lines): environment, integrity hashes, full
  `make test`, `make test-i003`, drain regression,
  `nm -D` proof, Samtal 200 before/after.
- `Makefile` - additive `test-i003` target only.
- This document.

Untouched, verified byte-identical to base in the
recorded run: `README.md`, `include/mimir_alloc.h`.
No push and no merge by this team; the keeper banks.
