# Phase 3 — Minimal Allocator Design: Namespaced, Mapping-Backed

> **HONESTY BANNER — DESIGN ONLY.** This document is design only.
> Nothing in it is implemented, nothing in it is measured, and
> nothing in it describes existing behaviour. Status: **Proposed**.
> Issue: **MIM-005** — a Roadmap S13 suggested issue ID, not a
> contract ID. Base commit: **5ea3a8d**. Slice: **B-006, Part 1**
> (Sections 1–6). Every contract row and every architecture layer
> cited here is itself **Proposed**; no cited row is adopted or
> implemented fact (scout INC-B006-10).

## 1. Scope and headline (design)

This Part 1 designs, at design level only, the smallest Phase 3
form of MimirAlloc: a namespaced, mapping-backed allocator whose
only public entry points are the design labels `mimir_malloc`
and `mimir_free`, placed at **L1 — Public boundary**.

- The names `mimir_malloc` / `mimir_free` are **design labels
  only**. The namespaced `mimir_*` API proposed in Roadmap S7.1
  is outside the A-008 (contracts.md) S6 scope and is recorded
  there as **[UNRESOLVED: U-03]**, with Appendix A stability
  Unassigned (contracts.md S1.3; scout INC-B006-1). This design
  therefore **never claims an ABI**: no signature, calling
  convention, symbol version, or stability promise is stated
  or implied for either name.
- The design maps these labels **by analogy only** to the core
  S6.1 rows: `mimir_malloc` by analogy to **MA-6.1-MALLOC-01**
  (success: non-null, suitably aligned, distinct storage),
  **MA-6.1-MALLOC-02** (failure: null, never an unallocated
  pointer), and **MA-6.1-MALLOC-04** (oversize = failure);
  `mimir_free` by analogy to **MA-6.1-FREE-01** (free a live
  in-scope pointer, retire exactly once),
  **MA-6.1-FREE-02** (free(NULL) is a no-op),
  **MA-6.1-FREE-03** (non-allocator / interior pointer: UB),
  and **MA-6.1-FREE-04** (double free: UB). That mapping is
  proposed analogy, not a direct namespaced contract.
- Ownership language follows **MA-6.2-001** (free only via a
  compatible allocator/API family, retire exactly once),
  **MA-6.2-010** (free(NULL) no-op, C ABI profile), and
  **MA-6.2-020** (an unsatisfiable request uses a defined
  failure channel and returns no partially constructed block).
- Deliberately **not** in Part 1 / B-006: checked-arithmetic
  and explicit-alignment detail, initialisation and error
  detail (deferred to B-007), and `calloc` / `realloc`
  semantics (deferred to B-008). Phase 3 exit evidence is a
  B-009 / later-implementation matter (scout INC-B006-4).

## 2. Source basis and terminology (design)

Source basis, all Proposed: the A-008 contract set in
`docs/contracts.md` (91 primary MA-* rows, counted in the
B-006 scout), the L1–L7 layers in
`docs/ARCHITECTURE_LAYERS.md` (ARCH S7.1), Roadmap Phase 3
(S8) and MIM-005 (S13), and invariants S9.1 — in particular
Invariant 1 (every live allocation has one valid owner and
non-overlapping usable storage) and Invariant 2 (metadata
describes actual allocated memory), which **L5** owns.

Terminology is bound by **MA-6.2-040** and **MA-6.2-041**:

- The terms *owner*, *block*, *internal logical page*, *system
  page size*, *span*, and *huge page* are used exactly as in
  the contracts.md definitions box (**MA-6.2-040**, L5/L6).
- Any size, count, or limit expressed in pages **must** be
  qualified as internal logical page or system page size
  before it is treated as a contract value
  (**MA-6.2-041**). Unqualified "page" wording is a defect
  in this design, not shorthand.
- An internal logical page is an allocator concept; a system
  page size is a host property obtained from the host. No
  rule in this design equates the two (definitions box,
  **MA-6.2-041**, L6).

## 3. Target and page size (design)

- Target for this design slice: Linux on aarch64 (ARM64), as
  probed on Raspberry Pi device purpleviking1 (**L6** — OS
  abstraction, the only layer with platform-specific code).
- **Observed fact, this Pi only:** `getconf PAGE_SIZE`
  reported **16384** (16 KiB system page size) on this Pi.
  Recorded as an observed fact, not an assumption for all
  targets (assignment wording; SUPPORTED_TARGETS record).
- **Contrast:** the SM-X400 tablet probed a **4096** (4 KiB)
  system page size. Same design, different system page size —
  proof by example that the value is a host property
  (**MA-6.2-041**, **L6**).
- **Design rule (L6):** the system page size **must** be
  discovered at runtime — for example via
  `sysconf(_SC_PAGESIZE)`, checked for failure — on every
  target. It is **never hardcoded** in the allocator design:
  no literal 16384, no literal 4096, and in particular no
  hardcoded 4096 alignment assumption (ARCH A9-13 line in
  the B-006 scout; **L5** forbids a hardcoded page size).
- Open, not resolved by this design: **V-07 / U-15** — the
  numeric default alignment guarantee and its relation to
  the target system page size remain UNRESOLVED in
  contracts.md, and **MA-6.1-MALLOC-01** states only the
  non-numeric suitably-aligned claim. Roadmap Phase 4, not
  Phase 3, measures 4 KiB and 16 KiB targets; this design
  infers nothing from the Pi value (scout INC-B006-3).

## 4. Collapsed Phase 3 form of L1–L7 (design)

Full L1–L7 is the S7.1 target form (Phases 7/8/10/11 add
distinct L3/L4 forms, diagnostics, and the rest). Phase 3
collapses it, honestly, as follows (ARCH S14; scout
INC-B006-8):

| Layer | Phase 3 collapsed form (design) |
|---|---|
| **L1** Public boundary | Only `mimir_malloc` / `mimir_free` as design labels (S1 above); no stats/trim entry points claimed. By analogy: **MA-6.1-MALLOC-***, **MA-6.1-FREE-***. |
| **L2** Admission + checked arithmetic | Present as a gate: reject what cannot be admitted, pass admitted requests downward. Detail of checked arithmetic is B-007. By analogy: **MA-6.1-MALLOC-04**, **MA-6.2-020**, **MA-6.2-024**. |
| **L3** Small-object fast path | **Pass-through only** to L4 — its initial honest form (ARCH S7.1, A9-6 line). No cache, no thread-local lists in Phase 3. |
| **L4** Central ownership | **Degenerate:** no size classes yet (those arrive Phases 7/8). Owns ownership bookkeeping at design level and is the sole client of L5. By analogy: **MA-6.2-001**, **MA-6.2-023**, **MA-6.2-054**; S9.1 Invariants 1 and 6. |
| **L5** Page/span manager | **Degenerate:** one mapping per allocation (mapping-per-allocation is acceptable in Phase 3 **only** as an explicitly experimental correctness baseline, Roadmap Phase 3 text). Acquire / return mappings via L6 only; maintain metadata and occupancy (S9.1 Invariant 2 owner). |
| **L6** OS abstraction | Present in full as a boundary: runtime system-page-size discovery and mapping / unmapping requests (S5 below). Only layer with platform-specific code. |
| **L7** Optional observability | **Compiled out** in Phase 3. The allocator is complete with L7 compiled out; L7 is a sidecar, never in-line, and no counter emission is designed here. |

Dependency law, unchanged by the collapse (design):
request flow **L1→L2→(L3|L4)→L5→L6**; L3 miss / pass-through
falls to L4; L4 is the only client of L5; L5 is the only
policy client of L6; free flow is **L1→L4** (owner
determination). **No layer skips to L6.** The sole sanctioned
non-adjacent dependency is L3/L4 consuming L6 threading
primitives via L6's published interface — not used by the
mapping design in S5, and recorded here only so the collapse
does not silently invent a direct L5→OS or L4→OS edge
(scout INC-B006-5).

## 5. OS mappings (design, L6 via L5)

All mapping requests flow **L5→L6**; L6 is the only layer
that touches the OS (**L5** is its only policy client).
The concrete Linux/aarch64 forms below are **design
choices for L6**, not contract citations: contracts.md
S1.3 expressly puts system-call and platform-service
usage, page management, and mapping policy in
**non-scope**, so this design **invents no contract
citation** for any syscall, flag, or protection value.

- **Acquire (design, L6):** `mmap` with a null hint address,
  length as computed in S6, protection
  `PROT_READ | PROT_WRITE`, and flags
  `MAP_PRIVATE | MAP_ANONYMOUS`. **No `MAP_FIXED`** and no
  `MAP_FIXED_NOREPLACE`: the allocator never overwrites an
  existing mapping and never demands a specific address.
  Failure is the `mmap` failure return, surfaced by L6 to
  L5 as an explicit error, and by analogy upward as the
  **MA-6.1-MALLOC-02** / **MA-6.2-020** defined failure
  (null, no partially constructed block, L1/L2).
