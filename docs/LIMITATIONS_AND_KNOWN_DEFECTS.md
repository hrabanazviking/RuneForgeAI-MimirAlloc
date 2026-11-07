# Limitations and Known Defects - MimirAlloc (public register)

Slice M-008 (Phase 13). This register collects, in one place and in
plain language, every limitation, unmet exit, owed run, and open
inconsistency (INC) that the banked record carries at base
6b312e3197cdb2e491ab0ffa24a4ba8ab95a1b17. Sources: every phase exit
packet, the M-001 carried-owed register, the M-002/M-003/M-004
records, `docs/DECISION_LEDGER.md`, `docs/EVIDENCE_LEDGER.md`, and
WAVE_PLAN.md keeper bookkeeping (cited as bookkeeping, never as a
public document). The sweep method and counts are in
`docs/PHASE_13_SBOM_SECURITY_PLATFORMS_M008.md`. Where this register
doubted whether an item was still open, it included the item and
marked it doubtful rather than dropping it. Phase grades below are
the last banked grades; this register does not re-grade any phase.

## Part (i) - Capability limitations (what the allocator does not do)

- L-01 No global interposition by default. MimirAlloc is adopted
  explicitly through its four `mimir_*` functions; no artifact or
  default replaces the system `malloc` family for a whole process.
  Interposition routes are opt-in only. Source: M-001 plan S6 /
  SRC-18. Owner: standing design posture.
- L-02 No aligned-allocation public surface yet. The public header
  declares exactly mimir_malloc, mimir_free, mimir_calloc,
  mimir_realloc; there is no explicit-alignment entry point, no
  usable-size, statistics, trim, or reallocarray entry point in the
  public ABI. Absence at link time is the documented unsupported
  behaviour. Source: `include/mimir_alloc.h`; B-008 S6.1.
- L-03 No budget enforcement. Soft budgets are defined in full
  (Phase 9, I-002) but enforcement was adopted by no slice;
  INC-I001-14 stands and the budget failure counter is 0-by-absence
  (there is no budget to fail against). Source: Phase 9 exit packet
  X1 PARTIAL. Owner: a future slice adopting enforcement by recorded
  decision.
- L-04 Telemetry counters are internal-only, with known partial
  coverage. Statistics exist as one locked internal snapshot
  surface, not a public API. Cross-thread frees are counted on the
  mapping path only; the small path contributes exactly 0 by
  recorded choice (INC-J002-01). Small request sizes are not
  retained, so one defined telemetry quantity cannot be provided
  (INC-J002-05 / INC-I002-02). Lock contention is not counted: the
  counter is absent by recorded deferral (INC-J002-02).
- L-05 Single-lock contention price. The core serialises on one
  global lock. Phase 11 measured the consequence honestly: see
  L-06. Source: Phases 8 and 11 records (H-004 contention controls;
  K-series measurements).
- L-06 Performance: MimirAlloc loses to glibc on every workload
  measured in Phase 11, stated in plain numbers as banked. K-002
  baseline: median factors about 5x to about 65x slower than the
  host glibc allocator across the measured shapes. K-008 figures
  include a small-path figure of 1,167 ns against glibc 56 ns, and
  large-path figures around 3.7 microseconds against glibc 75 ns
  and 56 ns. Batching (K-007) lost everywhere it was measured. The
  one shipped improvement (-O2, K-004) cut small64 median from 168
  to 86 ns/op and burst256 from 1,488 to 661 ns/op; large4096 stayed
  flat. No document may claim a universal performance win.
  Source: Phase 11 exit packet (PARTIAL, 6 MET / 2 PARTIAL).
- L-07 Diagnostics detect; they do not repair, and they are not on
  by default. The production diagnostic channel emits one fixed
  line per genuine failure, but emission is gated on the
  `MIMIRALLOC_DIAG=1` environment setting because the default-on
  reading collided with a banked silence battery; whether it is on
  by default is a ruling still owed (INC-J007-01). Detection sites
  found by the metadata-integrity work are not all wired to the
  channel (INC-J007-03). Allocator checks are not ECC (Roadmap
  S9.2).
