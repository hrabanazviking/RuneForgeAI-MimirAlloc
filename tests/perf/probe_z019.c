/* probe_z019.c - Z-019 lock hold/wait histogram probe.
 *
 * Drives the BANKED K-002 workloads (bench_k002.c included
 * UNMODIFIED below, its main() renamed away) plus two
 * supplementary entry-point phases and one contended phase
 * against the INSTRUMENTED core build (-DMIMIR_LOCK_HIST=1;
 * see src/core/mimir_core.c Z-019 section). One phase per
 * process; the histogram is reset after the phase warm-up
 * so only timed work is recorded.
 *
 * Phases (argv[1]):
 *   bench <workload> [passes]
 *       One K-002 workload (small_churn / mixed_batch /
 *       large_batch / mixed_lifecycle) through the bench's
 *       own alloc/free pointers (mimir_malloc / mimir_free).
 *       Warm-up: the bench's 1/10-scale pass, exactly as
 *       bench_k002. Then `passes` timed passes (default 1).
 *   calloc_churn [iters]
 *       calloc(1, n) + touch + free, n cycling the bench's
 *       small size table. Attributes CALLOC hold/wait (and
 *       FREE for the matching frees).
 *   realloc_cycle [iters]
 *       Per iteration: seed realloc(NULL,16) (recorded under
 *       MALLOC by the core's tag rule: realloc(NULL) delegates
 *       to the alloc entry, which owns the sample), then
 *       realloc grows 64 -> 256 -> 1024 -> 4096, a shrink to
 *       32, touch at each step, free at the end. Attributes
 *       REALLOC hold/wait.
 *   contended <threads> [iters_per_thread]
 *       <threads> pthreads each running the small_churn
 *       shape (alloc/touch/free, small size table) against
 *       the one global lock. Attributes hold/wait under
 *       contention; the runner pins the process to a whole
 *       CCD per the Z-004 protocol.
 *
 * Output (stdout): one META line and one HIST line per
 * (op, kind):
 *   META,phase,detail,threads,ops,checksum,cyc_per_ns
 *   HIST,phase,op,kind,count,sum_cyc,min_cyc,max_cyc,
 *        lock_failures,b0,b1,...,b207
 * op in malloc/calloc/realloc/free/other; kind hold/wait.
 * Buckets (quarter-octave, see mimir_core.h Z-019 block):
 * idx < 64 is the exact cycle count; idx >= 64 encodes
 * e = 6 + (idx-64)/4, sub = (idx-64) % 4, covering
 * [2^e + sub*2^(e-2), +2^(e-2)) cycles; samples at or
 * above 2^42 cycles clamp into the last bucket and are
 * counted in the series' overflow tally (count always
 * equals the sum of buckets as printed).
 * Exit 0 iff the phase completed and its checksum matched
 * the phase's own expected pattern (bench phases: the
 * bench workload's returned checksum, identical across
 * processes - the summariser enforces cross-process
 * equality, the K-002 discipline).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#endif

#define main z019_renamed_k002_main
#include "bench_k002.c"
#undef main

#include "../../src/core/mimir_core.h"

static const char *const z19_op_names[MIMIR_LOCK_HIST_OPS] = {
    "malloc", "calloc", "realloc", "free", "other"
};

static uint64_t z19_stamp(void)
{
#if defined(__x86_64__) || defined(__i386__)
    _mm_lfence();
    return (uint64_t)__rdtsc();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) +
           (uint64_t)ts.tv_nsec;
#endif
}

/* Calibrate cycles per nanosecond against CLOCK_MONOTONIC
 * over a 200 ms sleep; returns 1.0 on the fallback source
 * (stamp already returns nanoseconds). */
static double z19_cyc_per_ns(void)
{
#if defined(__x86_64__) || defined(__i386__)
    struct timespec req = { 0, 200000000L };
    uint64_t c0, c1, n0, n1;
    n0 = now_ns(); c0 = z19_stamp();
    nanosleep(&req, NULL);
    c1 = z19_stamp(); n1 = now_ns();
    if (n1 <= n0 || c1 <= c0) return 0.0;
    return (double)(c1 - c0) / (double)(n1 - n0);
#else
    return 1.0;
#endif
}

