# MimirAlloc — Core API / ABI Contract Specification

## Document control

| Field | Value |
|---|---|
| Deliverable | A-008 — contract-first API/ABI specification |
| Roadmap source | Roadmap §6 (Contract-first API and ABI work), with supporting material from §§2, 5, 9, 10, 13, 15 |
| Status | Proposed — specification only |
| Implementation status | No implementation exists; none is claimed |
| ABI version | No ABI version assigned — see §5.1 |
| Runtime-configuration version | `<unassigned>` — see §§3.6, 5.3 |

> [PROPOSED] This document proposes contracts for review. No row in it
> describes existing behaviour, and no row is evidence that any symbol
> is implemented, tested, integrated, or measured. Contract
> identifiers are stable identifiers for review, discussion, and
> later test derivation; stability of an identifier does not imply
> adoption of the contract it names.

---

## 1. Scope, basis, and reading rules

### 1.1 Purpose

This specification is contract-first. Its purpose is to state, before
any implementation work: what each candidate core allocation symbol
is proposed to do; under what preconditions; what result follows on
success and, separately, on failure; which error channel carries a
failure; what ownership and side effects follow from a call; how a
case is classified when a precondition is broken; where behaviour is
proposed to vary by target; and which questions remain unresolved
and therefore carry no contract.

A reader is intended to be able to review a proposed contract without
reading allocator internals, compare it against a named baseline,
identify every target-dependent point by its variation identifier
(V-nn), identify every open question by its unresolved identifier
(U-nn), and derive a test hook from a defined case without inventing
semantics for an undefined or unresolved case.

### 1.2 Scope

The scope of §2 is limited to the core API candidates named in
Roadmap §6.1: `malloc`, `free`, `calloc`, `realloc`, `aligned_alloc`,
`posix_memalign`, plus — conditional only — `reallocarray`,
`malloc_usable_size`, and the C++ `operator new` / `operator delete`
variant family with each variant treated separately. §§3–4 cover
Roadmap §§6.2–6.3. §§5–7 are supporting sections (ABI stability,
conformance mapping, unresolved register); they introduce no new
API requirement.

For each in-scope symbol, this document addresses only: the callable
signature as a contract surface; preconditions and named edge cases;
required behaviour in tagged normative rows; return value on success
and on failure, stated separately; the error channel, where one is
proposed; ownership transfer and other caller-visible side effects;
classification of defined, undefined, and debug-detectable cases;
target variance by V-nn reference; and a test hook, where a defined
case admits one.

### 1.3 Non-scope

This document does not specify: allocator internals of any kind;
data structures, arenas, bins, lists, or size classes; locking or
synchronisation design; system-call or platform-service usage; page
management or mapping policy; growth, shrink, coalescing, splitting,
or reuse policy; tracing, logging, or statistics facilities except
where a named §6.1 symbol itself is such a facility; debugging or
sanitiser implementations; performance targets or measurements;
symbols named only outside Roadmap §6.1; or a C++ runtime design.
Silence in this document is absence of a contract, not a default
contract borrowed from any baseline in §1.6. A topic in non-scope is
not resolved by silence. The namespaced `mimir_*` API proposed in
Roadmap §7.1 is outside §6 scope and is recorded as U-03, not
specified here.

### 1.4 Evidence labels

Evidence labels are taken from Roadmap §2. A label describes the
evidence held for a claim; it does not strengthen the claim itself.

| Label | Meaning when applied to a claim |
|---|---|
| Proposed | Stated for review; no implementation evidence is attached |
| Implemented | An implementation is recorded as existing |
| Host-tested | Behaviour is recorded as tested on a host environment |
| Device-tested | Behaviour is recorded as tested on a target device |
| Integration-proven | Behaviour is recorded as proven in an integrated system |
| Performance-measured | A performance figure is recorded as measured |
| Blocked | Progress on the claim is recorded as blocked |

Every contract in this document carries the label **Proposed** and
no other. A baseline behaviour recorded in §2.11 is a statement about
that baseline, not evidence about MimirAlloc. A test hook identifier
is a pointer to a proposed test location, not a record that a test
exists or has run.

### 1.5 Normative language and notation

[NORM] The key words "MUST", "MUST NOT", "REQUIRED", "SHALL",
"SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED", "MAY", and
"OPTIONAL" are to be interpreted as described in RFC 2119 only when
they appear in a statement tagged [NORM] or [COND: gate], and only
within the scope of that statement. Lowercase "must", "should", and
"may" in untagged prose are ordinary English. Column headings such
as "Required behaviour" are structural labels, not normative
statements.

| Tag | Meaning | Normative force |
|---|---|---|
| [NORM] | Unconditional normative statement | RFC 2119 capitals apply inside the statement |
| [COND: gate] | Normative statement active only when the named gate holds | RFC 2119 capitals apply only inside the condition |
| [UNRESOLVED: U-nn] | Open question recorded by identifier | No normative verb; no contract is stated |
| [PROPOSED] | Proposal, explanation, or structural statement | No RFC 2119 force |

`MA-` begins a contract identifier for a core symbol; `MA-COND-`
for a conditional symbol or variant. `V-nn` identifies a
target-variation row in §2.11. `U-nn` identifies an unresolved item
in the register in §7. "Target-dependent → V-nn" means no single
value is proposed for all targets and §2.11 records the variation.
"None — UB" as a test hook means no behavioural test hook is
proposed because the case is classified as undefined behaviour.
Test-hook identifiers are not uniform across sections in this issue:
§2 uses named proposed hooks, §§3–4 use `C-3nn` / `C-4nn` slots, and
§6 assigns `C-001`…`C-010` to the Roadmap §10 C-contracts mapping.
Normalising the hook namespace is editorial follow-up work, recorded
here so no reader mistakes the three schemes for one catalogue.

### 1.6 Classification vocabulary

Each contract row carries exactly one classification:

| Classification | Meaning |
|---|---|
| Defined | A behaviour is proposed for the case |
| UB | Undefined behaviour; no behaviour is proposed for the case |
| Debug-detectable only | No portable behaviour is proposed; detection is proposed only as a possible debug-mode outcome |

A row classified UB proposes no return value, no error value, and no
side effect. A Debug-detectable only row does not upgrade a UB case
to Defined in any build configuration, and a debug detection is not
a recovery contract unless a separate Defined row states one.
Classification is per case, not per function.

### 1.7 Baselines cited as variation sources only

The baselines cited in §2.11 are POSIX (baseline interface
description), glibc, musl, and bionic (individual C library
implementations) — cited solely as sources of recorded variation.
No baseline is adopted wholesale; no baseline is a default when
MimirAlloc is silent; no baseline behaviour is presented as
universal. In particular, glibc behaviour is recorded as glibc
behaviour only, never as the behaviour of C libraries in general
(Roadmap §6.1 states this rule explicitly). Where the exact
behaviour of a baseline for a named target is not certain, §2.11
records "To be verified against target documentation — UNRESOLVED"
rather than asserting a value. The roadmap cites only S5 (Linux
`malloc` manual) and S6 (POSIX `posix_memalign`) for this section;
that source base is insufficient for the non-glibc columns and the
gap is recorded as U-04.

---

## 2. Core API candidates [Roadmap §6.1]

Contract tables use these columns throughout: Contract ID; Case /
precondition; Required behaviour; Return value, with success and
failure in separate rows; `errno` / error channel, or
"Target-dependent → V-nn"; Ownership / side effects; Classification;
Target variance; and Test hook.

### 2.1 Inventory

| Symbol | Status tag | Contract section |
|---|---|---|
| `malloc` | [PROPOSED] | §2.2 |
| `free` | [PROPOSED] | §2.3 |
| `calloc` | [PROPOSED] | §2.4 |
| `realloc` | [PROPOSED] | §2.5 |
| `aligned_alloc` | [PROPOSED] | §2.6 |
| `posix_memalign` | [PROPOSED] | §2.7 |
| `reallocarray` | [COND: target requires the symbol and its semantics are defined per target] | §2.8 |
| `malloc_usable_size` | [COND: target requires the symbol and its semantics are defined per target] | §2.9 |
| C++ `operator new(std::size_t)` | [COND: C++ integration requires the variant for the target platform] | §2.10 |
| C++ `operator new(std::size_t, const std::nothrow_t&)` | [COND: C++ integration requires the variant for the target platform] | §2.10 |
| C++ `operator new[](std::size_t)` | [COND: C++ integration requires the variant for the target platform] | §2.10 |
| C++ `operator new[](std::size_t, const std::nothrow_t&)` | [COND: C++ integration requires the variant for the target platform] | §2.10 |
| C++ aligned `operator new(std::size_t, std::align_val_t)` | [COND: C++ integration requires the variant for the target platform] | §2.10 |
| C++ aligned `operator new[](std::size_t, std::align_val_t)` | [COND: C++ integration requires the variant for the target platform] | §2.10 |
| C++ `operator delete(void*)` | [COND: C++ integration requires the variant for the target platform] | §2.10 |
| C++ `operator delete[](void*)` | [COND: C++ integration requires the variant for the target platform] | §2.10 |
| C++ sized `operator delete(void*, std::size_t)` | [COND: C++ integration requires the variant for the target platform] | §2.10 |
| C++ sized `operator delete[](void*, std::size_t)` | [COND: C++ integration requires the variant for the target platform] | §2.10 |
| C++ aligned `operator delete(void*, std::align_val_t)` | [COND: C++ integration requires the variant for the target platform] | §2.10 |
| C++ aligned sized `operator delete(void*, std::size_t, std::align_val_t)` | [COND: C++ integration requires the variant for the target platform] | §2.10 |

Presence of a symbol in this inventory is candidacy only. A
conditional symbol or variant acquires a contract only through the
conditional rows in its own subsection. The final required symbol
set is itself open — Roadmap §15 Q4 — and is recorded as U-01.

### 2.2 `malloc`

Base allocation function. No conditional gate applies.

