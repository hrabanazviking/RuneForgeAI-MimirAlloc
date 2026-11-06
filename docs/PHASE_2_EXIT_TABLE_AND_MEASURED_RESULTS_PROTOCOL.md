# MimirAlloc — Phase 2 Exit Table Synthesis and Measured-Results Protocol

## Header

- **Slice:** B-005 (Phase 2, synthesis)
- **Deliverable:** `docs/PHASE_2_EXIT_TABLE_AND_MEASURED_RESULTS_PROTOCOL.md`
- **Companions:**
  - B-001 — `docs/PHASE_2_EVALUATION_PLAN.md`
  - B-002 — `docs/SYSTEM_ALLOCATOR_EVALUATION.md`
  - B-003 — `docs/CONFIGURABLE_BUILD_FLAGS_AND_LOCAL_PATCH_EVALUATION.md`
  - B-004 — NOT banked (see Section 1.4)
- **Status:** research / evaluation. Phase 2 is NOT exited by this slice.
- **Base:** `5ea3a8d` on branch `yrsa/mimiralloc-05`
- **Citations:** Roadmap Sections 8, 11, 2, 15; `EVIDENCE_LEDGER.md`
- **README note:** `README.md` was not touched.

> **B-004 banner:** B-004 (team-04, P2-R04) has NOT banked on base `5ea3a8d`. No B-004 document exists in this tree, its fields and results were NOT used, and Route 4 is covered solely from B-001 Sections 8/9. Reconcile when B-004 banks; the keeper banks B-005 after B-004.

---

## 1. Honest headline

### 1.1 What is proven (only these)

Only the following are proven on the evidence in this tree. Nothing beyond this list is claimed as proven in this document.

**B-002 Section 7 — Pi micro-probe (glibc 2.36):**

- C probe: 2470 bytes, sha256 `2ed8ac86c03ac7561ff5656019d751fc6316039fab860677f8e7509a8392f788`.
- Built with `gcc -O2 -Wall`, exit 0.
- 3 fresh runs: identical stdout, exit 0 — covering 10000x32 small allocations, 10x1MiB large allocations, `realloc`, `calloc`, and usable sizes.
- Configuration runs `MALLOC_CHECK_=3` and `GLIBC_TUNABLES=glibc.malloc.arena_max=2`: exit 0, identical output.
- Wall times 0.007 / 0.006 / 0.006 s — explicitly NOT a benchmark.
- Selection proof: `ldd` shows `libc.so.6` only, and `nm -D` shows `malloc@@GLIBC_2.17` as `T` at `0x92240`.

**B-003 Section A8 — build-flag probe:**

- Trivial 27-byte program whose behaviour is `return 42`.
- 9 flag sets: all exit 42, except `-Wl,-z,max-page-size=4096` (8936 bytes, alignment `0x1000`), which exited 139 (segfault), reproduced on 2 fresh builds.
- Default build: 70376 bytes, exit 42.
- `max-page-size=16384`: 21224 bytes, alignment `0x4000`, exit 42.
- text / data / bss: 1538 / 616 / 8, identical across the compared sets.
- `-static`: 702960 bytes, built, NOT run.

**B-001 Section 3 / B-003 searches — inventory facts only:**

- jemalloc present: `libjemalloc.so.2` / package `libjemalloc2:arm64` version 5.3.0-1.
- mimalloc absent; tcmalloc absent.
- Zero patch files found.

These inventory facts prove presence or absence on this Pi only. They are not a run, a benchmark, or an integration result for any comparator.

### 1.2 What is not proven

The inherited never-claim list stands in full:

- No Mojo result.
- No Aesir result.
- No multithreaded result.
- No fragmentation result.
- No RSS / PSS result.
- No latency result.
- No throughput result.
- No tablet result; no Termux result; no bionic result; no musl result; no x86-64 result.
- No real target rebuilt.
- No patch identified; no patch applied.
- Phase 1 remains open: A-005 is synthetic only, Integration is UNDETERMINED, and there is no real failure packet.
- No B-004 result exists in this tree.

### 1.3 Verdict summary

| Route | Evidence position | Verdict |
|---|---|---|
| R01 — system allocator | Device-tested (probe) / Blocked (Mojo) | Inconclusive for unblocking |
| R02 — configurable build flags | Proposed (real target) / Device-tested (probe) | Inconclusive |
| R03 — local patch | Blocked | Blocked |
| R04a / R04b / R04c — comparators | Proposed | TBD pending B-004 (owed) |

The full per-route verdicts, in the fixed B-001 vocabulary, are recorded in Section 3. This summary does not promote any verdict beyond that table.

### 1.4 B-004 status — mandatory banner, expanded

> B-004 (team-04, P2-R04) has NOT banked on base `5ea3a8d`. No B-004 document exists in this tree, its fields and results were NOT used, and Route 4 is covered solely from B-001 Sections 8/9. Reconcile when B-004 banks; the keeper banks B-005 after B-004.

Consequences for this document:

- Every Route 4 measured field in Sections 3 and 4 is recorded as **owed / pending**, never invented.
- No Route 4 command is presented as run; all Route 4 commands are design commands from B-001 Section 8, marked UNRUN.
- No Route 4 verdict is given; each Route 4 verdict is TBD pending B-004.
- When B-004 banks, this document must be reconciled against it: fill the owed fields from B-004 evidence only, and only then assign Route 4 verdicts.

---

## 2. What synthesis means, and scope

### 2.1 Route inventory (from B-001 Section 2)

- **R01 — System allocator:** use the allocator the system already provides.
- **R02 — Configurable build flags:** same source and same allocator, varied only by configurable build flags.
- **R03 — Local patch:** a local patch, in classes C1 / C2 / C3. The three classes are never conflated with one another.
- **R04 — Comparators:** mimalloc, jemalloc, and tcmalloc, as diagnostics only.

