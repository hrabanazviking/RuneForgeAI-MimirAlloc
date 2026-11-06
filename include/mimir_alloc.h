/* mimir_alloc.h - MimirAlloc public header (Phase 3 foundation).
 *
 * Slices: C-001 (mimir_malloc / mimir_free); C-005 adds
 * mimir_calloc / mimir_realloc, the declaration slice the
 * C-001 note (INC-C001) left owed. Status: Proposed. This
 * header declares the Phase 3 namespaced surface only; it
 * is not a stable ABI.
 *
 * Scope: ONLY the namespaced entry points scoped by
 * docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md (B-006) and
 * docs/PHASE_3_CALLOC_REALLOC_AND_UNSUPPORTED_BEHAVIOR.md
 * (B-008): mimir_malloc, mimir_free, mimir_calloc and
 * mimir_realloc -- the B-008 S7.2 supported-call list, as
 * amended by B-008. Nothing else is declared: no
 * standard-name malloc/free replacement, no
 * explicit-alignment entry point, no usable-size, statistics,
 * trim, or reallocarray entry point. Absent symbols are the
 * documented unsupported behaviour for those facilities
 * (B-008 S6.1: not present - absence at link time, never a
 * runtime refusal and never a silent fallback to another
 * allocator).
 *
 * U-03 caveat (read first): the namespaced mimir_* API has
 * NO direct contract rows in docs/contracts.md; it is
 * [UNRESOLVED: U-03], outside contracts S6 scope, Appendix A
 * stability Unassigned. Every contract mapping below is BY
 * ANALOGY ONLY to the core-symbol rows, Proposed only -
 * never a direct namespaced contract and never an ABI,
 * signature-stability, or symbol-version promise (B-008 N-10).
 *
 * Language: portable C11 core, C ABI (Decision D-006 in
 * docs/DECISION_LEDGER.md).
 *
 * Defined cases for the declared pair (by analogy):
 * - mimir_malloc(size) with size > 0 and representable:
 *   success is a non-null pointer to distinct storage,
 *   suitably aligned for any object type with a fundamental
 *   alignment requirement (MA-6.1-MALLOC-01 by analogy);
 *   failure - including an oversize/unrepresentable request
 *   (MA-6.1-MALLOC-04 by analogy) or an unsatisfiable
 *   request - is exactly a null return, never a pointer to
 *   storage not allocated to the caller, never a partial or
 *   smaller block (MA-6.1-MALLOC-02, MA-6.2-020, MA-6.2-024
 *   by analogy; B-007 S D.0).
 * - mimir_free(ptr) of a live pointer returned by this
 *   allocator family retires that block exactly once
 *   (MA-6.1-FREE-01, MA-6.2-001 by analogy).
 *   mimir_free(NULL) is a defined no-op that returns
 *   normally (MA-6.1-FREE-02, MA-6.2-010 by analogy).
 *   mimir_free returns no value on any defined path.
 * - mimir_calloc(nmemb, size) with both factors > 0
 *   (C-005): success is a non-null pointer to distinct
 *   storage, suitably aligned as in mimir_malloc of the
 *   product size, with every byte of the requested extent
 *   zero at return -- the usable extent is zeroed in
 *   construction, so calloc DOES promise zeroed user
 *   contents, unlike mimir_malloc (MA-6.1-CALLOC-01, -05
 *   by analogy). The product is checked before the
 *   multiplication is relied on: overflow is exactly a
 *   null return and no wrapped-around size is allocated
 *   (MA-6.1-CALLOC-02 by analogy); any other failure is
 *   exactly a null return with no ownership passing
 *   (MA-6.1-CALLOC-03 by analogy).
 * - mimir_realloc(ptr, new_size) with new_size > 0 (C-005):
 *   on success the caller owns a block of the new size at
 *   the returned pointer, which can differ from ptr, and
 *   the first min(old, new) requested bytes are preserved
 *   byte-for-byte, whether the storage moves or stays in
 *   place (MA-6.1-REALLOC-01 by analogy); bytes beyond the
 *   preserved range are unspecified and are NOT zeroed
 *   (MA-6.1-REALLOC-06 by analogy). On failure the result
 *   is exactly a null return and the original block
 *   remains valid, unchanged in contents and ownership,
 *   and freeable exactly as before (MA-6.1-REALLOC-02,
 *   MA-6.2-022 by analogy). mimir_realloc(NULL, n) behaves
 *   as mimir_malloc(n) for n > 0, success and failure
 *   alike (MA-6.1-REALLOC-03 by analogy). No in-place
 *   grow, pointer identity, or shrink memory return is
 *   promised at contract level (B-008 S4.4 / S6.3 X-6):
 *   in this design grow always moves and shrink retains
 *   the mapping, as design facts only -- callers must
 *   always use the returned pointer.
 *
 * Unresolved cases - NO behaviour is promised for these,
 * and no observed stub behaviour settles them:
 * - U-09: zero-size requests (mimir_malloc(0), a zero
 *   factor in mimir_calloc, mimir_realloc(NULL, 0)). No
 *   outcome is stated; callers must not rely on any. This
 *   implementation returns null for each of them as a
 *   posture only, which does not settle U-09.
 * - U-12: mimir_realloc(ptr, 0) for a live ptr. No outcome
 *   is stated; callers must not rely on any, and the call
 *   is outside the supported list -- use mimir_free(ptr)
 *   to release a block. This implementation returns null
 *   and leaves the original block live and unchanged, as a
 *   posture only, which does not settle U-12.
 * - U-10 / V-02: errno on failure is NOT promised. The only
 *   caller-observable failure channel of mimir_malloc is
 *   the null return; no test or caller may rely on errno
 *   from either declared function (B-007 S D.1.1).
 * - U-15 / V-07: no numeric default alignment value is
 *   claimed. Suitably aligned above is the non-numeric
 *   claim only (B-007 S B.1).
 * - U-11 / V-03: explicit-alignment validity is open and
 *   no explicit-alignment entry point exists here (above).
 *
 * Other boundaries (by analogy / design record):
 * - Contents of a successful allocation are unspecified -
 *   NOT zero-guaranteed (B-007 S C1). The single exception
 *   is mimir_calloc, whose requested extent is guaranteed
 *   zero at return (above).
 * - Freeing or reallocating a foreign, interior, or
 *   already-freed pointer, and use after free, are
 *   undefined behaviour with no defined error, return, or
 *   recovery (MA-6.1-FREE-03, MA-6.1-FREE-04,
 *   MA-6.1-REALLOC-05, MA-6.2-030..032 by analogy;
 *   B-007 S D.3).
 * - No async-signal-safety is stated or implied for either
 *   function (MA-6.3-030..032; B-008 S7.1 item 3).
 * - No fork-safety claim is made (U-17..U-21 remain open).
 */
