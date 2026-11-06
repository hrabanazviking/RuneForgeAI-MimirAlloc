# C-007 notes — portable unit/contract test suite per B-009:
# implementation record, per-case verdicts, and inconsistencies

Slice C-007 implements the portable unit/contract test suite
designed in `docs/PHASE_3_TEST_SUITE_PLAN.md` (B-009) for the
banked public API — `mimir_malloc`, `mimir_free`,
`mimir_calloc`, `mimir_realloc` — exactly as banked at base
56eb4dc (C-001 header, C-002 platform, C-003 core, C-004
B-007 audit, C-005 calloc/realloc). This slice builds ONLY on
that base; it does not depend on C-006 (startup/init), which
banks after it. Evidence: `tests/contract/test_c007.c`, run
on the Pi (ARM64) via `make test` and standalone via
`make test-c007` (see `tests/contract/recorded_run_c007.txt`).
Existing smoke (18/18), core (29 checks), B-007 (70 checks)
and C-005 (95 checks) suites are unchanged in expectation and
stay green; the new suite adds 122 checks of its own, 0 failed.

**x86-64 run remains OWED.** This suite was run only on the
Pi (ARM64 Linux + glibc). No x86-64 host exists in this
campaign (B-009 S3.4); no result here may be worded as
covering x86-64, the differential cases (P3-T-DIFF-001/-002)
have no pair, and Phase 3 exit stays UNMET until both
targets pass. The Phase 3 exit packet itself is C-009, not
this slice. No unsupported-behaviour enforcement was added
(C-008 owns it) and no new public API was added.

Implementation shape:

- One portable C11 programme, `tests/contract/test_c007.c`,
  including ONLY the public header `include/mimir_alloc.h`
  plus C11/POSIX headers present on both exit targets
  (pthread, fork/wait, sysconf). No internal header, no
  ARM64-only assumption, and no literal system page size
  anywhere in the source: every page quantity is derived
  from the runtime-discovered system page size
  (`sysconf(_SC_PAGESIZE)`, failure-checked, B-009 S3.2).
- The programme carries the full B-009 Appendix A registry
  (119 case IDs) as data, reports each registered case
  exactly once as PASS (run on the Pi in this run), SKIPPED
  (not run, reason printed in the run and recorded below),
  or INSPECTED (audit case discharged by the source/document
  inspection worksheets in item 6 below, never by a runtime
  claim), and exits 0 only if no run case failed.
- Makefile wiring is additive, following the existing test
  patterns: `test_c007` joins `all` and `make test`, and a
  standalone `make test-c007` target runs the new suite
  alone. No existing rule was changed.
- Oracle discipline (B-009 S5, S7.3): every contract
  citation is BY ANALOGY under U-03; errno is never
  asserted (U-10); no numeric alignment value is asserted —
  only suitability for `max_align_t`, the non-numeric
  fundamental-alignment claim (U-15); pointer addresses are
  never oracles (the single REA-002 exception is item 5
  below); no timing is an oracle; zero-size postures are
  never exercised as contract cases (CAL-031 / REA-041 /
  FAIL-041 stay BLOCKED, exactly as B-009 writes them).

Run summary (recorded run, this Pi, ARM64): cases 40 PASS /
0 FAIL / 70 SKIPPED / 9 INSPECTED of 119 registered; checks
122 run, 0 failed; suite RESULT: PASS.

## Per-case verdicts — case -> B-009 section -> verdict

Verdict key: **PASS (run on Pi)** = the case ran in the
recorded run and its B-009 pass criterion was satisfied.
**SKIPPED** = not run; the reason is the one printed in the
recorded run, shortened here. **INSPECTED** = audit case
discharged by inspection (worksheets in item 6).

### SYS / DIFF (B-009 S4, S5, S6, S9)

| Case | B-009 section | Verdict |
|---|---|---|
| P3-T-SYS-001 | S4.2 | PASS (run on Pi) — registry loads; all 119 IDs well-formed and unique under the frozen scheme |
| P3-T-SYS-002 | S4.4 | SKIPPED — no completed per-target annex exists (Appendix B is a template only, INC-C007-03); absent annex decision leaves the case BLOCKED on this target |
| P3-T-SYS-003 | S9.2, S4.3f | PASS (run on Pi) — schema-validation unit: a record missing the Seed or the Result mandatory field is rejected, never counted as PASS |
| P3-T-DIFF-001 | S5 | SKIPPED — x86-64 leg OWED in full (S3.4); no paired record exists |
| P3-T-DIFF-002 | S5 | SKIPPED — x86-64 leg OWED; no pair, so no divergence judgement can be made |

