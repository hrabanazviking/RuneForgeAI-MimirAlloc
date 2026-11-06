# MimirAlloc Provenance Ledger Process

Slice: A-004 (Wave 1) — clean-room ruleset + provenance process
Source of record: docs/MimirAlloc_Research_and_Development_Roadmap.md Sections 4, 4.1, 14 (roadmap base commit b0b8374)
Companion document: docs/CLEAN_ROOM_RULESET.md
Companion ledgers: docs/EVIDENCE_LEDGER.md, docs/DECISION_LEDGER.md

## 0. Status and Scope

Status: process document only — this document governs the future live ledger at `docs/PROVENANCE_LEDGER.md`; it contains a template and a clearly-labelled non-factual example only; no real entries are populated by this slice.

This document defines how provenance is recorded for MimirAlloc. It does not itself record the provenance of any real artifact, claim any result, or assign any evidence level to any real work.

The live ledger, `docs/PROVENANCE_LEDGER.md`, is created by a later slice. When that slice lands, entries are added to the live ledger following the field list, workflow, chain-of-custody rules, and audit procedure defined here.

Deviation from the Roadmap layout, declared openly: Roadmap §13 proposed a single combined document, `docs/clean-room-provenance.md`. Slice A-004 instead uses two documents — `docs/CLEAN_ROOM_RULESET.md` and this process document — following the A-002 / A-003 precedent recorded as N-5 / N-6. The substance required by Roadmap §§4, 4.1, and 14 is not reduced by the split; it is divided between the two documents, and §8 of this document maps the two documents onto each other duty by duty.

Scope: this process applies to every artifact that influences a specification, a design, a code slice, a test, a dependency choice, a data or benchmark claim, or a release of MimirAlloc. If an artifact influenced any of those things and it is not in the ledger, it is unprovenanced under §5.

Out of scope: this document does not define the clean-room rules themselves. Those rules are the companion document, `docs/CLEAN_ROOM_RULESET.md`. This document defines the record that shows whether those rules were followed for a given artifact.

A-001 honesty rule, applied throughout: a row is done only when its evidence pointer names a file a reader can open in this repository. A provenance entry that points to evidence a reader cannot open in this repository is not a completed entry.

## 1. Principles

### Principle 1 — Recorded origin before reliance

Every artifact has a recorded origin before it is relied on. No specification, design, code slice, test, dependency, dataset, image, or claim is built upon until a provenance entry for it exists in the live ledger, or a retrospective entry is made under the explicit flagging rule in §5.

Origin means, at minimum: who produced it, in which clean-room role, from which sources, under which licence or terms where applicable, and by which derivation, as defined by the fields in §4.

### Principle 2 — Append-only; supersession, never editing

The live ledger is append-only, on the same terms as the A-002 ledgers. An entry, once landed, is never edited in place and never deleted.

A correction is made by appending a new entry that supersedes the old entry. The old entry's Status becomes `Superseded` by way of a superseding entry that names it; the old entry's own text is not rewritten.

A withdrawal for cause is made by a quarantine entry under §3 and §7, not by removal.

### Principle 3 — The Honesty Rule: provenance is not evidence

The A-002 Honesty Rule applies to this ledger in substance. Quoted in substance from the Evidence Ledger: never claim a measurement, a result, or an evidence level that was not actually run and logged on the named device and in the named environment.

Evidence Ledger Rules 3–4 apply without exception: "no inferred evidence levels", and no promotion of a level beyond what was run. In particular, there is no promotion:

- from emulator to device;
- from compile to interposition;
- from synthetic input to compatibility;
- or between any other pair of levels distinguished in Roadmap §2.

The same rule applies to tests specifically (Roadmap §14.11): never claim tests that were not run.

A provenance entry records where an artifact came from. It does not certify that the artifact works. A complete, correct provenance entry for a broken artifact is a good entry about a broken artifact; a missing provenance entry for a working artifact is still a gap. Neither fact changes the other.

