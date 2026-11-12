# MimirAlloc Decision Ledger

Slice: A-002 (Wave 1) - Evidence and decision ledgers
Source of record: `docs/MimirAlloc_Research_and_Development_Roadmap.md`
(roadmap base commit b0b8374).
Companion ledger: `docs/EVIDENCE_LEDGER.md`

Roadmap Section 15, titled "Decision register", in fact records thirteen open
questions and zero decisions. This ledger is the project's first register of decisions
actually adopted in the roadmap body, plus the one decision the roadmap explicitly
leaves open. Statuses in this ledger mean:

- **Adopted:** the roadmap body states the decision as the project's course of action.
- **OPEN:** the roadmap raises the decision and deliberately does not settle it; the
  owning slice that must settle it is named in the entry.

## Rules of this ledger

1. **Append-only.** Entries are never edited in place and never deleted. A changed
   decision is recorded by appending a new entry that supersedes the old one.
2. **Traceability.** Every entry cites the roadmap section(s) that adopt or raise it.
   A decision with no roadmap or owner citation does not belong in this ledger.
3. **OPEN stays OPEN.** An OPEN entry is closed only by the slice named as its owner,
   appending a superseding entry that records the decision and its working proof.
   No other slice may silently close it.
4. **Location.** This ledger lives at `docs/DECISION_LEDGER.md`. Roadmap Section 13 left
   ledger locations unsettled (an `evidence/` root versus `docs/decisions/`); the A-002
   brief fixes both ledgers under `docs/` (see note N-5 below).

## Supersession rules

- An entry is superseded only by a later entry that: (a) carries a new sequential ID,
  (b) names the ID it supersedes in its Supersedes field, and (c) states the new
  decision and the evidence or owner direction behind the change.
- A superseded entry stays in this file unchanged; its superseded state is declared by
  the superseding entry and reflected in the index table only.
- Reopening an Adopted decision follows the same path: a superseding entry that first
  records the decision as OPEN, naming its new owner.

## Index

| ID | Decision | Status | Roadmap basis | Owner / settler |
|---|---|---|---|---|
| D-001 | Clean-room engineering process | Adopted | Section 4 | Project process (all slices) |
| D-002 | Contract-first API and ABI | Adopted | Section 6 | A-008 (specification slice) |
| D-003 | ARM64-first targeting | Adopted | Sections 1, 5 | A-003 (target matrix slice) |
| D-004 | Allocation-free startup and diagnostics | Adopted | Section 6.3 | A-008 (specification slice) |
| D-005 | Implementation language (C11 vs C++17 core) | Superseded by D-006 | Section 7.2 | A-009 |
| D-006 | Implementation language — C11 core | Adopted | Section 7.2 | A-009 |
| D-007 | Z13 redesign: TLS cache (Wave 4), per-CCD arenas (Wave 5) | Adopted | Z13 roadmap Z-026 | Z-026 (settler); consumers Z-028, Z-037, Z-039 |

---

## D-001 - Clean-room engineering process

- **Status:** Adopted
- **Roadmap basis:** Section 4, "Clean-room engineering and provenance", including the
  practical process in Section 4.1.
- **Decision:** MimirAlloc is engineered clean-room. The project writes a behavior
  specification in original language from public documents, standards, and observed
  requirements; creates an original design from that specification; keeps third-party
  source snippets, copied implementation code, proprietary material, and unlicensed
  disassembly out of the implementation; maintains a dated source ledger, commit
  history, provenance notes, contributor sign-off, and dependency notices; and compares
  observable behavior using independent test cases.
- **Recorded limitations (from Section 4.1 itself):** when strict personnel separation
  is unavailable, that limitation is recorded; a solo or AI-assisted process is not
  labelled a legally verified two-team clean room. Section 4 also states the roadmap is
  an engineering process, not a legal opinion, and that license and attribution
  requirements matter if any open-source code is actually reused.
- **Supersedes:** None. **Status of entry:** Current.

## D-002 - Contract-first API and ABI

