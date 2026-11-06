# Phase 5 - Foundation Plan: ABI Completeness and Interposition

Slice: **E-001 (team-01)**. Wave 5, Phase 5 foundation plan.
Deliverable: `docs/PHASE_5_FOUNDATION_PLAN.md` (this file only).
Base: `8619a4ba3b6badb58ea3a3cc8001a569a850053b` on branch
`yrsa/mimiralloc-01`, worktree `/home/pi/mimiralloc-team-01`
on Raspberry Pi `purpleviking1`. Governing text: Roadmap
Section 8 Phase 5 ("ABI completeness and interposition"),
read with Roadmap Sections 2 (evidence levels), 3.3
(linkage/integration vocabulary), 5, 6 and 15, the banked
contract specification `docs/contracts.md` (A-008), the
banked header `include/mimir_alloc.h`, and the banked ELF/ABI
inspection `docs/AARCH64_ELF_ABI_INSPECTION.md` (D-002).
Wave 5 slice queue: WAVE_PLAN.md in the coordinator checkout
`~/mimiralloc-forge` (E-002..E-009), read by this team on
2026-11-06. `README.md` was not touched by this slice.

> **PHASE 5 IS NOT EXITED, AND THIS PLAN EXITS NOTHING.**
> The Roadmap Phase 5 exit sentence (Section 8, Phase 5) is,
> in substance: an independent test executable and a target
> tool show the intended symbols binding to MimirAlloc and
> pass lifecycle tests, and the exact link route is
> recorded. At this base **no** Phase 5 exit item is met:
> there is no library target in the banked Makefile at all
> (no `.a`, no `.so`, no SONAME, no install story -
> INC-D002-01), no visibility control or version script
> anywhere in the build, no independent test executable
> that links a MimirAlloc library (every banked test
> executable links the three library objects directly),
> no allocation counter or audit hook, and no recorded
> link route. What exists is a Phase 3 namespaced
> allocator, built and test-green on one target only -
> native ARM64 Linux + glibc on this Pi - whose four
> public symbols are proven at object level (D-002), plus
> one known, recorded ABI defect (INC-D002-02, Section 4)
> that this phase exists in large part to remedy. This
> slice is a plan and an evidence map. It builds nothing,
> runs no allocator test, adds no symbol, and records no
> link route as chosen.
>
> **CARRIED DEBT, STATED BEFORE ANYTHING ELSE:** Phase 4
> exit remains **UNMET** (banked verdict, D-009:
> `docs/PHASE_4_EXIT_EVIDENCE.md` - combined MET 5 /
> UNMET 8 / OWED 15 of 28). The physical Android run is
> OWED: there is no Android NDK on the Pi, no Android
> build of MimirAlloc, and no MimirAlloc run on any
> Android device. Phase 3 exit also remains UNMET in the
> banked record (the x86-64 suite run is OWED, D-005).
> Nothing in this plan, and nothing in Wave 5, closes,
> discharges, or softens either debt. Wave 5 proceeds on
> the Pi-native Linux/glibc target only, under the
> Roadmap Section 8 rule that later phases may begin
> early only when their work cannot misrepresent an
> unproven feature; every Wave 5 claim is therefore a
> Pi-native claim, never an Android, cross-target, or
> Mojo-integration claim (see INC-E001-08).

## 1. What this plan does

1. States the documented symbol contract for the Phase 5
   integration route (Section 3): exactly the four banked
   public symbols, what each promises (by the banked
   analogy mapping to `docs/contracts.md`), and - with
   equal weight - what Phase 5 will **NOT** implement,
   export, or test as if it existed.
2. Records the known defect this phase must remedy,
   INC-D002-02 (Section 4), and the exact remedy plan
   E-002 will execute.
3. Lays out the link-route options table (Section 5):
   every route the roadmap permits for this surface, the
   exact commands/routes E-002, E-003 and E-004 will
   test, and the evidence each route must record. The
   plan honestly leaves the naming of THE exact link
   route to E-004's evidence (Section 5.4).
4. Maps every Phase 5 task and exit item to the Wave 5
   slice (E-001..E-009) that produces its evidence, with
   honest OWED markers (Section 6), including the
   carried items Phase 5 does not close (Section 6.3).
5. Records every inconsistency this team found while
   planning, as INC-E001-* (Section 7). None is silently
   resolved; three of them (INC-E001-01, INC-E001-05,
   INC-E001-07) change what later Wave 5 slices may
   honestly claim or build.

## 2. Evidence actually checked on the Pi in this slice

Only the facts in this section were checked by this team
in this slice. Everything else in this document is cited
to a banked document and labelled as such. No build was
run and no allocator test was run by this slice (the plan
needs inventory and contract facts, not a re-run; the
Phase 3/4 recorded runs stand as banked and are cited,
not restated as new).

Commands run on `purpleviking1` on 2026-11-06, in
`/home/pi/mimiralloc-team-01` unless noted:

- `free -g` - 15 GiB total, 13 GiB available at check
  time. Light load confirmed before any work; nothing
  heavy was run. Samtal and the other Pi residents were
  not disturbed by this slice (documents only).
- `git rev-parse HEAD` - exactly the base above;
  `git branch --show-current` - `yrsa/mimiralloc-01`;
  `git log --oneline -5` - HEAD is the D-009 Phase 4 exit
  evidence packet; `git status --short` - clean except
  the untracked `hidden_files/` assignment directory.
  `git diff` of `README.md` against the base: empty.
- `uname -a` - `Linux purpleviking1 6.12.112-v8-16k-
  hailo+ #1 SMP PREEMPT Sat Oct 10 09:05:25 EDT 2026
  aarch64 GNU/Linux`. `uname -m` - `aarch64`.
  `getconf PAGESIZE` - `16384`.