- L-08 Cancellation and fork postures are narrower than the full
  POSIX surface. Deferred cancellation and teardown are proven;
  asynchronous cancellation and fork-while-busy are measured
  unsupported and stand as decided postures: fork is supported
  quiescent / fork+exec only. Source: Phase 8 packet S5 PARTIAL;
  INC-H006-01, INC-H006-02.
- L-09 Zero-size and realloc(ptr, 0) behaviour is posture, not
  contract. U-09 and U-12 remain unresolved: this implementation
  returns null (and, for realloc(ptr, 0), leaves the original block
  live), as a posture only; callers must not rely on it. errno on
  failure is likewise not promised (U-10). Source: public header.

## Part (ii) - Unmet exits and owed runs

- O-01 Phase 3 exit UNMET: the x86-64 run of the portable suite is
  owed in full - no host, no run, no record. Discharge: an x86-64
  host run of the suite with its per-target annexes, differential
  report, and page-size record. Source: Phase 3 exit packet.
- O-02 Phase 4 exit UNMET: the physical Android device run is owed
  in full; no NDK build or device run exists. Discharge: the D-009
  device run (allocate/fill/free + multithread, with logs) on a
  physical Android device. Source: Phase 4 exit packet.
- O-03 Phase 12 exit NOT MET; the Qwen3 comparison is OWED. The
  Qwen3-1.7B Q4_K_M model is absent on the Pi (INC-L007-01/02,
  INC-L001-01) and no runnable aarch64 Aesir engine exists
  (INC-L003-01/02). The complete gated protocol exists (L-007); the
  run was never performed. TinyLlama results are a different,
  non-exit comparison and are never folded in. Discharge: both
  preconditions present, then the L-007 protocol run.
- O-04 Phase 1 original-failure literal retry is still owed, and
  Phase 6 Clause A is UNMET / X1 BLOCKED. The original failing Mojo
  command was never recorded exactly; the affected environment
  (SM-X400, Android/Termux glibc) and both failing Mojo versions
  are absent from the Pi. Phase 6 exited only via the
  blocker-record alternative (X4). The tablet failure is not fixed
  and not reproduced. Source: Phase 6 exit packet (F-009).
- O-05 Phase 7 X5 instrument debt: retained memory versus the
  platform allocator stays PARTIAL for lack of an
  allocator-attributable instrument on the platform side
  (INC-G001-06). Phase 7 as last graded is PARTIAL; its X4/X6
  unload reasons were later discharged by H-001, but no re-grade
  packet exists, so the last graded verdict stands (INC-M001-02).
- O-06 4 KiB-page host run owed: the suite has never run on a
  4,096-byte-page host (INC-D003-02). Every banked figure is a
  16 KiB-page figure. Discharge: a suite run on a 4 KiB host.
- O-07 Phase 10 merged-debug whole is owed as one tree: the debug
  flavour's distinction was proven per slice; the merged-tree debug
  figures and flavour grading of three by-design-divergent checks
  were owed at packet time (INC-J005-03, INC-J008-01, INC-J004-02).
  Phase 10 exit is PARTIAL (MET 4 / PARTIAL 6).
- O-08 Phase 9 remainder: fragmentation leg F-6 cross-slice
  sampling is owed (INC-I006-02); F-1(a) in allocator-state form
  and F-5 (no budget) are unmeasurable as banked. Phase 9 exit is
  PARTIAL. The Android pressure track is owed with O-02.
- O-09 Phase 11 remainder: RSS/PSS (X-K4) and energy (X-K5) grade
  PARTIAL as banked; no discharge is claimed in Wave 13.
