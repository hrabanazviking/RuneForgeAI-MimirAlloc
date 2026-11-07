# E-006 - Aligned-allocation surface assessment + decision record

Phase 5 (ABI completeness and interposition), Wave 5, slice 6 of 9.
Base: ea1404895b67ebc0e51ddf2732ef1ac309914321 (E-005 banked;
Wave 5 = 5 of 9 at slice start).
Branch: yrsa/mimiralloc-06.
Worktree: /home/pi/mimiralloc-team-06 on purpleviking1
(Raspberry Pi, Cortex-A76), user pi.
Recorded run date: 2026-11-06T19:59:40-05:00.
Toolchain: Debian GNU/Linux 12 (bookworm); cc 12.2.0;
kernel 6.12.112-v8-16k-hailo+; getconf PAGESIZE = 16384;
glibc 2.36. Pi load stayed light (sequential small makes
only; free -g showed 13 GiB available; Samtal on port 8787
answered HTTP 200 before and after the runs, recorded run
S0 / S7).

Governing texts: roadmap Phase 5 (implement only the
documented symbol contract; test aligned allocations);
docs/PHASE_5_FOUNDATION_PLAN.md (E-001 - N2 no aligned
entry points, task T6 scoped to decision record + alignment
lifecycle tests, INC-E001-05); docs/contracts.md (A-008 -
Sections 2.6 / 2.7 aligned_alloc / posix_memalign candidate
rows, V-03 / V-06 / V-07 variance rows, U-01 / U-11 / U-14 /
U-15 open); docs/LIBC_ALIGNMENT_COMPATIBILITY.md (D-008 -
measured alignment facts, INC-D008-08); include/mimir_alloc.h
(the public surface); docs/PHASE_5_AUDIT_HOOK_E005.md
(E-005 - the audit counters are internal Proposed-only
state and export nothing; this slice changes no counter
and no source file).

## 0. Bottom line

- DECISION (status: DECIDED for the current surface): the
  aligned-allocation surface stays EMPTY. No aligned entry
  point is added by this slice or authorised for Wave 5.
  The public contract remains exactly the four symbols
  mimir_malloc, mimir_free, mimir_calloc, mimir_realloc,
  and libmimiralloc.so still exports exactly those four
  (nm -D count 4, recorded run S2 / S5). This is Option 1
  of Section 4, with its documentation duty discharged by
  this record: the over-alignment the current construction
  happens to deliver is stated here as a measured fact
  with explicit limits, never as a promise.
- PROPOSED (status: PROPOSED only, not decided, not
  implemented): if a real consumer need for guaranteed
  over-alignment ever appears, the recorded future route
  is ONE namespaced entry point, working name
  mimir_aligned_alloc, taken up only by a future contract
  + ABI slice that first settles U-11 / V-03 per target
  (Section 5.2). Nothing in this slice pre-decides its
  signature, validity rules, or error model, and no code,
  header line, or export anticipates it.
- Option 3 (serve alignment requests through the existing
  four symbols as a documented service) is REJECTED as a
  contract posture: what the four symbols deliver beyond
  the fundamental-alignment floor is a construction fact,
  and documenting it as a service would promise what
  INC-D008-08 forbids promising. It survives only as the
  factual description inside Option 1 (callers may
  observe over-alignment; they may rely only on the
  floor).
- Measured this slice (recorded run S3 / S5, reproduced
  from make clean): the alignment lifecycle test
  tests/abi/test_e006.c, linked only against
  libmimiralloc.so, passes 159 checks, 0 failed, exit 0,
  twice. Every one of the 49 non-null returns observed
  across the malloc ladder, the calloc paths and the
  realloc chain satisfied the contract floor
  (0 mod _Alignof(max_align_t) = 16), and every one was
  also 0 mod the 16384 page size - the second fact is a
  construction observation, not a claim (Section 3).
- make test composition is unchanged and exits 0 (twice
  in the recorded run). README.md untouched (diff vs
  base = 0 lines). One commit on yrsa/mimiralloc-06; no
  push or merge (the keeper banks in roadmap order).

Evidence file: tests/abi/recorded_run_e006.txt (247 lines,
7,904 bytes, sha256 aa0f178f) - every command with its
exit code, in sections S0-S7, including a full clean
re-run (S5) reproducing the counts exactly. This document
is the decision record; the recorded run is the evidence.

## 1. The question, the hard law, and the open items

