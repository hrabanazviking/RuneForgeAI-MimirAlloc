# C-004 — B-007 completeness audit and gap closure

Slice: **TEAM-04 C-004**. Base: **72e52e1** (banked C-003).
Audited surface: the banked C-003 core line
(`src/core/mimir_core.c`, `src/core/mimir_core.h`,
`src/api/mimir_alloc.c`, `include/mimir_alloc.h`) and the C-002
platform layer it consumes (`src/platform/mimir_platform.c/.h`),
against `docs/PHASE_3_ARITHMETIC_ALIGNMENT_INIT_ERROR_DESIGN.md`
(**B-007**) and `docs/contracts.md` (**A-008**).
Host: Raspberry Pi `purpleviking1`, runtime system page size
16384 bytes, recorded runs only (see Evidence below).

**U-03 caveat, carried unchanged:** `mimir_malloc` /
`mimir_free` have no direct contract rows in `docs/contracts.md`
([UNRESOLVED: U-03], stability Unassigned). Every contract
mapping in this audit is **by analogy only, Proposed only** —
never a direct namespaced contract and never an ABI claim.
`MA-6.3-*` bootstrap/diagnostics rows are cited directly, as
B-007 cites them.

## Verdict summary

- **Two literal gaps in the banked C-003 core were found and
  are closed by this slice** (code changes, in
  `src/core/mimir_core.c` only):
  1. **A9** (pointer-range check) was discharged only by
     construction in C-003; B-007 A9 requires the check to be
     *stated in code*, not assumed. It is now an explicit
     check before publication, with the A.3 failure
     disposition (mapping released, nothing published).
  2. **A10** (bookkeeping checks) covered only the
     `live_bytes` addition in C-003; the `live_mappings += 1`
     increment was unchecked. The check now also refuses
     publication when `live_mappings == SIZE_MAX`.
  No other behaviour changed.
- **Everything else B-007 requires in Phase 3 scope was
  already implemented** in the banked C-002/C-003 code, item
  by item in the tables below. Where this audit adds anything
  beyond the two closures, it is tests and this document —
  no work was invented.
- **Evidence is split honestly into two classes:**
  *runtime-proved* (a recorded Pi test observes the
  behaviour) and *inspection-only* (the code states the check
  and its disposition, but the failure branch cannot be
  triggered through the public API on a healthy host and no
  fault-injection hook exists in Phase 3 scope —
  INC-C004-02). No inspection-only item is claimed as tested.
- **Open by design, not gaps:** U-09 (zero-size), U-10
  (`errno`), U-11 (explicit-alignment validity), U-15
  (numeric alignment), U-17..U-21 (fork), the B.3
  over-aligned re-specification, and the INC-B007-A1 /
  INC-C003-01 total-mapping formula reconciliation (owed by
  a B-006 design revision; the code is unaffected in result
  because `usable_offset` is exactly one whole system page).
- **New inconsistencies recorded:** INC-C004-01 ..
  INC-C004-05 (register at the end). One (INC-C004-04, a
  misleading code comment) was fixed in-slice; the rest are
  recorded with their dispositions.
- **Header wording:** no correction to
  `include/mimir_alloc.h` is required by B-007. The header's
  claims are exactly B-007's non-numeric suitably-aligned
  claim (B.1), the null-only failure channel (D.0/D.1.1),
  the contents non-promise (C1), and the open register
  (U-09/U-10/U-11/U-15). Nothing for the keeper to flag.

## Evidence

- C-002 recorded run: `tests/platform/recorded_run.txt`
  (platform A5/A6 rejections, mapping pattern, lock).
- C-003 recorded run: `tests/core/recorded_run.txt`
  (smoke 11/11, core 29/29 at the banked base).
- C-004 recorded run: `tests/core/recorded_run_c004.txt`
  (`make clean && make test` on the Pi for this slice:
  smoke, C-003 core test, and the new
  `tests/core/test_b007.c` audit test; counts are read from
  that file, not restated from memory here).
- Code inspection: this audit's line-by-line read of the
  four audited files at base 72e52e1 plus the C-004 diff.

