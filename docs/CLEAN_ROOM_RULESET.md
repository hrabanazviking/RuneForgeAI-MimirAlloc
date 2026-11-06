# MimirAlloc Clean-Room Ruleset

Slice: A-004 (Wave 1) — clean-room ruleset + provenance process
Source of record: docs/MimirAlloc_Research_and_Development_Roadmap.md Section 4 (roadmap base commit b0b8374)
Companion document: docs/PROVENANCE_LEDGER_PROCESS.md
Companion ledgers: docs/EVIDENCE_LEDGER.md, docs/DECISION_LEDGER.md

Status: Process document only — no implementation, measurement, or
legally verified clean-room claim is made by this slice.

Deviation from roadmap layout: Roadmap §13 proposed a single combined
`docs/clean-room-provenance.md`. A-004 supersedes that layout with two
documents — this ruleset and `docs/PROVENANCE_LEDGER_PROCESS.md` —
following the `docs/` precedent established by A-002 and A-003.
See Notes N-5 / N-6 and note A4-1 below.

## §0 Status and Scope

This document operationalises Decision D-001 (Adopted, Roadmap §4) as recorded in `docs/DECISION_LEDGER.md`. D-001 is the decision to develop MimirAlloc under a clean-room discipline: original implementation work derived from an original-language behaviour specification, public contracts, and observed requirements, rather than from the protected expression of any third-party allocator.

This ruleset is binding process for the project from adoption until it is superseded. Any change to the rules in this document requires a superseding decision entry in `docs/DECISION_LEDGER.md` that:

- names this document and the section(s) changed,
- states the reason for the change,
- states the effective date and the slice or release from which it applies, and
- is recorded before the changed rule is relied upon.

No silent amendment is permitted. A pull request, commit message, issue comment, or prompt that contradicts this ruleset does not amend it.

Scope: this ruleset governs source material, specification writing, implementation, testing, dependency adoption, attestation, and release for MimirAlloc. It does not govern the technical design of the allocator itself, which is addressed elsewhere in the roadmap.

Disclaimer: This document describes an engineering process. It is not a legal opinion, not legal advice, and not a representation that compliance with this process satisfies any particular legal standard in any jurisdiction. Legal questions — including licence interpretation, trademark, and patent risk — are escalated under §9 and §10 of this document and are not decided here.

Honesty rules carried forward:

- A-001 honesty rule: A row, requirement, or process step is done only when its evidence pointer names a file a reader can open in this repository. A claim without an openable pointer is not done.
- A-002 honesty rule: Never claim a measurement or result that was not actually run and logged on the Pi. No inferred, extrapolated, or upgraded evidence levels. If it was not run and logged, it is not claimed.
- Roadmap §14.11, stated for tests specifically: Never claim tests that were not run. A test result may be stated only for a test that was actually run, in the environment named, and logged.

These honesty rules apply to every statement made under this ruleset, including attestations, ledger entries, and release notes.

## §1 Purpose and Definitions

### 1.1 Purpose

The purpose of the clean-room process is to produce an original allocator implementation whose provenance can be shown, artifact by artifact, from records kept at the time the work was done — not reconstructed afterwards.

The process exists to:

1. keep third-party protected expression out of the specification, the code, the comments, the tests, and the prompts used to direct implementing agents;
2. make every transfer of information into the implementation visible, dated, and attributable; and
3. limit every public claim about the process to what the records actually support.

### 1.2 Definition: clean-room (as used in this project)

Clean-room, in this project, means: an original implementation written from an original-language behaviour specification, public contracts, and observed requirements, where the implementers did not copy or paraphrase third-party implementation source into the specification, code, comments, tests, or prompts.

The term describes the discipline followed. It does not, by itself, assert a legal conclusion.

### 1.3 Definitions

Source material: Any document, code, data, recording, or other material consulted, read, observed, or received in the course of project work, whether or not it influenced the output.