The public contract has exactly four symbols and no
aligned-allocation entry points: there is no
mimir_aligned_alloc, no memalign analogue, and no
posix_memalign analogue. Code that needs storage aligned
more strongly than the fundamental requirement - cache-
line, SIMD-vector, page or device-DMA alignment - has no
MimirAlloc route today. This slice answers, with
evidence, what the aligned-allocation surface should be.

The hard law governing the answer: no silent ABI
expansion. Any new public symbol is a contract change.
It may be recorded here only as Proposed; it may not be
added to include/mimir_alloc.h, defined in src/, or
admitted to the libmimiralloc.so export list by this
slice. E-001 states the same law as non-goal N2, and
scopes roadmap task T6 ("test aligned allocations") to
exactly this shape - decision record plus alignment
lifecycle tests of the existing four symbols - because a
literal reading of T6 would require implementing the
very entry points whose existence is the open question
(INC-E001-05).

Open items this question touches, none of which this
slice closes:

- U-01 - the final required symbol set is an open
  project decision. A-008's aligned_alloc and
  posix_memalign are candidates for the STANDARD-NAME
  surface; the implemented surface is the namespaced
  one, and the two must not be conflated (INC-E006-01).
- U-03 - the namespaced mimir_ API has no direct
  contract rows; its mapping to A-008 is by analogy
  only. Any future aligned entry point inherits this
  until a contract slice settles it.
- U-11 / V-03 - alignment validity (which alignment
  values are valid per target) and any size-multiple
  rule are UNRESOLVED in contracts.md Sections 2.6 / 2.7
  (rows MA-6.1-ALIGNED-03 / -04, MA-6.1-POSIXMA-03).
  There is nothing to attach validity rules to while no
  entry point exists; Section 5.2 records them as the
  first debt of any future implementing slice.
- U-14 / V-06 - the posix_memalign-style error numbers
  are UNRESOLVED; relevant only if a future slice chose
  that error model (this record does not).
- U-15 / V-07 - no numeric default alignment value is
  claimed anywhere. The header's claim is the
  non-numeric "suitably aligned" only; 16 is a measured
  target fact (D-008), not a promoted number, and this
  decision keeps it that way.

## 2. What exists today (three layers, kept separate)

Layer 1 - the implemented surface (PROVEN at this base).
include/mimir_alloc.h declares exactly mimir_malloc,
mimir_free, mimir_calloc, mimir_realloc, and says in its
own scope note that no explicit-alignment entry point
exists and that absent symbols are the documented
unsupported behaviour (absence at link time, never a
runtime refusal, never a silent fallback - B-008 S6.1).
The version script src/api/mimir_alloc.map exports
exactly the same four names from libmimiralloc.so.
Recorded run S4 re-proves the absence at this base: no
align-named symbol is defined in the .so (grep exit 1),
no aligned name is declared or defined anywhere in src/
or the public header, and the E-006 consumer's undefined
mimir_ references are exactly the four.

Layer 2 - the A-008 specification (PROPOSED, different
surface). contracts.md Sections 2.6 / 2.7 state contract
rows for aligned_alloc (MA-6.1-ALIGNED-01 / -02 NORM
success and failure rows) and posix_memalign
(MA-6.1-POSIXMA-01 / -02), with the validity, size-
multiple and zero-size rows expressly UNRESOLVED
(U-11, U-09). These rows specify the standard-name
candidates of Roadmap Section 6.1; the document's own
control says specification only, no implementation
exists or is claimed, no ABI version assigned, and
Appendix A lists both symbols with stability Unassigned.
They are evidence that the project has THOUGHT about
aligned allocation, not evidence that any part of it is
owed to the namespaced surface in Wave 5.

Layer 3 - the measured behaviour (PROVEN, D-008 +
Section 3). On the Pi-native glibc target, every pointer
the four symbols return is aligned to 16 =
_Alignof(max_align_t), and in fact page-aligned, because
the Phase 3 construction returns mapping_base +
usable_offset with usable_offset exactly one system page
(B-006 S7.1) over an OS mapping whose base is page-
aligned. D-008 measured this over a 33-size ladder plus
the calloc / realloc paths (163 checks); this slice
re-measures it as a lifecycle through the linked library
and adds the over-alignment demand ladder (Section 3).

## 3. What the current implementation delivers (measured)