The Evidence level claimed field in §4 therefore defaults to `Not assigned`, and may carry one of Roadmap §2's labels only where a linked Evidence Ledger entry, and the raw log it names, support exactly that label on exactly that environment.

### Principle 4 — No secrets in entries

No entry may contain secrets, credentials, private prompts, or device serial numbers. This follows Roadmap §§3.1 and 13 and applies to every field, including Source(s) consulted, Derivation statement, and Reviewer attestation ref.

Where a source, a tool, or an environment cannot be described without such material, the entry describes it by role, kind, and date only, and records that the identifying detail is held outside the ledger. An entry that would require a secret to be complete is completed without the secret and flagged as so limited.

## 2. What Must Be Logged

Rule: if it influenced a spec, a design, code, or a claim, it gets an entry. The following artifact classes must be logged. The list is a floor, not a ceiling; doubt is resolved in favour of logging.

1. **Source documents consulted.** Every document, paper, manual, header, source file, or web source read for the purpose of a specification, design, or implementation decision, whether or not it is later cited in the artifact itself.
2. **Behaviour specifications and revisions.** Every behaviour spec and every revision of one, including the frozen revision from which an implementation was derived.
3. **Design documents and decisions.** Every design document, and every design decision that changes a design, whether it also appears in the Decision Ledger or not. Design entries record the alternatives considered and the decision taken (Roadmap §4.1.3). Where it does, the two entries cross-link under §8.
4. **Code slices and commits.** Every code slice and every commit that lands code, in whole-slice units matching the commit or slice identifier recorded in §4.
5. **Tests.** Every test or test suite, recorded as either an independent test or a licensed-reuse test. The derivation statement in §4 must say which; a test of unstated derivation is treated as unprovenanced.
6. **Dependencies and notices.** Every dependency adopted, with its version, its licence, and the location of its notice or licence text in this repository.
7. **Data and benchmark artifacts.** Every dataset, input corpus, benchmark script, and benchmark result artifact, together with the run environment required by §4. A result artifact without a recorded run environment asserts nothing under Principle 3.
8. **Images and binary artifacts.** Every image, binary, model file, or other non-text artifact, identified by SHA-256 under §4 and subject to hash verification on transfer under §6.
9. **External contributions.** Every contribution, suggestion, patch, or artifact received from outside the clean-room roles, recorded on receipt, before it is used, and subject to the triggered audit in §7.
10. **Attestations.** Every review attestation, audit outcome, and release attestation, recorded as entries of Type `Attestation` in the same sequence as all other entries.
11. **Quarantine events.** Every quarantine: the artifact affected, the reason, who imposed it, and the entry that later lifts it or confirms it, if any. See §3 and §7.

## 3. Entry ID Scheme

Entries are numbered sequentially: `P-001`, `P-002`, `P-003`, and so on, in the order they are appended to the live ledger.

IDs are never reused. A superseded, quarantined, or withdrawn entry keeps its ID permanently; its successors take the next unused ID.

There is one sequence only. Quarantine entries are ordinary P-IDs carrying `Type: Quarantine`; there is no separate quarantine numbering series. This follows the A-002 pattern of a single sequence distinguished by type and status, rather than parallel series that can drift apart.

An ID is assigned when the entry is appended, not when the artifact is started. Draft entries held outside the ledger have no ID and no standing.

## 4. Fields Per Entry

The field list below is new process filling a gap: Roadmap §4.1 requires provenance recording but gives no entry field list. The list here is therefore labelled as A-004 process, not as a Roadmap quotation, and any change to it is a decision under §8.

Every entry in the live ledger carries every field below. A field that genuinely does not apply is recorded as `none` or `Not assigned`, as the field specifies; it is never omitted.

