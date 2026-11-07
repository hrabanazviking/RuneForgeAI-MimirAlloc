# Phase 6 - Minimal Mojo Program Run, With and Without MimirAlloc

Slice: **F-004 (team-04)**. Wave 6, Phase 6 task **T3** (run a
minimal Mojo program). Deliverables: this file,
`tests/mojo/test_f004.mojo` (the program),
`tests/mojo/recorded_run_f004.txt` (the real Pi run), and an
additive, standalone, Mojo-guarded Makefile target
(`test-f004-mojo`, +22 lines, append-only).
Base: `c6d367222759b0403a218f74255c937e06784345` (F-002
banked; Wave 6 = 2 of 9) on branch `yrsa/mimiralloc-04`,
worktree `/home/pi/mimiralloc-team-04` on Raspberry Pi
`purpleviking1`. Governing texts: Roadmap Section 8 Phase 6;
banked F-001 `docs/PHASE_6_FOUNDATION_PLAN.md` (Sections 6-7);
banked F-002 `docs/PHASE_6_MOJO_TOOLCHAIN_READINESS.md`.
Work style: **solo careful work** by the team lead, no
sub-agents. Pi load stayed light (`free -g`: 15 GiB total,
13 GiB available before and after; Samtal HTTP 200 on port
8787 checked after the run). Nothing was installed; the
existing pixi environment was used exactly as the
`/usr/local/bin/mojo` wrapper invokes it, never re-resolved.
`README.md` is byte-untouched (sha256 of the worktree file
equals `git show HEAD:README.md`); no C source or header was
touched.

> **THIS SLICE CLOSES NO PART OF THE PHASE 6 EXIT.**
> It proves one narrow thing: a minimal, allocation-touching
> Mojo program **runs on this Pi** - the WITHOUT (baseline)
> half **PROVEN**, the WITH (MimirAlloc-wired) half **OWED**
> to F-003, which had not banked when this slice ran.
> Per F-001 Section 7, exit element **X1 stays BLOCKED on
> the Pi via the X4 blocker door**: the original failing
> command was a tablet (Samsung SM-X400, Android/Termux)
> Mojo 1.2.0 event that was never recorded exactly
> (INC-F001-01), on an environment and version this Pi does
> not hold. Per the X5 exclusion, recorded in advance by
> F-001, no minimal-program success in this slice counts
> toward X1 - and this program is in any case not a bare
> hello-world (Section 1). F-008 owns the X1 blocker record;
> F-009 owns the exit packet.

## 0. Verdicts at a glance

| Half | Verdict | Evidence |
|---|---|---|
| WITHOUT: `mojo build` + native run, 3 repetitions | **PROVEN** | 3/3 builds exit 0, 3/3 runs exit 0, identical PASS stdout; binaries byte-identical (Section 2) |
| WITHOUT: `mojo run` of the same source, 1 repetition | **PROVEN** | exit 0, stdout identical to the native runs (Section 2) |
| WITHOUT: standalone `make test-f004-mojo` | **PROVEN** | exit 0 with PASS output; with Mojo off PATH it prints a skip line and exits 0 (guard proven) |
| WITH: same program through the F-003 B2+B1 boundary | **OWED** | F-003 not banked at run time (proof in the recorded run); exact join procedure in Section 3 |
| Phase 6 exit element X1 | **BLOCKED** | F-001 Section 7, via the X4 blocker door; unchanged by this slice |

## 1. The program (design)

`tests/mojo/test_f004.mojo` is a small, original, clean-room
Mojo program written for this slice. It is deliberately
**not** a bare hello-world:

- It builds a growable `List[Int]` buffer of **192 entries**
  with known contents, `v_i = (i*7+3) % 256` for
  `i in 0..<192` - appending past the list's initial
  capacity, so the stdlib list's storage must grow
  (reallocate) during the build. This is the
  allocation-touching element: the allocations are the Mojo
  stdlib/runtime's own, made through the default allocator.
- It then mutates every entry in place, `v_i = v_i*3 + 5`.
- It verifies the result against expected values computed
  **independently of Mojo** (in Python, from the same
  formulas, before the first Mojo run): count `192`,
  checksum (plain sum) `72096`, weighted sum
  `sum((i+1) * v_i) = 7130112`.
