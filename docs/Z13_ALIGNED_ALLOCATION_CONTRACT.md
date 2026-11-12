# Z13 Aligned Allocation Contract

Slice: Z-073 - Aligned allocation: contract first (Wave 9
opener). Status: contract written and RED-proven on the
banked core, 2026-11-12, team-02 worktree. The
implementation slice is Z-074; this slice changes NO
allocator code and NO public header.
Artifacts: tests/contract/mimir_aligned_z073.h (the
proposed surface), tests/contract/test_z073_aligned_
contract.c (the suite), tests/contract/run_z073.sh (the
runner), tests/contract/recorded_run_z073.txt (the red
evidence).

Roadmap gate: "contract text in docs/contracts.md style
+ red tests." This document follows docs/contracts.md
conventions: normative rows carry [NORM] with RFC 2119
capitals inside the row only; every row is classified
Defined / UB / Debug-detectable only; open items are
named by their U-identifier and state no behaviour.
Row identifiers are AL-073-* (this slice's namespace).

## 0. Position and honesty notes

- The banked core has NO aligned entry point: the public
  surface is exactly mimir_malloc / mimir_free /
  mimir_calloc / mimir_realloc (include/mimir_alloc.h),
  and the absence is the documented unsupported behaviour
  (B-008 S6.1; E-006 decision record: the aligned surface
  stays EMPTY until a contract slice settles it, and
  E-006 names exactly this route - one namespaced entry
  taken up by a contract + ABI slice that first settles
  U-11 / V-03 per target). Z-073 is that contract slice
  for this host; Z-074 is the ABI/implementation slice.
- U-series items this contract SETTLES for the named
  surface on this target (x86-64, 4 KiB pages, glibc 2.44
  host): U-11 (alignment validity + size-multiple rule,
  V-03) by decisions D-Z073-1 and D-Z073-2 below, and
  U-14 (posix-shaped error numbers, V-06) by D-Z073-3.
  Items it deliberately does NOT settle: U-09 (zero
  size) and U-10 (errno on the pointer-returning entry)
  - both are owned by Z-078; S5/S6 pin only the interim
  family posture so a later change is visible, never
  silent. U-03 (namespaced API has no direct contract
  rows) is discharged for THIS surface by this document:
  these rows are direct, not by analogy.
- Standard-name symbols are out of scope: no aligned_
  alloc, no posix_memalign, no memalign family is
  promised, exported, or interposed by this contract.
  Interposition is Wave 10 (Z-083 names the memalign
  family over Z-074). This contract covers exactly two
  namespaced entries (S2).
- This contract promises NO speed number and NO internal
  layout. Z-074 implements "over the core's class
  machinery" (roadmap); how it reconciles over-alignment
  with span geometry is its own recorded design work.

## 1. Evidence the contract must serve

- Alignment floor (Z-017, tests/perf/recorded_run_z13_
  z017.txt, this host): both allocators returned a
  sub-16-aligned pointer 0 times in 163,840 blocks per
  leg; MimirAlloc minimum alignment is 16 or 32 by class.
  The 16-byte floor (_Alignof(max_align_t) = 16 on this
  host) is therefore a banked fact this contract builds
  on: any valid alignment at or below 16 is already
  satisfied by every block the core returns.
- Carve geometry warning (Z-017, same run): MimirAlloc
  classes that are multiples of 64 B return pointers
  EXCLUSIVELY at ptr mod 64 = 32 (8192/8192 in twelve
  classes). The current machinery therefore does NOT
  incidentally satisfy alignments above 16, and a caller
  may not be told otherwise: over-alignment is a real
  service Z-074 must construct, not a fact it inherits.
  (E-006 said the same in the other direction: observed
  over-alignment is never a promise. Here the measured
  fact is stronger - above 16 there is often nothing to
  observe.)
- Size classes (src/core/mimir_core.c, banked): 20 small
  classes serve 1..1024 B (16,32,48,64,80,96,112,128,
  160,192,224,256,320,384,448,512,640,768,896,1024);
  above 1024 the mapping path serves. The size domain
  in S4 spans both, and P6 pins page alignment at
  1 MiB, deep in mapping territory.
- glibc baseline MEASURED on this host for this slice
  (glibc 2.44, probe run by the team lead 2026-11-12,
  load 0.17/0.33/0.34; full table in the recorded run):
  posix_memalign with alignment 0,1,2,4,24,100 returns
  EINVAL (22), leaves *memptr unchanged, and does not
  touch errno; alignment 8 and 16 succeed. aligned_alloc
  with alignment 0,3,24 returns NULL (errno EINVAL as an
  observation, not adopted - see U-10 note in S6);
  alignment 8,16,256 succeed. aligned_alloc(256,100)
  SUCCEEDS: glibc on this host does not enforce the C11
  size-multiple rule. aligned_alloc(64,0) and
  posix_memalign(64,0) return a unique freeable pointer
  (the glibc zero-size behaviour; this family diverges
  deliberately, S5). posix_memalign(16,SIZE_MAX)
  returns ENOMEM (12), *memptr unchanged.
- Host constants (Z-002): page size 4096 B; cache line
  64 B (Z-016/Z-018); sizeof(void *) = 8.

## 2. The surface Z-074 must provide

Declared in tests/contract/mimir_aligned_z073.h,
test-only for this slice. Z-074 promotes both
declarations, unchanged in signature, to
include/mimir_alloc.h and exports the symbols from the
shared library (its ABI link proofs are E-series style,
per the roadmap; the export-count change is Z-074's to
record, never this slice's to anticipate silently).

- void *mimir_aligned_alloc(size_t alignment,
  size_t size) - the aligned_alloc-shaped entry.
- int mimir_posix_memalign(void **memptr,
  size_t alignment, size_t size) - the posix_memalign-
  shaped entry: result delivered through the output
  parameter, outcome reported by the return value.

## 3. Alignment domain (settles U-11 / V-03, form half)

Decision D-Z073-1 (validity is a property of FORM, not
of magnitude):

| Row | Case | Required behaviour | Result | Channel | Class |
|---|---|---|---|---|---|
| AL-073-01 [NORM] | mimir_aligned_alloc, alignment is a power of two >= 1 | The request is VALID; alignments below 16 MUST be promoted to the 16-byte floor, never refused for smallness | Per S4/S6 | NULL on failure only | Defined |
| AL-073-02 [NORM] | mimir_aligned_alloc, alignment is 0 or not a power of two | The call MUST fail and MUST NOT allocate | NULL | None promised (S6) | Defined |
| AL-073-03 [NORM] | mimir_posix_memalign, alignment is a power of two AND a multiple of sizeof(void *) (= 8 here) | The request is VALID | Per S4/S6 | Return value | Defined |
| AL-073-04 [NORM] | mimir_posix_memalign, alignment fails AL-073-03 (incl. powers of two below 8: 1,2,4) | The call MUST return EINVAL, MUST NOT allocate, and MUST leave *memptr unchanged | EINVAL | Return value only | Defined |
| AL-073-05 [NORM] | mimir_posix_memalign, memptr == NULL | The call MUST return EINVAL and MUST NOT allocate (a defined refusal where POSIX leaves the case undefined - the safer namespaced choice, recorded as a deliberate divergence) | EINVAL | Return value only | Defined |
| AL-073-06 [NORM] | Either entry, alignment a valid power of two above 4096 (the page) | Validity does NOT expire at the page: the call MAY succeed (mapping path) or MAY fail cleanly per S6; it MUST NOT return a pointer that violates the requested alignment | Aligned pointer or clean failure | Per entry | Defined |

Magnitude note: no numeric cap is placed on valid
alignments. A huge valid alignment with a small size is
a satisfiability question (S6), not a validity one -
the same split POSIX/glibc draw (S1: glibc refuses by
form, EINVAL, and by unsatisfiability, ENOMEM, never
mixing the two).

## 4. Size domain (settles U-11 / V-03, multiple half)

Decision D-Z073-2 (NO size-multiple rule):

| Row | Case | Required behaviour | Result | Class |
|---|---|---|---|---|
| AL-073-10 [NORM] | size >= 1, any alignment valid per S3 | Size need NOT be a multiple of alignment; the call MUST be judged exactly as if the rule did not exist (glibc on this host measured the same, S1; the C11 aligned_alloc multiple rule is NOT adopted for this namespaced surface) | Per S6 | Defined |
| AL-073-11 [NORM] | Success, either entry | The returned pointer MUST satisfy ptr % alignment == 0 AND the 16-byte floor, MUST point to at least size writable bytes, MUST be distinct from every other live allocation, and its contents are unspecified (NOT zeroed - zeroing is mimir_calloc's contract, not this one) | Non-null / 0 + stored pointer | Defined |
| AL-073-12 [NORM] | size > 1024 (mapping path sizes), incl. 1 MiB at alignment 4096 | The contract is IDENTICAL across the small/mapping boundary; page alignment at 1 MiB is GUARANTEED service, not best-effort (pinned by suite P6) | Per S6 | Defined |

## 5. Zero size - INTERIM posture (U-09 NOT settled)

| Row | Case | Interim behaviour pinned by the suite | Class |
|---|---|---|---|
| AL-073-20 | mimir_aligned_alloc(a, 0), a valid | Returns NULL, allocates nothing - the banked family posture for mimir_malloc(0) (header note, posture only) carried to this entry | Defined-interim |
| AL-073-21 | mimir_posix_memalign(p, a, 0), a valid | Returns ENOMEM, leaves *p unchanged | Defined-interim |

This DIVERGES from glibc on this host, which returns a
unique freeable pointer for zero size (S1) - the same
deliberate divergence the existing four symbols already
carry (INC-D008-01 heritage). Z-078 owns the final
U-09 closure for the whole family; if it rules
otherwise, it amends these two rows and the suite's P3
zero checks in the open, in that slice.

## 6. Failure behaviour (settles U-14 for this surface)

Decision D-Z073-3 (error model): the posix-shaped
entry reports ONLY through its return value - EINVAL
for form failures (S3), ENOMEM for unsatisfiable
requests - exactly the numbers POSIX names and glibc
measured on this host (S1); errno is not its channel
(glibc likewise left errno untouched on the EINVAL
leg). The pointer-returning entry reports ONLY through
NULL; errno is NOT promised for it (U-10 stays open,
Z-078 owns it; glibc's observed errno=EINVAL/ENOMEM on
aligned_alloc is recorded in S1 as a baseline fact and
explicitly not adopted as a promise here).

| Row | Case | Required behaviour | Result | Class |
|---|---|---|---|---|
| AL-073-30 [NORM] | Unsatisfiable size (incl. size near SIZE_MAX and any size whose aligned over-allocation arithmetic would overflow) | The call MUST fail cleanly, MUST NOT wrap the arithmetic, MUST NOT return a pointer to storage not allocated to the caller, and MUST NOT disturb any live allocation | NULL / ENOMEM with *memptr unchanged | Defined |
| AL-073-31 [NORM] | Any failure, either entry | No ownership passes, no block is allocated, and the allocator MUST remain fully usable: the next ordinary or aligned call is judged on its own merits (suite P3/P4 pin the recovery leg) | Per row | Defined |
| AL-073-32 [NORM] | Success pointer released | A pointer from either entry MUST be releasable by mimir_free exactly once, and by no other obligation: no separate aligned-free entry exists or is needed | - | Defined |
| AL-073-33 [NORM] | Success pointer passed to mimir_realloc | Realloc keeps ONLY its own banked contract: contents preserved to min(old,new), result guaranteed only the 16-byte floor - the original alignment does NOT survive realloc, and no caller may rely on it doing so (suite P4 pins the floor, not the alignment) | Per realloc contract | Defined |
| AL-073-34 | Freeing a foreign, interior, or already-freed aligned pointer; use after free | No behaviour is proposed (inherited from the family: MA-6.1-FREE-03/-04 by the header's analogy, now direct for this surface) | None | UB |

## 7. What this contract does not promise

- No speed, no RSS, no internal waste bound for
  over-aligned blocks (the over-allocation cost of
  alignment is Z-074's to measure, not this slice's
  to cap sight unseen).
- No C++ operator new/delete aligned forms (Wave 9
  names none; contracts.md S2.10 keeps them COND).
- No standard-name aligned symbols, no interposition
  (Wave 10), no SONAME/install consequence (Z-080).
- No async-signal-safety, no fork-safety (the family's
  standing non-claims carry over unchanged).

## 8. Green-by-design review (for Z-074)

The suite (130 checks) was reviewed clause by clause
against this contract before the red run:

- P0 (3) pins S2's two symbols by weak reference.
- P1 (38) pins AL-073-01/-02 over 13 valid and 12
  invalid alignments, checking the floor jointly with
  the requested alignment on every success.
- P2 (35) pins AL-073-03/-04/-05: 6 valid alignments,
  11 invalid (including 1,2,4 - powers of two that fail
  the sizeof(void *) multiple), the sentinel-unchanged
  rule on every invalid leg, and NULL memptr.
- P3 (35) pins S4/S5/S6: 12 sizes spanning both paths
  at alignment 64 with full-extent writes, the 4
  no-multiple pairs of AL-073-10, the 3 interim zero
  checks of AL-073-20/-21, and the 4 oversize checks
  of AL-073-30 (SIZE_MAX at both entries, SIZE_MAX-7
  at page alignment - the overflow-shaped case).
- P4 (5) pins AL-073-32/-33 and the AL-073-31
  recovery leg: aligned alloc/free, plain malloc after
  it, realloc content preservation with floor-only
  alignment on the result.
- P5 (5) pins AL-073-11 under concurrency of live
  blocks: 32 live aligned blocks, pairwise distinct,
  full-extent patterns intact, reuse after free-all.
- P6 (4) pins AL-073-12 (1 MiB at 4096, guaranteed)
  and AL-073-06 (8192/65536: success-if-aligned,
  clean failure accepted, misaligned success never).
- P7 (5) is the public regression leg: the banked
  four symbols' floor, zeroing, and NULL-free no-op,
  green on BOTH the old and the Z-074 core, so a
  Z-074 regression in the old surface cannot hide
  behind the new one's green.

No clause in S3-S6 lacks a suite leg, and no suite
leg asserts a behaviour S3-S7 do not state. The one
conditional leg (P6 beyond-page) is conditional
because AL-073-06 is conditional; everything else is
unconditional and deterministic.

## 9. Gate statement

Roadmap gate for Z-073: contract text in
contracts.md style + red tests. This document is the
contract text (rows AL-073-*, decisions D-Z073-1..3,
U-11/U-14 settled for this surface, U-09/U-10 interim
postures named as interim). The red tests are the
suite above: on the banked pre-Z-074 core it runs
130 checks, 125 failed, RESULT: FAIL, exit 1, the 5
passes being exactly the P7 public-regression leg -
recorded in tests/contract/recorded_run_z073.txt.
The standing gate stays GREEN 4988/0/77 (the suite is
standalone, not wired into make test, Z-028 precedent).
