# Phase 6 - Representative Program Repeated-Compile Comparison

Slice: **F-006 (team-06)**. Wave 6, Phase 6 tasks **T4**
(compile a representative program repeatedly) + **T6**
(compare against the same revision without the
replacement), per Roadmap Section 8 Phase 6 and the F-001
exit-evidence matrix (Section 7, row T4+T6).
Base: `63c2dc1f8970bdd0476e9fcfc9b99e51f5a9a86d` (F-005
banked; Wave 6 = 5 of 9) on branch `yrsa/mimiralloc-06`,
worktree `/home/pi/mimiralloc-team-06` on Raspberry Pi
`purpleviking1`. Date of the recorded run: 2026-11-06
(~22:26 EST). Toolchain: Debian GNU/Linux 12 (bookworm);
cc 12.2.0; Mojo **1.1.0 (8189361e)** via the
`/usr/local/bin/mojo` pixi wrapper, used exactly as it
stands; kernel 6.12.112-v8-16k-hailo+; `getconf PAGESIZE`
= 16384. Pi load stayed light (sequential small builds
only; `free -g` showed 13 GiB available before and after;
Samtal HTTP 200 on port 8787 before and after). Nothing
was installed. `README.md` is byte-untouched (sha256 of
the worktree file equals `git show HEAD:README.md`, both
`bd7f66148fe77b8d465875562373e94a1c35baf51c0df5ba74c8b2b8b9a46702`);
the public header is byte-untouched (sha256
`55107f05b3eb8c120230f76b2193fdc953af800af57a91caa405c84971af8772`
in both places) and no C source was touched.

Governing texts: Roadmap Section 8 Phase 6; banked F-001
`docs/PHASE_6_FOUNDATION_PLAN.md` (Sections 6-7; the
T4+T6 row and INC-F001-03/-11); banked F-004
`docs/PHASE_6_MINIMAL_MOJO_RUN_F004.md` (Section 3
comparison discipline); banked F-005
`docs/PHASE_6_ALLOCATOR_USE_PROOF_F005.md` (the WITH
leg's proof shape; INC-F005-01/-03/-04 carried in
Sections 5-6 below).

> **THIS SLICE CLOSES NO PART OF THE PHASE 6 EXIT.**
> Exit element **X1 stays BLOCKED** exactly as
> F-001/F-004/F-005 recorded it (INC-F001-01..03: the
> original failing command was a tablet (Samsung
> SM-X400, Android/Termux) Mojo 1.2.0 event, never
> recorded exactly, on an environment and version this
> Pi does not hold). F-008 owns the X1 blocker record;
> F-009 owns the exit packet. This slice's contribution
> to X3 is bounded in Section 7 (INC-F006-03). Servicing
> is claimed **only** for the WITH leg program's own
> explicit `mimir_*` calls - never for the Mojo
> compiler's, stdlib's, or runtime's allocations.

## 0. Bottom line

- **WITHOUT leg** (`tests/mojo/test_f004.mojo`, the
  banked F-004 baseline): **5/5 builds exit 0, 5/5 runs
  exit 0**, stdout identical every repetition
  (`F004 RESULT: PASS (count=192 sum=72096
  weighted=7130112)`), and the five built binaries are
  **byte-identical** (one distinct sha256,
  `197e68c2e4a82ac1ded8b396e2980d64bcd19aedff29c59dc2ed64387dc378d6`
  - the same hash F-004 itself recorded for this source
  at its own base, so the baseline build reproduces
  across slices at Mojo 1.1.0).
- **WITH leg** (`tests/mojo/test_f005.mojo`, DYNAMIC leg
  only - the F-003 B2 boundary as proven binding-grade
  by F-005): **5/5 builds exit 0, 5/5 runs exit 0**,
  every run **CHECKS: 18 run, 0 failed** with
  `F-005 DYNAMIC LEG RESULT: PASS`; the mapped-bytes
  evidence is exact and identical in all five runs
  (quiescent baseline 2475163648; +131072 for 4x
  `mimir_malloc(64)`; +0 for 4x libc `malloc(64)`;
  +65536 for 2x `mimir_calloc(16, 8)`; +0 for the
  realloc grow; final total exactly the baseline after
  all frees); the five built binaries are
  **byte-identical** (one distinct sha256,
  `6a1df97f7c09c9ee288cfa4b4594ec2d92589957dabeda4014598be1497bdac2`;
  BuildID `a175b3e0e663947c55d956e77617d470ee67faa8`,
  the same BuildID F-005 recorded for this program).
  An inspection run under `LD_DEBUG=bindings` records
  exactly **4** binding lines from the generated
  executable to this tree's `libmimiralloc.so`, one per
  `mimir_*` symbol (Section 4).
