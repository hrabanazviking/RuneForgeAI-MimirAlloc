/* mimir_tune_g007.h - TEST-ONLY declarations for slice G-007.
 *
 * This header lives under tests/core/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document: the public header include/mimir_alloc.h
 * declares exactly the four public entry points and is
 * unchanged by G-007. The two functions declared here are
 * the tuning surface of docs/PHASE_7_TUNING_PARAMETERS_G007.md:
 * internal tuning state in the INC-E001-07 shape (b),
 * reachable only by linking the library OBJECTS directly,
 * as `make test-g007` does. The shared library compiles the
 * core object with hidden visibility and the version script
 * binds every non-public symbol local, so neither function
 * appears in libmimiralloc.so's dynamic symbol table and no
 * consumer of the .so can link against them (proven in
 * tests/core/recorded_run_g007.txt by nm -D staying at
 * exactly 4).
 *
 * Semantics (exact, per src/core/mimir_core.c):
 * - mimir_tune_set_retention_bound(v): set the per-class
 *   empty-span retention bound (G-005 RP-2). Valid range is
 *   0..MIMIR_TUNE_RETENTION_MAX (16). Returns 0 on success.
 *   Returns -1 when v is out of range: the call is refused
 *   BEFORE any state is touched and the previous effective
 *   value is kept exactly (validation, no partial
 *   application). Returns -2 when the allocator lock cannot
 *   be taken: again nothing changes. On success the new
 *   value is stored in the same locked discipline that
 *   guards the span registry, so every later free-time
 *   retention decision reads exactly this value. The bound
 *   applies at free time (G-005 RP-3): lowering it does NOT
 *   eagerly release spans already retained; each class
 *   converges to the new bound as its spans next empty.
 * - mimir_tune_get_retention_bound(): the effective bound,
 *   read back exactly under the same lock. Returns
 *   (size_t)-1 only if the lock cannot be taken.
 * - With no setter call at all, the effective value is the
 *   banked default 2, so unconfigured behaviour is exactly
 *   the G-003/G-005 behaviour.
 *
 * Contract status: NOT part of the contract.
 * Proposed-internal tuning state only; [UNRESOLVED: U-03]
 * applies as it does to the whole namespaced surface. Any
 * promotion to the public ABI requires a future contract
 * slice (INC-G007-01).
 */
#ifndef MIMIR_TUNE_G007_H
#define MIMIR_TUNE_G007_H

#include <stddef.h>

#define MIMIR_TUNE_RETENTION_DEFAULT ((size_t)2)
#define MIMIR_TUNE_RETENTION_MAX ((size_t)16)

int mimir_tune_set_retention_bound(size_t value);
size_t mimir_tune_get_retention_bound(void);

#endif /* MIMIR_TUNE_G007_H */