**MimirAlloc is NOT a Phase 2 route (INC-3).** It is not listed as a route above and is not evaluated as one here.

Comparators do not cancel MimirAlloc. A comparator result, when one exists, is diagnostic evidence about the comparison point; it is not a substitute for, and does not remove the need for, the MimirAlloc work itself.

### 2.2 Falsifiable hypothesis per route

Each route carries one falsifiable hypothesis, drawn from B-002 and B-003. A hypothesis is retained only while a concrete test could, in principle, refute it:

- **R01:** the system allocator, selected and configured as in B-002, is sufficient to unblock the blocked consumer on the Pi. Refuted if the probe behaviour does not transfer to the real consumer, or if the consumer remains blocked for allocator reasons.
- **R02:** a configurable build-flag change alone, with source and allocator held constant, changes the relevant outcome for a real target. Refuted if no flag set changes the outcome, or if a flag set that works on the trivial probe fails or faults on a real target — as the 4096-byte max-page-size set already faults on the probe.
- **R03:** a specific, identified local patch in exactly one of C1, C2, or C3 removes a specific, evidenced failure. Refuted, and currently untestable, while no patch and no real failure packet have been identified.
- **R04:** a comparator allocator, run diagnostically under the same conditions, isolates whether a behaviour follows the allocator or follows the consumer. Currently untested: no comparator run exists in this tree, and the detailed fields are owed to B-004.

### 2.3 Target honesty

The Pi target is: Linux, AArch64, glibc, 16 KiB pages.

That target is **not** the tablet, and a Pi result does not transfer silently to:

- a tablet target,
- bionic (Android libc),
- musl,
- or a 4 KiB-page target.

Any claim for those targets requires its own evidence on that target.

### 2.4 Evidence labels and the no-promotion rule

The evidence labels used in this document (Section 2 list) are, in order:

1. Proposed
2. Implemented
3. Host-tested
4. Device-tested
5. Integration-proven
6. Performance-measured
7. Blocked

**No-promotion rule:** a row, result, or route may never be labelled higher than the evidence actually held for it. A probe result is Device-tested for the probe only; it does not promote the same route, on a real target or for a real consumer, to Device-tested, Integration-proven, or Performance-measured. Compilation alone promotes nothing. Inventory presence promotes nothing.

---

## 3. Phase 2 exit table

This table uses the B-001 Section 9 columns (in shortened header form), with two declared shape changes: one insertion described in the note below the table (a Consumer/layer column immediately after Route), and B-001's separate Route ID and Route columns carried as one Route cell, the ID leading each cell (so each row has 11 cells, matching the header). Its values are recorded honestly as Probe only or TBD, and are never silently filled.

| Route | Consumer / layer | Modes | Exact commands | Environment | License | Linking | Measured result | Evidence label | Limitations | Verdict |
|---|---|---|---|---|---|---|---|---|---|---|
| R01 — System allocator | Probe only | Default; MALLOC_CHECK_=3; arena_max=2 | RUN: gcc -O2 -Wall -o /tmp/mimiralloc_probe /tmp/mimiralloc_probe.c && /tmp/mimiralloc_probe — plus B-001 Section 5 design commands, marked UNRUN | Pi, AArch64, glibc 2.36, PAGESIZE 16384, gcc 12.2.0, ld 2.40; real target TBD | LGPL-2.1-or-later | Dynamic; ldd shows libc.so.6 only | B-002 Section 7 probe only: 2470B probe, sha256 2ed8ac86c03ac7561ff5656019d751fc6316039fab860677f8e7509a8392f788, 3 fresh runs identical stdout exit 0, both config runs exit 0 identical, wall 0.007/0.006/0.006 s NOT a benchmark, malloc@@GLIBC_2.17 T 0x92240 | Device-tested (probe); Blocked (Mojo) | Probe only; no Mojo, real consumer, multithreaded, fragmentation, RSS/PSS, latency, or throughput evidence; Pi target only | Inconclusive |
| R02 — Configurable build flags | Probe only; real target TBD | 9 flag sets on a trivial program; same source, same allocator | RUN: probe commands in Appendix A of B-003 — plus A4 design commands, marked UNRUN | Pi, AArch64, glibc 2.36, PAGESIZE 16384, gcc 12.2.0, ld 2.40; real target TBD | GPL-3.0+Runtime Exception | Dynamic, except -static set built NOT run | B-003 Section A8 probe only: 27B return-42 program, all 9 sets exit 42 except max-page-size=4096 (8936B, align 0x1000) exit 139 segfault reproduced on 2 fresh builds; default 70376B exit 42; max-page-size=16384 21224B align 0x4000 exit 42; text/data/bss 1538/616/8 identical; -static 702960B built NOT run | Device-tested (probe); Proposed (real target) | Trivial probe only; no real target rebuilt; a flag set that faults the probe may not be assumed safe elsewhere; Pi target only | Inconclusive |
| R03 — Local patch (C1 / C2 / C3, never conflated) | TBD | C1; C2; C3 — recorded separately, none selected | Design only: B-001 Section 7 and B-003 Section B3; none executed | Pi, AArch64, glibc 2.36, PAGESIZE 16384, gcc 12.2.0, ld 2.40; real target TBD | TO-VERIFY | TBD | TBD — not run; no patch identified and no patch applied; zero patch files found in the B-001 Section 3 / B-003 searches | Blocked | No real failure packet; Phase 1 open and Integration UNDETERMINED, so there is no evidenced failure for a patch to answer; class cannot be chosen honestly | Blocked |
| R04a — Comparator: mimalloc | TBD | Diagnostic only | Design only: B-001 Section 8; UNRUN | Pi, AArch64, glibc 2.36, PAGESIZE 16384, gcc 12.2.0, ld 2.40; comparator environment TBD, owed to B-004 | TO-VERIFY | TBD, owed to B-004 | TBD — not run; owed to B-004; inventory fact only: mimalloc absent on this Pi | Proposed | No run exists in this tree; B-004 has not banked; diagnostic only and does not cancel MimirAlloc | TBD pending B-004 |
| R04b — Comparator: jemalloc | TBD | Diagnostic only | Design only: B-001 Section 8; UNRUN | Pi, AArch64, glibc 2.36, PAGESIZE 16384, gcc 12.2.0, ld 2.40; comparator environment TBD, owed to B-004 | TO-VERIFY | TBD, owed to B-004 | TBD — not run; owed to B-004; inventory fact only: libjemalloc.so.2 / libjemalloc2:arm64 5.3.0-1 present on this Pi | Proposed | Presence is not a run; no run exists in this tree; B-004 has not banked; diagnostic only and does not cancel MimirAlloc | TBD pending B-004 |
| R04c — Comparator: tcmalloc | TBD | Diagnostic only | Design only: B-001 Section 8; UNRUN | Pi, AArch64, glibc 2.36, PAGESIZE 16384, gcc 12.2.0, ld 2.40; comparator environment TBD, owed to B-004 | TO-VERIFY | TBD, owed to B-004 | TBD — not run; owed to B-004; inventory fact only: tcmalloc absent on this Pi | Proposed | No run exists in this tree; B-004 has not banked; diagnostic only and does not cancel MimirAlloc | TBD pending B-004 |

