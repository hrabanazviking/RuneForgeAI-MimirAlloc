# MimirAlloc Evidence Ledger

Slice: A-002 (Wave 1) - Evidence and decision ledgers
Source of record: `docs/MimirAlloc_Research_and_Development_Roadmap.md` Section 2,
"Evidence already available and unresolved questions" (research date: November 3, 2026;
roadmap base commit b0b8374).
Companion ledger: `docs/DECISION_LEDGER.md`

This ledger is a transcription ledger, not a findings document. It records exactly
what the roadmap Section 2 states, including what each claim does not establish.
No claim in this ledger has been independently re-verified by slice A-002; verification
work belongs to later slices (notably the Phase 1 failure-capture work in A-005).

## Rules of this ledger

1. **Append-only.** Entries are never edited in place and never deleted. A correction
   is made by appending a new entry that supersedes the old one (see supersession).
2. **Verbatim transcription.** The Claim and Does-not-establish fields reproduce the
   roadmap Section 2 table wording exactly, including its source markers ([S1]-[S4]).
   Editorial changes to a claim belong in a superseding entry, never in a silent edit.
3. **Honesty rule - no inferred evidence levels.** Roadmap Section 2 defines seven
   evidence labels (listed below) but assigns none of them to any of its six claims.
   Every entry in this ledger therefore records its evidence level exactly as
   "Not assigned in roadmap Section 2". No transcriber, reviewer, or later reader may
   infer, improve, or upgrade a level in this ledger. A level may only appear here when
   a later slice produces the named evidence and appends a superseding entry citing it.
4. **No promotion.** Per roadmap Section 2: never promote an emulator result to
   physical-device proof, a synthetic test to full application compatibility, or a
   successful compile to proof of runtime interposition.
5. **Location.** This ledger lives at `docs/EVIDENCE_LEDGER.md`. Roadmap Section 13 left
   ledger locations unsettled (an `evidence/` root versus `docs/decisions/`); the A-002
   brief fixes both ledgers under `docs/`, and that fixing is recorded as part of
   inconsistency note N-5 below.

## Supersession rules

- An entry is superseded only by a later entry that: (a) carries a new sequential ID,
  (b) names the ID it supersedes in its Supersedes field, and (c) states what changed
  and on what new evidence.
- A superseded entry stays in this file unchanged, with its Status changed only by the
  superseding entry's declaration recorded in the index table (the entry body itself is
  not rewritten).
- If two entries conflict and neither supersedes the other, both stand and the conflict
  is recorded in the Notes section until a superseding entry resolves it.

## Evidence labels defined in roadmap Section 2

The roadmap defines these seven labels for use throughout the project. They are
reproduced here for reference only; as recorded above, Section 2 assigns none of them
to the six claims below.

- **Proposed:** an idea not implemented.
- **Implemented:** code exists, but has not passed the full test gate.
- **Host-tested:** passed named tests in a specified host environment.
- **Device-tested:** ran on the named physical ARM or Android device.
- **Integration-proven:** the exact Mojo or Aesir workflow succeeded with MimirAlloc demonstrably handling the relevant allocations.
- **Performance-measured:** results have complete scripts, workload, baseline, versions and hardware.
- **Blocked:** a specific prerequisite or test is unmet, with an actionable resumption point.

## Index

| ID | Topic | Evidence level | Status |
|---|---|---|---|
| E-001 | ARM64 support | Not assigned in roadmap Section 2 | Current |
| E-002 | TCMalloc architecture | Not assigned in roadmap Section 2 | Current |
| E-003 | Tablet | Not assigned in roadmap Section 2 | Current |
| E-004 | Android memory pages | Not assigned in roadmap Section 2 | Current |
| E-005 | Name | Not assigned in roadmap Section 2 | Current |
| E-006 | Mojo blocker | Not assigned in roadmap Section 2 | Current |

---

## E-001 - ARM64 support

- **Topic (roadmap Section 2):** ARM64 support
- **Claim (verbatim):** TCMalloc lists Linux AArch64 as supported [S1], and its public implementation contains AArch64-specific code.
- **What it does not establish (verbatim):** That any particular build works on Android, a particular kernel configuration, a specific VA width, Mojo revision, or device.
- **Evidence level:** Not assigned in roadmap Section 2
- **Source:** Roadmap Section 2 table, row 1; source marker [S1] (TCMalloc platform support).
- **Supersedes:** None. **Status:** Current.

## E-002 - TCMalloc architecture

- **Topic (roadmap Section 2):** TCMalloc architecture
- **Claim (verbatim):** Public documentation describes small-object caches, central services, backend memory management, and different per-CPU/per-thread approaches [S2].
- **What it does not establish (verbatim):** A requirement for MimirAlloc to reproduce the same internal design.
- **Evidence level:** Not assigned in roadmap Section 2
- **Source:** Roadmap Section 2 table, row 2; source marker [S2] (TCMalloc architecture).
- **Supersedes:** None. **Status:** Current.