| # | Field | Content required |
|---|-------|------------------|
| 1 | ID | Sequential `P-###` under §3; never reused. |
| 2 | Date | ISO 8601 date and time, with timezone. |
| 3 | Artifact | Path or name, plus version or commit hash; SHA-256 for binaries and images. |
| 4 | Type | One of: Source / Spec / Design / Code / Test / Dependency / Data / Attestation / Quarantine / Release. |
| 5 | Origin | Author and clean-room role — Reference / Clean / Reviewer / External — plus the identifier used in repo history. |
| 6 | Source(s) consulted | Dated source-ledger references; `[S#]` where the source is a Roadmap source; version pinned for each source — edition, revision, release, or commit (Roadmap §14.3); licence or terms stated for each source. Primary sources are consulted in preference to secondary summaries (§14.3). |
| 7 | Derivation statement | One of: `original` / `derived-from-spec vX` / `licensed-reuse under <licence>` / `independent test`, with the spec version or licence named. |
| 8 | Clean-room role + separation note | The role under which the artifact was produced, and how separation from other roles was kept, including any role-overlap limitation. |
| 9 | Evidence link | Evidence Ledger E-ID and/or Decision Ledger D-ID, or `none`. Where a result is asserted, the link must name a file a reader can open in this repository. |
| 10 | Evidence level claimed | One of Roadmap §2's seven labels — Proposed, Implemented, Host-tested, Device-tested, Integration-proven, Performance-measured, Blocked — or `Not assigned`. Default: `Not assigned`. |
| 11 | Environment where produced/run | Device, OS, and architecture. Required if any result is asserted. Record `Pi: purpleviking1 / Mimir` only if the artifact was actually run there. |
| 12 | Supersedes / Superseded-by | P-ID superseded by this entry, or `none`; and, once known by superseding entry, the P-ID that supersedes this one. |
| 13 | Status | One of: Current / Superseded / Quarantined / Released. |
| 14 | Reviewer attestation ref | Reference to the reviewer's attestation entry or record, or `none` where review has not yet occurred. |

Notes on individual fields:

- **Origin and role overlap.** Where one person acted in more than one clean-room role for an artifact, the separation note must say so plainly and state what separation was and was not possible. Role overlap is a recorded limitation, never a silent fact.
- **Evidence level claimed.** This field records a claim made elsewhere and linked here; it does not make the claim. If the linked Evidence Ledger entry supports a lower level than this field states, the entry is discrepant and §7 applies.
- **Environment.** Naming an environment asserts that the artifact was produced or run there. An environment may not be named from intention, plan, or similarity to another environment.

### Blank entry skeleton

```markdown
### P-___ — <artifact short name>

- ID: P-___
- Date:
- Artifact:
- Type:
- Origin:
- Source(s) consulted:
- Derivation statement:
- Clean-room role + separation note:
- Evidence link: none
- Evidence level claimed: Not assigned
- Environment where produced/run:
- Supersedes: none
- Superseded-by: none
- Status: Current
- Reviewer attestation ref: none
```

A copy-paste version of this skeleton is also given in Appendix A.

## 5. Recording Workflow

Entries are created contemporaneously. That is: at consultation for a source, at spec freeze for a specification, at commit for code, at test run for a test or result, and at adoption for a dependency. Entries are never reconstructed at release time from memory or from the artifacts alone. A release-time reconstruction is exactly the failure this ledger exists to prevent, because the facts of origin — what was consulted, in which role, and in what order — are the facts most easily lost and least easily re-established later.

Division of labour: the originator writes the entry; the reviewer verifies it. Verification covers the fields, not only the artifact: the reviewer checks that the sources named are the sources recorded in the source ledger for that date, that the derivation statement matches the artifact, that the separation note is accurate including any role overlap, and that any evidence link resolves to a file a reader can open in this repository.

Timing of landing: the ledger update lands in the same commit or slice as the artifact it records, following Roadmap §14.11's rule that work lands in coherent, reviewable slices. An artifact landed without its entry is a process defect in that slice, to be corrected before the slice is built upon.

Unprovenanced artifacts: an artifact without an entry is unprovenanced. It may not be built on — no spec, code, test, or claim may rely on it — until the entry is back-filled. A back-filled entry must carry the explicit flag `Recorded retrospectively on <date>` in its separation note, with the reason reconstruction was necessary. The flag is permanent; a later superseding entry repeats it. A retrospective entry is honest about being retrospective; it does not acquire contemporaneous standing by being superseded.

