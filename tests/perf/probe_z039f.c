/* probe_z039f.c - Z-039 follow-up ladder + shard correctness,
 * MimirAlloc Z13, Wave 5.
 *
 * The geometry-decision follow-up probe: the Z-038 ladder
 * machinery (probe_z038.c) carried over UNCHANGED in shape -
 * same workloads (bench_k002.c included unmodified), same
 * rungs 1 2 3 4 6 8 12 16 20 24 28 32, same per-thread
 * timing window, identity pinning, checksum discipline
 * (small_churn chk_each 6371544, mixed_batch 6627200) - for
 * the two legs the Z-039 decision names:
 *   banked  : the banked core, this tree built flag-off (the
 *             run_z039f.sh zero-cost proof shows the flag-off
 *             objects byte-identical to the seat tree, so
 *             this leg IS the banked allocator).
 *   sharded : geometry B, the MIMIR_LOCK_SHARD build of this
 *             tree (core + platform with -DMIMIR_LOCK_SHARD=1,
 *             shared API object). The #error guard below
 *             refuses a sharded-labelled build WITHOUT the
 *             flag, so the banked core can never be measured
 *             under the sharded name.
 *
 * Extra phase (sharded leg only):
 *   shardcheck
 *       The shard-map assertions of the Z-039 paper design
 *       (class 0 -> shard 0, class 1 -> shard 1, classes
 *       2..3 -> shard 2, classes 4..19 -> shard 3), an
 *       8-thread cross-shard storm (all 20 classes, cross-
 *       thread frees through a single-writer/single-reader
 *       handoff ring, cross-shard realloc moves), and the
 *       closing audit read: every shard acquisition counter
 *       nonzero and the platform lock-order audit at zero
 *       violations. In a non-sharded build the phase prints
 *       its refusal and exits 3.
 *
 * Phases (argv[1]): selfcheck | shardcheck |
 *   ladder <workload> <threads> [iters]
 *
 * LADDER line (stdout, identical format to Z-020/Z-021/Z-038):
 *   LADDER,workload,threads,iters_per_thread,ops_per_thread,
 *          total_ops,elapsed_ns,agg_ops_per_sec,
 *          per_thread_ops_per_sec,chk_each,checksum
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>

#ifdef LEG_SHARDED
/* Sharded leg: built with the landed Z-039 follow-up flag
 * MIMIR_LOCK_SHARD (core + platform -DMIMIR_LOCK_SHARD=1 per
 * tests/perf/run_z039f.sh; the public API object is shared).
 * The ladder below uses only the public mimir_malloc/
 * mimir_free surface, exactly like the banked leg. The guard
 * refuses a sharded-labelled binary built WITHOUT the flag,
 * so the banked core can never be measured under the
 * sharded name. */
#ifndef MIMIR_LOCK_SHARD
#error "Z-039F sharded leg refused: LEG_SHARDED without MIMIR_LOCK_SHARD would measure the banked core under the sharded name. Build core/platform with -DMIMIR_LOCK_SHARD=1 (the landed Z-039 follow-up flag)."
#endif
#endif

/* _GNU_SOURCE already pulled in features.h, which defines
 * _POSIX_C_SOURCE; bench_k002.c defines it again and -Werror
 * would fail the build. Undef it: features.h is already
 * processed, so nothing functional depends on the value. */
#undef _POSIX_C_SOURCE
#define main z038_renamed_k002_main
#include "bench_k002.c"
#undef main

/* ---- thread-local mixed_batch replica (see header) ---- */
static uint64_t w_mixed_batch_mt(long batches, uint64_t *ops_out)
{
    static __thread void *ptrs[BATCH];
    uint64_t chk = 0;
    long b;
    int i;
    for (b = 0; b < batches; b++) {
        for (i = 0; i < BATCH; i++) {
            size_t n = mixed_sizes[(i + (int)b) % 20];
            ptrs[i] = alloc_fn(n);
            if (!ptrs[i]) { fprintf(stderr, "FAIL mixed_batch null\n"); exit(1); }
            chk += touch(ptrs[i], n, (unsigned)(b + i));
        }
        for (i = BATCH - 1; i >= 0; i--) free_fn(ptrs[i]);
    }
    *ops_out = (uint64_t)batches * BATCH;
    return chk;
}

/* ---- ladder machinery ---- */
#define MAX_THREADS 32
static pthread_barrier_t bar;
static const char *cur_workload;
static long cur_iters;
static uint64_t thread_chk[MAX_THREADS];
static uint64_t thread_ops[MAX_THREADS];
static uint64_t thread_start[MAX_THREADS];
static uint64_t thread_end[MAX_THREADS];

struct targ { int idx; int cpu; };

