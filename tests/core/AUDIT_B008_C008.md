# C-008 — B-008 audit: unsupported-behaviour enforcement
# (Sections 6–7) and roadmap §9.3 controlled failures

Slice: **TEAM-08 C-008**. Base: **94e1a8d** (banked
C-001..C-007). Audited surface: the banked implementation
line — L1 `src/api/mimir_alloc.c`, core
`src/core/mimir_core.c` / `mimir_core.h`, L6
`src/platform/mimir_platform.c` / `mimir_platform.h`, and
`include/mimir_alloc.h` — against
`docs/PHASE_3_CALLOC_REALLOC_AND_UNSUPPORTED_BEHAVIOR.md`
(**B-008**) Sections 6 (the four-bucket unsupported record),
7 (documentation channels and the supported-call list) and
8 (exit evidence E-1..E-7, for disposition only), and
roadmap Section 9.3 (controlled failure tests), read with
`docs/contracts.md` (A-008) rows as B-008 maps them. Host:
Raspberry Pi `purpleviking1`, runtime system page size
16384 bytes, recorded runs only (see Evidence).

**U-03 caveat, carried unchanged:** the `mimir_*` names
have no direct contract rows in `docs/contracts.md`
([UNRESOLVED: U-03]). Every mapping in this audit, exactly
as in B-008 itself, is **by analogy only, Proposed only** —
never a direct namespaced contract and never an ABI claim.

## Verdict summary

- **No gap in any Defined refusal was found; no `src/` or
  header change is made by this slice.** All three Defined
  refusals of B-008 §6.4 are enforced by the banked core
  (checked arithmetic before any mapping is relied on;
  failure is exactly a null return; a failed `realloc`
  leaves the original wholly preserved) and are re-verified
  in this slice under §9.3 controlled-failure framing, each
  induced failure followed by a sentinel check and a
  recovery probe (`tests/core/test_c008.c`).
- **The §6.1 not-present bucket is verified at link
  level.** The L1 object defines exactly the four
  supported names; no standard name, aligned, usable-size,
  statistics, trim, `reallocarray`, tuning or C++ operator
  symbol exists anywhere in the implementation objects
  (recorded `nm` output, Section A).
- **The §6.2 UB bucket is conforming by non-claim.** The
  design promises no refusal for any UB row, and the
  implementation claims none. Its validation course for
  invalid `free` / `realloc` (registry walk over
  allocator-owned links only, never dereferencing the
  unvalidated pointer; no-op / null on no match) is
  verified by inspection and recorded as a baseline
  course, **not** as a Defined refusal — citing it as one
  would contradict B-008 §6.4's closing rule (INC-C008-03).
  Under the house NO-TEST-UB discipline no UB case is run.
- **The §6.3 unresolved bucket is posture-only / BLOCKED
  throughout.** No posture is invented for any open
  U-item. The zero cases keep the C-005 postures
  (INC-C005-01) and hook C-006 stays BLOCKED; `errno`
  (U-10), fork-child state (U-17..U-21), the teardown
  interval (X-8) and every performance property (X-5)
  remain no-claim, exactly as the header and code state.
- **Roadmap §9.3 disposition:** boundaries, low-limit
  (RLIMIT_AS child), fragmented address space, kill /
  restart (isolation only) and the no-hang / no-overlap
  storm are **run** in `test_c008.c`; thread churn and
  initialisation are run by the C-007 / C-006 suites in the
  same `make test` session and are cited, not redone;
  the injection sweep, the EMFILE/EAGAIN classes and the
  Android / proot tracks are **SKIPPED/NOT-YET** with
  reasons (INC-C008-01, INC-C008-04).
- **New inconsistencies recorded:** INC-C008-01 ..
  INC-C008-06 (register at the end). None required a code
  change inside this slice's scope; each states why.

## Evidence

- C-008 recorded run: `tests/core/recorded_run_c008.txt`
  (`make clean && make test` on the Pi for this slice:
  smoke, C-003 core, B-007 audit, C-005, C-006, C-007 and
  the new `tests/core/test_c008.c`; counts are read from
  that file, not restated from memory here).
- Prior recorded runs, unchanged and cited where a B-008
  row was already proved there: `tests/core/recorded_run.
  txt`, `recorded_run_c004.txt`, `recorded_run_c005.txt`,
  `recorded_run_c006.txt`, `tests/contract/
  recorded_run_c007.txt`, `tests/platform/recorded_run.
  txt`.