## E-003 - Tablet

- **Topic (roadmap Section 2):** Tablet
- **Claim (verbatim):** In this conversation, Codex running locally reported Samsung Galaxy Tab S10 Lite Wi-Fi SM-X400, Android 16, Exynos 1380, approximately 5.57 GiB usable RAM and 1.52 GiB available at one point.
- **What it does not establish (verbatim):** A guaranteed persistent free-memory value; whether native Mojo or MimirAlloc can run there.
- **Evidence level:** Not assigned in roadmap Section 2
- **Source:** Roadmap Section 2 table, row 3. No [S] source marker is given in the roadmap for this row.
- **Supersedes:** None. **Status:** Current.

## E-004 - Android memory pages

- **Topic (roadmap Section 2):** Android memory pages
- **Claim (verbatim):** Android documents 16 KiB page support alongside 4 KiB configurations [S3].
- **What it does not establish (verbatim):** The actual page size of this specific tablet, which must be measured.
- **Evidence level:** Not assigned in roadmap Section 2
- **Source:** Roadmap Section 2 table, row 4; source marker [S3] (Android page-size guidance).
- **Supersedes:** None. **Status:** Current.

## E-005 - Name

- **Topic (roadmap Section 2):** Name
- **Claim (verbatim):** Microsoft has an existing allocator named mimalloc [S4].
- **What it does not establish (verbatim):** That MimirAlloc is legally unavailable or available.
- **Evidence level:** Not assigned in roadmap Section 2
- **Source:** Roadmap Section 2 table, row 5; source marker [S4] (Microsoft mimalloc project).
- **Supersedes:** None. **Status:** Current.

## E-006 - Mojo blocker

- **Topic (roadmap Section 2):** Mojo blocker
- **Claim (verbatim):** The user reports a tcmalloc-related ARM build problem and difficulty getting upstream fixes accepted.
- **What it does not establish (verbatim):** The exact error, affected component, revision, cause, or whether replacing the allocator solves it. These require a reproducible evidence packet.
- **Evidence level:** Not assigned in roadmap Section 2
- **Source:** Roadmap Section 2 table, row 6. No [S] source marker is given in the roadmap for this row.
- **Supersedes:** None. **Status:** Current.

---

## Notes - roadmap inconsistencies discovered in the A-002 design phase

The following inconsistencies were discovered while designing this slice. Per campaign
law, a discovered inconsistency is never left unrecorded. They are recorded here and in
the companion decision ledger; fixing the roadmap itself is outside A-002 scope and is
left for a roadmap-revision slice or the project owner.

- **N-1. Section 2 defines seven evidence levels but assigns none to its six claims.**
  The labels are defined in the "Claims and evidence levels" subsection and never mapped
  to the table rows. This ledger records every level as "Not assigned in roadmap Section 2"
  rather than inferring one.
- **N-2. Section 7.2 records no implementation-language decision.** The heading promises a
  decision; the text leaves C11 versus C++17 open. Recorded as OPEN decision D-005 in the
  decision ledger, owned by slice A-009.
- **N-3. Section 15 decision register holds 13 open questions and zero recorded decisions.**
  The register is entirely questions; the decisions the roadmap body does adopt (Sections 4
  and 6) appear nowhere in it. The decision ledger (D-001 to D-004) is the first recorded
  register of adopted decisions.
- **N-4. License framing is stale.** The roadmap (Sections 4 and 15, question 12) treats the
  license as undecided, but the repository already carries an MIT LICENSE (Copyright (c) 2026
  Volmarr Wyrd) and `docs/ENGINEERING_GUIDE.md` instructs contributors to preserve the MIT
  license. The undecided framing no longer matches the repository.
- **N-5. Ledger locations are unsettled in the roadmap.** Section 13 proposes both an
  `evidence/` directory at repository root and a `docs/decisions/` directory, without fixing
  where the evidence and decision ledgers live. The A-002 brief fixes both under `docs/`
  (`docs/EVIDENCE_LEDGER.md`, `docs/DECISION_LEDGER.md`).
- **N-6. Section 13 proposed layout does not match the actual repository tree.** The proposed
  tree (for example `ROADMAP.md` at root, `include/`, `src/`, `tests/`, `benchmarks/`,
  `evidence/`, `scripts/`) does not exist; the actual tree holds the roadmap under `docs/`,
  plus `docs/research/` and `docs/field-notes/`, and no code directories yet.
- **N-7. Evidence-label vocabularies diverge across documents.** The roadmap Section 2 label
  set (Proposed / Implemented / Host-tested / Device-tested / Integration-proven /
  Performance-measured / Blocked) is not used consistently across the research library
  README and the Android field notes, which use their own status wording. A single
  vocabulary reconciliation is owed by a later documentation slice.
