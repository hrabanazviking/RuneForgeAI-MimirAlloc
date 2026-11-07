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
#include <stdint.h>

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

/* On-demand trim (I-003, docs/PHASE_9_TRIM_IMPLEMENTATION.md;
 * semantics fixed by I-002 Section 5): the drain's release
 * operation invoked on demand instead of at unload, through
 * the same locked class-fair walk (one release path, I-002
 * S5.1). mimir_core_trim_retained releases every retained
 * EMPTY span; mimir_core_trim_retained_some releases at
 * most max_spans of them (the budget step-3 stop-at-need
 * mechanism, I-002 S5.2.3). Both return the number of spans
 * actually released (bytes = spans x page size; every span
 * is exactly one page), touch no live span or mapping, are
 * non-terminal and idempotent, and return 0 when nothing is
 * released or the lock cannot be taken. The two queries
 * read Q-RETAINED and Q-MAPPED (I-002 S3.2) as single
 * locked snapshots at quiescent points, returning
 * (size_t)-1 only if the lock cannot be taken (G-007 getter
 * posture). Internal only, exactly like the drain:
 * declared here and re-declared for tests only in
 * tests/core/mimir_trim_i003.h, bound local in the shared
 * library (nm -D stays exactly 4). */
size_t mimir_core_trim_retained(void);
size_t mimir_core_trim_retained_some(size_t max_spans);
size_t mimir_core_retained_spans(void);
size_t mimir_core_retained_bytes(void);
size_t mimir_core_mapped_bytes(void);

/* G-007 tuning pair (docs/PHASE_7_TUNING_PARAMETERS_G007.md):
 * the per-class empty-span retention bound setter/getter,
 * defined in mimir_core.c over the one locked scalar.
 * Internal only, INC-E001-07 shape (b) exactly like the
 * drain and trim above: NOT part of the public surface in
 * include/mimir_alloc.h, bound local in the shared library
 * (nm -D stays exactly 4); the test-only redeclaration is
 * tests/core/mimir_tune_g007.h. Declared HERE as well since
 * M-003 (Phase 13 warning-gates sweep,
 * docs/PHASE_13_WARNING_GATES_M003.md): until M-003 the
 * defining translation unit saw no prototype for either
 * function (-Wmissing-prototypes, gcc 12.2), so a signature
 * drift between mimir_core.c and the test-only header would
 * have compiled and linked silently - C binds by name only.
 * Valid range 0..16 (MIMIR_RETENTION_MAX in mimir_core.c);
 * setter returns 0 ok, -1 out of range (nothing changed),
 * -2 lock failure (nothing changed); getter returns
 * (size_t)-1 only if the lock cannot be taken. */
int mimir_tune_set_retention_bound(size_t value);
size_t mimir_tune_get_retention_bound(void);

