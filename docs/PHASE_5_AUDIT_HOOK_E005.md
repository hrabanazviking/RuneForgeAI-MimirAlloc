# E-005 — Visible allocation counter / audit hook: the intended call path provably invokes MimirAlloc

Phase 5 (ABI completeness and interposition), Wave 5, slice 5 of 9.
Base: `966214d28501ea003fa935fd36f1f38d75d21b12` (E-003 banked).
Branch: `yrsa/mimiralloc-05`.
Worktree: `/home/pi/mimiralloc-team-05` on purpleviking1 (Raspberry Pi,
Cortex-A76), user `pi`. Date of the recorded run: 2026-11-06T19:45-05:00.
Toolchain: Debian GNU/Linux 12 (bookworm); cc 12.2.0; kernel
6.12.112-v8-16k-hailo+; `getconf PAGESIZE` = 16384; glibc 2.36.
Pi load stayed light (sequential small makes only; Samtal on port
8787 answered HTTP 200 before the build and was never starved;
`free -g` showed 13 GiB available at slice start).

Governing texts: roadmap Phase 5 (Section 8); `docs/PHASE_5_
FOUNDATION_PLAN.md` (E-001 — task T5, and INC-E001-07, which governs
the shape of this slice); `docs/SHARED_LIBRARY_BUILD.md` (E-002 —
the shared library and its exactly-4 export proof); `docs/PHASE_5_
LINK_ROUTES_E003.md` (E-003 — routes R1 static / R2 dynamic);
`docs/contracts.md` (A-008 — core-symbol rows, by analogy only; the
namespaced `mimir_*` surface is [UNRESOLVED: U-03] and this slice
settles nothing about it); `include/mimir_alloc.h` (the public
surface: exactly `mimir_malloc`, `mimir_free`, `mimir_calloc`,
`mimir_realloc` — unchanged by this slice).

## 0. Bottom line

- **T5 is PROVEN on the Pi-native target, in two legs.**
  *Static/test-only leg* (`make test-e005`, 31 checks, 0 failed,
  exit 0, twice from clean): six internal audit counters track a
  scripted lifecycle **exactly** — final books: call_malloc 6,
  call_calloc 4, call_realloc 4, call_free 9, mappings published
  10, mappings retired 10, live 0 — and a libc-bypass phase in the
  same process (3 allocations, 1 realloc, 3 frees) moves **no**
  counter. *Dynamic leg* (`make test-e005-dyn`, 25 checks,
  0 failed, exit 0, twice from clean): a consumer linked only
  against `libmimiralloc.so` cannot read the counters at all, and
  instead proves servicing by implementation-only effects —
  every returned block carries a valid MimirAlloc descriptor
  (magic, self-base, requested size, LIVE state) and the
  process's total mapped bytes move by exactly
  page_round_up(page + size) per allocation (+131072 for
  4×malloc(64), +65536 for 2×calloc(16,8), **+0** for 4×libc
  malloc(64)) and return **exactly** to baseline after all frees.
- **The public ABI did not move.** The counters are INC-E001-07
  shape (b): internal state, declared in no installed header,
  bound **local** in the shared library. `nm -D --defined-only`
  on the rebuilt `.so` still lists **exactly 4** symbols; the six
  counters appear in the `.so`'s full symbol table only as local
  `b`; a consumer that references one against the `.so` **fails
  to link** (recorded, exit 1, as designed). No shape-(c)
  contract decision was needed, and none is claimed: the hook's
  contract status is **Proposed-internal, explicitly NOT part of
  the contract** (Section 1.3).
- `make test` composition is unchanged and exits 0 (twice in the
  recorded run). README.md untouched (diff vs base = 0 lines).
  One commit on `yrsa/mimiralloc-05`; no push or merge (the
  keeper banks in roadmap order — E-004 banks before E-005).

Evidence file: `tests/abi/recorded_run_e005.txt` (177 lines,
7,736 bytes) — every command with its exit code, in sections
S0–S7, including a full clean re-run (S6) reproducing both legs'
counts exactly. This document summarises it; the recorded run
is the record.

