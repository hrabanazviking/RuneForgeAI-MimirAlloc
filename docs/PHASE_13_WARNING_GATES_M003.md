# Phase 13 — Compiler-Warning Gates Sweep (M-003)

Slice M-003, Wave 13, TEAM-03 (solo lead). Base:
62298e289411396a5c00304045a893b96e7764e3 (the M-001 bank).
Run on Volmarr's Raspberry Pi Mimir (aarch64, user pi),
2026-11-07. Recorded run: tests/abi/recorded_run_m003.txt
(every compile command line, every warning verbatim, all
CHECKS/RESULT summaries).

SLICE HEADLINE — ONE REAL DEFECT FOUND AND FIXED IN-SLICE:
the G-007 tuning pair (mimir_tune_set_retention_bound /
mimir_tune_get_retention_bound, src/core/mimir_core.c:1976
and :1989) was defined with no prototype visible to its own
translation unit. Prototypes added to the internal header
src/core/mimir_core.h (+21 lines). After the fix, ALL THREE
build flavours pass the full extended warning gate with
-Werror and zero warnings. Details in S5. No other source,
header, Makefile or README change; the public header
include/mimir_alloc.h is byte-identical to base.

## 1. Purpose and scope

The roadmap's Phase 13 (docs/MimirAlloc_Research_and_
Development_Roadmap.md, Phase 13 — Security and production
release) names "compiler-warning gates" as one of its six
work items, alongside code review, API/ABI audit, fuzzing,
long soak and target-matrix verification. This slice sweeps
every build flavour the Makefile supports with an extended
warning gate, triages every warning it produces, fixes what
is genuinely defective, and closes with the exact flag set
the release gate (M-009) should require.

Scope of each flavour's sweep: the flavour's canonical
build — production: `make test` + `make lib` (3 library
sources, 8 test sources, 3 PIC objects); debug: `make
DEBUG=1 test` + `make lib-debug` (same units under
MIMIR_DEBUG, plus debug PIC objects); hardened: `make
lib-hardened` + `make test-j008`. Standalone per-slice test
targets outside those canonical builds (test-e002 and kin)
are not separate flavours and were not swept; their sources
compile under the same CFLAGS in their own slices.

Method (recorded run, Method paragraph): per flavour, an
ENUMERATION pass (extended set, no -Werror, `make -k`, clean
tree) so every warning in every unit is captured, then —
after the in-slice fix — a GATE pass (extended set with
-Werror, clean tree). Zero warnings is claimed only where
the gate pass's own compile lines and exit status prove the
gate ran.

## 2. Flavour inventory (as found in the Makefile at base)

Compiler: cc/gcc (Debian 12.2.0-14+deb12u1) 12.2.0.

PRODUCTION (default). Makefile:43:
  CFLAGS = -std=c11 -Wall -Wextra -Werror -pthread
           -Iinclude -Isrc -O2
Warning flags already carried: -Wall -Wextra -Werror.
Targets: all, test, lib, static-lib; objects in build/.
(-O2 arrived with K-004 on measured evidence; see the
Makefile comment at lines 44-51.)

DEBUG. Makefile:61-66 (ifeq ($(DEBUG),1)): BUILD becomes
build-debug and CFLAGS += -DMIMIR_DEBUG=1 (line 63). The
J-005 library leg uses its own variables, Makefile:1247-
1248: DBUILD = build-debug, DCFLAGS = $(CFLAGS)
-DMIMIR_DEBUG=1. Warning flags: inherited from CFLAGS
(-Wall -Wextra -Werror); the flavour adds no warning flag
of its own. Targets: test-debug, test-j004, test-j005,
lib-debug. Per the J-004/J-005 comments, the separate
directory exists so no debug object can be linked into a
production artifact by stale-file accident.

HARDENED. Makefile:1369-1371: HARD = build-hardened;
HCFLAGS = $(CFLAGS) -O2 -D_FORTIFY_SOURCE=3
-fstack-protector-strong -fstack-clash-protection -Wformat
-Wformat-security -Werror=format-security; HARDLINK =
-Wl,-z,relro,-z,now,-z,noexecstack. Warning flags already
carried: all of CFLAGS plus -Wformat -Wformat-security
-Werror=format-security. Targets: lib-hardened, test-j008,
bench-j008. Per-flag verdicts for the hardening set itself
were banked in docs/PHASE_10_HARDENED_BUILD.md (J-008);
this slice does not re-litigate them.

## 3. The extended gate

Candidate extended set (per the slice assignment):
-Wconversion -Wshadow -Wstrict-prototypes
-Wmissing-prototypes -Wundef -Wdouble-promotion, on top of
each flavour's own flags, with -Werror for the gate pass.

