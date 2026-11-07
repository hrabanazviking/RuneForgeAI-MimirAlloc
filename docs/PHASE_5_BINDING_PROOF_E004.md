# E-004 - Independent test executable binding proof (dynamic route proven)

Phase 5 (ABI completeness and interposition), Wave 5, slice 4 of 9.
Base: 966214d (E-003 banked; Wave 5 = 3 of 9 at slice start).
Branch: yrsa/mimiralloc-04. Worktree: /home/pi/mimiralloc-team-04
on purpleviking1 (Raspberry Pi, Cortex-A76), user pi.
Recorded run date: 2026-11-06T19:28:32-05:00. Toolchain: Debian
GNU/Linux 12 (bookworm); cc 12.2.0, GNU ld 2.40; kernel
6.12.112-v8-16k-hailo+; getconf PAGESIZE = 16384; glibc 2.36.
Pi load stayed light (sequential makes only; free -g showed
13 GiB available; Samtal unimpeded).

Governing texts: roadmap Phase 5 exit sentence (independent test
executable and target tool show the intended symbols binding to
MimirAlloc and pass lifecycle tests; exact link route recorded);
docs/PHASE_5_FOUNDATION_PLAN.md (E-001); docs/SHARED_LIBRARY_BUILD.md
(E-002); docs/PHASE_5_LINK_ROUTES_E003.md (E-003, Sections 4-5:
R2 owed, exact proving commands in its Section 4.2 / recorded
run S7, and the naming of THE exact link route left to E-004).

## 0. Bottom line

- R2 - shared library, dynamic link: PROVEN. INC-E003-01 is
  CLOSED by this slice (Section 3).
- New independent executable tests/abi/test_e004.c (159 lines,
  5,730 bytes), linked only against build/libmimiralloc.so
  via make lib + make test-e004, passes 19/19 lifecycle
  checks, exit 0, repeatedly in the recorded run.
- Binding proof (recorded run S3-S5): the executable object
  and the executable dynsym carry exactly the four mimir_*
  symbols as undefined (U); readelf -d shows DT_NEEDED
  libmimiralloc.so; ldd resolves it to the built .so in
  this worktree; LD_DEBUG=bindings shows exactly one
  binding line per public symbol, each binding
  ./build/test_e004 to the built libmimiralloc.so for
  mimir_malloc, mimir_free, mimir_calloc, mimir_realloc.
- Exact link route for Phase 5 exit purposes is named in
  Section 4: R2 dynamic (with R1 static, proven by E-003,
  as the recorded alternative).
- make test composition unchanged, exit 0 (recorded run S8).
  README.md untouched. One commit; no push or merge.

Evidence file: tests/abi/recorded_run_e004.txt - every
command with exit codes, sections S1-S8. This document
summarises it; the recorded run is the record.

## 1. The independent executable

tests/abi/test_e004.c is new in this slice and independent
of test_e002.c / test_e003.c (different sizes, patterns and
check set; written for this slice). It uses only the four
public symbols from include/mimir_alloc.h, no internal
name and no standard-name allocator entry point. Its
19 checks: two distinct live malloc blocks with undisturbed
patterns and fundamental alignment; calloc zeroed at return
then written; realloc grow 192->640 preserving the first
192 bytes; realloc shrink 640->40 preserving 40 bytes;
realloc(NULL,56) as malloc; malloc(SIZE_MAX) and
calloc(SIZE_MAX,2) refusals returning exactly NULL;
free(NULL) no-op; frees of live blocks; a fresh malloc
after the frees succeeding and writable over its extent.
Zero-size and errno are not checked (U-09 / U-10 unresolved).

Makefile wiring is additive only: a test_e004.o rule, a
build/test_e004 rule linking the object with
-Lbuild -lmimiralloc -Wl,-rpath,$ORIGIN (the same route
E-002 used for its consumer), and a standalone test-e004
target. make test is untouched.

## 2. Binding proof (recorded run S3-S5)

- nm build/test_e004.o: U mimir_malloc, U mimir_free,
  U mimir_calloc, U mimir_realloc (plus libc imports).
- nm -D build/test_e004: the same four symbols undefined;
  count of U mimir_ lines = 4. readelf --dyn-syms entries
  4, 11, 13, 15 are those four UND symbols.
- readelf -d build/test_e004: DT_NEEDED libmimiralloc.so
  and libc.so.6; RUNPATH $ORIGIN.
- ldd build/test_e004: libmimiralloc.so resolves to
  /home/pi/mimiralloc-team-04/build/libmimiralloc.so.
- LD_DEBUG=bindings run exits 0 with 19/19 PASS on stdout;
  stderr binding lines binding ./build/test_e004 to the
  built libmimiralloc.so: exactly 1 per public symbol
  (4 total), quoted verbatim in the recorded run S5.
- The library itself (S1): make lib exit 0; nm -D
  --defined-only lists exactly the 4 public symbols
  (count 4); the .so is 71,048 bytes, BuildID unchanged
  from E-002 (a25ec2c4...), i.e. this slice proves binding
  to the banked library, not a rebuilt variant of it.

Binding here is load-time/run-time binding of the
executable own undefined references, shown by the loader
itself - stronger than E-003 R1 static presence evidence,
and exactly the evidence E-001 Section 5.2 requires for R2.
Presence-and-binding still is not a serviced-call counter:
the allocation counter / audit hook item remains E-005
scope, unchanged (see INC-E004-03).

## 3. INC-E003-01 closure - R2 commands executed (S6)

