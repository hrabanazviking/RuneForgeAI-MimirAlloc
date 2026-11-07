/* mimir_stats_j002.h - TEST-ONLY declarations for slice J-002.
 *
 * This header lives under tests/core/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document: the public header include/mimir_alloc.h
 * declares exactly the four public entry points and is
 * unchanged by J-002. The snapshot entry point and the
 * struct mimir_stats layout are the Phase 10 statistics
 * surface of docs/PHASE_10_STATISTICS.md: an internal core
 * entry point in the INC-E001-07 shape (b), reachable only
 * by linking the library OBJECTS directly, as
 * `make test-j002` does. The shared library compiles the
 * core object with hidden visibility and the version script
 * binds every non-public symbol local, so the entry point
 * never appears in libmimiralloc.so's dynamic symbol table
 * and no consumer of the .so can link against it (proven in
 * tests/core/recorded_run_j002.txt by nm -D staying at
 * exactly 4).
 *
 * The struct definition itself lives in the internal core
 * header src/core/mimir_core.h (NOT installed); this header
 * includes it by relative path so there is exactly one
 * definition of the layout in the tree -- the I-003 pattern
 * of re-declaring the entry points is extended to the
 * struct by inclusion rather than by a second, drifting
 * copy (recorded as INC-J002-03).
 *
 * Semantics (exact, per src/core/mimir_core.h and
 * docs/PHASE_10_STATISTICS.md S2): one locked snapshot;
 * bytes_requested / bytes_allocated are cumulative granted
 * totals at successful new-block publications;
 * bytes_cached is Q-RETAINED and bytes_mapped is Q-MAPPED
 * at the snapshot instant; bytes_released is cumulative
 * Q-RELEASED; peak_mapped is Q-PEAKMAPPED;
 * alloc_failures is Q-FAILED-OS and alloc_posture_nulls
 * counts the disjoint U-09 / U-12 posture NULLs;
 * cross_thread_frees is PARTIAL (mapping path only,
 * INC-J002-01); lock_contention is ABSENT
 * (lock_contention_measured == 0, INC-J002-02).
 *
 * Contract status: NOT part of the contract.
 * Proposed-internal only; [UNRESOLVED: U-03] applies as it
 * does to the whole namespaced surface.
 */
#ifndef MIMIR_STATS_J002_H
#define MIMIR_STATS_J002_H

#include "../../src/core/mimir_core.h"

#endif /* MIMIR_STATS_J002_H */