| Contract ID | Case / precondition | Required behaviour | Return value | errno / error channel | Ownership / side effects | Classification | Target variance | Test hook |
|---|---|---|---|---|---|---|---|---|
| MA-6.1-MALLOC-01 [NORM] | Success; requested size is greater than zero and an allocation can be satisfied | The allocation MUST be suitably aligned for any object type with a fundamental alignment requirement, and its storage MUST be distinct from every other live allocation | Success: non-null pointer to the allocated storage | Not applicable on success | Ownership of the storage passes to the caller; no other live allocation is disturbed | Defined | V-07 | MA-MALLOC-SUCCESS |
| MA-6.1-MALLOC-02 [NORM] | Failure; an allocation cannot be satisfied | The call MUST NOT return a pointer to storage that is not allocated to the caller | Failure: null pointer | Target-dependent → V-02 | No ownership passes; no live allocation is disturbed | Defined | V-02 | MA-MALLOC-FAILURE |
| MA-6.1-MALLOC-03 | Zero-size request; size equals zero | [UNRESOLVED: U-09] No behaviour is stated here; the candidate outcomes are recorded in V-01 | [UNRESOLVED: U-09] | [UNRESOLVED: U-09] | [UNRESOLVED: U-09] | [UNRESOLVED: U-09] | V-01 | [UNRESOLVED: U-09] |
| MA-6.1-MALLOC-04 [NORM] | Requested size exceeds any size that can be represented or satisfied | The call MUST be treated as a failure case under MA-6.1-MALLOC-02 | Failure: null pointer | Target-dependent → V-02 | No ownership passes | Defined | V-02 | MA-MALLOC-OVERSIZE |

Contents of newly allocated storage are not specified by any row in
this subsection.

### 2.3 `free`

Base release function, paired with allocations returned by the
allocation functions in this section. The ownership rules governing
this contract are in §3. No conditional gate applies.

| Contract ID | Case / precondition | Required behaviour | Return value | errno / error channel | Ownership / side effects | Classification | Target variance | Test hook |
|---|---|---|---|---|---|---|---|---|
| MA-6.1-FREE-01 [NORM] | Success; argument is a pointer previously returned by an in-scope allocation function and not yet released | The storage MUST cease to be owned by the caller and MUST become eligible for reuse | Success: no value returned | Not applicable | Ownership ends; the caller retains no right to access the storage | Defined | None | MA-FREE-SUCCESS |
| MA-6.1-FREE-02 [NORM] | Argument is a null pointer | The call MUST have no effect | Success: no value returned | Not applicable | No ownership change; no side effect | Defined | None | MA-FREE-NULL |
| MA-6.1-FREE-03 | Argument was not returned by an in-scope allocation function, or is an interior pointer | No behaviour is stated | No return value is stated | No error channel is stated | No side effect is stated | UB | None | None — UB |
| MA-6.1-FREE-04 | Argument was already released by `free` or by a successful `realloc` move | No behaviour is stated | No return value is stated | No error channel is stated | No side effect is stated | Debug-detectable only | None | None — UB |

### 2.4 `calloc`

Allocation with a two-factor size and zeroing. No conditional gate
applies.

| Contract ID | Case / precondition | Required behaviour | Return value | errno / error channel | Ownership / side effects | Classification | Target variance | Test hook |
|---|---|---|---|---|---|---|---|---|
| MA-6.1-CALLOC-01 [NORM] | Success; count and size are such that their product can be satisfied | Every byte of the allocated storage MUST be set to zero before the pointer is returned, and the storage MUST be suitably aligned as in MA-6.1-MALLOC-01 | Success: non-null pointer to zeroed storage | Not applicable on success | Ownership of the storage passes to the caller | Defined | V-07 | MA-CALLOC-SUCCESS-ZEROED |
| MA-6.1-CALLOC-02 [NORM] | Overflow; the product of count and size cannot be represented | The overflow MUST be detected before multiplication is relied on for an allocation size, and the call MUST be treated as a failure | Failure: null pointer | Target-dependent → V-02 | No ownership passes; no allocation of a wrapped-around size occurs | Defined | V-02 | MA-CALLOC-OVERFLOW |
| MA-6.1-CALLOC-03 [NORM] | Failure other than overflow; the represented product cannot be satisfied | The call MUST NOT return a pointer to storage that is not allocated to the caller | Failure: null pointer | Target-dependent → V-02 | No ownership passes | Defined | V-02 | MA-CALLOC-FAILURE |
| MA-6.1-CALLOC-04 | Zero factor; count equals zero, or size equals zero, or both | [UNRESOLVED: U-09] No behaviour is stated here; zero-size outcomes are recorded in V-01 | [UNRESOLVED: U-09] | [UNRESOLVED: U-09] | [UNRESOLVED: U-09] | [UNRESOLVED: U-09] | V-01 | [UNRESOLVED: U-09] |
| MA-6.1-CALLOC-05 [NORM] | Zeroing check; storage returned under MA-6.1-CALLOC-01 is inspected byte by byte | Every inspected byte MUST read as zero at the point of return | Success: as in MA-6.1-CALLOC-01 | Not applicable | No additional side effect | Defined | None | MA-CALLOC-ZEROING |

### 2.5 `realloc`

Resize of an existing allocation, with preservation of contents and
preservation of the original allocation on failure. No conditional
gate applies.

| Contract ID | Case / precondition | Required behaviour | Return value | errno / error channel | Ownership / side effects | Classification | Target variance | Test hook |
|---|---|---|---|---|---|---|---|---|
| MA-6.1-REALLOC-01 [NORM] | Success; pointer refers to a live allocation and the new size can be satisfied | The first min(old size, new size) bytes of contents MUST be preserved, whether the storage moves or stays in place | Success: pointer to storage of the new size, which can differ from the input pointer | Not applicable on success | If the pointer changes, ownership of the old storage ends and ownership of the new storage passes to the caller; the old pointer is no longer usable | Defined | None | MA-REALLOC-SUCCESS-COPY-MIN |
| MA-6.1-REALLOC-02 [NORM] | Failure; the new size cannot be satisfied | The original allocation MUST be left unchanged and still owned by the caller | Failure: null pointer | Target-dependent → V-02 | No ownership change; the original pointer remains usable with its original size and contents | Defined | V-02 | MA-REALLOC-PRESERVE-ON-FAILURE |
| MA-6.1-REALLOC-03 [NORM] | Null first argument; `realloc(NULL, size)` | The call MUST behave as `malloc(size)` for the same size, including its success and failure cases | Success or failure: as in §2.2 for that size | Target-dependent → V-02 | As in §2.2 | Defined | V-01, V-02 | MA-REALLOC-NULL |
| MA-6.1-REALLOC-04 | Zero new size; `realloc(p, 0)` with a non-null live pointer p | [UNRESOLVED: U-12] No behaviour is stated here; the candidate outcomes are recorded in V-04 | [UNRESOLVED: U-12] | [UNRESOLVED: U-12] | [UNRESOLVED: U-12] | [UNRESOLVED: U-12] | V-04 | [UNRESOLVED: U-12] |
| MA-6.1-REALLOC-05 | Pointer does not refer to a live allocation, or was already released | No behaviour is stated | No return value is stated | No error channel is stated | No side effect is stated | UB | None | None — UB |
| MA-6.1-REALLOC-06 [NORM] | Content beyond min(old size, new size) after a successful grow | No preservation claim MUST be read into bytes beyond the preserved range | Success: as in MA-6.1-REALLOC-01 | Not applicable | Contents beyond the preserved range are unspecified | Defined | None | MA-REALLOC-GROW-TAIL |

### 2.6 `aligned_alloc`

Allocation with an explicit alignment argument. Validity of the
alignment argument and any size-multiple rule are target-specific
and are recorded in V-03.

| Contract ID | Case / precondition | Required behaviour | Return value | errno / error channel | Ownership / side effects | Classification | Target variance | Test hook |
|---|---|---|---|---|---|---|---|---|
| MA-6.1-ALIGNED-01 [NORM] | Success; alignment is valid for the target under V-03 and the size can be satisfied | The returned pointer MUST be aligned to the requested alignment, and the storage MUST be releasable by `free` | Success: non-null pointer aligned as requested | Not applicable on success | Ownership of the storage passes to the caller | Defined | V-03, V-07 | MA-ALIGNED-SUCCESS |
| MA-6.1-ALIGNED-02 [NORM] | Failure; a valid request cannot be satisfied | The call MUST NOT return a pointer to storage that is not allocated to the caller | Failure: null pointer | Target-dependent → V-02 | No ownership passes | Defined | V-02, V-03 | MA-ALIGNED-FAILURE |
| MA-6.1-ALIGNED-03 | Alignment validity; alignment is not a value accepted as valid for the target under V-03 | [UNRESOLVED: U-11] No behaviour is stated here, because validity itself is target-specific in V-03 | [UNRESOLVED: U-11] | [UNRESOLVED: U-11] | [UNRESOLVED: U-11] | [UNRESOLVED: U-11] | V-03 | [UNRESOLVED: U-11] |
| MA-6.1-ALIGNED-04 | Size-multiple rule; size is not a multiple of alignment where the target applies such a rule | [UNRESOLVED: U-11] No behaviour is stated here; the rule and its consequence are recorded in V-03 | [UNRESOLVED: U-11] | [UNRESOLVED: U-11] | [UNRESOLVED: U-11] | [UNRESOLVED: U-11] | V-03 | [UNRESOLVED: U-11] |
| MA-6.1-ALIGNED-05 | Zero-size request with an otherwise valid alignment | [UNRESOLVED: U-09] No behaviour is stated here; zero-size outcomes are recorded in V-01 | [UNRESOLVED: U-09] | [UNRESOLVED: U-09] | [UNRESOLVED: U-09] | [UNRESOLVED: U-09] | V-01, V-03 | [UNRESOLVED: U-09] |

### 2.7 `posix_memalign`

Allocation with an explicit alignment argument and an error model
distinct from the null-pointer model of §2.6. The error model is
recorded in V-06.

