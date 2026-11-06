# Phase 3 — Exit Evidence Packet

Slice: **C-009 (team-09)**. Packet base: **94e1a8d** (banked
main: C-001..C-007; Wave 3 = 7 of 9). Host for every run
cited: Raspberry Pi `purpleviking1` (device name
purpleviking1), ARM64 Linux + glibc — `Linux purpleviking1
6.12.112-v8-16k-hailo+ #1 SMP PREEMPT Sat Oct 10 09:05:25
EDT 2026 aarch64 GNU/Linux`, runtime-discovered system
page size **16384** (`getconf PAGESIZE`), compiler
`cc (Debian 12.2.0-14+deb12u1) 12.2.0`. This packet is an
evidence assembly only: no implementation, test, or
Makefile change is made by this slice.

> **EXIT VERDICT — Phase 3 exit is UNMET.**
> The roadmap Phase 3 exit sentence (Section 8, Phase 3)
> is, verbatim: *"portable unit/contract suite passes for
> explicitly supported calls on x86-64 and ARM64 Linux.
> Document unsupported behavior."* Two required things do
> not exist at this packet's base:
>
> 1. **The x86-64 run of the portable suite is OWED in
>    full.** No x86-64 host exists in this campaign
>    (B-009 S3.4). Every PASS record in this packet is an
>    ARM64-only, per-target record; no ARM64 result is
>    presented as covering x86-64.
> 2. **C-008 is OWED.** Unsupported-behavior enforcement
>    plus the Roadmap S9.3 controlled-failure tests are
>    slice C-008, being rebuilt in parallel; C-008 is NOT
>    in this packet's base (94e1a8d) and none of its
>    evidence is claimed here. Where a criterion below
>    depends on C-008, its verdict is OWED (C-008).
>
> Phase 3 exit stays honestly **UNMET** until both targets
> pass **and** C-008 lands. The keeper reconciles this
> packet against C-008's banked evidence at C-009 banking
> time.

## 1. Verdict vocabulary and reading rules

- **MET** — the evidence the criterion specifies exists,
  in the form it specifies, in the banked repo (or, for
  packet-internal record items, in this packet as shipped).
- **UNMET** — some evidence exists, but the criterion as
  written is not satisfied (a required target, artefact,
  or half of the criterion is missing).
- **OWED** — the evidence is wholly absent at this base;
  the row names who owes it.
- Every B-009 S10 item requires **both** exit targets
  unless it is a package/record item. An item whose ARM64
  leg is held but whose x86-64 leg is owed is therefore
  UNMET, never MET; the ARM64 column says exactly what is
  held so the partial state is not hidden.
- Contract mappings for the namespaced `mimir_*` surface
  are **by analogy only, under U-03** (the surface has no
  direct rows in `docs/contracts.md`), exactly as in
  C-004/C-006/C-007. Nothing here is an ABI claim.
- Counts are copied from the recorded-run files named in
  each row, never restated from memory or from another
  slice's summary.

## 2. Supported-call list (stated before any pass claim)

Per B-006 S13 item 1, as amended by B-008 S7.2 (the
reconciliation recorded as INC-B009-10), the Phase 3
supported list — the list every claim in this packet is
read against — is:

- `mimir_malloc(size)` with size > 0 and representable
  (by analogy MA-6.1-MALLOC-01 / -02 / -04).
- `mimir_free(ptr)` of a live in-scope pointer, exactly
  once, or `mimir_free(NULL)` as a defined no-op
  (by analogy MA-6.1-FREE-01 / -02, MA-6.2-010).
- `mimir_calloc(nmemb, size)` with both factors > 0:
  checked product, requested extent zero at return
  (by analogy MA-6.1-CALLOC-01 / -02 / -03 / -05).
- `mimir_realloc(ptr, new_size)` with new_size > 0,
  including `mimir_realloc(NULL, n)` acting as
  `mimir_malloc(n)`; on failure the original block stays
  valid, unchanged, and caller-owned (by analogy
  MA-6.1-REALLOC-01 / -02 / -03 / -06).