Verdict words used below: **PROVED** = runtime-proved by a
recorded run named in the row. **INSPECTED** = implemented
and verified by code inspection; failure branch not
triggerable via the public API (INC-C004-02). **BY DESIGN**
= the conforming behaviour is an absence / non-claim, and
the code correctly does not implement or claim it.

## A. Checked arithmetic (B-007 Section A)

| Item | Where implemented | Test evidence | Verdict |
|---|---|---|---|
| A.1 check-before-evaluate rule | Every arithmetic site in `mimir_core.c` and `mimir_platform.c` tests its predicate before evaluating (sites A3–A10 below); no wrap-then-check anywhere in either file | Inspection + the refusal rows below | INSPECTED (rule), PROVED at its observable sites |
| A1 requested-size admission (`> 0`, representable) | `mimir_core_alloc`: `size == 0` returns NULL (posture only, U-09 open); representability enforced by A3/A5 downstream | test_b007 + test_core: `malloc(0)` posture NULL; oversize rows | PROVED (posture); zero-size itself stays U-09 **BY DESIGN** |
| A2 page-size validity (runtime discovery, no fallback, no hardcoded value) | `mimir_platform_page_size`: `sysconf` checked, value must be non-zero, fit `size_t`, power of two (stricter than A2 text — INC-C004-03); core fails NULL on any discovery failure, attempts no mapping | C-002 run: query succeeds, 16384 observed; all tests use the runtime value. Discovery-failure branch not triggerable on a healthy host | PROVED (success path), INSPECTED (failure branch) |
| A3 `metadata_reserve + requested_size` checked | `mimir_core_alloc`: `if (size > SIZE_MAX - usable_offset)` before `total_unrounded = usable_offset + size`; `usable_offset` is the S7.1 reserve of exactly one page (INC-C003-01: the two B-006 formula forms coincide for a whole-page reserve) | test_b007: `SIZE_MAX`, `SIZE_MAX - 8`, `SIZE_MAX - page + 1` return NULL; sentinel undisturbed after each | PROVED |
| A4 `usable_offset + usable_region` checked; offset == exactly one system page | Same checked addition as A3 (the layout computes one sum; L6 rounds it); `usable_offset = page_size` assigned from the discovered value, never a literal | test_b007 descriptor read-back: `usable_offset == page` on every inspected allocation | PROVED |
| A5 round-up, addition form `x <= SIZE_MAX - (page_size - 1)` before evaluate | `page_round_up_checked` in `mimir_platform.c` (C-002) | C-002 run: `acquire(SIZE_MAX)` → ERR_OVERFLOW before any OS call; test_b007: `SIZE_MAX - page` (passes A3 exactly) returns NULL from the core | PROVED |
| A6 round-up, count form (division first; `page_count <= SIZE_MAX / page_size` before multiply) | `page_round_up_checked`, both forms satisfied in sequence (C-002) | C-002 run as A5; test_b007 boundary sizes read back exact `mapping_length == page + page_round_up(requested)` for page−1, page, page+1, 2page−1, 2page, 2page+1 | PROVED |
| A7 alignment round-up | Not performed on the ordinary path — B.007 B.1: alignment is by construction, no numeric round-up exists to check. No explicit-alignment entry point exists (B.2 gate open) | Absence verified: header declares only the namespaced pair; nm check in recorded runs | **BY DESIGN** (gate open, U-11) |
| A8 `usable_size = mapping_length - usable_offset`, subtraction checked; publication gate `usable_size >= requested_size` | `mimir_core_alloc` after acquire: `usable_offset > mapping_length` refused, then `usable_size < size` refused; both release the mapping and publish nothing | Gate's pass side PROVED on every test_b007 descriptor (`usable_size == mapping_length - usable_offset >= requested`); failure branch needs an L6 short mapping, which the all-or-nothing L6 never returns | PROVED (pass), INSPECTED (failure branch) |
| A9 `user_ptr = mapping_base + usable_offset` pointer-range check before evaluate | **Closed by C-004.** C-003 discharged A9 by construction only (comment-cited); the check is now explicit code before publication: `usable_offset > UINTPTR_MAX - (uintptr_t)base` → release mapping, unlock, NULL | Normal-path regression: full `make test` passes with the check in place. Failure branch untriggerable: a real mapping base is never within one page of `UINTPTR_MAX` on this host | **PROVED (closure present, normal path)**, INSPECTED (failure branch) — gap closed by this slice |
| A10 bookkeeping additions checked under the lock, same section as insertion (`live_bytes += mapping_length`, `live_mappings += 1`) | **Completed by C-004.** C-003 checked only `live_bytes`; the exact check before publication now reads `mapping_length > SIZE_MAX - live_bytes \|\| live_mappings == SIZE_MAX` → release, unlock, NULL. Increments happen in the same locked section as `registry_insert` | Continued correct operation across all recorded tests (including 24-cycle and interleaved runs in test_b007). Failure branch untriggerable: it needs `SIZE_MAX` live bytes or mappings | **PROVED (closure present, normal path)**, INSPECTED (failure branch) — gap closed by this slice |
| A.3 uniform failure (null, before syscall for arithmetic failures, no wrapped/partial block, registry and counters unchanged) | Every check site above follows the one disposition: unlock (releasing any acquired mapping first) and return NULL through L1 | test_b007: after each of five refused/unsatisfiable requests the sentinel pattern is intact and a fresh allocation succeeds, is usable and frees — the observable consequence of unchanged registry/counters | PROVED (observable side) |
| A.4 boundary cases | Decided by the first failing step, per the table above: `SIZE_MAX` (A3), `SIZE_MAX - reserve` (A3 passes, A5 fails), `+1` (A3), exact-page (rounds to itself), one-over (one extra page, included in the checked total) | test_b007 exercises each named case and reads back exact mapping lengths at the page boundaries | PROVED |
| A.5 / INC-B007-A1 formula discrepancy | Carried open, exactly as INC-C003-01 records: the code computes the checked sum and lets L6 round up; both textual forms give the identical length for a whole-page reserve, and no test cites either unreconciled text as settling `mapping_length` | — | **BY DESIGN** (reconciliation owed by a B-006 design revision, not by code) |

