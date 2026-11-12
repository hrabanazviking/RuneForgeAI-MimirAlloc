// header_only_z078.cpp - Z-078 header self-containment
// proof from C++ (E-007 precedent): includes ONLY the
// public header and takes the allocation-family
// functions' addresses with their exact C signatures,
// proving the declarations sit inside the header's
// extern "C" guards after the Z-078 contract text
// edits. Compile-only; built by `make test-z078`,
// never run.
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
