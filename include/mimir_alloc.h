/* mimir_alloc.h - MimirAlloc public header (Phase 3 foundation).
 *
 * Slice: C-001. Status: Proposed. This header declares the
 * Phase 3 namespaced surface only; it is not a stable ABI.
 *
 * Scope: ONLY the namespaced pair scoped by
 * docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md (B-006):
 * mimir_malloc and mimir_free. Nothing else is declared:
 * no standard-name malloc/free replacement, no
 * mimir_calloc / mimir_realloc in this header (those design
 * labels are B-008 scope and are not declared here), no
 * explicit-alignment entry point, no usable-size, statistics,
 * or trim entry point. Absent symbols are the documented
 * unsupported behaviour for those facilities (B-008 S6.1:
 * not present - absence at link time, never a runtime
 * refusal and never a silent fallback to another allocator).
 *
 * U-03 caveat (read first): the namespaced mimir_* API has
 * NO direct contract rows in docs/contracts.md; it is
 * [UNRESOLVED: U-03], outside contracts S6 scope, Appendix A
 * stability Unassigned. Every contract mapping below is BY
 * ANALOGY ONLY to the core-symbol rows, Proposed only -
 * never a direct namespaced contract and never an ABI,
 * signature-stability, or symbol-version promise (B-008 N-10).
 *
 * Language: portable C11 core, C ABI (Decision D-006 in
 * docs/DECISION_LEDGER.md).
 *
 * Defined cases for the declared pair (by analogy):
 * - mimir_malloc(size) with size > 0 and representable:
 *   success is a non-null pointer to distinct storage,
 *   suitably aligned for any object type with a fundamental
 *   alignment requirement (MA-6.1-MALLOC-01 by analogy);
 *   failure - including an oversize/unrepresentable request
 *   (MA-6.1-MALLOC-04 by analogy) or an unsatisfiable
 *   request - is exactly a null return, never a pointer to
 *   storage not allocated to the caller, never a partial or
 *   smaller block (MA-6.1-MALLOC-02, MA-6.2-020, MA-6.2-024
 *   by analogy; B-007 S D.0).
 * - mimir_free(ptr) of a live pointer returned by this
 *   allocator family retires that block exactly once
 *   (MA-6.1-FREE-01, MA-6.2-001 by analogy).
 *   mimir_free(NULL) is a defined no-op that returns
 *   normally (MA-6.1-FREE-02, MA-6.2-010 by analogy).
 *   mimir_free returns no value on any defined path.
 *
 * Unresolved cases - NO behaviour is promised for these,
 * and no observed stub behaviour settles them:
 * - U-09: zero-size requests (mimir_malloc(0)). No outcome
 *   is stated; callers must not rely on any.
 * - U-10 / V-02: errno on failure is NOT promised. The only
 *   caller-observable failure channel of mimir_malloc is
 *   the null return; no test or caller may rely on errno
 *   from either declared function (B-007 S D.1.1).
 * - U-15 / V-07: no numeric default alignment value is
 *   claimed. Suitably aligned above is the non-numeric
 *   claim only (B-007 S B.1).
 * - U-11 / V-03: explicit-alignment validity is open and
 *   no explicit-alignment entry point exists here (above).
 *
 * Other boundaries (by analogy / design record):
 * - Contents of a successful allocation are unspecified -
 *   NOT zero-guaranteed (B-007 S C1).
 * - Freeing a foreign, interior, or already-freed pointer,
 *   and use after free, are undefined behaviour with no
 *   defined error, return, or recovery (MA-6.1-FREE-03,
 *   MA-6.1-FREE-04, MA-6.2-030..032 by analogy; B-007 S D.3).
 * - No async-signal-safety is stated or implied for either
 *   function (MA-6.3-030..032; B-008 S7.1 item 3).
 * - No fork-safety claim is made (U-17..U-21 remain open).
 */
#ifndef MIMIR_ALLOC_H
#define MIMIR_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* By analogy to MA-6.1-MALLOC-01/-02/-04. See header notes:
 * failure is a null return only; errno is not promised
 * (U-10); zero-size is unresolved (U-09); no numeric
 * alignment value is claimed (U-15). */
void *mimir_malloc(size_t size);

/* By analogy to MA-6.1-FREE-01/-02. mimir_free(NULL) is a
 * defined no-op. Invalid pointers are undefined behaviour
 * with no defined error (see header notes). */
void mimir_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* MIMIR_ALLOC_H */
