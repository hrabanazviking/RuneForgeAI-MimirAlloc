/* header_only_z081.c - Z-081 header self-containment
 * proof (E-007 precedent): this translation unit
 * includes ONLY the public header and takes the
 * addresses of ALL TEN public entry points with their
 * exact prototypes, proving include/mimir_alloc.h is
 * self-contained for C and that the audited surface is
 * exactly the declared surface. Compile-only; built by
 * `make test-z081`, never run. */
#include "mimir_alloc.h"

void *(*const z081_ref_malloc)(size_t) = &mimir_malloc;
void (*const z081_ref_free)(void *) = &mimir_free;
void *(*const z081_ref_calloc)(size_t, size_t) = &mimir_calloc;
void *(*const z081_ref_realloc)(void *, size_t) = &mimir_realloc;
void *(*const z081_ref_reallocarray)(void *, size_t,
                                     size_t) =
    &mimir_reallocarray;
int (*const z081_ref_trim)(size_t) = &mimir_trim;
void *(*const z081_ref_aligned_alloc)(size_t, size_t) =
    &mimir_aligned_alloc;
int (*const z081_ref_posix_memalign)(void **, size_t,
                                     size_t) =
    &mimir_posix_memalign;
size_t (*const z081_ref_usable_size)(const void *) =
    &mimir_usable_size;
int (*const z081_ref_stats_snapshot)(struct mimir_stats_v1 *) =
    &mimir_stats_snapshot;
