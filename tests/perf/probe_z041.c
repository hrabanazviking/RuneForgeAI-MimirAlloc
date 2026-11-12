/* probe_z041.c - Z-041 SMT and pinning policy probe.
 *
 * Derived from the banked Z-020/Z-021 ladder probe (probe_z020.c,
 * included bench_k002.c UNMODIFIED, its main renamed away) with
 * one added axis: AFFINITY MODE. The ladder of Z-020/Z-021 was
 * pinned-only; this probe runs the identical fixed-work-per-thread
 * workloads (small_churn, mixed_batch, same scales 50000 / 200
 * batches = 51200 ops) in two modes:
 *   pinned   - worker t pins itself to CPU t (identity order:
 *              0..15 physical cores, 0-7 CCD0 and 8-15 CCD1;
 *              16..31 SMT siblings, 16-23 CCD0 and 24-31 CCD1),
 *              verified with sched_getcpu() before AND after the
 *              timed pass; a pinned thread that ends on another
 *              CPU fails the process.
 *   floating - no affinity call at all; the scheduler places and
 *              migrates threads freely inside the runner taskset
 *              0-31 bound. Each worker records sched_getcpu()
 *              before and after its timed pass; the LADDER line
 *              carries mig = workers whose end CPU differs from
 *              their start CPU (a floor on migration: a thread
 *              can migrate and return uncounted).
 * Thread counts of record: 8 (one CCD of physical cores, pinned),
 * 16 (all physical cores) and 32 (all logical CPUs, SMT engaged).
 * The 16-vs-32 pinned comparison isolates SMT; pinned-vs-floating
 * at equal thread count isolates scheduler placement.
 * Built twice by run_z041.sh: production core (mimir leg) and
 * -DLEG_GLIBC (glibc leg, bench_k002.c leg switch), identical
 * workload code in both legs.
 * Timing: PER-THREAD (the Z-021 window): each worker stamps
 * CLOCK_MONOTONIC (bench now_ns()) right after the barrier
 * releases and again at the end of its timed pass; the point
 * elapsed is max(end) - min(start) across workers. DEVIATION
 * FROM Z-020, WITH CAUSE (Z-021 precedent, re-proven here):
 * Z-020 times release..last-join on the MAIN thread; attempt 1
 * of this slice with that window produced correct-checksum
 * glibc reps up to 14.9 Gops/s and a 702% spread at 32 pinned
 * threads, because a main thread waking late from the barrier
 * (or starved by 32 pinned workers) truncates the window while
 * the work is already done (hidden_files/z041_attempt1_*.raw,
 * superseded, kept as method evidence). Per-thread stamps make
 * the window immune to main-thread wake latency. Every worker runs an
 * untimed 1/10 warm-up, then all workers plus main rendezvous on
 * a barrier and run the timed pass simultaneously. Checksum
 * discipline (K-002): every worker runs identical work, so all
 * per-thread checksums must be EQUAL; divergence aborts (exit 3).
 * mixed_batch runs the Z-020 thread-local replica (the bench
 * function keeps its pointer array in a static); the selfcheck
 * phase proves replica == bench before any number is taken.
 *
 * Phases (argv[1]):
 *   selfcheck
 *   ladder <workload> <threads> <pinned|floating> [iters]
 *   hist <threads> [iters]   (Z-020 carry-over; not used by
 *                             run_z041.sh; pinned mode only)
 *
 * LADDER line (stdout):
 *   LADDER,workload,threads,mode,iters_per_thread,ops_per_thread,
 *          total_ops,elapsed_ns,agg_ops_per_sec,
 *          per_thread_ops_per_sec,chk_each,checksum,mig
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
#define main z020_renamed_k002_main
#include "bench_k002.c"
#undef main

/* ---- thread-local mixed_batch replica (see header) ---- */
static uint64_t w_mixed_batch_local(alloc_t af, free_t ff, long batches,
                                    uint64_t *ops_out)
{
    void *ptrs[BATCH];
    uint64_t chk = 0;
    long b;
    int i;
    for (b = 0; b < batches; b++) {
        for (i = 0; i < BATCH; i++) {
            size_t n = mixed_sizes[(i + (int)b) % 20];
            ptrs[i] = af(n);
            if (!ptrs[i]) { fprintf(stderr, "FAIL mixed_batch null\n"); exit(1); }
            chk += touch(ptrs[i], n, (unsigned)(b + i));
        }
        for (i = BATCH - 1; i >= 0; i--) ff(ptrs[i]);
    }
    *ops_out = (uint64_t)batches * BATCH;
    return chk;
}

