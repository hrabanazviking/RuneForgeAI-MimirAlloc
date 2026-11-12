/* mimir_arena_z037.h - TEST-ONLY declarations for Z-037.
 *
 * Slice Z-037 (docs/MIMIRALLOC_Z13_ROADMAP.md Wave 5): the
 * per-CCD arena prototype surface that exists only in a
 * MIMIR_ARENA_CCD build of the core. This header lives
 * under tests/core/ on purpose. It is NOT installed, NOT
 * part of the public surface (include/mimir_alloc.h still
 * declares exactly the four public entry points), and NOT
 * an ABI: it follows the J-002 pattern of including the
 * internal core header by relative path, so the snapshot
 * struct has exactly one definition in the tree. The
 * shared library binds these symbols local, so nm -D on
 * libmimiralloc.so stays exactly 4.
 */
#ifndef MIMIR_ARENA_Z037_H
#define MIMIR_ARENA_Z037_H

#ifndef MIMIR_ARENA_CCD
#define MIMIR_ARENA_CCD 1
#endif
#include "../../src/core/mimir_core.h"

#endif /* MIMIR_ARENA_Z037_H */
