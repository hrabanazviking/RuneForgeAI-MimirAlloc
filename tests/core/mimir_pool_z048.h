/* mimir_pool_z048.h - TEST-ONLY declarations for slice Z-048.
 *
 * This header lives under tests/core/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document: the public header include/mimir_alloc.h
 * declares exactly the four public entry points and is
 * unchanged by Z-048. The functions declared here are the
 * mapping-cache query pair of the Z-048 implementation:
 * internal core entry points in the INC-E001-07 shape (b),
 * reachable only by linking the library OBJECTS directly,
 * as `make test-z048` does. The shared library compiles the
 * core object with hidden visibility and the version script
 * binds every non-public symbol local, so neither appears
 * in libmimiralloc.so's dynamic symbol table (nm -D stays
 * exactly 4; proven in tests/perf/
 * recorded_run_z13_z048.txt).
 *
 * Semantics (exact, per src/core/mimir_core.c):
 * - mimir_core_pool_count(): mappings currently held in
 *   the Z-048 mapping cache (dead large mappings retained
 *   for exact-length reuse instead of being unmapped at
 *   free). Bounded by MIMIR_POOL_MAX_COUNT (64).
 * - mimir_core_pool_bytes(): their summed mapping_length.
 *   Bounded by MIMIR_POOL_MAX_BYTES (32 MiB). These bytes
 *   are inside Q-MAPPED (mimir_core_mapped_bytes) but are
 *   NOT live bytes and NOT Q-RETAINED (span-only).
 * - Each query is one locked snapshot and returns
 *   (size_t)-1 only if the lock cannot be taken (G-007
 *   getter posture).
 *
 * Contract status: NOT part of the contract.
 * Proposed-internal only.
 */
#ifndef MIMIR_POOL_Z048_H
#define MIMIR_POOL_Z048_H

#include <stddef.h>

size_t mimir_core_pool_count(void);
size_t mimir_core_pool_bytes(void);

#endif /* MIMIR_POOL_Z048_H */
