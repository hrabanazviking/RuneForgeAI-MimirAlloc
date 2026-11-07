# Phase 7 — Central Fallback Integration (Slice G-004)

Status: IMPLEMENTED and recorded in the recorded run
`tests/core/recorded_run_g004.txt` (real run on this host,
Raspberry Pi purpleviking1, 16 KiB pages, 2026-11-07).
G-004 is the integration audit of the G-003 size-class
span path against the Phase 3 mapping-per-allocation
central fallback, plus the full correctness re-run G-003
commissioned. Plain verdict: **every request class routes
exactly as the banked G-002 rules R-1..R-6 require, the
two-registry free path handles every pointer either path
can produce, and the full re-run is green end to end** —
`make test` plus every standalone target, 24 of 24 exit 0,
including the re-taught E-005 dynamic leg whose segfault
was the last known red target (INC-G003-02, executed
here). Phase 7 is NOT exited by this slice; the exit
assembly remains G-009's (see section 6).

Companions: `docs/PHASE_7_SIZE_CLASSES_G002.md` (G-002,
routing rules R-1..R-6), `docs/PHASE_7_SMALL_OBJECT_PATH_G003.md`
(G-003, the implementation audited here, esp. INC-G003-01
and INC-G003-02), `tests/core/test_g004.c` (the audit test,
64 checks), `tests/core/recorded_run_g004.txt`.

## 1. What was built and repaired

- **New audit test** `tests/core/test_g004.c` (543 lines,
  64 checks, sections F0–F8 plus final accounting), wired
  additively as `make test-g004` (standalone, G-001/G-003
  pattern; `make test` membership unchanged). It audits
  through the four public symbols plus both audit-counter
  sets (E-005 mapping counters, G-003 span/object
  counters) that the span path and the mapping fallback
  are integrated through the ONE central dispatcher
  (`alloc_any_locked`) with no route left unserved and no
  pointer left ownerless.
- **INC-G003-02 executed**: `tests/abi/test_e005_dyn.c`
  re-taught span-aware (section 3).
- **Same-class re-teaches found by the full re-run**
  (section 4): `tests/platform/test_d003.c`,
  `tests/abi/test_e008_mixed.c`, and the Mojo F-005 legs
  (`tests/mojo/f005_probe.c`, `tests/mojo/f005_audit.c`,
  `tests/mojo/test_f005.mojo`,
  `tests/mojo/test_f005_audit.mojo`).
- **`src/core/mimir_core.c`: comment-only change**
  (8 lines, 4 hunks): stale INC cross-references in
  G-003's own comments corrected (INC-G004-04). No code
  line changed; the allocator's behaviour in this slice
  is exactly G-003's banked behaviour, audited.
- **Makefile: additive only** (+24 lines): the
  `test-g004` target and its build rules. No existing
  target changed.

Public header `include/mimir_alloc.h`: byte-identical to
base. README.md: byte-identical to base. `nm -D` on the
rebuilt .so: exactly 4 defined dynamic symbols
(mimir_malloc, mimir_calloc, mimir_realloc, mimir_free).

## 2. Routing audit — R-1..R-6, route by route

Verdicts rest on `tests/core/test_g004.c` as run in the
recorded run (64 checks, 0 failures; final AUDIT line:
span_published=9 span_retired=1 small_alloc=562
small_free=562 published=8 retired=8 live_spans=8),
cross-checked by the re-taught legs of section 3–4.

- **R-1 Allocation routing — PASS.** F1 boundary:
  sizes 1 and 1024 move only the small counters; sizes
  1025 and 4096 move only the mapping counters; frees
  route back to the owning registry. F2 span exhaustion
  (class 32): a full span capacity of live objects plus
  10 more publishes a further span of the same class —
  the class path falls back to a fresh central span,
  never to a spurious failure and never to a private
  mapping (exhaustion publishes NO mapping counter
  movement); every object distinct and pattern-clean,
  and every one freed via spans. F3 retirement: forcing
  class 1024 to at least 3 live spans and freeing
  everything retires exactly the span beyond the G-003
  retention bound (retained empty spans of the class
  ≤ 2 beyond the pre-batch live count). F8 defined
  failures: malloc(0) posture NULL, malloc(SIZE_MAX) and
  malloc(SIZE_MAX−100) exactly NULL, publishing and
  retiring nothing in either registry. One sub-case
  rests on code audit, not execution: a span *backing*
  failure (L6 fails under a small request) cannot be
  forced through the public API on a healthy host — no
  fault-injection hook exists in scope — so the
  no-silent-re-route rule for that case is verified by
  reading the dispatcher: `alloc_any_locked` returns
  the span path's NULL and contains no path that
  re-routes a small request to `alloc_locked`. (The
  F8 header comment in test_g004.c cites this audit
  under a wrong INC number; see INC-G004-06.)
