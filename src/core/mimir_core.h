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
/* Z-048 mapping-cache queries (internal only, shape (b)
 * like the trim queries above): pooled dead large mappings
 * now / their total bytes; (size_t)-1 on lock failure. The
 * full trim and drain entry points above also release the
 * whole pool; trim_retained_some is span-budget only and
 * never touches it. */
size_t mimir_core_pool_count(void);
size_t mimir_core_pool_bytes(void);

/* Z-050 trim policy under pressure (docs/Z13_TRIM_POLICY.md):
 * the conservative return-to-OS policy over the I-003 trim
 * surface. Driven by a /proc/pressure/memory sample (PSI) and
 * by how long the retained pool has been idle. Internal only,
 * INC-E001-07 shape (b) exactly like drain/trim above: NOT
 * part of the public surface in include/mimir_alloc.h, bound
 * local in the shared library (nm -D stays exactly 4); the
 * test-only redeclaration is
 * tests/core/mimir_trim_policy_z050.h. */
struct mimir_pressure_sample {
    double some_avg10;      /* PSI some avg10, percent */
    double full_avg10;      /* PSI full avg10, percent */
    uint64_t some_total_us; /* PSI some total stall, us */
    uint64_t full_total_us; /* PSI full total stall, us */
};

/* Decision values of mimir_trim_decision_for /
 * mimir_core_trim_policy_apply / the tick report. */
#define MIMIR_TRIM_DECISION_NONE   0
#define MIMIR_TRIM_DECISION_PARTIAL 1
#define MIMIR_TRIM_DECISION_FULL   2

struct mimir_trim_policy_report {
    struct mimir_pressure_sample sample;
    uint64_t idle_seconds;   /* retained-pool idle time at tick */
    int decision;            /* MIMIR_TRIM_DECISION_* */
    size_t released_spans;   /* spans this tick released */
    size_t retained_before;  /* Q-RETAINED bytes before */
    size_t retained_after;   /* Q-RETAINED bytes after */
};

/* Pure parser for /proc/pressure/memory text (both lines,
 * kernel field order). Returns 0 on success, -1 on any
 * malformed or missing field; *out is untouched on failure. */
int mimir_pressure_parse(const char *text,
                         struct mimir_pressure_sample *out);
/* Read and parse this host's live /proc/pressure/memory.
 * Returns 0 on success, -1 if the file cannot be read or
 * parsed (policy then does nothing: fail-calm, never
 * fail-trim). */
int mimir_core_pressure_read(struct mimir_pressure_sample *out);
/* The decision table of docs/Z13_TRIM_POLICY.md S2, as a pure
 * function: pressure band x idle seconds -> decision. */
int mimir_trim_decision_for(const struct mimir_pressure_sample *sample,
                            uint64_t idle_seconds);
/* Execute one decision against the retained span pool
 * (FULL: mimir_core_trim_retained; PARTIAL: half the retained
 * spans, class-fair, via mimir_core_trim_retained_some;
 * NONE: nothing). Returns spans actually released and, when
 * out_decision is non-NULL, stores the decision taken. Once
 * the Z-048 mapping cache is banked, its full-trim surface
 * also releases the mapping pool; see the CONDITION-PENDING
 * section of docs/Z13_TRIM_POLICY.md. */
size_t mimir_core_trim_policy_apply(
    const struct mimir_pressure_sample *sample,
    uint64_t idle_seconds, int *out_decision);
/* One live policy tick: read PSI, derive the retained pool's
 * idle time from the J-002 activity counters
 * (bytes_requested + bytes_released unchanged while bytes
 * are retained == idle), apply the decision, and file the
 * report. Returns 0 on success, -1 on a read/snapshot
 * failure, in which case nothing is trimmed. */
