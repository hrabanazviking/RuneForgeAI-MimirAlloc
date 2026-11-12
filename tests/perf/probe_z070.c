/* probe_z070.c -- Z-070 double-free detection economics
 * probe (docs/MIMIRALLOC_Z13_ROADMAP.md slice Z-070).
 *
 * One binary per build leg (the runner compiles the
 * core five ways: off / off2 (A/A control) / member-only
 * / poison-only / full). Modes:
 *   churn1 C       single-block LIFO alloc/free pairs,
 *                  same thread (the TLS course: bin scan
 *                  + poison fill/verify live here)
 *   churnN C N     N blocks live, freed LIFO, repeated
 *                  (bin batches drain to the central
 *                  free list as they fill)
 *   churnF C N     N blocks live, freed FIFO, repeated
 *                  (membership at depth: frees land on
 *                  already-long lists)
 *   xfree C N      N blocks allocated in main, freed by
 *                  a worker thread (the central course:
 *                  the J-006 free-list walk lives here
 *                  in EVERY leg -- it predates Z-070)
 * Output: one RESULT line per invocation with ns per
 * alloc+free pair and a content checksum that is equal
 * across legs by construction (patterns depend only on
 * indices, never on addresses), so the runner's
 * checksum-equality check proves workload equivalence.
 * No corruption is injected here: detection must never
 * fire in a perf run.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#include "mimir_alloc.h"

static uint64_t checksum;

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void touch(void *p, size_t class_size, unsigned seed)
{
    unsigned char *b = (unsigned char *)p;
    b[0] = (unsigned char)seed;
    b[class_size - 1] = (unsigned char)(seed ^ 0xFFu);
    checksum += b[0] + b[class_size - 1];
}

static int churn1(size_t class_size, long iters)
{
    long i;
    double t0 = now_s();
    for (i = 0; i < iters; i++) {
        void *p = mimir_malloc(class_size);
        if (p == NULL) {
            return 2;
        }
        touch(p, class_size, (unsigned)i);
        mimir_free(p);
    }
    double dt = now_s() - t0;
    printf("RESULT mode=churn1 class=%zu n=1 pairs=%ld"
           " ns_per_pair=%.3f checksum=%llu\n",
           class_size, iters, dt * 1e9 / (double)iters,
           (unsigned long long)checksum);
    return 0;
}

static int churnN(size_t class_size, long n, long rounds,
                  int fifo)
{
    void **ptrs = malloc(sizeof(void *) * (size_t)n);
    long r, i;
    double t0, dt;
    if (ptrs == NULL) {
        return 2;
    }
    t0 = now_s();
    for (r = 0; r < rounds; r++) {
        for (i = 0; i < n; i++) {
            ptrs[i] = mimir_malloc(class_size);
            if (ptrs[i] == NULL) {
                return 2;
            }
            touch(ptrs[i], class_size, (unsigned)i);
        }
        if (fifo) {
            for (i = 0; i < n; i++) {
                mimir_free(ptrs[i]);
            }
        } else {
            for (i = n - 1; i >= 0; i--) {
                mimir_free(ptrs[i]);
            }
        }
    }
    dt = now_s() - t0;
    printf("RESULT mode=%s class=%zu n=%ld pairs=%ld"
           " ns_per_pair=%.3f checksum=%llu\n",
           fifo ? "churnF" : "churnN", class_size, n,
           rounds * n, dt * 1e9 / ((double)rounds * (double)n),
           (unsigned long long)checksum);
    free(ptrs);
    return 0;
}

/* xfree: one persistent worker thread frees each round's
 * array (central course for every block). */
static sem_t g_go, g_done;
static void **g_ptrs;
static long g_n;
static int g_stop;

static void *xfree_worker(void *unused)
{
    long i;
    (void)unused;
    for (;;) {
        sem_wait(&g_go);
        if (g_stop) {
            return NULL;
        }
        for (i = 0; i < g_n; i++) {
            mimir_free(g_ptrs[i]);
        }
        sem_post(&g_done);
    }
}

static int xfree(size_t class_size, long n, long rounds)
{
    void **ptrs = malloc(sizeof(void *) * (size_t)n);
    pthread_t th;
    long r, i;
    double t0, dt;
    if (ptrs == NULL) {
        return 2;
    }
    sem_init(&g_go, 0, 0);
    sem_init(&g_done, 0, 0);
    g_ptrs = ptrs;
    g_n = n;
    g_stop = 0;
    if (pthread_create(&th, NULL, xfree_worker, NULL) != 0) {
        return 2;
    }
    t0 = now_s();
    for (r = 0; r < rounds; r++) {
        for (i = 0; i < n; i++) {
            ptrs[i] = mimir_malloc(class_size);
            if (ptrs[i] == NULL) {
                return 2;
            }
            touch(ptrs[i], class_size, (unsigned)i);
        }
        sem_post(&g_go);
        sem_wait(&g_done);
    }
    dt = now_s() - t0;
    g_stop = 1;
    sem_post(&g_go);
    pthread_join(th, NULL);
    printf("RESULT mode=xfree class=%zu n=%ld pairs=%ld"
           " ns_per_pair=%.3f checksum=%llu\n",
           class_size, n, rounds * n,
           dt * 1e9 / ((double)rounds * (double)n),
           (unsigned long long)checksum);
    free(ptrs);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: probe_z070 MODE CLASS"
                " [N] [ROUNDS]\n");
        return 2;
    }
    {
        const char *mode = argv[1];
        size_t class_size = (size_t)atol(argv[2]);
        long n = argc > 3 ? atol(argv[3]) : 1;
        long rounds = argc > 4 ? atol(argv[4]) : 2000;
        if (strcmp(mode, "churn1") == 0) {
            return churn1(class_size,
                          argc > 3 ? atol(argv[3]) : 100000);
        }
        if (strcmp(mode, "churnN") == 0) {
            return churnN(class_size, n, rounds, 0);
        }
        if (strcmp(mode, "churnF") == 0) {
            return churnN(class_size, n, rounds, 1);
        }
        if (strcmp(mode, "xfree") == 0) {
            return xfree(class_size, n, rounds);
        }
        fprintf(stderr, "unknown mode %s\n", mode);
        return 2;
    }
}
