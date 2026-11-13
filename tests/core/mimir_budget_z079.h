/* mimir_budget_z079.h - TEST-ONLY declarations for slice Z-079.
 *
 * This header lives under tests/core/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document: the public header include/mimir_alloc.h
 * is unchanged by Z-079. The functions declared here are
 * the Phase-9 soft-budget surface of
 * docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md S3.6
 * (adopted by decision D-014, docs/Z13_BUDGET_Z079.md):
 * internal core entry points in the INC-E001-07 shape (b),
 * reachable only by linking the library OBJECTS directly,
 * as `make test-z079` does. The shared library compiles
 * the core object with hidden visibility and the version
 * script binds every non-public symbol local, so none of
 * them appears in libmimiralloc.so's dynamic symbol table
 * and no consumer of the .so can link against them.
 *
 * Semantics (exact, per src/core/mimir_core.c):
 * - mimir_budget_set_mapped_bytes(v): store the budget
 *   over Q-MAPPED (live mapping bytes + live span bytes +
 *   pooled mapping-cache bytes) under the global lock.
 *   0 unsets the budget (the default; I-002 S3.2
 *   sentinel). Setting performs NO release: a budget set
 *   below current Q-MAPPED binds only future
 *   acquisitions. Returns 0 on success, -2 on lock
 *   failure with nothing changed.
 * - mimir_budget_get_mapped_bytes(): the effective
 *   budget read back under the same lock; (size_t)-1
 *   only if the lock cannot be taken.
 * - mimir_core_budget_failures(): Q-REFUSED-BUDGET, the
 *   monotone count of entry-point calls refused by the
 *   budget, read lock-free (the Z-064 getter pattern).
 *   Disjoint from alloc_failures (Q-FAILED-OS) and from
 *   alloc_posture_nulls in the J-002 snapshot, which also
 *   carries the pair as budget_failures /
 *   budget_mapped_bytes.
 *
 * Contract status: NOT part of the contract.
 * Proposed-internal only; [UNRESOLVED: U-03] applies as
 * it does to the whole namespaced surface. Any promotion
 * to the public ABI requires a future contract slice
 * (INC-I002-05 posture).
 */
#ifndef MIMIR_BUDGET_Z079_H
#define MIMIR_BUDGET_Z079_H

#include <stddef.h>
#include <stdint.h>

int mimir_budget_set_mapped_bytes(size_t value);
size_t mimir_budget_get_mapped_bytes(void);
uint64_t mimir_core_budget_failures(void);

#endif /* MIMIR_BUDGET_Z079_H */