int mimir_core_trim_policy_tick(struct mimir_trim_policy_report *report);

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
 * - bytes_mapped: Q-MAPPED now (live mapping bytes +
 *   pooled mapping-cache bytes (Z-048) + live span bytes).
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
 * - unlock_failures: Z-064 / D-01 post-unlock failure
 *   tally -- central (global, composite shard-build, and
 *   per-shard) unlock calls whose L6 unlock returned a
 *   failure status. Before Z-064 every unlock discarded
 *   its status (INC-M002-01); this tally is the owed
 *   observability. Monotone.
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
    /* Z-064 (D-01): monotone count of central unlock calls
     * whose L6 status was a failure, recorded at the unlock
     * site by a pre-lock atomic (see mimir_core.c). Appended
     * at the END of the struct so every earlier field keeps
     * its offset. A snapshot copies the value under the
     * lock; a failure of the snapshot's own unlock is
     * counted after the copy and appears in the NEXT
     * snapshot (and immediately in the lock-free getter
     * below). */
    uint64_t unlock_failures;
};
int mimir_core_stats_snapshot(struct mimir_stats *out);

/* Z-064 (D-01): lock-free read of the unlock_failures tally
 * above. The stats snapshot is the reporting surface of
 * record, but a snapshot takes the very lock whose unlock
 * may have failed; this getter is a single atomic load, so
 * the tally stays readable (and provable) even then.
 * Internal only, same INC-E001-07 shape (b) as the snapshot:
 * reached by tests through tests/core/mimir_stats_j002.h's
 * inclusion of this header; bound local in the shared
 * library (nm -D stays exactly 4). */
uint64_t mimir_core_unlock_failures(void);

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

#ifdef MIMIR_LOCK_HIST
/* Z-019 lock hold/wait histograms (MIMIRALLOC_Z13_ROADMAP.md
 * Wave 3, slice Z-019): rdtsc instrumentation of the one
 * global lock, per public entry point. This surface exists
 * ONLY in a build that defines MIMIR_LOCK_HIST (the dedicated
 * instrumented build of tests/perf/run_lockhist_z019.sh);
 * the production Makefile never defines it, so a production
 * object contains none of this surface and pays exactly zero
 * cost (proven in tests/perf/recorded_run_z13_z019.txt).
 * Internal only, INC-E001-07 shape (b) exactly like the
 * surfaces above: declared here; bound local in any shared
 * build by the same version-script discipline.
 * Buckets (quarter-octave): index < 64 is the exact cycle
 * count; index >= 64 encodes e = 6 + (idx-64)/4 and
 * sub = (idx-64) % 4, covering the interval
 * [2^e + sub*2^(e-2), 2^e + (sub+1)*2^(e-2)) cycles -
 * zero width error below 64 cycles, at most 25% above.
 * Samples at or above 2^42 cycles clamp into the last
 * bucket and are counted in overflow_samples.
 * mimir_core_lock_hist_snapshot copies the histograms at a
 * QUIESCENT point (no thread inside the allocator); it
 * deliberately takes no lock, so reading never pollutes
 * the measurement. Returns 0 on success, -1 on NULL out.
 * mimir_core_lock_hist_reset zeroes all series; call only
 * at quiescent points for the same reason. */
#define MIMIR_LOCK_HIST_OPS 5
#define MIMIR_LOCK_HIST_BUCKETS 208
enum mimir_lock_hist_op {
    MIMIR_LH_OP_MALLOC  = 0,
    MIMIR_LH_OP_CALLOC  = 1,
    MIMIR_LH_OP_REALLOC = 2,
    MIMIR_LH_OP_FREE    = 3,
    MIMIR_LH_OP_OTHER   = 4
};
struct mimir_lock_hist_series {
    uint64_t count;
    uint64_t sum_cycles;
    uint64_t min_cycles;
    uint64_t max_cycles;
    uint64_t overflow_samples;
    uint64_t buckets[MIMIR_LOCK_HIST_BUCKETS];
};
struct mimir_lock_hist {
    struct mimir_lock_hist_series hold[MIMIR_LOCK_HIST_OPS];
    struct mimir_lock_hist_series wait[MIMIR_LOCK_HIST_OPS];
    uint64_t lock_failures;
    uint64_t anomalies;
};
int mimir_core_lock_hist_snapshot(struct mimir_lock_hist *out);
void mimir_core_lock_hist_reset(void);
#endif /* MIMIR_LOCK_HIST */

