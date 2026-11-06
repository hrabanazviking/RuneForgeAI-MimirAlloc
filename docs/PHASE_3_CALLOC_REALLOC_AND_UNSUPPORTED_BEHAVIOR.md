# Phase 3 — Minimal Allocator Design (continued): `calloc` / `realloc` Semantics and Unsupported Behaviour

## Document control

| Field | Value |
|---|---|
| Deliverable | B-008 — Phase 3 design: `calloc` / `realloc` precise semantics + unsupported-behaviour document |
| Roadmap source | Roadmap Phase 3 (Section 8, Phase 3 text and exit), Section 6.1–6.3, Section 7.1–7.3, Section 9.1–9.3, Section 10 |
| Design base | B-006 — `docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md` (namespaced mapping-backed minimal allocator, issue MIM-005, design only, Proposed) |
| Contract source | A-008 — `docs/contracts.md` (all rows **Proposed**; no row is adopted or implemented fact) |
| Status | **Proposed — design only** |
| Implementation status | No implementation exists; none is claimed. No code is delivered by this slice. |
| Evidence label | **Proposed** for every statement in this document (Roadmap Section 2 labels) |

> **HONESTY BANNER — DESIGN ONLY.** This document is design only.
> Nothing in it is implemented, nothing in it is measured, and
> nothing in it describes existing behaviour. It states what a
> later implementation of the B-006 minimal allocator is designed
> to do for `calloc` and `realloc`, and what that allocator
> deliberately does **not** support. Where a source contract is
> unresolved, this document records the case as unresolved and
> states **no** behaviour for it; it does not settle any
> unresolved register entry. The Phase 3 exit is **not** met by
> this document (see Section 8).

---

## 1. Scope, names, and reading rules

### 1.1 What this document adds to B-006

B-006 designed `mimir_malloc` and `mimir_free` only. Its
Section 12.2 states, verbatim in substance: B-006 provides NO
`mimir_calloc` and NO `mimir_realloc` — no design label, no
signature, no semantics for either name. This document (B-008)
is the slice B-006 Section 12.2 deferred that work to. It adds,
at design level only:

1. `mimir_calloc` — design label for a namespaced zeroing
   allocation entry point (Section 3 of this document).
2. `mimir_realloc` — design label for a namespaced resizing
   entry point (Section 4 of this document).
3. The unsupported-behaviour record for the Phase 3 minimal
   allocator as a whole, including the two new entry points
   (Sections 6–7), which the Roadmap Phase 3 exit sentence
   requires: *"Document unsupported behavior."*

Nothing else is added. In particular this document does not
add aligned allocation, `malloc_usable_size`, statistics, trim,
interposition, or any standard-name symbol (see Section 6).

### 1.2 The namespaced-name caveat (U-03) — read first

The contract rows cited throughout this document are the
**core-symbol** rows of `docs/contracts.md` (`MA-6.1-CALLOC-*`,
`MA-6.1-REALLOC-*`, `MA-6.2-*`, `MA-6.3-*`). Those rows name
`calloc` and `realloc`. The namespaced names `mimir_calloc`
and `mimir_realloc` have **no contract rows of their own**:
contracts Section 1.3 puts the namespaced `mimir_*` API of
Roadmap Section 7.1 outside Section 6 scope, recorded as
**[UNRESOLVED: U-03]**, and B-006 recorded the same gap for
`mimir_malloc` / `mimir_free` as INC-B006-1.

Therefore every mapping in this document from a `MA-*` row to
`mimir_calloc` or `mimir_realloc` is a mapping **by analogy
under U-03**, exactly as in B-006. It is a proposed design
mapping, not a direct namespaced contract. If U-03 is later
closed by giving the namespaced API its own contract document,
that document governs and this one must be reconciled to it.

### 1.3 Relation to B-007 (checked arithmetic, alignment, clean initialisation)

Roadmap Phase 3 bundles "checked allocation arithmetic,
alignment, clean initialization, errors, and tests" into one
phase; Wave 2 split that bundle across B-006 (mapping / lock),
B-007 (arithmetic / alignment / init / errors, Sections
6.2–6.3 and 9.1 in detail), B-008 (this slice), and B-009
(exit test suite) — the split B-006 recorded as INC-B006-4.

This document therefore **consumes** B-007's subjects as an
interface, and does not re-design them:

- **Checked arithmetic.** `calloc`'s count × size product and
  `realloc`'s new-size admission pass through the same L2
  admission gate B-006 designed (B-006 Sections 5–6 in its
  layer terms: L2 admission + checked arithmetic). The
  detailed rules, and their proofs — overflow-checked
  evaluation of size, alignment, rounding, and total-mapping
  calculations, per Roadmap Section 9.1 Invariant 3 — are
  B-007 scope (B-006 Section 12.1). This document states only
  the order of operations `calloc` / `realloc` require of
  that gate, and the caller-visible result of each outcome.