Protected expression: The expressive content of a work — its source code text and structure, comments, documentation prose, test text, naming and arrangement where expressive — as distinct from facts, interfaces, behaviour, and general concepts. This is a working engineering definition for process purposes, not a legal definition.

Behaviour specification (spec): The project's own written description of required behaviour, interfaces, invariants, error handling, and observable outcomes, expressed in original language by the Reference / Investigator Team (see §4). The spec is the only substantive channel from source material to implementers.

Implementation artifact: Any output of the Clean / Implementation Team, including source code, comments, tests, build files, documentation, commit messages, and release notes.

Contamination: The presence, in a specification, implementation artifact, test, comment, prompt, or other implementation-side material, of protected expression derived from forbidden source material (§3), or of any source material whose provenance was not recorded before use. Contamination is a process fact to be recorded and handled under §7; it is not resolved by editing.

Quarantine: The recorded state of an artifact that is suspected or confirmed to be contaminated. A quarantined artifact is removed from the implementation branch and build, preserved as a sealed audit record, and barred from reuse until quarantine is lifted by a new ledger entry under §7. See §7 for the procedure.

Attestation: A dated, signed statement by an identified person or agent, in the form required by §6, recording which spec version was used, that no §3 material was consulted or copied for the attested work, and the known limitations of that claim.

### 1.4 Claim ceiling

Per Roadmap §4.1.6, the claim ceiling is absolute:

- Never label a solo or AI-assisted process a "legally verified two-team clean room."
- Never upgrade a claim retrospectively because the output looks original, passes tests, or differs in wording from a source.
- Permitted claim language is limited to what the provenance ledger and attestations actually show, for example: "developed under the clean-room process recorded in `docs/PROVENANCE_LEDGER_PROCESS.md`, with the limitations recorded therein."

Where separation was limited (see §4.4), the claim must say so.

## §2 Allowed Source Material

Whitelist rule: Only the categories listed in this section are allowed. If material is not listed here, treat it as forbidden until a provenance entry clears it for a stated use. The burden is on listing and recording, not on arguing similarity afterwards.

Allowed material informs the specification only. It does not pass directly to implementers except as re-expressed in the spec, in original language, per Roadmap §4.1.2. See §5.2 (spec-freeze gate).

### 2.1 Standards and authoritative documentation

(a) Standards and authoritative technical documentation, including POSIX, the C11 standard, and Linux man pages [S5] [S6]. Facts about required interfaces, semantics, error numbers, and conformance behaviour may be recorded in the spec in original language.

### 2.2 Public high-level documentation

(b) Public high-level documentation and architecture descriptions [S1] [S2] [S7], including Android ABI and page-size documentation [S3] [S10]. High-level architectural facts, public interface descriptions, and stated design goals may inform the spec. Implementation source that may be linked from, or adjacent to, such documentation is not included in this allowance (see §3).

### 2.3 Project's own material

(c) The project's own roadmap, contracts, field notes, and research library, provided the material was itself produced under this ruleset or its provenance is recorded. Project material that embeds third-party protected expression does not become allowed by being copied into a project file.

### 2.4 Observable behaviour

(d) Observable behaviour of existing systems, including symbol tables, linkage and ABI facts, error messages as observed, and black-box outcomes of running a system through its public interface. Observations must be recorded as observations — what was run, what was observed, when, and by whom — in the provenance or evidence records. Observation does not licence copying implementation internals obtained by disassembly or decompilation (see §3(b)).

### 2.5 Independently written test cases

(e) Independently written test cases, per Roadmap §4.1.8. A test written from the spec or from observed public behaviour is allowed. A test copied from a third-party suite is not covered by this allowance; it is governed by §3(c).

### 2.6 General concepts

(f) General allocator concepts and textbook knowledge: algorithms, data structures, and principles in general circulation (for example size classes, free lists, arenas, thread caching as a concept). General concepts may inform design discussion and the spec. A specific third-party implementation's expression of a concept is not a general concept.

### 2.7 Recording duty for allowed material

