# Phase 6 - Boundary Wiring Build: B2+B1 Demonstrated (F-003)

Slice: **F-003 (team-03)**. Wave 6, Phase 6 boundary wiring build.
Deliverables: `docs/PHASE_6_BOUNDARY_WIRING_F003.md` (this file),
`tests/mojo/test_f003.mojo` (the Mojo program),
`tests/mojo/recorded_run_f003.txt` (the recorded build +
inspection + run), and one additive Makefile target
(`test-f003`, standalone; `make test` composition unchanged).
Base: `491ecf17de6f32e4c69326af2e9a28b38bd9c0df` (F-001 banked;
Wave 6 = 1 of 9) on branch `yrsa/mimiralloc-03`, worktree
`/home/pi/mimiralloc-team-03` on Raspberry Pi `purpleviking1`.
Governing text: `docs/PHASE_6_FOUNDATION_PLAN.md` (F-001),
especially the boundary catalogue B1..B7 (its Section 6), the
exit-evidence matrix row for F-003 (its Section 7, item T1),
and INC-F001-07 (the reading that F-003's primary branch - a
buildable harness - governs, because the toolchain is present
and what was unproven was the boundary). `README.md` was not
touched by this slice; `include/mimir_alloc.h` and the C
library sources were not touched - this slice consumes the
ABI, it does not change it. Work style: solo careful work, no
sub-agents. Pi load stayed light throughout (`free -g`: 15 GiB
total, 13 GiB available at every check; each `mojo build`
took ~4 s; Samtal was not starved).

> **HEADLINE: the B2+B1 boundary is PROVEN at this slice's
> evidence grade.** A generated Mojo executable now exists in
> this repository's test tree whose own explicit allocations
> are serviced by the four namespaced MimirAlloc symbols: the
> program calls `mimir_malloc` / `mimir_calloc` /
> `mimir_realloc` / `mimir_free` explicitly from Mojo source
> (B1), `libmimiralloc.so` is a `DT_NEEDED` of the generated
> executable placed there by `mojo build`'s documented
> `-Xlinker` flags (B2), and the program's four step verdicts
> all PASS with exit code 0 (recorded run, Section 5).
>
> **SCOPE, STATED BEFORE ANYTHING ELSE:** this proves the
> B2+B1 scope only - a generated program's *own explicit*
> allocations. It does not touch the Mojo compiler's own
> allocations, the Mojo stdlib's internal allocations, or the
> tablet failure, and it is not Phase 6 exit evidence: X1
> remains BLOCKED (INC-F001-01..03; F-008 owns the blocker
> record), and the allocator-use proof of E-004/E-005 strength
> (loader bindings / implementation-only effects) remains
> F-005's deliverable (INC-F003-05). Phase 4 and Phase 3 exits
> remain UNMET as carried by E-009/F-001; nothing here
> discharges them.

## 1. Verdict per element

