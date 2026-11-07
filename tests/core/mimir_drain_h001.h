/* mimir_drain_h001.h - TEST-ONLY declaration for slice H-001.
 *
 * This header lives under tests/core/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document: the public header include/mimir_alloc.h
 * declares exactly the four public entry points and is
 * unchanged by H-001. The function declared here is the
 * unload-drain entry point of
 * docs/PHASE_7_UNLOAD_DRAIN_H001.md: internal core state in
 * the INC-E001-07 shape (b), reachable only by linking the
 * library OBJECTS directly, as `make test-h001` does. The
 * shared library compiles the core object with hidden
 * visibility and the version script binds every non-public
 * symbol local, so the function never appears in
 * libmimiralloc.so's dynamic symbol table and no consumer
 * of the .so can link against it (proven in
 * tests/core/recorded_run_h001.txt by nm -D staying at
 * exactly 4).
 *
 * Semantics (exact, per src/core/mimir_core.c):
 * - mimir_core_drain_retained(): under the one global lock,
 *   release every retained EMPTY span (live_count == 0)
 *   whole through L6, and return how many spans were
 *   actually released. Spans holding live blocks and live
 *   large mappings are never touched (INC-G008-03 posture,
 *   H-001 slice doc Section 3). The drain is non-terminal
 *   and idempotent: registries, counters and the lock are
 *   left fully valid, the allocator stays usable, and a
 *   call with nothing retained returns 0. If the lock
 *   cannot be taken, nothing is released and 0 is
 *   returned. The image destructor registered in
 *   mimir_core.c calls this same entry point at dlclose,
 *   before the image unmaps, and at process exit.
 *
 * Contract status: NOT part of the contract.
 * Proposed-internal only; [UNRESOLVED: U-03] applies as it
 * does to the whole namespaced surface. Any promotion to
 * the public ABI (e.g. an explicit shutdown entry point)
 * requires a future contract slice (INC-G007-01 pattern).
 */
#ifndef MIMIR_DRAIN_H001_H
#define MIMIR_DRAIN_H001_H

#include <stddef.h>

size_t mimir_core_drain_retained(void);

#endif /* MIMIR_DRAIN_H001_H */