- **Release (design, L6):** `munmap` over exactly the
  acquired address and length, issued only from L5 via
  L6 on the free path (free flow L1→L4→L5→L6), after L4
  has retired the block exactly once — by analogy
  **MA-6.1-FREE-01**, **MA-6.2-001** (L4).
- **All-or-nothing (design, L5/L6):** a single `mmap`
  either acquires the whole requested extent or acquires
  nothing; there is no partial mapping to roll back from
  one call. Partiality can arise **only in multi-step
  sequences** (for example acquire-then-record-metadata),
  and any such sequence must roll back or abandon its
  internal steps and publish no half-owned block —
  **MA-6.2-024** (multi-step failure), S9.1 Invariant 1
  (L4/L5).
- No other OS mechanism is designed in Part 1: no huge
  pages (a host/mapping property, **MA-6.2-040**, L6),
  no reclamation advice, no growth/shrink policy. Each
  would need its own cited design slice.

## 6. Granularity and alignment (design)

- **Granularity (design, L5):** one allocation is backed
  by a whole number of system pages, where "system page"
  means the runtime-discovered system page size of S3 —
  qualified per **MA-6.2-041** (never an unqualified page
  count, never an internal logical page silently equated
  to a system page, L5/L6).
- **Total mapping size (design, L5):**
  `total_mapped = page_round_up(metadata_reserve + requested_size)`
  where `page_round_up` rounds up to a multiple of the
  runtime-discovered system page size, `requested_size` is
  the size admitted at L2, and `metadata_reserve` is the
  design-level reserve for L5 metadata/occupancy records
  (S9.1 Invariant 2, **L5**). The arithmetic form is stated
  here only as a design formula; overflow-checked
  evaluation of it is B-007 detail (by analogy
  **MA-6.2-024**, S9.1 Invariant 3, L2).
- Because the mapping base is system-page aligned by the
  OS mapping design in S5 and the usable block is placed
  inside that mapping by L5, an ordinary allocation meets
  the non-numeric suitably-aligned claim only **by
  analogy** to **MA-6.1-MALLOC-01** (L1/L5). No numeric
  alignment value is claimed: V-07 / U-15 are open (S3).
- **Explicit alignment is deferred to B-007 (design):**
  requests carrying an explicit alignment
  (`aligned_alloc`- / `posix_memalign`-shaped requests)
  are **not** designed in Part 1. Their validity and any
  size-multiple rule are target-specific and unresolved —
  **V-03 / U-11** (by analogy **MA-6.1-ALIGNED-01..05**,
  **MA-6.1-POSIXMA-01..05**, L2) — compounded by the open
  page-size relation in **U-15**. Part 1 must not be read
  as promising aligned-allocation behaviour of any kind.

---

*End of B-006 Part 1 (Sections 1–6). Design only; Proposed.
Metadata placement/layout, lock design, and free/used
bookkeeping detail follow in Part 2 of this document.*
# B-006 Part 2 -- Metadata, Bookkeeping, Lock, Failure, Flows

Slice: TEAM-06 B-006, Phase 3 design -- namespaced mapping-backed
minimal allocator (Roadmap Phase 3; issue MIM-005).
Status: DESIGN ONLY. Nothing in this part is implemented, measured,
adopted, or shipped. Every MA-* contract cited is Proposed in
docs/contracts.md (A-008); every layer cited is Proposed in
docs/ARCHITECTURE_LAYERS.md. No performance claim is made.
Scope of this part: sections S7 to S11. Sections S1 to S6 (Part 1)
cover scope, terms, mapping acquisition, granularity, and alignment.
Language: portable C11 core, per decision D-006 (L1-L6 below).
Namespaced entry points: mimir_malloc / mimir_free at L1. NOTE:
contracts.md gives the namespaced pair no direct contract rows;
they are [UNRESOLVED: U-03]. Every mapping in this part to the core
MA-6.1-MALLOC-* / MA-6.1-FREE-* rows is therefore by analogy only
and is Proposed, not a direct namespaced contract (INC-B006-1,
INC-B006-9). MIM-005 is a Roadmap S13 issue ID, not a contract ID.
Experimental posture: mapping-per-allocation is acceptable only as
an explicitly experimental correctness baseline (Roadmap Phase 3);
it must not be presented as a performance allocator.

## S7. Metadata placement and layout

### S7.1 Decision: Option A -- in-mapping reserve page (CHOSEN)

Each OS mapping carries its own descriptor in a reserve page at
the start of that same mapping. The mapping is laid out, at design
level only, as:

  [ reserve page | usable region ........................ ]
  ^ mapping_base   ^ mapping_base + usable_offset = user pointer

- usable_offset is one whole system page: the reserve occupies
  exactly one system page, discovered at runtime via L6
  (sysconf(_SC_PAGESIZE), checked for failure). On this Pi the
  observed page size is 16384 bytes (16 KiB); that is a fact about
  this Pi only, never a hardcoded constant and never assumed for
  all targets (INC-B006-3; MA-6.2-041: every page quantity is
  qualified as system page size here, not an internal logical
  page). Owners: L5 requests the layout, L6 discovers the page
  size and performs the mapping; L5 is the sole policy client
  of L6 and no layer skips to L6.
- The user pointer is therefore page-aligned by construction in
  this baseline, which satisfies the alignment need of the core
  success rows by construction rather than by later adjustment:
  MA-6.1-MALLOC-01 (success: non-null, suitably aligned, distinct
  storage), by analogy for mimir_malloc (see header note, U-03).
- The descriptor lives for exactly as long as its mapping lives
  and for no longer. It is created after a successful mapping,
  published only when fully written (S8, S11), consulted on free,
  unlinked, and then destroyed together with the mapping by the
  single munmap that releases the whole mapping (S8.2). It is
  never copied out to longer-lived storage, and it is never
  accessed after that munmap.
  This is a direct design discharge of Invariant 2 (Roadmap
  S9.1): "Metadata describes actual allocated memory and is not
  accessed after its own lifetime ends." It is also the reason
  the descriptor address is derivable from the user pointer by
  subtracting usable_offset, with no external lookup structure
  needed on the free path (L5 owns metadata and occupancy;
  Invariant 2 owner per ARCHITECTURE_LAYERS S7.1).

### S7.2 Option B -- separate metadata table (REJECTED)

A separate descriptor table, held apart from the mappings it
describes (for example a central array, list arena, or hash table
allocated as its own storage), is rejected for this baseline on
bootstrap grounds, not on taste:

1. Circularity. Storage for the table would itself have to come
   from somewhere. If it came from the allocator's normal path,
   describing an allocation would require an allocation, which
   would require a descriptor, without end. MA-6.3-002 forbids
   exactly this for bootstrap storage: bootstrap initialisation
   storage MUST NOT come from the allocator's normal path.
   MA-6.3-003 adds that init must not self-allocate recursively
   and must terminate without a prior completed init.
2. Growth bound. MA-6.3-012: initialisation bookkeeping that
   grows with the number of allocations or threads MUST be held
   in fixed, preallocated, or bootstrap-owned storage only.
   A separate table that grows with live allocations is such
   bookkeeping. Making it conform would force a fixed-size,
   preallocated, bootstrap-owned table, which then imposes a
   fixed cap on live allocations unrelated to available memory,
   and a table-full failure mode the in-mapping design does
   not have. That is a worse baseline, not a simpler one.
3. Lifetime decoupling. A table entry can outlive, be reused
   independently of, or be indexed after its mapping is gone.
   Each of those is an Invariant 2 hazard that Option A removes
   structurally: in Option A the descriptor cannot outlive its
   mapping because it is inside it (S7.1).
   Together these are scout finding INC-B006-6: metadata
   placement cannot allocate its own metadata via the allocator
   it describes; in-mapping or static/bootstrap metadata is
   required. Option A is the in-mapping branch of that rule.

What IS static / bootstrap-owned in the chosen design, and is
not a counterexample to the above: the registry anchors and
counters of S8 (list head, live-mapping count, live-byte count)
and the lock object of S9. Those are a fixed, small, constant
amount of storage, independent of the number of allocations,
held in static / bootstrap-owned storage owned below L1 (the
bootstrap sits below L1; it is not an S7.1 layer). They hold no
per-allocation payload. Per-allocation metadata is exclusively
the in-mapping descriptor. Owners: bootstrap (storage), L4
(registry discipline, S8), L5 (descriptor contents).

### S7.3 Descriptor layout -- 96 bytes, design level only

