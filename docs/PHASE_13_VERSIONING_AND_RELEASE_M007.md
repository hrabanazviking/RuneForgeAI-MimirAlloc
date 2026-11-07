# Phase 13 — Versioning, SONAME/install decision, checksum-verified
# artifacts and reproducible build (slice M-007)

Campaign: MimirAlloc (clean-room ARM64 memory allocator, research
phase, evidence before claims). Wave 13, Phase 13 (security and
production release). Base (parent) commit:
6b312e3197cdb2e491ab0ffa24a4ba8ab95a1b17. Branch yrsa/mimiralloc-07.
All evidence in this record was produced on the Raspberry Pi Mimir
(purpleviking1) and is reproduced verbatim in
tests/abi/recorded_run_m007.txt.

## Plain-language verdict (for Volmarr)

This slice gives MimirAlloc its first honest release identity:
version **0.1.0**, a research release. The shared library now has a
real SONAME (`libmimiralloc.so.0`), a versioned filename
(`libmimiralloc.so.0.1.0`) with the usual symlink chain, and an
`install` target a downstream packager can actually use — proven by
installing into a throwaway staging root on the Pi and running a
consumer program against that staged install. Four release artifacts
were cut on the Pi, hashed with SHA-256 (hashes committed in
`release/SHA256SUMS`), verified from a clean extraction, and built
twice from a clean tree with byte-identical results. What a consumer
can now do that they could not before: link against an installed,
version-identified library whose `DT_NEEDED` names a stable SONAME,
and verify any re-cut artifact byte-for-byte against the committed
checksums. What remains deliberately unclaimed: this is not 1.0, not
production-ready, and not ABI-stable — the public surface is still
Proposed (contracts.md, U-03 unresolved), the allocator still loses
its measured Phase 11 benchmarks to glibc, and exits on x86-64,
Android and the Qwen3 comparison remain unmet. Nothing is signed (no
signing-key infrastructure exists; see D4) and nothing was published
or tagged — cutting an actual public release is Volmarr's act.

## 1. Owed-item register (assignment Section 2) with dispositions

- INC-E002-01 class (Phase 5, E-002; docs/SHARED_LIBRARY_BUILD.md
  lines 136-145): no SONAME, no versioned filename, no install
  target; consumers find the library only through an `$ORIGIN` rpath
  into the build tree. Re-verified live at this base by the lead:
  `grep -c SONAME Makefile` = 0, no `^install` rule, `readelf -d` on
  the freshly built `.so` shows no SONAME entry.
  Disposition: **DECIDED-IMPLEMENTED** (D3a).
- INC-E004-01..04 class (Phase 5, E-004;
  docs/PHASE_5_BINDING_PROOF_E004.md lines 158-165): the binding
  proofs record the same absence — consumers link the unversioned
  `.so` by path/rpath; nothing defines what a downstream links
  against after an install.
  Disposition: **DECIDED-IMPLEMENTED** (D3a; consumer gate re-proven
  against the staged install and against the extracted artifacts).
- INC-M001-04 (Phase 13, M-001; docs/PHASE_13_FOUNDATION_PLAN.md
  Section 5): "M-007 cannot publish versioned artifacts honestly
  until the Phase-5-owed SONAME/install decision is recorded."
  Disposition: **DECIDED-IMPLEMENTED** — this record is that
  recording, made before any artifact was cut (method step 5).
- INC-M002-05 (Phase 13, M-002 ABI audit Section 6): no SONAME, no
  versioned filename, no install target, anonymous version node,
  ABI version unassigned; consistent with the Proposed surface in
  docs/contracts.md (A-008).
  Disposition: **DECIDED-IMPLEMENTED** for SONAME, versioned
  filename and install target (D3a); **DECIDED** for the version
  node and ABI version — the anonymous node is retained by decision
  and the ABI version is assigned as the SONAME epoch (D2). The
  contracts.md surface itself stays Proposed; this slice assigns a
  release version, not an ABI-stability promotion (that remains
  gated by contracts.md Section 5.5, which no slice has met).
- A-009 / D-006 line: the public surface is a C ABI of exactly 4
  functions (mimir_malloc, mimir_free, mimir_calloc, mimir_realloc).
  Every decision below versions exactly that surface. E-006 aligned
  entry points remain PROPOSED only and E-005 audit counters remain
  internal/test-only; neither enters the versioned public surface.
  Disposition: **DECIDED** (respected as a constraint in D1/D2).
