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
 * replacement, no usable-size, trim, or reallocarray
 * entry point -- plus, from Z-076 (Wave 9), the public
 * statistics entry point mimir_stats_snapshot below,
 * which promotes the J-002 internal snapshot. Absent symbols are the
 * from Z-077 (Wave 9), mimir_trim and mimir_reallocarray.
 * Nothing else is declared: no standard-name malloc/free
 * replacement, no usable-size or statistics entry point.
 * Absent symbols are the
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
 * Settled cases (Z-078, decision D-013 in
 * docs/DECISION_LEDGER.md) - these ARE contract for this
 * target, stated here in full; the evidence is
 * docs/Z13_Z078_ZERO_SIZE_ERRNO.md:
 * - U-09 SETTLED: a zero-size request is a defined,
 *   non-failure refusal. mimir_malloc(0), mimir_calloc
 *   with a zero factor (either or both),
 *   mimir_realloc(NULL, 0), mimir_reallocarray with a
 *   zero factor and a null ptr, and mimir_aligned_alloc
 *   with a valid alignment and size 0 each return
 *   exactly null; no storage is allocated, no ownership
 *   passes, and the result owns nothing (there is
 *   nothing to free; mimir_free(null) is the defined
 *   no-op above). mimir_posix_memalign with a valid
 *   alignment and size 0 returns ENOMEM and leaves
 *   *memptr unchanged. A zero-size refusal is NOT a
 *   failure: it is counted separately
 *   (alloc_posture_nulls in the statistics surface, a
 *   field name kept from its posture era) and never in
 *   alloc_failures. This target deliberately does NOT
 *   adopt the glibc unique-freeable-pointer outcome
 *   (measured on this host, slice doc S2): a live block
 *   of this allocator always has a non-zero usable
 *   extent (D-012), so a zero-size live block could
 *   not be represented honestly.
 * - U-12 SETTLED: mimir_realloc(ptr, 0) for a live
 *   ptr, and mimir_reallocarray with a live ptr and a
 *   zero factor, return exactly null and leave the
 *   original block live, unchanged in contents and
 *   ownership, and freeable exactly as before -- the
 *   block is NOT freed. Every null return of the
 *   realloc family therefore has one uniform meaning:
 *   the original block, if any, remains caller-owned.
 *   Use mimir_free(ptr) to release a block. (glibc
 *   frees the block in this case; this target does
 *   not -- the deviation is recorded in D-013.)
 * - U-10 / V-02 SETTLED: errno is NOT promised, on
 *   any entry point, in either direction: the
 *   allocator never sets errno as a contract channel,
 *   and no caller may rely on its value after any
 *   call (B-007 S D.1.1). The caller-observable
 *   channels are exactly the return values (null / 0
 *   / 1 / -1, and for mimir_posix_memalign the
 *   returned EINVAL / ENOMEM number, which is a
 *   return value, not errno). One refinement IS
 *   promised: on the zero-size paths above and on
 *   every checked refusal that never reaches the
 *   operating system (arithmetic overflow, invalid
 *   alignment, a size refused by arithmetic alone),
 *   errno is left exactly as it was on entry. On a
 *   genuine operating-system failure errno is
 *   unspecified -- an OS call may have set it
 *   incidentally -- and must not be read.
 *
 * Unresolved cases - NO behaviour is promised for these,
 * and no observed stub behaviour settles them:
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
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* By analogy to MA-6.1-MALLOC-01/-02/-04. See header notes:
 * failure is a null return only; errno is not promised
 * (U-10, settled by D-013); zero-size is the D-013
 * contract null (U-09, settled by Z-078); no numeric
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
 * only). A zero factor is the D-013 contract null
 * (MA-6.1-CALLOC-04; U-09 settled by Z-078: exactly
 * null, not a failure, see header notes); errno is not
 * promised (U-10, settled by D-013). */
void *mimir_calloc(size_t nmemb, size_t size);

/* C-005, per B-008 S4. By analogy to MA-6.1-REALLOC-01
 * (success: min(old, new) preserved, move or stay),
 * MA-6.1-REALLOC-06 (grow tail unspecified, not zeroed),
 * MA-6.1-REALLOC-02 (failure: null, original unchanged
 * and caller-owned) and MA-6.1-REALLOC-03 (NULL ptr acts
 * as mimir_malloc). mimir_realloc(ptr, 0) is the D-013
 * contract case (MA-6.1-REALLOC-04; U-12 settled by
 * Z-078): exactly null, and the original block stays
 * live, unchanged and caller-owned -- it is NOT freed
 * (see header notes). Invalid pointers are undefined
 * behaviour (MA-6.1-REALLOC-05). No in-place or
 * pointer-identity promise is made; errno is not promised
 * (U-10, settled by D-013). */