- Link-level inspection (run on the Pi for this slice,
  objects built from the banked base plus this slice's
  additive test wiring; `src/` objects unchanged):
  `nm -g --defined-only` over `build/mimir_alloc.o`,
  `build/mimir_core.o`, `build/mimir_platform.o` shows —
  L1: `mimir_calloc`, `mimir_free`, `mimir_malloc`,
  `mimir_realloc` and nothing else; core: the four
  `mimir_core_*` internal names; L6: the five
  `mimir_platform_*` names. No `malloc` / `free` /
  `calloc` / `realloc` standard name, no aligned /
  usable-size / statistics / trim / `reallocarray` name,
  no C++ operator symbol is defined by any object.
- Source inspection (this audit's line-by-line read of the
  five audited files at base 94e1a8d): the only external
  facilities named in `src/` are `sysconf`, `mmap`,
  `munmap`, `pthread_mutex_lock` / `unlock`, `memset` /
  `memcpy`. No `getenv`, no `pthread_atfork`, no signal
  API, no libc allocation call, no stdio exists in the
  implementation tree (the two grep hits for "signal" /
  "reallocarray" are comment text recording the
  non-claims, in `src/api/mimir_alloc.c` and
  `include/mimir_alloc.h`).

Verdict words: **PROVED** = runtime-proved by a recorded
run named in the row. **INSPECTED** = verified by code or
link-level inspection; not triggerable or not permitted
through the public API (INC-C008-03). **BY DESIGN** = the
conforming behaviour is an absence / non-claim, and the
code correctly does not implement or claim it.
**BLOCKED** = an open register item (named) prevents any
verdict; posture-only where a posture exists.

## A. B-008 §6.1 — Not present (N-1..N-11)

| # | Item | Evidence | Verdict |
|---|---|---|---|
| N-1 | No standard-name replacement / interposition | `nm` (Evidence): L1 defines only the four `mimir_*` names; a program linked only to this library keeps its system allocator for standard names; no silent substitution exists in code | PROVED (link level) |
| N-2 | No explicit-alignment entry point (`mimir_aligned_alloc`, `mimir_posix_memalign`, any form) | `nm` + header grep: no such symbol or declaration; header states the absence expressly. Validity semantics stay [UNRESOLVED: U-11] | PROVED (absence), BLOCKED (U-11 semantics) |
| N-3 | No `mimir_malloc_usable_size` | `nm` + header: symbol absent; callers track requested sizes (U-7 reliance bound, Section B) | PROVED (absence) |
| N-4 | No `mimir_reallocarray` | `nm` + header: symbol absent on this target, as on every Phase 3 target; gate U-16 open | PROVED (absence) |
| N-5 | No statistics / trim / release call | `nm`: no such symbol; the only counters are the core's two internal bookkeeping counters, unreachable from the public surface (no accessor exists) | PROVED (absence), INSPECTED (counters internal) |
| N-6 | No runtime tuning (no `mallopt` analogue, no env config, no knobs) | Source inspection: no `getenv` and no configuration surface anywhere in `src/`; MA-6.2-050..053 are N/A for Phase 3 exactly as B-008 records | INSPECTED |
| N-7 | No thread caching / fast paths / size classes / sharding | Core inspection: L3 is pass-through; every call takes the one L6 lock and the mapping slow path; no cache structure exists | INSPECTED |
| N-8 | No C++ `operator new` / `delete` family | `nm`: no C++ symbol in any object; the implementation tree is C11 throughout | PROVED (link level) |
| N-9 | No debug facilities shipped (red zones, canaries, poisoning, quarantine, leak checker) | Source inspection: no such facility or switch exists in the baseline build; §6.2's debug-detectable-only rows therefore describe no Phase 3 capability (Section B) | INSPECTED |
| N-10 | No stable ABI / versioning promise | Header states it verbatim (not a stable ABI, no symbol versioning); no version symbol exists in the objects | BY DESIGN |
| N-11 | No target claim at all | Design posture kept: B-008's list is a design list; this slice's runs are Pi (ARM64 Linux) evidence only, the x86-64 leg is still owed (INC-C007-02), and no target is claimed supported by this audit | BY DESIGN |

## B. B-008 §6.2 — UB, no refusal promised (U-1..U-8)