- O-10 Phase 0 remainder, as last recorded: naming availability
  stays unverified until separately checked; package-name checks
  were never recorded; no code of conduct or issue templates were
  adopted in the Phase 0 record. Source: Phase 0 exit checklist.
  (Ledgers named owed there now exist as docs/DECISION_LEDGER.md
  and docs/EVIDENCE_LEDGER.md in this tree; the checklist itself
  was not rewritten.)
- O-11 dlopen / R3 consumer route untested by decision since
  Phase 5 (E-001 S5.2, O-10). The proven routes are R2 (dynamic
  link) and R1 (static). The unload harness's dlopen/dlclose cycles
  are not an R3 consumer-route proof. Discharge: a deliberately
  commissioned R3 slice.
- O-12 Fuzzer and fault-injection history, honestly stated: the
  Phase 3-era injection set was owed at its packet (no shim then);
  the shim later landed in Phase 9 (I-004, discharging
  INC-I001-01). Any reader comparing packets should use the later
  record. Phase 13 fuzzing (M-004) carries its own coverage limits
  in Part (iii).

## Part (iii) - Open defects and INCs

First among equals, as the assignment directs:

- D-01 INC-M002-01 (Invariant 9 GAP): lock-unlock failure is never
  observed - every unlock call discards its status. A failed unlock
  would wedge every later call with no counter, no report, and no
  termination. Low likelihood, high opacity. Owed: a post-unlock
  failure tally or a documented ruling accepting the posture.
- D-02 INC-M002-02 (Invariant 9 GAP, reporting only): silent
  lock-failure courses - free on lock failure returns silently with
  the block live (safe), and drain/trim return 0 on lock failure,
  indistinguishable from nothing retained. No ownership is ever
  lost; only the reporting leg is unmet.

Standing register (every still-open INC the sweep found in the
banked record, grouped; citation chains deduplicated by INC
identity):

- D-03 INC-H001-01 / INC-G008-02 chain: H-001 fixed the unload
  orphan defect (drain at unload, growth exactly 0 B/cycle), which
  inverted the banked G-008 harness's stale growth PASS-check:
  standalone `make test-g008` stands at 214 checks / 1 failure BY
  DESIGN of the fix; `test_h001` supersedes it for unload
  behaviour. Re-pointing the check is an evidence-policy decision
  still owed (owner SRC-08; decision owed before M-009). The full
  `make test` suite passes 4590/0 - see Part (iv).
- D-04 INC-I001-14: budget enforcement not adopted (see L-03).
- D-05 INC-I009-01: `docs/SUPPORTED_TARGETS.md` freshness - the
  A-003-era file still claims a documentation-only repository.
  Assigned to M-006, which discharges or re-owes it. Until M-006
  banks, this project's platform truth lives in
  `docs/SUPPORTED_PLATFORMS.md` (M-008) with its disclosure header.
- D-06 K-007 proposal P-1: the batching proposal P-1 is recorded,
  not implemented. No Wave 13 slice is assigned it; it stands
  unresolved for the public register.
- D-07 SONAME / install / version items, OPEN at this base:
  the built `.so` has no SONAME, no versioned file name, no
  `install` target, and no ldconfig story; consumers' DT_NEEDED
  names the plain filename (INC-E002-01, INC-E003-06,
  INC-E004-01, INC-M002-05). Owner: M-007, in flight at this base;
  at this base they are recorded OPEN with M-007 named as owner.
- D-08 INC-M003-01: tool-noise item recorded by the M-003 warning
  gates slice (owner: Makefile / a future slice, as recorded
  there).
- D-09 INC-M004-01 and INC-M004-02: Phase 13 fuzzing coverage
  limits, recorded honestly by M-004 - failure-shaped realloc
  coverage and the multithread leg are limited as stated there.
- D-10 INC-M002-03: contracts.md Appendix A omits mimir_calloc and
  mimir_realloc from its U-03 inventory although both have been
  declared and implemented since C-005. Documentation gap only;
  owed an Appendix A amendment when U-03 is next touched. U-03
  itself remains open by design.
