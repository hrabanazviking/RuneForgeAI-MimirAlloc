/* mimir_trim_i003.h - TEST-ONLY declarations for slice I-003.
 *
 * This header lives under tests/core/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document: the public header include/mimir_alloc.h
 * declares exactly the four public entry points and is
 * unchanged by I-003. The functions declared here are the
 * on-demand trim surface of
 * docs/PHASE_9_TRIM_IMPLEMENTATION.md (semantics fixed by
 * I-002 Section 5): internal core entry points in the
 * INC-E001-07 shape (b), reachable only by linking the
 * library OBJECTS directly, as `make test-i003` does. The
 * shared library compiles the core object with hidden
 * visibility and the version script binds every non-public
 * symbol local, so none of them appears in
 * libmimiralloc.so's dynamic symbol table and no consumer
 * of the .so can link against them (proven in
 * tests/core/recorded_run_i003.txt by nm -D staying at
 * exactly 4).
 *
 * Semantics (exact, per src/core/mimir_core.c):
 * - mimir_core_trim_retained(): under the one global lock,
 *   release every retained EMPTY span (live_count == 0)
 *   whole through L6, class-fair round-robin, and return
 *   how many spans were actually released. Bytes released
 *   are spans x page size exactly (every span is one page).
 * - mimir_core_trim_retained_some(max): the same walk
 *   stopped after at most max spans (I-002 S5.2.3
 *   stop-at-need mechanism; max == 0 releases nothing).
 * - Live spans, live small objects and live large mappings
 *   are never touched. Both trims are non-terminal and
 *   idempotent: nothing retained, or a lock failure,
 *   returns 0 with nothing released.
 * - mimir_core_retained_spans() / _retained_bytes():
 *   Q-RETAINED (I-002 S3.2) as one locked snapshot: the
 *   count of EMPTY spans and the sum of their
 *   mapping_lengths. mimir_core_mapped_bytes(): Q-MAPPED
 *   (live mapping bytes + live span bytes) as one locked
 *   snapshot. Each query returns (size_t)-1 only if the
 *   lock cannot be taken (G-007 getter posture).
 *
 * Contract status: NOT part of the contract.
 * Proposed-internal only; [UNRESOLVED: U-03] applies as it
 * does to the whole namespaced surface. Any promotion to
 * the public ABI (e.g. a public trim entry point) requires
 * a future contract slice; the I-003 slice doc records the
 * decision as internal-only.
 */
#ifndef MIMIR_TRIM_I003_H
#define MIMIR_TRIM_I003_H

#include <stddef.h>

size_t mimir_core_trim_retained(void);
size_t mimir_core_trim_retained_some(size_t max_spans);
size_t mimir_core_retained_spans(void);
size_t mimir_core_retained_bytes(void);
size_t mimir_core_mapped_bytes(void);

#endif /* MIMIR_TRIM_I003_H */