### MAP / FAIL / DATA (B-009 S8.1)

| Case | B-009 section | Verdict |
|---|---|---|
| P3-T-MAP-001 | S8.1.1 (MA-6.1-MALLOC-01) | PASS (run on Pi) — non-null returns; fundamental alignment asserted non-numerically |
| P3-T-MAP-002 | S8.1.1 (MALLOC-01) | PASS (run on Pi) — live blocks pairwise distinct and non-overlapping over full extents |
| P3-T-MAP-003 | S8.1.1 (MALLOC-01) | PASS (run on Pi) — every requested byte writable/readable, sizes derived from the runtime system page size |
| P3-T-MAP-011 | S8.1.1 (MALLOC-02) | PASS (run on Pi) — representable unsatisfiable request (SIZE_MAX/2) returns null; failure induced without leaving the supported list, per the C-005 public-API precedent (INC-C005-03) |
| P3-T-MAP-012 | S8.1.1 (MALLOC-02) | PASS (run on Pi) — after failure, live sentinel unchanged and the next ordinary allocation behaves as MAP-001. Counters/registry baseline is observable only as behaviour (INC-C007-04) |
| P3-T-MAP-021 | S8.1.1 (MALLOC-04) | PASS (run on Pi) — SIZE_MAX and SIZE_MAX-8 refused with null, no ownership passes (return-value aspect; the no-syscall aspect is FAIL-011, SKIPPED) |
| P3-T-MAP-031 | S8.1.1 (FREE-01) | PASS (run on Pi) — free completes, block retired exactly once (single free call), storage eligible for reuse (fresh allocation succeeds; no address is asserted) |
| P3-T-MAP-032 | S8.1.1 (FREE-01) | PASS (run on Pi) — mixed sequence: freeing each block exactly once disturbs no other live block's bytes |
| P3-T-MAP-041 | S8.1.1 (FREE-02 / MA-6.2-010, hook C-005) | PASS (run on Pi) — free(NULL) is a no-op returning normally; later allocations behave normally |
| P3-T-MAP-042 | S8.1.1 (FREE-02 / MA-6.2-010, hook C-005) | PASS (run on Pi) — repeated/interleaved free(NULL), including before any allocation in the process (this case runs first among runtime cases), changes nothing observable |
| P3-T-MAP-051 | S8.1.1 (MA-6.2-001) | PASS (run on Pi) — block released by the compatible family exactly once |
| P3-T-FAIL-001 | S8.1.2 (F-01) | SKIPPED — needs the fault-injection shim on the L6 mapping boundary; none exists in the banked base (INC-C007-01). The failure+recovery substance available via the public API is run as MAP-011/MAP-012 |
| P3-T-FAIL-002 | S8.1.2 (F-01 sweep) | SKIPPED — N-sweep needs the shim (INC-C007-01) |
| P3-T-FAIL-003 | S8.1.2 (F-01 recovery probe) | SKIPPED — the probe is specified against an injected failure (shim); uninjected recovery after a real unsatisfiable failure is run as MAP-012 |
| P3-T-FAIL-011 | S8.1.2 (F-02) | SKIPPED — the no-syscall aspect needs the shim's attempt counter; rejection aspect run as MAP-021 / ARITH-001 / ARITH-002 |
| P3-T-FAIL-021 | S8.1.2 (F-03) | SKIPPED — EMFILE/ENFILE-class injection needs the shim (INC-C007-01) |
| P3-T-FAIL-031 | S8.1.2 (F-04) | SKIPPED — EAGAIN-class injection and attempt counting need the shim (INC-C007-01) |
| P3-T-FAIL-041 | S8.1.2 (F-05) | SKIPPED — BLOCKED-by-U-09; B-009 writes no pass criterion and none is written here |
| P3-T-FAIL-051 | S8.1.2 (F-06) | SKIPPED — release-failure injection needs the shim; the branch rests on code inspection as at C-004 (INC-C007-01) |
| P3-T-FAIL-061 | S8.1.2 (F-07) | SKIPPED — lock/primitive fault injection does not exist in the banked base (INC-C007-01) |
| P3-T-FAIL-081 | S8.1.2 (F-09) | SKIPPED — DIAGNOSTIC-ONLY case for a DEBUG/internal-check build; no such build exists in the banked base and it is never a conformance PASS (S7.2) |
| P3-T-FAIL-091 | S8.1.2 (F-10) | INSPECTED — inapplicability recorded per its own row: the baseline makes ONE mapping attempt per allocation (worksheet, item 6); the multi-step variant guarded does not exist in Phase 3 |
| P3-T-FAIL-101 | S8.1.2 (F-11) | SKIPPED — forcing discovery failure needs interposition on the L6 query (INC-C007-01); discovery itself is failure-checked and exercised by every run of this suite |
| P3-T-FAIL-111 | S8.1.2 (F-12) | SKIPPED — needs an instrumented reporting path in a cannot-allocate state (INC-C007-01) |
| P3-T-DATA-001 | S8.1.3 (S10 data integrity; S13 item 4) | PASS (run on Pi) — single allocate/write/verify/free cycle; post-cycle allocation succeeds (baseline as behaviour, INC-C007-04) |
| P3-T-DATA-002 | S8.1.3 | PASS (run on Pi) — 200 cycles, each returning to baseline behaviour |
| P3-T-DATA-011 | S8.1.3 | PASS (run on Pi) — live set of seeded random sizes (seed 0x243F6A8885A308D3), unique patterns verified repeatedly incl. first/last bytes |
| P3-T-DATA-012 | S8.1.3 | PASS (run on Pi) — pairwise non-overlap; neighbours written/freed/newly allocated change no live bytes |
| P3-T-DATA-021 | S8.1.3 (S9.3 confirmation) | PASS (run on Pi) — large allocation spanning many runtime-discovered system pages (size derived from the manifest value), pattern verified incl. page-straddle offsets |
| P3-T-DATA-022 | S8.1.3 (S9.3 confirmation) | PASS (run on Pi) — 100 churn operations against a persistent live set; survivors intact, retired exactly once; completion itself is the no-hang evidence (no timing oracle) |

