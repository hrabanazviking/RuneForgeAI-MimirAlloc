# MimirAlloc — Phase 0 Exit Checklist

**Status:** Checklist for slice A-001. Source: Roadmap §8, Phase 0 — Charter, naming and baseline (`docs/MimirAlloc_Research_and_Development_Roadmap.md`).
**Scope:** Phase 0 only. This checklist records the state of the Phase 0 tasks and exit gate as of the A-001 commit landing.

**Honesty rule:** A row is done only when its evidence pointer names a file a reader can open in this repository. Partial work is never rounded up to done.

---

## Status legend

| Status | Meaning |
|---|---|
| done | The task is complete and its evidence pointer names a file a reader can open in this repository. |
| owed | The task is not done and no complete artifact for it exists yet. |
| unverified | Some related evidence exists, but the check or record the task requires has not itself been separately performed or pinned. |

---

## Phase 0 task mapping (Roadmap §8)

| # | Phase 0 task (roadmap §8 wording) | Artifact | Status | Evidence pointer | Notes |
|---|---|---|---|---|---|
| 1 | Create project charter | docs/PROJECT_CHARTER.md (slice A-001) | done | docs/PROJECT_CHARTER.md | The charter lands in the same commit as this checklist, so this row is done as of the A-001 commit landing. |
| 2 | Determine owner | Charter §2 | done | LICENSE (Copyright (c) 2026 Volmarr Wyrd); repo hrabanazviking/RuneForgeAI-MimirAlloc | Owner is recorded in the LICENSE copyright line at repo root. |
| 3 | Determine license | Charter §3; LICENSE | done | LICENSE — full MIT text at repo root | Roadmap §4/§15 Q12 still phrase this as open; the LICENSE file governs. |
| 4 | Reserve/check repository name | Charter §10 | unverified | The GitHub repo exists under this name (README links); no reservation/availability record exists in the repo beyond that. | Existence of the repository is not a recorded name-availability check. |
| 5 | Check package names | Charter §10 | unverified | None exists — no registry check (crates.io, PyPI, npm, distro) is recorded anywhere in docs/. | No package-name check was run or recorded for A-001. |
| 6 | Record the mimalloc naming concern | Charter §10 | done (as a record) | Roadmap title block; §2 evidence table "Name" row; §15 Q11; source [S4] microsoft/mimalloc | Recorded, not resolved — legal availability is rows 4/5's unverified, not this row's done. |
| 7 | Set code of conduct | CODE_OF_CONDUCT.md (proposed, repo root) | owed | None: no such file exists; no .github/ directory exists in the repo. | Proposed location only; nothing has been adopted yet. |
| 8 | Set issue templates | .github/ISSUE_TEMPLATE/ (proposed) | owed | None: no .github/ exists. | Roadmap §13's MIM-001…MIM-012 list is suggested initial issues, not issue templates — do not cite it as this row's artifact. |
| 9 | Create evidence ledger | Project-level ledger (no file yet) | owed | Partial predecessors only: field-notes 01_EVIDENCE_AND_TIMELINE.md (scoped to the tablet investigation), 14_SOURCES_AND_ARTIFACTS.md, research source ledgers. | No single project-level evidence ledger exists at docs/ root. |
| 10 | Create decision ledger | Project-level ledger (no file yet) | owed | Partial predecessor only: roadmap §15 decision register (13 open questions, phrased as questions, not decisions). | Roadmap §13 proposes docs/decisions/; it does not exist. |
| 11 | Capture current Aesir build revision (baseline) | Baseline record | unverified | Field-notes 01 records one Aesir checkout (3358973c…) used to preserve device diagnostics — a diagnostics artifact reference, not a captured current-build baseline. | No pinned current Aesir revision statement exists. The roadmap tree itself pins no Aesir revision. |
| 12 | Capture current Mojo build revisions (baseline) | Baseline record | owed | Observations exist in field-notes 01: Mojo 1.2.0 stable (Linux ARM64, glibc) and nightly 1.3.0.dev2026110205, both failing at TCMalloc startup; Modular source 99ab7e90… noted. | Caveat from 01: wheel↔source correspondence not established — observations are not a pinned baseline. |
| 13 | Capture tablet inventory | Field-notes 02 | done | docs/field-notes/android-arm64-2026-11/02_TABLET_INVENTORY.md: Samsung Galaxy Tab S10 Lite Wi-Fi SM-X400, Android 16 / API 36, build X400XXS8CZI2, SoC s5e8835 (Exynos 1380), MemTotal 5,841,464 KiB, page size 4,096 bytes in one probe. | 02's "Missing signals" stand: no thermal/battery/low-memory-threshold data was obtained. The inventory is a 2–3 Nov 2026 snapshot — done for the inventory as scoped, not a standing device claim. |

---

## Phase 0 exit gate (Roadmap §8 Exit line)

| # | Exit item | Status | Evidence / Notes |
|---|---|---|---|
| E1 | Project scope documented | done | Lands with the charter, slice A-001 (docs/PROJECT_CHARTER.md). |
| E2 | Target matrix documented | owed | External: slice A-003's artifact. This checklist cross-references A-003 only and never copies the §5 table; A-001 cannot close E2. |
| E3 | Release terminology documented | done | The charter adopts Roadmap §2 labels. |
| E4 | Exact baseline documented | owed | Depends on rows 11–13; row 11 is unverified and row 12 is unpinned, so E4 cannot close in A-001. |
| E5 | Naming availability | unverified | Roadmap's own exit text: "Naming availability remains unverified until separately checked." No separate check exists or was run for A-001. |

---

## Bottom line

After A-001 lands, Phase 0 is NOT exited — E2 (A-003), E4 (baseline pinning) and E5 (a separately-run naming check) remain open.

**Tally:** task rows — 4 full done + 1 done (as a record) / 5 owed / 3 unverified (of 13). Row 6 is counted in its own record-only category: it is not counted in the full-done total, and it is not counted as owed.

---

## Cross-reference

The supported-target matrix skeleton is slice A-003's artifact. This checklist references it; it does not contain it.

## What this checklist is not

- Not a naming clearance.
- Not a release note.
- Not evidence for any later phase.