The descriptor occupies the first 96 bytes of the reserve page.
The remaining bytes of the reserve page are unused in this
baseline (no packing of further descriptors into the page; one
descriptor per mapping, one mapping per allocation).

  Offset  Size  Field
  ------  ----  ------------------------------------------------
     0      8   magic
     8      8   mapping_base
    16      8   mapping_length
    24      8   usable_offset
    32      8   requested_size
    40      8   usable_size
    48      4   state
    52      4   flags
    56      8   list_next
    64      8   list_prev
    72     24   reserved (must be zero on publication)
    --     ---
   Total   96   bytes

There is deliberately no sequence / generation field in this
layout. An earlier draft idea for such a field is dropped; its
bytes are folded into reserved, keeping the total at 96 bytes
and keeping every field at its natural alignment on the target
word size assumed at design level (8-byte fields 8-aligned,
4-byte fields 4-aligned, list pointers 8-aligned).

Field meanings (design semantics, not a wire format):
- magic: a fixed design constant identifying a live MimirAlloc
  descriptor of this layout revision. Its sole uses are the
  free-path sanity check of S8.2 and the corruption response of
  S10 F-09. It is not a security boundary and not a proof of
  provenance: MA-6.2-005 (provenance cannot be established).
- mapping_base: the exact address handed to munmap; equal to
  descriptor address in this layout. Recorded explicitly so the
  free path never has to re-derive the release address from an
  unvalidated pointer.
- mapping_length: the exact length handed to munmap; reserve
  page plus usable region, in bytes.
- usable_offset: mapping_base to user pointer distance; one
  system page in this baseline (S7.1).
- requested_size: the size the caller asked for, after L2
  admission. Kept for accounting and later diagnostics; it is
  not the release length and must never be used as one.
- usable_size: mapping_length minus usable_offset. At least
  requested_size for a published descriptor.
- state: LIVE or RETIRED, design-level enumeration. Only LIVE
  descriptors are on the registry (S8). The transition to
  RETIRED happens under the lock, immediately before unlink
  and munmap, and no code may follow a descriptor after munmap
  (Invariant 2).
- flags: design-level bitfield, no bits assigned in this
  baseline except "bootstrap-served" reserved for the bootstrap
  interaction in MA-6.3-006 / S10 F-08. Unassigned bits are zero.
- list_next / list_prev: registry links of S8, meaningful only
  while the descriptor is on the registry and only under the
  lock of S9.
- reserved: zero on publication; available for a future design
  revision without changing the 96-byte total.

PRECISION STATEMENT: this layout is a DESIGN, not a measured
ABI. No implementation exists; no sizeof, offset, alignment,
endianness, or packing fact is claimed as measured, compiled,
or stable. The offsets above are design intent for a later C11
implementation (D-006) to express and then verify; until such
an implementation and its tests exist, no caller, test, or
other slice may rely on these byte offsets as an ABI. Layer
owner of the layout: L5. Page-size input: L6 (S7.1).

## S8. Free / used bookkeeping

### S8.1 Registry of live descriptors (design)

Used memory is known in exactly one way in this baseline:
the set of LIVE descriptors. There are no free lists of reusable
memory, no span occupancy bitmaps, and no size classes in
Phase 3 (those forms belong to Phases 7/8; INC-B006-8).

- Structure: one doubly-linked list, the registry, threading
  list_next / list_prev of S7.3 through every LIVE descriptor.
  A descriptor is inserted once, at publication (after its
  mapping exists and its fields are fully written), and removed
  once, at free, before its mapping is released. A descriptor is
  on the registry if and only if its state is LIVE. This is the
  bookkeeping form of Invariant 1 (one valid owner, non-
  overlapping usable storage) and Invariant 6 (no list node
  appears simultaneously in incompatible lists -- here there is
  only one list, and membership is single and exclusive).
- Counters: two design-level counters, kept beside the list
  head in the static / bootstrap-owned anchors of S7.2:
  (a) live_mappings -- number of descriptors on the registry;
  (b) live_bytes -- sum of mapping_length over the registry
  (total bytes currently obtained from the OS; requested and
  usable totals may be added by a later diagnostics design,
  Phase 10, and are not specified here).
  Counters are updated in the same locked section that inserts
  or removes the descriptor, so list and counters can never be
  observed mid-update by another thread (S9). Counters are
  bookkeeping only: Invariant 8 -- statistics do not drive
  unsafe decisions if they are approximate or stale; here they
  drive no allocation decision at all. Owner: L4 (central
  ownership and accounting discipline). Storage of the anchors:
  bootstrap-owned static storage (S7.2). Descriptor contents:
  L5. Mapping operations behind both: L6, via L5 only.
- Empty registry, zero counters: the defined starting state,
  reached without any allocation (bootstrap, S10 F-08).

### S8.2 Free sequence (design)

mimir_free(p), by analogy to MA-6.1-FREE-01 (free a live
in-scope pointer, retire exactly once) and MA-6.2-001 (free
only via a compatible allocator / API family, retire exactly
once), proceeds as in the end-to-end flow of S11.2. In brief,
all under the single lock of S9:

1. p == NULL: no-op, return immediately, before any locking or
   descriptor access. MA-6.1-FREE-02; MA-6.2-010 (C ABI profile).
   Invariant 4: freeing a null pointer behaves according to
   the target contract -- here, this defined no-op.
2. Derive the candidate descriptor address from p by
   subtracting the baseline usable_offset (one system page),
   then VALIDATE before trusting any field: magic must equal
   the design constant, state must be LIVE, mapping_base plus
   usable_offset must equal p, and mapping_length must be
   consistent with mapping_base and usable_size. Validation
   reads only the candidate reserve page.
3. On validation success: mark state RETIRED, unlink the
   descriptor from the registry, decrement live_mappings and
   live_bytes, capture mapping_base and mapping_length in
   locals, release the lock only as specified in S9.4 (the
   baseline holds the lock across munmap; see S9.4 decision),
   and munmap the WHOLE mapping -- never a part of it.
   After munmap, the descriptor, and every pointer into the
   mapping, are dead and are never dereferenced (Invariant 2).
4. On validation failure: this is not a defined free. See
   S8.3 and S10 F-09. The mapping is not unmapped, the registry
   is not modified, and no counter is changed on suspicion.

Retirement is exactly once by construction of steps 2-3: a
second free of the same pointer finds no LIVE descriptor at
the derived address in the defined case where the memory is
gone; where the address has been reused by the OS for a new
mapping, detection limits are those of S8.3, stated honestly.

### S8.3 Invalid frees: undefined, debug-detectable only

The following are Undefined Behaviour. The design claims no
recovery, no defined return, and no safe continuation for them
in a normal build:

- foreign pointer (not produced by this allocator):
  MA-6.1-FREE-03; MA-6.2-002 (free via a different allocator is
  UB; no silent treatment as its own) and MA-6.2-030.
- interior pointer (into a live allocation, not its start):
  MA-6.1-FREE-03; MA-6.2-030.
- double free: MA-6.1-FREE-04; MA-6.2-032.
- use after free generally: MA-6.2-031.

A DEBUG build MAY detect some of these -- via the magic and
state checks of S8.2 step 2, and via registry membership --
and on detection MUST preserve evidence, stop / report, and
never continue as if the free were valid: MA-6.2-033
[COND: debug]. Where metadata corruption is found internally,
the design MUST NOT claim correction; it preserves evidence
and stops / reports: MA-6.2-034 (see also S10 F-09). Detection
is explicitly best-effort: a foreign pointer whose bytes happen
to satisfy the checks, an interior pointer into the reserve
page, and a double free after OS address reuse may all evade
detection. No text in this design may be read as making any
invalid free Defined. Owner: L4 (owner determination on the
free path, L1 -> L4); validation data: L5.

### S8.4 Cross-thread free is Defined

A block allocated on one thread may be freed on a different
thread. That handoff preserves ownership exactly, and distinct
live blocks stay non-overlapping: MA-6.2-054; test hook C-009
(repeated cross-thread alloc / free cycles complete, subsequent
allocations remain valid). In this baseline the property rests
on two facts only: all registry state is under the one global
lock (S9), and there is no thread-local cache to drain, hand
over, or reconcile (L3 is pass-through / absent in Phase 3,
INC-B006-8). Introducing any thread-local state later re-opens
this section. Owner: L4 (remote-free handling).

### S8.5 Deliberately no reuse

Freed memory is returned to the OS in full, every time. This
baseline has: no reuse of freed mappings for later requests,
no splitting of a mapping to serve a smaller request from a
larger freed one, no coalescing of adjacent freed mappings,
no size classes, no free lists of usable memory, and no trim
or caching policy. A later allocation is a fresh mapping or
a failure; it is never satisfied from retained freed memory.
All of that machinery is deferred -- size-class and free-span
forms are Phase 7 / Phase 8 work in the layer phasing, not
omissions to be improvised inside this baseline. The cost is
accepted by the Roadmap Phase 3 text itself: this is a slow,
mapping-per-allocation, experimental correctness baseline.

