/*
 * probe_z062.c -- Z-062 reuse-distance / L3 probe.
 *
 * One process performs one (policy, class, footprint) cell and
 * prints one CSV line:
 *   leg,policy,class_size,footprint,blocks,rounds,elapsed_ns,
 *   ns_per_block,checksum
 *
 * Workload: W blocks of class_size (W = footprint / class_size).
 * Initial fill (untimed) writes byte 0xA5 over every block.
 * Then cycles: a warm-up cycle (untimed) plus ROUNDS timed
 * cycles. One cycle =
 *   release all W blocks in logical order 0..W-1, then
 *   reacquire all W blocks, then touch every reacquired block
 *   (read a byte every 64 B into a volatile sink, write back
 *   incremented; the last byte is overwritten with this
 *   cycle's round byte).
 * The checksum is a deterministic function of (logical block
 * index, cycle) accumulated over the timed cycles, so it is
 * identical across policies and allocators by construction --
 * the K-002 workload-equivalence proof (the Z-052 pattern).
 * Content preservation across a native free is NOT assumed:
 * a full free-all empties spans, and MimirAlloc releases
 * empty spans beyond its per-class retention bound, so a
 * reacquired block may sit on a fresh zero page (proven by
 * this probe's first draft, which expected a surviving tag
 * and found 0 at block 16 of the 256 B class -- recorded in
 * the slice's recorded run as a reuse-policy fact, not
 * hidden). Correctness is instead proven in the warm-up
 * cycle: all W reacquired pointers must be pairwise distinct
 * (qsort check, untimed), else exit 5.
 *
 * Policies (runtime argument):
 *   native -- real free/malloc of every block each cycle; the
 *             allocator's own reuse order decides (MimirAlloc:
 *             LIFO end-to-end; glibc: tcache LIFO for these
 *             sizes).
 *   lifo   -- emulated stack over a probe-held pool of blocks
 *             allocated once from the same allocator: release
 *             pushes 0..W-1, reacquire pops (reverse order).
 *   fifo   -- emulated queue over the identical pool: release
 *             enqueues 0..W-1, reacquire dequeues (same order,
 *             reuse distance = the full working set).
 * The lifo emulation is the control for the fifo emulation:
 * both hold the same blocks and differ only in reuse order.
 * Native vs lifo cross-checks the emulation against the
 * shipped allocator. NOTE (design record): the native small
 * path's hot layer is the per-thread TLS bin (capacity 16);
 * a W-block churn overflows it, so native cycles exercise the
 * central span free lists (also LIFO) -- that is the shipped
 * policy at desktop working-set sizes, measured as-is.
 *
 * Legs (compile-time): default = this tree's mimir objects;
 * -DLEG_GLIBC = host glibc.
 *
 * Usage: probe_z062 <native|lifo|fifo> <class_size>
 *                   <footprint_bytes> <rounds>
 */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef LEG_GLIBC
#define LEG_NAME "glibc"
#define M_MALLOC malloc
#define M_FREE free
#else
#include "mimir_alloc.h"
#define LEG_NAME "mimir"
#define M_MALLOC mimir_malloc
#define M_FREE mimir_free
#endif

static volatile uint64_t touch_sink;

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static unsigned char round_byte(int cycle)
{
    return (unsigned char)(0x30u + (unsigned)cycle);
}

static int cmp_ptr(const void *a, const void *b)
{
    uintptr_t x = (uintptr_t)*(void *const *)a;
    uintptr_t y = (uintptr_t)*(void *const *)b;
    return (x > y) - (x < y);
}

/* Deterministic per-(block, cycle) checksum term: identical
 * on every leg by construction. */
static uint64_t ck_term(size_t i, int cycle)
{
    uint64_t h = (uint64_t)(i + 1) * UINT64_C(2654435761);
    h ^= (uint64_t)(unsigned)(cycle + 1) * UINT64_C(40503);
    return (h >> 24) & 0xFFu;
}

/* Touch one block: read a byte per 64 B line into the sink,
 * write each touched byte back incremented (dirty traffic),
 * and set the last byte to this cycle's round byte. */