### ARITH / ALIGN / INIT / ERR (B-009 S8.2)

| Case | B-009 section | Verdict |
|---|---|---|
| P3-T-ARITH-001 | S8.2.1 (MALLOC-04 + Inv. 3) | PASS (run on Pi) — SIZE_MAX rejected with the defined failure, no wrapped allocation |
| P3-T-ARITH-002 | S8.2.1 | PASS (run on Pi) — symbolic boundary family (SIZE_MAX, -1, -8, -16, SIZE_MAX/2) each rejected, never wraps to a small allocation |
| P3-T-ARITH-011 | S8.2.1 (MA-6.2-020) | PASS (run on Pi) — failure via the defined channel, no partial block; caller inputs and pre-existing live block unchanged |
| P3-T-ARITH-012 | S8.2.1 (MA-6.2-020) | PASS (run on Pi) — oversize, mapping-failure and calloc-overflow causes all yield the same failure class, no false success |
| P3-T-ARITH-031 | S8.2.1 (MA-6.2-024) | SKIPPED — metadata-allocation failure injection needs the shim (INC-C007-01) |
| P3-T-ARITH-032 | S8.2.1 (MA-6.2-024) | SKIPPED — partial-descriptor failure needs fault injection (INC-C007-01); the following-allocation half is run as MAP-012 |
| P3-T-ARITH-041 | S8.2.1 | SKIPPED — the namespaced API takes no alignment parameter, so alignment-rounding overflow has no executable instance; size+reserve overflow is run as ARITH-002 (INC-C007-05) |
| P3-T-ARITH-042 | S8.2.1 | SKIPPED — the before-any-mapping-attempt aspect needs the shim counter (count 0); rejection aspect run as ARITH-001/-002 |
| P3-T-ARITH-043 | S8.2.1 | PASS (run on Pi) — over-rejection guard: sizes 1, alignment-1, alignment, alignment+1, page-1, page, page+1 (all derived, no literals) succeed and verify |
| P3-T-ALIGN-001 | S8.2.2 (ALIGNED-01, hook C-007 valid half) | SKIPPED — no explicit-alignment entry point exists in the banked surface (documented absence, B-008 S6.1; INC-C007-05). Fundamental alignment of mimir_malloc is run under MAP-001 |
| P3-T-ALIGN-002 | S8.2.2 (ALIGNED-01) | SKIPPED — no explicit-alignment entry point (INC-C007-05) |
| P3-T-ALIGN-011 | S8.2.2 (ALIGNED-02) | SKIPPED — no explicit-alignment entry point; failure induction would also need the shim (INC-C007-01/-05) |
| P3-T-ALIGN-012 | S8.2.2 (ALIGNED-02) | SKIPPED — no explicit-alignment entry point (INC-C007-05) |
| P3-T-ALIGN-021 | S8.2.2 (ALIGNED-03) | SKIPPED — BLOCKED-by-U-11; no pass criterion exists |
| P3-T-ALIGN-031 | S8.2.2 (ALIGNED-04) | SKIPPED — BLOCKED-by-U-11; no pass criterion exists |
| P3-T-ALIGN-041 | S8.2.2 (ALIGNED-05) | SKIPPED — BLOCKED-by-U-09 (also U-11); no pass criterion exists |
| P3-T-ALIGN-051 | S8.2.2 (POSIXMA-01) | SKIPPED — posix_memalign is not in the banked surface (symbol set unsettled, U-01; documented absence) |
| P3-T-ALIGN-052 | S8.2.2 (POSIXMA-01) | SKIPPED — posix_memalign absent (U-01) |
| P3-T-ALIGN-061 | S8.2.2 (POSIXMA-02, hook C-008) | SKIPPED — posix_memalign absent, so the unchanged-output model has no executable instance (U-01) |
| P3-T-ALIGN-062 | S8.2.2 (POSIXMA-02 numbers) | SKIPPED — BLOCKED-by-U-14; no numeric criterion is written |
| P3-T-ALIGN-071 | S8.2.2 (POSIXMA-03) | SKIPPED — BLOCKED-by-U-11; no pass criterion exists |
| P3-T-ALIGN-081 | S8.2.2 (POSIXMA-04) | SKIPPED — BLOCKED-by-U-09; no pass criterion exists |
| P3-T-INIT-001 | S8.2.3 (MA-6.3-001, hook C-010) | SKIPPED — BLOCKED-by-U-05 for promotion (bound undecided; results would be PRELIMINARY per contracts S6.3). Startup/init is also C-006 scope, not in this base |
| P3-T-INIT-011 | S8.2.3 (MA-6.3-002, C-010) | SKIPPED — BLOCKED-by-U-05; the instrumented normal path it needs does not exist in the banked base |
| P3-T-INIT-021 | S8.2.3 (MA-6.3-003, C-010) | SKIPPED — BLOCKED-by-U-05 |
| P3-T-INIT-031 | S8.2.3 (MA-6.3-004, C-010) | SKIPPED — BLOCKED-by-U-05 |
| P3-T-INIT-041 | S8.2.3 (MA-6.3-005, C-010) | SKIPPED — BLOCKED-by-U-05 (inherits INIT-001); also needs a pre-main shared-library harness that does not exist in the base |
| P3-T-INIT-051 | S8.2.3 (MA-6.3-006) | SKIPPED — no distinct bootstrap path exists in the banked base to serve a block under C-010 conditions (registry anchors and lock are static, B-006 F-08 precondition fact), so the case cannot be instantiated as specified |
| P3-T-INIT-061 | S8.2.3 (MA-6.3-010) | INSPECTED — audit case: init-path dependency list enumerated (worksheet, item 6); no C++ container/utility on the path; this programme reaches main and allocates |
| P3-T-INIT-071 | S8.2.3 (MA-6.3-011) | SKIPPED — no ordering mechanism for allocating static/global construction is stated by any banked design, so the precondition cannot be satisfied as specified |
| P3-T-INIT-081 | S8.2.3 (MA-6.3-012) | INSPECTED — audit case: central bookkeeping is fixed static storage; per-block descriptors live inside each block's own mapping (worksheet, item 6). Runtime footprint measurement is not claimed |
| P3-T-ERR-031 | S8.2.4 (MA-6.2-033) | SKIPPED — DIAGNOSTIC-ONLY; no DEBUG/internal-check build exists in the banked base; never a conformance PASS (S7.3) |
| P3-T-ERR-041 | S8.2.4 (MA-6.2-034) | SKIPPED — needs an internal-check build plus induced metadata corruption, neither available via the public API or the base (INC-C007-01) |
| P3-T-ERR-051 | S8.2.4 (MA-6.2-040) | SKIPPED — full-set terminology audit is an artefact of the S7.6 completeness audit (exit item 12), not a runtime case this suite can pass; no claim is made |
| P3-T-ERR-061 | S8.2.4 (MA-6.2-041) | SKIPPED — documentation audit as for ERR-051; this suite's own records qualify every page quantity as system page size or requested bytes |
| P3-T-ERR-071 | S8.2.4 (MA-6.3-020) | SKIPPED — needs the reporting path instrumented during an induced failure (its context case FAIL-111 is likewise skipped) |
| P3-T-ERR-081 | S8.2.4 (MA-6.3-021) | SKIPPED — needs reporting invoked from an induced corrupted state (F-09/F-12 induction unavailable) |
| P3-T-ERR-091 | S8.2.4 (MA-6.3-022) | SKIPPED — needs bootstrap-path reporting instrumentation (INIT-011, BLOCKED-by-U-05) |
| P3-T-ERR-101 | S8.2.4 (MA-6.3-023) | SKIPPED — needs cascaded-error instrumentation with retained-storage accounting |
| P3-T-ERR-111 | S8.2.4 (MA-6.3-030) | SKIPPED — full claim-surface audit belongs to the S7.6 completeness audit; the header-scoped half is INSPECTED under UNS-071 |
| P3-T-ERR-121 | S8.2.4 (MA-6.3-031) | SKIPPED — documentation audit as for ERR-111 (implied claims included) |
| P3-T-ERR-131 | S8.2.4 (MA-6.3-032) | SKIPPED — audit half belongs to the S7.6 audit; any stronger runtime signal-state claim is BLOCKED-by-U-05 and is not made |
| P3-T-ERR-141 | S8.2.4 (MA-6.3-040) | SKIPPED — BLOCKED-by-U-05 (also U-19): no atfork registration or lock discipline is documented in a banked design; no fork claim is made |
| P3-T-ERR-151 | S8.2.4 (MA-6.3-041, parent case only) | PASS (run on Pi) — parent case only: live parent blocks valid and byte-identical after fork, freed exactly once; the child made no allocator call and no child claim is made |
| P3-T-ERR-161 | S8.2.4 (MA-6.3-042) | SKIPPED — BLOCKED-by-U-05 (also U-17..U-21); no pass criterion exists |
| P3-T-ERR-171 | S8.2.4 (MA-6.2-050) | SKIPPED — BLOCKED-by-U-03, out of Phase 3 scope: no runtime configuration surface exists (L7 compiled out, B-006 S12.3) |
| P3-T-ERR-181 | S8.2.4 (MA-6.2-051) | SKIPPED — BLOCKED-by-U-03, out of scope: no statistics setting exists |
| P3-T-ERR-191 | S8.2.4 (MA-6.2-052) | SKIPPED — BLOCKED-by-U-03, out of scope: no diagnostics setting exists |
| P3-T-ERR-201 | S8.2.4 (MA-6.2-053) | SKIPPED — BLOCKED-by-U-03, out of scope: no config or config version ID exists |
| P3-T-ERR-211 | S8.2.4 (MA-6.2-054, hook C-009) | PASS (run on Pi) — 50 cycles: block allocated on thread A, bytes verified and freed on thread B, ownership preserved exactly |
| P3-T-ERR-212 | S8.2.4 (MA-6.2-054, hook C-009) | PASS (run on Pi) — churn: 4 rounds x 4 threads created/destroyed while blocks transfer thread->main (seeded); each block verified and retired exactly once by its correct owner; a main-owned live set held across the churn stayed intact. Correctness only |