| Contract ID | Case / precondition | Required behaviour | Return value | errno / error channel | Ownership / side effects | Classification | Target variance | Test hook |
|---|---|---|---|---|---|---|---|---|
| MA-6.1-POSIXMA-01 [NORM] | Success; alignment is valid for the target under V-03 and the size can be satisfied | The pointer stored through the output parameter MUST be aligned to the requested alignment, and the storage MUST be releasable by `free` | Success: return value zero, with the allocated pointer stored through the output parameter | Error channel is the return value, per V-06; `errno` is not the proposed channel for this symbol | Ownership of the storage passes to the caller | Defined | V-03, V-06, V-07 | MA-POSIXMA-SUCCESS |
| MA-6.1-POSIXMA-02 [NORM] | Failure; a valid request cannot be satisfied | The value stored through the output parameter MUST be left unchanged on failure | Failure: a non-zero error number as recorded in V-06, separate row from the success value zero | Error number is carried by the return value, per V-06 | No ownership passes; caller storage named by the output parameter is unchanged | Defined | V-06 | MA-POSIXMA-FAILURE-UNCHANGED |
| MA-6.1-POSIXMA-03 | Alignment validity; alignment is not a value accepted as valid for the target under V-03, including any target rule on relation to the size of a pointer | [UNRESOLVED: U-11] No behaviour is stated here, because validity itself is target-specific in V-03; the corresponding error number, if any, belongs to V-06 | [UNRESOLVED: U-11] | [UNRESOLVED: U-11] | [UNRESOLVED: U-11] | [UNRESOLVED: U-11] | V-03, V-06 | [UNRESOLVED: U-11] |
| MA-6.1-POSIXMA-04 | Zero-size request with an otherwise valid alignment | [UNRESOLVED: U-09] No behaviour is stated here; zero-size outcomes are recorded in V-01 | [UNRESOLVED: U-09] | [UNRESOLVED: U-09] | [UNRESOLVED: U-09] | [UNRESOLVED: U-09] | V-01, V-06 | [UNRESOLVED: U-09] |
| MA-6.1-POSIXMA-05 | Output parameter is a null pointer | No behaviour is stated | No return value is stated | No error channel is stated | No side effect is stated | UB | None | None — UB |

### 2.8 [CONDITIONAL] `reallocarray`

This subsection states no unconditional contract. The symbol is a
candidate only under its gate, and only with semantics defined per
target.

| Contract ID | Case / precondition | Required behaviour | Return value | errno / error channel | Ownership / side effects | Classification | Target variance | Test hook |
|---|---|---|---|---|---|---|---|---|
| MA-COND-REALLOCARRAY-01 [COND: target requires `reallocarray` and its semantics are defined per target in §2.11] | Success; pointer refers to a live allocation or is null, and the product of count and size can be represented and satisfied | The call MUST behave as `realloc` to the represented product size, the overflow MUST be checked before multiplication is relied on, and contents MUST be preserved as in MA-6.1-REALLOC-01 | Success: pointer to storage of the product size | Target-dependent → V-02 | As in MA-6.1-REALLOC-01 | Defined | V-02 | MA-COND-REALLOCARRAY-SUCCESS |
| MA-COND-REALLOCARRAY-02 [COND: target requires `reallocarray` and its semantics are defined per target in §2.11] | Overflow; the product of count and size cannot be represented | The call MUST be treated as a failure, and the original allocation, if any, MUST be left unchanged | Failure: null pointer | Target-dependent → V-02 | No ownership change to any original allocation | Defined | V-02 | MA-COND-REALLOCARRAY-OVERFLOW |
| MA-COND-REALLOCARRAY-03 [COND: target requires `reallocarray` and its semantics are defined per target in §2.11] | Failure other than overflow | The original allocation, if any, MUST be left unchanged, as in MA-6.1-REALLOC-02 | Failure: null pointer | Target-dependent → V-02 | No ownership change | Defined | V-02 | MA-COND-REALLOCARRAY-PRESERVE |
| MA-COND-REALLOCARRAY-04 | Gate does not hold for the target | [UNRESOLVED: U-16] No presence, behaviour, or linkage claim is stated for a target outside the gate | [UNRESOLVED: U-16] | [UNRESOLVED: U-16] | [UNRESOLVED: U-16] | [UNRESOLVED: U-16] | None | [UNRESOLVED: U-16] |

### 2.9 [CONDITIONAL] `malloc_usable_size`

This subsection states no unconditional contract. The symbol is a
candidate only under its gate, and only with presence and semantics
defined per target in V-05.

| Contract ID | Case / precondition | Required behaviour | Return value | errno / error channel | Ownership / side effects | Classification | Target variance | Test hook |
|---|---|---|---|---|---|---|---|---|
| MA-COND-USABLE-01 [COND: target requires `malloc_usable_size` and its semantics are defined per target in V-05] | Success; argument is a pointer to a live allocation obtained from an in-scope allocation function | The reported size MUST be the usable size of that allocation as defined for the target in V-05, and the call MUST NOT modify the allocation | Success: usable size of the named allocation | No error channel is proposed for the success case | No ownership change; no modification of the allocation | Defined | V-05 | MA-COND-USABLE-SUCCESS |
| MA-COND-USABLE-02 [COND: target requires `malloc_usable_size` and its semantics are defined per target in V-05] | Null argument | The result, if one is defined for the target, MUST be the result recorded for the null case in V-05 | Success or failure: as recorded in V-05 for the target | Target-dependent → V-05 | No ownership change | Defined | V-05 | MA-COND-USABLE-NULL |
| MA-COND-USABLE-03 | Argument is not a pointer to a live allocation from an in-scope allocation function, or was already released | No behaviour is stated | No return value is stated | No error channel is stated | No side effect is stated | UB | V-05 | None — UB |
| MA-COND-USABLE-04 | Gate does not hold for the target, or semantics are not defined per target | [UNRESOLVED: U-13] No presence or semantics claim is stated; presence and semantics are recorded in V-05 | [UNRESOLVED: U-13] | [UNRESOLVED: U-13] | [UNRESOLVED: U-13] | [UNRESOLVED: U-13] | V-05 | [UNRESOLVED: U-13] |

A reported usable size, where the gate holds, is not by itself a
proposal that a caller can safely write beyond the size originally
requested; any such proposal would require its own row.

### 2.10 [CONDITIONAL] C++ `operator new` / `operator delete` variant family

This subsection states no unconditional contract. Each variant is a
separate conditional row; adoption of one variant proposes nothing
about any other variant. Per-platform ABI rules and exception rules
govern this family where the gate holds; no platform rule is stated
here as universal. The family-level decision is recorded as U-06.

| Contract ID | Variant / case | Required behaviour | Return value | errno / error channel | Ownership / side effects | Classification | Target variance | Test hook |
|---|---|---|---|---|---|---|---|---|
| MA-COND-CXX-01 [COND: C++ integration requires this variant for the target platform] | Throwing `operator new(std::size_t)`; success | The variant MUST return suitably aligned storage of the requested size, under the platform ABI rules that apply to the variant | Success: non-null pointer | Error channel is the platform exception channel, not `errno`, under the gate | Ownership passes to the caller under C++ rules for the variant | Defined | V-07 | MA-COND-CXX-NEW-SUCCESS |
| MA-COND-CXX-02 [COND: C++ integration requires this variant for the target platform] | Throwing `operator new(std::size_t)`; failure | The variant MUST NOT return a null pointer as an ordinary failure result; failure is reported through the platform exception channel recorded for the target | Failure: no return value; control leaves by exception under the platform rules recorded for the target | Platform exception channel; the specific exception type is [UNRESOLVED: U-06] in this document | No ownership passes | Defined | None | MA-COND-CXX-NEW-FAILURE |
| MA-COND-CXX-03 [COND: C++ integration requires this variant for the target platform] | Nothrow `operator new(std::size_t, const std::nothrow_t&)`; failure | The variant MUST report failure by its return value and MUST NOT report that failure by leaving through the exception channel | Failure: null pointer | Return value is the error channel under the gate; `errno` is target-dependent → V-02 | No ownership passes | Defined | V-02 | MA-COND-CXX-NEW-NOTHROW-FAILURE |
| MA-COND-CXX-04 [COND: C++ integration requires this variant for the target platform] | Array `operator new[](std::size_t)`, throwing and nothrow forms treated as separate integrations of this row by variant signature | Each form MUST follow the failure model of its own form, throwing as in MA-COND-CXX-02 or nothrow as in MA-COND-CXX-03 | Success: non-null pointer; failure: as in the corresponding form row | As in the corresponding form row | Ownership passes on success only; any array bookkeeping is a platform and language matter, not an allocator contract here | Defined | V-07 | MA-COND-CXX-NEW-ARRAY |
| MA-COND-CXX-05 [COND: C++ integration requires this variant for the target platform] | Aligned `operator new(std::size_t, std::align_val_t)` and aligned `operator new[](std::size_t, std::align_val_t)`, each as a separate variant | The returned pointer for the adopted variant MUST satisfy the alignment argument supplied to that variant, and validity of that alignment MUST be judged under the target rules in V-03 | Success: non-null pointer; failure: by the exception or nothrow model of the exact variant signature, as recorded for the target | Platform channel of the exact variant; `errno` is not proposed as that channel | Ownership passes on success only | Defined | V-03, V-07 | MA-COND-CXX-NEW-ALIGNED |
| MA-COND-CXX-06 [COND: C++ integration requires this variant for the target platform] | `operator delete(void*)` and `operator delete[](void*)`, each as a separate variant; argument is a pointer obtained from the corresponding new form, or a null pointer | A null argument MUST have no effect, and a non-null argument MUST end ownership of the named storage | Success: no value returned | No error channel is proposed | Ownership ends for a non-null argument; no effect for a null argument | Defined | None | MA-COND-CXX-DELETE |
| MA-COND-CXX-07 [COND: C++ integration requires this variant for the target platform] | Sized `operator delete(void*, std::size_t)` and sized `operator delete[](void*, std::size_t)`, each as a separate variant | The size argument MUST be treated as the size supplied by the language for the corresponding allocation, and the release MUST have the same ownership effect as MA-COND-CXX-06 | Success: no value returned | No error channel is proposed | Ownership ends for a non-null pointer argument | Defined | None | MA-COND-CXX-DELETE-SIZED |
| MA-COND-CXX-08 [COND: C++ integration requires this variant for the target platform] | Aligned `operator delete(void*, std::align_val_t)` and aligned sized `operator delete(void*, std::size_t, std::align_val_t)`, each as a separate variant | The release MUST be matched to the aligned allocation form that produced the pointer; a mismatched pairing is not given a behaviour by this row | Success: no value returned | No error channel is proposed | Ownership ends for a correctly matched non-null pointer | Defined | V-03 | MA-COND-CXX-DELETE-ALIGNED |
| MA-COND-CXX-09 | Any variant in this family; pointer, size, or alignment argument does not match the allocation or variant that produced it | No behaviour is stated | No return value is stated | No error channel is stated | No side effect is stated | UB | None | None — UB |
| MA-COND-CXX-10 | Gate does not hold for a named variant on the target | [UNRESOLVED: U-06] No presence, ABI, linkage, or exception claim is stated for that variant on that target | [UNRESOLVED: U-06] | [UNRESOLVED: U-06] | [UNRESOLVED: U-06] | [UNRESOLVED: U-06] | None | [UNRESOLVED: U-06] |

