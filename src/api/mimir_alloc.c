/* mimir_alloc.c - MimirAlloc L1 public boundary (slice C-003;
 * calloc/realloc forwarders added by slice C-005).
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
 * - mimir_malloc(0): returns NULL in this implementation.
 *   Zero-size behaviour is [UNRESOLVED: U-09]: this is an
 *   implementation posture, continuous with the C-001 stub,
 *   NOT a contract claim, and no caller or test may cite it
 *   as settling U-09 (B-006 F-05; B-007 A1/A4).
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
 *   A zero factor returns NULL as an implementation posture
 *   only: the case is [UNRESOLVED: U-09] (MA-6.1-CALLOC-04),
 *   NOT a contract claim (tests/core/NOTES_C005.md,
 *   INC-C005-01).
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
 *   original block left LIVE and unchanged, as an
 *   implementation posture only: the case is [UNRESOLVED:
 *   U-12] (MA-6.1-REALLOC-04), outside the supported list,
 *   NOT a contract claim (INC-C005-01). An invalid ptr is
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

void *mimir_malloc(size_t size)
{
    return mimir_core_alloc(size);
}

void mimir_free(void *ptr)
{
    mimir_core_free(ptr);
}

void *mimir_calloc(size_t nmemb, size_t size)
{
    return mimir_core_calloc(nmemb, size);
}

void *mimir_realloc(void *ptr, size_t size)
{
    return mimir_core_realloc(ptr, size);
}