static void z19_dump(const char *phase, const char *detail,
                     long threads, uint64_t ops, uint64_t chk,
                     double cyc_per_ns)
{
    struct mimir_lock_hist h;
    int op, kind, b;
    if (mimir_core_lock_hist_snapshot(&h) != 0) {
        fprintf(stderr, "snapshot failed\n");
        exit(1);
    }
    printf("META,%s,%s,%ld,%llu,%llu,%.4f\n", phase, detail,
           threads, (unsigned long long)ops,
           (unsigned long long)chk, cyc_per_ns);
    for (op = 0; op < MIMIR_LOCK_HIST_OPS; op++) {
        for (kind = 0; kind < 2; kind++) {
            const struct mimir_lock_hist_series *s =
                (kind == 0) ? &h.hold[op] : &h.wait[op];
            printf("HIST,%s,%s,%s,%llu,%llu,%llu,%llu,%llu",
                   phase, z19_op_names[op],
                   (kind == 0) ? "hold" : "wait",
                   (unsigned long long)s->count,
                   (unsigned long long)s->sum_cycles,
                   (unsigned long long)s->min_cycles,
                   (unsigned long long)s->max_cycles,
                   (unsigned long long)h.lock_failures);
            for (b = 0; b < MIMIR_LOCK_HIST_BUCKETS; b++)
                printf(",%llu",
                       (unsigned long long)s->buckets[b]);
            printf("\n");
        }
    }
    fflush(stdout);
}

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

/* Observer-effect calibration: back-to-back stamp pairs.
 * The median consecutive-stamp delta is the floor any
 * measured wait/hold contains (two stamps + loop); the
 * recorded run quotes it beside every histogram. */
static int phase_overhead(void)
{
    enum { N = 100000 };
    static uint64_t deltas[N];
    uint64_t prev = z19_stamp();
    int i;
    for (i = 0; i < N; i++) {
        uint64_t cur = z19_stamp();
        deltas[i] = cur - prev;
        prev = cur;
    }
    qsort(deltas, N, sizeof deltas[0], cmp_u64);
    z19_dump("overhead", "stamp_pair_median_cyc", 1,
             (uint64_t)N, deltas[N / 2], z19_cyc_per_ns());
    return 0;
}

static int phase_bench(const char *name, long passes)
{
    size_t w;
    long r;
    for (w = 0; w < sizeof(workloads) / sizeof(workloads[0]); w++) {
        uint64_t ops = 0, total_ops = 0, chk = 0;
        if (strcmp(name, workloads[w].name) != 0) continue;
        (void)workloads[w].run(alloc_fn, free_fn,
                               workloads[w].timed_iters / 10, &ops);
        mimir_core_lock_hist_reset();
        for (r = 0; r < passes; r++) {
            uint64_t o = 0;
            chk = workloads[w].run(alloc_fn, free_fn,
                                   workloads[w].timed_iters, &o);
            total_ops += o;
        }
        z19_dump("bench", name, 1, total_ops, chk,
                 z19_cyc_per_ns());
        return 0;
    }
    fprintf(stderr, "unknown workload: %s\n", name);
    return 2;
}

static int phase_calloc(long iters)
{
    uint64_t chk = 0;
    long i;
    for (i = 0; i < iters / 10; i++) {
        size_t n = small_sizes[i % 8];
        void *p = mimir_calloc(1, n);
        if (!p) { fprintf(stderr, "calloc null\n"); return 1; }
        chk += touch(p, n, (unsigned)i);
        mimir_free(p);
    }
    mimir_core_lock_hist_reset();
    chk = 0;
    for (i = 0; i < iters; i++) {
        size_t n = small_sizes[i % 8];
        void *p = mimir_calloc(1, n);
        if (!p) { fprintf(stderr, "calloc null\n"); return 1; }
        chk += touch(p, n, (unsigned)i);
        mimir_free(p);
    }
    z19_dump("calloc_churn", "calloc1_small", 1,
             (uint64_t)iters, chk, z19_cyc_per_ns());
    return 0;
}

