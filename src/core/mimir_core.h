/*
 * mimir_core.h -- MimirAlloc core (L2/L4/L5 collapsed form), internal.
 *
 * Slice C-003. Internal header: NOT installed, NOT part of the
 * public surface in include/mimir_alloc.h, and no ABI of any
 * kind is claimed for it (the namespaced pair itself is
 * [UNRESOLVED: U-03]; see the public header).
 *
 * The L1 translation unit (src/api/mimir_alloc.c) is a thin
 * boundary that forwards to these two functions. All allocator
 * logic lives in src/core/mimir_core.c; all OS interaction is
 * consumed through the L6 platform layer's published interface
 * (src/platform/mimir_platform.h) -- no layer here names an OS
 * facility (B-006 S4 dependency law, S9.1).
 */
#ifndef MIMIR_CORE_H
#define MIMIR_CORE_H

#include <stddef.h>

/* Allocate size bytes (size > 0 admitted) or return NULL.
 * Zero-size: returns NULL as an implementation posture only;
 * U-09 remains UNRESOLVED (B-006 F-05, B-007 A1/A4). */
void *mimir_core_alloc(size_t size);

/* Free a live pointer from mimir_core_alloc exactly once;
 * NULL is a defined no-op. Invalid pointers are undefined
 * behaviour with no defined error (B-006 S8.3). */
void mimir_core_free(void *ptr);

/* Calloc (C-005, B-008 S3 by analogy under U-03): checked
 * nmemb * size product (overflow refused before the
 * multiplication is relied on, no wrapped allocation),
 * usable extent explicitly zeroed before publication.
 * Zero factor: returns NULL as an implementation posture
 * only; U-09 remains UNRESOLVED (MA-6.1-CALLOC-04). */
void *mimir_core_calloc(size_t nmemb, size_t size);

/* Realloc (C-005, B-008 S4 by analogy under U-03): NULL ptr
 * behaves as mimir_core_alloc(new_size); grow moves and
 * preserves min(old, new); shrink retains the mapping and
 * updates only the requested-size record; any failure
 * returns NULL with the original block LIVE, unchanged and
 * caller-owned. new_size == 0 with a live ptr: returns NULL
 * with the original preserved, as an implementation posture
 * only; U-12 remains UNRESOLVED (MA-6.1-REALLOC-04).
 * Invalid pointers are undefined behaviour with no defined
 * error (MA-6.1-REALLOC-05). */
void *mimir_core_realloc(void *ptr, size_t new_size);

/* Unload drain (H-001, docs/PHASE_7_UNLOAD_DRAIN_H001.md):
 * under the one global lock, release every retained EMPTY
 * span (live_count == 0) whole through L6 and return how
 * many spans were actually released. Spans holding live
 * blocks and live large mappings are never touched
 * (INC-G008-03 posture). Non-terminal and idempotent:
 * registries, counters and the lock are left fully valid
 * and the allocator stays usable; a call with nothing
 * retained returns 0, as does a call whose lock cannot be
 * taken (nothing is released then). The image destructor
 * registered in mimir_core.c calls this same entry point
 * at dlclose and at process exit. Internal only: NOT
 * part of the public surface in include/mimir_alloc.h,
 * bound local in the shared library (nm -D stays exactly
 * 4); the test-only redeclaration is
 * tests/core/mimir_drain_h001.h (INC-E001-07 shape (b)). */
size_t mimir_core_drain_retained(void);

#endif /* MIMIR_CORE_H */