## 6. Chain of Custody

Provenance forms an unbroken chain for every released artifact:

Source → Spec revision → Design → Code commit → Test run → Release artifact

Each hop in the chain is a P-ID. Each hop's entry cites the previous hop's P-ID and, where the previous hop is a file or binary, its commit hash or SHA-256. A hop that cannot cite its predecessor is the start of a new chain and must say why — for example, an original work with no source, recorded as `original` under §4 — or it is a custody gap under the rule below.

Transfer rules: every transfer requires an entry. This includes:

- role-to-role transfer, for example a frozen spec passing from the Reference role to the Clean role;
- branch-to-branch transfer, where an artifact moves between branches with any change in content or context;
- device-to-device transfer, where an artifact, log, or binary moves from the environment where it was produced to another environment.

Hash verification: on transfer of any binary, image, log, or data artifact, the SHA-256 is re-computed at the receiving end and checked against the sending entry. A mismatch quarantines the artifact under §7 until the discrepancy is resolved by a superseding entry.

Custody gaps: a gap in the chain is recorded as a gap, in an entry that names the last established hop and the first later established hop, and states that custody between them is not established. A gap is never bridged by assumption, by similarity of content, or by recollection. An artifact with an open custody gap in its chain may not be released until the gap is closed by evidence or the artifact is re-derived from the last established hop.

## 7. Audit Procedure

Audits occur on two bases:

- **Scheduled:** at every release, and at every phase exit.
- **Triggered:** on any quarantine event, on receipt of any external contribution that is proposed for use, and on any licence question affecting a logged source, dependency, or reuse.

Procedure:

1. Trace entries end-to-end along the §6 chain, from source to release artifact, checking that every hop cites its predecessor and that hashes match where hashes are recorded.
2. Check every Evidence level claimed field against the linked Evidence Ledger E-ID and against the raw log on the named device or environment. A claim the log does not support is a discrepancy.
3. Check forbidden-material completeness under the companion ruleset and licence-notice completeness for every Source, Dependency, and licensed-reuse entry in scope: licence or terms stated, notice present in the repository where required.
4. Confirm that no open quarantine stands against any artifact in the audit scope. Open quarantines block release: a release entry may not be appended for a scope containing an open quarantine.
5. Append the outcome as an entry of `Type: Attestation` (audit), recording the auditor, the scope, the findings, and every unresolved item by P-ID. An unresolved item is named, never summarised away.

Independence: an auditor may not audit their own artifacts alone. Where the auditor produced any artifact in scope, a second reviewer covers those artifacts, and the audit entry records the division.

Discrepancies: any discrepancy found — a broken chain hop, an unsupported evidence-level claim, a missing licence or notice, an unrecorded source — is corrected by a superseding entry under Principle 2. Where the origin of an artifact cannot be established, the artifact is quarantined: a `Type: Quarantine` entry is appended, its Status becomes `Quarantined`, and it may not be built on or released until origin is established or it is re-derived cleanly.

## 8. Relationship to the A-002 Ledgers

The three ledgers answer three different questions. None substitutes for another.

| Ledger | Question it answers | ID form | Governing rule |
|--------|---------------------|---------|----------------|
| Provenance Ledger (this process) | Where did this come from, and who touched it? | P-### | This document; companion ruleset. |
| Evidence Ledger | What is claimed, and what does the evidence not establish? | E-### | Honesty Rule; no inferred levels; no promotion. |
| Decision Ledger | What was decided, by whom, and on what basis? | D-### | Decisions by entry and supersession only. |

Consequences of the division:

- Provenance never substitutes for evidence. A flawless P-entry does not raise any evidence level by any amount. Evidence levels change only through Evidence Ledger entries supported by named, openable files, per the A-001 honesty rule.
- Evidence never substitutes for provenance. A passing test does not establish where the thing tested came from.
- Decisions govern both. D-001 governs both A-004 documents — this process and `docs/CLEAN_ROOM_RULESET.md`. New clean-room or provenance decisions are proposed as new Decision Ledger entries and take effect by supersession of the governing entry, never by silent change to either A-004 document or to the live ledger's practice.