Zero-size is **not** on the list (`mimir_malloc(0)`, a
calloc zero factor, `mimir_realloc(p, 0)`:
[UNRESOLVED: U-09 / U-12]; the implementation's null
returns there are posture only, recorded by C-005, and
settle nothing). Nothing else is supported: no
explicit-alignment entry point, no `malloc_usable_size`,
no `reallocarray`, no statistics/trim surface, no
standard-name interposition (see Section 6).

## 3. Fresh run for this packet (C-009, this Pi, this base)

Command, run once on the Pi in `/home/pi/mimiralloc-team-09`
at base 94e1a8d on 2026-11-06: `make clean && make test`.
Exit code **0**. Counts exactly as logged:

| Suite | Result as logged |
|---|---|
| smoke (`tests/contract/test_smoke.c`) | `SMOKE 18/18 checks passed` |
| core (`tests/core/test_core.c`, C-003) | `CHECKS: 29 run, 0 failed` — `RESULT: PASS` |
| B-007 (`tests/core/test_b007.c`, C-004) | `CHECKS: 70 run, 0 failed` — `RESULT: PASS` |
| C-005 (`tests/core/test_c005.c`) | `CHECKS: 95 run, 0 failed` — `RESULT: PASS` |
| C-006 (`tests/core/test_c006.c`) | `CHECKS: 23 run, 0 failed` — `RESULT: PASS` |
| C-007 (`tests/contract/test_c007.c`) | `SUITE cases: 40 passed, 0 failed, 70 skipped, 9 inspected (of 119 registered)`; `CHECKS: 122 run, 0 failed` — `RESULT: PASS` |

Raw log of this fresh run: `/tmp/c009_fresh_run.txt` on
the Pi at run time (525 lines, sha256
`89576ae61d1d989af6ab1044c1e4bf4305233618d35df1202945721131c350cc`);
it is a run record, not a repo artefact — the counts
above are the banked record of it. The fresh counts are
identical to the banked C-007 recorded run's counts for
the five suites that run shares, and identical to the
keeper's verification run on banked main at 94e1a8d
(WAVE_PLAN.md, C-007 banking entry), which is the first
recorded run containing all six suites on one tree (see
INC-C009-01). The standalone C-002 platform test is not
part of `make test` (INC-C009-05); its evidence is its
own recorded run (Section 4).

## 4. Banked evidence inventory (C-001..C-007)

All commits are banked main commits; file names are the
banked paths. Counts are as logged in the named file.

