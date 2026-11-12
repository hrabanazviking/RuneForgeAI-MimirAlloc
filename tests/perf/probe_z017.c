/* probe_z017.c - Z-017 alignment audit.
 * Dual-leg probe (bench_k002 pattern): built twice from this
 * one source - glibc leg (-DLEG_GLIBC, stdlib malloc/free)
 * and MimirAlloc leg (mimir_malloc/mimir_free, linked with
 * the production LIBOBJS). For each of the 20 small size
 * classes (16..1024, the mimir_class_sizes table in
 * src/core/mimir_core.c) the probe requests exactly the
 * class size, holds N blocks live at once, and records the
 * returned-pointer alignment distribution:
 *   - offset of each pointer modulo 64 (the cacheline size
 *     on this host), bucketed in 16-byte steps;
 *   - any pointer not 16-byte aligned (sub16: a contract
 *     violation on x86-64 for either allocator);
 *   - cacheline-split count for the requested extent: a
 *     block splits a line iff (p % 64) + req > 64. For
 *     req > 64 every block spans lines by size alone; the
 *     split column is still printed but the recorded run
 *     grades only req <= 64 classes for Wave 4 naming.
 * ROUNDS rounds per class: round 1 is pure carve order;
 * round 2 runs after freeing every block, so it measures
 * the free-list reuse distribution. Every block is touched
 * (first and last byte) so allocations cannot be elided.
 * Exit 0 iff no NULL and no sub-16 pointer in any class. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef LEG_GLIBC
#include <malloc.h>
static void *leg_alloc(size_t n) { return malloc(n); }
static void leg_free(void *p) { free(p); }
static const char *LEG_NAME = "glibc";
#else
#include "mimir_alloc.h"
static void *leg_alloc(size_t n) { return mimir_malloc(n); }
static void leg_free(void *p) { mimir_free(p); }
static const char *LEG_NAME = "mimir";
#endif

#define NCLASS 20
#define NBLK 4096
#define ROUNDS 2
#define LINE 64u

static const size_t classes[NCLASS] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};

static void *ptrs[NBLK];
static volatile unsigned long sink;

int main(void)
{
    int bad = 0;
    printf("leg: %s\n", LEG_NAME);
    printf("classes: %d  blocks_per_class_per_round: %d  rounds: %d"
           "  line: %u\n", NCLASS, NBLK, ROUNDS, LINE);
    for (int ci = 0; ci < NCLASS; ci++) {
        size_t req = classes[ci];
        /* totals across rounds */
        long tot_n = 0, tot_sub16 = 0, tot_split = 0;
        long tot_mod[4] = {0, 0, 0, 0}; /* p%64 in {0,16,32,48} */
        unsigned tot_minctz = 64;
#ifdef LEG_GLIBC
        double usable_sum = 0.0;
#endif
        for (int round = 0; round < ROUNDS; round++) {
            long n = 0, sub16 = 0, split = 0;
            long mod[4] = {0, 0, 0, 0};
            unsigned minctz = 64;
            for (int i = 0; i < NBLK; i++) {
                void *p = leg_alloc(req);
                ptrs[i] = p;
                if (p == NULL) { bad = 1; continue; }
                ((volatile unsigned char *)p)[0] =
                    (unsigned char)i;
                ((volatile unsigned char *)p)[req - 1] =
                    (unsigned char)(i >> 3);
                uintptr_t a = (uintptr_t)p;
                unsigned off = (unsigned)(a % LINE);
                if (a % 16 != 0) sub16++;
                else mod[off / 16]++;
                if (off + req > LINE) split++;
                unsigned ctz = (unsigned)__builtin_ctzll(
                    (unsigned long long)a);
                if (ctz < minctz) minctz = ctz;
#ifdef LEG_GLIBC
                usable_sum += (double)malloc_usable_size(p);
#endif
                n++;
            }
            if (ptrs[0] != NULL)
                sink += ((volatile unsigned char *)ptrs[0])[0];
            printf("round %d class %4zu: n=%ld mod64_0=%ld"
                   " mod64_16=%ld mod64_32=%ld mod64_48=%ld"
                   " sub16=%ld split=%ld min_align=%u\n",
                   round + 1, req, n, mod[0], mod[1], mod[2],
                   mod[3], sub16, split,
                   minctz >= 64 ? 0u : (1u << minctz));
            tot_n += n; tot_sub16 += sub16; tot_split += split;
            for (int b = 0; b < 4; b++) tot_mod[b] += mod[b];
            if (minctz < tot_minctz) tot_minctz = minctz;
            if (sub16 > 0) bad = 1;
            for (int i = 0; i < NBLK; i++) {
                if (ptrs[i] != NULL) leg_free(ptrs[i]);
                ptrs[i] = NULL;
            }
        }
        printf("TOTAL class %4zu: n=%ld mod64_0=%ld mod64_16=%ld"
               " mod64_32=%ld mod64_48=%ld sub16=%ld split=%ld"
               " split_pct=%.2f a64_pct=%.2f min_align=%u",
               req, tot_n, tot_mod[0], tot_mod[1], tot_mod[2],
               tot_mod[3], tot_sub16, tot_split,
               tot_n ? 100.0 * (double)tot_split / (double)tot_n
                     : 0.0,
               tot_n ? 100.0 * (double)tot_mod[0] / (double)tot_n
                     : 0.0,
               tot_minctz >= 64 ? 0u : (1u << tot_minctz));
#ifdef LEG_GLIBC
        printf(" usable_avg=%.1f", tot_n
               ? usable_sum / (double)tot_n : 0.0);
#endif
        printf("\n");
    }
    printf("RESULT: %s (leg %s, sink=%lu)\n",
           bad ? "FAIL" : "PASS", LEG_NAME, sink);
    return bad ? 1 : 0;
}
