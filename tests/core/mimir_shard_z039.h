/* mimir_shard_z039.h - TEST-ONLY declarations for Z-039.
 *
 * Slice Z-039 follow-up (docs/MIMIRALLOC_Z13_ROADMAP.md
 * Wave 5): the lock-sharding (geometry B) prototype surface
 * that exists only in a MIMIR_LOCK_SHARD build of the core. This header lives
 * under tests/core/ on purpose. It is NOT installed, NOT
 * part of the public surface (include/mimir_alloc.h still
 * declares exactly the four public entry points), and NOT
 * an ABI: it follows the J-002 pattern of including the
 * internal core header by relative path, so the snapshot
 * struct has exactly one definition in the tree. The
 * shared library binds these symbols local, so nm -D on
 * libmimiralloc.so stays exactly 4.
 */
#ifndef MIMIR_SHARD_Z039_H
#define MIMIR_SHARD_Z039_H

#ifndef MIMIR_LOCK_SHARD
#define MIMIR_LOCK_SHARD 1
#endif
#include "../../src/core/mimir_core.h"

#endif /* MIMIR_SHARD_Z039_H */