## S9. The simple lock

### S9.1 One global coarse mutex, via L6 primitives

Synchronisation in this baseline is ONE global, coarse mutex.
There is no second lock, no read-write split, no per-mapping
lock, and no lock ordering table -- with one lock, no ordering
question exists. Layers and ownership, stated explicitly
because lock placement spans three layers (INC-B006-5):

- L6 PROVIDES the primitive, through its published threading-
  primitive interface. L6 is the only layer with platform-
  specific code, so the primitive type and its operations are
  L6 facts, consumed -- never re-implemented -- above.
- L4 OWNS the synchronisation discipline: it decides what the
  lock guards (S9.2), when it is taken and released (S9.4),
  and what happens on failure (S9.5). Central ownership owns
  synchronisation per ARCHITECTURE_LAYERS S7.1 (L4).
- L5 OWNS the metadata the lock protects (descriptors, registry
  data as stored), but L5 does NOT own, embed, or directly call
  the lock. A design that placed the lock inside L5 alone, or
  that called pthread / OS primitives directly from L5, would
  violate the architecture dependency law: L5 -> L6 is for
  mapping policy requests only, and threading primitives reach
  L3 / L4 via the published L6 interface (the sole sanctioned
  non-adjacent dependency). The lock therefore does NOT live
  in L5 alone, and no L5 code names a platform primitive.

Primitive choice (design decision, recorded with its loser):
a POSIX pthread mutex (pthread_mutex_t, normal / default type,
statically initialised where the target permits) is RECOMMENDED
over a C11 mtx_t from <threads.h>. Reasons, design level only:
(a) the baseline target is Linux ARM64, where pthread is the
native, always-present threading interface that L6 wraps for
every other primitive need, so one primitive family is carried;
(b) pthread mutex error behaviour and static initialisation are
the better-understood path for the bootstrap constraints of
S10 F-08 (no allocation, no constructor dependency). Loser
recorded: C11 mtx_t -- portable in principle under D-006, but
rejected for this baseline because its availability and error
reporting on the target rest on the C library's <threads.h>
support, adding a second, less certain primitive family for no
correctness gain. This choice is revisitable by a later design
with target evidence; it is not a measured result.

The lock object itself lives in the fixed static / bootstrap-
owned anchors of S7.2, never in a mapping and never in storage
obtained through the allocator (MA-6.3-002, MA-6.3-012).

### S9.2 What the lock guards -- and what it does not

The lock guards, and only:
- every descriptor field of S7.3 while its descriptor is LIVE
  or being published / retired;
- the registry list links, head, live_mappings, and live_bytes
  of S8.1;
- the state transitions LIVE -> RETIRED and unpublished ->
  LIVE, so no thread can observe a half-written descriptor or
  a half-updated counter pair.

The lock NEVER guards user bytes. The usable region of a live
allocation is the caller's alone from the return of mimir_malloc
until mimir_free is called; the allocator neither reads nor
writes it under the lock (or at all, in this baseline, apart
from no touch whatsoever -- zeroing belongs to calloc, deferred
to B-008). Synchronising access to user data is the caller's
contract, not the allocator's. Owners: L4 (guard discipline),
L5 (guarded metadata), L1 (the boundary at which user bytes
pass to and from the caller).

### S9.3 Contention posture: correctness first

This baseline chooses correctness over contention performance,
explicitly and without apology, as Roadmap S7.3 directs (begin
simple, potentially coarse; correctness measurable first):
- no lock-free algorithms, no atomics-based registry, no
  sharding of the registry or counters, no thread-local caches,
  and no lock elision or try-lock-and-drop fast paths;
- every allocation and every free serialises its (short)
  registry section through the one lock, plus -- per the S9.4
  decision -- the mapping syscall made inside that section;
- per-thread caching, sharding, or finer locks may be proposed
  only by a later phase, only on benchmark evidence, and only
  with the ABA / reclamation / false-sharing reasoning S7.3
  requires before any lock-free step. None is claimed here.
Expected consequence, stated as design expectation and not as
measurement: throughput under many threads will be poor. That
is the accepted price of the Phase 3 experimental baseline.

### S9.4 Holding rules -- including the mmap / munmap decision

Never held:
1. NEVER held across a return to the caller. The lock is
   released before mimir_malloc returns (success or failure)
   and before mimir_free returns. No caller-visible state is
   "protected" between calls by a held lock.
2. NEVER held across any callback into code outside the
   allocator (pressure, diagnostic, or user callbacks of any
   kind; none exist in this baseline, and none may be added
   under the lock). Pressure callbacks are not reservations
   (L2 note, ARCHITECTURE_LAYERS) and gain no lock rights.
3. NEVER held across error reporting or logging. A failure
   detected under the lock is recorded in locals in a bounded,
   preallocated form, the lock is released, and only then is
   anything reported. Grounds: MA-6.3-020 (reporting: no
   unbounded dynamic allocation, completable within a stated
   bound, not from the failing path), MA-6.3-021 (reporting in
   a failed / corrupted state must not depend on that state's
   health; truncated over none), MA-6.3-022 (the bound holds on
   the bootstrap path too), MA-6.3-023 (same bound per report
   under cascading errors). Reporting while holding the lock
   would also invite re-entry deadlocks through any logging
   path that allocates (MA-6.3-004 discipline).
4. NEVER claimed to be held, released, or usable from a signal
   handler. No function in this design is claimed async-signal-
   safe: MA-6.3-030 (no such claim without a separate individual
   proof and contract row), MA-6.3-031 (docs / headers /
   diagnostics must not state or imply signal safety without
   that proof), MA-6.3-032 (signal while the lock is held or
   state is in transition: no promise; a handler must not call
   the allocator to inspect or repair). Fork behaviour is
   likewise NOT claimed Defined for this lock-holding design:
   MA-6.3-040..042 and unresolved U-17 (INC-B006-7).

DECISION -- held across mmap / munmap in this baseline:
the lock IS held across the mmap call on the allocation path
and across the munmap call on the free path. Rationale: with
one lock and an in-mapping descriptor, the mapping, its
descriptor publication, and its registry insertion are one
atomic step with respect to every other thread; likewise
retirement, unlinking, and unmapping are one atomic step.
Releasing around the syscall would open a window in which a
mapping exists but is unpublished (or is published but already
unmapped), requiring a third descriptor state, a pending list,
and re-validation after re-acquisition -- exactly the state
machinery a minimal baseline must not grow. The known cost --
all threads serialise behind any slow mapping syscall -- is
accepted under S9.3. RECORDED RECONSIDERATION: dropping the
lock around the syscalls, with an explicit pending state, is
a Phase 8 reconsideration item, to be decided then on measured
contention evidence. It is not part of this baseline, and no
claim in this part depends on it.

### S9.5 Lock failure

A failure to acquire, release, or initialise the primitive, or
any primitive error return, is a synchronisation failure under
Invariant 9: it is reported or terminates safely in accordance
with the contract, and it is NEVER hidden by returning
overlapping memory, by proceeding without the lock, by retrying
silently forever, or by publishing a descriptor whose registry
state is uncertain. Concretely at design level: the allocation
in progress fails without returning storage (by analogy to
MA-6.1-MALLOC-02: failure returns null, never a pointer to
storage not allocated to the caller; MA-6.2-020: no partially
constructed block; MA-6.2-024: multi-step failure rolls back
or abandons internally, no half-owned block is published), or
the process terminates through the bounded reporting path of
S9.4 item 3. Which of report-and-fail versus terminate applies
to a given primitive error is an implementation decision the
later code must record; silently continuing unlocked is not
an available option in either case. See S10 F-07.

## S10. Failure and partial-mapping behaviour