void *mimir_realloc(void *ptr, size_t size);
/* Z-077 (Wave 9), per the contracts.md S2.8 conditional
 * rows MA-COND-REALLOCARRAY-01/-02/-03, taken up directly
 * for this surface as Z-073/Z-074 took up the aligned
 * rows. The requested size is the checked product
 * nmemb * size: the product is checked before the
 * multiplication is relied on, and overflow is exactly a
 * null return -- no wrapped-around size is allocated and
 * the original block, if any, remains valid, unchanged in
 * contents and ownership, and freeable exactly as before
 * (with MA-6.1-REALLOC-02 / MA-6.2-022 by analogy). On
 * success it behaves exactly as mimir_realloc(ptr,
 * nmemb * size), including min(old, new) preservation;
 * a null ptr behaves as mimir_malloc(product). A zero
 * factor makes the product zero: the case is the D-013
 * contract null (U-09 settled for a null ptr, U-12
 * settled for a live ptr, both by Z-078): exactly null,
 * and a live original block stays live, unchanged and
 * caller-owned -- it is NOT freed. errno is not
 * promised (U-10, settled by D-013). Invalid pointers
 * are undefined behaviour (MA-6.1-REALLOC-05 by
 * analogy). */
void *mimir_reallocarray(void *ptr, size_t nmemb,
                         size_t size);

/* Z-077 (Wave 9): the public trim, over the I-003
 * on-demand trim machinery (docs/PHASE_9_TRIM_
 * IMPLEMENTATION.md; semantics fixed by I-002 S5) and
 * the Z-048 mapping cache. pad is a byte count the
 * caller asks to KEEP in retained memory:
 * - pad == 0 releases every retained empty span and
 *   every pooled dead mapping -- the full trim.
 * - pad > 0 retains up to pad bytes of retained EMPTY
 *   spans: spans are exactly one page on this host, so
 *   the kept amount is pad rounded UP to whole spans
 *   (a partial span cannot be retained; the rounding
 *   never keeps less than pad), and the surplus spans
 *   are released. The mapping pool is NOT touched by a
 *   pad > 0 call.
 * Live spans, live small objects and live large
 * mappings are never touched. The return is exactly
 * 1 if any span or pooled mapping was actually
 * released, else exactly 0 -- including when nothing
 * was retained and when the allocator lock could not
 * be taken (nothing is released then). The call is
 * non-terminal and idempotent: the allocator stays
 * fully usable, and a second identical call with
 * nothing left to release returns 0. No errno is
 * promised (U-10, settled by D-013). */
int mimir_trim(size_t pad);


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
 * is a failure (null), never an invalidity. Zero size
 * with a valid alignment is the D-013 contract null
 * (U-09 settled by Z-078): exactly null, not a failure.
 * errno is not promised (U-10, settled by D-013). A block
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
 * releasable by mimir_free. Zero size with a valid
 * alignment is the D-013 contract case (U-09 settled
 * by Z-078, making the Z-073 contract's interim
 * posture final): ENOMEM with *memptr unchanged. */
int mimir_posix_memalign(void **memptr, size_t alignment,
                         size_t size);

/* Z-083 (Wave 10), per decision D-016 (by analogy under
 * U-03 to MA-6.1-ALIGNED-01/-02, the Z-073/Z-074 aligned
 * rows): the historical memalign shape over the Z-074
 * construction. The validity domain is EXACTLY
 * mimir_aligned_alloc's -- a non-zero power of two -- and
 * every outcome is that entry's: an invalid alignment is
 * a defined failure (exactly null), zero size is the
 * D-013 contract null, a valid but unsatisfiable request
 * is exactly null, and on success the returned pointer
 * satisfies (uintptr_t)ptr % alignment == 0 and is
 * releasable by mimir_free exactly once. This diverges
 * from this host's glibc by recorded decision, as Z-074
 * already did: glibc's memalign accepts non-power-of-two
 * alignments and returns a distinct pointer for size 0
 * (both measured at Z-083); this family refuses the first
 * and nulls the second. The size need NOT be a multiple
 * of the alignment. errno is not promised (U-10, settled
 * by D-013). */
void *mimir_memalign(size_t alignment, size_t size);

