# MimirAlloc — Architecture Layers

- Slice: A-009
- Status: research document
- Evidence status: Proposed (roadmap S2 labels)
- Source of record: roadmap
  `docs/MimirAlloc_Research_and_Development_Roadmap.md`,
  cited by section throughout as (S< section >).
- Companion: `docs/IMPLEMENTATION_LANGUAGE_DECISION.md`
- Claim ceiling: S7 preamble — this is a candidate
  architecture for evaluation, not a description of
  tcmalloc internals to copy, and not an
  implementation claim.

Filename justification: banked slices in this
campaign use the flat `docs/UPPER_SNAKE.md`
convention (for example `SUPPORTED_TARGETS.md`,
`DECISION_LEDGER.md`). Roadmap S13 proposes a
lowercase `docs/architecture.md` in a proposed
repository layout whose locations the ledger
notes N-5 / N-6 already record as unsettled.
This file therefore follows the banked convention.
The divergence is recorded, not hidden, as
inconsistency A9-1 in Section 16 of this document.

## 1. Honest headline

No layer described in this document is implemented.

- No layer is Implemented, Host-tested,
  Device-tested, Integration-proven, or
  Performance-measured in the sense of S2.
- Every layer in Sections 4 to 10 carries the
  single evidence status Proposed (S2).
- This document organises the seven layers named
  in S7.1, states their boundaries, dependencies,
  forbidden behaviours, and promotion criteria.
- It creates no code, no directories, no symbols,
  and no performance, memory, or latency claim.
- Any sentence that sounds like a capability must
  be read as a proposed responsibility, to be
  proven by a later slice under S8 and S10.

## 2. Source basis and citation method

Every factual claim about the roadmap in this
document carries a roadmap section citation.
Paraphrase is used; roadmap wording is not copied
at length. Where this document makes a design
decision the roadmap does not make, that decision
is labelled as an A-009 decision and, where it
conflicts with or extends the roadmap, recorded
in Section 16.

Primary sections relied on:

- S5 — supported-target matrix.
- S6 — contract-first API and ABI work, including
  S6.1 core API candidates, S6.2 ownership rules,
  and S6.3 allocation-free startup / diagnostics.
- S7.1 — the seven proposed layers (S7.1.1 to
  S7.1.7 in the numbering used by this slice).
- S7.2 — implementation language decision.
- S7.3 — threading and slow paths.
- S7.4 — rseq and per-CPU designs.
- S7.5 — large allocations and address-space safety.
- S9.1 — the ten reliability invariants.
- S10 — test and verification matrix.
- S15 — decision register and open questions.

Ownership of neighbouring questions:

- A-008 owns the S6 contract specification.
  This document decides no symbol list, no
  zero-size rule, no errno rule, and no realloc
  semantics (see Sections 11 and 15).
- A-003 owns the S5 supported-target matrix.
  This document decides no target.
- A-007 owns the S3.3 integration-layer map.
  This document decides no integration point.
- This document decides none of those. It only
  places the S7.1 layers relative to them.

Research divergence, recorded honestly: research
documents in this repository propose different
layerings. `docs/research/ai_memory_synthesis/09`
proposes five layers A to E, including a
managed-object layer, domain adapters, and an
optional learned policy. `docs/research/01`
proposes five layers that fold admission into
contracts and the OS layer into a backend. Those
are research proposals above and beyond S7.1.
They are NOT S7.1 layers. A-009 follows the
roadmap: seven layers, S7.1 items 1 to 7, and
imports nothing from those alternative layerings.

## 3. Dependency law

The following law is stated exactly and governs
Sections 4 to 10, the matrix below, and Section 11:

Request flow downward only L1->L2->(L3|L4)->L5->L6;
L3 miss falls to L4; L4 only client of L5; L5 only
policy client of L6. Free flow L1->L4 (owner
determination), L3 drained/transferred only via
L3-L4 interface. No layer skips to L6. Sole
sanctioned non-adjacent dependency: L6 threading
primitives may be consumed by L3/L4 via L6's
published interface (S7.1 item 6 assigns threading
primitives to L6). L7 is a sidecar, never in-line:
L1-L6 only emit to fixed counter slots owned by
L7, no return value used for control (S9.1 inv 8);
L7 only reads snapshots, calls nothing in L1-L6;
allocator complete with L7 compiled out
(S7.1 'Optional').

Reading the law:

- Downward means toward the OS. No upward calls.
- A miss in L3 is not an error; it is the normal
  L3 to L4 fall-through.