### CAL / REA / COND (B-009 S8.3)

| Case | B-009 section | Verdict |
|---|---|---|
| P3-T-CAL-001 | S8.3.1 (CALLOC-01, hook C-002) | PASS (run on Pi) — non-null, suitably aligned (non-numeric), every byte zero before any caller write; includes a non-multiple-of-alignment product |
| P3-T-CAL-002 | S8.3.1 (CALLOC-01) | PASS (run on Pi) — same across the registered size set incl. non-multiples of the alignment |
| P3-T-CAL-003 | S8.3.1 (CALLOC-05, hook C-002) | PASS (run on Pi) — every inspected byte of the requested product (37x11) zero at return; no byte beyond the requested product inspected as an oracle (INC-C005-02 discipline kept) |
| P3-T-CAL-011 | S8.3.1 (CALLOC-02, hook C-001) | PASS (run on Pi) — overflow products refused with null in both factor orders and at SIZE_MAX x SIZE_MAX, no wrapped allocation; errno not asserted |
| P3-T-CAL-012 | S8.3.1 (CALLOC-02) | PASS (run on Pi) — representable product above the target maximum (SIZE_MAX/2) refused with null in both factor orders |
| P3-T-CAL-021 | S8.3.1 (CALLOC-03) | PASS (run on Pi) — non-overflow failure (representable, unsatisfiable product) returns null, no ownership; next ordinary calloc behaves normally. Induced via the public API per INC-C005-03 precedent, not via shim/rlimit |
| P3-T-CAL-031 | S8.3.1 (CALLOC-04) | SKIPPED — BLOCKED-by-U-09; no pass criterion exists and none is written. The implementation's zero-factor posture is C-005's record, not this suite's |
| P3-T-REA-001 | S8.3.2 (REALLOC-01, hook C-004; also serves MA-6.2-023 per S8.3.3) | PASS (run on Pi) — grow and shrink each preserve the first min(old, new) bytes byte-for-byte |
| P3-T-REA-002 | S8.3.2 (REALLOC-01; MA-6.2-023 per S8.3.3) | PASS (run on Pi) — grow preserves the range on a move; the move instance is observed per item 5 below; the returned block is retired exactly once |
| P3-T-REA-011 | S8.3.2 (REALLOC-02, hook C-003; also serves MA-6.2-022 per S8.3.3) | PASS (run on Pi) — failed realloc (SIZE_MAX) returns null; original unchanged, still caller-owned and usable; errno not asserted |
| P3-T-REA-012 | S8.3.2 (REALLOC-02; MA-6.2-022 per S8.3.3) | PASS (run on Pi) — failed realloc (SIZE_MAX/2): original intact and freeable exactly as before; fresh allocation after succeeds |
| P3-T-REA-021 | S8.3.2 (REALLOC-03) | PASS (run on Pi) — realloc(NULL, 64) behaves as mimir_malloc(64) |
| P3-T-REA-022 | S8.3.2 (REALLOC-03) | PASS (run on Pi) — realloc(NULL, unsatisfiable/oversize) behaves as the malloc failure cases: null, no ownership |
| P3-T-REA-031 | S8.3.2 (REALLOC-06) | PASS (run on Pi) — preserved range verified; the tail is recorded UNSPECIFIED and never asserted, exactly as the row requires |
| P3-T-REA-041 | S8.3.2 (REALLOC-04) | SKIPPED — BLOCKED-by-U-12; in particular neither the return value nor the ownership/freeability of p is asserted. The implementation posture is C-005's record, not this suite's |
| P3-T-COND-001 | S8.3.4 (REALLOCARRAY-01) | SKIPPED — BLOCKED-by-U-16: the gate does not hold for this target (no annex records it; reallocarray is absent from the surface — nm evidence in the recorded run) |
| P3-T-COND-002 | S8.3.4 (REALLOCARRAY-02) | SKIPPED — BLOCKED-by-U-16 |
| P3-T-COND-003 | S8.3.4 (REALLOCARRAY-03) | SKIPPED — BLOCKED-by-U-16 |