Program: tests/abi/test_e006.c (365 lines, 13,210 bytes,
sha256 f80dced3), built by the additive make test-e006
target (Makefile +26 lines, 0 deletions) and linked only
against build/libmimiralloc.so in the E-004 route.
159 checks, 0 failed, exit 0, in recorded run S3 and
again from make clean in S5 with identical counts.

| Phase | What was exercised | Result |
| --- | --- | --- |
| ENV | sysconf page size, _Alignof(max_align_t), pointer width | page 16384, alignof 16, 64-bit (3 checks) |
| A | mimir_malloc ladder, 24 sizes (1 .. 1,048,576; odd, boundary, page-crossing), all live at once, filled + verified, pairwise distinct | 96 checks PASS |
| B | over-alignment demand via plain mimir_malloc(4096): moduli 32, 64, 128, 256, 1024, 4096, 16384 | 14 checks PASS (construction checks, this host) |
| B-obs | demand 32768 (2x page) | OBSERVED only: mod read 16384 in the recorded run, 0 in the pre-run smoke - placement varies, not delivered (INC-E006-03) |
| C | mimir_calloc x6 ((1,1), (3,7), (16,32), (100,100), (1,65537), (7,9999)): non-null, floor, full extent zeroed | 18 checks PASS |
| D | mimir_realloc chain: 37 -> 1000 -> 70000 -> 13 -> 13 (same size), realloc(NULL, 41), grow 41 -> 20000; floor + prefix preservation at every step | 19 checks PASS |
| E | interleave: free one block, allocate another; survivor byte-intact; free(NULL) no-op | 5 checks PASS |
| F | defined failures: mimir_malloc(SIZE_MAX) and overflow mimir_calloc return exactly NULL; a normal mimir_malloc(48) then succeeds at the floor | 4 checks PASS |

The three-way distinction this slice exists to keep
sharp, stated against those numbers:

- PROMISED (contract, by analogy under U-03): a
  successful return from any of the four symbols is
  suitably aligned for any fundamental-alignment type.
  Operationally on this target: 0 mod 16. All 49
  observed returns satisfied it, through allocation,
  calloc zeroing, realloc growth (which moves),
  shrink / same-size (which retain), and realloc(NULL,
  n). A caller may rely on exactly this, and on nothing
  stronger.
- DELIVERED BY CONSTRUCTION (fact, not a promise): all
  49 returns were also 0 mod 16384, the runtime page
  size, and Phase B shows the construction therefore
  satisfies every over-alignment demand up to and
  including the page size on this host - cache-line
  (64), SIMD (32 / 64), and page (16384) demands alike -
  without any aligned entry point. The mechanism is
  visible in src/core/mimir_core.c: alloc_locked
  returns mapping_base + one page, and the OS mapping
  base is page-aligned. The same construction on a
  4096-page host delivers a 4096 modulus, and the
  Phase 7 redesign (size classes, splitting, caching)
  may lawfully deliver only the 16 floor. That is
  INC-D008-08, re-verified here, not a new claim.
- NOT DELIVERED: (a) any alignment ABOVE the runtime
  page size - the 32768 observation varied between
  runs (0 in the smoke run, 16384 in the recorded run),
  which is kernel placement, not construction; (b)
  alignment as a REQUEST - no caller can state a
  requirement and receive either satisfaction or a
  defined refusal; a caller needing more than the
  floor must check the returned modulus itself and
  cope (leak-free coping is its own problem: an
  over-aligned interior pointer cannot be handed to
  mimir_free, which takes exact live pointers only);
  (c) any aligned guarantee at all on any other target
  - musl, bionic, x86-64 and 4096-page hosts are
  unmeasured for this question (INC-E006-05).

Interaction with E-005: the audit counters are untouched
by this slice and irrelevant to it - alignment is a
property of returned pointers, observable from outside
the library, so no counter evidence was needed and none
is cited. The counters remain internal Proposed-only
state exactly as E-005 left them.

## 4. Options weighed

Option 1 - No aligned entry points; document the
over-alignment fact with its limits. Cost: a caller with
a hard over-alignment requirement has no guaranteed
route and must either accept the floor, check moduli
itself, or use another allocator for those blocks
(mixed-allocator ownership is then the caller's burden,
and E-008's rejection of ambiguous ownership applies
inside MimirAlloc). Benefit: zero ABI growth while U-01,
U-11 and U-15 are all open; the surface that E-002 -
E-005 just spent four slices closing to exactly four
symbols is not reopened one slice later; the documented
fact (construction delivers up to page alignment on
this host) covers every realistic present need - no
consumer in the campaign (tests, the Phase 6 Mojo road,
the Aesir road) has recorded a requirement above the
floor.

