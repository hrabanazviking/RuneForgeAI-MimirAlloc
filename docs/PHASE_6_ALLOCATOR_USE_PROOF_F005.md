# Phase 6 - Allocator-Use Proof on the Mojo Path (Binding Grade)

Slice: **F-005 (team-05)**. Wave 6, Phase 6 - the allocator-use
proof F-001's matrix assigns to this slice (F-001 Section 7,
row T7+X2) and F-003 left owed as INC-F003-05.
Base: `08433aec2b9a58d2c38d6e10d5e8647a3d0b38b9` (F-004 banked;
Wave 6 = 4 of 9) on branch `yrsa/mimiralloc-05`, worktree
`/home/pi/mimiralloc-team-05` on Raspberry Pi `purpleviking1`.
Date of the recorded run: 2026-11-06 (~22:01 EST).
Toolchain: Debian GNU/Linux 12 (bookworm); cc 12.2.0; Mojo
1.1.0 (8189361e) via the `/usr/local/bin/mojo` pixi wrapper,
used exactly as it stands; kernel 6.12.112-v8-16k-hailo+;
`getconf PAGESIZE` = 16384. Pi load stayed light (sequential
small builds only; `free -g` showed 13 GiB available before
and after; Samtal HTTP 200 on port 8787 before and after).
Nothing was installed. `README.md` is byte-untouched (sha256
of the worktree file equals `git show HEAD:README.md`, both
`bd7f66148fe77b8d465875562373e94a1c35baf51c0df5ba74c8b2b8b9a46702`);
no C source, no public header, and no ABI surface was touched.