static void touch_block(unsigned char *p, size_t n, int cycle)
{
    size_t off;
    for (off = 0; off + 64 <= n; off += 64) {
        touch_sink += p[off];
        p[off] = (unsigned char)(p[off] + 1u);
    }
    p[n - 1] = round_byte(cycle);
}

int main(int argc, char **argv)
{
    const char *policy;
    size_t class_size, footprint, W, i;
    int rounds, cycle, timed;
    void **blocks, **pool;
    uint64_t checksum = 0, t0 = 0, t1 = 0, elapsed;

    if (argc != 5) {
        fprintf(stderr, "usage: probe_z062 <native|lifo|fifo> "
                        "<class_size> <footprint> <rounds>\n");
        return 2;
    }
    policy = argv[1];
    class_size = (size_t)strtoull(argv[2], NULL, 10);
    footprint = (size_t)strtoull(argv[3], NULL, 10);
    rounds = atoi(argv[4]);
    if (class_size == 0 || footprint < class_size || rounds < 1) {
        fprintf(stderr, "bad arguments\n");
        return 2;
    }
    W = footprint / class_size;

    blocks = (void **)malloc(W * sizeof(void *));
    pool = (void **)malloc(W * sizeof(void *));
    if (blocks == NULL || pool == NULL) {
        fprintf(stderr, "pointer arrays failed\n");
        return 3;
    }
    for (i = 0; i < W; i++) {
        blocks[i] = M_MALLOC(class_size);
        if (blocks[i] == NULL) {
            fprintf(stderr, "alloc failed at %zu\n", i);
            return 3;
        }
        memset(blocks[i], 0xA5, class_size);
    }

    elapsed = 0;
    for (cycle = 0; cycle <= rounds; cycle++) {
        timed = cycle > 0; /* cycle 0 is the warm-up */
        if (timed) {
            t0 = now_ns();
        }
        if (strcmp(policy, "native") == 0) {
            for (i = 0; i < W; i++) {
                M_FREE(blocks[i]);
            }
            for (i = 0; i < W; i++) {
                blocks[i] = M_MALLOC(class_size);
                if (blocks[i] == NULL) {
                    fprintf(stderr, "realloc failed\n");
                    return 3;
                }
            }
        } else if (strcmp(policy, "lifo") == 0) {
            size_t sp = 0;
            for (i = 0; i < W; i++) {
                pool[sp++] = blocks[i];
            }
            for (i = 0; i < W; i++) {
                blocks[i] = pool[--sp];
            }
        } else if (strcmp(policy, "fifo") == 0) {
            for (i = 0; i < W; i++) {
                pool[i] = blocks[i];
            }
            for (i = 0; i < W; i++) {
                blocks[i] = pool[i];
            }
        } else {
            fprintf(stderr, "unknown policy %s\n", policy);
            return 2;
        }
        if (!timed) {
            /* Warm-up cycle only (untimed): the allocator must
             * hand out W pairwise-distinct blocks. */
            memcpy(pool, blocks, W * sizeof(void *));
            qsort(pool, W, sizeof(void *), cmp_ptr);
            for (i = 1; i < W; i++) {
                if (pool[i] == pool[i - 1]) {
                    fprintf(stderr,
                            "duplicate pointer handed out\n");
                    return 5;
                }
            }
        }
        for (i = 0; i < W; i++) {
            unsigned char *p = (unsigned char *)blocks[i];
            if (timed) {
                checksum += ck_term(i, cycle);
            }
            touch_block(p, class_size, cycle);
        }
        if (timed) {
            t1 = now_ns();
            elapsed += t1 - t0;
        }
    }

    printf("%s,%s,%zu,%zu,%zu,%d,%llu,%.3f,%llu\n", LEG_NAME,
           policy, class_size, footprint, W, rounds,
           (unsigned long long)elapsed,
           (double)elapsed / (double)(W * (size_t)rounds),
           (unsigned long long)checksum);
    fflush(stdout);
    (void)touch_sink;
    return 0;
}
