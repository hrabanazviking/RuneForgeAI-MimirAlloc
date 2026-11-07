# Phase 9 - Genuine Allocation-Failure Handling (Slice I-004)

Status: **IMPLEMENTED and measured on the Pi.** This slice
adds the fault-injection shim whose absence is
INC-I001-01 (carried from INC-C007-01 / INC-C008-01), the
evidence test `tests/core/test_i004.c` with its recorded
run `tests/core/recorded_run_i004.txt`, and one additive
Makefile target (`test-i004`). It changes **no** file
under `src/` or `include/`: the shim is a test-only object
interposed at link time, so the production allocator is
byte-for-byte the banked build. It makes **no** change to
`README.md` (byte-identical to base, sha256-verified in
the recorded run), and it adds **no public symbol**:
`nm -D --defined-only` on the built `libmimiralloc.so` is
exactly **4**, and the `.so` contains no shim symbol at
all (both proven in the recorded run).

Base: `267b361b9e13a8ed3c8a9ae6f89f37f9d8e7bffb` (I-003
banked; I-002 `6b495be` and I-001 `ccc83d8` before it),
branch `yrsa/mimiralloc-04`, worktree reset `--hard` to
base and HEAD verified at muster.

Governing texts: `docs/PHASE_9_FOUNDATION_PLAN.md`
(I-001: the failure shape read from the code, Section 2.5;
the exit item X4; INC-I001-01, which names I-004 as the
shim's owner), `docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md`
(I-002: Section 6.3, the failure-handling contract FC-1 to
FC-6 this slice proves), `docs/PHASE_9_TRIM_IMPLEMENTATION.md`
(I-003: INC-I003-03, the release-failure continuation
inherited but not fault-injected there, addressed here in
Leg 5), and the banked C-007 / C-008 records
(`tests/contract/NOTES_C007.md`, `tests/core/NOTES_C008.md`,
`tests/core/AUDIT_B008_C008.md`), whose injection cases
stood SKIPPED/NOT-YET for want of this instrument.
Roadmap Section 9.3 ("artificially fail the Nth mapping")
is the charter.

## 0. What was actually verified on the Pi in this slice

- Base HEAD `267b361` verified by `git rev-parse HEAD`
  after the reset. `getconf PAGESIZE` = **16384**;
  `uname -m` = **aarch64**; `free -g` at slice start:
  15 GiB total, 13 GiB available; Samtal on port 8787
  answered HTTP **200** before the work and **200** after
  (both readings inside the recorded run).
- Every number in Sections 3 and 4 comes from the
  recorded run `tests/core/recorded_run_i004.txt`
  (fresh run on the Pi in this slice: `make clean &&
  make test` exit **0**, `make test-i004` **248 checks /
  0 failures**, `make lib` exit 0, `nm -D` count **4**).
  No other measurement is claimed. No allocator defect
  was found by any leg; no `src/` fix was needed or made
  (Section 7 records this as a finding, not an omission).

## 1. The shim: design and road chosen

### 1.1 The problem

Every mapping-failure row of I-001 Section 2.5 rested on
code inspection, because no way existed to make the L6
mapping entry fail on demand. B-009 Section 4.3b had
specified a shim on the L6 mapping boundary since Phase 3;
C-007 and C-008 both carried its absence as their first
inconsistency (INC-C007-01, INC-C008-01), and I-001 made
it exit-critical (INC-I001-01) with I-004 as owner.

### 1.2 The road: link-time interposition, test-only

The least invasive honest road in this tree is the
linker's `--wrap` interposition, and it is the road taken:

- `tests/core/mimir_fault_shim_i004.c` defines
  `__wrap_mimir_platform_map_acquire`,
  `__wrap_mimir_platform_map_release`,
  `__wrap_mimir_platform_global_lock` and
  `__wrap_mimir_platform_page_size`. The additive
  Makefile rule for `test_i004` passes
  `-Wl,--wrap=<name>` for those four L6 entries, so in
  **that binary only**, the core object's calls to L6
  resolve to the wrappers, and the wrappers call the
  genuine entries through the `__real_` names.
- No production object carries injection code. The
  library objects in the test binary are the same
  `LIBOBJS` every other test links; `make test`'s eight
  binaries are built and linked exactly as banked (their
  recorded counts are unchanged, Section 3); the shared
  library never sees the shim (recorded run: `nm` over
  `libmimiralloc.so` finds **0** `mimir_fault` symbols).
- The control surface is INC-E001-07 shape (b), exactly
  like the E-005 counters and the I-003 trim surface:
  declared only in a test-only header
  (`tests/core/mimir_fault_i004.h`), reachable only by
  linking the shim object directly, bound to no installed
  header, and no part of any contract ([UNRESOLVED: U-03]
  posture unchanged).

### 1.3 Shim semantics (exact)

Each wrapper counts its calls in a dedicated counter
triple (attempts / injected / real-failed). Arming is
single-shot and counted from the arming call:
`mimir_fault_arm_acquire(n)` (n >= 1) lets n-1 acquire
calls forward, then fails the n-th once and disarms
itself. The injected failure returns **exactly the error
the real entry returns for the corresponding genuine
failure, without performing the operation**:

| Channel | Injected return | Operation performed? | State left, i.e. the genuine-failure state |
|---|---|---|---|
| map_acquire | `ERR_MAP`, out params NULL / 0 | No `mmap` call | Nothing mapped (all-or-nothing, F-01) |
| map_release | `ERR_UNMAP` | No `munmap` call | The mapping stays mapped (what F-06 rollback is written for) |
| global_lock | `ERR_LOCK` | Mutex not taken | No lock held (F-07 posture) |
| page_size | `ERR_PAGESIZE` | No value stored | Discovery failed (F-11) |

Every non-injected call forwards to the real entry
unchanged, and a non-OK result from the real entry
increments that channel's **real-failed** counter -- so
injected failures and genuine OS failures are counted
separately by construction (the FC-4 separation, for the
OS kind). The shim allocates nothing, takes no lock of
its own, and never calls back into the allocator: an
injected failure is a counter decrement and a return
(FC-3's no-retry, no-recursion, allocate-nothing error
path). Allocator-driven calls reach the wrappers with
the core's global lock already held, which serialises
the counters for this single-threaded evidence test.

## 2. The FC contract, point by point (I-002 Section 6.3)

| I-002 clause | I-004 standing, from the recorded run |
|---|---|
| **FC-1 NULL-only failure** | **Proven.** Every injected failure -- acquire on both allocation paths, calloc on both paths, every realloc construction, lock, page-size -- surfaced as exactly `NULL` from the entry point. No partial block, no smaller substitute, no pointer into unowned storage: in the Leg 1 sweep the five survivors of each round are pairwise non-overlapping over their full requested extents, and every leg's publication-counter delta equals exactly its scripted successes. |
| **FC-2 Realloc failure-atomicity** | **Proven, including the branch that rested on inspection since C-005.** Grow failures on the mapping path, the small path and the cross-class small path, `realloc(NULL, n)` failure, and the transfer-step release failure (Leg 5c) each returned `NULL` with the original LIVE, byte-identical over its full requested extent, and freeable exactly once afterwards (retired-counter deltas prove the exactly-once). In the transfer case the new mapping is shown published-then-retired (published +1, retired +1) while the original's retirement never moved. |
| **FC-3 No hang, bounded path** | **Proven by construction and completion.** Neither the shim nor any allocator error path retries, waits or allocates; the test is a fixed script (30 storm rounds, fixed sweeps) whose completion is the no-hang evidence, and the recorded run additionally wraps the binary in `timeout 120`, which did not fire (exit 0). No wall-clock figure is claimed as an oracle. |
| **FC-4 Counter separation** | **Proven with the shim's counters; the core-counter form stays owed (INC-I004-06).** Across the whole run: acquire attempts **112** = injected **47** + real **1** + successes; the single real failure is Leg 0's genuine unsatisfiable request, counted as real and never as injected. Posture NULLs (below) moved no failure counter. Budget refusals are **0 by construction**: no budget exists in this tree (INC-I001-14 stands, Section 5), so every non-posture NULL in the run is OS-kind and is accounted for. The E-005 / G-003 identities hold exactly at the end: published == retired for mappings (**41/41**) and spans (**23/23**), alloc == free for small objects (**30/30**). |
| **FC-5 Frees safe after failures** | **Proven.** `free(NULL)` is a no-op before, inside (as a posture neighbour) and after the storm; freeing every block allocated before, during and after the injected sequences returns Q-MAPPED and Q-RETAINED to exactly **0** at quiescent points after every leg, and a fresh allocation succeeds after the storm. Failures leak nothing: the closing trim of Leg 7 retires exactly the 6 spans the script retained, no more. |
| **FC-6 Posture cases stay postures** | **Proven.** Leg 0's posture set -- `malloc(0)`, `calloc(0, 8)`, `calloc(8, 0)`, calloc product overflows, `malloc(SIZE_MAX)`, `malloc(SIZE_MAX - 8)`, `realloc(ptr, 0)` -- returned `NULL` (original byte-identical for the realloc case) with **zero** acquire attempts, zero release attempts and zero movement in any fault or publication counter: posture NULLs are never reclassified as OS failures, and `errno` is never asserted anywhere (U-10 posture unchanged). Oversize rejections are shown to happen **before** any mapping attempt, which is also FAIL-011's missing no-attempt evidence. |

Retained/trim consistency (the tasking's I-003 clause):
at every quiescent point the test reads Q-RETAINED and
Q-MAPPED through the I-003 locked snapshots; after every
leg both are exactly 0, and Leg 5 exercises trim itself
under release failure (Section 4). No leg observed a torn
or stale retained figure.

## 3. Measured figures (recorded run only)

Page size 16384 B throughout. `test_i004`: **248 checks,
0 failures**, exit 0.

- **Leg 0** (transparency + separation): unarmed, exactly
  2 acquire attempts and 2 release attempts for a scripted
  large+small alloc/free/drain cycle, 0 injected, 0 real.
  Posture set: 0 attempts of any kind (table above). The
  genuine `malloc(SIZE_MAX/2)`: 1 attempt, counted **real**,
  recovery succeeds.
- **Leg 1** (Nth sweep, large malloc): for each N in
  1..4, six 2048-byte mallocs under `arm_acquire(N)`
  produce exactly one `NULL`, at exactly position N;
  6 attempts, 1 injected, 5 published per round; survivors
  pairwise non-overlapping and byte-identical; a recovery
  allocation succeeds after each round.
- **Leg 2** (small malloc): fresh classes 16 / 48 / 80 /
  160 / 320 / 640 each fail their first (span-acquiring)
  allocation with exactly 1 attempt / 1 injected / 0
  published, then recover; a warmed class then serves two
  objects with 0 further attempts.
- **Leg 3** (calloc): large product 4096 and small
  product 96 each fail once (1 injected, 0 published) and
  recover with the recovered extent verified zero.
- **Leg 4** (realloc, acquire failure): cases (a) to (e)
  of Section 2 FC-2, 1 injected failure each; same-size
  and shrink under an armed shim attempt **no** mapping
  (attempt delta 0) and the armed failure fires on the
  subsequent grow, as scripted.
- **Leg 5** (release failure): free rollback (retired
  delta 0 across the failed free, +1 on the retry);
  partial trim `trim_retained_some(1)` under a failed
  first release returns **1**, retired delta **1**,
  retained stays **1**; full trim under the same
  single-shot failure returns **2** (the failed span is
  retried on the walk's next pass, INC-I004-05);
  realloc transfer failure (published +1 / retired +1,
  original preserved); bound-0 span release failure
  (object freed, span retained, reuse with 0 attempts,
  next emptying free releases it). Release channel
  totals for the run: attempts **69**, injected **5**,
  real **0**.
- **Leg 6** (lock + page-size): 3 injected lock failures
  (malloc / free / realloc), each with 0 mapping attempts
  and the F-07 state preserved; 2 injected page-size
  failures (large and small), 0 mapping attempts, recovery
  immediate. Lock channel: attempts **237**, injected
  **3**, real **0**. Page-size channel: attempts **116**,
  injected **2**, real **0**.
- **Leg 7** (storm): 30 injected large-malloc failures
  interleaved with 6 large + 6 small successes and a
  mid-storm posture probe. Exact deltas: attempts **+42**
  (30 + 6 + 6), injected **+30**, real **+0**, mappings
  published/retired **+6/+6**, spans published **+6**,
  small alloc/free **+6/+6**; closing trim retires
  exactly **6** spans; Q-MAPPED and Q-RETAINED return to
  0; allocator fully usable afterwards.
- **Whole-suite standing:** `make test` exit **0** with
  the banked composition unchanged -- smoke 18/18,
  test_core 29/0, test_b007 69/0, test_c005 95/0,
  test_c006 23/0, test_c007 122/0 (its SKIPPED lines
  unchanged, Section 4), test_c008 42/0 with 7
  skipped-not-yet (unchanged), test_d004 4590/0;
  **4988 checks, 0 failed**, the I-001 baseline total.
  `nm -D --defined-only` = **4** (`mimir_malloc`,
  `mimir_free`, `mimir_calloc`, `mimir_realloc`); shim
  symbols in the `.so`: **0**.

## 4. The owed C-007 / C-008 injection cases, case by case

The banked `test_c007` / `test_c008` binaries are
**unchanged** by this slice and still print their
SKIPPED/NOT-YET lines (visible in the recorded run's
`make test` output: test_c007 122 run / 0 failed with its
registry verdicts as banked; test_c008 42 run / 0 failed,
7 skipped-not-yet). This slice **supersedes** the owed
cases with the runs below rather than rewriting banked
suites -- a rewritten suite could no longer be compared
against its own banked recorded runs, and the cases'
substance is what Phase 9's exit item X4 grades.

| Case | Disposition in I-004 |
|---|---|
| P3-T-FAIL-001 (F-01, injected mapping failure) | **RUN** -- Leg 1 at N=1 and every per-entry-point single failure in Legs 2-4 and 6. |
| P3-T-FAIL-002 (F-01 N-sweep) | **RUN** -- Leg 1, N = 1..4 over six mallocs each. |
| P3-T-FAIL-003 (F-01 recovery probe) | **RUN** -- a recovery allocation follows every injected failure in Legs 1-4 and 6, and closes Leg 7. |
| P3-T-FAIL-011 (F-02, rejection before any mapping attempt) | **RUN** -- Leg 0's posture/oversize set and Leg 6 make **zero** acquire attempts; the shim's attempt counter is exactly the instrument this case was missing. |
| P3-T-FAIL-021 (F-03, EMFILE/ENFILE class) | **RUN-BY-CLASS** -- L6 collapses every `mmap` failure to `ERR_MAP`, so the injected `ERR_MAP` is the exact failure the allocator can ever observe from that class; the errno flavour is unobservable by design (U-10), so no stronger run exists to owe. |
| P3-T-FAIL-031 (F-04, EAGAIN class) | **RUN-BY-CLASS** -- same `ERR_MAP` collapse as FAIL-021. |
| P3-T-FAIL-051 (F-06, release failure) | **RUN** -- Leg 5 (a) free, (b) trim, (c) realloc transfer, (d) span release. The C-005 inspection-only transfer branch (INC-C005-03) is now run-proven. |
| P3-T-FAIL-061 (F-07, lock failure) | **RUN** -- Leg 6 lock channel (malloc, free, realloc). |
| P3-T-FAIL-101 (F-11, discovery failure) | **RUN** -- Leg 6 page-size channel (large and small paths). |
| P3-T-FAIL-111 (F-12, reporting under failure) | **Still SKIPPED, with reason** -- there is no reporting path to instrument: L7 is compiled out and no error-report facility exists in the tree. This is an absent facility, not a shim gap. |
| P3-T-FAIL-041 (F-05, zero-size) | **Still BLOCKED-by-U-09** -- B-009 writes no pass criterion; not an injection case. Its posture is exercised in Leg 0 as FC-6 evidence, not as a case pass. |
| P3-T-FAIL-081 (diagnostic-only) | **Still SKIPPED, with reason** -- it requires a debug/internal-check build by its own definition; the shim is not one. |
| P3-T-ARITH-031 (metadata-allocation failure) | **SUPERSEDED-BY-CONSTRUCTION** -- this allocator performs no separate metadata allocation: the descriptor lives in the mapping's reserve page and the span header in the span's page (B-006 S7.1, G-002 S7), so there is no metadata call to fail; the mapping failure *is* the metadata failure and is run in Legs 1-4. |
| P3-T-ARITH-032 (partial descriptor) | **RUN-BY-CONSTRUCTION** -- Leg 1 shows the observable content of the case: after each injected failure nothing is published, counters are unchanged, and subsequent allocations behave exactly as before; no partial descriptor state exists in this design to inspect further. |
| P3-T-ARITH-042 (overflow before any mapping) | **RUN in its executable half** -- size+reserve overflow makes zero attempts (Leg 0); the alignment-parameter half still has no executable instance because no alignment entry point exists (INC-C007-05, unchanged). |
| S93-INJ-1 (fail-the-Nth sweep) | **RUN** -- Leg 1, the same instrument as FAIL-001/002. |
| S93-INJ-2 (metadata-allocation failure) | **SUPERSEDED-BY-CONSTRUCTION** -- as ARITH-031. |
| S93-INJ-3 (EMFILE/EAGAIN class) | **RUN-BY-CLASS** -- as FAIL-021/031. |

## 5. The budget-enforcement gap (INC-I001-14) -- not adopted

INC-I001-14 stands **exactly as recorded**, as it did
after I-003: this slice implements no budget scalar, no
setter/getter and no refusal path, and `grep -rni budget
src/` still finds only comments. FC-4's budget half is
therefore discharged in the only honest form available:
Q-REFUSED-BUDGET is **0 by absence**, every non-posture
`NULL` in the recorded run is OS-kind, and the shim's
counters account for each one exactly. A later slice
that implements the I-002 Section 3.3 procedure must
re-prove FC-4 with a non-zero budget counter of its own;
nothing here pre-decides that work.

## 6. Exit item X4 (I-001 grading) after this slice

X4 asked for: with the Nth mapping artificially failed,
every affected call returns exactly `NULL`, no live block
is corrupted or overlapped, no call hangs, a recovery
allocation succeeds, and realloc failure leaves the
original byte-for-byte intact -- across both allocation
paths and several N. On this slice's evidence X4's
substance is **MET**: the instrument exists (Section 1),
the sweep runs at N = 1..4 on the mapping path and at
N = 1 on every other construction (small malloc, calloc
both paths, realloc in all its moving forms), the
release-failure half of the failure contract is run for
the first time (Leg 5, closing INC-I003-03's "not
exercised" caveat and INC-C005-03's inspection-only
branch), and the counter separation X4's Phase 10
successors need is demonstrated exactly (Section 3).
The formal exit grading belongs to I-009's packet; the
honest caveats it must carry are INC-I004-02 (errno-class
collapse), INC-I004-03 (FAIL-111 / FAIL-081 still
unrun), and INC-I004-06 (the separation is proven with
test-linked shim counters, not with core counters, which
do not exist yet).

## 7. Inconsistencies and deviations (INC-I004-NN)

- **INC-I004-01 (instrument shape, recorded):** the shim
  exists only in a binary linked with the shim object
  and the `--wrap` flags. That is the least invasive
  road -- production code is untouched -- and its
  price: the banked `test_c007` / `test_c008` binaries
  do not gain the instrument retroactively, so their
  printed SKIPPED lines persist and Section 4's
  supersede-not-rewrite disposition is the slice's
  honest accounting. Any future binary that wants
  injection wires the same additive pattern.
- **INC-I004-02 (errno classes collapse):** FAIL-021 /
  FAIL-031 / S93-INJ-3 are run **by class**, not by
  errno: the platform layer maps every `mmap` failure
  to the single status `ERR_MAP`, so EMFILE, ENFILE,
  EAGAIN and ENOMEM are one allocator-visible failure.
  Distinguishing them would require L6 to promise
  errno, which U-10 expressly withholds. Closed as a
  design fact, not left as a silent gap.
- **INC-I004-03 (two cases remain unrun, with reasons):**
  FAIL-111 (no reporting facility exists to instrument)
  and FAIL-081 (diagnostic-only; needs a debug build).
  Neither is a mapping-boundary case, and neither is
  claimed anywhere in this slice's evidence.
- **INC-I004-04 (metadata cases have no instance):**
  ARITH-031 and S93-INJ-2 presuppose a metadata
  allocation separate from the mapping; this design
  has none (descriptor in the reserve page, header in
  the span page). Recorded as superseded-by-construction
  so no future reader re-opens them as shim debt.
- **INC-I004-05 (trim retry semantics, discovered):**
  I-002 S5.2.4 says a failed release leaves the span
  retained, is not counted, and the walk "continues with
  the next span". Leg 5b shows the full behaviour of
  the banked walk: continuation holds (a partial trim
  releases the next class's span in the same call and
  counts only successes), **and** a full trim retries
  the transiently failed span on its next pass, because
  the walk loops while any class progresses -- so under
  a transient failure a full trim releases it in the
  same call. Both halves are consistent with S5.2.4's
  count rule; the retry half was not documented in
  I-003 and is recorded here for I-009. No code change
  is proposed: retrying a transient failure inside one
  call is bounded (each pass must release a span to
  continue) and cannot hang.
- **INC-I004-06 (FC-4's core-counter form still owed):**
  Q-FAILED-OS and Q-REFUSED-BUDGET as **core** counters
  (I-002 Section 3.2, Proposed-internal) still do not
  exist; INC-I001-03's gap is closed for test-linked
  evidence by the shim's counters, not for the
  allocator's own telemetry. Adding core failure
  counters belongs with the telemetry work (I-007 /
  Phase 10), which owns the counter set as a whole.
- **INC-I004-07 (release-failure observability is
  internal posture):** Leg 5 reads a block's bytes after
  its free's release failed. That is legitimate
  evidence **inside** this implementation -- F-06
  rollback keeps the block recorded LIVE and caller-
  owned -- but it is not a public-contract observation:
  `free` returns void and INC-I001-05 (release failure
  is caller-invisible) stands unchanged for callers of
  the shared library. The injection proves the rollback;
  it does not create a diagnostic channel.
- **INC-I004-08 (single-threaded, GNU-ld scope):** all
  injection evidence is single-threaded, like I-003's:
  arming is a countdown consumed by whichever thread
  calls next, so a multi-threaded injection schedule
  would be racy by construction and none is claimed.
  The `--wrap` mechanism is a GNU linker feature,
  proven here on this Pi's toolchain only; the carried
  x86-64 debt (INC-C007-02) now additionally includes
  re-proving the wrapped link there.
- **Defects found: none.** Every leg passed on the
  first recorded run (248/0), no allocator behaviour
  contradicted FC-1..FC-6, and the I-001 Section 2.5
  code reading is now run-proven rather than merely
  read. No `src/` change arises from this slice.

## 8. Files in this slice

- `tests/core/mimir_fault_shim_i004.c` -- the shim:
  four `--wrap` interpositions on the L6 platform
  entries with per-channel attempt / injected /
  real-failure counters and single-shot arming.
- `tests/core/mimir_fault_i004.h` -- test-only
  declarations of the shim's counters and arming
  controls (shape (b)).
- `tests/core/test_i004.c` -- evidence test, 248 checks
  (Legs 0-7, final reconciliation, case ledger).
- `tests/core/recorded_run_i004.txt` -- recorded run:
  environment, integrity hashes, full `make test`,
  `make test-i004` under `timeout 120`, `make lib` +
  `nm -D` proof, Samtal 200 before/after.
- `Makefile` -- additive `test-i004` target only
  (shim object, test object, wrapped link).
- This document.

Untouched, verified byte-identical to base in the
recorded run: `README.md`, `include/mimir_alloc.h`, and
every file under `src/`. No push and no merge by this
team; the keeper banks.
