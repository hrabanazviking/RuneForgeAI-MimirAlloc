# Phase 13 Foundation Plan — Security and Production Release

Slice: M-001 (Wave 13, team-01). Worktree base:
`0b00f088ea6e522631b46c1b3abbdd5b3f9cf59d` (Wave 12 COMPLETE head).
Doc-only slice: no src, Makefile, README or header changes.

## 0. Baseline evidence (run on this Pi, this slice)

- `make clean && make test` exit 0: `CHECKS: 4590 run, 0 failed`,
  `RESULT: PASS`.
- `make lib` exit 0; `nm -D --defined-only build/libmimiralloc.so`
  = exactly 4 symbols: `mimir_calloc`, `mimir_free`, `mimir_malloc`,
  `mimir_realloc`.
- Samtal `http://127.0.0.1:8787/` HTTP 200 before and after;
  `free -g` showed 13G available throughout.
- Note (INC-L009-01): `make test` alone does not leave the `.so`
  in the tree; the ABI check requires an explicit `make lib` first.

## 1. Phase 13 scope, restated verbatim

Roadmap Phase 13 — Security and production release:

> Run code review, API/ABI audit, compiler-warning gates, fuzzing,
> long soak and target-matrix verification. Publish signed or
> checksum-verified artifacts, versioning, SBOM, reproducible-build
> instructions, documented supported platforms and a
> security-reporting channel. No default global interposition
> until well validated.
>
> Exit: explicit stable release criteria met; all limitations and
> unresolved defects public.

Plain statement: MimirAlloc is an experimental / research
allocator. It is not a stable production release, and no default
global interposition is claimed or planned as validated in
Wave 13. Any interposition use remains explicit, opt-in and
consumer-chosen, exactly as banked in earlier phases.

## 2. Slice map — Wave 13 (M-002..M-009)

| Slice | Team | Phase 13 element | Deliverable / exit evidence |
|---|---|---|---|
| M-002 | 02 | Code review + API/ABI audit | Full source review; ABI exactly 4 symbols; contract audit vs the A-008 contract-first spec |
| M-003 | 03 | Compiler-warning gates | All flavours built with `-Wall -Wextra -Werror` evidence; zero-warning gate record |
| M-004 | 04 | Fuzzing | Harness over alloc/write/realloc/transfer/free/trim/thread-exit vs the ownership model; seeds saved |
| M-005 | 05 | Long soak | Bounded long soak + repeated cycles on the Pi, Samtal-safe, with recorded run |
| M-006 | 06 | Target-matrix verification | Matrix re-verified; `docs/SUPPORTED_TARGETS.md` refreshed (discharges INC-I009-01 / INC-D009-02); x86-64 + Android honestly OWED if still unrun |
| M-007 | 07 | Versioning + artifacts | Version scheme, checksum-verified artifacts, reproducible-build instructions; SONAME/install decision recorded (see Section 4) |
| M-008 | 08 | SBOM + platforms + reporting channel | SBOM, documented supported platforms, `SECURITY.md` reporting channel, public limitations/defects register |
| M-009 | 09 | Exit packet | Phase 13 exit evidence graded honestly against Section 3 criteria; no default global interposition claimed |

Slices bank in roadmap order. Each slice is a complete vertical
deliverable with its own recorded run on the Pi; no slice may
claim a measurement it did not run and log.

## 3. Explicit stable release criteria (for M-009 to grade)

A criterion is MET only on banked, recorded evidence in the
tree M-009 grades. Anything else is PARTIAL, UNMET or OWED,
named as such. All criteria must be MET for a stable claim;
if any fails, M-009 must grade the exit NOT MET or PARTIAL
and publish the failing list — it must not soften the grade.

- SRC-01 Suite: `make clean && make test` exit 0 on the Pi,
  `CHECKS: 4590 run, 0 failed` or a higher banked count with
  0 failed, recorded in the exit run file.
- SRC-02 ABI: `make lib` then `nm -D --defined-only` on the
  built `.so` = exactly 4 symbols (`mimir_malloc`, `mimir_free`,
  `mimir_calloc`, `mimir_realloc`); no additional exported symbol.
- SRC-03 Code review: M-002 review record covers every file
  under `src/` and `include/`, with findings either fixed or
  entered as public defects; no unreviewed source file remains.
- SRC-04 API contract: M-002 audit shows every public behaviour
  matches the A-008 contract-first spec, or each divergence is
  listed publicly with its contract status.
- SRC-05 Warnings: M-003 record shows all shipped flavours
  build clean under `-Wall -Wextra -Werror` (zero warnings).
- SRC-06 Fuzzing: M-004 harness runs its recorded seed corpus
  to completion with 0 ownership-model violations, 0 crashes
  not already filed as public defects, and seeds saved in tree.
- SRC-07 Soak: M-005 bounded soak + repeated cycles complete
  with counters returning to baseline, no growth trend beyond
  the recorded bound, and Samtal HTTP 200 throughout.