Each item of allowed source material actually consulted for a spec must be entered in the provenance ledger before, or at the time, the spec version relying on it is frozen (see §5.2 and `docs/PROVENANCE_LEDGER_PROCESS.md`). Unrecorded consultation is treated as contamination risk under §7 if discovered later.

Consult primary sources in preference to secondary summaries, and pin versions (Roadmap §14.3): each provenance entry for a consulted source records the version consulted — edition, revision, release, or commit — so the record shows exactly what was read. An entry without a pinned version is incomplete.

## §3 Forbidden Material and Conduct

The following are forbidden on the implementation side of the project, without exception unless a superseding decision under §0 expressly permits a stated item and records the licence basis.

### 3.1 Forbidden list

(a) Third-party implementation source copied or paraphrased into the spec, code, comments, or PROMPTS to implementing agents (Roadmap §4.1.4). Paraphrase is not a cleansing step. Renaming identifiers, reordering statements, translating language, or summarising code structure for an implementer all remain forbidden transfer of protected expression.

(b) Copied snippets from any source; proprietary material; and material obtained by unlicensed disassembly or decompilation. A snippet does not become allowed by being short.

(c) Verbatim third-party tests, unless the licence permitting that use is followed and recorded in the provenance ledger and in dependency notices before use (Roadmap §4.1.8). If the licence cannot be identified or its conditions cannot be met, the test is forbidden.

(d) Branding that implies Google or Microsoft affiliation, and branding confusable with mimalloc (Roadmap §4, §15 Q11). Names, logos, package names, and documentation must not suggest endorsement, origin, or affiliation that does not exist, and must pass the name check in §9 before release.

(e) Material with unknown or unrecorded licence. If the licence is unknown, the material is forbidden until the licence is identified and recorded. "Found online" is not a licence.

### 3.2 Boundary rule: TCMalloc source

Reading TCMalloc source [S8] is Reference-Team-only.

Only TCMalloc's public documentation and interface facts, re-expressed in original language in the behaviour specification, may cross to implementers. TCMalloc source text, structure, comments, internal naming, and paraphrases thereof must not appear in the spec, in prompts, in issues, or in any other channel to the Clean / Implementation Team.

Licence note (Roadmap §4 preamble): TCMalloc is open source. Its licence and attribution requirements matter if any of its source is actually reused; reuse on a recorded licence basis is licensed reuse under §9.2, not contamination. An upstream maintainer declining a patch is not itself a licence restriction, and creates no bar to independent work under this ruleset.

Flag: This boundary is recorded as A-004 interpretation of the roadmap, arising from note A4-2 (see Notes). Roadmap §4 does not state the boundary in this exact form; A-004 adopts the stricter reading — source confined to the Reference Team, only re-expressed public facts crossing via the spec — because the looser reading would defeat §4.1.4. If the project owner intends a different boundary, a superseding decision under §0 is required; until then the stricter reading governs.

### 3.3 Forbidden conduct

In addition to forbidden material, the following conduct is forbidden:

- showing forbidden material to an implementer "for context";
- asking an implementing agent to reproduce, complete, or "clean up" third-party code;
- pasting forbidden material into an issue, chat, or prompt that an implementing agent will read;
- removing provenance or licence notices to make material appear project-original; and
- deleting evidence of contamination (see §7, step 1).

## §4 Roles and Separation

### 4.1 Roles

Reference / Investigator Team: Consults allowed and, where this ruleset expressly confines it to this role, reference-only material; investigates observable behaviour; writes and revises the behaviour specification in original language.

Clean / Implementation Team: Implements solely from the frozen spec version, public contracts recorded in the spec, and project material cleared for implementers. Does not consult third-party allocator implementation source for the work in hand.

Independent Reviewer: Reviews specs, artifacts, attestations, and quarantine records for compliance with this ruleset; performs or commissions the source-similarity review in §9; countersigns attestations and quarantine lifts where a reviewer exists. The reviewer must not have written the artifact under review.

Roles are functions, not headcount. One person or agent may hold different roles on different tasks, subject to §4.4.

### 4.2 Role permissions table

