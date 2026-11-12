# Z13 Trim and Reallocarray Contract

Slice: Z-077 - Trim and reallocarray surface (Wave 9).
Status: contract written and implemented by this slice,
2026-11-12, team-05 worktree. Artifacts:
docs/Z13_Z077_TRIM_REALLOCARRAY.md (this document),
include/mimir_alloc.h (the two declarations and their
contract notes), src/core/mimir_core.c
(mimir_core_trim, mimir_core_reallocarray),
src/api/mimir_alloc.c (L1 forwarders + E-005 audit
counters), src/api/mimir_alloc.map (export contract,
six -> eight symbols), tests/abi/test_z077.c (the
contract suite), tests/abi/test_z077_dyn.c and
tests/abi/test_z077_cpp.cpp (the .so consumers),
tests/abi/header_only_z077.c / .cpp,
tests/abi/run_z077.sh, filed as
tests/abi/recorded_run_z13_z077.txt.

Roadmap gate: "contract tests incl. overflow edges
green." The suite is standalone (`make test-z077`), NOT
part of `make test`; the standing gate count is
unchanged. Normative rows carry [NORM] with RFC 2119
capitals inside the row only; every row is classified
Defined / UB / Debug-detectable only; open items are
named by their U-identifier and state no behaviour.
Row identifiers are TR-077-* (trim) and RA-077-*
(reallocarray).

## 0. Position and honesty notes

- Before this slice the public surface was exactly six
  entry points (the C-001/C-005 four plus the Z-074
  aligned pair), and the absence of trim and
  reallocarray was the documented unsupported behaviour
  (B-008 S6.1). The I-003 slice doc recorded the debt
  verbatim: the trim machinery stays internal-only and
  "a public trim API is owed to a later ABI/contract
  slice". This slice is that slice for trim; for
  reallocarray it takes up the contracts.md S2.8
  conditional rows MA-COND-REALLOCARRAY-01/-02/-03
  directly for the namespaced surface, as Z-073/Z-074
  took up the aligned rows under U-03.
- U-series items this contract does NOT settle:
  U-09 (zero size) and U-12 (realloc to zero) - both
  owned by Z-078; S3 pins only the interim family
  posture so a later change is visible, never silent.
  U-10 (errno) stays open: no errno is promised by
  either entry (S4). U-03 is discharged for THIS
  surface by this document: these rows are direct,
  not by analogy.
- Standard-name symbols are out of scope: no
  malloc_trim, no reallocarray is promised, exported,
  or interposed by this contract. Interposition is
  Wave 10.
- This contract promises NO speed number and NO
  internal layout.

## 1. The surface

Exactly two namespaced entries are added:

    int mimir_trim(size_t pad);
    void *mimir_reallocarray(void *ptr, size_t nmemb,
                             size_t size);

The shared library's dynamic exports grow from six to
exactly eight; the version script stays an anonymous
node (no ABI-stability promise is made, B-008 N-10),
and every internal symbol stays bound local (proven in
the recorded run S2/S3).

## 2. Trim rows (over the I-003 machinery)

The public trim is one locked composition over the
banked machinery: the I-003 retained-span walk
(trim_some_locked, the same single release path the
drain uses, I-002 S5.1) and, for the full form only,
the Z-048 mapping-pool release
(pool_release_all_locked - the same trigger
mimir_core_trim_retained fires). Spans on this host
are exactly one 4096-byte page (span publication
proves mapping_length == page size). Decisions:

- D-Z077-1 (signature and return): the glibc shape -
  an int return, a size_t pad. The return is a report
  of fact, not of effort: exactly 1 if at least one
  span or pooled mapping was actually released by
  this call, else exactly 0.
- D-Z077-2 (pad pricing): pad is a byte count the
  caller asks to KEEP in retained empty spans. The
  kept span count is pad rounded UP to whole spans
  (ceil(pad / page_size)): a partial span cannot be
  retained, and the rounding never keeps less than
  pad. pad == 0 keeps nothing and is the FULL trim
  (spans + pool). pad > 0 is the span-budget form
  only: the mapping pool is never touched, exactly
  the mimir_core_trim_retained_some contract.

Rows:

- TR-077-01 [NORM] Defined: mimir_trim(0) MUST
  release every retained empty span and every pooled
  dead mapping, and MUST return 1 if at least one
  span or mapping was released, else 0.
- TR-077-02 [NORM] Defined: for pad > 0, mimir_trim
  MUST retain at least pad bytes of retained empty
  spans (whole-span rounding per D-Z077-2), MUST
  release the surplus spans, MUST NOT touch the
  mapping pool, and MUST return 1 iff at least one
  span was released.
- TR-077-03 [NORM] Defined: live spans, live small
  objects and live large mappings MUST NOT be
  touched by any trim call.
