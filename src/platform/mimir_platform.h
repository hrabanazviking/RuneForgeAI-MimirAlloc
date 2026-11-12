/*
 * mimir_platform.h -- MimirAlloc L6 OS abstraction (platform layer).
 *
 * Slice C-002. This header is the published interface of the ONLY
 * layer permitted platform-specific code (B-006 S4/S5, layer L6).
 * It deliberately exposes no platform types: callers see only ISO C
 * types, so upper layers consume the primitive without naming any
 * OS facility (B-006 S9.1: L6 provides, upper layers consume --
 * never re-implement).
 *
 * Provided, and nothing else:
 *   1. Runtime system page size discovery (B-006 S3, design rule
 *      L6): obtained from the host at runtime via
 *      sysconf(_SC_PAGESIZE), checked for failure. NEVER a fixed
 *      constant: this Pi reports a 16384-byte system page size,
 *      other admitted targets report 4096; both are host facts,
 *      not assumptions (MA-6.2-041: every page quantity here is a
 *      system page size, never an internal logical page).
 *   2. OS mapping acquire/release: one all-or-nothing mapping per
 *      acquire (B-006 S5); size arithmetic is checked before it is
 *      evaluated, per B-007 Section A.
 *   3. One simple global lock (B-006 S9): a single coarse mutex
 *      provided by L6 through this interface. It guards allocator
 *      metadata only; it never guards user bytes (B-006 S9.2).
 *
 * Error posture (B-007 D.0/D.1): errno is NOT promised. The
 * bounded caller observation is the status return value below.
 *
 * Clean-room note: written from the banked MimirAlloc design
 * documents (B-006, B-007) and POSIX.1 / ISO C11 knowledge only.
 */

#ifndef MIMIR_PLATFORM_H
#define MIMIR_PLATFORM_H

#include <stddef.h>

/* Status codes returned by every function in this interface. */
enum mimir_platform_status {
    MIMIR_PLATFORM_OK            =  0, /* success                     */
    MIMIR_PLATFORM_ERR_INVALID   = -1, /* null out-param, zero size,
                                          zero length, null base      */
    MIMIR_PLATFORM_ERR_OVERFLOW  = -2, /* checked size arithmetic
                                          rejected the request before
                                          any evaluation (B-007 A)    */
    MIMIR_PLATFORM_ERR_PAGESIZE  = -3, /* runtime system page size
                                          discovery failed or the
                                          host value was not admitted */
    MIMIR_PLATFORM_ERR_MAP       = -4, /* OS mapping acquire failed   */
    MIMIR_PLATFORM_ERR_UNMAP     = -5, /* OS mapping release failed   */
    MIMIR_PLATFORM_ERR_LOCK      = -6  /* global lock operation failed*/
};

/*
 * Discover the host system page size at runtime and store it in
 * *out_page_size (bytes). The value is queried from the OS on
 * every call; nothing is cached and nothing is hardcoded.
 * Admission (B-007 A2): the queried value must be positive, fit
 * in size_t, and be a power of two, else ERR_PAGESIZE.
 */
int mimir_platform_page_size(size_t *out_page_size);

/*
 * Acquire one OS mapping of at least requested_bytes bytes.
 * The mapped length is requested_bytes rounded up to a whole
 * number of system pages (B-006 S6), computed with the checked
 * arithmetic of B-007 A5/A6 (check before evaluate). On success
 * *out_base is the mapping base (system-page aligned by the OS)
 * and *out_length is the exact mapped length, which the caller
 * must hand back unchanged to mimir_platform_map_release
 * (B-006 S5: release over exactly the acquired address/length).
 * Acquire form (B-006 S5): null hint address, read/write
 * protection, private anonymous mapping, no fixed placement --
 * the layer never overwrites an existing mapping and never
 * demands a specific address. All-or-nothing: on any failure
 * *out_base is NULL, *out_length is 0, and nothing is mapped.
 * requested_bytes == 0 is ERR_INVALID.
 */
int mimir_platform_map_acquire(size_t requested_bytes,
                               void **out_base,
                               size_t *out_length);

/*
 * Release exactly one mapping previously returned by
 * mimir_platform_map_acquire: base and length must be the exact
 * acquired pair. NULL base or zero length is ERR_INVALID and no
 * OS call is made. After a successful release the caller must
 * not touch the region again (B-006 S7.1).
 */