- **Status:** Adopted
- **Roadmap basis:** Section 6, "Contract-first API and ABI work" (Sections 6.1-6.3).
- **Decision:** The API and ABI are specified by contract before implementation. The
  core C candidates are `malloc`, `free`, `calloc` (with overflow checking and zeroing),
  `realloc` (preserving the old block on failure), `aligned_alloc` and `posix_memalign`,
  with further interfaces only when required and defined per target. Ownership rules in
  Section 6.2 are part of the contract: a pointer is freed only by a compatible
  allocator, there is no silent fallback to a different allocator, and behavior is never
  presented as universal across libc implementations - a target-specific contract table
  is built from authoritative documentation.
- **Settler of the full specification:** slice A-008 (contract-first API/ABI
  specification). This entry records the adopted approach; A-008 produces the spec.
- **Supersedes:** None. **Status of entry:** Current.

## D-003 - ARM64-first targeting

- **Status:** Adopted
- **Roadmap basis:** Section 1 (vision and project goals) and Section 5
  (supported-target matrix).
- **Decision:** MimirAlloc is initially focused on 64-bit ARM devices. Per Section 5,
  Linux AArch64 with glibc is the first production candidate; Linux x86-64 with glibc is
  the development and reference platform; Android ARM64 with bionic is a separate native
  port, claimed only after separate native-device evidence. No target is claimed until
  tested, and a working ARM64 Linux result under `proot` is not Android proof.
- **Settler of the detailed matrix:** slice A-003 (supported-target matrix and baseline
  inventory). This entry records the adopted priority order.
- **Supersedes:** None. **Status of entry:** Current.

## D-004 - Allocation-free startup and diagnostics

- **Status:** Adopted
- **Roadmap basis:** Section 6.3, "Allocation-free startup and diagnostics".
- **Decision:** The allocator's own startup and diagnostics must not depend on the
  allocator itself. The design provides a minimal bootstrap path, avoids
  allocator-dependent C++ containers during initialization, explicitly handles
  reentrancy, and avoids unbounded dynamic allocation in error reporting. Allocator use
  from signal handlers and from the child of a multithreaded `fork()` follows a narrowly
  specified policy, and async-signal safety is not claimed for functions that cannot
  meet it.
- **Settler of the detailed specification:** slice A-008, as part of the contract-first
  API/ABI specification.
- **Supersedes:** None. **Status of entry:** Current.

## D-005 - Implementation language

- **Status:** OPEN
- **Roadmap basis:** Section 7.2, "Implementation language decision". Despite its
  heading, Section 7.2 records no decision (see note N-2 below).
- **Question:** Does the core start as portable C11 or as carefully constrained C++17?
  Per Section 7.2: the core must work even if Mojo is unavailable on the target, and a C
  ABI is exposed either way. C11 minimizes runtime and bootstrap coupling; constrained
  C++ can help express invariants but makes startup, exceptions, and C++ allocation
  recursion more complex. A Mojo interface may be developed above the core without
  making a working Mojo compiler a bootstrap dependency.
- **Owner:** slice A-009 (architecture layers and implementation-language decision
  record, per roadmap Sections 7.1-7.2). Per Section 7.2, the decision is recorded with
  a working proof. Until A-009 appends a superseding entry, this decision remains OPEN
  and no slice may treat either language as settled.
- **Supersedes:** None. **Status of entry:** Current (OPEN).

## D-006 - Implementation language

- **Status:** Adopted
- **Roadmap basis:** Section 7.2, "Implementation language decision".
- **Decision:** The MimirAlloc core starts as portable C11. The C ABI is the core's own ABI. No C++ is used in the core. Any C++ surface (e.g. operator new/delete variants, S6.1) is added only when a mapped integration actually requires it, and then only inside Layer 1, Public boundary, as a shim over the C ABI — it is not an eighth layer and it does not change the core language. A Mojo interface, where supported, is developed above the core (S7.2) and is never a bootstrap dependency.
- **Working proof:** Doc `IMPLEMENTATION_LANGUAGE_DECISION.md` Appendices A-B; purpleviking1, aarch64, gcc/g++ 12.2.0, both -std=c11 and -std=c++17 probes compiled/linked/ran exit 0, identical unmangled C symbol, no Mojo invoked; ceiling: proves toolchain/ABI feasibility only, not interposition/correctness/performance.
- **Supersedes:** D-005. **Status of entry:** Current.

---

## D-007 - Z13 redesign: TLS cache for Wave 4, per-CCD arenas for Wave 5