Governing texts: Roadmap Section 8 Phase 6; banked F-001
`docs/PHASE_6_FOUNDATION_PLAN.md` (Sections 6-7); banked
F-003 `docs/PHASE_6_BOUNDARY_WIRING_F003.md` (the B2+B1
boundary this slice upgrades; INC-F003-04 and INC-F003-05
are this slice's commissions); banked F-004
`docs/PHASE_6_MINIMAL_MOJO_RUN_F004.md` (Section 3 join,
addressed in Section 5 below); banked E-004
`docs/PHASE_5_BINDING_PROOF_E004.md` (the LD_DEBUG binding
shape); banked E-005 `docs/PHASE_5_AUDIT_HOOK_E005.md`
(the audit counters and the two-leg proof this slice
carries to the Mojo path).

> **THIS SLICE CLOSES INC-F003-05 AND STRENGTHENS
> INC-F003-04. IT DOES NOT EXIT PHASE 6.** Exit element X1
> stays BLOCKED exactly as F-001/F-004 recorded it
> (INC-F001-01..03; F-008 owns the blocker record, F-009
> the exit packet). Nothing here is an X1 result, and no
> sentence below claims servicing for any allocation other
> than the generated programs' own explicit `mimir_*`
> calls (F-001 Section 6 scope discipline).

## 0. Bottom line

- **INC-F003-05 is CLOSED: the Mojo-path servicing proof
  is now binding grade, in two legs mirroring E-005.**
  *Dynamic leg* (`tests/mojo/test_f005.mojo`, linked
  against `libmimiralloc.so` through the F-003 B2
  boundary): `LD_DEBUG=bindings` records exactly **4**
  binding lines tying the generated executable's own
  `mimir_malloc` / `mimir_calloc` / `mimir_realloc` /
  `mimir_free` references to this tree's
  `libmimiralloc.so` at runtime (E-004's shape, recorded
  run S6); and the E-005 dynamic-leg effects hold exactly
  - **18 checks, 0 failed**: every returned block carries
  a valid MimirAlloc descriptor, and the process's mapped
  bytes move by exactly `page_round_up(page + size)` per
  allocation (+131072 for 4x malloc(64), **+0** for 4x
  libc malloc(64), +65536 for 2x calloc(16,8)), by +0 for
  a realloc grow, and return **exactly** to the quiescent
  baseline after all frees. *Static leg*
  (`tests/mojo/test_f005_audit.mojo`, library objects
  linked directly, E-005's static construction): **24
  checks, 0 failed** - the six internal E-005 audit
  counters move exactly with the Mojo program's explicit
  calls across E-005's own phase script and finish at
  **call_malloc 6, call_calloc 4, call_realloc 4,
  call_free 9, published 10, retired 10, live 0** -
  numerically identical to E-005's C leg - while an
  in-process libc bypass phase (3 allocations, 1 realloc,
  3 frees) moves **no** counter.
- **INC-F003-04 is answered as strongly as the design
  allows.** `mimir_free` still returns void, but its
  servicing is no longer inferred from a clean exit: in
  the static leg each free of a live block provably
  retires exactly one mapping in the implementation
  (retired 2 -> 6 -> 10 across the free phases, books
  balancing at published == retired), and in the dynamic
  leg the six frees provably release exactly the mapped
  bytes the six allocations published (final total ==
  baseline, byte for byte). A free that silently did
  nothing, or freed the wrong thing, fails both legs.
- **Stability:** the recorded run repeats the dynamic leg
  5/5 PASS and the audit leg 3/3 PASS (S5b); during
  development the stabilised dynamic leg passed 15/15
  consecutive runs. The one flakiness source found - the
  Mojo runtime's own one-shot 64 MiB lazy mapping - is
  recorded in full as INC-F005-01, including the two
  failing runs of the unstabilised program; it is a
  runtime behaviour, never an allocator one (every
  MimirAlloc delta was exact in every run, failing or
  passing).
- **The public ABI did not move.** `nm -D
  --defined-only` on the rebuilt `libmimiralloc.so` still
  lists exactly the four public symbols; the audit
  counters remain local there (a link probe against the
  .so fails, recorded run S2). Makefile change is one
  appended standalone block (+42 lines, 0 deletions);
  `make test` composition is unchanged and exits **0**
  with the slice in place.

Evidence file: `tests/mojo/recorded_run_f005.txt` (336
lines, 16,652 bytes, sha256
`da9824e275d0e10c9756fc15d69fe54ee7c4b425d60e1753b9524a770e74159b`)
- every command with its exit code, in sections S0-S10.
This document summarises it; the recorded run is the
record.

Deliverables: this file; `tests/mojo/test_f005.mojo`
(357 lines, sha256
`67f99568ec62fe29b0b110845b4557210bc19267c050142098befc355a1ac4db`);
`tests/mojo/test_f005_audit.mojo` (365 lines, sha256
`8a42c99d835fb86bda5c522f8aa3e92b346c6f42d79caa1315c0704a05f605e6`);
`tests/mojo/f005_probe.c` (103 lines, sha256
`b02dd783609cd50dd42283acb3a950d3d162d2a6a001cfb8341d368f2d783501`);
`tests/mojo/f005_audit.c` (65 lines, sha256
`bd1e24df26b8dcbfd7dc055657959bb6d32c144510dc40a77ea4f5f598287839`);
`tests/mojo/recorded_run_f005.txt`; the additive Makefile
block (`make test-f005`).

## 1. Design: E-005's two legs, carried over unchanged in substance

E-005 proved the intended call path in C with two legs
because the audit counters are deliberately unreadable
from a consumer of the shared library. Exactly the same
constraint governs the Mojo path, so this slice builds
the same two legs around the same two Mojo-visible
mechanisms:

- **Why a getter shim (static leg).** Mojo's
  `std.ffi external_call` calls *functions*; it cannot
  read a C global variable. `tests/mojo/f005_audit.c`
  therefore wraps the six E-005 counters (declared for
  tests in `tests/abi/mimir_audit_e005.h`) in
  parameterless `f005_audit_*` getter functions. The
  static leg's generated executable links the library
  **objects** (`build/mimir_alloc.o`,
  `build/mimir_core.o`, `build/mimir_platform.o`) plus
  `build/f005_audit.o` directly - via `mojo build
  -Xlinker <object>` arguments, a use of the B2 link
  surface this slice demonstrates for the first time -
  so the counters the getters read are the very counters
  the executable's own `mimir_*` calls move.
- **Why an observer library (dynamic leg).** The dynamic
  leg must stay on the F-003 route (the generated
  executable linked against `libmimiralloc.so`), where
  the counters cannot be read at all - the recorded run
  re-proves this (S2: a C probe referencing
  `mimir_audit_call_malloc` against the .so fails to
  link, exit 1). So, exactly as in E-005's dynamic leg,
  servicing is proven by implementation-only effects
  observed from inside the process: descriptor validation
  and mapped-bytes accounting. `tests/mojo/f005_probe.c`
  packages those two observations (plus a page-size
  getter) as `build/libf005probe.so`, a test-only shared
  object that holds **no allocator state** and provides
  **no** `mimir_*` symbol (INC-F005-03 records the
  two-library shape honestly). The descriptor check is
  E-005's, unchanged: magic `MIMIRALL`, `mapping_base`
  == the descriptor's own address, `usable_offset` ==
  one page, `requested_size` == the requested size,
  state == LIVE, `mapping_length` ==
  `page_round_up(page + size)` - applied **only** to
  pointers returned by this program's own `mimir_*`
  calls, never to a foreign pointer. The mapped-bytes
  accounting is E-005's: the byte sum over
  `/proc/self/maps`, never a line count (INC-E005-02).
- **Quiescence (dynamic leg only).** The Mojo process
  carries a runtime E-005's C consumer did not have; see
  INC-F005-01. The dynamic program warms up and settles
  before baselining. The audit leg needs no such
  discipline: counters cannot be moved by runtime
  mappings, and it was deterministic in every run.

## 2. Dynamic leg evidence (recorded run S3-S6, S5b)

Exact build (the same line `make test-f005` runs):

```
$ mojo build -o build/test_f005 tests/mojo/test_f005.mojo \
    -Xlinker -Lbuild -Xlinker -lmimiralloc \
    -Xlinker -lf005probe -Xlinker -rpath -Xlinker '$ORIGIN'
(exit 0)
```

ELF record (S4): `DT_NEEDED` = `libKGENCompilerRTShared.so`,
`libmimiralloc.so`, `libf005probe.so`, `libc.so.6`;
RUNPATH `[/home/pi/mojo-dev/.pixi/envs/default/lib:$ORIGIN]`
(the toolchain prefix is INC-F003-03, carried). The
executable's undefined dynamic symbols are exactly the
four `mimir_*` plus the three `f005_*` observer calls.
`ldd` resolves both project libraries to this tree's
`build/` directory.

Loader bindings (S6, `LD_DEBUG=bindings ./build/test_f005`,
exit 0, program output identical to S5) - the complete
set of binding lines from the generated executable to
`libmimiralloc.so` (count: **4**):

```
binding file ./build/test_f005 [0] to /home/pi/mimiralloc-team-05/build/libmimiralloc.so [0]: normal symbol `mimir_malloc'
binding file ./build/test_f005 [0] to /home/pi/mimiralloc-team-05/build/libmimiralloc.so [0]: normal symbol `mimir_calloc'
binding file ./build/test_f005 [0] to /home/pi/mimiralloc-team-05/build/libmimiralloc.so [0]: normal symbol `mimir_realloc'
binding file ./build/test_f005 [0] to /home/pi/mimiralloc-team-05/build/libmimiralloc.so [0]: normal symbol `mimir_free'
```

(Three further binding lines tie the executable's
`f005_page_size` / `f005_mapped_bytes` /
`f005_descriptor_valid` references to `libf005probe.so`;
no `mimir_*` name binds anywhere else.)

Run (S5; page 16384, so `page_round_up(page + 64)` =
`page_round_up(page + 128)` = 32768):

| Step | Mapped total | Delta | Verdict |
|---|---|---|---|
| quiescent baseline | 2475163648 | - | - |
| 4x `mimir_malloc(64)`, 4/4 descriptors valid | 2475294720 | **+131072** = 4 x 32768 | PASS |
| 4x libc `malloc(64)` (negative) | 2475294720 | **+0** | PASS |
| libc frees | 2475294720 | +0 | PASS |
| 2x `mimir_calloc(16, 8)`, 2/2 descriptors valid at 128, extents zeroed | 2475360256 | **+65536** = 2 x 32768 | PASS |
| `mimir_realloc` grow 64 -> 96, descriptor records 96, bytes preserved | 2475360256 | **+0** (publish + retire) | PASS |
| all 6 blocks freed via `mimir_free` | 2475163648 | back to baseline **exactly** | PASS |

Test-side counting (INC-E001-07 shape (a)): 6 `mimir_*`
allocation call sites, 6 descriptor-validated blocks -
equal. **CHECKS: 18 run, 0 failed. RESULT: PASS, exit 0.**
Repetitions: 5/5 PASS in the recorded run (S5b); 15/15
in the development loop after stabilisation.

## 3. Static leg evidence (recorded run S3-S5, S5b)

Exact build (the same line `make test-f005` runs):

```
$ mojo build -o build/test_f005_audit tests/mojo/test_f005_audit.mojo \
    -Xlinker build/mimir_alloc.o -Xlinker build/mimir_core.o \
    -Xlinker build/mimir_platform.o -Xlinker build/f005_audit.o \
    -Xlinker -lpthread
(exit 0)
```

ELF record (S4): the executable **defines** the four
public symbols itself (`T mimir_malloc` etc. in its full
symbol table - the library objects are inside it) and
has **no** `libmimiralloc.so` DT_NEEDED - the expected
shape of a static leg, and the reason the two legs are
separate builds, exactly as in E-005.

The phase script is E-005's static script unchanged in
substance; the recorded AUDIT lines (S5) are:

```
AUDIT start                     call_malloc=0 call_calloc=0 call_realloc=0 call_free=0 published=0  retired=0  live=0
AUDIT phase1-mimir-malloc-x5    call_malloc=5 published=5  retired=0  live=5
AUDIT phase2-libc-bypass        call_malloc=5 published=5  retired=0  live=5   (no counter moved)
AUDIT phase3-mimir-calloc-x3    call_calloc=3 published=8  retired=0  live=8
AUDIT phase4a-realloc-grow-x2   call_realloc=2 published=10 retired=2  live=8
AUDIT phase4b-realloc-shrink-same call_realloc=4 published=10 retired=2 live=8  (shrink/same publish nothing; both retain their pointers)
AUDIT phase5-defined-failures   call_malloc=6 call_calloc=4 published=10 retired=2 (oversize malloc + overflow calloc returned null)
AUDIT phase6a-free-null         call_free=1 published=10 retired=2               (call counted, nothing retired)
AUDIT phase6b-free-x4           call_free=5 retired=6  live=4
AUDIT phase7-free-rest          call_free=9 published=10 retired=10 live=0        (books balance exactly)
```

(Full six-counter lines are in the recorded run; the
abbreviation above drops only counters that did not move
in that phase.) The final books - **call_malloc 6,
call_calloc 4, call_realloc 4, call_free 9, published
10, retired 10, live 0** - are numerically identical to
E-005's C static leg on the same script, now produced by
a *generated Mojo program's* explicit calls. **CHECKS:
24 run, 0 failed. RESULT: PASS, exit 0.** Repetitions:
3/3 PASS in the recorded run (S5b).

One Mojo construction deserves its own line: Phase 6's
`mimir_free(NULL)` cannot be spelled literally in Mojo
source (INC-F005-02). The program derives the zero
address at runtime (a counter minus itself), prints and
checks it is 0, and passes that - the value reaching
`mimir_free` is genuinely NULL; only its spelling
differs from C.

## 4. Negative controls (recorded run S2, S7)

- **Dynamic source without the boundary:** the identical
  source built with no `-Xlinker` argument fails to link
  (exit 1) with **28** undefined-reference lines - the
  four `mimir_*` names at 12 call sites and the three
  `f005_*` names at 15 (per-symbol counts in S7). No
  other component of the toolchain provides either set.
- **Audit source without the objects:** exit 1, **38**
  undefined-reference lines (the four `mimir_*` and the
  six `f005_audit_*` names).
- **In-process libc controls:** Phase 2 of the static
  leg (libc malloc/calloc/realloc/free in the same
  process) moves no audit counter; the dynamic leg's
  libc phase moves no mapped byte. The Mojo runtime's
  and stdlib's own allocations likewise never reach the
  `mimir_*` entry points: the static leg starts at all
  counters 0 *after* the runtime has started, and moves
  only at this program's explicit call sites.
- **Counters vs the .so:** a consumer referencing an
  audit counter against `libmimiralloc.so` fails to link
  (S2) - the two-leg split is forced by the design, not
  chosen for convenience.

## 5. F-004 Section 3 join - status

F-004 banked with its WITH half OWED and defined the
join exactly (its Section 3): once F-003 was banked on
main, the keeper re-runs F-003's recorded commands and
this slice's WITHOUT commands on one revision, completes
F-004's comparison table, and re-labels the WITH verdict
on that evidence alone.

**Status as of this slice:** F-003 and F-004 are both
banked, and the coordinator main stands at this slice's
base, `08433ae`. The keeper has re-run F-003's banked
`test-f003` and F-004's `test-f004-mojo` on merged main
at `08433ae`, **both exit 0** *(keeper evidence, labelled
as such; reported to this team with the assignment, not
re-derived here)*. Independently, this slice re-ran both
targets in its own worktree on the same base (recorded
run S8): `make test-f003` exit 0 (4/4 steps PASS) and
`make test-f004-mojo` exit 0 (`F004 RESULT: PASS
(count=192 sum=72096 weighted=7130112)`) - the two legs
of F-004's comparison now stand on one revision in this
record too. The WITH verdict F-004 anticipated -
*a generated Mojo executable whose DT_NEEDED lists
libmimiralloc.so, whose own explicit allocations are
serviced by the four namespaced symbols, exiting 0 with
its PASS verdict* - is not merely met but superseded in
strength by this slice: Sections 2-3 prove that
servicing at binding grade, not linkage grade. The
re-labelling of F-004's own document is the keeper's
banking-time act under F-004's procedure; this slice
does not rewrite F-004's record.

## 6. Verification of the slice itself

- `make test-f005`: exit **0** (both legs PASS; S9
  rebuilds from clean F-005 artifacts through the
  Makefile recipes themselves).
- Native `make test` with the slice in place: exit
  **0** (S10; composition unchanged - the F-005 target
  is standalone and not part of it).
- `git diff HEAD --numstat`: `Makefile` +42 / -0
  (append-only) plus the six new files of Section 0.
  `include/mimir_alloc.h`, all C sources, and the
  4-symbol ABI surface are untouched; `README.md`
  byte-identical to HEAD (S10 hashes).
- Samtal HTTP 200 and `free -g` 13 GiB available after
  the run (S10) - the Pi's other residents were never
  starved.

## 7. Inconsistencies recorded in this slice - INC-F005-NN

None of these is silently resolved. Each names its owner
or its resumption point.

- **INC-F005-01 - The Mojo runtime lazily maps exactly
  64 MiB once, at a nondeterministic early moment; the
  unstabilised dynamic leg failed 2 of its first 13
  runs.** Development record, kept whole: the first
  recorded-run attempt's S5 run reported the calloc
  phase delta as 67174400 instead of 65536 and the
  post-free total as baseline + 67108864 (18 checks, 2
  failed, exit 1); a later 10-run loop failed once more
  with the jump inside the malloc phase (delta 67239936
  instead of 131072). In every failing run the excess
  was exactly **67108864** bytes (64 MiB), it arrived
  once and never left, and every MimirAlloc-attributable
  delta was itself exact - when the same mapping happened
  to land *before* the baseline measurement, the
  unstabilised program passed in full. The mapping is
  the Mojo runtime's own (this process's only other
  mapper; MimirAlloc's per-allocation mappings are
  32768 bytes at these sizes and are all accounted).
  Its internal purpose inside the runtime was not
  investigated - out of scope here. Resolution inside
  this slice: the dynamic program warms up (prints,
  pattern loop, mimir + libc alloc/free pairs,
  descriptor reads) and settles (mapped total stable
  across three consecutive 20 ms-spaced readings) before
  baselining; the stabilised program passed 15/15 in
  development and 5/5 in the recorded run. Owner: any
  future Mojo-path mapped-bytes proof must reuse this
  quiescence discipline; the audit-counter leg is
  immune by construction and needs none.
- **INC-F005-02 - A literal null Pointer is
  inexpressible in Mojo source on this toolchain.**
  `Pointer(unsafe_from_address=0)` fails at compile time
  ("constraint failed: Pointer is non-nullable. To
  construct a null pointer, use Optional[Pointer] to
  model nullability." - probe build recorded in S7,
  exit 1). This extends INC-F003-02 (Pointer non-null by
  design) from returns to arguments: a nullable C
  argument position has no literal Mojo spelling either.
  The static leg's workaround - a runtime-derived zero,
  printed and checked before the call - is honest but is
  a workaround. Owner: every later Mojo slice needing a
  NULL argument; E-005's C leg remains the literal-NULL
  proof for the counter behaviour itself.
- **INC-F005-03 - The dynamic leg's executable carries
  two project DT_NEEDED entries.** `libf005probe.so` is
  a test-only observer that exists because the dynamic
  route cannot read the counters; it holds no allocator
  state and exports no `mimir_*` name, and S6's bindings
  show the `mimir_*` references resolving to
  `libmimiralloc.so` alone. But the B2 boundary F-003
  defined is the `libmimiralloc.so` placement only - a
  reader who counts DT_NEEDED entries without reading
  this note could overstate the boundary. Standing note
  for F-006's comparison discipline (cf. INC-F004-04's
  direct-vs-transitive caution).