### 2.11 Target-specific variation table

This table records variation by dimension. A baseline entry is a
statement about that baseline only. The chosen-contract column
either names a contract identifier or records the point as
unresolved. No row adopts a baseline entry, and no row treats a
glibc entry as a rule for musl, bionic, POSIX, or MimirAlloc.
Verification of a baseline entry against target documentation is
future work and is not recorded as done here (see U-04).

| ID | Behaviour dimension | POSIX baseline | glibc | musl | bionic | MimirAlloc chosen contract | Status |
|---|---|---|---|---|---|---|---|
| V-01 | Zero-size allocation: `malloc(0)`, `calloc` with a zero factor (`realloc` size handling is in V-04) | Baseline descriptions differ in wording on whether a unique pointer or a null pointer results | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | UNRESOLVED → U-09 (see also U-02) | Proposed |
| V-02 | `errno` on allocation failure | An error indication by `errno` is part of the baseline description for several allocation functions | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | UNRESOLVED → U-10; failure return values are in MA-6.1-MALLOC-02 and the corresponding failure rows | Proposed |
| V-03 | Alignment validity and any size-multiple rule for `aligned_alloc` and `posix_memalign` | Baseline wording requires a valid alignment and, for some interfaces, a size that is a multiple of the alignment | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | UNRESOLVED → U-11; success alignment is in MA-6.1-ALIGNED-01 and MA-6.1-POSIXMA-01 | Proposed |
| V-04 | `realloc(p, 0)` for a non-null live pointer p | Baseline treatment has changed across editions and is not stated here as settled for every target | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | UNRESOLVED → U-12 (see also U-02) | Proposed |
| V-05 | `malloc_usable_size` presence and semantics, including the null-argument case and any claim about writing beyond the requested size | Not a baseline-standardised interface in the same sense as the core functions in §§2.2–2.7 | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | UNRESOLVED → U-13, except under the gate in MA-COND-USABLE-01 and MA-COND-USABLE-02 | Proposed |
| V-06 | `posix_memalign` error model: return-value error number, effect on the output parameter, and whether `errno` is involved | Baseline model returns an error number and leaves the output value unchanged on failure | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | MA-6.1-POSIXMA-01 and MA-6.1-POSIXMA-02 for the return-value model; the exact error numbers are UNRESOLVED → U-14 | Proposed |
| V-07 | Default alignment and page-size note: alignment guaranteed for an ordinary allocation, and any relation to a target page size | A fundamental-alignment guarantee is part of the baseline description; a specific numeric value is not stated here as universal | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | To be verified against target documentation — UNRESOLVED | UNRESOLVED → U-15 for any numeric value; the non-numeric alignment claim is in MA-6.1-MALLOC-01 | Proposed |

### 2.12 Pointer to the unresolved register

Open questions referenced in §2 are collected in the register in §7.
Identifiers referenced from §2: U-09 (zero-size behaviour, V-01);
U-10 (`errno` guarantee on allocation failure, V-02); U-11
(alignment validity and size-multiple rule, V-03); U-12
(`realloc(p, 0)` behaviour, V-04); U-13 (`malloc_usable_size`
presence and semantics, V-05); U-14 (exact `posix_memalign` error
numbers, V-06); U-15 (numeric default alignment and page-size
relation, V-07); U-06 (C++ variant presence, per-platform ABI rules,
and exception rules); U-16 (presence of `reallocarray` on a target
outside its gate); and U-01 / U-02 at register level (final symbol
set; per-target zero-size and `realloc(p,0)` selection). Until a
register entry is closed by an adopted contract row, the
corresponding case in §2 remains without a contract.

---

## 3. Ownership rules [Roadmap §6.2]

[PROPOSED] This section states the ownership contract for all storage
obtained from, returned to, or transferred through MimirAlloc.
Where a statement restates an invariant from Roadmap §9.1, the
restatement is identified in the case name and does not create a
second, divergent rule. Reading rules for the tables in §§3–4:
"Defined", "UB", and "Debug-detectable only" carry the meanings in
§1.6; a test hook of the form `C-###` names a conformance check slot
(see §1.5 on hook namespaces).

### 3.1 Compatible-allocator freeing only

[NORM]

| ID | Case | Required behaviour | Ownership / side effects | Classification | Test hook |
|---|---|---|---|---|---|
| MA-6.2-001 | Free of a live allocation through the compatible allocator and API family that produced it | The implementation MUST accept the pointer and retire the allocation exactly once | Ownership returns to the allocator; the caller's ownership ends at the call | Defined | C-301 |
| MA-6.2-002 | Free of a pointer through a different allocator implementation | The implementation MUST NOT silently treat the pointer as its own; the call is outside the contract unless rule MA-6.2-003 applies | No transfer occurs that the contract recognises; no silent fallback to a different allocator is permitted at free time | UB | None — UB |
| MA-6.2-003 | Cross-allocator free where an ownership record or API explicitly allows that pairing | The implementation MUST follow the explicit allowance for that pairing and no other | Ownership passes only along the documented pairing; all other pairings remain under MA-6.2-002 | Defined | C-302 |
| MA-6.2-004 | Allocation obtained from a fallback path, later presented for free | Every fallback MUST preserve ownership, or explicitly fail at allocation time; a fallback MUST NOT produce a pointer whose eventual free would be routed to the wrong implementation | The producing path records, by construction or by explicit metadata, which allocator owns the block | Defined | C-303 |
| MA-6.2-005 | Pointer whose provenance cannot be established | The implementation is not required to diagnose the pointer in a release build | Provenance is the caller's responsibility at the point of free | UB | None — UB |

Notes: the prohibition in MA-6.2-002 covers the indirect hazard as
well as the direct one — an allocator that guesses, probes, or
forwards a foreign pointer creates a pointer that could later be
freed by the wrong implementation a second time. Interposition,
replacement of a global allocator symbol, and mixed linkage do not
by themselves create an allowance under MA-6.2-003; the allowance is
explicit or it does not exist.

### 3.2 Freeing a null pointer

[NORM] [COND: per-target ABI profile selected]

| ID | Case | Required behaviour | Ownership / side effects | Classification | Test hook |
|---|---|---|---|---|---|
| MA-6.2-010 | `free(NULL)` under the C-compatible ABI profile | The call MUST be a no-op and MUST return normally | No ownership exists, none is transferred, and no allocator state changes | Defined | C-310 |
| MA-6.2-011 | Null passed to a typed or sized deallocation entry point under the C-compatible ABI profile | The call MUST be a no-op unless that entry point's own contract row states a different per-target definition | No ownership transfer; no counter, cache, or statistics change is attributed to the call | Defined | C-311 |
| MA-6.2-012 | Null passed to an entry point on a target whose ABI profile is not the C-compatible profile | The behaviour follows the definition recorded for that target's ABI profile, and that definition MUST be stated in the target's contract annex before the target is claimed as supported | Per the target annex; no cross-target assumption is valid | Defined | C-312 |

Per-target definition note (non-normative): null-free behaviour is
defined per chosen ABI, not assumed from C by analogy. A target
annex that is silent on MA-6.2-012 has not defined the case, and
silence is not support.

### 3.3 Allocation failure and input ownership

[NORM]

| ID | Case | Required behaviour | Ownership / side effects | Classification | Test hook |
|---|---|---|---|---|---|
| MA-6.2-020 | Allocation request cannot be satisfied | The call MUST report failure through the entry point's defined failure channel and MUST NOT return a partially constructed block | All inputs remain owned by the caller; no partial transfer of any input, buffer, or descriptor occurs | Defined | C-320 |
| MA-6.2-021 | Allocation request with caller-supplied input storage fails | The implementation MUST leave the input storage and its contents under caller ownership, unmodified except where the entry point's own row explicitly states otherwise | No input is consumed, retained, or freed on the failure path | Defined | C-321 |
| MA-6.2-022 | Failed `realloc` (cross-reference: reallocation contract in §2.5) | The original block MUST remain valid, unchanged in ownership, and freeable by the caller exactly as before the call | Ownership of the original block does not move; no new block is owed to the caller | Defined | C-322 |
| MA-6.2-023 | Successful `realloc` (cross-reference only) | Ownership of the original block ends and ownership of the returned block begins, with no interval in which both or neither is owned by the caller | Exactly one valid owner at every point in the transfer (§9.1 restatement) | Defined | C-323 |
| MA-6.2-024 | Failure inside a multi-step allocation operation | The implementation MUST roll back or abandon its own internal steps without publishing a result, and MUST NOT leave a half-owned block reachable by the caller | Internal partial work remains internal; nothing is transferred | Defined | C-324 |

### 3.4 The UB / debug-detection boundary

[NORM]