- Emission to L7 is a write to a fixed slot. It
  returns nothing that any layer may branch on.
- Snapshot reading by L7 is not a call into
  L1 to L6 and appears as Forbidden in the call
  matrix below, by design.

### 3.1 Dependency matrix (call / control)

A = Allowed on the named edge only.
F = Forbidden. Rows are callers, columns callees.

| From \ To | L1 | L2 | L3 | L4 | L5 | L6 | L7 |
|---|---|---|---|---|---|---|---|
| L1 | F | A | F | A | F | F | A |
| L2 | F | F | A | A | F | F | A |
| L3 | F | F | F | A | F | A | A |
| L4 | F | F | F | F | A | A | A |
| L5 | F | F | F | F | F | A | A |
| L6 | F | F | F | F | F | F | A |
| L7 | F | F | F | F | F | F | F |

Notes on the exact edges:

- L1->L2: request admission. L1->L4: free-path
  owner determination only. L1->L7: counter emit.
- L2->L3 and L2->L4: admitted request routing.
  L2->L7: counter emit.
- L3->L4: miss, drain, and transfer only.
  L3->L6: threading primitives only, via the
  published L6 interface. L3->L7: counter emit.
- L4->L5: sole client edge to L5. L4->L6:
  threading primitives only. L4->L7: counter emit.
- L5->L6: sole policy-client edge to L6.
  L5->L7: counter emit. L6->L7: counter emit.
- Every other cell is Forbidden, including all
  upward edges, all layer-skipping edges to L6,
  and every L7 outbound call.

## 4. L1 — Public boundary (S7.1.1)

(a) Roadmap text, paraphrased: S7.1 item 1 places
the standard C ABI plus an optional namespaced
API at the boundary, and requires explicit
ownership and error behaviour per interface (S7.1).
(b) Responsibilities: present the standard C ABI;
present the optional namespaced `mimir_malloc` /
`mimir_free` surface; present stats and trim entry
points as exports; state ownership and error
behaviour for every exported interface (S6, S7.1).
(c) Interface in: calls from application, runtime,
and language code in the form defined by the
A-008 S6 contract specification; no other entry.
(d) Interface out: admitted requests to L2;
free-path owner determination to L4; counter
emission to fixed L7 slots; results and errors
returned to the caller exactly as contracted.
(e) Allowed dependencies: L1->L2, L1->L4 (free
path only), L1->L7 (emit only). See Section 3.1.
(f) Forbidden dependencies / behaviours: no OS
calls; no size-class logic; no span logic; no
silent fallback to a different allocator, because
a pointer could later be freed by the wrong
implementation (S6.2); no invented symbols.
(g) Constraining sections: S6.1, S6.2, S6.3, S7.1,
S9.1, S15 Q4. The symbol list is NOT frozen here:
it belongs to A-008's S6 specification, and
S15 Q4 (which symbols are required) is open.
(h) S9.1 invariants owned / contributed:
Invariant 1 (one valid owner, non-overlapping
storage, as the caller-visible promise),
Invariant 4 (free of null per target contract),
Invariant 5 (failed realloc preservation, as the
caller-visible promise; semantics per A-008).
(i) Evidence status: Proposed. Promotion evidence:
the A-008 contract spec, plus S10 C-contract and
ABI / linking rows passing in a named environment,
plus S8 Phase 5 symbol-binding proof that the
intended consumer resolves to this boundary.

C++ note, stated here and recorded as A9-5:
S6.1 contemplates C++ `operator new` / `delete`
variants only when integration requires them.
The companion document places any such C++ shim
inside L1. S7.1 itself states no home for it.
No eighth layer is created by this document.

## 5. L2 — Admission and checked arithmetic (S7.1.2)

(a) Roadmap text, paraphrased: S7.1 item 2
validates sizes, alignment, and overflow, and
admits an optional explicit process budget; OS
pressure callbacks are separate from hard
reservations (S7.1).
(b) Responsibilities: validate request size and
alignment; perform checked arithmetic so size
and alignment calculations cannot overflow;
apply the optional explicit process budget if
one is set; keep pressure callbacks distinct
from reservations.
(c) Interface in: requests from L1 only, with
size, alignment, and call kind as contracted.
(d) Interface out: admitted requests to L3 or
L4; a defined failure to L1 on rejection;
counter emission to fixed L7 slots.
(e) Allowed dependencies: L2->L3, L2->L4,
L2->L7 (emit only). See Section 3.1.
(f) Forbidden dependencies / behaviours: no
mappings; no free lists; a pressure callback is
not a reservation and must never be treated as
one; no budget value is proposed in this
document — S15 Q7 (safe mobile budget) is
unmeasured, so the budget is unset by default;
statistics must not drive unsafe admission
(S9.1 Invariant 8).
(g) Constraining sections: S6.1 (including
calloc overflow checking before multiplication),
S7.1, S7.5 (checked arithmetic for sizes and
alignments), S9.1, S15 Q7.
(h) S9.1 invariants owned / contributed:
Invariant 3 (size and alignment calculations
cannot overflow) — owned here.
(i) Evidence status: Proposed. Promotion evidence:
S10 C-contract rows at integer and alignment
boundaries, S9.3 boundary tests, and a measured
budget study before any budget default exists.

