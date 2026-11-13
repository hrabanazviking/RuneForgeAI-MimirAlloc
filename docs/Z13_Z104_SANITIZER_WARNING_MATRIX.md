# Z13 — Sanitizer and Warning Matrix (Z-104)

Slice Z-104, Wave 12. Base: seat main c3424b2 (banked
Z-001..Z-101) merged into yrsa/mimiralloc-team-02 as
e97a13b. Run on purpleraider (ASUS ROG Flow Z13, x86-64),
2026-11-13. Recorded run:
tests/abi/recorded_run_z13_z104.txt (two runs of
`make test-z104`, same result line for line). Runner:
tests/abi/run_z104.sh. Controls: tests/abi/probe_z104.c.
This slice builds ahead: it banks only after Z-102 and
Z-103 in roadmap order.

SLICE HEADLINE — THE MATRIX IS CLEAN, WITH ONE FILED
PORTABILITY LOSS: the full suite passes under ASan,
UBSan and ASan+UBSan with zero sanitizer reports
(4988/0/77 in each leg), and all three shipped flavours
pass the M-003 extended warning gate with -Werror and
zero compiler warnings on the near-final core. The
detection controls prove both sanitizers can actually
see (a clean suite alone would prove nothing), and the
coverage probe measures the honest boundary of that
claim: ASan does not redzone mimir-served spans. One
finding is filed, not fixed: clang cannot compile
mimir_core.c (F-104-1, S6). No production source changed
in this slice.

## S1. Purpose and scope

Roadmap gate, verbatim: "ASan/UBSan builds of the full
suite plus the warning gates (M-003 heritage) on
x86-64. Gate: matrix report; every finding fixed or
filed with an owner."

The full suite is the standing gate's composition
(`make test`: test_smoke, test_core, test_b007,
test_c005, test_c006, test_c007, test_c008, test_d004).
The warning gate is M-003's: the extended set
-Wconversion -Wshadow -Wstrict-prototypes
-Wmissing-prototypes -Wundef -Wdouble-promotion over
each shipped flavour's canonical build (production,
debug, hardened). The shipped compiler is cc (gcc
15.3.0); a clang compile-only leg is included as an
informational portability check. TSan is out of scope:
the roadmap names ASan/UBSan for this slice.

## S2. Method

Warning legs run per flavour in two passes, M-003's
shape: an ENUMERATION pass (extended set, no -Werror,
make -k, clean tree) capturing every warning, then a
GATE pass (extended set + -Werror). Production builds
`make test lib` (the suite runs inside the gate pass;
counts parsed gate-style). Debug builds
`make DEBUG=1 all lib-debug` and is runtime-graded by
`make DEBUG=1 test-debug` per-binary exits. Hardened
builds `make lib-hardened test-j008`. Every flavour's
shared library must show exactly 13 dynamic exports
(the current public surface; M-003 graded 4 on its
tree — the surface has grown since, by banked slices).

Sanitizer legs build `make test` into private BUILD
directories with -O1 -g and: address; undefined with
-fno-sanitize-recover=all (any UB report aborts, so a
report cannot scroll past as a pass); both together.
A leg passes only on make exit 0 AND parsed counts
4988/0/77 AND zero sanitizer report lines AND no
RESULT: FAIL. Counts are parsed from the leg logs by
the runner, gate-style (SMOKE + CHECKS sums, SKIP
lines counted).

Controls (probe_z104.c) run after the suite legs:
asan-detect writes one byte past a 16-byte glibc block
under the ASan build (must be reported, nonzero exit);
ubsan-detect evaluates INT_MAX+1 under the UBSan build
(must be reported, nonzero exit); coverage reads
__asan_address_is_poisoned for the byte past a glibc
block and the byte past a mimir block under the ASan
build. All raw per-leg logs live in a private /tmp dir
for the run's duration; the recorded run files the
runner's report verbatim.

## S3. Warning matrix results

Enumeration: production 0 compiler warnings, debug 0,
hardened 0. Gate passes (-Werror): all three exit 0,
zero compiler warnings, exports exactly 13 each.
Production suite inside its gate pass: 4988 checks /
0 failed / 77 by-design skips. Hardened test_j008:
RESULT: PASS (25 checks, 0 failures).

Debug runtime grading matches M-003's table exactly:
test_b007 exit 1 and test_c006 exit 1, every other
binary (smoke, core, c005, c007, c008, d004, j004)
exit 0. Those two are the banked INC-J004-02
by-design divergences (checks that encode production
layout arithmetic and diverge under MIMIR_DEBUG);
no run aborted.

The debug builds print 18 GNU-make recipe lines
(overriding/ignoring old recipe for build-debug
targets). That is INC-M003-01, M-003's tool-noise
filing, unchanged: pre-existing at stock flags,
make-level, not compiler output, hiding no compiler
warning (the sweep counts the two classes separately
throughout). Owner stays Makefile / a future
build-hygiene slice.

Verdict: WIN against the heritage gate. M-003 found
two -Wmissing-prototypes warnings on the old core and
fixed them in-slice; on the near-final core the same
sweep finds none — the fix held across 100 slices of
core change and no new warning appeared under any of
the six flags in any flavour.

## S4. Sanitizer matrix results

