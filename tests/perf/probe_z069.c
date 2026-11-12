/*
 * probe_z069.c -- Z-069 canary economics probe.
 *
 * One process performs one (leg, class) cell and prints one CSV
 * line:
 *   leg,class_size,blocks,rounds,elapsed_ns,ns_per_op,checksum,
 *   canary_faults
 *
 * Workload (Z-004 protocol shape, as in probe_z062): W blocks of
 * class_size held in a probe array. One timed cycle =
 *   acquire all W blocks (mimir_malloc), touch byte 0 of each,
 *   then release all W blocks in order 0..W-1 (mimir_free).
 * ns_per_op is per alloc+free pair. A warm-up cycle runs untimed
 * first; all W pointers of the warm-up cycle must be pairwise
 * distinct (qsort check), else exit 5. The checksum is a
 * deterministic function of (block index, cycle) accumulated
 * over the timed cycles and is identical across legs by
 * construction (workload-equivalence proof, Z-052 pattern).
 *
 * Canary legs emulate the per-block canary exactly where a
 * production tail canary would live: in the last N bytes of the
 * block extent (the in-slack / stride-tail position). On acquire
 * the leg writes the 0xA5 pattern over those N bytes (the fill
 * an allocator performs at publication/handout); on release it
 * verifies all N bytes before freeing (the check an allocator
 * performs at free). The instruction cost of fill+verify is the
 * quantity being costed; the byte cost of each canary FORM
 * (stride growth, slots-per-page loss) is exact arithmetic from
 * the banked span geometry (96 B header, 4096 B page) and is
 * tabulated by summarize_z069.py, not measured here.
 *
 * Legs:
 *   base     -- no canary (production today)
 *   base_dup -- identical duplicate of base: the identical-leg
 *               agreement control, bound 5%
 *   can16    -- 16 B tail canary fill+verify per block
 *   can32    -- 32 B tail canary fill+verify per block
 *
 * For classes smaller than the canary (can32 on the 16 B
 * class) the fill+verify wraps over the extent in chunks
 * until the full N bytes of work are done, never writing a
 * single byte outside the block: the byte count -- the costed
 * quantity -- is exact, and no neighbouring block is touched.
 * (The first draft wrote the 32 B at offset 0 of a 16 B block
 * and segfaulted the sweep; fixed in-slice, recorded in the
 * slice's recorded run.)
 *
 * Exit codes: 0 ok; 2 bad args; 5 pointer-distinctness failure;
 * 6 canary verify failure inside the probe's own legs (a probe
 * bug or real corruption -- either way the cell is invalid).
 *
 * Usage: probe_z069 <leg> <class_size> <blocks> <rounds>
 */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mimir_alloc.h"

#define PATTERN 0xA5u

static volatile uint64_t touch_sink;

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static int cmp_ptr(const void *a, const void *b)
{
    uintptr_t x = (uintptr_t)*(void *const *)a;
    uintptr_t y = (uintptr_t)*(void *const *)b;
    return (x > y) - (x < y);
}

static size_t canary_len(const char *leg)
{
    if (strcmp(leg, "can16") == 0) return 16;
    if (strcmp(leg, "can32") == 0) return 32;
    return 0;
}

static void canary_fill(unsigned char *blk, size_t class_size, size_t n)
{
    size_t done = 0;
    if (class_size >= n) {
        memset(blk + (class_size - n), PATTERN, n);
        return;
    }
    while (done < n) {
        size_t chunk = class_size;
        if (chunk > n - done) chunk = n - done;
        memset(blk, PATTERN, chunk);
        done += chunk;
    }
}

static int canary_intact(const unsigned char *blk, size_t class_size,
                         size_t n)
{
    size_t done = 0;
    size_t i;
    if (class_size >= n) {
        const unsigned char *tail = blk + (class_size - n);
        for (i = 0; i < n; i++) {
            if (tail[i] != (unsigned char)PATTERN) return 0;
        }
        return 1;
    }
    while (done < n) {
        size_t chunk = class_size;
        if (chunk > n - done) chunk = n - done;
        for (i = 0; i < chunk; i++) {
            if (blk[i] != (unsigned char)PATTERN) return 0;
        }
        done += chunk;
    }
    return 1;
}

int main(int argc, char **argv)
{
    const char *leg;
    size_t class_size, blocks, ncan;
    long rounds, r;
    void **ptrs;
    uint64_t checksum = 0;
    uint64_t faults = 0;
    uint64_t t0, elapsed;
    size_t i;

    if (argc != 5) {
        fprintf(stderr, "usage: probe_z069 <leg> <class> <blocks> "
                        "<rounds>\n");
        return 2;
    }
    leg = argv[1];
    if (strcmp(leg, "base") != 0 && strcmp(leg, "base_dup") != 0 &&
        strcmp(leg, "can16") != 0 && strcmp(leg, "can32") != 0) {
        fprintf(stderr, "unknown leg %s\n", leg);
        return 2;
    }
    class_size = (size_t)strtoul(argv[2], NULL, 10);
    blocks = (size_t)strtoul(argv[3], NULL, 10);
    rounds = strtol(argv[4], NULL, 10);
    if (class_size == 0 || blocks == 0 || rounds <= 0) return 2;
    ncan = canary_len(leg);

    ptrs = malloc(blocks * sizeof(void *));
    if (ptrs == NULL) return 2;

    /* warm-up cycle (untimed) + distinctness proof */
    for (i = 0; i < blocks; i++) {
        ptrs[i] = mimir_malloc(class_size);
        if (ptrs[i] == NULL) return 5;
        ((unsigned char *)ptrs[i])[0] = (unsigned char)i;
        if (ncan) canary_fill(ptrs[i], class_size, ncan);
    }
    {
        void **sorted = malloc(blocks * sizeof(void *));
        if (sorted == NULL) return 2;
        memcpy(sorted, ptrs, blocks * sizeof(void *));
        qsort(sorted, blocks, sizeof(void *), cmp_ptr);
        for (i = 1; i < blocks; i++) {
            if (sorted[i] == sorted[i - 1]) return 5;
        }
        free(sorted);
    }
    for (i = 0; i < blocks; i++) {
        if (ncan && !canary_intact(ptrs[i], class_size, ncan)) {
            faults++;
        }
        touch_sink += ((unsigned char *)ptrs[i])[0];
        mimir_free(ptrs[i]);
    }
    if (faults) return 6;

    t0 = now_ns();
    for (r = 0; r < rounds; r++) {
        for (i = 0; i < blocks; i++) {
            ptrs[i] = mimir_malloc(class_size);
            if (ptrs[i] == NULL) return 5;
            ((unsigned char *)ptrs[i])[0] =
                (unsigned char)(0x30u + (unsigned)r);
            if (ncan) canary_fill(ptrs[i], class_size, ncan);
            checksum += (uint64_t)(i + 1) * (uint64_t)(r + 1);
        }
        for (i = 0; i < blocks; i++) {
            if (ncan && !canary_intact(ptrs[i], class_size, ncan)) {
                faults++;
            }
            touch_sink += ((unsigned char *)ptrs[i])[0];
            mimir_free(ptrs[i]);
        }
    }
    elapsed = now_ns() - t0;
    if (faults) return 6;

    {
        double ops = (double)blocks * (double)rounds;
        printf("%s,%zu,%zu,%ld,%llu,%.3f,%llu,%llu\n", leg,
               class_size, blocks, rounds,
               (unsigned long long)elapsed, (double)elapsed / ops,
               (unsigned long long)checksum,
               (unsigned long long)faults);
    }
    free(ptrs);
    return 0;
}