## B. Alignment (B-007 Section B)

| Item | Where implemented | Test evidence | Verdict |
|---|---|---|---|
| B.1 default alignment by construction (`mapping_base` page-aligned + one whole page offset ⇒ user pointer system-page aligned; suitably aligned by analogy to MA-6.1-MALLOC-01; **no numeric value claimed**, U-15 open) | Layout in `mimir_core_alloc` publication/return path; claim wording in `include/mimir_alloc.h` and `src/api/mimir_alloc.c` is the non-numeric claim only | test_b007 + test_core: every returned pointer checked `% page == 0` and `% _Alignof(max_align_t) == 0` across all sizes incl. boundaries | PROVED (structural fact on this host; not restated as a numeric contract) |
| B.2 explicit-alignment validity gate (U-11 open; no behaviour stated for invalid alignment, size-multiple, 0, non-power-of-two, sub-pointer, over-page values) | No explicit-alignment entry point exists in the header or the API translation unit; nothing is admitted, so nothing is silently defined | nm absent-symbol evidence in C-003/C-004 recorded runs: the API TU exports exactly `mimir_malloc`, `mimir_free` | **BY DESIGN** (gate open) |
| B.3 over-aligned (> page size) requests | Not designed and not served: no entry point can express one, and no ad-hoc offset enlargement exists in the core. The B.3 re-specification (oversized mapping + free-path descriptor derivation, INC-B007-7) remains owed before any support claim | Absence verified by inspection of the core: `usable_offset` is always exactly one page | **BY DESIGN** (not designed) |
| B-F1 ordinary-path arithmetic failure | A.3 uniform failure (Section A above) | As Section A | PROVED / INSPECTED as per site |
| B-F2 / B-F3 invalid alignment / size-multiple | No behaviour stated — gate open (U-11) | — | **BY DESIGN** |
| B-F4 valid-but-unsatisfiable explicit request | No such interface exists in this baseline; neither the null channel nor the POSIXMA return-value channel is imported into `mimir_malloc` (D.1.2) | Header/API inspection | **BY DESIGN** |
| B-F5 alignment > page size | Not designed (B.3); never silently downgraded — no code path returns a merely page-aligned pointer for an alignment request, because no alignment request can be expressed | Inspection | **BY DESIGN** |
| B-F6 zero-size with valid alignment | No behaviour stated (U-09) | — | **BY DESIGN** |
| B-F7 null output parameter (POSIXMA shape) | No such interface exists | — | **BY DESIGN** |
| B-F8 page-size discovery failure defeats even B.1 | Core: discovery failure returns NULL before any layout is computed; no fallback constant exists anywhere in `src/` | Inspection (branch untriggerable on healthy host); success path PROVED everywhere | INSPECTED (failure), PROVED (success) |