| ID | Case | Required behaviour | Ownership / side effects | Classification | Test hook |
|---|---|---|---|---|---|
| MA-6.2-030 | Invalid pointer presented for free or access | No recovery, diagnosis, or defined outcome is required | Ownership state is outside the contract from the point of violation | UB | None — UB |
| MA-6.2-031 | Use-after-free | No recovery, diagnosis, or defined outcome is required | The block has exactly one valid owner — the allocator — and caller use after free violates that ownership (§9.1 restatement) | UB | None — UB |
| MA-6.2-032 | Double free | No recovery, diagnosis, or defined outcome is required in a release build | The second free has no ownership to transfer | UB | None — UB |
| MA-6.2-033 | Detection of MA-6.2-030 to MA-6.2-032 in a debug build [COND: debug diagnostics enabled] | A debug build MAY detect and report the case where detection is possible; on detection it MUST preserve available evidence and stop or report in accordance with the host contract, and MUST NOT attempt to continue as if the block were valid | Detection does not restore ownership, repair the block, or convert the case to Defined | Debug-detectable only | C-330 |
| MA-6.2-034 | Corruption of allocator metadata found by an internal check | The implementation MUST NOT claim correction of the corruption; it MUST preserve evidence and stop or report in accordance with the host contract | No ownership conclusion is drawn from corrupted metadata | Debug-detectable only | C-331 |

Boundary summary (Roadmap §9.2 alignment):

| UB — ordinary C requires no recovery | Debug-detectable only — never a release promise |
|---|---|
| Invalid pointer | Double free, where heap state still permits detection |
| Use-after-free | Use-after-free, where lifetime tracking is present in the debug build |
| Double free (release build) | Invalid pointer, where the pointer falls in a tracked range |
| Access outside the extent of a live block | Metadata inconsistency found by a debug consistency check |
| Free through an incompatible allocator (MA-6.2-002) | Free of an interior pointer, where range data permits detection |

Detection cannot safely recover every corruption: a check that
fires has found a symptom, and the state that produced the symptom
is not trustworthy enough to repair in place. The policy on
detection is therefore preserve evidence and stop or report per the
host contract — not resume, not retry, and not silently quarantine
and continue. Allocator checks are not ECC; they do not detect, and
are never described as detecting, arbitrary corruption of caller
data inside a live block.

### 3.5 Terminology — definitions box

> **Internal logical page** — the allocator's own unit of internal
> bookkeeping and subdivision. Its size is a property of the
> allocator design. It is not required to equal any operating-system
> quantity, and no rule in this specification may be read as equating
> it to one.
>
> **System page size** — the page size reported by the host operating
> system for the running target. It is a property of the host, is
> obtained from the host, and may differ between targets running the
> same allocator build.
>
> **Span** — a contiguous run of internal logical pages managed as a
> single unit by the allocator. A span is an allocator concept; it
> is not a host mapping, even when a span happens to coincide with
> one.
>
> **Huge page** — a host-provided mapping using a page size larger
> than the system page size, obtained through a host mechanism. A
> huge page is a property of a mapping, not of allocator
> bookkeeping; a span backed by a huge page remains a span, counted
> in internal logical pages.

[NORM]

| ID | Case | Required behaviour | Ownership / side effects | Classification | Test hook |
|---|---|---|---|---|---|
| MA-6.2-040 | Any use of the four terms above in this specification, in diagnostics, or in statistics output | The terms MUST be used with exactly the meanings in the definitions box, identically in every section | None; this is a naming rule | Defined | C-340 |
| MA-6.2-041 | A size, count, or limit expressed in pages without qualification | The quantity MUST be restated with its term qualified — internal logical page or system page size — before it is treated as a contract value | None | Defined | C-341 |

### 3.6 Thread-safe, versioned runtime configuration

[NORM]

| ID | Case | Required behaviour | Ownership / side effects | Classification | Test hook |
|---|---|---|---|---|---|
| MA-6.2-050 | Runtime change to a setting that affects caching behaviour | The change MUST be applied thread-safely: concurrent allocation, free, and reallocation on other threads MUST observe either the old or the new setting in full, never a mixture | No ownership of any live block changes as a result of the change | Defined | C-350 |
| MA-6.2-051 | Runtime change to a setting that affects statistics collection | The change MUST be applied thread-safely, and counters collected under different settings MUST remain attributable to the setting in force when they were collected | Statistics carry, directly or by association, the configuration version under which they were gathered | Defined | C-351 |
| MA-6.2-052 | Runtime change to a setting that affects diagnostics | The change MUST be applied thread-safely and MUST NOT alter the classification of any case in §3.4 | Diagnostic verbosity or routing may change; the UB / debug-detectable boundary does not | Defined | C-352 |
| MA-6.2-053 | Reading the current configuration | A reader MUST be able to obtain the configuration together with its version identifier as one consistent observation | None | Defined | C-353 |
| MA-6.2-054 | Cross-thread handoff of a live block (§9.1 restatement) | Handoff between threads MUST preserve ownership unchanged: the receiving thread holds exactly the ownership the sending thread held, and non-overlapping storage for distinct live blocks MUST be maintained throughout | Ownership is thread-agnostic; handoff is not a transfer to the allocator and not a new allocation | Defined | C-354 |

Versioning note: every configuration state carries a version
identifier. The identifier's format and assignment authority are
`<unassigned>` in this revision; the format is settled in §5.3
(cross-reference), and no value, scheme, or example in this section
is to be read as pre-empting §5.3.

---

## 4. Allocation-free startup and diagnostics [Roadmap §6.3]

[PROPOSED] This section states the contract for the interval before
the allocator is fully ready, and for the diagnostics that must work
inside that interval and inside failure paths.

### 4.1 Bootstrap and reentrancy

The allocator can be entered before `main`: from dynamic loader
activity, from constructors of other components, and from logging or
runtime code that itself allocates. The bootstrap path is therefore
a contract surface of its own, not an implementation detail.

[NORM]

| ID | Case | Required behaviour | Ownership / side effects | Classification | Test hook |
|---|---|---|---|---|---|
| MA-6.3-001 | First entry before initialisation has run | The entry point MUST serve the request from the minimal bootstrap path, or fail it through its defined failure channel; it MUST NOT assume fully initialised state | Any block served on the bootstrap path carries normal §3 ownership from the moment it is returned | Defined | C-401 |
| MA-6.3-002 | Bootstrap initialisation itself needs storage | Bootstrap MUST NOT depend on the not-yet-ready allocator: initialisation MUST be completable using storage that does not come from the allocator's normal path | No bootstrap block is owed to, or reclaimable through, the normal path unless a later rule explicitly migrates it | Defined | C-402 |
| MA-6.3-003 | Self-allocation inside initialisation | A design in which initialisation allocates through itself in a way that can recurse indefinitely MUST NOT be shipped as conforming; initialisation MUST terminate without requiring a prior completed initialisation | None | Defined | C-403 |
| MA-6.3-004 | Reentrant entry while initialisation is in progress on the same thread | The reentrant call MUST be served by the bootstrap path, deferred by a defined mechanism, or failed through the defined failure channel; it MUST NOT re-enter the in-progress initialisation as if it were complete | Ownership of any block already handed out by the outer call is unaffected by the inner call | Defined | C-404 |
| MA-6.3-005 | Entry from a constructor or loader context before `main` | The entry point MUST behave as in MA-6.3-001; availability of hosted runtime services MUST NOT be assumed | None beyond MA-6.3-001 | Defined | C-405 |
| MA-6.3-006 | Free, after full initialisation, of a block served on the bootstrap path | The free MUST be accepted by the compatible allocator under §3.1, or the bootstrap path MUST have failed the original request instead of serving it | Bootstrap origin is not a second allocator for §3.1 purposes unless the block was never served | Defined | C-406 |

### 4.2 Initialisation constraint — no allocator-dependent containers

[NORM]

| ID | Case | Required behaviour | Ownership / side effects | Classification | Test hook |
|---|---|---|---|---|---|
| MA-6.3-010 | Use, during initialisation, of a C++ container or utility whose storage comes from the allocator being initialised | Such use MUST NOT occur on the initialisation path | No container on the initialisation path holds, owns, or frees allocator storage | Defined | C-410 |
| MA-6.3-011 | Static or global object whose construction allocates through MimirAlloc, with construction ordered against allocator initialisation | The design MUST NOT rely on that construction running after allocator initialisation unless the ordering is guaranteed by a mechanism stated in the design, not by observed link order | None | Defined | C-411 |
| MA-6.3-012 | Initialisation bookkeeping that grows with the number of allocations or threads | The bookkeeping MUST be held in fixed, preallocated, or bootstrap-owned storage, not in an allocator-dependent container | Bookkeeping storage is bootstrap-owned for the life of the process or until an explicit, specified migration | Defined | C-412 |

Non-normative note: the constraint is on dependence, not on
language. Fixed-size tables, intrusive lists threaded through
bootstrap-owned storage, and equivalent non-allocating structures
raise no issue under this subsection.

### 4.3 Bounded error reporting

[NORM]

| ID | Case | Required behaviour | Ownership / side effects | Classification | Test hook |
|---|---|---|---|---|---|
| MA-6.3-020 | Reporting any error, diagnostic, or detection under §3.4 | Reporting MUST NOT perform unbounded dynamic allocation; the reporting path MUST be completable within a stated bound on storage that does not come from the failing allocation path | Reporting owns no caller block and frees none; it does not borrow storage owned by the reported operation | Defined | C-420 |
| MA-6.3-021 | Error reporting while the allocator is in a failed or corrupted state | Reporting MUST NOT depend on the health of the state it is reporting on; if the full report cannot be produced within the bound, a truncated report within the bound MUST be produced instead of no report or an unbounded one | Evidence preserved under §3.4 is not discarded in order to format a longer report | Defined | C-421 |
| MA-6.3-022 | Error reporting on the bootstrap path, before full initialisation | The bound in MA-6.3-020 MUST hold on the bootstrap path as well; reporting MUST use bootstrap-available facilities only | None | Defined | C-422 |
| MA-6.3-023 | Repeated or cascading errors | Reporting of later errors MUST remain within the same bound per report and MUST NOT accumulate retained storage without limit across reports | Retained diagnostic state is itself bounded | Defined | C-423 |

Bound statement (contract, not mechanism): for each report, the
storage consumed by producing that report is finite, fixed in
advance of the failure, and independent of the size of the failing
request, the size of the heap, and the number of prior reports. Any
mechanism — static buffers, caller-supplied buffers, preallocated
rings, or equivalents — is conforming if and only if it meets that
statement.

