# Phase 3 — Portable Unit/Contract Test Suite Plan

> **HONESTY BANNER — DESIGN ONLY.** This document is design only.
> Nothing in it is implemented, nothing in it is measured, and
> no test in it has been run. Every test case named here is a
> **PLANNED** test: a specification, not a result. Status:
> **Proposed**. Slice: **B-009**. Base commit: **243f007**.
> Every contract row, hook, and architecture layer cited here
> is itself **Proposed**; no cited row is adopted or
> implemented fact (INC-B006-10).

## Document control

| Field | Value |
|---|---|
| Deliverable (when integrated by the lead) | `docs/PHASE_3_TEST_SUITE_PLAN.md` |
| This part | Builder A part: front matter, Sections 1–6, Appendix B |
| Evidence label throughout | **Proposed / design only** |
| Status | **Proposed** |
| Slice | B-009 (TEAM-09) |
| Base commit | 243f007 |
| Implementation code in this document | None |
| Tests run by this document | None |
| Measurements claimed by this document | None |

Source documents and their statuses (all **Proposed**, none
adopted fact): `docs/contracts.md` (91 primary MA-* rows);
`docs/ARCHITECTURE_LAYERS.md` (ARCH L1–L7, **Proposed**);
`docs/MimirAlloc_Research_and_Development_Roadmap.md`
(Phase 3, S9.1, S9.3, S10);
`docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md` (B-006, design
only, **Proposed**); the B-007 and B-008 designs
(`docs/PHASE_3_ARITHMETIC_ALIGNMENT_INIT_ERROR_DESIGN.md`,
`docs/PHASE_3_CALLOC_REALLOC_AND_UNSUPPORTED_BEHAVIOR.md`
— banked on main at c1f71e9 and reconciled against this
plan at B-009 banking, see S12 INC-B009-10); decision **D-006** in
`docs/IMPLEMENTATION_LANGUAGE_DECISION.md` (C11 build model);
`docs/SUPPORTED_TARGETS.md`.

**Reconciliation duty — PERFORMED at banking.** This plan
derives from `docs/contracts.md` directly. The B-007 and
B-008 designs were building in parallel when the draft was
written; at B-009 banking the plan was reconciled against
their final banked texts (main c1f71e9) and the outcome —
two drifts found and fixed in-slice, the rest verified
aligned — is recorded in S12 (INC-B009-10, INC-B009-11),
never silently patched (ASSIGNMENT item 6).

## 1. Scope and honesty

### 1.1 What this plan is

This is a plan for a portable unit/contract test suite for
Phase 3 of MimirAlloc. It specifies, at design level only:
the suite architecture, the case-ID scheme, the harness
components, how the identical suite runs on both exit
targets, how contract tests are derived from
`docs/contracts.md`, what evidence each case must produce,
and what evidence must exist before Phase 3 may be claimed
exited. It contains no implementation code and no test code,
and it presents no planned test as a run test.

The words *pass* / *passing* appear in this plan only as
(a) a defined pass criterion for a future run, or (b) the
future exit condition. They never appear in the past tense
about MimirAlloc.

### 1.2 Supported-call scope tested — stated before any pass claim

Per B-006 S13 item 1, the supported-call list is stated here,
before any pass claim anywhere in this plan. The Phase 3
minimal allocator's only public entry points are the design
labels `mimir_malloc` and `mimir_free`, and the supported
calls are exactly:

- `mimir_malloc` with size > 0 and representable (admitted at
  L2; oversize = defined failure), **by analogy** to
  MA-6.1-MALLOC-01, MA-6.1-MALLOC-02, and MA-6.1-MALLOC-04;
- `mimir_free` of a live in-scope pointer, or of NULL, **by
  analogy** to MA-6.1-FREE-01, MA-6.1-FREE-02, and
  MA-6.2-010.

Zero-size behaviour is **not** on this list:
MA-6.1-MALLOC-03 / U-09 are unresolved, and this plan gives
zero-size no behaviour. **Nothing else is supported.** In
particular, the namespaced pair has no contract rows of its
own (U-03, INC-B006-1): every citation of the pair in this
plan is **by analogy** and is labelled so.

Later groups in this plan (B-007 arithmetic/alignment, B-008
`calloc`/`realloc`) test the standard-name contract rows in
`docs/contracts.md`.

**B-008 amendment (reconciled at banking, S12 INC-B009-10).**
The final B-008 design (S7.2) amends the Phase 3
supported-call list itself: it adds `mimir_calloc(count,
size)` with both factors > 0 and a representable, satisfiable
product (overflow / unsatisfiable = defined failure), and
`mimir_realloc(ptr, n)` with `ptr` live and in-scope or NULL
and `n > 0` (failure preserves the original) — each **by
analogy under U-03**, exactly as for the pair above, since
the namespaced names have no contract rows of their own
(B-008 S1.2, INC-B008-2). Zero-size behaviour stays off the
list (U-09). The amended list is a **design list**: it states
what a future implementation is designed to support on a
named target and supports nothing anywhere yet (B-008 S7.2,
N-11). The S8.3 cases therefore test the B-008 surface as
finally designed — the standard-name rows and, by the same
analogy, the namespaced labels — and no pass claim for any
namespaced name may be read as a direct contract claim.

### 1.3 Explicit non-claims

This plan does **not** claim: an implementation exists; any
test has run; an ABI, signature, symbol version, or stability
promise for any name (U-03); any performance property; fork
safety (U-17..U-21 open, INC-B006-7); async-signal safety
(MA-6.3-030..032); or any non-Linux target. Planned tests are
**PLANNED** — never worded as run.

### 1.4 Exit statement

**Phase 3 exit is UNMET.** It stays UNMET until an
implementation exists **and** the suite passes on **both**
exit targets (Section 3.1), with the exit evidence defined
in Section 10 of the integrated plan (B-006 S13 items 1–10,
extended) recorded on both targets.

## 2. Source basis and ID-namespace glossary

### 2.1 Source basis

| Source | What this plan takes from it | Status |
|---|---|---|
| `docs/contracts.md` | 91 primary MA-* rows (S2–S4); hooks C-001..C-010 (S6); C-3nn/C-4nn slots (S3–S4); V-01..V-07 (S2.11); U-01..U-21 (S7) | **Proposed** |
| Roadmap S8 (Phase 3), S9.1, S9.3, S10 | Exit sentence, invariants as oracles, controlled-failure list, 10-row test matrix + closing fuzz/differential paragraph | **Proposed** |
| B-006 design (S1–S3, S10, S12, S13, S14) | Supported-call list (S13 item 1), failure rows F-01..F-12 (S10), exit items 1–10 (S13), INC-B006-1..10 (S14) | **Proposed / design only** |
| B-007 / B-008 designs (banked, main c1f71e9) | Arithmetic/alignment/init/error detail; `calloc`/`realloc` semantics + amended supported list (B-008 S7.2) — reconciled at B-009 banking (S12 INC-B009-10) | **Proposed / design only** |
| D-006 (`docs/IMPLEMENTATION_LANGUAGE_DECISION.md`) | Clean C11 build model (S4.5) | Decision, **Proposed** suite |
| `docs/SUPPORTED_TARGETS.md` | Target matrix; x86-64 evidence on record today: none for MimirAlloc | **Proposed** |

Terminology is bound by **MA-6.2-040** and **MA-6.2-041**:
the terms *owner*, *block*, *internal logical page*, *system
page size*, *span*, and *huge page* are used exactly as in
the contracts.md definitions box; any page quantity must be
qualified as internal logical page or system page size before
contract use. An internal logical page is an allocator
concept; a system page size is a host property. No rule in
this plan equates the two.

### 2.2 ID-namespace glossary — never conflated (INC-B006-9)

Six-plus namespaces appear in this plan. They are **never**
interchangeable, and every derivation row names the scheme
it cites:

| Namespace | Shape | What it is | What it is not |
|---|---|---|---|
| Contract IDs | `MA-6.1-*`, `MA-6.2-*`, `MA-6.3-*`, `MA-COND-*` | Behaviour rows in contracts.md S2–S4, all **Proposed** | Not hooks; not test cases |
| S6 hooks | `C-001`..`C-010` | Table 6-1 test hooks in contracts.md S6.1 only | Not the C-3nn/C-4nn slots; not contract rows |
| S3/S4 slots | `C-3nn`, `C-4nn` | Per-row slots in contracts.md S3/S4 tables (C-301..C-354, C-401..C-442) | Not entries in Table 6-1 |
| Named hooks | `MA-MALLOC-*`, `MA-FREE-*`, `MA-CALLOC-*`, `MA-REALLOC-*`, `MA-ALIGNED-*`, `MA-POSIXMA-*`, `MA-COND-*` | Named hooks attached to individual MA-* rows in contracts.md S2 | Not C-001..C-010; not C-3nn/C-4nn |
| Roadmap issue ID | `MIM-005` | Roadmap S13 suggested issue ID for the B-006 minimal-allocator design | Not a contract ID; not a hook |
| Failure rows | `F-01`..`F-12` | B-006 S10 design failure rows for the namespaced pair | Not contract IDs; not hooks |
| Open items | `U-01`..`U-21`, `V-01`..`V-07` | Unresolved decisions / per-target variations in contracts.md S7 / S2.11 | Not defects; not test results |
| Case IDs | `P3-T-<AREA>-<nnn>` | This plan's own test-case IDs (S4.2) | Not contract IDs; carry no architecture, page-size, or result suffix |

Two honesty consequences are recorded here, not resolved:

- **Hook coverage gap (scout NEW-1).** Table 6-1 maps only
  ten hooks; most [NORM] rows have no C-001..C-010 hook and
  rely on named hooks or C-3nn/C-4nn slots, and contracts S6.1
  itself says further rows are "to be added". The contracts
  S6 mapping rule is therefore currently unsatisfied, and
  this plan must never claim "S6 conformance" from
  C-001..C-010 alone.
- **Namespaced gap (INC-B006-1).** `mimir_malloc` /
  `mimir_free` have **no** contract rows (U-03). Every test of
  the pair in this plan is derived **by analogy** to MA-* rows
  and labelled so, row by row.

## 3. Target model

### 3.1 Exactly two exit targets

Phase 3 exit has exactly two targets (Roadmap Phase 3 exit
wording; B-006 S13 item 3):

| Exit target | Role (Roadmap S5) |
|---|---|
| x86-64 Linux (+ glibc) | Development / reference platform |
| ARM64 Linux (+ glibc) | First candidate platform |

musl, bionic, Android, Termux, and proot are **not** Phase 3
exit targets. They get **no annex in this plan** (S4.4). That
is recorded here, not silently dropped: the Roadmap S10
Platform row names some of them, and Section 6 records that
row as only partly mappable in Phase 3 (scout NEW-5). No
evidence gathered on any other environment may be worded as
satisfying either exit target.

### 3.2 Runtime system-page-size discovery — on every target

The system page size is a **host property**. It must be
discovered at runtime on **every** target — for example via
`sysconf(_SC_PAGESIZE)` — and the discovery must be
**failure-checked**. Discovery failure is itself a case:
by analogy to B-006 failure row F-11, a failed or unusable
discovery is a defined failure, with no mapping attempted
and **no fallback to any hardcoded constant** — never to
4096 or to any other literal (INC-B006-3).

No literal 16384 and no literal 4096 appears anywhere in
suite sources or case expectations. A case's expected values
are expressed against the runtime-discovered system page
size recorded in that run's target manifest (S4.4, plan S9).

### 3.3 Observed facts — this Pi only (dated)

Observed on Raspberry Pi device **purpleviking1** on
**2026-11-06** (scout re-run for B-009):

- `getconf PAGESIZE` = **16384**
- `uname -m` = **aarch64**
- `gcc --version` (first line): **gcc (Debian 12.2.0-14+deb12u1) 12.2.0**

These are dated observed facts **for this Pi only**. 16384
is never a universal assumption, never a target property,
and never an expected value in any case. (B-006 S3 recorded
the same 16384 on this Pi via `getconf PAGE_SIZE`; the
spelling differs, the host property is the same.)

**Contrast:** the SM-X400 tablet probed a **4096** system
page size (B-006 S3). Same design, different system page
size — proof by example that the value is a host property
(MA-6.2-041).

### 3.4 The x86-64 leg is OWED in full

**No x86-64 host exists in this campaign.** The only physical
target in hand is the aarch64 Pi above, and
`docs/SUPPORTED_TARGETS.md` records, for Linux x86-64 +
glibc: "Evidence on record today: None for MimirAlloc."

Therefore the x86-64 leg of Phase 3 exit is **OWED** evidence
in full (B-006 S13 item 3). No ARM64 or Pi result may be
worded as covering it. When a named x86-64 environment
exists, its record must contain: machine identification, OS
and version, kernel, libc and version, toolchain (compiler
and version, flags), the suite commit and build hash, and
its own runtime-discovered system page size record (S3.2).
Until such a record exists, the standing wording is: **x86-64
leg OWED**.

### 3.5 Per-target annex mechanism

Target differences live **only** in per-target annexes plus
target manifests — never in forked case copies. The mechanism
is specified in S4.4 and the annex template is Appendix B.
A case whose precondition cites a V-/U-item is instantiated
per target **only** through its annex; an absent annex
decision leaves the case **BLOCKED** on that target.

## 4. Suite architecture

### 4.1 Single source tree

There is **one** suite source tree and **one** case registry,
built per target from identical sources. The layout below is
a directory sketch at design level only — labels, not code:

| Path (design label) | Contents |
|---|---|
| `registry/` | The single case registry: every case ID, its area, group, contract ID(s), hook ID + scheme, status |
| `cases/` | Case specifications, one home per case, organised by AREA (S4.2) |
| `harness/` | Runner, evidence collector, differential comparator (S4.3) |
| `shim/` | Fault-injection shim on the L6 mapping boundary (S4.3b) |
| `watchdog/` | Bootstrap/reentrancy watchdog (S4.3c) |
| `fuzzer/` | Ownership-model fuzzer specification and model (S4.3d) |
| `annexes/` | Per-target annexes, one per exit target (S4.4, Appendix B) |
| `evidence/` | Per-run evidence records and artefact manifests (plan S9) |

Target differences live **only** in `annexes/` and in target
manifests. There are no forked case copies per target.

### 4.2 Case-ID scheme (frozen)

Form: `P3-T-<AREA>-<nnn>` (shape example only:
`P3-T-MAP-001`). P3 = Phase 3; T = test case. `nnn` =
001..999 within AREA, assigned in blocks of 10 per contract
sub-group or hook so parallel builders never share a block;
gaps are allowed and expected. An ID, once assigned, is
never reused or renumbered, even if the case is withdrawn
(withdrawal is a status; the registry keeps the row).

No architecture, page-size, or result suffix ever. The only
suffix is `-V<nn>`, a per-target-policy variant keyed to a
V-item (Section 5).

AREA registry and owners (frozen partition): SYS and DIFF —
Builder A; MAP, FAIL, DATA, ARITH, ALIGN, INIT, ERR —
Builder B; CAL, REA, COND, UNS — Builder C; EXIT is reserved
(no builder writes EXIT cases in B-009). A case lives in
exactly **one** area. Concurrency/cross-thread cases
(C-009, MA-6.2-054) are written **once** in ERR and cited,
not duplicated, elsewhere. Fuzzer-generated sequences are
not case IDs: the fuzzer specification is SYS, and a fuzzer
run is evidence under the case that invoked it.

Case IDs minted in this Builder A part (SYS/DIFF only; all
other areas are cited in Section 6 as AREA + contract ID
and resolved at integration from the B/C registries):

| Case ID | What it specifies (PLANNED) |
|---|---|
| P3-T-SYS-001 | The case registry loads and every registered ID is well-formed under this scheme |
| P3-T-SYS-002 | A per-target annex applies: the runner instantiates exactly the cases the annex admits, and BLOCKED cases stay BLOCKED |
| P3-T-SYS-003 | Evidence schema validation itself: a record missing any mandatory field (plan S9) is rejected by the collector, never counted as PASS |
| P3-T-DIFF-001 | Paired same-case records from both targets compare equal on defined observable behaviour only (Section 5) |
| P3-T-DIFF-002 | A divergence on defined observable behaviour produces a DIVERGENCE record against the case ID — never averaged, never resolved by preferring the ARM64 result |

### 4.3 Harness components

Each component below states its purpose, its interface at
design level, and its failure semantics. All are **PLANNED**;
none exists yet.

**(a) Runner.** Enumerates the registry, applies the target's
annex, executes one case per isolated process, enforces a
per-case timeout, captures stdout/stderr/exit status, and
emits the plan S9 evidence record for that case, target, and
run. A runner crash is a case **ERROR** (harness), never a
PASS.

**(b) Fault-injection shim + recovery probe.** Interposes on
the L6 mapping boundary to fail the **Nth** mapping attempt
(N swept over a stated range), and likewise injects
metadata-allocation failure (Roadmap S9.3, first line). The
design states, for every injection case: the counting origin
(which attempt is attempt 1), the N sweep range rule, an
uninjected control run, and the **recovery probe** — the next
uninjected attempt must succeed (B-006 S13 item 5). Builder B
specifies what the shim must do per case (N values, ranges);
this section specifies the shim itself.

**(c) Watchdog (bootstrap tests).** Places a finite bound
around bootstrap/reentrant cases for hook **C-010**
(contracts S6.3). **Expiry = FAILURE**, never inconclusive.
The bound's value is itself **[UNRESOLVED: U-05]**, so
watchdog cases carry status **BLOCKED-by-U-05** for promotion
purposes, and any early result under a provisional bound is
labelled **preliminary** per contracts S6.3 (scout NEW-7).
Builder B specifies per-case bounds needs; this section
specifies the watchdog itself.

**(d) Ownership-model fuzzer.** Per the Roadmap S10 closing
paragraph: generates seeded sequences over allocate, write,
reallocate, transfer (cross-thread), free, trim, and thread
exit, checked against a simple ownership model — one owner,
retire exactly once, no overlap, live bytes intact. Seeds are
saved and traces are versioned. A model violation is a FAIL
with the seed + trace as the artefact.

**Operation gating (honesty rule).** The fuzzer may only
generate operations that are in the supported-call list
(S1.2) or in an in-scope later group. In particular, **trim
has no Phase 3 operation**: no trim entry point is in the
B-006 supported list (U-03; trim/size classes are Phase 7
per B-006 S12.3), so trim is **excluded** from Phase 3
fuzzer sequences and recorded as excluded, not silently
dropped (scout NEW-5f). Reallocate sequences apply only
where the B-008 group is in scope. The fuzzer specification
must gate operations per the supported-call list; it must
never assume an operation exists.

**(e) Differential comparator.** Pairs same-case evidence
records from both targets and compares **only** defined
observable behaviour under the Section 5 rules. It emits the
differential report artefact (plan S9/S10 item 15).

**(f) Evidence collector.** Validates every record against
the plan S9 schema and builds the artefact manifest. A
schema-incomplete record is **no record**: the collector
rejects it, and it can never support a PASS or an exit item.
No artefact is evidence without its SHA-256 hash plus the
record that references it.

### 4.4 Per-target annex mechanism

One annex per exit target — `ANNEX-X86-64` and
`ANNEX-ARM64` — in the fixed template of Appendix B. Each
annex records: the target manifest fields; the selected
policies for V-01..V-07; the disposition, for that target,
of U-01, U-02, and U-09..U-15; the watchdog bound (U-05);
symbol-set presence (U-01); and the page-size record (S3.2).

A case whose precondition cites a V-/U-item is instantiated
per target **only** through its annex. An absent annex
decision means the case stays **BLOCKED** on that target.
Annexes for non-exit environments (S3.1) do not exist in
this plan.

### 4.5 Build model