- **R-2 calloc — PASS.** F7: a small calloc reusing a
  dirty freed block reads zero over the whole CLASS
  extent; a large calloc is mapping-served only and
  zeroed over its extent; product overflow is exactly
  NULL and publishes nothing. Cross-checked by the
  E-005 static leg (32 checks) and the re-taught
  dynamic leg (its calloc section moves the mapped
  total by exactly one span page for two small callocs).
- **R-3 realloc — PASS.** F6 matrix: within-class stay
  (50→64) and span shrink stay (64→32), contents
  preserved; cross-class span move (32→96) is
  span-to-span only, contents preserved; small-to-large
  move (96→5000) publishes exactly one mapping and
  frees exactly one span object, contents preserved;
  large shrink across the cutoff (5000→64) stays on
  its mapping and moves no counter (banked B-008
  posture, INC-G003-07 carried); large grow moves;
  SIZE_MAX realloc fails NULL on BOTH registries with
  the original block untouched (failure-atomic);
  realloc(NULL, 64) behaves as malloc.
- **R-4 Free path (two-registry) — PASS.** F4
  interleaved free: small and large blocks alternate,
  cross-order frees move exactly the owning registry's
  counters and leave survivors' patterns intact. F5
  posture probes: free(NULL), interior span pointer,
  span-header pointer, and foreign (libc) pointer move
  no counter and disturb no live block; span double
  free and mapping double free each move no counter,
  and the span class stays reusable and pattern-clean
  after its double-free probe. Posture note, as in
  G-003: invalid frees are UB at contract level; F5
  records THIS implementation's non-destructive course
  only, and no behaviour is promised for any invalid
  free.
- **R-5 Counters and observability — PASS.** Both
  counter sets keep their banked meanings throughout:
  span-served traffic moves only span/object counters,
  mapping-served traffic only mapping counters, in
  test_g004 (final accounting small_alloc == small_free
  == 562, published == retired == 8) and in every
  re-taught leg (E-005 static 32 checks, E-005 dynamic
  34 checks, E-008 mixed 19 checks, F-005 audit leg
  24 checks against the span counters). No object is
  folded into a mapping count anywhere.
- **R-6 Startup and lock — PASS (by re-run and diff).**
  This slice adds no startup allocation, no
  constructor, and no second lock: the only core change
  is comment text (INC-G004-04), and the dispatcher and
  lock structure are G-003's banked code unchanged.
  The suites' startup evidence still reads clean in
  the recorded run — the E-005 static leg's start line
  shows every counter at zero after init, and the full
  `make test` composition passes unchanged. No
  dedicated new R-6 test section was written; the
  verdict rests on the comment-only core diff plus the
  recorded suites, and is recorded at that grade.

## 3. INC-G003-02 — the E-005 dynamic leg re-teach

Pre-repair reproduction (recorded run section 0, same
session, this host, before any G-004 repair):
`make test-e005-dyn` segfaulted (make exit 2). Cause,
as G-003 recorded: the leg applied the mapping-form
serving-header validation and the mapping-form
mapped-bytes model to EVERY pointer; for a span object
at span_base + 96 the descriptor read at (ptr − page)
lands in an unmapped page.

The re-teach (`tests/abi/test_e005_dyn.c`, in the
INC-G003-01 spirit; NO check weakened):

- **Two-form header validation at full strength.**
  Mapping form (requests above the 1024 cutoff): the
  original descriptor validation, unchanged — magic
  "MIMIRALL", self-base, usable_offset == one page,
  requested_size == request, state LIVE, exact banked
  mapping length. Span form (1..1024): magic
  "MIMIRSPN" at the base of the span's one system
  page, self-base, mapping_length == exactly one
  page, usable_offset == 96, offset-32 field == the
  serving class size, usable_size == page − 96, state
  LIVE, class index == the class's index,
  live_count ≥ 1, and the user pointer at the class
  stride inside the object area.
- **Span-aware mapped-bytes model.** Mapping form
  keeps the exact per-allocation byte deltas. Span
  form: objects of one class share one-page spans —
  in the recorded run, 4 × malloc(64) moves the mapped
  total by exactly one page (16384), two malloc(4096)
  by exactly 2 × mapping_len_for(4096), two small
  callocs by one further page, realloc 64→96 by one
  page (new class span), realloc 64→2048 by exactly
  mapping_len_for(2048); after every block is freed
  the mapped total stands at baseline + 3 × page
  (49152 retained bytes = the script's retained empty
  spans under the G-003 bound of 2 per class).