Cross-linking is bidirectional:

- P-entries cite E-IDs and D-IDs in the Evidence link field wherever the artifact carries a claim or implements a decision.
- Later E-entries cite the P-ID of the artifact that produced the evidence, so a claim can be traced back to the artifact's origin.
- D-entries that concern a specific artifact cite its P-ID.

A cross-link that names an ID with no corresponding entry is a discrepancy under §7.

### Mapping: Clean-Room Ruleset duties ↔ this process

The table below is the 1:1 mapping between the duties in `docs/CLEAN_ROOM_RULESET.md` §§2–8 and the fields and entry types in this process. Read left-to-right it shows where each ruleset duty is recorded; read right-to-left it shows which ruleset duty each provenance field or entry type discharges. A duty with no row, or a field with no row, is a mapping defect to be raised as a Decision Ledger proposal under this section.

| Clean-Room Ruleset duty | Provenance field / entry type |
|-------------------------|-------------------------------|
| §4 — Act only within a declared clean-room role | Origin; Clean-room role + separation note |
| §4 — Declare any overlap of roles for one artifact | Clean-room role + separation note, including the role-overlap limitation |
| §4/§5 — Keep Reference and Clean work separated | Separation note; chain-of-custody transfer entry for the spec handoff (§6) |
| §2/§5 — Record every consulted source before the spec freezes | Type: Source entry; Source(s) consulted, dated, with `[S#]` where applicable |
| §5 — State the licence or terms of every source used | Source(s) consulted, licence/terms element; Type: Dependency entry for adopted dependencies |
| §5 — Freeze the specification revision implemented from | Type: Spec entry; Derivation statement `derived-from-spec vX` |
| §3/§7 — Do not carry forbidden material across the boundary | Type: Quarantine entry on any breach; audit step 3 (§7) |
| §5 — Derive code only from the frozen spec, or state reuse | Type: Code entry; Derivation statement, including `licensed-reuse under <licence>` |
| §2/§3 — Write tests independently, or state licensed reuse | Type: Test entry; Derivation statement `independent test` or `licensed-reuse under <licence>` |
| §8 — Record the environment of any run relied on | Environment where produced/run; Type: Data entry for result artifacts |
| §6 — Attest each slice and release before reliance | Reviewer attestation ref; Type: Attestation entry |
| §7 — Quarantine any artifact of unestablishable origin | Type: Quarantine entry; Status: Quarantined |
| §0/§6 — Correct the record only by supersession | Supersedes / Superseded-by; Status: Superseded |

## 9. Worked Example

The entry below is complete in form and empty in fact. It illustrates every §4 field for a hypothetical specification revision.

### EXAMPLE — NOT A REAL ARTIFACT

- ID: P-000 (example only; not assigned in the live ledger)
- Date: 2000-01-01T00:00:00+00:00 (example date)
- Artifact: `docs/example-spec.md`, revision v0-example (hypothetical)
- Type: Spec
- Origin: Example Author, Clean role, identifier `example-author` as it would appear in repo history
- Source(s) consulted: [S0] Example Source, consulted on the example date above, terms: example licence, for illustration only
- Derivation statement: derived-from-spec v0-example
- Clean-room role + separation note: Produced in the Clean role from the frozen example spec only. Example role-overlap limitation: in this hypothetical, the same person also acted as Reviewer, so full separation was not possible and review attestation is limited accordingly.
- Evidence link: none
- Evidence level claimed: Not assigned
- Environment where produced/run: Not applicable — no result is asserted by this example; no device is named because no run occurred.
- Supersedes: none
- Superseded-by: none
- Status: Current (within this example only)
- Reviewer attestation ref: none (example only)

No statement in this example is a statement about any real artifact, source, person, device, or result.

## Notes — Inconsistencies in the Source of Record

