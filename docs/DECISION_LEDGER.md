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
| D-008 | Z13 Wave 5 second geometry: lock-sharding (geometry B); per-thread arenas NOT taken | Adopted | Z13 roadmap Z-039; Z-039 recorded run decision rule R1-R4 | Z-039 follow-up (settler) |
| D-009 | Z13 placement policy trial: current policy retained; best-fit NOT taken, first-fit TIE | Adopted | Z13 roadmap Z-059; Z-059 recorded run pre-registered rule (a)-(f) | Z-059 (settler) |
| D-010 | Z13 silent courses (D-02): COUNTER chosen over sentinel for drain/trim/free lock-failure outcomes | Adopted | Z13 roadmap Z-065; defect register D-02 (INC-M002-02) | Z-065 (settler) |
| D-011 | Z13 diagnostics emission gate (L-07): the opt-in gate stands as the permanent posture; INC-J007-01 discharged | Adopted | Z13 roadmap Z-071; INC-J007-01 (docs/PHASE_10_DIAGNOSTICS.md S5) | Z-071 (settler) |
| D-012 | Z13 usable size: namespaced mimir_usable_size taken (malloc_usable_size shape NOT declared/exported); exact per-class semantics defined for this target (V-05) | Adopted | Z13 roadmap Z-075; contracts.md S2.9 (MA-COND-USABLE-01..04) | Z-075 (settler) |

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

## D-008 - Z13 Wave 5 second geometry: lock-sharding chosen at Z-039

- **Status:** Adopted
- **Roadmap basis:** docs/MIMIRALLOC_Z13_ROADMAP.md slice Z-039
  (Wave 5), which names this ledger as the decision's home;
  the pre-registered decision rule R1-R4 in
  tests/perf/recorded_run_z13_z039.txt; the banked evidence
  it applies to is tests/perf/recorded_run_z13_z038.txt
  (seat 7bc5473).
