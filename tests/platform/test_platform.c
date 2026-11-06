/*
 * test_platform.c -- standalone exercise of the C-002 platform layer.
 *
 * Slice C-002 evidence program. It:
 *   1. prints the runtime system page size (queried, not assumed);
 *   2. checks the invalid/overflow rejections of the acquire path
 *      (B-007 Section A: SIZE_MAX must be refused by the checked
 *      arithmetic, before any OS call);
 *   3. acquires one mapping, writes a byte pattern across the WHOLE
 *      mapped length, reads it back and verifies it;
 *   4. exercises the simple global lock: two threads increment a
 *      shared counter under the lock; the exact total proves the
 *      increments serialised;
 *   5. releases the mapping with the exact acquired pair.
 * Exit status 0 and a final "RESULT: PASS" line only if every
 * check passed. Prints only facts it observed.
 */

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>

#include "../../src/platform/mimir_platform.h"

#define THREAD_COUNT 2
#define INCREMENTS_PER_THREAD 10000

static int failures = 0;

static void check(int condition, const char *label)
{
    printf("  [%s] %s\n", condition ? "ok" : "FAIL", label);
    if (!condition) {
        failures++;
    }
}

static long shared_counter = 0;

static void *increment_worker(void *unused)
{
    int i;

    (void)unused;
    for (i = 0; i < INCREMENTS_PER_THREAD; i++) {
        if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
            return (void *)(intptr_t)1;
        }
        shared_counter++;
        if (mimir_platform_global_unlock() != MIMIR_PLATFORM_OK) {
            return (void *)(intptr_t)1;
        }
    }
    return NULL;
}

int main(void)
{
    size_t page_size = 0;
    size_t requested = 100; /* deliberately not a page multiple */
    void *base = NULL;
    size_t mapped_length = 0;
    unsigned char *bytes;
    size_t i;
    int pattern_ok = 1;
    int status;
    pthread_t threads[THREAD_COUNT];
    int t;
    int threads_ok = 1;

    printf("C-002 platform layer test\n");

    /* 1. Runtime page size. */
    status = mimir_platform_page_size(&page_size);
    printf("runtime system page size: %zu bytes (status %d)\n",
           page_size, status);
    check(status == MIMIR_PLATFORM_OK, "page size query succeeds");
    check(page_size > 0 && (page_size & (page_size - 1)) == 0,
          "page size is a nonzero power of two");

    /* 2. Rejections: invalid and overflow, before any OS call. */
    status = mimir_platform_map_acquire(0, &base, &mapped_length);
    printf("acquire(0): status %d (expect %d ERR_INVALID)\n",
           status, MIMIR_PLATFORM_ERR_INVALID);
    check(status == MIMIR_PLATFORM_ERR_INVALID && base == NULL,
          "zero request rejected as invalid, nothing mapped");

    status = mimir_platform_map_acquire(SIZE_MAX, &base, &mapped_length);
    printf("acquire(SIZE_MAX): status %d (expect %d ERR_OVERFLOW)\n",
           status, MIMIR_PLATFORM_ERR_OVERFLOW);
    check(status == MIMIR_PLATFORM_ERR_OVERFLOW && base == NULL,
          "SIZE_MAX request rejected by checked arithmetic");

    status = mimir_platform_map_release(NULL, page_size);
    check(status == MIMIR_PLATFORM_ERR_INVALID,
          "release(NULL, page_size) rejected as invalid");

    /* 3. Acquire, pattern write/read across the whole mapping. */
    status = mimir_platform_map_acquire(requested, &base, &mapped_length);
    printf("acquire(%zu): status %d, base %p, mapped length %zu\n",
           requested, status, base, mapped_length);
    check(status == MIMIR_PLATFORM_OK && base != NULL,
          "mapping acquired");
    check(mapped_length >= requested &&
          mapped_length % page_size == 0,
          "mapped length is a whole number of system pages");

    if (base != NULL) {
        bytes = (unsigned char *)base;
        for (i = 0; i < mapped_length; i++) {
            bytes[i] = (unsigned char)(i % 251u);
        }
        for (i = 0; i < mapped_length; i++) {
            if (bytes[i] != (unsigned char)(i % 251u)) {
                pattern_ok = 0;
                break;
            }
        }
        check(pattern_ok, "pattern verified across whole mapping");
    }

    /* 4. Global lock exercised by two threads. */
    for (t = 0; t < THREAD_COUNT; t++) {
        if (pthread_create(&threads[t], NULL, increment_worker,
                           NULL) != 0) {
            threads_ok = 0;
        }
    }
    for (t = 0; t < THREAD_COUNT; t++) {
        void *ret = NULL;

        if (pthread_join(threads[t], &ret) != 0 || ret != NULL) {
            threads_ok = 0;
        }
    }
    printf("lock-exercised counter: %ld (expect %d)\n",
           shared_counter, THREAD_COUNT * INCREMENTS_PER_THREAD);
    check(threads_ok, "lock/unlock succeeded in all worker calls");
    check(shared_counter == THREAD_COUNT * INCREMENTS_PER_THREAD,
          "shared counter exact under the global lock");

    /* 5. Release with the exact acquired pair. */
    if (base != NULL) {
        status = mimir_platform_map_release(base, mapped_length);
        printf("release(base, %zu): status %d\n", mapped_length, status);
        check(status == MIMIR_PLATFORM_OK, "mapping released");
    }

    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