These notes record tensions found in the Roadmap while drafting A-004. None is silently resolved; each is handled as stated. A-004 notes are numbered A4-1..A4-6 to avoid collision with A-002's N-series and A-003's I-series.

- **A4-1 — §13 layout vs. precedent.** Roadmap §13 proposed a single combined `docs/clean-room-provenance.md`. A-004 uses two documents per the A-002 / A-003 precedent (N-5 / N-6). Declared in §0.
- **A4-2 — TCMalloc [S8] source-reading boundary.** The boundary for reading the TCMalloc source [S8] is ambiguous in Roadmap §4: it is not clear from §4 alone which role may read it, and for what. This process records such consultations as Source entries and leaves the permission question to the companion ruleset and, if needed, a Decision Ledger entry.
- **A4-3 — Licence position open in the Roadmap.** The licence question is left open in Roadmap §§4 and 15-Q12, and §8 Phase 0 (roadmap L226) lists "determine owner and license" as a future task, although an MIT LICENSE (© 2026 Volmarr Wyrd) already exists in the repository. This document does not re-open the question: the project licence is settled as MIT in-repo; entries state the licence or terms of each source and dependency as recorded.
- **A4-4 — No field list in §4.1.** Roadmap §4.1 requires provenance recording but gives no entry field list. §4 of this document is new A-004 process filling that gap, and is labelled as such there.
- **A4-5 — §2 labels unassigned and inconsistent in application.** Roadmap §2's evidence labels are not assigned consistently to existing work. This process therefore defaults Evidence level claimed to `Not assigned` and permits a label only where a linked Evidence Ledger entry and its raw log support exactly that label.
- **A4-6 — Solo-agent working vs. §4.1 separation.** Roadmap §14's solo-agent working and §4.1's role separation are unreconciled in the Roadmap. This process handles the tension by record, not by assertion: role overlap must be declared in the separation note (§4), and an auditor may not audit their own artifacts alone (§7).

## Appendix A — Blank Entry Skeleton (Copy-Paste)

```markdown
### P-___ — <artifact short name>

- ID: P-___
- Date:
- Artifact:
- Type:
- Origin:
- Source(s) consulted:
- Derivation statement:
- Clean-room role + separation note:
- Evidence link: none
- Evidence level claimed: Not assigned
- Environment where produced/run:
- Supersedes: none
- Superseded-by: none
- Status: Current
- Reviewer attestation ref: none
```

## Appendix B — Audit Checklist

For use with §7. Every box must be checked, or the unchecked item named as unresolved in the audit Attestation entry.

- [ ] Audit basis recorded: scheduled (release / phase exit) or triggered (quarantine / external contribution / licence question).
- [ ] Scope named by P-ID range and artifact list.
- [ ] Every chain in scope traced end-to-end: Source → Spec revision → Design → Code commit → Test run → Release artifact.
- [ ] Every hop cites the previous hop's P-ID and hash where applicable.
- [ ] Every binary, image, log, and data transfer in scope hash-verified against its sending entry.
- [ ] Custody gaps, if any, recorded as gaps; none bridged by assumption.
- [ ] Every Evidence level claimed checked against its linked E-ID.
- [ ] Every such claim checked against the raw log on the named device or environment; the log is a file a reader can open in this repository.
- [ ] No inferred evidence levels; no promotion between levels.
- [ ] No environment named that the artifact was not actually run in.
- [ ] Forbidden-material check complete for the scope.
- [ ] Licence and notice completeness checked for every Source, Dependency, and licensed-reuse entry in scope.
- [ ] No entry in scope contains secrets, credentials, private prompts, or device serial numbers.
- [ ] No open quarantine stands against any artifact in scope; if one does, the release is blocked and the block is recorded.
- [ ] No retrospective entry in scope lacks the `Recorded retrospectively on <date>` flag.
- [ ] Discrepancies corrected by superseding entries only; no entry edited in place.
- [ ] Auditor independence confirmed; no auditor audited their own artifacts alone.
- [ ] Audit outcome appended as `Type: Attestation`, naming auditor, scope, findings, and every unresolved item by P-ID.