| # | Element | Verdict | Where proven |
|---|---|---|---|
| 1 | B1 mechanism: Mojo-side FFI call form for explicit adoption (`std.ffi external_call`) confirmable and working against the namespaced C surface | **PROVEN** | Sections 2-3; recorded run build exit 0 |
| 2 | B2: `libmimiralloc.so` placed on a generated Mojo executable's link via documented `mojo build` flags (`-Xlinker`) | **PROVEN** | Section 4: `DT_NEEDED [libmimiralloc.so]`, RUNPATH, `ldd` resolution |
| 3 | Servicing of the program's own explicit allocations (malloc pattern, calloc zeroing, realloc preservation, free) at this slice's grade (program step verdicts + ELF evidence) | **PROVEN** (4/4 steps, exit 0) | Section 5; recorded run lines "STEP1..STEP4: PASS" |
| 4 | Control: same source built WITHOUT the MimirAlloc link | **PROVEN meaningful** - the control build FAILS (exit 1, undefined references to all four symbols at six call sites); nothing else in the toolchain provides these symbols, so the `-lmimiralloc` link is load-bearing | Section 5.2; recorded run |
| 5 | Failure signalling: the program exits non-zero on any step failure (raise after cleanup) | **PROVEN as a mechanism** - a throwaway probe program whose `main` raises exits 1 on this toolchain (recorded run); the F-003 program uses the same path on every failure branch | Section 2.4; recorded run probe section |
| 6 | B3 / B4 / B7 (standard-name interposition, link wrapping, runtime-library substitution) | **documented-only** - unchanged from F-001: each requires a new standard-name surface that does not exist; not attempted, not adopted by silence | F-001 Section 6 |
| 7 | B5 (documented Mojo allocator hook) | **documented-only** - unchanged from F-001: absent in the banked sources; `mojo build --help` re-read in this slice still shows no allocator option (only `-Xlinker` / `--lld-path` under Linker options) | F-001 Section 6; this slice's `--help` read |
| 8 | B6 (substituting the allocator inside the Mojo compiler process) | **BLOCKED** - unchanged from F-001 (prebuilt stripped binary, internal TCMalloc-derived surface, no source checkout / Bazel on the Pi); not attempted | F-001 Section 6 |
| 9 | X1 (original Phase 1 failing command) | **BLOCKED** - unchanged (INC-F001-01..03); F-008 owns the blocker record. Nothing in this slice is an X1 result | F-001 Sections 5, 7 |
| 10 | Allocator-use proof at E-004/E-005 strength on the Mojo path | **OWED to F-005** - explicitly not claimed here (INC-F003-05) | F-001 Section 7, row T7+X2 |

## 2. Design: the Mojo program (`tests/mojo/test_f003.mojo`)

Original, clean-room Mojo source written for this slice. It
performs exactly the lifecycle the task names, printing one
PASS/FAIL line per step and exiting non-zero on any failure:

1. `mimir_malloc(64)`; write the known pattern
   `byte(i) = (i * 3 + 17) mod 256` over all 64 bytes; read
   back byte for byte.
2. `mimir_calloc(16, 4)` (64 bytes); verify every byte is 0.
3. `mimir_realloc(block1, 256)`; verify the first 64 bytes
   are preserved byte for byte, then write and re-verify the
   pattern across the grown tail (bytes 64..255), proving
   the new extent is writable.
4. `mimir_free` both blocks (the realloc result and the
   calloc block); PASS records that both calls returned
   normally and execution continued to the summary line
   (the ABI gives `mimir_free` no failure channel -
   INC-F003-04).

Summary line on success: `F-003 RESULT: PASS (4/4 steps)`,
exit 0.

### 2.1 FFI form, and how it was confirmed (not guessed)

The call form is Mojo's foreign-function surface
`external_call`, taken from on-Pi Mojo 1.1.0 evidence, never
from memory:

- `from std.ffi import external_call`, used as
  `external_call["name", ReturnType, ArgType...](args...)` -
  the exact form in working programs built and run with this
  toolchain on this Pi: `/home/pi/c2/spikes/phase_a/
  gguf_parse_spike.mojo` (fopen/fseek/ftell/fread) and
  `neon_fp16_matmul.mojo` (clock_gettime), and across
  `/home/pi/c2/aesir_engine/` (open64/fstat/read/close/...).
- Type mapping (C ABI -> Mojo): `void *` ->
  `Pointer[UInt8, MutUntrackedOrigin]` (`std.memory` /
  `std.origin`, the pointer type every one of those programs
  uses); `size_t` -> `UInt64`; `void` return -> `NoneType`
  (the form `/home/pi/c2` uses for
  `external_call["_exit", NoneType]`, e.g.
  `aesir_engine/core/posix_process.mojo`).
- The four wrappers in the program are one `external_call`
  each, with the header's signatures from
  `include/mimir_alloc.h`: `mimir_malloc(UInt64)`,
  `mimir_calloc(UInt64, UInt64)`,
  `mimir_realloc(Pointer, UInt64)`, `mimir_free(Pointer)`.

