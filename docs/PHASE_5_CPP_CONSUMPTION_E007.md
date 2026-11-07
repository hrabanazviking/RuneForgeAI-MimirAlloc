# E-007 - C/C++ variants: C++ consumption of the C contract

Phase 5 (ABI completeness and interposition), Wave 5, slice 7 of 9.
Base: 8520a4e29d0c4e7c3e9bfeeaaccec21bbc61fb16 (E-006 banked;
Wave 5 = 6 of 9 at slice start).
Branch: yrsa/mimiralloc-07.
Worktree: /home/pi/mimiralloc-team-07 on purpleviking1
(Raspberry Pi, Cortex-A76), user pi.
Recorded run date: 2026-11-06T20:16:56-05:00.
Toolchain: Debian GNU/Linux 12 (bookworm); cc / c++ / g++
12.2.0 (Debian 12.2.0-14+deb12u1); clang++ 14.0.6 (syntax-only
observation, Section 3); GNU ld 2.40; kernel
6.12.112-v8-16k-hailo+; getconf PAGESIZE = 16384; glibc 2.36.
Pi load stayed light (sequential small makes only; free -g
showed 13 GiB available; Samtal on port 8787 answered
HTTP 200 before and after the runs, recorded run S0 / S8).

Governing texts: roadmap Phase 5 task sentence ("Test
C/C++ variants if needed"); docs/PHASE_5_FOUNDATION_PLAN.md
(E-001 - task T2, work item W6, non-goal N4, INC-E001-06);
docs/IMPLEMENTATION_LANGUAGE_DECISION.md (A-009 - portable
C11 core, the C ABI is the core's own ABI, any C++ surface
only as a Layer-1 shim when a mapped integration actually
requires it); docs/contracts.md (A-008 - Section 2.10
conditional C++ operator family, U-06 open);
include/mimir_alloc.h (the public surface, with its banked
extern "C" guards); docs/PHASE_5_BINDING_PROOF_E004.md
(E-004 - the named R2 dynamic route this slice consumes);
docs/PHASE_5_ALIGNED_SURFACE_E006.md (E-006 - the latest
surface state: exactly four symbols, no aligned entry
points).

## 0. Bottom line

- PROVEN: the C contract is consumable from C++ exactly as
  A-009's decision requires, with NO header change and NO
  library change. A C++17 translation unit
  (tests/abi/test_e007.cpp) includes include/mimir_alloc.h
  under its existing extern "C" guards, exercises the full
  lifecycle through the four public symbols, links only
  against build/libmimiralloc.so in E-004's R2 dynamic
  route, and passes 24/24 checks, exit 0, twice from clean
  (recorded run S4 / S7).
- PROVEN: the header is C++-clean standalone and still
  C11-clean in the same slice. Syntax-only compiles of the
  bare header succeed as C++17 (c++, strict + -pedantic)
  and as C11 (cc, strict + -pedantic), and the two
  header-only translation units that bind the four
  declarations to exactly-typed function pointers compile
  clean as C++ and as C11 (recorded run S3). A clang++
  14.0.6 syntax-only pass over the header also succeeds -
  recorded as a second-compiler observation, not a second
  build proof (INC-E007-04).
- PROVEN: no name-mangling trap. The four mimir_*
  references leave the C++ object UNMANGLED (nm: U
  mimir_malloc / mimir_free / mimir_calloc / mimir_realloc;
  count of mimir_ lines carrying a mangled _Z name = 0),
  while the same object's own C++ class symbols ARE
  mangled (the MimirBuffer methods, demangled in S5.3 as
  the control). The executable carries DT_NEEDED
  libmimiralloc.so, ldd resolves it to this worktree's
  built .so, and LD_DEBUG=bindings shows exactly one
  binding line per public symbol, each binding
  ./build/test_e007 to the built libmimiralloc.so
  (recorded run S5). No header defect was found, so no
  header fix was needed or made: include/mimir_alloc.h is
  byte-identical to base (sha256 55107f05, Section 10).
- DECIDED (and proven in the negative): this slice adds
  NO C++ native surface. No operator new / operator
  delete of any form is declared, defined, or interposed
  in the library or the consumer (recorded run S6 greps
  all exit 1 = none); no C++-name entry point exists; the
  A-008 Section 2.10 operator family stays CONDITIONAL
  with U-06 open, and A-009's Layer-1 shim trigger is NOT
  met by consumption evidence (INC-E007-06). The public
  surface stays exactly four symbols: nm -D on the rebuilt
  .so = 4, re-proven in S2, S6.1 and S7.
- make test composition is unchanged and exits 0 (twice
  in the recorded run, 7 RESULT PASS lines each time).
  README.md untouched (diff vs base = 0 lines). One commit
  on yrsa/mimiralloc-07; no push or merge (the keeper
  banks in roadmap order).

Evidence file: tests/abi/recorded_run_e007.txt (242 lines,
12,048 bytes, sha256 1314f366) - every command with its
exit code, in sections S0-S8, including a full clean
re-run (S7) reproducing every count. This document is the
slice record; the recorded run is the evidence.

## 1. The question, and how "variants" is pinned

Roadmap Phase 5 says "Test C/C++ variants if needed"
without defining "variants" or "needed" (INC-E001-06).
E-001 pinned the only reading Wave 5 may execute: T2 =
consumption variants only. A C++ translation unit consumes
the banked C header under its extern "C" guards, links
the banked library, and runs the lifecycle. The competing
reading - provide the C++ operator new / delete family -
is conditional in A-008 (Section 2.10, every row gated on
"C++ integration requires the variant"), open in U-06,
and forbidden as a silent addition by E-001 non-goal N4.

A-009 fixes the shape of the answer in advance: the core
is portable C11, the C ABI is the core's own ABI, no C++
is used in the core, and a C++ surface may appear only
inside Layer 1 as a shim over the C ABI when a mapped
integration actually requires it. So this slice's success
condition is deliberately narrow: consumption must work
WITHOUT any C++ surface being added. If it had not - if
the header had failed as C++, or the references had left
the object mangled - that would have been a defect to fix
in-slice (if a header defect) or a finding that a shim is
genuinely required. Neither happened (Sections 3 and 5).

Open items this slice touches and does NOT close: U-01
(final symbol set), U-03 (namespaced API has no direct
contract rows; behaviour here is by analogy, as the header
states), U-06 (C++ operator set decision), U-09 / U-12
(zero-size postures - not exercised), U-10 (errno - not
promised, not checked).

## 2. What was built

Three new test files under tests/abi/, one additive
Makefile block (+38 lines, 0 deletions), this document,
and the recorded run. No file under src/ or include/ was
modified.

- tests/abi/test_e007.cpp (250 lines, 9,548 bytes) - the
  C++ lifecycle consumer. Independent of E-004's C
  consumer (different sizes, patterns, and check set, and
  a C++-only class phase). It calls only the four public
  symbols. Its 24 checks: two distinct live malloc blocks
  (128 bytes, patterns 0xA5 / 0x5A undisturbed,
  fundamental alignment via alignof(std::max_align_t));
  calloc(16, 16) zeroed at return, then patterned;
  realloc grow 256 -> 768 preserving the first 256 bytes;
  realloc shrink 768 -> 48 preserving 48 bytes;
  realloc(nullptr, 64) as malloc, writable over its full
  extent; a consumer-side RAII wrapper class (MimirBuffer,
  test-local) owning a 96-byte block - valid, size
  reported, storage written and read back, destructor
  releasing via mimir_free on scope exit, and the
  allocator serving a fresh 80-byte block afterwards;
  malloc(SIZE_MAX) and calloc(SIZE_MAX, 2) refusals
  returning exactly null; free(nullptr) as a defined
  no-op; frees of the live blocks; a fresh malloc after
  the frees, writable over its full extent.
- tests/abi/header_only_e007.cpp (36 lines, 1,402 bytes) -
  the header C++-clean proof TU: includes the header as
  C++ and binds the four declarations to function pointers
  of the exact declared types (Section 3). Compiled to an
  object by make test-e007; never linked or run.
- tests/abi/header_only_e007.c (21 lines, 943 bytes) - the
  C11 twin: the same binding under the banked C11 flags.
  Compiled to an object by make test-e007; never linked
  or run.
- Makefile block - CXX = c++ and CXXFLAGS (-std=c++17
  -Wall -Wextra -Werror -pthread -Iinclude); object and
  executable rules for the consumer (linking -Lbuild
  -lmimiralloc with -Wl,-rpath,$ORIGIN, the E-002 / E-004
  route); object rules for both header-only TUs; and the
  standalone test-e007 target. make test is untouched.

C++ consumption discipline is part of the evidence, and
it is visible in the source: void* results are converted
with static_cast (C++ has no implicit void* conversion),
the null pointer is spelled nullptr (including
mimir_realloc(nullptr, 64) and mimir_free(nullptr), the
C++ spellings of E-004's NULL cases), and the alignment
floor is alignof(std::max_align_t). See INC-E007-07 for
the consequence: the banked C consumers' code style does
not compile verbatim as C++.

## 3. Header proof (recorded run S3)

Five recorded results, all exit 0:

- S3.1 - the bare header, syntax-only as C++:
  c++ -x c++ -std=c++17 -Wall -Wextra -Werror -pedantic
  -pthread -Iinclude -fsyntax-only include/mimir_alloc.h.
- S3.2 - the bare header, syntax-only as C11:
  cc -x c -std=c11 -Wall -Wextra -Werror -pedantic
  -pthread -Iinclude -fsyntax-only include/mimir_alloc.h.
- S3.3 - the bare header, syntax-only as C++ under
  clang++ 14.0.6 (-std=c++17 -Wall -Wextra -Werror):
  exit 0. Observation only (INC-E007-04).
- S3.4 - header_only_e007.cpp compiled with the slice's
  C++ flags: exit 0. nm on the object shows exactly the
  four U mimir_* references, all unmangled (count of
  " U mimir_" lines = 4), beside the TU's own mangled
  pointer symbols - the declarations reach C++ with C
  linkage and exactly the declared signatures, or the
  function-pointer bindings in the TU would not compile.
- S3.5 - header_only_e007.c compiled with the banked C11
  flags: exit 0, nm shows the same four U references.

The guards were therefore correct as banked. The trap
this slice was ordered to watch for - missing or broken
extern "C" guards surfacing as mangled references and a
failed link - does not exist in this header. No fix was
required, and none was invented: the header's sha256 is
unchanged from base (Section 10).

## 4. Lifecycle result (recorded run S4 / S7)

make test-e007 builds the library (make lib), the C++
consumer, and both header-only objects, then runs the
consumer: E007 24/24 checks passed, RESULT: PASS, exit 0.
The S7 clean re-run (make clean, then make test exit 0
with 7 RESULT PASS lines, then make test-e007) reproduces
24/24 and exit 0 exactly. The consumer executable is a
71,640-byte aarch64 PIE (BuildID 31febb36, S4).

The RAII phase deserves one honest sentence of scope:
MimirBuffer proves a C++ class can own and release a
MimirAlloc block through the C surface on the normal
destruction path, and that the allocator is healthy
immediately after. It proves nothing about exception
paths, throwing construction, or allocation before main
during static initialisation - see INC-E007-05; the
startup-ordering half of that question is E-008's T8.

## 5. Binding proof: unmangled, to the C library (S5)

- S5.1 - nm build/test_e007.o: U mimir_calloc,
  U mimir_free, U mimir_malloc, U mimir_realloc. Exactly
  four undefined mimir_ references, spelled as C names.
- S5.2 - mangling control: of the nm lines mentioning
  mimir, the count carrying a mangled _Z name among the
  four public references is 0. (The grep also sees the
  MimirBuffer method lines; S5.3 separates them.)
- S5.3 - contrast control: the consumer's OWN C++ symbols
  in the same object ARE mangled - e.g.
  _ZN12_GLOBAL__N_111MimirBuffer4dataEv, which c++filt
  demangles to "(anonymous namespace)::MimirBuffer::data()".
  Mangling is therefore demonstrably active in this TU;
  its absence on the mimir_ references is the extern "C"
  guards working, not a compiler setting.
- S5.4 - nm -D on the executable: the same four symbols
  undefined in the dynamic table (count 4).
- S5.5 - readelf -d: DT_NEEDED libmimiralloc.so, plus
  libstdc++.so.6, libgcc_s.so.1 and libc.so.6 (the C++
  consumer's own runtime - INC-E007-03); RUNPATH $ORIGIN.
- S5.6 - ldd: libmimiralloc.so resolves to
  /home/pi/mimiralloc-team-07/build/libmimiralloc.so.
- S5.7 - LD_DEBUG=bindings: run exit 0 with 24/24 PASS on
  stdout; exactly 4 binding lines, one per public symbol,
  each binding ./build/test_e007 to the built
  libmimiralloc.so for mimir_malloc, mimir_calloc,
  mimir_realloc, mimir_free.
- S2 - the library side is unchanged C: the .so's only
  DT_NEEDED is libc.so.6, and its defined dynamic exports
  are exactly the four (count 4). Consumption from C++
  costs the library nothing: no C++ runtime, no new
  export, no rebuild variant.

Link success is itself part of the proof: had any
reference left the object mangled, the link against the
C library could not have resolved it - there is no
mangled definition anywhere to resolve against (S6).

## 6. What this does NOT prove (the decision recorded)

Per the slice order, stated as a decision with evidence,
not as a caveat:

1. No C++ native surface exists after this slice, and
   none is implied by it. S6.2: no _Zn / _Zdl symbol in
   the .so dynamic table (grep exit 1). S6.3: no operator
   new / delete DEFINED in the consumer executable (grep
   exit 1) - the consumer does not even define them for
   itself. S6.4: no "operator new" / "operator delete"
   text in src/ or include/ (grep exit 1). A C++ caller
   that writes new / delete keeps using its own C++
   runtime's operators, which do NOT route to MimirAlloc;
   nothing in this slice interposes, redirects, or
   services them. Routing them would be the A-008
   Section 2.10 conditional family under a U-06 decision
   and an A-009 Layer-1 shim slice - neither exists.
2. Consumption does not meet A-009's shim trigger. The
   trigger is "a mapped integration actually requires" a
   C++ surface. This slice is evidence of the opposite
   for the consumption integration: the C ABI sufficed,
   unmodified. The shim remains unbuilt, correctly, and
   no shim work is OWED by this evidence (INC-E007-06).
3. No ABI freeze, no stability promotion. The surface
   consumed is still PROPOSED and unversioned (U-03,
   INC-D002-07 stand). This slice proves this build's
   header and library work from C++ on this target; it
   does not make the C++ consumption a stable-ABI promise
   for other compilers, other language versions, or other
   targets (INC-E007-04).
4. The Phase 5 "target tool" reading (E-001 X2) gains a
   second separately-built consumer here - a C++ one -
   but the Mojo/Aesir tool demonstration remains Phase 6
   scope and OWED beyond Wave 5, unchanged.

## 7. Inconsistencies and limitations (INC-E007-*)

- INC-E007-01 - "Variants" remains undefined in the
  governing texts; this slice executes E-001's pin.
  Roadmap Phase 5's sentence and A-008's inventory still
  admit the operator reading, and only E-001's plan (and
  now this record) excludes it for Wave 5. The E-009
  packet should cite T2 as discharged in the consumption
  reading only, so the Phase 5 record does not silently
  absorb the ambiguity. U-01 / U-06 carry the underlying
  question; neither is closed here.
- INC-E007-02 - The C++ proof is standalone-only, so it
  is not a regression gate. make test does not build any
  C++ file: a future header edit that broke the extern
  "C" guards would leave make test green and fail only
  make test-e007. This matches every prior ABI slice
  (E-002..E-006 are all standalone), and this slice keeps
  that shape by design - but the property is worth
  naming: header C++-cleanliness is proven, not
  continuously guarded. A gate belongs to a future CI /
  ABI-freeze slice (the keeper's manual-only workflow law
  also bars wiring it into hosted CI by default).
- INC-E007-03 - The C++ consumer carries a runtime the C
  consumers do not. DT_NEEDED of build/test_e007 adds
  libstdc++.so.6 and libgcc_s.so.1 beside
  libmimiralloc.so and libc.so.6 (S5.5), because the test
  is linked with the c++ driver and uses C++ facilities.
  This is a consumer-side property of C++ programs, not
  a library property - the .so itself still needs only
  libc.so.6 (S2) - but any future integration note that
  quotes E-004's DT_NEEDED set for "the" consumer must
  not carry it over to C++ consumers. Recorded so the
  Phase 5 link-route record keeps the two sets separate.
- INC-E007-04 - The full proof is one compiler deep and
  one target deep. Compile + link + run evidence is
  g++ 12.2.0 on Pi-native aarch64 glibc only. clang++
  14.0.6 contributes a syntax-only header pass (S3.3) -
  no clang-built consumer was linked or run, no other
  C++ standard was used (C++17 only, per A-009's probe
  precedent), and no musl / bionic / x86-64 / MSVC
  consumption is measured. A clang link/run leg, and any
  second-target leg, are OWED if this evidence is ever
  cited beyond the Pi-native g++ target.
- INC-E007-05 - RAII and pre-main behaviour are only
  partly proven. MimirBuffer exercises normal scope-exit
  destruction inside main. Unproven: destruction during
  exception unwinding, a throwing constructor path, and
  any allocation demanded before main by a consumer's
  static initialisers - the consumer-side face of the
  U-06 pre-main tension (A-008 Appendix note (d)). No
  exception was thrown anywhere in this slice, and the
  test defines no static object that allocates. The
  startup-ordering test proper is E-008's T8; this item
  records that E-007's class evidence must not be cited
  as covering it.
- INC-E007-06 - A-009's shim placement is recorded but
  its trigger discipline cuts both ways. A-009 places
  any future C++ surface in Layer 1 as a shim, gated on
  a mapped integration requiring it. This slice shows
  the most obvious candidate integration - ordinary C++
  consumption - does not require it. If a future slice
  proposes the operator family, its justification must
  therefore come from a different, named integration
  (e.g. a consumer that must route new / delete), not
  from "C++ support" in general: after E-007, C++
  support in the consumption sense already exists.
- INC-E007-07 - The banked C test style is not C++
  source-compatible, and nothing says so until here.
  E-002..E-006's C consumers assign mimir_malloc results
  without casts and pass NULL - valid C11, invalid C++
  (no implicit void* conversion; NULL is usable but the
  casts are not optional). A reader copying E-004's
  consumer into a .cpp file gets compile errors, not a
  MimirAlloc failure. This slice's consumer is the first
  C++-spelled example (static_cast, nullptr,
  std::max_align_t) and this item is the record that the
  difference is a language fact, not a header or library
  defect. No C file is changed by it.

## 8. Self-test checklist (solo slice)

This slice ran solo-sequentially (team lead only, per
the keeper's muster), so verification is this documented
checklist, every item read back from the recorded run or
the tree, not from memory:

- [x] HEAD == 8520a4e and branch == yrsa/mimiralloc-07
  before building (S0; git rev-parse in run).
- [x] free -g showed 13 GiB available before the runs;
  Samtal HTTP 200 before (S0) and after (S8).
- [x] A C++ compiler exists on the Pi and is recorded
  with its version (c++ / g++ 12.2.0, S0) - the honest-
  blocker branch of the slice order was not needed.
- [x] make test exit 0 with composition unchanged - the
  Makefile diff is +38 / -0, additive test-e007 block
  only; no test target was added to make test (S1, S7,
  S8).
- [x] Header proofs: bare-header syntax-only exit 0 as
  C++17 and as C11 (S3.1 / S3.2); header-only TUs compile
  exit 0 as C++ and as C11 with the four U references
  unmangled (S3.4 / S3.5); clang++ syntax-only exit 0
  recorded as observation (S3.3).
- [x] make test-e007 exit 0, 24/24 checks, RESULT PASS,
  in S4 and again from make clean in S7 with identical
  counts.
- [x] Unmangled binding: nm U set exactly the four in the
  C++ object and the executable dynsym; mangled-mimir
  count 0 with the MimirBuffer demangle as control;
  DT_NEEDED libmimiralloc.so present; ldd resolves to
  the worktree .so; LD_DEBUG shows exactly 4 binding
  lines, one per symbol (S5).
- [x] nm -D --defined-only on the rebuilt .so lists
  exactly mimir_malloc, mimir_free, mimir_calloc,
  mimir_realloc; count 4 in S2, S6.1 and S7.
- [x] No C++ native surface: .so operator-symbol grep,
  consumer defined-operator grep, and src/ + include/
  operator-text grep all exit 1 = none (S6).
- [x] Header and every src/ file unchanged: git diff vs
  base touches Makefile only, plus the three new
  tests/abi/ files, the recorded run, and this document
  (S8, final status at commit). Header sha256 identical
  to base (Section 10).
- [x] README.md diff vs base = 0 lines (S8).

## 9. What remains OWED

- Phase 5 exit remains OWED to E-009's packet. This
  slice discharges E-001 task T2 (C/C++ variants, in the
  consumption reading of INC-E001-06 / INC-E007-01) and
  the W6 work item only; T7 / T8 belong to E-008.
- A clang-built C++ consumer link/run leg, a C++
  consumption run on any second target, and any
  threaded C++ consumption run are OWED if this evidence
  is cited beyond the Pi-native single-threaded g++
  target (INC-E007-04).
- The C++ operator family (A-008 Section 2.10) remains
  unimplemented by decision, gated on U-06 and on a
  named integration requiring it (INC-E007-06). Its
  pre-main design problem (INC-E007-05) is unexamined
  and belongs to whichever slice U-01 / U-06 commissions.
- A standing regression gate for header C++-cleanliness
  (INC-E007-02) is OWED to a future CI / ABI-freeze
  slice; Wave 5's standalone-target shape does not
  provide one.
- Carried debts untouched, as E-001 Section 6.3
  requires: Phase 4 exit UNMET (physical Android run
  OWED); Phase 3 exit UNMET (x86-64 suite run OWED);
  no target is promoted to supported by anything here.

## 10. Evidence files for this slice

- tests/abi/test_e007.cpp - the C++ lifecycle consumer
  (250 lines, 9,548 bytes, sha256 c91ad1c5): 24 checks as
  listed in Section 2, linked only against
  libmimiralloc.so.
- tests/abi/header_only_e007.cpp - the header C++-clean
  proof TU (36 lines, 1,402 bytes, sha256 0438f22c).
- tests/abi/header_only_e007.c - the header C11-clean
  proof TU (21 lines, 943 bytes, sha256 820930d7).
- tests/abi/recorded_run_e007.txt - the recorded run
  (242 lines, 12,048 bytes, sha256 1314f366): S0
  environment, S1 make test baseline, S2 nm -D export
  proof, S3 header proofs, S4 test-e007 run, S5 binding
  evidence, S6 absence proofs, S7 clean re-run
  reproducing every count, S8 README + diff scope.
- Makefile - additive test-e007 block only (+38 lines,
  0 deletions; sha256 of the merged file 683e983e).
- include/mimir_alloc.h - UNCHANGED by this slice
  (sha256 55107f05, identical to base): the extern "C"
  guards as banked were sufficient; no fix was needed.
- Prior banked evidence relied on:
  docs/PHASE_5_FOUNDATION_PLAN.md (E-001: T2, W6, N4,
  INC-E001-06), docs/IMPLEMENTATION_LANGUAGE_DECISION.md
  (A-009: the decision this slice executes, including its
  Appendix B C++17 probe precedent on this host),
  docs/contracts.md (A-008: Section 2.10, U-06),
  docs/PHASE_5_BINDING_PROOF_E004.md (E-004: the named R2
  route), docs/SHARED_LIBRARY_BUILD.md (E-002: the
  exactly-4 export discipline this slice re-proves),
  docs/PHASE_5_ALIGNED_SURFACE_E006.md (E-006: the
  surface state at this slice's base).

README.md was not touched by this slice.
