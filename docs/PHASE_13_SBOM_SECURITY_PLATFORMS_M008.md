# Phase 13 - SBOM, Security, Platforms, Limitations Register (M-008)

Slice: M-008 (team-08, SOLO sequential lead). Wave 13, Phase 13.
Base (parent, exact): 6b312e3197cdb2e491ab0ffa24a4ba8ab95a1b17
(M-004 bank; Wave 13 = 4 of 9 at muster). Branch yrsa/mimiralloc-08,
worktree /home/pi/mimiralloc-team-08 on Raspberry Pi purpleviking1.
Banking order: eighth in Wave 13, after M-007. This lead never
pushes and never merges. README.md byte-identical to the base
(diff 0 lines, verified). All deliverables are NEW files; no banked
document is modified.

## 1. Sources and dependency rules as applied

Read in full at the base: every phase exit packet (Phase 0
checklist; Phases 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 exit evidence);
the Wave 13 records banked so far (M-001 foundation plan including
its carried-owed register and SRC-01..SRC-20, M-002 code review,
M-003 warning gates, M-004 fuzzing); docs/DECISION_LEDGER.md and
docs/EVIDENCE_LEDGER.md; docs/SUPPORTED_TARGETS.md (A-003-era);
the Roadmap Sections 9.2 and 13/Phase 13; include/mimir_alloc.h;
the Makefile in full; and WAVE_PLAN.md in the coordinator checkout
(keeper bookkeeping - a source for owed items, cited as such,
never quoted as a public document).

Platform dependency rule, applied exactly as assigned: the M-006
matrix refresh had NOT banked at muster, so
docs/SUPPORTED_PLATFORMS.md is built from the A-003-era
SUPPORTED_TARGETS in this tree (235 lines, sha256
fcd425186e05f246f48ac8457979a722ca125fb1af7e7a8acbca1a74e521c204,
blob 9e24512 as recorded in the assignment) corrected only by
banked evidence in this tree, with the disclosure in its header.
No unbanked M-006 worktree content was imported, copied, or
anticipated.