## 6. L3 — Small-object fast path (S7.1.3)

(a) Roadmap text, paraphrased: S7.1 item 3
proposes initially simple thread-local lists or
bounded caches, subject to measured need (S7.1).
(b) Responsibilities: serve the common small
request from a simple thread-local list or a
bounded cache, when measurement justifies one;
fall through to L4 on any miss.
(c) Interface in: admitted small requests
from L2 only.
(d) Interface out: a block to L1 via the return
path; miss, drain, and transfer to L4 only;
counter emission to fixed L7 slots.
(e) Allowed dependencies: L3->L4, L3->L6
(threading primitives only, published interface),
L3->L7 (emit only). See Section 3.1.
(f) Forbidden dependencies / behaviours: no
direct mapping acquisition — backing memory
arrives only via L4 to L5; no unbounded caches
(bounded caches protect retained memory on
low-RAM tablets, S7.3); no per-CPU or rseq
design here (S7.4 is later and optional);
no lock-free structure adopted for speed's sake
(S7.3). The initial honest form of L3 may be a
pass-through to L4 until measurement justifies
a cache at all (recorded as A9-6).
(g) Constraining sections: S7.1, S7.3, S7.4,
S9.1, S10 concurrency and memory rows, S15 Q8
(whether per-thread caching is beneficial).
(h) S9.1 invariants owned / contributed:
Invariant 1 (single valid owner), Invariant 6
(no node in incompatible lists at once, for any
local list), Invariant 7 (bounded, testable
slow path on a full cache or failed backing).
(i) Evidence status: Proposed. Promotion evidence:
S8 Phase 7 exit material — measured size classes
and bounded local reuse with no correctness
regression, and latency / retained-memory reports
against the Phase 3 baseline and platform allocator.

## 7. L4 — Central ownership (S7.1.4)

(a) Roadmap text, paraphrased: S7.1 item 4 holds
size-class management, free-span accounting,
remote-free handling, and synchronization (S7.1).
(b) Responsibilities: own size classes; account
for free spans; handle frees of blocks owned by
another thread; own central synchronization;
perform thread-exit drain, transfer, or reclaim
of state handed to it from L3 (S7.3).
(c) Interface in: admitted requests and misses
from L2 / L3; free-path owner determinations
from L1; drained or transferred L3 state via
the L3-L4 interface only.
(d) Interface out: span requests to L5 (sole
client); blocks and ownership answers on the
return path; counter emission to L7 slots.
(e) Allowed dependencies: L4->L5, L4->L6
(threading primitives only, published interface),
L4->L7 (emit only). See Section 3.1.
(f) Forbidden dependencies / behaviours: no
syscalls; no direct OS mapping or advice calls;
begin coarse-locked (S7.3) — sharding or finer
locking only on benchmark evidence; cross-thread
handoff and thread-exit drain / transfer /
reclaim must be specified, not assumed; ownership
and memory-order reasoning for shared metadata
on the ARM weak memory model must be stated
explicitly, not assumed (S7.3).
(g) Constraining sections: S7.1, S7.3, S9.1,
S9.3 (thread churn tests), S10 ownership and
concurrency rows, S8 Phases 7 and 8.
(h) S9.1 invariants owned / contributed:
Invariant 1, Invariant 6 (cross-thread handoff
preserves ownership; no node in incompatible
free lists) — owned here, Invariant 7,
Invariant 9 (synchronization failure reported
or safely terminating, never hidden),
Invariant 10 (every fallback preserves ownership
or explicitly fails).
(i) Evidence status: Proposed. Promotion evidence:
S8 Phase 8 exit material — repeated cross-thread
stress with no corrupted live allocations or lost
ownership, plus S10 ownership / concurrency rows.

