/* probe_z021.c - Z-021 The contention ladder (glibc comparator),
 * MimirAlloc Z13, Wave 3.
 *
 * The glibc leg of the Z-020/Z-021 contention ladder pair. Team-06's
 * Z-020 probe (tests/perf/probe_z020.c, banked allocator leg) was
 * designed for exact reuse here - its header states: "Z-021 reuses
 * this exact probe built with -DLEG_GLIBC for the glibc comparator
 * leg (bench_k002.c's own leg switch)." This probe is that reuse:
 * the same ladder shape, the same phases, the same LADDER line
 * format, built with -DLEG_GLIBC so the included bench_k002.c
 * binds malloc/free. No Z-020 number is copied; every number in
 * the Z-021 recorded run is measured by this binary on this leg.
 *
 * Ladder shape (identical to Z-020, rung for rung):
 *   workloads : small_churn, mixed_batch (banked K-002 workloads,
 *               bench_k002.c included UNMODIFIED below, its
 *               main() renamed away)
 *   rungs     : 1 2 3 4 6 8 12 16 20 24 28 32 threads
 *   scales    : fixed work PER THREAD - 50000 small_churn
 *               iterations / 200 mixed_batch batches (51200 ops);
 *               the scale is printed on every LADDER line
 *   reps      : separate PROCESSES, driven by run_z021.sh
 *               (default 5 per ladder point, headline = median
 *               of processes, summarize_z021.py), plus one
 *               discarded warm-up process per point at 1/10
 *               scale and the probe's own in-process 1/10
 *               warm-up pass per worker
 *   pauses    : 2 s between ladder points (runner; Z-004
 *               protocol section 10 thermal honesty)
 * Threading model (identical to Z-020):
 *   - Every worker runs an untimed warm-up at 1/10 scale, then
 *     all workers + main rendezvous on a barrier and run the
 *     timed pass simultaneously. Timing is PER-THREAD: each
 *     worker stamps CLOCK_MONOTONIC (bench now_ns()) right
 *     after the barrier releases and again at the end of its
 *     timed pass; the point's elapsed is max(end) - min(start)
 *     across workers. DEVIATION FROM Z-020, WITH CAUSE: Z-020
 *     times release..last-join on the MAIN thread; the first
 *     aligned run of this probe (hidden_files/z021_run.log,
 *     superseded) showed correct-checksum reps with impossible
 *     aggregate throughput (up to 13.6 Gops/s at 28 threads),
 *     because an unpinned main thread waking late from the
 *     barrier truncates the window while the workers' work is
 *     already done. Per-thread stamps make the window immune
 *     to main-thread wake latency; pins, rungs, scales, reps
 *     and line format are unchanged, and chk_each equality
 *     proves the same work is timed. The observation is
 *     recorded for the Z-020 pairing in CAMPAIGN_STATE.md.
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
 *     chk_each and checksum = threads * chk_each.
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
 * LADDER line (stdout, identical format to Z-020):
 *   LADDER,workload,threads,iters_per_thread,ops_per_thread,
 *          total_ops,elapsed_ns,agg_ops_per_sec,
 *          per_thread_ops_per_sec,chk_each,checksum
 *
 * History: an earlier draft of this slice used a different
 * shape (rungs 1/2/4/8/16/32, in-process reps, K-002 single-
 * thread scales raised 100x/50x after sub-millisecond cells
 * proved jitter-dominated). Those attempts are preserved in
 * the team's hidden_files (z021_run_attempt1..3.log) and were
 * superseded the moment team-06's Z-020 probe landed with the
 * shared shape above: rung-for-rung comparability is the
 * gate, so the glibc leg follows the banked leg's shape
 * exactly, jitter discipline included (median of 5 processes,
 * warm-up process per point, pauses).
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>

/* _GNU_SOURCE already pulled in features.h, which defines
 * _POSIX_C_SOURCE; bench_k002.c defines it again and -Werror
 * would fail the build. Undef it: features.h is already
 * processed, so nothing functional depends on the value. */
#undef _POSIX_C_SOURCE
#define main z021_renamed_k002_main
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
    printf("SELFCHECK DONE Z-021 leg=glibc\n");
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
    fprintf(stderr, "usage: probe_z021 selfcheck | ladder <workload> <threads> [iters]\n");
    return 2;
}