Clean **C11 / D-006** build per target: portable C11 core, C
ABI, no C++ in the core. Compiler, version, and flags are
recorded in **every** evidence record (B-006 S13 item 8).
Toolchain probes are not build evidence.

Tooling posture: ASan/UBSan/TSan/Valgrind are run where
compatible, with the Roadmap S10 Tooling caveat written into
every tooling record — **running a tool is not coverage**;
custom-allocator visibility limits and unsupported
combinations must be named in the record, never glossed.

## 5. Identical execution on both architectures

- **Same sources, same case IDs, same registry** at the same
  suite commit on both targets. Build hash and suite commit
  are evidence-record fields, so non-identity is detectable,
  never assumed away.
- A case ID never carries an architecture suffix. Target is
  an evidence-record field and an annex instance — never a
  new case. The per-target annex (S4.4, Appendix B) is the
  **only allowed difference** between the two executions.
- Per-target-policy instances (hook C-006 zero-size /
  `realloc(p,0)`) get a variant suffix **only**: `-V<nn>`
  keyed to the V-item, one instance per documented target
  policy. A result matching **another** target's policy is a
  **FAILURE** (Table 6-1, C-006 pass criterion, in substance:
  the behaviour must match the documented selected per-target
  policy exactly).
- **Differential comparison covers DEFINED OBSERVABLE
  BEHAVIOUR ONLY:** return-value class; ownership outcome;
  byte contents; counters returning to baseline; defined
  failure vs success. The errno/error channel is compared
  **only** where a per-target contract promises it — V-02 /
  U-10 are open, and B-006 promises **no** errno for the
  namespaced pair (scout NEW-3), so no errno comparison is
  specified for it.
- **NEVER compared:** pointer addresses; absolute timing or
  durations; mapping addresses or layout; thread scheduling
  order. These may be recorded as diagnostics, labelled
  **non-oracle**.
- A cross-target divergence on defined behaviour is recorded
  as a **DIVERGENCE** record against the case ID (case
  P3-T-DIFF-002). It is never averaged away and never
  resolved by preferring the ARM64 result.

## 6. Roadmap S10 matrix mapping

Completeness rule: every one of the 10 Roadmap S10 rows maps
to at least one named case ID, or is expressly recorded
**NOT-YET-MAPPABLE** with the blocking U-/phase item.

Citation rule for this section (Builder A partition): Builder
A mints case IDs in the SYS and DIFF areas **only**. Rows
owned by Builders B/C are cited as **AREA + contract ID**;
their concrete case IDs are resolved at integration from the
B/C registries in Appendix A of the integrated plan. No
MAP/FAIL/DATA/ARITH/ALIGN/INIT/ERR/CAL/REA/COND/UNS case ID
is minted in this section. Status semantics are frozen:
**READY** means fully specifiable now — it never means
passed; **BLOCKED-by-U-nn** names the exact U-item(s).

| S10 row | S10 minimum-test wording | Case ID(s) | Area | Target(s) | What success means | Status |
|---|---|---|---|---|---|---|
| C contracts | "zero-size by target policy, NULL, overflow, errno, alignment, realloc copy/failure" (verbatim) | AREA + contract cases for MA-6.1-MALLOC-01/-02/-04, MA-6.1-FREE-01/-02, MA-6.2-020..024, MA-6.1-ALIGNED-*, MA-6.1-POSIXMA-*, MA-6.1-CALLOC-*, MA-6.1-REALLOC-*; hooks C-001..C-008 as cited per row in plan S7/S8 | MAP, ARITH, ALIGN, ERR, CAL, REA, COND | Both exit targets | Meets the documented selected ABI | READY in part; **BLOCKED-by-U-09/U-12** (zero-size, realloc(p,0)), **BLOCKED-by-U-10** (errno), **BLOCKED-by-U-11/U-15** (alignment validity/numerics) |
| Data integrity | "fill unique patterns, random sizes, check live allocations repeatedly" (verbatim) | AREA + contract cases for the B-006 S13 item 4 cycles (by analogy MA-6.1-MALLOC-01, MA-6.1-FREE-01); fuzzer spec P3-T-SYS-001..003 governs seed/trace records | DATA (with SYS) | Both exit targets | No unexpected changes to live bytes | READY (specifiable now, NOT run) |
| Ownership | "same-thread, cross-thread, thread exit, shared object boundaries" (verbatim) | AREA + contract cases for MA-6.2-001/-010/-054, hook C-009 — cross-thread cases written once in ERR and cited here, not duplicated; hook C-005 (free(NULL)) cases in FAIL/MAP per B's home | ERR, MAP, DATA | Both exit targets | Every valid allocation can be released exactly once by the correct owner | READY in part; thread-exit/shared-object-boundary cases **BLOCKED-by-U-05/U-17..U-21** where they depend on fork/thread-exit policy specifics |
| Concurrency | "thread churn, randomized operations, low/high contention, repeated seeds" (verbatim) | AREA + contract cases for MA-6.2-050..054, hook C-009, written once in ERR and cited here; minimal-concurrency cases per B-006 S13 item 6 in ERR/FAIL where F-rows touch them | ERR (cited by FAIL) | Both exit targets | No reproducible races, overlaps, hangs or incorrect ownership | READY for the minimal S13 item 6 subset (correctness only, no performance claim); wider churn/contention matrix is PLANNED under the same ERR cases |
| Memory | "repeated cycles, fragmentation, cache drain and trim, large mapping pressure" (verbatim) | AREA + contract cases for B-006 S13 item 4 repeated cycles (counters return to baseline) in DATA | DATA | Both exit targets | Counters and OS metrics align with their stated meanings | **NOT-YET-MAPPABLE in part**: repeated cycles/baseline are READY via DATA; cache drain and **trim have no Phase 3 operation** (L3 pass-through, L7 compiled out; trim is Phase 7, U-03) and fragmentation/large-mapping-pressure have no S13 exit item (scout NEW-5a) — recorded as deferred, never passed |
| Robustness | "failing OS calls, OOM, interrupted workloads, malformed debug inputs" (verbatim) | AREA + contract cases for B-006 failure rows F-01..F-12 (hooks C-005/C-009/C-010 cross-cited, named hooks), incl. the fail-the-Nth-mapping sweep + recovery probe (S4.3b); INIT cases for F-08/C-010 (watchdog, S4.3c) | FAIL, INIT, ERR | Both exit targets | Defined failure or diagnostic behavior; no false success | READY in part; C-010 watchdog cases **BLOCKED-by-U-05** for promotion (preliminary per contracts S6.3); fork/interrupted-workload cases **BLOCKED-by-U-17..U-21** |
| ABI/linking | "C and required C++ symbols, shared/static, dynamic loader" (verbatim) | No case IDs minted: beyond the namespaced pair there is nothing to map in Phase 3 | — (none in B-009) | Both exit targets (when mappable) | Actual consumer resolves intended implementation | **NOT-YET-MAPPABLE**: the namespaced API has no ABI or contract rows (U-03), the final symbol set is undecided (U-01), C++ operator set is undecided (U-06), and interposition is Phase 5 (scout NEW-5b) |
| Platform | "AArch64 Linux, native Android, Termux, optionally proot" (verbatim) | P3-T-SYS-002 (annex applies, per-environment attribution); P3-T-SYS-003 (manifest/schema validation); AREA + contract cases run under ANNEX-ARM64 | SYS (with all areas under annex) | ARM64 Linux annex only in Phase 3; x86-64 annex OWED (S3.4) | Results attributed to the exact environment and target | READY for per-environment attribution on the two exit targets only; the row's Android/Termux/proot environments are **NOT-YET-MAPPABLE as exit evidence** — they are not Phase 3 exit targets (S3.1, scout NEW-5e). Success is per-environment, never pooled |
| Tooling | "ASan, UBSan, TSan and Valgrind where compatible" (verbatim) | P3-T-SYS-003 (tooling records validated against plan S9, incl. the not-coverage caveat of S4.5) | SYS | Both exit targets | Explain custom allocator visibility and any unsupported combinations; avoid claiming coverage merely because the tool ran | READY as a recording specification (NOT run); no coverage is claimed by any tooling row |
| Regression | "every confirmed defect gets a minimal reproducer" (verbatim, faithful shortening) | No case IDs minted at design time: regression cases are created **only** from confirmed defects, under the regression-case creation RULE (plan S10 item 18 register) | — (rule only in B-009) | Both exit targets (when a defect exists) | The exact defect is caught without relying only on synthetic tests | **NOT-YET-MAPPABLE to cases**: the reproducer-per-defect register is empty at design time, and that emptiness is a stated fact, not a gap (scout NEW-5d) |

Notes on this mapping (recorded, not silently resolved):

- **Fuzz/differential paragraph (S10 closing).** Mapped to
  the fuzzer specification in S4.3d (SYS: P3-T-SYS-001..003
  govern its records; the sequences themselves are not case
  IDs) and to P3-T-DIFF-001 / P3-T-DIFF-002. Trim is gated
  out of Phase 3 sequences (S4.3d, U-03).
- **S9.3 controlled failures beyond the Nth mapping.** Only
  one of the eight S9.3 tests is carried by B-006 S13 item 5
  (scout NEW-6). The remaining seven are either scheduled
  under the FAIL/INIT/ERR citations above or expressly
  deferred in the integrated plan's cannot-prove-yet section
  — S13 alone does not discharge S9.3.
- **Exit scope note (scout NEW-4).** B-006 S13 item 2 names
  only C-005, C-009, C-010 + the MA-MALLOC/MA-FREE named
  hooks, consistent with its narrow S13 item 1 list. Because
  this plan is the Phase 3 exit suite across the B-006/B-007/
  B-008 split (INC-B006-4), the C contracts and Robustness
  rows above also cite the B-007/B-008 hook evidence
  (C-001..C-004 at minimum). Read without that addition, S13
  would under-scope the exit.

## 7. Contract-test derivation method and completeness audit method

### 7.1 Derivation method (applied identically in Sections 8.1-8.4)

For each source row, in this order:

1. **Identify the source row and its classification** in docs/contracts.md
   Sections 2-4: `[NORM]`, `[COND: <gate>]`, unresolved (a U-item is named
   and no behaviour is stated), or UB. Gaps in the MA numbering are not
   rows and are never derived (e.g. MA-6.2-006..009 do not exist).
2. **Label the analogy.** Every contract ID cited is Proposed. The
   namespaced pair `mimir_malloc` / `mimir_free` has NO contract rows
   (U-03, INC-B006-1): every derivation that targets the pair is BY
   ANALOGY under U-03 and says so in its Oracle cell. Standard-name rows
   (calloc, posix_memalign, ...) are derived as written for those names
   and are never silently re-pointed at the namespaced pair.
3. **Select the hook(s), naming the scheme** (contracts S1.5; the three
   schemes are never conflated): `C-0nn` = Table 6-1 hook (contracts S6);
   `C-3nn` / `C-4nn` = per-row slot in contracts S3 / S4; `named` = the
   MA-* named hook in contracts S2; `none` = no hook exists. A row may
   carry hooks from more than one scheme; each is listed with its scheme.
4. **Mint case IDs** under the frozen scheme `P3-T-<AREA>-<nnn>`, in
   blocks of 10 per contract sub-group or hook, gaps allowed, an ID once
   assigned never reused or renumbered. No architecture, page-size, or
   result suffix; only `-V<nn>` per-target-policy variants keyed to a
   V-item. BLOCKED rows are minted an ID (they are written as blocked
   tests). NO-TEST-UB rows are minted NO case ID.
5. **Write the precondition**, including any per-target annex decision
   the case needs (V-/U-item disposition). Absent annex decision = the
   case stays BLOCKED on that target, whatever its plan-level status.
6. **Name the oracle**: source document + row, and its kind (contracts
   row / POSIX baseline / per-target variation / B-006 analogy under
   U-03 / Roadmap invariant or controlled-failure list / B-006 failure
   row). Only defined observable behaviour is ever an oracle: return-
   value class, ownership outcome, byte contents, counters returning to
   baseline, defined failure vs success. Pointer addresses, absolute
   timing, mapping layout, and thread scheduling order are never oracles
   (they may be recorded as diagnostics, labelled non-oracle).
7. **Write the pass criterion** as a future condition only. The words
   pass/passing appear nowhere in this part as a past-tense claim.
8. **Name the evidence produced** (record + artefact names per plan S9;
   shorthand in 7.4 below). A record missing a mandatory S9 field is not
   a PASS record.
9. **Assign exactly one status** from the frozen taxonomy (7.2).

### 7.2 Status taxonomy (frozen; identical in every derivation table)

- **READY** — fully specifiable now, NOT run. A READY row states a
  complete precondition, oracle, and pass criterion today.
- **BLOCKED-by-U-nn** — names the exact U-item(s); the row states what
  decision unblocks it. A BLOCKED case is written as a blocked test: it
  is never given a pass/fail on UB and never given an expected result
  the unresolved item has not supplied (ASSIGNMENT item 2c: U-09/U-12).
- **NO-TEST-UB** — contracts S6.2 verbatim rule: no hook, no expected
  result. Debug/sanitiser observations may be specified only as
  DIAGNOSTIC-ONLY, in a separate diagnostic case where one exists, and
  are separated from conformance in every record and ledger.

Two standing qualifications ride on otherwise-READY rows and are stated
in the row itself, never hidden: (a) hook C-010 results are PRELIMINARY
per contracts S6.3 while U-05 is open (scout NEW-7) — bootstrap rows
whose execution needs the watchdog bound are BLOCKED-by-U-05 for
promotion purposes; (b) per-target instantiation of any row citing a
V-/U-item happens only through that target's annex (plan S4.4).

### 7.3 Two cross-cutting oracle prohibitions

- **errno is never asserted for the namespaced pair.** errno on
  allocation failure is UNRESOLVED (V-02 / U-10 open), and B-006 S10
  expressly promises no errno for mimir_malloc / mimir_free. Table 6-1
  wording that imports errno (C-001 "errno as specified", C-006) is NOT
  carried onto the namespaced pair (scout NEW-3). errno may be recorded
  as a non-oracle diagnostic. The one safe channel is C-008 /
  MA-6.1-POSIXMA-02, where the error number is a return value and errno
  is expressly not the channel; even there the exact numbers are
  BLOCKED-by-U-14 (see P3-T-ALIGN-062).
- **No expected result is ever written for UB.** Invalid-pointer,
  double-free, use-after-free, and mismatched-free behaviours in
  release builds get NO-TEST-UB rows only. Debug-build detection of the
  same events is a different, conditional contract (MA-6.2-033) and is
  specified as DIAGNOSTIC-ONLY in P3-T-ERR-031 / P3-T-FAIL-081.

### 7.4 Evidence shorthand used in the tables of Sections 8.1-8.2

Each code expands to the plan S9 record for that case/target/run plus:

- `EV-R` — per-case stdout/stderr captures.
- `EV-S` — EV-R + fault-injection shim injection log (N, counting origin).
- `EV-W` — EV-R + watchdog log (bound applied, or U-05 still-open mark).
- `EV-T` — EV-R + thread-sequence trace (seeded, versioned).
- `EV-A` — EV-R + audit worksheet (source lines checked, verbatim quotes).
- `EV-D` — diagnostic capture set, labelled DIAGNOSTIC-ONLY, filed
  separately from conformance records; never a PASS record.

### 7.5 The NEW-1 hook-coverage gap and how this plan answers it

Contracts S6 mapping rule: every `[NORM]` and applicable `[COND:]` row
in contracts SS2-4 shall be mapped to at least one hook in Table 6-1 or
expressly recorded in S7 as unmapped with a reason; UB rows are "No test
hook — UB" (S6.2). That rule is currently UNSATISFIED, and S7 does not
record the unmapped rows either (scout NEW-1). Table 6-1 (C-001..C-010)
maps only a fraction of the NORM rows. In particular, for this part:

| Rows with NO C-001..C-010 hook | What actually maps them today |
|---|---|
| MA-6.1-MALLOC-01, -02, -04 | named hooks MA-MALLOC-SUCCESS / -FAILURE / -OVERSIZE only |
| MA-6.1-FREE-01 | named hook MA-FREE-SUCCESS only (C-005 covers FREE-02 only) |
| MA-6.1-ALIGNED-02; MA-6.1-POSIXMA-01 | named hooks MA-ALIGNED-FAILURE / MA-POSIXMA-SUCCESS; POSIXMA-01 is not even listed as a Contract ID in Table 6-1 (C-008 names POSIXMA-02; its pass text asserts success behaviour without mapping the row) |
| MA-6.2-001, -003, -004, -020, -021, -022, -023, -024, -040, -041, -050, -051, -052, -053 | C-3nn slots only (C-301..C-354 as listed per row in S8); only -010 (in C-005) and -054 (in C-009) appear in Table 6-1 |
| MA-6.3-005, -006, -010, -011, -012, -020, -021, -022, -023, -030, -031, -032, -040, -041, -042 | C-4nn slots only (C-401..C-442 as listed per row in S8); only -001..-004 appear, in C-010 |

Recorded for Builder C's groups under the same rule (their derivation
rows live in S8.3): MA-6.1-CALLOC-03 has no hook at all (C-001/C-002
cover overflow and zeroing only); MA-6.1-REALLOC-03 and -06 have no hook
(C-003/C-004 cover -02/-01 only); no COND row (REALLOCARRAY-01..03,
USABLE-01/02, CXX-01..08) has any C-001..C-010 hook.

Contracts S6.1 itself concedes the gap: additional rows, including the
C-3nn/C-4nn slots, are "to be added" with consecutive C-### IDs. This
plan therefore: (1) does NOT mint new C-### hooks — assigning them is
the contracts document's job, not a test plan's; (2) does NOT claim S6
conformance from C-001..C-010 alone — such a claim would over-claim;
(3) gives every NORM/COND row in scope a derivation row naming its
named hook and/or C-3nn/C-4nn slot as the hook of record, or records it
**unmapped-with-reason** in the group ledger; and (4) carries the gap
itself into the completeness audit (7.6) and the exit evidence (plan
S10 item 12), so the exit package shows the hook-to-row mapping rather
than asserting it (extending B-006 S13 item 2).

### 7.6 Completeness audit method (contracts S6 mapping rule, operationalised)

- **Denominator.** Every `[NORM]` row and every applicable `[COND:]`
  row in contracts SS2-4 (the MA-6.1 / MA-6.2 / MA-6.3 families),
  counted from SS2-4 themselves. Where S6 and SS2-4 differ, SS2-4
  govern (contracts S6 preamble) — stated here so the audit never
  audits against Table 6-1 alone. Unresolved rows and UB rows are in
  the denominator too; they leave it only via their recorded status.
  A `[COND:]` row is "applicable" for a target when its gate holds for
  that target per the annex; a gate that does not hold is recorded
  with the gate's U-item, not silently dropped.
- **Accounting categories (exactly one per denominator row).**
  (a) DERIVED — a derivation row in S8 names at least one case ID;
  (b) BLOCKED-by-U-nn — a derivation row names the blocking U-item(s)
  and the decision that unblocks it; (c) NO-TEST-UB — recorded under
  the S6.2 rule, no case ID; (d) UNMAPPED-WITH-REASON — expressly
  listed in a group ledger with the reason no case is derived in this
  plan (e.g. the serving case is owned by another builder's area and
  is cited by AREA + contract ID until integration, or the behaviour
  is out of Phase 3 scope with the deferring section named).
