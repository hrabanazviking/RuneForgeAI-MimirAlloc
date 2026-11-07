/* probe_g001.c - G-001 Phase 3 baseline micro-probe.
 *
 * A small, bounded measurement probe - NOT a benchmark suite.
 * It measures, for one allocator at a time:
 *   (1) latency distribution of an allocate/touch/free pair for
 *       representative small sizes (per-op clock_gettime
 *       CLOCK_MONOTONIC samples, sorted; min/p50/p90/p99/max
 *       and mean reported);
 *   (2) retained memory: /proc/self/status VmRSS read before a
 *       batch allocation, while the batch is live and touched,
 *       and after the batch is freed (process-level RSS; see
 *       INC-G001-06 in docs/PHASE_7_FOUNDATION_PLAN.md);
 *   (3) churn stability: repeated allocate/free cycles of the
 *       same batch shape, VmRSS after each cycle.
 *
 * Usage: probe_g001 <mimir|libc>
 *   mimir - the banked Phase 3 allocator via the four public
 *           symbols in include/mimir_alloc.h (this slice uses
 *           mimir_malloc / mimir_free only);
 *   libc  - the platform allocator (malloc / free), same probe
 *           shape, so later slices can re-run the identical
 *           shape against the platform allocator and against
 *           the Phase 7 implementation.
 *
 * Bounds: 9 sizes x 2000 latency ops; retention batches of
 * 256 blocks; 10 churn cycles. Total live blocks at any time
 * <= 256; total ops < 100k. Runs in well under a second.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mimir_alloc.h"

static void *(*alloc_fn)(size_t);
static void (*free_fn)(void *);

static void *libc_alloc(size_t n) { return malloc(n); }
static void libc_free(void *p) { free(p); }
static void *mimir_alloc(size_t n) { return mimir_malloc(n); }
static void mimir_free_wrap(void *p) { mimir_free(p); }

static long long now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static int cmp_ll(const void *a, const void *b)
{
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}

static long read_rss_kb(void)
{
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256];
    long v = -1;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "VmRSS: %ld kB", &v) == 1) break;
    }
    fclose(f);
    return v;
}

#define LAT_OPS 2000
static long long samples[LAT_OPS];

static void latency_for(size_t size)
{
    long long sum = 0;
    int fails = 0;
    for (int i = 0; i < LAT_OPS; i++) {
        long long t0 = now_ns();
        void *p = alloc_fn(size);
        if (p) { ((volatile char *)p)[0] = (char)i; free_fn(p); }
        else fails++;
        long long t1 = now_ns();
        samples[i] = t1 - t0;
        sum += samples[i];
    }
    qsort(samples, LAT_OPS, sizeof(long long), cmp_ll);
    printf("LAT size=%zu ops=%d fails=%d min_ns=%lld p50_ns=%lld "
           "p90_ns=%lld p99_ns=%lld max_ns=%lld mean_ns=%lld\n",
           size, LAT_OPS, fails, samples[0], samples[LAT_OPS / 2],
           samples[(LAT_OPS * 90) / 100], samples[(LAT_OPS * 99) / 100],
           samples[LAT_OPS - 1], sum / LAT_OPS);
    fflush(stdout);
}

#define BATCH 256
static void *blocks[BATCH];

static void retention_for(size_t size)
{
    long before = read_rss_kb();
    int fails = 0;
    for (int i = 0; i < BATCH; i++) {
        blocks[i] = alloc_fn(size);
        if (blocks[i]) memset(blocks[i], 0xA5, size);
        else fails++;
    }
    long live = read_rss_kb();
    for (int i = 0; i < BATCH; i++) { free_fn(blocks[i]); blocks[i] = NULL; }
    long after = read_rss_kb();
    printf("RET size=%zu blocks=%d fails=%d rss_before_kb=%ld "
           "rss_live_kb=%ld rss_after_free_kb=%ld live_delta_kb=%ld "
           "retained_after_free_kb=%ld\n",
           size, BATCH, fails, before, live, after,
           (before >= 0 && live >= 0) ? live - before : -1,
           (before >= 0 && after >= 0) ? after - before : -1);
    fflush(stdout);
}

static void churn(size_t size, int cycles)
{
    long start = read_rss_kb();
    printf("CHURN size=%zu blocks=%d cycles=%d rss_start_kb=%ld",
           size, BATCH, cycles, start);
    for (int c = 0; c < cycles; c++) {
        for (int i = 0; i < BATCH; i++) {
            blocks[i] = alloc_fn(size);
            if (blocks[i]) memset(blocks[i], 0x5A, size);
        }
        for (int i = 0; i < BATCH; i++) { free_fn(blocks[i]); blocks[i] = NULL; }
        printf(" cycle%d_rss_kb=%ld", c, read_rss_kb());
    }
    printf("\n");
    fflush(stdout);
}

int main(int argc, char **argv)
{
    if (argc != 2 ||
        (strcmp(argv[1], "mimir") != 0 && strcmp(argv[1], "libc") != 0)) {
        fprintf(stderr, "usage: %s <mimir|libc>\n", argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "mimir") == 0) {
        alloc_fn = mimir_alloc; free_fn = mimir_free_wrap;
    } else {
        alloc_fn = libc_alloc; free_fn = libc_free;
    }
    printf("PROBE allocator=%s timing=clock_gettime(CLOCK_MONOTONIC) "
           "per-op around alloc+touch-first-byte+free; "
           "rss=/proc/self/status VmRSS (process-level)\n", argv[1]);
    static const size_t lat_sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024, 4096};
    for (size_t i = 0; i < sizeof lat_sizes / sizeof lat_sizes[0]; i++)
        latency_for(lat_sizes[i]);
    static const size_t ret_sizes[] = {16, 64, 256, 1024};
    for (size_t i = 0; i < sizeof ret_sizes / sizeof ret_sizes[0]; i++)
        retention_for(ret_sizes[i]);
    churn(64, 10);
    printf("PROBE DONE allocator=%s\n", argv[1]);
    return 0;
}
