/*
 * mimir_platform.c -- MimirAlloc L6 OS abstraction, Linux/POSIX form.
 *
 * Slice C-002. Implements mimir_platform.h from the banked design
 * documents only: B-006 (Phase 3 minimal allocator design, S3/S5/S9)
 * and B-007 (checked arithmetic, Section A), plus POSIX.1 knowledge.
 * Clean-room: no existing allocator's code was consulted or copied.
 *
 * This file is the only one in the slice that names OS facilities.
 */

#define _POSIX_C_SOURCE 200809L
/* MAP_ANONYMOUS is POSIX.1-2024; glibc exposes it under the
 * default feature set, so request that alongside POSIX 200809. */
#define _DEFAULT_SOURCE 1
/* _GNU_SOURCE is unconditional since Z-052: mremap (the
 * in-place growth primitive below) is a GNU extension on
 * this libc, as is sched_getcpu (Z-037, arena builds). */
#define _GNU_SOURCE 1

#include "mimir_platform.h"

#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#ifdef MIMIR_ARENA_CCD
#include <sched.h>
#endif

int mimir_platform_page_size(size_t *out_page_size)
{
    long queried;

    if (out_page_size == NULL) {
        return MIMIR_PLATFORM_ERR_INVALID;
    }
    *out_page_size = 0;

    /* Runtime discovery, checked for failure (B-006 S3, rule L6). */
    queried = sysconf(_SC_PAGESIZE);
    if (queried <= 0) {
        return MIMIR_PLATFORM_ERR_PAGESIZE;
    }
    /* Admission (B-007 A2): must fit size_t and be a power of two. */
    if ((uintmax_t)queried > (uintmax_t)SIZE_MAX) {
        return MIMIR_PLATFORM_ERR_PAGESIZE;
    }
    if ((queried & (queried - 1)) != 0) {
        return MIMIR_PLATFORM_ERR_PAGESIZE;
    }
    *out_page_size = (size_t)queried;
    return MIMIR_PLATFORM_OK;
}

/*
 * page_round_up_checked -- B-007 A5 + A6, both forms satisfied so
 * no unchecked step exists on either reading:
 *   A5 (addition form): x <= SIZE_MAX - (page_size - 1) is tested
 *       BEFORE x + (page_size - 1) is evaluated.
 *   A6 (count form): page_count = ceil(x / page_size) by division
 *       first; the final multiply is tested as
 *       page_count <= SIZE_MAX / page_size BEFORE it is evaluated.
 * page_size is an admitted value from mimir_platform_page_size
 * (nonzero), so page_size - 1 and the division are safe.
 */
static int page_round_up_checked(size_t x, size_t page_size,
                                 size_t *out_rounded)
{
    size_t page_count;

    if (x > SIZE_MAX - (page_size - 1)) {          /* A5, pre-check */
        return MIMIR_PLATFORM_ERR_OVERFLOW;
    }
    page_count = (x + (page_size - 1)) / page_size; /* A6, division */
    if (page_count > SIZE_MAX / page_size) {        /* A6, pre-check */
        return MIMIR_PLATFORM_ERR_OVERFLOW;
    }
    *out_rounded = page_count * page_size;
    return MIMIR_PLATFORM_OK;
}

int mimir_platform_map_acquire(size_t requested_bytes,
                               void **out_base,
                               size_t *out_length)
{
    size_t page_size;
    size_t mapped_length;
    void *base;
    int status;

    if (out_base == NULL || out_length == NULL) {
        return MIMIR_PLATFORM_ERR_INVALID;
    }
    *out_base = NULL;
    *out_length = 0;

    if (requested_bytes == 0) {
        return MIMIR_PLATFORM_ERR_INVALID;
    }

    status = mimir_platform_page_size(&page_size);
    if (status != MIMIR_PLATFORM_OK) {
        return status;
    }

    /* B-006 S6 granularity: a whole number of system pages. */
    status = page_round_up_checked(requested_bytes, page_size,
                                   &mapped_length);
    if (status != MIMIR_PLATFORM_OK) {
        return status;
    }

    /* B-006 S5 acquire form. All-or-nothing: one call either
     * acquires the whole extent or acquires nothing. */
    base = mmap(NULL, mapped_length, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) {
        return MIMIR_PLATFORM_ERR_MAP;
    }

    *out_base = base;
    *out_length = mapped_length;
    return MIMIR_PLATFORM_OK;
}

int mimir_platform_map_release(void *base, size_t length)
{
    if (base == NULL || length == 0) {
        return MIMIR_PLATFORM_ERR_INVALID;
    }
    /* B-006 S5 release form: exactly the acquired address/length. */
    if (munmap(base, length) != 0) {
        return MIMIR_PLATFORM_ERR_UNMAP;
    }
    return MIMIR_PLATFORM_OK;
}

/*
 * Z-052 mremap growth (see the header for the contract and
 * for why the no-relocation form is not offered). mremap
 * with MREMAP_MAYMOVE: the kernel extends the mapping at
 * its address when the room above it is free and otherwise
 * relocates the whole mapping by moving page-table entries
 * -- in neither case is any byte copied by the kernel or
 * by this layer, which is the entire point beside the
 * acquire + copy + release sequence it replaces. The grown
 * extent arrives as readable/writable anonymous pages with
 * the same protection as the old mapping, matching the
 * acquire form above. Length admission mirrors the layer's
 * checked discipline: both lengths must be whole system
 * pages and the growth strict, tested before the call.
 */