- On success it prints the count, both checksums, and
  `F004 RESULT: PASS (count=192 sum=72096 weighted=7130112)`
  and returns normally (exit 0). On any mismatch it prints
  FAIL and raises, so the process exits non-zero - the PASS
  line cannot be produced by a program that computed the
  wrong answer.

Source sha256 (as run, also in the recorded run):
`bebbf4cfbd731838e91c963c2372029761c84b59768c101d986549f57e0f8f8b`.

Scope discipline, stated plainly: the WITHOUT leg contains
**no** `mimir_*` reference, **no** `external_call`, and
**no** `-Xlinker` argument. It is the baseline leg F-001's
T3 row describes - "a minimal program run *without* any
boundary is the baseline leg, not a result" - and this
slice records it as exactly that: a baseline data point
for the with/without comparison, never as allocator
evidence.

## 2. WITHOUT half - evidence (PROVEN)

All commands below were run on the Pi in this slice on
2026-11-06 (~21:28 EST); full stdout is in
`tests/mojo/recorded_run_f004.txt`. The toolchain as used:

```
$ cat /usr/local/bin/mojo
#!/bin/bash
exec /home/pi/.pixi/bin/pixi run --manifest-path /home/pi/mojo-dev/pixi.toml mojo "$@"
$ mojo --version
Mojo 1.1.0 (8189361e)            exit 0
```

Build and native run, repeated 3 times for stability -
the exact command each time (only the output name varies):

```
$ timeout 180 mojo build tests/mojo/test_f004.mojo -o /tmp/f004/test_f004_<n>
$ /tmp/f004/test_f004_<n>
```

| Rep | Build exit | Run exit | Stdout (all four lines, identical every rep) |
|---|---|---|---|
| 1 | 0 | 0 | `F004 count: 192` / `F004 checksum: 72096` / `F004 weighted: 7130112` / `F004 RESULT: PASS (count=192 sum=72096 weighted=7130112)` |
| 2 | 0 | 0 | identical |
| 3 | 0 | 0 | identical |

The three built executables - and a fourth built by the
Makefile target below - are **byte-identical**:
sha256 `197e68c2e4a82ac1ded8b396e2980d64bcd19aedff29c59dc2ed64387dc378d6`
for `/tmp/f004/test_f004_1`, `_2`, `_3` and
`build/test_f004_mojo` (checked in this slice after the
recorded run). Build+run outcome is stable and the build
is reproducible at this toolchain revision.

`mojo run` of the same source (1 repetition):

```
$ timeout 180 mojo run tests/mojo/test_f004.mojo
F004 count: 192
F004 checksum: 72096
F004 weighted: 7130112
F004 RESULT: PASS (count=192 sum=72096 weighted=7130112)
mojo_run_exit:0
```

Identical stdout and exit code to the native runs - the
two invocation paths behave the same for this program.

Executable inspection (repetition-1 binary, in the
recorded run):

```
$ file /tmp/f004/test_f004_1
ELF 64-bit LSB pie executable, ARM aarch64, dynamically linked,
interpreter /lib/ld-linux-aarch64.so.1, not stripped
$ readelf -d /tmp/f004/test_f004_1 | grep NEEDED
 (NEEDED)  Shared library: [libKGENCompilerRTShared.so]
 (NEEDED)  Shared library: [libc.so.6]
$ nm -D /tmp/f004/test_f004_1 | grep -i mimir
 (no lines; grep exit 1 - no MimirAlloc symbol, as expected
  for the WITHOUT leg)
```

`ldd` resolves the Mojo runtime globals libraries
(`libKGENCompilerRTShared.so`, `libMSupportGlobals.so`,
`libAsyncRTRuntimeGlobals.so`, `libstdc++.so.6`,
`libgcc_s.so.1`) from the pixi env lib directory plus the
system `libc`/`libm`/`libdl` - full output in the recorded
run. **No `libmimiralloc.so` appears anywhere in the
WITHOUT leg's `DT_NEEDED`, `nm -D`, or `ldd` output.** That
absence is the baseline the WITH leg must differ from.