static int phase_realloc(long iters)
{
    static const size_t grows[] = { 64, 256, 1024, 4096 };
    uint64_t chk = 0;
    long i;
    int pass;
    for (pass = 0; pass < 2; pass++) {
        long n = (pass == 0) ? iters / 10 : iters;
        if (pass == 1) { mimir_core_lock_hist_reset(); chk = 0; }
        for (i = 0; i < n; i++) {
            void *p = mimir_realloc(NULL, 16);
            size_t g;
            if (!p) { fprintf(stderr, "realloc seed null\n"); return 1; }
            chk += touch(p, 16, (unsigned)i);
            for (g = 0; g < sizeof(grows) / sizeof(grows[0]); g++) {
                p = mimir_realloc(p, grows[g]);
                if (!p) { fprintf(stderr, "realloc grow null\n"); return 1; }
                chk += touch(p, grows[g], (unsigned)(i + (long)g));
            }
            p = mimir_realloc(p, 32);
            if (!p) { fprintf(stderr, "realloc shrink null\n"); return 1; }
            chk += touch(p, 32, (unsigned)i + 7u);
            mimir_free(p);
        }
    }
    z19_dump("realloc_cycle", "grow64_256_1024_4096_shrink32", 1,
             (uint64_t)iters * 6u, chk, z19_cyc_per_ns());
    return 0;
}

struct z19_worker { long iters; uint64_t chk; int failed; };
static void *z19_worker_main(void *arg)
{
    struct z19_worker *wk = (struct z19_worker *)arg;
    long i;
    wk->chk = 0; wk->failed = 0;
    for (i = 0; i < wk->iters; i++) {
        size_t n = small_sizes[i % 8];
        void *p = mimir_malloc(n);
        if (!p) { wk->failed = 1; return NULL; }
        wk->chk += touch(p, n, (unsigned)i);
        mimir_free(p);
    }
    return NULL;
}

static int phase_contended(long threads, long iters)
{
    pthread_t th[32];
    struct z19_worker wk[32];
    uint64_t chk = 0;
    long t;
    if (threads < 1 || threads > 32) return 2;
    /* Warm-up: one short single-thread pass, then reset. */
    { struct z19_worker warm = { 2000, 0, 0 };
      z19_worker_main(&warm);
      if (warm.failed) return 1; }
    mimir_core_lock_hist_reset();
    for (t = 0; t < threads; t++) {
        wk[t].iters = iters; wk[t].chk = 0; wk[t].failed = 0;
        if (pthread_create(&th[t], NULL, z19_worker_main, &wk[t]) != 0)
            return 1;
    }
    for (t = 0; t < threads; t++) {
        pthread_join(th[t], NULL);
        if (wk[t].failed) return 1;
        chk += wk[t].chk;
    }
    { char detail[32];
      snprintf(detail, sizeof detail, "small_churn_t%ld", threads);
      z19_dump("contended", detail, threads,
               (uint64_t)threads * (uint64_t)iters, chk,
               z19_cyc_per_ns()); }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: probe_z019 <phase> ...\n");
        return 2;
    }
    if (strcmp(argv[1], "overhead") == 0)
        return phase_overhead();
    if (strcmp(argv[1], "bench") == 0) {
        if (argc < 3) return 2;
        return phase_bench(argv[2],
                           (argc >= 4) ? strtol(argv[3], NULL, 10) : 1);
    }
    if (strcmp(argv[1], "calloc_churn") == 0)
        return phase_calloc((argc >= 3) ? strtol(argv[2], NULL, 10)
                                        : 100000);
    if (strcmp(argv[1], "realloc_cycle") == 0)
        return phase_realloc((argc >= 3) ? strtol(argv[2], NULL, 10)
                                         : 20000);
    if (strcmp(argv[1], "contended") == 0) {
        if (argc < 3) return 2;
        return phase_contended(strtol(argv[2], NULL, 10),
                               (argc >= 4) ? strtol(argv[3], NULL, 10)
                                           : 50000);
    }
    fprintf(stderr, "unknown phase: %s\n", argv[1]);
    return 2;
}