- **The comparison (T6) holds at the level the evidence
  honestly supports:** on the same banked revision
  (`63c2dc1`) and the same Mojo revision (1.1.0), both
  legs build and run repeatedly with **no failure in
  either leg**; the legs differ exactly where the
  boundary says they must - the WITHOUT executable's
  `DT_NEEDED` holds no MimirAlloc library and it
  references no `mimir_*` symbol, while the WITH
  executable's `DT_NEEDED` lists `libmimiralloc.so`
  (plus the test-only observer, INC-F005-03) and its
  own explicit allocations are serviced by the four
  namespaced symbols, proven inside every run. Stdout
  is **not** byte-compared across legs: the legs are
  different programs by design (Section 1).
- **No repetition failed in either leg, and none was
  hidden:** the harness records every build and run,
  pass or fail (its exit code is the failure count).
  The INC-F005-01 instability (the Mojo runtime's
  one-shot 64 MiB lazy mapping) did **not** appear in
  this series - the WITH program's warm-up + settle
  discipline was active in all five runs and the
  quiescent baseline came out identical (2475163648)
  five times out of five. That is an observation about
  this series, not a disproof of INC-F005-01
  (INC-F006-04).
- **The public ABI did not move.** `nm -D
  --defined-only` on the rebuilt `libmimiralloc.so`
  (sha256
  `d7d2f9981bcbbaa4d54664746325b89df1da375922b46209c43970c92f4e7fe1`)
  lists exactly the four public symbols. Makefile
  change is one appended standalone block (+21 lines,
  0 deletions); `make test` composition is unchanged
  and exits **0** with the slice in place; the series
  reproduces from `make clean` through `make test-f006`
  with identical counts (Section 8).

Evidence file: `tests/mojo/recorded_run_f006.txt` (408
lines, 19,454 bytes, sha256
`0c7a352912c40a3f5ffc99c21354d08d78018d8e5e3509a134f2ca84595de0a8`)
- every command with its exit code, in sections S0-S8.
This document summarises it; the recorded run is the
record.

Deliverables: this file; `tests/mojo/run_f006_compare.sh`
(the harness, 226 lines, sha256
`8cd9f97f8b3e0da93885ffa309f858986638aac14028df51876cdf8c260eb193`);
`tests/mojo/recorded_run_f006.txt`; the additive
Makefile block (`make test-f006`). No Mojo source is new
in this slice: both leg programs are banked files,
reused exactly as banked (hashes in Section 1).

## 1. The representative program: the choice, and why

F-001's T4+T6 row requires the representative program to
be **named and justified**, and INC-F001-11 governs any
use of the out-of-repo `/home/pi/c2` sources: choosing
one would be a recorded decision, never a silent
adoption. This slice adopts **nothing** from outside the
repository. Both legs are in-repo, clean-room, banked
Phase 6 programs:

- **WITHOUT leg - `tests/mojo/test_f004.mojo`** (banked
  F-004; source sha256
  `bebbf4cfbd731838e91c963c2372029761c84b59768c101d986549f57e0f8f8b`).
  A growable `List[Int]` workload (192 entries, forcing
  storage growth), in-place mutation, and verification
  against independently computed checksums. It contains
  no `mimir_*` reference, no `external_call`, and is
  built with no `-Xlinker` argument; the allocations it
  touches are the Mojo stdlib/runtime's own, through the
  default allocator. It is the natural baseline subject:
  it is the only banked Mojo program that runs entirely
  without the replacement.