| Role | Permitted inputs | Permitted outputs | Forbidden contacts |
|---|---|---|---|
| Reference / Investigator | §2 allowed material; reference-only material confined to this role (§3.2); observable behaviour records | Original-language behaviour spec; observation records; provenance entries | Must not pass protected expression, paraphrase, or source structure to implementers by any channel |
| Clean / Implementation | Frozen, provenance-logged spec version; project contracts cleared for implementers; independently written tests (§2.5) | Implementation artifacts; questions back to Reference Team via logged channel | Must not consult, request, or receive §3 material; must not consult the Reference Team's source notes |
| Independent Reviewer | Spec versions, implementation artifacts, provenance ledger, attestations, quarantine records | Review findings; countersignatures; quarantine lift or refusal entries | Must not review own work; must not edit a quarantined artifact clean (see §7) |

### 4.3 How separation is recorded

Separation is by role, artifact, and time, recorded per task. For each task or slice, the provenance ledger records:

- who or what agent held each role,
- which spec version was the implementation input,
- the date range during which each role was exercised, and
- every transfer from Reference Team to Implementation Team, which must be the frozen spec itself or a logged clarification issued as a new spec version.

An unrecorded transfer is treated as no transfer for claim purposes, and as a process breach for review purposes.

### 4.4 Mandatory limitation clause — role overlap

Where the same person or agent acts in both the Reference / Investigator role and the Clean / Implementation role sequentially — as is expected in a solo / AI-assisted project — then:

1. the role-switch must be logged in the provenance ledger, with dates and the artifacts affected;
2. the claim for the affected work is downgraded to "separation-limited"; and
3. the downgrade is never upgraded retrospectively, for any reason, including later independent review.

Independent review may verify that the recorded process was followed. It does not convert separation-limited work into two-team work.

## §5 Separation Maintenance

### 5.1 Worktree and branch hygiene

- No forbidden material in implementation branches: not in code, comments, tests, documentation, commit messages, branch names, issue text, or prompts stored in the repository.
- Reference material that is itself stored in the repository must live only where the provenance process designates, must never be merged into an implementation branch, and must be recorded in the provenance ledger.
- Before opening implementation work on a branch, check the branch diff for accidental inclusion of reference material. A discovery is handled under §7, not by quiet removal.

### 5.2 Spec-freeze gate

Implementation starts only from a dated, provenance-logged spec version. Working from a draft, a chat summary, or an unlogged revision is forbidden. A clarification that changes behaviour is a new spec version, frozen and logged in the same way.

The gate record for each implementation slice names: spec title, spec version, freeze date, provenance entry identifier, and the implementing role-holder.

### 5.3 No unlogged transfer

No information from reference material reaches implementers except through the frozen spec. Corridor conversations, chat messages, and prompt context are transfers if they convey substance; if it conveyed substance, it must be in the spec and in the ledger, or it must not happen.

### 5.4 Dependencies

Every new dependency gets a provenance entry and a licence notice before first use (Roadmap §4.1.7). The entry records: dependency name and version, source, licence as identified from the dependency itself, and the intended use. Use before entry is forbidden. Dependency licence obligations are carried into the release gate in §9.

### 5.5 Design from the specification

Per Roadmap §4.1.3, the design of each component is created from the behaviour specification and general allocator concepts (§2.6) — not from third-party implementation material. For each design, the alternatives considered and the decisions taken are documented in the design record; where a decision governs later work, it is also entered in `docs/DECISION_LEDGER.md`. A design without a documented alternatives-and-decisions record is incomplete under this ruleset. (This section states the process duty for design records; it does not itself make allocator design decisions.) Design documents and decisions are provenance-logged under `docs/PROVENANCE_LEDGER_PROCESS.md` §2(3).

## §6 Attestation

### 6.1 When attestation is required

Attestation is required per slice and per release. Work without a corresponding attestation is unattested and must not be described as covered by the clean-room process.

### 6.2 Required fields

Each attestation must contain all of the following:

