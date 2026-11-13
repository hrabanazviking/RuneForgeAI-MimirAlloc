# Z-105 - Release engineering (Wave 12)

Slice: Z-105 (team-05, SOLO-SEQUENTIAL lead - the lead ran the
six Mythic Engineering roles itself in sequence: release
archaeology, version design, SBOM sweep, reproducibility
measurement, gate verification, ledger + commit scope; no
sub-agents spawned, recorded plainly per campaign law).
Campaign: MimirAlloc Z13 (purpleraider, ASUS ROG Flow Z13,
x86-64, 4 KiB pages). Roadmap: docs/MIMIRALLOC_Z13_ROADMAP.md
slice Z-105 - "Version-bump decision packet, SBOM refresh,
reproducible-build proof on this host (two clean builds,
identical hashes). Gate: packet + reproducibility proof
in-tree."
Base: merge 080e254 of seat main c3424b2 (banked Z-001..Z-101)
into yrsa/mimiralloc-team-05; merged tree verified
byte-identical to the seat (git diff empty) before slice work;
this slice BUILDS AHEAD and banks only after Z-102, Z-103 and
Z-104 in roadmap order. Those three slices change no public
surface (chaos + sanitizer matrix), so nothing in this packet
is OWED to them; if any of them surprises with a surface
change, this packet's S1 table is re-checked at banking.
Evidence: tests/abi/recorded_run_z13_z105.txt (runner
tests/abi/run_z105.sh, standalone via `make test-z105`).
Decision recorded as D-017 in docs/DECISION_LEDGER.md.
README.md untouched by this slice.

## Plain-language verdict (for Volmarr)

The library you can ship today is still honestly version
0.1.0, and this slice proves the Z13 tree can be rebuilt
byte-for-byte on your Z13: two full clean builds produced
identical hashes for every release artifact. But 0.1.0 was
cut when the library exported 4 functions; it now exports
13, so the packet decides the next release cut is 0.2.0 -
decided here, executed at the cut itself (a public release
cut is your act, and the closing table Z-107 should land
first). The SONAME epoch stays 0 and nothing claims 1.0:
the surface is still Proposed and the allocator still loses
most measured workloads to glibc. The SBOM is refreshed to
the Z13 truth: still no third-party runtime dependency
beyond the C library. One real gap was found and filed, not
hidden: the opt-in interposing library still carries the
old 10-name namespaced set, not the current 13 (F-Z105-1).

## S1 - Version-bump decision packet

### S1.1 The rule being applied (M-007 D1/D2, banked)

- Scheme: SemVer, major held at 0 while the public surface
  is Proposed (contracts.md, U-03 unresolved). Minor
  increments for any compatible change to surface or
  behaviour (a symbol addition, or a behaviour change a
  conforming caller could observe); patch for fixes that
  change neither. Major moves to 1 only with the
  contracts.md Section 5.5 promotion, recorded by a future
  slice - never by accumulation of minors.
- ABI version = SONAME epoch (M-007 D2): compatible symbol
  addition leaves the epoch unchanged; symbol removal or
  signature change increments it. The version script's
  ANONYMOUS node is retained for the same reasons.
### S1.2 Surface history since the 0.1.0 cut (all banked, all measured)

| Step | Slice | Change | Default exports |
|---|---|---|---|
| 0.1.0 cut | M-007 (Pi line) | mimir_malloc/free/calloc/realloc | 4 |
| +aligned pair | Z-074 | mimir_aligned_alloc, mimir_posix_memalign | 6 |
| +usable size | Z-075 (D-012) | mimir_usable_size | 7 |
| +stats | Z-076 | mimir_stats_snapshot (+ struct v1) | 8 |
| +trim pair | Z-077 | mimir_trim, mimir_reallocarray | 10 |
| interpose variant | Z-082 (D-015) | separate opt-in .so, 10 std names; default unchanged | 10 |
| +memalign family | Z-083 (D-016) | mimir_memalign, mimir_valloc, mimir_pvalloc | 13 |

Proven on this tree by the recorded run S3: default and
versioned libraries export exactly 13; the interposing
variant exactly 20 (its own Z-082 contract). NOT surface
changes, recorded so nobody bumps for them: the Z-092 batch
prototype lives in the core only (0 mimir_batch_* dynamic
exports, proven in the recorded run); the Z-096 flag
adoption (-O3) changed build flags, not surface or promised
behaviour - at most patch-level under D1, and no release was
cut for it, so it folds into the next cut's number.
Behaviour changes a conforming caller could observe since
0.1.0 (each a recorded decision, each independently a
minor-bump reason under D1): D-013 zero-size/errno contract
(Z-078), D-014 budget enforcement (Z-079).