**Consumer / layer note (deviation, declared):** the Consumer/layer column inserted after Route is a deviation from the B-001 Section 9 column set. It is required by B-002 Section 8 together with the Integration layer material at Section 10.6 (item I-B003-5), because a route result is meaningless without naming which consumer and layer it was actually measured on. Its values above are Probe only where a probe is all that exists, and TBD where no consumer has been measured. They were not silently filled, and no value in that column promotes any row beyond its evidence label.

**Verdict vocabulary:** verdicts in this table use only the fixed B-001 vocabulary — Unblocks, Partially unblocks, Does not unblock, Inconclusive, Blocked — except the Route 4 rows, whose verdicts are recorded as TBD pending B-004 rather than forced into that vocabulary without evidence.

### 3.1 Exit rule

> Phase 2 passes only when every row has an exact command, a measured result, and integration limitations filled from evidence. An empty, name-only, or compile-only row is Blocked, not a pass. (B-001 Section 9, verbatim.)

### 3.2 Conclusion on current evidence

On current evidence the exit rule is **NOT met**, and Phase 2 is **not exited**:

- R01 and R02 hold probe evidence only; neither has a measured result for a real consumer or a real target.
- R03 has no patch, no executed command, and no measured result, and is Blocked.
- R04a, R04b, and R04c have no run, no measured result, and no verdict; their fields are owed to B-004, which has not banked.

No row in this table is presented as a pass, and this slice does not exit Phase 2.

## 4. Per-route evidence summaries

This section contains no new results. It summarises, route by route, what
the earlier slices already established, and points to the slice and
section where each fact lives. Nothing in this section promotes an
evidence label, fills a field that an earlier slice left as TBD, or
imports a result from any other worktree.

### 4.1 R01 — System allocator

Source: B-002.

- **Definition.** "System allocator" is defined per-libc, not as a
  singular thing. Which allocator is "the system allocator" depends on
  the C library in use on the consumer in question; the term must never
  be used as if it named one universal implementation.
- **Route.** Fall back to the libc allocator. Where no replacement is
  selected, the consumer uses its libc's allocator.
- **What this route cannot do.** It cannot remove a statically linked
  tcmalloc from a consumer that carries one; it cannot fix allocators
  internal to a consumer; and it cannot address address-width faults.
  Those are properties of the consumer, not of the fallback route.
- **Modes (per B-002).**
  - M1: null control.
  - M2+M3: direct.
  - M3 static: blocked.
  - M4: own-code only.
- **Measured.** The B-002 Section 7 probe only: 3/3 runs exit 0 with
  identical results; selection shown by `ldd` / `nm`; recorded sha
  beginning `2ed8ac86...`. The wall time of that probe is NOT a
  benchmark and must not be quoted as one.
- **NOT run.** The not-run list is B-002 Section 7.3, incorporated here
  by pointer only; nothing on that list is claimed as run.
- **Limitations.**
  - No failure packet exists for this route.
  - The static pixi mojo consumer is unreachable for this route
    without an owner rebuild.
  - Two facts, both from B-002 Section 2, must be kept distinct: the
    dynamically-linked Mimir mojo is already on the system allocator;
    the static pixi mojo is unreachable. Neither fact implies the
    other, and neither is a measurement of allocator performance.

### 4.2 R02 — Build flags

Source: B-003 Part A.

- **Definition.** The build-flags route changes how a consumer binary
  is built — its recorded build settings — rather than replacing the
  allocator at run time.
- **Modes (per B-003 Part A).**
  - Direct: Yes — the primary mode.
  - `LD_PRELOAD`: No.
  - Static: conditional.
  - Namespaced: No.
- **Measured.** The B-003 Part A A8 probe only. Its page-size finding
  is stated narrowly, and only narrowly: a binary whose maximum page
  size is below the kernel page size can fail on this 16 KiB-page Pi.
  The probe is NOT a benchmark; it is NOT a real target; and it does
  NOT establish the direction of the A2 Q4 question.
- **Recorded fields.** Every A9 field is TBD, except the toolchain and
  the probe, which are recorded in B-003 Part A.
- **Limitations.** The A10 limitations apply in full, including the
  mandatory statement, which is part of this route's record and must
  not be dropped in any summary: this route "does not establish
  allocator superiority".