- Attestor: name or agent identifier of the person attesting.
- Role: the role held for the attested work (see §4).
- Slice / artifact IDs: the slice identifier and the artifacts covered, specific enough to match the ledger and the commits.
- Spec version: the frozen spec version implemented from.
- Statement: that no material forbidden by §3 was consulted or copied for the attested work, by any channel including prompts.
- Known limitations: including any role overlap (§4.4), separation limits, and any other limit on the claim.
- Date: the date of attestation.
- Commit hash: the commit hash of the attested artifact state.

An attestation missing a required field is incomplete and does not support a claim until completed by a new, dated entry.

### 6.3 Filing and countersignature

Attestations are appended to the provenance ledger in the form given in Appendix B. They are append-only: corrections are new entries that reference the original.

Where an Independent Reviewer exists for the work, the reviewer countersigns the attestation. Where no reviewer exists, the attestation must record, verbatim in substance:

`Reviewer: none — limitation recorded`

Absence of a reviewer is a limitation on the claim, not a defect that may be hidden.

### 6.4 Attestation and honesty

An attestation states what the attestor knows from the records and from their own work. It must not attest to a measurement, review, or separation that did not occur (see §0 honesty rules).

## §7 Contamination Handling and Quarantine

### 7.1 Triggers

This procedure is triggered by: discovery of §3 material in any implementation-side artifact; discovery that consulted material was never provenance-logged; a credible report from any contributor or reviewer; or a source-similarity finding under §9 that cannot be explained by the spec, public contracts, or general concepts.

Suspicion is enough to trigger the procedure. Proof is the output of the procedure, not its precondition.

### 7.2 Procedure — in order

(1) STOP. Stop work on the affected artifact. Do not delete evidence. Do not edit the artifact to remove the suspected material; deletion or editing at this stage destroys the record the rest of the procedure depends on.

(2) MARK QUARANTINED. Enter the artifact in the provenance ledger as QUARANTINED, recording: date, discoverer, artifact identifier, and the scope as then known. Until this entry exists, no further step is treated as started (A-001 honesty rule).

(3) REMOVE AND SEAL. Remove the artifact from the implementation branch and from the build. Preserve a sealed record — content hash plus storage location — for audit only. The sealed record is not a source for reuse, reference, or "checking the original."

(4) IMPACT ASSESSMENT. Record an assessment listing every downstream artifact that derived from, included, or was written with knowledge of the quarantined artifact: code, tests, docs, spec sections, and releases. Each listed artifact is itself assessed; unassessed downstream artifacts block release (§9).

(5) REMEDIATION. Rewrite the affected work from the clean spec by a non-exposed implementer where possible. Where no non-exposed implementer exists — the expected case in solo / AI-assisted work — record that limitation expressly and restart the affected work from the spec; the remediated work carries the separation-limited status of §4.4 and is never described as if the exposure had not occurred.

(6) REVIEWER SIGN-OFF TO LIFT. Quarantine is lifted only by a new provenance ledger entry recording reviewer sign-off, the remediation performed, and the artifacts cleared. Where no reviewer exists, the lift entry must record that fact as a limitation in the same terms as §6.3. The original quarantine entry is never edited or deleted.

(7) RELEASE ESCALATION. If the contaminated artifact reached a commit intended for release, or a release itself, flag the matter for the project owner and for licence / legal review before any release proceeds. No release proceeds on the strength of the project's own assessment alone in this case.

### 7.3 Absolute rule

A quarantined artifact is never quietly edited clean. Editing the artifact and continuing as if nothing happened is itself a process breach, independent of whether the edit removed the contaminating material.

## §8 Evidence Required

The process claim rests on contemporaneous records. The minimum evidence set, per Roadmap §4.1.1 and §4.1.7, is:

- a dated source ledger (the provenance ledger, maintained under `docs/PROVENANCE_LEDGER_PROCESS.md`);
- behaviour specification versions, each dated and frozen (§5.2);
- attestations per slice and per release (§6);
- quarantine entries and lift entries, where any exist (§7);
- commit history that corresponds to the ledger entries;
- contributor sign-off for each contributor's work; and
- dependency notices: name, version, source, and licence for every dependency (§5.4).

