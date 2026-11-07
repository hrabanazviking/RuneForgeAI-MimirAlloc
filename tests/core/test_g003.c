/*
 * test_g003.c -- evidence test for the G-003 Phase 7
 * small-object path (docs/PHASE_7_SMALL_OBJECT_PATH_G003.md;
 * design: docs/PHASE_7_SIZE_CLASSES_G002.md).
 *
 * Run standalone via `make test-g003` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-g001 pattern). It checks, through the four public
 * symbols plus the internal audit counters (E-005 mapping
 * counters via tests/abi/mimir_audit_e005.h, G-003
 * span/object counters via mimir_small_g003.h):
 *
 *   S1 routing (R-1): small sizes move NO mapping counter
 *      and move the small counter; large sizes do the
 *      reverse.
 *   S2 every class size returns a 16-aligned pointer.
 *   S3 simultaneous small blocks: pattern integrity over
 *      the requested extents and pairwise non-overlap.
 *   S4 bounded reuse: 500 alloc/free cycles of one class
 *      publish NO new span; many-object batches work; after
 *      everything is freed, retained empty spans stay
 *      within the per-class bound (2 x 20 classes).
 *   S5 two-registry free path (R-4): free of a span object
 *      moves only the small counter; free of a mapping
 *      block moves only the mapping counter; free(NULL),
 *      an interior pointer, a span-header pointer and a
 *      double free move nothing and disturb nothing
 *      (posture probes in the C-008 idiom: invalid frees
 *      are UB at contract level; these record THIS
 *      implementation's non-destructive course only).
 *   S6 calloc on the small path (R-2): a reused block is
 *      zero over the whole class extent, link bytes
 *      included; large calloc keeps the banked behaviour;
 *      overflow is exactly NULL.
 *   S7 realloc (R-3 / B-008): within-class stay, shrink
 *      stay, cross-class move, small-to-large move,
 *      large shrink stay (the banked B-008 posture),
 *      failure atomicity, realloc(NULL) as malloc.
 *   S8 the large path is unchanged: patterns, alignment,
 *      mapping counters.
 *   S9 a small latency leg in the G-001 probe shape
 *      (alloc + touch + free, p50) for two small sizes and
 *      one large size, printed for comparison against the
 *      banked G-001 baseline -- printed facts only, no
 *      latency pass/fail on a shared host.
 *
 * Final accounting: live small objects 0, live mappings 0,
 * live spans within the retention bound.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */

#define _POSIX_C_SOURCE 200809L

#include "mimir_alloc.h"
#include "mimir_small_g003.h"
#include "../abi/mimir_audit_e005.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int failures = 0;
static int checks = 0;

static void check(int condition, const char *label)
{
    checks++;
    printf("  [%s] %s\n", condition ? "ok" : "FAIL", label);
    if (!condition) {
        failures++;
    }
}

static void fill_pattern(unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)((i * 31u + seed * 17u + (i >> 8)) & 0xffu);
    }
}

static int pattern_ok(const unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        unsigned char want =
            (unsigned char)((i * 31u + seed * 17u + (i >> 8)) & 0xffu);
        if (p[i] != want) {
            return 0;
        }
    }
    return 1;
}

static int ranges_overlap(const unsigned char *a, size_t an,
                          const unsigned char *b, size_t bn)
{
    uintptr_t a0 = (uintptr_t)a;
    uintptr_t b0 = (uintptr_t)b;
    return a0 < b0 + bn && b0 < a0 + an;
}

static unsigned long live_spans(void)
{
    return mimir_audit_span_published - mimir_audit_span_retired;
}

static const size_t class_sizes[20] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};

/* S9 latency leg, G-001 probe shape. */
#define LAT_OPS 2000
static long long samples[LAT_OPS];

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