Provenance honesty (INC-F003-01): the `/home/pi/c2` programs
are out-of-repo Aesir material with no MimirAlloc provenance
record - the same caveat class as INC-F001-11. They are used
here ONLY as evidence of the toolchain's FFI syntax (what the
installed compiler accepts), which the compiler then confirmed
directly by accepting this slice's own source. No Aesir code
was copied; the F-003 program is original to this repository.

### 2.2 The null-check idiom (a real toolchain finding)

The first build attempt of this program FAILED, and the
failure is part of this slice's evidence. Testing a returned
pointer with `if not p1:` is a compile error on this
toolchain - the compiler's exact words:

```
error: Pointer is non-null by design, so Bool(ptr) is not
meaningful. To model a null pointer, use
`Optional[Pointer[...]]` and check with `Bool(opt_ptr)` /
`!= None`.
```

(three occurrences, one per null test; build exit 1, before
any link step). The fix uses the address-test idiom proven in
the on-Pi programs: `Int(ptr) == 0`, the `ptr_addr` helper in
`/home/pi/c2/spikes/phase_a/posix_baseline.mojo` (which also
proves the reverse construction,
`Pointer(unsafe_from_address=Int)`). With that single change
the same source compiled and linked cleanly. Recorded as
INC-F003-02: any future Mojo consumer of a nullable C ABI on
this toolchain must model null by address (or by Optional);
the C idiom does not transfer literally.

### 2.3 Realloc failure posture

If `mimir_realloc` were to return null, the header contract
(by analogy, see `include/mimir_alloc.h`) leaves the original
block owned and unchanged. The program therefore tracks the
live pointer either way and frees exactly one block for the
malloc/realloc chain - no double-free, no leak-by-design on
the failure branch. (Not exercised at runtime in this slice:
the recorded realloc succeeded.)

### 2.4 Failure exits

Every failure branch prints its FAIL line and then raises
(`raise Error(...)`) after freeing whatever the program still
owns, so the process exits non-zero by Mojo's unhandled-error
path. That this path really exits non-zero on this toolchain
is itself recorded, not assumed: the recorded run builds and
runs a three-line probe (`def main() raises: ... raise
Error("probe failure")`) which prints its line, reports
`Unhandled exception caught during execution: probe failure`,
and exits 1.

## 3. Exact commands

Mojo invocation path used (F-001 INC-F001-05 discipline -
`mojo` is a wrapper, and the wrapper is what was used,
exactly as it stands):

```
$ which mojo
/usr/local/bin/mojo
$ cat /usr/local/bin/mojo
#!/bin/bash
exec /home/pi/.pixi/bin/pixi run --manifest-path /home/pi/mojo-dev/pixi.toml mojo "$@"
$ mojo --version
Mojo 1.1.0 (8189361e)            (exit 0)
```

Library (the banked target, unchanged by this slice):

```
$ make lib                       (exit 0)
  -> build/libmimiralloc.so; nm -D shows exactly four T
     exports: mimir_malloc, mimir_free, mimir_calloc,
     mimir_realloc (recorded run).
```

The F-003 build - the demonstrated B2 boundary, verbatim
(the same line the additive Makefile target runs):

```
$ mojo build -o build/test_f003 tests/mojo/test_f003.mojo \
    -Xlinker -Lbuild -Xlinker -lmimiralloc \
    -Xlinker -rpath -Xlinker '$ORIGIN'
(exit 0; no stdout/stderr)
```

Control build (Section 5.2):

```
$ mojo build -o /tmp/test_f003_nolink tests/mojo/test_f003.mojo
(exit 1; undefined references - see Section 5.2)
```

Reproduction from a clean checkout of this commit:
`make lib && make test-f003` (the standalone target rebuilds
`build/test_f003` with the exact command above and runs it;
`make test` neither needs nor invokes Mojo).

## 4. ELF evidence (from the recorded run)

`file build/test_f003`:

```
ELF 64-bit LSB pie executable, ARM aarch64, version 1 (SYSV),
dynamically linked, interpreter /lib/ld-linux-aarch64.so.1,
BuildID[sha1]=7500654b84e01da2dec558ce2c8ecf9cb0fe87e2,
for GNU/Linux 3.7.0, not stripped
```