static void *worker(void *p)
{
    struct targ *a = (struct targ *)p;
    cpu_set_t s;
    uint64_t ops = 0, chk;
    CPU_ZERO(&s);
    CPU_SET(a->cpu, &s);
    if (sched_setaffinity(0, sizeof(s), &s) != 0) {
        perror("sched_setaffinity");
        exit(3);
    }
    if (sched_getcpu() != a->cpu) {
        fprintf(stderr, "PIN MISMATCH thread=%d want cpu=%d got=%d\n",
                a->idx, a->cpu, sched_getcpu());
        exit(3);
    }
    /* Untimed warm-up at 1/10 scale (K-002 discipline). */
    if (strcmp(cur_workload, "small_churn") == 0)
        (void)w_small_churn(alloc_fn, free_fn, cur_iters / 10, &ops);
    else
        (void)w_mixed_batch_mt(cur_iters / 10, &ops);
    pthread_barrier_wait(&bar);
    thread_start[a->idx] = now_ns();
    if (strcmp(cur_workload, "small_churn") == 0)
        chk = w_small_churn(alloc_fn, free_fn, cur_iters, &ops);
    else
        chk = w_mixed_batch_mt(cur_iters, &ops);
    thread_end[a->idx] = now_ns();
    thread_chk[a->idx] = chk;
    thread_ops[a->idx] = ops;
    return NULL;
}

static int run_ladder(const char *workload, int nthreads, long iters)
{
    pthread_t th[MAX_THREADS];
    struct targ args[MAX_THREADS];
    uint64_t t0, t1, elapsed, total_ops = 0;
    int i;
    cur_workload = workload;
    cur_iters = iters;
    if (pthread_barrier_init(&bar, NULL, (unsigned)nthreads + 1) != 0) {
        fprintf(stderr, "barrier init failed\n");
        return 3;
    }
    for (i = 0; i < nthreads; i++) {
        args[i].idx = i;
        args[i].cpu = i; /* identity order: thread t -> CPU t */
        thread_chk[i] = 0;
        thread_ops[i] = 0;
        if (pthread_create(&th[i], NULL, worker, &args[i]) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            return 3;
        }
    }
    pthread_barrier_wait(&bar);
    for (i = 0; i < nthreads; i++) pthread_join(th[i], NULL);
    pthread_barrier_destroy(&bar);
    t0 = thread_start[0];
    t1 = thread_end[0];
    for (i = 1; i < nthreads; i++) {
        if (thread_start[i] < t0) t0 = thread_start[i];
        if (thread_end[i] > t1) t1 = thread_end[i];
    }
    elapsed = t1 - t0;
    for (i = 0; i < nthreads; i++) {
        if (thread_chk[i] != thread_chk[0]) {
            fprintf(stderr,
                    "CHECKSUM MISMATCH workload=%s threads=%d thread=%d got=%llu want=%llu\n",
                    workload, nthreads, i,
                    (unsigned long long)thread_chk[i],
                    (unsigned long long)thread_chk[0]);
            return 3;
        }
        total_ops += thread_ops[i];
    }
    printf("LADDER,%s,%d,%ld,%llu,%llu,%llu,%.1f,%.1f,%llu,%llu\n",
           workload, nthreads, iters,
           (unsigned long long)thread_ops[0],
           (unsigned long long)total_ops,
           (unsigned long long)elapsed,
           (double)total_ops * 1e9 / (double)elapsed,
           (double)total_ops * 1e9 / (double)elapsed / (double)nthreads,
           (unsigned long long)thread_chk[0],
           (unsigned long long)thread_chk[0] * (uint64_t)nthreads);
    fflush(stdout);
    return 0;
}

static int run_selfcheck(void)
{
    uint64_t ops_b = 0, ops_r = 0, ops_s = 0, ops_m = 0;
    uint64_t chk_b, chk_r, chk_s, chk_m;
    chk_b = w_mixed_batch(alloc_fn, free_fn, 37, &ops_b);
    chk_r = w_mixed_batch_mt(37, &ops_r);
    printf("SELFCHECK mixed_batch_replica batches=37 bench_ops=%llu replica_ops=%llu bench_checksum=%llu replica_checksum=%llu %s\n",
           (unsigned long long)ops_b, (unsigned long long)ops_r,
           (unsigned long long)chk_b, (unsigned long long)chk_r,
           (chk_b == chk_r && ops_b == ops_r) ? "MATCH" : "MISMATCH");
    if (chk_b != chk_r || ops_b != ops_r) return 3;
    chk_s = w_small_churn(alloc_fn, free_fn, 50000, &ops_s);
    printf("SELFCHECK small_churn_ref iters=50000 ops=%llu checksum=%llu\n",
           (unsigned long long)ops_s, (unsigned long long)chk_s);
    chk_m = w_mixed_batch_mt(200, &ops_m);
    printf("SELFCHECK mixed_batch_ref batches=200 ops=%llu checksum=%llu\n",
           (unsigned long long)ops_m, (unsigned long long)chk_m);
    printf("SELFCHECK DONE Z-039F leg=%s\n", leg_name);
    return 0;
}

/* ---- Z-039F shard correctness phase (sharded leg only) - */
#ifdef LEG_SHARDED
#include "../core/mimir_shard_z039.h"

#define SC_THREADS 8
#define SC_ITERS 20000
static void *sc_handoff[SC_THREADS];
static pthread_barrier_t sc_bar;
static const size_t sc_sizes[20] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};