### 4.3 R03 — Local patch

Source: B-003 Part B.

- **Status: NONE IDENTIFIED.** No local patch has been identified for
  this route. Nothing in this subsection invents, proposes, or implies
  a patch.
- **Sub-routes.** B-003 Part B records three sub-routes, C1, C2, and
  C3, in its table. Until a patch is identified, none of them is
  selected, started, or evidenced.
- **A patch packet must contain** (per B-003 Part B): the identified
  defect the patch addresses; the exact change proposed; the consumer
  and layer it touches; the selection / interposition proof that would
  apply; the test or probe that would be run; the expected effect and
  how it would be recognised; and the revert path (see the revert
  criteria below).
- **Runs.** The searches recorded in B8 are the only runs for this
  route. Searches are not measurements (see Section 5.1).
- **Recorded fields.** All B9 fields are TBD.
- **Limitations.** The B10 limitations apply in full.
- **Revert criteria.** The B11 revert criteria are summarised here by
  pointer: a patch under this route is reverted when its packet's
  criteria are met, and no patch is retained on the strength of an
  unmet criterion.
- **What this route establishes.** Nothing about MimirAlloc, and
  nothing about the original failure. An unidentified patch is not
  evidence for or against either.

### 4.4 R04 — Established comparators

Source: B-001 Sections 8 and 9. Status for every comparator below:
**OWED / PENDING ONLY.**

No B-004 document exists in this tree, and nothing from team-04's
worktree is used in this section or anywhere in this document. Every
measured field for every comparator below reads, verbatim in effect:
"TBD — not run; owed to B-004". Licence for every comparator below:
TO-VERIFY. Modes for every comparator below are design-only, from
B-001 Section 8. The guardrails of B-001 Section 8.4 — all 9 items —
apply to every comparator run when it is eventually made.

#### (a) mimalloc

- **Presence:** not present on the Pi (inventory finding, B-001).
- **Status:** owed / pending only; not run.
- **Open questions include:** behaviour on the 16 KiB page size of
  this Pi; and naming discipline — "mimalloc" and "MimirAlloc" are
  different names for different things and must be written in full,
  never shortened or conflated, in every record.
- **Measured fields:** TBD — not run; owed to B-004.
- **Licence:** TO-VERIFY.

#### (b) jemalloc

- **Presence:** `libjemalloc.so.2`, version 5.3.0-1, is present on the
  Pi. That is an INVENTORY ONLY finding: presence is not selection,
  and presence is not a measurement (see Sections 5.1 and 5.2).
- **Status:** owed / pending only; not run.
- **Open questions include:** the J-1 provenance question recorded in
  B-001, and the set-membership question INC-3 recorded in B-001.
  Both remain open and are carried here by pointer only.
- **Measured fields:** TBD — not run; owed to B-004.
- **Licence:** TO-VERIFY.

#### (c) tcmalloc

- **Standing:** tcmalloc is implicated, not neutral, in the original
  failure. It must never be treated as a neutral comparator.
- **Presence:** not present on the Pi (inventory finding, B-001).
- **Recorded build fact, guarded:** a prior summary records a build
  of 264/300 Bazel actions. That is a prior summary only; its
  artifacts are NOT in this repository, and it must not be restated
  as a result obtained in this tree.
- **Duties carried forward:** the address-width agreement duty
  recorded in B-001 applies to any future tcmalloc run.
- **Naming distinction:** `google/tcmalloc` and gperftools are
  distinct; every record must say which one it means, in full.
- **Measured fields:** TBD — not run; owed to B-004.
- **Licence:** TO-VERIFY.


## 5. Measured-results protocol

This section is normative for every future measured claim in this
programme. It restates the Roadmap Section 11 requirements together
with B-001 Section 4. A claim that does not satisfy this protocol is
not a measured result, whatever it is called elsewhere.

### 5.1 What counts as a measurement

A measurement is all of the following, together:

- a fresh-process run,
- on a named device,
- of a pinned artifact,
- under a recorded command,
- with selection proven (see Section 5.2),
- and with the raw log preserved.

The following are NOT measurements, and must never be reported as if
they were: inventory, presence, design, and searches. Precedents:
B-001 Section 3 and B-003 B8, where inventory and search activity were
recorded as what they are and claimed as nothing more.

### 5.2 Selection proof — never by name

Selection of an allocator is never proven by name, by presence, or by
compile success. Acceptable selection-proof artifacts include:

- `ldd` output;
- `readelf -d` / `readelf -sW` output;
- `nm -D` / `nm -u` output;
- `LD_DEBUG=bindings` output;
- `/proc/<pid>/maps` for the running consumer;
- an allocation counter or audit hook that observed the allocations.

A name, a file's presence on the system, or a successful compile is
never, on its own, proof that an allocator was selected.

### 5.3 Required recorded fields

Every measured claim must record all of the following. A missing
field makes the claim incomplete and it must be labelled as such.

- Run ID; date; device; operator.
- Route and pin: repository, commit, checksum; source tree.
- Mode; consumer and layer.
- Linkage of the consumer.
- Selection-proof artifact (per Section 5.2).
- Verbatim command; clean build directory: yes / no.
- Environment manifest: architecture / ABI; OS and kernel; libc and
  its package; toolchain; page size; thermal, core, and clock state;
  environment variables including `MALLOC_*`, `LD_PRELOAD`, and
  `GLIBC_TUNABLES`; and `/etc/ld.so.preload`.
- Workload / harness: revision, input, and benchmark class (Roadmap
  Section 11 classes 1–8, listed in Section 5.4 below).
- Repetitions.
- Results: median, p95, p99, and outliers; throughput; peak RSS and
  PSS; virtual size; retained after free; mapping count; startup;
  CPU time; and responsiveness / energy where they can be measured
  reliably.