- **WITH leg - `tests/mojo/test_f005.mojo`, dynamic leg**
  (banked F-005; source sha256
  `67f99568ec62fe29b0b110845b4557210bc19267c050142098befc355a1ac4db`).
  The same B2+B1 boundary F-003 first wired - the
  generated program calls `mimir_malloc` / `mimir_calloc`
  / `mimir_realloc` / `mimir_free` explicitly via Mojo
  FFI (B1) with `build/libmimiralloc.so` placed on its
  link line through `-Xlinker` (B2) - at the strongest
  grade Wave 6 holds: F-005 upgraded F-003's
  verdict+linkage proof to binding grade (descriptor
  validation + mapped-bytes accounting inside every run,
  via the test-only observer `tests/mojo/f005_probe.c`,
  sha256
  `b02dd783609cd50dd42283acb3a950d3d162d2a6a001cfb8341d368f2d783501`).
  F-003's own program (`test_f003.mojo`) is the boundary
  origin, but repeating only it would re-prove the
  weaker grade; F-005's dynamic leg **supersedes** it
  (F-005 Section 5) and is the honest head-of-line WITH
  subject at this base. F-005's static (audit) leg is
  not a WITH-leg candidate at all: it links the library
  objects directly and has no `libmimiralloc.so`
  boundary to compare.

**Why the legs are two different programs, stated
plainly.** No single in-repo Mojo source exists in both
wired and unwired forms, and F-004 Section 3 proved why
none can be manufactured honestly from the F-004 source:
building it with B2 alone would put the library on the
link line while servicing **none** of its allocations -
presence, not servicing. A "same source, with and
without" table built that way would be a stronger-looking
and false comparison. The honest T4+T6 shape on this
base is therefore a **pair**: a representative
allocation-touching Mojo workload run repeatedly without
the replacement, and the representative boundary workload
run repeatedly with it, on one revision, compared at
**verdict / exit-code / link-record level only**. This
is recorded as INC-F006-01 so F-009's packet carries
exactly this strength and no more.

**Same revision, both legs.** Both series ran in this
worktree at HEAD `63c2dc1` (the recorded run's S0 shows
`git rev-parse HEAD`), with the one installed Mojo
revision, 1.1.0 (8189361e), via the unmodified
`/usr/local/bin/mojo` wrapper. Per INC-F001-03, the
comparison is same-revision only *within* the Pi's
1.1.0; it says nothing about the failing tablet
revisions (1.2.0 / nightly).

## 2. Exact commands

Library prerequisites for the WITH leg (recorded run
S2; the same rules `make test-f006` depends on):

```
$ make build/libmimiralloc.so        (exit 0)
$ make build/libf005probe.so         (exit 0)
```

WITHOUT leg, per repetition `n` = 1..5 (only the `-o`
name varies):

```
$ timeout 180 mojo build tests/mojo/test_f004.mojo -o build/test_f006_without_<n>
$ ./build/test_f006_without_<n>
```

WITH leg, per repetition `n` = 1..5 (only the `-o` name
varies):

```
$ timeout 180 mojo build -o build/test_f006_with_<n> tests/mojo/test_f005.mojo \
    -Xlinker -Lbuild -Xlinker -lmimiralloc \
    -Xlinker -lf005probe -Xlinker -rpath -Xlinker '$ORIGIN'
$ ./build/test_f006_with_<n>
```

Binaries land in `build/` (untracked) so the WITH leg's
`$ORIGIN` rpath resolves the project libraries exactly
as in F-003/F-005. The whole series is one harness,
`tests/mojo/run_f006_compare.sh`, run standalone via
`make test-f006`; the recorded run is that harness's
full output. Wall times below are the harness's
millisecond readings (`date +%s%N` around each build
and run); Section 7 states what they are and are not.

## 3. Per-repetition results

### 3.1 WITHOUT leg (recorded run S3)

| Rep | Build exit | Build wall (ms) | Run exit | Run wall (ms) | Stdout verdict |
|---|---|---|---|---|---|
| 1 | 0 | 1149 | 0 | 6 | `F004 RESULT: PASS (count=192 sum=72096 weighted=7130112)` |
| 2 | 0 | 1101 | 0 | 6 | identical |
| 3 | 0 | 1319 | 0 | 15 | identical |
| 4 | 0 | 1145 | 0 | 6 | identical |
| 5 | 0 | 1090 | 0 | 8 | identical |

All four stdout lines (count, checksum, weighted,
RESULT) are identical in every repetition - within this
leg stdout **is** byte-comparable, because it is one
program repeated. Binary sha256: `197e68c2...78d6` for
all five builds (1 distinct hash; recorded run S5).

### 3.2 WITH leg (recorded run S4)