### 4.4 Signal-handler policy

[NORM]

| ID | Case | Required behaviour | Ownership / side effects | Classification | Test hook |
|---|---|---|---|---|---|
| MA-6.3-030 | Any MimirAlloc function called from a signal handler | No function is claimed as async-signal-safe; a caller MUST NOT rely on async-signal-safety for any function unless that function has a separate, individual proof and a contract row stating the claim | None; no ownership operation is contracted inside a handler | Defined | C-430 |
| MA-6.3-031 | Documentation, headers, or diagnostics describing a function | Async-signal-safety MUST NOT be stated, implied, or suggested by omission for any function without the separate proof required by MA-6.3-030 | None | Defined | C-431 |
| MA-6.3-032 | A signal arriving while an allocator lock is held or heap state is in transition | The contract makes no promise about allocator state observable from the handler; handler code MUST NOT call into the allocator to inspect or repair that state | State in transition remains owned by the interrupted operation | Defined | C-432 |

Narrowly specified policy (complete statement): the policy is
exhaustively the three rows above. There is no general
signal-handler mode, no handler-safe subset, and no deferred claim.
A future claim for any single named function arrives only as a new
contract row with its proof referenced. The open specifics are
recorded as U-05.

### 4.5 Fork and `pthread_atfork` policy

[NORM]

| ID | Case | Required behaviour | Ownership / side effects | Classification | Test hook |
|---|---|---|---|---|---|
| MA-6.3-040 | Registration of prepare / parent / child handlers through `pthread_atfork` | If handlers are registered, their registration and their intended lock discipline MUST be documented as part of the design before any fork behaviour is claimed as Defined | Registration itself transfers no ownership of any block | Defined | C-440 |
| MA-6.3-041 | Allocation or free in the parent immediately before `fork` | Such calls follow §3 unchanged; `fork` preparation MUST NOT invalidate, move, or change the ownership of any live block in the parent | Parent ownership after `fork` is exactly parent ownership before it | Defined | C-441 |
| MA-6.3-042 | Claim of any fork-related behaviour not covered by MA-6.3-040 or MA-6.3-041 | The behaviour MUST NOT be documented or relied on as Defined until it is moved out of the unresolved list below by per-platform evidence | None | Defined | C-442 |

Open behaviours — recorded as open, with no behaviour asserted for
them in this revision (all under register entry U-05):

- [UNRESOLVED: U-17] State of allocator locks in the child when
  `fork` is called from one thread of a multithreaded parent while
  another thread holds an allocator lock; the child's recovery,
  reinitialisation, or failure behaviour for that case is open.
- [UNRESOLVED: U-18] First allocation in the child of a
  multithreaded parent, including whether bootstrap-path behaviour
  under §4.1 is re-entered in the child.
- [UNRESOLVED: U-19] Interaction of any registered atfork handlers
  with handlers registered by the host application or by other
  libraries, including ordering effects.
- [UNRESOLVED: U-20] Behaviour of statistics and diagnostics state
  carried across `fork`, including attribution of counters gathered
  in the parent to reports made in the child (see also MA-6.2-051).
- [UNRESOLVED: U-21] Fork occurring during bootstrap, before
  initialisation has completed in the parent.

Resolution gate: each item above closes only on evidence from
watchdog-guarded tests run on every supported platform, per the
Roadmap §6.3 requirement — a test that hangs is a result the
watchdog records, not a gap to be argued around. Until an item
closes, platform documentation repeats the item as open alongside
any fork guidance, and no default behaviour is inferred from another
platform's result.

---

## 5. ABI stability and versioning

> **[PROPOSED] — in full.**
>
> No ABI version number is assigned by this document, none is
> implied, and no stability guarantee is in force. Nothing here
> describes an existing implementation or an existing ABI.

This section is a proposal for how stability *would* be governed if,
and only if, an ABI is later defined, audited, and promoted under
§5.5. Until that promotion occurs, no symbol carries a stability
class, and no caller may rely on binary compatibility between any
two builds, revisions, or targets.

### 5.1 Current position

`[PROPOSED]`

1. There is no implementation, no exported symbol set in force, no
   public header in force, and no ABI to be stable or unstable.
2. There is no ABI version. There is no runtime-configuration
   version. Both identifiers are `<unassigned>` (§5.3).
3. No document, roadmap section, test, or prototype may be cited as
   evidence that an ABI exists, that a symbol is exported, or that a
   calling convention has been selected.
4. The contract rows in §§2–4 define *proposed* behavioural
   obligations for a future implementation. They do not, by
   themselves, create an ABI.
5. Any future claim of stability requires the promotion gate in §5.5
   to have been met and recorded. Absent that record, the correct
   description remains: **Proposed — no ABI.**

### 5.2 ABI surface definition

`[PROPOSED]` If an ABI is later promoted, its surface *would*
consist of exactly the following, and nothing else:

| Surface element | Would count as ABI | Notes |
|---|---|---|
| Exported symbol set | Yes | The final, enumerated list of exported symbols. Its content is [UNRESOLVED: U-01]. Until settled, there is no surface to version. |
| Calling convention | Yes | Per-target calling convention, argument/result registers, stack alignment, and preservation rules for each exported symbol, once selected per target. |
| Public types and opaque handles | Yes | Any type, struct layout, enum value, or opaque handle exposed in a public header. Opaque handles are ABI by identity and lifetime rules, not by internal layout. |
| Runtime-configuration version | Yes — separately versioned | See §5.3(b). Versioned independently of the ABI version. |
| Error model surface | Yes | The `errno` values, return codes, and failure indications specified as [NORM] or [COND: …] in §§2–4, insofar as they are observable by a caller. |
| Ownership and lifetime rules | Yes | Rules stated in §§2–4. A change to such a rule is a breaking change (§5.4). |

The following are proposed **never-ABI**: internal (non-exported)
symbols; page, span, arena, bin, and size-class internals (except
where §§2–4 expressly make an observable behaviour [NORM]); debug,
tracing, and statistics internals in their internal form (a future
public statistics API, if separately contracted — see U-03 — would
require its own surface definition); and build-internal macros or
header implementation details not exposed as public types. No
element is added to the ABI surface by implication, by prototype
use, or by appearing in a test hook (§6).

### 5.3 Versioning rules

`[PROPOSED]` Two things are versioned separately. Neither is
assigned a value by this document.

**(a) ABI version** — Identifier: `<unassigned>`. Scope: the surface
defined in §5.2, excluding the runtime-configuration version. A
value is assigned only at promotion under §5.5. No placeholder value
resembling a release — in particular not `1`, `v1`, or `1.0` — is
used anywhere in this document.

**(b) Runtime-configuration version** — Identifier: `<unassigned>`.
Scope: the format, names, semantics, and compatibility of runtime
configuration. Basis: Roadmap §6.2 requires runtime-configuration
changes to be thread-safe and versioned (§3.6). Accordingly, a
version identifier and a compatibility rule for configuration MUST
exist before any configuration mechanism is claimed conformant; that
obligation is recorded here without assigning the identifier, its
format, or its compatibility rule. The configuration version is
independent of the ABI version unless a future audited rule
expressly links them.

**Mechanism** — [UNRESOLVED: U-07]. The mechanism by which either
version is expressed — for example symbol versioning, SONAME, a
configuration field, a query function, or a combination — is
unresolved. No mechanism is selected, preferred, or implied, and no
normative statement in §5 depends on a particular mechanism.

### 5.4 Change classes

`[PROPOSED]` definitions, for future use only. No change has
occurred, because there is no prior ABI to change from. Where a
proposed change could fall in more than one class, the most severe
applicable class governs.

| Class | Proposed definition | Proposed version consequence |
|---|---|---|
| **Additive** | Adds a new exported symbol, public type, configuration item, or optional capability without altering the signature, semantics, error model, or ownership rule of any existing item, and without removing anything. | Minor increment of the applicable version, when versions exist. |
| **Compatible** | Changes internal behaviour, performance characteristics, or never-ABI internals (§5.2) in a way that preserves every [NORM] row, every applicable [COND: …] row, every signature, and every ownership rule. | Patch increment of the applicable version, when versions exist. |
| **Breaking** | Any change to a [NORM] row; any change to an applicable [COND: …] row's condition or obligation; any change to a signature, public type layout, enum value, error model, or ownership/lifetime rule; any removal or renaming of an exported symbol; any change to a per-target selected semantics where that selection is part of the documented ABI for that target. | Major increment of the applicable version **and**, as a future obligation, an entry in the register (§7) at proposal time and a record in the ABI audit (§5.5). No breaking change is effected by editing this document alone. |

### 5.5 Promotion / audit gate

`[PROPOSED]` Status **Proposed** may be changed to **Stable** for a
defined surface only when **both** of the following are satisfied
and recorded:

1. **Symbol-set evidence.** Phase 1 evidence settles the final
   exported symbol set, including which conditional symbols are
   present per target — Roadmap §15 Q4. This closes U-01 for that
   surface.
2. **ABI audit.** A Phase 5 / Roadmap §13 ABI audit is completed
   against the settled surface, the selected per-target semantics,
   and this contract, with its findings recorded.

Until both conditions are met: no symbol is Stable, Provisional, or
Internal; no ABI version or configuration version is assigned; and
this document remains Proposed in full.

**Stability classes — defined for future use, applied to no symbol
yet:**

| Class | Future meaning |
|---|---|
| **Stable** | Part of the promoted ABI surface (§5.2). Subject to §5.4 in full. Breaking changes require a major increment. |
| **Provisional** | Exported or publicly exposed by a future decision, but expressly excluded from stability guarantees for a stated period or until a stated gate. Must be labelled Provisional wherever listed. |
| **Internal** | Never-ABI (§5.2). Not exported as contract; no stability guarantee; not callable by conforming users. |

Appendix A records every candidate symbol with stability class
**Unassigned**, which is the only correct class at this stage.

---

## 6. Conformance and test hooks

This section maps contract rows to test hooks. It does not create
new contract obligations. Where §6 and §§2–4 differ, §§2–4 govern.

