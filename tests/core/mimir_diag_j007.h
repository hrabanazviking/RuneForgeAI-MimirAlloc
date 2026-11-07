/* mimir_diag_j007.h - TEST-ONLY declarations for slice J-007.
 *
 * This header lives under tests/core/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document: the public header include/mimir_alloc.h
 * declares exactly the four public entry points and is
 * unchanged by J-007. The diagnostics channel declared here
 * is the Phase 10 error-reporting surface of
 * docs/PHASE_10_DIAGNOSTICS.md: an internal core entry point
 * in the INC-E001-07 shape (b), reachable only by linking
 * the library OBJECTS directly, as `make test-j007` does.
 * The shared library compiles the core object with hidden
 * visibility and the version script binds every non-public
 * symbol local, so the channel never appears in
 * libmimiralloc.so's dynamic symbol table and no consumer
 * of the .so can link against it (proven in
 * tests/core/recorded_run_j007.txt by nm -D staying at
 * exactly 4).
 *
 * The declarations themselves live in the internal core
 * header src/core/mimir_core.h (NOT installed); this header
 * includes it by relative path so there is exactly one
 * definition in the tree -- the J-002 pattern
 * (tests/core/mimir_stats_j002.h, INC-J002-03).
 *
 * Semantics (exact, per src/core/mimir_core.h): one fixed-
 * format line per genuine allocation failure, formatted
 * into a fixed stack buffer from fixed tables and written
 * with write(2) to fd 2; no allocation, no lock, no L6
 * call, no stdio on the path; nested attempts are counted
 * in the suppressed counter and dropped, so the maximum
 * report depth is 1. Posture NULLs emit nothing.
 *
 * Contract status: NOT part of the contract.
 * Proposed-internal only; [UNRESOLVED: U-03] applies as it
 * does to the whole namespaced surface.
 */
#ifndef MIMIR_DIAG_J007_H
#define MIMIR_DIAG_J007_H

#include "../../src/core/mimir_core.h"

#endif /* MIMIR_DIAG_J007_H */
