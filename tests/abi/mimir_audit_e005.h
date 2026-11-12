/* mimir_audit_e005.h - TEST-ONLY declarations for slice E-005.
 *
 * This header lives under tests/abi/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document: the public header include/mimir_alloc.h
 * declares exactly the four public entry points and is
 * unchanged by E-005. The six counters declared here are the
 * internal audit counters of docs/PHASE_5_AUDIT_HOOK_E005.md
 * (INC-E001-07 shape (b)): four L1 call counters in
 * src/api/mimir_alloc.c and two core mapping counters in
 * src/core/mimir_core.c.
 *
 * A test binary can read them ONLY by linking the library
 * OBJECTS (or the static archive built from them) directly,
 * as `make test-e005` does. The shared library binds every
 * one of them local (hidden visibility in the core object,
 * and the E-002 version script's local: * in the link), so
 * they are absent from libmimiralloc.so's dynamic symbol
 * table and a consumer of the .so cannot link against them
 * at all - the recorded run proves both directions.
 *
 * Semantics (exact, per the defining slices' code):
 * - mimir_audit_call_malloc / _calloc / _realloc / _free:
 *   invocations of the corresponding public entry point,
 *   counted at the L1 boundary, whatever the outcome
 *   (success, defined failure such as an oversize malloc,
 *   or an unresolved-case posture such as free(NULL)'s
 *   counterpart calls). A call that never reaches the
 *   MimirAlloc entry points - e.g. a libc malloc in the
 *   same process - cannot move them.
 * - mimir_audit_alloc_published: mappings published LIVE
 *   by the core (alloc_locked), covering malloc, calloc
 *   and realloc grow-by-move constructions.
 * - mimir_audit_mapping_retired: mappings successfully
 *   released by the core (retire_locked), covering free
 *   of a live block and realloc's transfer of the old
 *   block. free(NULL) and failed allocations move neither
 *   core counter. published - retired = live mappings.
 *
 * Contract status: NOT part of the contract.
 * Proposed-internal audit state only; the namespaced
 * surface itself is [UNRESOLVED: U-03]. No production
 * caller may rely on these counters, and no future slice
 * may promote them to the public ABI without the recorded
 * contract decision INC-E001-07 shape (c) requires.
 */
#ifndef MIMIR_AUDIT_E005_H
#define MIMIR_AUDIT_E005_H

extern unsigned long mimir_audit_call_malloc;
extern unsigned long mimir_audit_call_calloc;
extern unsigned long mimir_audit_call_realloc;
extern unsigned long mimir_audit_call_free;
/* Z-074: appended for the two Wave 9 entry points; the
 * original four declarations above are unchanged. */
extern unsigned long mimir_audit_call_aligned_alloc;
extern unsigned long mimir_audit_call_posix_memalign;
/* Z-075: appended for the usable-size entry point; the
 * declarations above are unchanged. */
extern unsigned long mimir_audit_call_usable_size;
extern unsigned long mimir_audit_call_stats_snapshot; /* Z-076 */
/* Z-077: appended for the two Wave 9 trim / reallocarray
 * entry points; every declaration above is unchanged. */
extern unsigned long mimir_audit_call_trim;
extern unsigned long mimir_audit_call_reallocarray;

extern unsigned long mimir_audit_alloc_published;
extern unsigned long mimir_audit_mapping_retired;

#endif /* MIMIR_AUDIT_E005_H */
