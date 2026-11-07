/* mimir_small_g003.h - TEST-ONLY declarations for slice G-003.
 *
 * This header lives under tests/core/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document: the public header include/mimir_alloc.h
 * declares exactly the four public entry points and is
 * unchanged by G-003. The four counters declared here are
 * the span/object audit counters of
 * docs/PHASE_7_SMALL_OBJECT_PATH_G003.md (G-002 R-5,
 * INC-G002-09): internal audit state in the INC-E001-07
 * shape (b), readable only by linking the library OBJECTS
 * directly, as `make test-g003` does. The shared library
 * binds them local (hidden visibility in the core object
 * plus the version script), so they never appear in
 * libmimiralloc.so's dynamic symbol table.
 *
 * Semantics (exact, per src/core/mimir_core.c):
 * - mimir_audit_span_published: spans acquired and
 *   published LIVE by the class path (span_acquire_locked).
 * - mimir_audit_span_retired: spans successfully released
 *   (span_release_locked). published - retired = live spans.
 * - mimir_audit_small_alloc: small objects handed out from
 *   spans (small_alloc_locked), covering small malloc,
 *   small calloc, and realloc constructions whose NEW size
 *   routes small.
 * - mimir_audit_small_free: small objects retired back to
 *   their spans (small_free_locked), covering free of a
 *   span object and realloc's transfer of an old span
 *   block. alloc - free = live small objects.
 *
 * The mapping counters of tests/abi/mimir_audit_e005.h
 * keep their banked meanings: a span-served allocation
 * moves NO mapping counter (R-5: statistics describe what
 * they name).
 *
 * Contract status: NOT part of the contract.
 * Proposed-internal audit state only; [UNRESOLVED: U-03]
 * applies as it does to the whole namespaced surface.
 */
#ifndef MIMIR_SMALL_G003_H
#define MIMIR_SMALL_G003_H

extern unsigned long mimir_audit_span_published;
extern unsigned long mimir_audit_span_retired;
extern unsigned long mimir_audit_small_alloc;
extern unsigned long mimir_audit_small_free;

#endif /* MIMIR_SMALL_G003_H */
