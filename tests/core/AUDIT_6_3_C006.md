# C-006 — Section 6.3 audit: allocation-free startup/init and
# diagnostics-free error paths

Slice: **TEAM-06 C-006**. Base: **56eb4dc** (banked C-005).
Audited surface: the banked implementation line — L1
`src/api/mimir_alloc.c`, core `src/core/mimir_core.c` /
`mimir_core.h`, L6 `src/platform/mimir_platform.c` /
`mimir_platform.h`, and `include/mimir_alloc.h` — against
`docs/contracts.md` **Section 4 [Roadmap §6.3]** (contract
IDs `MA-6.3-001..006`, `-010..012`, `-020..023`, `-030..032`,
`-040..042`) and **Section 6.3** (bootstrap / reentrancy
watchdog, hook C-010), read with `docs/PHASE_3_ARITHMETIC_
ALIGNMENT_INIT_ERROR_DESIGN.md` (**B-007**) Sections C2 and
D.4, which are this baseline's design application of the
same rows. Host: Raspberry Pi `purpleviking1`, runtime
system page size 16384 bytes, recorded runs only (see
Evidence).

**U-03 caveat, carried unchanged:** the `mimir_*` names
have no direct contract rows in `docs/contracts.md`
([UNRESOLVED: U-03]). Following B-007's own practice, the
`MA-6.3-*` rows are cited **directly** where they govern
the allocator's bootstrap/diagnostics surface itself (B-007
cites them directly, Sections C2/D.4); every other mapping
is **by analogy only, Proposed only** — never a direct
namespaced contract and never an ABI claim.

## Verdict summary

- **No implementation gap in §6.3 Phase 3 scope was found.**
  The banked design discharges §6.3 by *collapse*, not by a
  separate bootstrap machinery: there is no initialisation
  step at all. Registry anchors and counters are static
  storage in their defined starting state (empty registry,
  zero counters); the one global lock is a statically
  initialised `PTHREAD_MUTEX_INITIALIZER`; the first call
  enters the same single path every call enters, and that
  path is valid from the static zero state. Nothing on any
  path allocates through the allocator, performs stdio, or
  emits any diagnostic: the whole `src/` tree contains no
  `stdio.h` include and no output, allocation, logging,
  signal, atfork or constructor call at all (inspection
  inventory below). Per the slice instruction, code changes
  are therefore limited to added tests — **no `src/` or
  header change is made by this slice**, and no work was
  invented.
- **No new public API.** Section 6.3 names no initialisation
  entry point for the namespaced surface (and U-03 leaves
  the surface itself unresolved), so the scope guard's
  condition does not fire: the header is unchanged and
  continues to declare exactly the four C-001/C-005 entry
  points.
- **Evidence is split honestly into two classes:**
  *runtime-proved* (a recorded Pi test observes the
  behaviour) and *inspection-only* (the code states the
  property and its disposition, but the branch cannot be
  triggered or observed through the public API on a healthy
  host and no fault-injection, interposition or counter
  hook exists in Phase 3 scope — the INC-C004-02 limitation,
  carried here as INC-C006-03). No inspection-only item is
  claimed as tested.
- **Open by design, not gaps:** U-05 / U-17..U-21 (signal
  and fork specifics, watchdog bound), U-09 / U-12
  (zero-size postures), U-10 (`errno`). The header and code
  claim none of them; §4.4's non-claim and §4.5's open list
  are the conforming posture.
- **New inconsistencies recorded:** INC-C006-01 ..
  INC-C006-04 (register at the end). None required a code
  change inside this slice's scope; each states why.

## Evidence

- C-006 recorded run: `tests/core/recorded_run_c006.txt`
  (`make clean && make test` on the Pi for this slice:
  smoke, C-003 core, B-007 audit, C-005 and the new
  `tests/core/test_c006.c`; counts are read from that file,
  not restated from memory here).
- Prior recorded runs, unchanged: `tests/platform/
  recorded_run.txt`, `tests/core/recorded_run.txt`,
  `recorded_run_c004.txt`, `recorded_run_c005.txt`
  (first-call bootstrap was partially proven by
  `test_b007.c` at C-004; this slice extends it per the
  assignment and does not redo it).