- **Procedure.** Two independent passes over SS2-4 (one forward from
  the contract tables, one backward from the Appendix A registry);
  every discrepancy is a finding, not an edit. The audit cross-checks:
  derivation rows against the registry (no case ID exists only in a
  table, or only in the registry); hook cells against 7.5 (no row
  claims a C-001..C-010 hook that Table 6-1 does not contain); status
  cells against 7.2 (no BLOCKED row carries an expected result; no
  NO-TEST-UB row carries a case ID); cross-area citations against the
  one-home rule (a cited case's full row exists exactly once).
- **Artefact.** The audit produces the completeness-audit artefact of
  plan S10 item 12: the full denominator list with its category per
  row, the unmapped-with-reason list verbatim, and the audit date and
  suite commit. Phase 3 exit item 12 is UNMET until that artefact
  exists with zero unaccounted rows. A worked mini-audit for the two
  groups in this part is the pair of ledgers closing S8.1 and S8.2.
- **Reconciliation duty — performed.** This plan derives from
  contracts.md directly; the B-007/B-008 final designs were
  building in parallel when the draft was written. At B-009
  banking the audit was re-run against those final banked
  designs and the outcome is recorded in plan S12
  (INC-B009-10), not silently patched.

## 8. Contract test cases by slice group (design)

This section holds the derived contract tests, one home per
case, organised by the Wave 2 slice whose design the cases
prove: S8.1 proves the B-006 mapping design, S8.2 the B-007
arithmetic / alignment / initialisation / error design,
S8.3 the B-008 calloc / realloc design, and S8.4 the
unsupported-behaviour refusals. Every case is derived by the
method of S7 from a cited contract ID in docs/contracts.md;
the derivation tables are the normative form of this plan.
B-007 and B-008 were still building in parallel when this
plan was written: S8.2 and S8.3 are therefore derived from
contracts.md directly (B-006 S12.1 / S12.2 confirm the
deferred mappings). Reconciliation against the final banked
B-007 / B-008 designs was performed at B-009 banking and is
recorded in S12 (INC-B009-8, INC-B009-10). No case in this section has been run;
READY never means passed.

## 8.1 B-006 group — mapping behaviour (MAP / FAIL / DATA areas)

Source-ID inventory (ASSIGNMENT item 2a, exact): MA-6.1-MALLOC-01,
-02, -04; MA-6.1-FREE-01, -02; MA-6.2-001, -010, -020, -024; B-006
failure rows F-01..F-12; hooks C-005, C-009, C-010 and the named
MA-MALLOC / MA-FREE hooks. All MALLOC/FREE derivations target
mimir_malloc / mimir_free BY ANALOGY under U-03 (the pair has no
contract rows) and only inside the B-006 S13 item 1 supported-call
list: size > 0 and representable; free of a live in-scope pointer or
NULL; nothing else. Zero-size is not in this group: MA-6.1-MALLOC-03 /
U-09 is unresolved and its blocked test lives with the refusal and
zero-size material (plan S8.4 / hook C-006), cited here, not derived.
Hook homes in this group: C-005's case is P3-T-MAP-041; C-009's single
case home is ERR (P3-T-ERR-211/-212, S8.2) per the cross-area rule and
is cross-cited here and in FAIL, never duplicated; C-010's case home
is INIT (S8.2), cross-cited by F-08. Builder B specifies WHAT the
shim/watchdog must do per case (N values, bounds); Builder A specifies
the shim/watchdog themselves.

### 8.1.1 MAP derivation table — one row per contract ID

| Contract ID | Hook ID (+ scheme) | Case ID(s) | Precondition | Oracle | Pass criterion | Evidence produced | Status |
|---|---|---|---|---|---|---|---|
| MA-6.1-MALLOC-01 [NORM] | MA-MALLOC-SUCCESS (named); no C-hook (NEW-1) | P3-T-MAP-001, P3-T-MAP-002, P3-T-MAP-003 | In-scope call: size > 0, representable, satisfiable on the target; annex applied | contracts S2 row; B-006 analogy under U-03; Roadmap S9.1 Inv. 1 | MAP-001: non-null return. MAP-002: several live blocks pairwise distinct and non-overlapping over their full requested extents. MAP-003: every requested byte writable and readable back as written. Alignment is asserted as the contract's suitable (fundamental) alignment; no numeric value is asserted (U-15 open) | EV-R per case | READY |
| MA-6.1-MALLOC-02 [NORM] | MA-MALLOC-FAILURE (named); no C-hook (NEW-1) | P3-T-MAP-011, P3-T-MAP-012 (also served by P3-T-FAIL-001, P3-T-ARITH-011 — listed in their home rows) | A failure is induced without leaving the supported list: representable request made unsatisfiable under a low process limit (Roadmap S9.3) or by the shim (FAIL cases); errno is NOT part of the oracle (V-02/U-10, NEW-3) | contracts S2 row; B-006 analogy under U-03; B-006 S10 general failure law | MAP-011: return is null and no unallocated pointer is returned in any form. MAP-012: after the failure, no ownership has passed, registry/counters are at baseline, and the next ordinary in-scope allocation behaves as in MAP-001 | EV-R (MAP-012 also EV-S when shim-induced) | READY |
| MA-6.1-MALLOC-04 [NORM] | MA-MALLOC-OVERSIZE (named); no C-hook (NEW-1) | P3-T-MAP-021 (mapping aspect); P3-T-ARITH-001, P3-T-ARITH-002, P3-T-ARITH-042 (arithmetic aspect, specified in S8.2.1); P3-T-FAIL-011 (no-syscall aspect) | Requested size unrepresentable, or size + reserve/rounding unrepresentable; values derived symbolically from the target's size type, never as page-size literals | contracts S2 row; B-006 analogy under U-03; Roadmap S9.1 Inv. 3 | MAP-021: the request fails as in MALLOC-02 (null, no ownership passes) and is rejected at the checked-arithmetic layer before any mapping is published; detailed boundary behaviour is the ARITH cases named here | EV-R | READY |
| MA-6.1-FREE-01 [NORM] | MA-FREE-SUCCESS (named); no C-hook (NEW-1; C-005 covers FREE-02 only) | P3-T-MAP-031, P3-T-MAP-032 | Pointer is live, in-scope, obtained from mimir_malloc, not previously freed | contracts S2 row; B-006 analogy under U-03 | MAP-031: the free completes, the storage ceases to be caller-owned and becomes eligible for reuse, and the block is retired exactly once (bookkeeping returns to baseline). MAP-032: in a mixed sequence of live blocks, freeing each block exactly once returns all counters/bookkeeping to baseline with no other live block's bytes disturbed. Reuse is eligibility only: no case asserts a later allocation returns the same address (addresses are never oracles) | EV-R | READY |
| MA-6.1-FREE-02 [NORM] | MA-FREE-NULL (named); C-005 (C-0nn, shared with MA-6.2-010 — this case also serves that row, see its row below) | P3-T-MAP-041, P3-T-MAP-042 | None beyond a running suite process; no annex decision needed | contracts S2 row + Table 6-1 C-005; B-006 analogy under U-03; Roadmap S9.1 Inv. 4 | MAP-041: mimir_free(NULL) has no effect — no error, no state change — and later allocations behave normally. MAP-042: repeated and interleaved free(NULL) calls (before any allocation, between live allocations, after all frees) likewise change nothing observable | EV-R | READY |
| MA-6.2-001 [NORM] | C-301 (C-3nn slot); no C-hook (NEW-1) | P3-T-MAP-051 | Only the compatible family exists in scope: block obtained from mimir_malloc, released only via mimir_free | contracts S3 row; B-006 analogy under U-03 | The block is released by the compatible API family and retired exactly once; no second retirement is attempted or recorded. The cross-allocator half of the ownership family is MA-6.2-002/-003, which are not in this group's inventory: -002 is recorded unmapped-with-reason and -003's serving material is Builder C's COND/REA scope — see the S8.1 ledger | EV-R | READY |
| MA-6.2-010 [NORM] | C-310 (C-3nn slot); C-005 (C-0nn) | P3-T-MAP-041, P3-T-MAP-042 (the same cases as MA-6.1-FREE-02 — they serve both rows and are written once, here) | C-ABI profile target (both Phase 3 exit targets are) | contracts S3 row; Table 6-1 C-005 | free(NULL) under the C-ABI profile is a no-op and returns normally, exactly as specified for P3-T-MAP-041/-042 | EV-R | READY |

Cross-references inside this group's inventory (full derivation rows
live in S8.2.1, where the checked-arithmetic content is specified; the
rows are not duplicated here): MA-6.2-020 (C-320) is served by
P3-T-MAP-011, P3-T-FAIL-001 and its home cases P3-T-ARITH-011/-012;
MA-6.2-024 (C-324) is served by P3-T-FAIL-091, P3-T-FAIL-001 and its
home cases P3-T-ARITH-031/-032.

### 8.1.2 FAIL derivation table — B-006 S10 failure rows F-01..F-12

Source rows are B-006 design failure rows, not contract IDs; the
Contract ID column therefore names the governing contract row(s) each
F-row implements. General law for every row: a failed allocation
returns the defined failure channel, passes no ownership, and
publishes no partially constructed or half-owned block (MA-6.1-
MALLOC-02, MA-6.2-020/-024, Inv. 10); no row promises errno.

| Contract ID | Hook ID (+ scheme) | Case ID(s) | Precondition | Oracle | Pass criterion | Evidence produced | Status |
|---|---|---|---|---|---|---|---|
| F-01 (via MA-6.1-MALLOC-02, MA-6.2-020) | MA-MALLOC-FAILURE (named); shim fail-the-Nth-mapping | P3-T-FAIL-001, P3-T-FAIL-002, P3-T-FAIL-003 | Shim interposed on the L6 mapping boundary; counting origin = the first mapping attempt after case start; uninjected control run recorded first | B-006 S10 F-01; Roadmap S9.3 line 1; B-006 S13 item 5 | FAIL-001 (N=1, mmap ENOMEM): null return, no descriptor published, registry/counters unchanged, no partial mapping. FAIL-002 (sweep): for each swept N from 1 up to the number of mapping attempts in the control run, failing the Nth attempt gives the same defined failure. FAIL-003 (recovery probe): the next uninjected attempt after a failure succeeds as in P3-T-MAP-001 | EV-S | READY |
| F-02 (via MA-6.1-MALLOC-04, Inv. 3) | MA-MALLOC-OVERSIZE (named) | P3-T-FAIL-011 (also serves MA-6.1-MALLOC-04, see its row) | Shim armed to count mapping attempts; request in the F-02 class (unrepresentable size / size+alignment overflow) | B-006 S10 F-02 (detailed arithmetic deferred to B-007 = S8.2.1) | The request is rejected before any syscall: the shim records zero mapping attempts, the return is the defined failure, and no descriptor or counter changes | EV-S | READY |
| F-03 (via MA-6.1-MALLOC-02) | MA-MALLOC-FAILURE (named) | P3-T-FAIL-021 | Shim configured to return the EMFILE/ENFILE class on the first mapping attempt | B-006 S10 F-03 | As F-01: null, nothing published or counted; the allocator makes no close/evict/retry attempt against the limit (shim attempt count stays 1) | EV-S | READY |
| F-04 (via MA-6.1-MALLOC-02, Inv. 7) | MA-MALLOC-FAILURE (named) | P3-T-FAIL-031 | Shim configured to return the EAGAIN class on the first mapping attempt | B-006 S10 F-04 | As F-01, with no internal retry loop: shim attempt count stays 1, so the failure latency is bounded by one mapping attempt (a count, not a timing oracle) | EV-S | READY |
| F-05 (via MA-6.1-MALLOC-03, unresolved) | C-006 (C-0nn) | P3-T-FAIL-041 | None can be stated: zero-size behaviour is UNDECIDED and is excluded from the supported-call list; the design promises neither a unique pointer nor null | B-006 S10 F-05; V-01 / U-09 | No pass criterion exists and none is written. This blocked test is unblocked by a U-09 / V-01 decision recorded per target in the annex; only then is a -V variant instance specified under hook C-006 | EV-R (blocked-test record only) | BLOCKED-by-U-09 |
| F-06 (via MA-6.2-020/-024, Inv. 2) | none (no hook covers release failure) | P3-T-FAIL-051 | Shim interposed on the L6 release boundary to fail the unmap of a live block; block's bytes written and verified before the free | B-006 S10 F-06 | The free does NOT complete silently: the descriptor stays LIVE (not unlinked, counters not decremented), the allocation is still recorded as owned and its bytes remain intact and readable, the recorded length is never shortened on paper, and the failure is reported via the bounded path after lock release. A later successful release retires the block exactly once | EV-S | READY |
| F-07 (via Inv. 9; MA-6.2-034 for the report half) | none | P3-T-FAIL-061 | Shim/fault point forces a lock acquisition, release, or initialisation primitive error during an in-progress allocation | B-006 S10 F-07; Roadmap S9.1 Inv. 9 | The failure is reported or the process terminates safely per the contract; the allocator never proceeds unlocked, never returns overlapping memory, and the in-progress allocation fails with no ownership passing | EV-S | READY |
| F-08 (via MA-6.3-001..006/-010..012) | C-010 (C-0nn) | Cross-cited, not duplicated: P3-T-INIT-001, -011, -021, -031, -041, -051 (full rows in S8.2.3) | Bootstrap/reentrancy conditions as specified in the INIT rows | B-006 S10 F-08; contracts S6.3 | As specified in the INIT rows. Their results are PRELIMINARY while U-05 is open (NEW-7); no FAIL case is minted for F-08 | EV-W (via INIT records) | BLOCKED-by-U-05 (via the INIT rows) |
| F-09 (via MA-6.2-033 [COND debug]; release-build half via MA-6.2-030..032) | C-330 (C-3nn slot, debug only) | P3-T-FAIL-081 (debug build; also serves MA-6.2-033 — its full contract row is S8.2.4) | DEBUG/internal-check build only; a metadata magic/state mismatch is induced on the free path or an internal check | B-006 S10 F-09 | DIAGNOSTIC-ONLY: on detection the build preserves evidence and stops/reports; it never continues treating the block as valid, never claims the corruption corrected, never unmaps the suspect mapping on suspicion, and never silently drops the descriptor. The normal-build half is NO-TEST-UB (see MA-6.2-030..032 rows, S8.2.4) and this case makes no release promise | EV-D | READY (diagnostic-only) |
| F-10 (via MA-6.2-024/-020) | C-324 (C-3nn slot, shared with MA-6.2-024 — full row in S8.2.1) | P3-T-FAIL-091 (also serves MA-6.2-024) | A multi-step mapping variant is under test; against the B-006 baseline (ONE mapping per allocation, all-or-nothing) the row is discharged by P3-T-FAIL-001/-011 and this case records that inapplicability rather than a result | B-006 S10 F-10 (guard for later variants) | For a multi-step variant: when the sequence completes only in part, every piece already obtained is unmapped, no descriptor is published, no counter changes, and no smaller-than-requested allocation is returned | EV-S | READY |
| F-11 (via MA-6.2-041; INC-B006-3) | none | P3-T-FAIL-101 | Page-size discovery at L6 is forced to fail (sysconf error / unusable value) via interposition; runtime-discovered system page size is the only page-size input | B-006 S10 F-11 | Defined failure: no mapping attempted (shim count 0), no descriptor published, and no fallback to any hardcoded constant — the value 4096 and the Pi-observed 16384 are both forbidden as fallbacks; the case is specified symbolically so it reads the same on both targets | EV-S | READY |
| F-12 (via MA-6.3-020..023) | C-420..C-423 (C-4nn slots; full rows in S8.2.4) | P3-T-FAIL-111 (also serves MA-6.3-020..023 as their failure-context case; their bound-specification cases are P3-T-ERR-071..-101) | Error reporting is placed in the state where reporting itself cannot allocate, and errors are cascaded, during an allocation failure already decided | B-006 S10 F-12; B-006 S13 item 5 companion | Reporting stays within its stated bound and allocation-free path; a reporting failure never changes the allocation outcome already decided and never converts a failure into a returned pointer | EV-S | READY |

### 8.1.3 DATA cases — data integrity and non-overlap cycles

DATA cases are source rows of Roadmap S10 (Data integrity) and B-006
S13 item 4, exercised over the B-006 calls; their contract anchors are
MA-6.1-MALLOC-01 / MA-6.1-FREE-01 (S9.1 Inv. 1), whose derivation rows
are in 8.1.1. Patterns and sizes are seeded; the seed is an evidence
field, never an oracle.

| Contract ID | Hook ID (+ scheme) | Case ID(s) | Precondition | Oracle | Pass criterion | Evidence produced | Status |
|---|---|---|---|---|---|---|---|
| Roadmap S10 Data integrity row; B-006 S13 item 4 (anchored on MA-6.1-MALLOC-01 / -FREE-01) | MA-MALLOC-SUCCESS, MA-FREE-SUCCESS (named) | P3-T-DATA-001, P3-T-DATA-002 | In-scope calls only; cycle counts fixed in the registry entry, not by wall-clock | Roadmap S10 Data integrity ("no unexpected changes to live bytes"); S9.1 Inv. 1 | DATA-001: one allocate / write a unique pattern over the full extent / verify / free cycle leaves counters and bookkeeping at baseline. DATA-002: the same cycle repeated for the registered count of iterations returns to baseline after every cycle — no leaked mapping, block, or count | EV-R | READY |
| Roadmap S10 Data integrity row (anchored on MA-6.1-MALLOC-01) | MA-MALLOC-SUCCESS (named) | P3-T-DATA-011, P3-T-DATA-012 | A live set of blocks of seeded random sizes coexists while other allocations and frees proceed around it | Roadmap S10 Data integrity; S9.1 Inv. 1 (one owner, non-overlapping usable storage) | DATA-011: each live block holds a pattern unique to that block and every byte still reads as written on repeated checks, including the first and last requested bytes. DATA-012: live blocks are pairwise non-overlapping across their full requested extents and no live block's bytes change when neighbours are written, freed, or newly allocated | EV-R | READY |
| Roadmap S9.3 confirmation line (no hang, no overlap) over DATA cycles | MA-MALLOC-SUCCESS, MA-FREE-SUCCESS (named) | P3-T-DATA-021, P3-T-DATA-022 | DATA-021: one large in-scope allocation spanning many runtime-discovered system pages (size derived from the manifest value, never a literal). DATA-022: interleaved alloc/free churn against a persistent live set | Roadmap S9.3 ("failures neither hang indefinitely nor return overlapping valid blocks" — the confirmation half; the failure half is FAIL) | DATA-021: pattern verified at the start, middle, and end of the extent and at offsets straddling internal boundaries derived from the requested size. DATA-022: after churn, every surviving live block verifies intact and the retired set is retired exactly once; the runner's per-case timeout bounds the hang half (timeout expiry is a failure, per plan S4.3a) | EV-R | READY |

### 8.1.4 B-006 group ledger — BLOCKED / NO-TEST-UB / unmapped-with-reason

- BLOCKED-by-U-09: P3-T-FAIL-041 (F-05 / MA-6.1-MALLOC-03). Unblocked
  by a per-target zero-size decision (V-01) recorded in the annex.
- BLOCKED-by-U-05: F-08's serving INIT cases (results PRELIMINARY per
  contracts S6.3, NEW-7). Unblocked for promotion by a U-05 decision
  on the watchdog bound and signal/fork specifics.
- NO-TEST-UB in this group: none minted. The UB neighbours of this
  group (MA-6.1-FREE-03 foreign/interior pointer, FREE-04 double free)
  are not in the ASSIGNMENT 2a inventory; they are recorded here so
  they are not silently absent: both are UB with no hook, NO-TEST-UB
  under S6.2, and their only specified observations are the debug
  DIAGNOSTIC-ONLY cases P3-T-FAIL-081 / P3-T-ERR-031.
- Unmapped-with-reason: MA-6.2-002 (cross-allocator free must not be
  silently treated as own) — no case is derived in this group because
  Phase 3 has exactly one allocator family in scope, so the
  cross-allocator situation cannot be constructed inside the
  supported-call list; the row is recorded here under the S6 mapping
  rule and is revisited if a second family enters scope. MA-6.2-003
  is likewise outside the 2a inventory; its explicit-pairing material
  belongs with Builder C's conditional scope (S8.3).