- `cc --version` - GCC 12.2.0 (Debian 12.2.0-14+deb12u1).
  `ld --version` - GNU Binutils 2.40 (Debian).
- Source and build inspection: `ls src/api src/core
  src/platform` (`mimir_alloc.c`; `mimir_core.c/.h`;
  `mimir_platform.c/.h`); `cat include/mimir_alloc.h`
  in full; `grep` of the Makefile target list - `all` and
  `test` build only test executables (`test_smoke`,
  `test_core`, `test_b007`, `test_c005`, `test_c006`,
  `test_c007`, `test_c008`, `test_d004`, plus additive
  per-slice targets); **no `.a`/`.so` target, no
  `-fPIC`, no `-fvisibility`, no version-script
  reference exists in the Makefile** (INC-D002-01 and
  INC-D002-03, re-verified at this base).
- Internal-symbol inspection: `grep` of `src/core/
  mimir_core.h` and `src/platform/mimir_platform.h` -
  the internal surface is exactly the nine names D-002
  recorded: `mimir_core_alloc`, `mimir_core_free`,
  `mimir_core_calloc`, `mimir_core_realloc`,
  `mimir_platform_page_size`, `mimir_platform_map_
  acquire`, `mimir_platform_map_release`,
  `mimir_platform_global_lock`, `mimir_platform_
  global_unlock`.
- Startup-ordering inspection: `grep` for constructors
  and initialisation in the three source files - there
  is **no constructor, no `pthread_once`, and no init
  function**; the one global lock is a statically
  initialised `pthread_mutex_t` (`PTHREAD_MUTEX_
  INITIALIZER`, `src/platform/mimir_platform.c`), and
  the core states the same design fact ("No allocation,
  no constructor, no init step is needed or performed",
  `src/core/mimir_core.c`). First use needs no prior
  setup call. This is the factual basis E-008 tests.
- Document reads: the Roadmap (Sections 2, 3.3, 5, 6,
  8 in full, 15 register entries U-01/U-03/U-06/U-07);
  `docs/contracts.md` (document control, Sections 1-2
  inventory and Appendix A stability register);
  `docs/AARCH64_ELF_ABI_INSPECTION.md` (D-002, in
  full); `docs/INTEGRATION_LAYER_MAP.md` (A-007,
  linkage vocabulary and route findings);
  `docs/PHASE_2_EVALUATION_PLAN.md` (the four
  distinguished routes: LD_PRELOAD, direct link, static
  replacement, namespaced API); `docs/PHASE_4_EXIT_
  EVIDENCE.md` (D-009, exit verdict and tallies); and
  the Wave 5 queue in WAVE_PLAN.md.

## 3. The documented symbol contract for the integration route

### 3.1 The two contract layers, and why both must be named

Phase 5 inherits **two** different symbol stories, and
this plan's first duty is to keep them separate:

- **The A-008 contract specification** (`docs/contracts.
  md`, Roadmap Section 6) specifies, as PROPOSED
  contracts, the **standard-name** core candidates:
  `malloc`, `free`, `calloc`, `realloc`, `aligned_alloc`,
  `posix_memalign`, plus conditional `reallocarray`,
  `malloc_usable_size`, and the C++ operator family.
  Its own document control says: specification only, no
  implementation exists or is claimed, no ABI version
  assigned. Its final required symbol set is expressly
  **open** (U-01, Roadmap Section 15 Q4), and the
  namespaced `mimir_*` API is expressly **outside its
  Section 6 scope** (U-03, Appendix A: stability
  Unassigned for every `mimir_*` entry).
- **The banked implementation reality** (Phase 3, header
  `include/mimir_alloc.h`) is a **namespaced** surface
  of exactly four symbols, whose contract mapping to
  A-008 is **by analogy only** (the header's own U-03
  caveat): `mimir_malloc`, `mimir_free`, `mimir_calloc`,
  `mimir_realloc`. D-002 proved at object level that the
  API translation unit defines exactly these four and
  nothing else. No standard-name symbol is declared,
  defined, or exported anywhere in the banked tree.

**The Phase 5 integration route is the namespaced
route.** Phase 5 implements, links, and tests the four
`mimir_*` symbols as the documented symbol contract for
the integration route. It does not implement the
standard-name A-008 candidates; those remain PROPOSED
specifications whose final set is still an open project
decision (U-01). Treating the A-008 standard-name
inventory as the Phase 5 deliverable would contradict
the banked header, the banked design record (B-008's
supported-call list is the four `mimir_*` names), and
the Wave 5 queue the keeper derived. The tension itself
is recorded as INC-E001-01 and INC-E001-02; this plan
resolves the **scope** question for Wave 5 only, and
does not close U-01 or U-03.

### 3.2 The contract: exactly four public symbols

| # | Public symbol | Signature (banked header) | Contract (by analogy to A-008, PROPOSED only) |
|---|---|---|---|
| 1 | `mimir_malloc` | `void *mimir_malloc(size_t size)` | Size > 0 and satisfiable: non-null pointer to distinct storage, suitably aligned for any fundamental-alignment type (MA-6.1-MALLOC-01 by analogy). Failure, including oversize: exactly a null return, never a pointer to unallocated storage (MA-6.1-MALLOC-02/-04 by analogy). Contents unspecified, **not** zero-guaranteed. |
| 2 | `mimir_free` | `void mimir_free(void *ptr)` | A live pointer from this allocator family is retired exactly once (MA-6.1-FREE-01 by analogy). `mimir_free(NULL)` is a defined no-op (MA-6.1-FREE-02 by analogy). Foreign, interior, or already-freed pointers are undefined behaviour with no defined error or recovery (MA-6.1-FREE-03/-04 by analogy). Returns no value on any defined path. |
| 3 | `mimir_calloc` | `void *mimir_calloc(size_t nmemb, size_t size)` | Success: non-null pointer to distinct, suitably aligned storage with every byte of the requested extent zero at return (MA-6.1-CALLOC-01/-05 by analogy). Product overflow is checked before the multiplication is relied on: overflow is exactly a null return, no wrapped size is allocated (MA-6.1-CALLOC-02 by analogy); any other failure is exactly null (MA-6.1-CALLOC-03 by analogy). |
| 4 | `mimir_realloc` | `void *mimir_realloc(void *ptr, size_t size)` | Success (new size > 0): caller owns a block of the new size at the returned pointer, which may differ from the input; the first min(old, new) bytes are preserved (MA-6.1-REALLOC-01/-06 by analogy; grow tail unspecified, not zeroed). Failure: exactly null, and the original block stays valid, unchanged, and caller-owned (MA-6.1-REALLOC-02 by analogy). `mimir_realloc(NULL, n)` behaves as `mimir_malloc(n)` (MA-6.1-REALLOC-03 by analogy). No in-place, pointer-identity, or shrink-return promise. |