static void *sc_worker(void *p)
{
    long idx = (long)p;
    uint64_t rng = 0x9e3779b97f4a7c15ULL * (uint64_t)(idx + 1);
    long i;
    pthread_barrier_wait(&sc_bar);
    for (i = 0; i < SC_ITERS; i++) {
        size_t n;
        void *ptr;
        /* Consume the previous thread handoff FIRST (the
         * ring protocol never waits: a writer whose slot is
         * still full frees its block itself, so no thread can
         * ever block on another). */
        {
            long prev = (idx + SC_THREADS - 1) % SC_THREADS;
            void *hp = sc_handoff[prev];
            if (hp != NULL) { sc_handoff[prev] = NULL; free_fn(hp); }
        }
        rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
        n = sc_sizes[rng % 20];
        ptr = alloc_fn(n);
        if (!ptr) { fprintf(stderr, "SHARDCHECK alloc null\n"); exit(1); }
        memset(ptr, (int)(idx + 1), n < 64 ? n : 64);
        if (i % 7 == 3) {
            size_t m;
            void *ptr2;
            rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
            m = sc_sizes[rng % 20];
            ptr2 = mimir_realloc(ptr, m);
            if (!ptr2) { fprintf(stderr, "SHARDCHECK realloc null\n"); exit(1); }
            ptr = ptr2;
        }
        if (i % 11 == 5 && sc_handoff[idx] == NULL) {
            sc_handoff[idx] = ptr;
        } else {
            free_fn(ptr);
        }
    }
    pthread_barrier_wait(&sc_bar);
    if (sc_handoff[idx] != NULL) {
        void *hp = sc_handoff[idx];
        sc_handoff[idx] = NULL;
        free_fn(hp);
    }
    mimir_tls_cache_drain_thread();
    return NULL;
}

static int run_shardcheck(void)
{
    struct mimir_shard_snapshot snap;
    pthread_t th[SC_THREADS];
    long i;
    int fails = 0;
    size_t m0 = mimir_shard_of_class(0);
    size_t m1 = mimir_shard_of_class(1);
    size_t m2 = mimir_shard_of_class(2);
    size_t m3 = mimir_shard_of_class(3);
    size_t m4 = mimir_shard_of_class(4);
    size_t m19 = mimir_shard_of_class(19);
    if (m0 != 0 || m1 != 1 || m2 != 2 || m3 != 2 || m4 != 3 || m19 != 3) fails++;
    printf("SHARDCHECK map class0=%zu class1=%zu class2=%zu class3=%zu class4=%zu class19=%zu %s\n",
           m0, m1, m2, m3, m4, m19, fails ? "FAIL" : "OK");
    if (pthread_barrier_init(&sc_bar, NULL, SC_THREADS) != 0) return 3;
    for (i = 0; i < SC_THREADS; i++)
        if (pthread_create(&th[i], NULL, sc_worker, (void *)i) != 0) return 3;
    for (i = 0; i < SC_THREADS; i++) pthread_join(th[i], NULL);
    pthread_barrier_destroy(&sc_bar);
    if (mimir_shard_snapshot(&snap) != 0) {
        printf("SHARDCHECK snapshot FAILED\n");
        return 3;
    }
    printf("SHARDCHECK acquisitions s0=%llu s1=%llu s2=%llu s3=%llu order_violations=%llu\n",
           (unsigned long long)snap.acquisitions[0],
           (unsigned long long)snap.acquisitions[1],
           (unsigned long long)snap.acquisitions[2],
           (unsigned long long)snap.acquisitions[3],
           (unsigned long long)snap.order_violations);
    for (i = 0; i < 4; i++)
        if (snap.acquisitions[i] == 0) fails++;
    if (snap.order_violations != 0) fails++;
    printf("SHARDCHECK %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
#else
static int run_shardcheck(void)
{
    printf("SHARDCHECK REFUSED: this binary is not the sharded leg\n");
    return 3;
}
#endif

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "selfcheck") == 0)
        return run_selfcheck();
    if (argc >= 2 && strcmp(argv[1], "shardcheck") == 0)
        return run_shardcheck();
    if (argc >= 4 && strcmp(argv[1], "ladder") == 0) {
        const char *wl = argv[2];
        int nthreads = atoi(argv[3]);
        long iters;
        if (strcmp(wl, "small_churn") != 0 &&
            strcmp(wl, "mixed_batch") != 0) {
            fprintf(stderr, "unknown workload %s\n", wl);
            return 2;
        }
        if (nthreads < 1 || nthreads > MAX_THREADS) {
            fprintf(stderr, "threads out of range\n");
            return 2;
        }
        iters = (argc >= 5) ? atol(argv[4])
                            : (strcmp(wl, "small_churn") == 0 ? 50000 : 200);
        return run_ladder(wl, nthreads, iters);
    }
    fprintf(stderr, "usage: probe_z039f selfcheck | shardcheck | ladder <workload> <threads> [iters]\n");
    return 2;
}