`readelf -d build/test_f003` - the three NEEDED entries and
the RUNPATH (29 dynamic entries in total; full table in the
recorded run):

```
(NEEDED)  Shared library: [libKGENCompilerRTShared.so]
(NEEDED)  Shared library: [libmimiralloc.so]
(NEEDED)  Shared library: [libc.so.6]
(RUNPATH) Library runpath: [/home/pi/mojo-dev/.pixi/envs/default/lib:$ORIGIN]
```

**Answer to the slice's DT_NEEDED question: YES -
`libmimiralloc.so` is a DT_NEEDED of the generated Mojo
executable.** The `-rpath $ORIGIN` argument landed as the
tail of RUNPATH (the toolchain prepends its own env lib dir -
INC-F003-03).

Undefined dynamic symbols of the executable (`nm -D` /
`readelf -sW`, full tables in the recorded run) include
exactly the four namespaced symbols, each UND, FUNC, GLOBAL:

```
U mimir_malloc
U mimir_calloc
U mimir_realloc
U mimir_free
```

No other `mimir_*` name appears, and the `.so` side exports
exactly the same four as `T` (Section 3). `ldd` resolves the
dependency to this tree's library, not to any system copy:

```
libmimiralloc.so => /home/pi/mimiralloc-team-03/build/libmimiralloc.so
```

## 5. Run evidence

### 5.1 The run (recorded run, verbatim)

```
$ ./build/test_f003
STEP1 mimir_malloc(64) + pattern write/readback: PASS
STEP2 mimir_calloc(16, 4) arrives zeroed (64 bytes): PASS
STEP3 mimir_realloc grow 64 -> 256, contents preserved + tail writable: PASS
STEP4 mimir_free(realloc block) + mimir_free(calloc block): PASS
F-003 RESULT: PASS (4/4 steps)
run exit: 0
```

The same output was reproduced by the standalone target
(`make test-f003`, exit 0) inside the same recorded run, and
the native C suite gate passed with the slice in place
(`make test`, exit 0, final line of the recorded run).

### 5.2 The control, and what differs

The control is meaningful and was run: the identical source
built with the identical command minus every MimirAlloc
`-Xlinker` argument. It does not produce an executable at
all - the link fails (exit 1) with undefined references to
`mimir_malloc`, `mimir_calloc`, `mimir_realloc`, and
`mimir_free` at six call sites (`mimir_free` is called from
three places in the source). Two conclusions, no more:
(a) no other component of the Mojo toolchain (stdlib,
runtime libraries, libc) provides the namespaced symbols -
they come from `libmimiralloc.so` or from nowhere; (b) the
B2 link placement is therefore load-bearing for the B1
calls, not decorative. A "run without the allocator" leg is
not meaningful beyond this: there is no binary to run.

### 5.3 What this evidence does and does not show