Inclusion verdict: ALL SIX flags were used in EVERY
flavour. None was excluded. Evidence: gcc 12.2 supports
all six for C11; across the whole sweep the six produced
exactly two unique warnings (both -Wmissing-prototypes,
S4) and zero from the other five — there was no
false-positive flood to exclude a flag for. The flags were
injected by overriding CFLAGS on the make command line
with the flavour's full base set plus the extended set;
DCFLAGS and HCFLAGS are defined in terms of $(CFLAGS), so
the debug and hardened library builds inherit the extended
set (visible verbatim in the compile lines of recorded-run
Sections C, E and F).

## 4. Sweep results — every warning, triaged

Enumeration totals (compiler warning lines / unique):
production 4/2, debug 4/2 (+18 GNU-make lines, below),
hardened 4/2. The two unique warnings are the SAME pair in
every flavour, each appearing once per compilation of
src/core/mimir_core.c in that flavour (normal + PIC
objects):

  src/core/mimir_core.c:1976:5: warning: no previous
  prototype for 'mimir_tune_set_retention_bound'
  [-Wmissing-prototypes]
  src/core/mimir_core.c:1989:8: warning: no previous
  prototype for 'mimir_tune_get_retention_bound'
  [-Wmissing-prototypes]

(Verbatim, with source-excerpt and caret lines, in
recorded-run Sections A, B and C.)

TRIAGE — the tune pair: REAL DEFECT (minor, latent),
FIXED in-slice. Full reasoning in S5.

TRIAGE — the other five extended flags: NO WARNINGS AT
ALL. -Wconversion, -Wshadow, -Wstrict-prototypes, -Wundef
and -Wdouble-promotion produced zero warnings in every
flavour across all library and test translation units the
flavours compile. There is nothing to triage: no
conversion narrowing, no shadowing, no non-prototype
declaration, no undefined-macro use and no float-to-double
promotion exists in the swept code at base. This is a
measured result from the recorded compile lines, not an
assumption.

TRIAGE — GNU-make recipe warnings (debug flavour only):
TOOL NOISE. The DEBUG=1 builds print 18 make-level lines
('overriding recipe for target build-debug/...' /
'ignoring old recipe...', Makefile:74-83 vs 1253-1290).
Evidence they are pre-existing and unrelated to this
sweep: a stock-flags dry run (make DEBUG=1 -n lib-debug,
no extended flags) prints the same 18 lines; they arise
because the J-005 section defines build-debug targets that
the generic section also defines when BUILD=build-debug.
They are not compiler warnings and no compiler flag can
silence them. Recorded as INC-M003-01 (below) with the
Makefile as owner, deliberately NOT fixed here: reworking
the Makefile's target structure is outside this slice's
deliverables and risks the banked flavour separation; the
lines are harmless (the J-005 recipes are the ones make
uses, as the J-005 design intends).

RUNTIME NOTE (not a warning): in the debug enumeration,
'make DEBUG=1 test' stops at test_b007 (CHECKS: 69 run,
1 failed; the failing check is C1.4 span form). This is
banked, by-design behaviour: INC-J004-02 records that three
banked checks encode production layout arithmetic exactly
and diverge under DEBUG=1 (plan S5.2 permits the debug
layout change). Debug grading is by 'make DEBUG=1
test-debug', which reports each binary's exit separately
(S6).

## 5. The defect and the fix (slice headline)

What the code does: the pair is the G-007 tuning surface
(docs/PHASE_7_TUNING_PARAMETERS_G007.md) — a setter/getter
over the single locked scalar mimir_retention_bound
(src/core/mimir_core.c:882), valid range 0..16, setter
returning 0 / -1 (out of range, nothing changed) / -2
(lock failure, nothing changed).