- **Alignment.** The numeric alignment guarantee, alignment
  validity, and any size-multiple rule are B-007 scope over
  **V-07 / U-15** and **V-03 / U-11**, both open. This
  document requires only that a successful `mimir_calloc`
  return storage aligned exactly as a successful
  `mimir_malloc` of the same product size
  (**MA-6.1-CALLOC-01**, "suitably aligned as in
  MA-6.1-MALLOC-01"), and states no numeric value.
- **Clean initialisation / bootstrap.** The bootstrap,
  reentrancy, no-self-allocation, and bounded-reporting rules
  are **MA-6.3-001..006**, **MA-6.3-010..012**, and
  **MA-6.3-020..023**, designed at B-006 level and detailed
  by B-007. Sections 3.7 and 4.11 of this document state only
  what `calloc` / `realloc` add to them: nothing that
  allocates, and nothing that reports without bound.

**Recorded dependency risk (see INC-B008-1, Section 9):** at
this slice's base (243f007, B-006 banked) there is **no B-007
document in the repository**. B-007's rules exist at this base
only as the pointers in B-006 Sections 12.1 and 13 and the
contract rows themselves. This document is written against
those pointers. When B-007 banks, the keeper / a later
reconciliation slice must check this document against B-007's
actual text; any divergence is a defect to fix, not a
disagreement to leave standing.

### 1.4 Reading rules

- **Proposed throughout.** Every contract row cited is
  Proposed (INC-B006-10 carries into this slice unchanged).
  Citing a row states the design intent; it never claims the
  row is adopted, implemented, or tested.
- **Classification vocabulary** is contracts Section 1.6:
  **Defined** (a behaviour is proposed), **UB** (no behaviour
  is proposed; no return value, error value, or side effect
  is stated), **Debug-detectable only** (no portable behaviour
  is proposed; a debug build MAY detect and stop / report,
  which never converts the case to Defined and is never a
  recovery). This document adds one documentary bucket,
  **Not present**, for symbols the minimal allocator does not
  provide at all (Section 6.1): absence is a link / lookup
  fact about the design, not a runtime behaviour.
- **Unresolved means no claim.** A case tagged
  [UNRESOLVED: U-nn] has no behaviour in this document, even
  where a plausible candidate behaviour is listed for the
  record. Integrators must not rely on any candidate.
- **Hooks are three schemes, not one** (INC-B006-9 carries):
  `C-001..C-010` (contracts Section 6, Roadmap Section 10
  mapping), `C-3nn` / `C-4nn` slots (contracts Sections 3–4),
  and named hooks (`MA-CALLOC-*`, `MA-REALLOC-*`). This
  document cites each exactly as its source names it.

---

## 2. Design at a glance

Both entry points run on the unmodified B-006 machine:
L1 public boundary → L2 admission + checked arithmetic →
L4 central ownership (registry, the one simple lock) →
L5 page / span manager in its degenerate Phase 3 form
(one mapping per allocation, metadata in the mapping,
no split / coalesce / reuse / trim) → L6 OS abstraction
(runtime-discovered system page size; never a hardcoded
value — INC-B006-3). L3 is pass-through; L7 is compiled out.

| Question | `mimir_calloc` (design) | `mimir_realloc` (design) |
|---|---|---|
| Core act | Admit product, map, zero, publish | Validate old block, admit new size, move-or-retain, transfer ownership |
| Overflow rule | Product checked **before** multiplication is relied on; overflow = defined failure, null, no wrapped allocation (MA-6.1-CALLOC-02) | New size admitted by the same L2 gate as `malloc` of that size; unrepresentable / unsatisfiable = defined failure, original untouched (MA-6.1-REALLOC-02) |
| Contents on success | Usable extent zeroed in construction; every byte of the requested extent reads zero at return (MA-6.1-CALLOC-01, -05) | First `min(old, new)` bytes preserved byte-for-byte; bytes beyond that are unspecified (MA-6.1-REALLOC-01, -06) |
| Contents on failure | No block is returned; nothing is owed (MA-6.1-CALLOC-03, MA-6.2-020, -024) | Original block valid, unchanged, caller-owned, freeable exactly as before (MA-6.1-REALLOC-02, MA-6.2-022) |
| In-place promise | Not applicable | **None.** No in-place grow is promised; see the honest in-place account in Section 4.4 |
| Zero-size case | [UNRESOLVED: U-09] — no behaviour (MA-6.1-CALLOC-04) | `realloc(p, 0)`: [UNRESOLVED: U-12] — no behaviour (MA-6.1-REALLOC-04) |
| Null case | Not applicable (no pointer argument) | `realloc(NULL, n)` = `malloc(n)` for `n > 0`, success and failure alike (MA-6.1-REALLOC-03); `n == 0` unresolved (Section 4.7) |

---

## 3. `mimir_calloc` — precise semantics (design)

Design label only, by analogy under U-03 (Section 1.2):
`mimir_calloc(count, size)`, corresponding to the core
candidate `calloc(count, size)` of Roadmap Section 6.1 —
"with overflow checking before multiplication and guaranteed
zeroing of the requested bytes" — and contracts Section 2.4.

### 3.1 Preconditions for a Defined result

A Defined result requires all of: `count > 0` and `size > 0`
(the zero-factor case is unresolved, Section 3.6); the
product `count × size` is representable and admittable at L2;
and the backing mapping can be obtained. Every other Defined
case below is a failure case with a stated result. There is
no precondition on the caller's memory contents: `calloc`
takes no pointer argument, so no ownership precondition
exists beyond the general bootstrap / init state of
Section 3.7.

### 3.2 The product, and overflow — order of operations

The design fixes this order; no step may be reordered:

0. **Zero-factor gate — before any arithmetic.** If
   `count == 0` or `size == 0` (or both), the Defined flow
   below is **not entered at all**: the case is the
   unresolved zero-factor case of Section 3.6
   (MA-6.1-CALLOC-04, [UNRESOLVED: U-09]), and this section
   states no result for it — in particular it is **not**
   routed through admission as a product of zero, and no
   `malloc(0)` behaviour is inherited or implied. Every
   step below assumes both factors are greater than zero
   (Section 3.1).
1. **Check before multiplying.** L2 first establishes that
   `count × size` can be represented, using the
   checked-arithmetic discipline whose detail is B-007 scope
   (Section 1.3). The multiplication's result is not relied
   on — for admission, for a mapping size, or for any other
   purpose — until that check has passed. This is the
   direct design reading of **MA-6.1-CALLOC-02 [NORM]**:
   "The overflow MUST be detected before multiplication is
   relied on for an allocation size, and the call MUST be
   treated as a failure."
2. **Overflow outcome (Defined).** If the product cannot be
   represented: the call fails, returns null, and **no
   allocation of a wrapped-around size occurs**
   (MA-6.1-CALLOC-02). No block is published, no registry
   entry is left behind, and any internal partial work is
   rolled back or abandoned unpublished
   (**MA-6.2-024**; **MA-6.2-020**: no partially
   constructed block is ever returned). The same outcome
   applies, by the same gate, when the product is
   representable but exceeds any size that can be satisfied
   — the `calloc` analogue of **MA-6.1-MALLOC-04**. This
   outcome is **pre-admission**: no mapping exists, no
   descriptor exists, and the MA-6.2-024 rollback cited
   above is satisfied vacuously — there is no state to
   roll back, and the design says so rather than implying
   a cleanup step ran.
3. **Admit the product as a plain size.** Once checked, the
   product is admitted exactly as a `mimir_malloc(product)`
   request would be, including the total-mapping computation
   (usable extent + metadata reserve, in system-page units
   qualified per **MA-6.2-041**), which is itself
   overflow-checked at L2 (B-006 Section 12.1 pointer;
   Roadmap Section 9.1 Invariant 3: size and alignment
   calculations cannot overflow). Citation honesty for that
   computation: no contract row governs mapping-size
   arithmetic directly — contracts Section 1.3 puts mapping
   policy in non-scope — so its overflow outcome (treated
   as a step-4 failure, below) rests on Invariant 3 and on
   **MA-6.2-020 / -024** by analogy, and is labelled as
   such wherever it is cited.
4. **Mapping / construction failure (Defined).** If the
   mapping or any later construction step fails — including
   the zeroing step (step 5) and the total-mapping overflow
   of step 3 — the result is the **MA-6.1-CALLOC-03**
   failure: null return, no pointer to storage not
   allocated to the caller, no ownership passes. Unlike
   step 2, this outcome is **post-admission**, and it
   carries the real rollback duty: any mapping obtained,
   descriptor written, or registry change made is undone
   or abandoned unpublished per **MA-6.2-024**, so no
   half-admitted allocation, mapping, or count survives
   the failure (see also Section 3.3, failure inside
   zeroing). The error channel is target-dependent →
   **V-02**, and the `errno` guarantee is
   **[UNRESOLVED: U-10]**; this document states no
   `errno` value for any failure.
5. **Zeroing (Defined).** Zero the usable extent of the
   new block, per Section 3.3 — after the mapping succeeds,
   before publication. A zeroing failure is a step-4
   failure.
6. **Publication and return (Defined).** Publish the block
   as LIVE in the registry and return its pointer, per
   Section 3.3's moment rule. No caller or other thread
   can observe the block before this step.

Test derivation: hook **C-001** (contracts Section 6:
`calloc` overflow — request whose product overflows, or
exceeds the target maximum, fails as specified; no wrapped
size is allocated), with the named hook
**MA-CALLOC-OVERFLOW**.

### 3.3 Zeroing — extent, moment, and mechanism posture

- **Extent (Defined).** The zeroing step covers **the
  whole usable extent of the returned block** — the
  requested `count × size` product plus any rounding the
  allocator itself grants as usable storage — and on
  success **every byte of the requested extent reads as
  zero at the point of return**, suitably aligned as in
  **MA-6.1-MALLOC-01** (**MA-6.1-CALLOC-01 [NORM]**).
  Covering the usable extent satisfies both source
  wordings at once: the roadmap's "requested bytes" and
  the contract's "allocated storage" (and contracts
  Section 6's C-002 wording, "the usable allocation").
  The recorded tension (INC-B008-4) is therefore **not**
  resolved by adopting the weaker wording. The caller's
  *reliance* is still bounded by the requested extent —
  because no usable-size contract exists by which a larger
  extent could be named to the caller (Section 6.2, item
  U-7) — but reliance and construction are different
  things: the allocator zeroes the whole usable extent,
  and the caller is promised the requested part of it.
  Verification is byte-by-byte over the requested extent
  (**MA-6.1-CALLOC-05 [NORM]**; hook **C-002**, named hook
  **MA-CALLOC-ZEROING**), including sizes that are not
  multiples of the alignment; the usable-extent coverage
  beyond that is a construction property of this design,
  checked wherever a later debug facility can observe it.
- **Moment.** Zeroing is complete **before the pointer is
  returned** and before the block is published as LIVE to
  any other thread. There is no state in which a caller or
  another thread can observe a successful `calloc` block
  with non-zero bytes inside the requested extent.
- **Who writes the zeros.** `calloc` is the one Phase 3
  entry point whose design **does** write user bytes, and
  only before ownership passes. This is the exception B-006
  itself names: its lock section states the allocator
  neither reads nor writes user bytes under the lock "apart
  from no touch whatsoever — zeroing belongs to calloc,
  deferred to B-008". The boundary is temporal, not spatial:
  before return, the bytes are the allocator's work area;
  from return until free, they are the caller's alone and
  the allocator does not touch them (B-006 lock discipline,
  unchanged). See INC-B008-5 for the recorded wording
  tension this resolves.
- **Mechanism posture (design, not claim).** In the B-006
  machine every allocation is a fresh mapping, and a fresh
  anonymous mapping's pages are zero-filled by the host —
  but the design **must not rest the zeroing contract on
  that host property alone**, for two recorded reasons:
  (a) the contract is allocator-level (MA-6.1-CALLOC-01),
  and a host property is not an allocator guarantee until
  the design says the allocator checks it; (b) any later
  phase that reuses mappings (Phase 7 reuse, explicitly not
  in Phase 3) would silently break a host-property-only
  design. **The Phase 3 design decision is the explicit
  zeroing step**: zero the usable extent after the mapping
  succeeds and before publication (Section 3.2, step 5) —
  which, on a fresh mapping, writes zeros over zeros, at a
  CPU cost the design accepts for the correctness
  baseline. That is the design, not one branch of an
  undecided either/or. The only admissible alternative —
  debug-build verification plus a documented reliance
  statement naming the exact host mechanism — is a
  **reconsideration item**: it takes effect only if a
  later slice records it as a decision-ledger amendment
  replacing this paragraph; until then it has no standing.
  Silence is not admissible in any case: an implementation
  that neither zeroes nor holds such an amendment does not
  meet this design. This is a design posture, not a
  measurement; no zeroing cost figure is stated or implied.
- **Failure inside zeroing / construction.** Zeroing is a
  construction step under **MA-6.2-024**: if any part of
  construction cannot complete, nothing is published and
  the call is the MA-6.1-CALLOC-03 failure. A partially
  zeroed block is never returned.

### 3.4 Alignment

A successful `mimir_calloc` return satisfies the same
alignment as a successful `mimir_malloc` of the product
size (MA-6.1-CALLOC-01 by reference to MA-6.1-MALLOC-01).
No numeric alignment value, and no relation between the
product and any alignment unit, is stated here: **V-07 /
U-15** are open and are B-007 scope (Section 1.3). `calloc`
imposes no size-multiple precondition of its own: a product
that is not a multiple of any alignment unit is an ordinary
request, and C-002's byte-by-byte check expressly includes
such sizes.

### 3.5 Failure summary (all Defined unless noted)

| Case | Result | Contracts |
|---|---|---|
| Product overflow / unrepresentable | Null; no wrapped allocation; nothing published | MA-6.1-CALLOC-02; MA-6.2-020, -024 |
| Product representable, unsatisfiable (incl. mapping failure) | Null; no ownership passes | MA-6.1-CALLOC-03; MA-6.2-020 |
| Any multi-step construction failure | Null; internal steps rolled back / abandoned unpublished | MA-6.2-024 |
| `errno` on any failure | **No value stated** — target-dependent, unresolved | V-02; [UNRESOLVED: U-10] |

### 3.6 The zero-factor case — [UNRESOLVED: U-09], no behaviour

`calloc(0, size)`, `calloc(count, 0)`, and `calloc(0, 0)`
are **MA-6.1-CALLOC-04**, tagged [UNRESOLVED: U-09] in the
contract itself, with candidate outcomes recorded in
variation row **V-01** (return a unique freeable pointer /
return null, and their variants). B-008 does **not** choose
among the candidates, because the choice is recorded in the
contracts register as a **per-target contract decision**
(contracts Section 7, U-09 / U-02), and no per-target
decision exists at this base.

Design posture, honestly labelled as posture and not as
semantics:

- The zero-factor case is **outside the Phase 3 supported-
  call list** (Section 7.2). An integrator must not call
  `mimir_calloc` with a zero factor and rely on any result.
- A later implementation must not silently pick a candidate
  and ship it as if contracted: whichever behaviour it
  exhibits before U-09 closes per target is **uncontracted
  behaviour**, must be listed as such in its release notes,
  and is a failure of hook **C-006** (zero-size by target
  policy) for that target until the policy is selected and
  recorded.
- This posture mirrors B-006's treatment of zero-size
  `malloc` (MA-6.1-MALLOC-03, likewise U-09): zero-size is
  not on B-006's supported list either, so `calloc` is not
  made inconsistent with `malloc` by this section.

### 3.7 Clean-initialisation interaction (design)

`calloc` changes nothing in the bootstrap design, and is
bound by all of it:

- Bootstrap storage never comes from the normal path and
  initialisation never self-allocates recursively
  (**MA-6.3-002**, **MA-6.3-003**); bookkeeping that grows
  with allocations lives only in fixed / preallocated /
  bootstrap-owned storage (**MA-6.3-012**). `calloc`'s
  zeroing step needs no bookkeeping beyond the B-006
  descriptor, and must not introduce an allocator-dependent
  container on any path (**MA-6.3-010**).
- A `calloc` entered before initialisation completes, or
  reentrantly during it, is served by the bootstrap path,
  deferred by a defined mechanism, or failed through the
  defined failure channel — the **MA-6.3-001 / -004 / -005**
  outcomes, unchanged. A bootstrap-served `calloc` block
  carries normal Section 3 ownership from return, and its
  later free after full initialisation is governed by
  **MA-6.3-006**.
- The zeroing guarantee is not weakened on the bootstrap
  path: a successful bootstrap `calloc` is zeroed over its
  usable extent (requested extent guaranteed, Section 3.3)
  or the request fails. There is no
  "bootstrap blocks may be unzeroed" carve-out.
- Audit note on the row ranges cited in this section:
  contracts.md contains **no** MA-6.3-007..009 and no
  MA-6.3-013..019 rows — its Section 4 numbering runs
  001..006, 010..012, 020..023, 030..032, 040..042 — so
  the ranges cited here are the complete Section 4.1–4.3
  surface, not a silent selection from it.
- Error reporting on any `calloc` path is bounded, uses no
  unbounded dynamic allocation, and does not come from the
  failing path (**MA-6.3-020..023**) — including the
  overflow path of Section 3.2, which must be reportable
  without allocating.

### 3.8 `calloc` contract-mapping table

| Design element | Contract IDs (all Proposed; namespaced mapping by analogy, U-03) | Hook(s) |
|---|---|---|
| Success: zeroed, aligned, ownership passes | MA-6.1-CALLOC-01; MA-6.1-MALLOC-01 (alignment by reference) | C-002; MA-CALLOC-SUCCESS-ZEROED |
| Zeroing verified byte-by-byte | MA-6.1-CALLOC-05 | C-002; MA-CALLOC-ZEROING |
| Overflow detected pre-multiplication; failure; no wrap | MA-6.1-CALLOC-02; MA-6.2-020, -024 | C-001; MA-CALLOC-OVERFLOW |
| Other failure: null, nothing owed | MA-6.1-CALLOC-03; MA-6.2-020 | MA-CALLOC-FAILURE |
| Zero factor | MA-6.1-CALLOC-04 — [UNRESOLVED: U-09]; V-01 | C-006 (blocked until per-target policy) |
| Bootstrap / reentrancy / reporting bounds | MA-6.3-001..006, -010..012, -020..023 | C-010 (by reference) |

---

## 4. `mimir_realloc` — precise semantics (design)

Design label only, by analogy under U-03 (Section 1.2):
`mimir_realloc(ptr, new_size)`, corresponding to the core
candidate `realloc(ptr, size)` of Roadmap Section 6.1 —
"with preservation of the old block on allocation failure;
copy at most the smaller valid size" — contracts
Section 2.5, and the ownership cross-references of
contracts Section 3.3.

### 4.1 Preconditions for a Defined result

Defined cases require: `ptr` is null (Section 4.7) **or**
`ptr` is a live block obtained from an in-scope MimirAlloc
allocation function and not yet released, presented through
the compatible allocator (contracts **MA-6.2-001** family);
and `new_size > 0` (the zero case is unresolved,
Section 4.8). A `ptr` that is not a live in-scope block is
**MA-6.1-REALLOC-05: UB** (Section 4.9). `new_size` is
admitted through the same L2 checked gate as a fresh
allocation of that size.

### 4.2 What success means (Defined)

On success (**MA-6.1-REALLOC-01 [NORM]**):

- The caller owns a block of the new size, at the returned
  pointer, which **can differ** from the input pointer.
- The first `min(old_size, new_size)` bytes of contents are
  preserved byte-for-byte, **whether the storage moves or
  stays in place**.
- If the pointer changes, ownership of the old storage ends
  and ownership of the new storage passes at the same
  transfer point, with **no interval in which both or
  neither is owned by the caller** (**MA-6.2-023**;
  Roadmap Section 9.1 Invariant 5's family). The old pointer
  is no longer usable in any way: reading it, writing it,
  freeing it, or reallocating it again are respectively
  use-after-free / double-free-class cases under
  **MA-6.2-031 / -032** and **MA-6.1-FREE-04** (a block
  "already released by a successful `realloc` move").
- Test derivation: hook **C-004** (copy-min), named hook
  **MA-REALLOC-SUCCESS-COPY-MIN**.

Here `old_size` means the requested size of the original
allocation as recorded in its B-006 descriptor — the only
size the design can name, since no usable-size contract
exists (Section 6.2, item U-7). Preservation is stated over
that recorded size, never over mapping size or any larger
extent the caller might have written into.

Two qualifications complete the success statement:

- **Alignment of the result.** The returned block — moved
  or retained — satisfies the same alignment guarantee as
  a successful `mimir_malloc` of the new size. For a move
  this holds by construction: the new block is produced by
  the ordinary allocation path, whose guarantee is
  **MA-6.1-MALLOC-01**. For a retained block it holds
  because the block already satisfied that guarantee at
  its original allocation. MA-6.1-REALLOC-01 does not
  state alignment in its own words, so this statement is
  recorded as by-analogy under MA-6.1-MALLOC-01 (and, for
  the namespaced name, under U-03), not as a quotation of
  the realloc row.
- **The no-both / neither interval is an API-boundary
  statement.** MA-6.2-023's "no interval in which both or
  neither is owned by the caller" describes ownership as
  observable by the caller at the transfer point. The
  internal move sequence (Section 4.5) necessarily holds
  two mappings transiently — the caller's live old block
  and a complete but **unpublished** new block — under the
  lock; the unpublished block is internal partial work
  under MA-6.2-024, is not usable by, addressable by, or
  owned by the caller, and becomes the caller's at the
  same transfer step that retires the old block. Read any
  other way, the contract sentence would forbid the only
  failure-atomic move sequence that exists.

### 4.3 Grow and shrink

- **Grow (`new_size > old_size`).** The preserved range is
  the whole old requested extent (`min` = `old_size`).
  Bytes in `(old_size, new_size)` of the returned block are
  **unspecified** (**MA-6.1-REALLOC-06 [NORM]**: no
  preservation claim may be read into bytes beyond the
  preserved range; named hook **MA-REALLOC-GROW-TAIL**).
  In particular they are **not zeroed**: `realloc` is not
  `calloc`, and a caller that needs zeroed growth must
  zero it itself or use a fresh `mimir_calloc`. Any zeros
  an implementation happens to leave there (for example
  from a fresh mapping) are an accident of construction,
  not a contract, and must not be documented as a feature.
- **Shrink (`new_size < old_size`).** The preserved range
  is the first `new_size` bytes. Bytes beyond `new_size`
  in the old block cease to be the caller's at the transfer
  point, whatever physical mapping the design retains
  (Section 4.4); the caller must not read them back through
  the returned pointer, through a saved alias, or by any
  assumption that the old mapping still exists.
- **Same size (`new_size == old_size`).** Design decision,
  recorded as such: the call returns the input pointer
  unchanged, contents untouched, ownership continuous —
  a Defined success under MA-6.1-REALLOC-01 with a
  preserved range equal to the full extent. An
  implementation that instead moves a same-size block is
  also within MA-6.1-REALLOC-01, but this design chooses
  the no-move form because the B-006 machine has no reason
  to move it; the choice is a design choice, not a contract
  requirement, and no caller may rely on pointer identity
  even here (Section 6.2, item U-9).

### 4.4 In-place versus move — the honest Phase 3 account

The contract permits either outcome (MA-6.1-REALLOC-01:
preserved "whether the storage moves or stays in place").
What the **B-006 machine can actually do** is narrower, and
this section states it without dressing it up:

- **No in-place grow exists in this design.** Each block is
  exactly one mapping sized for its request plus metadata
  (B-006 L5 degenerate form). There is no adjacent-space
  mechanism, no split / coalesce machinery to extend into,
  and no `mremap`-style step anywhere in B-006. A grow
  therefore **always moves**: new mapping, copy, retire old
  (sequence in Section 4.5). Any document or header that
  implies grow-in-place, or a probability of it, misstates
  this design.
- **Shrink does not return memory.** A shrink retains the
  original mapping whole and returns the same pointer with
  a reduced recorded requested size. The design has no
  partial-unmap or trim step (B-006 Section 12.3: trim is
  Phase 7), so a shrink frees **no** system memory; it
  changes bookkeeping and the caller's usable extent only.
  **Two size records, two jobs (this closes the shrink
  bookkeeping question explicitly):** after a shrink the
  descriptor holds the *new requested size* and the
  *original mapping length* as separate records, and they
  are never substituted for each other. Preservation,
  copying, and every caller-visible guarantee use the
  requested size; a later `mimir_free` unmaps exactly the
  recorded mapping length (B-006's munmap rule: exact
  address + length), never a length derived from the
  shrunken requested size — which would be a partial
  unmap, i.e. the split this machine does not have.
  This is a deliberate correctness-baseline trade, stated
  here so no integrator budgets a shrink as memory relief.
  (Recorded tension with a naive reading of "resize":
  INC-B008-6.)
- **No pointer-identity promise in any case — at contract
  level.** The two statements above ("shrink returns the
  same pointer", "same size returns the input pointer")
  are **design-level facts about this baseline**, labelled
  as such, in exactly the same sense as the same-size
  design decision of Section 4.3; they are not contract
  promises. The contract-level statement an integrator may
  rely on is only MA-6.1-REALLOC-01 / -06, which permits a
  move in every case. A later phase with size classes and
  reuse may move blocks this design retains, and code
  written against Phase 3 pointer identity would break
  without any contract having been violated.

### 4.5 The move sequence, and failure atomicity

Failure in this section applies only where a new
allocation is required — a grow, or any case this design
moves. Shrink and same-size (Sections 4.3–4.4) allocate
nothing, return the input pointer, and **cannot** enter
the null-return failure path below; their only failure
territory is the validation case of Section 4.9, which is
UB, not a Defined failure.

A move (every grow; any other case an implementation of
this design chooses to move) executes in this order:

1. **Validate** `ptr` against the registry under the B-006
   lock (live, in-scope, not retired). Validation failure
   is the UB territory of Section 4.9 — the design states
   no recovery from it.
2. **Admit** `new_size` at L2 (checked arithmetic, B-007
   interface, Section 1.3).
3. **Construct the new block completely** — mapping,
   descriptor, registry publication — as an internal step
   the caller cannot yet observe, per **MA-6.2-024**.
4. **Copy** `min(old_size, new_size)` bytes from old to new.
5. **Transfer and retire:** publish the new block as the
   caller's, retire the old block exactly once and unmap
   it, as one ownership transfer under **MA-6.2-023**,
   observing B-006's lock discipline (the lock is held
   across the mapping syscalls in this baseline, B-006
   Section 9.4 decision, unchanged by this slice).

**Failure at any of steps 2–4 (Defined)** — unsatisfiable
size, mapping failure, any multi-step failure — is
**MA-6.1-REALLOC-02 [NORM]** / **MA-6.2-022**: the call
returns null, and **the original block remains valid,
unchanged in contents, unchanged in ownership, and freeable
by the caller exactly as before the call**. No partial move
occurs; no new block is owed; internal partial work is
rolled back or abandoned unpublished (MA-6.2-024). The
design's allocate-first ordering exists precisely to make
this outcome structural rather than a cleanup promise: the
old block is not touched, copied from destructively, or
retired until the new block is complete. Test derivation:
hook **C-003** (preserve-on-failure), named hook
**MA-REALLOC-PRESERVE-ON-FAILURE**; fault-injection
derivation in Section 8, item E-4. The error channel on
failure is target-dependent → **V-02**; `errno` is
[UNRESOLVED: U-10] and no value is stated.

### 4.6 Contents discipline (summary)

| Range | On success | On failure |
|---|---|---|
| `[0, min(old, new))` | Preserved byte-for-byte (MA-6.1-REALLOC-01) | Original wholly unchanged (MA-6.1-REALLOC-02) |
| `[min(old, new), new)` (grow tail) | Unspecified; **not** zeroed by contract (MA-6.1-REALLOC-06) | — (no new block) |
| Old block beyond `new` (shrink tail) | No longer the caller's; no read-back right | Still the caller's, as part of the unchanged original |

### 4.7 `realloc(NULL, n)` — Defined

**MA-6.1-REALLOC-03 [NORM]:** for `n > 0`,
`mimir_realloc(NULL, n)` behaves as `mimir_malloc(n)` for
the same `n`, including its success **and** failure cases:
success returns a fresh
block of `n` bytes with unspecified contents (not zeroed);
failure returns null with no ownership passing. For
`n == 0` the case inherits the malloc zero-size case and
is therefore [UNRESOLVED: U-09] by inheritance — this
section states no behaviour for `realloc(NULL, 0)`.
Named hook: **MA-REALLOC-NULL**; variance V-01, V-02.

### 4.8 `realloc(p, 0)` — [UNRESOLVED: U-12], no behaviour

For a non-null live `p`, **MA-6.1-REALLOC-04** is tagged
[UNRESOLVED: U-12], with candidate outcomes recorded in
variation row **V-04** — the contract notes the baseline
treatment "has changed across editions and is not stated
here as settled for every target". Candidates on record
include freeing `p` and returning null, freeing `p` and
returning a unique zero-size pointer, and returning null
without a settled statement of `p`'s fate. B-008 does
**not** choose, for the same reason as Section 3.6: the
register records this as a per-target decision (U-12 /
U-02), and none exists at this base.

Posture (labelled posture, not semantics): `realloc(p, 0)`
is **outside the Phase 3 supported-call list** (Section
7.2). An integrator needing to release a block uses
`mimir_free(p)` and a null result discipline of its own;
an integrator needing a possibly-zero resize must branch
on zero itself before calling. Hook **C-006** cannot pass
for a target until its U-12 policy is selected and
recorded.

### 4.9 Invalid pointers — UB, and the debug posture

**MA-6.1-REALLOC-05:** a `ptr` that does not refer to a
live allocation, or was already released (by free, or by a
successful realloc move), is **UB** — no behaviour, return
value, error channel, or side effect is stated, and there
is no test hook ("None — UB"). The same classification
covers, by their own rows: reallocating a foreign pointer
(**MA-6.2-002**), an interior pointer (**MA-6.2-030**
family), and a pointer whose provenance cannot be
established (**MA-6.2-005**).

Debug posture, exactly as contracts Section 3.4 bounds it:
in a debug build with diagnostics enabled, the case is
**Debug-detectable only** (**MA-6.2-033**, hook C-330) —
the build MAY detect and report it where detection is
possible, and on detection MUST preserve evidence and stop
or report per the host contract and MUST NOT continue as
if the block were valid. Detection does not restore
ownership, does not repair anything, and does not convert
the case to Defined in any build. A release build makes
**no** detection promise: the honest statement to
integrators is that a bad `realloc` in a release build may
corrupt the registry, crash, or appear to work, and none
of those outcomes would violate this design, because the
case is outside it.

### 4.10 Conditional `reallocarray` — gated, not provided

**MA-COND-REALLOCARRAY-01..03** are conditional rows, live
only where a target requires the symbol **and** its
semantics are defined per target (contracts Section 2.8),
with presence itself unresolved as **U-16**. The Phase 3
namespaced design provides **no** `mimir_reallocarray`:
it is Not present (Section 6.1). If a later target gate
opens it, its design is already fixed by the conditional
rows — behave as `realloc` to the checked product;
overflow is a failure with the original unchanged
(MA-COND-REALLOCARRAY-02); other failure likewise
preserves the original (-03) — and by Sections 3.2 and 4.5
of this document, which it composes and does not amend.

### 4.11 Threads, signals, fork (design)

- **Threads.** A block allocated on one thread may be
  reallocated on another: cross-thread handoff preserves
  ownership unchanged (**MA-6.2-054**, hook C-009 by
  reference). All registry steps of Section 4.5 run under
  the one B-006 lock; there is no per-thread fast path to
  reason about (L3 pass-through). No contention or speed
  property is stated.
- **Signals.** No MimirAlloc function, `mimir_realloc`
  included, is claimed async-signal-safe
  (**MA-6.3-030..032**). Calling it from a signal handler
  is outside the contract (Section 6).
- **Fork.** No fork behaviour is claimed Defined for this
  lock-holding design beyond **MA-6.3-040..041**;
  **MA-6.3-042** and **U-17..U-21** leave the child's lock
  state and first-allocation behaviour open (INC-B006-7,
  carried). `realloc` in a fork child is therefore a
  no-claim case, listed in Section 6.3.

### 4.12 `realloc` contract-mapping table

| Design element | Contract IDs (all Proposed; namespaced mapping by analogy, U-03) | Hook(s) |
|---|---|---|
| Success; copy-min preserved, move or stay | MA-6.1-REALLOC-01; MA-6.2-023 | C-004; MA-REALLOC-SUCCESS-COPY-MIN |
| Grow tail unspecified, not zeroed | MA-6.1-REALLOC-06 | MA-REALLOC-GROW-TAIL |
| Failure; original untouched and caller-owned | MA-6.1-REALLOC-02; MA-6.2-022, -020, -024 | C-003; MA-REALLOC-PRESERVE-ON-FAILURE |
| `realloc(NULL, n)` = `malloc(n)` | MA-6.1-REALLOC-03 | MA-REALLOC-NULL |
| `realloc(p, 0)` | MA-6.1-REALLOC-04 — [UNRESOLVED: U-12]; V-04 | C-006 (blocked until per-target policy) |
| Invalid / already-released / foreign pointer | MA-6.1-REALLOC-05 (UB); MA-6.2-002, -005, -030..032; debug: MA-6.2-033 | None — UB; debug C-330 |
| Cross-thread realloc | MA-6.2-054 | C-009 |

---

## 5. Combined supported-case index (design)

The complete Defined surface this slice adds, stated
positively — everything not in this table, and not in
B-006's supported list for `mimir_malloc` / `mimir_free`,
is unsupported in one of the senses of Section 6:

| Call | Defined cases |
|---|---|
| `mimir_calloc(count, size)` | Success (zeroed, aligned); overflow failure; other failure — all with `count > 0`, `size > 0` |
| `mimir_realloc(ptr, n)` | Success grow / shrink / same-size on a live in-scope `ptr`, `n > 0`; failure with original preserved; `ptr == NULL`, `n > 0`, as `malloc(n)` |

Explicitly **not** in the Defined surface, by omission and
by Section 6: both zero cases (U-09, U-12), every invalid-
pointer case, and every symbol other than the four
namespaced entry points (`mimir_malloc`, `mimir_free` from
B-006; the two here).

**Thread-safety of the supported calls themselves
(Defined, design level).** Every call in the table above
may be made concurrently from multiple threads: all
registry and mapping work serialises through the one
B-006 lock, and cross-thread handoff of a live block
preserves ownership (MA-6.2-054). That is a claim about
the *allocator's* state only. Synchronising access to the
*contents* of a block — one thread writing a block while
another frees or reallocates it, for example — is the
caller's contract, exactly as B-006's lock section states
(the lock never guards user bytes); racing payload access
against free / realloc is a caller data race and sits in
Section 6.2 territory, not in this Defined surface. The
absence of thread *caching* (N-7) is a performance
absence and must not be misread, in either direction, as
a thread-*safety* statement.

---

## 6. Unsupported behaviour — the full Phase 3 record (design)

Roadmap Phase 3 exit requires unsupported behaviour to be
documented; B-006 Section 13, items 9–10, carry that duty
into the exit package verbatim. This section is the
B-008 record. It uses four honest buckets — do not merge
them when quoting it:

- **Not present:** the symbol / facility does not exist in
  the design. The failure mode is absence (link error,
  missing symbol), never a runtime refusal.
- **Defined refusal:** the call is in-scope, the design
  states its outcome (normally null + no state change),
  and an integrator may rely on exactly that outcome.
- **UB (no refusal promised):** the call violates a
  precondition. The design states no outcome; a release
  build may crash, corrupt state, or appear to work.
  "Unsupported" here must never be read as "safely
  rejected".
- **Unresolved / no claim:** the sources leave the case
  open. The design states no outcome **and** records that
  a future per-target or per-evidence decision is owed.
  An integrator must not rely on any observed behaviour.

### 6.1 Not present — symbols, facilities, and properties the minimal allocator does not provide

This bucket has two sub-kinds, and they fail differently —
do not quote one as the other:

- **Symbols not provided** (N-1..N-6, N-8 in part): absence
  is detectable at link / lookup time; there is no runtime
  behaviour to describe.
- **Facilities and properties not provided** (N-7, N-9,
  N-10, N-11): nothing is missing at link time, because
  nothing was ever declared; the absence is a design
  statement, and an integrator detects it only by reading
  this record. Thread caching, a stable ABI, and debug
  facilities are of this kind.

Entries whose source rows are **conditional** (N-3, N-4,
N-8, and the entry-point part of N-2) are absent **from
the Phase 3 design, on every target, at this base** —
that is the design fact this bucket records. Their
conditional contract rows could bring them into a later
design only through the amendment route of Section 7.1,
item 6; until such an amendment, a target gate has not
"fired" anywhere, and listing them here is not a claim
about what a later phase may do. For N-2 the distinction
is sharper still: what is unresolved (U-11) is the
*validity semantics* an aligned entry point would have;
what is absent in Phase 3 is the *entry point itself*,
which B-006's supported list already excludes.

| # | Not provided | Source / phase that owns it later | If a caller tries it anyway |
|---|---|---|---|
| N-1 | Standard-name `malloc` / `free` / `calloc` / `realloc` replacement or any symbol interposition (`LD_PRELOAD` or link-time) | Phase 5 (ABI / interposition); B-006 Section 12.3 | The namespaced library simply does not export those names for replacement; a program linked only to it keeps its system allocator for standard names. No silent substitution occurs |
| N-2 | Every explicit-alignment entry point, by name: `mimir_aligned_alloc`, `mimir_posix_memalign`, and any namespaced form of the core candidates `aligned_alloc` / `posix_memalign` of contracts Sections 2.6–2.7 | B-007 designs alignment rules; an entry point is a later design. Contracts MA-6.1-ALIGNED-* / POSIXMA-* are core-symbol Proposed rows with U-11 open | Symbol absent. Explicit-alignment needs are unsupported in Phase 3, full stop — not "use `calloc` and hope for more alignment" |
| N-3 | `mimir_malloc_usable_size` | Conditional core candidate (MA-COND-USABLE-*), gated per target (V-05) | Symbol absent; no size query exists. Callers must track their requested sizes (see U-7) |
| N-4 | `mimir_reallocarray` | Conditional, target-gated (Section 4.10; U-16) | Symbol absent on every Phase 3 target |
| N-5 | Statistics, counters beyond B-006's two bookkeeping counters, snapshots, or any trim / release-to-OS call — the would-be names (written in this record as `mimir_stats` / `mimir_trim` purely for recognisability) are themselves **unassigned** under U-03, so there is not even a reserved name to link against | Phase 10; namespaced stats / trim themselves [UNRESOLVED: U-03]; L7 compiled out (B-006) | Symbol absent. Freed mappings are returned per B-006's free path; there is no call that forces release, and no counter an integrator may read |
| N-6 | Runtime tuning of any kind: no `mallopt` analogue, no environment-variable configuration, no cache-size / threshold / arena knobs | Runtime configuration rows MA-6.2-050..053 presuppose a configuration surface the minimal design does not have; tuning belongs to Phases 7–9 with their evidence. **Those rows are therefore marked N/A for Phase 3: no configuration surface exists, no configuration version exists, and no versioning guarantee of MA-6.2-053 is made or implied** | There is nothing to call and nothing to set. An environment variable a caller sets for its system allocator has no effect on this one, and that non-effect must not be mistaken for the variable being honoured |
| N-7 | Thread caching, per-thread / per-CPU fast paths, rseq mode, size classes, sharded registry | Phases 7, 8, 11; B-006 Sections 12.3, 9.3 (L3 pass-through by design) | Not a caller-visible symbol, but a facility integrators sometimes assume: every call takes the one-lock slow path. No tuning call can enable a cache, because none exists |
| N-8 | The C++ `operator new` / `delete` family, every form in the contracts Section 2.1 inventory: scalar and array `operator new` (throwing and nothrow), aligned `operator new` forms, and `operator delete` in its plain, array, sized, aligned, and aligned-sized forms | Conditional core candidates (MA-COND-CXX-*), only where a C++ integration requires them; Phase 5 | Symbols absent. C++ programs use their runtime's operators, whose storage does **not** come from MimirAlloc and must never be freed through it (see U-1) |
| N-9 | Debugging facilities as a shipped feature: no red zones, canaries, poisoning, quarantine, or leak checker in the baseline build | Phase 10 (debug / hardened builds) | Nothing to enable. Debug-detectable-only cases (Section 6.2) describe a possible future debug build, not a Phase 3 facility an integrator can switch on |
| N-10 | A stable ABI: no ABI version, symbol versioning, or binary-compatibility promise | Phase 5 / Phase 13; contracts Section 5.1 (no ABI version assigned) | Integrators link the design's headers as source-level, Proposed API. Any binary built against an eventual implementation carries no cross-version promise from this document |
| N-11 | **Any target claim at all — and, a fortiori, non-Linux targets.** No OS/architecture pair is claimed supported at this base: there is no implementation, so the claimed-target set is empty, on Linux exactly as on Android / bionic, musl, macOS, or Windows. ARM64 Linux and x86-64 Linux are exit-evidence obligations (Section 8, E-7), not held results | Phases 4–5; B-006 Section 12.3 | Behaviour on any target is unclaimed, not "probably the same". Linux/glibc-shaped wording in the sources is never universal (Roadmap Section 6.1 rule). Section 7.2's supported-call list is a design list for the future exit targets, and must never be quoted without this row |

### 6.2 UB — precondition violations with no refusal promised

Each row: no Defined outcome, no error channel, no
recovery. Debug-detectable only where the row says so —
and detection, where a future debug build offers it, is
stop / report, never repair (MA-6.2-033, -034; Roadmap
Section 9.2: detection is not correction, and allocator
checks are not ECC).

| # | Violation | Contracts | Honest consequence statement |
|---|---|---|---|
| U-1 | Freeing or reallocating a pointer from a **different allocator** (system `malloc`, a C++ operator, another library's allocator, a bootstrap block never served — MA-6.3-006 boundary aside). **The realistic case, named plainly:** because interposition is absent (N-1), most pointers in a real program come from the system allocator, and presenting one of those to `mimir_free` / `mimir_realloc` is this row | MA-6.2-002 (UB); MA-6.1-FREE-03; MA-6.1-REALLOC-05 | No silent treatment as its own, and no forwarding to the right allocator. Release outcome unstated: registry corruption, crash, or apparent success are all outside the contract. Interposition or mixed linkage creates **no** allowance; an allowance exists only where MA-6.2-003 documents the pairing, and Phase 3 documents none. The boundary is the allocator **family**, not the individual function: a block from any in-scope MimirAlloc entry point (`mimir_malloc`, `mimir_calloc`, a successful `mimir_realloc`) may be released by `mimir_free`, and that pairing is ordinary supported use, not a mismatch case |
| U-2 | **Double free**, including freeing a pointer already consumed by a successful `realloc` move | MA-6.1-FREE-04; MA-6.2-032; debug: MA-6.2-033 | UB in a release build; Debug-detectable only in a debug build. **Direct answer to the standing question:** the minimal allocator does **not** promise double-free detection. Its registry happens to record retired blocks at design level, which is what makes debug detection *possible*; possibility is not a promise, and a second free that arrives after the descriptor's state has been reused may not be detectable at all. Any detection a future internal debug build performs is best-effort, non-contractual, and not a Phase 3 deliverable (see also N-9) |
| U-3 | **Use after free** — read, write, or realloc of a released block; use of an old pointer after a successful realloc move | MA-6.2-031; MA-6.1-REALLOC-05 | UB. The block's sole owner is the allocator; in the mapping-per-allocation design the storage is typically unmapped, so a crash is *likely* — likelihood is an observation about the construction, not a contract, and must not be relied on as a safety property |
| U-4 | Free / realloc of an **interior pointer**, a stack / static / string-literal pointer, or any pointer not returned by an in-scope function; pointer of unestablishable provenance — which expressly includes a pointer value carried across `exec`, inherited from another process, or reconstructed from a serialised value | MA-6.1-FREE-03; MA-6.2-030, -005; MA-6.1-REALLOC-05 | UB; Debug-detectable only where range data permits (MA-6.2-033). No "free checks its argument and ignores bad ones" behaviour exists or may be inferred |
| U-5 | Writing or reading **outside a live block's requested extent**, including into the grow tail after a shrink or beyond a `calloc` extent | MA-6.2-030 family (access outside extent, contracts Section 3.4 boundary table) | UB. No bounds checking, red zone, or guard is provided in the baseline (N-9). Bytes physically present in the mapping beyond the requested extent are not the caller's |
| U-6 | Relying on **metadata layout**: reading, writing, or doing arithmetic that assumes where descriptors, registry nodes, or mapping boundaries sit | MA-6.2-034 (corrupted metadata: no correction claim) | Outside the contract entirely. Metadata is internal (contracts Section 5.2: Internal = never-ABI); corrupting it, deliberately or by overrun, yields Debug-detectable-only stop / report at best, and no ownership conclusion is drawn from corrupted metadata. Companion non-claim, stated so silence is not misread: corruption of **caller data inside a live block** is not detected at all — allocator checks are not ECC and no row in this record claims heap-content verification (contracts Section 3.4 boundary summary) |
| U-7 | Assuming a usable size **larger than requested** and using the excess (the excess physically exists: mappings are system-page-rounded) | No usable-size contract (N-3); MA-6.1-REALLOC-01 preservation is over recorded requested size only | Unsupported and unsafe to rely on: `realloc` preservation, `calloc` zeroing, and every other guarantee in this document are stated over the requested extent only. Code that uses page-rounded excess has no guarantee any byte of it survives, is zeroed, or stays mapped |
| U-8 | Calling any MimirAlloc function from a **signal handler**, or calling into the allocator from a handler to inspect / repair state after a signal interrupted it | MA-6.3-030..032 (Defined as a *prohibition*: no function is claimed async-signal-safe) | The call itself is outside the contract: no ownership operation is contracted inside a handler, and no promise is made about state observed there. This is a Defined refusal of the *claim*, with UB-class practical consequences for anyone who calls anyway |

### 6.3 Unresolved / no claim — must not be relied on

| # | Case | Register | What an integrator does instead |
|---|---|---|---|
| X-1 | Zero-size allocation in any form: `malloc(0)` analogue, `calloc` zero factor, `realloc(NULL, 0)` | U-09 / U-02; V-01; MA-6.1-CALLOC-04 | Branch on zero in caller code; treat zero as "no allocation needed" by the caller's own rule (Section 3.6) |
| X-2 | `realloc(p, 0)` for live `p` | U-12 / U-02; V-04; MA-6.1-REALLOC-04 | Call `mimir_free(p)` explicitly (Section 4.8) |
| — | *Disjointness of X-1 / X-2 (register hygiene, not a case):* U-09 covers allocation-side zero — no live block is presented. U-12 covers `realloc(p, 0)` — a live block **is** presented. The two registers are disjoint by that test; a per-target annex must close **both**, and closing one closes nothing about the other. `realloc(NULL, 0)` falls under X-1 (no live block), by the inheritance of Section 4.7 | U-09 vs U-12; U-02 is their register-level parent | — |
| X-3 | Exact `errno` value (or existence) on any allocation failure | U-10; V-02 | Test the null return only. Code that switches on `errno` from these entry points is written against an unmade promise |
| X-4a | Fork, child lock state: `fork` taken while another thread holds the allocator lock; the child's recovery / reinitialisation for that lock | U-17; MA-6.3-042 | No allocation call in such a child until a per-platform decision closes U-17. No `pthread_atfork` handler is provided by the Phase 3 design to rescue the case (a facility absence, Section 6.1 sense), and MA-6.3-040's documentation duty falls due before any future handler may be claimed |
| X-4b | Fork, first allocation in the child of a multithreaded parent; fork during bootstrap | U-18, U-21 | As X-4a. Whether bootstrap behaviour (Section 3.7) is re-entered in the child is part of what is open |
| X-4c | Fork, interaction with atfork handlers of the host or other libraries; statistics / diagnostics state carried across `fork` | U-19, U-20; MA-6.2-051 by analogy | Register nothing and assume no ordering. The parent side of `fork` itself is Defined and unaffected (MA-6.3-041): parent ownership before and after `fork` is identical |
| X-5 | Any performance property: speed, throughput, contention scaling, fragmentation bound, retained-memory bound, latency tail | Roadmap Phase 3 text (mapping-per-allocation is an explicitly experimental correctness baseline, "do not ship it as a performance allocator"); B-006 Section 12.3 | Budget nothing. The one design expectation B-006 states — poor multithread throughput behind one lock and mapping syscalls — is an expectation, not a measurement, and is quoted here only to prevent the opposite assumption |
| X-6 | In-place `realloc`, pointer identity, shrink returning memory to the OS | Section 4.4 of this document (design account, not a contract row) | Write move-tolerant caller code from the start: always use the returned pointer, never cache the old one across a call, never budget a shrink as memory relief |
| X-7 | Guarantees **beyond** the requested extent, as a reliance question only: expecting zeroing, preservation, or stability in page-rounded slack without ever touching it under U-7 | INC-B008-4; Sections 3.3, 4.2 | Track requested extents; rely on guarantees only inside them. Precedence rule, so this entry and U-7 are never quoted against each other: the **act** of using the excess is U-7 (UB, Section 6.2) and that bucket governs the act; this entry governs only expectation / reliance about bytes the caller does not use |
| X-8 | Calls after allocator teardown, during process-exit handlers, or from destructors ordered after the allocator's own teardown — and, symmetrically, any claim about that interval at all | No contract row covers the interval (contracts Sections 4.1–4.5 address entry *before* readiness, not exit after it) | Make no MimirAlloc call from exit-path code whose ordering against allocator teardown is not guaranteed by a mechanism stated in the integrator's own design. This entry records a gap in the sources (see INC-B008-8), not a behaviour |

### 6.4 Defined refusals — the complete list

For balance, the outcomes an integrator **may** rely on
when a call is refused (all Proposed / design). One rider
applies to **every** row of this table: each null refusal
is a refusal of the *allocation*, stated through the null
return alone — **errno: no claim** until U-10 closes per
target (X-3). No row in this table implies `ENOMEM` or
any other `errno` value, and none should be quoted as
POSIX-complete failure behaviour while U-10 is open.

| Refusal | Outcome relied on | Contracts |
|---|---|---|
| Oversize / overflow `calloc` or `malloc`-class request | Null; no wrapped or partial allocation; no state published | MA-6.1-CALLOC-02; MA-6.1-MALLOC-04; MA-6.2-020, -024 |
| Unsatisfiable `calloc` / `malloc` | Null; no ownership passes | MA-6.1-CALLOC-03; MA-6.1-MALLOC-02 |
| Unsatisfiable `realloc` (incl. overflow of the new size) | Null **and** the original block wholly preserved and caller-owned — preservation is guaranteed **on failure only**; movement and pointer identity on *success* are no-claim (X-6), and the two statements must never be merged into "realloc keeps your block" | MA-6.1-REALLOC-02; MA-6.2-022 |

Two former rows are removed from this table deliberately,
each to its correct home:

- **`free(NULL)` is not a refusal.** It is ordinary
  supported behaviour — a Defined no-op success
  (MA-6.1-FREE-02; MA-6.2-010) — and it is listed where
  supported behaviour belongs: B-006's list and
  Section 7.2 of this document. Calling it a refusal
  contradicted the positive-list rule of Section 7.1.
- **The signal-safety refusal is documentary only.** What
  is refused is the *claim* (MA-6.3-031: documentation and
  headers must not state or imply async-signal-safety).
  The *act* of calling from a signal handler is not a
  refusal with a safe outcome; it is governed solely by
  Section 6.2, item U-8 (UB-class consequences), and no
  row of this table may be quoted as giving it one.

Any refusal not in this table is not a designed refusal.
In particular there is **no** designed refusal for a bad
pointer: see Section 6.2's bucket definition.

---

## 7. How unsupported behaviour is documented to integrators (design)

The mechanism is part of the design, because Roadmap
Phase 3 makes the documentation itself an exit item.

### 7.1 The channels, and what each must carry

1. **The supported-call list is the primary document.**
   Support is stated **positively and exhaustively**
   (B-006 Section 13 item 1; Section 5 of this document):
   the list names each supported entry point and its
   Defined cases, and carries the sentence, in substance:
   *"Nothing else is supported."* A call not on the list
   needs no individual prohibition; its absence is the
   statement.
2. **This unsupported record ships with the exit
   evidence, verbatim.** B-006 Section 13 items 9–10
   require the NOT-done list and the documented-
   unsupported-behaviour record to be **included in the
   exit package, not dropped as an appendix**. The same
   rule binds this document: Sections 6.1–6.3 are part of
   the deliverable an integrator receives, at the same
   version as the code they describe.
3. **Header discipline.** When headers exist (a later
   implementation slice, not this one), each declared
   entry point's header comment must state its Defined
   cases and name its unresolved cases by register ID
   (U-09, U-12, U-10 at minimum for this slice's symbols),
   and must not state or imply async-signal-safety
   (**MA-6.3-031**), in-place behaviour, or any property
   in Section 6.3. A header that declares
   `mimir_calloc` / `mimir_realloc` without those notes
   does not conform to this design.
4. **No silent fallback, ever.** Where an unsupported call
   could be mistaken for a supported one, the design's
   answer is absence or the stated refusal — never a
   quiet rerouting to another allocator, whose pointer
   would later be freed by the wrong implementation
   (**MA-6.2-002**, **MA-6.2-004**: every fallback either
   preserves ownership or fails at allocation time).
5. **Per-target annexes carry the decisions this document
   leaves open.** U-09, U-12, and U-10 close **per
   target**, in that target's annex, before the target is
   claimed supported (the MA-6.2-012 pattern: silence in
   an annex is not support). Until an annex records a
   decision, the case stays in Section 6.3 for that
   target, and test hook **C-006** stands as its gate.
6. **Changes are ledger events.** Moving any item out of
   Section 6 — supporting a symbol, closing an unresolved
   case, adding a debug facility — requires the decision
   to be recorded (decision-ledger practice of this
   repository) and this document amended in the same
   change. An implementation that quietly supports more
   than the current version of this document states is a
   documentation defect, because integrators code against
   the document, not against the binary's accidents.
7. **Versioning honesty.** This document carries no ABI
   version (N-10). Each amendment states which slice made
   it and what evidence, if any, supports the change;
   design changes are labelled Proposed until the
   implementation evidence of Section 8 exists.

### 7.2 The Phase 3 supported-call list, as amended by this slice (design)

For the exit package, the supported list reads (design
level; B-006 Section 13 item 1 as its base). It is a
**design list for the future exit targets** — the named
ARM64 Linux and x86-64 Linux environments of Section 8,
E-7 — and, per N-11, **no target is claimed yet**: until
an implementation passes on a named target, this list
states what that target's implementation is designed to
support, and supports nothing anywhere yet.

- `mimir_malloc(size)` with `size > 0` and representable
  (admitted at L2; oversize = defined failure) — B-006.
- `mimir_free(ptr)` of a live in-scope pointer, or of
  NULL — B-006.
- `mimir_calloc(count, size)` with both factors > 0 and a
  representable, satisfiable product — success zeroed over
  its usable extent, requested extent guaranteed zero at
  return; overflow / unsatisfiable = defined
  failure — this slice, Section 3.
- `mimir_realloc(ptr, n)` with `ptr` live and in-scope or
  NULL, and `n > 0` — success per Section 4.2; failure
  preserves the original — this slice, Section 4.

Zero-size behaviour is not on the list (U-09). Nothing
else is on the list.

---

## 8. Phase 3 exit evidence this design must LATER satisfy (design)

**All items owed, none held.** This slice is design only;
no item below is satisfied by it, by B-006, or by B-007 /
B-009 work at this base. Roadmap Phase 3 exit, verbatim:
*"portable unit/contract suite passes for explicitly
supported calls on x86-64 and ARM64 Linux. Document
unsupported behavior."* The suite itself is B-009 scope;
this section states only the evidence this slice's
semantics add to B-006 Section 13's list:

- **E-1 (C-001):** `calloc` overflow evidence — products
  at and beyond the representability boundary fail with
  null, and no wrapped size is allocated (the allocated-
  size record, where a debug facility later provides one,
  shows no allocation at all).
- **E-2 (C-002):** `calloc` zeroing evidence — byte-by-byte
  zero over the requested extent for a size sweep
  including non-multiples of the alignment, zero-factor
  cases excluded as unresolved.
- **E-3 (C-004):** `realloc` copy-min evidence — grow,
  shrink, and move cases with pattern-filled blocks,
  verifying exactly the `min(old, new)` range and
  asserting nothing about the grow tail beyond its
  unspecified status.
- **E-4 (C-003 + Roadmap Section 9.3):** `realloc`
  preserve-on-failure evidence under fail-the-Nth-mapping
  injection swept over the move sequence's steps: at every
  injected failure the original block's contents, ownership,
  and freeability are verified unchanged, and the next
  uninjected call succeeds.
- **E-5 (C-006) — BLOCKED, honestly:** zero-size and
  `realloc(p, 0)` evidence **cannot** be produced until
  U-09 / U-12 close per target. This item stands as a
  named blocker on the Phase 3 exit for every target, not
  as a gap to be averaged away; an exit claim that omits
  it misstates the exit.
- **E-6:** the unsupported record (Section 6) and the
  supported list (Section 7.2) shipped in the exit
  package, per B-006 Section 13 items 9–10.
- **E-7:** both-target evidence — every item above on a
  named ARM64 Linux environment **and** a named x86-64
  Linux environment (B-006 Section 13 item 3). Pi-only
  evidence does not meet the roadmap wording.

Until E-1..E-7 exist as recorded evidence from an actual
implementation, the correct statement is: **Phase 3 exit
is unmet**, and `calloc` / `realloc` support is a design,
not a fact.

---

## 9. Inconsistencies recorded (design)

Recorded faithfully; recording does not resolve. INC-B006-1
(U-03 namespaced gap), INC-B006-3 (page-size tension),
INC-B006-4 (Phase 3 scope split), INC-B006-7 (fork / lock
state), INC-B006-9 (hook namespaces), and INC-B006-10 (all
sources Proposed) all carry into this slice and are not
repeated in full.

- **INC-B008-1 — B-007 NOT IN BASE.** This slice's tasking
  describes B-007's checked-arithmetic / clean-init rules
  "as designed in" the B-006 document, and at base 243f007
  that is the only form in which they exist in the
  repository: B-006 Sections 12.1 / 13 pointers plus the
  contract rows. No B-007 deliverable is present in this
  worktree or in the banked main at this base (verified by
  directory listing at slice start). Sections 1.3, 3.2,
  and 3.7 are therefore written against pointers, and a
  reconciliation check against B-007's banked text is owed
  when it lands (Section 1.3).
- **INC-B008-2 — NAMESPACED GAP EXTENDS (U-03).** As with
  `mimir_malloc` / `mimir_free` (INC-B006-1), the two new
  namespaced names have no contract rows; every citation
  in this document is by analogy. The gap now covers the
  entire Phase 3 namespaced surface, which strengthens,
  but does not change, the U-03 scope decision owed.
- **INC-B008-3 — UNRESOLVED CASES SIT INSIDE THE PHASE 3
  EXIT.** The roadmap's Phase 3 text says "add calloc and
  realloc only with precise semantics", yet the contracts
  leave two of their cases — the zero factor (U-09) and
  `realloc(p, 0)` (U-12) — unresolved with per-target
  decisions owed, and hook C-006 cannot pass without them.
  "Precise semantics" for Phase 3 therefore cannot mean
  "every case settled"; this document's reading — precise
  about what is settled, explicit about what is not, and
  the unsettled cases named as exit blockers (E-5) — is
  recorded as a reading, not presented as the roadmap's
  own words.
- **INC-B008-4 — ZEROING EXTENT WORDING.** Roadmap
  Section 6.1 guarantees zeroing of "the requested bytes";
  MA-6.1-CALLOC-01 requires every byte of "the allocated
  storage" to be zero. In a page-rounded, mapping-per-
  allocation design those extents differ materially, and
  no usable-size contract (N-3) exists by which the larger
  extent could be promised to a caller. Section 3.3
  resolves the construction question in favour of the
  stronger wording — the zeroing step covers the whole
  usable extent — while bounding the caller's *reliance*
  to the requested extent, the only extent C-002's hook
  can check and the only one a caller can name. Recorded
  here because the two source wordings are genuinely
  different statements, and a reader comparing them
  against either source alone should find the divergence
  acknowledged, not hidden.
- **INC-B008-5 — "NO TOUCH" VERSUS ZEROING.** B-006's lock
  section states the allocator neither reads nor writes
  user bytes under the lock, with zeroing named as
  calloc's deferred exception, without stating the
  temporal boundary. Section 3.3 supplies it (allocator
  writes only before ownership passes; never after). The
  boundary is this slice's clarification of B-006, flagged
  here so a reader of B-006 alone is not left with an
  apparent contradiction.
- **INC-B008-6 — SHRINK WITHOUT MEMORY RETURN.** A caller
  reading "resize" in Roadmap Section 6.1 could reasonably
  expect a shrink to release memory; the B-006 machine
  (no split / trim, Phase 7) cannot do so, and Section 4.4
  states the retention plainly. The tension is between the
  roadmap's brevity and the minimal design's poverty, and
  it is resolved in favour of stating the poverty.
- **INC-B008-7 — NO DELIVERABLE FILENAME WAS NAMED** for
  B-008 in WAVE_PLAN.md or in the keeper's tasking (the
  INC-B006-2 pattern repeating). The filename of this
  document — `docs/PHASE_3_CALLOC_REALLOC_AND_UNSUPPORTED_BEHAVIOR.md`
  — is this slice's choice, following the B-006 naming
  pattern, and is recorded here so the keeper can correct
  it at banking if a different name was intended.

- **INC-B008-8 — TEARDOWN INTERVAL NOT COVERED.** The
  contract sources specify entry before readiness in
  detail (MA-6.3-001..006) and say nothing about calls
  after teardown or from exit-path code. Section 6.3,
  item X-8 records the resulting no-claim; whether the
  interval deserves contract rows of its own is a
  contracts-amendment question, not this slice's to
  settle by silence in either direction.

**Crew review record (this slice, pre-commit).** Three
builder reviews were run against this design before
commit. Defects found and fixed in-slice, none left
standing: calloc — the zero-factor gate is now step 0 of
Section 3.2 (it was reachable through admission); the
zeroing extent now covers the usable extent (Section
3.3) instead of adopting the weaker source wording; the
zeroing mechanism is a decided explicit step, not an
either/or; rollback is stated per branch (pre-admission
vacuous, post-admission real); the total-mapping
overflow citation is labelled by-analogy. realloc —
shrink now records mapping length separately from
requested size (Section 4.4); pointer-identity statements
are scoped as design-level, contract promise none;
`realloc(NULL, n)` is scoped to `n > 0` wherever the
equation appears; MA-6.2-023 is qualified as an
API-boundary statement; the failure path is scoped to
allocating cases; result alignment is stated by analogy
to MA-6.1-MALLOC-01. Unsupported record — the signal
case and `free(NULL)` were removed from the Defined-
refusals table to their correct homes; every null
refusal now carries the errno no-claim rider;
MA-6.2-050..053 are marked N/A for Phase 3; conditional
not-present entries state their gate; the claimed-target
set is stated empty; foreign-pointer, cross-process,
mismatch-pairing, teardown, and caller-data-corruption
entries were added or cross-referenced; fork and
zero-size entries were split / made disjoint. The
tester pass over the pre-amendment draft (T1–T6) is
recorded in the slice's keeper handoff; the amended
document was re-verified by the lead before commit.

---

## Appendix A. Sources (design)

- Roadmap: `docs/MimirAlloc_Research_and_Development_Roadmap.md`
  — Section 6.1 (core candidates; `calloc` overflow +
  zeroing, `realloc` preservation), Section 6.2 (ownership),
  Section 6.3 (allocation-free startup), Section 7.1–7.3
  (layers; threading posture), Phase 3 text and exit
  (Section 8), Section 9.1 Invariants 1, 3, 5, Section 9.2
  (detection vs correction), Section 9.3 (fault injection),
  Section 10 (C-contracts row).
- Contracts: `docs/contracts.md` (A-008) — Sections 1.3,
  1.6, 2.4 (`MA-6.1-CALLOC-01..05`), 2.5
  (`MA-6.1-REALLOC-01..06`), 2.8
  (`MA-COND-REALLOCARRAY-01..04`), 3.1–3.6
  (`MA-6.2-001..054`), 4.1–4.5 (`MA-6.3-001..042`),
  Section 6 hooks C-001..C-006, Section 7 register
  U-02, U-03, U-09, U-10, U-12, U-16, U-17..U-21;
  variation rows V-01, V-02, V-04. All Proposed.
- B-006 design: `docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md`
  — layer form (L1–L7, Phase 3 degenerate), lock discipline
  and the calloc zeroing exception (its Section 9.2 in
  Part 2), Sections 12.1–12.3 (deferred lists), 13 (exit
  evidence items 1, 3, 9, 10), 14 (INC-B006-1..10),
  Appendices A–B.
- Assignment: `hidden_files/ASSIGNMENT.md` (team-08,
  B-008), as written at slice start.
- WAVE_PLAN.md (keeper bookkeeping, Pi main checkout) —
  Wave 2 queue line for B-008; B-006 banking note at
  243f007.

---

*End of B-008. Design only; Proposed. No implementation,
no measurement, and no Phase 3 exit claim is made anywhere
in this document.*
