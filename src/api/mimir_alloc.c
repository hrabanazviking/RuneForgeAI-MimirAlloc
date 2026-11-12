/* mimir_alloc.c - MimirAlloc L1 public boundary (slice C-003;
 * calloc/realloc forwarders added by slice C-005;
 * aligned forwarders added by slice Z-074; trim and
 * reallocarray forwarders added by slice Z-077).
 *
 * This translation unit is the L1 public boundary of the
 * Phase 3 minimal allocator (B-006 S4). It implements no
 * allocator logic of its own: it forwards the four namespaced
 * entry points declared in include/mimir_alloc.h to the core
 * in src/core/mimir_core.c, which owns admission (L2),
 * ownership bookkeeping (L4) and mapping layout (L5) and
 * consumes the OS only through the L6 platform layer
 * (src/platform/, slice C-002). No layer skips to L6, and no
 * code here names an OS facility (B-006 S4 dependency law).
 *
 * The allocator behind this boundary is the roadmap's
 * explicitly experimental mapping-per-allocation correctness
 * baseline: each allocation is backed by its own OS mapping.
 * It is NOT a performance allocator (B-006 S8.5: no reuse,
 * splitting, coalescing, size classes or caching).
 *
 * Behaviour, by analogy only (the namespaced pair has no
 * direct contract rows -- [UNRESOLVED: U-03]; see the public
 * header for the full caveat list):
 * - mimir_malloc(size), size > 0 and representable: on success
 *   a non-null pointer to distinct, suitably aligned storage
 *   (MA-6.1-MALLOC-01 by analogy); on any failure exactly a
 *   null return -- never a partial or smaller block, never a
 *   pointer to storage not allocated to the caller
 *   (MA-6.1-MALLOC-02/-04, MA-6.2-020/-024 by analogy).
 *   errno is not promised (U-10); the null return is the only
 *   caller-observable failure channel (B-007 D.0).
 * - mimir_malloc(0): returns NULL -- since Z-078 the
 *   D-013 contract null (U-09 settled): a defined,
 *   non-failure refusal, continuous with the C-001 stub's
 *   posture, now stated as contract in the public header
 *   (B-006 F-05; B-007 A1/A4).
 * - Allocation contents are unspecified -- NOT zero-guaranteed
 *   (B-007 C1). The allocator never touches user bytes.
 * - mimir_free(ptr) of a live pointer from this allocator
 *   retires that block exactly once (MA-6.1-FREE-01 by
 *   analogy); mimir_free(NULL) is a defined no-op
 *   (MA-6.1-FREE-02 by analogy). Foreign, interior or
 *   already-freed pointers, and use after free, are undefined
 *   behaviour with no defined error or recovery (B-006 S8.3).
 * - mimir_calloc(nmemb, size), both factors > 0 (C-005,
 *   B-008 S3 by analogy): the product is checked before the
 *   multiplication is relied on; overflow is exactly a null
 *   return with no wrapped allocation (MA-6.1-CALLOC-02 by
 *   analogy); on success the usable extent is explicitly
 *   zeroed before publication, so every byte of the
 *   requested extent reads zero at return (MA-6.1-CALLOC-01
 *   /-05 by analogy) -- calloc DOES promise zeroed user
 *   contents, unlike mimir_malloc (B-007 C1); other failure
 *   is exactly a null return (MA-6.1-CALLOC-03 by analogy).
 *   A zero factor returns NULL: the D-013 contract null
 *   since Z-078 (MA-6.1-CALLOC-04; U-09 settled; the
 *   C-005 posture it began as is recorded in
 *   tests/core/NOTES_C005.md, INC-C005-01).
 * - mimir_realloc(ptr, new_size), new_size > 0 (C-005,
 *   B-008 S4 by analogy): success preserves the first
 *   min(old, new) requested bytes byte-for-byte, whether
 *   the storage moves or stays (MA-6.1-REALLOC-01 by
 *   analogy; grow always moves in this machine, shrink and
 *   same-size retain -- B-008 S4.4 design facts, no
 *   pointer-identity promise); bytes beyond the preserved
 *   range are unspecified and not zeroed (MA-6.1-REALLOC-06
 *   by analogy). Failure is exactly a null return and the
 *   original block remains valid, unchanged and caller-owned
 *   (MA-6.1-REALLOC-02, MA-6.2-022 by analogy).
 *   mimir_realloc(NULL, n) behaves as mimir_malloc(n) for
 *   n > 0, success and failure alike (MA-6.1-REALLOC-03 by
 *   analogy). mimir_realloc(ptr, 0) returns NULL with the
 *   original block left LIVE, unchanged and caller-owned:
 *   since Z-078 the D-013 contract case (MA-6.1-REALLOC-
 *   04; U-12 settled; the C-005 posture it began as is
 *   recorded in INC-C005-01). An invalid ptr is
 *   undefined behaviour with no defined error
 *   (MA-6.1-REALLOC-05 by analogy).
 * - No async-signal-safety and no fork-safety is claimed.
 *
 * Clean-room: written from the banked design documents
 * (B-006, B-007, B-008) and this repository's own C-001/C-002
 * code only. The C-001 stub that previously occupied this
 * file is replaced in full by this slice; its documented
 * stub postures are superseded as noted above.
 */