- **INC-F005-04 - The static leg's RUNPATH has no
  `$ORIGIN`.** No `-Xlinker -rpath` was passed for it
  (there is no project library for it to resolve - the
  allocator is inside the executable), so its RUNPATH is
  the toolchain's own env lib dir alone. Recorded
  against INC-F003-03: generated-executable portability
  beyond this Pi remains unaddressed. Owners unchanged:
  F-002 / F-006.
- **INC-F005-05 - `make test-f005` is PATH-dependent,
  like `make test-f003` (INC-F003-06).** The standalone
  target invokes the `mojo` found on PATH; without Mojo
  it fails at its build lines. `make test` never
  references Mojo and passed in the same recorded run.
  Owner: none; standing note.

## 8. What this slice does not do

- It does not exit Phase 6, prove X1, or describe any Pi
  result as the tablet failure fixed (X1 is BLOCKED;
  F-008 owns the record, F-009 the packet).
- It claims servicing for **only** the two generated
  programs' own explicit `mimir_*` calls. The Mojo
  compiler's, stdlib's, and runtime's allocations are
  untouched and unclaimed - the static leg's counters
  prove they never reach the `mimir_*` entry points,
  and nothing here services them.
- It adds no symbol, no shim on the standard names, and
  no public surface: the observer and getter libraries
  are test-only files under `tests/mojo/`, the public
  header is untouched, and the .so still exports exactly
  four symbols. B3/B4/B7's new-surface decisions remain
  untaken future decisions, exactly as F-001 left them.
- It installs nothing and downloads nothing; the
  existing pixi env was used exactly as the wrapper
  uses it.
- It does not modify `README.md`, `include/mimir_alloc.h`,
  any C source, or the composition of `make test` (the
  Makefile change is one appended standalone target
  block, 42 insertions, 0 deletions).