- Roadmap Phase 13 requirement ("Publish signed or
  checksum-verified artifacts, versioning, SBOM,
  reproducible-build instructions, documented supported platforms
  and a security-reporting channel"): M-007 owns versioning,
  checksum-verified artifacts and reproducible-build instructions
  (**DECIDED-IMPLEMENTED**, D1/D4/D5). SBOM, supported platforms
  and the security channel are M-008 — **NOT-THIS-SLICE**. Signed
  artifacts: **DECIDED-DEFERRED** by absence of key infrastructure
  (D4), owner Volmarr, trigger: Volmarr establishes a release
  signing identity.
- LICENSE: already in tree at base; not reopened.
  **NOT-THIS-SLICE** (no licence decision taken).

## 2. Decisions D1–D5 (recorded before implementation)

### D1 — Version scheme and first version: SemVer, first version 0.1.0

Scheme: semantic versioning with the major number held at 0 for as
long as the public surface is Proposed. While major is 0: the minor
number increments for any compatible change to surface or behaviour
(a symbol addition, or a behaviour change a conforming caller could
observe); the patch number increments for fixes that change neither
surface nor promised behaviour. The major number moves to 1 only
when a future slice records the contracts.md Section 5.5 promotion
(surface no longer Proposed) together with the then-owed exit
evidence — no slice may reach 1.0 by accumulation of minors.

First version assigned by that rule to this tree: **0.1.0**.
Argument: (a) not 1.0.0 — the surface is Proposed, U-03 is
unresolved, the ABI carries no stability promise, Phase 11 measured
the allocator behind glibc, and x86-64 / Android / Qwen3 exits are
unmet; 1.0 would claim exactly what the evidence denies. (b) Not
0.0.x — 0.0 is the shape for a skeleton or a stub; this tree is a
complete, tested implementation (make test: 4590 checks, 0 failed,
on this Pi) with thirteen phases of banked evidence behind it. (c)
Minor 1, not a phase-count number such as 0.13.0 — the minor number
counts release-surface milestones under this scheme, and this is
the first versioned release; borrowing the phase number would
encode bookkeeping, not surface state, and would inflate the claim.

Representation in-tree: a single machine-readable `VERSION` file at
the repository root containing exactly `0.1.0`. The build reads it
for the versioned filename (D3). There is deliberately NO version
macro in the public header and NO version symbol in the library:
the header is the banked public contract surface (its four
declarations are the whole ABI), adding a macro would grow
unversioned public surface outside any contract row, and a version
symbol would break the exactly-4 dynamic-export invariant that
E-002 established and M-002 audited. The version identifies the
release and the library file; it does not ride inside the ABI.

What 0.1.0 does NOT claim: production readiness, ABI stability
beyond the D2 rule, performance competitiveness (Phase 11 losses
stand), or platform breadth beyond the M-006 matrix grades.

### D2 — ABI version rule: anonymous node retained; ABI version = SONAME epoch

The version script `src/api/mimir_alloc.map` keeps its ANONYMOUS
node, by decision, unchanged by this slice. A named node (e.g.
MIMIRALLOC_0.1) would add an ABS version-name symbol to `.dynsym`,
so the defined dynamic symbols would no longer be exactly the four
public functions, and it would assert a symbol-version promise the
header and contracts.md expressly withhold. A named node belongs to
the future promotion slice, not to a research release.

The ABI version is the SONAME integer (the ABI epoch), assigned
here as **0**, carried in the SONAME `libmimiralloc.so.0` (D3).
Relation to the project version, while the surface is Proposed:
- Compatible symbol addition (only via a future recorded decision;
  E-006/E-005 material stays out): project minor increments; ABI
  epoch unchanged; consumers' `DT_NEEDED` keeps resolving.
- Symbol removal or signature change: ABI epoch increments by 1
  (SONAME changes, so old consumers fail loudly at load instead of
  binding silently to a different ABI) AND project minor increments
  with the break recorded; while major is 0 there is no minor-number
  shelter for a silent break.
- Patch-level fix: neither number moves except project patch.
At the future 1.0 promotion the epoch rule is re-decided by that
slice; nothing here pre-commits it.

### D3 — SONAME and install: (a) IMPLEMENT

Chosen: option (a), implement. Argument against deferral: the
E-002/E-004 evidence shows path/rpath linking already works, so a
SONAME is not needed for this project's own tests — but the owed
item was never about the tests; it is about downstream identity. A
research release that ships a shared library with no SONAME forces
every consumer's `DT_NEEDED` to record whatever filename happened
to be at link time, which is precisely the ambiguity INC-E002-01
recorded. The cost of implementing is small and additive (the
banked unversioned `lib` target is untouched), and the research
status is protected by the epoch number itself: SONAME
`libmimiralloc.so.0` says "pre-stability ABI, epoch 0" in the
filename, which is the honest signal.

Implemented form:
- Real file: `libmimiralloc.so.0.1.0` (project version from
  `VERSION`), linked with `-Wl,-soname,libmimiralloc.so.0`.
- Symlink chain, produced in the build directory and by install:
  `libmimiralloc.so.0` -> `libmimiralloc.so.0.1.0`;
  `libmimiralloc.so` (development symlink) -> `libmimiralloc.so.0`.
  The banked `build/libmimiralloc.so` file target is NOT replaced:
  a new `lib-versioned` target builds alongside it from the same
  PIC objects, so every banked test route is byte-unaffected.
- Install target: `make install` honours `PREFIX` (default
  `/usr/local`) and `DESTDIR` (default empty); it installs the real
  file and the header into `$(DESTDIR)$(PREFIX)/lib` and
  `$(DESTDIR)$(PREFIX)/include`, creates the two symlinks, and runs
  `ldconfig` only when DESTDIR is empty (a staged install never
  touches the host linker cache). Proven by a staged install under
  /tmp plus a consumer compile+link+run against it (Section 4).

### D4 — Artifact set, checksums, signing

Release artifacts (staging names are the release names):
1. `libmimiralloc.so.0.1.0` — the versioned shared library (D3).
2. `libmimiralloc-0.1.0.a` — the static archive from the banked
   `static-lib` build, staged under a version-bearing name.
3. `mimir_alloc.h` — the public header, copied unchanged.
4. `mimiralloc-0.1.0.tar.gz` — source tarball of the release tree
   state (tracked files plus this slice's new files at cutting
   time; build products, .git and hidden_files excluded), generated
   with the normalised procedure of D5.
   The file list excludes release/SHA256SUMS and
   tests/abi/recorded_run_m007.txt: both are generated FROM the
   artifacts (they carry the artifact hashes), so including them
   would be circular, and would make a re-cut from the committed
   tree differ. See docs/REPRODUCIBLE_BUILD.md step 4.
The symlink names of D3 are part of the installed layout, not
checksum artifacts (a symlink has no content to hash); they are
listed in the recorded run instead.

Checksum algorithm: SHA-256. The checksum file is committed in-tree
at `release/SHA256SUMS` (one `sha256sum`-format line per artifact),
byte-identical to the staging copy verified in Section 5 step 5 of
the assignment procedure. Consumer verification: copy the artifacts
and SHA256SUMS into one directory and run `sha256sum -c SHA256SUMS`.

Signing: DEFERRED-BY-ABSENCE. The project has no signing-key
infrastructure on record; this slice generated no key on the Pi
(a release signing identity is Volmarr's decision, not a slice
improvisation) and no deliverable claims or implies a signature.
Checksum verification is the integrity mechanism for this release.
Owner: Volmarr. Trigger: Volmarr establishes a release signing
identity, after which a future slice adds signatures alongside the
checksums.

### D5 — Reproducibility stance

Byte-identical means: same sha256 for the artifact produced by two
full clean builds of the same tree state on this Pi, following
docs/REPRODUCIBLE_BUILD.md exactly. Inside the claim: the two built
binary artifacts (`libmimiralloc.so.0.1.0`,
`libmimiralloc-0.1.0.a`). The header is a verbatim copy, listed in
SHA256SUMS but not a build claim. The source tarball is inside the
claim ONLY because its generation is normalised (fixed file order
via --sort=name, owner/group 0, fixed mtime from SOURCE_DATE_EPOCH
= the base commit timestamp, gzip -n) — and it stays inside the
claim only for as long as the two-build comparison in this slice
shows it identical; the recorded comparison is the evidence.
Pinned inputs: compiler `cc (Debian 12.2.0-14+deb12u1) 12.2.0` on
aarch64 Debian 12, the Makefile's banked flags unchanged, LC_ALL=C
and TZ=UTC for the tarball step, SOURCE_DATE_EPOCH as above. The
claim is exactly what the two-build comparison proves — nothing
wider, and no cross-machine claim is made.

## 3. Implementation (every hunk; no .c source touched)

Files changed by this slice, and only these:
- `VERSION` (new): the single line `0.1.0`.
- `Makefile`: ONE appended block at end of file (M-007 block),
  purely additive — no banked line edited or removed, so the
  keeper's union against the M-004-banked Makefile is a pure append
  reconciliation. The block contains: (i) variables MIMIR_VERSION
  (read from VERSION), ABI_EPOCH=0, the versioned library/soname
  names, PREFIX ?= /usr/local; (ii) the versioned library rule
  linking the existing PICOBJS with the SONAME flag into
  build/libmimiralloc.so.0.1.0 plus its two symlink rules and the
  `lib-versioned` phony target; (iii) the `install` phony target
  per D3; (iv) a `print-version` helper used by the release
  procedure. The existing `lib`, `static-lib` and `test` targets
  are untouched, and `make test` composition is unchanged.
- `src/api/mimir_alloc.map`: NOT modified (D2 retains the anonymous
  node by decision; the file's own comment already records the
  reasoning, which D2 adopts).

No allocator or test .c source was modified; implementation needed
none. README.md is byte-identical to the base (diff 0 lines).

## 4. Verification and reproducibility results (summary)

Full verbatim evidence: tests/abi/recorded_run_m007.txt.
- Regression: on the versioned tree, `make test` exit 0 —
  CHECKS: 4590 run, 0 failed; `nm -D --defined-only` on the
  versioned library: exactly 4 (mimir_malloc, mimir_free,
  mimir_calloc, mimir_realloc).
- SONAME: `readelf -d build/libmimiralloc.so.0.1.0` shows
  `Library soname: [libmimiralloc.so.0]`.
- Consumer gate (i), staged install: `make install
  DESTDIR=/tmp/m007-stage-install PREFIX=/usr/local` produced the
  D3 tree (real file, both symlinks, header); the banked E-004
  consumer (tests/abi/test_e004.c) compiled against the staged
  include/lib, linked `-lmimiralloc`, ran with exit 0; its
  DT_NEEDED names `libmimiralloc.so.0`.
- Consumer gate (ii), extracted artifacts: in a fresh directory,
  `sha256sum -c SHA256SUMS` reported OK for all 4 artifacts; the
  same consumer linked against the extracted versioned library ran
  with exit 0.
- Reproducibility: second full clean build, same procedure —
  all artifacts inside the D5 claim byte-identical to the first
  build (per-artifact hashes and the comparison table are in the
  recorded run). No artifact differed; no claim was withdrawn.
- Checksum gate: the committed `release/SHA256SUMS` is
  byte-identical (sha256 compared) to the staging copy that passed
  `sha256sum -c`.

## 5. INC-M007 register

- INC-M007-01 (instructions defect, closed in-slice): the first
  draft of docs/REPRODUCIBLE_BUILD.md generated the tarball file
  list with a plain git ls-files. Once this slice is committed,
  that list would also name release/SHA256SUMS and
  tests/abi/recorded_run_m007.txt, so a re-cut from the committed
  tree would produce a different tarball and fail the committed
  checksums. Found by the finisher lead before commit; fixed by
  explicit exclusions in the instructions (step 4) and a matching
  clarification in D4, after which both release builds were
  re-run under the corrected procedure. The two draft-procedure
  builds were byte-identical to each other; they were discarded
  and are not recorded as results. The categories design error
  requiring a .c change and failed gate are explicitly EMPTY
  for this slice, and no artifact differed across the two builds
  of the final procedure.
- Carried forward (not raised by this slice, recorded so M-009
  sees them): signing remains deferred-by-absence (D4, owner
  Volmarr); the contracts.md Section 5.5 promotion gate remains
  unmet by design; M-008 owns SBOM / supported platforms /
  SECURITY.md against these artifacts.

## 6. Custody

No binary, archive or tarball is committed. The artifacts live in
the staging custody named in the recorded run (ephemeral /tmp on
the Pi); durability is re-cutting from the committed tree by
docs/REPRODUCIBLE_BUILD.md, which reproduces the committed
checksums byte-for-byte as proven above.