## 1. The design, and the decision INC-E001-07 required

### 1.1 Why shape (a) alone was judged insufficient

INC-E001-07's preferred shape (a) — a counter in the test binary
wrapping its own calls — counts **call sites in the consumer**.
It cannot by itself distinguish "the consumer called the names
and MimirAlloc serviced them" from "the consumer called the
names and something else serviced them": the wrapper increments
either way. E-001's T5 row demands the stronger statement
(allocations counted = allocations performed **by the
implementation**), so this slice implements shape (b) as the
primary mechanism and uses shape (a) as one component of the
dynamic leg (test-side site counts cross-checked against
descriptor-validated blocks).

### 1.2 Shape (b) as implemented: six internal counters

All changes are additive insertions (Makefile +39, API +36,
core +31 lines; 0 deletions):

- **L1 call counters** (`src/api/mimir_alloc.c`):
  `mimir_audit_call_malloc`, `mimir_audit_call_calloc`,
  `mimir_audit_call_realloc`, `mimir_audit_call_free`. Each
  increments exactly once per invocation of its entry point
  (`__atomic_fetch_add`, relaxed), whatever the outcome —
  success, defined failure, or a defined no-op call such as
  `mimir_free(NULL)`.
- **Core mapping counters** (`src/core/mimir_core.c`):
  `mimir_audit_alloc_published` increments at the single
  publication point in `alloc_locked` (so it covers malloc,
  calloc, and realloc's grow-by-move construction);
  `mimir_audit_mapping_retired` increments at the single
  successful-release point in `retire_locked` (so it covers
  free of a live block and realloc's transfer of the old
  block). published − retired = live MimirAlloc mappings.
  Increments there happen with the global lock held.
- **Test-only declarations**: `tests/abi/mimir_audit_e005.h`
  (57 lines) declares the six counters for test binaries. It
  lives under `tests/`, is not installed, and the public header
  does not mention the counters.

The two counter families intentionally measure different
things (calls vs mappings); Section 4 of the static test and
INC-E005-04 record the exact reconciliation.

### 1.3 Contract status — the explicit decision this slice owes

**Decision (recorded, not silent): the audit counters are
Proposed-internal audit state. They are NOT part of the public
ABI, NOT part of the documented symbol contract, and NOT a
statistics facility.** No new public symbol is proposed or
decided by this slice. Consequently:

- `include/mimir_alloc.h` is unchanged; the public surface
  remains exactly the four entry points.
- In `libmimiralloc.so` every counter is a **local** symbol:
  the core object is compiled `-fvisibility=hidden` (E-002)
  and the version script `src/api/mimir_alloc.map` binds every
  non-public name local, which also covers the API object
  (compiled with default visibility for its four entry points).
  Section 3 proves this from the artifacts.
- Only a binary that links the library **objects or static
  archive** directly can read the counters. That is the
  test-only build variant INC-E001-07 shape (b) names.
- If a future phase wants a public statistics/hook surface,
  that is shape (c): a recorded contract decision plus the
  re-proof obligation of INC-E001-02. Nothing here pre-decides
  it, and the counters' names/layout carry no stability
  promise of any kind (U-03 remains open; U-07 untouched).

Trade-off, honestly: the shipped objects now carry six BSS
words and one relaxed atomic increment per entry/publication/
retirement. That is a real, if tiny, change to the shipped
library's internals in exchange for implementation-resident
evidence. INC-E005-01 records the related tension with the
core's banked "L7 observability — compiled out" design note.

## 2. Static leg: the counters, exactly (recorded run S2, S6)

Program: `tests/abi/test_e005.c` (237 lines), built by the
additive `make test-e005` target from the library objects
(LIBOBJS), never from the `.so`. The phase script's expected
counts are derived in the source from the calls themselves,
not read back from the counters. Recorded AUDIT lines:

```text
AUDIT start                     call_malloc=0 call_calloc=0 call_realloc=0 call_free=0 published=0  retired=0  live=0
AUDIT phase1-mimir-malloc-x5    call_malloc=5 ... published=5  retired=0  live=5
AUDIT phase2-libc-bypass        call_malloc=5 ... published=5  retired=0  live=5   (no counter moved)
AUDIT phase3-mimir-calloc-x3    call_calloc=3 published=8  retired=0  live=8
AUDIT phase4a-realloc-grow-x2   call_realloc=2 published=10 retired=2  live=8
AUDIT phase4b-realloc-shrink-same call_realloc=4 published=10 retired=2 live=8   (shrink/same-size publish nothing)
AUDIT phase5-defined-failures   call_malloc=6 call_calloc=4 published=10 retired=2 (oversize malloc + overflow calloc returned NULL)
AUDIT phase6a-free-null         call_free=1 published=10 retired=2               (call counted, nothing retired)
AUDIT phase6b-free-x4           call_free=5 retired=6  live=4
AUDIT phase7-free-rest          call_free=9 published=10 retired=10 live=0       (books balance exactly)
```

(`...` elides counters shown in full in the recorded run.)
**31 checks run, 0 failed, RESULT: PASS, exit 0** — in S2 and
again in the S6 clean re-run with identical AUDIT lines.
Behaviour spot-checks inside the script: calloc extents read
zero (3×), realloc-grow contents preserved byte-for-byte,
shrink and same-size realloc return the same pointer.

The negative case is Phase 2 and it is the point of the
slice: libc `malloc`/`calloc`/`realloc`/`free` executed in
the same process, between counter readings, move **nothing** —
the counters reside in MimirAlloc's implementation, so only
the intended call path can move them.

## 3. Dynamic leg: proof from outside the `.so` (S3–S5)

Program: `tests/abi/test_e005_dyn.c` (245 lines), built by the
additive `make test-e005-dyn` target, linked `-lmimiralloc`
with rpath `$ORIGIN` exactly as the E-002 consumer. It includes
only the public header. Three observations, all recorded:

1. **Descriptor validation.** For each of 4×`mimir_malloc(64)`,
   2×`mimir_calloc(16, 8)` and 1×realloc-grow to 96, the test
   validates the block's reserve-page descriptor: magic
   `0x4D494D4952414C4C` ("MIMIRALL"), `mapping_base` == the
   descriptor's own address, `usable_offset` == 16384,
   `requested_size` == the requested size (64 / 128 / 96),
   `state` == LIVE, `mapping_length` == the exact expected
   mapping length. Validated count == test-side call-site
   count exactly (shape (a) cross-check). The descriptor is
   read only for pointers this process obtained from
   `mimir_*` — never for foreign pointers (INC-E005-03).
2. **Mapped-bytes accounting** (summed from `/proc/self/maps`):
   base 2,539,520 → +131,072 after the 4 mallocs
   (4 × 32,768) → **unchanged** after 4×libc `malloc(64)` and
   their frees → +65,536 after the 2 callocs → unchanged by
   the realloc grow (one mapping published, one retired,
   equal lengths) → **final == base exactly** after all frees.
3. **The counters are unreachable from here.** Full `nm` on
   the `.so` shows all six counters as local `b`;
   `nm -D --defined-only` lists exactly the 4 public symbols;
   and the negative link probe `tests/abi/test_e005_audit_
   link_probe.c` (22 lines), a consumer referencing
   `mimir_audit_call_malloc` linked against the `.so` only,
   **fails with "undefined reference" — exit 1, as designed**
   (S5). The audit hook provably did not expand the ABI.

**25 checks run, 0 failed, RESULT: PASS, exit 0** — in S3 and
again in the S6 clean re-run (final mapped bytes again exactly
the baseline).

## 4. Makefile wiring

Additive block only (+39 lines, appended): targets
`test-e005` and `test-e005-dyn` with their object/binary rules.
**`make test` is unchanged** — same eight binaries, same order;
the recorded run's S1 baseline and S6 re-run both exit 0 with
no e005 line in them. No source file outside the three listed
in Section 1.2 was modified; no header, map file, or existing
test was touched.

## 5. Inconsistencies and limitations (INC-E005-*)

- **INC-E005-01 — The core's banked design note says "L7
  observability — compiled out; nothing here" (C-003 header
  in `src/core/mimir_core.c`), and this slice adds permanent
  internal counters to that same layer.** Recorded, not
  silently overridden: the counters are audit state for
  Phase 5's T5 proof, not a general observability facility,
  and they export nothing. A future design revision of
  B-006 should either bless an audit-counter exception to
  the L7 note or retire the counters after E-009's packet
  has consumed this evidence; until then the note and the
  code disagree in wording, and this item is the record of
  that disagreement.
