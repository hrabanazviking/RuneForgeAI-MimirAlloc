/* header_only_z076.cpp - Z-076 header self-containment
 * proof for C++ (E-007 precedent): includes ONLY the
 * public header and uses the Z-076 declaration's types.
 * Compile-only; built by `make test-z076`, never run. */
#include "mimir_alloc.h"

int (*const z076_ref_stats)(mimir_stats_v1 *) =
    &mimir_stats_snapshot;
const uint32_t z076_ref_version = MIMIR_STATS_VERSION;
const size_t z076_ref_struct_size = sizeof(mimir_stats_v1);