### UNS (B-009 S8.4)

| Case | B-009 section | Verdict |
|---|---|---|
| P3-T-UNS-001 | S8.4 (zero-size umbrella) | SKIPPED — BLOCKED-by-U-09 (and U-12 for realloc(p, 0)): the written record lists zero-size as unsupported/unresolved and no case in this suite claims a zero-size result; no runtime refusal is asserted |
| P3-T-UNS-011 | S8.4 (explicit alignment) | SKIPPED — BLOCKED-by-U-11 (numeric guarantee also U-15); no failure model exists to assert |
| P3-T-UNS-021 | S8.4 (calloc/realloc on the B-006-only surface) | INSPECTED — audit case: B-006 states it provides NO mimir_calloc / mimir_realloc (worksheet, item 6); the pair's conformance home is S8.3 under the B-008 S7.2 amended list, run above as CAL/REA |
| P3-T-UNS-031 | S8.4 (interposition / standard-name replacement) | INSPECTED — audit case: the API translation unit exports exactly the four mimir_* names and no standard name (nm in the recorded run); no interposition is claimed or tested |
| P3-T-UNS-041 | S8.4 (size classes / trim / caches) | INSPECTED — audit case: no case in this suite asserts address reuse, trim, size classes, caches or sharding; the suite contains no trim operation and no fuzzer (the S4.3d fuzzer does not exist in the banked base, INC-C007-01) |
| P3-T-UNS-051 | S8.4 (statistics / trim API) | SKIPPED — BLOCKED-by-U-03: scope of any stats/trim surface is itself the open decision; no conformance case here depends on one |
| P3-T-UNS-061 | S8.4 (fork, U-17) | SKIPPED — BLOCKED-by-U-17 (under U-05); nothing about the child is asserted (see ERR-151 parent case) |
| P3-T-UNS-062 | S8.4 (fork, U-18) | SKIPPED — BLOCKED-by-U-18 (under U-05) |
| P3-T-UNS-063 | S8.4 (fork, U-19) | SKIPPED — BLOCKED-by-U-19 (under U-05) |
| P3-T-UNS-064 | S8.4 (fork, U-20) | SKIPPED — BLOCKED-by-U-20 (under U-05); no stats surface exists |
| P3-T-UNS-065 | S8.4 (fork, U-21) | SKIPPED — BLOCKED-by-U-21 (under U-05) |
| P3-T-UNS-071 | S8.4 (signal-handler use) | INSPECTED — audit case, header-scoped: the public header expressly states no async-signal-safety is stated or implied, and this suite contains no signal handler at all (worksheet, item 6). The full claim-surface audit remains ERR-111..131, SKIPPED above |
| P3-T-UNS-081 | S8.4 (non-exit targets) | INSPECTED — audit case: the only recorded run is ARM64 Linux + glibc on this Pi; no annex or case instance exists for Android/bionic/musl/Termux/proot and no evidence from them is worded as exit evidence; the x86-64 annex is likewise absent and its run OWED |
| P3-T-UNS-091 | S8.4 (performance claims) | INSPECTED — audit case: no timing value is an oracle in any case of this suite (worksheet, item 6); recorded-run timestamps are records, never oracles; no PASS line carries a performance conclusion |
| P3-T-UNS-101 | S8.4 (malloc_usable_size) | SKIPPED — BLOCKED-by-U-13 (presence also U-01); no presence, semantics, or null-case result is asserted |