Factual claims — about behaviour, measurements, compatibility, or observed outcomes — go to `docs/EVIDENCE_LEDGER.md`, with the evidence level and pointer that ledger requires. The provenance ledger proves where material came from; the evidence ledger supports what is claimed to be true. Neither substitutes for the other.

Missing evidence is recorded as missing. It is not backfilled from memory, and reconstructed records must be labelled as reconstructions with their actual creation date.

## §9 Release Gate

No release is made with an open quarantine. In addition, every release must pass the following pre-release checklist (Roadmap §4.1.9) before it is published:

### 9.1 Checklist

- [ ] Source-similarity review completed and recorded: artifacts compared against relevant third-party sources by the Independent Reviewer or a recorded method, findings filed, and any unexplained similarity handled under §7.
- [ ] Licence obligations discharged: every dependency and every item of licensed reuse carries its required notices and attribution, in the form its licence requires (§9.2).
- [ ] Trademark / name check completed: project name, package names, and documentation checked against §3(d) — no implied Google or Microsoft affiliation, no mimalloc-confusable branding (§15 Q11).
- [ ] Patent-risk escalation: any identified patent risk, and any uncertainty the reviewer flags as patent-relevant, is escalated to counsel before release. The project does not clear its own patent risk.
- [ ] Attestations for the release are complete and filed (§6), with limitations stated, including any role overlap.
- [ ] No open quarantine (§7), and no unassessed downstream artifact from any closed quarantine.

A checklist item is complete only when its record names a file a reader can open in this repository (A-001 honesty rule).

### 9.2 Licensed reuse versus contamination

Licensed reuse and contamination are different things and must never be conflated in records or release notes:

| Category | Basis | Treatment |
|---|---|---|
| Licensed reuse | Identified licence permits the use; conditions known before use | Licence / attribution matter: comply with the licence, carry notices and attribution, record in provenance ledger and dependency notices |
| Contamination | §3 material present without licence basis or provenance record | Quarantine matter: §7 procedure in full; no licence or attribution added afterwards cures it |

Attribution is not a remedy for contamination. Crediting a source does not convert copied protected expression into licensed reuse where no licence permitted the copying.

## §10 Limitations and Non-Claims

1. Not legal advice. This ruleset is an engineering process document (see §0). It does not opine on copyright, licence, trademark, or patent law, and following it is not represented as a guarantee of any legal outcome.

2. Project licence is settled. The project licence is already MIT, in-repo (`LICENSE`, © 2026 Volmarr Wyrd). This ruleset does not re-open, reinterpret, or replace that choice. The MIT licence governs the project's own code as stated in `LICENSE`; it does not licence any third-party material to the project.

3. Solo / AI-assisted limitation. This project is developed solo with AI assistance. Role separation under §4 is separation by role, artifact, and time, with role-switching logged and claims downgraded to "separation-limited" under §4.4. No document, attestation, or release note for this project may claim a legally verified two-team clean room (see §1.4).

4. Records limitation. Every claim under this ruleset extends only as far as the contemporaneous records extend. Where records are absent, the honest statement is that the process step is unattested — not that it probably happened.

5. Measurement limitation. Nothing in this ruleset is evidence of performance, correctness, or compatibility. Such claims require Pi-run, logged measurements under the A-002 honesty rule and entries in `docs/EVIDENCE_LEDGER.md`.

## Notes — Inconsistencies in the Source of Record

The following inconsistencies in the roadmap were identified while drafting this ruleset. Each is recorded briefly here; the treatment adopted in this document is stated, and any change of treatment requires a superseding decision under §0. A-004 notes are numbered A4-1..A4-6 to avoid collision with A-002's N-series and A-003's I-series.

A4-1 — Layout. Roadmap §13 proposed a single combined `docs/clean-room-provenance.md`, while A-002 / A-003 established a `docs/` precedent of separate process documents and ledgers. Treatment: A-004 supersedes §13 with two documents (see header; Notes N-5 / N-6).