## 8. L5 — Page/span manager (S7.1.5)

(a) Roadmap text, paraphrased: S7.1 item 5
acquires, divides, combines, and returns
mappings, and maintains metadata and occupancy
(S7.1).
(b) Responsibilities: acquire mappings via L6;
divide mappings into spans; combine spans;
return mappings via L6; maintain span metadata
and occupancy truthfully for its lifetime.
(c) Interface in: span requests from L4 only.
L4 is the only client of L5 (Section 3).
(d) Interface out: spans to L4; mapping,
unmapping, and reclamation requests to L6;
counter emission to fixed L7 slots. No pointer
or span detail is exported to L1.
(e) Allowed dependencies: L5->L6 (sole policy
client), L5->L7 (emit only). See Section 3.1.
(f) Forbidden dependencies / behaviours: no
size-class decisions (those are L4); no pointers
exported to L1; no hardcoded page size and no
hardcoded 48-bit virtual-address assumption
(S7.5); no huge virtual reservations without
measurement (S7.5); internal logical pages,
system page size, spans, and huge pages are
distinct concepts and must not be conflated
(S6.2). This document states no page-size value
as a target fact (recorded as A9-13).
(g) Constraining sections: S6.2, S7.1, S7.5,
S9.1, S10 memory row, S15 Q6 (page size and
VA range to be measured, not inferred).
(h) S9.1 invariants owned / contributed:
Invariant 2 (metadata describes actual memory
and is not accessed after its own lifetime) —
owned here, Invariant 7 (bounded slow path).
(i) Evidence status: Proposed. Promotion evidence:
S10 memory-row tests (cycles, fragmentation,
drain / trim, large-mapping pressure) with
counters and OS metrics aligned to their stated
meanings, on a named environment (S10, S8 Phase 4).

## 9. L6 — OS abstraction (S7.1.6)

(a) Roadmap text, paraphrased: S7.1 item 6 holds
page-size discovery, mappings, unmapping,
reclamation / advice, threading primitives, and
platform-specific behaviour (S7.1).
(b) Responsibilities: discover the actual page
size at runtime; create and remove mappings;
give reclamation / advice to the OS; provide
threading primitives through a published
interface; contain all platform-specific code.
L6 is the ONLY layer with platform-specific code.
(c) Interface in: policy requests from L5 only;
threading-primitive requests from L3 / L4 via
the published interface (Section 3 exception).
(d) Interface out: mappings, capabilities, and
explicit errors to L5 (and primitive results to
L3 / L4); counter emission to fixed L7 slots.
No upward calls into any layer.
(e) Allowed dependencies: L6->L7 (emit only).
Every other outbound call is Forbidden.
(f) Forbidden dependencies / behaviours: no
allocation policy; no upward calls; platform
behaviour differences (glibc, musl, bionic — S5)
must not leak upward except as explicit
capability or error returns; no layer above L6
may contain platform-specific code. Rseq probing
lives here as a later, optional capability
(S7.4; assigned by A-009, recorded as A9-7).
(g) Constraining sections: S5, S7.1, S7.4, S7.5,
S9.1, S12 (page-size environments measured).
(h) S9.1 invariants owned / contributed:
Invariant 7 (bounded handling of failed backing
allocation), Invariant 9 (synchronization and
primitive failure surfaced, never hidden).
(i) Evidence status: Proposed. Promotion evidence:
S8 Phase 4 exit material — native AArch64 and
Android builds with physical-device tests, page
size and ABI recorded; a cross-compile alone
does not pass that gate (S8 Phase 4).

## 10. L7 — Optional observability (S7.1.7)

(a) Roadmap text, paraphrased: S7.1 item 7 is
optional observability — low-overhead counters
and opt-in diagnostic snapshots; default to no
private-data collection and no network telemetry
(S7.1).
(b) Responsibilities: own the fixed counter slots
that L1 to L6 emit to; serve opt-in snapshots
read from those slots; define what each counter
means (compare S8 Phase 10 counter list).
(c) Interface in: counter emissions from L1 to
L6 into fixed slots; snapshot requests from a
diagnostic consumer outside the allocator.
(d) Interface out: snapshots to that consumer
only. L7 calls nothing in L1 to L6.
(e) Allowed dependencies: none outbound.
L7 is a sidecar, never in-line (Section 3).
(f) Forbidden dependencies / behaviours: never
allocate through the allocator it observes —
bootstrap and diagnostics must not recurse into
the allocator (S6.3; decision D-004 as cited in
the campaign ledger); no unbounded dynamic
allocation in error reporting (S6.3); counters
are approximate or stale and drive no decisions
in any layer (S9.1 Invariant 8); diagnostics
preserve evidence and stop or report — they
never 'repair' arbitrary corruption (S9.2);
no private-data collection by default; NO
network telemetry.
(g) Constraining sections: S6.3, S7.1, S9.1,
S9.2, S10 memory and robustness rows, S8 Phase 10.
(h) S9.1 invariants owned / contributed:
Invariant 8 (statistics do not drive unsafe
decisions if approximate or stale) — owned here.
(i) Evidence status: Proposed. Promotion evidence:
S8 Phase 10 exit material — statistics balance
under tests, error reports are useful and do not
recursively allocate, and production versus debug
behaviour is clearly distinguished. The allocator
must also be shown complete with L7 compiled out.