Bucket rule (B-008 §6 preamble): no Defined outcome, no
error channel, no recovery is promised for any row here,
and "unsupported" must never be read as "safely rejected".
Accordingly this audit issues **no PROVED verdict in this
section**: the conforming state is the non-claim itself,
plus the inspected baseline course where one exists. No
row is runtime-tested (NO-TEST-UB, INC-C008-03).

| # | Violation | Baseline course (inspected) | Verdict |
|---|---|---|---|
| U-1 | Free / realloc of a foreign-allocator pointer | `find_candidate_locked` walks only allocator-owned registry links and compares computed user pointers; the unvalidated pointer is never dereferenced. No match → `free` returns without touching anything, `realloc` returns null. Nothing is forwarded to another allocator (no fallback exists, §7.1 item 4). The in-family pairing (any `mimir_*` block freed by `mimir_free`, resized by `mimir_realloc`) is supported use and is PROVED by the C-005 family-pairing case | INSPECTED (course), BY DESIGN (no promise) |
| U-2 | Double free (incl. after a successful realloc move) | After retirement the descriptor is unmapped with its mapping and unlinked, so a second free finds no candidate and takes the U-1 course. B-008's own caveat stands: if a fresh mapping reuses the address, the stale pointer can alias the new block and detection is impossible — no detection is promised (N-9) | INSPECTED (course), BY DESIGN (no promise) |
| U-3 | Use after free | No mechanism exists or is promised; in this mapping-per-allocation design the storage is typically unmapped at free, so a crash is likely — an observation about the construction, never a safety property (B-008's wording, kept) | BY DESIGN |
| U-4 | Interior / stack / static / literal / cross-process / reconstructed pointer | Same validation course as U-1: exact-pointer match against the registry or no action. No "free checks and ignores" behaviour is documented or promised — the course is a baseline fact, not a contract | INSPECTED (course), BY DESIGN (no promise) |
| U-5 | Read / write outside a live block's requested extent | No bounds checking, red zone or guard exists in the baseline (N-9); nothing in code inspects caller accesses | BY DESIGN |
| U-6 | Metadata-layout reliance / metadata corruption | Descriptors are internal, inside the reserve page, never ABI (contracts §5.2); no correction claim exists; caller-data corruption inside a live block is not detected at all (allocator checks are not ECC) | BY DESIGN |
| U-7 | Using usable size larger than requested | No usable-size query exists (N-3); preservation and zeroing guarantees are stated and tested over the requested extent only (C-005 / C-007 discipline: no byte beyond a requested extent is ever inspected as an oracle) | BY DESIGN |
| U-8 | Calling from a signal handler | The claim is refused, the act is UB-class: the header states no async-signal-safety for any entry point, and no signal API is used in `src/`. Not run: a handler call while the one lock is held by the interrupted thread would deadlock by construction, which is the reason for the prohibition, not a test outcome | BY DESIGN (non-claim kept), INSPECTED |

## C. B-008 §6.3 — Unresolved / no claim (X-1..X-8)

No row in this section is closed by this slice; each is
recorded **posture-only / BLOCKED** with its register ID,
per the slice rule that no posture may be invented where
B-008 leaves the case undecided.

| # | Case | Register | Posture on record (not a behaviour claim) | Verdict |
|---|---|---|---|---|
| X-1 | Zero-size allocation, every form | U-09 / U-02, V-01 | Implementation returns null for `mimir_malloc(0)`, any zero `calloc` factor and `mimir_realloc(NULL, 0)`; recorded as posture only by C-003 / C-005 (INC-C005-01); hook C-006 cannot pass | BLOCKED (U-09) |
| X-2 | `realloc(p, 0)`, live `p` | U-12 / U-02, V-04 | Returns null with the original left LIVE, unchanged and freeable (refused at L2 before any state is touched); posture only (INC-C005-01); disjoint from X-1 as B-008 requires — a live block is presented here, none there | BLOCKED (U-12) |
| X-3 | Exact `errno` on any failure | U-10, V-02 | No value stated anywhere; no test in any suite asserts `errno`; the null return is the only caller-observable failure channel | BLOCKED (U-10) |
| X-4a | Fork while another thread holds the lock | U-17 | No `pthread_atfork` handler exists in `src/` (source inspection); no child recovery is provided or claimed | BLOCKED (U-17) |
| X-4b | First allocation in a fork child; fork during bootstrap | U-18, U-21 | There is no bootstrap interval to re-enter (C-006 collapse), but the child's lock/first-call behaviour is still no-claim; C-006/C-007 forked children assert none of it | BLOCKED (U-18, U-21) |
| X-4c | Host/other-library atfork interaction; diagnostics across fork | U-19, U-20 | Nothing registered, no ordering assumed; the parent side of `fork` is Defined and was run by C-007 (ERR cases) | BLOCKED (U-19, U-20) |
| X-5 | Any performance property | Roadmap Phase 3 text | None stated, none measured by this slice; the baseline is the experimental correctness baseline and no test asserts a timing property | BY DESIGN (no claim) |
| X-6 | In-place realloc / pointer identity / shrink memory return | B-008 §4.4 | Contract promise: none (a move is permitted in every case). Design facts observed by C-005 and labelled design observations there: grow always moves, shrink and same-size return the input pointer, shrink releases no memory (mapping length and requested size kept as separate records, free unmaps the recorded mapping length) | BY DESIGN (no contract promise) |
| X-7 | Guarantees beyond the requested extent (reliance only) | INC-B008-4 | Reliance is bounded to the requested extent in header, code comments and every test oracle; the act of *using* the excess is U-7 (Section B) and that bucket governs the act | BY DESIGN |
| X-8 | Calls after teardown / from exit-path code | No contract row (INC-B008-8) | No teardown mechanism exists in the baseline and no claim is made about the interval; C-008 adds no evidence | BLOCKED (no source row) |

## D. B-008 §6.4 — Defined refusals (the complete list)

Rider, applied to every row (B-008's own): each refusal is
stated through the null return alone — **`errno`: no
claim** until U-10 closes (X-3 above). `free(NULL)` is not
a refusal (it is the Defined no-op of the supported list,
§7.2) and the signal-safety refusal is documentary only
(Section B, U-8); both are recorded in their correct homes
here exactly as B-008 §6.4 directs.

| Refusal | Enforcement (inspected) | Evidence | Verdict |
|---|---|---|---|
| Oversize / overflow `calloc` or `malloc`-class request → null, no wrapped or partial allocation, no state published | calloc: zero-factor gate, then `nmemb > SIZE_MAX / size` checked before the product is relied on (B-008 §3.2 steps 0–2). malloc-class: `size > SIZE_MAX - usable_offset` checked before the total is evaluated in `alloc_locked`; L6 page round-up is itself checked (A5/A6 forms) before any `mmap`. Overflow refusals are pre-admission: no mapping, no descriptor, nothing to roll back | C-004 (A3/A6 rows), C-005 (calloc overflow both orders), C-007 (ARITH/MAP rows); re-run under §9.3 framing in test_c008 S93-1: `malloc(SIZE_MAX)`, `malloc(SIZE_MAX - page + 1)`, `calloc(SIZE_MAX, 2)`, `calloc(2, SIZE_MAX)`, `calloc(SIZE_MAX/2 + 1, 2)` — each null, sentinel intact, recovery probe succeeds | PROVED |
| Unsatisfiable `calloc` / `malloc` → null, no ownership passes | Admission proceeds to one all-or-nothing L6 acquire; on failure `alloc_locked` returns null with registry and counters unchanged (post-admission rollback of any partial step per MA-6.2-024) | C-005 / C-007 (representable `SIZE_MAX` / `SIZE_MAX/2` forms); re-run in test_c008 S93-1 and S93-5, and in genuine OS-refusal form in S93-2 (RLIMIT_AS child allocates to refusal, recovers after freeing) | PROVED |
| Unsatisfiable `realloc` (incl. overflow of the new size) → null **and** the original wholly preserved, caller-owned, freeable as before | Grow sequence is allocate-first (B-008 §4.5): the new block is constructed completely before the copy, and the old block is retired only at the transfer step; any failure at admission / construction returns null with the old block untouched. Shrink and same-size allocate nothing and cannot enter this path | C-005 (preserve-on-failure, both failure forms; original grown and freed afterwards), C-006 lock battery; re-run in test_c008 S93-1 (`realloc` to `SIZE_MAX` and to `SIZE_MAX/2`, original verified byte-for-byte, freed exactly as before) and S93-5 (storm rounds) | PROVED |

**Enforcement-gap statement:** the audit compared every
§6.4 row against the code path that must produce it and
against a recorded run that observes it. No row lacks
either. No `src/` change was therefore made, and — per
the C-006 precedent — no change was invented to fill the
slice. The one class of §9.3 failure this table cannot be
given by the public API is failure *inside* the move
sequence after admission (the fail-the-Nth sweep of
B-008 E-4): it needs the missing shim and is recorded
SKIPPED/NOT-YET (INC-C008-01), resting on the inspected
allocate-first ordering above.

## E. B-008 §7 — documentation channels and supported list

| Item | Evidence | Verdict |
|---|---|---|
| §7.1.1 Supported list stated positively and exhaustively | Header scope paragraph names exactly the B-008 §7.2 list (four entry points, their Defined cases) and states that nothing else is declared; §7.2's "Nothing else is on the list" is mirrored by the symbol set itself (Section A) | PROVED (header + link level) |
| §7.1.2 Unsupported record ships with the exit evidence | This audit and B-008 §§6.1–6.3 are in the repository at the same version as the code; the Phase 3 exit packet itself is C-009 scope and is not claimed here | INSPECTED |
| §7.1.3 Header discipline: Defined cases stated, unresolved named by register ID (U-09, U-12, U-10 minimum), no implied signal-safety / in-place / §6.3 property | Header read in full for this audit: it names U-09, U-12, U-10 / V-02, U-15 / V-07 and U-11 / V-03; it states verbatim that no async-signal-safety is stated or implied and that no in-place grow, pointer identity or shrink memory return is promised at contract level | INSPECTED |
| §7.1.4 No silent fallback, ever | No fallback exists: core undefined symbols are only `memcpy` / `memset` and the L6 interface (Evidence); a refused call returns null, never another allocator's pointer | INSPECTED |
| §7.1.5 Per-target annexes carry the open decisions | No completed per-target annex exists (INC-C007-03, carried); U-09 / U-12 / U-10 therefore stay open for every target and hook C-006 stands as their gate — the conforming state at this base | BLOCKED (annex owed) |
| §7.1.6 Changes are ledger events | This slice moves no item out of Section 6: it supports no new symbol, closes no unresolved case and adds no debug facility; the audit and NOTES are the slice's record | BY DESIGN |
| §7.2 Supported-call list == implemented surface | `mimir_malloc(size > 0)`, `mimir_free(live / NULL)`, `mimir_calloc(both factors > 0)`, `mimir_realloc(live or NULL, n > 0)` — exactly the four declarations in the header and the four definitions in the L1 object; zero-size is not on the list and is not implemented as a supported case (Section C) | PROVED |

## F. Roadmap §9.3 — controlled failure tests, disposition

| §9.3 bullet | Disposition |
|---|---|
| Fail the Nth mapping / metadata allocation | SKIPPED/NOT-YET — needs the fault-injection shim; none exists (INC-C007-01 → INC-C008-01). The public-API failure + recovery substance is run as S93-1 / S93-5 |
| Low process limits | RUN — S93-2: RLIMIT_AS child allocates to genuine OS refusal (null, alive, no hang), frees, recovers under the same limit (INC-C008-05 threshold caveat) |
| Fragmented address space | RUN — S93-3: holed-space churn, survivors intact, new allocations succeed, all live extents pairwise disjoint |
| Kill / restart during application-level state writes | RUN in the only form this layer supports — S93-4: SIGKILL of a working child; fresh process and parent both healthy. Application-level state recovery is out of Phase 3 scope (INC-C008-02) |
| Allocate near integer / alignment boundaries; overflow rejected | RUN — S93-1 (page and boundary sweep, both overflow forms, recovery after each); the wider arithmetic battery remains C-004 / C-007 evidence, cited not redone |
| Repeatedly create / destroy threads while transferring objects | RUN by C-007 (hook C-009: cross-thread free cycles; 4×4 thread churn with transfers) in the same `make test` session; cited, not redone |
| Process initialisation, shared-library loading, teardown order | Initialisation RUN by C-006 (same session). Shared-library pre-`main` harness does not exist in the banked base (C-007 INIT-041 skipped). Teardown order: BLOCKED — no contract row covers the interval (X-8) |
| Failures neither hang nor return overlapping valid blocks | RUN — S93-5 storm (completion is the no-hang evidence) + S93-3 / S93-5 pairwise-disjoint verification over requested extents |
| Tracked separately for Linux / Android / proot | Linux (this Pi): RUN. Android, proot: SKIPPED/NOT-YET (INC-C008-04). x86-64 Linux leg also still owed (INC-C007-02) |

B-008 §8 exit evidence, disposition only: E-1 / E-2 /
E-3 were produced by C-005 / C-007 recorded runs; E-4 is
partial (public-API preserve-on-failure proved; the
injection sweep is INC-C008-01); E-5 stands BLOCKED on
U-09 / U-12 exactly as B-008 states; E-6 is satisfied by
B-008 itself plus this audit entering the record; E-7 is
open (x86-64 leg owed). **Phase 3 exit is not claimed by
this slice** — the exit packet is C-009 scope.

## Inconsistencies recorded by this slice (INC-C008-*)

1. **INC-C008-01 — §9.3 INJECTION DEPENDENCE (INC-C007-01
   CARRIED).** Roadmap §9.3's first bullet and B-008's E-4
   sweep presuppose a fault-injection shim on the L6
   mapping boundary; none exists in the banked base and
   building one is outside this slice's scope guard (it
   would be a new internal facility, and B-009 already
   owns its specification). Every injection-dependent
   case in `test_c008.c` is therefore printed
   SKIPPED/NOT-YET with this reason, and the affected
   code branches (release failure at the realloc transfer
   step, mapping failure mid-construction) rest on the
   inspection recorded in Section D. No run was faked.

2. **INC-C008-02 — §9.3's KILL/RESTART BULLET IS
   APPLICATION-LEVEL.** The bullet speaks of killing a
   process "during separate application-level state
   writes". The allocator holds no persistent or
   cross-process state (registry and counters are
   per-process statics; descriptors live and die with
   their process's mappings), so the strongest statement
   any allocator-layer test can make is the S93-4 one:
   a killed process cannot corrupt its parent or a fresh
   process. Application state recovery is an application
   contract, not an allocator property, and none is
   claimed here.

3. **INC-C008-03 — THE §6.2 UB BUCKET RESTS ON
   INSPECTION (NO-TEST-UB).** B-009 mints no case for any
   UB row and C-005 established the discipline of not
   running them: a UB call cannot produce contract
   evidence, and several rows (use after free, handler
   calls under a held lock) could crash or hang the suite
   itself. Section B's verdicts therefore rest on the
   recorded line-by-line inspection. The inspected
   non-destructive course for invalid `free` / `realloc`
   is a baseline fact only: it is **not** a Defined
   refusal, B-008 §6.4 forbids quoting it as one, and a
   caller who relies on it relies on nothing. First
   place detection could become evidence is a Phase 10
   debug build (N-9).

4. **INC-C008-04 — OFF-HOST §9.3 TRACKS AND THE x86-64
   LEG.** §9.3 requires results tracked separately for
   normal Linux, Android and proot; this host is a Linux
   Pi only, so the Android and proot tracks are
   SKIPPED/NOT-YET and no result is claimed for them.
   The x86-64 Linux leg of B-008 E-7 likewise remains
   owed in full (INC-C007-02, carried unchanged).

5. **INC-C008-05 — THE RLIMIT THRESHOLD IS
   ENVIRONMENT-SHAPED.** In S93-2 the byte level at which
   the child's allocations begin to fail depends on the
   test binary's own footprint, the host page size and
   the inherited address space, none of which §9.3 fixes.
   The case therefore asserts the §9.3 *shape* — success
   until refusal, refusal as exactly null with the
   process alive, recovery after freeing under the same
   limit — and deliberately asserts no threshold number.
   A future per-target annex could pin one; none exists
   (INC-C007-03).

6. **INC-C008-06 — TEARDOWN INTERVAL STILL HAS NO
   SOURCE (INC-B008-8 CARRIED).** B-008 §6.3 X-8 records
   that no contract row covers calls after teardown or
   from exit-path code. This slice confirms the gap is
   unchanged at base 94e1a8d (no teardown mechanism, no
   exit-path code, no row) and adds no evidence; whether
   the interval deserves rows of its own remains a
   contracts-amendment question above this slice's scope.

Carried forward, unchanged: **INC-C005-01** (zero-case
postures, Section C), **INC-C006-01** (discarded unlock
results — noted again here because a failed unlock would
surface first as a §9.3-style hang, and still cannot be
induced through the public API), **INC-C007-01..05**
except as cited, **INC-B008-8** (as INC-C008-06).

*End of C-008 audit.*