### S1.3 Decision (D-017)

(1) The next release cut of this tree is version 0.2.0.
Nine compatible symbol additions and two contract-level
behaviour decisions since 0.1.0 make 0.1.0 a false label
for the current tree; D1's minor rule is exactly this case.
One minor step, not nine: the minor counts release-surface
milestones between cuts (M-007 D1 argument (c)), and there
has been exactly one unreleased accumulation since 0.1.0.
(2) ABI epoch UNCHANGED at 0: every addition is compatible
(no removal, no signature change), so consumers'
DT_NEEDED (libmimiralloc.so.0) keeps resolving. The
versioned filename at the cut becomes
libmimiralloc.so.0.2.0 under the same SONAME.
(3) The anonymous version node is RETAINED at the cut.
(4) 1.0 remains WITHHELD: contracts.md S5.5 promotion is
unmet, U-03 is unresolved, and the Wave-11 verdict's losses
stand. Nothing in this packet promotes the surface.
(5) Execution is at the cut, not in this slice: VERSION
stays 0.1.0 in-tree until the cutting slice (or Volmarr)
performs the cut - M-007's own precedent is that cutting a
public release is the owner's act, and Z-106/Z-107 should
land first so the cut carries the closing table. The cut
checklist: bump VERSION to 0.2.0; re-run `make test-z105`
(the runner is version-agnostic: it stages whatever
VERSION names); regenerate release/SHA256SUMS from the
cutting host's two-build proof and refresh
docs/REPRODUCIBLE_BUILD.md's pinned inputs for that host;
refresh docs/SBOM.md + docs/sbom.json version fields.
(6) Signing remains DEFERRED-BY-ABSENCE (M-007 D4 stands):
no signing identity exists; checksums are the integrity
mechanism. Owner: Volmarr; trigger: he establishes one.
What would change this decision: a symbol removal or
signature change before the cut (epoch increments to 1
and the cut becomes 0.3.0 with the break recorded, per D2);
a Section 5.5 promotion before the cut (the promoting
slice re-decides the number; this packet does not
pre-commit it).
## S2 - SBOM refresh (docs/SBOM.md + docs/sbom.json, in place)