- Code inspection: this audit's line-by-line read of the
  five audited files at base 56eb4dc, plus the call-site
  inventory: the only external facilities named anywhere in
  `src/` are `sysconf`, `mmap`, `munmap`,
  `pthread_mutex_lock` / `pthread_mutex_unlock`, and the
  ISO C `memset` / `memcpy`. Includes are exactly:
  api — `mimir_alloc.h`, `mimir_core.h`; core — `stdint.h`,
  `string.h`, the two internal headers; platform —
  `stdint.h`, `sys/mman.h`, `unistd.h`, `pthread.h`. No
  `stdio.h`, no libc `malloc` family, no `write`, no
  `pthread_atfork`, no signal API, no constructor attribute
  exists in the implementation tree.

Verdict words: **PROVED** = runtime-proved by a recorded
run named in the row. **INSPECTED** = verified by code
inspection; not triggerable/observable via the public API
(INC-C006-03). **BY DESIGN** = the conforming behaviour is
an absence / non-claim, and the code correctly does not
implement or claim it. **COLLAPSED** (with PROVED or
INSPECTED) = the row is discharged by the bootstrap/normal
path collapse described in the summary.

## A. Assignment item (a) — allocation-free, lazy-or-explicit
##    initialisation exactly as §6.3 prescribes