| leg | make exit | checks/failed/skips | sanitizer reports |
|---|---|---|---|
| ASan | 0 | 4988/0/77 | 0 |
| UBSan (no-recover) | 0 | 4988/0/77 | 0 |
| ASan+UBSan | 0 | 4988/0/77 | 0 |

Every binary's RESULT: PASS lines present (7 per leg;
test_d004 reports via its CHECKS line), zero
RESULT: FAIL. LeakSanitizer, active by default inside
ASan on this platform, reported no leaks: the
allocator's retained spans and caches stay reachable
from its globals at exit, which is the designed
retention behaviour, not a leak.

## S5. Controls and the coverage boundary

Both detection controls DETECTED: ASan reported
heap-buffer-overflow on the glibc-block overflow
(probe exit 1); UBSan reported the signed overflow as
a runtime error (probe exit 1 under
-fno-sanitize-recover=all). The clean legs in S4 are
therefore not vacuous: the same runtimes, in the same
run, caught the same classes of defect on memory they
can see.

Coverage probe, measured: glibc block byte-past
poisoned = 1 (ASan redzone present); mimir block
byte-past poisoned = 0. MimirAlloc serves client
blocks from its own mmap spans, which ASan does not
redzone or poison, so a client overflow inside a mimir
span is invisible to ASan. What the sanitizer legs DO
cover: undefined behaviour anywhere in the allocator
or test code (UBSan is not heap-dependent), stack and
global overflows in instrumented code, and all
glibc-heap traffic in the test scaffolding. Client
traffic inside mimir spans remains the job of the
MIMIR_DEBUG redzones and quarantine (J-004/J-005) and
the allocator's own metadata detection (Wave 8;
Z-103 grades its rate at scale). This slice claims
coverage only inside that boundary — the research
docs' warning against claiming coverage merely
because a tool ran is the reason the controls exist.

## S6. Findings register

- F-104-1 (FILED — PORTABILITY LOSS, owner: the next
  core-touching portability slice, production roadmap
  PBR-053 line). clang 22.1.8 cannot compile
  src/core/mimir_core.c: it rejects the GCC-style
  __atomic_* builtins applied to _Atomic-qualified
  pointers ("address argument to atomic operation must
  be a pointer to integer..."), first at line 783
  (mimir_metadata_detections); the file carries 122
  __atomic call sites. src/api/mimir_alloc.c and
  src/platform/mimir_platform.c both pass clang under
  the full extended set with -Werror. Under the
  shipped compiler (cc/gcc) the same file is
  warning-clean, so the shipped build is unaffected;
  this is a loss for a hypothetical clang build, not
  a defect in the shipped one. The repair — migrating
  the core atomic surface to C11 <stdatomic.h>
  operations or plain-pointer casts, with the
  memory-ordering audit that deserves — is a core
  change far outside a matrix slice's scope and is
  deliberately not attempted here. It is filed so
  Z-105/Z-106 and the production roadmap inherit it
  named, not discovered.
- INC-M003-01 (CARRIED, tool noise): the 18 make
  recipe lines of S3. Owner unchanged.
- Runner defect (FIXED in-slice, in this slice's own
  tooling): the runner's first version kept per-leg
  logs under build/z104-logs/; the Makefile clean
  target removes the whole build/ tree, so the first
  make clean deleted the logs mid-run and every leg
  failed on a missing redirect target. Logs and probe
  objects moved to a private mktemp dir under /tmp;
  the filed runner is the fixed one. Found by the
  matrix's own first runs; no tree file was affected.
- Debug-override note (method, recorded so the next
  runner does not rediscover it): overriding CFLAGS on
  the make command line defeats the Makefile's
  `CFLAGS += -DMIMIR_DEBUG=1` for the DEBUG=1 flavour,
  so the debug leg's override string must carry
  -DMIMIR_DEBUG=1 itself (M-003's recorded lines show
  the same doubling). Without it the debug test
  binaries build as production flavour against debug
  core objects and test_j004 fails a capacity check —
  a harness artefact, proven by the corrected run
  grading exactly as M-003's table.
- No defect was found in production code, the suite,
  or the public headers. Nothing discovered is left
  unowned: F-104-1 and INC-M003-01 carry owners above.

## S7. Verdict against the roadmap gate

Gate: "matrix report; every finding fixed or filed
with an owner." MET. The matrix is S3+S4, filed as
the recorded run; the warning gate is a WIN (zero
warnings, all flavours, -Werror clean, exports 13);
the sanitizer legs are a clean sweep inside the
measured coverage boundary of S5; the one new finding
(F-104-1, clang portability) is filed with an owner
and a named repair shape, and the one carried finding
(INC-M003-01) keeps its owner. No benchmark verdict
belongs to this slice and none is claimed.

## S8. Files and integrity

New: docs/Z13_Z104_SANITIZER_WARNING_MATRIX.md (this
file), tests/abi/run_z104.sh,
tests/abi/probe_z104.c,
tests/abi/recorded_run_z13_z104.txt. Changed: Makefile
(test-z104 block appended at EOF, +18 lines, no
existing line touched). No src/, include/ or README.md
change. Standing gate before and after: GREEN
4988/0/77 (team hidden_files/z104_baseline_gate.log,
z104_gate.log). The slice suite is standalone
(`make test-z104`), so the gate total does not move.
Load stayed light throughout (start 0.17, end 0.57
1-minute figures); no number in this slice is
load-sensitive.
