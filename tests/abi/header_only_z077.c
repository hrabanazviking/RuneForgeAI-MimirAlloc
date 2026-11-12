/* header_only_z077.c - Z-077 header self-containment proof
 * (E-007 precedent): this translation unit includes ONLY
 * the public header and uses the two Wave 9 declarations'
 * types, proving include/mimir_alloc.h is self-contained
 * for C after the Z-077 additions. Compile-only; built by
 * `make test-z077`, never run. */
#include "mimir_alloc.h"

int (*const z077_ref_trim)(size_t) = &mimir_trim;
void *(*const z077_ref_reallocarray)(void *, size_t,
                                     size_t) =
    &mimir_reallocarray;