- **Decision:** (1) Geometry B is chosen: lock-sharding of
  the central core -- K = 4 shard locks keyed by size class
  (16 B and 32 B each own a shard, 48 B + 64 B share one,
  80 B..1024 B share the last), per-shard span lists, the
  mapping registry keeping the global lock taken LAST in a
  fixed ascending order (shards 0..3, then global), TLS
  refill/drain and central small alloc/free taking exactly
  their class's shard, cross-thread frees taking only the
  found span's shard, and all-shard acquisitions (flush,
  realloc, large path, snapshots, drain/trim) honouring
  the same order by superset. (2) Geometry A (per-thread
  arenas, scaled from Z-037's per-CCD prototype) is NOT
  taken: the road not taken, with its reason, is recorded
  in tests/perf/recorded_run_z13_z039_followup.txt. The
  deciding evidence, applied exactly as pre-registered:
  in Z-038's 8..16 thread decision band the arena leg's
  median ratios to banked are small_churn 1.009 / 1.002 /
  0.988 at 8 / 12 / 16 threads and mixed_batch 0.969 /
  1.074 / 0.989 -- R1 (arena beats banked by >5% on BOTH
  workloads in the band) fails on small_churn at every
  rung and on mixed_batch at two of three; R2 (arena
  within the 5% TIE band at 8..16 on either workload)
  fires, and R3's split route also lands on R2 because
  Z-036's verdict names no single priority workload. The
  arena leg's only wins sit at mixed_batch 24/28/32
  threads (+13.1% / +16.1% / +8.1%), outside the decision
  band. (3) Form of adoption, the Z-037 pattern: geometry
  B is built behind the build flag MIMIR_LOCK_SHARD with
  exactly zero production cost when the flag is off
  (flag-off objects byte-identical to the seat's); the
  production default remains the banked central lock
  until a later adoption slice flips the default on its
  own ladder evidence. The follow-up's own ladder table
  (banked vs sharded, same shapes and protocol as Z-038)
  is the gate evidence in
  tests/perf/recorded_run_z13_z039_followup.txt.
- **Supersedes:** None. **Status of entry:** Current.

---
## D-009 - Z13 placement policy: current policy retained at Z-059

- **Status:** Adopted
- **Roadmap basis:** docs/MIMIRALLOC_Z13_ROADMAP.md slice Z-059
  (Wave 7), which names this ledger as the decision's home;
  the pre-registered decision rule (a)-(f) in
  tests/perf/probe_z059.c; the banked oracle it applies to
  is tests/perf/recorded_run_z13_z055.txt (seat c2ed635).
- **Decision:** (1) The current placement policy - the
  class's active span first, then a first-fit scan of the
  span list, then a fresh span, with LIFO free-list reuse
  inside the span - is RETAINED as the production policy.
  No placement change is adopted. (2) Best-fit (serve
  from the can-serve span with the fewest servable slots)
  is NOT taken: on the Z-055 fragmentation oracle's
  decision leg (uneven fragmentation, 24 follow-on
  allocations per class, classes 64 and 256, three runs
  per policy with bit-identical counters) it LOSES to the
  current policy at class 256 (follow-ons spread over 3
  spans vs 2; spans left empty after the originals are
  freed 1 vs 2) and TIEs at class 64 - pre-registered
  clauses (b) and (c) fail. Its mechanism is recorded in
  tests/perf/recorded_run_z13_z059.txt: tightest-first
  consumes the small holes first and is then forced into
  the emptiest span, the span it exists to spare. (3)
  First-fit (ignore the active span, pure list-order
  scan) TIEs the current policy on every measured
  quantity in both legs; the active-span shortcut costs
  nothing measurable on this workload, and no change is
  made on a tie. (4) Form of the trial, the Z-037/Z-039
  pattern: both trial policies exist only behind the
  build flags MIMIR_PLACEMENT_FIRSTFIT and
  MIMIR_PLACEMENT_BESTFIT in the two mirrored selection
  sites (small_alloc_locked, tls_take_one_locked); with
  neither flag defined the core object's .text is
  byte-identical to the seat's. The flags stay available
  for a future re-trial. (5) Re-open condition, named
  only: a pre-registered burst-size sweep at volumes
  that fit inside the tight holes (e.g. 4..16 at class
  256), with the best-fit span-scan cost priced by a
  spans-scanned counter; no such run exists yet, and
  this entry stands until one supersedes it.
- **Evidence:** tests/perf/recorded_run_z13_z059.txt
  (leg A reproduces Z-055 exactly in all three builds:
  F-3 32064, R-EXT 0.260937, R-INT 0.091146; leg B table
  and clause record in the same file), with the standing
  gate green (4988/0/77) on the final tree.
- **Supersedes:** None. **Status of entry:** Current.

---
## D-010 - Z13 silent courses: counter, not sentinel, at Z-065

- **Status:** Adopted
- **Roadmap basis:** docs/MIMIRALLOC_Z13_ROADMAP.md slice Z-065
  (Wave 8), which leaves the mechanism open in its own text --
  "(counter or sentinel, chosen by ledger decision)" -- and
  names this ledger as the decision's home; the defect it
  discharges is register entry D-02 / INC-M002-02 in
  docs/LIMITATIONS_AND_KNOWN_DEFECTS.md (drain/trim return 0
  on lock failure, indistinguishable from nothing retained;
  free on lock failure returns silently with the block live).
- **Decision:** (1) COUNTER is chosen; the sentinel road is
  NOT taken. Three monotone tallies are appended at the END
  of struct mimir_stats -- drain_lock_failures,
  trim_lock_failures, free_lock_failures -- each a pre-lock
  atomic in the Z-040 posture (own cache line, incremented
  at the failure site with no lock held), copied by the
  stats snapshot and also readable through lock-free
  getters, exactly the reporting shape Z-064 (D-01) set for
  the neighbouring unlock_failures tally. (2) No return
  value changes: drain/trim keep returning 0 on lock
  failure and free keeps its F-07 posture (the block stays
  LIVE, nothing is freed unlocked). The courses become
  distinguishable through the tallies, not through a
  changed contract. (3) The sentinel alternative -- e.g.
  returning (size_t)-1 from drain/trim on lock failure --
  is rejected on three recorded grounds: it changes the
  I-003 return contract every existing caller was written
  against (the Z-050 trim policy reads the return as a
  released-span count, and the unload drain ignores it);
  a sentinel count can collide with arithmetic on the
  value (span budgets, byte reconciliation) at any caller
  that misses the new meaning; and it reports only to the
  immediate caller, while a counter reports to every later
  observer, including post-hoc diagnosis of a course whose
  caller has long returned. (4) Scope of the free tally:
  every F-07 free course counts -- the legacy central
  path, the owner-routed remote path, the shard-build
  remote path, and the TLS full-bin drain inside a local
  free. The thread-exit drain counts as a drain course.
  Allocation-path lock failures are NOT in scope: they
  already return NULL with a diagnostics report (not
  silent) and keep that channel. (5) Relation to Z-064:
  the two tallies are disjoint by construction -- Z-064
  counts failed UNLOCKS (D-01), Z-065 counts failed LOCKS
  at the silent courses (D-02); neither site touches the
  other's counter. Whichever of the two slices banks
  first, the appended stats fields land in slice order at
  the struct's end and no earlier field's offset moves.
- **Evidence:** tests/perf/recorded_run_z13_z065.txt
  (contract suite tests/core/test_z065_silent.c: each
  course's return is proven identical to its quiet no-op
  return while its tally moves by exactly one per armed
  failure via the I-004 lock shim; the nothing-retained
  control proves the tallies stay at 0 when no failure
  occurs), with the standing gate green (4988/0/77) on
  the final tree.
- **Supersedes:** None. **Status of entry:** Current.

---

## D-011 - Z13 diagnostics emission gate (L-07): the opt-in gate stands, at Z-071

- **Status:** Adopted
- **Roadmap basis:** docs/MIMIRALLOC_Z13_ROADMAP.md slice
  Z-071 (Wave 8), which requires the campaign's ruling on
  the diagnostics default to be recorded in this ledger,
  discharging INC-J007-01 "one way or the other"; the item
  itself is docs/PHASE_10_DIAGNOSTICS.md S5/S8
  (INC-J007-01) and limitation L-07 in
  docs/LIMITATIONS_AND_KNOWN_DEFECTS.md.
- **Decision:** (1) The emission gate STANDS as the
  permanent production posture: mimir_core_diag_report
  emits only when the process environment carries
  MIMIRALLOC_DIAG=1. The default does NOT flip to on.
  J-001 S5.4's default-on reading ("never no words") is
  superseded by this entry: the channel's words are
  wired, proven and available in every production build,
  and the process opts in. (2) Detection and counting
  are NOT gated and never were: every wired detection
  (Z-064 unlock failures, Z-065 silent-course tallies,
  Z-066 length detections, Z-067 live_count release
  refusals) moves its stats-surface tally with the gate
  closed; only the stderr line is opt-in. The always-on
  record is the stats surface; the line is a courtesy
  copy for an opted-in operator. (3) No gate bypass for
  corruption detections (the open half of INC-J007-02):
  the gate applies uniformly to every channel emission,
  the discipline Z-066 and Z-067 shipped. (4)
  INC-J007-03 is discharged by inventory, not wiring:
  the remaining unwired sites are listed with their
  state and reason in the Z-071 packet S6; no site was
  wired by this slice. Grounds, measured on purpleraider
  at Z-071 (packet of record below): gate-open versus
  gate-closed is a TIE on all four oracle workloads
  (medians within -0.26%..+0.29%, inside the
  0.07%..1.09% identical-leg agreement; emitted exactly
  0 and stderr exactly 0 bytes on every success leg in
  both postures -- success paths make no report calls,
  so the gate costs nothing there and the flip would
  buy nothing there); emission costs a measured ~97 ns
  per genuine failure (one fixed line, 88.5-93.5 bytes,
  exactly one per failure) on a path already returning
  NULL. Cost therefore decides neither way; the
  contract does: the banked C-006 silence battery
  (MA-6.3-020..023, inside the standing gate) grades
  zero bytes on fds 1/2 as a banked observable, and
  four banked suites (test_c006, test_j007, test_z066,
  test_z067) assert the unset-means-silent posture. An
  allocator does not write uninvited to a host
  process's fd 2; opting in is one environment
  variable. Flipping would amend four banked evidence
  suites to change a default whose measured price is
  zero and whose benefit is a line the operator can
  already have by asking.
- **Evidence:** tests/perf/recorded_run_z13_z071.txt
  (probe / runner / summariser tests/perf/probe_z071.c,
  run_z071.sh, summarize_z071.py; raw and summary logs
  in the slice's team hidden_files), with the standing
  gate green (4988/0/77) on the final tree.
- **Supersedes:** the default-on reading of J-001 S5.4
  as recorded in docs/PHASE_10_DIAGNOSTICS.md S5 (the
  interim gate posture itself is unchanged and becomes
  permanent). **Status of entry:** Current.

---

## D-012 - Z13 usable size: the namespaced shape, exact per class, at Z-075

- **Status:** Adopted
- **Roadmap basis:** docs/MIMIRALLOC_Z13_ROADMAP.md slice
  Z-075 (Wave 9), which requires the naming decision
  (mimir_usable_size vs malloc_usable_size shape) to be
  recorded and the implementation to be exact per class,
  never a guess, with the decision in this ledger. The
  contract hooks are docs/contracts.md S2.9, rows
  MA-COND-USABLE-01..04, whose semantics are conditional
  on the per-target record V-05; this entry IS that
  record for the Z13 target.
- **Decision:** (1) NAMING: the public surface is
  `size_t mimir_usable_size(const void *ptr)`, the
  namespaced shape. The standard name
  `malloc_usable_size` is NOT declared in the public
  header and NOT exported by the library (the slice's
  ABI recorded run proves its absence from .dynsym).
  Grounds: every surface this line has shipped is
  namespaced (the B-008 S7.2 supported list; Z-074's
  mimir_aligned_alloc and the posix-shaped-but-
  namespaced mimir_posix_memalign); a bare malloc_*
  name in the default build would interpose silently
  in any consumer that links the .so for the
  namespaced API -- the exact surprise the Wave 10
  interposition slice (Z-082) exists to audit and
  opt into deliberately, never a side effect of a
  Wave 9 query. The const parameter records
  MA-COND-USABLE-01's no-modification clause in the
  type: the call is a pure query. (2) SEMANTICS
  (V-05 for this target): the answer is read from the
  allocator's own records, never derived from the
  request. A span-registry block answers exactly its
  span's class size (the G-002 table, 16..1024) --
  no per-object requested-size record exists
  (INC-G003-03) and the class extent is the block's
  whole usable extent. A mapping-registry block
  answers exactly its descriptor's usable_size
  (mapping_length - usable_offset, revalidated by
  descriptor_is_live on the lookup itself); a realloc
  shrink lowers only the requested-size record, so
  the answer after a shrink is still the mapping's
  full usable extent. NULL answers 0. A pointer the
  registries can determine is not a live block --
  foreign, interior, or already freed (span free
  list, retired or pooled descriptor, or the calling
  thread's own TLS bin, which the core checks by
  identity) -- answers 0 by free's non-destructive
  S8.3 course; lock failure answers 0 (the F-07
  posture carried to a query). (3) THE NAMED
  BOUNDARY: a block resident in ANOTHER thread's TLS
  bin is indistinguishable from live to any other
  thread -- the cache holds no cross-thread identity
  index by design (Z-028 contract S9) -- and is
  answered as its class extent. No promise is made
  for it: asking after a freed block is use after
  free, UB across the family, and MA-COND-USABLE-03
  states no behaviour for a released argument. The
  boundary is recorded here rather than hidden; a
  future cache redesign that adds a cross-thread
  residence index may narrow it, at its own slice's
  recorded decision. (4) The slice adds one public
  symbol: the shared library's export list grows
  from six to exactly seven as an explicit contract
  change, recorded in the slice's ABI run.
- **Evidence:** tests/abi/recorded_run_z13_z075.txt
  (static exactness test tests/abi/test_z075.c --
  every request 1..1024 against the class table, the
  mapping ladder, calloc/aligned composition, NULL /
  interior / foreign / freed refusals, realloc shrink
  and move postures, cross-thread courses, the audit
  counter -- plus the dynamic and C++ consumers and
  the run_z075.sh ABI proofs), with the standing gate
  green (4988/0/77) on the final tree. Above-page
  aligned extents proved to be per-block record state:
  the mapping base is page-aligned, not
  alignment-aligned, so the usable offset -- and with
  it the answer -- varies with the base's alignment
  residue (probe on this host: aligned_alloc(8192,
  100) answered 12288, aligned_alloc(16384, 100)
  answered 8192); the test pins invariants for that
  case and exact values everywhere a closed form
  exists.
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