Makefile wiring for this leg (additive only): the
standalone target `make test-f004-mojo` builds the same
source to `build/test_f004_mojo` and runs it. Run in this
slice: exit 0, same PASS stdout. With Mojo removed from
PATH (`env PATH=/usr/bin:/bin make test-f004-mojo`) it
prints `test-f004-mojo: mojo not on PATH - SKIPPED
(Mojo-guarded standalone target)` and exits 0 - the guard
is proven, and the C suite can never be made to depend on
Mojo by this target. `make test` composition is unchanged
(the target is not part of it); native `make test` with
the whole slice in place exits **0** (Section 5).

## 3. WITH half - OWED to F-003, with the exact join point

**State at run time (proven, in the recorded run):**
F-003 had **not banked**. The coordinator checkout's main
was still at `c6d3672` (F-002) and
`docs/PHASE_6_BOUNDARY_WIRING_F003.md` existed neither
there nor in this worktree. F-003 banks before F-004 in
roadmap order, so this slice does not invent, borrow, or
pre-run F-003's results; the WITH half is **OWED**, with
the join defined exactly below.

**Why the WITH leg cannot be faked from this program
alone:** F-001 Section 6 rates the only honest near-term
boundary as **B2+B1** - B2 (`mojo build -Xlinker <ARG>`)
places `libmimiralloc.so` on a generated executable's link
line, and B1 (explicit adoption) is a Mojo program calling
the four `mimir_*` symbols itself via the Mojo FFI. This
slice's program makes no explicit `mimir_*` calls, so
building it with B2 alone would put the library on the
link line while servicing **none** of the program's
allocations - presence, not servicing (F-001's discipline:
a name on a link line is not proof any allocation was
serviced). This slice therefore does not run a B2-only
build and does not report one as a WITH result.

**Join point - the exact procedure, to be executed by the
keeper (or an F-004 follow-up) at F-004 banking time, once
F-003 is banked on main:**

1. Confirm the banked state: `git log --oneline -1` on the
   coordinator main names F-003's banking commit, and
   `docs/PHASE_6_BOUNDARY_WIRING_F003.md` is present on
   main with its recorded exact `mojo build` command
   (including every `-Xlinker` argument) and its wired
   Mojo source path under `tests/mojo/`.
2. Re-run F-003's recorded commands **exactly as banked**
   in the F-004 worktree (rebased onto the F-003 banking
   commit by the keeper): build F-003's wired variant,
   record build/run exit codes and full stdout, and its
   `file` / `readelf -d` (`DT_NEEDED` must list
   `libmimiralloc.so`) / `nm -D` record, in an addendum to
   `tests/mojo/recorded_run_f004.txt`.
3. Re-run this slice's WITHOUT commands unchanged on the
   same base, so both legs stand on one revision.
4. Complete the comparison table in Section 4 from those
   two records and re-label the WITH verdict from OWED to
   PROVEN or BLOCKED on that evidence alone - never on
   F-003's worktree state before banking.

The expected honest comparison shape is already known and
is recorded here so the join cannot quietly upgrade it:
the two programs are **different sources** (F-003's wired
variant exercises explicit `mimir_*` calls; this slice's
program exercises stdlib `List` growth), so stdout will
**not** be byte-identical across legs, and the comparison
is at the level of verdicts, exit codes, and link records
- not identical output. What the WITH leg will add, if
F-003's evidence holds, is: a generated Mojo executable
whose `DT_NEEDED` lists `libmimiralloc.so`, whose own
explicit allocations are serviced by the four namespaced
symbols, exiting 0 with its PASS verdict - beside this
slice's baseline, whose `DT_NEEDED` lists no MimirAlloc
library and which exits 0 with its PASS verdict through
the default allocator.

## 4. Side-by-side comparison