Why the warning is a true positive: the functions are
INC-E001-07 shape (b) internal state — declared, by banked
design, only in the test-only header
tests/core/mimir_tune_g007.h:55-56, in NO installed header,
and bound local in the shared library. But the defining
file, src/core/mimir_core.c, includes only mimir_core.h
(line 158) — which declared every other internal core
function and not these two. So no translation unit ever
checked the definitions against a declaration. C binds by
name only: had a future edit changed a definition's
signature (say the setter's size_t parameter), the test
consumers would still have compiled against the test-only
header and linked silently to a mismatched definition —
undefined behaviour with no diagnostic anywhere. That is
exactly the failure -Wmissing-prototypes exists to catch,
and exactly why silencing it (a cast, a flag, a pragma)
would have been cosmetic suppression of a true positive,
which this campaign forbids.

The fix: declare the pair in src/core/mimir_core.h,
immediately after the trim block (which uses the identical
pattern: "declared here and re-declared for tests only in
tests/core/mimir_trim_i003.h"), with a comment recording
the shape-(b) status, the semantics, and this slice as the
reason. +21 lines, prototypes only — no definition, no
behaviour, no flag changes. Shape (b) is preserved in
full: mimir_core.h is a src-internal header, not
installed; the public header include/mimir_alloc.h is
untouched; the version script still binds both symbols
local, proven by nm -D staying at exactly 4 in every
flavour after the fix (S6). The test-only header remains
the declaration test consumers include; the two
declarations are textually identical signatures, and the
compiler now enforces that they stay so.

## 6. Gate passes after the fix (all with -Werror)

PRODUCTION — clean `make test lib`, extended set: exit 0,
ZERO warnings. make test: CHECKS: 4590 run, 0 failed.
nm -D --defined-only build/libmimiralloc.so = exactly 4:
mimir_malloc, mimir_free, mimir_calloc, mimir_realloc.
(Recorded-run Section D.)

DEBUG — clean `make -k DEBUG=1 all lib-debug`, extended
set: build exit 0, ZERO compiler warnings (the 18
make-level recipe lines of S4 appear as before; they are
not compiler output). `make DEBUG=1 test-debug` exits:
smoke 0, core 0, b007 1, c005 0, c006 1, c007 0, c008 0,
d004 0, j004 0 — b007 and c006 are the INC-J004-02
by-design divergences (C1.4 production-layout checks), no
run aborted. nm -D on build-debug/libmimiralloc.so = 4.
(Recorded-run Sections B and E; the test-debug table was
read back from the gated build in this run.)

HARDENED — clean `make lib-hardened test-j008`, extended
set on top of HCFLAGS: exit 0, ZERO warnings. test_j008:
RESULT: PASS (25 checks, 0 failures). nm -D on
build-hardened/libmimiralloc.so = 4. (Recorded-run
Sections C and F.)

REGRESSION — clean stock `make test` with the Makefile's
own CFLAGS after the fix: exit 0, CHECKS: 4590 run,
0 failed. The fix changed no stock-build behaviour.

## 7. Release-gate recommendation for M-009

M-009 should require, as the standing Phase 13 release
gate, a clean build of ALL THREE flavours with this exact
flag set (each flavour's own flags retained, extended set
added):

  -Wall -Wextra -Werror
  -Wconversion -Wshadow -Wstrict-prototypes
  -Wmissing-prototypes -Wundef -Wdouble-promotion

with the gate graded as: build exit 0 AND zero compiler
'warning:' lines in each flavour; production `make test`
at CHECKS 4590 run / 0 failed (or the then-current banked
total, read from the run, never assumed); `make DEBUG=1
test-debug` showing no exit other than the two banked
INC-J004-02 divergences (b007, c006) and no aborts;
hardened test-j008 PASS; and nm -D --defined-only =
exactly 4 on all three flavours' shared libraries.

Honest grading at this slice's close: as of the fix in
S5, ALL THREE flavours PASS that gate today (S6) — no
flavour is failed, waived or dropped. Before the fix, all
three flavours FAILED the extended gate on the same two
-Wmissing-prototypes warnings; that failure is recorded in
S4 and in recorded-run Sections A-C, not erased by the
fix. If a future slice's code reintroduces a warning under
any of the six flags, the gate fails by construction
(-Werror), which is the point of requiring it.

One boundary, stated plainly: this gate covers the
flavours' canonical builds (S1 scope). It does not by
itself sweep the standalone per-slice test targets; M-009
may widen the sweep, but should not narrow the flag set.

## 8. INC register (M-003)

- INC-M003-01 (TOOL NOISE, owner: Makefile / a future
  build-hygiene slice): 18 GNU-make 'overriding recipe' /
  'ignoring old recipe' lines in every DEBUG=1 build,
  from the J-005 section redefining build-debug targets
  the generic section also defines. Harmless today (the
  J-005 recipes win, as designed), proven pre-existing at
  base with stock flags. A fix would restructure the
  flavour sections (e.g. guard the generic rules by
  flavour); it is deliberately not attempted inside this
  slice's two-deliverable scope. No compiler warning is
  hidden by these lines: the sweep counted compiler and
  make warnings separately throughout.
- No other INC is opened by this slice. The one real
  defect found was fixed in-slice (S5); nothing discovered
  is left unowned.

## 9. Files and integrity

New: docs/PHASE_13_WARNING_GATES_M003.md (this file) and
tests/abi/recorded_run_m003.txt. Changed by the labelled
defect fix: src/core/mimir_core.h (+21 lines, prototypes
and comment only). README.md, include/mimir_alloc.h and
the Makefile are byte-identical to base 62298e2 (verified
by git diff: empty). Pi load stayed light throughout
(short compiles only); Samtal on :8787 returned 200
before and after the run.
