/* probe_z033.c - Z-033 Size-class retune (conditional):
 * condition-independent harness.
 *
 * Z-032 (size-class spacing study) has NOT landed: no
 * adopted retune table exists on disk. This probe therefore
 * makes NO production change and measures the CURRENT table
 * as the BEFORE legs Z-033's gate needs once Z-032 lands:
 *
 *   TABLECHECK (mimir leg only) - behaviourally binds the
 *     mirror table below to the compiled core: mimir realloc
 *     stays iff new_size <= span class size (mimir_core.c),
 *     so for EVERY request size 1..1024: alloc(size)
 *     realloc(class) stays, realloc(class+1) moves (full
 *     sweep, not boundary-only). Any mismatch prints FAIL
 *     and the process exits nonzero. glibc leg prints SKIP
 *     (glibc classes are its own business).
 *   FRAGCLASS (mimir leg only) - exact arithmetic internal
 *     fragmentation of the mirror table: per class, the
 *     request range served, worst waste, uniform-mean waste
 *     (x100), worst waste as % of class (x100). The
 *     summarizer recomputes these from the TABLE line and
 *     cross-checks; disagreement fails the run.
 *   LAT CHURN  - alloc + touch ends + free, one block
 *     recycled, per boundary size (class-1, class, class+1,
 *     plus 1 and 1025), both legs (oracle legs).
 *   LAT LIVESET - timed alloc phase holding 512 blocks
 *     live, per boundary size, both legs.
 *
 * All allocator calls go through volatile function
 * pointers (Z-025 discipline). Checksums are a pure
 * function of size, identical across legs and processes.
 * Exit 0 iff no NULL and (mimir leg) no TABLECHECK fail.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define NCLASS 20
static const size_t CLASSES[NCLASS] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};

#ifdef LEG_GLIBC
static void *(*volatile vp_malloc)(size_t) = malloc;
static void *(*volatile vp_realloc)(void *, size_t) = realloc;
static void (*volatile vp_free)(void *) = free;
static const char *LEG = "glibc";
#else
#include "mimir_alloc.h"
static void *(*volatile vp_malloc)(size_t) = mimir_malloc;
static void *(*volatile vp_realloc)(void *, size_t) = mimir_realloc;
static void (*volatile vp_free)(void *) = mimir_free;
static const char *LEG = "mimir";
#endif

static volatile uint64_t SINK;
static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}
static unsigned char pat(size_t size, size_t i) {
    return (unsigned char)((size * 131u + i * 7u + 13u) & 0xffu);
}

#ifndef LEG_GLIBC
static size_t class_of(size_t s) {
    for (int i = 0; i < NCLASS; i++) if (s <= CLASSES[i]) return CLASSES[i];
    return 0;
}

static int tablecheck(void) {
    int fails = 0;
    /* Full sweep, every request size 1..1024 (review fold-in:
       boundary-only checks would miss a mis-mirrored interior).
       Core rule (mimir_core.c realloc): new_size <= span class
       returns the same pointer; otherwise alloc_any_locked runs
       BEFORE small_free_locked, so the new block is allocated
       while the old is still live and the pointer MUST differ. */
    for (size_t sz = 1; sz <= 1024; sz++) {
        size_t c = class_of(sz);
        void *p = vp_malloc(sz);
        if (!p) { printf("%s,TABLECHECK,FAIL,null_at_%zu\n", LEG, sz); fails++; continue; }
        void *q = vp_realloc(p, c);
        if (q != p) { printf("%s,TABLECHECK,FAIL,stay_sz_%zu_class_%zu\n", LEG, sz, c); fails++; vp_free(q); continue; }
        if (c < 1024) {
            void *r = vp_realloc(p, c + 1);
            if (r == p) { printf("%s,TABLECHECK,FAIL,move_sz_%zu_class_%zu\n", LEG, sz, c); fails++; }
            vp_free(r);
        } else {
            vp_free(p);
        }
    }
    void *big = vp_malloc(1025);
    if (!big) { printf("%s,TABLECHECK,FAIL,null_at_1025\n", LEG); fails++; }
    else vp_free(big);
    printf("%s,TABLECHECK,%s\n", LEG, fails ? "FAIL" : "OK");
    return fails;
}