/* J-002 statistics snapshot (docs/PHASE_10_STATISTICS.md;
 * roadmap Phase 10; quantities in the I-002 vocabulary of
 * docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md S3.2/S4.1).
 * mimir_core_stats_snapshot copies the whole statistics
 * surface under the one global lock, so every field of one
 * snapshot describes the same instant; it returns 0 on
 * success and -1 (with *out untouched) when out is NULL or
 * the lock cannot be taken (G-007 getter posture carried to
 * a struct). Internal only, INC-E001-07 shape (b) exactly
 * like the trim surface above: declared here and
 * re-declared for tests only in
 * tests/core/mimir_stats_j002.h; bound local in the shared
 * library (nm -D stays exactly 4).
 *
 * Field meanings (exact; slice doc S2 has the full table):
 * - bytes_requested: cumulative caller-requested bytes of
 *   successful NEW block publications (malloc, calloc
 *   product, realloc-NULL, and realloc grow/move new size).
 *   Staying/shrink/same-size realloc publishes nothing and
 *   adds nothing. Monotone.
 * - bytes_allocated: cumulative usable bytes actually
 *   granted at those same publications (small: the class
 *   size; large: usable_size = mapping_length - one page).
 *   Monotone.
 * - bytes_cached: bytes of retained EMPTY spans now
 *   (Q-RETAINED, registry walk under the lock).
 * - bytes_mapped: Q-MAPPED now (live mapping bytes + live
 *   span bytes).
 * - bytes_released: Q-RELEASED, cumulative successfully
 *   released mapping_length over published mappings and
 *   spans (retire_locked / span_release_locked successes).
 *   Monotone.
 * - peak_mapped: Q-PEAKMAPPED, high-water of Q-MAPPED
 *   sampled at the two publication points under the lock.
 * - alloc_failures: Q-FAILED-OS -- entry-point calls that
 *   returned NULL for a genuine reason (backing, page-size
 *   or lock failure, or a pre-admission arithmetic
 *   rejection such as calloc overflow / oversize). Posture
 *   NULLs are NOT counted here.
 * - alloc_posture_nulls: entry-point calls that returned
 *   NULL purely as an implementation posture (zero-size
 *   U-09, calloc zero factor U-09, realloc(ptr,0) U-12).
 *   Disjoint from alloc_failures.
 * - cross_thread_frees: PARTIAL (INC-J002-01) -- frees of
 *   MAPPING-path blocks whose allocating thread (recorded
 *   in the mapping's reserve page past the 96-byte
 *   descriptor at publication; the descriptor's own flags
 *   and reserved bytes stay zero per B-007 C1.4) differs
 *   from the freeing thread. Small-object frees are not
 *   counted: no per-object owner record exists.
 * - lock_contention / lock_contention_measured:
 *   ABSENT (INC-J002-02) -- always 0 / 0; no truthful cheap
 *   measure exists under the one L6 global lock.
 * - live_* and span_* fields: occupancy at the snapshot
 *   instant (per-class live objects, span counts, empty
 *   span counts and summed per-span capacities). */
#define MIMIR_STATS_CLASS_COUNT 20
struct mimir_stats {
    uint64_t bytes_requested;
    uint64_t bytes_allocated;
    uint64_t bytes_cached;
    uint64_t bytes_mapped;
    uint64_t bytes_released;
    uint64_t peak_mapped;
    uint64_t alloc_failures;
    uint64_t alloc_posture_nulls;
    uint64_t cross_thread_frees;
    uint64_t lock_contention;
    uint64_t lock_contention_measured;
    uint64_t live_mappings;
    uint64_t live_spans;
    uint64_t live_small_objects;
    uint64_t live_mapping_bytes;
    uint64_t live_span_bytes;
    uint64_t page_size_bytes;
    uint64_t span_class_size[MIMIR_STATS_CLASS_COUNT];
    uint64_t span_live_objects[MIMIR_STATS_CLASS_COUNT];
    uint64_t span_total_spans[MIMIR_STATS_CLASS_COUNT];
    uint64_t span_empty_spans[MIMIR_STATS_CLASS_COUNT];
    uint64_t span_capacity_objects[MIMIR_STATS_CLASS_COUNT];
};
int mimir_core_stats_snapshot(struct mimir_stats *out);

#ifdef MIMIR_DEBUG
/* J-004 on-demand red-zone verification (debug builds only;
 * docs/PHASE_10_DEBUG_REDZONES.md): walks every span under
 * the global lock, revalidates span headers and the guards
 * of every live small object; returns the count of corrupted
 * units (0 = clean) or -1 on lock failure. Absent entirely
 * from production builds. Internal only, INC-E001-07 shape
 * (b): re-declared for tests in
 * tests/core/mimir_debug_j004.h; bound local in the shared
 * library (nm -D stays exactly 4 in both flavours). */
int mimir_core_debug_verify_all(void);
#endif
#ifdef MIMIR_DEBUG

/* J-005 debug-only quarantine surface
 * (docs/PHASE_10_POISON_QUARANTINE.md; plan S5.2/S5.3).
 * These three entry points exist ONLY in a MIMIR_DEBUG
 * build; a production object does not contain them.
 * mimir_debug_quarantine_count: blocks currently held in
 * the bounded quarantine (capacity 32, slice doc S2).
 * mimir_debug_quarantined_bytes: bytes_quarantined, the
 * S5.3 own-named quantity -- the summed class extents of
 * the held blocks, reported separately from D-CACHED,
 * whose production definition is unchanged.
 * mimir_debug_quarantine_verify: on-demand verification
 * of every held block against the poison pattern; returns
 * how many held blocks are corrupted (0 = all clean),
 * -1 if the lock cannot be taken; it never aborts -- the
 * allocator-triggered eviction check is the abort path,
 * exercised only in disposable child processes.
 * Internal only, INC-E001-07 shape (b) exactly like the
 * trim surface above: re-declared for tests only in
 * tests/core/mimir_quarantine_j005.h; bound local in the
 * shared library (nm -D stays exactly 4 both flavours). */
