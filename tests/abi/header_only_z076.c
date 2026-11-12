/* header_only_z076.c - Z-076 header self-containment proof
 * (E-007 precedent): this translation unit includes ONLY
 * the public header and uses the Z-076 declaration's
 * types, proving include/mimir_alloc.h is self-contained
 * for C after the Z-076 additions. Compile-only; built by
 * `make test-z076`, never run. */
#include "mimir_alloc.h"

int (*const z076_ref_stats)(struct mimir_stats_v1 *) =
    &mimir_stats_snapshot;
const uint32_t z076_ref_version = MIMIR_STATS_VERSION;
const size_t z076_ref_struct_size = sizeof(struct mimir_stats_v1);