Option 2 - Record a Proposed aligned entry point for a
future ABI slice. Shape sketched, NOT specified: one
namespaced function in the mimir_ family (working name
mimir_aligned_alloc) taking an alignment and a size,
returning storage aligned as requested and releasable
by mimir_free, with the null-return failure model of
the existing family (not the posix_memalign return-code
model, whose numbers are U-14 unresolved). Benefit: the
future road is named, so if the need arrives the work
starts from a recorded direction instead of a fresh
argument. Cost and risk: even as Proposed it is a
magnet - later slices may treat the sketch as a
commitment. Mitigation: Section 5.2 fences it with
explicit non-decisions (signature, validity, size-
multiple rule, error numbers all undecided) and gates
it behind the reopen triggers of Section 6.

Option 3 - Serve alignment requests through the
existing four symbols with documented limits. In
substance: document that mimir_malloc requests "with
alignment up to the page size" are served, with the
limit stated. This is Option 1's fact wearing a
service's clothing. The limit IS the construction fact,
so the documentation would either promise the page
modulus (forbidden by INC-D008-08 and U-15: no numeric
claim) or promise nothing beyond the floor, in which
case it is Option 1 with extra words and a misleading
title. REJECTED as a posture; its true content (the
measured fact) is carried by Option 1's documentation.

Considered and rejected without an option number:
standard-name aligned_alloc / posix_memalign on the
IMPLEMENTED surface. E-001 N1 forbids standard-name
symbols in Phase 5; the A-008 rows for them belong to
the unsettled U-01 standard-name question; and
posix_memalign's error model (V-06 / U-14) is
unresolved. If U-01 ever settles in favour of a
standard-name surface, those rows are its starting
point - a different campaign decision, not this one.

## 5. Decision

### 5.1 DECIDED - the surface stays at four symbols

Status: DECIDED (for Wave 5 and the current, pre-freeze
ABI; reversible only through Section 6's triggers and a
future contract slice, never by drift).

1. No aligned-allocation entry point is added, in any
   form: no namespaced variant, no standard name, no
   conditional export. include/mimir_alloc.h and
   src/api/mimir_alloc.map are unchanged by this slice,
   and libmimiralloc.so exports exactly the four public
   symbols (recorded run S2 / S5).
2. The documented position for callers is: rely on the
   fundamental-alignment floor (suitably aligned;
   measured 16 on this target); observe, but do not
   rely on, the construction's page alignment; treat
   any requirement above the floor as unserved by
   MimirAlloc today.
3. U-11 / V-03, U-01 and U-15 remain OPEN, deliberately:
   validity rules for a parameter no entry point accepts
   would be fiction, and this slice does not write
   fiction into the contract register. Their openness is
   now a recorded decision state, not an oversight.

### 5.2 PROPOSED - the future route, fenced

Status: PROPOSED only. Recorded so the direction exists;
not a decision to build, not a signature, not a schedule.

- One namespaced aligned entry point (working name
  mimir_aligned_alloc), in the family failure model
  (null on failure), releasable by mimir_free.
- Before any implementation, a contract slice must
  settle, per target: alignment validity (V-03 -
  e.g. powers of two only? a floor of sizeof(void*)?),
  any size-multiple rule (V-03), the zero-size
  interaction (U-09), and the by-analogy vs direct-
  contract question for the new name (U-03). If the
  ABI-freeze / versioning work (INC-E002-01's owed
  SONAME + install story; roadmap Phase 13) has not
  landed first, that slice must also state how a fifth
  symbol interacts with versioning.
- The implementing slice owes the full re-proof set by
  analogy to E-002 / E-005: header + map change as an
  explicit contract change, nm -D re-count, lifecycle
  tests for the new entry point including invalid-
  alignment refusals, and an E-009-style packet update.

## 6. Reopen triggers

This decision is re-opened, and Section 5.2's proposal
may be taken up, when ANY of the following is evidenced:

- R-1: a real consumer (the Phase 6 Mojo integration, an
  Aesir workload, or a target tool) records a requirement
  for guaranteed alignment above _Alignof(max_align_t),
  or above the host page size at any value.
- R-2: the Phase 7 redesign (size classes, splitting,
  reuse, caching) is designed - before it lands, the
  delivered-alignment statement of Section 3 must be
  re-measured with this slice's test (extended if the
  redesign changes return construction), because the
  redesign is exactly the event that can silently
  retire the page-alignment fact callers may have come
  to observe.