- Correctness result.
- `free -g` before and after, and impact on Samtal on `:8787`.
- Raw log: path and sha256.
- Evidence label (per Section 2; no promotion — see Section 5.5).
- Negative results, preserved, not deleted or omitted.

### 5.4 Repetition and environment protocol

- Fresh processes for every repetition; no in-process repeats counted
  as repetitions.
- One variable at a time.
- A-B-A ordering: baseline interleaved with the variant, so drift is
  visible; warm-up runs are discarded and not counted.
- Identical harness, input, and settings across compared runs.
- The baseline row comes first. Tuned versus untuned are separate
  rows; a system-package build versus a pinned build are separate
  rows. Rows are never merged after the fact.
- Benchmark classes (Roadmap Section 11, verbatim titles):
  (1) Single-threaded small allocation/free cycles with varied size
  distributions. (2) Producer/consumer and cross-thread free
  patterns. (3) Thread churn and multiple CPU clusters on ARM
  big.LITTLE devices. (4) Large allocations and mixed short/long
  lifetimes. (5) Fragmentation, return-to-OS and repeated
  startup/shutdown. (6) A real clean Mojo build and compiler
  invocation, if integration is proven. (7) Aesir load, prefill,
  decode and unload on the same model/configuration. (8) A
  time-limited multitasking and thermal soak on the physical
  tablet. Classes 1-5 only for measured claims in this phase;
  classes 6-7 only if integration is proven for that mode; class 8
  is tablet-only and is never run or claimed on the Pi.
- Targets are set after a device baseline exists. No fixed
  superiority claim is made in advance of, or independently of, that
  baseline.
- Pi safety: record `free -g` before and after; stop if
  `MemAvailable` falls; never starve Samtal.

### 5.5 Never-claim list

The following claims are never made, in any document in this
programme:

1. A probe is not a benchmark and not a performance result.
2. A trivial pass is not the Phase 6 gate.
3. A result on the Pi is not a result on the tablet, Android, bionic,
   musl, x86-64, or a 4 KiB-page system.
4. Presence is not selection.
5. Compilation is not interposition.
6. A synthetic or emulator result is not device proof and not
   application proof.
7. A namespaced success is not a drop-in replacement.
8. An `LD_PRELOAD` failure on a static consumer is not an allocator
   verdict.
9. A latency win that raises RSS, retention, or stutter is not a win
   against the mobile objective.
10. Swap is not physical RAM.
11. No licence is asserted without verification from the pinned
    artifact itself.
12. No result transfers across libcs, toolchains, or commits.
13. Evidence labels are never promoted beyond Section 2.

### 5.6 Worked application — the two existing probes

Applying this protocol retrospectively to the only two probes that
exist in this programme:

- **B-002 probe (R01).** Fresh-process runs of a pinned probe under a
  recorded command, with selection shown by `ldd` / `nm`, 3/3 exit 0
  identical, and a recorded sha. Label: **Device-tested (probe)**.
- **B-003 probe (R02, A8).** A recorded probe establishing only the
  narrow page-size finding of Section 4.2. Label:
  **Device-tested (probe)**.

Neither probe is **Performance-measured**, and neither may be promoted
to that label. The fields this protocol requires for a performance
claim are missing in both cases, including: the Roadmap Section 11
statistics (median / p95 / p99 with outliers, recorded as benchmark
results); RSS / PSS and the other resource fields of Section 5.3;
repetitions run as a benchmark rather than as a probe check; a
comparator row; and a tracked harness — item I-B002-7 remains the
tracked harness item and is not discharged by either probe.

That is the whole of the measured record at Phase 2 exit: two
device-tested probes, correctly labelled, and every performance field
still TBD until a run satisfying this protocol exists.


## 6. Phase 6 gate restated for synthesis

The Phase 6 gate is restated here for synthesis only. It is not
re-decided by this slice, and it is not closed by this slice for
any route.

> Phase 6 gate = the original failing command succeeds, with
> selection proof, on the exact pinned artifact, in the exact
> failing environment, or a blocker is recorded.

In substance this is the gate carried in B-002 Section 6 and in
B-003 A7 + B7:

- "Original failing command" means the command that failed, not
  a substitute command, a reduced command, or a neighbouring
  command that happens to pass.
- "Selection proof" means evidence that the intended allocator
  or route was actually selected and used in that run, not merely
  that the process started, linked, or exited cleanly.
- "Exact pinned artifact" means the artifact identified by path,
  sha256, and BuildID where those are required, not a rebuild of
  approximately the same source and not an unpinned copy.
- "Exact failing environment" means the environment in which the
  failure occurred. A pass in a different environment is different
  evidence, for that other environment.
- "Or a blocker is recorded" means a named, evidenced blocker is
  entered in the record. Silence, absence of a result, and an
  unrun test are not a recorded blocker by themselves.

Two negative rules follow and are stated explicitly:

1. A trivial or hello-world pass does not close the Phase 6
   gate. A small program that was never the failing case cannot
   stand in for the original failing command.
2. A Pi pass is not tablet evidence. Evidence gathered on the
   Raspberry Pi (Mimir) does not transfer to, substitute for, or
   close a gate owed on a tablet or any other distinct device or
   environment, and tablet evidence would not transfer to the Pi
   either. Each environment owes its own run, its own selection
   proof, and its own pinned-artifact identification.

Status under this restatement: the Phase 6 gate is **not closed
by this slice (B-005) for any route** — not for R01, not for
R02, not for R03, and not for R04 in any of its sub-parts. This
section restates the gate so that Sections 7 and 8, and any
later resumption, apply the same gate and do not quietly weaken
it.

## 7. Verdict and resumption

### 7.1 Per-route blocks