| Slice | Banked commit | Evidence files | Recorded result (as logged) |
|---|---|---|---|
| C-001 foundation: public header, stub, smoke test | 1beb7ac | `include/mimir_alloc.h`, `src/api/mimir_alloc.c` (stub), `tests/contract/test_smoke.c`, `Makefile` | Smoke 9/9 PASS and `nm` showing only `mimir_malloc`/`mimir_free` exported — recorded in the keeper's WAVE_PLAN.md banking entry; **no in-repo recorded-run file exists for C-001** (INC-C009-04). Superseded as a run by every later slice's smoke. |
| C-002 platform layer (L6) | b6d51e9 | `tests/platform/recorded_run.txt`, `tests/platform/test_platform.c`, `tests/platform/NOTES.md` | `RESULT: PASS`, exit 0. Runtime page size 16384; `acquire(0)` rejected ERR_INVALID, `acquire(SIZE_MAX)` rejected ERR_OVERFLOW before any OS call; lock-exercised counter **20000 (expect 20000)** exact under the global lock. Standalone run, not wired into `make test`. |
| C-003 mapping-backed core | 72e52e1 | `tests/core/recorded_run.txt`, `tests/core/test_core.c`, `tests/core/NOTES.md` | Smoke **11/11**; core `CHECKS: 29 run, 0 failed`, `RESULT: PASS`; make test exit 0; sizes 1..49275 pattern-verified, pairwise non-overlap, `SIZE_MAX` refused, `mimir_malloc(0)` NULL posture only (U-09 not settled); `nm` exactly the two C-001 symbols. |
| C-004 B-007 completeness audit + gap closure (A9/A10) | 9b36f25 | `tests/core/recorded_run_c004.txt`, `tests/core/AUDIT_B007_C004.md` (270 lines), `tests/core/test_b007.c` | Smoke 11/11; core 29/0; B-007 audit test `CHECKS: 70 run, 0 failed`, `RESULT: PASS`; exit 0. Audit verdict: A9 (explicit pointer-range check) and A10 (`live_mappings` check) were the only B-007 gaps in C-003 scope; both closed in `src/core/mimir_core.c`. INC-C004-01..05 recorded. |
| C-005 calloc/realloc per B-008 | 56eb4dc | `tests/core/recorded_run_c005.txt`, `tests/core/test_c005.c`, `tests/core/NOTES_C005.md` | Smoke **18/18**; core 29/0; B-007 70/0; C-005 `CHECKS: 95 run, 0 failed`, `RESULT: PASS`; exit 0; `nm` exports exactly the four B-008 S7.2 symbols (`mimir_malloc`, `mimir_free`, `mimir_calloc`, `mimir_realloc`). INC-C005-01..06 recorded (zero-size postures are posture only; U-09/U-12 not settled). |
| C-006 S6.3 startup/init + error-path audit | e846614 | `tests/core/recorded_run_c006.txt`, `tests/core/AUDIT_6_3_C006.md` (259 lines), `tests/core/test_c006.c` | Smoke 18/18; core 29/29; B-007 70/70; C-005 95/95; C-006 `CHECKS: 23 run, 0 failed`, `RESULT: PASS`; `MAKE_EXIT: 0`. Audit verdict: **no implementation gap** in S6.3 Phase 3 scope — init is allocation-free by collapse (no init step; static zero state + statically initialised lock); zero `src/` or header changes. INC-C006-01..04 recorded. |
| C-007 portable suite per B-009 | 94e1a8d | `tests/contract/recorded_run_c007.txt` (781 lines), `tests/contract/test_c007.c`, `tests/contract/NOTES_C007.md` | `make test` exit 0 at its base (56eb4dc + C-007 wiring): smoke 18/18, core 29/0, B-007 70/0, C-005 95/0 (no C-006 suite in that run — INC-C009-01). C-007 suite: **40 PASS / 0 FAIL / 70 SKIPPED / 9 INSPECTED of 119** registered B-009 cases; `CHECKS: 122 run, 0 failed`; `RESULT: PASS`; the file itself closes `x86-64 run of this suite: OWED (B-009 S3.4); Phase 3 exit: UNMET`. INC-C007-01..05 recorded; no allocator defects found. |
| C-008 enforcement + S9.3 | — | — | **OWED.** Not in this base; being rebuilt in parallel. Nothing claimed. |
| C-009 this packet | (this slice) | `docs/PHASE_3_EXIT_EVIDENCE.md` | Fresh run in Section 3. |

## 5. Criterion-by-criterion map

### 5.1 Roadmap Phase 3 build criteria (Roadmap S8, Phase 3 text)

| # | Criterion (roadmap wording, in substance) | Banked evidence | Verdict |
|---|---|---|---|
| R1 | Namespaced allocation/free using OS mappings and metadata under a simple lock | C-002 platform layer (runtime page size, checked mmap/munmap wrappers, global lock; `tests/platform/recorded_run.txt`, lock counter exact 20000) + C-003 mapping-backed core behind the L1 boundary (`tests/core/recorded_run.txt`, 29/0 PASS) | **MET on ARM64** (both recorded runs on this Pi); counts toward exit only via item 3 below, which is UNMET |
| R2 | Checked allocation arithmetic, alignment, clean initialization, errors, and tests delivered | C-004 audit + closures + 70/0 B-007 run (`recorded_run_c004.txt`); C-006 S6.3 audit + 23/0 run (`recorded_run_c006.txt`); alignment is the non-numeric suitably-aligned claim only (U-15 open) | **MET on ARM64** in the B-007/B-009 scope, with the inspection-only halves named in the audits (INC-C004-02, INC-C006-03) |
| R3 | calloc and realloc added only with precise semantics and ownership | C-005 per B-008: checked calloc with usable-extent zeroing, failure-atomic realloc (`recorded_run_c005.txt`, 95/0 PASS; header declarations in `include/mimir_alloc.h`) | **MET on ARM64**; zero-size forms stay unresolved (U-09/U-12), posture only |
| R4 | A slow mapping-per-allocation prototype is acceptable only as an explicitly experimental correctness baseline; not shipped as a performance allocator | C-003 `tests/core/NOTES.md` and B-006 record the mapping-per-allocation design as exactly that baseline; no performance claim exists anywhere in the banked evidence; B-009 UNS-091 (INSPECTED) confirms no timing value is an oracle in the suite | **MET** (as a record: the baseline is labelled, and no performance claim is made) |
| R5 | **Exit sentence:** portable unit/contract suite passes for explicitly supported calls on x86-64 **and** ARM64 Linux; unsupported behavior documented | ARM64 leg: Section 3 + Section 4. x86-64 leg: **OWED in full**. Unsupported documentation: Section 6 (record held); enforcement: **OWED (C-008)** | **UNMET** |