Shown: the calls in the generated program resolve, through
its DT_NEEDED entry, to the library this tree builds; the
servicing behaviour the program can observe (pattern
integrity, calloc zeroing, realloc preservation, clean
frees, clean exit) is correct. Not shown (F-005's scope):
loader binding records (`LD_DEBUG=bindings`) tying each
call to that library instance at runtime, and any
implementation-internal effect (counters / mapped-bytes
movement) of the E-005 kind. Per F-001's discipline, a name
match plus correct observable behaviour is this slice's
finding; the binding-grade upgrade is F-005's deliverable,
and no sentence in this document claims it early.

## 6. Inconsistencies recorded in this slice - INC-F003-NN

None of these is silently resolved. Each names its owner or
its resumption point.

- **INC-F003-01 - F-001's "mechanism cannot be confirmed
  from the Pi artifact" is resolved by a different evidence
  kind than F-001 anticipated.** F-001 rated B1
  documented-only because the installed stdlib is
  compiled-only and no banked source describes the Mojo FFI
  path. This slice confirmed the path from working on-Pi
  Mojo 1.1.0 programs OUTSIDE the repository
  (`/home/pi/c2/...`, no MimirAlloc provenance record - the
  INC-F001-11 caveat class) plus direct compiler acceptance
  of this slice's own original source. The B1 rating is
  therefore PROVEN-by-demonstration, with the syntax
  evidence's provenance stated openly. Owner: F-002, which
  may supersede the syntax citation with genuine Modular
  documentation if it finds any; the demonstration stands
  either way.
- **INC-F003-02 - Mojo's Pointer is non-null by design, so
  C's nullable-return idiom has no literal Mojo form.**
  `Bool(ptr)` is a compile error on this toolchain (exact
  error in Section 2.2; the slice's first build failed on
  it). Nullable C returns must be modelled by address test
  (`Int(ptr) == 0`, the proven on-Pi idiom) or by
  `Optional[Pointer[...]]`. Owner: every later Mojo slice
  (F-004/F-005 use the same idiom); recorded here so the
  next team does not re-derive it from a failed build.
- **INC-F003-03 - The generated executable's RUNPATH is not
  this slice's alone.** `readelf -d` shows RUNPATH
  `[/home/pi/mojo-dev/.pixi/envs/default/lib:$ORIGIN]`: the
  toolchain prepends the pixi env's lib dir (for its own
  runtime libraries) ahead of the `$ORIGIN` this slice
  requested. Consequence: the executable as built depends
  on the pixi env path existing at runtime, while
  `libmimiralloc.so` itself resolves via `$ORIGIN` (the
  executable's own directory, `ldd`-proven). Portability
  of generated executables beyond this Pi is unaddressed.
  Owner: F-002 (toolchain record) / F-006 (comparison
  discipline must keep both legs in this same environment).
- **INC-F003-04 - Step 4's PASS is a weak observation, by
  the ABI's own design.** `mimir_free` returns void and has
  no failure channel; its PASS records only that both calls
  returned normally and the process reached a clean exit.
  A free that silently corrupted allocator state would pass
  this slice undetected. That is precisely the gap F-005's
  implementation-effect proof exists to close; this slice
  does not claim otherwise. Owner: F-005.
- **INC-F003-05 - This slice's servicing proof is
  verdict-plus-linkage grade, not binding grade.** DT_NEEDED
  + UND entries + `ldd` resolution + correct observable
  behaviour show the calls are serviced through the named
  library; they do not include a runtime binding record
  (E-004's `LD_DEBUG=bindings` shape) or an
  implementation-internal effect (E-005's shape), both of
  which F-001's matrix assigns to F-005. Any reader who
  needs the stronger claim must wait for F-005; this
  document's PROVEN labels are scoped to the grade stated
  in Section 1, row 3. Owner: F-005.
- **INC-F003-06 - `make test-f003` is PATH-dependent by
  design.** The standalone target invokes the `mojo` found
  on PATH (on the Pi, the pixi wrapper per INC-F001-05);
  on a machine without Mojo the target fails at its build
  line. This is the intended guard shape: `make test` -
  whose composition this slice verified unchanged (the
  Makefile diff is append-only) - never references Mojo
  and passed in the same recorded run. Owner: none;
  standing note for whoever runs the target elsewhere.

## 7. What this slice does not do

- It does not exit Phase 6, prove X1, or describe the Pi
  result as the tablet failure fixed (F-001 Sections 5, 7;
  X1 is BLOCKED and F-008 owns the record).
- It adds no symbol, no shim, no standard-name surface -
  B3/B4/B7's new-surface decisions remain untaken future
  decisions, exactly as F-001 left them.
- It does not touch the compiler's or the stdlib's internal
  allocations, and claims nothing about them.
- It installs nothing and downloads nothing; the existing
  pixi env was used exactly as the wrapper uses it.
- It does not modify `README.md`, `include/mimir_alloc.h`,
  any C source, or the composition of `make test` (the
  Makefile change is one appended standalone target block,
  22 insertions, 0 deletions).