Verdicts in this section use **only the verdict vocabulary of
B-001**. No verdict label from any other document is used as a
verdict here; other vocabularies are dealt with as an
inconsistency in Section 8 (I-B005-2), not adopted.

#### R01 — Inconclusive for unblocking; Device-tested probe / Blocked Mojo

- **Label / verdict (B-001 vocabulary):** Inconclusive for
  unblocking. In the fuller B-001 phrasing carried through
  B-002 and B-003: Device-tested probe / Blocked Mojo.
- **Evidence basis (one sentence):** R01 rests on a
  device-tested probe only, with no Mojo consumer of the route
  demonstrated, so the probe does not unblock a Mojo use of R01
  (see B-001 Sections 5–7, B-002 Section 7 and the R01 row of
  Section 3 of this document, and B-003 Part A).
- **Resumption requires:** a real Phase 1 packet passing the
  Integration Section 12 Step 0 gate — path + sha256 + BuildID
  pinned for the exact artifact to be consumed — followed by a
  Mojo consumer run against that pinned artifact, with selection
  proof, under the Phase 6 gate as restated in Section 6.

#### R02 — Inconclusive

- **Label / verdict (B-001 vocabulary):** Inconclusive.
- **Evidence basis (one sentence):** R02 has no result on a
  real, pinned target — its record is probe and comparator work
  without a named real target carried through a clean,
  checked build — so no unblocking or blocking verdict on a
  real target is earned (see B-001 Sections 5–7, B-003
  A8 / A9 / B1 / B9 and the R02 row of Section 3 of this
  document, and the B-003 Appendix A draft pointer for
  `/tmp/b003-route2.md`).
- **Resumption requires:** a pinned pristine named target;
  one-family-at-a-time clean rebuilds, with no family mixed
  into another family's rebuild; a per-build `ldd` / symbol
  check for every such rebuild; and a tracked harness — a
  harness held in the tree under version control, not an
  untracked or ad-hoc runner (see also I-B002-7, cited in
  Section 8).

#### R03 — Blocked

- **Label / verdict (B-001 vocabulary):** Blocked.
- **Evidence basis (one sentence):** R03 is recorded Blocked in
  B-001 Sections 1.3 / 9 on its stated evidence, and B-005
  carries that label without re-claiming the underlying work
  and without resolving the B-001 internal tension about the
  label noted in I-B005-3 (see B-001 Sections 5–7, B-003
  Part B and the R03 row of Section 3 of this document, and
  the B-003 Appendix A draft pointer for
  `/tmp/b003-route3.md`).
- **Resumption requires:** a real Phase 1 packet passing the
  Integration Section 12 Step 0 gate — path + sha256 + BuildID
  pinned for the exact artifact to be consumed — after which
  R03 is re-evaluated on new evidence, not reopened by the
  packet alone.

#### R04 (a / b / c, as one block) — TBD pending B-004

- **Label / verdict (B-001 vocabulary):** TBD pending B-004.
  This is **not** a verdict of Blocked, and no B-004 verdict
  is anticipated, inferred, or pre-written here.
- **Evidence basis (one sentence):** B-004 had not banked and
  was absent from `main` at synthesis time, so R04 sub-parts
  (a), (b) and (c) are owed evidence rather than decided
  routes, and they are therefore treated as one block in this
  synthesis (see Sections 3 and 4.4 of this document, the R04
  rows there marked TBD, and I-B005-5 below).
- **Resumption requires:** B-004 banking, followed by
  reconciliation of the R04 rows in Sections 3 / 4.4 by a
  superseding entry or slice (see Section 7.3); no other step
  substitutes for B-004 banking.

### 7.2 Overall Phase 2 verdict

Applying the B-001 Section 9 exit rule to the per-route blocks
in Section 7.1:

> **Phase 2 is NOT exited, and is honestly not-yet-exitable.**

The reasons, route by route, are exactly the Section 7.1
blocks and no others:

- R01: probe-only, no Mojo consumer — does not satisfy an exit
  that requires an unblocked, consumed route.
- R02: no real-target result — there is no real-target outcome
  to exit on.
- R03: Blocked.
- R04: owed — B-004 has not banked, so R04 cannot be counted
  for exit in either direction.

And, underlying all four routes:

- **Phase 1 remains open.** A Phase 2 exit cannot be honestly
  declared on top of an open Phase 1 whose packets the routes
  themselves require for resumption (Sections 7.1, R01 / R03).

The resting state recorded by this slice is therefore stated
in full as:

> **Phase 2: evaluated, honestly Blocked / not-yet-exitable.**

That phrase — "evaluated, honestly Blocked / not-yet-exitable"
— is the resting state this slice records. It is a description
of where the evidence leaves Phase 2 after evaluation, not a
claim that Phase 2 has exited, and not a claim that it can
exit in its present state. See I-B005-4 in Section 8 for why
this naming was needed at all.

### 7.3 Banking-order note

- B-005 banks **after** B-004 in banking order, even though
  this synthesis was drafted while B-004 was still absent from
  `main` (see I-B005-5).
- When B-004 banks, the R04 rows in Sections 3 and 4.4 of this
  document are to be reconciled **by a superseding entry or
  slice**, recording what B-004 actually decided and citing
  where it banked.
- They are **never** to be reconciled by silently editing this
  verdict — not the R04 TBD labels in Section 7.1, not the
  overall verdict in Section 7.2, and not the TBD fields in
  Sections 3 / 4.4. A silent edit would destroy the record of
  what was and was not known at synthesis time, which is part
  of what this slice exists to preserve.

## 8. Inconsistencies recorded

Inconsistencies are recorded here, not silently resolved.
Inherited items are **cited, not re-claimed**: B-005 does not
present another slice's inconsistency findings as its own new
findings, and does not re-litigate them.

### 8.1 Inherited, cited not re-claimed