- D-11 INC-M002-04 / INC-J002-01: cross_thread_frees counter is
  partial (mapping path only) - see L-04.
- D-12 INC-J006-01..09 family (Phase 10 metadata integrity): of 25
  corruption probes, 16 were detected, 6 classes stayed SILENT
  (including consistent length corruption, INC-J006-02; a free_head
  naming a live object, INC-J006-04; live_count trusted by release
  paths, INC-J006-06) and 2 classes crashed as expected on wild
  pointers no in-process check can validate without reading them.
  No run claims repair of arbitrary corruption.
- D-13 INC-J003-01..04 family: statistics balance clauses R10 and
  the Tier 2 (small-path) part of R11 are NOT-PROVABLE from the
  shipped surface, and D-LOCK / D-FAIL-BUDGET are not-provable as
  recorded absences. Named, not skipped.
- D-14 INC-C007-01 chain: failure-injection-dependent contract
  cases were skipped in the C-007 run for want of a shim; the shim
  later landed (Phase 9, I-004). The C-007 recorded run stands as
  history; the open remainder is the Phase 3 exit itself (O-01).
- D-15 INC-L001-04: MimirWell is not MimirAlloc. Aesir itself
  allocating through MimirAlloc is NOT TRUE on present evidence
  (zero mimir_* references in Aesir source); only the boundary and
  explicit-adoption wiring are proven. See also O-03.
- D-16 INC-L009-01: the built `.so` is absent after `make test`
  alone, making ABI verification order-dependent; verification
  procedures name `make lib` explicitly (INC-M001-03).
- D-17 INC-M001-01: the release gate cannot be `make test` alone,
  because standalone test-g008 carries the known 214/1 of D-03
  while `make test` passes 4590/0. M-009 must show both.
- D-18 INC-M001-04: versioned artifacts cannot be published
  honestly until the SONAME/install decision (D-07) is recorded;
  the queue order M-007 before M-009 is load-bearing.
- D-19 Open contract questions carried by the header and contracts
  record (U-01, U-03, U-07 ABI-freeze decisions; U-09, U-10, U-12,
  U-15, U-17..U-21 as listed in L-09 and the header): decisions the
  project owner has not yet taken, recorded open by design.
- D-20 Doubtful / carried for completeness: INC-I002-02 (small
  request sizes not retained - counted in L-04), INC-I006-02
  (counted in O-08), INC-I007-02 (+16,384 B resident residual
  reported as kernel-managed), INC-J007-03 (counted in L-07),
  INC-H009-01 (Phase 8 E2 banking-owed at its packet; H-008 has
  since banked in this tree, but no re-grade packet exists, so the
  PARTIAL of record stands until M-009 verifies - INC-M001-02).

## Part (iv) - Known non-defects (readers mistake these for defects)

- N-01 test-g008 standalone 214/1: the one failure is the stale
  growth check inverted BY the H-001 fix (D-03). It is an
  evidence-policy debt, not an allocator defect; `make test`
  passes 4590/0 and test_h001 proves the fixed unload behaviour.
- N-02 realloc(p, 0) and zero-size postures: returning null for
  zero-size requests, and realloc(ptr, 0) returning null while
  leaving the original block live, are this implementation's
  recorded postures over contract questions (U-09, U-12) that
  remain unresolved by design - surprising, but documented
  contract behaviour, not a defect (header, B-008).
- N-03 Skipped checks in suite output (for example 7
  skipped-not-yet cases in one sub-suite of this slice's baseline
  run) are named SKIPPED/NOT-YET lines tied to instrumentation
  that does not exist (INC-C007-01 chain), printed in the open -
  they are not hidden failures; every run total still reads
  0 failed.

Every entry above names its banked source; a reader with this tree
can verify each in minutes. Items this register could not settle
from the banked record were included and marked, never dropped.
