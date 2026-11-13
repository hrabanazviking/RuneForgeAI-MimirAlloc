/* header_only_z083.c - Z-083 header self-containment
 * proof (E-007 precedent): this translation unit
 * includes ONLY the public header and takes the three
 * memalign-family declarations' addresses with their
 * exact signatures, proving include/mimir_alloc.h is
 * self-contained for C after the Z-083 additions.
 * Compile-only; built by `make test-z083`, never run. */
#include "mimir_alloc.h"

void *(*const z083_ref_memalign)(size_t, size_t) =
    &mimir_memalign;
void *(*const z083_ref_valloc)(size_t) = &mimir_valloc;
void *(*const z083_ref_pvalloc)(size_t) = &mimir_pvalloc;