A4-2 — TCMalloc source boundary. The roadmap's treatment of TCMalloc source [S8] is ambiguous as to who may read it and what may cross to implementers. Treatment: §3.2 adopts the stricter reading — Reference-Team-only, only re-expressed public facts cross via the spec — and flags it as A-004 interpretation.

A4-3 — Project licence treated as open. Roadmap §§4 / 15 Q12 discuss project licence as if undecided, and §8 Phase 0 (roadmap L226) lists "determine owner and license" as a future task, although an MIT `LICENSE` (© 2026 Volmarr Wyrd) already exists in-repo. Treatment: §10(2) records the licence as settled and not re-opened by this process.

A4-4 — Missing procedures in §4.1. Roadmap §4.1 states principles but contains no contamination procedure, no attestation procedure, and no required field list. Sections §6 and §7 of this ruleset are therefore new process, created by A-004, and are labelled as such; they are not presented as restatements of roadmap text.

A4-5 — Evidence labels. Roadmap §2 assigns evidence labels that are attached to no specific claim and used inconsistently across the document. Treatment: where no label is properly assigned, the default recorded in ledgers is "Not assigned"; no label is inferred or upgraded.

A4-6 — Solo duty versus team separation. Roadmap §14 frames duty in solo-agent terms, while §4.1.5 / §4.1.6 frame separation in team terms; the two are never reconciled in the roadmap. Treatment: §4 of this ruleset is the reconciliation — roles are functions held per task, separation is by role, artifact, and time, role-switching is logged, and claims are downgraded to "separation-limited" (§4.4).

## Appendix A — Do / Don't Table (one page)

| Do | Don't |
|---|---|
| Write the spec in your own original language from allowed sources | Copy or paraphrase third-party source into the spec |
| Enter every consulted source in the provenance ledger before the spec freezes | Consult first and log later — or never |
| Implement only from a dated, frozen, logged spec version | Implement from drafts, chats, or memory |
| Record observable behaviour as dated observations | Copy internals obtained by disassembly or decompilation |
| Write tests independently from the spec (§4.1.8) | Copy third-party tests without a followed, recorded licence |
| Give every dependency a provenance entry + licence notice before first use | Add a dependency and document it afterwards |
| Log every role-switch; downgrade the claim to "separation-limited" | Claim a two-team clean room for solo / AI-assisted work |
| Record `Reviewer: none — limitation recorded` where no reviewer exists | Leave the reviewer field blank or imply a review occurred |
| STOP, mark QUARANTINED, and seal the record on any contamination trigger | Quietly edit a suspect artifact clean and carry on |
| Distinguish licensed reuse (attribution matter) from contamination (quarantine matter) | Treat attribution as a cure for contamination |
| Claim only measurements actually run and logged on the Pi | Infer, extrapolate, or upgrade evidence levels |
| Escalate licence, trademark, and patent questions under §9 | Decide legal questions inside the engineering process |

## Appendix B — Attestation Template

Copy this template into the provenance ledger for each slice and each release (see §6). All fields are required.

```text
ATTESTATION
Attestor:           <name or agent identifier>
Role:               <Reference / Investigator | Clean / Implementation | other, stated>
Slice / artifacts:  <slice ID; artifact IDs covered>
Spec version:       <spec title, version, freeze date, provenance entry ID>
Statement:          I attest that, for the work identified above, I did not consult or copy any material forbidden by docs/CLEAN_ROOM_RULESET.md §3, by any channel including prompts, and that the implementation derives from the spec version named above.
Known limitations:  <role overlap, separation limits, or "none known" — state expressly>
Reviewer:           <reviewer name + countersignature entry ID, or: none — limitation recorded>
Date:               <YYYY-MM-DD>
Commit hash:        <full commit hash of attested state>
```

An attestation that omits a field, or that attests to a review, measurement, or separation that did not occur, does not support any process claim (see §0 and §6.4).
