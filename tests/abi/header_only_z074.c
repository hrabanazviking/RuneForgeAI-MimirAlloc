/* header_only_z074.c - Z-074 header self-containment proof
 * (E-007 precedent): this translation unit includes ONLY
 * the public header and uses the two Wave 9 declarations'
 * types, proving include/mimir_alloc.h is self-contained
 * for C after the Z-074 additions. Compile-only; built by
 * `make test-z074`, never run. */
#include "mimir_alloc.h"

void *(*const z074_ref_aligned)(size_t, size_t) =
    &mimir_aligned_alloc;
int (*const z074_ref_posix)(void **, size_t, size_t) =
    &mimir_posix_memalign;