General laws for every row below: a failed allocation returns
the defined failure channel and passes no ownership
(MA-6.1-MALLOC-02; MA-6.2-020: no partially constructed block;
MA-6.2-024: multi-step failure is rolled back or abandoned
internally, no half-owned block published; Invariant 10: every
fallback either preserves ownership or explicitly fails).
errno IS NOT PROMISED by this design: whether errno is set on
allocation failure, and to what, is target-dependent and
UNRESOLVED (V-02 -> U-10, Open). A later per-target contract
decision may add an errno promise; until then no caller may
rely on errno from mimir_malloc / mimir_free, and no text here
states one. (The posix_memalign-style rule that errno is not
the channel is a different interface's row, MA-6.1-POSIXMA-02,
and is not imported into the namespaced pair.)
Layer shorthand: L2 admits / rejects, L4 owns bookkeeping,
L5 owns metadata, L6 performs (and reports) OS operations.

 ID    Failure / condition                  Design behaviour
 ----- ------------------------------------ ----------------------
 F-01  mmap fails with ENOMEM               Allocation fails:
       (insufficient memory)                return null (analogy:
                                            MA-6.1-MALLOC-02). No
                                            descriptor exists, none
                                            is published, registry
                                            and counters unchanged.
                                            No partial mapping: a
                                            failed mmap maps
                                            nothing. L6 reports,
                                            L5 creates nothing,
                                            L4 changes nothing.

 F-02  Request unrepresentable: size        Rejected at L2, BEFORE
       overflow, or size / alignment        any syscall. No mmap
       that cannot be represented           is attempted. Treated
       (EINVAL / EOVERFLOW class)           as failure under
                                            MA-6.1-MALLOC-04
                                            (oversize = failure
                                            under -02) and
                                            Invariant 3 (size and
                                            alignment calculations
                                            cannot overflow).
                                            Detailed checked-
                                            arithmetic rules are
                                            deferred to B-007; this
                                            row states only that
                                            the rejection exists,
                                            sits at L2, and issues
                                            no OS call.

 F-03  mmap fails with EMFILE or ENFILE     As F-01: allocation
       (descriptor / file limits)           fails, nothing is
                                            published or counted.
                                            The design does not
                                            close, evict, or retry
                                            against the limit;
                                            limits are the
                                            process's, reported by
                                            L6 as a plain failure.

 F-04  mmap fails with EAGAIN               As F-01. No internal
       (temporarily unavailable)            retry loop, bounded or
                                            otherwise, is specified
                                            in this baseline; a
                                            retry policy, if ever
                                            wanted, is a later
                                            design decision, so
                                            that failure latency
                                            here is bounded by one
                                            syscall (Invariant 7:
                                            the slow / failure path
                                            is bounded and testable).

 F-05  Zero-size request                    EXCLUDED from supported
                                            calls in this design.
                                            Behaviour is UNDECIDED:
                                            MA-6.1-MALLOC-03 is
                                            [UNRESOLVED: U-09], as
                                            are the zero-factor and
                                            zero-size aligned rows
                                            (V-01). This design
                                            gives zero-size NO
                                            behaviour: it neither
                                            returns a unique pointer
                                            nor promises null, and
                                            a zero-size call is
                                            outside the explicitly
                                            supported call set the
                                            Phase 3 exit speaks of.
                                            Resolution requires the
                                            per-target decision
                                            recorded against U-09.

 F-06  munmap fails, or reports a partial   The free does NOT
       release, during mimir_free           complete silently. The
                                            descriptor STAYS LIVE:
                                            it is not unlinked,
                                            counters are not
                                            decremented, and the
                                            allocation remains
                                            recorded as owned, so
                                            the failure is visible
                                            and the bookkeeping
                                            still describes actual
                                            memory (Invariant 2).
                                            The failure is reported
                                            through the bounded
                                            path of S9.4 item 3,
                                            after lock release.
                                            The descriptor is never
                                            silently dropped, the
                                            length is never
                                            shortened on paper to
                                            match a partial release,
                                            and no later step treats
                                            the memory as freed.
                                            (Design order in S8.2 /
                                            S11.2 therefore checks
                                            the munmap result before
                                            the retirement is made
                                            final; an implementation
                                            that unlinks first and
                                            discovers the failure
                                            afterwards must restore
                                            the LIVE entry under the
                                            lock -- it must not lose
                                            it.) Owners: L6 reports,
                                            L5 retains metadata,
                                            L4 retains ownership.

 F-07  Lock acquisition / release / init    Synchronisation failure:
       failure (any primitive error)        report or terminate
                                            safely; never proceed
                                            unlocked, never return
                                            overlapping memory.
                                            Invariant 9; S9.5 in
                                            full. An allocation in
                                            progress fails with no
                                            ownership passing
                                            (MA-6.1-MALLOC-02,
                                            MA-6.2-020, -024).

 F-08  Entry before initialisation is       Bootstrap path, below L1.
       complete; reentrant entry during     First entry pre-init is
       init on the same thread; entry       served from the minimal
       from a constructor / loader,         bootstrap path or fails
       pre-main                             as defined -- initialised
                                            state is never assumed:
                                            MA-6.3-001; constructor /
                                            loader entry is the
                                            same case, no hosted
                                            services assumed:
                                            MA-6.3-005. Bootstrap
                                            storage never comes
                                            from the normal path:
                                            MA-6.3-002; init does
                                            not self-allocate
                                            recursively and must
                                            terminate without a
                                            prior completed init:
                                            MA-6.3-003. Reentrant
                                            entry during init:
                                            bootstrap, defer, or
                                            defined failure; init is
                                            never re-entered as if
                                            complete: MA-6.3-004.
                                            No allocator-dependent
                                            C++ container sits on
                                            the init path:
                                            MA-6.3-010; ordering of
                                            allocating static
                                            construction is not
                                            relied on without a
                                            stated mechanism:
                                            MA-6.3-011. A bootstrap-
                                            served block freed after
                                            full init is accepted by
                                            the compatible allocator,
                                            or bootstrap must have
                                            failed it instead:
                                            MA-6.3-006. Test framing:
                                            hook C-010 (watchdog-
                                            bounded bootstrap /
                                            reentrancy). In THIS
                                            baseline the consequence
                                            is narrow and stated
                                            plainly: the registry
                                            anchors and lock of
                                            S7.2 / S9.1 are static,
                                            need no allocation and
                                            no constructor, and the
                                            empty registry is valid
                                            before any init step
                                            runs; any future init
                                            step must preserve that
                                            property or use the
                                            defined-failure branch.

 F-09  Metadata magic or state mismatch     Corruption response,
       found on the free path or in an      debug-first. In a DEBUG
       internal consistency check           build: preserve evidence,
                                            stop / report, never
                                            continue as if valid,
                                            and never claim the
                                            metadata has been
                                            corrected: MA-6.2-033
                                            [COND: debug],
                                            MA-6.2-034. In a normal
                                            build this case is the
                                            UB territory of S8.3
                                            (MA-6.2-030..032) and no
                                            behaviour is promised.
                                            In neither build is the
                                            suspect mapping unmapped
                                            on suspicion, and in
                                            neither build is a
                                            descriptor silently
                                            dropped (compare F-06).

 F-10  A multi-step mapping sequence        Not a baseline shape: the
       completes only in part (design       baseline issues ONE mmap
       guard for any later variant that     per allocation (S7.1), and
       maps in pieces)                      mmap itself is all-or-
                                            nothing for its requested
                                            length. If any later
                                            variant maps in pieces
                                            and a piece fails, every
                                            piece already obtained
                                            for that request MUST be
                                            unmapped, no descriptor
                                            is published, and no
                                            counter changes:
                                            MA-6.2-024 (roll back /
                                            abandon internally, no
                                            half-owned block
                                            published); MA-6.2-020.
                                            A partial mapping is
                                            never returned to a
                                            caller as a smaller
                                            allocation than asked.

 F-11  Page-size discovery fails at L6      Defined failure, no
       (sysconf returns an error /          mapping attempted, no
       an unusable value)                   descriptor created. The
                                            baseline cannot compute
                                            usable_offset or
                                            mapping_length without a
                                            valid system page size,
                                            so it fails the request
                                            rather than guessing --
                                            in particular it never
                                            falls back to a
                                            hardcoded 4096 (or any
                                            other constant): ARCH
                                            rule against hardcoded
                                            page size; INC-B006-3.
                                            Cached discovery, if a
                                            later design adds it,
                                            must preserve this
                                            failure semantics.

 F-12  Error reporting itself cannot        Reporting is bounded and
       allocate, or errors cascade          allocation-free in this
                                            design (S9.4 item 3):
                                            MA-6.3-020 (no unbounded
                                            dynamic allocation,
                                            completable within the
                                            stated bound, not from
                                            the failing path),
                                            MA-6.3-021 (must not
                                            depend on the health of
                                            the failed / corrupted
                                            state; a truncated
                                            report is preferred over
                                            none or an unbounded
                                            one), MA-6.3-022 (bound
                                            holds on the bootstrap
                                            path too, bootstrap
                                            facilities only),
                                            MA-6.3-023 (same bound
                                            per report when errors
                                            repeat or cascade; no
                                            unlimited retained
                                            storage). A reporting
                                            failure never changes
                                            the allocation outcome
                                            already decided, and
                                            never converts a failure
                                            into a returned pointer.

Out of table, stated once: no row above promises errno
(V-02 / U-10, Open), no row promises async-signal safety
(MA-6.3-030..032), and no row defines fork behaviour
(MA-6.3-040..042; U-17, Open; INC-B006-7).

## S11. End-to-end flows

