/* mimir_interpose.c - MimirAlloc Z-082: the interposing
 * variant's standard-name forwarders (Wave 10, L-01's
 * opt-in route).
 *
 * This translation unit is built ONLY into the opt-in
 * interposing library, build/libmimiralloc_interpose.so
 * (Makefile target lib-interpose, version script
 * src/api/mimir_interpose.map). It is NEVER linked into
 * the default libmimiralloc.so, the static archive, or
 * any LIBOBJS consumer: the default library keeps exactly
 * its ten namespaced mimir_* exports and interposes
 * nothing (limitation L-01 stands for the default build;
 * decision D-015 in docs/DECISION_LEDGER.md; the slice
 * record is docs/Z13_Z082_INTERPOSING_VARIANT.md).
 *
 * Each function below defines ONE standard malloc-family
 * name and forwards it, unchanged in signature, to the
 * namespaced L1 entry point of the matching shape in
 * src/api/mimir_alloc.c. The forwarder adds no logic of
 * its own -- no size arithmetic, no errno handling, no
 * fallback -- so the interposed name inherits the
 * namespaced contract EXACTLY, including where that
 * contract deliberately differs from glibc (decision
 * D-013, settled at Z-078):
 * - malloc(0) returns NULL (glibc: a distinct freeable
 *   pointer per call).
 * - realloc(ptr, 0) on a live block returns NULL and
 *   leaves the block LIVE, unchanged and caller-owned;
 *   it is NOT freed (glibc frees it). A caller who relies
 *   on the glibc reading leaks the block under this
 *   variant; it is never corrupted or double-owned.
 * - calloc with a zero factor returns NULL.
 * - posix_memalign with a valid alignment and size 0
 *   returns ENOMEM and leaves *memptr unchanged (glibc
 *   returns 0 with *memptr set).
 * - No entry point sets errno as a contract channel;
 *   checked refusals leave errno exactly as on entry
 *   (glibc sets ENOMEM on allocation failure).
 * - aligned_alloc does NOT require size to be a multiple
 *   of alignment (the Z-073/Z-074 recorded deviation from
 *   the C11 shape, in the glibc posture).
 * These divergences are the price of one uniform contract
 * across both surfaces, they are pinned by
 * tests/abi/test_z082.c, and the Wave 10 verdict slice
 * (Z-090) owns restating them for preload users.
 *
 * The interposed set is exactly the standard names with a
 * direct namespaced counterpart at this slice:
 *   malloc, free, cfree (glibc's legacy alias of free,
 *   forwarded identically), calloc, realloc, reallocarray,
 *   aligned_alloc, posix_memalign, malloc_usable_size
 *   (the standard name decision D-012 expressly reserved
 *   to this opt-in variant), malloc_trim (over
 *   mimir_trim, whose 1/0 return has the same shape).
 * Every other glibc malloc-family symbol on this host is
 * ABSENT from this library and named, with its reason, in
 * the audit filed at docs/Z13_Z082_INTERPOSING_VARIANT.md
 * S4 and tests/abi/recorded_run_z13_z082.txt S4 -- the
 * memalign/valloc/pvalloc family is the roadmap's Z-083
 * assignment, never a silent gap here.
 *
 * Recursion safety: the core and platform layers call no
 * public malloc-family name (verified by inspection at
 * this slice: src/core and src/platform name only the L6
 * OS wrappers -- mmap/munmap and kin -- and the C11
 * thread/TSS machinery), so an interposed call can never
 * re-enter this TU through the allocator's own work. The
 * TU is compiled -fno-builtin (Makefile Z-082 block) so
 * the compiler never rewrites these definitions or their
 * callers' calls using builtin malloc-family assumptions.
 */
#include "mimir_alloc.h"

#include <stddef.h>

/* Standard-name declarations, stated locally so this TU
 * does not depend on which feature-test macros a consumer
 * of <stdlib.h> / <malloc.h> would need for the legacy
 * names (cfree). Signatures are the C / POSIX / glibc
 * shapes, identical to the system headers on this host. */
void *malloc(size_t size);
void free(void *ptr);
void cfree(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *reallocarray(void *ptr, size_t nmemb, size_t size);
void *aligned_alloc(size_t alignment, size_t size);
int posix_memalign(void **memptr, size_t alignment,
                   size_t size);
size_t malloc_usable_size(void *ptr);
int malloc_trim(size_t pad);

void *malloc(size_t size)
{
    return mimir_malloc(size);
}

void free(void *ptr)
{
    mimir_free(ptr);
}

/* cfree is glibc's legacy (pre-standard) name for free --
 * the same address as free in this host's libc.so.6
 * (recorded run S4). It is forwarded identically: the
 * alias carries no distinct semantics to honour. */
void cfree(void *ptr)
{
    mimir_free(ptr);
}

void *calloc(size_t nmemb, size_t size)
{
    return mimir_calloc(nmemb, size);
}

void *realloc(void *ptr, size_t size)
{
    return mimir_realloc(ptr, size);
}

void *reallocarray(void *ptr, size_t nmemb, size_t size)
{
    return mimir_reallocarray(ptr, nmemb, size);
}

void *aligned_alloc(size_t alignment, size_t size)
{
    return mimir_aligned_alloc(alignment, size);
}

int posix_memalign(void **memptr, size_t alignment,
                   size_t size)
{
    return mimir_posix_memalign(memptr, alignment, size);
}

size_t malloc_usable_size(void *ptr)
{
    return mimir_usable_size(ptr);
}

int malloc_trim(size_t pad)
{
    return mimir_trim(pad);
}