Cross-cutting contract facts every Wave 5 slice inherits
(all from the banked header and D-008's audit, none new):

- **Failure channel:** the null return is the only
  caller-observable failure channel. `errno` is **not**
  promised (U-10 / V-02 open); the banked sources never
  touch `errno` (D-008). No Wave 5 test may assert an
  `errno` value.
- **Alignment:** the claim is the non-numeric "suitably
  aligned" only; no numeric alignment value is
  contracted (U-15 / V-07 open). Measured fact (D-002,
  D-008): this build in fact returns page-aligned
  storage - a construction observation, not a contract,
  and E-006 must not promote it into one.
- **Zero-size:** unresolved (U-09); `mimir_realloc
  (ptr, 0)` for a live pointer is unresolved (U-12).
  The implementation's current postures (null returns)
  are postures only and settle nothing. Wave 5
  lifecycle tests use the defined cases; they must not
  silently codify the postures as contract.
- **Ownership:** a pointer is owned by exactly one
  allocator family. Freeing or reallocating a foreign
  pointer is UB in-contract; the Phase 5 operational
  rule (Roadmap Phase 5: "Reject ambiguous mixed-
  allocator ownership") is specified in Section 6.2
  item T7 and tested by E-008.
- **Threading and startup:** one simple global lock;
  no constructor, no init call, no setup ordering
  requirement (Section 2, checked in this slice).
  No async-signal-safety claim; no fork-safety claim
  (U-17..U-21 open).
- **ABI stability:** this surface is PROPOSED and is
  **not a stable ABI** (header position; INC-D002-07:
  exports are unversioned, no symbol-version nodes, no
  SONAME exists yet). Phase 5 proves binding and
  lifecycle behaviour for this build; it does not
  freeze an ABI, and no Wave 5 document may call it one.

### 3.3 What Phase 5 will implement and test

Within the four-symbol contract, and only within it:

- **W1 - A real library surface.** E-002 creates the
  shared library and E-003 the static archive, so the
  four symbols become linkable artifacts instead of
  objects linked ad hoc into test executables
  (remedying INC-D002-01 as a side effect of the
  INC-D002-02 remedy, Section 4).
- **W2 - Export discipline.** The shared library's
  dynamic symbol table contains exactly the four public
  symbols as defined function symbols - no more
  (Section 4; proven by E-002, re-proven by E-004).
- **W3 - Both link routes exercised.** Static-archive
  linking and shared-library dynamic linking, each with
  recorded commands and binding evidence (E-003), and
  an independent executable binding proof (E-004).
- **W4 - Call-path proof.** A visible allocation
  counter or explicit audit hook proving the intended
  call path actually invokes MimirAlloc - name match
  alone is not proof (Roadmap Section 3.3 discipline,
  carried through A-007 and the Phase 2 plan) (E-005,
  under the export constraint of INC-E001-07).
- **W5 - Lifecycle proof.** Allocate / fill / verify /
  resize / free lifecycles through the linked library,
  including cross-module ownership within the one
  library instance, and startup-ordering behaviour
  (E-004, E-008).
- **W6 - C/C++ consumption.** The header consumed from
  a C++ translation unit under its existing `extern
  "C"` guards, recorded (E-007). This is consumption
  of the C surface from C++, nothing more.
- **W7 - Alignment as it exists today.** Lifecycle
  tests may verify the documented non-numeric claim
  (returned storage satisfies fundamental alignment,
  checked against `_Alignof(max_align_t)` as D-008
  did). The aligned-allocation **surface** question is
  a decision record only (E-006, Section 3.4).

### 3.4 What Phase 5 will NOT do (no silent ABI expansion)

Each item below is a hard non-goal for every Wave 5
slice. Adding any of them is a silent ABI expansion
and is forbidden by this plan, by the banked header
("Absent symbols are the documented unsupported
behaviour"), and by the Roadmap Phase 5 opening words
("Implement **only** the documented symbol contract"):

- **N1 - No standard-name symbols.** No `malloc`,
  `free`, `calloc`, or `realloc` definitions, and no
  LD_PRELOAD-style global replacement, are added.
  (Consequence for the interposition route: Section
  5.3, route R4; recorded as INC-E001-01.)
- **N2 - No aligned-allocation entry points.** No
  `aligned_alloc`, `posix_memalign`, or `mimir_*`
  aligned variant is declared or defined. E-006
  produces the **decision record** on the aligned
  surface (provide later / not provide / per-target,
  against U-01, U-11 / V-03) plus alignment lifecycle
  tests of the existing four symbols. A decision to
  provide would still need its own later slice and
  contract work; E-006 itself adds nothing.
- **N3 - No conditional A-008 symbols.** No
  `reallocarray`, no `malloc_usable_size`, no
  statistics/trim/query API. Their gates (U-13, U-16,
  U-03) were never opened.
- **N4 - No C++ operators.** No `operator new` /
  `operator delete` variant of any form (U-06 open;
  the A-008 operator family is conditional and its
  pre-main problem is unexamined). E-007 is
  consumption proof only (W6).
- **N5 - No new exported audit symbol by default.**
  The E-005 counter/hook must not become a fifth
  public dynamic symbol; see INC-E001-07 for the
  permitted shapes and the decision E-005 owes if
  none of them suffices.
- **N6 - No ABI freeze, no stability promotion.** No
  SONAME/version-node choice in E-002 may be worded
  as an ABI stability promise (U-07 open; INC-D002-07
  stands until a deliberate, separately decided
  versioning slice exists - none is queued in Wave 5).
- **N7 - No target promotion.** No Wave 5 result
  promotes any target to "supported" (Roadmap
  Section 5 discipline, carried from D-001/D-009),
  and no Wave 5 result is an Android, musl, x86-64,
  or Mojo result.

## 4. The known defect: INC-D002-02, and the E-002 remedy plan

### 4.1 The defect, exactly as banked

INC-D002-02 (banked in `docs/AARCH64_ELF_ABI_INSPECTION.
md`, D-002; re-verified against the sources at this
base in Section 2):

> A shared library linked from the banked objects would
> export **13 dynamic function symbols, not 4**. The API
> object defines exactly the four public symbols, but
> the core object defines `mimir_core_alloc`,
> `mimir_core_calloc`, `mimir_core_free`,
> `mimir_core_realloc`, and the platform object defines
> `mimir_platform_page_size`, `mimir_platform_map_
> acquire`, `mimir_platform_map_release`,
> `mimir_platform_global_lock`, `mimir_platform_
> global_unlock` - nine internal names, all with
> default visibility. Nothing in the banked build hides
> them: no `-fvisibility=hidden`, no version script,
> and no library target at all.

Why it matters, in contract terms: the documented
symbol contract (Section 3.2) is a four-symbol surface.
A 13-symbol `.so` would (a) publish nine internal names
as de facto ABI that consumers could link against and
later break, (b) invite cross-module calls into
internals whose contracts are internal-only (the core
and platform headers are not public headers), and
(c) make the Phase 5 exit claim "the intended symbols
bind to MimirAlloc" ambiguous - binding evidence is
only meaningful against a known, closed export set.
"Exactly four" held at D-002 only for the API object
and for the (non-exporting) test executables; it has
never held for any shared library, because no shared
library target has ever existed.

Companion facts E-002 inherits (all banked, D-002):
INC-D002-01 - no library target, SONAME, or install
story exists; INC-D002-03 - PIC is incidental, not
specified (no `-fPIC`/`-fpic` in CFLAGS; the emitted
code was PC-relative in practice and the ad-hoc `.so`
had no TEXTREL, but the build guarantees nothing);
INC-D002-07 - exports are unversioned and the ABI is
expressly not stable.

### 4.2 The remedy plan E-002 will execute

E-002 (team-02) - shared library build
`libmimiralloc.so` with symbol visibility control /
version script so dynamic exports are exactly the four
public symbols, plus recorded ELF inspection (Wave 5
queue). Under this plan E-002 must:

1. **Add a real library target** to the Makefile
   (additively; `make test` composition and its exit-0
   result must be preserved): build the three library
   objects with `-fPIC` **specified** for the library
   objects (closing the INC-D002-03 gap for this
   target at minimum; whether `-fPIC` also applies to
   the test-object builds is E-002's recorded choice,
   not a silent side change), and link them into
   `libmimiralloc.so`.
2. **Apply BOTH layers of export control**, so the
   closed surface does not depend on one mechanism:
   (a) compile the library objects with
   `-fvisibility=hidden` and mark exactly the four
   public API functions with default visibility at
   their definitions (or an equivalent, explicitly
   recorded mechanism in the API translation unit);
   and (b) link with a version script
   (`-Wl,--version-script=<script>`) whose global list
   names exactly `mimir_malloc`, `mimir_free`,
   `mimir_calloc`, `mimir_realloc` and whose local list
   is `*`. The D-002 remedy named hidden visibility
   **or** a version script; this plan requires both
   because each guards a different failure (a future
   object added without visibility discipline is still
   caught by the script; a future link that drops the
   script is still caught by visibility) - and E-002
   records both mechanisms in its inspection evidence.
   The version script's node name, if any, is a build
   identifier only and must not be worded as ABI
   versioning (N6, U-07 open).
3. **Record a SONAME decision.** Either set
   `-Wl,-soname,libmimiralloc.so.0` (or a recorded
   equivalent) or record explicitly in the E-002
   document why the slice ships no SONAME. Silence is
   not an option: E-003's dynamic route and E-004's
   binding proof both need to know what `DT_NEEDED` /
   `DT_SONAME` values to expect.
4. **Prove the closed surface from the artifact**, in
   a recorded run kept with the slice: `nm -D
   --defined-only` on the built `.so` lists exactly
   four defined function symbols (the four public
   names, type `T`); `readelf --dyn-syms`, `readelf
   -d` (SONAME / NEEDED / TEXTREL absence), and `nm
   -g --defined-only` on the component objects are
   recorded beside it. The nine internal names must
   appear **nowhere** in the dynamic symbol table.
   The pre-fix 13-symbol state is cited from D-002,
   not re-created as theatre.
5. **Prove nothing else regressed:** full `make test`
   exit 0 at E-002's base-plus-slice, recorded; the
   library objects' undefined libc surface is recorded
   and compared against D-002's record (any new
   undefined symbol is named and explained, not passed
   over).
6. **State the limit of the fix honestly:** visibility
   and script control the **dynamic** surface of the
   shared library. They do not change the static
   archive's symbol table (a static consumer links
   objects, and internal names existing in an archive
   member is normal, not a leak - E-003 records this
   distinction rather than claiming a static "export"
   fix), and they do not make the ABI stable (N6).

If E-002 finds any step above impossible as stated
(for example, a toolchain behaviour that defeats one
layer), it records the failure as its own INC-E002-*
and ships the strongest working subset with the gap
named - it does not quietly drop a layer.

## 5. Link-route options

### 5.1 Route vocabulary

Routes are named in the Roadmap's own vocabulary:
Section 8 Phase 5 names shared libraries and
static/dynamic linking; Section 3.3 (via A-007) gives
the linkage findings vocabulary (Statically linked,
Dynamically linked, Explicitly called, Interposed,
Absent); the Phase 2 plan distinguishes the four
integration modes **LD_PRELOAD, direct link, static
replacement, namespaced API**. For this surface the
four modes collapse honestly into the table below:
because the public symbols are namespaced, "direct
link" and "namespaced API" are the same consumption
(one explicit call surface, two link mechanisms), and
LD_PRELOAD interposition of standard allocator names
is not available at all (R4).

### 5.2 The options table

| Route | Mechanism | Exact commands / route to be tested | Tested by | Evidence each route must record |
|---|---|---|---|---|
| **R1 - Static archive, direct link** | `libmimiralloc.a` from the three library objects, linked into the consumer executable at build time; consumer calls the four `mimir_*` names explicitly (Explicitly called + Statically linked finding) | E-003 creates the archive (`ar rcs build/libmimiralloc.a <objs>` via a Makefile target). Consumer: `cc -std=c11 -Iinclude consumer.c -Lbuild -lmimiralloc -pthread -o consumer_static` (equivalently with the explicit archive path). Link order and the full command line are recorded verbatim. | E-003 (route built + recorded); E-004 (independent executable uses it as one of its proven routes) | Recorded link command + link map (`-Wl,-Map`); `nm` on the executable showing the four symbols defined (`T`) and originating from the archive; `readelf -d` showing **no** `DT_NEEDED` for libmimiralloc; lifecycle run output + exit code (E-004); the archive's full member symbol list, with the internal-names-in-archive distinction stated (Section 4.2 step 6) |
| **R2 - Shared library, dynamic link** | `libmimiralloc.so` (E-002) named on the link line; consumer carries `DT_NEEDED` for it and binds at load time (Dynamically linked + Explicitly called finding) | E-002 creates the `.so` (Section 4.2). Consumer: `cc -std=c11 -Iinclude consumer.c -Lbuild -lmimiralloc -pthread -Wl,-rpath,<abs build dir> -o consumer_dynamic` for the in-tree test form; any installed-prefix form is recorded separately if E-003 chooses to test one. | E-002 (library + export proof); E-003 (route built + recorded); E-004 (binding proof) | `nm -D --defined-only` on the `.so` = exactly 4 (E-002); `readelf -d` on the consumer showing `DT_NEEDED` naming the library (+ SONAME expectation per E-002's decision); `ldd` output; `nm -D` / `readelf --dyn-syms` on the consumer showing the four symbols undefined (`U`) there and defined in the `.so`; `LD_DEBUG=bindings` excerpt or an equivalent loader record naming the `.so` as the provider of each of the four symbols; lifecycle run output + exit code (E-004) |
| **R3 - Shared library via `dlopen` / `dlsym`** | Consumer loads `libmimiralloc.so` at runtime and resolves the four names explicitly | **Not queued in Wave 5.** No slice E-001..E-009 is assigned this route. It is recorded here so its absence is a decision, not an oversight: the roadmap's Phase 5 text does not name it, the exit needs one exact recorded route (not three), and the audit-hook proof (E-005) is stronger on a link-time route. A future phase that needs plugin-style loading must queue it deliberately. | Nobody in Wave 5 - **OWED by design, not by failure** | None in Wave 5. E-009's packet must list R3 as untested-by-decision so no later reader mistakes silence for coverage |
| **R4 - LD_PRELOAD interposition of the standard allocator names** | `LD_PRELOAD=libmimiralloc.so` causing the process's `malloc`/`free`/`calloc`/`realloc` calls to bind to MimirAlloc | **NOT AVAILABLE for this surface, and NOT permitted in Phase 5.** The library defines no standard-name symbols (N1), so preloading it interposes nothing: a consumer's `malloc` still binds to libc. Demonstrating "interposition" would require adding standard-name definitions - a silent ABI expansion into the un-decided U-01 territory. Roadmap Section 13's own caution stands over the whole topic: "No default global interposition until well validated" (that gate belongs to Phase 13, not Phase 5). | Nobody - **rejected route, with reason recorded** | This plan is the record. E-009 must carry the rejection (with this reasoning) into the exit packet, so the phase's "interposition" title is never cited as interposition coverage (INC-E001-01) |

Notes that bind all routes:

- **"Static replacement"** in the Phase 2 vocabulary
  meant replacing the allocator **inside** a rebuilt
  binary. For the namespaced surface, R1 is the only
  sense available: the allocator code is inside the
  consumer, but it services only explicit `mimir_*`
  calls - it replaces nothing implicitly. No Wave 5
  document may call R1 a drop-in replacement.
- **Presence is not use.** A symbol on a link line, a
  `DT_NEEDED` entry, or a name match proves presence
  only. Every route's proof is completed by E-005's
  counter/hook evidence that the calls were actually
  serviced by MimirAlloc code (Section 6.2, T5).
- **One library instance.** R2's cross-module test
  (E-008) only proves shared ownership if both modules
  bind to the **same** loaded `.so` instance; mixing
  an R1-linked module and an R2-linked module in one
  process creates two allocator instances, which is
  exactly the ambiguous-ownership case T7 rejects
  (Section 6.2).

### 5.3 Why R4 is rejected, stated once and plainly

The Phase 5 heading contains the word
"interposition", and the exit speaks of symbols
"binding to MimirAlloc". For a namespaced allocator,
binding is proven by link and loader evidence on the
`mimir_*` names (R1/R2 + E-004 + E-005). Interposition
in the LD_PRELOAD sense is a property of
standard-name replacement surfaces, which this
project has not decided to build (U-01 open), has
forbidden itself to grow silently (N1), and has
gated even for the long term ("no default global
interposition until well validated"). Phase 5
therefore delivers the binding half of its heading
completely and the interposition half not at all -
by contract, not by shortfall. INC-E001-01 records
the heading/surface mismatch so the roadmap itself
can be corrected by its owner in due course.

### 5.4 THE exact link route: honestly left to E-004's evidence

The Phase 5 exit requires: "Exact link route
recorded." This plan does **not** pre-name that route.
At this base neither route exists as an artifact, and
choosing between R1 and R2 today would be a decision
without evidence - the precise failure this project's
evidence discipline exists to prevent. The decision
procedure is fixed here instead:

- E-002 lands the shared library + export proof;
  E-003 lands both routes with recorded commands and
  binding records; E-004 runs the **same** independent
  lifecycle executable through **both** routes.
- **E-004's document names THE exact link route** for
  Phase 5 exit purposes, chosen on its recorded
  evidence (binding proof strength, lifecycle result,
  and which route the target-tool demonstration used),
  and states the other route's result beside it.
  Either route is an acceptable winner; an unrecorded
  preference is not.
- E-009's exit packet may claim the exit item "exact
  link route recorded" only by citing E-004's naming.
  If E-004 cannot honestly name one (for example, one
  route failed), the exit item is UNMET with the
  failure recorded - never papered over by this plan.

## 6. Exit-evidence matrix

Status vocabulary (carried from D-009 so Wave 5 grades
in the same words as Wave 4): **MET** - the evidence
the item specifies exists and is recorded; **UNMET** -
some evidence exists but the item as written is not
satisfied; **OWED** - the evidence is wholly absent at
this base and the producing slice is named. Every row
below starts OWED or UNMET at this base: this slice
produces the plan, not the evidence.

### 6.1 Phase 5 exit items (Roadmap Section 8, Phase 5, exit sentence)

| # | Exit item (substance) | Producing slice(s) | Evidence that must exist | Status at this base |
|---|---|---|---|---|
| X1 | An **independent test executable** shows the intended symbols binding to MimirAlloc | E-004 (consuming E-002's `.so` and E-003's routes) | An executable that is **not** one of the banked in-tree test binaries and does not link the loose objects: it links a library artifact (R1 and/or R2), with `nm`/`readelf`/`ldd` (and loader-binding record for R2) showing the four symbols provided by MimirAlloc; source + recorded run kept with the slice | **OWED** - no library artifact and no such executable exist |
| X2 | A **target tool** shows the same binding | E-004, with the term pinned by INC-E001-04 | E-004 must name what "target tool" means for this phase **before** claiming the item: the only honest reading available in Wave 5 is a second, separately built consumer (for example the E-007 C++ consumer or the E-008 multi-module program) binding the same library. A Mojo/Aesir tool demonstration is Phase 6 scope and is **OWED beyond Wave 5** | **OWED** - term undefined in the banked record; Mojo reading OWED to Phase 6 |
| X3 | **Lifecycle tests pass** through the bound route | E-004 (core lifecycle); E-006 (alignment lifecycle); E-008 (cross-module + startup lifecycle) | Recorded runs with check counts and exit codes: allocate/fill/verify/free across the four symbols, `realloc` growth preservation, `calloc` zeroing, `free(NULL)`, failure-null cases, through the **linked library** (not loose objects) | **OWED** - banked lifecycle evidence covers loose-object test binaries only |
| X4 | **Exact link route recorded** | Named by E-004 per the Section 5.4 procedure; routes built by E-002/E-003; carried by E-009 | E-004's document naming one route as THE route, with the verbatim commands and the other route's recorded result beside it | **OWED** - deliberately not pre-named by this plan (Section 5.4) |

**Exit tally at this base: MET 0 / UNMET 0 / OWED 4,
of 4.** Phase 5 exit is **UNMET** (an exit with any
item outstanding is unmet; the tally words follow
D-009's discipline). It becomes claimable only when
E-009's packet can cite recorded evidence for X1-X4
on the Pi-native target - and even then it is a
Pi-native Phase 5 exit, carrying the Section 6.3
debts untouched.

### 6.2 Phase 5 task items (Roadmap Section 8, Phase 5, task sentence, decomposed)

| # | Task item (Roadmap wording, decomposed without merging) | Producing slice | Evidence / deliverable | Status at this base |
|---|---|---|---|---|
| T1 | Implement **only** the documented symbol contract for the integration route | E-001 (this plan, contract stated); E-002/E-003 (library surface = the contract) | Section 3 of this plan; E-002's exactly-4 dynamic export proof; E-003's archive record. Compliance is also negative: every later slice's diff is checkable against N1-N7 | **In progress** - contract stated; implementation OWED to E-002/E-003 |
| T2 | Test **C/C++ variants if needed** | E-007 | A C++ translation unit consuming `include/mimir_alloc.h` under its `extern "C"` guards, compiled + linked + run against the library, recorded. "Variants" means consumption variants only - the C++ operator family is N4-forbidden (INC-E001-06) | **OWED** - header guards exist and are banked, but no C++ consumer has ever been built in this repo |
| T3 | Test **exported symbol visibility** | E-002 (primary); E-004 (re-proof on the consumed artifact) | Section 4.2 step 4 records: exactly-4 `nm -D` result, both control mechanisms named, internal names absent from the dynamic table | **OWED** - and the underlying defect (INC-D002-02) is live at this base |
| T4 | Test **shared libraries** and **static/dynamic linking** | E-002 (shared library); E-003 (both routes) | Section 5.2 rows R1/R2 evidence columns, complete | **OWED** - no library target exists (INC-D002-01) |
| T5 | Prove the intended call path with a **visible allocation counter or explicit audit hook** | E-005 (primary); consumed by E-004/E-009 as the use-proof | Counter/hook counts recorded across a lifecycle run, matching the run's allocation calls exactly (allocations counted = allocations performed, frees likewise), in a shape permitted by INC-E001-07 (no fifth public dynamic symbol) | **OWED** - no counter, hook, or statistics facility of any kind exists in the banked tree |
| T6 | Test **aligned allocations** | E-006 | **Decision record** on the aligned surface (against U-01, U-11/V-03, and the header's absence-is-unsupported position) + alignment lifecycle tests of the existing four symbols (fundamental-alignment checks in the D-008 manner). No aligned entry point is added (N2, INC-E001-05) | **OWED** - no decision record exists; the surface question is genuinely open |
| T7 | **Reject ambiguous mixed-allocator ownership**, and test **cross-module ownership** | E-008 | (a) Same-instance sharing proven: allocate in module A, free in module B, both bound to the one R2 library instance - recorded pass. (b) Rejection specified and demonstrated at the level the contract permits: the plan-level rule is that a pointer must be freed by the family that allocated it and through the same library instance; `mimir_free` of a libc-`malloc` pointer (and vice versa) is UB in-contract (MA-6.1-FREE-03 by analogy), so "rejection" is a documented rule + a test-design prohibition + any debug-detectable behaviour E-008 can honestly record - **not** a promised runtime error, because the contract defines none. If E-008 wants a runtime check, that is a contract change and must be recorded as a decision, not slipped in | **OWED** - no multi-module program of any kind exists in the banked tree |
| T8 | Test **startup ordering** | E-008 | A consumer that allocates before `main` (static-initialiser / constructor-order case for the **consumer's** own objects - the allocator itself needs no init, Section 2) and a first-call-from-multiple-threads case, run against the linked library and recorded. Any failure is a finding against the no-constructor design fact, recorded as such | **OWED** - the design fact is banked (statically initialised lock, no init step) but has never been tested as a startup-ordering claim |

**Task tally at this base: OWED 7 / in progress 1
(T1), of 8.** No task item is met by this plan; T1's
contract statement is the only Wave 5 work product
that exists today, and it is this document.

### 6.3 Carried items Phase 5 does NOT close (honest OWED markers)

These items sit outside the Phase 5 gate, and Wave 5
must never be read as discharging them. E-009's
packet is required to restate every one of them:

| # | Carried item | Banked status | What would close it (none of it in Wave 5) |
|---|---|---|---|
| C1 | **Phase 4 exit** - physical-device allocate/fill/free + multithread tests pass on Android, with page size and ABI recorded (exit items E1-E4 of D-009) | **UNMET** (D-009: exit MET 1 / UNMET 4; combined 5/8/15 of 28) | An Android NDK (install reserved for Volmarr's word), an NDK build, and a physical SM-X400 run of the ready D-007 program with its Section 6 evidence list |
| C2 | **Phase 4 tasks T2/T6/T11/T12/T13** - NDK build exists; target-API decision taken; native Android program run; Termux verified separately; Android app verified separately | **OWED** (D-009) | The same NDK + device road as C1, plus the minimum-API decision (INC-D006-04) |
| C3 | **4096-page-host allocator run** (Phase 4 T9 leg; INC-D003-02) and the **x86-64 suite run** (Phase 3 exit debt; D-005: BUILD leg blocked on the missing x86-64 libc sysroot, install reserved for Volmarr's word) | **OWED** | A 4096-page environment run; an x86-64 sysroot + suite run. Phase 3 exit remains UNMET until the latter lands |
| C4 | **musl leg** of libc compatibility (D-008 INC-D008-06: musl ABSENT - BLOCKED, install reserved for Volmarr's word); **bionic leg** beyond by-analogy (INC-D008-07) | **BLOCKED / OWED** | musl toolchain install at Volmarr's word; the C1 NDK road for bionic |
| C5 | **U-01 final symbol set** and **U-03 namespaced-API contract scope** (and with them U-06 C++ operators, U-07 ABI versioning) | **Open** in `docs/contracts.md` | A project-owner decision at the Roadmap Section 15 Q4 gate. Wave 5 works **under** the open questions (four-symbol de facto surface), it does not answer them |
| C6 | **Mojo integration proof** (Phase 6) - the original failing command succeeding with proof MimirAlloc serviced allocations | **Not started** (Phase 6 scope) | Phase 6 slices, after Phase 5's recorded route exists. X2's Mojo reading lives here |
| C7 | **Route R3 (`dlopen`)** - untested by decision (Section 5.2) | **OWED by design** | A future deliberate slice, if a phase ever needs plugin-style loading |

## 7. Inconsistencies recorded by this slice (INC-E001-*)

- **INC-E001-01 - The Phase 5 heading promises
  "interposition"; the banked surface cannot
  interpose.** The only implemented surface is
  namespaced (`mimir_*`); LD_PRELOAD interposition
  works on standard names, of which the library
  defines none. Either the heading means "binding"
  loosely (in which case Wave 5's R1/R2 evidence
  satisfies the substance), or it means standard-name
  replacement (which is un-decided under U-01,
  N1-forbidden in this phase, and long-term gated by
  Roadmap Section 13). This plan adopts the first
  reading and records the rejection of R4 (Sections
  5.2-5.3); the roadmap wording itself is its owner's
  to correct. No Wave 5 document may claim
  interposition coverage.
- **INC-E001-02 - The contract's final symbol set is
  open, but the implementation's set is closed, and
  Phase 5 must build on the closed one.** A-008
  records the final required set as U-01-open and the
  `mimir_*` API as U-03-out-of-scope, yet Phase 3
  built exactly four `mimir_*` symbols and Phase 5
  must treat them as its contract. This plan does so
  explicitly as a **de facto** contract (banked
  header + B-008 supported-call list + D-002 object
  proof), without closing U-01/U-03. Risk recorded:
  a later Q4 decision that adds symbols will reopen
  the export-control and binding evidence, which must
  then be re-proven, not assumed to carry over.
- **INC-E001-03 - INC-D002-02's remedy is queued as
  an E-002 task, but its prerequisites are three
  separate banked defects.** A version script alone
  cannot be executed as the queue line suggests:
  there is no library target to attach it to
  (INC-D002-01), PIC is unspecified for the objects
  it would link (INC-D002-03), and no SONAME decision
  exists for E-003/E-004 to test against. Section 4.2
  therefore scopes E-002 wider than its one-line
  queue entry (target + PIC + both control layers +
  SONAME decision + inspection). The keeper's queue
  entry is not wrong, but read literally it
  understates the slice by roughly half.
- **INC-E001-04 - The Phase 5 exit's "target tool"
  is nowhere defined.** The banked record names no
  target tool for this phase: candidate readings are
  (a) a second independent consumer program, (b) the
  Mojo compiler/toolchain (whose integration is
  expressly Phase 6's exit, with its own stricter
  wording), or (c) an Android target tool (impossible
  in Wave 5 - C1/C2). This plan pins reading (a) for
  Wave 5 (Section 6.1, X2) and records (b) as OWED to
  Phase 6. Until E-004 states the pinning in its own
  document, X2 cannot be claimed by anyone.
- **INC-E001-05 - "Test aligned allocations" is a
  Phase 5 task with no aligned allocation to test.**
  The header states the absence of any aligned entry
  point **is** the documented unsupported behaviour,
  and A-008 leaves aligned validity unresolved
  (U-11/V-03). A literal reading of the task would
  either test nothing or pressure a slice into
  adding `aligned_alloc`/`posix_memalign` - a silent
  ABI expansion. This plan scopes the task to E-006
  as decision record + alignment lifecycle of the
  existing symbols (T6). If the project owner wants
  aligned entry points tested as implementations,
  that requires a U-01/U-11 decision and a new
  contract slice first.
- **INC-E001-06 - "Test C/C++ variants if needed"
  has no banked definition of "variants" or of
  "needed".** Readings range from "consume the C
  header from C++" (the header already carries
  `extern "C"` guards - the need is demonstrably
  real, since C++ consumers are a stated integration
  direction) to "provide the C++ operator family"
  (conditional in A-008, U-06-open, N4-forbidden).
  This plan pins E-007 to consumption proof only
  (T2). The operator reading is recorded here so its
  exclusion is a decision with a reason, not a gap
  discovered later.
- **INC-E001-07 - The audit-hook task (T5) and the
  exactly-four export contract are in direct tension.**
  A "explicit audit hook" implemented as a new
  exported function (or a public counter variable)
  would make the dynamic surface five symbols and
  re-break the contract E-002 just closed. Permitted
  shapes for E-005, in preference order: (a) a
  test-side counter - the consumer/test harness
  interposes its own wrappers **in the test binary**
  around the four calls and counts there, with the
  library untouched; (b) an internal (hidden-
  visibility) counter readable only through a
  test-only build variant, never in the shipped
  `.so`'s dynamic table; (c) only if (a) and (b)
  are honestly insufficient, a **recorded contract
  decision** naming the new surface and reopening
  INC-E001-02's re-proof obligation. Shape (c)
  without the decision record is a silent ABI
  expansion and is forbidden (N5).
- **INC-E001-08 - Wave 5 runs while Phase 4 (and
  Phase 3) exits are UNMET, which changes what its
  evidence may claim.** Roadmap Section 8 permits
  early starts only where the work "cannot
  misrepresent an unproven feature". Library binding
  on Pi-native glibc does not depend on the owed
  Android runs, so proceeding is legitimate - but
  every Wave 5 evidence record is single-target
  (aarch64 Linux/glibc, 16 KiB pages, this Pi), and
  any wording that lets a Phase 5 result stand in
  for target breadth ("the allocator links and runs
  on its targets") would misrepresent C1-C4. E-009's
  packet must restate the carried debts (Section
  6.3) in its own text; a Phase 5 exit claim without
  them is incomplete by this plan's standard.
- **INC-E001-09 - D-002's remedy wording ("hidden
  visibility OR a version script") and this plan's
  requirement (BOTH) differ deliberately.** Recorded
  so the difference is auditable rather than a
  silent scope change: the queue line for E-002
  itself says "visibility control / version script",
  and Section 4.2 gives the two-failure-modes
  reasoning. If E-002's lead judges one layer
  sufficient, it must rebut that reasoning in its
  own document and record the choice as an
  INC-E002-* - it may not simply follow the shorter
  D-002 wording.

## 8. Bottom line

Phase 5 has a four-symbol namespaced contract
(Section 3), one live defect with a scoped remedy
(Section 4, INC-D002-02 -> E-002), two real link
routes and two honestly closed ones (Section 5), and
a complete evidence map in which **nothing is met
yet**: exit OWED 4 of 4, tasks OWED 7 of 8 with T1 in
progress, and seven carried items (Section 6.3) -
headed by the UNMET Phase 4 exit and its owed
physical Android run - that Wave 5 does not touch.
The next slice is E-002: build `libmimiralloc.so`
with both layers of export control, and prove from
the artifact that the dynamic surface is exactly
`mimir_malloc`, `mimir_free`, `mimir_calloc`,
`mimir_realloc`.

`README.md` was not touched by this slice.