M-007 dependency rule, applied: versioning/SONAME/artifacts are
M-007 work in flight (verified at the base: no VERSION file, no
release/SHA256SUMS in this tree). This slice refers to the release
generically ("the current release artifacts and their published
checksums"), invents no version number, SONAME, or artifact name,
and raises INC-M008-02 below.

## 2. Sweep methods and counts

Dependency sweep (for the SBOM): Makefile read in full; every
`#include` in src/ and include/ swept and classified (C standard:
stddef, stdint, stdlib, string, threads; POSIX/system: unistd,
sys/mman, pthread; project-internal: mimir_alloc.h, mimir_core.h,
mimir_platform.h - no third-party header exists); readelf -d and
ldd on the built .so recorded verbatim in the recorded run
(DT_NEEDED = [libc.so.6] only); scripts/ and tests/ inventoried
for external tools (none beyond the toolchain and the Mojo-guarded
consumer targets); root tree listed and classified package vs
non-package (root media files are out-of-package).

Contact-route sweep (for SECURITY.md): the tree (README.md,
LICENSE, docs/, .github/) was grepped for published contact or
security routes and for "@". Result: NO email address is published
anywhere in the swept tree for this project; .github/ contains
only workflows/. SECURITY.md therefore prints no address at all
and routes reports through GitHub's private vulnerability
reporting / Security Advisories mechanism, described generically,
plus a minimal public issue asking for a private channel if that
route is unavailable. Any repository path printed was quoted from
`git remote -v` output in the recorded run. Inventing a contact
address would have been a critical defect of this slice; none was
invented, created, or registered, and no repository setting was
changed (if private vulnerability reporting needs enabling, that
is an owner action, flagged here and in SECURITY.md's wording).

Limitations sweep (for the register): every phase exit packet and
the M-001 carried-owed register were read end to end; docs/ was
grepped for the INC- pattern and for OWED / UNMET / PARTIAL verdict
lines. Counts: documents in docs/ listed = 116; distinct INC
identifiers found by the docs-wide grep = 534 (that figure counts
every historical INC ever named in docs/, most of them discharged
inside their own waves). Register worksheet outcome: candidates
carried into the register = 44 entries (Part i: 9 capability
limitations; Part ii: 12 unmet exits / owed runs; Part iii: 20
open-defect entries covering every still-open INC the sweep found
standing in the exit packets, the M-001 carried register, and the
M-002/M-003/M-004 records, deduplicated by INC identity with full
citation chains; Part iv: 3 known non-defects). Excluded with
reason: every other distinct INC identifier is either (a)
discharged or closed inside its own banked packet with the
discharge recorded (the register cites the discharge where a later
packet superseded an earlier one, e.g. the Phase 9 shim
discharging INC-I001-01), (b) a carried-forward restatement of an
INC already entered once under its canonical identity, or (c) a
slice-local finding whose own slice fixed it in-slice. After
drafting, docs/ was re-grepped for INC- and every still-open INC
in the banked record was confirmed present in the register or in
this excluded-with-reason accounting; the re-grep count lines are
in the recorded run.

Platform extraction: from the Section 2 sources only; each row was
drafted with its evidence citation before prose. Page-size rows
(16 KiB verified on this Pi class; 4 KiB owed) are separate cells,
never merged. The AArch64 Linux/glibc row reconciles the stale
A-003 text against the banked 4590/0 suite record and says so.

## 3. Format decisions and reasoning

SBOM location: the JSON SBOM is committed at docs/sbom.json,
beside its human rendering docs/SBOM.md, so the pair is discovered
together and no new root file competes with SECURITY.md's
GitHub-recognised root location. SPDX-lite shape: document name,
packages with name, versionInfo or the exact phrase
"NOASSERTION - unversioned at base ..." for the project (the
assignment's convention: NOASSERTION where a value was not
verified in this slice), supplier, licenseConcluded for the
project (MIT, as in-tree) and NOASSERTION where a component
licence was not verified in this slice, and the dependency
relationships the evidence supports (the library DEPENDS_ON the C
runtime only; build tools as build dependencies; the Mojo
environment as a test dependency with
LicenseRef-Modular-Proprietary). Validation: python3 -m json.tool
on the Pi; output recorded in the recorded run. An invalid JSON
would have failed the gates; it parsed clean.

Consistency pass: all four products were checked against each
other and the base tree for version status (unversioned at this
base, everywhere), target grades (register owed items match the
platform document's OWED/BLOCKED rows), owed items, and licence
(MIT). No post-draft banked change was chased: the base is the
base; later banks are M-009 reconciliation via INC-M008-01/02.

## 4. INC-M008 register (raised by this slice)

- INC-M008-01 (reconciliation, owner M-009): if the keeper banks
  M-006 (target-matrix refresh) before M-008, reconcile
  docs/SUPPORTED_PLATFORMS.md (built on the A-003-era matrix plus
  banked evidence, per the dependency rule) against the refreshed
  matrix. Raised for exactly that purpose, as assigned.
- INC-M008-02 (reconciliation, owner M-009): tie SECURITY.md and
  the SBOM's generic release references to the banked M-007
  outputs (version scheme, SONAME/install decision, artifact
  names and checksum file) once M-007 banks; at this base those
  items are OPEN and recorded open in the register (Part iii,
  D-07) with M-007 as owner.
- INC-M008-03 (source defect found by this sweep): the A-003-era
  docs/SUPPORTED_TARGETS.md headline ("No target is supported yet
  ... no MimirAlloc code or test exists") is false against the
  banked tree at this base and has been since Phase 3 banked. It
  is already carried as INC-I009-01 with M-006 as owner; this item
  records that the M-008 sweep relied on that file only under
  correction, so M-009 grades the register knowing the source
  itself misstates a fact.
- INC-M008-04 (owner action, not a slice action): GitHub private
  vulnerability reporting may need enabling in repository
  settings by the owner. This slice changed no repository
  settings, as assigned; SECURITY.md describes the channel as one
  the owner may enable.

## 5. Completeness self-assessment (against the Section 6 method)

Baseline gate: make test exit 0, CHECKS 4590 run / 0 failed;
nm -D --defined-only exactly 4 - recorded. SBOM validity gate:
json.tool parses clean; every SBOM component traces to a sweep
output in the recorded run; the third-party runtime count stated
(NONE beyond libc) equals what readelf/ldd show (one DT_NEEDED).
Contact gate: grep of SECURITY.md for "@" returns no address at
all, matching the sweep finding of no published address; the gate
records that explicitly in the recorded run. Completeness gate:
sweep counts stated above; post-draft INC- re-grep performed and
recorded. Consistency gate: held as described in Section 3.
Purity gates: README diff vs base = 0; git show --stat lists only
the six Section 7 files; all deliverables are NEW files. Health
gate: Samtal HTTP 200 at the end; free -g in the 13G-available
class; nothing installed; no process left running.

## 6. Plain-language verdict for Volmarr

A reader who opens this repository now can learn, without reading
twelve phases: exactly what the allocator is made of (its own
seven source files, the C library, and nothing else at runtime -
with the proof commands recorded); how to report a security
problem honestly (privately, through GitHub, with no fake email
address and no promised response time, because this is a
one-person research project); where it actually runs today (one
platform, verified hard: AArch64 Linux with glibc on 16 KiB-page
Pi-class hardware - everything else is owed, blocked, or not a
target, said in those words); and everything that is still wrong,
owed, or undecided, in one register - including that it loses to
glibc on every measured workload by about 5x to 65x, that the
x86-64 and Android runs have never happened, that the Qwen3
comparison was never run, and that the library still has no
version, no SONAME, and no install story. What is still not public
or not settled after this slice: the M-006 matrix refresh and the
M-007 versioning/artifact decisions (both in flight, both flagged
for M-009 to tie in), the Phase 13 exit grade itself (M-009's job,
not this slice's), and the owner action of enabling GitHub private
vulnerability reporting if it is not already on.