size_t mimir_debug_quarantine_count(void);
size_t mimir_debug_quarantined_bytes(void);
int mimir_debug_quarantine_verify(void);
#endif /* MIMIR_DEBUG */

/* J-006 metadata integrity check
 * (docs/PHASE_10_METADATA_INTEGRITY.md): one locked walk
 * over both registries verifying the per-structure
 * invariants, the counter cross-sums, free-list shape and
 * the C1.4 zero fields; returns the violation count
 * (0 = consistent) or -1 on lock failure. Internal only,
 * INC-E001-07 shape (b) exactly like the statistics
 * surface above: declared here and re-declared for tests
 * only in tests/core/mimir_integrity_j006.h; bound local
 * in the shared library (nm -D stays exactly 4). No
 * production entry point calls it. */
int mimir_core_metadata_check(void);

/* J-007 diagnostics channel (docs/PHASE_10_DIAGNOSTICS.md;
 * J-001 S5.4, exit item X6): allocation-free, recursion-
 * proof error reporting. mimir_core_diag_report formats ONE
 * fixed-format line into a fixed stack buffer from fixed
 * name tables and emits it with write(2) to fd 2 (the
 * J-004/J-005 precedent shape, generalised; L6 publishes no
 * diagnostic channel -- INC-J005-05's re-home). It calls no
 * allocator, no L6 entry and no stdio; a static guard makes
 * a nested report attempt a counted no-op, so reporting can
 * never recursively allocate or re-enter. Fields, in order:
 * op (table below), cause (table below), req (caller-
 * requested bytes; SIZE_MAX marks an unrepresentable calloc
 * product), failures / posture (the J-002 counters,
 * atomically loaded at emit time -- the failure being
 * reported is already counted, so failures >= 1 on every
 * wired path). Wired ONLY to genuine failure NULLs:
 * allocation backing/arithmetic/pagesize/internal failures
 * and lock failure at the entry points. Posture NULLs
 * (U-09/U-12) and invalid-pointer realloc refusals emit
 * NOTHING: posture is not an error, and alloc_failures /
 * alloc_posture_nulls keep their J-002 disjoint semantics.
 * Internal only, INC-E001-07 shape (b) exactly like the
 * stats surface: declared here, re-declared for tests only
 * in tests/core/mimir_diag_j007.h; bound local in the
 * shared library (nm -D stays exactly 4). Emission is
 * gated by the process environment (MIMIRALLOC_DIAG=1),
 * the recorded INC-J007-01 reconciliation with the banked
 * C-006 silence battery; the gate check itself is
 * allocation-free (getenv) and a closed gate is a plain
 * return with no counter movement. The three
 * counters are cumulative since process start:
 * emitted (lines fully written), suppressed (nested
 * attempts + lines whose write failed), max_depth (deepest
 * report nesting ever entered; the guard keeps it at 1). */
enum mimir_diag_op {
    MIMIR_DIAG_OP_MALLOC  = 0,
    MIMIR_DIAG_OP_CALLOC  = 1,
    MIMIR_DIAG_OP_REALLOC = 2
};
enum mimir_diag_cause {
    MIMIR_DIAG_CAUSE_NONE       = 0,
    MIMIR_DIAG_CAUSE_BACKING    = 1, /* L6 map acquire failed */
    MIMIR_DIAG_CAUSE_ARITHMETIC = 2, /* checked arithmetic refused */
    MIMIR_DIAG_CAUSE_LOCK       = 3, /* global lock failure */
    MIMIR_DIAG_CAUSE_PAGESIZE   = 4, /* page-size discovery failed */
    MIMIR_DIAG_CAUSE_INTERNAL   = 5  /* layout/publication gate */
};
void mimir_core_diag_report(int op, int cause, uint64_t requested);
uint64_t mimir_core_diag_emitted(void);
uint64_t mimir_core_diag_suppressed(void);
uint64_t mimir_core_diag_max_depth(void);

#endif /* MIMIR_CORE_H */