| Rep | Build exit | Build wall (ms) | Run exit | Run wall (ms) | Checks | Verdict |
|---|---|---|---|---|---|---|
| 1 | 0 | 1377 | 0 | 75 | 18 run, 0 failed | `F-005 DYNAMIC LEG RESULT: PASS` |
| 2 | 0 | 1357 | 0 | 67 | 18 run, 0 failed | PASS (identical) |
| 3 | 0 | 1443 | 0 | 87 | 18 run, 0 failed | PASS (identical) |
| 4 | 0 | 1372 | 0 | 67 | 18 run, 0 failed | PASS (identical) |
| 5 | 0 | 1342 | 0 | 87 | 18 run, 0 failed | PASS (identical) |

Mapped-bytes record, **identical in all five runs**
(page 16384, so `page_round_up(page + 64)` =
`page_round_up(page + 128)` = 32768):

| Step | Mapped total | Delta | Verdict |
|---|---|---|---|
| quiescent baseline (after warm-up + settle) | 2475163648 | - | - |
| 4x `mimir_malloc(64)`, 4/4 descriptors valid | 2475294720 | **+131072** = 4 x 32768 | PASS |
| 4x libc `malloc(64)` (negative) | 2475294720 | **+0** | PASS |
| 2x `mimir_calloc(16, 8)`, 2/2 descriptors valid at 128, extents zeroed | 2475360256 | **+65536** = 2 x 32768 | PASS |
| `mimir_realloc` grow 64 -> 96 | 2475360256 | **+0** (publish + retire) | PASS |
| all 6 blocks freed via `mimir_free` | 2475163648 | back to baseline **exactly** | PASS |

Binary sha256: `6a1df97f...ac2` for all five builds
(1 distinct hash; recorded run S5).

## 4. Side-by-side comparison (T6)

| Aspect | WITHOUT (F-004 program) | WITH (F-005 dynamic program) |
|---|---|---|
| Source | `tests/mojo/test_f004.mojo` - stdlib `List[Int]` growth + mutation, no `mimir_*`, no FFI | `tests/mojo/test_f005.mojo` - explicit `mimir_*` calls via Mojo FFI (B1) |
| Build command | `timeout 180 mojo build tests/mojo/test_f004.mojo -o <out>` (no `-Xlinker`) | same `mojo build` plus the B2 `-Xlinker` set (`-Lbuild -lmimiralloc -lf005probe -rpath $ORIGIN`) |
| Builds | **5/5 exit 0** | **5/5 exit 0** |
| Runs | **5/5 exit 0** | **5/5 exit 0** |
| Verdict | `F004 RESULT: PASS (...)`, identical every rep | `F-005 DYNAMIC LEG RESULT: PASS`, 18/18 checks every rep |
| Binary stability | 1 distinct sha256 across 5 builds; equals F-004's banked hash | 1 distinct sha256 across 5 builds; BuildID equals F-005's recorded BuildID |
| `file` | ELF 64-bit LSB pie, ARM aarch64, dynamically linked, BuildID `1cd78d76...` | same class, BuildID `a175b3e0...` |
| `DT_NEEDED` (direct, recorded run S6) | `libKGENCompilerRTShared.so`, `libc.so.6` - **no MimirAlloc library** | `libKGENCompilerRTShared.so`, `libmimiralloc.so`, `libf005probe.so` (observer, INC-F005-03), `libc.so.6` |
| `nm -D` | no `mimir` symbol (grep exit 1, as expected) | exactly the four `mimir_*` undefined + the three `f005_*` observer calls |
| Loader bindings (inspection run, S7) | n/a (no `mimir_*` reference exists to bind) | exactly **4** lines, executable -> this tree's `libmimiralloc.so`, one per `mimir_*` symbol; run exit 0, PASS |
| Allocator servicing the program's allocations | Mojo runtime default allocator (stdlib List storage) | the four namespaced `mimir_*` symbols, for this program's explicit allocations only |
| New correctness failures vs the other leg | - | **none observed**: both legs 5/5 + 5/5, all checks green (bounded as INC-F006-03) |

Reading of the table, plainly: repeated compilation and
execution are **stable on both sides of the boundary**
at this revision - the replacement's presence on the
WITH leg's link line changes nothing about build or run
success, and the WITH leg's in-run proof (descriptors,
mapped bytes, bindings) shows the servicing is real,
not linkage-only. That is the whole T4+T6 claim. It is
a Pi-scope, Mojo-1.1.0 claim about two generated
programs; it is not a claim about the Mojo compiler, the
stdlib's internal allocations, or the tablet failure.