Stats ownership, stated plainly: S7.1.1 puts a
stats entry point in the L1 API while S7.1.7
puts counters in L7. The split used here — L1
exports, L7 owns — is an A-009 design decision,
recorded as A9-9. The roadmap does not state it.

## 11. End-to-end flows

All flows follow the Section 3 dependency law
and no other edges. Steps are narrated on the
DAG; no timing or performance is claimed.

### 11.1 Small allocation

1. Caller enters at L1 (S7.1.1).
2. L1 passes the request to L2.
3. L2 validates size, alignment, and overflow,
   and applies the budget only if one is set.
4. L2 routes the admitted request to L3.
5. L3 hit: L3 returns a block on the return path.
6. L3 miss: L3 falls to L4 (Section 3).
7. L4 selects a size class and a free span; if
   no span suffices, L4 requests one from L5.
8. L5 requests a mapping from L6 if needed,
   divides it, records metadata and occupancy.
9. The block returns L4 to L3 / L2 to L1 to the
   caller. Each layer emits counters to L7 slots;
   no return value from L7 is used for control.

### 11.2 Large allocation

1. Caller enters at L1; L1 passes to L2.
2. L2 validates with checked arithmetic (S7.5).
3. L2 routes directly to L4 — large requests do
   not enter the L3 small-object cache.
4. L4 asks L5 for a span of the required extent.
5. L5 asks L6 for a mapping; L6 discovers the
   actual page size at runtime (S7.5).
6. L5 records metadata; the block returns to
   the caller via L4 and L1. No huge virtual
   reservation is made without measurement (S7.5).

### 11.3 Same-thread free

1. Caller presents the pointer at L1.
2. L1 performs owner determination to L4.
3. If L4 places the block in the owning thread's
   L3 list, it moves only via the L3-L4 interface.
4. Otherwise L4 returns it to its span's free
   accounting; L5 updates occupancy. No OS call
   is made by L4; any return to the OS is
   L5 to L6 only.

### 11.4 Remote free (different thread frees)

1. Caller presents the pointer at L1; L1 to L4
   for owner determination, as in 11.3.
2. L4, not the freeing thread's L3, takes the
   block: remote-free handling is L4's (S7.1.4).
3. Ownership transfers under L4 synchronization
   with stated ARM memory-order reasoning (S7.3);
   the block appears in exactly one list (Inv 6).
4. L5 occupancy is updated by L4's accounting;
   no layer skips to L6.

### 11.5 Trim and stats

Trim: L1 trim entry -> L2 admission of the
request -> L4 identifies releasable spans -> L5
combines / prepares return -> L6 unmaps or
advises reclamation. Stats: L1 stats entry reads
a snapshot served by L7 from its fixed slots;
L7 calls nothing, and the snapshot drives no
admission or allocation decision (Inv 8).

Realloc: shown only as 'per A-008 contract,
S6.1'. This document invents no realloc
semantics, copy rule, or zero-size behaviour.

## 12. Cross-cutting constraints

- Bootstrap (S6.3): allocation-free startup and
  diagnostics have NO layer in S7.1. Honestly:
  bootstrap sits below L1, as a pre-layer concern.
  A-009 records this; it does not invent a layer.
  Startup must not allocate via the allocator it
  initialises, and error reporting must avoid
  unbounded dynamic allocation (S6.3).
- Threading (S7.3): begin simple and coarse-
  locked; allocator / freer threads may differ;
  thread termination drains, transfers, or
  reclaims local state; bounded caches protect
  low-RAM devices; ARM weak-model ownership and
  memory order are stated, never assumed.
