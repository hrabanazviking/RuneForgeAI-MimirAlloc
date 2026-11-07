/* test_e008_threads.c - E-008 startup ordering: first call
 * raced from multiple threads (foundation plan T8's second
 * startup case).
 *
 * This program makes NO MimirAlloc call before the worker
 * threads are created and released together behind a
 * pthread barrier, so the allocator's first-ever calls in
 * this process race each other from 8 threads at once. If
 * first use required any ordered init step, or the
 * statically initialised lock were not usable from the
 * very first concurrent calls, this is the shape that
 * would break. Each thread allocates its block as its
 * first action after the barrier, fills it with its own
 * pattern and records the pointer; main verifies every
 * block (distinctness + pattern byte-for-byte), then each
 * block is freed from a DIFFERENT thread than the one
 * that allocated it (cross-thread ownership rides along
 * as a bonus observation, labelled as such in the slice
 * record - the contracted claim here is the startup one).
 *
 * No constructor exists in this TU and the executable
 * links only build/libmimiralloc.so. Defined cases only.
 * Standalone via make test-e008; NOT part of make test.
 */
#define _POSIX_C_SOURCE 200809L
/* pthread barriers are POSIX.1-2001; strict -std=c11 hides
 * them without this feature-test macro (the same pattern
 * src/platform/mimir_platform.c uses). */
#include "mimir_alloc.h"

#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define E008_THREADS 8
#define E008_TBLOCK 96

static pthread_barrier_t g_barrier;
static unsigned char *g_blocks[E008_THREADS];
static int g_alloc_ok[E008_THREADS];

static unsigned char tpat(int t)
{
    return (unsigned char)(0x21u + (unsigned)t * 0x11u);
}

static void *alloc_worker(void *arg)
{
    long t = (long)arg;
    unsigned char *p;
    pthread_barrier_wait(&g_barrier);
    /* First MimirAlloc call of this thread - and, racing
     * the other seven, among the first in the process. */
    p = (unsigned char *)mimir_malloc(E008_TBLOCK);
    g_blocks[t] = p;
    g_alloc_ok[t] = (p != NULL);
    if (p != NULL) {
        memset(p, tpat((int)t), E008_TBLOCK);
    }
    return NULL;
}

static void *free_worker(void *arg)
{
    long t = (long)arg;
    /* Free the NEXT thread's block: the free executes in a
     * different thread from the allocation. */
    mimir_free(g_blocks[(t + 1) % E008_THREADS]);
    return NULL;
}

static int passed = 0;
static int failed = 0;

static void check(int condition, const char *name)
{
    if (condition) {
        passed++;
        printf("PASS %s\n", name);
    } else {
        failed++;
        printf("FAIL %s\n", name);
    }
}

int main(void)
{
    pthread_t th[E008_THREADS];
    long t;
    int i;
    int j;
    int ok;
    unsigned char *after;

    printf("E-008 concurrent first-call consumer "
           "(8 threads race the allocator's first calls)\n");
    pthread_barrier_init(&g_barrier, NULL, E008_THREADS);
    for (t = 0; t < E008_THREADS; t++) {
        g_blocks[t] = NULL;
        g_alloc_ok[t] = 0;
        pthread_create(&th[t], NULL, alloc_worker, (void *)t);
    }
    for (t = 0; t < E008_THREADS; t++) {
        pthread_join(th[t], NULL);
    }
    pthread_barrier_destroy(&g_barrier);

    for (t = 0; t < E008_THREADS; t++) {
        check(g_alloc_ok[t], "racing first mimir_malloc returns non-NULL");
    }
    ok = 1;
    for (i = 0; i < E008_THREADS; i++) {
        for (j = i + 1; j < E008_THREADS; j++) {
            if (g_blocks[i] != NULL && g_blocks[i] == g_blocks[j]) { ok = 0; }
        }
    }
    check(ok, "all 8 racing first blocks are pairwise distinct");
    ok = 1;
    for (t = 0; t < E008_THREADS; t++) {
        if (g_blocks[t] == NULL) { ok = 0; continue; }
        for (i = 0; i < E008_TBLOCK; i++) {
            if (g_blocks[t][i] != tpat((int)t)) { ok = 0; }
        }
    }
    check(ok, "every racing block holds its own thread's pattern byte-for-byte");

    for (t = 0; t < E008_THREADS; t++) {
        pthread_create(&th[t], NULL, free_worker, (void *)t);
    }
    for (t = 0; t < E008_THREADS; t++) {
        pthread_join(th[t], NULL);
    }
    check(1, "each block freed from a different thread than its allocator, all return normally");

    after = (unsigned char *)mimir_malloc(64);
    check(after != NULL, "allocator healthy in main after concurrent first use");
    if (after != NULL) {
        memset(after, 0x5A, 64);
        check(after[0] == 0x5A && after[63] == 0x5A,
              "post-race block writable over full extent");
        mimir_free(after);
    }
    printf("E008-THREADS %d/%d checks passed\n", passed, passed + failed);
    printf("RESULT: %s\n", failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}