- **An explicit above-cutoff section** now exercises
  the mapping form in this leg — the original script's
  sizes were all ≤ 128, mapping-served only by the
  pre-Phase-7 accident, so it never needed one.
- Test-side call counting now matches validated counts
  per serving form.

Result in the recorded run: `make test-e005-dyn`
exit 0, 34 checks, 0 failures. INC-G003-02 is closed
by this slice.

## 4. Further re-teaches the full re-run found

The first full re-run (recorded run section 0) failed
four more targets, all the INC-G003-01 coupling class
(banked test programs coupled to the mapping-per-
allocation accident G-002 section 9 disowns) in files
G-003 did not amend. Each was re-taught in the same
spirit — span-served sizes get span-equivalent rigour,
mapping-served sizes keep every original check at full
strength — and each is green in the post-repair re-run:

- **test-d003** (INC-G004-02): Part A3 segfaulted on
  span-served sizes 1 and 100 (descriptor read at
  p − page). A3 now branches on serving form: above
  the cutoff, original descriptor checks unchanged;
  span-served, the equivalent span-header verification
  (magic, self-base, one-page length, usable_offset
  96, class-size field, usable_size == page − 96,
  pointer at class stride) plus 16-alignment.
  Recorded: 141 checks, 0 failures, exit 0.
- **test_e008_mixed** (INC-G004-03): the mixed leg
  expected mapping counters for a malloc(64) and a
  page-modulus property (9 checks, 3 FAIL pre-repair).
  The leg now exercises BOTH registries at full
  strength: the 64-byte block in the span counters,
  a 4096-byte block in the mapping counters.
  Recorded: mixed leg 19 checks, 0 failures; whole
  test-e008 target exit 0 (first 9/9, threads 13/13,
  main 33/33).
- **Mojo F-005 legs** (INC-G004-05): f005_probe.c's
  descriptor validation segfaulted on the first
  span-served pointer, taking test-f005 down and all
  five WITH-leg build/runs of test-f006 with it. The
  probe now runs the two-form validation (mapping form
  unchanged above the cutoff; span form at equivalent
  rigour below it); f005_audit.c exposes the G-003
  span counters beside the mapping counters, and both
  Mojo scripts assert the two-registry expectations.
  Recorded: test-f005 exit 0 (dynamic leg 18/18,
  audit leg 24/24); test-f006 exit 0 (harness PASS,
  all builds and runs of both legs passed).
- **mimir_core.c comments** (INC-G004-04): four
  comment hunks cited wrong INC-G003 numbers for the
  facts they describe (no-thread-local-cache note,
  retention-bound note, realloc copy-length notes).
  Corrected to the INC-G003 entries that actually
  carry those facts. Comment-only; no code changed.

## 5. Full correctness re-run — recorded table

From `tests/core/recorded_run_g004.txt` (post-repair
run: `make clean` then `make test` plus EVERY
standalone target, same session, this host). Every
exit code below is the recorded make exit code; check
counts are the counts printed by each target in that
run. The finisher's light re-verification after
landing is section 7, not this table.

| Target | Exit | Recorded result |
|---|---|---|
| make test (8 suites) | 0 | smoke 18/18, core 29, b007 69, c005 95, c006 23, c007 122, c008 42 (7 skipped-not-yet), d004 4590 — all PASS, 0 failures |
| test-g001 | 0 | G-001 probe ran both legs (mimir + libc) to PROBE DONE |
| test-g003 | 0 | 54 checks, 0 failures |
| test-g004 | 0 | 64 checks, 0 failures |
| test-c007 | 0 | 122 checks, 0 failures |
| test-d003 | 0 | 141 checks, 0 failures |
| test-d004 | 0 | 4590 checks, 0 failures |
| test-d005 | 0 | freestanding probe PASS (exit 42 verified twice) |
| test-d006 | 0 | Android toolchain probe completed (toolchain ABSENT on this host, as recorded by the probe) |
| test-d007 | 0 | RESULT: PASS |
| test-d008 | 0 | 163 checks, 0 failures |
| test-e002 | 0 | 16 checks, 0 failures |
| test-e003 | 0 | 16/16 checks passed |
| test-e004 | 0 | 19/19 checks passed |
| test-e005 | 0 | 32 checks, 0 failures |
| test-e005-dyn | 0 | 34 checks, 0 failures (INC-G003-02 closed) |
| test-e006 | 0 | 159 checks, 0 failures |
| test-e007 | 0 | 24/24 checks passed |
| test-e008 | 0 | first 9/9, threads 13/13, mixed 19, main 33/33 — all PASS |
| test-f003 | 0 | PASS (4/4 steps) |
| test-f004-mojo | 0 | PASS (count=192 sum=72096 weighted=7130112) |
| test-f005 | 0 | dynamic leg 18/18, audit leg 24/24 |
| test-f006 | 0 | harness PASS (all builds and runs of both legs) |
| test-f007 | 0 | probe COMPLETE (mojo run road live; bit test attempt 22 run, 22 passed) |