- Hook coverage for this group, stated plainly (NEW-1): of the three
  Table 6-1 hooks named for B-006 (C-005, C-009, C-010), only C-005's
  cases live in this section; C-009's live in ERR and C-010's in INIT,
  each written once. The MALLOC/FREE behaviour itself is carried by
  the named hooks, exactly as B-006 S13 item 2 requires the mapping
  to be shown.

## 8.2 B-007 group — checked arithmetic / alignment / clean initialisation / error semantics (ARITH / ALIGN / INIT / ERR areas)

Source-ID inventory (ASSIGNMENT item 2b, exact): MA-6.1-ALIGNED-01..
-05, MA-6.1-POSIXMA-01..-05; MA-6.2-020..-024, -030..-034, -040,
-041, -050..-054; MA-6.3-001..-006, -010..-012, -020..-023,
-030..-032, -040..-042; Roadmap S9.1 invariants and S9.3 controlled-
failure tests as oracles (those not already in FAIL). Notes that bind
the whole section: the aligned and posix_memalign names are standard-
name contract rows, NOT namespaced-pair calls — B-006 states no
mimir_aligned_alloc / mimir_posix_memalign label, so no analogy to the
pair is claimed for them; errno is never asserted for the namespaced
pair anywhere in this section (7.3); page quantities are always
qualified as internal logical page vs system page size (MA-6.2-040/
-041, whose audit cases are P3-T-ERR-051/-061).

### 8.2.1 ARITH derivation table — checked arithmetic

