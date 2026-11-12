/* probe_z025.c - Z-025 calloc and realloc anatomy.
 *
 * Dual-leg probe (bench_k002 / probe_z017 pattern): built
 * twice from this one source - glibc leg (-DLEG_GLIBC,
 * libc calloc/realloc/malloc/free) and MimirAlloc leg
 * (mimir_calloc/mimir_realloc/mimir_malloc/mimir_free,
 * linked with the production LIBOBJS). One process sweeps
 * every workload once and prints CSV; the runner
 * (run_z025.sh) interleaves 7 measured processes per leg
 * pinned to CPU 2 under the Z-004 protocol, and
 * summarize_z025.py takes medians.
 *
 * Workloads:
 *   CALLOC    calloc(1,size) + touch-ends + free, single
 *             block recycled. Before timing, the size's
 *             pool is DIRTY-PRIMED (blocks allocated,
 *             filled 0xA5, freed) so a recycled block is
 *             dirty: the zeroing measured is real work,
 *             not the kernel zero-page illusion. The full
 *             zero verification is a SEPARATE untimed pass
 *             (VERIFY lines) so the scan never pollutes
 *             the timed loop.
 *   MALLOC    malloc(size) + touch-ends + free: the
 *             no-zeroing allocation baseline.
 *   MALLOCSET malloc + memset(0,size) + touch + free:
 *             explicit-zeroing comparator. calloc vs
 *             MALLOCSET isolates allocator-internal
 *             zeroing; calloc vs MALLOC is the raw
 *             zeroing premium.
 *   BULK      calloc nblk blocks held live at once
 *             (carve/fresh path, spans or mappings),
 *             timed alloc phase only; zero verification
 *             untimed afterwards (VERIFYBULK lines).
 *   REALLOC   alloc(from) + pattern fill, timed realloc
 *             to `to`, record moved (returned pointer !=
 *             old pointer) and verify the preserved
 *             prefix checksum over min(from,to). The
 *             checksum is a pure function of the pattern,
 *             so it must be IDENTICAL across legs and
 *             processes (K-002 checksum equivalence);
 *             the fill/checksum work is untimed.
 *
 * All allocator calls go through volatile function
 * pointers: the compiler cannot fold malloc+memset into
 * calloc, elide a calloc whose bytes are "unused", or
 * assume realloc semantics it can shortcut. Timed loops
 * touch first/last byte and fold pointers into a
 * volatile sink.
 *
 * Exit 0 iff no NULL, no nonzero byte in any calloc
 * verification, and no realloc prefix mismatch.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef LEG_GLIBC
static void *(*volatile vp_calloc)(size_t, size_t) = calloc;
static void *(*volatile vp_realloc)(void *, size_t) = realloc;
static void *(*volatile vp_malloc)(size_t) = malloc;
static void (*volatile vp_free)(void *) = free;
static const char *LEG_NAME = "glibc";
#else
#include "mimir_alloc.h"
static void *(*volatile vp_calloc)(size_t, size_t) = mimir_calloc;
static void *(*volatile vp_realloc)(void *, size_t) = mimir_realloc;
static void *(*volatile vp_malloc)(size_t) = mimir_malloc;
static void (*volatile vp_free)(void *) = mimir_free;
static const char *LEG_NAME = "mimir";
#endif

static volatile unsigned long sink;

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) +
           (uint64_t)ts.tv_nsec;
}

static long clamp_iters(size_t size, long budget, long lo, long hi)
{
    long it = (long)(budget / (size ? size : 1));
    if (it < lo) it = lo;
    if (it > hi) it = hi;
    return it;
}

/* Dirty-prime: cycle dirty blocks of this size through
 * the allocator so recycled memory is not kernel-zero. */
static void dirty_prime(size_t size, int nblk)
{
    void *blk[64];
    int n = nblk > 64 ? 64 : nblk;
    for (int i = 0; i < n; i++) {
        blk[i] = vp_malloc(size);
        if (blk[i]) memset(blk[i], 0xA5, size);
    }
    for (int i = 0; i < n; i++)
        if (blk[i]) vp_free(blk[i]);
}

static int verify_zero(size_t size)
{
    unsigned char *p = vp_calloc(1, size);
    long nonzero = 0;
    if (!p) return -1;
    for (size_t i = 0; i < size; i++)
        if (p[i] != 0) nonzero++;
    printf("VERIFY,%s,%zu,%ld\n", LEG_NAME, size, nonzero);
    vp_free(p);
    return nonzero ? 1 : 0;
}