- SRC-08 Known stale check: the `test-g008` standalone target's
  single known failure (INC-H001-01, Section 4) is either
  re-pointed by an authorised evidence-policy decision or
  explicitly carried in the public register; it is never
  silently excluded from the release evidence.
- SRC-09 Targets: M-006 target matrix matches recorded runs
  only; every target without a run is marked OWED / unsupported,
  and `docs/SUPPORTED_TARGETS.md` no longer claims the repo
  is documentation-only (INC-I009-01 discharged or re-owed).
- SRC-10 x86-64: a full suite run on x86-64 is recorded, or
  x86-64 is publicly listed as an unmet carried item and no
  stable claim is made for x86-64 (Phase 3 exit stays UNMET).
- SRC-11 Android: a physical Android device run is recorded,
  or Android is publicly listed as unmet/owed and no stable
  claim is made for Android (Phase 4 exit stays UNMET).
- SRC-12 Aesir comparison: the Qwen3 comparison run owed from
  Phase 12 is recorded under its protocol, or Phase 12 exit
  stays NOT MET in the public register and no Aesir-workload
  stability claim is made.
- SRC-13 Versioning: M-007 records a version scheme and the
  SONAME / install-target decision; artifacts carry that
  version, or the absence of SONAME/install is stated publicly
  as a limitation of the release.
- SRC-14 Artifacts: every published artifact has a checksum
  that verifies against a checksum file built in the same run,
  and the reproducible-build instructions rebuild a
  byte-identical or explicitly diffed artifact on the Pi.
- SRC-15 SBOM: M-008 SBOM enumerates every component in the
  shipped artifact with version and licence; a component not
  in the SBOM is a release-blocking defect.
- SRC-16 Platforms + reporting: documented supported platforms
  match SRC-09 exactly, and `SECURITY.md` (or equivalent)
  names a working security-reporting channel.
- SRC-17 Public register: all limitations and unresolved
  defects from Section 4 appear in the public register
  (M-008); no carried item in Section 4 is omitted.
- SRC-18 Interposition: no artifact, doc or default enables
  global interposition by default; any interposition route
  is opt-in only. A default-on route fails this criterion.
- SRC-19 Performance honesty: the public register states
  plainly that MimirAlloc loses to glibc on all measured
  workloads (Phase 11), with the banked figures; no doc
  claims a universal performance win.
- SRC-20 Pi safety: every Wave 13 run records Samtal HTTP 200
  and free memory; a run that starved Samtal is invalid and
  must be re-run, not graded.

## 4. Carried-owed reconciliation (from the banked tree)

Grades below are the last banked grades, read from the banked
exit packets and `WAVE_PLAN.md` at base `0b00f08`. This plan
does not re-grade any earlier phase; it carries each item to
the Wave 13 slice that owns it, or states that it stays owed
beyond Wave 13.

- Phase 3 exit — UNMET. The x86-64 run of the portable suite
  is owed in full (Phase 3 packet, C-009). Owner in Wave 13:
  M-006 records the matrix honestly; the run itself stays
  OWED unless an x86-64 host is provided. Blocks SRC-10.
- Phase 4 exit — UNMET. The physical Android device run
  (allocate/fill/free + multithread, with logs) is owed; no
  NDK build or device run exists (Phase 4 packet, D-009).
  Owner: M-006 records it; the run stays OWED. Blocks SRC-11.
- Phase 12 exit — NOT MET (L-009 packet: exit elements
  1 MET / 1 PARTIAL / 5 UNMET-OWED). The Qwen3-1.7B Q4_K_M
  comparison run is OWED: the model is absent on the Pi
  (INC-L007-01/02) and no aarch64 Aesir engine exists
  (INC-L009-01/02 context; L-003 blocker record). Owner:
  no Wave 13 slice can discharge it without both
  preconditions; M-009 must carry it. Blocks SRC-12.
- Phase 7 exit — PARTIAL. X4/X6 were PARTIAL on the unload
  dimension (INC-G008-02, since fixed by H-001 in Wave 8);
  X5 (retained memory vs platform allocator) stays PARTIAL
  for lack of an allocator-attributable instrument on the
  platform side (INC-G001-06). No re-grade packet exists;
  M-009 carries Phase 7 as PARTIAL as last graded.
- Phase 8 — S5 posture caveats (PARTIAL as graded): deferred
  cancellation and teardown are MET, but asynchronous
  cancellation (INC-H006-02: 5 of 6 lock deaths) and
  fork-while-busy (INC-H006-01: 2 of 3 children hung) are
  measured unsupported and stand as decided postures:
  fork is supported quiescent / fork+exec only. These
  postures must appear in the M-008 public register.
  E2 was PARTIAL on H-008 banking-owed at packet time;
  no later re-grade packet was found in the banked tree,
  so M-009 must verify H-008's banked state before grading.