static void fragclass(void) {
    printf("%s,TABLE", LEG);
    for (int i = 0; i < NCLASS; i++) printf(",%zu", CLASSES[i]);
    printf("\n");
    for (int i = 0; i < NCLASS; i++) {
        size_t c = CLASSES[i];
        size_t lo = (i == 0) ? 1 : CLASSES[i - 1] + 1;
        size_t hi = c;
        size_t worst = c - lo;
        /* uniform-mean waste x100 = (c - (lo+hi)/2) * 100
           = (2c - lo - hi) * 50, exact integer arithmetic */
        long mean_x100 = (long)(2 * c - lo - hi) * 50L;
        long pct_x100 = (long)(worst * 10000u / c);
        printf("%s,FRAGCLASS,%zu,%zu,%zu,%zu,%zu,%ld,%ld\n",
               LEG, c, lo, hi, hi - lo + 1, worst, mean_x100, pct_x100);
    }
}

#endif /* LEG_GLIBC */

/* boundary sizes: 1, and per class c-1, c, c+1 (<=1025), deduped */
static size_t boundary_sizes(size_t *out) {
    size_t n = 0;
    out[n++] = 1;
    for (int i = 0; i < NCLASS; i++) {
        size_t c = CLASSES[i];
        size_t cand[3] = { c - 1, c, c + 1 };
        for (int k = 0; k < 3; k++) {
            size_t s = cand[k];
            int seen = 0;
            for (size_t j = 0; j < n; j++) if (out[j] == s) seen = 1;
            if (!seen) out[n++] = s;
        }
    }
    return n; /* includes 1025 via class 1024 +1 */
}

static int lat_churn(size_t size, long iters) {
    uint64_t sum = 0;
    uint64_t t0 = now_ns();
    for (long it = 0; it < iters; it++) {
        unsigned char *p = vp_malloc(size);
        if (!p) return 1;
        p[0] = pat(size, 0);
        p[size - 1] = pat(size, size - 1);
        sum += p[0] + p[size - 1];
        vp_free(p);
    }
    uint64_t dt = now_ns() - t0;
    SINK += sum;
    printf("%s,LAT,CHURN,%zu,%llu,%llu\n", LEG, size,
           (unsigned long long)(dt / (uint64_t)iters),
           (unsigned long long)(sum % 1000003u));
    return 0;
}

static int lat_liveset(size_t size, long nblk) {
    static void *blk[512];
    if (nblk > 512) return 1;
    uint64_t sum = 0;
    uint64_t t0 = now_ns();
    for (long i = 0; i < nblk; i++) {
        unsigned char *p = vp_malloc(size);
        if (!p) return 1;
        p[0] = pat(size, 0);
        p[size - 1] = pat(size, size - 1);
        sum += p[0] + p[size - 1];
        blk[i] = p;
    }
    uint64_t dt = now_ns() - t0;
    for (long i = 0; i < nblk; i++) vp_free(blk[i]);
    SINK += sum;
    printf("%s,LAT,LIVESET,%zu,%llu,%llu\n", LEG, size,
           (unsigned long long)(dt / (uint64_t)nblk),
           (unsigned long long)(sum % 1000003u));
    return 0;
}

int main(void) {
    long pid = (long)getpid();
    printf("%s,PID,%ld\n", LEG, pid);
    int bad = 0;
#ifdef LEG_GLIBC
    printf("%s,TABLECHECK,SKIP\n", LEG);
    printf("%s,TABLE", LEG);
    for (int i = 0; i < NCLASS; i++) printf(",%zu", CLASSES[i]);
    printf("\n");
#else
    if (tablecheck()) bad = 1;
    fragclass();
#endif
    size_t sizes[64];
    size_t ns = boundary_sizes(sizes);
    for (size_t i = 0; i < ns; i++) {
        size_t s = sizes[i];
        long iters = (s <= 256) ? 20000 : (s <= 1024 ? 8000 : 1500);
        if (lat_churn(s, iters)) bad = 1;
        if (lat_liveset(s, 512)) bad = 1;
    }
    printf("%s,DONE,%llu\n", LEG, (unsigned long long)SINK);
    return bad ? 1 : 0;
}