The design has **neither** a lazy init step **nor** an
explicit one: it has no init step. That is a stronger form
of the §6.3 requirement, not an evasion of it — every
`MA-6.3` bootstrap row is written against designs in which
initialisation *exists* and can be re-entered, raced, or
depend on itself. Here the interval those rows govern is
empty by construction (B-007 C2.1: "the registry anchors
and lock are static, need no allocation and no constructor,
and the empty registry is valid before any init step
runs").

| Item | Where | Evidence | Verdict |
|---|---|---|---|
| MA-6.3-001 first entry before initialisation has run | No init state exists beyond static zero: `mimir_registry_head` (NULL), `mimir_live_mappings` / `mimir_live_bytes` (0) in `mimir_core.c`; lock statically initialised in `mimir_platform.c`. The single entry path consumes only that state plus L6 discovery | test_b007 first call (C-004, prior); test_c006: process's first call is `mimir_free(NULL)` (returns normally), first `mimir_malloc` succeeds; two fresh-image children whose first calls are `mimir_calloc` and `mimir_realloc(NULL, n)` succeed | PROVED, COLLAPSED |
| MA-6.3-002 bootstrap storage not from the normal path | Initialisation needs no storage: anchors/counters are fixed static storage (B-006 S7.2); per-allocation metadata is the in-mapping descriptor, created with its mapping, never allocated separately | Inspection of storage definitions; no allocation call exists in `src/` | INSPECTED, COLLAPSED |
| MA-6.3-003 no self-allocation / no indefinite recursion in init | There is no init code, so no call graph from init into the allocator exists. The runtime call graph is acyclic toward L6: L1 → core → L6 primitives only; `alloc_locked` / `retire_locked` / `find_candidate_locked` never call a public entry point or re-take the lock | Inspection of all call sites in the three `.c` files | INSPECTED, COLLAPSED |
| MA-6.3-004 reentrant entry while init in progress | No init is ever in progress, so the case cannot arise. Same-thread re-entry into the *allocator* from within a held lock also cannot arise from inside the implementation (no callbacks, no reporting, no nested public calls — Section B) | test_c006: 8 threads racing first entry in a fresh image, 150 mixed cycles each, all succeed (the observable concurrency side) | PROVED (concurrent entry), INSPECTED (reentrancy absence), COLLAPSED |
| MA-6.3-005 constructor / loader (pre-`main`) entry | Same single path as MA-6.3-001; the path assumes no hosted service beyond `sysconf` / `mmap` / the mutex, and needs no constructor of its own | As MA-6.3-001 rows above; a true pre-`main` caller (a constructor in another TU linked against this archive) is not constructible with the Pi facilities this slice may use without inventing an integration the roadmap has not scoped — recorded under INC-C006-03 | PROVED (first-entry equivalent), INSPECTED (pre-`main` form) |
| MA-6.3-006 free, after full init, of a bootstrap-served block | There are no bootstrap-served blocks distinct from normal blocks: every block is served by the one path and freed by the one registry discipline, so the compatible-allocator acceptance holds trivially | Every free in every recorded run, including of the first block allocated in each test process | PROVED, COLLAPSED |
| MA-6.3-010 no allocator-dependent container on the init path | The implementation is C11 throughout; no C++ translation unit, container or allocating utility exists in the repo's implementation tree | File/include inventory (Evidence) | INSPECTED |
| MA-6.3-011 no reliance on allocating static construction order | The only statics are a pointer, two counters, the descriptor constants, and the mutex initialiser — none has a constructor and none allocates; no ordering is relied on | Inspection of all static definitions | INSPECTED |
| MA-6.3-012 growing bookkeeping in fixed / bootstrap-owned storage | Bookkeeping is exactly: one registry head pointer + two counters (fixed, constant-size), plus per-allocation descriptors inside the allocations' own mappings. Nothing grows in allocator-obtained or container storage | Inspection; descriptor read-backs in test_b007 / test_c006 confirm the in-mapping placement | INSPECTED |
| Contracts §6.3 hook C-010 watchdog | `[COND: watchdog procedure selected]`; the bound and procedure are [UNRESOLVED: U-05], and C-010 results "are recorded as preliminary and cannot support a promotion claim" until U-05 closes. No watchdog is defined for this target, so no C-010 pass is claimed by this slice | — | BY DESIGN (gate open, U-05) — see INC-C006-03 |

## B. Assignment item (b) — diagnostics-free error paths
##    (no allocation, stdio, logging or diagnostics output)

| Item | Where | Evidence | Verdict |
|---|---|---|---|
| No error path performs stdio / logging / diagnostics output | Every failure return in core and platform returns a value only (NULL through L1; a status enum inside L6). No output facility is included or named anywhere in `src/` (inventory in Evidence); L7 observability is compiled out (core header) | test_c006 silence battery: fds 1/2 redirected to a tmpfile across induced malloc / calloc / realloc failures and successes — captured byte count exactly 0 | PROVED (fd-observable side), INSPECTED (no other channel exists in code) |
| No error path allocates | Failure paths perform only checked arithmetic, at most one mapping acquire/release pair, registry/counter updates, and lock operations. No allocation call exists in `src/` at all | Inspection (call-site inventory) | INSPECTED |
| MA-6.3-020 reporting performs no unbounded allocation, within a stated bound | There is no reporting: the storage consumed by "producing a report" is fixed at zero bytes, independent of the failing request, the heap, and prior reports — the bound statement holds at its strongest point. B-007 D.4 rule 1 records this baseline's bound as exactly this: "reporting is allocation-free" with L7 compiled out | Inspection + silence battery | INSPECTED (vacuous bound) — tension recorded as INC-C006-02 |
| MA-6.3-021 reporting independent of the reported state's health; truncated report instead of none | No report is produced in any state, healthy or corrupted, so no report depends on state health. The row's "truncated instead of no report" clause presupposes a reporting facility; this baseline has none in Phase 3 scope | Inspection | INSPECTED — INC-C006-02 |
| MA-6.3-022 the bound holds on the bootstrap path | One path only: the (zero) bound is the same on first entry as everywhere else | First-entry rows in test_c006 run under the same code path; silence battery includes the posture call and first-window successes | PROVED (path identity), INSPECTED (bound) |
| MA-6.3-023 repeated / cascading errors keep the bound, no unbounded retained diagnostic state | No diagnostic state exists to accumulate; retained state after any number of failures is exactly the live registry of successful blocks | test_c006: 500 consecutive failures all return NULL and a health cycle succeeds immediately after; 60-round mixed storm green | PROVED (observable side), INSPECTED (no-state side) |
| B-007 D.4 rule 5: never report under the lock | Nothing is reported anywhere, under the lock or otherwise; the lock is likewise never held across any callback (there are no callbacks) | Inspection | INSPECTED, COLLAPSED |

## C. Assignment item (c) — no deadlock / no leaked lock
##    on any error path

Lock discipline was audited return-by-return. The L6 lock
is a non-recursive pthread mutex; a missed unlock on any
failure return would deadlock the next caller, and a nested
lock attempt would deadlock the caller itself. Neither
exists:

| Entry point | Returns before the lock (no lock held) | Returns after the lock (each releases it) |
|---|---|---|
| `mimir_core_alloc` | `size == 0` posture NULL; lock-acquisition failure NULL (F-07: never proceed unlocked) | single exit: `alloc_locked` result returned after `(void)…unlock()` on both its success and NULL outcomes |
| `mimir_core_calloc` | zero-factor posture NULL; product-overflow NULL (checked before any locking); lock failure NULL | single exit as above |
| `mimir_core_realloc` | `ptr == NULL` delegates to `mimir_core_alloc` (own locking); `new_size == 0` posture NULL; lock failure NULL with the original untouched | invalid descriptor → unlock, NULL; same-size → unlock, `ptr`; shrink → unlock, `ptr`; grow construction failure → unlock, NULL, original LIVE; transfer release failure → unlock, NULL, original restored LIVE by `retire_locked`'s F-06 rollback; success → unlock, new pointer |
| `mimir_core_free` | `ptr == NULL` defined no-op (before any locking); lock failure returns with the block left LIVE | invalid descriptor → unlock, return; release failure → unlock, return (block still LIVE per F-06); success → unlock, return |
| `alloc_locked` (lock held throughout) | — | every internal failure returns NULL to its caller-wrapper with any acquired mapping released first (page-size, A3, acquire, A8, A9, A10 branches), so the wrapper's unlock is the single release point |
| L6 primitives | page-size / acquire / release return status enums and never touch the lock; lock/unlock wrap exactly one pthread call each | — |

Deadlock freedom inside the lock: no function called with
the lock held attempts to take it again, calls a public
entry point, or calls out to any code outside the three
implementation files (Section A, MA-6.3-003 row).

| Claim | Evidence | Verdict |
|---|---|---|
| Every inducible failure releases the lock | test_c006 lock battery: after each of malloc oversize ×2, malloc unsatisfiable, calloc overflow ×2, calloc unsatisfiable, realloc grow failures ×2 (original verified byte-for-byte intact), a fresh alloc/write/free cycle succeeds; plus the 60-round storm and 500-failure cascade | PROVED |
| No lock is held across any return to a caller | Return-by-return table above | INSPECTED |
| Unlock-failure handling | The core discards every unlock result (`(void)` casts). See INC-C006-01 | INSPECTED — recorded, not closed |

## D. Assignment item (d) — startup state fully formed
##    before any pointer is published (B-007 C1/C2)

| Item | Where | Evidence | Verdict |
|---|---|---|---|
| C1.4 descriptor fully written before publication | `alloc_locked` publication block: every field assigned, `reserved` zeroed, then `registry_insert` + counter updates in the same locked section; the user pointer is computed and returned only after the wrapper unlocks | test_b007 descriptor read-backs (C-004); test_c006 first-block read-back repeats the check on this slice's first allocation | PROVED |
| C1.4 registry/counters updated atomically with insertion | Single locked section (above); counters drive no allocation decision (Invariant 8) | Inspection; interleaved alloc/free patterns across all recorded runs stay consistent | INSPECTED (atomicity), PROVED (observable consistency) |
| C2 starting state needs no init run | Static zero state + statically initialised lock (Section A) | First-entry rows of test_c006 / test_b007 | PROVED, COLLAPSED |
| C1 user bytes never promised / never touched by publication | Publication writes only the reserve-page descriptor; the calloc zeroing and realloc copy are the documented construction/transfer exceptions (C-005) and lie outside startup | Prior suites (unchanged expectations stay green) | PROVED (by the unchanged C-003..C-005 suites) |

## E. Signal and fork rows (§4.4 / §4.5) — non-claims kept

| Item | Position | Verdict |
|---|---|---|
| MA-6.3-030..032 no async-signal-safety claimed | The public header states verbatim that no async-signal-safety is stated or implied; no function carries such a claim anywhere in docs or code of this surface; no signal API is used | BY DESIGN (non-claim kept) |
| MA-6.3-040 atfork handlers, if registered, documented | No `pthread_atfork` registration exists in `src/`; nothing to document; no fork behaviour is claimed | BY DESIGN |
| MA-6.3-041 parent calls around `fork` follow §3 | Unchanged §3 behaviour; no fork-specific code exists | BY DESIGN |
| MA-6.3-042 / U-17..U-21 open fork behaviours | Header: "No fork-safety claim is made (U-17..U-21 remain open)." test_c006's forked children are a fresh-image harness only and assert none of these behaviours | BY DESIGN (open register kept) |

## Inconsistencies recorded by this slice

1. **INC-C006-01 — UNLOCK RESULTS ARE DISCARDED AT EVERY
   CORE CALL SITE.** `mimir_core.c` casts every
   `mimir_platform_global_unlock()` result to `(void)`,
   although the L6 wrapper does check `pthread_mutex_unlock`
   and returns `MIMIR_PLATFORM_ERR_LOCK`. If an unlock ever
   failed, the failure would be invisible to the caller and
   the mutex could remain held, deadlocking later calls —
   the one lock-discipline outcome this audit cannot clear
   by inspection alone. It cannot be induced through the
   public API on a healthy host (a normal mutex unlock by
   its owner has no failure mode reachable here) and the
   design provides no report channel for it (B-007 D.4 /
   F-07 leaves report-or-terminate to a later recorded
   decision; C-003's header already notes "no safe report
   channel" for this case). Disposition: recorded as an
   INSPECTED limitation in the INC-C004-02 class; no code
   change in this slice, because any change would have to
   invent the missing reporting/termination policy, which
   is a design decision above §6.3 audit scope.

2. **INC-C006-02 — MA-6.3-021's "TRUNCATED REPORT INSTEAD
   OF NO REPORT" vs THE BASELINE'S NO-REPORTING POSTURE.**
   §4.3's rows are written for an allocator that reports;
   MA-6.3-021 requires a truncated report *instead of no
   report* when a full one exceeds the bound. This baseline
   reports nothing at all (L7 compiled out; B-007 D.4
   records its bound as allocation-free reporting, and the
   C-003 core header states "this baseline reports nothing
   at all"). The §6.3 *bounds* (020/022/023) hold vacuously
   at zero bytes, and the slice's assigned property —
   diagnostics-free error paths — holds absolutely; but a
   reader applying 021 literally would find the absence of
   any report, truncated or otherwise, unaddressed by the
   contract text. Disposition: recorded as a contract-vs-
   baseline tension for the design owner; implementing a
   reporting facility is L7 work outside Phase 3 scope and
   outside this slice's scope guard.

3. **INC-C006-03 — EVIDENCE LIMITATION (INC-C004-02
   CARRIED).** Three §6.3 properties cannot be given
   runtime evidence with what the Pi offers this slice:
   (i) allocation-freedom of the (non-existent) init path
   and of error paths — no interposition or wrapping of
   `mmap`/allocation counters is available, per the
   assignment's own constraint; (ii) true pre-`main`
   constructor/loader entry — no scoped integration exists
   to construct one against; (iii) hook C-010's watchdog —
   its procedure and bound are [UNRESOLVED: U-05], so no
   watchdog result can be more than preliminary and none
   is claimed. All three rest on the code inspection
   recorded in this audit and are labelled INSPECTED
   wherever they appear. No run was faked for any of them.

4. **INC-C006-04 — "SECTION 6.3" NAMES TWO PLACES.**
   Roadmap §6.3 (startup/diagnostics) is specified in
   `contracts.md` §4, while `contracts.md` §6.3 is the
   bootstrap/reentrancy *watchdog* conformance note for
   hook C-010. This audit covers both (Sections A–D for §4,
   the C-010 row for §6.3) and records the overlap so no
   later reader mistakes a §4 verdict for a §6.3 one or
   vice versa. Editorial only; no source document was
   changed by this slice.

*End of C-006 audit.*