int mimir_platform_map_release(void *base, size_t length);

/*
 * Z-052: grow one live mapping to a larger exact length by
 * mremap: base must be a mapping previously returned by
 * mimir_platform_map_acquire, old_length its exact acquired
 * length, and new_length a whole number of system pages
 * strictly greater than old_length (both lengths are exact:
 * this layer performs no rounding here -- the caller owns
 * the checked page arithmetic, exactly as release owns the
 * exact acquired pair). The kernel either extends the
 * mapping at its existing address (when the address room
 * above it is free) or relocates it whole -- page tables
 * only, the contents are never copied by anyone -- and
 * *out_base receives the resulting base, which equals the
 * input base exactly in the extension case. After a
 * successful call the old address is invalid whenever
 * *out_base differs from base, exactly as after a release.
 * On any failure the status is non-OK, *out_base is NULL,
 * and the original mapping is untouched at its old length
 * and old address. A no-relocation variant of this call
 * was measured and rejected on this host before this form
 * was chosen: the kernel's top-down placement packs every
 * fresh mapping directly against the VMA above it, so an
 * in-place-only extension of a fresh mapping is refused
 * (ENOMEM) essentially always -- the audit and the maps
 * evidence are recorded in docs/Z13_MREMAP_GROWTH.md.
 * This is the only facility in the layer that can change
 * a live mapping's length, and it exists for realloc's
 * growth path (MIMIRALLOC_Z13_ROADMAP.md Z-052).
 */
int mimir_platform_map_grow(void *base, size_t old_length,
                            size_t new_length, void **out_base);

/*
 * The one simple global lock of B-006 S9: a single coarse mutex,
 * statically initialised in the implementation (no allocation, no
 * constructor dependency; B-006 S9.1). Lock/unlock return OK or
 * ERR_LOCK. The lock is never held across a return to the caller
 * of any allocator entry point (B-006 S9.4) -- that discipline
 * belongs to the owning layer; this interface only provides the
 * primitive.
 */
int mimir_platform_global_lock(void);
int mimir_platform_global_unlock(void);

#ifdef MIMIR_LOCK_SHARD
/*
 * Z-039 geometry B (lock-sharding, slice Z-039 follow-up):
 * K shard locks beside the global lock. This declaration
 * block exists ONLY in a MIMIR_LOCK_SHARD build; a production
 * build of this header is unchanged. The shard locks are
 * statically initialised in the implementation, exactly like
 * the global lock (no allocation, no constructor dependency).
 * mimir_platform_shard_acquisitions / _order_violations are
 * the lock-order audit surface: the implementation counts
 * every shard acquisition per shard and every acquisition
 * that violates the campaign's fixed order (a shard may be
 * taken only while no strictly higher shard is held by the
 * calling thread; the global lock is taken last, after all
 * shards -- the platform layer cannot see that step, so the
 * audit covers the shard-to-shard order, and the core's
 * acquisition helpers are the only road to the global lock
 * in a shard build).
 */
#include <stdint.h>
#define MIMIR_PLATFORM_SHARD_COUNT ((size_t)4)
int mimir_platform_shard_lock(size_t shard);
int mimir_platform_shard_unlock(size_t shard);
uint64_t mimir_platform_shard_acquisitions(size_t shard);
uint64_t mimir_platform_shard_order_violations(void);
#endif /* MIMIR_LOCK_SHARD */

#ifdef MIMIR_ARENA_CCD
/*
 * Z-037 per-CCD arena prototype (MIMIRALLOC_Z13_ROADMAP.md
 * Wave 5): the calling thread's current logical CPU index,
 * discovered from the OS on every call (sched_getcpu on the
 * Linux/POSIX form). This declaration exists ONLY in a
 * MIMIR_ARENA_CCD build; a production build of this header
 * is unchanged. Returns MIMIR_PLATFORM_OK and stores the
 * index; MIMIR_PLATFORM_ERR_INVALID for a NULL out-param;
 * a failed OS query returns -7, a prototype-local status
 * outside the banked enum (the arena caller treats every
 * non-OK status as CPU-unknown and falls back to arena 0,
 * so no banked status needed a second meaning).
 */
int mimir_platform_current_cpu(size_t *out_cpu);
#endif /* MIMIR_ARENA_CCD */

#endif /* MIMIR_PLATFORM_H */