## C. Clean initialization (B-007 Section C)

| Item | Where implemented | Test evidence | Verdict |
|---|---|---|---|
| C1.1 user contents unspecified, never zero-promised | Core never reads or writes user bytes; header and API comments state the non-promise verbatim in B-007's terms | No test in any recorded run asserts zero contents for `mimir_malloc` (asserting it is forbidden by C1.3) | **BY DESIGN** (non-promise kept in code, docs and tests) |
| C1.2 allocator never touches user bytes; lock never guards them | Publication writes only the descriptor in the reserve page; free path reads only registry-owned descriptors | test_b007/test_core: full-extent patterns undisturbed across frees, partial frees and later allocations | PROVED |
| C1.3 incidental mmap zero-fill is not a guarantee (INC-B007-3) | Recorded in core header comment: fresh mappings reading as zero is called incidental, never promised | No zero assertion exists to contradict it | **BY DESIGN** |
| C1.4 descriptor fully written before publication (`magic`, base, length, offset, requested, usable, `state` LIVE, links; `flags` unassigned bits zero; `reserved` zero) | Publication block in `mimir_core_alloc`: every field assigned, `reserved` memset to zero, `flags = 0`, then `registry_insert` writes both links — all in the same locked section | test_b007 reads back every field via the S7.3 offsets for the first call, all boundary sizes, and 24 repeated cycles: magic, `mapping_base ==` descriptor address, length, offset, requested, usable, state, flags, reserved, and (single-live-block) NULL links all exact | PROVED |
| C1.4 registry/counters atomic with insertion; starting state needs no init run | Registry anchors and both counters are static storage (empty / zero); insertion and both increments happen in the one locked publication section | Behavioural: interleaved alloc/free/alloc runs in test_b007 stay consistent and every later allocation succeeds; counters themselves have no public observability (L7 compiled out), so atomicity as such rests on inspection of the single locked section | PROVED (behaviour), INSPECTED (atomicity mechanism) |
| C1.5 calloc zeroing is B-008 scope, not imported | No `mimir_calloc` exists (C-005 scope); the contrast is named in the core header comment only as the C1 non-promise | nm absence evidence | **BY DESIGN** |
| C2.1 pre-main / first entry (MA-6.3-001, -005): assume nothing, serve or fail via defined channel | No init step exists to assume: anchors/counters static, lock statically initialised; the ordinary path *is* the first-entry path | test_b007: the first `mimir_malloc` in the process succeeds with no prior call. Entry literally before `main` (constructor/loader context) has **no test harness** in this baseline | PROVED (first-call-in-process), INSPECTED (pre-main context — no harness; recorded here, not claimed) |
| C2.2 bootstrap-owned storage, no self-allocation, no recursion (MA-6.3-002, -003, -012) | All bootstrap bookkeeping is fixed static storage below L1 (anchors, counters, the L6 mutex object); there is no initialization function at all, so nothing can recurse | Inspection: no allocation occurs on any path before the first mapping, and none is needed | INSPECTED (structural; nothing exists to trigger) |
| C2.3 reentrancy during init (MA-6.3-004) | No init interval exists in which to re-enter; a reentrant call takes the same locked ordinary path | Inspection (structural) | INSPECTED |
| C2.4 no allocator-dependent containers; no silent ordering reliance (MA-6.3-010, -011) | The implementation is C11 with no containers and no allocating static objects | Inspection | INSPECTED |
| C2.5 lock statically initialised (B-006 S9.1) | `static pthread_mutex_t mimir_global_lock = PTHREAD_MUTEX_INITIALIZER;` in `mimir_platform.c` | C-002 run: lock exercised by two threads, counter exact at 20000; every core test runs under it | PROVED |
| C2.6 bootstrap-served blocks are not a second allocator (MA-6.3-006) | There is no separate bootstrap path: every block is served by the one path and freed by the one free | Every recorded free of every test block succeeds through the same `mimir_free` | PROVED (vacuously, by single-path construction) |
| C2.7 fork during bootstrap — **no claim** (U-21) | No `pthread_atfork` registration, no fork claim in header, API or core comments | — | **BY DESIGN** (no claim made) |