static void latency_for(size_t size)
{
    long long sum = 0;
    int fails = 0;
    int i;
    for (i = 0; i < LAT_OPS; i++) {
        long long t0 = now_ns();
        void *p = mimir_malloc(size);
        if (p) {
            ((volatile char *)p)[0] = (char)i;
            mimir_free(p);
        } else {
            fails++;
        }
        samples[i] = now_ns() - t0;
        sum += samples[i];
    }
    qsort(samples, LAT_OPS, sizeof(long long), cmp_ll);
    printf("  LAT size=%zu ops=%d fails=%d min_ns=%lld p50_ns=%lld "
           "p90_ns=%lld p99_ns=%lld max_ns=%lld mean_ns=%lld\n",
           size, LAT_OPS, fails, samples[0], samples[LAT_OPS / 2],
           samples[(LAT_OPS * 90) / 100], samples[(LAT_OPS * 99) / 100],
           samples[LAT_OPS - 1], sum / LAT_OPS);
    check(fails == 0, "latency leg allocations all succeeded");
}

int main(void)
{
    long queried = sysconf(_SC_PAGESIZE);
    size_t page = queried > 0 ? (size_t)queried : 0;
    unsigned long map_pub0 = mimir_audit_alloc_published;
    unsigned long map_ret0 = mimir_audit_mapping_retired;
    unsigned long small_alloc0 = mimir_audit_small_alloc;
    unsigned long small_free0 = mimir_audit_small_free;
    size_t i, j;

    printf("G-003 small-object path test\n");
    printf("runtime system page size: %zu bytes\n", page);
    check(page > 0, "runtime page size discovered");

    /* ---- S1 routing ---- */
    printf("S1 routing (R-1)\n");
    {
        static const size_t small_sizes[] = {1, 16, 17, 100, 1024};
        static const size_t large_sizes[] = {1025, 2048, 4096};
        unsigned long mp, sa;
        void *ptrs[8];
        int ok = 1;

        mp = mimir_audit_alloc_published;
        sa = mimir_audit_small_alloc;
        for (i = 0; i < 5; i++) {
            ptrs[i] = mimir_malloc(small_sizes[i]);
            if (!ptrs[i]) ok = 0;
        }
        check(ok, "small sizes all allocated");
        check(mimir_audit_alloc_published - mp == 0,
              "small sizes published no mapping");
        check(mimir_audit_small_alloc - sa == 5,
              "small sizes served by the class path (5)");
        for (i = 0; i < 5; i++) mimir_free(ptrs[i]);

        mp = mimir_audit_alloc_published;
        sa = mimir_audit_small_alloc;
        ok = 1;
        for (i = 0; i < 3; i++) {
            ptrs[i] = mimir_malloc(large_sizes[i]);
            if (!ptrs[i]) ok = 0;
        }
        check(ok, "large sizes all allocated");
        check(mimir_audit_alloc_published - mp == 3,
              "large sizes published mappings (3)");
        check(mimir_audit_small_alloc - sa == 0,
              "large sizes never touch the class path");
        for (i = 0; i < 3; i++) mimir_free(ptrs[i]);
    }

    /* ---- S2 alignment for every class ---- */
    printf("S2 alignment (every class, floor 16)\n");
    {
        void *ptrs[21];
        int ok = 1;
        for (i = 0; i < 20; i++) {
            ptrs[i] = mimir_malloc(class_sizes[i]);
            if (!ptrs[i] || (uintptr_t)ptrs[i] % 16 != 0) ok = 0;
        }
        ptrs[20] = mimir_malloc(1);
        if (!ptrs[20] || (uintptr_t)ptrs[20] % 16 != 0) ok = 0;
        check(ok, "all 20 class sizes + size 1 return 16-aligned");
        for (i = 0; i < 21; i++) mimir_free(ptrs[i]);
    }

    /* ---- S3 patterns + non-overlap ---- */
    printf("S3 pattern integrity and non-overlap\n");
    {
        static const size_t mix[] = {16, 32, 64, 100, 128, 300, 512, 1000};
        void *blocks[48];
        size_t sizes[48];
        int ok = 1, overlap = 0;
        for (i = 0; i < 48; i++) {
            sizes[i] = mix[i % 8];
            blocks[i] = mimir_malloc(sizes[i]);
            if (!blocks[i]) { ok = 0; break; }
            fill_pattern(blocks[i], sizes[i], (unsigned int)(i + 1));
        }
        check(ok, "48 mixed small blocks allocated");
        if (ok) {
            for (i = 0; i < 48; i++) {
                if (!pattern_ok(blocks[i], sizes[i],
                                (unsigned int)(i + 1))) ok = 0;
                for (j = i + 1; j < 48; j++) {
                    if (ranges_overlap(blocks[i], sizes[i],
                                       blocks[j], sizes[j])) overlap = 1;
                }
            }
            check(ok, "all 48 patterns intact after all allocations");
            check(!overlap, "no two live small blocks overlap");
        }
        for (i = 0; i < 48; i++) mimir_free(blocks[i]);
    }

    /* ---- S4 bounded reuse ---- */
    printf("S4 bounded reuse\n");
    {
        void *p = mimir_malloc(64);
        unsigned long pub_before;
        unsigned long spans_before = live_spans();
        size_t cap64 = page > 96 ? (page - 96) / 64 : 0;
        size_t n = cap64 + 10;
        void **batch;
        int ok = 1, overlap = 0;

        check(p != NULL, "warm-up allocation");
        mimir_free(p);
        pub_before = mimir_audit_span_published;
        for (i = 0; i < 500; i++) {
            p = mimir_malloc(64);
            if (!p) { ok = 0; break; }
            mimir_free(p);
        }
        check(ok, "500 alloc/free cycles of size 64");
        check(mimir_audit_span_published == pub_before,
              "500 cycles published no new span (reuse is real)");

        batch = mimir_calloc(n, sizeof(void *));
        /* batch itself is a large (mapping) allocation on
         * this host's page size; that is fine, it is test
         * scaffolding, freed at the end of the section. */
        check(batch != NULL, "batch scaffolding allocated");
        if (batch) {
            for (i = 0; i < n; i++) {
                batch[i] = mimir_malloc(64);
                if (!batch[i]) { ok = 0; break; }
                fill_pattern(batch[i], 64, (unsigned int)(i + 7));
            }
            check(ok, "capacity+10 blocks of size 64 live at once");
            if (ok) {
                for (i = 0; i < n; i++) {
                    if (!pattern_ok(batch[i], 64,
                                    (unsigned int)(i + 7))) ok = 0;
                    for (j = i + 1; j < n; j++) {
                        if (ranges_overlap(batch[i], 64,
                                           batch[j], 64)) overlap = 1;
                    }
                }
                check(ok, "batch patterns intact");
                check(!overlap, "batch blocks pairwise non-overlapping");
            }
            for (i = 0; i < n; i++) mimir_free(batch[i]);
            mimir_free(batch);
        }
        {
            static void *flood[800];
            unsigned long ret_before = mimir_audit_span_retired;
            int fok = 1;
            for (i = 0; i < 800; i++) {
                flood[i] = mimir_malloc(64);
                if (!flood[i]) { fok = 0; break; }
            }
            check(fok, "800 blocks of size 64 live at once "
                       "(forces more spans than the bound)");
            for (i = 0; i < 800; i++) mimir_free(flood[i]);
            check(mimir_audit_span_retired > ret_before,
                  "empty spans beyond the bound are released whole");
        }
        check(live_spans() <= spans_before + 2,
              "class-64 empty-span retention within bound (+2)");
        check(live_spans() <= 40,
              "total retained spans within 20 classes x 2 bound");
    }

    /* ---- S5 two-registry free path ---- */
    printf("S5 free-path routing (R-4)\n");
    {
        unsigned long sf, mr;
        void *s = mimir_malloc(100);
        void *l = mimir_malloc(5000);
        check(s != NULL && l != NULL, "one small + one large live");

        sf = mimir_audit_small_free;
        mr = mimir_audit_mapping_retired;
        mimir_free(s);
        check(mimir_audit_small_free - sf == 1 &&
              mimir_audit_mapping_retired - mr == 0,
              "free(small) retires via the span registry only");
        mimir_free(l);
        check(mimir_audit_mapping_retired - mr == 1 &&
              mimir_audit_small_free - sf == 1,
              "free(large) retires via the mapping registry only");

        sf = mimir_audit_small_free;
        mr = mimir_audit_mapping_retired;
        mimir_free(NULL);
        check(mimir_audit_small_free == sf &&
              mimir_audit_mapping_retired == mr,
              "free(NULL) moves no counter");

        /* Posture probes (C-008 idiom; invalid frees are UB
         * at contract level, outcomes promised to no one). */
        {
            unsigned char *q = mimir_malloc(100);
            void *base;
            check(q != NULL, "posture-probe block allocated");
            fill_pattern(q, 100, 42);
            mimir_free(q + 8);
            check(mimir_audit_small_free == sf &&
                  pattern_ok(q, 100, 42),
                  "interior free (q+8) is a non-destructive no-op");
            base = (void *)((uintptr_t)q & ~(uintptr_t)(page - 1));
            mimir_free(base);
            check(mimir_audit_small_free == sf &&
                  pattern_ok(q, 100, 42),
                  "span-header pointer free is a no-op (INC-G002-05)");
            mimir_free(q);
            check(mimir_audit_small_free - sf == 1,
                  "the probed block still frees exactly once");
            mimir_free(q);
            check(mimir_audit_small_free - sf == 1,
                  "double free is a non-destructive no-op");
        }
    }

    /* ---- S6 calloc ---- */
    printf("S6 calloc on the small path (R-2)\n");
    {
        unsigned char *x = mimir_malloc(100);
        unsigned char *y = mimir_malloc(100);
        unsigned char *b;
        int zero = 1;
        check(x != NULL && y != NULL, "two class-112 blocks live");
        memset(x, 0xFF, 112); /* the class extent (white-box) */
        mimir_free(x);
        b = mimir_calloc(10, 10);
        check(b == x, "calloc reuses the freed block (free-list head)");
        if (b) {
            for (i = 0; i < 112; i++) {
                if (b[i] != 0) zero = 0;
            }
            check(zero, "reused calloc block zero over the class extent");
            mimir_free(b);
        }
        mimir_free(y);
        {
            unsigned long mp = mimir_audit_alloc_published;
            unsigned char *big = mimir_calloc(1, 2000);
            int bigzero = 1;
            check(big != NULL &&
                  mimir_audit_alloc_published - mp == 1,
                  "large calloc takes the mapping path");
            if (big) {
                for (i = 0; i < 2000; i++) {
                    if (big[i] != 0) bigzero = 0;
                }
                check(bigzero, "large calloc zero over requested extent");
                mimir_free(big);
            }
        }
        check(mimir_calloc(SIZE_MAX, 2) == NULL,
              "calloc overflow is exactly NULL");
    }

    /* ---- S7 realloc ---- */
    printf("S7 realloc (R-3 / B-008)\n");
    {
        unsigned char *p = mimir_malloc(50);
        unsigned char *q;
        check(p != NULL, "realloc subject allocated (50, class 64)");
        fill_pattern(p, 50, 3);
        q = mimir_realloc(p, 60);
        check(q == p && pattern_ok(q, 50, 3),
              "realloc within class stays, contents preserved");
        q = mimir_realloc(q, 20);
        check(q == p && pattern_ok(q, 20, 3),
              "realloc shrink stays, prefix preserved");
        mimir_free(q);

        {
            unsigned char *r = mimir_malloc(50);
            unsigned char *s2;
            unsigned long sa = mimir_audit_small_alloc;
            unsigned long sf = mimir_audit_small_free;
            fill_pattern(r, 50, 9);
            s2 = mimir_realloc(r, 200);
            check(s2 != NULL && s2 != r && pattern_ok(s2, 50, 9),
                  "cross-class realloc moves, prefix preserved");
            check(mimir_audit_small_alloc - sa == 1 &&
                  mimir_audit_small_free - sf == 1,
                  "cross-class move: one small construction, one retire");
            mimir_free(s2);
        }
        {
            unsigned char *t = mimir_malloc(100);
            unsigned char *u;
            unsigned long mp = mimir_audit_alloc_published;
            unsigned long sf = mimir_audit_small_free;
            fill_pattern(t, 100, 11);
            u = mimir_realloc(t, 5000);
            check(u != NULL && pattern_ok(u, 100, 11),
                  "small-to-large realloc moves, prefix preserved");
            check(mimir_audit_alloc_published - mp == 1 &&
                  mimir_audit_small_free - sf == 1,
                  "small-to-large: mapping published, span object retired");
            mimir_free(u);
        }
        {
            unsigned char *v = mimir_malloc(3000);
            unsigned char *w;
            unsigned long sa = mimir_audit_small_alloc;
            fill_pattern(v, 3000, 13);
            w = mimir_realloc(v, 100);
            check(w == v && pattern_ok(w, 100, 13) &&
                  mimir_audit_small_alloc == sa,
                  "large shrink stays on its mapping (B-008 posture)");
            mimir_free(w);
        }
        {
            unsigned char *p2 = mimir_malloc(80);
            fill_pattern(p2, 80, 17);
            check(mimir_realloc(p2, SIZE_MAX) == NULL &&
                  pattern_ok(p2, 80, 17),
                  "realloc failure is atomic: original intact");
            mimir_free(p2);
        }
        {
            unsigned long sa = mimir_audit_small_alloc;
            void *n2 = mimir_realloc(NULL, 64);
            check(n2 != NULL && mimir_audit_small_alloc - sa == 1,
                  "realloc(NULL, small) behaves as small malloc");
            mimir_free(n2);
        }
    }

    /* ---- S8 large path unchanged ---- */
    printf("S8 large path unchanged\n");
    {
        static const size_t sizes[] = {1025, 4096, 10000};
        int ok = 1;
        for (i = 0; i < 3; i++) {
            unsigned char *p = mimir_malloc(sizes[i]);
            if (!p || (uintptr_t)p % 16 != 0) { ok = 0; break; }
            fill_pattern(p, sizes[i], (unsigned int)(30 + i));
            if (!pattern_ok(p, sizes[i], (unsigned int)(30 + i))) ok = 0;
            mimir_free(p);
        }
        check(ok, "large sizes: aligned, pattern-intact, freed");
    }

    /* ---- S9 latency leg ---- */
    printf("S9 latency (G-001 probe shape; G-001 banked baseline: "
           "mapping path p50 4889-5167 ns at every size 8-4096)\n");
    latency_for(64);
    latency_for(512);
    latency_for(2048);

    /* ---- final accounting ---- */
    printf("final accounting\n");
    check(mimir_audit_small_alloc - small_alloc0 ==
          mimir_audit_small_free - small_free0,
          "live small objects return to zero");
    check(mimir_audit_alloc_published - map_pub0 ==
          mimir_audit_mapping_retired - map_ret0,
          "live mappings return to zero");
    check(live_spans() <= 40, "live spans within the retention bound");
    printf("live_spans=%lu live_span_publications=%lu "
           "span_retirements=%lu small_allocs=%lu small_frees=%lu\n",
           live_spans(),
           mimir_audit_span_published - 0,
           mimir_audit_span_retired - 0,
           mimir_audit_small_alloc - small_alloc0,
           mimir_audit_small_free - small_free0);
    printf("checks=%d failures=%d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