Both flows follow the architecture dependency law without
exception: request flow L1 -> L2 -> (L3 | L4) -> L5 -> L6;
free flow L1 -> L4; no layer skips to L6; the only non-adjacent
use is L4 consuming L6 threading primitives through the
published L6 interface (S9.1). In Phase 3, L3 is pass-through:
it holds no cache and makes no decision (INC-B006-8), and L7,
if present at all, only receives fixed-slot counter emissions
and is never in-line; the allocator is complete with L7
compiled out. Bootstrap state (S7.2, S9.1, F-08) is valid
before either flow can first run.

### S11.1 mimir_malloc(size) -- step by step

 1. L1 (public boundary). The caller enters through the
    namespaced C ABI entry mimir_malloc. L1 performs no
    validation of its own beyond passing the request down;
    ownership and error semantics are those of the interface
    contract cited per step below. [D-006: C11, C ABI.]
 2. L2 (admission and checked arithmetic). Validate the
    request: a representable, satisfiable size is admitted;
    an unrepresentable or overflowing computation is rejected
    here with NO syscall (S10 F-02; MA-6.1-MALLOC-04;
    Invariant 3). Zero-size is not admitted as a supported
    call (S10 F-05; U-09 UNRESOLVED). Detailed arithmetic and
    alignment rules are B-007 scope; this flow requires only
    that admission precedes every later layer. On rejection:
    return null to the caller through L1 (MA-6.1-MALLOC-02,
    by analogy). Flow ends.
 3. L3 (small-object fast path). Pass-through in this phase:
    the request is handed to L4 unchanged. No list is
    consulted, because none exists (INC-B006-8).
 4. L4 (central ownership). Take the global lock via the L6
    threading-primitive interface (S9.1). Lock failure: S9.5 /
    F-07 -- fail with no ownership passing, or terminate
    safely; never proceed unlocked. Under the lock, L4 asks
    L5 for one fresh mapping-backed extent of the admitted
    size. (No free span, size class, or cached extent is
    consulted: none exists, S8.5.)
 5. L5 (page / span manager). Compute the layout: obtain the
    system page size from L6 (discovery failure: F-11);
    usable_offset = one system page; mapping_length =
    usable_offset + usable region rounded up to whole system
    pages covering the admitted size -- a checked computation
    whose overflow detail is B-007 scope, with Invariant 3
    applying here as at L2. Ask L6 for exactly that mapping.
 6. L6 (OS abstraction). Issue the single mmap for
    mapping_length (private, anonymous, read-write design
    intent for Linux ARM64; flag-level detail belongs to the
    implementation design of Part 1 / later code review, not
    to this bookkeeping part). On failure, return the error
    to L5 as a plain failure, classified by S10: ENOMEM F-01,
    EMFILE / ENFILE F-03, EAGAIN F-04, EINVAL class F-02 if
    it ever reaches the syscall. On success, return the
    mapping base to L5. The lock is held across this call
    (S9.4 DECISION).
 7. Failure branch (from step 6). L5 creates no descriptor;
    L4 changes no registry entry and no counter; L4 releases
    the lock; L1 returns null. No ownership passes, no live
    allocation is disturbed (MA-6.1-MALLOC-02). No error is
    reported while the lock is held (S9.4 item 3); errno is
    not promised (V-02 / U-10).
 8. Success branch. L5 writes the descriptor in the reserve
    page of the new mapping, in full: magic, mapping_base,
    mapping_length, usable_offset, requested_size,
    usable_size, state = LIVE, flags, reserved zeroed
    (S7.3). Only with the descriptor complete does L4 link
    it into the registry and increment live_mappings and
    live_bytes by mapping_length (S8.1) -- publication is a
    single locked step, so no thread can observe the mapping
    as live before its metadata is complete (Invariant 1,
    Invariant 2).
 9. L4 releases the lock. The lock is never held across the
    return to the caller (S9.4 item 1).
10. L1 returns mapping_base + usable_offset to the caller.
    Success semantics, by analogy: non-null, suitably aligned
    (page-aligned by construction, S7.1), distinct storage
    that overlaps no other live allocation (MA-6.1-MALLOC-01;
    Invariant 1; MA-6.2-054 for the cross-thread case). The
    usable bytes are now the caller's alone; the allocator
    does not touch them (S9.2).

### S11.2 mimir_free(p) -- step by step

 1. L1 (public boundary). The caller enters through the
    namespaced C ABI entry mimir_free. Free flow enters at
    L1 and goes to L4 (owner determination); it does not pass
    through L2 or L3.
 2. Null check, before any lock or descriptor access.
    p == NULL: no-op, return at once (MA-6.1-FREE-02;
    MA-6.2-010; Invariant 4; S8.2 step 1). Flow ends.
 3. L4 (central ownership). Take the global lock via the L6
    threading-primitive interface (S9.1). Lock failure: S9.5 /
    F-07 -- report or terminate safely; the allocation is NOT
    freed unlocked, and its descriptor stays LIVE.
 4. L4 with L5 (validation). Derive the candidate descriptor
    address as p minus one system page (L6 page size, as in
    S11.1 step 5) and validate exactly as in S8.2 step 2:
    magic, state == LIVE, mapping_base + usable_offset == p,
    length consistency, and registry membership. Validation
    failure: release the lock, then follow S8.3 / F-09 --
    in a debug build preserve evidence and stop / report
    (MA-6.2-033, MA-6.2-034); in a normal build the case is
    UB and no behaviour is promised (MA-6.1-FREE-03, -04;
    MA-6.2-030..032). Nothing is unmapped, unlinked, or
    counted on suspicion. Flow ends without a free.
 5. L5 with L6 (release). With the descriptor validated, L4
    marks it RETIRED and L5 asks L6 to munmap the WHOLE
    mapping (mapping_base, mapping_length from the descriptor,
    never values re-derived from p). The lock is held across
    this call (S9.4 DECISION).
 6. munmap failure branch. Follow F-06 in full: the descriptor
    is restored to / remains LIVE, stays on the registry,
    counters are unchanged, the lock is released, and only
    then is the failure reported through the bounded path
    (MA-6.3-020..023). The memory remains recorded as owned;
    it is never silently dropped. Flow ends; the caller still
    owns the block as far as this allocator's bookkeeping is
    concerned, and a retry is a new call, not a continuation.
 7. munmap success branch. L4 unlinks the descriptor from the
    registry and decrements live_mappings by one and
    live_bytes by mapping_length (S8.1), in the same locked
    section. Retirement has now happened exactly once
    (MA-6.1-FREE-01; MA-6.2-001). The descriptor ceases to
    exist with its mapping and is never accessed again --
    no field is read, logged, or checked after this point
    (Invariant 2).
 8. L4 releases the lock. The lock is never held across the
    return to the caller, and no reporting or logging happens
    while it is held (S9.4 items 1 and 3).
 9. L1 returns to the caller (void). The pointer p and every
    pointer into the former mapping are dangling from this
    point; any later use is use-after-free UB (MA-6.2-031)
    and any second free is double-free UB (MA-6.2-032),
    subject only to the debug detection limits of S8.3.
    Cross-thread note: steps 1-9 are identical when the
    freeing thread differs from the allocating thread; that
    case is Defined (MA-6.2-054; hook C-009; S8.4).

### S11.3 Closing statements for Part 2

- Every element above is design over Proposed contracts and
  Proposed layers (INC-B006-10). Nothing here is implemented,
  measured, adopted, or a performance claim, and the Phase 3
  exit (portable suite passing for explicitly supported calls
  on x86-64 AND ARM64 Linux) is a B-009 / later-implementation
  matter that B-006 alone does not satisfy (INC-B006-4).
- Deferred, not designed here: checked arithmetic and
  alignment detail (B-007); calloc / realloc semantics
  (B-008); reuse, splitting, coalescing, size classes, and
  caches (Phases 7/8); diagnostics counters beyond the two
  bookkeeping counters of S8.1 (Phase 10).
- Open items carried, unresolved by this part: U-03
  (namespaced contract rows), U-09 (zero-size), U-10 (errno,
  via V-02), U-17 (lock state in a fork child). None is
  silently resolved by any choice recorded above.

-- End of B-006 Part 2 (S7-S11). Design only. --
# Phase 3 — Minimal Allocator Design (continued): Part 3

> **HONESTY BANNER — DESIGN ONLY.** This Part 3 is design only.
> Nothing in it is implemented, nothing in it is measured, and
> nothing in it describes existing behaviour. Status: **Proposed**.
> Issue: **MIM-005** — a Roadmap S13 suggested issue ID, not a
> contract ID. Slice: **B-006, Part 3** (Sections 12–14 and
> Appendices A–C). Every contract row and every architecture
> layer cited here is itself **Proposed**; no cited row is
> adopted or implemented fact (INC-B006-10). Part 3 states
> what B-006 does **not** do, what evidence is still owed,
> and what inconsistencies the design records — it claims
> no completion of any of them.