## D. Error semantics (B-007 Section D)

| Item | Where implemented | Test evidence | Verdict |
|---|---|---|---|
| D.0 malloc failure is exactly null, never partial/smaller/unallocated; free returns nothing; `free(NULL)` defined no-op | All core failure paths return NULL; free is `void` and returns before any locking for NULL | test_smoke + test_core + test_b007: null refusals, `free(NULL)` returns normally (including after failures) | PROVED |
| D.1.1 `errno` not promised (U-10 open) | No code in `src/` sets, clears or documents an `errno` value for the pair; header states the non-promise. (L6's internal `mmap` may set `errno` incidentally; per B-007 that carries no semantics) | No test asserts an `errno` value | **BY DESIGN** (non-promise) |
| D.1.2 POSIXMA return-code model not imported | No output-parameter interface exists; `mimir_malloc` has only the null channel | Header/API inspection | **BY DESIGN** |
| F-01 mmap fails (ENOMEM) | L6 all-or-nothing acquire → `ERR_MAP`; core returns NULL, no descriptor exists, registry/counters unchanged | test_b007: `SIZE_MAX / 2` (representable, unsatisfiable) returns NULL with sentinel undisturbed and allocator healthy after | PROVED |
| F-02 request unrepresentable | Rejected at admission before any syscall (A3/A5 sites) | test_b007 + C-002 run refusals | PROVED |
| F-03 mmap fails EMFILE/ENFILE | Same L6 disposition as F-01; no close/evict/retry code exists in L6 | Inspection (same code path as PROVED F-01; not separately triggered — would need descriptor-limit exhaustion) | INSPECTED |
| F-04 mmap fails EAGAIN, no retry | Same disposition; no retry loop exists anywhere in L6 or the core | Inspection (no-retry is an absence, verified by reading both files) | INSPECTED |
| F-05 zero-size | **No observation promised** (U-09). Implementation posture is NULL, recorded as posture in code, header notes and tests — never cited as settling U-09 | Posture observed in all recorded runs | **BY DESIGN** (excluded/undecided) |
| F-06 munmap fails during free: descriptor stays LIVE, stays linked, counters unchanged; restore-on-failure permitted form | Core free path marks RETIRED, unlinks, decrements, then releases via L6 under the lock; on release failure it restores state LIVE, re-links and restores both counters in the same locked section — the F-06 outcome. (Reporting half of F-06: INC-C004-01. Re-link position: INC-C004-05) | Not triggerable: `munmap` of a valid, exactly-acquired mapping does not fail, and no fault-injection hook exists | INSPECTED |
| F-07 lock failure: never proceed unlocked, never return overlapping/unpublished memory | Core alloc: lock failure → NULL before any work. Core free: lock failure → return with the block left LIVE and recorded as owned. Unlock failure after sound publication returns the pointer (comment-reasoned in code; no report channel exists under the design) | Not triggerable with a statically initialised normal mutex on a healthy host | INSPECTED |
| F-08 entry before init completes / reentrant / pre-main | Section C2 rows above | As C2.1 | PROVED (first call), INSPECTED (pre-main) |
| F-09 metadata magic/state mismatch (corruption) | Free path validates the registry-found descriptor (magic, state LIVE, `mapping_base + usable_offset == ptr`, `mapping_base ==` own address) and on mismatch takes the non-destructive course: nothing unmapped, unlinked or counted. No correction is claimed. Invalid frees remain UB (D.3); this baseline course is **not** a defined behaviour and no test cites it as one (NOTES.md item 2) | Deliberately untested as a behaviour (UB territory) | **BY DESIGN** (posture recorded, no claim) |
| F-10 multi-step sequence completes in part | Baseline issues exactly one mapping per allocation; L6 acquire is all-or-nothing, so no partial sequence exists to roll back; post-acquire check failures (A8/A9/A10) release the one mapping and publish nothing | A.3 observable side PROVED in test_b007 health-after-failure checks | PROVED (observable side), INSPECTED (internal rollback sites) |
| F-11 page-size discovery fails: defined failure, no fallback constant | L6 `ERR_PAGESIZE`; core NULL, no mapping attempted. No page-size literal (4096/16384 or other) appears in `src/` | Inspection + the A2 row; branch untriggerable on a healthy host | INSPECTED |
| F-12 reporting cannot allocate / errors cascade | The baseline reports nothing at all (L7 compiled out, B-006 S4), so reporting performs no allocation and a reporting failure cannot arise or change an outcome — see INC-C004-01 for the tension this creates with D.4 | Inspection | INSPECTED, with INC-C004-01 recorded |
| D.3 UB boundary (foreign / interior / already-freed pointer, use after free: no defined error of any kind) | Header, API and core comments classify all of these as UB with no defined error or recovery; the registry-walk validation never dereferences memory derived from the unvalidated pointer (NOTES.md item 2). Debug detection is best-effort only and converts nothing to Defined | Deliberately untested (no defined outcome exists to check) | **BY DESIGN** |
| D.4 bounded, allocation-free reporting, never under lock (MA-6.3-020..023) | No reporting exists to bound: L7 is compiled out and no failure path emits any report, under the lock or otherwise. The never-under-lock half is therefore vacuously satisfied; the report-itself half is INC-C004-01 | Inspection | INSPECTED, with INC-C004-01 recorded |
| Cross-thread handoff (MA-6.2-054, by analogy; B-006 S8.4) | One global lock guards all registry/counter state; ownership records are thread-agnostic | C-002 run proves the lock primitive under two threads; no C-004 test allocates on one thread and frees on another — recorded plainly as not covered by this slice's tests | INSPECTED (lock PROVED; handoff sequence untested) |

## Contracts cross-check (docs/contracts.md)

- Rows exercised by analogy and proved above: MA-6.1-MALLOC-01
  (success: non-null, suitably aligned, distinct),
  MA-6.1-MALLOC-02 (failure: null, no unallocated pointer, no
  live block disturbed), MA-6.1-MALLOC-04 (oversize treated
  as failure under -02), MA-6.1-FREE-01 (live pointer retired
  exactly once), MA-6.1-FREE-02 / MA-6.2-010 (`free(NULL)`
  no-op), MA-6.2-001 (compatible free accepted exactly once),
  MA-6.2-020 / MA-6.2-024 (no partially constructed or
  half-owned block published), MA-6.2-041 (page quantities are
  always the runtime system page size, never a literal).
- Rows held open exactly as contracts.md holds them:
  MA-6.1-MALLOC-03 (U-09), MA-6.1-ALIGNED-03/-04 and
  MA-6.1-POSIXMA-03 (U-11), MA-6.1-ALIGNED-05 /
  MA-6.1-POSIXMA-04 (U-09), V-02 → U-10 (`errno`),
  V-07 → U-15 (numeric alignment), MA-6.3-040..042 with
  U-17..U-21 (fork: no claim).
- Rows whose subject does not exist in this baseline, with
  the absence itself the conforming posture: all
  MA-6.1-CALLOC-* and MA-6.1-REALLOC-* (B-008 design;
  C-005 implementation scope — not audited here and not
  imported), MA-6.1-ALIGNED-* / MA-6.1-POSIXMA-* success and
  failure shapes (no such entry points), MA-6.3-030..032
  (no async-signal-safety claimed anywhere).
- MA-6.3-001..006, -010..012 (bootstrap): Section C rows.
  MA-6.3-020..023 (reporting): D.4 row and INC-C004-01.
  MA-6.2-030..034 (UB / debug detection): D.3 and F-09 rows.
  MA-6.2-002/-005 (foreign free): UB, D.3 row.
  MA-6.2-050..053 (runtime configuration): no runtime
  configuration surface exists in Phase 3; nothing to audit.
  MA-6.2-021/-022/-023 (realloc/input-storage rows): B-008
  scope, no such entry points.

## Inconsistencies recorded by this slice (INC-C004-*)

1. **INC-C004-01 — B-007 D.4 / F-06 reporting has no
   implementation (compiled out).** B-007 D.2 F-06 ends
   "Reported via D.4, after lock release", and D.4 specifies
   bounded, allocation-free reporting. The banked baseline
   reports **nothing at all**: L7 observability is compiled
   out per B-006 S4, and no failure path in the core or L6
   emits a report. Caller-visible semantics are unaffected
   (D.0: `mimir_free` returns nothing; `mimir_malloc`
   failure is the null return, with or without a report),
   and F-12's rule (a reporting failure never changes an
   outcome) is vacuously satisfied. But the F-06 reporting
   sentence is not discharged by any code. Recorded, not
   resolved: a reporting channel is L7 / Phase 10 design
   territory, outside this slice's scope; the tension is
   between B-006 S4 (compiled out in Phase 3) and B-007
   D.4's expectation, and belongs to a design revision.