int mimir_platform_map_grow(void *base, size_t old_length,
                            size_t new_length, void **out_base)
{
    size_t page_size;
    void *result;
    int status;

    if (base == NULL || out_base == NULL || old_length == 0 ||
        new_length <= old_length) {
        return MIMIR_PLATFORM_ERR_INVALID;
    }
    *out_base = NULL;
    status = mimir_platform_page_size(&page_size);
    if (status != MIMIR_PLATFORM_OK) {
        return status;
    }
    if (old_length % page_size != 0 || new_length % page_size != 0) {
        return MIMIR_PLATFORM_ERR_INVALID;
    }
    result = mremap(base, old_length, new_length, MREMAP_MAYMOVE);
    if (result == MAP_FAILED) {
        return MIMIR_PLATFORM_ERR_MAP;
    }
    *out_base = result;
    return MIMIR_PLATFORM_OK;
}

/*
 * The one simple global lock (B-006 S9.1): a POSIX pthread mutex,
 * normal/default type, statically initialised -- no allocation and
 * no constructor dependency (bootstrap constraint, B-006 S10 F-08
 * as cited in S9.1). The object lives in static storage owned by
 * this layer, never in a mapping and never in allocator-obtained
 * storage (MA-6.3-002 / MA-6.3-012, via B-006 S9.1).
 */
static pthread_mutex_t mimir_global_lock = PTHREAD_MUTEX_INITIALIZER;

int mimir_platform_global_lock(void)
{
    if (pthread_mutex_lock(&mimir_global_lock) != 0) {
        return MIMIR_PLATFORM_ERR_LOCK;
    }
    return MIMIR_PLATFORM_OK;
}

int mimir_platform_global_unlock(void)
{
    if (pthread_mutex_unlock(&mimir_global_lock) != 0) {
        return MIMIR_PLATFORM_ERR_LOCK;
    }
    return MIMIR_PLATFORM_OK;
}

#ifdef MIMIR_LOCK_SHARD
/*
 * Z-039 geometry B shard locks: K mutexes, each in its own
 * cache-line-sized slot (the Z-016 padding rule: a lock word
 * never shares a line with another lock word), statically
 * initialised like the global lock above. The audit state is
 * two atomic counters plus a per-thread held-mask in TLS:
 * taking shard s while a higher shard is held (or while s
 * itself is already held) is an order violation and is
 * counted, never hidden; the acquisition still proceeds, so
 * a violation is evidence, not a new failure mode.
 */
struct mimir_shard_lock {
    _Alignas(64) pthread_mutex_t mutex;
    char padding[64];
};

static struct mimir_shard_lock
    mimir_shard_locks[MIMIR_PLATFORM_SHARD_COUNT] = {
        { PTHREAD_MUTEX_INITIALIZER, { 0 } },
        { PTHREAD_MUTEX_INITIALIZER, { 0 } },
        { PTHREAD_MUTEX_INITIALIZER, { 0 } },
        { PTHREAD_MUTEX_INITIALIZER, { 0 } }
    };

static uint64_t mimir_shard_acq[MIMIR_PLATFORM_SHARD_COUNT];
static uint64_t mimir_shard_violations;
static _Thread_local unsigned int mimir_shard_held_mask;

int mimir_platform_shard_lock(size_t shard)
{
    unsigned int bit;

    if (shard >= MIMIR_PLATFORM_SHARD_COUNT) {
        return MIMIR_PLATFORM_ERR_LOCK;
    }
    bit = 1u << shard;
    if ((mimir_shard_held_mask & bit) != 0 ||
        (mimir_shard_held_mask & (~0u << (shard + 1))) != 0) {
        __atomic_fetch_add(&mimir_shard_violations, 1ULL,
                           __ATOMIC_RELAXED);
    }
    if (pthread_mutex_lock(&mimir_shard_locks[shard].mutex) != 0) {
        return MIMIR_PLATFORM_ERR_LOCK;
    }
    mimir_shard_held_mask |= bit;
    __atomic_fetch_add(&mimir_shard_acq[shard], 1ULL,
                       __ATOMIC_RELAXED);
    return MIMIR_PLATFORM_OK;
}

int mimir_platform_shard_unlock(size_t shard)
{
    if (shard >= MIMIR_PLATFORM_SHARD_COUNT) {
        return MIMIR_PLATFORM_ERR_LOCK;
    }
    if (pthread_mutex_unlock(&mimir_shard_locks[shard].mutex) != 0) {
        return MIMIR_PLATFORM_ERR_LOCK;
    }
    mimir_shard_held_mask &= ~(1u << shard);
    return MIMIR_PLATFORM_OK;
}

uint64_t mimir_platform_shard_acquisitions(size_t shard)
{
    if (shard >= MIMIR_PLATFORM_SHARD_COUNT) {
        return 0;
    }
    return __atomic_load_n(&mimir_shard_acq[shard],
                           __ATOMIC_RELAXED);
}

uint64_t mimir_platform_shard_order_violations(void)
{
    return __atomic_load_n(&mimir_shard_violations,
                           __ATOMIC_RELAXED);
}
#endif /* MIMIR_LOCK_SHARD */

#ifdef MIMIR_ARENA_CCD
/* Z-037: current logical CPU of the calling thread. */
int mimir_platform_current_cpu(size_t *out_cpu)
{
    int cpu;

    if (out_cpu == NULL) {
        return MIMIR_PLATFORM_ERR_INVALID;
    }
    *out_cpu = 0;
    cpu = sched_getcpu();
    if (cpu < 0) {
        return -7; /* prototype-local failure status */
    }
    *out_cpu = (size_t)cpu;
    return MIMIR_PLATFORM_OK;
}
#endif /* MIMIR_ARENA_CCD */