- Rseq (S7.4): a later, optional optimisation
  only. Probing of kernel / libc ownership, ABI,
  registration, and fallback is assigned to L6
  (A9-7). The reliable path must work with rseq
  unavailable, blocked, or disabled (S7.4).
- Large / VA (S7.5): checked arithmetic; runtime
  page-size discovery; no hardcoded VA width;
  MTE, PAC, and top-byte-ignore are L6
  compatibility concerns, not universal guarantees.

## 13. Invariant-to-layer map (S9.1, all ten)

| Inv (S9.1) | Short form | Owner | Contributes |
|---|---|---|---|
| 1 | One owner, non-overlapping | L4 | L1, L3 |
| 2 | Metadata truth / lifetime | L5 | L4 |
| 3 | No size/alignment overflow | L2 | L1 |
| 4 | free(NULL) per contract | L1 | L4 |
| 5 | Failed realloc preserves | L1 | L2, L4 |
| 6 | Handoff; one list only | L4 | L3 |
| 7 | Bounded slow path | L4 | L3, L5, L6 |
| 8 | Stats drive no decisions | L7 | L2 |
| 9 | Sync failure not hidden | L4 | L6 |
| 10 | Fallback preserves or fails | L4 | L1, L5 |

Wording above is abbreviated; S9.1 governs.

## 14. Evidence status and promotion criteria

Every layer — L1, L2, L3, L4, L5, L6, L7 — is
Proposed. Nothing in this document promotes any
layer. Promotion requires, per layer (Sections
4 to 10, part (i)): the named S10 test rows
passing in a named environment, and the S8 phase
exits — Phase 3 / 4 for the minimal allocator and
ARM foundation, Phase 5 for ABI proof, Phases 7
/ 8 for L3 / L4 forms, Phase 10 for diagnostics.
Phase phasing gap, recorded honestly: Phase 3
builds only namespaced alloc / free via OS
mappings under a simple lock. Distinct L3 / L4
forms arrive in Phases 7 / 8, diagnostics in
Phase 10, rseq in Phase 11 (S8). S7.1 presents
all seven layers at once with no maturity
annotation; this section supplies that annotation
externally and changes no layer's status.

## 15. What this document does not decide

It does not decide: the S6 symbol list or any
contract semantics (A-008); the S5 target matrix
(A-003); the S3.3 integration map (A-007); the
implementation language (companion document);
any budget value; any page-size value; any size
classes; any benchmark, tuning, or performance
target; repository layout or directories; or
whether replacing tcmalloc fixes the Mojo issue.

## 16. Inconsistencies and open divergences

- A9-1: S13 layout (lowercase `docs/architecture.md`,
  `docs/decisions/`) vs banked UPPER_SNAKE; extends N-5/N-6.
- A9-2: S7.2 heading promises a decision its text never
  makes (N-2); discharged by companion doc + proposed D-006.
- A9-3: S7.2 rests on Mojo evidence that does not exist:
  S15 Q1/Q2/Q4/Q5 open, S3 gate unmet, A-007 blocked for
  want of a reproducible Mojo failure packet.
- A9-4: S15 register (13 questions) omits the language
  question entirely — no C11 / C++17 entry; pattern of N-3.
- A9-5: C++ placement has no S7.1 home (S6.1 contemplates
  operator new/delete; 7 layers, no shim); companion places
  it inside L1, no 8th layer. Roadmap does not state this.
- A9-6: L3 exists before its justification (S7.1.3 'subject
  to measured need' + S7.3 gate vs unconditional drawing);
  initial form is simplest / pass-through.
- A9-7: Rseq (S7.4) is owned by no S7.1 layer; assigned
  to L6 by A-009, not by the roadmap.
- A9-8: D-005 closure says 'A-009 appends', but
  DECISION_LEDGER.md is absent from team-09 base b0b8374;
  resolved via companion Appendix C + keeper banking.
- A9-9: Stats ownership split unstated (S7.1.1 L1 API vs
  S7.1.7 L7 counters); L1 exports / L7 owns is A-009's call.
- A9-10: 'Working proof' is demanded by S7.2, defined
  nowhere; S2 warns compile is not interposition proof.
- A9-11: S13 code layout src/api|core|platform (3 dirs)
  vs 7 layers; mapping deferred per S13; no dirs created.
- A9-12: L2 budget has no number anywhere (S7.1.2 vs
  S15 Q7); optional, unset by default, none proposed here.
- A9-13: Page size must not be hardcoded as a target fact
  in this doc; runtime discovery sits in L5 / L6 (S7.5, S5).