2. **INC-C004-02 — Evidence limitation for six failure
   branches.** The failure branches of A8 (short mapping),
   A9 (pointer-range wrap), A10 (counter overflow), F-06
   (munmap failure), F-07 (lock failure) and F-11
   (page-size discovery failure) cannot be triggered
   through the public API on a healthy host, and Phase 3
   has no fault-injection hook (Roadmap S9.3 / B-009
   territory). Their audit verdicts rest on code
   inspection. This is an evidence limitation, not a code
   defect; it is recorded so no later reader mistakes the
   INSPECTED verdicts in the tables for recorded runs.
3. **INC-C004-03 — L6 page-size admission is stricter than
   B-007 A2's text.** A2 admits a discovered page size that
   is non-zero with `page_size - 1` representable.
   `mimir_platform_page_size` additionally requires a power
   of two. The stricter rule is safe on every admitted
   target (system page sizes are powers of two, and the
   A5/A6 forms rely on it), but the texts differ. Recorded;
   reconciliation belongs to a design revision. The code
   takes no position.
4. **INC-C004-04 — Misleading A10 comment in the banked
   core (FIXED in-slice).** The comment before the L6
   acquire call in `mimir_core.c` was headed "Bookkeeping
   check before any mapping (B-007 A10 form)" but no
   pre-acquire check code followed it — only prose saying
   an exact test runs later. A10 itself is discharged by
   the exact post-acquire, pre-publication check (A10
   requires the check before the *addition*, under the
   lock, in the insertion section — all satisfied), so the
   behaviour was always conforming; the comment wording
   was not. The heading now states plainly that no
   pre-acquire bookkeeping evaluation is performed and
   that the exact A10 check runs after acquire, before
   publication. Comment-only change.
5. **INC-C004-05 — F-06 rollback restores list membership,
   not list position (observation).** On a release failure
   the free path re-links the descriptor with
   `registry_insert`, which inserts at the registry head,
   not at the descriptor's original position. F-06 and
   B-006 require the descriptor to stay LIVE, stay linked
   and keep the counters unchanged — all restored exactly;
   no text specifies list order. Recorded so no later
   reader assumes positional restore.

Carried forward, unchanged: **INC-C003-01 / INC-B007-A1**
(total-mapping formula, A.5 row above), **INC-B007-7**
(over-aligned free-path derivation, B.3 row above).

## Scope guard confirmation

No `calloc`/`realloc` work (C-005). No new public API: the
header is untouched and the API translation unit still
exports exactly `mimir_malloc` and `mimir_free` (nm evidence
in the C-004 recorded run). Code changes are confined to
`src/core/mimir_core.c`: the A9 explicit check, the A10
`live_mappings` check, the C-004 header-comment addendum,
and the INC-C004-04 comment correction. README.md untouched.