Method (M-008's, repeated on this tree): Makefile read in
full; every #include in src/ and include/ swept and
classified; readelf -d and ldd on the libraries built in
this slice's recorded run; src/ + include/ inventoried.
Refreshed files carry a Z-105 refresh header naming this
base; the M-008 original stays in git history.

What changed since M-008 (Pi base 6b312e3):
- Project version: was "unversioned, M-007 in flight"; now
  0.1.0 (VERSION file, M-007 banked), with D-017's 0.2.0
  decision for the next cut recorded in both renderings.
- Shipped files 7 -> 9: src/api/mimir_interpose.c and
  src/api/mimir_interpose.map added by Z-082 (the opt-in
  variant's translation unit and export contract).
- Public surface: header declares the 13 namespaced entry
  points of S1.2 (was 4); default library exports exactly
  13, interposing variant exactly 20 (recorded run S3).
- Runtime story UNCHANGED in kind: sole DT_NEEDED beyond
  the loader is libc.so.6 (on this host's build readelf
  also lists ld-linux-x86-64.so.2 as a NEEDED entry - the
  host toolchain's direct loader dependency, recorded
  verbatim in the run; ldd resolves only libc + vdso).
  Third-party runtime libraries: NONE.
- Include classification grew with the core, still with no
  third-party header anywhere: C standard (stddef, stdint,
  stdlib, string, threads, errno, time, stdio), POSIX/system
  (unistd, sys/mman, pthread, sched, x86intrin on this
  arch), project-internal (mimir_alloc.h, mimir_core.h,
  mimir_platform.h). One coupling is named plainly:
  src/core/mimir_core.c includes the project-internal test
  contract header tests/contract/mimir_tls_cache_z028.h
  for its _Static_assert class-count contract (Z-029) - a
  compile-time assertion only; no test code ships.
- Build environment of record is now this host: gcc 15.3.0
  (Ubuntu 15.3.0-4ubuntu1), GNU Make 4.4.1, ld 2.47,
  glibc 2.44, flags -O3 per Z-096 (M-008 recorded gcc
  12.2 / Make 4.3 / glibc 2.36 on the Pi).
- Root-tree media files remain out-of-package (M-008's
  boundary stands, unchanged).
Consistency: the two renderings were cross-checked against
each other and against the recorded run before commit;
the JSON parses clean (python3 -m json.tool, in the run
log set for this slice).
## S3 - Reproducible-build proof on this host

Procedure: tests/abi/run_z105.sh (the Z13 successor of
docs/REPRODUCIBLE_BUILD.md's Pi procedure, same
normalisation discipline): two full clean builds
(`make clean`, then `make lib lib-versioned static-lib
lib-interpose`) of the same tree state, staged outside the
tree (/tmp/z105-relA, /tmp/z105-relB), source tarball
normalised (git file list in LC_ALL=C sort order, owner/
group 0, --numeric-owner, fixed mtime = SOURCE_DATE_EPOCH
1794577930, the seat base c3424b2's committer time,
gzip -n), then hash comparison artifact by artifact, a
staged `make install` + E-004 consumer proof against the
install, and sha256sum -c verification from a fresh
directory. The file list excludes release/SHA256SUMS and
this slice's recorded run (both carry artifact hashes -
M-007's INC-M007-01 circularity rule).

Result (recorded run, build A vs build B, this host):
ALL FIVE artifacts byte-identical - the versioned library,
the static archive, the interposing library, the header
copy, and the normalised source tarball - and the two
staged SHA256SUMS files byte-identical. Hashes are in the
recorded run S1/S2; the runner exits non-zero and names
both hashes if any artifact ever differs.
Scope (M-007 D5 restated): the claim is THIS host, THIS
compiler, THIS tree state. The committed
release/SHA256SUMS (Pi, aarch64, gcc 12.2, the 4-symbol
header, pre-Z13 flags) differ from this host's hashes for
every artifact - including the header, whose content
itself changed as the surface grew - exactly as the
same-host scope predicts. Recorded in the run S6 as
EXPECTED-DIFFER, not a failure and not a claim withdrawn:
the Pi proof stands for the Pi tree state it measured.

## S4 - Findings register

- F-Z105-1 (surface gap, filed, NOT fixed in this slice):
  the interposing variant's namespaced set is the Z-082-era
  10 - it does not carry mimir_memalign / mimir_valloc /
  mimir_pvalloc (Z-083), and its standard-name set has no
  memalign / valloc / pvalloc compositions either (proven:
  recorded run S3 nm listing, exactly 20 with the trio
  absent from both sets). Consequence: an LD_PRELOAD user
  of the interposing library gets the pre-Z-083 surface.
  Why not fixed here: the variant's export list is a
  recorded contract (D-015) whose own runner proves
  "exactly 20"; changing it is an export-contract decision
  needing its own slice and decision entry, not a release
  -engineering side effect. Owner: the next gap-closure
  slice after Z-104, or the Z-108 hand-off if none is
  commissioned; recorded as immediate next work in this
  team's CAMPAIGN_STATE.md per the never-leave-a-bug law.
- F-Z105-2 (stale release record, discharged by S1.3(5)):
  docs/REPRODUCIBLE_BUILD.md's pinned inputs and
  release/SHA256SUMS describe the Pi 0.1.0 tree state and
  cannot verify any current artifact on any host. They
  stand as that cut's honest record; the cut checklist in
  S1.3(5) regenerates both at the 0.2.0 cut. No silent
  rewrite of the Pi record was made by this slice.
- Slice-harness defect (fixed in-slice): the runner's
  first trial FAILED one check - its S3 read the
  unversioned build/libmimiralloc.so, which its own build
  step had never built (it built only the versioned,
  static and interpose targets). Fixed by adding the `lib`
  target to the build step; the trial's other 17 checks,
  including the full two-build identity, passed unchanged.
  No allocator code was involved.

## S5 - Gates

- Standing correctness gate (scripts/z13/run_gate.sh):
  baseline on the merged tree AND final on the slice tree,
  both GREEN 4988 checks / 0 failed / 77 by-design skips,
  exit 0 (team hidden_files/z105_gate_baseline.log and
  z105_gate_final.log) - identical to the Z-001 baseline;
  this slice adds no code to the gate's composition.
- Slice gate (roadmap: packet + reproducibility proof
  in-tree): MET - this packet (with D-017) plus the
  recorded run's two-build proof; `make test-z105` PASS
  (runner checks 18/18, exit 0) on the final tree.
- Load state: builds and gates ran in a light-to-moderate
  shared-host window (loadavg 0.46-2.5 on 32 threads across
  the slice, recorded per-run in the run file); no timing
  is graded anywhere in this slice - hashes and counts are
  load-independent.

## Custody

No binary, archive or tarball is committed. Staged
artifacts live in ephemeral /tmp custody (z105-relA,
z105-relB, z105-extract, z105-install); durability is
re-running `make test-z105` from the committed tree,
which reproduces the recorded hashes byte-for-byte as
proven above.
