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
 * amended by B-008 -- plus, from Z-074 (Wave 9), the two
 * explicit-alignment entry points of the Z-073 contract:
 * mimir_aligned_alloc and mimir_posix_memalign -- plus,
 * from Z-075 (Wave 9), the usable-size query
 * mimir_usable_size (decision D-012). Nothing
 * else is declared: no standard-name malloc/free
 * replacement, no statistics, trim, or
 * reallocarray entry point. Absent symbols are the
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
 * - U-11 / V-03: explicit-alignment validity was open
 *   while no entry point existed. The Z-073 contract
 *   records this host's rules and Z-074 implements them:
 *   an alignment is valid iff it is a non-zero power of
 *   two (the posix-shaped entry additionally requires a
 *   multiple of sizeof(void *)). These are THIS HOST's
 *   recorded rules, not a portable claim for other
 *   targets. See the aligned entries' notes below.
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

/* Z-074, per the Z-073 contract (by analogy under U-03 to
 * MA-6.1-ALIGNED-01/-02). A valid alignment is a non-zero
 * power of two (this host's recorded V-03 rule, above);
 * any other value -- including 0 -- is a defined failure:
 * exactly a null return, no allocation. On success the
 * returned pointer satisfies (uintptr_t)ptr % alignment
 * == 0 and is releasable by mimir_free exactly once (the
 * exact returned pointer only; interior pointers are
 * undefined behaviour, as for every family block). The
 * size need NOT be a multiple of the alignment -- a
 * recorded deviation from C11 aligned_alloc, in the
 * glibc posture. A valid alignment too large to satisfy
 * is a failure (null), never an invalidity. Zero size is
 * [UNRESOLVED: U-09]: this implementation returns null as
 * a posture only. errno is not promised (U-10). A block
 * from this entry realloc'd through mimir_realloc keeps
 * the realloc rules above; the RESULT of a realloc makes
 * no alignment promise beyond the family floor, though a
 * failure leaves the original block -- and its alignment
 * -- intact, and same-size / shrink results are the
 * original pointer in this implementation (facts, not
 * promises). */
void *mimir_aligned_alloc(size_t alignment, size_t size);

/* Z-074, per the Z-073 contract (by analogy under U-03 to
 * MA-6.1-POSIXMA-01/-02; the V-06 error model: the return
 * value is the error channel and errno is NOT set by this
 * entry). Validation precedence is fixed: a null memptr
 * returns EINVAL (POSIX leaves the case undefined; EINVAL
 * is this implementation's recorded choice); an alignment
 * that is not a non-zero power of two multiple of
 * sizeof(void *) returns EINVAL with *memptr unchanged;
 * a valid but unsatisfiable request returns ENOMEM with
 * *memptr unchanged; success returns 0 with *memptr set
 * to a pointer aligned as in mimir_aligned_alloc and
 * releasable by mimir_free. Zero size is [UNRESOLVED:
 * U-09]: this implementation returns
 * ENOMEM with *memptr unchanged, as the Z-073
 * contract's interim posture for this surface (final
 * closure is Z-078's). */
int mimir_posix_memalign(void **memptr, size_t alignment,
                         size_t size);

/* Z-075, per decision D-012 (by analogy under U-03 to
 * MA-COND-USABLE-01/-02; the V-05 semantics for this
 * target are defined by D-012): the exact usable extent
 * of a live block from this allocator family -- the size
 * class's extent for a class-served block, the mapping's
 * usable extent for a mapping-served block (which a
 * realloc shrink does NOT reduce: shrink lowers only the
 * requested-size record). The result is read from the
 * allocator's records, never derived from the request,
 * and the call modifies nothing (MA-COND-USABLE-01's
 * no-modification clause is part of this signature: the
 * parameter is const). mimir_usable_size(NULL) is 0
 * (MA-COND-USABLE-02's null case, defined for this
 * target). A pointer that is not a live family block --
 * foreign, interior, or already freed -- is answered 0
 * whenever the allocator can determine it (a block freed
 * by the calling thread is always determined); the
 * single indeterminable residence, a block sitting in
 * another thread's TLS cache bin, is recorded in D-012:
 * asking after any freed block is use after free,
 * undefined behaviour across the family
 * (MA-COND-USABLE-03), and no caller may rely on an
 * answer for one. The standard name malloc_usable_size
 * is NOT declared by this header and NOT exported by
 * the library (D-012): standard-name interposition is
 * the Wave 10 opt-in variant's business, never a
 * surprise from the namespaced surface. errno is not
 * promised (U-10): 0 is the only refusal value and also
 * the NULL answer, by design -- a live block's usable
 * extent is never 0. */
size_t mimir_usable_size(const void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* MIMIR_ALLOC_H */
