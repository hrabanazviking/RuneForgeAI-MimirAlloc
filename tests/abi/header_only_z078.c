/* header_only_z078.c - Z-078 header self-containment
 * proof (E-007 precedent): this translation unit
 * includes ONLY the public header and uses the
 * allocation-family declarations' types, proving
 * include/mimir_alloc.h is self-contained for C after
 * the Z-078 contract text edits. Compile-only; built
 * by `make test-z078`, never run. */
#include "mimir_alloc.h"

void *(*const z078_ref_malloc)(size_t) = &mimir_malloc;
void *(*const z078_ref_calloc)(size_t, size_t) = &mimir_calloc;
void *(*const z078_ref_realloc)(void *, size_t) = &mimir_realloc;
void *(*const z078_ref_reallocarray)(void *, size_t,
                                     size_t) =
    &mimir_reallocarray;
void *(*const z078_ref_aligned_alloc)(size_t, size_t) =
    &mimir_aligned_alloc;
int (*const z078_ref_posix_memalign)(void **, size_t,
                                     size_t) =
    &mimir_posix_memalign;