/* Z-029 TLS cache surface (contract
 * docs/Z13_TLS_CACHE_CONTRACT.md; slice doc
 * docs/Z13_TLS_CACHE_IMPLEMENTATION.md): the six functions
 * of tests/contract/mimir_tls_cache_z028.h, defined in
 * mimir_core.c over the per-thread freelist cache.
 * Internal only, INC-E001-07 shape (b) exactly like the
 * surfaces above: NOT part of the public surface in
 * include/mimir_alloc.h, bound local in the shared
 * library (nm -D stays exactly 4). The snapshot struct is
 * forward-declared here; its full definition is the
 * contract header's, so a signature drift between this
 * file and the defining translation unit (which includes
 * both) is a compile error, never a silent miss. */
#ifdef MIMIR_LOCK_SHARD
/* Z-039 geometry B (lock-sharding) surface: exists ONLY in
 * a MIMIR_LOCK_SHARD build of the core (slice Z-039
 * follow-up). Internal only, INC-E001-07 shape (b) exactly
 * like the surfaces above: declared here, re-declared for
 * tests only in tests/core/mimir_shard_z039.h; bound local
 * in the shared library (nm -D stays exactly 4). The shard
 * map is the Z-039 paper design's grouping: the hot small
 * classes 16 B and 32 B each own a shard, 48 B + 64 B share
 * one, and every colder class (80 B..1024 B) shares the
 * last -- K = 4 shard locks beside the retained global
 * lock, which in a shard build guards the mapping registry
 * and is always taken LAST (fixed order: shards ascending,
 * then global). */
#define MIMIR_SHARD_COUNT ((size_t)4)
struct mimir_shard_snapshot {
    uint64_t acquisitions[MIMIR_SHARD_COUNT];
    uint64_t order_violations;
};
size_t mimir_shard_of_class(size_t class_index);
int mimir_shard_snapshot(struct mimir_shard_snapshot *out);
#endif /* MIMIR_LOCK_SHARD */

struct mimir_tls_cache_snapshot;
int mimir_tls_cache_snapshot(struct mimir_tls_cache_snapshot *out);
#ifdef MIMIR_ARENA_CCD
/* Z-037 arena surface (roadmap Wave 5, slice Z-037): exists
 * ONLY in a MIMIR_ARENA_CCD build. Internal only, INC-E001-07
 * shape (b) exactly like the surfaces above: declared here
 * and re-declared for tests only in
 * tests/core/mimir_arena_z037.h; bound local in the shared
 * library (nm -D stays exactly 4). The snapshot is taken
 * under the one global lock, so every field describes the
 * same instant; selections counts arena selections made by
 * the small path (one per small_alloc / TLS refill take
 * transaction); spans and live_objects are per-arena
 * registry counts at the snapshot instant. Returns 0 on
 * success, -1 on NULL out or lock failure. */
#define MIMIR_ARENA_COUNT 2
struct mimir_arena_snapshot {
    uint64_t spans[MIMIR_ARENA_COUNT];
    uint64_t live_objects[MIMIR_ARENA_COUNT];
    uint64_t selections[MIMIR_ARENA_COUNT];
};
int mimir_arena_snapshot(struct mimir_arena_snapshot *out);
/* The calling thread's arena under the Z-012 CCD map
 * (CCD0: logical 0-7 + SMT 16-23; CCD1: 8-15 + 24-31).
 * Lock-free; falls back to arena 0 when the CPU query
 * fails. */
size_t mimir_arena_current(void);
#endif /* MIMIR_ARENA_CCD */
size_t mimir_tls_cache_bin_capacity(size_t class_index);
size_t mimir_tls_cache_batch(size_t class_index);
void mimir_tls_cache_drain_thread(void);
uint64_t mimir_tls_cache_exit_drains(void);
uint64_t mimir_tls_cache_exit_drain_blocks(void);

#endif /* MIMIR_CORE_H */