**Mapping rule.** Every `[NORM]` row and every applicable
`[COND: …]` row in §§2–4 shall be mapped to at least one hook in
Table 6-1, or be expressly recorded in §7 as unmapped with a reason.
A row whose behaviour is undefined behaviour (UB) is not mapped to a
conformance hook: it is recorded as `No test hook — UB` (§6.2).

**Roadmap §10 basis.** Roadmap §10, C-contracts row, requires checks
covering, in its words: *"zero-size by target policy, NULL, overflow,
errno, alignment, realloc copy/failure — Meets the documented
selected ABI."* Table 6-1 includes hooks for each of those items.
Passing a hook is evidence about the tested target and variation
only; it does not promote the ABI (§5.5).

### 6.1 Mapping table

**Table 6-1 — Contract-to-hook mapping**

| Contract ID | Roadmap §10 reference | Hook ID | Oracle | Pass criterion | Applicability |
|---|---|---|---|---|---|
| MA-6.1-CALLOC-02 (`calloc` overflow) | C-contracts: overflow, errno | C-001 | POSIX baseline; per-target variation where recorded | An allocation request whose `count × size` overflows, or exceeds the target maximum request, fails as specified in §2.4 (null return, `errno` as specified); no allocation is returned; no wrapped size is allocated. | All targets |
| MA-6.1-CALLOC-01, MA-6.1-CALLOC-05 (`calloc` zeroing) | C-contracts: allocation correctness | C-002 | POSIX baseline | Every byte of the usable allocation returned by a successful `calloc` reads as zero on first observation by the caller, for tested sizes including non-multiples of the alignment. | All targets |
| MA-6.1-REALLOC-02 (`realloc` preserve-on-failure) | C-contracts: realloc copy/failure | C-003 | POSIX baseline | On a failed `realloc`, the original block remains valid, unmodified in its contents, and owned by the caller; the specified error indication is produced; no partial move occurs. | All targets |
| MA-6.1-REALLOC-01 (`realloc` copy-min) | C-contracts: realloc copy/failure | C-004 | POSIX baseline | On a successful `realloc` to a different block, the first `min(old size, new size)` bytes are preserved byte-for-byte. | All targets |
| MA-6.1-FREE-02, MA-6.2-010 (`free(NULL)`) | C-contracts: NULL | C-005 | POSIX baseline | `free(NULL)` is a no-op: no error, no state change observable to the caller, subsequent allocations behave normally. | All targets |
| MA-6.1-MALLOC-03, MA-6.1-REALLOC-04 (zero-size; `realloc(p,0)`) | C-contracts: zero-size by target policy | C-006 | V-01 / V-04 variation rows for the selected per-target policy [UNRESOLVED: U-09, U-12] | Behaviour matches the *documented selected policy for that target exactly* — return value, ownership of `p` in the `realloc(p,0)` case, `errno`, and freeability of any returned pointer. A result matching a different target's policy is a failure. | Per target; one hook instance per target policy |
| MA-6.1-ALIGNED-01 (alignment validity) | C-contracts: alignment | C-007 | POSIX baseline; per-target maximum alignment where recorded | Successful allocations satisfy the alignment specified in §2.6 for their request class; a request with an invalid alignment parameter fails by the specified model and allocates nothing. | All targets |
| MA-6.1-POSIXMA-02 (`posix_memalign` error model) | C-contracts: errno, alignment | C-008 | POSIX baseline (V-06) | On failure, `posix_memalign` returns the specified error number, does not use `errno` as its error channel, and does not modify the caller's result pointer; on success it returns zero and stores an allocation meeting the requested alignment. | Targets where `posix_memalign` is in the settled symbol set (U-01) |
| MA-6.2-054 (cross-thread ownership) | §10 ownership row; C-contracts row by reference | C-009 | POSIX baseline | A block allocated on one thread may be freed on a different thread exactly as §3 provides, with no corruption, double-free diagnostic, or loss; repeated cross-thread alloc/free cycles complete and subsequent allocations remain valid. | All multithreaded targets |
| MA-6.3-001…004 (bootstrap / reentrancy) | C-contracts row by reference; Roadmap §6.3 watchdog requirement | C-010 | Target watchdog procedure specified under §6.3 | Allocation during bootstrap and reentrant entry covered by §4 completes, fails as specified, or is deferred as specified, within the watchdog bound; a hang, deadlock, or unbounded recursion is a failure regardless of return values. See §6.3. | Targets / configurations where bootstrap or reentrancy applies |

Additional rows required by the mapping rule, including other
`errno`, alignment, and `realloc` rows in §§2–4 and the `C-3nn` /
`C-4nn` slots named there, are to be added to this table with
consecutive `C-###` identifiers as those rows are enumerated. No
`[NORM]` or `[COND: …]` row is conformant by assertion alone.

### 6.2 Undefined behaviour rows

Rows in §§2–4 that specify UB — including, without limitation,
freeing a pointer not returned by this allocator, freeing the same
block twice, and using a block after free — are recorded in the
mapping as: **No test hook — UB**. No hook shall be written that
purports to pass or fail on UB, no expected result shall be assigned
to UB, and a diagnostic observed in one build shall not be recorded
as contract. Sanitiser, debug-build, or watchdog observations of UB
may be reported as diagnostics only, clearly separated from
conformance results.

### 6.3 Bootstrap / reentrancy watchdog

`[COND: watchdog procedure selected for the target]`

Hook C-010 depends on a bounded watchdog: the test harness places a
finite bound on bootstrap / reentrant allocation, and expiry of that
bound is a failure, not an inconclusive result. The specific bound,
the signal-handler interaction, and the fork-child behaviour
exercised alongside bootstrap are [UNRESOLVED: U-05], pending the
watchdog tests that will define them. Until U-05 is decided, C-010
results are recorded as preliminary and cannot support a promotion
claim under §5.5.

---

## 7. Unresolved items register

Items in this register are open. An entry here creates no contract
obligation beyond the obligation to decide it at its stated gate.
Options listed are possibilities recorded for decision, not
selections.

**Table 7-1 — Unresolved items**

| ID | Item | Raised in | Blocked by | Options | Decision gate | Status |
|---|---|---|---|---|---|---|
| U-01 | Final exported symbol set, including which conditional symbols are present per target | §§2–4; §5.2; App. A | Phase 1 evidence | (a) Core C set only; (b) core + aligned-allocation symbols; (c) core + C++ operator set (see U-06); (d) per-target differing sets, each enumerated | Roadmap §15 Q4; then §5.5 audit | Open |
| U-02 | Per-target zero-size semantics and `realloc(p,0)` chosen semantics (register-level entry; detail in U-09 and U-12) | §2; §4; §6 C-006 | Authoritative per-target sources (U-04); target selection | Per target: return unique freeable pointer / return `NULL` with or without freeing `p` / other semantics documented by that target's source — to be recorded per target, not globally | Per-target contract decision before C-006 can pass for that target | Open |
| U-03 | Namespaced `mimir_malloc` / `mimir_free` and a statistics / trim API described in Roadmap §7.1 — **not in §6 scope**; scope decision needed | Roadmap §7.1; absent from §6 | Scope decision by the project owner / lead | (a) Parallel contract document; (b) express out-of-scope declaration; (c) later amendment to this contract | Scope decision gate, before any §7.1 API is specified normatively | Open |
| U-04 | Authoritative contract sources for musl and bionic are missing. §6 cites only S5 (Linux man pages) and S6 (POSIX), which are insufficient for non-glibc columns of the per-target tables | §1.7; §2.11; §6 | Identification of authoritative musl / bionic sources | (a) Cite musl upstream docs/source; (b) cite bionic upstream docs/source; (c) restrict contract claims to glibc/POSIX targets until sources exist | Gate for any non-glibc [NORM] claim | Open |
| U-05 | Signal-handler allocation policy and fork-child policy specifics (detail in U-17…U-21) | §4.4; §4.5; §6.3 | Watchdog and fork/signal tests not yet specified or run | To be enumerated from test design: allowed / forbidden / deferred-in-child, per case | Watchdog test decision gate (§6.3) | Open |
| U-06 | C++ operator set decision, in tension with the §6.3 constructor-before-main path: if operators are provided, allocation can be demanded before `main`, during static construction | §2.10 conditional C++ rows; Roadmap §6.1 vs §6.3 | U-01; bootstrap evidence (U-05) | (a) No C++ operators in scope; (b) operators in scope with a specified pre-main path; (c) operators conditional per target | Decision with U-01 at Roadmap §15 Q4 | Open |
| U-07 | ABI versioning and runtime-configuration versioning mechanism (§5.3) | §5.3 | U-01; §5.5 promotion | Symbol versioning / SONAME / configuration field / query function / combination — none selected | §5.5 promotion gate | Open |
| U-08 | Target-table axis granularity: whether rows are keyed by OS × libc × arch, or additionally by device / environment. Proot and Termux are environments, not ABIs; SM-X400 and Raspberry Pi are devices, not ABIs | §2.11; Roadmap §5 target list | Target-model decision | (a) ABI axis only (OS × libc × arch), devices/environments as test instances; (b) ABI axis plus a separate environment/device axis; (c) per-device rows | Gate before §2.11 tables can be completed normatively | Open |
| U-09 | Zero-size behaviour for `malloc(0)`, `calloc` zero factor, and zero-size aligned requests (V-01) | §2.2–§2.7 | U-04; per-target selection (U-02) | As recorded in V-01 | Per-target contract decision | Open |
| U-10 | `errno` guarantee on allocation failure (V-02) | §2 passim | U-04 | As recorded in V-02 | Per-target contract decision | Open |
| U-11 | Alignment validity and any size-multiple rule for `aligned_alloc` / `posix_memalign` (V-03) | §§2.6–2.7 | U-04 | As recorded in V-03 | Per-target contract decision | Open |
| U-12 | `realloc(p, 0)` behaviour for a non-null live pointer (V-04) | §2.5 | U-04; U-02 | As recorded in V-04 | Per-target contract decision | Open |
| U-13 | `malloc_usable_size` presence and semantics, including the null case (V-05) | §2.9 | U-01; U-04 | As recorded in V-05 | Gate in §2.9 | Open |
| U-14 | Exact `posix_memalign` error numbers per target (V-06) | §2.7 | U-04 | As recorded in V-06 | Per-target contract decision | Open |
| U-15 | Numeric default alignment guarantee and its relation to target page size (V-07) | §§2.2, 2.6, 2.7 | Measured target data; U-08 | As recorded in V-07 | Per-target contract decision | Open |
| U-16 | Presence of `reallocarray` on a target outside its §2.8 gate | §2.8 | U-01 | Provide / do not provide, per target | Decision with U-01 | Open |
| U-17 | Allocator lock state in the child of a multithreaded `fork` where another thread held a lock | §4.5 | Watchdog tests (U-05) | Recovery / reinitialisation / defined failure, per platform evidence | §4.5 resolution gate | Open |
| U-18 | First allocation in the child of a multithreaded parent, including bootstrap re-entry | §4.5 | Watchdog tests (U-05) | As evidenced per platform | §4.5 resolution gate | Open |
| U-19 | Interaction and ordering of MimirAlloc atfork handlers with host-application and other-library handlers | §4.5 | Watchdog tests (U-05) | As evidenced per platform | §4.5 resolution gate | Open |
| U-20 | Statistics and diagnostics state carried across `fork`, including counter attribution | §4.5; MA-6.2-051 | Watchdog tests (U-05) | As evidenced per platform | §4.5 resolution gate | Open |
| U-21 | Fork occurring during bootstrap, before initialisation completes | §4.5 | Watchdog tests (U-05) | As evidenced per platform | §4.5 resolution gate | Open |