- R-3: an ABI-freeze, SONAME / symbol-versioning, or
  stable-release slice is commissioned (roadmap
  Phase 13 or a dedicated slice) - the last cheap moment
  to add a symbol deliberately.
- R-4: a second target is measured where the facts
  differ materially (a 4096-page host, musl, bionic) -
  the decision's factual basis is one target deep
  (INC-E006-05) and widens or falls with new targets.
- R-5: U-01 is settled for the standard-name surface in
  a way that includes aligned_alloc / posix_memalign -
  the namespaced question must then be re-argued
  against that settlement, not inherited from it.

## 7. Inconsistencies and limitations (INC-E006-*)

- INC-E006-01 - A-008 specifies aligned contracts that
  no surface implements. contracts.md Sections 2.6 / 2.7
  carry NORM success rows (MA-6.1-ALIGNED-01 / -02,
  MA-6.1-POSIXMA-01 / -02) while the implemented
  namespaced surface has no aligned entry point and
  U-01 leaves the standard-name set itself open. A
  reader skimming A-008 can mistake specification
  coverage for implementation coverage. This record is
  the counterweight: the rows are PROPOSED candidates
  for an unsettled surface. Resolution belongs to the
  U-01 decision; the E-009 packet should carry this
  item so the Phase 5 record does not repeat the
  ambiguity.
- INC-E006-02 - The construction's over-alignment is
  useful, observed, and uninsurable. Any caller that
  quietly relies on page alignment today breaks
  silently the day a lawful redesign delivers only the
  floor, and no test failure anywhere will announce it
  (the floor checks keep passing). Mitigations recorded
  here: the reliance is called out in Sections 3 / 5.1,
  and R-2 makes re-measurement a gate on the redesign.
  No code change is made or owed by this slice.
- INC-E006-03 - Alignment above the page size is not
  merely unpromised, it is non-deterministic in
  observation. The 32768 demand read mod 0 in this
  slice's smoke run and mod 16384 in the recorded run,
  same binary, same host. Any document or test that
  reports a single 2x-page observation as a property
  is wrong in both directions; the honest statement is
  that the construction delivers exactly the page
  modulus and nothing above it.
- INC-E006-04 - realloc preserves alignment here, but
  alignment after realloc is not a contract. Grow moves
  to a fresh page-aligned mapping; shrink and same-size
  retain the original pointer (B-008 S4.4 design facts).
  Both paths returned page-aligned pointers in Phase D,
  but the only promise a caller holds is the floor on
  the RETURNED pointer, re-derived after every realloc.
  There is no aligned-realloc notion in this surface,
  and a caller that obtained an (observed) over-aligned
  block and reallocs it has no aligned guarantee to
  carry over - the same unpromised fact, one lifecycle
  later.
- INC-E006-05 - The evidence is one target deep and
  single-threaded. Pi-native glibc, 16384-byte pages,
  one thread. The over-alignment fact is page-size-
  relative: on a 4096-page host the identical
  construction delivers at most a 4096 modulus, so
  "delivers up to page alignment" must never be quoted
  as a number. musl / bionic / x86-64 delivered
  alignment is unmeasured (D-008's musl leg BLOCKED,
  bionic leg OWED), and no threaded alignment run
  exists; a threaded re-run of test_e006 is OWED if
  this evidence is ever cited for concurrent paths
  (the mechanism - one global lock, fresh mapping per
  allocation - is argued race-free by construction,
  as in INC-E005-05, but argument is not a run).
- INC-E006-06 - This slice and D-008 deliberately
  overlap, and must not be double-counted. D-008
  measured the floor against the loose-object build as
  a libc-compatibility probe; E-006 measures the floor
  plus the demand ladder as a lifecycle through the
  linked .so. They are two views of one construction,
  not two independent constructions; the E-009 packet
  should cite D-008 for the cross-libc frame and this
  slice for the Phase 5 decision + linked-library
  lifecycle, once each.
- INC-E006-07 - "No aligned route" interacts with
  mixed-allocator ownership. A caller that routes
  over-aligned blocks to the system allocator and
  ordinary blocks to MimirAlloc owns a two-family
  programme: MimirAlloc pointers must never reach the
  system free and vice versa (foreign free is UB in-
  contract; E-008 tests the rejection posture). The
  absence decided here therefore exports an ownership
  burden to exactly the callers most likely to need
  alignment. Recorded as a real cost of Option 1, not
  a reason to have expanded the ABI silently; it is
  part of what R-1 evidence would weigh.