## 5. INC-F005-01 discipline, applied

The WITH program carries F-005's quiescence discipline
inside itself: it warms up (printing, a pattern loop,
mimir and libc alloc/free pairs, descriptor reads) and
then samples the mapped total until it is stable across
three consecutive 20 ms-spaced readings **before** the
baseline is taken, so the Mojo runtime's one-shot lazy
64 MiB mapping lands in the warm-up, not inside a
measured phase. This slice changed nothing about that
discipline and added no measurement of its own: the
mapped-byte numbers above are the program's, produced
under its discipline, repeated five times.

Result for this series: **no instability event
occurred.** All five runs took the identical quiescent
baseline (2475163648 - also the baseline F-005's own
recorded run took on this Pi) and every MimirAlloc
delta was exact in every run. Labelled honestly: five
clean repetitions under the discipline do not retire
INC-F005-01, whose failing signature (2 failures in the
unstabilised program's first 13 runs) belongs to the
*unstabilised* shape; the discipline's continued use is
the standing requirement (INC-F006-04). Had any
repetition failed, the harness would have recorded it
in place and exited non-zero; none did.

## 6. Carried notes: INC-F005-03 and INC-F005-04

- **INC-F005-03 (observer library `DT_NEEDED`) - bears
  directly on this slice's link records.** The WITH
  executable carries **two** project `DT_NEEDED`
  entries: `libmimiralloc.so` (the boundary) and
  `libf005probe.so` (the test-only observer, which
  holds no allocator state and exports no `mimir_*`
  name). Section 4's table and the recorded run's S6
  keep the two distinct, and S7's binding count is
  taken only over lines whose source is the generated
  executable itself and whose target is
  `libmimiralloc.so` - the F-005 S6 shape - so the
  observer can neither inflate the boundary nor be
  mistaken for it. A reader who counts `DT_NEEDED`
  entries without this note would overstate the
  boundary, exactly the failure INC-F005-03 warns of.
- **INC-F005-04 (generated-executable portability) -
  unchanged, and visible in this slice's records.**
  The WITH executable's `RUNPATH` is
  `[/home/pi/mojo-dev/.pixi/envs/default/lib:$ORIGIN]`
  (recorded run S6): the `$ORIGIN` element is what
  resolves the project libraries from `build/`, and the
  toolchain prefix is this Pi's pixi environment path.
  The WITHOUT executable's `RUNPATH` is the toolchain
  prefix alone. These binaries are therefore evidence
  **on this Pi, in this tree**; nothing in this slice
  makes a generated executable portable beyond it, and
  no portability claim is made. Owners unchanged
  (F-002 / this slice's records).

## 7. Timing: recorded, never an oracle

The harness records wall milliseconds per build and run
(Section 3). Observed ranges in this series: WITHOUT
builds 1090-1319 ms, runs 6-15 ms; WITH builds
1342-1443 ms, runs 67-87 ms. These numbers are recorded
because the roadmap's repeated-compile task is about
stability over repetitions, and the raw readings are
part of that record. They are **not** a performance
comparison and support no performance claim: the two
legs are different programs doing different work, and
the Pi is a shared, loaded home (Samtal and other
residents ran throughout). No conclusion is drawn from
them beyond "each build and run completed, repeatedly,
without timeout" (the 180 s timeout never came close
to binding).

**X3 bound (from F-001's matrix):** X3 (no new
correctness failures) reads on F-006 + F-007 + F-009.
This slice's contribution is exactly: *no new failures
observed in the F-004/F-006 runs* - both legs fully
green across 5 + 5 repetitions on this revision. If
F-007 is BLOCKED, X3 can be stated in exactly those
words and no stronger (INC-F006-03).

## 8. Verification of the slice itself

- `make test-f006` from `make clean`: exit **0**; the
  series reproduces with identical counts (WITHOUT
  5/5 + 5/5, WITH 5/5 + 5/5, 5x `CHECKS: 18 run,
  0 failed`, 4 binding lines, harness PASS) - run in
  this slice after the recorded run; the recorded run
  file itself was not rewritten by the reproduction
  (its sha256 is unchanged, Section 0).
- Native `make test` with the slice in place: exit
  **0** (full C suite; composition unchanged - the
  F-006 target is standalone and not part of it).
- `git diff HEAD --numstat`: `Makefile` +21 / -0
  (append-only) plus the three new files of Section 0.
  `include/mimir_alloc.h`, all C sources, and the
  4-symbol ABI surface are untouched; `README.md`
  byte-identical to HEAD (hashes in the headnote).
- Samtal HTTP 200 and `free -g` 13 GiB available after
  the run (recorded run S8) - the Pi's other residents
  were never starved.

## 9. Inconsistencies recorded in this slice - INC-F006-NN

None of these is silently resolved. Each names its
owner or its resumption point.

- **INC-F006-01 - T4's "representative program" is a
  pair, not one source.** No in-repo Mojo source exists
  in both wired and unwired forms, and F-004 Section 3
  bars manufacturing one by a B2-only build (presence,
  not servicing). F-006 therefore compares two banked
  programs at verdict / exit-code / link-record level,
  and stdout is never byte-compared across legs.
  Owner: F-009, whose packet must carry T4/T6 at
  exactly this strength; a future same-source pair
  would require a wired variant *source* written and
  banked as its own slice first.
- **INC-F006-02 - The harness's first bindings filter
  over-counted, and was corrected before banking.**
  Development record, kept whole: the first full series
  run (all builds and runs passing identically) counted
  **14** `LD_DEBUG` lines naming `libmimiralloc.so`,
  because the filter also matched the library's *own*
  bindings to libc (the library as binding source).
  Only 4 of the 14 are bindings from the generated
  executable. The filter was narrowed to the F-005 S6
  shape (source == the executable, target ==
  `libmimiralloc.so`), the harness's S1 hash changed
  accordingly, and the **entire series was re-run**;
  the recorded run in this slice is the corrected run
  (count: exactly 4). No result from the first run is
  cited anywhere in this slice except in this note.
- **INC-F006-03 - X3 cannot be closed by this slice.**
  F-001's matrix assigns X3 to F-006 + F-007 + F-009
  jointly, and caps it at "no new failures observed in
  the F-004/F-006 runs" if F-007 is BLOCKED. This slice
  supplies the F-006 half of that evidence and states
  the cap in Section 7. Owner: F-007 (its record),
  F-009 (the synthesis).
- **INC-F006-04 - INC-F005-01 is not retired by five
  clean repetitions.** The runtime's lazy 64 MiB
  mapping is nondeterministic by F-005's record; this
  series observed zero events in five disciplined
  runs. The quiescence discipline remains mandatory
  for any future Mojo-path mapped-byte measurement,
  and any future failure under it must be recorded in
  place, as the harness here records failures (exit
  code = failure count, failed repetitions printed,
  never deleted). Owner: every later Mojo slice.
- **INC-F006-05 - Build reproducibility is an
  observation, not a guarantee.** Each leg produced
  exactly one distinct binary hash across five builds,
  the WITHOUT hash equals F-004's banked hash, and the
  WITH BuildID equals F-005's recorded BuildID - all
  at Mojo 1.1.0 on this Pi, with the pixi environment
  as installed. A toolchain or environment change may
  break byte-reproducibility without breaking any
  verdict in this slice; the verdicts, not the hashes,
  are the comparison's load-bearing evidence. Owner:
  none; standing note for F-009's packet.

## 10. What this slice does not do

- It does not exit Phase 6, prove X1, or describe any
  Pi result as the tablet failure fixed (X1 is BLOCKED;
  F-008 owns the record, F-009 the packet).
- It claims servicing for **only** the WITH program's
  own explicit `mimir_*` calls. The Mojo compiler's,
  stdlib's, and runtime's allocations are untouched
  and unclaimed; the WITHOUT leg's allocations are the
  default allocator's by construction.
- It writes no new Mojo program, adopts no out-of-repo
  source (INC-F001-11), and invents no same-source
  comparison the base cannot honestly hold
  (INC-F006-01).
- It adds no symbol, no shim, and no public surface:
  the public header is byte-untouched and the .so
  still exports exactly four symbols. B3/B4/B7's
  new-surface decisions remain untaken future
  decisions, exactly as F-001 left them.
- It makes no performance claim: timings are recorded
  as run readings only (Section 7).
- It installs nothing and downloads nothing; the
  existing pixi env was used exactly as the wrapper
  uses it.
- It does not modify `README.md`, `include/mimir_alloc.h`,
  any C source, or the composition of `make test` (the
  Makefile change is one appended standalone target
  block, 21 insertions, 0 deletions).