static const struct workload *find_workload(const char *name)
{
    size_t w;
    for (w = 0; w < sizeof(workloads) / sizeof(workloads[0]); w++)
        if (strcmp(name, workloads[w].name) == 0) return &workloads[w];
    return NULL;
}

static uint64_t run_one(const char *name, long iters, uint64_t *ops_out)
{
    if (strcmp(name, "mixed_batch") == 0)
        return w_mixed_batch_local(alloc_fn, free_fn, iters, ops_out);
    {
        const struct workload *w = find_workload(name);
        if (!w) { fprintf(stderr, "unknown workload: %s\n", name); exit(2); }
        return w->run(alloc_fn, free_fn, iters, ops_out);
    }
}

static int g_pinned = 1;

struct worker {
    const char *name;
    long iters;
    int idx;
    int warm;               /* 1 = run the 1/10 warm-up pass */
    pthread_barrier_t *bar;
    uint64_t chk;
    uint64_t ops;
    int cpu_seen;
    int cpu_end;
    uint64_t t_start;
    uint64_t t_end;
    int failed;
};

static void *worker_main(void *arg)
{
    struct worker *wk = (struct worker *)arg;
    uint64_t dummy = 0;
    if (g_pinned) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(wk->idx, &set);
        if (pthread_setaffinity_np(pthread_self(), sizeof set, &set) != 0) {
            fprintf(stderr, "FAIL pin thread %d to cpu %d\n", wk->idx, wk->idx);
            wk->failed = 1;
            return NULL;
        }
        wk->cpu_seen = sched_getcpu();
        if (wk->cpu_seen != wk->idx) {
            fprintf(stderr, "FAIL thread %d running on cpu %d\n",
                    wk->idx, wk->cpu_seen);
            wk->failed = 1;
            return NULL;
        }
    } else {
        wk->cpu_seen = sched_getcpu();
    }
    /* Untimed warm-up at 1/10 scale (bench discipline).
       Hist-phase timed workers pass warm=0: their warm pass
       ran before the histogram reset (see phase_hist). */
    if (wk->warm) (void)run_one(wk->name, wk->iters / 10, &dummy);
    pthread_barrier_wait(wk->bar);
    wk->t_start = now_ns();
    wk->chk = run_one(wk->name, wk->iters, &wk->ops);
    wk->t_end = now_ns();
    wk->cpu_end = sched_getcpu();
    if (g_pinned && wk->cpu_end != wk->idx) {
        fprintf(stderr, "FAIL pinned thread %d ended on cpu %d\n", wk->idx, wk->cpu_end);
        wk->failed = 1;
    }
    return NULL;
}

