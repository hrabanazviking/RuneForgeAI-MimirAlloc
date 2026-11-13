/* mimir_batch_z092.h - TEST-ONLY declarations for Z-092.
 *
 * Slice Z-092 (docs/Z13_Z092_BATCH_API_PROTOTYPE.md): the
 * contract surface of the batch API prototype - bulk
 * same-class alloc/free over the Wave-4 TLS cache
 * (Z-028/Z-029), shaped by Z-091's post-mortem verdict
 * (docs/Z13_Z091_BATCHING_POSTMORTEM.md).
 *
 * This header lives under tests/contract/ on purpose. It
 * is NOT installed, NOT part of the public surface
 * (include/mimir_alloc.h declares exactly the 13 public
 * entry points; the roadmap gate for Z-092 is explicit:
 * "not yet a public symbol"), and NOT an ABI: it is the
 * INC-E001-07 shape (b) test-only redeclaration pattern
 * used by every internal surface in this tree. The core
 * TU (src/core/mimir_core.c) defines these symbols; the
 * shared library binds them local, so nm -D on
 * libmimiralloc.so stays exactly 13 (proven by the
 * runner, tests/contract/run_z092.sh).
 *
 * Contract constants (fixed by Z-092):
 * - MIMIR_BATCH_MAX_COUNT 4096: a call naming more is
 *   refused whole, before any allocation or free.
 * - Same-class only: one size per alloc call, one array
 *   per free call. Mixed classes are the caller's loop.
 */
#ifndef MIMIR_BATCH_Z092_H
#define MIMIR_BATCH_Z092_H

#include <stddef.h>
#include <stdint.h>

#define MIMIR_BATCH_MAX_COUNT ((size_t)4096)

/* Process-global monotone observability snapshot for the
 * prototype (contract-suite use only; not a public
 * statistics promise). All fields monotone except none;
 * max_count is the constant MIMIR_BATCH_MAX_COUNT. */
struct mimir_batch_snapshot {
    uint64_t alloc_calls;
    uint64_t free_calls;
    uint64_t blocks_allocated;
    uint64_t blocks_freed;
    uint64_t failures;
    uint64_t max_count;
};

/* 0 on success, -1 on NULL out. Taking a snapshot must
 * not allocate and must not move any counter. */
int mimir_batch_snapshot(struct mimir_batch_snapshot *out);

/* Allocate count blocks of size bytes each, all-or-
 * nothing: on success returns count and out[0..count)
 * holds distinct, 16-byte-aligned, writable blocks.
 * On any failure returns 0, allocates nothing net, and
 * leaves out[] byte-identical to entry. count == 0 is a
 * defined no-op returning 0 (not a failure). Refusals
 * (failures counter +1, return 0): out == NULL,
 * count > MIMIR_BATCH_MAX_COUNT, size == 0 (the D-013
 * refusal, exactly the mimir_core_alloc(0) answer, per
 * the batch), or an element allocation failing (every
 * block this call obtained is freed in reverse order
 * before returning). errno is not promised (D-013). */
size_t mimir_batch_alloc(size_t size, void **out, size_t count);

/* Free the non-NULL entries of ptrs[0..count) in REVERSE
 * (LIFO) order and return how many were actually freed;
 * NULL entries are skipped and not counted. count == 0
 * is a defined no-op returning 0 (not a failure).
 * Refusals (failures counter +1, return 0, nothing
 * freed): ptrs == NULL or count > MIMIR_BATCH_MAX_COUNT.
 * A duplicate pointer inside one batch is a double
 * free: undefined behaviour, exactly as two
 * mimir_core_free calls would be. The array itself is
 * never written. */
size_t mimir_batch_free(void **ptrs, size_t count);

#endif /* MIMIR_BATCH_Z092_H */