## 12. Explicit NOT-done / deferred list (design)

The following are **not** designed, **not** implemented, and
**not** claimed by B-006. Listing them here is part of the
design; absence from this list does not imply inclusion.

### 12.1 Deferred to B-007

- Checked arithmetic in detail, and its proofs: overflow-
  checked evaluation of size, alignment, rounding, and
  total-mapping calculations (by analogy S9.1 Invariant 3;
  detail and proof obligations are B-007's, not B-006's).
- Explicit-alignment detail and its proofs: validity of an
  alignment value, any size-multiple rule, and the numeric
  alignment guarantee — **V-03 / U-11**, **V-07 / U-15** open
  (by analogy **MA-6.1-ALIGNED-01..05**,
  **MA-6.1-POSIXMA-01..05**).
- Initialisation / bootstrap and error-path detail beyond
  the design-level constraints already stated in Parts 1–2
  (B-007 covers S6.2–S6.3 and S9.1 in detail).

### 12.2 Deferred to B-008 — no `calloc` / `realloc` in B-006

- **B-006 provides NO `mimir_calloc` and NO `mimir_realloc`**
  — no design label, no signature, no semantics for either
  name is stated or implied in B-006.
- `calloc` semantics are B-008: **MA-6.1-CALLOC-*** rows,
  in particular **MA-6.1-CALLOC-01** (success: zeroed,
  aligned), **-02** (count×size overflow detected
  pre-multiply, failure), **-03** (other failure: null),
  **-04** (zero factor — [UNRESOLVED: U-09]), **-05**
  (zeroing verified byte-by-byte); test hooks **C-001 /
  C-002** as cited for that work.
- `realloc` semantics are B-008: **MA-6.1-REALLOC-*** rows,
  in particular **MA-6.1-REALLOC-01** (success: min(old,new)
  bytes preserved, may move), **-02** (failure: original
  unchanged, still caller-owned), **-03**
  (realloc(NULL,n) = malloc(n)), **-04** (realloc(p,0) —
  [UNRESOLVED: **U-12**; also recorded as **V-04** target
  variation — unresolved, no behaviour claimed), **-05**
  (invalid / already-released pointer: UB), **-06** (bytes
  beyond min() after grow: unspecified); test hooks
  **C-003 / C-004** as cited for that work.
- Conditional `reallocarray` rows (**MA-COND-REALLOCARRAY-***)
  are likewise B-008 / target-gated, not B-006.

### 12.3 Deferred to later phases — not in Phase 3 at all

- **Interposition / ABI** (Phase 5): no symbol interposition,
  no standard-name `malloc`/`free` replacement, no ABI,
  symbol version, or stability promise. The namespaced API
  itself has no contract rows (**U-03**, INC-B006-1).
- **Size classes, splitting, coalescing, reuse, trim**
  (Phase 7): L4 in Phase 3 is degenerate, with no size
  classes; L5 does not split, coalesce, reuse, or trim
  mappings in this design.
- **Caches / sharding** (Phase 8): L3 is pass-through only;
  no thread-local cache, bounded cache, or sharding.
- **Budgets** (Phase 9): no process budget, pressure
  callback, or reservation behaviour is designed here
  (L2 budget form is later-phase).
- **Stats / L7** (Phase 10): L7 is compiled out in Phase 3;
  no counters, snapshots, or diagnostics are designed.
  Namespaced stats/trim entry points are **[UNRESOLVED:
  U-03]** in any case.
- **rseq** (Phase 11): no rseq design, registration, or
  per-CPU claim.
- **Fork / atfork**: **U-17..U-21 remain open** and
  **MA-6.3-040..042** leave fork behaviour not Defined
  until moved off the unresolved list by per-platform
  evidence. **B-006 makes no fork claim** — no fork safety,
  no atfork handler, no child-state claim (INC-B006-7).
- **Android / bionic / musl**: no target claim, annex, or
  behaviour for Android, bionic, or musl. Linux/glibc-shaped
  wording, where it appears by analogy, is never presented
  as universal.
- **Any performance claim**: none. Mapping-per-allocation
  is acceptable in Phase 3 **only** as an explicitly
  experimental correctness baseline (Roadmap Phase 3 text).
  B-006 is not a performance allocator and states no speed,
  overhead, fragmentation, or scalability figure.

## 13. Phase 3 exit evidence this design must LATER satisfy (design)

All items in this section are **owed, none held**. No item
below is satisfied by B-006, by Parts 1–3, or by any work
in this slice. Exit evidence is a B-009 / later-
implementation matter (INC-B006-4). Roadmap Phase 3 exit,
verbatim: *"portable unit/contract suite passes for
explicitly supported calls on x86-64 and ARM64 Linux.
Document unsupported behavior."*

A later implementation of this design must produce:

1. **An explicit supported-call list**, stated before any
   pass claim: at design level that list is
   `mimir_malloc` with size > 0 and representable (admitted
   at L2; oversize = defined failure, by analogy
   **MA-6.1-MALLOC-04**), and `mimir_free` of a live
   in-scope pointer or of NULL (by analogy
   **MA-6.1-FREE-01 / -02**, **MA-6.2-010**). Zero-size
   behaviour is not on the list: **MA-6.1-MALLOC-03** /
   **U-09** are unresolved. Nothing else is supported.
2. **A-008 test-hook evidence**: hooks **C-005, C-009,
   C-010** plus the **MA-MALLOC / MA-FREE named hooks**,
   passing — cited exactly as named, without conflating
   the three hook schemes (C-001..C-010, C-3nn/C-4nn slots,
   named hooks; contracts S1.5; INC-B006-9). The hook-to-
   row mapping must be shown, not asserted.
3. **Passing on BOTH named targets**: a named ARM64 Linux
   environment **and** a named x86-64 Linux environment,
   each identified by machine, OS, and toolchain in the
   evidence record. **Pi-only evidence does NOT meet the
   roadmap wording** — the x86-64 leg is still owed in
   full, and no ARM64 result may be presented as covering it.
4. **Data-integrity cycles**: repeated allocate / write /
   verify / free cycles in which written bytes read back
   intact, distinct live blocks do not overlap (S9.1
   Invariant 1), and allocator counters / bookkeeping
   **return to baseline** after each cycle — no leaked
   mapping, block, or count.
5. **Fail-the-Nth-mapping injection** (Roadmap **§9.3**):
   fault injection that fails the Nth mapping attempt,
   for swept N, showing a defined failure (null, no
   partially constructed / half-owned block — by analogy
   **MA-6.2-020 / MA-6.2-024**), with recovery on the
   next uninjected attempt.
6. **Minimal concurrency correctness**: concurrent
   allocate/free from multiple threads, including
   cross-thread handoff (allocate on one thread, free on
   another), with ownership preserved exactly and no node
   in incompatible lists (by analogy **MA-6.2-054**,
   S9.1 Invariants 1, 6, 9). Minimal means correctness
   only — no concurrency performance claim.
7. **A per-environment page-size record**: for each
   evidence environment, the runtime-discovered system
   page size, the query method and its failure check, and
   the date/machine of the record — qualified as system
   page size per **MA-6.2-041**. The Pi value 16384 is a
   record for this Pi only, not a target assumption
   (INC-B006-3).
8. **C11 / D-006 build evidence**: a clean build under the
   adopted D-006 decision (portable C11 core, C ABI, no
   C++ in the core) on each named target, with compiler
   and flags recorded. Toolchain feasibility probes are
   not build evidence for this allocator.
9. **The NOT-done list shipped as part of the exit
   evidence**: Section 12 of this design, or its successor,
   included verbatim in the exit package — deferred,
   unsupported, and unresolved items are evidence, not
   an appendix to be dropped.
10. **Documented unsupported behaviour**: a written record
    of what is unsupported (Section 12 items, zero-size,
    explicit alignment, fork, non-Linux targets, and any
    call outside item 1), satisfying the exit sentence
    *"Document unsupported behavior."*

Until items 1–10 exist as recorded evidence, the correct
statement is: **Phase 3 exit is unmet.**

## 14. Inconsistencies recorded (design)

Recorded faithfully from the B-006 scout. Recording an
inconsistency does not resolve it.

- **INC-B006-1 — NAMESPACED CONTRACT GAP (critical):**
  Phase 3 + MIM-005 require NAMESPACED alloc/free, but
  contracts.md gives `mimir_malloc`/`mimir_free` NO
  contract rows — they are [UNRESOLVED: U-03], "not in S6
  scope", Appendix A stability Unassigned. B-006 can map
  its design to the core malloc/free MA-* rows by analogy
  only, and must state that mapping is proposed, not a
  direct namespaced contract.
- **INC-B006-2 — NO TARGET FILENAME:** neither
  WAVE_PLAN.md nor ASSIGNMENT.md names the B-006
  deliverable file (checked both verbatim).
- **INC-B006-3 — PAGE SIZE TENSION:** assignment fixes
  16 KiB as Pi-observed; contracts V-07/U-15 leave numeric
  alignment/page relation UNRESOLVED; ARCH L5 forbids
  hardcoded page size (A9-13) and S15 Q6 says measure,
  don't infer; roadmap Phase 4 measures 4+16 KiB. Design
  must use runtime discovery and treat 16384 as this-Pi
  fact only.
- **INC-B006-4 — PHASE 3 SCOPE SPLIT:** roadmap Phase 3
  text bundles checked arithmetic/alignment/init/errors/
  tests + calloc/realloc into the single Phase 3 exit,
  but Wave 2 splits it across B-006 (mapping/lock),
  B-007 (arithmetic/alignment/init/errors, S6.2–6.3+9.1),
  B-008 (calloc/realloc+unsupported doc), B-009 (exit test
  suite, S10). B-006 alone cannot satisfy the Phase 3
  exit; exit evidence is a B-009/later-implementation
  matter (suite must PASS on x86-64 AND ARM64 Linux —
  no implementation exists in this design slice).
- **INC-B006-5 — LOCK PLACEMENT SPANS LAYERS:** "metadata
  under a simple lock" (Phase 3) touches L4 (synchronization
  owner), L5 (metadata/occupancy owner, Inv. 2), L6
  (threading primitives provider). A design placing the
  lock inside L5 alone, or calling pthread/OS primitives
  directly from L5, would violate the ARCH dependency law
  (L5→L6 is policy mappings only; threading primitives
  reach L3/L4 via L6 published interface). Lock scope
  must be assigned explicitly.
