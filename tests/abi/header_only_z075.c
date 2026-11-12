/* header_only_z075.c - Z-075 header self-containment proof
 * (E-007 precedent): this translation unit includes ONLY
 * the public header and uses the Z-075 declaration's type,
 * proving include/mimir_alloc.h is self-contained for C
 * after the Z-075 addition. Compile-only; built by
 * `make test-z075`, never run. */
#include "mimir_alloc.h"

size_t (*const z075_ref_usable)(const void *) =
    &mimir_usable_size;
