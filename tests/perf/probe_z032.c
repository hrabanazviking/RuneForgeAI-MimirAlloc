/* probe_z032.c - Z-032 size-class spacing study (MimirAlloc Z13).
 * MimirAlloc leg only (the glibc comparator distributions are
 * already banked in Z-017; this study audits MimirAlloc's own
 * 20-class table on this machine). NO timing is measured: the
 * probe records distributions and geometry only, so contention
 * cannot shift its results (the Z-017 precedent).
 *
 * Modes:
 *   map          for EVERY request size r in 1..1024, allocate
 *                6 blocks of size r and derive the serving class
 *                from the minimum absolute stride between
 *                consecutive returned pointers (carve order and
 *                free-list LIFO order both step by exactly the
 *                class size within a span; a span crossing is a
 *                strictly larger jump). Prints MAP lines.
 *   span <c>     in a fresh process, allocate blocks of exactly
 *                class size c and count how many land on the
 *                first block's 4096-byte page before the first
 *                block on a different page: that count is the
 *                objects-per-span for c. Prints SPAN line.
 *   cache        for each exact class size, 2048 blocks live at
 *                once, two rounds (fresh carve, then free-list
 *                reuse after freeing all): ptr mod 64 buckets,
 *                sub-16 count, cacheline-split count for the
 *                requested extent, min alignment. Prints TOTAL
 *                lines in the Z-017 shape.
 * Exit 0 iff no NULL and no sub-16-aligned pointer anywhere. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mimir_alloc.h"
static void *leg_alloc(size_t n) { return mimir_malloc(n); }
static void leg_free(void *p) { mimir_free(p); }

#define NCLASS 20
static const size_t classes[NCLASS] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};

static volatile unsigned long sink;

static int mode_map(void)
{
    int bad = 0;
    size_t r;
    printf("mode: map  requests: 1..1024  blocks_per_request: 6\n");
    for (r = 1; r <= 1024; r++) {
        void *p[6];
        size_t i, best = 0;
        int sub16 = 0;
        for (i = 0; i < 6; i++) {
            p[i] = leg_alloc(r);
            if (p[i] == NULL) { bad = 1; printf("MAP %zu NULL\n", r); break; }
            ((volatile unsigned char *)p[i])[0] = (unsigned char)r;
            ((volatile unsigned char *)p[i])[r - 1] = (unsigned char)(r >> 4);
            if (((uintptr_t)p[i] % 16) != 0) sub16 = 1;
        }
        if (bad) break;
        for (i = 0; i < 5; i++) {
            intptr_t d = (intptr_t)((char *)p[i + 1] - (char *)p[i]);
            size_t ad = (size_t)(d < 0 ? -d : d);
            if (ad != 0 && (best == 0 || ad < best)) best = ad;
        }
        printf("MAP %zu %zu sub16=%d\n", r, best, sub16);
        if (sub16) bad = 1;
        for (i = 0; i < 6; i++) leg_free(p[i]);
    }
    printf("MAPDONE bad=%d\n", bad);
    return bad;
}

static int mode_span(size_t c)
{
    /* allocate 4 full spans' worth; count the first page run */
    size_t n = 4096 / c * 4 + 16, i, first_run = 0;
    void **p = (void **)malloc(n * sizeof(void *));
    uintptr_t page0;
    int bad = 0, sub16 = 0;
    if (p == NULL) return 1;
    for (i = 0; i < n; i++) {
        p[i] = leg_alloc(c);
        if (p[i] == NULL) { bad = 1; break; }
        ((volatile unsigned char *)p[i])[0] = 1;
        ((volatile unsigned char *)p[i])[c - 1] = 2;
        if (((uintptr_t)p[i] % 16) != 0) sub16 = 1;
    }
    if (!bad) {
        page0 = (uintptr_t)p[0] & ~(uintptr_t)4095;
        while (first_run < n &&
               (((uintptr_t)p[first_run] & ~(uintptr_t)4095) == page0))
            first_run++;
        printf("SPAN %zu objs=%zu first_off=%zu sub16=%d n=%zu\n",
               c, first_run,
               (size_t)((uintptr_t)p[0] & (uintptr_t)4095), sub16, n);
        if (sub16) bad = 1;
    }
    for (i = 0; i < n; i++) if (p[i]) leg_free(p[i]);
    free(p);
    printf("SPANDONE %zu bad=%d\n", c, bad);
    return bad;
}

#define CNBLK 2048
#define CROUNDS 2
static void *cptrs[CNBLK];

static int mode_cache(void)
{
    int bad = 0, ci, round;
    printf("mode: cache  classes: %d  blocks_per_class_per_round: %d  rounds: %d  line: 64\n",
           NCLASS, CNBLK, CROUNDS);
    for (ci = 0; ci < NCLASS; ci++) {
        size_t c = classes[ci];
        long tot_mod[4] = {0, 0, 0, 0};
        long tot_split = 0, tot_sub16 = 0;
        int tot_minal = 64;
        for (round = 0; round < CROUNDS; round++) {
            long mod[4] = {0, 0, 0, 0}, split = 0, sub16 = 0;
            int minal = 64, i;
            unsigned long acc = 0;
            for (i = 0; i < CNBLK; i++) {
                void *q = leg_alloc(c);
                size_t m64;
                if (q == NULL) { bad = 1; break; }
                cptrs[i] = q;
                ((volatile unsigned char *)q)[0] = (unsigned char)i;
                ((volatile unsigned char *)q)[c - 1] = (unsigned char)(i >> 3);
                m64 = (size_t)((uintptr_t)q % 64);
                mod[m64 / 16]++;
                if (((uintptr_t)q % 16) != 0) sub16++;
                if (m64 + c > 64) split++;
                { size_t al = 16; while (al * 2 <= 4096 && ((uintptr_t)q % (al * 2)) == 0) al *= 2; if ((int)al < minal) minal = (int)al; }
                acc += m64;
            }
            sink += acc;
            printf("round %d class %4zu: n=%d mod64_0=%ld mod64_16=%ld mod64_32=%ld mod64_48=%ld sub16=%ld split=%ld min_align=%d\n",
                   round + 1, c, CNBLK, mod[0], mod[1], mod[2], mod[3],
                   sub16, split, minal);
            for (i = 0; i < 4; i++) tot_mod[i] += mod[i];
            tot_split += split; tot_sub16 += sub16;
            if (minal < tot_minal) tot_minal = minal;
            for (i = 0; i < CNBLK; i++) leg_free(cptrs[i]);
            if (bad) break;
        }
        printf("TOTAL class %4zu: n=%d mod64_0=%ld mod64_16=%ld mod64_32=%ld mod64_48=%ld sub16=%ld split=%ld split_pct=%.2f min_align=%d\n",
               c, CNBLK * CROUNDS, tot_mod[0], tot_mod[1], tot_mod[2],
               tot_mod[3], tot_sub16, tot_split,
               100.0 * (double)tot_split / (double)(CNBLK * CROUNDS),
               tot_minal);
        if (tot_sub16) bad = 1;
    }
    printf("CACHEDONE bad=%d\n", bad);
    return bad;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "map") == 0) return mode_map();
    if (argc >= 3 && strcmp(argv[1], "span") == 0) return mode_span((size_t)atol(argv[2]));
    if (argc >= 2 && strcmp(argv[1], "cache") == 0) return mode_cache();
    fprintf(stderr, "usage: probe_z032 map|cache|span <class>\n");
    return 2;
}