static int phase_ladder(const char *name, long threads, long iters)
{
    pthread_t th[32];
    struct worker wk[32];
    pthread_barrier_t bar;
    uint64_t t0, t1, elapsed, total_ops = 0, chk_each = 0;
    long mig = 0;
    long t;
    if (threads < 1 || threads > 32) return 2;
    if (pthread_barrier_init(&bar, NULL, (unsigned)threads + 1) != 0)
        return 1;
    for (t = 0; t < threads; t++) {
        wk[t].name = name; wk[t].iters = iters; wk[t].idx = (int)t;
        wk[t].warm = 1;
        wk[t].bar = &bar; wk[t].chk = 0; wk[t].ops = 0;
        wk[t].cpu_seen = -1; wk[t].cpu_end = -1; wk[t].failed = 0;
        if (pthread_create(&th[t], NULL, worker_main, &wk[t]) != 0)
            return 1;
    }
    pthread_barrier_wait(&bar);
    for (t = 0; t < threads; t++) pthread_join(th[t], NULL);
    pthread_barrier_destroy(&bar);
    t0 = wk[0].t_start; t1 = wk[0].t_end;
    for (t = 1; t < threads; t++) {
        if (wk[t].t_start < t0) t0 = wk[t].t_start;
        if (wk[t].t_end > t1) t1 = wk[t].t_end;
    }
    elapsed = t1 - t0;
    for (t = 0; t < threads; t++) {
        if (wk[t].failed) return 1;
        if (wk[t].cpu_end != wk[t].cpu_seen) mig++;
        if (t == 0) chk_each = wk[t].chk;
        else if (wk[t].chk != chk_each) {
            fprintf(stderr, "FAIL checksum divergence: thread 0=%llu "
                    "thread %ld=%llu\n",
                    (unsigned long long)chk_each, t,
                    (unsigned long long)wk[t].chk);
            return 3;
        }
        total_ops += wk[t].ops;
    }
    printf("LADDER,%s,%ld,%s,%ld,%llu,%llu,%llu,%.1f,%.1f,%llu,%llu,%ld\n",
           name, threads, g_pinned ? "pinned" : "floating", iters,
           (unsigned long long)(total_ops / (uint64_t)threads),
           (unsigned long long)total_ops,
           (unsigned long long)elapsed,
           (double)total_ops * 1e9 / (double)elapsed,
           (double)total_ops * 1e9 / (double)elapsed / (double)threads,
           (unsigned long long)chk_each,
           (unsigned long long)(chk_each * (uint64_t)threads), mig);
    fflush(stdout);
    return 0;
}

static int phase_selfcheck(void)
{
    uint64_t bench_ops = 0, repl_ops = 0, bench_chk, repl_chk;
    const struct workload *mb = find_workload("mixed_batch");
    const struct workload *sc = find_workload("small_churn");
    uint64_t sc_ops = 0, sc_chk;
    if (!mb || !sc) return 2;
    bench_chk = mb->run(alloc_fn, free_fn, 37, &bench_ops);
    repl_chk = w_mixed_batch_local(alloc_fn, free_fn, 37, &repl_ops);
    printf("SELF,mixed_batch,37,%llu,%llu,%llu,%llu,%s\n",
           (unsigned long long)bench_chk, (unsigned long long)repl_chk,
           (unsigned long long)bench_ops, (unsigned long long)repl_ops,
           (bench_chk == repl_chk && bench_ops == repl_ops)
               ? "MATCH" : "MISMATCH");
    sc_chk = sc->run(alloc_fn, free_fn, 100000, &sc_ops);
    printf("SELF,small_churn,100000,%llu,%llu,%s\n",
           (unsigned long long)sc_chk, (unsigned long long)sc_ops,
           (sc_chk == UINT64_C(12756656)) ? "MATCH_K002" : "NOTE");
    fflush(stdout);
    return (bench_chk == repl_chk && bench_ops == repl_ops) ? 0 : 1;
}

#ifdef MIMIR_LOCK_HIST
#include "../../src/core/mimir_core.h"
static const char *const z20_op_names[MIMIR_LOCK_HIST_OPS] = {
    "malloc", "calloc", "realloc", "free", "other"
};
static void hist_dump(long threads, uint64_t ops, uint64_t chk)
{
    struct mimir_lock_hist h;
    int op, kind, b;
    if (mimir_core_lock_hist_snapshot(&h) != 0) {
        fprintf(stderr, "snapshot failed\n");
        exit(1);
    }
    printf("META,ladder_hist,small_churn_t%ld,%ld,%llu,%llu,0.0\n",
           threads, threads, (unsigned long long)ops,
           (unsigned long long)chk);
    for (op = 0; op < MIMIR_LOCK_HIST_OPS; op++) {
        for (kind = 0; kind < 2; kind++) {
            const struct mimir_lock_hist_series *s =
                (kind == 0) ? &h.hold[op] : &h.wait[op];
            printf("HIST,ladder_hist,%s,%s,%llu,%llu,%llu,%llu,%llu",
                   z20_op_names[op], (kind == 0) ? "hold" : "wait",
                   (unsigned long long)s->count,
                   (unsigned long long)s->sum_cycles,
                   (unsigned long long)s->min_cycles,
                   (unsigned long long)s->max_cycles,
                   (unsigned long long)h.lock_failures);
            for (b = 0; b < MIMIR_LOCK_HIST_BUCKETS; b++)
                printf(",%llu", (unsigned long long)s->buckets[b]);
            printf("\n");
        }
    }
    fflush(stdout);
}
#endif