- **INC-B006-6 — METADATA BOOTSTRAP CONSTRAINT:**
  MA-6.3-012 requires init bookkeeping in fixed/
  preallocated/bootstrap-owned storage; MA-6.3-002 forbids
  bootstrap storage from the normal path. Metadata
  placement design therefore cannot allocate its own
  metadata via the allocator it describes; in-mapping or
  static/bootstrap metadata is required, or the exception
  stated.
- **INC-B006-7 — FORK/LOCK STATE UNRESOLVED:** contracts
  U-17 (lock state in fork child), U-18..U-21 and
  MA-6.3-042 mean no fork behaviour for a lock-holding
  minimal allocator is Defined yet. Design must not claim
  fork safety.
- **INC-B006-8 — L3/CACHES NOT IN PHASE 3:** S7.1 presents
  all 7 layers at once with no maturity annotation (ARCH
  S14); Phase 3 minimal design should treat L3 as absent/
  pass-through and L4 size classes as not-yet (Phases
  7/8), not import the full S7.1 form. Mapping-per-
  allocation is acceptable ONLY as explicitly experimental
  correctness baseline (Phase 3 text).
- **INC-B006-9 — ID NAMESPACE CONFUSION RISK:** MIM-005
  is a roadmap S13 issue ID, MA-* are contract IDs,
  C-001..C-010/C-3nn/C-4nn/named hooks are three different
  test-hook schemes (contracts S1.5). Citations in B-006
  must not conflate them.
- **INC-B006-10 — ALL SOURCE STATUS IS PROPOSED:**
  contracts (all rows Proposed), ARCH layers (all
  Proposed, none implemented), roadmap Phase 3 exit
  unmet. B-006 is a design over proposed contracts — it
  must not cite any MA-* row as adopted/implemented fact.

## Appendix A. Contract-mapping table (design)

Every B-006 design element, mapped to contract IDs and
layer(s). Where the namespaced API has no contract rows,
the mapping is **by analogy** under **U-03** and is
labelled as such; it is proposed, never direct.

| Design element | Contract IDs (all Proposed) | Layer(s) |
|---|---|---|
| mmap acquisition (mapping-per-allocation) | By analogy (U-03): MA-6.1-MALLOC-01, MA-6.1-MALLOC-02, MA-6.2-020; S9.1 Inv. 1, 7. No contract citation for the syscall itself — contracts S1.3 puts syscalls/mapping policy in non-scope | L5 via L6; failure surfaced L2/L1 |
| munmap release (exact address+length, free path only) | By analogy (U-03): MA-6.1-FREE-01, MA-6.2-001 (retire exactly once) | L5 via L6; authorised by L4 |
| Metadata reserve (in total mapping size) | MA-6.2-040, MA-6.2-041 (page terms qualified); S9.1 Inv. 2 (metadata describes actual memory) | L5 |
| Lock (simple lock guarding metadata) | MA-6.2-054 (cross-thread handoff); S9.1 Inv. 6, 9. Lock owned at L4, primitives via L6 published interface (INC-B006-5) | L4 (owner), L6 (primitives) |
| Registry / bookkeeping (live/used/free records) | By analogy (U-03): MA-6.2-001, MA-6.2-023, MA-6.2-024; S9.1 Inv. 1, 2 | L4 / L5 |
| Free validation (live ptr, NULL, invalid/double) | By analogy (U-03): MA-6.1-FREE-01, -02 (NULL no-op), -03 (non-allocator/interior: UB), -04 (double free: UB); MA-6.2-010, MA-6.2-030..034 | L1 → L4 |
| Failure handling (mapping failure, multi-step rollback) | By analogy (U-03): MA-6.1-MALLOC-02, MA-6.1-MALLOC-04; MA-6.2-020, MA-6.2-021, MA-6.2-024; S9.1 Inv. 7, 9, 10 | L2, L4, L5, L6 |
| Bootstrap (metadata/init storage not from normal path) | MA-6.3-001..006, MA-6.3-010..012 — in particular MA-6.3-002 (not from normal path), MA-6.3-012 (fixed/preallocated/bootstrap-owned only) (INC-B006-6) | Below L1 (no S7.1 layer); storage used by L4/L5 |

## Appendix B. Layer-mapping summary — L1–L7, Phase 3 form (design)

| Layer | Phase 3 form in B-006 (design) |
|---|---|
| **L1** Public boundary | `mimir_malloc` / `mimir_free` design labels only; no ABI, no stats/trim, no calloc/realloc. By analogy MA-6.1-MALLOC-*/FREE-* (U-03) |
| **L2** Admission + checked arithmetic | Gate only in B-006: admit / defined-failure. Checked arithmetic + alignment detail deferred to B-007 |
| **L3** Small-object fast path | Absent / pass-through to L4. No cache (Phase 8) |
| **L4** Central ownership | Degenerate: ownership + registry, owns lock discipline, sole client of L5. No size classes (Phase 7) |
| **L5** Page/span manager | Degenerate: one mapping per allocation, metadata + occupancy, Inv. 2 owner. No split/coalesce/reuse/trim (Phase 7) |
| **L6** OS abstraction | Runtime page-size discovery; mmap/munmap boundary. Only layer with platform-specific code; threading primitives via published interface |
| **L7** Optional observability | Compiled out. No stats/counters (Phase 10; U-03) |

Dependency law (design): request L1→L2→(L3|L4)→L5→L6;
free L1→L4→L5→L6; no layer skips to L6; sole exception
is L3/L4 consuming L6 threading primitives via L6's
published interface. Bootstrap sits below L1.

## Appendix C. Sources (design)

- Scout: `hidden_files/scout-B006.md` (B-006 scout digest,
  2026-11-06; worktree HEAD 5ea3a8d) — source of
  INC-B006-1..10, contract counts, and verbatim texts.
- Style / Parts 1–2 form: `hidden_files/b006-part1.md`
  (Sections 1–6) and the B-006 Part 2 sections on metadata,
  lock, and bookkeeping, in the same worktree.
- Contracts: `docs/contracts.md` (A-008) — 91 primary
  MA-* rows, all Proposed; S6.1/S6.2/S6.3 rows, S1.5 hook
  schemes, U-01..U-21 unresolved register, V-01..V-07.
- Architecture: `docs/ARCHITECTURE_LAYERS.md` — S7.1
  L1–L7, dependency law, S14 phase phasing; all Proposed.
- Roadmap: `MimirAlloc_Research_and_Development_Roadmap.md`
  — Phase 3 (S8) text and exit, MIM-005 (S13), S6, S7,
  S9.1 invariants, §9.3 fault injection.
- Language: D-006 in `IMPLEMENTATION_LANGUAGE_DECISION.md`
  + `DECISION_LEDGER.md` — portable C11 core, C ABI.
- Targets: `SUPPORTED_TARGETS.md` — Pi system page size
  16384 observed on this Pi; runtime discovery required.

---

*End of B-006 Part 3 (Sections 12–14, Appendices A–C).
Design only; Proposed. No implementation, no measurement,
no Phase 3 exit claim is made anywhere in this part.*
