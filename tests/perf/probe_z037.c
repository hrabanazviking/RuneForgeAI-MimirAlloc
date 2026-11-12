/* probe_z037.c - Z-037 arena prototype functional proof.
 *
 * Modes:
 *   place <cpu> <size> <count>
 *     Pin this thread to <cpu>, drain the TLS cache, then
 *     allocate <count> blocks of <size> through the public
 *     API and print the arena snapshot. In a fresh process
 *     every live object must sit in the arena of <cpu>'s
 *     CCD (the Z-012 map), and no span may exist in the
 *     other arena.
 *   crossfree
 *     Allocate on CCD0 (pinned cpu 2), hand the blocks to a
 *     thread pinned to CCD1 (cpu 10) which frees them all;
 *     then verify the stats surface reports zero live small
 *     objects and the J-006 metadata check is clean.
 *   storm
 *     16 worker threads, half pinned to CCD0 physical cores
 *     and half to CCD1, each churning alloc/fill/verify/free
 *     over mixed small classes; every block's fill pattern
 *     is verified before free. Prints a checksum; exits
 *     nonzero on any mismatch, NULL, or metadata violation.
 *
 * Exit 0 and a trailing "probe_result: OK" line mean every
 * assertion in the mode held; anything else is a failure.
 */
#define _GNU_SOURCE 1
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mimir_alloc.h"
#include "../core/mimir_arena_z037.h"
#include "../core/mimir_stats_j002.h"
#include "../contract/mimir_tls_cache_z028.h"

static int pin_cpu(int cpu)
{
    cpu_set_t set;

    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof set,
                                  &set) == 0 ? 0 : -1;
}

static int mode_place(int cpu, size_t size, size_t count)
{
    struct mimir_arena_snapshot snap;
    size_t expect = (size_t)(((cpu % 16) >= 8) ? 1 : 0);
    size_t other = 1 - expect;
    void **ptrs;
    size_t i;

    if (pin_cpu(cpu) != 0) {
        printf("place: pin to cpu %d FAILED\n", cpu);
        return 1;
    }
    if (mimir_arena_current() != expect) {
        printf("place: arena_current=%zu expect=%zu FAIL\n",
               mimir_arena_current(), expect);
        return 1;
    }
    mimir_tls_cache_drain_thread();
    ptrs = calloc(count, sizeof *ptrs);
    if (ptrs == NULL) {
        return 1;
    }
    for (i = 0; i < count; i++) {
        ptrs[i] = mimir_malloc(size);
        if (ptrs[i] == NULL) {
            printf("place: NULL at %zu FAIL\n", i);
            return 1;
        }
        memset(ptrs[i], 0xA5, size);
    }
    /* Z-029 accounting: bin publications are tallied per
     * thread and flushed to the core counters at the
     * owning thread's next central transaction. Drain the
     * cache so every tally is flushed before reading the
     * core-level span counts; the user blocks stay live. */
    mimir_tls_cache_drain_thread();
    if (mimir_arena_snapshot(&snap) != 0) {
        printf("place: snapshot FAILED\n");
        return 1;
    }
    printf("place cpu=%d size=%zu count=%zu expect_arena=%zu "
           "spans=%llu/%llu live=%llu/%llu sel=%llu/%llu\n",
           cpu, size, count, expect,
           (unsigned long long)snap.spans[0],
           (unsigned long long)snap.spans[1],
           (unsigned long long)snap.live_objects[0],
           (unsigned long long)snap.live_objects[1],
           (unsigned long long)snap.selections[0],
           (unsigned long long)snap.selections[1]);
    if (snap.live_objects[expect] != (uint64_t)count ||
        snap.live_objects[other] != 0 || snap.spans[other] != 0 ||
        snap.selections[expect] == 0 || snap.selections[other] != 0) {
        printf("place: PLACEMENT FAIL\n");
        return 1;
    }
    for (i = 0; i < count; i++) {
        mimir_free(ptrs[i]);
    }
    mimir_tls_cache_drain_thread();
    if (mimir_core_metadata_check() != 0) {
        printf("place: metadata check FAIL\n");
        return 1;
    }
    free(ptrs);
    printf("probe_result: OK\n");
    return 0;
}

struct crossfree_job {
    void **ptrs;
    size_t count;
    int cpu;
    int rc;
};

static void *crossfree_worker(void *arg)
{
    struct crossfree_job *job = arg;
    size_t i;

    job->rc = pin_cpu(job->cpu);
    if (job->rc != 0) {
        return NULL;
    }
    for (i = 0; i < job->count; i++) {
        mimir_free(job->ptrs[i]);
    }
    return NULL;
}

static int g_acpu = 2;  /* crossfree: allocating CPU */
static int g_fcpu = 10; /* crossfree: freeing CPU */