static int phase_hist(long threads, long iters)
{
#ifndef MIMIR_LOCK_HIST
    fprintf(stderr, "hist phase requires the -DMIMIR_LOCK_HIST=1 build\n");
    (void)threads; (void)iters;
    return 2;
#else
    pthread_t th[32];
    struct worker wk[32];
    pthread_barrier_t bar;
    uint64_t total_ops = 0, chk = 0;
    long t;
    if (threads < 1 || threads > 32) return 2;
    /* Warm-up + reset in worker_main already ran at 1/10 scale;
       reset here, after a full single warm pass, so only the
       timed pass below is recorded. */
    {
        struct worker warm;
        pthread_barrier_t wbar;
        pthread_t wth;
        pthread_barrier_init(&wbar, NULL, 2);
        warm.name = "small_churn"; warm.iters = iters / 10; warm.idx = 0;
        warm.warm = 1;
        warm.bar = &wbar; warm.chk = 0; warm.ops = 0;
        warm.cpu_seen = -1; warm.cpu_end = -1; warm.failed = 0;
        if (pthread_create(&wth, NULL, worker_main, &warm) != 0) return 1;
        pthread_barrier_wait(&wbar);
        pthread_join(wth, NULL);
        pthread_barrier_destroy(&wbar);
        if (warm.failed) return 1;
    }
    mimir_core_lock_hist_reset();
    if (pthread_barrier_init(&bar, NULL, (unsigned)threads + 1) != 0)
        return 1;
    for (t = 0; t < threads; t++) {
        wk[t].name = "small_churn"; wk[t].iters = iters; wk[t].idx = (int)t;
        wk[t].warm = 0;
        wk[t].bar = &bar; wk[t].chk = 0; wk[t].ops = 0;
        wk[t].cpu_seen = -1; wk[t].cpu_end = -1; wk[t].failed = 0;
        if (pthread_create(&th[t], NULL, worker_main, &wk[t]) != 0)
            return 1;
    }
    pthread_barrier_wait(&bar);
    for (t = 0; t < threads; t++) {
        pthread_join(th[t], NULL);
        if (wk[t].failed) return 1;
        total_ops += wk[t].ops;
        chk += wk[t].chk;
    }
    pthread_barrier_destroy(&bar);
    hist_dump(threads, total_ops, chk);
    return 0;
#endif
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: probe_z041 <selfcheck|ladder|hist> ...\n");
        return 2;
    }
    if (strcmp(argv[1], "selfcheck") == 0) return phase_selfcheck();
    if (strcmp(argv[1], "ladder") == 0) {
        long iters;
        if (argc < 5) return 2;
        if (strcmp(argv[4], "pinned") == 0) g_pinned = 1;
        else if (strcmp(argv[4], "floating") == 0) g_pinned = 0;
        else return 2;
        iters = (argc >= 6) ? strtol(argv[5], NULL, 10)
              : (strcmp(argv[2], "mixed_batch") == 0) ? 200 : 50000;
        return phase_ladder(argv[2], strtol(argv[3], NULL, 10), iters);
    }
    if (strcmp(argv[1], "hist") == 0) {
        if (argc < 3) return 2;
        return phase_hist(strtol(argv[2], NULL, 10),
                          (argc >= 4) ? strtol(argv[3], NULL, 10) : 50000);
    }
    fprintf(stderr, "unknown phase: %s\n", argv[1]);
    return 2;
}