static int run_calloc_modes(void)
{
    static const size_t sizes[] = {
        16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
        224, 256, 320, 384, 448, 512, 640, 768, 896, 1008,
        1023, 1024, 1025, 1536, 2048, 4096, 8192, 16384,
        32768, 65536, 131072, 262144, 1048576
    };
    int bad = 0;
    for (size_t si = 0; si < sizeof(sizes) / sizeof(sizes[0]); si++) {
        size_t size = sizes[si];
        long iters = clamp_iters(size, 8388608L, 200, 100000);
        dirty_prime(size, 32);
        bad |= verify_zero(size);
        /* CALLOC timed */
        {
            uint64_t t0 = now_ns();
            for (long i = 0; i < iters; i++) {
                unsigned char *p = vp_calloc(1, size);
                if (!p) { bad = 1; break; }
                sink += p[0] + p[size - 1];
                vp_free(p);
            }
            uint64_t dt = now_ns() - t0;
            printf("CALLOC,%s,%zu,%ld,%.2f\n", LEG_NAME, size,
                   iters, (double)dt / (double)iters);
        }
        /* MALLOC timed */
        {
            uint64_t t0 = now_ns();
            for (long i = 0; i < iters; i++) {
                unsigned char *p = vp_malloc(size);
                if (!p) { bad = 1; break; }
                sink += p[0] + p[size - 1];
                vp_free(p);
            }
            uint64_t dt = now_ns() - t0;
            printf("MALLOC,%s,%zu,%ld,%.2f\n", LEG_NAME, size,
                   iters, (double)dt / (double)iters);
        }
        /* MALLOCSET timed */
        {
            uint64_t t0 = now_ns();
            for (long i = 0; i < iters; i++) {
                unsigned char *p = vp_malloc(size);
                if (!p) { bad = 1; break; }
                memset(p, 0, size);
                sink += p[0] + p[size - 1];
                vp_free(p);
            }
            uint64_t dt = now_ns() - t0;
            printf("MALLOCSET,%s,%zu,%ld,%.2f\n", LEG_NAME, size,
                   iters, (double)dt / (double)iters);
        }
        /* BULK: nblk live at once, alloc phase timed */
        {
            long nblk = size <= 1024 ? 256 : (size <= 65536 ? 64 : 16);
            long rounds = 20;
            void **blk = malloc(sizeof(void *) * (size_t)nblk);
            if (!blk) return 1;
            dirty_prime(size, 32);
            uint64_t t0 = now_ns();
            for (long r = 0; r < rounds; r++) {
                for (long i = 0; i < nblk; i++) {
                    blk[i] = vp_calloc(1, size);
                    if (!blk[i]) { bad = 1; blk[i] = NULL; }
                    else sink += ((unsigned char *)blk[i])[0];
                }
                for (long i = 0; i < nblk; i++)
                    if (blk[i]) vp_free(blk[i]);
            }
            uint64_t dt = now_ns() - t0;
            long ops = nblk * rounds;
            printf("BULK,%s,%zu,%ld,%.2f\n", LEG_NAME, size, ops,
                   (double)dt / (double)ops);
            /* untimed full zero verification of a fresh bulk set */
            long nz = 0;
            for (long i = 0; i < nblk; i++) {
                blk[i] = vp_calloc(1, size);
                if (!blk[i]) { bad = 1; blk[i] = NULL; }
            }
            for (long i = 0; i < nblk; i++) {
                if (!blk[i]) continue;
                const unsigned char *b = blk[i];
                for (size_t k = 0; k < size; k++)
                    if (b[k] != 0) nz++;
            }
            printf("VERIFYBULK,%s,%zu,%ld\n", LEG_NAME, size, nz);
            if (nz) bad = 1;
            for (long i = 0; i < nblk; i++)
                if (blk[i]) vp_free(blk[i]);
            free(blk);
        }
    }
    return bad;
}

static unsigned char pat(size_t i)
{
    return (unsigned char)((i * 31u + 7u) & 0xffu);
}

static uint64_t prefix_checksum(const unsigned char *p, size_t n)
{
    uint64_t h = UINT64_C(1469598103934665603);
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= UINT64_C(1099511628211);
    }
    return h;
}

static int run_realloc(void)
{
    static const struct { size_t from, to; } pairs[] = {
        {17, 32}, {16, 48}, {48, 112}, {100, 112}, {112, 256},
        {256, 512}, {512, 896}, {896, 1024}, {1008, 1024},
        {1023, 1025}, {1024, 1025}, {1025, 2048}, {2048, 4096},
        {4096, 16384}, {16384, 65536}, {65536, 262144},
        {262144, 1048576}, {512, 512}, {4096, 4096},
        {1024, 16}, {2048, 1025}, {4096, 1024}, {1048576, 1024}
    };
    int bad = 0;
    for (size_t pi = 0; pi < sizeof(pairs) / sizeof(pairs[0]); pi++) {
        size_t from = pairs[pi].from, to = pairs[pi].to;
        size_t mx = from > to ? from : to;
        long reps = clamp_iters(mx, 4194304L, 100, 20000);
        long moved = 0;
        uint64_t cks = 0;
        uint64_t timed = 0;
        for (long r = 0; r < reps; r++) {
            unsigned char *p = vp_malloc(from);
            if (!p) { bad = 1; break; }
            for (size_t i = 0; i < from; i++) p[i] = pat(i);
            uint64_t want = prefix_checksum(p, from < to ? from : to);
            uint64_t t0 = now_ns();
            unsigned char *q = vp_realloc(p, to);
            timed += now_ns() - t0;
            if (!q) { bad = 1; vp_free(p); break; }
            if (q != p) moved++;
            uint64_t got = prefix_checksum(q, from < to ? from : to);
            if (got != want) bad = 1;
            cks = got;
            sink += q[0];
            vp_free(q);
        }
        printf("REALLOC,%s,%zu,%zu,%ld,%.2f,%ld,%llu\n",
               LEG_NAME, from, to, reps,
               reps ? (double)timed / (double)reps : 0.0,
               moved, (unsigned long long)cks);
    }
    return bad;
}

int main(void)
{
    int bad = 0;
    printf("leg: %s\n", LEG_NAME);
    bad |= run_calloc_modes();
    bad |= run_realloc();
    printf("DONE,%s,%d,%lu\n", LEG_NAME, bad, sink);
    return bad ? 1 : 0;
}
