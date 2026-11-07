# Phase 6 - Exit Evidence Packet

Slice: **F-009 (team-09)**. Wave 6, Phase 6 exit evidence
packet - the last Wave 6 slice. Synthesis plus one fresh
recorded run: this slice adds this packet and its recorded
run (`tests/recorded_run_f009.txt`); it changes no
implementation, no test, and no Makefile line (`make test`
composition is unchanged; no Makefile change at all).
Packet base: **35211e1792c131c0524488624fd0e9b7176eaf6a**
(banked main: F-001..F-008; Wave 6 = 8 of 9 at this base)
on branch `yrsa/mimiralloc-09`, worktree
`/home/pi/mimiralloc-team-09` on Raspberry Pi
`purpleviking1`. Host for every run cited: ARM64 Linux +
glibc - `Linux purpleviking1 6.12.112-v8-16k-hailo+
#1 SMP PREEMPT Sat Oct 10 09:05:25 EDT 2026 aarch64
GNU/Linux`, runtime page size **16384** (`getconf
PAGESIZE`), compiler `cc (Debian 12.2.0-14+deb12u1)
12.2.0`, glibc 2.36, Mojo **1.1.0 (8189361e)** via the
`/usr/local/bin/mojo` pixi wrapper, used exactly as it
stands. Governing text: Roadmap Section 8, Phase 6,
decomposed in `docs/PHASE_6_FOUNDATION_PLAN.md` (F-001)
into tasks T1-T7 and exit elements X1-X5, read with
Roadmap Sections 2, 3.1-3.3, 5 and 8 Phase 1. Modelled
on the banked Phase 5 packet `docs/PHASE_5_EXIT_EVIDENCE.md`
(E-009). `README.md` was not touched by this slice
(verified: sha256 `bd7f6614` before and after the fresh
run, and identical to `git show HEAD:README.md`).
`include/mimir_alloc.h` is likewise byte-identical to
base (sha256 `55107f05`); the dynamic ABI is exactly the
four public symbols, re-proven in the fresh run (S7).

> **EXIT VERDICT - Phase 6 exit is MET via the
> blocker-record alternative (exit element X4), and
> UNMET via the success branch (exit element X1), in
> exactly the scope stated here and no wider.**
> The Roadmap Phase 6 exit sentence (Section 8, Phase 6)
> is, verbatim: *"original failing command succeeds on
> the affected ARM environment with explicit proof of
> allocator use and no new correctness failures, or
> record a blocker with exact results. A successful
> unrelated Mojo hello-world does not close this
> milestone."* The sentence is disjunctive, and this
> packet maps each clause before returning a verdict:
>
> - **Clause A - "original failing command succeeds on
>   the affected ARM environment with explicit proof of
>   allocator use and no new correctness failures":
>   UNMET.** Exit element X1 is BLOCKED (F-008's blocker
>   record, restated in full in Section 6): the original
>   failing command was never recorded exactly, the
>   affected environment (Samsung SM-X400, Android/
>   Termux glibc) is absent from this Pi, both failing
>   Mojo versions (1.2.0, 1.3.0.dev2026110205) are
>   absent, and no boundary reaches the Mojo compiler
>   process in which the failure occurred. The tablet
>   failure is **not fixed and not reproduced**; no
>   sentence in this packet claims otherwise.
> - **Clause B - "or record a blocker with exact
>   results": MET.** Exit element X4 is satisfied by
>   F-008 (`docs/PHASE_6_ORIGINAL_FAILURE_RETRY_F008.md`
>   plus `tests/mojo/recorded_run_f008.txt`, 215 lines,
>   sha256 (as recorded by that slice):
>   `df97ce6f8be18ada5a810317e6fe43269dc5cc2cce425d3d38e7f289d9acca33`):
>   the blocker is stated precisely, its three legs are
>   each proven from the repo or the Pi, the closest
>   honest retry was run with exact commands, outputs
>   and exit codes (Pi analogue R1-R4, all exit 0; VA
>   probe at `1 << 39` OK on the Pi against ENOMEM on
>   the tablet), and the resumption point is named.
>   This packet assembles that record with every other
>   Wave 6 slice's exact results (Sections 3-5).
> - **Exclusion - "A successful unrelated Mojo
>   hello-world does not close this milestone":
>   RESPECTED.** Element X5: no minimal-program success
>   anywhere in Wave 6 is counted toward X1 (F-001
>   recorded the refusal in advance; F-004, F-005, F-006
>   and F-008 each restated it). The verdict above rests
>   on Clause B alone.
>
> **The exact scope of the MET claim:** a **Pi-native**
> Phase 6 exit by the blocker door. What Wave 6 proved
> positively, the verdict also carries, each in its own
> scope: the demonstrated boundary (B2+B1) is wired by
> documented build configuration and proven (F-003);
> explicit proof of allocator use exists **for a
> generated Mojo program's own explicit `mimir_*` calls
> only**, at binding grade (F-005; X2, Section 4) - it
> reaches neither the Mojo compiler's, the stdlib's, nor
> the runtime's internal allocations; the repeated-
> compile same-revision comparison passed with no new
> failures observed in its legs (F-006); and the
> upstream-test element is **PARTIAL** (F-007: 112 tests
> run, 112 passed, in 4 files of a 12-file relevant
> subset; 8 files compile-blocked by version skew; the
> WITH leg is one-sided), so X3 is **PARTIAL, not MET**,
> in the exact bounded words of Section 4. **Carried
> debts this exit does not discharge:** Phase 1's exit
> remains **unmet** (Section 6.3), Phase 4 exit remains
> **UNMET** and Phase 3 exit remains **UNMET** (E-009
> carried-owed register), and Phase 5's exit stands MET
> in Pi-native scope only. A Phase 6 exit claim quoted
> without this scope paragraph is incomplete.