#include "mimir_alloc.h"

#include "../core/mimir_core.h"

/*
 * E-005 audit counters (docs/PHASE_5_AUDIT_HOOK_E005.md;
 * INC-E001-07 shape (b)). These four counters count CALLS
 * at this L1 boundary: each increments exactly once per
 * invocation of its public entry point, whatever the
 * outcome (success, defined failure, or unresolved-case
 * posture). They are INTERNAL audit state, not a public
 * facility: they are declared in no installed header (the
 * public header include/mimir_alloc.h is unchanged and
 * still declares exactly the four entry points), they are
 * defined with default visibility in this object only so
 * that a test-only static build can read them (see
 * tests/abi/mimir_audit_e005.h), and in the shared library
 * the E-002 version script (src/api/mimir_alloc.map,
 * local: *) binds them local, so they never appear in
 * libmimiralloc.so's dynamic symbol table. Contract
 * status: NOT part of the contract - Proposed-internal,
 * [UNRESOLVED: U-03] applies as it does to the whole
 * namespaced surface; no caller may rely on them.
 * Increments use __atomic_fetch_add (relaxed) so the
 * counts stay exact under concurrent calls without
 * taking the allocator lock on this layer's behalf.
 */
_Alignas(64) unsigned long mimir_audit_call_malloc; /* Z-040: own 64 B line - lock-free per-call counter (Z-016 rule) */
_Alignas(64) unsigned long mimir_audit_call_calloc; /* Z-040: own 64 B line - lock-free per-call counter (Z-016 rule) */
_Alignas(64) unsigned long mimir_audit_call_realloc; /* Z-040: own 64 B line - lock-free per-call counter (Z-016 rule) */
_Alignas(64) unsigned long mimir_audit_call_free; /* Z-040: own 64 B line - lock-free per-call counter (Z-016 rule) */

void *mimir_malloc(size_t size)
{
    __atomic_fetch_add(&mimir_audit_call_malloc, 1UL,
                       __ATOMIC_RELAXED);
    return mimir_core_alloc(size);
}

void mimir_free(void *ptr)
{
    __atomic_fetch_add(&mimir_audit_call_free, 1UL,
                       __ATOMIC_RELAXED);
    mimir_core_free(ptr);
}

void *mimir_calloc(size_t nmemb, size_t size)
{
    __atomic_fetch_add(&mimir_audit_call_calloc, 1UL,
                       __ATOMIC_RELAXED);
    return mimir_core_calloc(nmemb, size);
}

void *mimir_realloc(void *ptr, size_t size)
{
    __atomic_fetch_add(&mimir_audit_call_realloc, 1UL,
                       __ATOMIC_RELAXED);
    return mimir_core_realloc(ptr, size);
}

/*
 * Z-074 audit counters: the E-005 shape carried to the two
 * Wave 9 entry points -- each counts CALLS at this L1
 * boundary, exactly once per invocation, whatever the
 * outcome. They are APPENDED after the original four
 * counters so every existing reader of the first four is
 * undisturbed; like them they are declared in no installed
 * header, read only by a test-only static build (the
 * declarations are appended to tests/abi/mimir_audit_e005.h),
 * and bound local in the shared library by the version
 * script's local: * -- the .so export count is exactly the
 * six public entry points after Z-074, never these.
 */
_Alignas(64) unsigned long mimir_audit_call_aligned_alloc;
_Alignas(64) unsigned long mimir_audit_call_posix_memalign;

void *mimir_aligned_alloc(size_t alignment, size_t size)
{
    __atomic_fetch_add(&mimir_audit_call_aligned_alloc, 1UL,
                       __ATOMIC_RELAXED);
    return mimir_core_aligned_alloc(alignment, size);
}

int mimir_posix_memalign(void **memptr, size_t alignment,
                         size_t size)
{
    __atomic_fetch_add(&mimir_audit_call_posix_memalign, 1UL,
                       __ATOMIC_RELAXED);
    return mimir_core_posix_memalign(memptr, alignment, size);
}

/*
 * Z-075 audit counter: the E-005 shape carried to the
 * usable-size entry point -- counts CALLS at this L1
 * boundary, exactly once per invocation, whatever the
 * outcome (a live answer, the NULL answer, or a refusal
 * 0). APPENDED after the Z-074 counters so every
 * existing reader is undisturbed; declared in no
 * installed header, read only by a test-only static
 * build (the declaration is appended to
 * tests/abi/mimir_audit_e005.h), and bound local in the
 * shared library by the version script's local: * --
 * the .so export count is exactly the seven public
 * entry points after Z-075, never this.
 */
_Alignas(64) unsigned long mimir_audit_call_usable_size;

size_t mimir_usable_size(const void *ptr)
{
    __atomic_fetch_add(&mimir_audit_call_usable_size, 1UL,
                       __ATOMIC_RELAXED);
    return mimir_core_usable_size(ptr);
}