- **From B-001 Section 10: INC-1 through INC-11.** Cited as a
  set. Two are singled out because they bear directly on this
  synthesis:
  - **INC-3:** the comparator set naming a not-yet-existing
    MimirAlloc and omitting jemalloc — the comparator set as
    named does not match the comparators that actually exist
    and were available to be run.
  - **INC-10:** support versus failure — the record contains
    both a support claim and a failure result bearing on the
    same matter, unreconciled in B-001.
- **From B-002: I-B002-1 through I-B002-9.** Cited as a set.
  Three are singled out because they bear directly on this
  synthesis:
  - **I-B002-3:** the jemalloc paradox — jemalloc figures in
    the record in ways that do not sit consistently together
    (compare INC-3 above on its omission from the named
    comparator set).
  - **I-B002-7:** no tracked harness — results rest in part
    on a harness that is not tracked in the tree; this is
    the same defect Section 7.1 (R02) makes a resumption
    requirement.
  - **I-B002-9:** cited as recorded in B-002, in substance as
    there stated; B-005 relies on it as a cited item and does
    not restate it as a new finding.
- **From B-003: I-B003-1 through I-B003-10.** Cited as a set.
  Several of the new I-B005 items below are explicitly derived
  from B-003 items (I-B003-1, I-B003-3, I-B003-4, I-B003-5 and
  I-B003-8); the derivation is named in each case so a reader
  can trace it, and the B-003 item itself remains B-003's.

### 8.2 New inconsistencies recorded by B-005

#### I-B005-1 — Exit-table shape divergence

B-002 Section 8 uses a vertical **Field | Value** table shape.
B-001 Section 9 and B-003 Section 8 use a wide exit-table
shape. The two shapes do not line up field-for-field, so a
mechanical merge of the tables would lose or misplace fields.
**B-005 follows the wide shape** (B-001 Section 9 / B-003
Section 8), and flags its deviation on consumer / layer in
Section 3 of this document. Derived from I-B003-1 and
I-B003-5. Recorded, not resolved: the banked B-002 table is
not rewritten.

#### I-B005-2 — Three unreconciled verdict vocabularies

Three verdict vocabularies are in circulation in the Phase 2
record and have never been reconciled with each other:

| Source | Vocabulary, as used there |
|---|---|
| B-001 | Unblocks / … / Blocked (the B-001 verdict set, including Inconclusive and TBD as used in Section 7.1) |
| Integration document | VIABLE / VIABLE WITH OWNERSHIP DEFECT / … (the Integration verdict set) |
| B-002 | Mixed — B-002 uses labels drawn from more than one of the above sets in different places |

A reader mapping between them has to guess. The mapping a
reader is most likely to assume is recorded here only as an
aid, **not** as an adopted equivalence:

| B-001 (adopted here) | Integration | B-002 (mixed) |
|---|---|---|
| Unblocks | ≈ VIABLE | varies by row |
| Unblocks, with a defect carried | ≈ VIABLE WITH OWNERSHIP DEFECT | varies by row |
| Inconclusive | ≈ no Integration equivalent stated | varies by row |
| Blocked | ≈ not viable as recorded there | varies by row |
| TBD | ≈ not yet assessed | varies by row |

**B-005 adopts the B-001 vocabulary** for all of its verdicts
(Section 7), and **does not silently resolve** the three-way
divergence: the `≈` rows above are approximate reader aids,
not declared equivalences, and no other document is edited by
this slice to conform. Derived from I-B003-3.

#### I-B005-3 — B-001 self-contradiction on the R03 label

B-001 contradicts itself on R03: Sections 1.3 / 9 record R03
as **Blocked**, while Section 7 states the per-row rule
"Not assigned, never inferred" for such labels. Both statements
stand in the banked B-001. B-005 carries the Sections 1.3 / 9
label (Blocked) in Section 7.1 because that is the label the
exit rule in Section 9 itself uses, and records the
contradiction here **unresolved**: B-005 does not decide which
B-001 statement governs B-001, and does not edit B-001.
Derived from I-B003-4.

#### I-B005-4 — No defined "evaluated, honestly Blocked" resting state

Phase 2 exit, as literally specified, defines an exited state
and the conditions for it, but it has **no defined resting
state for a Phase 2 that has been evaluated and is honestly
Blocked / not-yet-exitable while Phase 1 remains open**. There
is no named box in the literal specification for "we did the
evaluation, the answer is no, and here is the evidenced
record of that no." This slice names its verdict in
Section 7.2 — **not-yet-exitable** — as that state. That is a
**naming, not a roadmap amendment**: B-005 does not change
the exit conditions, does not add an exit path, and does not
claim the roadmap authorised the name. Derived from
I-B003-8.

#### I-B005-5 — B-004 absent at synthesis time: one route owed

B-004 was absent from `main` at synthesis time. That forces a
synthesis written with **one route owed**: R04 (a / b / c)
could only be carried as TBD (Section 7.1). Consequently the
completeness of the exit table in this document is
**structural, not evidential**:

- **Structural completeness:** every route is present, every
  row and field exists, and no route is silently omitted.
- **Not evidential completeness:** the R04 fields are TBD —
  there is no R04 evidence behind them yet.

**Readers must not mistake a complete-looking table for a
completed Phase 2.** A table with all its rows present and
one route's fields marked TBD is a record of work owed, not
a record of work done. See also Section 7.3 on how the R04
rows are to be reconciled once B-004 banks.

#### I-B005-6 — Stale banking banner in the banked B-002

The B-002 header banner — in substance, "B-001 has NOT banked
… fields were NOT used" — is **stale in-tree**: it describes a
banking state that no longer held once B-001 banked, yet it
remains, unamended, in the banked B-002 document that B-005
synthesises from (see I-B003-1). The general lesson is
recorded plainly:

> Quoted banners in banked slices cannot be trusted as
> current banking state. **WAVE_PLAN and `git log` govern**
> banking state; a banner inside a banked document is a
> statement about the moment that document was written, at
> best, and may already have been overtaken when it banked.

B-005 therefore took banking order and banking state from
WAVE_PLAN / `git log`, not from in-document banners, wherever
the two could differ — including for the B-004 absence
recorded in I-B005-5 and the banking-order note in
Section 7.3.

### 8.3 Check made, no new inconsistency found

A check was made of the roadmap Section 11 versus the
assignment wording for this slice: the assignment says
"Section 11 principles" for the measured-results protocol
and "roadmap Phase 2 (Section 8)" for the Phase 2 exit.
Those two references are **consistent** — Section 11 is the
principles section the protocol rests on, and Section 8 is
the roadmap's Phase 2 section; they refer to different
sections because they refer to different things, and neither
reference contradicts the roadmap as written. **No new
inconsistency is recorded on this point**; the check itself
is recorded so a later reader does not have to wonder
whether it was made.

## Appendix A. Source index

Every filled cell and claim in this document is traceable to
a source below. Pointer-only cells (TBD, owed, not run) trace
to the item that records them as owed, not to evidence for a
result.

| Claim / cell in this document | Source |
|---|---|
| Route definitions, scope and route labels R01–R04 | B-001 Section 3 |
| Route evidence, per-route results and failures | B-001 Section 5; B-001 Section 6 |
| Per-route verdict material and the "Not assigned, never inferred" rule | B-001 Section 7 (see also I-B005-3) |
| Comparator and measured-results material | B-001 Section 8; B-002 Section 7 |
| Phase 2 exit rule applied in Section 7.2 | B-001 Section 9 |
| Exit-table wide shape followed by B-005 | B-001 Section 9; B-003 Section 8 (contrast B-002 Section 8; see I-B005-1) |
| R01 material, device-tested probe record | B-002 Section 7; B-002 Section 8 |
| B-002 artifact identity, Appendix A sha256 | B-002 Appendix A (sha256 as there recorded) |
| Phase 6 gate restatement, Section 6 | B-002 Section 6; B-003 A7 + B7, in substance |
| R02 synthesis material | B-003 A8; B-003 A9; B-003 B1; B-003 B9 |
| R03 synthesis material | B-003 Part B (see the draft pointer below) |
| B-003 source and draft pointers generally | B-003 Appendix A |
| R02 working draft behind B-003 | `/tmp/b003-route2.md`, sha256 `3058576034cae2b735fd40d3388bc049b68a83f7d3b6bc263d26b458c4c29e42` |
| R03 working draft behind B-003 | `/tmp/b003-route3.md`, sha256 `98873c765feade85c09f62d8ba0b7507fc86aa9d49259a4207b9f73337fe801a` |
| Evidence labels quoted or relied on | EVIDENCE_LEDGER labels, as labelled there |
| Inherited inconsistencies INC-1 … INC-11 | B-001 Section 10 |
| Inherited inconsistencies I-B002-1 … I-B002-9 | B-002, as there recorded |
| Inherited inconsistencies I-B003-1 … I-B003-10 | B-003, as there recorded |
| Banking order and banking state (incl. B-004 absence) | WAVE_PLAN; `git log` (see I-B005-6) |

**Note on the `/tmp` drafts:** `/tmp/b003-route2.md` and
`/tmp/b003-route3.md` are **working records, not banked
artifacts**. They are cited by path and sha256 so the chain
from working draft to B-003 to this synthesis can be checked,
but no claim in this document rests on a `/tmp` draft alone
where a banked source exists, and the drafts' presence in
`/tmp` gives them no banked status.

## Appendix B. Acceptance mapping

| Acceptance item | How this document stands to it |
|---|---|
| Roadmap Section 8 — Phase 2 exit | This document supplies the **synthesis table and the measured-results protocol** owed for Phase 2. It does **NOT exit Phase 2**: the verdict recorded in Section 7.2 is NOT exited / not-yet-exitable, Phase 1 remains open, and R04 remains owed pending B-004. |
| Roadmap Section 11 — principles | Section 5 of this document states the protocol in accordance with the Section 11 principles, and **Section 5.6** applies those principles retrospectively to the Phase 2 record, recording where the record meets them and where it does not. |
| Roadmap Section 15 — Q2 | Touched by this synthesis where the route evidence bears on it; **not closed** by this slice. |
| Roadmap Section 15 — Q3 | Touched by this synthesis where the route evidence bears on it; **not closed** by this slice. |
| Roadmap Section 15 — Q4 | Touched by this synthesis where the route evidence bears on it; **not closed** by this slice. |
| Roadmap Section 15 — Q10 | Touched by this synthesis where the route evidence bears on it; **not closed** by this slice. |
| Roadmap Section 15 — Q12 | Project licence: **MIT, settled** for the project itself. Comparator licences: **TO-VERIFY** — no comparator licence is claimed as settled by this slice, and none is to be inferred from the project's MIT licence. |

"Touched" in the Q2 / Q3 / Q4 / Q10 rows means the question is
engaged by evidence or reasoning in this synthesis. It does
not mean answered to closure, and no Section 15 question is
closed by B-005.

## Change log

| Date | Slice | Change |
|---|---|---|
| 2026-11-06 | B-005 | Initial synthesis: Phase 2 exit table and measured-results protocol, including the Section 6 Phase 6 gate restatement, the Section 7 per-route and overall verdicts (Phase 2 NOT exited, not-yet-exitable), the Section 8 inconsistencies (inherited cited, I-B005-1 … I-B005-6 new), and Appendices A–B. R04 carried as TBD pending B-004, to be reconciled by a superseding entry or slice after B-004 banks (Section 7.3). |