- Phase 10 exit — PARTIAL (J-009 packet: MET 4 / PARTIAL 6).
  Caps include: counters PARTIAL (cross-thread frees counted
  on the mapping path only, INC-J002-01; lock contention
  ABSENT by deferral, INC-J002-02); balance clauses
  NOT-PROVABLE as named (INC-J003-01..04); metadata-integrity
  SILENT/CRASH classes unrepaired by design (INC-J006-01..09);
  diagnostic channel default-on is a ruling still owed
  (INC-J007-01, gated on `MIMIRALLOC_DIAG=1`); debug/production
  distinction proven per slice, owed as one merged whole at
  packet time (INC-J005-03 / INC-J008-01). M-002/M-003 must
  re-check the merged-tree state; M-009 carries the rest.
- Phase 11 exit — PARTIAL (K-009 packet: 6 MET / 2 PARTIAL /
  0 UNMET of its X-K items; RSS/PSS and energy PARTIAL).
  Headline, stated plainly: the allocator loses to glibc on
  all measured workloads — about 5x to about 65x across the
  measured shapes in the banked packets; batching loses
  everywhere measured in K-007; tail p99 is far worse than
  glibc on the burst shapes. Owner: SRC-19 (public register,
  M-008). No Wave 13 slice may claim this is fixed without
  new measured, banked evidence.
- Phase 9 exit — PARTIAL, carried unchanged: soft budgets
  defined, enforcement adopted by no slice (see INC-I001-14);
  fragmentation item PARTIAL (F-5 unmeasurable without a
  budget). Owner: public register (M-008).
- INC-I001-14 — budget enforcement not adopted: no budget
  exists in the allocator; D-FAIL-BUDGET is 0-by-absence.
  Carried, not discharged. Public register (M-008).
- INC-I009-01 — `docs/SUPPORTED_TARGETS.md` freshness:
  the file is stale relative to banked target evidence
  (still claims documentation-only). ASSIGNED to M-006,
  which discharges or re-owes it. Blocks SRC-09.
- K-007 P-1 — batching proposal P-1 recorded, not
  implemented. Carried; no Wave 13 slice is assigned it;
  M-009 lists it as unresolved in the public register.
- INC-G008 / INC-H001-01 — stale growth check: H-001 fixed
  the INC-G008-02 unload orphan defect (drain at unload,
  growth exactly 0 B/cycle), which inverted the banked
  G-008 harness's stale growth PASS-check: standalone
  `make test-g008` stands at 214 checks / 1 failure by
  design of the fix, reproduced in the Phase 8 and Phase 10
  packets. `test_h001` supersedes it for unload behaviour.
  Re-pointing the check is an evidence-policy decision
  still owed. Owner: SRC-08; decision owed before M-009.
- SONAME / install target — owed since Phase 5
  (INC-E002-01, INC-E003-06, INC-E004-01): the built `.so`
  has no SONAME, no versioned file name, no `install`
  target and no ldconfig story; consumers' `DT_NEEDED`
  names the plain filename. Owner: M-007 must record the
  decision (adopt or publicly decline). Blocks SRC-13.
- dlopen / R3 route — owed by decision since Phase 5
  (E-001 S5.2, O-10): route R3 (`dlopen`/`dlsym`, including
  dlopen-time construction and ownership) was untested by
  decision; the proven route is R2 (dynamic link) with R1
  (static) also proven. Note the G-008/H-001 harness does
  exercise dlopen/dlclose cycles for unload behaviour;
  that is not an R3 consumer-route proof. Owner: M-002
  audit must state this distinction; any R3 proof needs a
  deliberately commissioned slice.
- Phase 1 exit unmet and Phase 6 Clause A UNMET / X1
  BLOCKED (blocker-record alternative only) are carried
  unchanged from earlier packets (restated in G-001/G-009);
  nothing in Wave 13 discharges them. Public register.

## 5. INC-M001 register (found by this slice)

- INC-M001-01: the release gate cannot be `make test` alone:
  standalone `test-g008` carries a known 214/1 failure
  (INC-H001-01) while `make test` passes 4590/0. SRC-08
  exists because of this divergence; M-009 must show both.
- INC-M001-02: Phase 8 E2's PARTIAL reason (H-008 banking
  owed) and Phase 7 X4/X6's PARTIAL reasons (pre-H-001)
  may have been discharged by later banking, but no
  re-grade packet exists in the banked tree, so the last
  graded verdicts stand until M-009 verifies and records
  the current state. A stale PARTIAL is still the grade
  of record — claiming MET without a packet would be worse.
- INC-M001-03: INC-L009-01 (the `.so` is absent after
  `make test`) makes ABI verification order-dependent;
  SRC-02 therefore names `make lib` explicitly.
- INC-M001-04: M-007 cannot publish versioned artifacts
  honestly until the SONAME/install decision (owed since
  Phase 5) is recorded; the queue order M-007 before
  M-009 is therefore load-bearing, not cosmetic.

## 6. What Wave 13 does not claim

- No stable release is claimed by this plan. Whether the
  Section 3 criteria are met is M-009's graded verdict,
  on banked evidence only.
- No default global interposition is claimed or planned
  as validated.
- No x86-64, Android, or Qwen3/Aesir comparison result is
  claimed; each is owed for the named reason above.
- The allocator remains experimental/research, and loses
  to glibc on all measured workloads as banked in Phase 11.

*End of M-001.*
