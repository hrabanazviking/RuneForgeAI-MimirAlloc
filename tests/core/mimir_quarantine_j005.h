/* mimir_quarantine_j005.h - TEST-ONLY declarations for J-005.
 *
 * This header lives under tests/core/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document: the public header include/mimir_alloc.h
 * declares exactly the four public entry points and is
 * unchanged by J-005. The three entry points declared here
 * are the Phase 10 debug quarantine surface of
 * docs/PHASE_10_POISON_QUARANTINE.md: internal core entry
 * points in the INC-E001-07 shape (b), reachable only by
 * linking the DEBUG library objects directly, as
 * `make test-j005` does (build-debug/, -DMIMIR_DEBUG=1).
 * They do not exist in a production build at all. The
 * shared library compiles the core object with hidden
 * visibility and the version script binds every non-public
 * symbol local, so they never appear in libmimiralloc.so's
 * dynamic symbol table in either flavour (nm -D stays
 * exactly 4; proven in tests/core/recorded_run_j005.txt).
 *
 * Semantics (exact, per src/core/mimir_core.h):
 * - mimir_debug_quarantine_count: blocks currently held in
 *   the bounded quarantine (capacity exactly 32).
 * - mimir_debug_quarantined_bytes: bytes_quarantined, the
 *   summed class extents of held blocks -- the S5.3
 *   own-named quantity, disjoint from D-CACHED.
 * - mimir_debug_quarantine_verify: number of held blocks
 *   whose bytes no longer match the poison pattern 0xDD
 *   (0 = all clean), -1 on lock failure; never aborts.
 * Getter posture on lock failure is (size_t)-1 for the two
 * size_t forms (G-007 posture).
 *
 * Contract status: NOT part of the contract.
 * Proposed-internal only; [UNRESOLVED: U-03] applies as it
 * does to the whole namespaced surface.
 */
#ifndef MIMIR_QUARANTINE_J005_H
#define MIMIR_QUARANTINE_J005_H

#include <stddef.h>

size_t mimir_debug_quarantine_count(void);
size_t mimir_debug_quarantined_bytes(void);
int mimir_debug_quarantine_verify(void);

#endif /* MIMIR_QUARANTINE_J005_H */