Oracles: MA-6.1-MALLOC-04, MA-6.2-020/-021/-024 (with -022/-023
carried in the same family table), Roadmap S9.1 Inv. 3 ("size and
alignment calculations cannot overflow") and S9.3 ("allocate near
integer and alignment boundaries; verify overflow is rejected").
Boundary values are derived symbolically from the target's size type
and the runtime-discovered system page size; no literal page size
appears in any case or expectation.

| Contract ID | Hook ID (+ scheme) | Case ID(s) | Precondition | Oracle | Pass criterion | Evidence produced | Status |
|---|---|---|---|---|---|---|---|
| MA-6.2-020 [NORM] | C-320 (C-3nn slot); no C-hook (NEW-1) | P3-T-ARITH-011, P3-T-ARITH-012 (also served by P3-T-MAP-011 and P3-T-FAIL-001, listed in their home rows) | An unsatisfiable request is presented through an in-scope call; any caller-supplied inputs are in place and checksummed before the call | contracts S3 row; Roadmap S9.1 Inv. 10 | ARITH-011: the failure is reported via the defined failure channel and no partially constructed block is returned or reachable; inputs stay caller-owned. ARITH-012: the failure channel is the same class (defined failure, no ownership) whether the cause is oversize, mapping failure, or metadata failure — no cause produces a distinct false success | EV-S | READY |
| MA-6.2-021 [NORM] | C-321 (C-3nn slot); no C-hook (NEW-1) | Served by P3-T-ALIGN-061 (posix_memalign output-parameter case, full row in S8.2.2 — the case serves both rows and is written once, there) | The namespaced pair takes no caller-supplied input storage, so the row's only Phase 3 instance in Builder B's scope is the posix_memalign output parameter; realloc instances belong to Builder C's REA area | contracts S3 row | As specified for P3-T-ALIGN-061: on failure the caller-supplied output storage is left unmodified and caller-owned | EV-R (via ALIGN record) | READY |
| MA-6.2-022 [NORM] | C-322 (C-3nn slot); no C-hook (NEW-1) | REA area, Builder C (S8.3) — unresolved citation by AREA + contract ID, resolved at integration from Builder C's registry; no ARITH case is minted, so the case is never duplicated | A realloc implementation exists in scope (B-008); until then the row has no executable instance in this plan | contracts S3 row; Roadmap S9.1 Inv. 5 | Failed realloc leaves the original valid, with unchanged ownership, freeable as before — pass criterion carried verbatim into the REA row in S8.3, whose case discharges this row | none in this part | READY (discharged by the S8.3 REA case) |
| MA-6.2-023 [NORM] | C-323 (C-3nn slot); no C-hook (NEW-1) | REA area, Builder C (S8.3) — unresolved citation as for -022 | As for -022 | contracts S3 row | Successful realloc ends the old ownership and begins the new with no interval in which the block is both-owned or neither-owned — carried into the S8.3 REA row; ownership-model evidence is the fuzzer trace under the invoking case (plan S4.3d), gated to in-scope operations only | none in this part | READY (discharged by the S8.3 REA case) |
| MA-6.2-024 [NORM] | C-324 (C-3nn slot); no C-hook (NEW-1) | P3-T-ARITH-031, P3-T-ARITH-032 (also served by P3-T-FAIL-091 and P3-T-FAIL-001, listed in their home rows) | Shim can fail (a) a mapping attempt mid-sequence and (b) the metadata allocation (Roadmap S9.3 line 1, second half); counters and registry observable via the evidence record | contracts S3 row; B-006 F-10/F-01 | ARITH-031 (metadata-allocation failure): the attempt rolls back or is abandoned internally; no half-owned block is reachable, counted, or returnable. ARITH-032 (failure after a descriptor is partially built): the partial descriptor is never published; a following ordinary allocation behaves as in P3-T-MAP-001 and counters are at baseline | EV-S | READY |
| MA-6.1-MALLOC-04 + Inv. 3 + S9.3 boundary family (contract row itself is in S8.1.1; the arithmetic cases serving it are specified here, as that row lists) | MA-MALLOC-OVERSIZE (named) | P3-T-ARITH-001, P3-T-ARITH-002, P3-T-ARITH-041, P3-T-ARITH-042, P3-T-ARITH-043 | Requests are table-driven from symbolic boundary values: the maximum representable size, that value minus/plus each overhead term the B-007 arithmetic design names (reserve, rounding, total-mapping), and alignment-rounding overflow points; the table is generated per target from its size type, identically sourced on both targets | contracts S2 MALLOC-04; Roadmap S9.1 Inv. 3; Roadmap S9.3 boundary line; B-007 checked-arithmetic design (reconciled at banking, S12 INC-B009-10; the total-mapping formula itself is UNRESOLVED per B-007 INC-B007-1 / S12 INC-B009-11 — boundary values here are symbolic over both formula forms and no case cites a canonical formula) | ARITH-001: a request at the maximum representable size is rejected with the defined failure, no wrapped allocation. ARITH-002: each boundary request is rejected exactly when the checked computation (size + reserve, rounded, total) is unrepresentable, and never wraps to a small allocation. ARITH-041: alignment rounding that would overflow is rejected by the same channel (valid alignments only; invalid-alignment validity itself is BLOCKED-by-U-11 in ALIGN). ARITH-042: total-mapping computation overflow is rejected before any mapping attempt (shim count 0 for the unrepresentable entries). ARITH-043 (over-rejection guard): requests just inside the representable boundary (size 1; size one below an alignment boundary; largest representable satisfiable class under the case's limits) succeed or fail only via the defined channel, never via a spurious arithmetic rejection | EV-S (EV-R for ARITH-043) | READY |

### 8.2.2 ALIGN derivation table — alignment

Hook split recorded before the rows (scout NEW-2): C-007 is two
cases wearing one hook. Its valid-alignment half is READY
(P3-T-ALIGN-001/-002). Its invalid-alignment half — "a request with
an invalid alignment parameter fails by the specified model" — has
no specified model to fail by: ALIGNED-03/-04 are UNRESOLVED U-11 and
V-03 is unresolved for every libc column, so that half is
BLOCKED-by-U-11 (P3-T-ALIGN-021) and C-007 as written is unfalsifiable
for it until U-11 is decided. C-008 likewise splits: the error model
is READY, the exact error numbers are BLOCKED-by-U-14
(P3-T-ALIGN-062), and per-target instantiation waits on the annex's
U-01 symbol-set decision.

| Contract ID | Hook ID (+ scheme) | Case ID(s) | Precondition | Oracle | Pass criterion | Evidence produced | Status |
|---|---|---|---|---|---|---|---|
| MA-6.1-ALIGNED-01 [NORM] | MA-ALIGNED-SUCCESS (named); C-007 valid half (C-0nn) | P3-T-ALIGN-001, P3-T-ALIGN-002 | Alignment is valid per the target baseline (V-03 success half is stated in the contract row); sizes are chosen as multiples of the alignment so the READY rows do not depend on the unresolved size-multiple rule (U-11) | contracts S2 row; POSIX baseline | ALIGN-001: the returned pointer's address is an exact multiple of the requested alignment and the block is releasable by free. ALIGN-002: the same holds across the registered range of valid alignments and sizes, with every live block simultaneously satisfying its own alignment | EV-R | READY |
| MA-6.1-ALIGNED-02 [NORM] | MA-ALIGNED-FAILURE (named); no C-hook (NEW-1) | P3-T-ALIGN-011, P3-T-ALIGN-012 | Request is valid (as in ALIGNED-01) but unsatisfiable: induced by shim mapping failure or by a representable alignment+size the target cannot satisfy under the case's limits | contracts S2 row | ALIGN-011: return is null and no unallocated pointer is returned. ALIGN-012: after the failure no ownership has passed, counters are at baseline, and a following satisfiable aligned request succeeds as in ALIGN-001 | EV-S | READY |
| MA-6.1-ALIGNED-03 (unresolved) | C-007 invalid half (C-0nn) | P3-T-ALIGN-021 | None can be stated: which alignments are invalid is target-specific and UNRESOLVED (V-03) | contracts S2 row (no behaviour stated); U-11 | No pass criterion is written. Unblocked by a U-11 / V-03 decision per target (validity rule recorded in the annex); only then is the invalid-value table and its specified failure model specified | EV-R (blocked-test record only) | BLOCKED-by-U-11 |
| MA-6.1-ALIGNED-04 (unresolved) | none | P3-T-ALIGN-031 | None can be stated: whether a size-multiple-of-alignment rule applies is target-specific and UNRESOLVED | contracts S2 row (no behaviour stated); U-11 / V-03 | No pass criterion is written. Unblocked by the same U-11 / V-03 decision as ALIGNED-03, stating whether the rule applies per target | EV-R (blocked-test record only) | BLOCKED-by-U-11 |
| MA-6.1-ALIGNED-05 (unresolved) | none | P3-T-ALIGN-041 | None can be stated: zero-size behaviour is UNRESOLVED (U-09 / V-01) and the alignment-validity half also waits on U-11 | contracts S2 row (no behaviour stated); U-09, U-11 | No pass criterion is written. Unblocked by the U-09 zero-size decision plus the U-11 validity decision, recorded per target in the annex | EV-R (blocked-test record only) | BLOCKED-by-U-09 (also U-11) |
| MA-6.1-POSIXMA-01 [NORM] | MA-POSIXMA-SUCCESS (named); not listed as a Contract ID in Table 6-1 (NEW-1; C-008's pass text asserts this behaviour without mapping the row) | P3-T-ALIGN-051, P3-T-ALIGN-052 | posix_memalign is in the target's settled symbol set per the annex (U-01); alignment valid per the contract row's own rule (power of two, multiple of the pointer size) | contracts S2 row; POSIX baseline | ALIGN-051: return value is 0, the stored pointer is aligned to the requested alignment, and the block is releasable by free. ALIGN-052: the same holds across the registered range of valid alignments and sizes | EV-R | READY |
| MA-6.1-POSIXMA-02 [NORM] | MA-POSIXMA-FAILURE-UNCHANGED (named); C-008 (C-0nn) | P3-T-ALIGN-061, P3-T-ALIGN-062 | Failure induced (shim / unsatisfiable valid request); the output parameter is pre-set to a sentinel pointer value and checksummed; annex has settled the symbol set (U-01). errno is not consulted as an oracle anywhere in this row | contracts S2 row; Table 6-1 C-008; V-06 model (exact numbers U-14) | ALIGN-061: the return is a non-zero error number, the output parameter is byte-identical to its sentinel (unchanged, still caller-owned — this case also serves MA-6.2-021, see S8.2.1), and no allocation is published. The case asserts the model (non-zero, unchanged, errno not the channel), never a specific number | EV-S | READY (model); the exact-number instance is P3-T-ALIGN-062, BLOCKED-by-U-14 |
| MA-6.1-POSIXMA-02, exact-number instance (part of the same contract row, split per NEW-2's C-008 analysis) | C-008 (C-0nn) | P3-T-ALIGN-062 | A per-target table of exact error numbers exists — it does not: V-06 numbers are UNRESOLVED | contracts S2 row; U-14 | No numeric pass criterion is written. Unblocked by a U-14 decision recording the exact error number per failure class per target in the annex | EV-R (blocked-test record only) | BLOCKED-by-U-14 |
| MA-6.1-POSIXMA-03 (unresolved) | none | P3-T-ALIGN-071 | None can be stated: invalid alignment, including the pointer-size relation rule, is UNRESOLVED | contracts S2 row (no behaviour stated); U-11 | No pass criterion is written. Unblocked by the U-11 / V-03 decision as for ALIGNED-03 | EV-R (blocked-test record only) | BLOCKED-by-U-11 |
| MA-6.1-POSIXMA-04 (unresolved) | none | P3-T-ALIGN-081 | None can be stated: zero-size with valid alignment is UNRESOLVED (U-09 / V-01) | contracts S2 row (no behaviour stated); U-09 | No pass criterion is written. Unblocked by the U-09 zero-size decision recorded per target in the annex | EV-R (blocked-test record only) | BLOCKED-by-U-09 |
| MA-6.1-POSIXMA-05 | none (UB: no hook, per S6.2) | none — no case ID is minted | Null output parameter: the behaviour is UB, so no precondition, oracle, or expected result is specified | contracts S2 row; contracts S6.2 | No pass criterion exists. No diagnostic case is specified for this row in this plan; any future sanitiser observation would be DIAGNOSTIC-ONLY under 7.2 and would not change this status | none | NO-TEST-UB |

### 8.2.3 INIT derivation table — clean initialisation / bootstrap

Hook C-010 (MA-6.3-001..-004) is PRELIMINARY per contracts S6.3 while
U-05 is open (NEW-7): the watchdog bound value itself is
[UNRESOLVED: U-05], expiry of the bound is a FAILURE (never
inconclusive), and no C-010 result can support a promotion claim
until U-05 is decided. Every INIT case that must execute inside the
watchdog bound is therefore BLOCKED-by-U-05 for promotion purposes
even though its procedure is fully written below; Builder B specifies
per case WHAT must complete inside the bound, Builder A specifies the
watchdog itself. The B-006 baseline helps and is recorded as a
precondition fact, not a result: baseline registry anchors and lock
are static, need no allocation or constructor, and an empty registry
is valid before any init (B-006 F-08).

| Contract ID | Hook ID (+ scheme) | Case ID(s) | Precondition | Oracle | Pass criterion | Evidence produced | Status |
|---|---|---|---|---|---|---|---|
| MA-6.3-001 [NORM] | C-401 (C-4nn slot); C-010 (C-0nn) | P3-T-INIT-001 | First allocator entry occurs before initialisation completes (harness entry point arranged pre-init); case executes inside the watchdog bound | contracts S4 row; B-006 F-08 | Within the bound, the entry is served from the minimal bootstrap path or fails via the defined channel; it does not assume initialised state, does not hang, and does not recurse without bound. Watchdog expiry is a failure of this case | EV-W | BLOCKED-by-U-05 (procedure specified; bound value undecided, result PRELIMINARY per S6.3) |
| MA-6.3-002 [NORM] | C-402 (C-4nn slot); C-010 family (C-0nn) | P3-T-INIT-011 | Bootstrap executes with the normal path instrumented to record any use as bootstrap storage | contracts S4 row; INC-B006-6 | Bootstrap initialisation storage demonstrably does not come from the not-yet-ready allocator's normal path: the instrumented normal path records zero bootstrap-storage allocations, and bootstrap completes (or defined-fails) inside the watchdog bound | EV-W | BLOCKED-by-U-05 (as for -001) |
| MA-6.3-003 [NORM] | C-403 (C-4nn slot); C-010 family (C-0nn) | P3-T-INIT-021 | Init is arranged to attempt self-allocation during init | contracts S4 row | The self-allocation does not recurse indefinitely: bootstrap terminates (serves, defers, or defined-fails) without any completed prior init, inside the watchdog bound; recursion depth, if recorded, is a diagnostic only | EV-W | BLOCKED-by-U-05 (as for -001) |
| MA-6.3-004 [NORM] | C-404 (C-4nn slot); C-010 family (C-0nn) | P3-T-INIT-031 | A reentrant entry on the same thread is forced during initialisation | contracts S4 row | The reentrant entry is handled by the bootstrap path, deferred, or defined-failed; it never re-enters as if initialisation were complete, inside the watchdog bound | EV-W | BLOCKED-by-U-05 (as for -001) |
| MA-6.3-005 [NORM] | C-405 (C-4nn slot); behaviour stated as MA-6.3-001 | P3-T-INIT-041 | Entry arrives via a constructor / dynamic-loader path before main, with no hosted services assumed; harness is a shared-library load arrangement | contracts S4 row ("as 001"); Roadmap S9.3 init/loading/teardown line | As for P3-T-INIT-001, through the pre-main path; teardown order is recorded in the evidence and no teardown step calls into a path already torn down (the S9.3 ordering half) | EV-W | BLOCKED-by-U-05 (inherits -001) |
| MA-6.3-006 [NORM] | C-406 (C-4nn slot); no C-hook (NEW-1) | P3-T-INIT-051 | A block was served by the bootstrap path (obtained under the preliminary C-010 conditions of INIT-001) and full initialisation has since completed | contracts S4 row | The disjunction in the contract holds, and the case records which branch occurred: either the compatible allocator accepts the free of the bootstrap-served block after full init (retiring it exactly once), or the bootstrap path had defined-failed the block at serve time so no such block exists. Neither branch may strand the block | EV-R | READY |
| MA-6.3-010 [NORM] | C-410 (C-4nn slot); no C-hook (NEW-1) | P3-T-INIT-061 | Source/build audit plus a pre-main run: the init path's dependency list is enumerable at design level | contracts S4 row | Audit case: no allocator-dependent C++ container or utility appears on the initialisation path, and the pre-main run reaches main with init complete or defined-failed. The audit worksheet names every init-path dependency checked | EV-A | READY |
| MA-6.3-011 [NORM] | C-411 (C-4nn slot); no C-hook (NEW-1) | P3-T-INIT-071 | An allocating static/global construction exists in the test arrangement; any mechanism the design claims for ordering it after allocator init is stated in the precondition before the case is instantiated | contracts S4 row | Audit + runtime case: the construction is not relied on to run after allocator init without the stated mechanism; with the mechanism in place, the construction's allocation behaves as an ordinary in-scope allocation and no bootstrap assumption is violated | EV-A | READY |
| MA-6.3-012 [NORM] | C-412 (C-4nn slot); no C-hook (NEW-1) | P3-T-INIT-081 | Init bookkeeping structure is identifiable in the design (fixed, preallocated, or bootstrap-owned storage); a growth arrangement exercises many allocations and many threads | contracts S4 row; INC-B006-6 | Bookkeeping storage is in the stated fixed/preallocated/bootstrap-owned class, and its footprint does not grow with the number of allocations or threads beyond the bound the design states; footprint is recorded as a count/size in the evidence, never as a timing claim | EV-R | READY |

### 8.2.4 ERR derivation table — error semantics

Binding cautions for the whole table: MA-6.2-030..-034 debug
detection is diagnostics-only and is never worded as a release
promise (Roadmap S9.2: debug features can detect some corruption,
cannot safely recover every corrupted heap, and allocator checks must
not be called ECC); MA-6.3-030..-032 are specified as
documentation-audit cases, because the contract rows govern what may
be claimed, not a runtime behaviour; the S9.3 controlled-failure
lines not already in FAIL are discharged here and in ARITH/INIT
(boundary tests in ARITH, thread churn at MA-6.2-054 below, init and
teardown order in INIT) — the remaining S9.3 lines (low limits /
fragmented address space as failure contexts) are preconditions of
P3-T-MAP-011 and the FAIL cases, and the S9.3 per-environment tracking
line is a plan S6 Platform matter, not an ERR case: Android/proot are
not Phase 3 exit targets and get no annex in this plan.

| Contract ID | Hook ID (+ scheme) | Case ID(s) | Precondition | Oracle | Pass criterion | Evidence produced | Status |
|---|---|---|---|---|---|---|---|
| MA-6.2-030 | none (UB: no hook, per S6.2) | none — no case ID is minted | Invalid-pointer free/access is UB; no precondition or expected result is specified | contracts S3 row; contracts S6.2 | No pass criterion exists. Debug-build detection of this event class is specified only in P3-T-ERR-031 as DIAGNOSTIC-ONLY | none | NO-TEST-UB |
| MA-6.2-031 | none (UB: no hook, per S6.2) | none — no case ID is minted | Use-after-free is UB; as for -030 | contracts S3 row; contracts S6.2 | No pass criterion exists; debug observation only via P3-T-ERR-031, DIAGNOSTIC-ONLY | none | NO-TEST-UB |
| MA-6.2-032 | none (UB: no hook, per S6.2) | none — no case ID is minted | Double free is UB; as for -030 | contracts S3 row; contracts S6.2 | No pass criterion exists; debug observation only via P3-T-ERR-031, DIAGNOSTIC-ONLY | none | NO-TEST-UB |
| MA-6.2-033 [COND: debug] | C-330 (C-3nn slot); no C-hook (NEW-1) | P3-T-ERR-031 (also served by P3-T-FAIL-081 in the free-path context, listed in its home row) | DEBUG/internal-check build only; one of the -030..-032 event classes is induced | contracts S3 row (debug MAY detect; on detection it MUST preserve evidence and stop/report); Roadmap S9.2 | DIAGNOSTIC-ONLY: where the debug build detects the event, it preserves the evidence and stops/reports rather than continuing; where it does not detect, the record says so and no failure is recorded against conformance. This case is never a release-build promise and its records are filed as EV-D, separated from conformance | EV-D | READY (diagnostic-only) |
| MA-6.2-034 [NORM] | C-331 (C-3nn slot); no C-hook (NEW-1) | P3-T-ERR-041 (also served by P3-T-FAIL-081, listed in its home row) | An internal check finds metadata corruption (induced mismatch, as in F-09) in a build with internal checks enabled | contracts S3 row | The allocator does not claim to have corrected the corruption; it preserves evidence and stops/reports per the host contract. Any record wording claiming correction or recovery is itself a failure of this case | EV-D | READY (diagnostic/internal-check) |
| MA-6.2-040 [NORM] | C-340 (C-3nn slot); no C-hook (NEW-1) | P3-T-ERR-051 | The documents under audit are named in the worksheet: contracts, this plan, the B-006/B-007 designs, headers as they come to exist | contracts S3 row + its definitions box | Terminology audit: every use of owner, block, internal logical page, system page size, span, and huge page in the audited documents matches the definitions box exactly; every finding is a quoted line with its location. This plan's own tables are in the audit set | EV-A | READY (documentation audit) |
| MA-6.2-041 [NORM] | C-341 (C-3nn slot); no C-hook (NEW-1) | P3-T-ERR-061 | As for -040; evidence records are in scope as well as prose | contracts S3 row | Terminology audit: every page quantity in the audited documents and in sampled evidence records is qualified as internal logical page or system page size before any contract use; an unqualified page quantity is a finding. (This plan complies by construction: the only numeric page size it carries is the runtime-discovered system page size in target manifests.) | EV-A | READY (documentation audit) |
| MA-6.3-020 [NORM] | C-420 (C-4nn slot); no C-hook (NEW-1) | P3-T-ERR-071 (failure-context case: P3-T-FAIL-111) | An allocation failure is forced while the reporting path is instrumented for dynamic-allocation use and total reporting work | contracts S4 row; B-006 F-12 | Error reporting performs no unbounded dynamic allocation and completes within the bound the design states; reporting work is not taken from the failing path itself. The numeric bound is the design's stated bound, recorded in the evidence, not invented here | EV-S | READY |
| MA-6.3-021 [NORM] | C-421 (C-4nn slot); no C-hook (NEW-1) | P3-T-ERR-081 (also served by P3-T-FAIL-111) | Reporting is invoked from a failed/corrupted state (induced as in F-09/F-12) | contracts S4 row | Reporting does not depend on the health of the failed state: a report is produced, truncated in preference to absent, and its production does not read the corrupted metadata as if valid | EV-D | READY |
| MA-6.3-022 [NORM] | C-422 (C-4nn slot); no C-hook (NEW-1) | P3-T-ERR-091 | Reporting is invoked on the bootstrap path, before full init | contracts S4 row | The -020 bound holds on the bootstrap path and only bootstrap facilities are used; no normal-path facility is touched (instrumentation as in P3-T-INIT-011) | EV-R | READY |
| MA-6.3-023 [NORM] | C-423 (C-4nn slot); no C-hook (NEW-1) | P3-T-ERR-101 (also served by P3-T-FAIL-111) | Errors are repeated and cascaded for the registered count of reports | contracts S4 row | The same bound holds per report, every time, and retained reporting storage does not grow without limit across the cascade; total retained storage is recorded as a count/size | EV-S | READY |
| MA-6.3-030 [NORM] | C-430 (C-4nn slot); no C-hook (NEW-1) | P3-T-ERR-111 | The claim surface under audit is named: headers, docs, diagnostics text | contracts S4 row | Documentation audit: no function is claimed async-signal-safe anywhere in the audited surface unless a separate, individual proof plus contract for that function is cited at the claim site; an uncited claim is a finding. No runtime signal-safety behaviour is asserted by this case | EV-A | READY (documentation audit) |
| MA-6.3-031 [NORM] | C-431 (C-4nn slot); no C-hook (NEW-1) | P3-T-ERR-121 | As for -030, including implied claims (wording, examples, diagnostic labels) | contracts S4 row | Documentation audit: the audited surface does not state or imply async-signal-safety without the -030 proof; implication findings are quoted verbatim in the worksheet | EV-A | READY (documentation audit) |
| MA-6.3-032 [NORM] | C-432 (C-4nn slot); no C-hook (NEW-1) | P3-T-ERR-131 | As for -030; where a signal handler exists in a test arrangement, its call list is enumerable | contracts S4 row; U-05 (any runtime signal-state claim) | Documentation audit: the design/docs state no promise about allocator state observable from a handler for a signal arriving while a lock is held or state is in transition, and any handler in scope is audited to call no allocator entry point. Any stronger runtime claim is BLOCKED-by-U-05 and is not made here | EV-A | READY (documentation audit; runtime half BLOCKED-by-U-05) |
| MA-6.3-040 [NORM, conditional on any fork claim] | C-440 (C-4nn slot); no C-hook (NEW-1) | P3-T-ERR-141 | None can be satisfied yet: no atfork registration or lock discipline is documented in a design, and fork policy specifics are UNRESOLVED (U-05, detail U-17..-21) | contracts S4 row; INC-B006-7 | No pass criterion is written. Unblocked by (a) a design documenting any pthread_atfork registration and the lock discipline, and (b) the U-05 / U-19 decisions on handler ordering and interaction. Until then this plan makes no fork claim of any kind | EV-R (blocked-test record only) | BLOCKED-by-U-05 (also U-19) |
| MA-6.3-041 [NORM] | C-441 (C-4nn slot); no C-hook (NEW-1) | P3-T-ERR-151 (parent case only) | Live parent blocks are allocated and filled immediately before a fork; the case observes ONLY the parent after the fork; the child performs no allocator call and exits without touching the allocator (the child's allocator behaviour is exactly what U-17..-21 leave open and is not exercised) | contracts S4 row (fork prep MUST NOT invalidate, move, or change ownership of live parent blocks) | Parent case: after the fork, every live parent block is still valid at the same ownership, its bytes read as written, and each is freed normally exactly once, returning counters to baseline. This case supports no claim about the child | EV-R | READY (parent case only; child half BLOCKED-by-U-17..U-21) |
| MA-6.3-042 [NORM] | C-442 (C-4nn slot); no C-hook (NEW-1) | P3-T-ERR-161 | None: any fork behaviour beyond -041's parent case is not Defined until moved off the unresolved list by per-platform evidence | contracts S4 row; U-05, U-17..-21 | No pass criterion is written, and no document in this plan may document or rely on any other fork behaviour as Defined. Unblocked per-platform by the U-17..-21 decisions plus the per-platform evidence the contract row names | EV-R (blocked-test record only) | BLOCKED-by-U-05 (also U-17..U-21) |
| MA-6.2-050 [COND: a runtime caching-setting exists] | C-350 (C-3nn slot); no C-hook (NEW-1) | P3-T-ERR-171 | Out of Phase 3 scope, with reason: the Phase 3 design has no runtime configuration surface at all — L7 is compiled out (B-006 S12.3, Phase 10) and the namespaced scope decision is itself open (U-03) — so no caching setting exists to change, concurrently or otherwise | contracts S3 row; B-006 S12.3 | No pass criterion is written for Phase 3. The row is recorded blocked on the scope decision, not silently dropped: unblocked by a U-03 scope decision plus a designed config surface, at which point the thread-safety case (observers see the old or the new setting in full, never a mixture) is specified | EV-R (blocked-test record only) | BLOCKED-by-U-03 (out of Phase 3 scope: no config surface) |
| MA-6.2-051 [COND: a runtime statistics-setting exists] | C-351 (C-3nn slot); no C-hook (NEW-1) | P3-T-ERR-181 | Out of Phase 3 scope, same reason as -050: no statistics setting exists in the Phase 3 design (L7 compiled out; stats are U-03 / Phase 10) | contracts S3 row; B-006 S12.3 | No pass criterion is written for Phase 3. Unblocked as for -050; the case then specified asserts counters are attributable to the setting in force when each operation ran | EV-R (blocked-test record only) | BLOCKED-by-U-03 (out of Phase 3 scope: no config surface) |
| MA-6.2-052 [COND: a runtime diagnostics-setting exists] | C-352 (C-3nn slot); no C-hook (NEW-1) | P3-T-ERR-191 | Out of Phase 3 scope, same reason as -050: no diagnostics setting exists in the Phase 3 design | contracts S3 row; B-006 S12.3 | No pass criterion is written for Phase 3. Unblocked as for -050; the case then specified asserts a diagnostics-setting change never alters the S3.4 (contracts S4 diagnostics) classification of any event | EV-R (blocked-test record only) | BLOCKED-by-U-03 (out of Phase 3 scope: no config surface) |
| MA-6.2-053 [COND: a config reader exists] | C-353 (C-3nn slot); no C-hook (NEW-1) | P3-T-ERR-201 | Out of Phase 3 scope, same reason as -050: there is no config and no config version ID in the Phase 3 design to observe | contracts S3 row; B-006 S12.3 | No pass criterion is written for Phase 3. Unblocked as for -050; the case then specified asserts a reader obtains the config and its version ID as one consistent observation, never a torn pair | EV-R (blocked-test record only) | BLOCKED-by-U-03 (out of Phase 3 scope: no config surface) |
| MA-6.2-054 [NORM] | C-354 (C-3nn slot); C-009 (C-0nn) — THE cross-thread case: written ONCE, here in ERR per the cross-area rule, and cross-cited (never duplicated) by plan S6's Concurrency/Ownership rows, by S8.1's FAIL table where F-rows touch cross-thread behaviour, and by B-006 S13 item 6 | P3-T-ERR-211, P3-T-ERR-212 | Multithreaded target (both Phase 3 exit targets are); blocks allocated on one thread, handed to another through the harness's ordinary synchronisation; cycle counts and the thread-churn seed are registered, not time-based | contracts S3 row; Table 6-1 C-009; Roadmap S9.1 Inv. 6; Roadmap S9.3 thread-churn line; B-006 S13 item 6 | ERR-211: a block allocated on thread A is freed on thread B exactly as if on one thread — ownership preserved exactly, storage non-overlapping throughout, no corruption, no double-free diagnostic in the debug build, no loss — over repeated registered cycles, with the block's bytes intact at handoff. ERR-212 (churn): threads are repeatedly created and destroyed while objects are transferred between them (seeded sequence); every transferred block is released exactly once by its correct owner at thread exit or after, and no list node ever appears simultaneously in incompatible free lists. Correctness only: no performance, timing, or scheduling-order claim is made or compared | EV-T | READY |

### 8.2.5 B-007 group ledger — BLOCKED / NO-TEST-UB / unmapped-with-reason

- BLOCKED-by-U-05: P3-T-INIT-001, -011, -021, -031, -041 (C-010
  family; procedures written, bound undecided, results PRELIMINARY);
  P3-T-ERR-141 (also U-19), P3-T-ERR-161 (also U-17..-21); the
  runtime half of P3-T-ERR-131. Unblocked by the U-05 decision on the
  watchdog bound, signal interaction, and fork-child specifics.
- BLOCKED-by-U-09: P3-T-ALIGN-041 (also U-11), P3-T-ALIGN-081.
  Unblocked by the per-target zero-size decision (V-01).
- BLOCKED-by-U-11: P3-T-ALIGN-021, -031, -071 (and the invalid half of
  hook C-007, NEW-2). Unblocked by the alignment-validity and
  size-multiple decision (V-03) per target.
- BLOCKED-by-U-14: P3-T-ALIGN-062 (exact posix_memalign error
  numbers). The C-008 model itself is READY at P3-T-ALIGN-061.
- BLOCKED-by-U-03, out of Phase 3 scope with reason: P3-T-ERR-171,
  -181, -191, -201 (MA-6.2-050..-053; no runtime config surface
  exists — L7 compiled out, B-006 S12.3 / Phase 10).
- NO-TEST-UB: MA-6.1-POSIXMA-05; MA-6.2-030, -031, -032. No hook, no
  expected result, no case ID minted for any of them; their only
  specified observations are the DIAGNOSTIC-ONLY cases P3-T-ERR-031
  and P3-T-FAIL-081, filed separately from conformance.
- Unmapped-with-reason in this group: none. Every inventory ID has a
  derivation row above or an express cross-area discharge
  (MA-6.2-022/-023 to Builder C's REA rows in S8.3, cited by AREA +
  contract ID and resolved at integration — recorded here so the
  completeness audit counts them as category (d) until integration
  and category (a) after it).
- Invariant coverage note (Roadmap S9.1, for plan S6's use): Inv. 1
  is discharged by MAP/DATA/ERR-211; Inv. 2 by F-06 and S8.2.4 -034;
  Inv. 3 by ARITH; Inv. 4 by MAP (C-005); Inv. 5 by the REA discharge
  above; Inv. 6 by ERR-211/-212; Inv. 7 by F-04 and MA-6.2-020;
  Inv. 8 has no Phase 3 instance (statistics do not exist in scope —
  same U-03 / L7 reason as MA-6.2-050..-053, recorded here, not
  silently passed); Inv. 9 by F-07; Inv. 10 by FAIL's general law and
  MA-6.2-020/-024. Invariants 5 and 8 are the only ones whose
discharge leaves this part, and both say so in their rows.

*End of B-009 Part B (Builder B: plan S7, S8.1, S8.2). Design only;
no test in this part has been run.*
## 8.3 B-008 group — calloc / realloc / conditional (CAL / REA / COND areas)

Source-ID inventory (ASSIGNMENT item 2c, exact):
MA-6.1-CALLOC-01..05; MA-6.1-REALLOC-01..06;
MA-COND-REALLOCARRAY-01..03 (with -04, the gate-off
row, recorded under the same gate); hooks C-001,
C-002, C-003, C-004, and C-006 / C-007 / C-008 only as
cross-references where a row touches them; named
hooks MA-CALLOC-* and MA-REALLOC-*; ownership
cross-references MA-6.2-022 (C-322) and MA-6.2-023
(C-323). Also recorded, not derived to CAL/REA/COND
conformance cases: MA-COND-USABLE-01..04 and
MA-COND-CXX-01..10 (see 8.3.4 ledger).

Scope note (reconciled at banking, S12 INC-B009-10).
These rows test the standard-name contract rows in
contracts.md S2. B-006 itself states no `mimir_calloc` /
`mimir_realloc` label, signature, or semantics (B-006
S12.2) — but the final B-008 design does: B-008 S3 / S4
design the namespaced pair **by analogy under U-03**
(B-008 S1.2) and B-008 S7.2 amends the Phase 3
supported-call list to include them (plan S1.2, B-008
amendment paragraph). The cases below therefore cover
the B-008 surface as finally designed: the standard-name
rows directly, and the namespaced labels by the same
U-03 analogy — never as direct namespaced contract
claims. A `calloc` / `realloc` pass claim, when a run
exists, will be a claim about that reconciled B-008
surface only.

Evidence shorthand is Builder B's S7.4 shorthand
(EV-R, EV-S, EV-W, EV-T, EV-A, EV-D), expanded and
governed by Section 9 of this part. A record missing
any mandatory S9 field is not a PASS record.

### 8.3.1 CAL derivation table — one row per contract ID

| Contract ID | Hook ID (+ scheme) | Case ID(s) | Precondition | Oracle | Pass criterion | Evidence produced | Status |
|---|---|---|---|---|---|---|---|
| MA-6.1-CALLOC-01 [NORM] | C-002 (C-0nn); MA-CALLOC-SUCCESS-ZEROED (named) | P3-T-CAL-001, P3-T-CAL-002 | B-008 surface exists per final B-008 design (reconciled at banking, S12 INC-B009-10); count and size are such that their product is representable and satisfiable on the target; annex applied | contracts S2 row; Table 6-1 C-002 | CAL-001: return is non-null, storage is suitably aligned as in MA-6.1-MALLOC-01 (no numeric alignment value asserted, U-15 open), and every byte reads zero before any caller write. CAL-002: the same holds across a size set that includes sizes that are non-multiples of the alignment, per C-002's wording in substance | EV-R per case | READY |
| MA-6.1-CALLOC-05 [NORM] | C-002 (C-0nn); MA-CALLOC-ZEROING (named) | P3-T-CAL-003 (also serves CALLOC-01; written once, here) | Storage returned under a successful CALLOC-01 call, inspected byte by byte at the point of return, before any caller write | contracts S2 row; Table 6-1 C-002 | Every inspected byte reads as zero at the point of return. No byte beyond the requested product size is inspected as an oracle, and no claim is made about it | EV-R | READY |
| MA-6.1-CALLOC-02 [NORM] | C-001 (C-0nn); MA-CALLOC-OVERFLOW (named) | P3-T-CAL-011, P3-T-CAL-012 | count x size cannot be represented, values derived symbolically from the target size type, never as page-size literals; CAL-012 uses the exceeds-target-maximum form named in C-001 | contracts S2 row; Table 6-1 C-001, with its errno wording expressly NOT imported (U-10 open, NEW-3) | CAL-011: overflow is detected before multiplication is relied on for an allocation size; the call fails with a null return and no allocation of a wrapped-around size occurs. CAL-012: a representable product above the target maximum likewise fails with null and no wrapped allocation. In both: no ownership passes. errno is not asserted | EV-R per case | READY |
| MA-6.1-CALLOC-03 [NORM] | MA-CALLOC-FAILURE (named); no C-hook (NEW-1: C-001 / C-002 cover overflow and zeroing only) | P3-T-CAL-021 | Represented product cannot be satisfied, induced without leaving the contract surface (shim failure or low process limit, as in the FAIL method of S8.1); the failure is not the CALLOC-02 overflow case | contracts S2 row | Return is null; no pointer to storage not allocated to the caller is returned in any form; no ownership passes; after the failure the next ordinary in-scope allocation behaves normally. errno is not asserted (V-02 / U-10) | EV-R (EV-S when shim-induced) | READY |
| MA-6.1-CALLOC-04 | none stated in the row ([UNRESOLVED: U-09]) | P3-T-CAL-031 | None can be stated: a zero factor (count = 0, size = 0, or both) is UNRESOLVED (U-09 / V-01), exactly as for MALLOC-03 | contracts S2 row; V-01 | No pass criterion exists and none is written. Unblocked only by a U-09 / V-01 decision recorded per target in the annex; only then is a per-target-policy instance specified, under hook C-006, with a `-V<nn>` variant suffix keyed to the V-item | EV-R (blocked-test record only) | BLOCKED-by-U-09 |

### 8.3.2 REA derivation table — one row per contract ID

| Contract ID | Hook ID (+ scheme) | Case ID(s) | Precondition | Oracle | Pass criterion | Evidence produced | Status |
|---|---|---|---|---|---|---|---|
| MA-6.1-REALLOC-01 [NORM] | C-004 (C-0nn); MA-REALLOC-SUCCESS-COPY-MIN (named) | P3-T-REA-001, P3-T-REA-002 (REA-002 also serves MA-6.2-023, see 8.3.3) | Pointer refers to a live allocation whose bytes are filled with a known pattern; new size can be satisfied; cases cover grow and shrink, and (REA-002) a move to a different block | contracts S2 row; Table 6-1 C-004; Roadmap S9.1 Inv. 1 | REA-001: on success the first min(old size, new size) bytes are preserved byte-for-byte, whether the storage moves or stays in place. REA-002: on a move, the same preserved range matches byte-for-byte. No byte beyond that range is asserted by this row (see REALLOC-06) | EV-R per case | READY |
| MA-6.1-REALLOC-02 [NORM] | C-003 (C-0nn); MA-REALLOC-PRESERVE-ON-FAILURE (named) | P3-T-REA-011, P3-T-REA-012 (both also serve MA-6.2-022, see 8.3.3) | Live allocation with known contents; new size cannot be satisfied, induced without leaving the contract surface (shim failure or low process limit) | contracts S2 row; Table 6-1 C-003, with its "specified error" wording expressly NOT read as an errno assertion (U-10 open, NEW-3); Roadmap S9.1 Inv. 5 | REA-011: return is null, the original allocation is unchanged in contents, still caller-owned and usable, and no partial move occurs. REA-012: the original remains freeable exactly as before the call and bookkeeping returns to baseline when it is then freed. errno is not asserted | EV-R (EV-S when shim-induced) | READY |
| MA-6.1-REALLOC-03 [NORM] | MA-REALLOC-NULL (named); no C-hook (NEW-1: C-003 / C-004 cover -02 / -01 only) | P3-T-REA-021, P3-T-REA-022 | First argument is NULL. REA-021 uses a size with size > 0, representable and satisfiable; REA-022 uses a representable size made unsatisfiable as in REA-011 | contracts S2 row (behaves as `malloc(size)`, including its success and failure cases) | REA-021 behaves as a successful allocation for that size: non-null return, ownership passes, releasable by the corresponding free. REA-022 behaves as the failure case for that size: null return, no ownership passes. Zero size is NOT used in either case: it would import the unresolved MALLOC-03 / U-09 question, which is CALLOC-04 / MALLOC-03 territory, not this row | EV-R per case | READY |
| MA-6.1-REALLOC-06 [NORM] | MA-REALLOC-GROW-TAIL (named); no C-hook (NEW-1) | P3-T-REA-031 | Live allocation grown successfully to a larger new size (the REA-001 grow setup) | contracts S2 row: no preservation claim MUST be read into bytes beyond min(old, new) | The case asserts ONLY that no preservation is claimed for the tail: it checks and records the preserved range (first min(old, new) bytes, as in REA-001), and records the tail (bytes beyond min(old, new)) as **unspecified** in the evidence record. It **never asserts tail contents** — neither that they are preserved, zeroed, nor any other value. A record or report claiming a tail value fails this case's own specification | EV-R | READY |
| MA-6.1-REALLOC-04 | C-006 (C-0nn, shared with MALLOC-03) ([UNRESOLVED: U-12]) | P3-T-REA-041 | None can be stated: `realloc(p, 0)` with p a non-null live pointer is UNRESOLVED (U-12 / V-04) | contracts S2 row; V-04 | No pass criterion exists and none is written; in particular neither the return value nor the ownership / freeability of p is asserted. Unblocked only by a U-12 / V-04 decision recorded per target in the annex; only then is a per-target-policy instance specified under C-006 with a `-V<nn>` suffix. A result matching another target's policy would then be a failure, per C-006 in substance | EV-R (blocked-test record only) | BLOCKED-by-U-12 |
| MA-6.1-REALLOC-05 | None — UB (contracts S6.2) | none (no case ID minted, by rule) | Pointer does not refer to a live allocation, or was already released | contracts S2 row; contracts S6.2 verbatim rule | No hook, no expected result, no pass criterion. Debug / sanitiser observations of this event, if any are ever specified, are DIAGNOSTIC-ONLY under the separate debug-detection material (MA-6.2-033, Builder B's ERR / FAIL diagnostic cases), separated from conformance; they are not a case in this group | none (ledger entry only) | NO-TEST-UB |

### 8.3.3 Ownership cross-references — MA-6.2-022 / MA-6.2-023

These two rows are cross-references only (contracts
S3 wording). They get no new case IDs: their content
is already the REA content above, written once there.

| Contract ID | Hook ID (+ scheme) | Case ID(s) | Precondition | Oracle | Pass criterion | Evidence produced | Status |
|---|---|---|---|---|---|---|---|
| MA-6.2-022 [NORM, cross-ref to REALLOC-02] | C-322 (C-3nn slot); no C-hook (NEW-1) | P3-T-REA-011, P3-T-REA-012 (home rows in 8.3.2) | As in the REALLOC-02 row | contracts S3 row; Roadmap S9.1 Inv. 5 | A failed realloc leaves the original block valid, unchanged in ownership, and freeable by the caller exactly as before the call — exactly the REA-011 / REA-012 criteria | EV-R (via REA records) | READY |
| MA-6.2-023 [NORM, cross-ref to REALLOC-01] | C-323 (C-3nn slot); no C-hook (NEW-1) | P3-T-REA-001, P3-T-REA-002 (home rows in 8.3.2) | As in the REALLOC-01 row | contracts S3 row; Roadmap S9.1 Inv. 1 / 6 in substance | A successful realloc ends ownership of the original block and begins ownership of the returned block, with no interval in which both or neither is owned by the caller — checked as: the old pointer is not used after a move, the new block is retired exactly once, and bookkeeping returns to baseline | EV-R (via REA records) | READY |

### 8.3.4 COND derivation table — reallocarray (gated) + out-of-scope COND families

Gate rule for every row in this subsection. The
`reallocarray` rows are [COND: target requires
`reallocarray` and its semantics are defined per
target]. That gate, and presence outside it, are
U-16 / U-01 territory. Until the gate holds for a
target — recorded in that target's annex — each case
below stays **BLOCKED-by-U-16** on that target.
"BLOCKED" here does not mean the behaviour is
undefined: the contract behaviour in the row is
Defined under its gate. It means no instance may be
promoted to a pass claim for a target whose annex
does not record the gate as holding. MA-COND-
REALLOCARRAY-04 states the same point from the
gate-off side ([UNRESOLVED: U-16]: no presence,
behaviour, or linkage claim outside the gate); it
is recorded here and mints no separate case.

| Contract ID | Hook ID (+ scheme) | Case ID(s) | Precondition | Oracle | Pass criterion | Evidence produced | Status |
|---|---|---|---|---|---|---|---|
| MA-COND-REALLOCARRAY-01 [COND] | MA-COND-REALLOCARRAY-SUCCESS (named); no C-hook (NEW-1: no COND row has any C-001..C-010 hook) | P3-T-COND-001 | Gate holds for the target per its annex (else BLOCKED, below); pointer is live or NULL; count x size is representable and satisfiable | contracts S2 row; MA-6.1-REALLOC-01 for preservation | The call behaves as realloc to the represented product size: success return, and contents preserved as in REALLOC-01 over the first min(old, product) bytes | EV-R | BLOCKED-by-U-16 |
| MA-COND-REALLOCARRAY-02 [COND] | MA-COND-REALLOCARRAY-OVERFLOW, MA-COND-REALLOCARRAY-PRESERVE (named); no C-hook (NEW-1) | P3-T-COND-002 | Gate holds per annex (else BLOCKED); count x size cannot be represented; original allocation, if any, has known contents | contracts S2 row | Overflow is checked before multiplication is relied on; the call is treated as failure (null return) and the original allocation, if any, is left unchanged and still caller-owned. errno is not asserted (U-10) | EV-R | BLOCKED-by-U-16 |
| MA-COND-REALLOCARRAY-03 [COND] | MA-COND-REALLOCARRAY-PRESERVE (named); no C-hook (NEW-1) | P3-T-COND-003 | Gate holds per annex (else BLOCKED); failure other than overflow, induced as in REA-011; original allocation, if any, has known contents | contracts S2 row; MA-6.1-REALLOC-02 | Return is null and the original allocation, if any, is left unchanged, as in REALLOC-02; no ownership change. errno is not asserted (U-10) | EV-R (EV-S when shim-induced) | BLOCKED-by-U-16 |

Out-of-Phase-3-scope COND families — recorded,
never silently dropped, no conformance case minted
in this plan for either family:

| Contract family | Disposition in this plan | Reason (exact) |
|---|---|---|
| MA-COND-USABLE-01 / -02 [COND], -03 (UB), -04 ([UNRESOLVED: U-13]) | **Out of Phase 3 scope.** No COND case minted. -01 / -02 are recorded here as gate-off in Phase 3; -03 is NO-TEST-UB (no hook, no expected result); -04 is the gate-off row itself. The family's refusal-side expectation is owned once, in the UNS area, as P3-T-UNS-101 (BLOCKED-by-U-13), and is cited there, not duplicated here | Presence and semantics of `malloc_usable_size`, including the null case, are UNRESOLVED (U-13 / V-05) and the symbol's presence is part of the unsettled exported symbol set (U-01). Phase 3's supported-call list (plan S1.2) contains no usable-size call, and B-006 / B-008 assign it no Phase 3 semantics. Unblocked, if ever, by a U-13 decision plus a U-01 symbol-set decision recorded per target in the annex — in a later phase's plan, not by editing this disposition silently |
| MA-COND-CXX-01..08 [COND], -09 (UB), -10 ([UNRESOLVED: U-06]) | **Out of Phase 3 scope.** No COND case minted. -09 is NO-TEST-UB (mismatched pointer / size / alignment for any variant: no hook, no expected result). -10 is the gate-off row itself | The C++ operator set is undecided (U-06, in tension with the pre-main construction path) and its presence is part of the unsettled symbol set (U-01). The Phase 3 build model is a clean C11 core with no C++ in the core (D-006; B-006 S13 item 8), and Roadmap S10's ABI/linking row is NOT-YET-MAPPABLE in Phase 3 for the same reason (plan S6, scout NEW-5b). No presence, ABI, linkage, or exception claim is made for any variant. Unblocked, if ever, by a U-06 decision with U-01, in a later plan |

### 8.3.5 CAL / REA / COND ledger — BLOCKED / NO-TEST-UB / unmapped

- BLOCKED-by-U-09: MA-6.1-CALLOC-04 (P3-T-CAL-031).
  Unblock: per-target U-09 / V-01 decision in the annex.
- BLOCKED-by-U-12: MA-6.1-REALLOC-04 (P3-T-REA-041).
  Unblock: per-target U-12 / V-04 decision in the annex.
- BLOCKED-by-U-16: MA-COND-REALLOCARRAY-01 / -02 / -03
  (P3-T-COND-001 / -002 / -003), until the target gate
  holds per that target's annex (with U-01). The
  gate-off row -04 is recorded at the head of 8.3.4.
- NO-TEST-UB: MA-6.1-REALLOC-05; MA-COND-USABLE-03;
  MA-COND-CXX-09. No case ID, no hook, no expected
  result for any of them (contracts S6.2).
- UNMAPPED-WITH-REASON: MA-COND-USABLE-01 / -02 / -04
  and MA-COND-CXX-01..08 / -10 — out of Phase 3 scope
  for the reasons tabled in 8.3.4. They are accounted
  for in the completeness audit (plan S7.6 / exit
  item 12) under this category, never omitted.
- Hook gap carried forward (NEW-1): CALLOC-03,
  REALLOC-03, and REALLOC-06 have no C-001..C-010
  hook; no COND row has one. This plan mints no new
  C-### hooks and claims no S6 conformance from
  C-001..C-010 alone (see Builder B S7.5, which
  records the same gap for this group).
- errno: asserted in no row above (U-10 open).

## 8.4 Unsupported-behaviour refusal tests (UNS area)

This section **is** the plan for the Roadmap Phase 3
exit sentence "Document unsupported behavior."
(B-006 S13 item 10: a written record of unsupported
items — S12 items, zero-size, explicit alignment,
fork, non-Linux targets, and any call outside the
S13 item 1 supported list.) Exit item 10 in Section
10 below is satisfied, when it ever is, by this
section's record shipped in the exit package — not
by any single runtime result.

**Governing rule (frozen, design S3).** A refusal
case may assert a refusal ONLY where a Defined
failure channel exists in a contract or design row.
Where the behaviour is UNRESOLVED the case is
BLOCKED-by-U-nn and states no expected result the
unresolved item has not supplied. Where it is UB
the case is NO-TEST-UB. **A refusal test never
invents behaviour the contract leaves open.**

Two kinds of UNS case follow from that rule, and
each row says which kind it is:

- **Runtime refusal case** — a call is made and a
  Defined failure is asserted (null return / defined
  failure channel, no ownership passing). Only one
  inventory item supports this kind today, by analogy:
  a request outside the supported size range is a
  defined failure under MA-6.1-MALLOC-04 — and that
  case already lives in MAP / ARITH / FAIL (Builder B),
  so it is cited here, not duplicated. No UNS row
  below is a runtime refusal case; inventing runtime
  refusals for the items below is exactly what the
  governing rule forbids.
- **Documentation / registry audit case (EV-A)** —
  the case checks the written record: the supported-
  call list, the design's NOT-done / unsupported list,
  the annex set, and the registry agree that the item
  is unsupported, unclaimed, and ungated into any
  conformance case. Its pass criterion is about the
  record, never about calling a function that does
  not exist or whose behaviour is open. A missing or
  contradictory record is a FAIL of the audit case.

| Unsupported item | Hook / source | Case ID(s) | Refusal expectation (only where a Defined channel exists) | Evidence produced | Status |
|---|---|---|---|---|---|---|
| Zero-size calls: `malloc(0)`, calloc zero factor, zero-size aligned / posix_memalign, `realloc(p, 0)` | C-006 (C-0nn); MA-6.1-MALLOC-03, CALLOC-04, ALIGNED-05, POSIXMA-04, REALLOC-04; B-006 F-05 / S13 item 1 (zero-size NOT on the supported list) | P3-T-UNS-001 (audit + blocked-test umbrella; the per-row blocked tests themselves live at P3-T-CAL-031, P3-T-REA-041, P3-T-FAIL-041 and in Builder B's ALIGN rows — cited, not duplicated) | No runtime refusal is asserted: no behaviour — neither unique pointer, null, free, nor keep — is promised by any source row. The audit asserts only that the written record lists zero-size as unsupported / unresolved and that no conformance case in the registry claims a zero-size result | EV-A + blocked-test records via the cited cases | BLOCKED-by-U-09 (and BLOCKED-by-U-12 for the `realloc(p, 0)` form, named in the row) |
| Explicit alignment beyond the B-007 scope / supported list | MA-6.1-ALIGNED-03 / -04, POSIXMA-03; B-006 S12.1 (alignment detail deferred to B-007), S12.3 / S13 item 10 (explicit alignment listed unsupported in B-006) | P3-T-UNS-011 | No runtime refusal is asserted: alignment validity and the size-multiple rule are UNRESOLVED, so no failure model exists to assert (scout NEW-2: the invalid-alignment half of C-007 is unfalsifiable until decided). The audit asserts only that B-006's record lists explicit alignment as unsupported and that alignment conformance, where specified, is Builder B's ALIGN group under the final B-007 design (reconciled at banking, S12 INC-B009-10) | EV-A | BLOCKED-by-U-11 (numeric guarantee also U-15, named here) |
| calloc / realloc on the B-006-only surface (historical state, superseded at banking) | B-006 S12.2 (no `mimir_calloc` / `mimir_realloc` label, signature, or semantics stated or implied in B-006); B-008 S3 / S4 / S7.2 (the pair as finally designed, by analogy under U-03) | P3-T-UNS-021 | Audit only, and it is Defined as a record: B-006 alone contains no calloc / realloc claim, and no runtime call may be specified from B-006 alone. As reconciled at banking (S12 INC-B009-10), the namespaced pair now exists as B-008 design surface and its conformance home is 8.3 under the B-008 S7.2 amended supported list — by analogy under U-03, never as a direct contract claim, and never as an implemented fact | EV-A | READY (audit specification, NOT run) |
| Interposition / standard-name replacement (exporting or replacing `malloc`, `free`, etc. for other consumers) | B-006 S12.3 (interposition / ABI deferred to Phase 5; no standard-name replacement in Phase 3); U-03 / U-01 | P3-T-UNS-031 | Audit refusal only: Phase 3 claims no interposition and no standard-name replacement, and the namespaced pair itself has no ABI / symbol / stability claim (U-03). No runtime interposition test is specified in Phase 3; a suite run that resolved a standard name to MimirAlloc would be outside this plan, not a pass under it | EV-A | READY (audit specification, NOT run) |
| Size classes / splitting / coalescing / reuse / trim / caches and sharding as allocator behaviour | B-006 S8.5 / S12.3 (no reuse in the baseline; size classes Phase 7; caches / sharding Phase 8; L3 pass-through, L4 degenerate, L7 compiled out) | P3-T-UNS-041 | Audit refusal only: none of these behaviours is claimed, and no case may assert them — in particular no case asserts that a later allocation reuses a freed address (addresses are never oracles; FREE-01 promises eligibility for reuse only), and the ownership-model fuzzer (plan S4.3d) gates `trim` and out-of-scope operations out of its operation set. A fuzzer trace containing a trim operation in Phase 3 fails the audit | EV-A | READY (audit specification, NOT run) |
| Statistics / trim API (and any namespaced stats surface) | U-03 (namespaced pair plus stats / trim in Roadmap S7.1 are not in contracts S6 scope; scope decision needed); B-006 S12.3 (stats / L7 to Phase 10, L7 compiled out) | P3-T-UNS-051 | No refusal is asserted and no presence or absence behaviour is promised: whether any stats / trim surface is even in scope is itself the open U-03 decision. The blocked case records the item so it cannot be silently treated as supported; it asserts only that no Phase 3 conformance case depends on a stats / trim call | EV-A (blocked-test record only) | BLOCKED-by-U-03 |
| Fork claims: lock state in the fork child (U-17), first allocation in the child (U-18), atfork ordering vs host / other libraries (U-19), stats / diagnostics across fork (U-20), fork during bootstrap (U-21) | MA-6.3-040 / -041 / -042 (only -041's parent-side half is Defined; -042 expressly withholds any other fork behaviour as Defined); B-006 S12.3 / INC-B006-7 (no fork claim) | P3-T-UNS-061 (U-17), P3-T-UNS-062 (U-18), P3-T-UNS-063 (U-19), P3-T-UNS-064 (U-20), P3-T-UNS-065 (U-21) | No runtime refusal and no fork-safety expectation is asserted for any of the five: each behaviour is UNRESOLVED under its own U-item, under the U-05 umbrella. What IS Defined, and what the accompanying record asserts, is only MA-6.3-041's parent-side half: allocation / free in the parent immediately before fork follows S3, and fork preparation MUST NOT invalidate, move, or change ownership of live parent blocks. That parent-side assertion is checked, if at all, without asserting anything about the child | EV-A (blocked-test records only) | BLOCKED-by-U-17 / -U-18 / -U-19 / -U-20 / -U-21 respectively (all under U-05) |
| Use of any MimirAlloc function from a signal handler | MA-6.3-030 [Defined, C-430]; MA-6.3-031 (no claim may be stated / implied), MA-6.3-032 (no promise about state observable from a handler; a handler MUST NOT call the allocator) | P3-T-UNS-071 | This item DOES have a Defined channel, and it is a prohibition, not a result: no function is claimed async-signal-safe without a separate individual proof plus a contract row, and no such proof / row exists in Phase 3. The case is therefore an audit plus a suite-construction rule: the record asserts no async-signal-safety claim appears in docs / headers / diagnostics, and no conformance case calls an allocator function from a handler or asserts handler-observed state. It specifies NO runtime handler call whose outcome could be read as a result | EV-A | READY (audit specification, NOT run) |
| Android / bionic / musl targets (and Termux / proot environments) | Roadmap S5 / SUPPORTED_TARGETS (Phase 3 exit targets are x86-64 + ARM64 Linux + glibc only, plan S3.1); U-04 (authoritative musl / bionic sources missing; contracts cite insufficient sources for non-glibc columns) | P3-T-UNS-081 | Audit refusal only: these targets get no annex in this plan, no case is instantiated for them, and no evidence gathered on them may be worded as satisfying either exit target (plan S6 Platform row, NEW-5e). No behaviour on them is asserted — non-glibc contract columns are themselves UNRESOLVED (U-04) | EV-A | READY (audit specification, NOT run; the underlying non-glibc behaviour remains BLOCKED-by-U-04, named here) |
| Performance claims of any kind (speed, throughput, latency, memory-efficiency superiority) | Roadmap Phase 3 text (a slow mapping-per-allocation prototype is acceptable ONLY as an explicitly experimental correctness baseline; do not ship it as a performance allocator); B-006 S12.3 (any performance claim: none) | P3-T-UNS-091 | Audit refusal only: no timing value is ever an oracle in this suite (plan S5), no PASS record may carry a performance conclusion, and timestamps in evidence records are recorded, never oracles (Section 9). A report drawing a performance conclusion from a Phase 3 run fails this audit case | EV-A | READY (audit specification, NOT run) |
| `malloc_usable_size` outside its COND gate | MA-COND-USABLE-04 ([UNRESOLVED: U-13]); V-05 | P3-T-UNS-101 (single home for the USABLE refusal side; the COND disposition is recorded in 8.3.4 and cites this case, not a duplicate) | No presence, semantics, or null-case result is asserted outside the gate. See the NO-TEST-UB note for USABLE-03 in the ledger below | EV-A (blocked-test record only) | BLOCKED-by-U-13 (presence also U-01, named here) |

### 8.4.1 UNS ledger — BLOCKED / NO-TEST-UB / audit-only

- BLOCKED-by-U-09 / -U-12: zero-size umbrella
  (P3-T-UNS-001). BLOCKED-by-U-11 (with U-15):
  alignment beyond scope (P3-T-UNS-011).
  BLOCKED-by-U-03: stats / trim API (P3-T-UNS-051).
  BLOCKED-by-U-17..U-21 (under U-05): fork cases
  P3-T-UNS-061..065. BLOCKED-by-U-13 (with U-01):
  usable-size refusal (P3-T-UNS-101).
- NO-TEST-UB in this section: none minted as a
  case. For completeness, the UB events nearest
  this section — calling with an invalid / released
  pointer (REALLOC-05, USABLE-03, CXX-09) — are
  NO-TEST-UB in the 8.3.5 ledger; signal-handler
  reliance is not UB here, it is the Defined
  prohibition of MA-6.3-030 / -032 above.
- READY audit-only (no runtime refusal asserted):
  P3-T-UNS-021, -031, -041, -071, -081, -091.
  "READY" for these means the audit specification
  is complete now; it does not mean any audit has
  been performed, and it never licenses a runtime
  claim about the unsupported item itself.

## 9. Evidence schema — per-case records and artefact manifest

Design only. This section defines what a future run
MUST produce; it produces no evidence itself. One
record exists **per case, per target, per run**. A
record missing any mandatory field is not a PASS
record: the evidence collector rejects it and the
case counts as having no record (plan S4.3f).

### 9.1 Result vocabulary and record semantics (frozen)

| Result | Meaning (exactly this, nothing stronger) |
|---|---|
| PASS | The case ran on that target in that run, every mandatory field below is present, the exit status and captured output satisfy the case's stated pass criterion, and every referenced artefact exists with a matching SHA-256. PASS is per case / per target / per run; it never aggregates across targets and never implies any other case |
| FAIL | The case ran and its pass criterion was not satisfied — including a defined-behaviour divergence, a watchdog expiry (contracts S6.3: expiry = FAILURE, not inconclusive), and a case-process **crash or timeout**, which are FAIL (with the crash / timeout recorded as the observed outcome), **never PASS and never silently reclassified**. A FAIL record carries the same mandatory fields, with the failing observation stated |
| ERROR | The harness itself failed to execute or record the case as specified (runner crash, shim / watchdog malfunction, schema-incomplete capture, lost artefact). ERROR is a statement about the harness, not about the allocator, and it is **never PASS**: the case counts as unexecuted for exit purposes until a clean re-run produces a PASS / FAIL / BLOCKED record. A runner crash is a case ERROR (plan S4.3a) |
| BLOCKED-by-U-nn | The case could not be promoted to a pass/fail judgement because the named U-item(s) are undecided (or, for COND cases, the target gate does not hold). A BLOCKED record states the blocking item(s), what decision unblocks it, and the annex state; it carries **no expected result** and is never counted toward a PASS total |
| DIAGNOSTIC-ONLY | An observation specified only as diagnostic (debug / sanitiser detection of UB-class events, errno recorded as a diagnostic, addresses / timing recorded as diagnostics). Filed separately from conformance records; never a PASS record and never evidence for an exit item |

Crash and timeout rule, stated once for the whole
plan: **a crash or a timeout is ERROR (harness /
execution failure) or FAIL (the case's criterion,
including a watchdog bound, was not met) — in
either classification it is never PASS, never
BLOCKED, and never omitted from the run's records.**
Which of ERROR / FAIL applies is decided by the
case specification (watchdog expiry is FAIL by
definition; a runner crash before the case process
ran is ERROR) and the record states the basis.

### 9.2 Mandatory fields — every PASS record (and, mutatis mutandis, every FAIL / ERROR / BLOCKED record)

1. Case ID, including any `-V<nn>` per-target-policy
   variant suffix and the V-item it is keyed to.
2. Contract ID(s) the case serves (all of them, where
   a case serves two rows — e.g. P3-T-CAL-003,
   P3-T-REA-011 / -012).
3. Hook ID **and scheme** (C-0nn / C-3nn / C-4nn /
   named / none), naming every scheme separately;
   the three schemes are never conflated (S1.5).
4. Suite commit (full hash) and case-registry version.
5. Build hash (binary / source-tree hash as defined
   by the build model, plan S4.5), plus toolchain:
   compiler, compiler version, and flags (clean C11 /
   D-006 build), and build date.
6. Target manifest, complete: machine ID;
   architecture (`x86-64` / `ARM64`); OS and version;
   kernel; libc and version; environment (native /
   other, labelled — Phase 3 exit evidence is native
   Linux on the two exit targets only); **runtime-
   discovered system page size, the query method
   used (e.g. `sysconf(_SC_PAGESIZE)`), the failure
   check applied to that query and its outcome, and
   the date the manifest was recorded and the machine
   it was recorded on**. No page-size value is ever
   copied from another machine's manifest.
7. Annex ID and annex version applied, with the
   annex's dispositions for every V-/U-item the
   case's precondition cites.
8. Exact command line as run (verbatim, including
   the case selection and any harness options).
9. Seed: the fuzzer / randomised seed where the case
   is seeded; for deterministic cases, the fixed
   seed or the explicit none-by-design flag.
10. Fault-injection parameters where applicable:
    the N value, the counting origin, and the swept
    range the run belongs to — or an explicit none.
11. Watchdog bound applied where applicable, or an
    explicit none; where the bound is U-05-dependent,
    the bound's preliminary status (see exit item 17).
12. Precondition state as instantiated (annex
    decisions in force, limits / shim configuration).
13. Oracle invoked (source document + row, and its
    kind, per plan S7 method step 6).
14. Exit status of the case process (exit code, or
    signal / timeout / crash as observed), plus
    start and end timestamps — recorded, **never
    an oracle**.
15. Artefact paths and a SHA-256 for **every**
    artefact the record references. An artefact
    without a hash, or with a mismatching hash, is
    not evidence and the record is not a PASS record.
16. Result (from 9.1), with the observed outcome
    stated for FAIL / ERROR / BLOCKED.
17. Differential pair reference: the record ID of
    the matching record for the same case ID on the
    other exit target, or the explicit pending-pair
    marker. A PASS record with a pending pair is a
    valid per-target PASS and is **not** differential
    evidence until the pair exists (Section 10,
    item 15).

### 9.3 Artefact manifest (one per run)

Every run produces exactly one artefact manifest.
Each manifest entry states: artefact name (following
the run's name pattern), producing case or component,
format, SHA-256, the evidence record(s) referencing
it, and a retention note. No artefact is evidence
without its hash plus the record that references it.

Named artefact classes (design S5, carried in full):

| Artefact class | Produced by | Contents (design level) |
|---|---|---|
| Per-case stdout / stderr captures | Runner, per case | Verbatim captured streams for that case process, unedited |
| Fuzzer seed + trace files | Ownership-model fuzzer | Seed set as run, versioned trace of the operation sequence, model definition version (plan S4.3d) |
| Shim injection log | Fault-injection shim | N, counting origin, attempts counted, failure class injected, uninjected control-run reference |
| Watchdog log | Watchdog | Bound applied, cases bounded, expiry events if any, U-05 preliminary mark where applicable |
| Target manifests | Runner / collector, per target | The full field set of 9.2 item 6, one per target per run |
| Annex copies as applied | Collector, per target | Byte copies of the annex versions named in the records, so a record can be re-checked against the exact annex text |
| Differential report | Differential comparator | Paired-record comparison over defined observable behaviour only, with every DIVERGENCE record listed against its case ID (plan S5) |
| BLOCKED / NO-TEST-UB ledger export | Collector | Every BLOCKED-by-U-nn and NO-TEST-UB row for the run, with its blocking item(s) / S6.2 basis — the export that feeds exit items 12 and 17 |
| Completeness-audit artefact | Audit (plan S7.6) | Denominator list with its category per contract row (derived / BLOCKED / NO-TEST-UB / unmapped-with-reason), audit date, suite commit |

Retention note (applies to every class): artefacts
named by an exit-package record are retained with
that package; diagnostic-only artefacts (EV-D) are
retained separately and are never merged into the
conformance artefact set.

## 10. Phase 3 exit evidence definition

Numbered checklist. Each item states the exact
artefact(s) and PASS records that must exist before
Phase 3 may be claimed exited. **Every item is
marked UNMET in this issue of the plan.** Items
1–10 restate B-006 S13 items 1–10 in checklist
form; items 11–20 are the extensions defined in
design-B009 S6. No item is met by this plan itself:
a plan is not evidence, a PLANNED case is not a
PASS record, and ARM64 evidence is not x86-64
evidence.

| # | Exit item (exact evidence required) | Required artefacts / PASS records | Target coverage | Status |
|---|---|---|---|---|
| 1 | Supported-call list stated before any pass claim | The S13 item 1 list, verbatim in substance, at the head of the exit package: `mimir_malloc` size > 0 and representable; `mimir_free` of a live in-scope pointer or NULL; by analogy MA-6.1-MALLOC-01 / -02 / -04, MA-6.1-FREE-01 / -02, MA-6.2-010; zero-size not on the list; nothing else supported | Both targets (the list the package's claims are read against) | **UNMET** |
| 2 | Test-hook evidence with the hook-to-row mapping shown, not asserted | PASS records for hooks C-005, C-009, C-010 plus the MA-MALLOC / MA-FREE named hooks, cited exactly without conflating the three hook schemes, with the mapping table included in the package. Carried qualification (NEW-4 / NEW-7): read with items 12 and 19 — this B-006 set contains no calloc / realloc / alignment hook, so items 12 / 19 must additionally account for the B-007 / B-008 hook evidence (at minimum C-001..C-004) or the package must state Phase 3 exit remains partial; and C-010 evidence is PRELIMINARY while U-05 is open (item 17) | Both targets | **UNMET** |
| 3 | Passing on BOTH named targets | A named ARM64 Linux environment record AND a named x86-64 Linux environment record (machine, OS, kernel, libc, toolchain each), with the suite's PASS records on each. The x86-64 leg is OWED in full in this campaign (no x86-64 host exists); no ARM64 / Pi result may be presented as covering it | Both targets, separately | **UNMET** |
| 4 | Data-integrity cycles returning to baseline | PASS records for repeated allocate / write / verify / free cycles: written bytes intact, distinct live blocks non-overlapping (Inv. 1), counters / bookkeeping back to baseline after each cycle, no leaked mapping / block / count | Both targets | **UNMET** |
| 5 | Fail-the-Nth-mapping sweep plus recovery | PASS records for the swept-N injection set (defined failure: null, no partially constructed / half-owned block, MA-6.2-020 / -024) plus the recovery probe: the next uninjected attempt succeeds. Shim logs (9.3) included | Both targets | **UNMET** |
| 6 | Minimal concurrency correctness | PASS records for concurrent alloc / free including cross-thread handoff (MA-6.2-054, hook C-009): ownership preserved exactly, no node in incompatible lists (Inv. 1 / 6 / 9). Correctness only; no performance claim may be attached | Both targets | **UNMET** |
| 7 | Per-environment system-page-size records | One target manifest per environment (9.2 item 6): runtime-discovered system page size, query method, failure check, date, machine, qualified as system page size (MA-6.2-041). The Pi's observed 16384 is a record for that Pi only, never a substitute for another machine's record | Both targets, per environment | **UNMET** |
| 8 | Clean C11 / D-006 builds | Build evidence per target: clean build under D-006 (portable C11 core, C ABI, no C++ in the core), compiler and flags recorded in every record (9.2 item 5). Toolchain probes are not build evidence | Both targets | **UNMET** |
| 9 | NOT-done list shipped inside the exit package | B-006 S12 (or its successor) included verbatim in the exit package: every deferred / unsupported item the package does not claim, unedited | Both targets (one list, shipped with the package) | **UNMET** |
| 10 | Documented unsupported behaviour | Section 8.4 of this plan, as reconciled at banking, shipped in the exit package as the written record satisfying the exit sentence "Document unsupported behavior.", together with its UNS ledger (8.4.1) | Both targets (the record the claims are read against) | **UNMET** |
| 11 | This plan itself, frozen, with its registry | The integrated plan at a named commit, with Appendix A registry at the same commit; every PASS record in the package cites a suite commit and registry version consistent with it (9.2 item 4) | Both targets (one plan, both packages) | **UNMET** |
| 12 | Contract and S10 completeness audits | The completeness-audit artefact (9.3): every contracts SS2–4 [NORM] / applicable [COND:] row accounted for as derived / BLOCKED-by-U-nn / NO-TEST-UB / unmapped-with-reason, with zero unaccounted rows and the hook-to-row mapping shown (answering NEW-1); plus the S10 completeness artefact (item 13's companion record in plan S6 terms): all 10 Roadmap S10 matrix rows mapped or NOT-YET-MAPPABLE with a reason | Both targets (audits run over the whole registry; per-target applicability recorded per row) | **UNMET** |
| 13 | S10 completeness artefact (as an exit item in its own right) | The plan S6 mapping artefact at exit time: each of the 10 S10 rows (C contracts, Data integrity, Ownership, Concurrency, Memory, Robustness, ABI/linking, Platform, Tooling, Regression) either maps to named case IDs with PASS records or is recorded NOT-YET-MAPPABLE with its blocking U- / phase item — including the Memory, ABI/linking, Tooling, and Platform qualifications of NEW-5, stated, not passed over | Both exit targets only; non-exit environments expressly excluded | **UNMET** |
| 14 | Both per-target annexes, completed | ANNEX-X86-64 and ANNEX-ARM64 in the Appendix B template: V-01..V-07 dispositions selected per target; U-01, U-02, U-09..U-15 decided per target or explicitly marked still-open (a still-open mark keeps every dependent case BLOCKED — it completes the annex as a record, it does not unblock anything); watchdog bound entry (U-05); symbol-set entry (U-01); page-size record | One completed annex per target | **UNMET** |
| 15 | Differential report over defined behaviour only | The differential report artefact (9.3) over paired same-case records from both targets, comparing only defined observable behaviour (never pointer addresses or timing), with every DIVERGENCE record closed or carried as an open defect — none averaged, none resolved by preferring the ARM64 result | Both targets, paired | **UNMET** |
| 16 | Fuzzer seed / trace evidence | Seed set as run, versioned traces, and the ownership-model definition as run (one owner, retire exactly once, no overlap, live bytes intact), with operations gated per the supported-call list (realloc / trim only where their group is in scope). A model violation is a FAIL with its seed + trace as the artefact | Both targets | **UNMET** |
| 17 | C-010 watchdog status carried, never omitted | Watchdog evidence for hook C-010 (bootstrap / reentrancy cases, plan INIT group) **or** its BLOCKED-by-U-05 status, carried into the exit package in either case and labelled **PRELIMINARY** for as long as U-05 is open (contracts S6.3, NEW-7). Omitting C-010 from the package, or presenting its evidence without the preliminary label while U-05 is open, fails this item | Both targets | **UNMET** |
| 18 | Regression register | The reproducer-per-defect register artefact: the rule (every confirmed defect gets a minimal reproducer that becomes a registry case) plus the register itself. Empty-at-design-time is a stated fact in this plan, not a gap; at exit time the register must list every defect confirmed against the implementation, each with its reproducer case ID, or state that none was confirmed | Both targets (reproducers run per target) | **UNMET** |
| 19 | B-007 / B-008 reconciliation record | A written record that this plan, as banked, was reconciled against the FINAL B-007 and B-008 designs: every drift found, its disposition, and any resulting case / status changes recorded in plan S12. This Part C's 8.3 derivation from contracts.md directly (flagged at its head) is an explicit input to that record | Both targets (one record, shipped with the package) | **UNMET** |
| 20 | Cannot-prove-yet shipped inside the exit package | Section 11 of this plan, as updated at exit time, included inside the exit package itself — mirroring item 9's NOT-done rule — so no reader of the package can mistake its PASS records for proof of the things Section 11 lists | Both targets (one list, shipped with the package) | **UNMET** |

**Closing statement (verbatim in substance).**
Until every item above exists as recorded evidence
on **both** targets, **Phase 3 exit is UNMET**.
Phase 3 exit stays UNMET until an implementation
exists **and** the suite passes on **both** x86-64
and ARM64 Linux. No subset of these items, no
single-target package, and no plan — including this
one — may be presented as a Phase 3 exit.

## 11. Cannot-prove-yet

Everything in this section is unproven by this plan
and remains unproven until the named missing thing
exists. This section ships inside the exit package
(exit item 20); it is not an appendix to be dropped.

- **Implementation existence.** No implementation
  exists. There is nothing to build, run the suite
  against, or produce a single PASS record with.
  Every case in this plan is PLANNED, never run.
- **x86-64 host.** No x86-64 host exists in this
  campaign and no MimirAlloc evidence of any kind
  exists for x86-64 (SUPPORTED_TARGETS: "Evidence
  on record today: None for MimirAlloc"). The whole
  x86-64 leg — builds, PASS records, annex, page-
  size record, differential pairing — is OWED.
- **Per-target decisions U-01 / U-02 / U-09..U-15.**
  All open, and each gates the cases named:
  U-01 final exported symbol set per target (gates
  C-008's target set, every COND gate, UNS-031);
  U-02 register-level zero-size and `realloc(p, 0)`
  semantics (detail in U-09 / U-12); U-09 zero-size
  (CAL-031, UNS-001, FAIL-041); U-10 errno guarantee
  (no case in this plan asserts errno, and none may
  until this is decided per target); U-11 alignment
  validity and the size-multiple rule, with U-15
  numeric default alignment and its relation to
  page size (UNS-011; no numeric alignment value is
  asserted anywhere in Part C); U-12 `realloc(p, 0)`
  (REA-041, UNS-001); U-13 `malloc_usable_size`
  presence / semantics including the null case
  (UNS-101, 8.3.4 USABLE disposition); U-14 exact
  `posix_memalign` error numbers per target (no
  Part C case depends on them; Builder B's ALIGN
  group carries that block); U-15 as above.
  Also open where they touch Part C: U-03 (the
  namespaced surface and stats / trim scope itself,
  UNS-051), U-04 (non-glibc sources, UNS-081),
  U-06 (C++ operator set, 8.3.4 CXX disposition),
  and U-16 (the `reallocarray` gate, COND-001..003).
- **Watchdog and fork evidence — U-05, U-17..U-21.**
  The C-010 watchdog bound itself is [UNRESOLVED:
  U-05]; every watchdog-dependent result is
  PRELIMINARY until U-05 is decided and cannot
  support a promotion claim (contracts S6.3).
  Fork behaviour in the child — lock state (U-17),
  first allocation (U-18), atfork ordering (U-19),
  stats / diagnostics across fork (U-20), fork
  during bootstrap (U-21) — is unproven and
  unprovable under this plan: UNS-061..065 are
  blocked records, not tests awaiting a run.
- **Numeric alignment (U-15).** No numeric value
  for fundamental / default alignment, and no
  relation between alignment and system page size,
  can be proved or even asserted: V-07 leaves both
  open, and the Pi's observed system page size
  (16384, this Pi only) proves nothing about either.
- **errno (U-10).** No errno value, and no promise
  that errno is set at all, can be proved for any
  allocation failure. Any future evidence record
  that treats an errno value as an oracle before a
  per-target U-10 decision exists is invalid.
- **B-007 / B-008 final designs — reconciled, still
  not proof.** Both designs are now banked (main
  c1f71e9) and this plan was reconciled against them
  at B-009 banking (S12 INC-B009-10): the B-008 S7.2
  supported-list amendment and the B-007 INC-B007-1
  formula discrepancy were the two drifts found and
  are recorded there. Reconciliation proves only
  that the plan and the designs now agree as texts;
  it proves nothing about any implementation, and
  exit item 19 still requires the reconciliation
  record to ship inside the exit package.
- **Sanitizer / Valgrind visibility limits.** Per
  the Roadmap S10 Tooling row: ASan, UBSan, TSan,
  and Valgrind may be run "where compatible", and
  running a tool is not coverage. A custom
  allocator's visibility to these tools is limited
  and combination-dependent; unsupported
  combinations must be named, and no tool run in
  this plan's future execution proves the absence
  of the defects the tool cannot see into the
  allocator's own mappings and metadata. Tooling
  records are evidence about the tool run, recorded
  under Section 9 — never a substitute for the
  contract cases.
- **Anything needing a Phase 4+ host or phase.**
  Non-glibc targets, interposition, size classes,
  caches, budgets, and performance properties are
  later-phase matters (8.4); this plan can prove
  nothing about them and claims nothing about them.

## 12. Inconsistencies recorded (design)

Recorded faithfully; recording does not resolve. INC-B006-1
.. INC-B006-10 are carried from the B-006 design S14 and are
not renumbered. New items found by this slice are INC-B009-*.

- **INC-B009-1 - HOOK COVERAGE GAP (from scout NEW-1):**
  contracts.md Table 6-1 maps only a fraction of [NORM] rows
  to C-001..C-010. No C-hook exists for MA-6.1-MALLOC-01 /
  -02 / -04, MA-6.1-FREE-01, MA-6.1-CALLOC-03,
  MA-6.1-REALLOC-03 / -06, MA-6.1-ALIGNED-02, or any COND
  reallocarray / usable / C++ row; most MA-6.2-* and
  MA-6.3-* rows rest only on C-3nn / C-4nn slots that Table
  6-1 does not list. Contracts S6.1 concedes the rows are
  "to be added", so its own mapping rule is currently
  unsatisfied and S7 records none of them as
  unmapped-with-reason. This plan answers with the S7.5 /
  S7.6 audit rule; it does not mint new C-### hooks and
  does not claim S6 conformance from C-001..C-010 alone.
- **INC-B009-2 - C-007 INVALID HALF UNTESTABLE (NEW-2):**
  hook C-007 requires an invalid-alignment request to "fail
  by the specified model", but that model is UNRESOLVED
  (MA-6.1-ALIGNED-03 / -04, U-11 via V-03). The invalid
  half of C-007 is BLOCKED-by-U-11 in S8.2; only the valid
  half is READY.
- **INC-B009-3 - ERRNO WORDING CONFLICT (NEW-3):** the
  C-001 / C-006 pass criteria invoke "errno as specified",
  but errno on failure is UNRESOLVED (U-10 via V-02) and
  B-006 S10 expressly promises NO errno for the namespaced
  pair. This plan never asserts errno for mimir_malloc /
  mimir_free; errno instances exist only per-target,
  through an annex, where a future contract decision
  promises it.
- **INC-B009-4 - B-006 S13 UNDER-SCOPES THE EXIT (NEW-4):**
  B-006 S13 item 2 requires only C-005 / C-009 / C-010 plus
  named MA-MALLOC / MA-FREE hooks, omitting the calloc /
  realloc / zero-size / alignment evidence the Roadmap
  Phase 3 text bundles into the same exit. S10 item 2 of
  this plan is therefore read together with the B-007 /
  B-008 hook evidence (C-001..C-004 at minimum), as noted
  in S6; read alone, S13 would under-scope the exit.
- **INC-B009-5 - S10 ROWS WITHOUT S13 ITEMS (NEW-5 / 6):**
  the Roadmap S10 Memory, ABI/linking, Tooling, and
  Regression rows, the S10 closing fuzz paragraph, and 7
  of the 8 Roadmap S9.3 controlled-failure tests have no
  counterpart in B-006 S13 items 1-10. This plan maps
  them anyway (S6) and carries them into the exit
  definition as S10 extension items 11-20; the gap is in
  the source documents, not filled silently here.
- **INC-B009-6 - C-010 PRELIMINARY STATUS DROPPED (NEW-7):**
  contracts S6.3 records C-010 results as preliminary
  while U-05 is open and unable to support a promotion
  claim; B-006 S13 item 2 omits that caveat. This plan
  carries the caveat everywhere C-010 appears (S8.2 INIT,
  S10 exit item 17).
- **INC-B009-7 - S10 FUZZ LISTS TRIM:** the Roadmap S10
  closing paragraph fuzz sequence includes trim, but
  Phase 3 has no trim operation: namespaced stats / trim
  is [UNRESOLVED: U-03] and B-006 S12.3 defers trim to
  Phase 7. The S4.3 fuzzer therefore gates trim out for
  Phase 3; the roadmap wording is recorded, not obeyed
  literally, because obeying it would invent an operation.
- **INC-B009-8 - PARALLEL-SLICE RECONCILIATION OWED:**
  S8.2 and S8.3 are derived from contracts.md while the
  B-007 and B-008 designs were still building in parallel
  worktrees (both at base 243f007, no drafts, when this
  slice began). If a final B-007 / B-008 design narrows or
  renames any behaviour relied on here, this plan drifts;
  reconciliation at banking (keeper, in roadmap order
  after B-008) must diff S8.2 / S8.3 against those final
  designs and record the outcome - S10 exit item 19
  makes that record part of the exit evidence.
  **DISCHARGED at B-009 banking:** the diff was performed
  against the banked texts (main c1f71e9); outcome in
  INC-B009-10 / INC-B009-11 below.
- **INC-B009-10 - BANKING RECONCILIATION PERFORMED
  (B-007 / B-008, main c1f71e9):** S8.2 and S8.3 were
  diffed against the final banked B-007
  (`PHASE_3_ARITHMETIC_ALIGNMENT_INIT_ERROR_DESIGN.md`)
  and B-008
  (`PHASE_3_CALLOC_REALLOC_AND_UNSUPPORTED_BEHAVIOR.md`)
  texts at B-009 banking. Subject coverage verified
  aligned: B-007 Sections A-D (checked arithmetic,
  alignment, clean initialisation, error semantics;
  U-03 / U-09 / U-10 / U-11 / U-14 / U-15 / U-17..U-21
  all carried open there exactly as this plan carries
  them) match the S8.2 ARITH / ALIGN / INIT / ERR
  derivation ranges; B-008 Sections 3-6 (`calloc`,
  `realloc`, combined supported-case index, unsupported
  record) match the S8.3 CAL / REA / COND ranges and
  the S8.4 refusal record, including U-09 / U-12 as
  BLOCKED (B-008 S8 E-5 states the same blocker) and
  B-008 S8 E-1..E-4 / E-6 / E-7 as the evidence this
  plan's S10 items must produce for the B-008 surface.
  TWO DRIFTS FOUND AND FIXED IN-SLICE: (1) this plan
  presented the B-006-only supported-call list and
  treated `calloc` / `realloc` as a standard-name-only
  surface never read across to the namespaced pair;
  the final B-008 S7.2 amends the supported list to add
  `mimir_calloc` / `mimir_realloc` by analogy under
  U-03 — S1.2, the S8.3 scope note, and P3-T-UNS-021
  were corrected to the amended list (design list only;
  nothing is supported anywhere yet). (2) B-007 records
  INC-B007-1 (total-mapping formula discrepancy), which
  this plan had not carried — see INC-B009-11. No case
  ID, status, or pass criterion was changed by this
  reconciliation; exit item 19's record is this entry,
  to be shipped in the exit package.
- **INC-B009-11 - B-007 INC-B007-1 CARRIED OPEN
  (total-mapping formula):** B-007 Appendix C records
  that B-006 S6, S7.1, and S11.1 step 5 state the
  total-mapping size in forms that agree only under
  facts none of them states (UNRESOLVED in B-007).
  This plan does not resolve it: the S8.2.1 boundary
  cases are specified symbolically over the overhead
  terms (reserve, rounding, total-mapping) so they
  check both formula forms, and no case in this plan
  cites either form as canonical. Any future arithmetic
  proof must be stated against the reconciled B-006
  formula once a B-006 revision supplies it.
- **INC-B009-9 - NO TARGET FILENAME (carried pattern):**
  as with B-006 (INC-B006-2), neither WAVE_PLAN.md nor
  the keeper tasking named this slice's deliverable file.
  The name docs/PHASE_3_TEST_SUITE_PLAN.md was chosen by
  this team following the repo's docs/ naming pattern and
  is recorded here so the choice is visible.

---

*End of Sections 1-12. Design only; Proposed. No test in
this plan has been run, no implementation exists for it to
run against, and the Phase 3 exit remains UNMET (S10).*
## Appendix A. Case registry (design)

The single registry of case IDs minted by this plan.
Section S8 tables are each case's one home and definition;
this appendix is the index, and in any disagreement the S8
row governs. IDs are never reused; gaps inside blocks of
10 are allowed and expected. Total minted: 119 case IDs
(SYS 5 incl. DIFF, MAP 11, FAIL 13, DATA 6, ARITH 9,
ALIGN 13, INIT 9, ERR 20, CAL 7, REA 8, COND 3, UNS 15;
EXIT reserved, 0 minted). Derivation-row totals across
S8.1-S8.3: B-006/B-007 groups 70 rows (47 READY, 19
BLOCKED, 4 NO-TEST-UB) plus B-008 group rows per S8.3.5.

| Area | Case IDs minted (all P3-T-<AREA>-<nnn>) | Home |
|---|---|---|
| SYS | 001, 002, 003 | S4 / S6 |
| DIFF | 001, 002 | S5 |
| MAP | 001-003, 011-012, 021, 031-032, 041-042, 051 | S8.1.1 |
| FAIL | 001-003, 011, 021, 031, 041, 051, 061, 081, 091, 101, 111 | S8.1.2 |
| DATA | 001-002, 011-012, 021-022 | S8.1.3 |
| ARITH | 001-002, 011-012, 031-032, 041-043 | S8.2.1 |
| ALIGN | 001-002, 011-012, 021, 031, 041, 051-052, 061-062, 071, 081 | S8.2.2 |
| INIT | 001, 011, 021, 031, 041, 051, 061, 071, 081 | S8.2.3 |
| ERR | 031, 041, 051, 061, 071, 081, 091, 101, 111, 121, 131, 141, 151, 161, 171, 181, 191, 201, 211-212 | S8.2.4 |
| CAL | 001, 002, 003, 011, 012, 021, 031 (031 BLOCKED-by-U-09) | S8.3.1 |
| REA | 001, 002, 011, 012, 021, 022, 031, 041 (041 BLOCKED-by-U-12) | S8.3.2 |
| COND | 001, 002, 003 (all BLOCKED-by-U-16) | S8.3.4 |
| UNS | 001, 011, 021, 031, 041, 051, 061, 062, 063, 064, 065, 071, 081, 091, 101 | S8.4 |
| EXIT | none minted - reserved for the exit package itself | S10 |

Cross-thread hook C-009 lives once, as P3-T-ERR-211 /
-212 (S8.2.4), and is cross-cited from S8.1 - never
duplicated. Per-target-policy variants (hook C-006) take
the -V<nn> suffix of S5 only; no variant instance exists
yet, because no per-target policy has been selected
(U-02 / U-09 / U-12 open).

## Appendix B. Per-target annex template (S10 mapping support)

One annex per exit target, in this fixed template:
`ANNEX-X86-64` and `ANNEX-ARM64`. An annex is the **only**
allowed per-target difference in the suite (Section 5). A
field left blank, or a decision marked still-open, leaves
every case depending on it **BLOCKED** on that target (S4.4).

### B.1 Annex header

| Field | Entry (per target) |
|---|---|
| Annex ID + version | `ANNEX-X86-64` / `ANNEX-ARM64`, version: ___ |
| Suite commit + registry version this annex applies to | ___ |
| Date of record | ___ |
| Status | **Proposed / design only** — no field below is evidence until a run records it |

### B.2 Target manifest fields

| Field | Entry (per target) |
|---|---|
| Machine ID | ___ |
| Architecture | x86-64 / ARM64 |
| OS + version | ___ |
| Kernel | ___ |
| libc + version | ___ |
| Environment (native / proot / other, labelled) | ___ |
| Compiler + version, flags (D-006 C11 build) | ___ |
| Build hash (binary / source-tree hash) | ___ |

Worked example (observed facts, **this Pi only**, dated
2026-11-06 — an example of the form, not evidence for any
other target): machine purpleviking1; architecture aarch64
(ARM64); compiler gcc (Debian 12.2.0-14+deb12u1) 12.2.0;
runtime-discovered system page size 16384 via `getconf
PAGESIZE` (see B.5). The x86-64 annex has **no** worked
example: that leg is OWED in full (S3.4).

### B.3 Selected policies for V-01..V-07

| Variation | Policy selected for this target (or STILL-OPEN) | Blocking U-item |
|---|---|---|
| V-01 zero-size allocation | ___ / STILL-OPEN | U-09 (register detail U-02) |
| V-02 errno on allocation failure | ___ / STILL-OPEN | U-10 |
| V-03 alignment validity + size-multiple rule | ___ / STILL-OPEN | U-11 |
| V-04 realloc(p,0) | ___ / STILL-OPEN | U-12 (register detail U-02) |
| V-05 malloc_usable_size presence/semantics incl. null | ___ / STILL-OPEN | U-13 |
| V-06 posix_memalign error numbers for this target | ___ / STILL-OPEN | U-14 |
| V-07 numeric default alignment | ___ / STILL-OPEN | U-15 |

### B.4 Disposition of U-items for this target

One row per item — U-01, U-02, U-09, U-10, U-11, U-12,
U-13, U-14, U-15 — each stating: the decision taken for this
target, or **STILL-OPEN** with the cases it leaves BLOCKED.
In addition:

| Item | Entry (per target) |
|---|---|
| U-01 symbol-set presence (which symbols, incl. conditional, are present on this target) | ___ / STILL-OPEN |
| U-05 watchdog bound applied on this target (value + unit, or BLOCKED-by-U-05) | ___ / BLOCKED-by-U-05 |

### B.5 Page-size record (per target, per S3.2)

| Field | Entry (per target) |
|---|---|
| Runtime-discovered system page size | ___ (a value recorded from a run, never a literal in suite sources) |
| Query method (e.g. `sysconf(_SC_PAGESIZE)`) | ___ |
| Failure check applied to the query | ___ |
| Date / machine of record | ___ |

### B.6 Instantiation rule (repeated from S4.4)

A case whose precondition cites a V-/U-item is instantiated
for this target **only** through this annex. Absent annex
decision = the case stays **BLOCKED** on this target. No
annex exists in this plan for any non-exit environment
(S3.1).

---

*End of Builder A part (front matter, Sections 1–6,
Appendix B). Sections 7–12 and Appendices A, C, D are the
parts of Builders B and C; case IDs in B/C areas cited in
Section 6 as AREA + contract ID are resolved at integration
from the B/C registries. Nothing in this part is implemented,
measured, or run. Status: **Proposed / design only**.*
## Appendix C. Sources (design)

- Contracts: docs/contracts.md (A-008) - 91 primary MA-*
  rows, all Proposed; S6 Table 6-1 hooks C-001..C-010;
  C-3nn / C-4nn slots; U-01..U-21; V-01..V-07.
- B-006 design: docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md
  - S5 / S6 mapping + granularity, S10 failure rows
  F-01..F-12, S11 flows, S12 deferred lists, S13 exit
  items 1-10, S14 INC-B006-1..10.
- Roadmap: docs/MimirAlloc_Research_and_Development_
  Roadmap.md - Phase 3 text + exit (S8), S6, S9.1
  invariants, S9.3 controlled failure tests, S10 test
  matrix + closing fuzz paragraph, S5 targets.
- Targets: docs/SUPPORTED_TARGETS.md - Pi system page
  size 16384 observed on this Pi; x86-64 + glibc Planned,
  no MimirAlloc evidence; runtime discovery required.
- Language: D-006 (portable C11 core, C ABI) in
  docs/IMPLEMENTATION_LANGUAGE_DECISION.md +
  docs/DECISION_LEDGER.md.
- Team working papers (hidden_files/): scout-B009.md
  (source digest + NEW-1..NEW-7), design-B009.md
  (frozen outline), b009-partA.md / b009-partB.md /
  b009-partC.md (builder sections integrated verbatim
  into S1-S11 of this document).

## Appendix D. Change log

No version number is assigned to this document (pattern
of docs/contracts.md Appendix B).

| Date | Slice / issue | Description |
|---|---|---|
| 2026-11-06 | B-009 - initial Proposed issue | Initial issue of the Phase 3 portable unit/contract test-suite plan. Design only. No implementation exists; no test has been run; Phase 3 exit UNMET. |