NO-TEST-UB rows (no case ID minted by B-009, recorded here
so they are not silently absent): MA-6.1-REALLOC-05,
MA-6.2-030/-031/-032, MA-6.1-POSIXMA-05, MA-COND-USABLE-03,
MA-COND-CXX-09 — no hook, no expected result (contracts
S6.2). Unmapped-with-reason families (B-009 S8.3.4):
MA-COND-USABLE-01/-02/-04 and MA-COND-CXX-01..08/-10 are
out of Phase 3 scope for the reasons tabled there.

## Inconsistencies and postures recorded

1. INC-C007-01 — NO FAULT-INJECTION SHIM, WATCHDOG, OR
   DEBUG BUILD EXISTS IN THE BANKED BASE (carried from
   INC-C004-02 and INC-C005-03). B-009 specifies a shim on
   the L6 mapping boundary (S4.3b), a bootstrap watchdog
   (S4.3c, bound itself BLOCKED-by-U-05), an ownership-model
   fuzzer (S4.3d), a differential comparator (S4.3e) and an
   evidence collector (S4.3f). None of these components
   exists in the banked base at 56eb4dc. Consequences, all
   recorded rather than papered over: every FAIL case that
   needs injection (FAIL-001/-002/-003/-011/-021/-031/
   -051/-061/-101/-111), ARITH-031/-032/-042, ERR-031/-041
   and the ERR-071..-101 reporting cases are SKIPPED above;
   failure behaviour is evidenced only at the points a
   caller can actually induce through the public API
   (oversize arithmetic refusal and representable-but-
   unsatisfiable requests), exactly the INC-C005-03
   precedent; SYS-003 exercises the S9.2 collector RULE on
   in-memory sample records and is not a collector; exit
   items 5, 16 and 17 of B-009 S10 remain UNMET on this
   target. Building the shim/fuzzer/collector is a scope
   expansion this slice does not take; it is recorded here
   for the keeper to schedule.

