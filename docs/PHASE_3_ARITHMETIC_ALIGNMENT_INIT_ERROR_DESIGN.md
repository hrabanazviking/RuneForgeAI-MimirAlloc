# Phase 3 — Checked Arithmetic, Alignment, Clean Initialization, and Error Semantics Design

> **DESIGN ONLY — Proposed — nothing implemented, nothing measured.**
> This document is a design only. Nothing in it is implemented,
> nothing in it is measured, and nothing in it describes existing
> behaviour. Status: **Proposed**.
> Slice: **TEAM-07 B-007** (Roadmap Phase 3; Sections 6.2–6.3, 9.1).
> Base: **243f007** (B-006 banked; `docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md`).
> Base commit full form as recorded in the assignment: 243f007 is the
> banked B-006 head on which this worktree branch `yrsa/mimiralloc-07`
> was reset. No implementation, measurement, performance, ABI, or
> Phase 3 exit claim is made anywhere in this document.

---

## 0. Honesty / status, and the U-03 caveat (design)

- **Design only, Proposed.** Every rule, table, and mapping in this
  document is a proposed design choice for a future implementation.
  No code in this slice implements it, no test in this slice measures
  it, and no statement here may be quoted as an implemented or
  measured fact about MimirAlloc.
- **Contracts and layers are Proposed.** Every `MA-*` row cited is
  Proposed in `docs/contracts.md` (A-008); every layer cited is
  Proposed in `docs/ARCHITECTURE_LAYERS.md`. Citation does not adopt,
  promote, or implement either source.
- **U-03 caveat — read first, it governs every mapping in this file.**
  `mimir_malloc` / `mimir_free` have **no direct contract rows** in
  `docs/contracts.md`: they are **[UNRESOLVED: U-03]**, outside the
  Section 6 scope, Appendix A stability **Unassigned**. Therefore
  **all MA-* mappings for the namespaced pair in this document are
  by analogy only, Proposed only** — never a direct namespaced
  contract, never an adopted or implemented fact, and never an ABI
  claim. The only rows cited directly (not by analogy) are the
  `MA-6.3-*` bootstrap / diagnostics rows where a fragment says so
  expressly; those rows govern the bootstrap interval and diagnostics,
  not a namespaced ABI.
- **No ABI / signature promise.** No signature, calling convention,
  symbol version, symbol interposition, or stability promise is
  stated or implied for `mimir_malloc`, `mimir_free`, or any other
  name in this document.
- **Inconsistencies are recorded, not silently resolved.** Appendix C
  records the inconsistencies found in the source basis. Recording
  one does not resolve it, and no design rule in Sections A–D depends
  on a silent resolution of a recorded inconsistency.

## 1. Scope (design)

### 1.1 What B-007 does

B-007 supplies, as design only, the detail that B-006 Section 12.1
deferred to B-007, for the B-006 minimal, namespaced, mapping-backed
allocator (one OS mapping per allocation, metadata in an in-mapping
reserve page, one global coarse lock, no reuse in Phase 3):

- **Checked arithmetic** — Section A: overflow / validity predicates
  tested before evaluation for every arithmetic step in the B-006
  allocation paths, and the uniform failure on any failed predicate
  (Roadmap Sections 6.2–6.3 posture, Section 9.1 Invariant 3).
- **Alignment** — Section B: the default alignment that holds by
  construction in the B-006 layout, the open validity gate for
  explicit-alignment requests, and the handling proposed for
  over-aligned requests (Roadmap Section 9.1 Invariants 1 and 3).
- **Clean initialization** — Section C: the disambiguation between
  (C1) user-memory contents — unspecified, never zero-promised — and
  (C2) allocator-owned state and bootstrap initialization before
  publication / before ready (Roadmap Section 6.3 allocation-free
  startup; Section 9.1 Invariants 1, 2, and 7 in part).
- **Error semantics** — Section D: for every failure mode in the
  B-006 path (its F-01..F-12 table, extended), what the caller
  observes, what no caller may rely on observing, the undefined-
  behaviour boundary for invalid frees, and bounded, allocation-free
  reporting that is never performed under the lock. This includes
  the **errno-style reporting posture** per the A-008 contracts:
  `errno` is **not promised** — that question is UNRESOLVED
  (V-02 → U-10, Open; see Sections 2, A.3, B.4, and D.1).

Every element is mapped, in the fragments and in Appendix A, to
B-006, to the architecture layers, and to the Section 9.1 invariant
it discharges (Appendix D gives the invariant-by-invariant account).

### 1.2 What B-007 explicitly does NOT do

- **`calloc` / `realloc` semantics = B-008.** No `mimir_calloc` or
  `mimir_realloc` design label, signature, or semantics is stated or
  implied here. `MA-6.1-CALLOC-*` and `MA-6.1-REALLOC-*` rows are
  B-008 scope; Section 9.1 Invariant 5 (failed realloc preserves) is
  therefore **not discharged by B-007** (Appendix D).
- **Test-suite / Phase 3 exit = B-009.** No test suite, exit evidence,
  or pass claim is designed or made here. Phase 3 exit — the portable
  unit/contract suite passing for explicitly supported calls on
  x86-64 **and** ARM64 Linux, with unsupported behaviour documented —
  remains unmet and is a B-009 / later-implementation matter.
- **Zero-size remains unresolved — U-09.** No behaviour for a
  zero-size request (or a zero factor, or a zero-size aligned request)
  is stated, promised, or imported from any other rule in this file.
- **No ABI / signature promise** (Section 0). No interposition, no
  standard-name `malloc` / `free` replacement.
- **No implementation, no measurement, no performance claim.** No
  speed, overhead, fragmentation, or scalability figure is stated.
- **Nothing else deferred by B-006 Section 12.3 is taken up:** no
  size classes, splitting, coalescing, reuse, or trim (Phase 7); no
  caches or sharding (Phase 8); no budgets (Phase 9); no stats / L7
  design (Phase 10, compiled out in Phase 3); no rseq (Phase 11);
  no Android / bionic / musl claim; no fork claim (Section 2).

## 2. Source basis and unresolved register carried open (design)

### 2.1 Source basis

- **B-006:** `docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md` — Sections
  1–6 (scope, mappings, granularity/alignment), S7–S11 (metadata,
  bookkeeping, lock, failure F-01..F-12, flows S11.1/S11.2), Section
  12 (deferral of this slice), Section 13 (exit evidence owed),
  Section 14 and Appendices A–C.
- **Contracts:** `docs/contracts.md` (A-008) — `MA-6.1-*`,
  `MA-6.2-*`, `MA-6.3-*` rows, all Proposed; variation rows
  V-01..V-07; unresolved register U-01..U-21.
- **Layers:** `docs/ARCHITECTURE_LAYERS.md` — L1–L7, dependency law,
  and the Section 13 invariant-to-layer map; all Proposed.
- **Roadmap:** `docs/MimirAlloc_Research_and_Development_Roadmap.md`
  — Phase 3, Sections 6.2 (ownership rules), 6.3 (allocation-free
  startup and diagnostics), 9.1 (invariants 1–10), 9.3 (fault
  injection, cited for B-009 context only).
- **Builder fragments assembled here:**
  `hidden_files/b007_builder1_AB.md` (Sections A+B, 486 lines) and
  `hidden_files/b007_builder2_CD.md` (Sections C+D, 466 lines),
  reproduced in full in Sections A–D below.
- **Assignment:** `hidden_files/ASSIGNMENT.md` (TEAM-07 B-007) and
  `WAVE_PLAN.md` in the forge directory (B-007 line; no B-007
  filename is prescribed there or in B-006 Section 12 — see the
  filename note in Appendix C, INC-B007-5).

### 2.2 Unresolved register carried open — none is resolved by B-007

| ID | Subject | Status in B-007 |
|---|---|---|
| **U-03** | Namespaced `mimir_malloc` / `mimir_free` (and stats/trim) have no contract rows; not in Section 6 scope; stability Unassigned | **Open.** All namespaced MA-* mappings by analogy only, Proposed (Section 0). |
| **U-09** | Zero-size behaviour: `malloc(0)`, calloc zero factor, zero-size aligned requests (V-01) | **Open.** No behaviour stated anywhere in B-007. |
| **U-10 / V-02** | `errno` guarantee on allocation failure | **Open.** `errno` is not promised; the bounded caller observation is the failure return itself (Sections A.3, D.0–D.1). |
| **U-11 / V-03** | Alignment validity and any size-multiple rule for explicit alignment | **Open.** The validity gate in Section B.2 has no rule yet; no behaviour is invented for an invalid alignment or a failed size-multiple rule. |
| **U-14 / V-06** | Exact `posix_memalign`-model error numbers; return-value error model for that interface only | **Open.** That model is **not imported** into the namespaced design (Sections B.4, D.1). |
| **U-15 / V-07** | Numeric default alignment guarantee and its relation to target page size | **Open.** Only the non-numeric, by-construction claim of Section B.1 is made; no numeric alignment value is claimed. |
| **U-21** | Fork occurring during bootstrap, before initialization completes | **Open — no claim** (Section C.2 / C2.7). |
| **U-17..U-20 (fork)** | U-17 lock state in a fork child; U-18 first allocation in the child / bootstrap re-entry; U-19 atfork-handler interaction and ordering; U-20 statistics/diagnostics state across fork (all Section 4.5 / MA-6.3-040..042 territory) | **Open — no claim.** B-007 makes no fork-safety, atfork, child-state, or fork-during-bootstrap claim of any kind. MA-6.3-040..042 leave fork behaviour not Defined until moved off the unresolved list by per-platform evidence. |

Any later per-target decision closing one of these items must be
recorded against that item; nothing in Sections A–D closes one
silently, by implication, or by choosing a convenient reading.

---

---

## Sections A-D - builder fragments reproduced in full (design)

The four sections below are the builder fragments for this slice,
reproduced verbatim-in-substance and in full, including all of
their tables, banners, and caveats: Sections A (Checked Arithmetic)
and B (Alignment) from `hidden_files/b007_builder1_AB.md`, then
Sections C (Clean Initialization) and D (Error Semantics) from
`hidden_files/b007_builder2_CD.md`. Their internal headings, rule
numbers, and mapping tables are retained exactly as drafted.

---

# B-007 — Builder-1 Draft: Sections A and B Only

> **HONESTY BANNER — DESIGN ONLY.** This draft is design only.
> Nothing in it is implemented, nothing in it is measured, and
> nothing in it describes existing behaviour. Status: **Proposed**.
> Slice: **TEAM-07 B-007** (Roadmap Phase 3; Sections 6.2–6.3, 9.1),
> builder-1 portion only: Section A (checked arithmetic) and
> Section B (alignment). Clean-initialisation and full
> error-semantics sections are NOT in this draft.
> Base design: B-006, `docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md`
> (Sections 6, S7, S10, S11, Section 12).
> Contracts: `docs/contracts.md` (A-008), all rows **Proposed**.
> Layers: `docs/ARCHITECTURE_LAYERS.md`, all layers **Proposed**.
> Invariants: Roadmap Section 9.1.

> **U-03 — NO DIRECT CONTRACT ROWS (read first).**
> `mimir_malloc` / `mimir_free` have **NO direct contract rows**
> in contracts.md: they are **[UNRESOLVED: U-03]**, outside the
> Section 6 scope, Appendix A stability Unassigned. Every MA-*
> mapping in Sections A and B below is therefore **by analogy
> only, Proposed only** — never a direct namespaced contract,
> never an adopted or implemented fact, and never an ABI claim:
> no signature, calling convention, symbol version, or stability
> promise is stated or implied for either name.