/*
 * Z-076 audit counter: the E-005 shape carried to the
 * statistics entry point -- counts CALLS at this L1
 * boundary, exactly once per invocation, whatever the
 * outcome. Appended after the Z-074 counters; declared in
 * no installed header, read only by a test-only static
 * build (tests/abi/mimir_audit_e005.h carries the
 * declaration), and bound local in the shared library by
 * the version script's local: * -- the .so export count is
 * exactly the seven public entry points after Z-076,
 * never this.
 */
_Alignas(64) unsigned long mimir_audit_call_stats_snapshot;

/*
 * Z-076: the public statistics forwarder. It owns no
 * statistics logic: it takes the one locked internal
 * snapshot (mimir_core_stats_snapshot, J-002) and copies
 * it field by field into the public versioned struct,
 * stamping the version pair first. The copy is explicit
 * (never a struct memcpy across the boundary) so the
 * public layout stays an independent, versioned record
 * even as the internal struct grows by appends. On a NULL
 * out or a failed snapshot it returns -1 and *out is
 * untouched -- the internal snapshot already guarantees
 * the untouched half; the NULL half is checked here
 * before anything is written.
 */
int mimir_stats_snapshot(struct mimir_stats_v1 *out)
{
    struct mimir_stats s;
    size_t i;

    __atomic_fetch_add(&mimir_audit_call_stats_snapshot, 1UL,
                       __ATOMIC_RELAXED);
    if (out == NULL) {
        return -1;
    }
    if (mimir_core_stats_snapshot(&s) != 0) {
        return -1;
    }
    out->struct_version = MIMIR_STATS_VERSION;
    out->struct_size = (uint32_t)sizeof(struct mimir_stats_v1);
    out->bytes_requested = s.bytes_requested;
    out->bytes_allocated = s.bytes_allocated;
    out->bytes_cached = s.bytes_cached;
    out->bytes_mapped = s.bytes_mapped;
    out->bytes_released = s.bytes_released;
    out->peak_mapped = s.peak_mapped;
    out->alloc_failures = s.alloc_failures;
    out->alloc_posture_nulls = s.alloc_posture_nulls;
    out->cross_thread_frees = s.cross_thread_frees;
    out->lock_contention = s.lock_contention;
    out->lock_contention_measured = s.lock_contention_measured;
    out->live_mappings = s.live_mappings;
    out->live_spans = s.live_spans;
    out->live_small_objects = s.live_small_objects;
    out->live_mapping_bytes = s.live_mapping_bytes;
    out->live_span_bytes = s.live_span_bytes;
    out->page_size_bytes = s.page_size_bytes;
    for (i = 0; i < MIMIR_PUBLIC_STATS_CLASS_COUNT; i++) {
        out->span_class_size[i] = s.span_class_size[i];
        out->span_live_objects[i] = s.span_live_objects[i];
        out->span_total_spans[i] = s.span_total_spans[i];
        out->span_empty_spans[i] = s.span_empty_spans[i];
        out->span_capacity_objects[i] =
            s.span_capacity_objects[i];
    }
    out->unlock_failures = s.unlock_failures;
    out->drain_lock_failures = s.drain_lock_failures;
    out->trim_lock_failures = s.trim_lock_failures;
    out->free_lock_failures = s.free_lock_failures;
    out->metadata_detections = s.metadata_detections;
    out->metadata_collateral_refusals =
        s.metadata_collateral_refusals;
    out->live_release_detections = s.live_release_detections;
    out->live_release_collateral_refusals =
        s.live_release_collateral_refusals;
    out->double_free_detections = s.double_free_detections;
    out->poison_detections = s.poison_detections;
    out->cross_thread_small_frees = s.cross_thread_small_frees;
    out->small_frees_unrecorded = s.small_frees_unrecorded;
    out->live_requested_bytes = s.live_requested_bytes;
    out->live_requested_measured = s.live_requested_measured;
    return 0;
}

/*
 * Z-077 audit counters: the E-005 shape carried to the two
 * Wave 9 trim / reallocarray entry points -- each counts
 * CALLS at this L1 boundary, exactly once per invocation,
 * whatever the outcome. They are APPENDED after the Z-074
 * counters so every existing reader is undisturbed; like
 * them they are declared in no installed header, read only
 * by a test-only static build (the declarations are
 * appended to tests/abi/mimir_audit_e005.h), and bound
 * local in the shared library by the version script's
 * local: * -- the .so export count is exactly the eight
 * public entry points after Z-077, never these.
 */
_Alignas(64) unsigned long mimir_audit_call_trim;
_Alignas(64) unsigned long mimir_audit_call_reallocarray;

int mimir_trim(size_t pad)
{
    __atomic_fetch_add(&mimir_audit_call_trim, 1UL,
                       __ATOMIC_RELAXED);
    return mimir_core_trim(pad);
}

void *mimir_reallocarray(void *ptr, size_t nmemb, size_t size)
{
    __atomic_fetch_add(&mimir_audit_call_reallocarray, 1UL,
                       __ATOMIC_RELAXED);
    return mimir_core_reallocarray(ptr, nmemb, size);
}
