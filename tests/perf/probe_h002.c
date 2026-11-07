/* probe_h002.c - H-002 Phase 8 measured baseline.
 * Cross-thread free behaviour + contention latency at
 * 1/2/4/8 threads, bounded, with independent data-pattern
 * checks: every block is filled with a seeded byte pattern
 * by its allocating thread and verified byte-for-byte by
 * the (different) freeing thread before mimir_free.
 * Phase A: cross-thread frees (worker t frees the blocks
 * allocated by worker (t+1) mod T; T==1 is the degenerate
 * self-free control). Phase B: contention - every worker
 * alloc/fill/verify/free its own blocks against the one
 * global lock. Sizes mix the span path (64/256) and the
 * mapping path (2048). Exit 0 iff zero failures. */
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mimir_alloc.h"

#define MAX_THREADS 8
#define ITERS 1500

static void *g_ptrs[MAX_THREADS][ITERS];
static size_t g_sizes[MAX_THREADS][ITERS];
static pthread_barrier_t g_bar;
static long g_pat_fail[MAX_THREADS];
static long g_null_fail[MAX_THREADS];

static size_t pick_size(int i) {
    if (i % 3 == 0) return 64;
    if (i % 3 == 1) return 256;
    return 2048;
}
static uint8_t pat_byte(int tid, int i) {
    return (uint8_t)(0xA5u ^ (unsigned)(tid * 31) ^ (unsigned)i);
}
static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

struct arg { int tid; int nthreads; long long cross_ns; long long cont_ns; };

static void *worker(void *p) {
    struct arg *a = (struct arg *)p;
    int t = a->tid;
    for (int i = 0; i < ITERS; i++) {
        size_t sz = pick_size(i);
        void *ptr = mimir_malloc(sz);
        g_ptrs[t][i] = ptr; g_sizes[t][i] = sz;
        if (!ptr) { g_null_fail[t]++; continue; }
        memset(ptr, pat_byte(t, i), sz);
    }
    pthread_barrier_wait(&g_bar);
    long long s = now_ns();
    int src = (t + 1) % a->nthreads;
    for (int i = 0; i < ITERS; i++) {
        void *ptr = g_ptrs[src][i];
        if (!ptr) continue;
        size_t sz = g_sizes[src][i];
        const uint8_t *b = (const uint8_t *)ptr;
        for (size_t k = 0; k < sz; k++)
            if (b[k] != pat_byte(src, i)) { g_pat_fail[t]++; break; }
        mimir_free(ptr);
    }
    a->cross_ns = now_ns() - s;
    pthread_barrier_wait(&g_bar);
    s = now_ns();
    for (int i = 0; i < ITERS; i++) {
        size_t sz = (i % 10 == 9) ? 2048 : 64;
        void *ptr = mimir_malloc(sz);
        if (!ptr) { g_null_fail[t]++; continue; }
        memset(ptr, pat_byte(t, i), sz);
        const uint8_t *b = (const uint8_t *)ptr;
        for (size_t k = 0; k < sz; k++)
            if (b[k] != pat_byte(t, i)) { g_pat_fail[t]++; break; }
        mimir_free(ptr);
    }
    a->cont_ns = now_ns() - s;
    return NULL;
}

static int run_config(int nthreads) {
    pthread_t th[MAX_THREADS];
    struct arg args[MAX_THREADS];
    memset(g_pat_fail, 0, sizeof g_pat_fail);
    memset(g_null_fail, 0, sizeof g_null_fail);
    pthread_barrier_init(&g_bar, NULL, (unsigned)nthreads);
    long long wall0 = now_ns();
    for (int t = 0; t < nthreads; t++) {
        args[t].tid = t; args[t].nthreads = nthreads;
        args[t].cross_ns = 0; args[t].cont_ns = 0;
        if (pthread_create(&th[t], NULL, worker, &args[t]) != 0) {
            printf("CONFIG threads=%d CREATE_FAIL\n", nthreads);
            return 1;
        }
    }
    for (int t = 0; t < nthreads; t++) pthread_join(th[t], NULL);
    long long wall = now_ns() - wall0;
    pthread_barrier_destroy(&g_bar);
    long pf = 0, nf = 0; long long cmax = 0, ksum = 0;
    for (int t = 0; t < nthreads; t++) {
        pf += g_pat_fail[t]; nf += g_null_fail[t];
        if (args[t].cross_ns > cmax) cmax = args[t].cross_ns;
        ksum += args[t].cont_ns;
    }
    long ops = (long)nthreads * ITERS;
    printf("CONFIG threads=%d cross_ops=%ld pattern_fail=%ld null_fail=%ld "
           "cross_max_ns=%lld contention_ops=%ld contention_sum_ns=%lld "
           "contention_ns_per_op=%lld wall_ns=%lld %s\n",
           nthreads, ops, pf, nf, cmax, ops, ksum,
           ops ? ksum / ops : 0, wall,
           (pf == 0 && nf == 0) ? "OK" : "FAIL");
    return (pf == 0 && nf == 0) ? 0 : 1;
}

int main(void) {
    int rc = 0;
    const int cfgs[4] = {1, 2, 4, 8};
    for (int i = 0; i < 4; i++) rc |= run_config(cfgs[i]);
    printf("RESULT: %s\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}