- **Status:** Adopted (provisional only in the named re-open
  conditions of the dossier, section 7; a re-open is a
  superseding entry, never an edit of this one)
- **Roadmap basis:** docs/MIMIRALLOC_Z13_ROADMAP.md slice Z-026
  (Wave 3), which names this ledger as the decision's home;
  consuming slices Z-028 (TLS cache contract), Z-037
  (per-CCD arena prototype), Z-039 (second arena geometry).
- **Decision:** (1) Wave 4's road is the TLS freelist cache:
  per-thread bins for the hot size classes over the banked
  span machinery, lock-free on cache hit, batched refill/drain
  to the central core on miss (Z-029), cross-thread frees
  routed through a bounded remote-free queue per owner thread
  (Z-030), contract first (Z-028). The 48 B and 64 B alignment
  defects named by Z-017 are Wave 4 work on this road. (2)
  Wave 5's first road is per-CCD arenas: two arenas, one per
  CCD, behind a build flag, sharing the Wave 4 cache machinery
  (Z-037), judged by the three-way contention ladder at Z-038.
  (3) Per-thread arenas are NOT adopted as a primary road;
  they may be taken up at Z-039 only if Z-038's landed ladder
  argues for them. (4) Sharded central locks are NOT adopted
  as a primary road; they are Wave 5's named fallback geometry
  at Z-039 if Z-038 does not argue for per-thread arenas. (5)
  The large path is not redesigned in Waves 4/5; KL-C belongs
  to Wave 6 (Z-046..Z-054).
- **Evidence:** docs/Z13_REDESIGN_DOSSIER.md (slice Z-026),
  scored against docs/Z13_DESIGN_CONSTANTS.md (Z-018) and the
  banked recorded runs it cites: Z-003 loss table (small path
  LOSS 10.4x), Z-005 counters (3.6x..46x instruction excess;
  small path is a code-path-length wound), Z-019 lock
  histograms (uncontended wait at the measurement floor; hold
  is the serialised wound - contended wait ~12x hold at 4
  threads, ~26x at 8), Z-012 CCD constants (cross-CCD 3.2x;
  ARENA_PER_CCD = YES), Z-016 padding rule, Z-017 alignment
  naming (48 B, 64 B), Z-006 RSS standings. Inputs Z-020..
  Z-025 were in flight and are recorded in the dossier as
  OWED with named re-open triggers; no owed number is cited
  as evidence for this decision.
- **Supersedes:** None. **Status of entry:** Current.

---

## Notes - roadmap inconsistencies discovered in the A-002 design phase

The following inconsistencies were discovered while designing this slice. Per campaign
law, a discovered inconsistency is never left unrecorded. They are recorded here and in
the companion evidence ledger; fixing the roadmap itself is outside A-002 scope and is
left for a roadmap-revision slice or the project owner.

- **N-1. Section 2 defines seven evidence levels but assigns none to its six claims.**
  Every evidence-ledger entry therefore records "Not assigned in roadmap Section 2".
- **N-2. Section 7.2 records no implementation-language decision.** The heading promises
  a decision; the text leaves C11 versus C++17 open. Recorded above as OPEN decision
  D-005, owned by A-009.
- **N-3. Section 15 decision register holds 13 open questions and zero recorded
  decisions.** This ledger (D-001 to D-004) is the first recorded register of adopted
  decisions; the Section 15 questions remain open research questions, not decisions.
- **N-4. License framing is stale.** The roadmap (Sections 4 and 15, question 12) treats
  the license as undecided, but the repository already carries an MIT LICENSE
  (Copyright (c) 2026 Volmarr Wyrd) and `docs/ENGINEERING_GUIDE.md` instructs
  contributors to preserve the MIT license.
- **N-5. Ledger locations are unsettled in the roadmap.** Section 13 proposes both an
  `evidence/` directory at repository root and a `docs/decisions/` directory. The A-002
  brief fixes both ledgers under `docs/`.
- **N-6. Section 13 proposed layout does not match the actual repository tree.** The
  proposed code and evidence directories do not exist; the actual tree holds the roadmap
  under `docs/`, plus `docs/research/` and `docs/field-notes/`.
- **N-7. Evidence-label vocabularies diverge across documents.** The roadmap Section 2
  label set is not used consistently across the research library README and the Android
  field notes.
