/* probe_z038.c - Z-038 Arena ladder, MimirAlloc Z13, Wave 5.
 *
 * The three-way contention ladder: the Z-020 (banked allocator)
 * / Z-021 (glibc comparator) ladder pair re-run under ONE probe,
 * ONE timing window and ONE session discipline for the three
 * legs the roadmap names:
 *   banked : the current banked core (seat main, banked
 *            Z-001..Z-035 at build time - a NEWER core than the
 *            one Z-020 measured: Wave-4 TLS cache, size-class
 *            retune and metadata work are inside it). Built
 *            default (bench_k002.c binds mimir_malloc/mimir_free).
 *   glibc  : built with -DLEG_GLIBC (bench_k002.c's own leg
 *            switch binds host malloc/free).
 *   arena  : the Z-037 per-CCD arena variant (2 arenas, Z-012
 *            constants, build-time flag MIMIR_ARENA_CCD). At the
 *            first Z-038 session Z-037 had NOT landed, so this
 *            leg was OWED, not measured. It was enabled on the
 *            merged fixed tree (seat d0185ab, Z-037 + the Z-029
 *            TLS fix) for the completion re-run: the #error
 *            guard below now refuses an arena-labelled build
 *            WITHOUT the landed flag rather than refusing the
 *            leg outright, so the banked core still can never
 *            be measured under the arena name.
 *
 * No Z-020 or Z-021 number is copied into the Z-038 table: the
 * banked and glibc legs are re-measured fresh in the Z-038
 * session. The ladder shape is frozen identical to Z-020/Z-021
 * so the arena re-run, when it lands, is directly comparable:
 *   workloads : small_churn, mixed_batch (banked K-002 workloads,
 *               bench_k002.c included UNMODIFIED below, its
 *               main() renamed away)
 *   rungs     : 1 2 3 4 6 8 12 16 20 24 28 32 threads
 *   scales    : fixed work PER THREAD - 50000 small_churn
 *               iterations / 200 mixed_batch batches (51200 ops);
 *               the scale is printed on every LADDER line
 *   reps      : separate PROCESSES, driven by run_z038.sh
 *               (default 5 per ladder point, headline = median
 *               of processes, summarize_z038.py), plus one
 *               discarded warm-up process per point at 1/10
 *               scale and the probe's own in-process 1/10
 *               warm-up pass per worker
 *   pauses    : 2 s between ladder points (runner; Z-004
 *               protocol section 10 thermal honesty)
 * Threading model (identical to Z-021):
 *   - Every worker runs an untimed warm-up at 1/10 scale, then
 *     all workers + main rendezvous on a barrier and run the
 *     timed pass simultaneously. Timing is PER-THREAD (the
 *     Z-021 window, adopted for all Z-038 legs): each worker
 *     stamps CLOCK_MONOTONIC (bench now_ns()) right after the
 *     barrier releases and again at the end of its timed pass;
 *     the point's elapsed is max(end) - min(start) across
 *     workers. Z-020's main-thread release..last-join window is
 *     NOT used: Z-021 proved it can truncate (impossible reps
 *     up to 13.6 Gops/s) when the unpinned main thread wakes
 *     late. This is the errata Z-021 recorded for this slice.
 *   - Pinning (Z-004 protocol section 3): worker t pins ITSELF
 *     to CPU t, identity order 0-15 = physical cores (0-7 CCD0,
 *     8-15 CCD1), 16-31 = SMT siblings (16-23 CCD0, 24-31
 *     CCD1). The pin is verified with sched_getcpu(); a
 *     mismatch aborts. Threads 17..32 engage SMT siblings BY
 *     DESIGN - the ladder's x-axis is thread count on this
 *     16-core host.
 *   - mixed_batch caveat: the bench's w_mixed_batch keeps its
 *     pointer array in a static, so it is NOT thread-safe. This
 *     probe runs a thread-local replica (own array, same size
 *     cycle mixed_sizes[(i+b)%20], same touch seeds (b+i), same
 *     reverse-order frees). The selfcheck phase proves the
 *     replica's checksum and op count identical to the bench
 *     function's for the same batch count, in the same process,
 *     before any ladder number is taken.
 *   - Checksum discipline (K-002): every worker runs identical
 *     work, so every per-thread checksum must be EQUAL; the
 *     probe aborts (exit 3) if they differ, and prints both
 *     chk_each and checksum = threads * chk_each. Ladder-scale
 *     references (from Z-020/Z-021, same bench): small_churn
 *     chk_each 6371544, mixed_batch chk_each 6627200 - the
 *     summariser verifies every point against the selfcheck
 *     references printed by THIS binary in THIS session.
 *
 * Phases (argv[1]):
 *   selfcheck
 *       mixed_batch replica vs bench function at 37 batches
 *       (odd count, catches order slips), plus the bench
 *       reference checksums at the ladder scales (small_churn
 *       at 50000 iterations, mixed_batch replica at 200
 *       batches).
 *   ladder <workload> <threads> [iters]
 *       One ladder point. iters = per-thread iterations for
 *       small_churn, per-thread BATCHES for mixed_batch.
 *
 * LADDER line (stdout, identical format to Z-020/Z-021):
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

#ifdef LEG_ARENA
/* Arena leg enabled 2026-11-12 on the merged fixed tree (seat
 * d0185ab): Z-037 has landed with the build-time flag
 * MIMIR_ARENA_CCD (core + platform built with
 * -DMIMIR_ARENA_CCD=1 per tests/perf/run_z037.sh; the public
 * API object is shared). The ladder below uses only the public
 * mimir_malloc/mimir_free surface, exactly like the banked leg,
 * so no arena header is needed here. The guard is kept in the
 * opposite direction: an arena-labelled binary built WITHOUT
 * the landed flag is refused, so the banked core can never be
 * measured under the arena name. */
#ifndef MIMIR_ARENA_CCD
#error "Z-038 arena leg refused: LEG_ARENA without MIMIR_ARENA_CCD would measure the banked core under the arena name. Build core/platform with -DMIMIR_ARENA_CCD=1 (the landed Z-037 flag)."
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
    printf("SELFCHECK DONE Z-038 leg=%s\n", leg_name);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "selfcheck") == 0)
        return run_selfcheck();
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
    fprintf(stderr, "usage: probe_z038 selfcheck | ladder <workload> <threads> [iters]\n");
    return 2;
}
