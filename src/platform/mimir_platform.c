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

#include "mimir_platform.h"

#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

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