| Aspect | WITHOUT (this slice) | WITH (through F-003 boundary) |
|---|---|---|
| Source | `tests/mojo/test_f004.mojo` - stdlib `List[Int]` growth + mutation, no `mimir_*`, no FFI | F-003's wired variant (banked path owed) - explicit `mimir_*` calls via Mojo FFI (B1) |
| Build command | `timeout 180 mojo build tests/mojo/test_f004.mojo -o <out>` (no `-Xlinker`) | F-003's banked command, incl. every `-Xlinker` argument (B2) - **OWED** |
| Build exit | 0 (3/3, plus Makefile-target build) | **OWED** |
| Run exit | 0 (3/3 native; 1/1 `mojo run`) | **OWED** |
| Stdout / verdict | `F004 RESULT: PASS (count=192 sum=72096 weighted=7130112)`, identical every run | F-003 variant's own PASS verdict - **OWED**; not byte-comparable by design (Section 3) |
| `file` | ELF 64-bit LSB pie, ARM aarch64, dynamically linked | **OWED** (expected same class) |
| `DT_NEEDED` | `libKGENCompilerRTShared.so`, `libc.so.6` - **no `libmimiralloc.so`** | must list `libmimiralloc.so` to count - **OWED** |
| Allocator servicing the program's allocations | Mojo runtime default allocator (stdlib List storage) | the four namespaced `mimir_*` symbols, for the wired program's explicit allocations only (F-001 B2+B1 scope) |
| Verdict | **PROVEN** | **OWED-to-F-003** (join in Section 3) |

## 5. Verification of the slice itself

- Native `make test` with the slice in place: **exit 0**
  (full C suite; composition unchanged - the F-004 target
  is standalone and not part of `make test`).
- `make test-f004-mojo`: exit 0, PASS output; Mojo-absent
  guard: skip line, exit 0 (both run in this slice).
- `git diff` for the slice: `Makefile` +22 lines
  (append-only addition), plus new files
  `docs/PHASE_6_MINIMAL_MOJO_RUN_F004.md`,
  `tests/mojo/test_f004.mojo`,
  `tests/mojo/recorded_run_f004.txt`. No other tracked
  file modified; C sources and `include/mimir_alloc.h`
  untouched; `README.md` byte-identical to HEAD.
- Samtal on the Pi after the run: HTTP 200 (port 8787);
  `free -g` still 13 GiB available.

## 6. Inconsistencies recorded in this slice - INC-F004-NN

- **INC-F004-01 - The WITH half is OWED, not run.**
  F-003 (the B2+B1 boundary this slice's WITH leg must
  pass through) had not banked when this slice ran
  (recorded-run proof: coordinator main at `c6d3672`, no
  F-003 doc on main or in this worktree). Owner: the
  keeper at F-004 banking time, executing the Section 3
  join exactly. Until then, any statement that a Mojo
  program has run "with MimirAlloc" in Wave 6 rests on
  F-003's own slice record alone, not on F-004.
- **INC-F004-02 - F-001's T3 row vs this slice's queue
  wording.** The queue phrases F-004 as "minimal Mojo
  program run **with and without** MimirAlloc", but
  F-001 Section 7 states the "with" leg is only meaningful
  through the F-003 boundary, which banks *after* this
  slice's base was cut. A single-base F-004 therefore
  cannot honestly contain both legs; this slice resolves
  the tension by delivering the WITHOUT leg in full and
  the WITH leg as a defined, joinable OWED - and records
  the tension here rather than papering over it with a
  B2-only build.
- **INC-F004-03 - Parallel `tests/mojo/` creation.**
  F-003 and F-004 both create the new `tests/mojo/`
  directory in parallel worktrees on different bases.
  Filenames are distinct by design (`test_f003.mojo` /
  `test_f004.mojo`; `recorded_run_f003.txt` /
  `recorded_run_f004.txt`), so no path collision is
  expected, but both slices also append to the Makefile -
  the keeper must union the two appends at banking time
  and verify both standalone targets survive, rather than
  taking either side's Makefile wholesale.
- **INC-F004-04 - `DT_NEEDED` is short; `ldd` is long.**
  The generated executable's direct `DT_NEEDED` holds only
  `libKGENCompilerRTShared.so` and `libc.so.6`, while
  `ldd` lists eight libraries (the remaining six arrive
  transitively through the KGEN runtime library). Not a
  defect - but any future binding proof (F-005) must read
  the direct record, not the transitive one, or it will
  overstate what the generated program itself links.
- **INC-F004-05 - X1 remains BLOCKED; nothing here moves
  it.** Carried from INC-F001-01/-02/-03: the original
  failing command was never recorded exactly, and the
  affected environment (SM-X400, Android/Termux, Mojo
  1.2.0) is not this Pi (native Linux, Mojo 1.1.0). This
  slice's Pi success is a T3 baseline data point only
  (X5 guard). Owner: F-008 (blocker record), F-009
  (packet).