Every command recorded in E-003 recorded_run_e003.txt S7
was executed verbatim in form (ABSDIR =
/home/pi/mimiralloc-team-04/build), with the E-003 consumer
source tests/abi/test_e003.c, now that E-002 is banked in
this base (make lib, src/api/mimir_alloc.map and
build/libmimiralloc.so all present):

- compile test_e003.c to build/test_e003.o: exit 0.
- link with -Lbuild -lmimiralloc -Wl,-rpath,<ABSDIR> to
  build/test_e003_dynamic: exit 0.
- ./build/test_e003_dynamic: E003 16/16, RESULT PASS,
  DYNAMIC_RUN_EXIT=0.
- nm -D --defined-only on the .so: exactly the 4 symbols.
- readelf -d on the consumer: DT_NEEDED libmimiralloc.so,
  RUNPATH the absolute build dir.
- ldd: libmimiralloc.so resolves to the built .so.
- nm -D on the consumer: the four symbols undefined (U).
- LD_DEBUG=bindings | grep mimir: one binding line per
  public symbol to the built libmimiralloc.so; pipe exits
  0 / grep 0. (The grep also matches the .so own libc
  bindings because their lines name the .so path; the
  consumer binding lines are the four quoted in S6.)

Verdict: R2 moves from OWED (E-003) to PROVEN.
INC-E003-01 is CLOSED. INC-E003-03 is confirmed in passing:
at this base the -L/-l form indeed binds the .so, which is
why E-003 form B is not cited as static evidence here.

## 4. Exact link route (Phase 5 exit item)

Named on the dual-route evidence (E-003 R1 + this slice R2,
same consumer family, both green):

- THE recorded route for a dynamically bound consumer:
  build the library with make lib, compile the consumer
  against include/mimir_alloc.h, link with
  -Lbuild -lmimiralloc and an rpath naming the library
  location ($ORIGIN for the in-tree standalone targets;
  the absolute build dir in the E-003 S7 form), and run -
  the loader binds each undefined mimir_* reference to
  libmimiralloc.so, as recorded in S5/S6.
- The static alternative remains proven (E-003 R1:
  explicit archive path build/libmimiralloc.a; DT_NEEDED
  libc only). Consumers choose per deployment; both routes
  are recorded, neither is a drop-in standard-name
  replacement, and R3 (dlopen) stays owed-by-decision and
  R4 (LD_PRELOAD) rejected, as E-001/E-003 recorded.

## 5. make test regression (S8)

make test composition unchanged (same eight binaries as
E-003 recorded): FINAL_MAKE_TEST_EXIT=0; the tail and
per-binary CHECKS/RESULT lines are in the recorded run S8.
make test-e004 re-run in S8: 19/19 PASS, exit 0.

## 6. Inconsistencies recorded by this slice (INC-E004-*)

- INC-E004-01 - No SONAME in the shared library. readelf -d
  on build/libmimiralloc.so (S1) shows no SONAME entry;
  consumers DT_NEEDED therefore names the plain filename
  libmimiralloc.so. Carried from INC-D002-01 / INC-E003-06:
  there is still no install story and no SONAME/versioning
  decision. The binding proof is unaffected in-tree, but an
  installed deployment has no versioned identity yet.
  Status: OPEN, carried; belongs to a later install slice.
- INC-E004-02 - Two rpath spellings in the proven evidence.
  The standalone targets (E-002, this slice) use $ORIGIN;
  the E-003 S7 form uses the absolute build dir. Both are
  proven; they are not the same route text. Section 4
  records both explicitly so no reader merges them into
  one claim. Status: RECORDED, not a defect.
- INC-E004-03 - Binding is not a serviced-call counter.
  LD_DEBUG proves the loader bound the references and the
  lifecycle proves the calls behaved; neither counts calls
  serviced by MimirAlloc inside the process. The roadmap
  counter/audit-hook proof remains owed (E-005 scope).
  Status: OWED, unchanged by this slice.
- INC-E004-04 - LD_DEBUG grep breadth. The S7 command form
  (grep mimir) also returns the .so own libc binding lines
  because they contain the library path. The four consumer
  binding lines are unambiguous (binding file
  ./build/test_e003_dynamic / ./build/test_e004 to the
  .so), and S5 additionally counts exactly 1 per symbol
  for test_e004. Status: RECORDED; future runs should grep
  for the consumer filename to narrow the output.

## 7. Files changed by this slice

| File | Change |
|---|---|
| Makefile | Additive E-004 block: test_e004.o, test_e004 link (dynamic, $ORIGIN rpath), test-e004 target. make test unchanged. |
| tests/abi/test_e004.c | New independent consumer: 159 lines, 5,730 bytes; 19 checks, 4 public symbols only. |
| tests/abi/recorded_run_e004.txt | New recorded evidence: sections S1-S8, 548 lines, 30,487 bytes. |
| docs/PHASE_5_BINDING_PROOF_E004.md | This document. |

No source file (src/, include/) changed. README.md
untouched. hidden_files/ASSIGNMENT.md updated in the
worktree (untracked working note, not part of the commit).

## 8. Bottom line, restated

An independent executable now binds dynamically to the
banked libmimiralloc.so with loader-level proof for all
four public symbols and passes its lifecycle 19/19;
E-003 own consumer passes 16/16 through the same dynamic
route under its own recorded S7 commands. R2 is PROVEN,
INC-E003-01 is CLOSED, and the exact link route is named
in Section 4 for E-009 exit packet to cite.
