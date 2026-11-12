/* probe_z048.c - Z-048 large_batch re-measurement.
 *
 * The workload is bench_k002.c's w_large_batch copied
 * verbatim (LBATCH 32, the same six large sizes, the same
 * endpoint touch, the same warm-up/timed split in main):
 * ONE source, THREE binaries, exactly the K-002 discipline:
 *   (default)      mimir_pool: this tree's core (Z-048 pool live)
 *   -DLEG_BASE     mimir_base: the pre-Z-048 core (seat
 *                  517bbb1) - the same-window A/B control
 *   -DLEG_GLIBC    glibc: host malloc/free
 * An op is ONE allocation plus its matching free. The
 * checksum must equal the K-002 large_batch checksum
 * (1106008 at 300 timed batches) on every leg, proving the
 * workloads are the same work Z-009 graded.
 *
 * Output: one CSV line, the bench_k002 format:
 *   leg,large_batch,ops,elapsed_ns,ops_per_sec,ns_per_op,checksum
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
#ifdef LEG_BASE
static const char *leg_name = "mimir_base";
#else
static const char *leg_name = "mimir_pool";
#endif
#endif

typedef void *(*alloc_t)(size_t);
typedef void (*free_t)(void *);

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

/* W3 large_batch (bench_k002.c, verbatim): batches of 32
 * above-cutoff blocks, touched, freed forward. */
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

int main(void)
{
    uint64_t ops = 0, chk;
    uint64_t t0, t1, elapsed;
    /* Warm-up at 1/10 scale, untimed (bench_k002 main). */
    (void)w_large_batch(alloc_fn, free_fn, 300 / 10, &ops);
    t0 = now_ns();
    chk = w_large_batch(alloc_fn, free_fn, 300, &ops);
    t1 = now_ns();
    elapsed = t1 - t0;
    printf("%s,large_batch,%llu,%llu,%.1f,%.2f,%llu\n",
           leg_name,
           (unsigned long long)ops,
           (unsigned long long)elapsed,
           (double)ops * 1e9 / (double)elapsed,
           (double)elapsed / (double)ops,
           (unsigned long long)chk);
    return 0;
}