> **Deliberately NOT done in this draft (design):** `calloc` /
> `realloc` semantics (B-008); the test-suite / Phase 3 exit
> design (B-009); clean-initialisation detail and the full
> error-semantics catalogue (the other B-007 sections, not
> builder-1's A/B); any implementation, measurement, or
> performance claim.

---

## A. Checked Arithmetic (design)

### A.1 Governing rule: check before evaluate (design)

Owner: **L2 — Admission and checked arithmetic**, which owns
Roadmap S9.1 **Invariant 3** (primary for this section:
"Size and alignment calculations cannot overflow";
ARCHITECTURE_LAYERS.md Section 5(h), Section 13).

Rule A-0 (design). For every arithmetic step in the B-006
allocation path, the overflow / validity predicate is tested
**before** the step is evaluated, in a form that itself cannot
overflow. No step is ever evaluated first and inspected for
wrap afterwards, and no step relies on unsigned wrap, signed
overflow, or a wrapped result being "detected later".

- Addition `a + b`: evaluate only if `b <= SIZE_MAX - a`.
- Subtraction `a - b`: evaluate only if `b <= a`.
- Multiplication `a * b`: evaluate only if `b == 0` is handled
  separately or `a <= SIZE_MAX / b` (with `b != 0`).
- Round-up (A.2, steps A5–A7): the addition inside the
  round-up is checked under the addition rule first; the
  division / multiplication form is preferred where it avoids
  an addition altogether, but choosing a form does not remove
  the check on the form actually used.
- `SIZE_MAX` above names the representability bound of the
  size type used by the (future, D-006 C11) implementation.
  It is a design bound, not a measured target fact.

Failure of any predicate is the **uniform failure** of A.3:
rejected at **L2**, before any syscall, with no wrapped
allocation. A representable result is not thereby satisfiable:
a total that fits the size type may still exceed what can be
mapped or satisfied, and then fails under the same uniform
failure (by analogy **MA-6.1-MALLOC-04**: a request that
exceeds any size that can be represented **or satisfied** is
treated as failure under **MA-6.1-MALLOC-02**).

Ordering across layers (design): admission arithmetic is done
at **L2** and the checked results pass downward. **L5** (page /
span manager — metadata and layout, Invariant 2) performs the
layout computation of B-006 S11.1 step 5 using the same checked
predicates and the checked values admitted at L2; it must not
recompute any step unchecked, and a layout check that fails at
L5 still fails before **L6** is asked for a mapping. **L6** (OS
abstraction) receives only the final checked mapping length and
performs the mapping request; L6 does no size policy and no
layer skips to L6. In short: **all** arithmetic that can fail
completes, checked, before the single `mmap` of B-006 S5/S11.

### A.2 Step table — every arithmetic step in the B-006 path (design)

The path covered is B-006 S6 + S7.1 + S7.3 + S11.1:
admission of `requested_size`, reserve / offset additions,
the usable-region sizing, `page_round_up`, the final mapping
length, the descriptor's `usable_size`, and the user-pointer
addition. Each row: check **before** evaluate.

| Step | Computation (design form) | Check before evaluate | On check failure |
|---|---|---|---|
| A1 | `requested_size` validity (admission, L2) | `requested_size` is in the explicitly supported set: `> 0` and representable. Zero-size is **not** admitted as supported: **MA-6.1-MALLOC-03** is [UNRESOLVED: U-09] and B-006 S10 F-05 gives zero-size no behaviour. No arithmetic is performed on an unadmitted size. | Uniform failure, A.3 (zero-size itself remains unresolved, U-09 — see A.4) |
| A2 | System page size validity (input to all steps below; discovered at runtime via L6, B-006 S3/S7.1) | Discovery succeeded and the value is usable: non-zero, and `page_size - 1` is representable (i.e. the value is not so large that step A5 cannot even be stated). Never a hardcoded 4096 or 16384 (MA-6.2-041; B-006 S10 F-11). Failure of discovery is a defined failure with no mapping attempted. | Uniform failure, A.3; no guessed or fallback page size |
| A3 | `metadata_reserve + requested_size` (B-006 S6 total-mapping input, L2/L5) | `requested_size <= SIZE_MAX - metadata_reserve`, tested before the addition. `metadata_reserve` is the S6 design-level reserve term — its exact relation to `usable_offset` is **not** settled (see A.5, recorded inconsistency). | Uniform failure, A.3 |
| A4 | `usable_offset + usable_region` (B-006 S7.1 / S11.1 step 5 layout, L5) | `usable_region <= SIZE_MAX - usable_offset`, tested before the addition, where `usable_offset` is one whole system page (S7.1) and `usable_region` is the region sized in A5. Also checked before use: `usable_offset` equals exactly one system page as discovered in A2 — not a literal. | Uniform failure, A.3 |
| A5 | `page_round_up(x)`, addition form: `x + (page_size - 1)` (applies with `x` = the A3 sum **and**, under the other reading in A.5, with `x` = `requested_size` for the usable region alone) | `x <= SIZE_MAX - (page_size - 1)`, tested before the addition, for **each** `x` the reconciled formula (A.5) turns out to use — both forms are checked in this draft so reconciliation cannot introduce an unchecked step. `page_size - 1` itself is safe only because A2 admitted `page_size`. | Uniform failure, A.3 |
| A6 | `page_round_up(x)`, count form: `page_count = ceil(x / page_size)`; `rounded = page_count * page_size` | Division first (no addition needed for the count). Multiplication checked before evaluate: `page_count <= SIZE_MAX / page_size` (with `page_size != 0`, from A2). A design that derives `page_count` from the A5 addition instead must satisfy A5 **and** this row for its final multiply. | Uniform failure, A.3 |
| A7 | Alignment round-up: `round_up_to(v, al)`, addition form `v + (al - 1)` and mask / multiply forms (generic step; see Section B for when it may be used at all) | Same pattern as A5/A6: `(al - 1)` is statable, `v <= SIZE_MAX - (al - 1)` before any addition, and any `count * al` form is checked as in A6 — all before evaluate, and **only** for an `al` that has passed the alignment validity gate of Section B.2, which is currently **open**. For the ordinary B-006 path no numeric alignment round-up is performed at all: alignment is by construction (Section B.1), and the operative rounding is the system-page rounding of A5/A6. | Uniform failure, A.3; an unvalidated alignment never reaches this step (B.2) |
| A8 | `usable_size = mapping_length - usable_offset` (descriptor field, B-006 S7.3, L5) | `usable_offset <= mapping_length`, tested before the subtraction. Publication gate (not arithmetic): the resulting `usable_size` must be `>= requested_size` (S7.3); a descriptor that fails this is never published. | Uniform failure, A.3; nothing published |
| A9 | `user_ptr = mapping_base + usable_offset` (B-006 S11.1 step 10, L5/L1 return path) | Pointer-range check before evaluate: `usable_offset <= (address-space bound) - mapping_base` in design terms — the addition must not wrap the address space. `mapping_base` is the successful mapping's base (page-aligned, B.1) and `mapping_length` from A4–A6 already bounds the extent, so this row is a stated check, not an assumption a later implementation may skip. | Uniform failure, A.3; a mapping already acquired on this path is released / abandoned internally and nothing is published (A.3) |
| A10 | Bookkeeping additions on publication: `live_bytes += mapping_length` (and `live_mappings += 1`) (B-006 S8.1, L4) | `mapping_length <= SIZE_MAX - live_bytes`, tested before the addition, under the lock, in the same locked section as registry insertion (S8.1). Counters are bookkeeping only and drive no allocation decision (Invariant 8). | Uniform failure, A.3; descriptor not inserted, counters unchanged, mapping not published |

Notes on the table (design):

- Steps A3 and A4/A5 are **both** listed, deliberately,
  because of the recorded formula inconsistency in A.5.
  Until that inconsistency is reconciled, this draft checks
  every step of **both** textual forms rather than quietly
  picking one form and checking only its steps.
- No multiplication occurs in the ordinary `mimir_malloc`
  path of B-006 apart from A6 (and a future A7 form). The
  checked-multiplication rule of A.1 / A6 is stated here so
  that the `calloc` product rule (by analogy
  **MA-6.1-CALLOC-02**, checked before multiplication) is not
  imported by accident: `calloc` semantics are B-008, not
  designed in this draft.
- Address arithmetic (A9) and size arithmetic (A1–A8, A10)
  are distinct checks. Passing one does not discharge the
  other.

### A.3 Uniform failure (design)

Every check failure in A.2 has exactly one outcome:

1. Rejected at **L2** (or, for the L5 layout rows A4–A6/A8,
   rejected at L5 before L6 is invoked — in either case
   **before any syscall**: no `mmap` is attempted for the
   request, and no other OS call is used to "try anyway").
2. The defined failure channel is returned through L1:
   **null**, no ownership passes, no live allocation is
   disturbed — by analogy **MA-6.1-MALLOC-02**.
3. Oversize / unrepresentable is a failure case, not a
   smaller allocation — by analogy **MA-6.1-MALLOC-04**
   (treated as failure under -02). **No wrapped allocation
   ever occurs**: a request whose true total wraps the size
   type is never served as the wrapped, smaller total.
4. No partially constructed block is returned or published,
   and any internal partial work is rolled back or abandoned
   internally — by analogy **MA-6.2-020** (unsatisfiable
   request: defined failure channel, no partially constructed
   block) and **MA-6.2-024** (multi-step failure: roll back /
   abandon, no half-owned block published). Concretely:
   no descriptor is published, the registry is unchanged,
   and `live_mappings` / `live_bytes` are unchanged.
5. `errno` is **not promised**: whether it is set, and to
   what, is target-dependent and UNRESOLVED (**V-02 → U-10**,
   Open; B-006 S10). No caller may rely on it from this design.

This is the design-level detail behind B-006 S10 row F-02
(request unrepresentable: rejected at L2 before any syscall),
which in B-006 stated only that the rejection exists.

### A.4 Boundary cases (design)

Each case below is decided by the first step of A.2 whose
check it fails; none reaches a syscall on the failure path.

- `requested_size == SIZE_MAX`: fails A3 (any non-zero
  reserve makes the sum unrepresentable) — and would fail
  A5 regardless. Uniform failure.
- `requested_size == SIZE_MAX - metadata_reserve`: the A3
  sum is exactly `SIZE_MAX` — representable, so A3 passes —
  but A5 then fails: `SIZE_MAX + (page_size - 1)` cannot be
  checked-and-evaluated for any `page_size > 1`, and the A6
  count form likewise yields a product that cannot be
  represented once a reserve page is included. Representable
  is not satisfiable (MA-6.1-MALLOC-04, by analogy).
- `requested_size == SIZE_MAX - metadata_reserve + 1` and
  every larger value: fails A3 directly. Uniform failure.
- Exact-page boundary: a total that is already an exact
  multiple of the system page size rounds to itself in
  A5/A6 (`page_size - 1` is added only inside the checked
  form; the count form gives the same total) — no extra page
  is added by rounding, and no check is skipped because the
  value "looks aligned".
- One byte over an exact-page boundary: rounds up by a
  whole system page in A5/A6; the extra page is part of the
  checked total, not an afterthought, so a request that only
  overflows **because** of that extra page fails A5/A6.
- `page_size == 1` (degenerate, design completeness only):
  `page_size - 1 == 0`; A5/A6 reduce to the identity and the
  A2/A3 checks still govern. No target is claimed to have
  this value; the case is stated so the formulas have no
  hidden `page_size > 1` assumption.
- Page-size discovery failure or an unusable discovered
  value: A2 fails before any other step is stated (compare
  B-006 S10 F-11). No hardcoded fallback value is used.
- Alignment boundary (conditional, Section B): an alignment
  round-up in A7 whose addition or multiplication crosses
  `SIZE_MAX` fails A7 exactly as a size round-up fails
  A5/A6 — but only a request that first passes the open
  validity gate of B.2 can reach A7 at all.
- `requested_size == 0`: **not** a boundary success case.
  It is excluded from the supported set and its behaviour
  is UNRESOLVED (**MA-6.1-MALLOC-03 / U-09**; B-006 F-05).
  This draft does not turn it into a checked-arithmetic
  failure, a unique pointer, or a null promise.

### A.5 Recorded inconsistency — S6 vs S7/S11 total-mapping formula (design, UNRESOLVED)

> **INC-B007-A1 — TOTAL-MAPPING FORMULA DISCREPANCY.**
> Recorded in-doc, **not silently resolved** by this draft.
>
> - **B-006 Section 6** states the total mapping size as
>   `total_mapped = page_round_up(metadata_reserve + requested_size)`
>   — a single round-up applied to one sum, using a term
>   `metadata_reserve` whose value Section 6 does not state.
> - **B-006 S7.1** states the layout as one reserve system
>   page plus a usable region, with `usable_offset` equal to
>   exactly one system page and the user pointer at
>   `mapping_base + usable_offset`. Section 6 nowhere states
>   that `metadata_reserve == usable_offset`, or that the
>   reserve is exactly one system page.
> - **B-006 S11.1 step 5** states:
>   `mapping_length = usable_offset + usable region rounded up
>   to whole system pages covering the admitted size` — wording
>   that parses **two ways**: (i) `page_round_up(usable_offset
>   + requested_size)`, the S6 shape; or (ii)
>   `usable_offset + page_round_up(requested_size)`, a round-up
>   of the usable region alone, added to the offset afterwards.
>
> The two shapes give the same total **only if** extra facts
> are added that none of the three texts states in place —
> in particular that the reserve / offset term is a whole
> number of system pages (so rounding distributes over it).
> This draft **does not add those facts and does not choose**
> between (i) and (ii). Consequences held open until a future
> revision of the B-006 design document reconciles them
> in-doc (naming one canonical formula, defining
> `metadata_reserve` against `usable_offset`, and restating
> S6, S7.1, and S11.1 step 5 in that one form):
>
> 1. Section A.2 checks the steps of **both** forms (A3 + A5
>    on the sum; A4 + A5/A6 on the offset-plus-region form),
>    so reconciliation cannot introduce an unchecked step.
> 2. No test, proof, or later slice may cite this draft as
>    settling `mapping_length`. Any arithmetic proof owed
>    for B-007 must be stated against the reconciled formula
>    once it exists, not against either unreconciled text.
> 3. The descriptor field `usable_size = mapping_length -
>    usable_offset` (S7.3, step A8) inherits the same
>    openness: its value depends on which formula is
>    canonical, even though the A8 check itself does not.

### A.6 Mapping for Section A (design, by analogy only — U-03)

| Element | Contracts (all Proposed, by analogy) | Invariant (S9.1) | Layers |
|---|---|---|---|
| Check-before-evaluate, all steps | MA-6.1-MALLOC-04; MA-6.2-020, MA-6.2-024 | **3 (primary)** | L2 (owner); L5 (layout rows); L6 (receives checked length only) |
| Uniform failure before syscall | MA-6.1-MALLOC-02; MA-6.1-MALLOC-04 | 3 (primary); 10 (fallback preserves or fails) contributes | L2 → L1 (return); L5, L6 (no call made) |
| No wrapped allocation | MA-6.1-MALLOC-04; MA-6.2-020 | 3 (primary); 1 (no overlapping / false block) contributes | L2, L5 |
| No partial / half-owned block | MA-6.2-020; MA-6.2-024 | 3 (primary for the arithmetic trigger); 1, 10 contribute | L2, L5 (L4 bookkeeping unchanged) |
| Page terms qualified, no hardcoded size | MA-6.2-041 (with MA-6.2-040 terms) | 3 (checks use the discovered value) | L2, L5, L6 (discovery) |

---

## B. Alignment (design)

### B.1 Default alignment: by construction (design)

For an ordinary `mimir_malloc` allocation in the B-006
baseline (no explicit alignment parameter exists on that
entry point):

```text
user_ptr = mapping_base + usable_offset
usable_offset = one whole system page   (B-006 S7.1)
```

- The mapping base is system-page aligned by the OS mapping
  design of B-006 S5 (L6 via L5), and `usable_offset` is
  exactly one system page — itself a whole multiple of the
  system page size. The user pointer is therefore
  **system-page aligned by construction**: no post-hoc
  adjustment, search for an aligned sub-address, or numeric
  alignment round-up is performed or needed on this path.
  This is a structural property of the layout, stated at
  design level only.
- Contract mapping, by analogy only (U-03): this construction
  is how the design addresses **MA-6.1-MALLOC-01** (success:
  non-null, suitably aligned for any object type with a
  fundamental alignment requirement, distinct storage).
  Layers: **L5** owns the layout, **L6** supplies the
  runtime-discovered system page size and the mapping,
  **L2** admits the request (Section A), **L1** returns the
  pointer. Invariants: S9.1 Invariant 1 (distinct,
  non-overlapping storage) and Invariant 3 (no unchecked
  arithmetic — the A9 addition) apply; alignment by
  construction is a layout property, not itself an invariant.
- Conditional mathematical note (design, not a validity
  rule): a system-page-aligned pointer is also aligned to
  any alignment value that **divides** the system page size.
  This draft does not use that note to admit any explicit
  alignment request: which alignment values are valid is
  the open gate of B.2, not a consequence of this note.

**No numeric alignment value is claimed (design).**

- **V-07 / U-15 are OPEN.** The numeric default alignment
  guarantee, and its relation to the target system page
  size, are UNRESOLVED in contracts.md. **MA-6.1-MALLOC-01**
  states only the non-numeric "suitably aligned" claim
  quoted above.
- Accordingly this section claims **no number**: not 16,
  not the system page size as a contract number, and not
  any target's observed value. The system page size
  observed on the Raspberry Pi host of B-006 (16384) is a
  fact about that host only (B-006 S3), is never hardcoded,
  and proves nothing about any other target (a 4096 host is
  recorded in B-006 S3 as the contrasting example).
- "Page-aligned by construction" (structural, above) and
  "a numeric alignment guarantee" (contract, open) are
  different statements. A later per-target contract decision
  closing U-15 is required before the first may be restated
  as the second.

### B.2 Explicit alignment: the validity gate is OPEN (design)

Requests carrying an explicit alignment (the
`aligned_alloc`- / `posix_memalign`-shaped requests of
contracts.md Sections 2.6–2.7) are **not designed** in
B-006 and are **not admitted** by anything in this draft.
`mimir_malloc` has no alignment parameter; this section
designs no new entry point, signature, or parameter.

- **V-03 / U-11 are OPEN.** Alignment validity — which
  values are valid for a target, any rule relating alignment
  to the size of a pointer, any maximum, and any
  size-multiple rule (whether size must be a multiple of
  alignment, and the consequence if it is not) — is
  target-specific and UNRESOLVED. By analogy:
  **MA-6.1-ALIGNED-03** and **MA-6.1-ALIGNED-04** state no
  behaviour ([UNRESOLVED: U-11]); **MA-6.1-POSIXMA-03**
  likewise states no behaviour ([UNRESOLVED: U-11]).
- Therefore this draft specifies **no behaviour** for, in
  particular: alignment `0`; a non-power-of-two alignment;
  an alignment smaller than the size of a pointer; a size
  that is not a multiple of the alignment; or an alignment
  larger than the system page size **as a validity matter**
  (its layout consequence is treated separately in B.3).
  None of these is silently defined as success, as null /
  failure, as an error number, or as UB by this draft.
- What **is** stated for a hypothetical future explicit
  path, by analogy only, is only the success / unsatisfiable
  shape **once validity has been settled per target**:
  - **MA-6.1-ALIGNED-01** (by analogy): success requires a
    valid alignment under V-03 and a satisfiable size; the
    returned pointer is aligned to the requested alignment
    and the storage is releasable by the compatible free.
  - **MA-6.1-ALIGNED-02** (by analogy): a valid request that
    cannot be satisfied returns no unallocated pointer;
    failure is null (for an `aligned_alloc`-shaped channel).
  - **MA-6.1-ALIGNED-05**: zero-size with otherwise valid
    alignment — [UNRESOLVED: U-09], no behaviour stated.
  - **MA-6.1-POSIXMA-01** (by analogy): success is a
    **different channel shape** — return value zero, with
    the aligned pointer stored through an output parameter.
  - **MA-6.1-POSIXMA-02** (by analogy): failure is a
    non-zero error number carried by the **return value**
    (per V-06; `errno` is not the proposed channel for that
    symbol), and the caller's output-parameter storage is
    left **unchanged**.
  - **MA-6.1-POSIXMA-04**: zero-size with otherwise valid
    alignment — [UNRESOLVED: U-09], no behaviour stated.
  - **MA-6.1-POSIXMA-05**: a null output parameter is **UB**;
    no behaviour, return value, or channel is stated.
- The two channel shapes (null-return vs. return-value
  error number with an output parameter) must not be
  conflated, and **neither is imported into
  `mimir_malloc`**: the namespaced pair's failure channel in
  this design remains the null return of Section A.3, with
  `errno` not promised (V-02 / U-10). POSIXMA rows are
  analogy for the explicit-alignment design space only.
- Layer / invariant note: any future validity gate sits at
  **L2** (admission validates alignment, ARCHITECTURE_LAYERS
  Section 5), its checked arithmetic is step A7 under
  Invariant 3, and its layout consequences fall on **L5/L6**
  (B.3). Until U-11 closes per target, L2 has no validity
  rule to apply — the gate is open, not defaulted.

### B.3 Over-aligned requests: alignment greater than the system page size (design)

This subsection states a **re-specification requirement**,
not a design. No over-aligned behaviour is designed here.

Why the B-006 baseline cannot serve it as-is (design):

1. **Construction no longer suffices.** A user pointer that
   is system-page aligned (B.1) need not be aligned to an
   alignment `al > page_size`: page multiples are not in
   general `al` multiples. Serving such a request requires
   choosing a user offset larger than one system page —
   the first offset at or after the reserve page at which
   `mapping_base + offset` is `al`-aligned — inside a
   mapping made **oversized** by the alignment slack.
2. **New checked arithmetic (under Section A).** The
   oversized total adds steps not in A.2's ordinary path:
   rounding `usable_offset` up to `al` (step A7 form),
   adding the resulting slack to the A3–A6 total, and
   re-checking the A8 publication gate
   (`usable_size >= requested_size`) against the enlarged
   layout. Each step is check-before-evaluate under A.1 /
   Invariant 3, at L2/L5, before any syscall; overflow of
   the enlarged total is the uniform failure of A.3
   (by analogy MA-6.1-MALLOC-04 / -02, MA-6.1-ALIGNED-02).
3. **Free-path descriptor derivation must be re-specified.**
   The B-006 free path (S8.2 step 2, S11.2 step 4) derives
   the descriptor address as `p - usable_offset` with
   `usable_offset` fixed at exactly one system page, then
   validates magic / state / `mapping_base + usable_offset
   == p`. With a variable, alignment-dependent offset that
   derivation is **circular as specified**: the offset is
   stored in the descriptor (S7.3 field `usable_offset`),
   but the descriptor's location is what the derivation is
   trying to find. A re-specification must state, at
   design level, at least: where the descriptor lives
   relative to the aligned user pointer; how the free path
   locates it from `p` alone without trusting unvalidated
   fields (for example — **illustrative only, not chosen** —
   deriving a fixed reserve location by rounding `p` down
   to a stated boundary, with the true offset and mapping
   base/length then read from a descriptor validated there);
   how `mapping_base` and `mapping_length` for the single
   `munmap` are recovered; and how the S8.2 validation and
   Invariant 2 (metadata lifetime) are preserved.
   The S7.3 field `usable_offset` existing in the layout
   does **not** by itself resolve this: a field that can
   hold a variable value is not a derivation rule.
4. **Nothing else in B-006 silently carries over.** Registry
   insertion, counters (step A10 against the enlarged
   `mapping_length`), cross-thread free (Defined in B-006
   S8.4 for the baseline layout), and the S11.2 flow all
   cite the one-page offset; each must be restated against
   the re-specified derivation before any over-aligned
   claim is made.

Until that re-specification exists **and** the B.2 validity
gate is closed per target (V-03 / U-11), an over-aligned
request has **no designed behaviour** in this baseline:
it is not served by enlarging the offset ad hoc, it is not
silently served as an ordinary page-aligned allocation
(that would return a pointer failing the requested
alignment, by analogy contrary to MA-6.1-ALIGNED-01), and
its failure channel is not invented here (B.4).

### B.4 Alignment failure modes (design)

| ID | Failure / condition | Design behaviour |
|---|---|---|
| B-F1 | Ordinary allocation, checked arithmetic for the layout fails (Section A steps A2–A6/A8/A9) | Uniform failure of A.3: null through L1, before any syscall, no partial block. By analogy MA-6.1-MALLOC-02, MA-6.1-MALLOC-04; MA-6.2-020, MA-6.2-024. Invariant 3. Layers L2, L5, L6. |
| B-F2 | Explicit alignment value is invalid for the target | **No behaviour stated — gate open.** V-03 / U-11 unresolved; by analogy MA-6.1-ALIGNED-03, MA-6.1-POSIXMA-03 state no behaviour. This draft invents neither a failure channel, an error value, nor a UB classification for the namespaced design. A future per-target decision must state it. |
| B-F3 | Size fails a size-multiple-of-alignment rule | **No behaviour stated — gate open.** By analogy MA-6.1-ALIGNED-04 ([UNRESOLVED: U-11]): the rule and its consequence are recorded in V-03, not decided here. |
| B-F4 | Explicit alignment valid (once B.2 closes) but the request cannot be satisfied — including overflow of the B.3 oversized total | Failure with no unallocated / partially constructed pointer returned. Channel **by interface shape, by analogy only**: null for an `aligned_alloc`-shaped channel (MA-6.1-ALIGNED-02, under MA-6.1-MALLOC-02/-04 posture of A.3); return-value error number with output storage unchanged, `errno` not the channel, for a `posix_memalign`-shaped channel (MA-6.1-POSIXMA-02, V-06). Neither channel is a `mimir_malloc` channel (U-03). Invariant 3 (overflow trigger); MA-6.2-020 / -024 (no partial block). Layers L2, L5. |
| B-F5 | Alignment greater than the system page size | **Not designed.** Requires the B.3 re-specification (oversized mapping + free-path descriptor derivation) **and** closure of B.2. No success, failure, or fallback behaviour is claimed; in particular the request is never silently downgraded to ordinary page alignment. |
| B-F6 | Zero-size request with an otherwise valid alignment | **No behaviour stated.** By analogy MA-6.1-ALIGNED-05 and MA-6.1-POSIXMA-04 are [UNRESOLVED: U-09]. |
| B-F7 | `posix_memalign`-shaped call with a null output parameter | **UB** by analogy to MA-6.1-POSIXMA-05: no behaviour, return value, or error channel stated. Recorded so the case is named, not so it is handled. |
| B-F8 | Page-size discovery fails, so even default (B.1) alignment by construction cannot be stated | Defined failure, no mapping attempted, no fallback constant (B-006 S10 F-11; step A2). A pointer whose alignment rests on an undiscovered page size is never returned. Layers L6 (discovery), L5, L2. |

Out of table, stated once (design): no row above promises
a numeric alignment (V-07 / U-15, B.1), an `errno` value
(V-02 / U-10; the POSIXMA return-value model of B-F4 is a
different interface's channel, not an `errno` promise), or
any behaviour for an entry point that B-006 / this draft
does not design (U-03).

### B.5 Mapping for Section B (design, by analogy only — U-03)

| Element | Contracts (all Proposed, by analogy) | Invariant (S9.1) | Layers |
|---|---|---|---|
| Default alignment by construction | MA-6.1-MALLOC-01 (non-numeric claim only; V-07 / U-15 open) | 1 (distinct storage); 3 (A9 addition checked) | L5 (layout), L6 (page size / mapping), L2, L1 |
| Explicit alignment — success / failure shapes | MA-6.1-ALIGNED-01, MA-6.1-ALIGNED-02; MA-6.1-POSIXMA-01, MA-6.1-POSIXMA-02 (distinct channel) | 3 (A7 arithmetic); 10 (fallback preserves or fails) contributes | L2 (future gate), L5, L6 |
| Explicit alignment — validity / size-multiple | MA-6.1-ALIGNED-03, MA-6.1-ALIGNED-04; MA-6.1-POSIXMA-03 — all [UNRESOLVED: U-11], via V-03 | — (no invariant discharged; gate open) | L2 (gate site, no rule yet) |
| Explicit alignment — zero-size / null out-param | MA-6.1-ALIGNED-05, MA-6.1-POSIXMA-04 ([UNRESOLVED: U-09]); MA-6.1-POSIXMA-05 (UB) | — | L1 / L2 (named, not handled) |
| Over-aligned (> system page size) | MA-6.1-ALIGNED-01 (the alignment that would have to be met); MA-6.1-ALIGNED-02 (failure shape) — re-spec required, B.3 | 2 (descriptor lifetime / derivation); 3 (oversize arithmetic) | L5 (layout + descriptor), L4 (free-path owner determination), L6 |

---

*End of builder-1 draft, Sections A and B only. Design only;
Proposed; all contract mappings by analogy only under U-03.
Recorded and unresolved in this draft: INC-B007-A1 (A.5),
U-03, U-09, U-10 (via V-02), U-11 (via V-03), U-15 (via V-07).
No implementation, measurement, or Phase 3 exit claim is
made anywhere in this draft.*

---

# B-007 Builder-2 Draft — Sections C and D Only (design)

> **HONESTY BANNER — DESIGN ONLY.** This draft is design only.
> Nothing in it is implemented, measured, adopted, or shipped.
> Status: **Proposed**. Slice: **TEAM-07 B-007**, builder-2
> sections **C (Clean Initialization)** and **D (Error
> Semantics)** only. Sections A and B of B-007 are other
> builders' work and are not in this file.
> Base design: B-006, `docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md`
> (cited below as B-006 S7–S11). Contracts: A-008,
> `docs/contracts.md`. Layers: `docs/ARCHITECTURE_LAYERS.md`.
> Roadmap: `docs/MimirAlloc_Research_and_Development_Roadmap.md`
> S6.3 and S9.1.
>
> **No direct contract rows for the namespaced pair (U-03).**
> `mimir_malloc` / `mimir_free` have NO direct contract rows in
> contracts.md; they are recorded there as [UNRESOLVED: U-03],
> outside S6 scope, stability Unassigned. Every mapping in this
> draft from those names to MA-6.1-* / MA-6.2-* rows is **by
> analogy only, Proposed only** — never a direct namespaced
> contract. MA-6.3-* rows are cited directly: they govern the
> bootstrap interval and diagnostics, not a namespaced ABI.

## C. Clean Initialization (design)

### C.0 The disambiguation this section exists to make

"Initialized" has two different subjects in this design, with
opposite guarantees. Conflating them is the defect this section
forbids — in prose, in headers, in tests, and in any later code
comment:

- **C1 — user memory contents:** the usable bytes handed to the
  caller by a successful `mimir_malloc`. **NOT initialized by
  the allocator. No zero guarantee. Contents unspecified.**
- **C2 — allocator-owned state and bootstrap:** the descriptor,
  the registry, the counters, and the lock, plus the interval
  before that state can be assumed ready. **Fully initialized
  before publication**, under the bootstrap rules of C2.

A sentence that says "MimirAlloc returns initialized memory"
without naming which subject it means is non-conforming with
this section.

### C.1 User memory contents — unspecified, never zero-promised

**C1.1 No contents guarantee of any kind.** By analogy to
MA-6.1-MALLOC-01 (success: non-null, suitably aligned, distinct
storage), a successful `mimir_malloc` promises the pointer,
its alignment, and its distinctness from every other live
allocation — and nothing about its bytes. contracts.md S2.2
states it in one line, and this draft adopts that line as the
whole of C1: *"Contents of newly allocated storage are not
specified by any row in this subsection."* There is no zero
promise, no pattern promise, no preservation promise, and no
"reads as zero on this target" promise.

**C1.2 The allocator never touches user bytes.** B-006 S9.2:
the lock never guards user bytes; the usable region is the
caller's alone from the return of `mimir_malloc` until
`mimir_free`, and in this baseline the allocator neither reads
nor writes it at all. Zeroing user bytes is not a hidden step
of the allocation path, of publication, or of free. A caller
that reads usable bytes before writing them reads unspecified
contents, at its own responsibility.

**C1.3 Incidental `mmap` zero-fill is NOT a guarantee.** In
the B-006 baseline each allocation is a fresh OS mapping, and
fresh anonymous mappings happen on common targets to read as
zero. That is an incidental property of the current backing
mechanism, not a design promise, for three stated reasons:
(1) the contract line in C1.1, which governs, promises no
contents; (2) the backing mechanism is a Phase 3 baseline
choice — any later reuse, splitting, or caching form (deferred
to Phases 7/8 in B-006 S8.5) would return previously used bytes
under exactly the same `mimir_malloc` name; (3) a guarantee
that depends on which mechanism happens to sit underneath is
not portable across targets. Therefore: no caller may rely on
fresh `mimir_malloc` storage reading as zero, and no test for
`mimir_malloc` may assert zero contents. A zero assertion is a
`calloc` assertion (C1.5) and belongs to B-008's test scope.

**C1.4 Allocator-owned memory IS fully initialized before
publication — the opposite rule, for the opposite subject.**
The allocator's own structures carry a complete-initialization
duty, discharged before any other thread can observe them:

- **Descriptor (B-006 S7.3, 96 bytes, L5-owned):** every field
  is fully written before publication — `magic`, `mapping_base`,
  `mapping_length`, `usable_offset`, `requested_size`,
  `usable_size`, `state` (LIVE), `list_next` / `list_prev`.
  **`flags`: unassigned bits are zero** (the only reserved use
  is the "bootstrap-served" flag of MA-6.3-006 / B-006 F-08).
  **`reserved` (offset 72, 24 bytes): must be zero on
  publication** — B-006 S7.3 states this as a layout rule, and
  this draft carries it as a C1 rule, not a hope. The rest of
  the reserve page is unused, is not user storage, and carries
  no contents claim in either direction.
- **Registry and counters (B-006 S8.1, L4 discipline, storage
  bootstrap-owned per S7.2):** a descriptor is inserted into
  the registry once, at publication, after its mapping exists
  and its fields are fully written; `live_mappings` and
  `live_bytes` are updated in the same locked section that
  inserts it, so no thread can observe a half-written
  descriptor or a half-updated counter pair (B-006 S9.2).
  Publication is the single transition unpublished -> LIVE,
  under the lock, before the lock is released and before
  `mimir_malloc` returns.
- **Starting state needs no initialization run:** the empty
  registry with zero counters is the defined starting state,
  reached without any allocation (B-006 S8.1, F-08), and the
  lock object lives in the same fixed static / bootstrap-owned
  anchors (B-006 S7.2, S9.1).

Together C1.4 is the design discharge, for initialization, of
**Invariant 2** (metadata describes actual allocated memory —
a published descriptor describes a real, fully recorded
mapping) and **Invariant 6** (single, exclusive registry
membership: one list, a node on it exactly once, never in an
incompatible second state) — see the mapping table in C.3.

**C1.5 `calloc` zeroing is cited ONLY as the excluded B-008
contrast.** MA-6.1-CALLOC-01 / MA-6.1-CALLOC-05 require every
byte of a successful `calloc` allocation to be zero before the
pointer is returned, verified byte by byte. That duty belongs
to `calloc`, is deferred to **B-008**, and is **not** imported
into `mimir_malloc` by analogy, by the fresh-mapping accident
of C1.3, or by any other route in this slice. It is named here
only so the boundary is visible: zeroing is a named function's
contract, not a general allocator habit.

### C.2 Bootstrap initialization — before `main`, before ready

C2 governs the interval in which C1.4's state exists as static
storage but no completed initialization may be assumed. Source
basis: Roadmap S6.3 (global allocators may execute before
`main`, from constructors, loader code, or logging; a design
that allocates with the same allocator in its own
initialization can recurse indefinitely) and contracts S4.1 /
S4.2 (MA-6.3-001..006, MA-6.3-010..012), as already applied to
this baseline in B-006 F-08.

**Layer placement.** The bootstrap **sits below L1**, as a
pre-layer concern — ARCHITECTURE_LAYERS S12 states this
honestly and invents no layer for it. Within that placement:
bootstrap owns the static storage (registry anchors, counters,
lock object — B-006 S7.2); **L4** owns registry and lock
discipline (what the lock guards, when it is taken, failure
behaviour — B-006 S9.1/S9.5); **L5** owns the descriptor
contents that initialization must not half-publish (C1.4);
**L6** provides the statically initialized lock primitive and
the page-size / mapping operations through its published
interfaces, and is the only layer with platform-specific code.

**C2.1 Pre-main / first entry — assume nothing
(MA-6.3-001, MA-6.3-005).** First entry before initialization
has run MUST be served from the minimal bootstrap path or
failed through the defined failure channel of Section D; fully
initialized state MUST NOT be assumed. Entry from a
constructor or loader context before `main` is the same case
(MA-6.3-005): availability of hosted runtime services MUST NOT
be assumed. In this baseline the consequence is narrow, and is
stated plainly as in B-006 F-08: the registry anchors and lock
are static, need no allocation and no constructor, and the
empty registry is valid before any init step runs; any future
init step must preserve that property or use the
defined-failure branch.

**C2.2 Bootstrap-owned storage; no normal-path recursion
(MA-6.3-002, MA-6.3-003, MA-6.3-012).** Storage for
initialization MUST NOT come from the allocator's normal path
(MA-6.3-002) — the rule that also rejected a separate metadata
table in B-006 S7.2. Initialization MUST NOT self-allocate
recursively and MUST terminate without requiring a prior
completed initialization (MA-6.3-003). Bookkeeping that grows
with the number of allocations or threads MUST be held only
in fixed, preallocated, or bootstrap-owned storage
(MA-6.3-012); this baseline's bootstrap bookkeeping is fixed
and constant-size by construction (S7.2 anchors), so the rule
is discharged structurally, not by a runtime check.

**C2.3 Reentrancy (MA-6.3-004).** A reentrant entry while
initialization is in progress on the same thread MUST be
served by the bootstrap path, deferred by a defined mechanism,
or failed through the defined failure channel; it MUST NOT
re-enter the in-progress initialization as if it were
complete. Ownership of any block already handed out by the
outer call is unaffected by the inner call. No path in this
design calls the normal allocation path from inside
initialization — that is the recursion C2.2 forbids, stated
from the caller's side.

**C2.4 No allocator-dependent containers; no silent ordering
reliance (MA-6.3-010, MA-6.3-011).** No C++ container or
utility whose storage comes from the allocator being
initialized may be used on the initialization path
(MA-6.3-010). The design MUST NOT rely on an allocating static
or global object's construction running after allocator
initialization unless that ordering is guaranteed by a stated
mechanism, not by observed link order (MA-6.3-011).

**C2.5 The lock is statically initialized (B-006 S9.1).**
The lock object lives in static / bootstrap-owned storage and
is statically initialized where the target permits (the
recommended primitive is a POSIX pthread mutex, normal /
default type — B-006 S9.1 records the choice and its loser).
Initialization therefore never waits for a runtime lock-init
step before the lock can guard publication, and lock storage
is never obtained through the allocator (MA-6.3-002,
MA-6.3-012). A primitive initialization failure, where the
target can even report one, is a synchronization failure under
Section D / F-07 — never a licence to proceed unlocked
(**Invariant 9**).

**C2.6 Bootstrap-served blocks are not a second allocator
(MA-6.3-006).** A block served on the bootstrap path and freed
after full initialization MUST be accepted by the compatible
allocator, or the bootstrap path MUST have failed the original
request instead of serving it. Bootstrap origin does not
create a second allocator for free purposes. Note the C1
consequence: a bootstrap-served block's user bytes are still
C1 bytes — unspecified, not zero-promised. Bootstrap service
changes the serving path, never the contents rule.

**C2.7 Fork during bootstrap — NO CLAIM (U-21).** Fork
occurring during bootstrap, before initialization completes,
is **[UNRESOLVED: U-21]** in contracts S4.5, and this draft
makes **no claim** for it: no child-state, lock-state,
reinitialization, or failure behaviour is stated, promised,
or inferred from any other fork case. Under MA-6.3-042, fork
behaviour not covered by MA-6.3-040 / MA-6.3-041 MUST NOT be
documented or relied on as Defined. B-006 S9.4 / INC-B006-7
record the wider fork posture the same way (MA-6.3-040..042;
U-17 also open). Any text that needs fork-during-bootstrap
behaviour must wait for the per-platform watchdog evidence
that alone closes U-21.

### C.3 When is / is not initialized — table (design)

| Moment / state | User bytes (C1) | Allocator-owned state (C1.4 / C2) |
|---|---|---|
| Before first entry; no init has run | No storage exists; nothing promised | Registry empty, counters zero — the defined starting state; lock statically initialized where the target permits; full init NOT assumed (MA-6.3-001) |
| During bootstrap / init in progress | No ownership passes on this path alone | Init is incomplete; reentrant entry must not treat it as complete (MA-6.3-004); storage is bootstrap-owned only (MA-6.3-002) |
| Mapping succeeded, publication not yet done (under lock) | Not yet the caller's; contents unspecified; caller must not read them — it has no pointer yet | Descriptor being fully written; not LIVE, not on the registry, not observable by another thread |
| Successful return of `mimir_malloc` | Caller's, contents **unspecified — NOT zero-guaranteed** (C1.1, C1.3) | Descriptor fully written and LIVE; registry and counters updated in the same locked section; `reserved` zero, unassigned `flags` bits zero |
| Allocation failure (any Section D row) | No pointer passes; no bytes are the caller's | No descriptor published; registry and counters unchanged |
| Bootstrap-served block, after full init | Still unspecified (C2.6) | Normal ownership; free accepted by the compatible allocator, or the block must never have been served (MA-6.3-006) |
| After a completed `mimir_free` | Pointer is dead; bytes must never be accessed again | Descriptor destroyed with its mapping by the single `munmap`; never accessed after its lifetime ends (Invariant 2) |
| Fork during bootstrap | **No claim** | **No claim — U-21 open (C2.7)** |

### C.4 Section C mapping table (design, Proposed)

| Rule | Contracts (direct unless marked analogy) | Invariant S9.1 | Layers |
|---|---|---|---|
| C1.1 contents unspecified | contracts S2.2 line; MA-6.1-MALLOC-01 **by analogy** (U-03) | 2 (only allocator metadata is truth-bound; user bytes are not metadata) | L1 boundary, L5 |
| C1.2 never touches user bytes | B-006 S9.2 design rule | 2, 6 | L1, L4, L5 |
| C1.4 descriptor fully written, `reserved`/`flags` zero | B-006 S7.3 / S8.1 / S9.2 | **2, 6** | L5 owns contents; L4 owns publication discipline |
| C1.4 registry/counters atomic with insertion | B-006 S8.1 / S9.2 | **6**, 2 | L4; storage bootstrap (below L1) |
| C2.1 pre-main / first entry | **MA-6.3-001, MA-6.3-005** | 9 (no assumed state to fail silently behind) | Bootstrap below L1; L6 |
| C2.2 bootstrap storage, no recursion | **MA-6.3-002, MA-6.3-003, MA-6.3-012** | 6, 9 | Bootstrap below L1; L4 |
| C2.3 reentrancy | **MA-6.3-004** | 6, 9 | Bootstrap below L1; L4 |
| C2.4 no containers / no ordering reliance | **MA-6.3-010, MA-6.3-011** | 9 | Bootstrap below L1 |
| C2.5 statically initialized lock | **MA-6.3-002, MA-6.3-012**; B-006 S9.1 | **9** | L6 provides primitive; L4 owns discipline; storage bootstrap |
| C2.6 bootstrap block freed later | **MA-6.3-006** | 2, 6 | Bootstrap below L1; L4, L5 |
| C2.7 fork during bootstrap: no claim | **MA-6.3-042**; U-21 open | — (no invariant is discharged by an unmade claim) | Bootstrap below L1 |

## D. Error Semantics (design)

### D.0 The caller-observable rule, stated once

For every **defined** failure of a valid call in this design:

- `mimir_malloc` failure is observed as exactly one thing:
  **a null return. Never a partial pointer** — never a pointer
  to a smaller allocation than asked, never a partially
  constructed block, never storage not allocated to the caller.
  By analogy: MA-6.1-MALLOC-02 (failure: null; MUST NOT return
  a pointer to storage not allocated to the caller),
  MA-6.1-MALLOC-04 (oversize is a failure under -02);
  directly for the internal shape: MA-6.2-020 (defined failure
  channel, no partially constructed block) and MA-6.2-024
  (multi-step failure rolls back or abandons internally, no
  half-owned block published).
- `mimir_free` **returns nothing** on every path it defines.
  By analogy: MA-6.1-FREE-01 (success: no value returned) and
  MA-6.1-FREE-02 / MA-6.2-010 (`free(NULL)`: a no-op that
  returns normally). A free therefore carries **no error
  return at all** — including the F-06 case in D.2, whose
  failure surface is bookkeeping state plus bounded reporting,
  not a code handed back to the caller.
- **No other caller-visible error channel is promised.** Not
  `errno` (D.1), not a return code (D.1), not an output
  parameter. Diagnostics (D.4) are an internal / host reporting
  surface, not a per-call error channel, and must never be
  described as one.

Defined failures are bounded failures: the slow / failure path
is bounded and testable (**Invariant 7**, primary for this
section), every fallback either preserves ownership or
explicitly fails (**Invariant 10**, primary), and a
synchronization failure is reported or terminates safely and
is never hidden (**Invariant 9**, primary). See D.5.

### D.1 Two error models that are NOT adopted

**D.1.1 `errno` IS NOT PROMISED.** Whether `errno` is set on
allocation failure, and to what, is target-dependent and
**UNRESOLVED — V-02 -> U-10, Open** (contracts S2, S6). B-006
S10 states the same posture as a general law of its failure
table, and this draft carries it unchanged: until a later
per-target contract decision adds an `errno` promise, no
caller may rely on `errno` from `mimir_malloc` / `mimir_free`,
no test may assert an `errno` value for them, and no text in
this design states one. This is a non-promise, not a claim
that `errno` is untouched: the design simply assigns it no
meaning, so incidental values carry no semantics.

**D.1.2 The `posix_memalign` return-code model is NOT
imported.** MA-6.1-POSIXMA-01 / MA-6.1-POSIXMA-02 define a
different interface: success is return value zero with the
pointer stored through an output parameter, failure is a
non-zero error number returned, the output parameter is left
unchanged, and `errno` is not that interface's channel
(**V-06**; the exact error numbers are themselves
**UNRESOLVED -> U-14**). None of that shape is imported into
the namespaced pair: `mimir_malloc` has no output parameter
and no error-number return, and `mimir_free` has no return at
all. Citing POSIXMA rows to give `mimir_malloc` a return-code
semantics is non-conforming with this section.

### D.2 Internal failure table — B-006 F-01..F-12 extended

This table **extends, does not replace**, B-006 S10. It adds
the two columns B-006 left implicit — what the caller observes
(D.0) and what this section's rule is — and invents no new
F-numbers. B-006 S10 governs where any wording differs.

| ID | Failure / condition | Caller observes | Internal disposition (design) |
|---|---|---|---|
| F-01 | `mmap` fails, ENOMEM | Null | No descriptor exists or is published; registry and counters unchanged; a failed `mmap` maps nothing, so no partial mapping exists. L6 reports, L5 creates nothing, L4 changes nothing. (Analogy MA-6.1-MALLOC-02; MA-6.2-020.) |
| F-02 | Request unrepresentable: overflow, or size/alignment that cannot be represented | Null | Rejected at L2 **before any syscall**; no `mmap` attempted. Arithmetic detail is B-007 Section A's subject; this row claims only the rejection, its position, and its observation. (MA-6.1-MALLOC-04; Invariant 3.) |
| F-03 | `mmap` fails, EMFILE / ENFILE (descriptor / file limits) | Null | As F-01. The design does not close, evict, or retry against the limit; the limit is the process's, reported by L6 as a plain failure. |
| F-04 | `mmap` fails, EAGAIN (temporarily unavailable) | Null | As F-01, with **no retry**: no internal retry loop, bounded or otherwise, is specified in this baseline. A retry policy is a later design decision. Failure latency is bounded by one syscall (Invariant 7). |
| F-05 | Zero-size request | **No observation is promised** | **EXCLUDED / UNDECIDED**, outside the supported call set: MA-6.1-MALLOC-03 is [UNRESOLVED: U-09]. This section gives zero-size no behaviour — neither the null rule of D.0 nor a unique pointer — and D.0 must not be read onto it. Resolution requires the per-target U-09 decision. |
| F-06 | `munmap` fails, or reports a partial release, during `mimir_free` | Nothing — `mimir_free` returns nothing (D.0) | The free does **NOT** complete silently. The descriptor **stays LIVE**: not unlinked, counters not decremented, allocation still recorded as owned, so bookkeeping still describes actual memory (Invariant 2). The length is never shortened on paper to match a partial release, and no later step treats the memory as freed. Reported via D.4, after lock release. An implementation that unlinks first must restore the LIVE entry under the lock — it must not lose it. L6 reports, L5 retains metadata, L4 retains ownership. |
| F-07 | Lock acquisition / release / initialization failure (any primitive error) | Null for an allocation in progress; or safe termination | **Never proceed unlocked**, never return overlapping memory, never publish a descriptor whose registry state is uncertain, never retry silently forever. Report or terminate safely (Invariant 9; B-006 S9.5). Whether report-and-fail or terminate applies to a given primitive error is a decision the later implementation must record; silently continuing unlocked is available in neither branch. |
| F-08 | Entry before initialization completes; reentrant entry; constructor / loader entry, pre-main | Bootstrap service, or null via the defined failure channel | Section C2 in full: MA-6.3-001..006, MA-6.3-010..012. Initialized state is never assumed; init is never re-entered as if complete. |
| F-09 | Metadata magic / state mismatch on the free path or an internal check (corruption) | No defined observation — see D.3 | **Debug-only response.** Debug build: preserve evidence, stop / report, never continue as if valid, never claim correction (MA-6.2-033 [COND: debug], MA-6.2-034). Normal build: this is the UB territory of D.3 and no behaviour is promised. In neither build is the suspect mapping unmapped on suspicion, and in neither build is a descriptor silently dropped (compare F-06). |
| F-10 | A multi-step mapping sequence completes only in part (guard for any later variant; the baseline issues one `mmap` per allocation) | Null | Every piece already obtained for that request is unmapped; no descriptor is published; no counter changes. A partial mapping is never returned as a smaller allocation. (MA-6.2-024, MA-6.2-020; Invariant 10.) |
| F-11 | Page-size discovery fails at L6 (`sysconf` error or an unusable value) | Null | **Defined failure, no fallback.** No mapping attempted, no descriptor created. The baseline cannot compute `usable_offset` or `mapping_length` without a valid **system page size**, so it fails rather than guessing — in particular it **never falls back to a hardcoded 4096 or any other constant** (architecture rule against hardcoded page size; B-006 INC-B006-3; MA-6.2-041 on qualified page terms). Cached discovery, if a later design adds it, must preserve this failure semantics. |
| F-12 | Error reporting itself cannot allocate, or errors cascade | The already-decided observation is unchanged | Reporting is bounded and allocation-free (D.4: MA-6.3-020..023). A reporting failure never changes the allocation outcome already decided and never converts a failure into a returned pointer. |

Out of table, stated once, as in B-006 S10: no row above
promises `errno` (V-02 / U-10, Open), no row promises
async-signal safety (MA-6.3-030..032), and no row defines fork
behaviour (MA-6.3-040..042; U-17 and, for bootstrap, U-21,
Open — see C2.7).

### D.3 The UB boundary — invalid frees have no defined error

The failure table of D.2 applies only to valid calls. The
following are **Undefined Behaviour**, and this design assigns
them **no defined error of any kind** — no null, no `errno`
value, no return code, no guaranteed diagnostic, no recovery,
and no safe continuation in a normal build:

- foreign pointer (not produced by this allocator), and free
  through an incompatible allocator: MA-6.1-FREE-03;
  MA-6.2-002, MA-6.2-005, **MA-6.2-030**;
- interior pointer (into a live allocation, not its start):
  MA-6.1-FREE-03; **MA-6.2-030**;
- use after free: **MA-6.2-031**;
- double free: MA-6.1-FREE-04; **MA-6.2-032**.

**Debug detection is the only softening, and it changes no
classification.** Under MA-6.2-033 [COND: debug] a debug build
MAY detect some of these cases (in this baseline, via the
B-006 S8.2 magic / state validation and registry membership);
on detection it MUST preserve evidence, stop / report, and
never continue as if the free were valid. Under MA-6.2-034,
metadata corruption found by an internal check MUST NOT be
claimed as corrected; evidence is preserved and the response
is stop / report (see F-09). Detection is best-effort and its
limits are stated in B-006 S8.3: a foreign pointer whose bytes
happen to satisfy the checks, an interior pointer into the
reserve page, and a double free after OS address reuse may all
evade detection. No detection outcome converts a UB case into
a Defined one (**MA-6.2-030..034** read together).

### D.4 Bounded, allocation-free reporting — never under lock

Reporting is how defined failures and debug detections become
visible inside the process; it is not a caller error channel
(D.0, D.1). Its rules, from contracts S4.3 and B-006 S9.4
item 3 / F-12:

1. **Bounded and allocation-free.** Reporting MUST NOT perform
   unbounded dynamic allocation and MUST be completable within
   a stated bound, on storage that does not come from the
   failing allocation path (MA-6.3-020). In this design the
   bound is the stronger, simpler one B-006 F-12 records:
   reporting is allocation-free, using bounded, preallocated
   form only. The bound is per report and is independent of
   the failing request's size.
2. **Independent of the reported state's health.** Reporting
   MUST NOT depend on the health of the failed or corrupted
   state it reports on; a truncated report within the bound is
   produced instead of no report or an unbounded one
   (MA-6.3-021). Evidence is not discarded to format a longer
   report.
3. **The same bound on the bootstrap path** (MA-6.3-022):
   bootstrap-available facilities only — which is why reporting
   may not wait for, or allocate through, full initialization
   (Section C2).
4. **The same bound under cascade** (MA-6.3-023): repeated or
   cascading errors keep the same per-report bound and MUST
   NOT accumulate retained storage without limit.
5. **Never under the lock.** A failure detected under the lock
   is recorded in locals in bounded, preallocated form, the
   lock is released, and only then is anything reported
   (B-006 S9.4 rule 3). The lock is likewise never held across
   any callback into code outside the allocator (S9.4 rule 2),
   and reporting while holding it would invite re-entry
   through any logging path that allocates (MA-6.3-004
   discipline). Layer note: diagnostics that observe through
   L7 do so as a sidecar — L7 never allocates through the
   allocator it observes, and its counters drive no decision
   in any layer (Invariant 8).

### D.5 Section D mapping table (design, Proposed)

| Rule | Contracts (direct unless marked analogy) | Invariant S9.1 (primary first) | Layers |
|---|---|---|---|
| D.0 null, never partial | MA-6.1-MALLOC-02 / -04 **by analogy** (U-03); MA-6.2-020, MA-6.2-024 | **10, 7** | L1 boundary; L2 admission; L4, L5 internal rollback |
| D.0 free returns nothing; `free(NULL)` no-op | MA-6.1-FREE-01 / -02 **by analogy** (U-03); MA-6.2-010, MA-6.2-001 | **10**, 4 | L1; L4 owner determination |
| D.1.1 `errno` not promised | V-02 -> **U-10 Open**; B-006 S10 | **7** (the bounded observation is the null return, nothing else) | L1 (the non-promise sits at the boundary) |
| D.1.2 POSIXMA model not imported | MA-6.1-POSIXMA-01 / -02 (that interface only); V-06 -> **U-14 Open** | — (a non-import discharges no invariant) | L1 |
| D.2 F-04 no retry; F-11 no fallback value | B-006 F-04, F-11; MA-6.2-041 (F-11) | **7, 10** | L6 reports / discovers; L5, L4 change nothing |
| D.2 F-06 descriptor stays LIVE | B-006 F-06; MA-6.2-034 posture (no silent loss) | **10**, 2 | L6 reports; L5 retains; L4 retains ownership |
| D.2 F-07 never proceed unlocked | B-006 S9.5, F-07 | **9** | L4 owns discipline; L6 provides primitive |
| D.3 UB boundary | MA-6.1-FREE-03 / -04 **by analogy** (U-03); **MA-6.2-030..034** | **9** (debug stop/report, never hidden continuation), 7 | L1 -> L4 owner determination; L5 validation data |
| D.4 bounded reporting, never under lock | **MA-6.3-020..023**; B-006 S9.4 rule 3 | **7, 9** | L4 releases before reporting; L7 sidecar observes |

### D.6 What Sections C and D deliberately do NOT do (design)

- No `calloc` / `realloc` semantics: **B-008**. `calloc`
  zeroing appears in C1.5 as an excluded contrast only, and
  Invariant 5 (failed `realloc` preservation) is therefore not
  discharged by anything in this draft.
- No checked-arithmetic or alignment rules: those are B-007
  Sections A / B; D.2 F-02 claims only the rejection point and
  its caller observation.
- No zero-size behaviour: U-09 is open and F-05 records the
  exclusion rather than resolving it.
- No `errno` promise (U-10), no `posix_memalign` error numbers
  (U-14), no fork behaviour (U-17, U-21), no async-signal
  safety claim (MA-6.3-030..032).
- No test-suite exit design: **B-009**. Test hooks named in the
  contracts (notably C-010 for bootstrap / reentrancy) are
  cited as contract framing only; no test is designed,
  specified as passing, or claimed as evidence here.
- No implementation, no measurement, no performance claim.
  This file is a builder draft for the B-007 design document;
  it has not been committed and makes no banking claim.

*End of builder-2 draft — Sections C and D only.*

---

## Appendix A. Consolidated contract-ID mapping table (design, Proposed)

Consolidated from the per-section mapping tables of Sections A–D
(A.6, B.5, C.4, D.5 — reproduced in full immediately below this
summary) and from B-006 Appendix A. **Every mapping for the
namespaced pair is by analogy only under U-03** (Section 0), except
`MA-6.3-*` bootstrap / diagnostics rows cited directly where a
fragment says so. All rows are Proposed.

| B-007 element | Contract IDs (all Proposed) | S9.1 Inv. | Layer(s) |
|---|---|---|---|
| Checked arithmetic, every step (Sec. A) | By analogy: MA-6.1-MALLOC-02, MA-6.1-MALLOC-04; MA-6.2-024 | 3 | L2 (owner); L5 layout re-check |
| Uniform failure: null, no partial block (A.3, D.0) | By analogy: MA-6.1-MALLOC-02 / -04; MA-6.2-020, MA-6.2-024 | 7, 10 | L1, L2, L4, L5 |
| Default alignment by construction (B.1) | By analogy: MA-6.1-MALLOC-01 (non-numeric only; V-07 / U-15 open) | 1, 3 | L5, L6, L2, L1 |
| Explicit alignment success / failure, once gate closes (B.2–B.4) | By analogy, that interface only: MA-6.1-ALIGNED-01 / -02; MA-6.1-POSIXMA-01 / -02 | 3 | L2, L5 |
| Explicit alignment validity / size-multiple (B.2, B.4) | MA-6.1-ALIGNED-03 / -04, MA-6.1-POSIXMA-03 — all [UNRESOLVED: U-11] via V-03 | — gate open | L2 (gate site, no rule yet) |
| Zero-size, any form (A.4, B.4, C/D F-05) | MA-6.1-MALLOC-03, MA-6.1-ALIGNED-05, MA-6.1-POSIXMA-04 — all [UNRESOLVED: U-09] | — | L1 / L2 (named, not handled) |
| User contents unspecified, never zero-promised (C.1) | contracts S2.2 line; by analogy MA-6.1-MALLOC-01 (U-03) | 2 (metadata only is truth-bound) | L1, L5 |
| Bootstrap / allocator-owned init before ready (C.2) | Direct: MA-6.3-001..006, MA-6.3-010..012 — in particular MA-6.3-002, MA-6.3-012 | 1, 2, 7 in part | Below L1; storage used by L4 / L5 |
| Free: live pointer, NULL no-op, invalid / double = UB (D.0, D.3) | By analogy: MA-6.1-FREE-01, -02, -03, -04; MA-6.2-001, MA-6.2-010, MA-6.2-030..034 | 4, 9, 10 | L1 → L4; L5 |
| Mapping failure / rollback, fail-the-Nth context (D.2, B-006 F-01..F-12) | By analogy: MA-6.1-MALLOC-02, MA-6.2-020, MA-6.2-021, MA-6.2-024 | 7, 9, 10 | L2, L4, L5, L6 |
| munmap failure: descriptor stays LIVE (D.2 / B-006 F-06) | By analogy: MA-6.1-FREE-01, MA-6.2-001; reporting MA-6.3-020..023 | 2, 10 | L4, L5 via L6 |
| Lock, cross-thread handoff, sync failure (B-006 S9; D.2–D.4) | MA-6.2-054; reporting bounded under MA-6.3-020..023 posture | 6, 9 | L4 (owner), L6 (primitives) |
| `errno` not promised (D.1) | V-02 → U-10 Open; no MA-* row promises it for the namespaced pair | 7 (observation is the null return) | L1 |
| POSIXMA error-number model not imported (D.1) | MA-6.1-POSIXMA-01 / -02 (that interface only); V-06 → U-14 Open | — non-import | L1 |
| Fork, incl. fork during bootstrap (C.2) | MA-6.3-040..042; U-17..U-21 Open — no behaviour claimed | — | Below L1 / L4 (named only) |
| Registry / bookkeeping truth (B-006 S8; C.2, D.2) | By analogy: MA-6.2-001, MA-6.2-023, MA-6.2-024 | 1, 2 | L4 / L5 |
| calloc / realloc rows | MA-6.1-CALLOC-*, MA-6.1-REALLOC-* — **B-008, not mapped by B-007** | 5 = B-008 | — |

### A-EXT. Per-section mapping tables reproduced in full from the builder fragments

The four tables below are extracted verbatim from Sections A.6,
B.5, C.4, and D.5 as they appear in Sections A–D of this document;
they are the detailed form of the consolidated table above and are
not a summary of it.

### A.6 Mapping for Section A (design, by analogy only — U-03)

| Element | Contracts (all Proposed, by analogy) | Invariant (S9.1) | Layers |
|---|---|---|---|
| Check-before-evaluate, all steps | MA-6.1-MALLOC-04; MA-6.2-020, MA-6.2-024 | **3 (primary)** | L2 (owner); L5 (layout rows); L6 (receives checked length only) |
| Uniform failure before syscall | MA-6.1-MALLOC-02; MA-6.1-MALLOC-04 | 3 (primary); 10 (fallback preserves or fails) contributes | L2 → L1 (return); L5, L6 (no call made) |
| No wrapped allocation | MA-6.1-MALLOC-04; MA-6.2-020 | 3 (primary); 1 (no overlapping / false block) contributes | L2, L5 |
| No partial / half-owned block | MA-6.2-020; MA-6.2-024 | 3 (primary for the arithmetic trigger); 1, 10 contribute | L2, L5 (L4 bookkeeping unchanged) |
| Page terms qualified, no hardcoded size | MA-6.2-041 (with MA-6.2-040 terms) | 3 (checks use the discovered value) | L2, L5, L6 (discovery) |

---

### B.5 Mapping for Section B (design, by analogy only — U-03)

| Element | Contracts (all Proposed, by analogy) | Invariant (S9.1) | Layers |
|---|---|---|---|
| Default alignment by construction | MA-6.1-MALLOC-01 (non-numeric claim only; V-07 / U-15 open) | 1 (distinct storage); 3 (A9 addition checked) | L5 (layout), L6 (page size / mapping), L2, L1 |
| Explicit alignment — success / failure shapes | MA-6.1-ALIGNED-01, MA-6.1-ALIGNED-02; MA-6.1-POSIXMA-01, MA-6.1-POSIXMA-02 (distinct channel) | 3 (A7 arithmetic); 10 (fallback preserves or fails) contributes | L2 (future gate), L5, L6 |
| Explicit alignment — validity / size-multiple | MA-6.1-ALIGNED-03, MA-6.1-ALIGNED-04; MA-6.1-POSIXMA-03 — all [UNRESOLVED: U-11], via V-03 | — (no invariant discharged; gate open) | L2 (gate site, no rule yet) |
| Explicit alignment — zero-size / null out-param | MA-6.1-ALIGNED-05, MA-6.1-POSIXMA-04 ([UNRESOLVED: U-09]); MA-6.1-POSIXMA-05 (UB) | — | L1 / L2 (named, not handled) |
| Over-aligned (> system page size) | MA-6.1-ALIGNED-01 (the alignment that would have to be met); MA-6.1-ALIGNED-02 (failure shape) — re-spec required, B.3 | 2 (descriptor lifetime / derivation); 3 (oversize arithmetic) | L5 (layout + descriptor), L4 (free-path owner determination), L6 |

---

*End of builder-1 draft, Sections A and B only. Design only;
Proposed; all contract mappings by analogy only under U-03.
Recorded and unresolved in this draft: INC-B007-A1 (A.5),
U-03, U-09, U-10 (via V-02), U-11 (via V-03), U-15 (via V-07).
No implementation, measurement, or Phase 3 exit claim is
made anywhere in this draft.*

### C.4 Section C mapping table (design, Proposed)

| Rule | Contracts (direct unless marked analogy) | Invariant S9.1 | Layers |
|---|---|---|---|
| C1.1 contents unspecified | contracts S2.2 line; MA-6.1-MALLOC-01 **by analogy** (U-03) | 2 (only allocator metadata is truth-bound; user bytes are not metadata) | L1 boundary, L5 |
| C1.2 never touches user bytes | B-006 S9.2 design rule | 2, 6 | L1, L4, L5 |
| C1.4 descriptor fully written, `reserved`/`flags` zero | B-006 S7.3 / S8.1 / S9.2 | **2, 6** | L5 owns contents; L4 owns publication discipline |
| C1.4 registry/counters atomic with insertion | B-006 S8.1 / S9.2 | **6**, 2 | L4; storage bootstrap (below L1) |
| C2.1 pre-main / first entry | **MA-6.3-001, MA-6.3-005** | 9 (no assumed state to fail silently behind) | Bootstrap below L1; L6 |
| C2.2 bootstrap storage, no recursion | **MA-6.3-002, MA-6.3-003, MA-6.3-012** | 6, 9 | Bootstrap below L1; L4 |
| C2.3 reentrancy | **MA-6.3-004** | 6, 9 | Bootstrap below L1; L4 |
| C2.4 no containers / no ordering reliance | **MA-6.3-010, MA-6.3-011** | 9 | Bootstrap below L1 |
| C2.5 statically initialized lock | **MA-6.3-002, MA-6.3-012**; B-006 S9.1 | **9** | L6 provides primitive; L4 owns discipline; storage bootstrap |
| C2.6 bootstrap block freed later | **MA-6.3-006** | 2, 6 | Bootstrap below L1; L4, L5 |
| C2.7 fork during bootstrap: no claim | **MA-6.3-042**; U-21 open | — (no invariant is discharged by an unmade claim) | Bootstrap below L1 |

### D.5 Section D mapping table (design, Proposed)

| Rule | Contracts (direct unless marked analogy) | Invariant S9.1 (primary first) | Layers |
|---|---|---|---|
| D.0 null, never partial | MA-6.1-MALLOC-02 / -04 **by analogy** (U-03); MA-6.2-020, MA-6.2-024 | **10, 7** | L1 boundary; L2 admission; L4, L5 internal rollback |
| D.0 free returns nothing; `free(NULL)` no-op | MA-6.1-FREE-01 / -02 **by analogy** (U-03); MA-6.2-010, MA-6.2-001 | **10**, 4 | L1; L4 owner determination |
| D.1.1 `errno` not promised | V-02 -> **U-10 Open**; B-006 S10 | **7** (the bounded observation is the null return, nothing else) | L1 (the non-promise sits at the boundary) |
| D.1.2 POSIXMA model not imported | MA-6.1-POSIXMA-01 / -02 (that interface only); V-06 -> **U-14 Open** | — (a non-import discharges no invariant) | L1 |
| D.2 F-04 no retry; F-11 no fallback value | B-006 F-04, F-11; MA-6.2-041 (F-11) | **7, 10** | L6 reports / discovers; L5, L4 change nothing |
| D.2 F-06 descriptor stays LIVE | B-006 F-06; MA-6.2-034 posture (no silent loss) | **10**, 2 | L6 reports; L5 retains; L4 retains ownership |
| D.2 F-07 never proceed unlocked | B-006 S9.5, F-07 | **9** | L4 owns discipline; L6 provides primitive |
| D.3 UB boundary | MA-6.1-FREE-03 / -04 **by analogy** (U-03); **MA-6.2-030..034** | **9** (debug stop/report, never hidden continuation), 7 | L1 -> L4 owner determination; L5 validation data |
| D.4 bounded reporting, never under lock | **MA-6.3-020..023**; B-006 S9.4 rule 3 | **7, 9** | L4 releases before reporting; L7 sidecar observes |



## Appendix B. L1–L7 layer summary (design, Proposed)

Phase 3 collapsed form, per B-006 Appendix B and
`docs/ARCHITECTURE_LAYERS.md` Section 13 (invariant-to-layer map).
Dependency law: request L1→L2→(L3|L4)→L5→L6; free L1→L4→L5→L6;
no layer skips to L6; the sole non-adjacent use is L3/L4 consuming
L6 threading primitives through the published L6 interface.
**Bootstrap sits below L1** — it has no S7.1 layer, and its storage
is bootstrap-owned / preallocated, not taken from the normal path
(MA-6.3-002, MA-6.3-012).

| Layer | Phase 3 form and B-007 role |
|---|---|
| **L1** Public boundary | `mimir_malloc` / `mimir_free` design labels only; no ABI, no stats/trim, no calloc/realloc. Caller-observable failure (null / no-op / UB boundary) sits here. Contributes to Inv. 4 (owner) in the layers map via L4, and to Inv. 1, 5, 10. By analogy MA-6.1-MALLOC-* / FREE-* (U-03). |
| **L2** Admission and checked arithmetic | **Owner of S9.1 Invariant 3** (size and alignment calculations cannot overflow). Admits or rejects before any syscall (Section A); holds the explicit-alignment validity gate site (Section B.2, rule still open, U-11). B-006 gate-only form is given its detail by B-007 Sections A–B. |
| **L3** Small-object fast path | Absent / pass-through to L4 in Phase 3. No cache, no list, no decision (Phase 8). Contributes to Inv. 1, 6, 7 in the layers map only as a pass-through. |
| **L4** Central ownership | **Owner of S9.1 Invariant 1** (one owner, non-overlapping) **and Invariant 6** (cross-thread handoff; no node in incompatible lists), and of Invariants 7, 9, and 10 in the layers map. Degenerate Phase 3 form: ownership + registry, owns lock discipline, sole client of L5. No size classes (Phase 7). |
| **L5** Page / span manager | **Owner of S9.1 Invariant 2** (metadata describes actual memory; not accessed after its lifetime ends). Degenerate: one mapping per allocation, metadata + occupancy, layout computation re-checked under Section A. No split / coalesce / reuse / trim (Phase 7). |
| **L6** OS abstraction | Runtime page-size discovery; mmap / munmap boundary; threading primitives via its published interface. Only layer with platform-specific code. Contributes to Inv. 7 and 9. |
| **L7** Optional observability | Compiled out in Phase 3. No stats / counters designed (Phase 10; namespaced stats also U-03). **Owner of Invariant 8 in the layers map** (statistics do not drive unsafe decisions) — discharged in Phase 3 only in the vacuous form that no statistics exist to drive a decision. |
| **Bootstrap (below L1)** | Allocation-free startup path (Roadmap S6.3; Section C.2): descriptor, registry, counters, and lock fully initialized before publication / before ready, from bootstrap-owned storage only. Fork during bootstrap: no claim, U-21. |

## Appendix C. Inconsistencies register (design) — recorded, not resolved

Recording an inconsistency does not resolve it. Where a governing
reading is named below, it is named because a source text itself
gives that reading precedence for the free path, not because B-007
has edited the source.

- **INC-B007-1 — TOTAL-MAPPING FORMULA DISCREPANCY (UNRESOLVED).**
  B-006 Section 6 states
  `total_mapped = page_round_up(metadata_reserve + requested_size)`
  — one round-up over one sum, with `metadata_reserve` not valued
  in that section and nowhere stated to equal `usable_offset`.
  B-006 S11.1 step 5 states
  `mapping_length = usable_offset + usable region rounded up to whole system pages covering the admitted size`,
  which parses both as that Section 6 shape and as
  `usable_offset + page_round_up(requested_size)` — a round-up of
  the usable region alone, added to the offset afterwards.
  **Recorded, not silently resolved:** Section A.5 records the same
  discrepancy in full. **Both forms are checked in Section A** —
  every arithmetic step of either form is subjected to the
  check-before-evaluate predicates of A.1/A.2, so the checked-
  arithmetic design holds under either reading. Which formula is
  canonical, and the definition of `metadata_reserve` against
  `usable_offset`, remain for a reconciling revision of B-006.
- **INC-B007-2 — FREE ORDERING: S8.2 vs S11.2 / F-06.** B-006
  Section 8.2 step 3 orders the free as: mark state RETIRED,
  **unlink the descriptor from the registry and decrement the
  counters before `munmap`**, then munmap the whole mapping.
  B-006 Section 11.2 steps 5–7 and failure row **F-06** order it
  the other way: mark RETIRED, **`munmap` first**; only on munmap
  success unlink and decrement in the same locked section; and on
  munmap failure (F-06) the descriptor is restored to / remains
  LIVE, stays on the registry, and counters are unchanged — the
  allocation is never silently dropped. **S11.2 / F-06 governs:**
  it is the later, flow-level, failure-aware statement, and S8.2s
  pre-munmap unlink would make the F-06 failure branch impossible
  as written. Sections C and D of this document follow S11.2 / F-06.
- **INC-B007-3 — ZERO-FILL: OS FACT vs NO GUARANTEE.** A fresh
  anonymous OS mapping (`MAP_ANONYMOUS`) reads as zero on the
  targets in view — an operating-system fact about the backing
  mechanism. That fact is **not** a zeroing guarantee for
  `mimir_malloc`: contracts S2.2 states that contents of newly
  allocated storage are not specified by any row, and Section C.1
  therefore promises no zero contents, permits no caller reliance
  on zero, and permits no `mimir_malloc` test asserting zero. Any
  later reuse / splitting / caching backing would return previously
  used bytes under the same name. A zero guarantee exists only
  where a contract row says so (for example the calloc success row
  — B-008 scope, not B-007).
- **INC-B007-4 — NAMESPACED CONTRACT GAP (U-03).** Roadmap Phase 3
  and MIM-005 require namespaced alloc / free, but `contracts.md`
  gives `mimir_malloc` / `mimir_free` **no contract rows** — they
  are [UNRESOLVED: U-03], not in Section 6 scope, Appendix A
  stability Unassigned (also INC-B006-1). Every mapping in this
  document for that pair is therefore by analogy only, Proposed.
- **INC-B007-5 — NO PRESCRIBED B-007 FILENAME.** Neither
  `WAVE_PLAN.md` (forge directory; its B-007 line gives the topic
  only) nor B-006 (Sections 12–14, Appendices) nor the B-007
  assignment prescribes a filename for this deliverable (checked
  for B-007 in WAVE_PLAN and in B-006 Section 12). This document
  therefore uses the descriptive name
  `docs/PHASE_3_ARITHMETIC_ALIGNMENT_INIT_ERROR_DESIGN.md`,
  following the B-006 naming pattern. (Compare INC-B006-2, the
  same gap recorded for B-006.)
- **INC-B007-6 — PHASE 3 EXIT SPLIT ACROSS B-006..B-009.** The
  Roadmap Phase 3 text bundles the minimal allocator, checked
  arithmetic / alignment / initialization / error semantics,
  calloc / realloc, and the exit test suite into one Phase 3 exit,
  but Wave 2 splits that work across **B-006** (mapping / metadata /
  lock design), **B-007** (this document: arithmetic, alignment,
  clean init, error semantics), **B-008** (calloc / realloc and
  unsupported behaviour), and **B-009** (Phase 3 exit test-suite
  design). No one slice — including this one — satisfies the
  Phase 3 exit, and no slice may be read as claiming it
  (also INC-B006-4).
- **INC-B007-7 — OVER-ALIGNED FREE-PATH DERIVATION (from B.3).**
  For an over-aligned allocation the usable offset becomes
  alignment-dependent and variable, while the B-006 free path
  derives the descriptor as `p - usable_offset` with the offset
  fixed at one system page, and the offset itself is stored in
  the descriptor being located — circular as specified. Section
  B.3 records this in full; the derivation must be re-specified
  before over-aligned support can be claimed. Recorded, not
  resolved by the existence of the S7.3 `usable_offset` field.

## Appendix D. Section 9.1 invariants 1–10 — discharge by B-007 section (design)

Roadmap Section 9.1 wording governs; the short forms below are
abbreviated. Owner layers are from `ARCHITECTURE_LAYERS.md`
Section 13 (see Appendix B).

| Inv. | Invariant (short form) | Discharged by B-007 section | Note |
|---|---|---|---|
| 1 | Every live allocation has one valid owner and non-overlapping usable storage | A (no wrapped / overlapping size can be admitted), B.1 (distinct, page-placed storage by construction), C.2 (publication only after complete initialization), D.0 / D.2 (no partial block ever returned) | Owner L4; B-007 discharges the arithmetic, alignment, init, and failure aspects; the ownership / registry design itself is B-006. |
| 2 | Metadata describes actual allocated memory and is not accessed after its own lifetime ends | C.1 / C.2 (only allocator metadata is truth-bound; fully initialized before publication), D.2 / F-06 (descriptor stays LIVE and registered on munmap failure; never accessed after its mapping is gone) | Owner L5. |
| 3 | Size and alignment calculations cannot overflow | **Section A in full** (check before evaluate, every step, both formula forms of INC-B007-1); Section B.3 (over-aligned total re-checked under A) | Owner L2. The central invariant of this slice. |
| 4 | Freeing a null pointer behaves according to the target contract | **Section D.0 / D.2 (F-table free rows), with C.3/D mapping rows:** `mimir_free(NULL)` is a defined no-op, before any lock or descriptor access, by analogy MA-6.1-FREE-02 and MA-6.2-010 (U-03) | Owner L1. B-006 S8.2 step 1 / S11.2 step 2 state the same rule; B-007 carries it in its error semantics. |
| 5 | A failed `realloc` preserves the original valid allocation where the selected target contract requires it | **NOT discharged by B-007 — B-008** | Realloc semantics are B-008 in full (Section 1.2). No realloc rule is stated here. Owner L1 in the layers map. |
| 6 | Cross-thread handoff preserves ownership; no list node appears simultaneously in incompatible free lists | D.2 / D.4 in part (failure and reporting never break ownership; reporting is bounded, allocation-free, and never under the lock), on the B-006 lock / handoff design | Owner L4. Phase 3 has no free lists and L3 is pass-through, so the list clause is discharged only in its degenerate Phase 3 form; the handoff design itself is B-006 (MA-6.2-054). |
| 7 | A full cache or failed backing allocation has a bounded and testable slow path | A.3 (uniform failure), C.2 (bounded bootstrap), D.0 / D.2 (every backing failure: null, no partial block, bounded observation), D.4 (bounded, allocation-free reporting) | Owners L4 (layers map), with L3 / L5 / L6 contributing. Phase 3 has no cache; the failed-backing-allocation clause is the operative one. Testability itself is B-009. |
| 8 | Statistics do not drive unsafe decisions if they are approximate or stale | No B-007 section — discharged only vacuously in Phase 3 | Owner L7, which is compiled out; B-007 designs no statistics and no decision reads one (Section 1.2). |
| 9 | A synchronization failure is reported or terminates safely per the contract; it is not hidden by returning overlapping memory | D.2 (lock-failure rows, extending B-006 S9.5 / F-07), D.3 (UB boundary is never a hidden continuation), D.4 (reporting path) | Owner L4, L6 contributing. No unlocked path ever returns memory. |
| 10 | Every fallback either preserves allocation ownership or explicitly fails | A.3, D.0 (explicit null failure, no ownership passes), D.2 in full (each F-row preserves ownership or fails explicitly; F-06 keeps the block owned), D.1 (no silent fallback to a different allocator or error model) | Owners L4, with L1 / L5 contributing. Roadmap S6.2: no silent fallback to a different allocator. |

## Sources (design)

- Assignment: `hidden_files/ASSIGNMENT.md` — TEAM-07 B-007, base
  243f007, design-only scope and NOT-done list.
- Builder fragments assembled verbatim in Sections A–D:
  `hidden_files/b007_builder1_AB.md` (Sections A+B, 486 lines) and
  `hidden_files/b007_builder2_CD.md` (Sections C+D, 466 lines).
- Wave plan: `WAVE_PLAN.md` in `/home/pi/mimiralloc-forge` —
  B-007 line (topic only, no filename prescribed; INC-B007-5).
- B-006: `docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md` — Sections
  1–6, S7–S11, Sections 12–14, Appendices A–C; base design for
  every rule in this document.
- Contracts: `docs/contracts.md` (A-008) — MA-6.1-* / MA-6.2-* /
  MA-6.3-* rows (all Proposed), V-01..V-07, U-01..U-21.
- Architecture: `docs/ARCHITECTURE_LAYERS.md` — L1–L7, dependency
  law, Section 13 invariant-to-layer map; all Proposed.
- Roadmap: `docs/MimirAlloc_Research_and_Development_Roadmap.md`
  — Phase 3, Sections 6.2, 6.3, 9.1 invariants 1–10, 9.3.
- Language: D-006 in `docs/IMPLEMENTATION_LANGUAGE_DECISION.md`
  and `docs/DECISION_LEDGER.md` — portable C11 core, C ABI
  (cited as context; no code is written in this slice).
- Targets: `docs/SUPPORTED_TARGETS.md` — runtime page-size
  discovery required; any numeric page size is a per-environment
  record only, not a target assumption (U-15 / V-07 open).

---

*End of B-007. Design only; Proposed. No implementation, no
measurement, no ABI promise, and no Phase 3 exit claim is made
anywhere in this document.*