2. INC-C007-02 — THE x86-64 LEG IS OWED IN FULL (B-009
   S3.4). This suite is written to run unchanged on x86-64
   Linux + glibc (portable C11 + POSIX pthread/fork/sysconf,
   no page-size literal, symbolic boundary values), but it
   has not run there: no x86-64 host exists in this
   campaign. DIFF-001/-002 are SKIPPED, every PASS above is
   an ARM64-only, per-target PASS (B-009 S9.1: PASS never
   aggregates across targets), and Phase 3 exit item 3
   stays UNMET. The standing wording is: x86-64 run OWED.

3. INC-C007-03 — NO COMPLETED PER-TARGET ANNEX EXISTS.
   B-009 Appendix B is a template; no ANNEX-ARM64 (or
   ANNEX-X86-64) has been completed, so every case whose
   precondition cites a V-/U-item disposition stays BLOCKED
   on this target (S4.4) — that is the recorded status of
   SYS-002 and of all BLOCKED-by-U-nn cases above. This
   slice does not mint an annex: selecting per-target
   policies (V-01..V-07) is a decision record, not a test
   result, and no decision was given to this slice.

4. INC-C007-04 — "COUNTERS/REGISTRY RETURN TO BASELINE"
   IS NOT OBSERVABLE THROUGH THE PUBLIC API. Several
   B-009 pass criteria (MAP-012/-031/-032, DATA-001/-002,
   REA-012) include registry/counters returning to
   baseline. The public surface exposes no counters, and
   this suite deliberately includes no internal header, so
   those halves are evidenced here only by their observable
   consequences: frees complete, no live block's bytes are
   disturbed, and fresh allocations after each cycle behave
   exactly as before it. The internal-counters half rests
   on the C-003/C-004 core evidence and code inspection,
   not on this run. Recorded so no reader mistakes the
   behavioural evidence for a counter measurement.