/* Z-083 (Wave 10), per decision D-016: the page-aligned
 * shape -- a request of the given size whose pointer is
 * aligned to this host's page size (4096, recorded at
 * Z-002). The size is NOT rounded up; only the pointer's
 * alignment is page-grade, and the block is releasable by
 * mimir_free exactly once. Zero size is the D-013
 * contract null (glibc returns a distinct pointer;
 * diverged from, by D-013). Failure is exactly null;
 * errno is not promised (U-10, settled by D-013). */
void *mimir_valloc(size_t size);

/* Z-083 (Wave 10), per decision D-016: valloc with the
 * size first rounded UP to a whole multiple of this
 * host's page size -- the historical pvalloc shape, so
 * mimir_usable_size of a successful result is at least
 * the rounded size. The round-up is checked before it is
 * relied on: a size whose rounding would overflow is
 * exactly null and no wrapped-around size is allocated.
 * Zero size is the D-013 contract null, applied BEFORE
 * any rounding -- it is never rounded into a one-page
 * allocation (glibc returns a minimum block for
 * pvalloc(0); diverged from, by D-013). The result is
 * page-aligned and releasable by mimir_free exactly
 * once. errno is not promised (U-10, settled by D-013). */
void *mimir_pvalloc(size_t size);

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
/* Z-076: public statistics (the J-002 internal snapshot
 * promoted; docs/Z13_PUBLIC_STATISTICS.md has the full
 * per-counter disposition table). The struct is VERSIONED:
 * struct_version is MIMIR_STATS_VERSION (1) and
 * struct_size is sizeof(struct mimir_stats_v1) as written
 * by this entry point; future versions APPEND fields and
 * bump the version, and this v1 entry point always writes
 * exactly the v1 layout below, so a v1 caller is never
 * overrun. Field meanings are the J-002 definitions
 * (docs/PHASE_10_STATISTICS.md S2) carried unchanged,
 * with the Z-076 dispositions: cross_thread_frees covers
 * both paths for recorded blocks (its small half is
 * cross_thread_small_frees; small frees with no owner
 * record are counted honestly as small_frees_unrecorded,
 * never folded in); lock_contention counts central-lock
 * acquisitions that found the lock already held and
 * lock_contention_measured is 1; live_requested_bytes is
 * a PRICED gap (INC-J002-05): live_requested_measured is
 * 0 and the value is 0 -- no truthful value exists while
 * small objects carry no per-object requested-size
 * record. mimir_stats_snapshot returns 0 on success and
 * -1 (with *out untouched) when out is NULL or the
 * snapshot lock cannot be taken. By analogy under U-03,
 * like every entry point in this header. */
#define MIMIR_STATS_VERSION 1u
#define MIMIR_PUBLIC_STATS_CLASS_COUNT 20u
struct mimir_stats_v1 {
    uint32_t struct_version;
    uint32_t struct_size;
    uint64_t bytes_requested;
    uint64_t bytes_allocated;
    uint64_t bytes_cached;
    uint64_t bytes_mapped;
    uint64_t bytes_released;
    uint64_t peak_mapped;
    uint64_t alloc_failures;
    uint64_t alloc_posture_nulls;
    uint64_t cross_thread_frees;
    uint64_t lock_contention;
    uint64_t lock_contention_measured;
    uint64_t live_mappings;
    uint64_t live_spans;
    uint64_t live_small_objects;
    uint64_t live_mapping_bytes;
    uint64_t live_span_bytes;
    uint64_t page_size_bytes;
    uint64_t span_class_size[MIMIR_PUBLIC_STATS_CLASS_COUNT];
    uint64_t span_live_objects[MIMIR_PUBLIC_STATS_CLASS_COUNT];
    uint64_t span_total_spans[MIMIR_PUBLIC_STATS_CLASS_COUNT];
    uint64_t span_empty_spans[MIMIR_PUBLIC_STATS_CLASS_COUNT];
    uint64_t span_capacity_objects[MIMIR_PUBLIC_STATS_CLASS_COUNT];
    uint64_t unlock_failures;
    uint64_t drain_lock_failures;
    uint64_t trim_lock_failures;
    uint64_t free_lock_failures;
    uint64_t metadata_detections;
    uint64_t metadata_collateral_refusals;
    uint64_t live_release_detections;
    uint64_t live_release_collateral_refusals;
    uint64_t double_free_detections;
    uint64_t poison_detections;
    /* Z-076 appends (end of struct, the house discipline): */
    uint64_t cross_thread_small_frees;
    uint64_t small_frees_unrecorded;
    uint64_t live_requested_bytes;
    uint64_t live_requested_measured;
};
int mimir_stats_snapshot(struct mimir_stats_v1 *out);

#ifdef __cplusplus
}
#endif

#endif /* MIMIR_ALLOC_H */
