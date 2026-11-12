/*
 * probe_z052.c -- Z-052 growth-ladder probe.
 *
 * One process performs one ladder and prints one CSV line:
 *   leg,growth_ladder,ops,elapsed_ns,ns_per_op,inplace,checksum
 * Ladder: malloc(34 MiB), then realloc through
 * 40, 48, 56, 64, 80, 96, 128 MiB (7 grows, 8 ops total).
 * After every step the whole current extent is filled with
 * the step's byte; before each grow the previous extent is
 * verified page-sampled (first+last byte of every page).
 * `inplace` counts the grows that returned the input
 * pointer. The checksum sums the final extent's sampled
 * bytes and is identical across legs by construction
 * (the pattern depends only on the ladder, never on the
 * allocator), which is the K-002 workload-equivalence proof.
 *
 * Legs (compile-time, the run_z048.sh pattern):
 *   default:   this tree's mimir objects (Z-052, -DLEG_AFTER
 *              is set by the runner for readability only)
 *   -DLEG_BASE: the pre-Z-052 tree's mimir objects
 *   -DLEG_GLIBC: the host glibc allocator
 */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef LEG_GLIBC
#define LEG_NAME "glibc"
#define M_MALLOC malloc
#define M_REALLOC realloc
#define M_FREE free
#else
#include "mimir_alloc.h"
#define M_MALLOC mimir_malloc
#define M_REALLOC mimir_realloc
#define M_FREE mimir_free
#ifdef LEG_BASE
#define LEG_NAME "mimir_before"
#else
#define LEG_NAME "mimir_after"
#endif
#endif

#define MIB ((size_t)1024 * 1024)
static const size_t RUNGS[] = {
    34 * MIB, 40 * MIB, 48 * MIB, 56 * MIB,
    64 * MIB, 80 * MIB, 96 * MIB, 128 * MIB
};
#define NRUNGS (sizeof(RUNGS) / sizeof(RUNGS[0]))

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static unsigned char step_byte(size_t step)
{
    return (unsigned char)(0x40u + step * 7u);
}

/* Verify the extent still carries `byte` at the first and
 * last byte of every page; returns 0 on any mismatch. */
static int verify_extent(const unsigned char *p, size_t bytes,
                         unsigned char byte, size_t page)
{
    size_t off;
    for (off = 0; off < bytes; off += page) {
        size_t last = off + page - 1 < bytes ? off + page - 1 : bytes - 1;
        if (p[off] != byte || p[last] != byte) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    long ps = sysconf(_SC_PAGESIZE);
    size_t page;
    unsigned char *p;
    size_t step;
    int inplace = 0;
    uint64_t t0, t1, checksum = 0;

    if (ps <= 0) {
        printf("%s,growth_ladder,0,0,0,0,0\n", LEG_NAME);
        return 1;
    }
    page = (size_t)ps;

    t0 = now_ns();
    p = M_MALLOC(RUNGS[0]);
    if (p == NULL) {
        printf("%s,growth_ladder,0,0,0,0,0\n", LEG_NAME);
        return 1;
    }
    memset(p, step_byte(0), RUNGS[0]);
    for (step = 1; step < NRUNGS; step++) {
        unsigned char *q;
        if (!verify_extent(p, RUNGS[step - 1], step_byte(step - 1), page)) {
            printf("%s,growth_ladder,0,0,0,0,0\n", LEG_NAME);
            M_FREE(p);
            return 1;
        }
        q = M_REALLOC(p, RUNGS[step]);
        if (q == NULL) {
            printf("%s,growth_ladder,0,0,0,0,0\n", LEG_NAME);
            M_FREE(p);
            return 1;
        }
        if (q == p) {
            inplace++;
        }
        p = q;
        /* Pattern-touch the whole grown extent (Z-004:
         * every block is pattern-touched). */
        memset(p, step_byte(step), RUNGS[step]);
    }
    if (!verify_extent(p, RUNGS[NRUNGS - 1], step_byte(NRUNGS - 1), page)) {
        printf("%s,growth_ladder,0,0,0,0,0\n", LEG_NAME);
        M_FREE(p);
        return 1;
    }
    {
        size_t off;
        for (off = 0; off < RUNGS[NRUNGS - 1]; off += page) {
            size_t last = off + page - 1 < RUNGS[NRUNGS - 1] ?
                off + page - 1 : RUNGS[NRUNGS - 1] - 1;
            checksum += p[off] + p[last];
        }
    }
    M_FREE(p);
    t1 = now_ns();
    {
        uint64_t elapsed = t1 - t0;
        printf("%s,growth_ladder,%zu,%llu,%.2f,%d,%llu\n", LEG_NAME,
               NRUNGS, (unsigned long long)elapsed,
               (double)elapsed / (double)NRUNGS, inplace,
               (unsigned long long)checksum);
    }
    return 0;
}