- **INC-E005-02 — `/proc/self/maps` line counts cannot count
  allocations.** In the pre-slice prototype (run twice on
  the Pi, outputs summarised here, prototype not banked),
  4 live MimirAlloc mappings added only **2** lines: adjacent
  anonymous mappings with equal permissions merge in the
  kernel's listing. A maps-line counter would have
  undercounted and "proven" a falsehood. The dynamic leg
  therefore sums mapped **bytes**, which was exact in both
  prototype runs and in both recorded runs. Any future
  slice citing maps evidence must use the byte sum.
- **INC-E005-03 — The dynamic leg's descriptor validation
  rests on an internal layout that is expressly not an
  ABI.** B-006 S7.3 calls the descriptor layout "still not
  a stable ABI". The leg proves that **this build's**
  implementation serviced the calls; if the layout changes,
  the test — not the contract — must change. The descriptor
  is read only for pointers returned by this process's own
  `mimir_*` calls; no foreign pointer's memory is ever
  dereferenced to "check" it, so the negative case on the
  dynamic route rests on mapped-bytes accounting, not on
  descriptor absence.
- **INC-E005-04 — The two counter families measure
  different things, by design, and must not be conflated.**
  Call counters count invocations (including the defined
  failures of Phase 5 and the `free(NULL)` call); mapping
  counters count only publications and successful releases.
  In particular `call_free` (9) ≠ retirements caused by
  free alone (retired also counts realloc's 2 transfers;
  free caused 8 of the 10 retirements, and 1 of the 9 calls
  was the NULL no-op). The recorded AUDIT lines carry both
  families so the reconciliation is auditable.