- TR-077-04 [NORM] Defined: if the allocator lock
  cannot be taken, nothing is released and the
  return is 0; the course is counted in the Z-065
  trim lock-failure tally, so the 0 is never the only
  evidence (D-010 posture).
- TR-077-05 [NORM] Defined: trim is non-terminal and
  idempotent - the allocator stays fully usable, and
  a repeated call with nothing left to release
  returns 0.
- TR-077-06 (fact, not a promise): every trim call
  opens with the I-003 preamble (TLS flush + drain)
  shared with mimir_core_trim_retained; the drain can
  itself retire drained empty spans beyond the
  G-007 retention bound. Callers measuring retained
  bytes across a first trim call will observe that
  settlement; the pad budget governs everything
  after it. The contract suite absorbs it with one
  settle call before its exact assertions (test
  S6).

## 3. Reallocarray rows (the overflow contract)

The requested size is the product nmemb * size,
contracted with the mimir_core_calloc order of
operations (B-008 S3.2 carried to this entry):

- RA-077-01 [NORM] Defined: on success
  mimir_reallocarray(ptr, n, s) MUST behave exactly
  as mimir_realloc(ptr, n * s): the first
  min(old, product) requested bytes are preserved
  byte-for-byte, move or stay; a null ptr MUST
  behave as mimir_malloc(product)
  (MA-COND-REALLOCARRAY-01 with MA-6.1-REALLOC-01/
  -03).
- RA-077-02 [NORM] Defined: the product MUST be
  checked before the multiplication is relied on
  (nmemb <= SIZE_MAX / size, with the zero-factor
  gate first). Overflow MUST be exactly a null
  return: no wrapped-around size is allocated,
  nothing is published, and the original block, if
  any, MUST remain valid, unchanged in contents and
  ownership, and freeable exactly as before
  (MA-COND-REALLOCARRAY-02/-03 with
  MA-6.1-REALLOC-02, MA-6.2-022).
- RA-077-03 [NORM] Defined (accounting): overflow
  is a genuine failure - counted in the J-002
  alloc_failures (Q-FAILED-OS), never in
  alloc_posture_nulls, and reported on the J-007
  diagnostics channel as op REALLOC / cause
  ARITHMETIC with req = SIZE_MAX, the calloc
  overflow shape verbatim. An unsatisfiable
  representable product is likewise a genuine
  failure NULL with the original preserved (the
  realloc failure row).
- RA-077-04 (posture only, NOT a settlement): a
  zero factor makes the product zero - [UNRESOLVED:
  U-09] with a null ptr, [UNRESOLVED: U-12] with a
  live one. This implementation returns null, leaves
  the original block live and unchanged, and counts
  the call as a posture null. No caller or test may
  cite this as settling U-09 or U-12; Z-078 owns the
  closure and may change the behaviour.
- RA-077-05 [NORM] UB: a foreign, interior, or
  already-freed ptr is undefined behaviour with no
  defined error (MA-6.1-REALLOC-05).

## 4. errno

Neither entry promises errno (U-10 / V-02 remains
open). The null return (reallocarray) and the 1/0
return (trim) are the only caller-observable failure
channels. In particular mimir_reallocarray does NOT
adopt the glibc ENOMEM-on-overflow behaviour as a
promise, and no test asserts it.

## 5. Audit counters

The E-005 shape is carried to both entries:
mimir_audit_call_trim and mimir_audit_call_
reallocarray, appended after the Z-074 pair in
src/api/mimir_alloc.c (each counts exactly one per
L1 invocation, whatever the outcome), declared for
tests only in tests/abi/mimir_audit_e005.h, bound
local in the shared library. The contract suite
reconciles both counters exactly against its own
counting wrappers (test S8).

## 6. Evidence

tests/abi/test_z077.c (65 checks): reallocarray
success paths small and large; the overflow edge set
(SIZE_MAX, 2), (2, SIZE_MAX), (SIZE_MAX/2 + 1, 2),
(SIZE_MAX, SIZE_MAX), (2^32, 2^32) - the last wraps
to exactly 0 and must still refuse; failure
preservation with a live ptr (contents byte-intact,
stats deltas exact: overflow and unsatisfiable are
failures, zero factors are postures, the two never
mixed); worker-thread use; the trim ladder (settle,
keep-everything pads release nothing, keep-one-page
exactness, full trim to zero, idempotent repeat);
the pool legs (pad > 0 never touches the pool,
pad == 0 releases it); audit reconciliation. The
dynamic and C++ consumers prove the .so surface;
the recorded run files the nm -D / DT_NEEDED /
absence proofs. The standing correctness gate
(scripts/z13/run_gate.sh) is run on the final tree
and its counts are recorded in the slice's
CAMPAIGN_STATE.md entry; the gate total does not
move, because this suite is standalone by design.