Summary line of the recorded run: 24 targets, every
exit code 0. Header/README integrity is recorded at
the head of the run (both diff-quiet exit 0, md5sums
matching HEAD) and the run closes with `nm -D`:
defined dynamic symbol count 4.

## 6. Exit-evidence status (honest)

Phase 7 is NOT exited by G-004. Of the G-001
X-criteria: X1 (full prior correctness re-run green)
is now supported at the grade G-003 promised — the
full re-run including the re-taught dynamic leg is
recorded green in this slice. X2/X4 keep their G-003
first readings and remain unproven at exit grade
until G-009 assembles the exit evidence under a
stated noise protocol; this slice adds no new latency
or retention claims of its own (the G-001 probe lines
inside the recorded run are the banked probe's, read
as micro-probe readings per INC-G003-10).

## 7. Finisher's light verification (after landing)

Run by the finisher in this worktree after the
engineering above had landed, WITHOUT regenerating
the recorded run:

- `make test-e005-dyn` — exit 0 (34 checks, 0 failures).
- `make test-g004` — exit 0 (64 checks, 0 failures;
  AUDIT line identical to the recorded run's:
  span_published=9 span_retired=1 small_alloc=562
  small_free=562 published=8 retired=8 live_spans=8).
- `git diff` vs base for README.md and
  include/mimir_alloc.h: both empty.
- `nm -D --defined-only` on build/libmimiralloc.so:
  exactly 4 symbols (mimir_malloc, mimir_calloc,
  mimir_realloc, mimir_free).

## 8. Inconsistencies and gaps — INC-G004

- **INC-G004-01 (resolved in-slice).** The E-005
  dynamic leg re-teach commissioned as INC-G003-02,
  executed and closed here (section 3). The file's own
  header cites the commissioning number INC-G003-02;
  this entry is the G-004-side record of the same work.
- **INC-G004-02 (resolved in-slice).** test_d003
  Part A3 shape-aware amendment (section 4).
- **INC-G004-03 (resolved in-slice).** test_e008_mixed
  two-registry re-teach (section 4).
- **INC-G004-04 (resolved in-slice).** Stale INC
  cross-references in mimir_core.c comments corrected
  (section 4). Comment-only; caught because the
  finisher's diff audit read every changed hunk.
- **INC-G004-05 (resolved in-slice).** Mojo F-005
  two-form re-teach — probe, audit counters, and both
  Mojo scripts (section 4).
- **INC-G004-06 (recorded, not fixed in-slice).**
  test_g004.c's F8 header comment cites "INC-G004-02"
  for the span backing-failure code audit, but
  INC-G004-02 is the test_d003 amendment; the audit
  itself is section 2 (R-1) of this doc. A comment
  cross-reference mismatch with no behavioural
  effect. Not edited here: test_g004.c is landed,
  hash-verified work of this slice, and the finisher's
  remit was the doc and the commit, not a rebuild of
  landed files. A future slice touching test_g004.c
  should correct the citation to point at this doc's
  section 2.
- **INC-G004-07 (evidence limit, carried).** The R-1
  backing-failure sub-case (section 2) is proven by
  code audit only, because no fault-injection hook
  exists in scope to force an L6 failure through the
  public API. If a future slice adds fault injection,
  this sub-case should get an executed test and this
  entry closes.

## 9. Files changed by this slice

- `Makefile` — additive `test-g004` wiring only (+24).
- `src/core/mimir_core.c` — comment-only INC
  cross-reference corrections (INC-G004-04).
- `tests/core/test_g004.c` — new (64-check audit test).
- `tests/core/recorded_run_g004.txt` — new (recorded
  run, 2785 lines).
- `tests/abi/test_e005_dyn.c` — INC-G003-02 span-aware
  re-teach (section 3).
- `tests/platform/test_d003.c` — INC-G004-02
  shape-aware Part A3.
- `tests/abi/test_e008_mixed.c` — INC-G004-03
  two-registry re-teach.
- `tests/mojo/f005_probe.c`, `tests/mojo/f005_audit.c`,
  `tests/mojo/test_f005.mojo`,
  `tests/mojo/test_f005_audit.mojo` — INC-G004-05
  two-form re-teach.
- `docs/PHASE_7_CENTRAL_FALLBACK_G004.md` — this file.

Public header `include/mimir_alloc.h`: byte-identical.
README.md: byte-identical.