- **INC-E005-05 — Counter exactness under concurrency is
  argued, not run.** L1 increments are atomic and core
  increments happen under the global lock, so the mechanism
  is race-free by construction; but no multithreaded run in
  this slice reads the counters (D-004's threaded test does
  not), so T5 evidence is single-threaded. A threaded
  counter run is OWED if T5 is ever cited for concurrent
  call paths.
- **INC-E005-06 — `mimir_realloc(NULL, n)` accounting rests
  on code reading, not on a run.** By the code, it counts
  as one `call_realloc` and one publication (the core
  delegates to its alloc path; the API malloc counter does
  not move). The static script deliberately does not
  exercise it (its phase arithmetic stays minimal), so
  this one accounting line is inspection-grade, recorded
  here as such rather than run-grade.

## 6. What remains OWED

- **E-004** (independent executable + exact-route naming)
  banks before this slice and is expected to consume this
  slice's evidence as the T5 use-proof; nothing here names
  THE exact link route (that remains E-004's, per E-001
  Section 5.4). This slice exercised both R1 (objects/
  archive form) and R2 (`.so`) without ranking them.
- **Phase 5 exit** remains OWED to E-009's packet (X1–X4 as
  a whole, T2/T6/T7/T8 to their own slices). T5's evidence
  is complete for the Pi-native glibc target only.
- **Route R3 (`dlopen`)** untested by decision (E-001
  Section 5.2); **R4 interposition** rejected (INC-E001-01);
  no Wave 5 result here is an Android, musl, x86-64, or
  Mojo result, and no target is promoted to "supported".
- Carried debts untouched, as E-001 Section 6.3 requires:
  Phase 4 exit UNMET with its physical Android run OWED;
  Phase 3 exit UNMET (x86-64 suite run OWED); U-01/U-03
  remain open.

`README.md` was not touched by this slice.