## 8. Self-test checklist (solo slice)

This slice ran solo-sequentially (team lead only, per
the keeper's muster), so verification is this documented
checklist, every item read back from the recorded run or
the tree, not from memory:

- [x] HEAD == ea14048 and branch == yrsa/mimiralloc-06
  before building (S0; git rev-parse in run).
- [x] free -g showed 13 GiB available before the runs;
  Samtal HTTP 200 before (S0) and after (S7).
- [x] make test exit 0 with composition unchanged - the
  Makefile diff is +26 / -0, additive test-e006 block
  only; no test target was added to make test (S1, S5,
  S6).
- [x] make test-e006 exit 0, 159 checks / 0 failed,
  RESULT PASS, in S3 and again from make clean in S5
  with identical counts and identical OBSERVED page-
  modulus line (49 of 49).
- [x] nm -D --defined-only on the rebuilt .so lists
  exactly mimir_malloc, mimir_free, mimir_calloc,
  mimir_realloc; count 4 in S2 and again in S5.
- [x] No aligned symbol anywhere: nm on the .so finds no
  align-named defined symbol; grep finds no aligned
  declaration or definition in src/ or the public
  header; the consumer's undefined mimir_ references
  are exactly the four (S4).
- [x] Header, map file, and every src/ file unchanged:
  git diff vs base touches Makefile only, plus the two
  new tests/abi/ files and this document (S6, final
  status at commit).
- [x] README.md diff vs base = 0 lines (S6).
- [x] Every number in Sections 0 / 3 is copied from the
  recorded run; the one number that varied between runs
  (the 32768 observation) is reported as varying
  (INC-E006-03), not averaged away.

## 9. What remains OWED

- Phase 5 exit remains OWED to E-009's packet. This
  slice discharges E-001 task T6 (decision record +
  alignment lifecycle) and the X3 alignment-lifecycle
  component only; T7 / T8 belong to E-007 / E-008.
- The PROPOSED entry point of Section 5.2 is
  unimplemented by design. Its contract debts (U-11 /
  V-03 per target, U-09 interaction, U-03 posture,
  versioning interaction) are OWED to whichever future
  slice a reopen trigger commissions.
- A threaded run of test_e006, and any run on a
  4096-page host, a musl build, or a bionic build, are
  OWED if this evidence is cited beyond the Pi-native
  single-threaded glibc target (INC-E006-05;
  INC-D003-02 and INC-D008-06 / -07 carry the host and
  libc legs).
- Carried debts untouched, as E-001 Section 6.3
  requires: Phase 4 exit UNMET (physical Android run
  OWED); Phase 3 exit UNMET (x86-64 suite run OWED);
  no target is promoted to supported by anything here.

## 10. Evidence files for this slice

- tests/abi/test_e006.c - the alignment lifecycle test
  (365 lines, 13,210 bytes, sha256 f80dced3): ENV +
  phases A-F as in its header; floor results are
  checks, page-modulus results are OBSERVED lines.
- tests/abi/recorded_run_e006.txt - the recorded run
  (247 lines, 7,904 bytes, sha256 aa0f178f): S0
  environment, S1 make test baseline, S2 nm -D export
  proof, S3 full test-e006 output, S4 absence proofs,
  S5 clean re-run reproducing every count, S6 README +
  diff scope, S7 host state after the runs.
- Makefile - additive test-e006 block only (+26 lines,
  0 deletions; sha256 of the merged file e6c4ceaf).
- This document - the assessment and decision record,
  with the INC-E006-* register (Section 7) and the
  reopen triggers (Section 6).
- Prior banked evidence relied on: docs/PHASE_5_
  FOUNDATION_PLAN.md (E-001: N2, T6, INC-E001-05),
  docs/contracts.md (A-008: Sections 2.6 / 2.7, V-03 /
  V-06 / V-07, U-register), docs/LIBC_ALIGNMENT_
  COMPATIBILITY.md (D-008: measured floor + page
  modulus, INC-D008-08), docs/SHARED_LIBRARY_BUILD.md
  (E-002: the exactly-4 export discipline this slice
  re-proves), docs/PHASE_5_AUDIT_HOOK_E005.md (E-005:
  counter state, unchanged).

README.md was not touched by this slice.