5. INC-C007-05 — REA-002's MOVE OBSERVATION AND THE
   NO-ADDRESS-ORACLE RULE. B-009 forbids pointer addresses
   as oracles (S5). REA-002's criterion, however, is about
   a move instance ("on a move, the same preserved range
   matches"). This implementation always moves on grow
   (B-008 S4.4 design fact), so the case records the
   returned pointer's inequality with the original as a
   PRECONDITION observation — proof the move instance the
   criterion addresses actually occurred — while the pass
   criterion itself is only the byte-for-byte preservation
   of the min(old, new) range. No other case in this suite
   compares addresses except pairwise distinctness/
   non-overlap of simultaneously live blocks (MAP-002,
   DATA-012), which is the Inv. 1 oracle B-009 itself
   specifies. Recorded as a deliberate, bounded reading,
   not a general licence.

6. Inspection worksheets for the INSPECTED verdicts
   (performed at this slice's run, on the banked sources):
   - FAIL-091: `grep -c mimir_platform_map_acquire
     src/core/mimir_core.c` = 1 — one mapping-acquire call
     site (`alloc_locked`); partial failures after acquire
     release inside `alloc_locked` before any descriptor is
     published. The baseline is one mapping per allocation,
     all-or-nothing; F-10's multi-step variant does not
     exist in Phase 3.
   - INIT-061: includes of the three implementation units —
     src/api/mimir_alloc.c (mimir_alloc.h, mimir_core.h);
     src/core/mimir_core.c (stdint.h, string.h,
     mimir_platform.h); src/platform/mimir_platform.c
     (stdint.h, sys/mman.h, unistd.h, pthread.h). All C11;
     no C++ container or utility anywhere on the init path.
   - INIT-081: central bookkeeping in mimir_core.c is a
     static registry head pointer plus static counters;
     descriptors are allocated inside each block's own
     mapping (B-006 S7.3 layout). Central footprint is
     therefore fixed regardless of allocation/thread count;
     per-block metadata is per-mapping by construction.
   - UNS-021: `grep -n "provides NO" docs/
     PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md` — line 1175:
     "B-006 provides NO mimir_calloc and NO mimir_realloc"
     (S12.2). The B-008 surface is the conformance home.
   - UNS-031: `nm -g --defined-only build/mimir_alloc.o`
     in the recorded run lists exactly mimir_malloc,
     mimir_free, mimir_calloc, mimir_realloc; and
     `nm build/test_c007 | grep -i "posix_memalign|
     aligned_alloc|malloc_usable_size|reallocarray"` = 0
     matches, backing the ALIGN/COND absence statements.
   - UNS-041: the word "trim" occurs in test_c007.c only
     inside the UNS-041/UNS-051 verdict texts themselves;
     no trim call, no fuzzer, no reuse assertion exists in
     the suite.
   - UNS-071: `grep -n "sigaction|signal(|sig_atomic"
     tests/contract/test_c007.c` = 0 matches; and
     include/mimir_alloc.h states "No async-signal-safety
     is stated or implied for either function".
   - UNS-091: `grep -n "clock_gettime|gettimeofday|
     difftime|time(NULL" tests/contract/test_c007.c` = 0
     matches; no timing assertion exists in the source.
   - UNS-081: by the recorded run's own manifest (this Pi,
     aarch64, Debian glibc) and the absence of any other
     annex or run in the repo.

7. Defects found: none. Every run case passed on the
   first recorded run (122 checks, 0 failed), and no
   allocator behaviour contradicted its B-009 criterion,
   so no INC-C007-* defect entry and no allocator fix
   arises from this slice. (INC-C007-01..05 above are
   evidence-scope limitations and one oracle-discipline
   reading, not implementation defects.) Had a case
   failed, the failure would have been recorded here as
   INC-C007-* with the case ID, per the assignment.

8. Scope kept: no C-006 dependence (the INIT cases that
   need it are SKIPPED, not faked); no unsupported-
   behaviour enforcement (C-008); no Phase 3 exit packet
   (C-009); no new public API; no change to allocator
   behaviour; README.md untouched.