## 1. Verdict vocabulary and reading rules

- **MET** - the evidence the item specifies exists, in
  the form it specifies, in the banked repo (or, for the
  fresh-run items, in this packet's recorded run).
- **UNMET** - some evidence exists, but the item as
  written is not satisfied.
- **PARTIAL** - one named part of the item is met and
  another named part is discharged only in a weaker,
  explicitly recorded sense, or is owed.
- **BLOCKED** - the item cannot be executed in this
  venue at all; the row names the exact blocking facts
  and the resumption point. (E-009 needed no BLOCKED
  grade; F-001's matrix pre-defined it for X1.)
- **OWED** - the evidence is wholly absent at this base;
  the row names exactly what is missing.
- Counts are copied from the recorded-run files named
  in each row, never restated from memory or from
  another slice's summary. Where the fresh run in
  Section 2 reproduces a banked count, both are cited.
- Every Wave 6 claim is single-target (Pi-native
  AArch64 Linux/glibc, Mojo 1.1.0) unless a row says
  otherwise. No target is promoted to "supported"
  (Roadmap Section 5 sense) by anything in this packet.
- Contract mappings for the namespaced `mimir_*`
  surface remain by analogy only, under U-03, exactly
  as in every prior wave. Nothing here is an ABI
  freeze: the public header still declares the surface
  Proposed, not stable.

## 2. Fresh verification run for this packet (F-009)

Run once on the Pi in `/home/pi/mimiralloc-team-09` at
base `35211e1` on 2026-11-06 (22:59-23:0x EST), from a
clean build, sequentially, load kept light. Raw log:
`tests/recorded_run_f009.txt` (1,201 lines, 76,872
bytes, sha256
`599fc5c4684b79128ad890f64743560f1cc0abaab2adf889d3c8ecf74783055b`),
sections S0-S7. The recorded run lives
directly under `tests/`, following the assignment's
example path and Wave 4's D-009 placement, while every
other Wave 6 recorded run lives under `tests/mojo/` -
the placement seam is recorded as INC-F009-03.
`free -g` before and after: 15 GiB total, 13 GiB
available; Samtal answered HTTP 200 on port 8787 before
and after - not starved.

| Section | Command | Exit | Headline result (as recorded) |
|---|---|---|---|
| S1 | `make clean` | 0 | build tree removed |
| S1 | `make test` | 0 | all 8 binaries `RESULT: PASS`; per-binary CHECKS 29 / 70 / 95 / 23 / 122 / (c007 suite PASS) / 42 run 0 failed 7 skipped-not-yet / final binary (test_d004) `CHECKS: 4590 run, 0 failed`, `RESULT: PASS` |
| S2 | `make test-f003` | 0 | `F-003 RESULT: PASS (4/4 steps)` |
| S3 | `make test-f004-mojo` | 0 | `F004 RESULT: PASS (count=192 sum=72096 weighted=7130112)` |
| S4 | `make test-f005` | 0 | dynamic leg `CHECKS: 18 run, 0 failed`, `F-005 DYNAMIC LEG RESULT: PASS`; audit leg `CHECKS: 24 run, 0 failed`, `F-005 AUDIT LEG RESULT: PASS` |
| S5 | `make test-f006` | 0 | `WITHOUT SUMMARY: builds_passed:5/5 runs_passed:5/5`; `WITH SUMMARY: builds_passed:5/5 runs_passed:5/5`; exactly **4** binding lines; 1 distinct binary hash per leg; `F-006 HARNESS RESULT: PASS` |
| S6 | `make test-f007` | 0 | `F-007 PROBE RESULT: COMPLETE`; probe re-run of `test_bit.mojo`: `22 tests run: 22 passed, 0 failed`; presence verdicts as banked (env suite ABSENT, `mojo test` ABSENT, lit ABSENT, built binary ABSENT) |
| S7 | `nm -D --defined-only build/libmimiralloc.so` | - | exactly **4** defined dynamic symbols: `mimir_malloc`, `mimir_free`, `mimir_calloc`, `mimir_realloc`; README/header sha256 unchanged after the run |

Two corroborations the fresh run adds, stated at
observation strength only (INC-F009-07): the S5 binary
hashes are identical to the hashes F-006 banked at its
own base (WITHOUT:
`197e68c2e4a82ac1ded8b396e2980d64bcd19aedff29c59dc2ed64387dc378d6`;
WITH:
`6a1df97f7c09c9ee288cfa4b4594ec2d92589957dabeda4014598be1497bdac2`),
so
both legs reproduce byte-identically across bases at
Mojo 1.1.0 on this Pi; and every Phase 6 standalone
target that exists at this base (test-f003,
test-f004-mojo, test-f005, test-f006, test-f007) exits 0
at the packet base, with the banked headline counts
reproduced exactly.

## 3. Banked evidence inventory (F-001..F-008)

All eight slices banked on main in roadmap order; this
packet's base `35211e1` is F-008's banked commit.

| Slice | Banked commit | Deliverable | One-line result |
|---|---|---|---|
| F-001 | `491ecf1` | `docs/PHASE_6_FOUNDATION_PLAN.md` | Foundation plan + exit-evidence matrix (T1-T7, X1-X5); X1 BLOCKED on the Pi via the X4 door; INC-F001-01..12 |
| F-002 | `c6d3672` | `docs/PHASE_6_MOJO_TOOLCHAIN_READINESS.md` | Mojo 1.1.0 PRESENT via pixi and PROVEN running; acquisition road + licence posture recorded; installs NOT RUN (reserved for Volmarr's word); INC-F002-01..10 |
| F-003 | `b876574` | `docs/PHASE_6_BOUNDARY_WIRING_F003.md`, `tests/mojo/test_f003.mojo`, `tests/mojo/recorded_run_f003.txt`, Makefile `test-f003` | B2+B1 boundary PROVEN at verdict+linkage grade: 4/4 steps PASS, `DT_NEEDED [libmimiralloc.so]`, control build without the link fails; INC-F003-01..06 |
| F-004 | `08433ae` | `docs/PHASE_6_MINIMAL_MOJO_RUN_F004.md`, `tests/mojo/test_f004.mojo`, `tests/mojo/recorded_run_f004.txt`, Makefile `test-f004-mojo` | Minimal program WITHOUT leg PROVEN (3/3 builds + 3/3 runs identical PASS, byte-identical binaries); WITH leg OWED at slice time, closed later by join (Section 5, T3; INC-F009-06); INC-F004-01..05 |
| F-005 | `63c2dc1` | `docs/PHASE_6_ALLOCATOR_USE_PROOF_F005.md`, `tests/mojo/test_f005.mojo`, `tests/mojo/test_f005_audit.mojo`, probe/audit sources, `tests/mojo/recorded_run_f005.txt`, Makefile `test-f005` | Allocator-use proof at **binding grade**: exactly 4 `LD_DEBUG` bindings; dynamic leg 18/18 checks with exact mapped-byte deltas; static audit leg 24/24 checks, finals call_malloc 6, call_calloc 4, call_realloc 4, call_free 9, published 10, retired 10, live 0; INC-F003-05 CLOSED; INC-F005-01..05 |
| F-006 | `38b27b0` | `docs/PHASE_6_REPEATED_COMPILE_COMPARISON_F006.md`, `tests/mojo/run_f006_compare.sh`, `tests/mojo/recorded_run_f006.txt`, Makefile `test-f006` | Repeated-compile comparison: WITHOUT 5/5 builds + 5/5 runs PASS, WITH 5/5 builds + 5/5 runs PASS (18/18 checks every repetition), binaries byte-identical per leg, no new failures observed; INC-F006-01..05 |
| F-007 | `789a7f0` | `docs/PHASE_6_UPSTREAM_TESTS_F007.md`, `tests/mojo/recorded_run_f007.txt`, `tests/mojo/run_f007_upstream_probe.sh`, Makefile `test-f007` | Upstream tests PARTIALLY EXECUTED: suite ratified at slice level before running; 12-file relevant subset - 4 files ran, **112 tests run, 112 passed, 0 failed**; 8 files compile-blocked by version skew only; lit/C++ suites unrunnable; WITH leg one-sided; INC-F007-01..06 |
| F-008 | `35211e1` | `docs/PHASE_6_ORIGINAL_FAILURE_RETRY_F008.md`, `tests/mojo/recorded_run_f008.txt` | X1 blocker record with exact results: three-leg blocker proven; Pi analogue R1-R4 all exit 0; VA probe `2^39` OK on Pi vs ENOMEM on tablet; no Makefile wiring, by decision; INC-F008-01..08 |

## 4. Exit gate, item by item (F-001 X1-X5)

| # | Exit element (F-001 Section 7) | Verdict | Evidence |
|---|---|---|---|
| X1 | The original failing command succeeds on the affected ARM environment | **UNMET (BLOCKED)** | F-008 blocker record (Section 6 of this packet restates both blocker legs in full, per INC-F008-08). No literal retry exists to run: no recorded command, no affected environment on the Pi, no failing version on the Pi, and no compiler-process boundary |
| X2 | Explicit proof of allocator use | **MET, in B2+B1 scope only** | F-003 (boundary demonstrated, 4/4 steps) + F-005 (binding grade: exactly 4 `LD_DEBUG` binding lines to this tree's `libmimiralloc.so`; dynamic leg 18/18 with implementation-only mapped-byte effects; static leg 24/24 with the six internal audit counters moving exactly, libc bypass moving none). Scope: a generated program's **own explicit** `mimir_*` calls. Nothing here proves servicing for the compiler's, stdlib's, or runtime's allocations, and X2's MET grade is an evidence-inventory grade inside that scope - it does **not** satisfy Clause A of the exit sentence, which additionally requires the original command itself to succeed (see INC-F009-01) |
| X3 | No new correctness failures | **PARTIAL** | F-007 Section 6 fixes the exact words, quoted verbatim: *"no failures observed in the F-004/F-006 runs, in this slice's re-run of the F-003/F-005 programs, or in the 112 relevant upstream stdlib tests executed at this base; the remainder of the relevant upstream subset is compile-blocked by version skew and the lit/C++ suites are unrunnable here, so X3 is PARTIAL, not MET."* F-009's own addition, on its own evidence only: the fresh run at this base (Section 2) observed no failure in `make test` or in any Phase 6 target. The WITH side of the upstream record remains one-sided (INC-F007-04), so the grade does not rise |
| X4 | OR a blocker is recorded with exact results | **MET** | F-008's record + its recorded run (exact commands, outputs, exit codes for the Pi analogue R1-R4 and the VA probe; absence checks for the environment and both failing versions; field-note facts re-verified from the repo at named lines), assembled by this packet. This is the branch the exit verdict rests on |
| X5 | Exclusion: an unrelated Mojo hello-world does not close the milestone | **Guard RESPECTED** | F-001 Section 7 recorded the refusal in advance; F-004's program is in any case not a bare hello-world (192-entry growable-list computation with checksums), and no slice counts any minimal-program success toward X1 |

**Tally for the exit sentence:** Clause A UNMET
(X1 BLOCKED; X2 and X3, which belong to Clause A's
wording, stand at MET-in-scope and PARTIAL respectively
and cannot complete it without X1); Clause B MET (X4);
exclusion respected (X5). Because the sentence joins
the clauses with "or", the exit **is met** - by Clause
B. INC-F009-01 records the one stricter reading under
which a reader could grade it otherwise, openly.

## 5. Task map, item by item (F-001 T1-T7)

| # | Roadmap Phase 6 task | Verdict | Evidence |
|---|---|---|---|
| T1 | Wire the demonstrated build or runtime boundary using documented build configuration | **MET** | F-003: `mojo build` with documented `-Xlinker` flags places `libmimiralloc.so` on the generated executable's link (B2); the program calls the four symbols explicitly via `std.ffi external_call` (B1); control build without the link fails with undefined references to all four symbols. Fresh run S2 reproduces 4/4 PASS at this base. B3/B4/B7 (new standard-name surface) remain untaken future decisions; B5 (Mojo allocator hook) remains absent in the banked sources; B6 (compiler-process substitution) remains BLOCKED - none of these was T1's demonstrated boundary |
| T2 | Rebuild from clean sources, or use the appropriate published package only where legally and technically possible | **MET as a readiness record, with the licence caveat carried** | F-002: the installed published package (Mojo 1.1.0, pixi/conda road) is the package Phase 6 used, PROVEN running on this Pi; the source-build road is BLOCKED as observed (recorded Bazel failure, ~3.8 h elapsed); acquisition road, sizes and licence posture (`LicenseRef-Modular-Proprietary`) recorded; the optional 1.2.0 upgrade was NOT RUN, reserved for Volmarr's word. The roadmap's "legally possible" condition is recorded but not evaluated (INC-F001-12 / INC-F002-09, carried as INC-F009-05) |
| T3 | Run a minimal Mojo program | **MET** | F-004 WITHOUT leg PROVEN (3/3 + 3/3, identical PASS). The WITH leg, OWED at F-004's base because F-003 banked after it, was closed by the join F-004 itself defined: F-005 Section 5 records the keeper's re-run of both targets on merged main at `08433ae` (both exit 0, labelled keeper evidence) and F-005's own re-run on the same base (`make test-f003` exit 0, 4/4 PASS; `make test-f004-mojo` exit 0, PASS). This packet's fresh run repeats both on one revision at the packet base (S2, S3, both exit 0) |
| T4 | Compile a representative program repeatedly | **MET** | F-006: 5 builds + 5 runs per leg on one revision, all PASS, binaries byte-identical per leg. The "representative program" is a banked pair, not one source in two forms (INC-F006-01, carried in Section 8). Fresh run S5 reproduces the full series at this base with identical hashes |
| T5 | Execute relevant upstream tests | **PARTIAL** | F-007: 112/112 passed in the 4 runnable files of the ratified 12-file relevant subset; 8 files compile-blocked by 1.2.0.dev-vs-1.1.0 skew only (no executed test failed); C2 lit and C3 C++ suites unrunnable here (no lit, no same-source built toolchain); C4 MAX tests and C5 package-shipped tests absent. WITH leg one-sided by boundary scope (Section 7 carries the ratification note per INC-F007-01) |
| T6 | Compare against the same revision without the replacement | **MET at F-006's bounded strength** | F-006: same banked revision (`63c2dc1`), same Mojo revision (1.1.0) on both legs; the legs differ exactly where the boundary says they must (`DT_NEEDED`, symbol references, servicing proof inside every WITH run) and nowhere else; stdout is never byte-compared across legs by design (INC-F006-01). For upstream tests the comparison dimension is one-sided and stays F-007's record, not merged into this one (INC-F007-04) |
| T7 | Capture proof MimirAlloc actually serviced allocations | **MET** | F-005, binding grade, two legs (see X2 row). The proof method is E-004's loader-binding shape plus E-005's two-leg shape carried to the Mojo path, exactly as F-001's matrix prescribed; INC-F003-05 (F-003's weaker grade) is CLOSED by it |

**Task tally:** MET 5 (T1, T3, T4, T6, T7), MET with
carried caveat 1 (T2), PARTIAL 1 (T5). No task is OWED
or BLOCKED as a whole; the blocked sub-items (B6, the
source-build road, the skew-blocked upstream remainder,
the lit/C++ suites) are named in their rows and in
Section 8.

## 6. The blocker, restated in full (INC-F008-08)

INC-F008-08 requires this packet to restate **both**
blocker legs from F-008 and Phase 1's exit position.
Nothing below is summarised into softness; the full
record is F-008's file and recorded run.

### 6.1 First leg - the literal retry cannot be run

Three independent facts block it; any one suffices
(F-008 Section 5):

1. **No command to retry.** The banked record holds a
   faithful prose *account* (field note 03, line 5 and
   line 11; field note 01's ledger rows) and no argv,
   working directory, environment, exit code/signal, or
   stderr log. A-005 inconsistency 1, A-006's A6-10 and
   A-007 truth (ii) concur: no recorded Mojo failure
   command, revision, or failure log exists anywhere in
   the repository, and the only failure packet in
   `evidence/runs/` is the synthetic smoke case.
   Inventing an argv and calling it the original would
   be fabrication.
2. **No affected environment on the Pi.** The failure
   happened on a Samsung SM-X400 (Android 16) in a
   Termux glibc userspace with a 39-bit user-VA ceiling
   and 4,096-byte pages. On this Pi: the Termux paths
   are absent (recorded absence checks, exit 2/1/2);
   the ELF interpreter `/lib/ld-linux-aarch64.so.1` -
   the path whose absence was the tablet's step-1
   failure - is **present**; the VA probe at `1 << 39`
   **succeeds** here where the tablet returned ENOMEM;
   the page size is 16384 against 4096 there.
3. **No failing version on the Pi.** The tablet failed
   on Mojo **1.2.0** and nightly
   **1.3.0.dev2026110205**; the only Mojo on this Pi is
   **1.1.0 (8189361e)** (conda-meta holds only 1.1.0
   records; the artifact search is empty). Installing a
   failing version waits on Volmarr's word (F-002) and
   would still land in the wrong environment for a
   command that does not exist.

The closest honest retry was run (F-008 Section 3):
R1 `mojo --version`, R2 the compiler binary directly,
R3 the explicit glibc-loader invocation mirroring the
tablet's aborting step, R4 R1 repeated - **all exit 0**,
output `Mojo 1.1.0 (8189361e)` each time. The tablet
failure does not reproduce here, and per the X5
exclusion that success is not X1 and is claimed as
nothing more than the exact results attached to the
blocker.

### 6.2 Second leg - no compiler-process boundary

Even with all three facts above cured, X1's full
reading asks for the command to succeed *with
MimirAlloc wired*. The process that aborted is the Mojo
**compiler itself**. F-001's boundary B6 (substituting
the allocator inside the compiler process) is rated
**absent / BLOCKED**: the compiler is a prebuilt,
stripped binary whose runtime library defines a
TCMalloc-derived internal surface, and MimirAlloc's
whole surface is four namespaced symbols with no
standard-name provider (B3/B4/B7 each require a new
surface that does not exist, under a future contract
decision). The boundary F-003 proved (B2+B1) wires a
*generated program's own explicit* calls; it does not
reach the compiler process. X1 is therefore blocked on
the wiring question as well as on the retry question
(INC-F008-05).

**Resumption point (F-008 Section 5, exact):** the
literal retry becomes runnable when (a) the SM-X400
tablet environment, or an equivalent 39-bit-VA
Android/Termux glibc environment ratified as
equivalent, is brought into scope **by Volmarr's
word**, with (b) Mojo 1.2.0 or the 1.3.0.dev2026110205
nightly installed there as they were for the field
work, and (c) the invocation captured through the
A-005 harness so the packet Phase 1 never produced
finally exists.

### 6.3 Phase 1's exit position, restated

**Phase 1 exit remains unmet.** Roadmap Section 8
Phase 1 exit: *"documented causal hypothesis and a
repeatable validation command; include conflicting
evidence and exact unknowns."* The banked record holds
a causal *account* at probable strength at best (A-006's
vocabulary caps the address-space-configuration shape
at *probable* on a single packet, and no packet exists),
and it holds **no repeatable validation command**,
because the original command was never recorded
(Section 6.1, leg 1). A-005 additionally recorded that
**three differing Phase 1 exit gates are unreconciled**
(its inconsistency 5): Section 3.3's gate (someone else
can reproduce the failure and explain the integration
point), Section 8 Phase 1's exit (above), and Section
17's definition of done (items 1 and 5) state three
different bars, and which one governs Phase 1 is not
settled in the roadmap. This X4 blocker record is
Phase 6's conformant outcome; it does not cure,
discharge, or soften Phase 1's position, and Wave 6
proceeding never did (INC-F001-09, carried by
INC-F008-08 to here).

## 7. Suite-ratification note (INC-F007-01)

INC-F007-01 requires this packet to carry the
ratification explicitly. It is carried here, verbatim
in substance: **the Roadmap names no suite, revision,
or location for T5** (INC-F001-08). F-007 ratified the
suite **at slice level, before running**: the
modular/mojo stdlib tests (`Mojo/stdlib/test/**`) from
the operator-local checkout
`/home/pi/mojo-build/mojo-src` at
`24f4ceb2ff1701aaca81dc974b7b4794d7d2a0a5`
(Mojo 1.2.0.dev2026100406), with the relevant subset
named file by file before execution (12 files: the
allocation-bearing collections, string and memory
units). The other candidates were inventoried and
decided on evidence: C2 lit suites and C3 C++ units
(unrunnable here), C4 MAX tests and C5 package-shipped
tests (absent). The ratification is therefore
reproducible from F-007's record, but a future reader
of the roadmap alone cannot reproduce the choice - the
roadmap text still names no suite, and this packet
does not edit it. INC-F001-08 is slice-discharged, not
roadmap-discharged.

## 8. Carried-owed register

Nothing here blocks the Clause B verdict; everything
here survives the exit, exactly as E-009's register
survived Phase 5's.

| # | Owed / carried item | Source | Future home / trigger |
|---|---|---|---|
| O-01 | **Phase 1 exit unmet** (Section 6.3): no failure packet, no repeatable validation command, three unreconciled exit gates | A-005 inconsistencies 1 and 5; INC-F001-09; INC-F008-08 | The Section 6.2 resumption point: tablet environment by Volmarr's word + A-005 harness capture; the gate reconciliation is a roadmap-editing decision for the project owner |
| O-02 | **Phase 4 exit UNMET**: physical-device allocate/fill/free + multithread tests on Android still OWED | E-009 O-16 | An Android NDK (install reserved for Volmarr's word), an NDK build, and a physical SM-X400 run |
| O-03 | **Phase 3 exit UNMET**: the x86-64 suite run still OWED (the fresh run's own `test_c007` output restates it in-line - INC-F009-04) | E-009 O-17 | An x86-64 sysroot + suite run; a 4096-page environment run |
| O-04 | X1 literal retry (both blocker legs) | F-008 Sections 5-6; INC-F008-01..05 | Section 6.2 resumption point; any new-surface (B3/B4/B7) or compiler-boundary work is a future slice under its own contract decision |
| O-05 | T5 remainder: 8 skew-blocked subset files; C2/C3 suites | F-007 Section 5; INC-F007-02/-03/-05 | A ratified matching pair (published 1.2.0-line install at Volmarr's word, or the test tree at the 1.1.0 tag as a recorded acquisition); C2/C3 additionally need a completed source build or a lit installation |
| O-06 | T5 WITH leg: upstream tests cannot run against the replacement under the B2+B1 scope | INC-F007-04 | Only a future standard-name surface or a documented Mojo allocator hook (B3/B5) could create one - a design decision for Volmarr, not a test step |
| O-07 | Licence evaluation for the published package (`LicenseRef-Modular-Proprietary`) | INC-F001-12; INC-F002-09; INC-F009-05 | OWED before any redistribution-shaped step involving a MimirAlloc-linked Mojo artifact |
| O-08 | TCMalloc provenance unresolved: public source selects jemalloc, shipped wheels aborted in TCMalloc, the Pi's 1.1.0 runtime defines a TCMalloc-derived internal surface | INC-F001-10; F-008 fact F6 | A per-binary provenance record for any artifact a future slice claims to have given a replacement allocator; until then no "replaced Mojo's allocator" claim for the compiler |
| O-09 | Generated-executable portability beyond this Pi (toolchain-prepended RUNPATH; static leg has no `$ORIGIN`) | INC-F003-03; INC-F005-04 | A future slice that moves a generated executable off this Pi |
| O-10 | Quiescence discipline for Mojo-path mapped-byte proofs (the runtime's lazy one-shot 64 MiB mapping) | INC-F005-01; INC-F006-04 | Standing requirement for every later Mojo slice; five clean F-006 repetitions and this packet's fresh run do not retire it |
| O-11 | T4/T6 same-source pair: the comparison is a banked pair at verdict/exit/link-record level, stdout never byte-compared across legs | INC-F006-01 | A future wired-variant *source* written and banked as its own slice first |
| O-12 | No target is "supported" in the Roadmap Section 5 sense; the Linux AArch64 + glibc row gains Phase 6 B2+B1-scope Mojo evidence only, and still lacks native CI | E-009 Section 8, carried | Unchanged by this packet; target promotion is a project-owner decision on fuller evidence |

## 9. Inconsistencies recorded by this slice (INC-F009-*)

None of these is silently resolved. Each names its
owner or its resumption point.

- **INC-F009-01 - The exit verdict depends on a stated
  reading of the exit sentence.** The sentence attaches
  "no new correctness failures" (X3) to the success
  branch only; the blocker branch requires only "a
  blocker with exact results". This packet returns MET
  via Clause B while grading X3 PARTIAL and X2
  MET-in-scope - X2's proof, like X3's, was gathered in
  the B2+B1 scope, not in the original command's
  process. A reader who holds that the blocker branch
  implicitly demands the full Clause A proof set at MET
  strength would grade this exit UNMET or PARTIAL
  instead. The packet states its reading and its clause
  map openly (headnote, Section 4) rather than hiding
  the dependence; F-001's matrix pre-authorised exactly
  this outcome shape ("can close X1 only through the
  X4 door - a blocker recorded with exact results -
  ... That is a roadmap-conformant outcome"). Owner:
  Volmarr, if he wants the stricter reading ruled in;
  the roadmap text is his to amend.
- **INC-F009-02 - F-007 and F-008 were authored in
  parallel on the same base, and their cross-references
  are asymmetric.** F-008's header records base
  `38b27b0` (F-006 banked), the same base as F-007;
  banking order on main is F-007 (`789a7f0`) then F-008
  (`35211e1`). F-008's exit accounting therefore cites
  only "F-006's bounded statement" for X3 and never
  cites F-007. This packet synthesises both records;
  no evidence conflict results (F-007 does not touch
  X1), but a reader following F-008 alone understates
  X3's basis. Recorded here; banked files are not
  rewritten.
- **INC-F009-03 - Recorded-run placement seam.** This
  packet's recorded run is `tests/recorded_run_f009.txt`
  (the assignment's example path; Wave 4's D-009 used
  the same placement), while every other Wave 6
  recorded run is under `tests/mojo/`
  (`recorded_run_f003.txt` .. `recorded_run_f008.txt`).
  Same seam class as INC-E009-04. Future readers must
  look in both places; no file is moved by this slice,
  because moving banked neighbours is not this slice's
  to do and this file's path was assigned.
- **INC-F009-04 - `make test` is green while carrying
  the Phase 3 debt in its own output.** The fresh run's
  S1 shows `test_c007` printing `x86-64 run of this
  suite: OWED (B-009 S3.4); Phase 3 exit: UNMET` inside
  an otherwise fully passing suite (exit 0, 8/8
  `RESULT: PASS`). Not a contradiction - the suite is
  target-scoped - but a standing trap: "make test exit
  0" must never be read as "Phase 3 exited". This
  packet restates Phase 3 UNMET in its headnote and in
  O-03 for exactly that reason.
- **INC-F009-05 - T2's licence clause is recorded, not
  evaluated (carries INC-F001-12 / INC-F002-09).**
  Section 5 grades T2 MET on the readiness-record
  requirement F-001's matrix specified, while the
  roadmap's own condition - the published package used
  "only where legally and technically possible" - has
  no evaluation anywhere in the banked record. On-Pi
  research use is what every Wave 6 slice actually did;
  any redistribution-shaped use remains gated on the
  owed evaluation (O-07). Owner: a future slice at
  Volmarr's word.
- **INC-F009-06 - F-004's banked document still reads
  OWED for its WITH leg.** The join that closed it
  (keeper re-run at `08433ae` + F-005's Section 5
  record + this packet's S2/S3) lives outside F-004's
  file, whose Section 0 table still shows the WITH
  verdict as OWED. The evidence is complete and this
  packet is the reconciling record, but a reader of
  F-004 alone sees a stale verdict. Recorded, not
  repaired: banked slice documents are not rewritten by
  later slices (F-005 Section 5 states the same
  discipline).
- **INC-F009-07 - Cross-base byte-reproducibility is
  an observation, not a guarantee (carries
  INC-F006-05).** The fresh run's S5 hashes equal
  F-006's banked hashes although the bases differ
  (`35211e1` vs `63c2dc1`). This packet cites the match
  as corroboration that both legs reproduce at Mojo
  1.1.0 on this Pi; a toolchain or environment change
  may break byte-reproducibility without breaking any
  verdict, and no verdict in this packet rests on the
  hashes.

## 10. Bottom line

Wave 6 did everything the Pi venue honestly allows,
and the packet says exactly where the wall is. The
boundary is wired and proven (T1, T7/X2): a generated
Mojo program's own explicit allocations are serviced
by MimirAlloc, proven at binding grade. The minimal
and representative programs run and re-run green, with
and without the replacement, on one revision (T3, T4,
T6). The relevant upstream tests ran as far as the
version skew allows - 112 of 112 executed tests passed,
and the rest is honestly PARTIAL (T5, X3). And the
original failing command - the milestone's first
branch - remains **UNMET and BLOCKED**, for three
proven reasons plus a fourth on the wiring side, with
the blocker recorded in exact results (X4) and both
legs restated here. Under the roadmap's own disjunctive
exit sentence, **Phase 6 is exited via the blocker
door**: MET via X4, UNMET via X1, X5 respected, scope
Pi-native and B2+B1 only, with Phase 1, Phase 3 and
Phase 4 exits still unmet behind it. The tablet failure
is not fixed. What would fix it is named in Section
6.2, and it starts with Volmarr's word.

- Files changed: this file
  (`docs/PHASE_6_EXIT_EVIDENCE.md`, added) and
  `tests/recorded_run_f009.txt` (added). No other
  tracked file modified; no Makefile change.
- `README.md`: byte-untouched - sha256
  `bd7f66148fe77b8d465875562373e94a1c35baf51c0df5ba74c8b2b8b9a46702`
  in the worktree, in `HEAD`, before and after the
  fresh run (recorded run S0/S7).
- Public header and ABI: `include/mimir_alloc.h`
  byte-untouched (sha256:
  `55107f05b3eb8c120230f76b2193fdc953af800af57a91caa405c84971af8772`);
  freshly built
  `libmimiralloc.so` defines exactly the 4 public
  dynamic symbols (recorded run S7).
- One commit on `yrsa/mimiralloc-09`, parent exactly
  `35211e1`. Not pushed, not merged - the keeper banks
  last and completes Wave 6.
- Nothing installed, downloaded, or removed in this
  slice. Samtal HTTP 200 before and after; `free -g`
  showed 13 GiB available before and after.