### 5.2 B-009 S10 exit items 1–20 (extending B-006 S13 items 1–10)

B-006 S13 items 1–10 are restated as B-009 S10 items
1–10; items 11–20 are B-009's extensions. This table is
the governing map (see INC-C009-02).

| # | Exit item | ARM64 (Pi) evidence held | Exit verdict |
|---|---|---|---|
| 1 | Supported-call list stated before any pass claim | The list is stated in B-006 S13 item 1 as amended by B-008 S7.2, in `include/mimir_alloc.h`, and in Section 2 of this packet, before every count in Sections 3–4 | **MET** (record item; the list both targets' claims are read against) |
| 2 | Test-hook evidence with hook-to-row mapping shown (C-005, C-009, C-010 + MA-MALLOC/MA-FREE named hooks; read with items 12/19 for C-001..C-004) | Partial: C-009 hook **is** evidenced by C-007 PASS cases P3-T-ERR-211/-212 (MA-6.2-054); CAL/REA cases cite hooks C-001..C-004 (e.g. REA-001 cites C-004, REA-011 cites C-003, in `recorded_run_c007.txt`); C-010 watchdog evidence is PRELIMINARY/BLOCKED-by-U-05 (C-006 audit, BY DESIGN); injection-dependent hook evidence does not exist (no shim, INC-C007-01) | **UNMET** |
| 3 | Passing on BOTH named targets | ARM64 leg held (Sections 3–4). x86-64 leg: no host, no run, no record | **UNMET** — x86-64 **OWED** in full |
| 4 | Data-integrity cycles returning to baseline | Behavioural half held on ARM64: core test repeated pattern/non-overlap cycles (29/0); C-007 DATA-001/-002/-011/-012/-021/-022 PASS. Counters/registry half is **not observable** through the public API (INC-C007-04) and rests on C-003/C-004 inspection, not on a counter record | **UNMET** (ARM64 behavioural evidence held; formal counter evidence and the x86-64 leg missing) |
| 5 | Fail-the-Nth-mapping sweep plus recovery (Roadmap S9.3) | No fault-injection shim exists in the banked base (INC-C007-01); FAIL-001/-002/-003 SKIPPED in the C-007 run. Public-API failure+recovery substance is held instead: MAP-011 (unsatisfiable → null) and MAP-012 (sentinel unchanged, next allocation normal) PASS; oversize refusals PASS in core/B-007/C-005/C-006 runs | **OWED (C-008)** — the swept-N injection set is C-008/S9.3 scope; the substitute public-API evidence above does not discharge it |
| 6 | Minimal concurrency correctness (incl. cross-thread handoff, hook C-009) | Held on ARM64: C-007 ERR-211 PASS (50 cycles, alloc on thread A, verified and freed on thread B) and ERR-212 PASS (churn, 4 rounds × 4 threads created/destroyed with transfers, each block retired exactly once); C-002 lock counter exact 20000; C-006 race test PASS (8 threads race first entry, 150 cycles each, in the 23/0 run) | **UNMET** (ARM64 leg held in full; x86-64 leg owed) |
| 7 | Per-environment system-page-size records | Pi record held: 16384, runtime-discovered (`sysconf`), failure-checked, recorded in every recorded run in Section 4 and in Section 3 — for **this Pi only** | **UNMET** — x86-64 environment record **OWED** |
| 8 | Clean C11 / D-006 builds per target | ARM64 held: every recorded run builds clean under `-std=c11 -Wall -Wextra -Werror` with compiler and flags logged, including the fresh run (Section 3) | **UNMET** (x86-64 build evidence owed) |
| 9 | NOT-done list shipped inside the exit package | Section 7 of this packet ships the deferred/unsupported list in substance; the verbatim source is B-006 S12 (`docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md`) | **MET** (packet-internal record item) |
| 10 | Documented unsupported behaviour | The written record is held: B-008 S6 (`docs/PHASE_3_CALLOC_REALLOC_AND_UNSUPPORTED_BEHAVIOR.md`), B-009 S8.4, the header's absence record, summarised in Section 6. **Enforcement** of that record is a separate thing and is **OWED (C-008)** | **MET** (the documentation the exit sentence requires); enforcement OWED under item 5 / Section 5.3 |
| 11 | This plan itself, frozen, with its registry | Held: B-009 plan banked at 2baab43 with the 119-case Appendix A registry; the C-007 run and `NOTES_C007.md` cite that registry case-by-case | **MET** (record item) |
| 12 | Contract and S10 completeness audits | Partial: C-004 (B-007 completeness over the C-003 core) and C-006 (S6.3) audits are banked; C-007 `NOTES_C007.md` accounts for all 119 registry cases one by one. No single B-009 S9.3 completeness artefact over all contracts SS2–4 rows exists, and no audit covers an x86-64 package | **UNMET** |
| 13 | S10 completeness artefact at exit time | B-009 S6 maps the 10 Roadmap S10 rows as a **design**; at exit time several rows stand NOT-YET-MAPPABLE with reasons (Memory: trim/cache-drain have no Phase 3 operation; ABI/linking: C++ set undecided, U-06; Tooling: no sanitizer/Valgrind run is recorded as exit evidence; Platform: Android/Termux/proot are not Phase 3 exit targets) | **UNMET** |
| 14 | Both per-target annexes, completed | Neither annex exists: B-009 Appendix B is a template only; SYS-002 SKIPPED for exactly this reason (INC-C007-03). V-01..V-07 and U-01/U-02/U-09..U-15 per-target dispositions are undecided | **OWED** |
| 15 | Differential report over defined behaviour only | No x86-64 pair exists; DIFF-001/-002 SKIPPED in the C-007 run | **OWED** (depends on the x86-64 run) |
| 16 | Fuzzer seed/trace evidence | No fuzzer exists in the banked base (INC-C007-01; UNS-041 INSPECTED records its absence) | **OWED** |
| 17 | C-010 watchdog status carried, never omitted | Carried here and in the C-006 audit: hook C-010 is **BLOCKED-by-U-05** (no watchdog bound/procedure selected for this target), every dependent result is labelled **PRELIMINARY**, and no C-010 pass is claimed by any banked slice | **MET** (the carrying duty this item imposes); the watchdog evidence itself remains OWED under U-05 |
| 18 | Regression register (reproducer per confirmed defect) | No register artefact exists in the repo. Factual basis it must record: C-004 found and closed two design-completeness gaps (A9/A10) with tests added in-slice; C-005, C-006 and C-007 each record **no allocator defects found** in their runs | **OWED** (the register artefact itself) |
| 19 | B-007/B-008 reconciliation record | Held: B-009 was reconciled against the final banked B-007/B-008 at its banking — two drifts found and recorded in B-009 S12 as INC-B009-10 (the B-008 S7.2 supported-list amendment, and the B-007 INC-B007-1 formula discrepancy, carried as INC-B009-11); C-007 then implemented against the amended list | **MET** (record item; shipped here by citation) |
| 20 | Cannot-prove-yet shipped inside the exit package | Section 8 of this packet ships the updated cannot-prove-yet list inside the package | **MET** (packet-internal record item) |

**Tally (B-009 S10): MET 7 / UNMET 9 / OWED 4, of 20**
(items 1, 9, 10, 11, 17, 19, 20 met — all of them record
items; every item that requires a run on both targets is
unmet or owed). No tally changes the verdict: **Phase 3
exit is UNMET.**

### 5.3 Roadmap S9.3 controlled-failure tests (C-008 scope)

| S9.3 line | Evidence at this base | Verdict |
|---|---|---|
| Artificially fail the Nth mapping or metadata allocation | No shim (INC-C007-01); FAIL/ARITH injection cases SKIPPED | **OWED (C-008)** |
| Enforce low process limits; simulate fragmented address space | No recorded run in the banked repo | **OWED (C-008)** |
| Kill/restart a process during application-level state writes | No recorded evidence; application-level persistence is not a Phase 3 allocator surface | **UNMET** (no evidence; not claimed) |
| Allocate near integer and alignment boundaries; overflow rejected | Held on ARM64: B-007 test 70/0 (SIZE_MAX, SIZE_MAX−8, SIZE_MAX−page+1, page−1/page/page+1 boundary read-backs); C-007 ARITH cases PASS | **MET on ARM64** (exit still gated by item 3) |
| Repeatedly create/destroy threads while transferring objects | Held on ARM64: ERR-212 PASS (see item 6) | **MET on ARM64** (exit still gated by item 3) |
| Process initialization, shared-library loading, teardown order | Init held on ARM64 (C-006 audit + 23/0 run; first-call forms PROVED, pre-`main` form INSPECTED, INC-C006-03). Shared-library loading/teardown order: no recorded test | **UNMET** (partial) |
| Failures neither hang indefinitely nor return overlapping valid blocks | Behavioural evidence on ARM64: C-006 60-round failure/success storm and 500 consecutive failures, allocator healthy after, no leaked lock (23/0 run); non-overlap proven by core/DATA cases. The hang half rests on those bounded runs, not on a watchdog (U-05 open) | **UNMET** (strong ARM64 behavioural evidence; no watchdog-bounded proof) |
| Results tracked separately for normal Linux, Android, proot | Only ARM64 Linux + glibc results exist; Android/proot are Phase 4 targets (INC-C009-06) | **UNMET** |

## 6. Unsupported behaviour — the documented record (summary)

The full record is B-008 S6 + B-009 S8.4; this is the
packet's shipped summary (exit item 10). **Documented
is not enforced: enforcement is OWED (C-008).**

- **Not present (absence at link time, never a runtime
  refusal, never a silent fallback):** every
  explicit-alignment entry point; `malloc_usable_size`;
  `reallocarray`; statistics/trim entry points; any
  standard-name `malloc`/`free` replacement or
  interposition. The banked surface exports exactly the
  four Section 2 symbols (C-005/C-007 `nm` records;
  C-007 UNS-031 INSPECTED).
- **Undefined behaviour, no refusal promised:** freeing
  or reallocating a foreign, interior, or already-freed
  pointer; use after free (B-008 S6.2).
- **Unresolved, must not be relied on:** zero-size
  (U-09), `realloc(p, 0)` (U-12), `errno` (U-10),
  numeric alignment (U-15), explicit-alignment validity
  (U-11), fork behaviour (U-17..U-21), async-signal
  safety (no claim stated or implied).
- **Defined refusals (the complete list):** oversize /
  unrepresentable requests and unsatisfiable requests
  return exactly null, with no partial block and no
  ownership passing (evidenced in Section 4 runs).

## 7. NOT-done list (shipped with this packet; source: B-006 S12)

Deferred to B-007 and since discharged only as recorded
in Section 4: checked-arithmetic and alignment detail
(C-004), init/error detail (C-006). Still not done, not
implemented, and not claimed by Phase 3: interposition
and any ABI/stability promise (Phase 5); size classes,
splitting, coalescing, reuse, trim (Phase 7); caches and
sharding (Phase 8); budgets and pressure behaviour
(Phase 9); statistics/diagnostics surface (Phase 10);
rseq (Phase 11); fork/atfork behaviour (U-17..U-21
open); Android/bionic/musl targets; any performance
claim of any kind. The C++ operator set is undecided
(U-06) and out of Phase 3 scope.

## 8. Cannot-prove-yet (updated at this packet; source: B-009 S11)

- **x86-64:** everything — builds, PASS records, annex,
  page-size record, differential pairing. OWED in full.
- **C-008:** unsupported-behavior enforcement and the
  S9.3 injection set. OWED; this packet claims none of it.
- **Fault injection, fuzzer, evidence collector,
  watchdog:** none of these harness components exists in
  the banked base (INC-C007-01); every case needing one
  is SKIPPED, not passed.
- **Per-target decisions:** U-01, U-02, U-09..U-15 (and
  U-03, U-04, U-05, U-06, U-16, U-17..U-21 where they
  gate cases) are open; dependent cases stay BLOCKED.
- **Counters/registry baseline:** not observable through
  the public API (INC-C007-04); behavioural evidence is
  not a counter measurement.
- **Sanitizer/Valgrind:** no tool run is recorded as
  Phase 3 exit evidence; a tool run would not be
  coverage of the allocator's own mappings in any case.
- **Phase 4+ matters:** non-glibc targets, interposition,
  size classes, caches, budgets, performance — nothing
  here proves or claims anything about them.

## 9. Inconsistencies recorded by this slice (INC-C009-*)

1. **INC-C009-01 — THE BANKED C-007 RUN PREDATES C-006.**
   `tests/contract/recorded_run_c007.txt` was recorded at
   base 56eb4dc, before C-006 banked, so it contains no
   C-006 suite; the keeper's verification run on banked
   main at 94e1a8d (WAVE_PLAN.md) and this packet's fresh
   run (Section 3) are the first recorded runs containing
   all six suites on one tree. A reader of the C-007 file
   alone could miss the seam. No count anywhere differs.
2. **INC-C009-02 — TWO EXIT LISTS EXIST.** B-006 S13
   states 10 exit items; B-009 S10 states 20 (the same 10
   plus extensions defined there, answering INC-B009-4/5:
   S13 under-scopes the exit). This packet maps the
   20-item list as governing and cross-references S13;
   the discrepancy is B-009's recorded finding, not
   silently resolved here.
3. **INC-C009-03 — TWO REQUIRED ARTEFACTS EXIST
   NOWHERE.** No completed per-target annex (item 14)
   and no regression register (item 18) exist anywhere
   in the banked repo — both are OWED as artefacts, not
   merely unrun.
4. **INC-C009-04 — C-001 HAS NO IN-REPO RECORDED RUN.**
   Its 9/9 smoke evidence lives only in the keeper's
   WAVE_PLAN banking record. Every later slice's smoke
   run supersedes it in-repo, so no exit item depends on
   the missing file; recorded so the inventory in
   Section 4 does not imply a file that is not there.
5. **INC-C009-05 — THE PLATFORM SUITE IS NOT IN
   `make test`.** A green `make test` (including Section
   3's fresh run) says nothing about `tests/platform/`
   in the same run; C-002's evidence is solely its own
   standalone `recorded_run.txt`.
6. **INC-C009-06 — S9.3 ASKS FOR ANDROID/PROOT
   TRACKING.** Roadmap S9.3's last line tracks results
   separately for normal Linux, Android and proot, but
   Phase 3's exit targets are x86-64 + ARM64 Linux only
   (B-009 S3.1) and Android is a Phase 4 target. Recorded
   as UNMET in Section 5.3, not silently dropped and not
   claimed.

## 10. Bottom line

On ARM64 Linux on this Pi, the Phase 3 implementation
line C-001..C-007 is built, audited, and green: the fresh
run for this packet passes all six suites with the exact
counts in Section 3, and no allocator defect was found
by any banked slice. That is one target of two, with the
C-007 suite itself standing at 40 PASS / 70 SKIPPED /
9 INSPECTED of its 119 registered cases on that target.

**Phase 3 exit is UNMET.** It becomes claimable only
when (a) the portable suite has run and passed on an
x86-64 Linux target with its own recorded evidence,
(b) C-008's enforcement and S9.3 controlled-failure
evidence has banked and been reconciled into this
packet, and (c) the OWED items above (14, 15, 16, 18,
and the injection half of 5) have artefacts. Until then
no subset of this packet — and no single-target result —
may be presented as a Phase 3 exit.