#ifndef MIMIR_ALLOC_H
#define MIMIR_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* By analogy to MA-6.1-MALLOC-01/-02/-04. See header notes:
 * failure is a null return only; errno is not promised
 * (U-10); zero-size is unresolved (U-09); no numeric
 * alignment value is claimed (U-15). */
void *mimir_malloc(size_t size);

/* By analogy to MA-6.1-FREE-01/-02. mimir_free(NULL) is a
 * defined no-op. Invalid pointers are undefined behaviour
 * with no defined error (see header notes). */
void mimir_free(void *ptr);

/* C-005, per B-008 S3. By analogy to MA-6.1-CALLOC-01/-05
 * (success zeroed over the requested extent, suitably
 * aligned), MA-6.1-CALLOC-02 (overflow checked before the
 * multiplication is relied on: null, no wrapped
 * allocation) and MA-6.1-CALLOC-03 (other failure: null
 * only). A zero factor is [UNRESOLVED: U-09]
 * (MA-6.1-CALLOC-04): this implementation returns null as
 * a posture only (see header notes); errno is not promised
 * (U-10). */
void *mimir_calloc(size_t nmemb, size_t size);

/* C-005, per B-008 S4. By analogy to MA-6.1-REALLOC-01
 * (success: min(old, new) preserved, move or stay),
 * MA-6.1-REALLOC-06 (grow tail unspecified, not zeroed),
 * MA-6.1-REALLOC-02 (failure: null, original unchanged
 * and caller-owned) and MA-6.1-REALLOC-03 (NULL ptr acts
 * as mimir_malloc). mimir_realloc(ptr, 0) is [UNRESOLVED:
 * U-12] (MA-6.1-REALLOC-04): this implementation returns
 * null and leaves the original live, as a posture only
 * (see header notes). Invalid pointers are undefined
 * behaviour (MA-6.1-REALLOC-05). No in-place or
 * pointer-identity promise is made; errno is not promised
 * (U-10). */
void *mimir_realloc(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* MIMIR_ALLOC_H */