### 7.1 Roadmap inconsistencies recorded

The following are recorded as gaps or inconsistencies in the roadmap
material this contract is based on. They are **recorded, not
silently filled**. No text in §§1–6 of this contract should be read
as resolving them.

(a) **§7.1 namespaced API absent from §6.** Roadmap §7.1 describes a
`mimir_*` namespaced API and statistics/trim functions that appear
nowhere in Roadmap §6's contract scope. Recorded as U-03. Note:
Roadmap §6.2's versioned runtime-configuration clause is the only §6
hook that anticipates anything resembling a statistics/configuration
API — it does not specify one.

(b) **§6 source base insufficient for its own table demand.**
Roadmap §6 demands per-target (including non-glibc) contract columns
while citing only S5 (Linux man pages) and S6 (POSIX). Recorded as
U-04.

(c) **§5 target list mixes categories.** The Roadmap §5 target list
mixes ABIs, environments (proot, Termux), and devices (SM-X400,
Raspberry Pi) in a single list. Recorded as U-08.

(d) **C++ conditional vs pre-main constructors tension.** Roadmap
§6.1 treats the C++ operator set as conditional, while Roadmap §6.3
requires a working path for constructors running before `main`,
which presupposes early allocation availability. Recorded as U-06.

(e) **Unnamed core topics in Roadmap §6.** Roadmap §6 does not name:
(i) core-function thread-safety (only thread-safety of
runtime-configuration changes is named, §6.2); (ii) maximum-request
limits as a contract topic in their own right; nor (iii) an
exclusion list for adjacent libc functions (e.g. `mallopt`,
`mallinfo`, `strdup` and similar — in scope, out of scope, or
forwarded). These are gaps in the source material. This contract
records them here and in §7 rather than inventing positions for
them.

---

## Appendix A. Symbol inventory

[PROPOSED] Candidate symbols only. Inclusion here does **not** mean
a symbol is exported, will be exported, or is in scope. Final set:
[UNRESOLVED: U-01]. Stability class for every entry: **Unassigned**
(§5.5).

| Symbol | Kind | Status tag | Stability class |
|---|---|---|---|
| `malloc` | Core C allocation | [NORM] candidate (§2.2) | Unassigned |
| `free` | Core C allocation | [NORM] candidate (§2.3) | Unassigned |
| `calloc` | Core C allocation | [NORM] candidate (§2.4) | Unassigned |
| `realloc` | Core C allocation | [NORM] candidate (§2.5) | Unassigned |
| `aligned_alloc` | Aligned allocation | [NORM] candidate (§2.6) | Unassigned |
| `posix_memalign` | Aligned allocation | [NORM] candidate (§2.7) | Unassigned |
| `reallocarray` | Core C allocation | [COND: target requires it and semantics defined per target] (§2.8) | Unassigned |
| `malloc_usable_size` | Introspection | [COND: target requires it and semantics defined per target] (§2.9) | Unassigned |
| C++ `operator new` / `operator new[]` (throwing, nothrow, aligned forms) | C++ allocation | [COND: C++ integration requires the variant] (§2.10; U-06) | Unassigned |
| C++ `operator delete` / `operator delete[]` (unsized, sized, aligned forms) | C++ allocation | [COND: C++ integration requires the variant] (§2.10; U-06) | Unassigned |
| `mimir_malloc` | Namespaced API (Roadmap §7.1) — namespaced form of §2.2 | [UNRESOLVED: U-03] — not in §6 scope; as-built surface of record inventoried by Z-081 (see the Z-081 note below) | Unassigned |
| `mimir_free` | Namespaced API (Roadmap §7.1) — namespaced form of §2.3 | [UNRESOLVED: U-03] — not in §6 scope; as-built surface of record inventoried by Z-081 | Unassigned |
| `mimir_calloc` | Namespaced API (Roadmap §7.1) — namespaced form of §2.4 | [UNRESOLVED: U-03] — not in §6 scope; as-built surface of record inventoried by Z-081. This row discharges INC-M002-03 for `calloc` | Unassigned |
| `mimir_realloc` | Namespaced API (Roadmap §7.1) — namespaced form of §2.5 | [UNRESOLVED: U-03] — not in §6 scope; as-built surface of record inventoried by Z-081. This row discharges INC-M002-03 for `realloc` | Unassigned |
| `mimir_aligned_alloc` | Namespaced API — namespaced form of §2.6 | [UNRESOLVED: U-03] — not in §6 scope; contract of record is docs/Z13_ALIGNED_ALLOCATION_CONTRACT.md (Z-073), implemented by Z-074; inventoried by Z-081 | Unassigned |
| `mimir_posix_memalign` | Namespaced API — namespaced form of §2.7 | [UNRESOLVED: U-03] — not in §6 scope; contract of record is docs/Z13_ALIGNED_ALLOCATION_CONTRACT.md (Z-073), implemented by Z-074; inventoried by Z-081 | Unassigned |
| `mimir_usable_size` | Namespaced API — namespaced form of §2.9 | [UNRESOLVED: U-03] — not in §6 scope; naming and semantics of record are decision D-012 (Z-075); the standard name `malloc_usable_size` is NOT declared or exported (D-012); inventoried by Z-081 | Unassigned |
| `mimir_stats_snapshot` | Namespaced API (Roadmap §7.1 statistics; name and versioned struct `mimir_stats_v1` specified by Z-076) | [UNRESOLVED: U-03] — not in §6 scope; surface of record is docs/Z13_PUBLIC_STATISTICS.md (Z-076); inventoried by Z-081 | Unassigned |
| `mimir_trim` | Namespaced API (Roadmap §7.1 trim; name specified by Z-077) | [UNRESOLVED: U-03] — not in §6 scope; surface of record is docs/Z13_Z077_TRIM_REALLOCARRAY.md (Z-077); inventoried by Z-081 | Unassigned |
| `mimir_reallocarray` | Namespaced API — namespaced form of §2.8 | [UNRESOLVED: U-03] — not in §6 scope; surface of record is docs/Z13_Z077_TRIM_REALLOCARRAY.md (Z-077); inventoried by Z-081 | Unassigned |
| `memalign`, `valloc`, `pvalloc` | Legacy aligned allocation | Not named in Roadmap §6.1 — no candidacy asserted; recorded here only so their absence from §2 is a visible non-decision, not an oversight | Unassigned |

**[Z-081 amendment — as-built inventory, 2026-11-12]** Slice
Z-081 (Contracts appendix and ABI audit, D-10) re-ran the
M-002-style ABI audit over the widened Wave 9 surface on the
Z13 host (purpleraider, x86-64, 4 KiB pages); the report is
filed as docs/Z13_Z081_ABI_AUDIT.md
(`docs/Z13_Z081_ABI_AUDIT.md`). The audit found the exported
surface — exactly ten dynamic symbols — in exact three-way
agreement across the public header, the version script, and
the shared library's `.dynsym`, but found eight of the ten
namespaced symbols missing from this appendix (only
`mimir_malloc` and `mimir_free` were inventoried; the
statistics/trim row named no function). The ten
`mimir_*` rows above now inventory the whole as-built
surface of record, and INC-M002-03 is discharged. This
inventory is **not** a promotion: U-01 and U-03 remain open,
no ABI version is assigned, and the stability class of every
entry stays **Unassigned** (§5.5).

Functions named only in the recorded exclusion-list gap (§7.1(e)) —
e.g. `mallopt`, `mallinfo`, `strdup` — are **not** listed as
candidates. Their absence records the gap; it is not a decision to
exclude them.

## Appendix B. Change log

No version number is assigned to this document or to any ABI or
configuration described in it (§5.3). This log records issues of the
document, not versions of an ABI.

| Date | Slice / issue | Description |
|---|---|---|
| 2026-11-06 | A-008 — initial Proposed issue | Initial issue of *MimirAlloc — Core API / ABI Contract Specification* in Proposed status. No implementation exists; none is claimed. No ABI version assigned. |
| 2026-11-12 | Z-081 — Contracts appendix and ABI audit (D-10) | Appendix A amended to inventory the whole as-built namespaced surface: `mimir_calloc` and `mimir_realloc` added (discharging INC-M002-03), the Wave 9 surface added by name (`mimir_aligned_alloc`, `mimir_posix_memalign`, `mimir_usable_size`, `mimir_stats_snapshot`, `mimir_trim`, `mimir_reallocarray`), and the unnamed statistics/trim row replaced by its two named functions. Audit report: docs/Z13_Z081_ABI_AUDIT.md. No status, scope, or stability change: U-01/U-03 remain open and every entry remains Unassigned. |
