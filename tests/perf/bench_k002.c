/* bench_k002.c - K-002 Phase 11 reproducible baseline
 * benchmark harness: MimirAlloc vs host glibc allocator.
 *
 * ONE source, TWO binaries. The workload code below is
 * byte-identical for both legs; the only difference is
 * which allocator function pointers main() hands to the
 * workloads:
 *   -DLEG_GLIBC : stdlib malloc/free (host glibc 2.36)
 *   (default)   : mimir_malloc/mimir_free from the banked
 *                 libmimiralloc objects (production build,
 *                 exactly as `make test` builds them)
 * An op is ONE allocation plus its matching free. Every
 * workload runs a warm-up pass at 1/10 scale (untimed),
 * then ONE timed pass; repetitions are separate processes
 * driven by tests/perf/run_bench_k002.sh (>= 5 measured
 * repetitions per leg per workload). The harness prints
 * one CSV line per workload:
 *   leg,workload,ops,elapsed_ns,ops_per_sec,ns_per_op,checksum
 * Distributions (median/min/max) are computed from those
 * lines by tests/perf/summarize_k002.py - a single run of
 * this binary is never a verdict on its own.
 * Timing: CLOCK_MONOTONIC around the whole timed pass.
 * The pattern write/read on every block keeps the work
 * real (the allocator cannot be optimised away) and the
 * checksum is printed so a silent no-op would show.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef LEG_GLIBC
static void *(*alloc_fn)(size_t) = malloc;
static void (*free_fn)(void *) = free;
static const char *leg_name = "glibc";
#else
#include "mimir_alloc.h"
static void *(*alloc_fn)(size_t) = mimir_malloc;
static void (*free_fn)(void *) = mimir_free;
static const char *leg_name = "mimiralloc";
#endif

typedef void *(*alloc_t)(size_t);
typedef void (*free_t)(void *);

/* Banked size classes (src/core/mimir_core.c, G-002):
 * 16,32,48,64,80,96,112,128,160,192,224,256,320,384,
 * 448,512,640,768,896,1024. Workload size cycles below
 * are drawn from this table plus above-cutoff sizes. */
static const size_t small_sizes[] = {
    16, 32, 48, 64, 96, 128, 192, 256
};
static const size_t mixed_sizes[] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};
static const size_t large_sizes[] = {
    2048, 8192, 16384, 65536, 262144, 1048576
};

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) +
           (uint64_t)ts.tv_nsec;
}

static unsigned char touch(void *p, size_t n, unsigned seed)
{
    unsigned char *b = (unsigned char *)p;
    b[0] = (unsigned char)(seed & 0xFFu);
    b[n - 1] = (unsigned char)((seed >> 3) & 0xFFu);
    return (unsigned char)(b[0] ^ b[n - 1]);
}

/* W1 small_churn: one block at a time, alloc/touch/free,
 * sizes cycling the small end of the class table. */
static uint64_t w_small_churn(alloc_t af, free_t ff, long iters,
                              uint64_t *ops_out)
{
    uint64_t chk = 0;
    long i;
    for (i = 0; i < iters; i++) {
        size_t n = small_sizes[i % 8];
        void *p = af(n);
        if (!p) { fprintf(stderr, "FAIL small_churn null\n"); exit(1); }
        chk += touch(p, n, (unsigned)i);
        ff(p);
    }
    *ops_out = (uint64_t)iters;
    return chk;
}

/* W2 mixed_batch: batches of 256 live blocks spanning the
 * full class table, touched, freed in reverse order. */
#define BATCH 256
static uint64_t w_mixed_batch(alloc_t af, free_t ff, long batches,
                              uint64_t *ops_out)
{
    static void *ptrs[BATCH];
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

/* W3 large_batch: batches of 32 above-cutoff blocks
 * (mapping path for MimirAlloc), touched, freed forward. */
#define LBATCH 32
static uint64_t w_large_batch(alloc_t af, free_t ff, long batches,
                              uint64_t *ops_out)
{
    static void *ptrs[LBATCH];
    uint64_t chk = 0;
    long b;
    int i;
    for (b = 0; b < batches; b++) {
        for (i = 0; i < LBATCH; i++) {
            size_t n = large_sizes[(i + (int)b) % 6];
            ptrs[i] = af(n);
            if (!ptrs[i]) { fprintf(stderr, "FAIL large_batch null\n"); exit(1); }
            chk += touch(ptrs[i], n, (unsigned)(b + i));
        }
        for (i = 0; i < LBATCH; i++) ff(ptrs[i]);
    }
    *ops_out = (uint64_t)batches * LBATCH;
    return chk;
}

/* W4 mixed_lifecycle: 512 live mixed small+large blocks;
 * free the odd slots, refill them, then free all. One
 * round = 512 + 256 = 768 ops. */
#define MBATCH 512
static uint64_t w_mixed_lifecycle(alloc_t af, free_t ff, long rounds,
                                  uint64_t *ops_out)
{
    static void *ptrs[MBATCH];
    static size_t sizes[MBATCH];
    uint64_t chk = 0;
    long r;
    int i;
    for (i = 0; i < MBATCH; i++) {
        sizes[i] = (i % 8 == 7) ? large_sizes[i % 3]
                                : mixed_sizes[i % 20];
    }
    for (r = 0; r < rounds; r++) {
        for (i = 0; i < MBATCH; i++) {
            ptrs[i] = af(sizes[i]);
            if (!ptrs[i]) { fprintf(stderr, "FAIL lifecycle null\n"); exit(1); }
            chk += touch(ptrs[i], sizes[i], (unsigned)(r + i));
        }
        for (i = 1; i < MBATCH; i += 2) { ff(ptrs[i]); ptrs[i] = NULL; }
        for (i = 1; i < MBATCH; i += 2) {
            ptrs[i] = af(sizes[i]);
            if (!ptrs[i]) { fprintf(stderr, "FAIL lifecycle refill null\n"); exit(1); }
            chk += touch(ptrs[i], sizes[i], (unsigned)(r + i + 1));
        }
        for (i = 0; i < MBATCH; i++) ff(ptrs[i]);
    }
    *ops_out = (uint64_t)rounds * 768;
    return chk;
}

struct workload {
    const char *name;
    uint64_t (*run)(alloc_t, free_t, long, uint64_t *);
    long timed_iters;
};

static const struct workload workloads[] = {
    { "small_churn",     w_small_churn,     100000 },
    { "mixed_batch",     w_mixed_batch,        1000 },
    { "large_batch",     w_large_batch,         300 },
    { "mixed_lifecycle", w_mixed_lifecycle,     400 },
};

int main(void)
{
    size_t w;
    for (w = 0; w < sizeof(workloads) / sizeof(workloads[0]); w++) {
        uint64_t ops = 0, chk;
        uint64_t t0, t1, elapsed;
        /* Warm-up at 1/10 scale, untimed. */
        (void)workloads[w].run(alloc_fn, free_fn,
                               workloads[w].timed_iters / 10, &ops);
        t0 = now_ns();
        chk = workloads[w].run(alloc_fn, free_fn,
                               workloads[w].timed_iters, &ops);
        t1 = now_ns();
        elapsed = t1 - t0;
        printf("%s,%s,%llu,%llu,%.1f,%.2f,%llu\n",
               leg_name, workloads[w].name,
               (unsigned long long)ops,
               (unsigned long long)elapsed,
               (double)ops * 1e9 / (double)elapsed,
               (double)elapsed / (double)ops,
               (unsigned long long)chk);
        fflush(stdout);
    }
    return 0;
}