static int mode_crossfree(void)
{
    enum { N = 512 };
    static void *ptrs[N];
    struct crossfree_job job;
    struct mimir_stats stats;
    pthread_t th;
    size_t i;

    if (pin_cpu(g_acpu) != 0) {
        printf("crossfree: pin FAILED\n");
        return 1;
    }
    mimir_tls_cache_drain_thread();
    for (i = 0; i < N; i++) {
        ptrs[i] = mimir_malloc(64);
        if (ptrs[i] == NULL) {
            printf("crossfree: NULL at %zu FAIL\n", i);
            return 1;
        }
        memset(ptrs[i], 0x5A, 64);
    }
    /* Quiesce this thread's Z-029 pending publication
     * tallies with a central transaction BEFORE the remote
     * frees, and prove the quiescent count is exact. (With
     * tallies still pending, a remote free leaves a
     * residual in live_small_objects even on the BANKED
     * core -- arena and production builds both read 6 on
     * the unquiesced sequence; that tally behaviour is
     * Z-029's, recorded in the slice's CAMPAIGN_STATE as
     * follow-up, and is not this prototype's to change.) */
    {
        struct mimir_stats pre;
        if (mimir_core_stats_snapshot(&pre) != 0 ||
            pre.live_small_objects != (uint64_t)N) {
            printf("crossfree: pre-free quiescent count FAIL "
                   "live=%llu\n",
                   (unsigned long long)pre.live_small_objects);
            return 1;
        }
    }
    /* The worker frees through the public API, whose TLS
     * path routes foreign blocks to the central core. */
    job.ptrs = ptrs;
    job.count = N;
    job.cpu = g_fcpu;
    job.rc = 0;
    if (pthread_create(&th, NULL, crossfree_worker, &job) != 0) {
        return 1;
    }
    (void)pthread_join(th, NULL);
    if (job.rc != 0) {
        printf("crossfree: worker pin FAIL\n");
        return 1;
    }
    /* Flush this thread's Z-029 pending publication
     * tallies (see place mode) before reading the core
     * counters: the worker already freed every block, and
     * the residual tally units are this thread's to flush. */
    {
        void *flush = mimir_malloc(64);
        mimir_free(flush);
        mimir_tls_cache_drain_thread();
    }
    if (mimir_core_stats_snapshot(&stats) != 0) {
        printf("crossfree: stats FAILED\n");
        return 1;
    }
    printf("crossfree live_small=%llu metadata=%d\n",
           (unsigned long long)stats.live_small_objects,
           mimir_core_metadata_check());
    if (stats.live_small_objects != 0 ||
        mimir_core_metadata_check() != 0) {
        printf("crossfree: FAIL\n");
        return 1;
    }
    printf("probe_result: OK\n");
    return 0;
}

struct storm_job {
    int cpu;
    int iters;
    uint64_t sum;
    int rc;
};

static void *storm_worker(void *arg)
{
    struct storm_job *job = arg;
    static const size_t sizes[6] = {16, 48, 64, 128, 256, 512};
    int it;

    job->rc = pin_cpu(job->cpu);
    if (job->rc != 0) {
        return NULL;
    }
    for (it = 0; it < job->iters; it++) {
        size_t sz = sizes[(size_t)(it + job->cpu) % 6];
        unsigned char *p = mimir_malloc(sz);
        size_t i;

        if (p == NULL) {
            job->rc = 2;
            return NULL;
        }
        memset(p, (int)(job->cpu + it), sz);
        for (i = 0; i < sz; i++) {
            if (p[i] != (unsigned char)(job->cpu + it)) {
                job->rc = 3;
                mimir_free(p);
                return NULL;
            }
            job->sum += p[i];
        }
        mimir_free(p);
    }
    return NULL;
}

static int mode_storm(void)
{
    enum { THREADS = 16, ITERS = 4000 };
    pthread_t th[THREADS];
    struct storm_job jobs[THREADS];
    uint64_t total = 0;
    int t;

    for (t = 0; t < THREADS; t++) {
        jobs[t].cpu = t; /* physical cores 0..15: 8 per CCD */
        jobs[t].iters = ITERS;
        jobs[t].sum = 0;
        jobs[t].rc = 0;
        if (pthread_create(&th[t], NULL, storm_worker,
                           &jobs[t]) != 0) {
            printf("storm: create %d FAILED\n", t);
            return 1;
        }
    }
    for (t = 0; t < THREADS; t++) {
        (void)pthread_join(th[t], NULL);
        if (jobs[t].rc != 0) {
            printf("storm: worker %d rc=%d FAIL\n", t, jobs[t].rc);
            return 1;
        }
        total += jobs[t].sum;
    }
    printf("storm threads=%d iters=%d checksum=%llu metadata=%d\n",
           THREADS, ITERS, (unsigned long long)total,
           mimir_core_metadata_check());
    if (mimir_core_metadata_check() != 0) {
        printf("storm: metadata FAIL\n");
        return 1;
    }
    printf("probe_result: OK\n");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "place") == 0 && argc == 5) {
        return mode_place(atoi(argv[2]),
                          (size_t)strtoul(argv[3], NULL, 10),
                          (size_t)strtoul(argv[4], NULL, 10));
    }
    if (argc >= 2 && strcmp(argv[1], "crossfree") == 0) {
        return mode_crossfree();
    }
    if (argc >= 2 && strcmp(argv[1], "crossfree-rev") == 0) {
        g_acpu = 10;
        g_fcpu = 2;
        return mode_crossfree();
    }
    if (argc >= 2 && strcmp(argv[1], "storm") == 0) {
        return mode_storm();
    }
    fprintf(stderr, "usage: probe_z037 place <cpu> <size> <count> "
                    "| crossfree | storm\n");
    return 2;
}
