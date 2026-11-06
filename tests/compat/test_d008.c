/*
 * test_d008.c -- D-008 libc / alignment compatibility probe
 * (Wave 4, Phase 4 ARM64 and Android foundation).
 * See docs/LIBC_ALIGNMENT_COMPATIBILITY.md.
 *
 * Purpose: measure, on this host, what alignment the banked
 * allocator actually returns for the four public symbols
 * (mimir_malloc, mimir_free, mimir_calloc, mimir_realloc),
 * across ordinary, odd and large sizes and through the
 * calloc / realloc paths, and check every returned pointer
 * against the A-008 contract expectation carried by
 * include/mimir_alloc.h: "suitably aligned for any object
 * type with a fundamental alignment requirement"
 * (MA-6.1-MALLOC-01 by analogy). The operational form of
 * that non-numeric claim (U-15: no numeric value is
 * claimed) is _Alignof(max_align_t) from <stddef.h>:
 * every non-null returned pointer must satisfy
 * (uintptr_t)ptr % _Alignof(max_align_t) == 0. Stronger
 * moduli actually observed (e.g. page alignment) are
 * printed as OBSERVATIONS of this build, never promoted
 * to contract claims.
 *
 * Phases:
 *   ENV  host facts measured in this run: page size via
 *        sysconf, _Alignof/sizeof(max_align_t), pointer
 *        width, libc identification macros where the host
 *        headers expose them.
 *   A    mimir_malloc alignment ladder: 33 sizes (odd,
 *        boundary and large), all kept live at once,
 *        pairwise distinct, each alignment-checked, then
 *        freed.
 *   B    mimir_calloc paths: alignment + zeroed extent.
 *   C    mimir_realloc paths: grow / grow / shrink with
 *        prefix preservation + alignment at every step;
 *        realloc(NULL, n) acts as malloc.
 *   D    host system malloc / calloc / realloc over the
 *        same ladder, alignment-checked the same way.
 *        Host-libc evidence ONLY (this host: glibc): the
 *        contrast baseline for the compatibility doc,
 *        not a MimirAlloc claim.
 *   E    posture OBSERVATIONS, never checks and never
 *        cited as settling an open item: zero-size calls
 *        (U-09 / U-12 unresolved) and errno after a failed
 *        oversize call (U-10: errno is NOT promised for
 *        the mimir_ symbols; the printed value is whatever
 *        this run happened to leave in errno). Defined
 *        failures ARE checks: oversize malloc and calloc
 *        overflow must return exactly NULL.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Posture lines are prefixed
 * "OBSERVED" and do not affect the result.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mimir_alloc.h"

static int checks_run;
static int checks_failed;
static long allocs_done;
static long frees_done;

static void check(int ok, const char *label, size_t size,
                  const void *ptr)
{
    checks_run++;
    if (!ok) {
        checks_failed++;
        printf("  [FAIL] %s size=%zu ptr=%p\n", label, size, ptr);
    } else {
        printf("  [ok] %s size=%zu ptr=%p\n", label, size, ptr);
    }
}

static int aligned_max(const void *ptr)
{
    return ((uintptr_t)ptr % (uintptr_t)_Alignof(max_align_t)) == 0;
}

static const size_t ladder[] = {
    1, 2, 3, 5, 7, 8, 15, 16, 17, 24, 31, 32, 33, 63, 64, 65,
    100, 127, 128, 255, 256, 1000, 1024, 4095, 4096, 4097,
    8192, 16383, 16384, 16385, 65536, 100000, 262144
};
#define LADDER_N (sizeof ladder / sizeof ladder[0])

int main(void)
{
    long page;
    size_t i, j;
    void *live[LADDER_N];
    size_t page_aligned_count = 0;

    printf("D-008 libc/alignment compatibility probe\n");

    /* ENV */
    page = sysconf(_SC_PAGESIZE);
    printf("ENV pagesize=%ld alignof_max_align_t=%zu "
           "sizeof_max_align_t=%zu ptr_bits=%zu\n",
           page, (size_t)_Alignof(max_align_t),
           sizeof(max_align_t), sizeof(void *) * 8);
#if defined(__GLIBC__)
    printf("ENV libc=glibc __GLIBC__=%d __GLIBC_MINOR__=%d\n",
           __GLIBC__, __GLIBC_MINOR__);
#else
    printf("ENV libc=no __GLIBC__ macro (host headers)\n");
#endif
    check(page > 0, "ENV sysconf pagesize positive", 0, NULL);
    check(_Alignof(max_align_t) > 0, "ENV alignof positive", 0, NULL);

    /* Phase A: mimir_malloc ladder, all live at once. */
    printf("PHASE A mimir_malloc ladder (%zu sizes)\n", LADDER_N);
    for (i = 0; i < LADDER_N; i++) {
        live[i] = mimir_malloc(ladder[i]);
        if (live[i] != NULL)
            allocs_done++;
        check(live[i] != NULL, "A non-null", ladder[i], live[i]);
        check(live[i] != NULL && aligned_max(live[i]),
              "A aligned to max_align_t", ladder[i], live[i]);
        if (live[i] != NULL && page > 0 &&
            ((uintptr_t)live[i] % (uintptr_t)page) == 0)
            page_aligned_count++;
        if (live[i] != NULL)
            memset(live[i], 0xA5, ladder[i]);
    }
    for (i = 0; i < LADDER_N; i++) {
        int distinct = 1;
        for (j = i + 1; j < LADDER_N; j++)
            if (live[i] != NULL && live[i] == live[j])
                distinct = 0;
        check(distinct, "A pairwise distinct", ladder[i], live[i]);
    }
    printf("OBSERVED A page-modulus: %zu of %zu live pointers "
           "were 0 mod pagesize (observation, not a claim)\n",
           page_aligned_count, LADDER_N);
    for (i = 0; i < LADDER_N; i++) {
        mimir_free(live[i]);
        if (live[i] != NULL)
            frees_done++;
    }
    mimir_free(NULL);
    check(1, "A mimir_free(NULL) returned normally", 0, NULL);

    /* Phase B: mimir_calloc alignment + zeroing. */
    printf("PHASE B mimir_calloc paths\n");
    {
        static const struct { size_t n, s; } cases[] = {
            {1, 1}, {3, 7}, {16, 32}, {100, 100}, {1, 65537}
        };
        size_t k;
        for (k = 0; k < sizeof cases / sizeof cases[0]; k++) {
            size_t total = cases[k].n * cases[k].s;
            unsigned char *p = mimir_calloc(cases[k].n, cases[k].s);
            int zeroed = 1;
            size_t t;
            if (p != NULL)
                allocs_done++;
            check(p != NULL, "B calloc non-null", total, p);
            check(p != NULL && aligned_max(p),
                  "B calloc aligned to max_align_t", total, p);
            if (p != NULL) {
                for (t = 0; t < total; t++)
                    if (p[t] != 0)
                        zeroed = 0;
            }
            check(p != NULL && zeroed, "B calloc extent zeroed",
                  total, p);
            mimir_free(p);
            if (p != NULL)
                frees_done++;
        }
    }

    /* Phase C: mimir_realloc alignment + preservation. */
    printf("PHASE C mimir_realloc paths\n");
    {
        unsigned char *p = mimir_malloc(37);
        unsigned char *q;
        int preserved;
        size_t t;
        if (p != NULL)
            allocs_done++;
        check(p != NULL && aligned_max(p), "C seed aligned", 37, p);
        if (p != NULL)
            for (t = 0; t < 37; t++)
                p[t] = (unsigned char)(t + 1);
        q = mimir_realloc(p, 1000);
        if (q != NULL && q != p)
            allocs_done++;
        check(q != NULL && aligned_max(q), "C grow-1000 aligned",
              1000, q);
        preserved = 1;
        if (q != NULL)
            for (t = 0; t < 37; t++)
                if (q[t] != (unsigned char)(t + 1))
                    preserved = 0;
        check(q != NULL && preserved, "C grow preserves 37", 1000, q);
        p = q;
        if (p != NULL)
            for (t = 0; t < 1000; t++)
                p[t] = (unsigned char)(t * 7 + 3);
        q = mimir_realloc(p, 70000);
        if (q != NULL && q != p)
            allocs_done++;
        check(q != NULL && aligned_max(q), "C grow-70000 aligned",
              70000, q);
        preserved = 1;
        if (q != NULL)
            for (t = 0; t < 1000; t++)
                if (q[t] != (unsigned char)(t * 7 + 3))
                    preserved = 0;
        check(q != NULL && preserved, "C grow preserves 1000",
              70000, q);
        p = q;
        q = mimir_realloc(p, 13);
        check(q != NULL && aligned_max(q), "C shrink-13 aligned",
              13, q);
        preserved = 1;
        if (q != NULL)
            for (t = 0; t < 13; t++)
                if (q[t] != (unsigned char)(t * 7 + 3))
                    preserved = 0;
        check(q != NULL && preserved, "C shrink preserves 13", 13, q);
        mimir_free(q);
        if (q != NULL)
            frees_done++;
        q = mimir_realloc(NULL, 41);
        if (q != NULL)
            allocs_done++;
        check(q != NULL && aligned_max(q),
              "C realloc(NULL,41) aligned", 41, q);
        mimir_free(q);
        if (q != NULL)
            frees_done++;
    }

    /* Phase D: host system allocator, same ladder (glibc here). */
    printf("PHASE D host malloc/calloc/realloc ladder\n");
    for (i = 0; i < LADDER_N; i++) {
        void *p = malloc(ladder[i]);
        check(p != NULL && aligned_max(p),
              "D host malloc aligned to max_align_t", ladder[i], p);
        free(p);
    }
    {
        unsigned char *p = calloc(3, 7);
        check(p != NULL && aligned_max(p),
              "D host calloc aligned", 21, p);
        free(p);
        p = malloc(37);
        p = realloc(p, 1000);
        check(p != NULL && aligned_max(p),
              "D host realloc aligned", 1000, p);
        free(p);
    }

    /* Phase E: defined failures (checks) + postures (observed). */
    printf("PHASE E defined failures + observed postures\n");
    {
        void *p;
        int saved;
        p = mimir_malloc(SIZE_MAX);
        check(p == NULL, "E oversize malloc returns NULL", 0, p);
        p = mimir_calloc(SIZE_MAX / 2 + 1, 2);
        check(p == NULL, "E calloc overflow returns NULL", 0, p);
        p = mimir_calloc(2, SIZE_MAX / 2 + 1);
        check(p == NULL, "E calloc overflow swapped NULL", 0, p);

        /* Postures: printed, never checked, never contract. */
        p = mimir_malloc(0);
        printf("OBSERVED mimir_malloc(0) -> %p "
               "(U-09 unresolved; posture only)\n", p);
        mimir_free(p);
        p = mimir_calloc(0, 8);
        printf("OBSERVED mimir_calloc(0,8) -> %p "
               "(U-09 unresolved; posture only)\n", p);
        mimir_free(p);
        p = mimir_calloc(8, 0);
        printf("OBSERVED mimir_calloc(8,0) -> %p "
               "(U-09 unresolved; posture only)\n", p);
        mimir_free(p);
        {
            unsigned char *b = mimir_malloc(64);
            void *r;
            if (b != NULL) {
                allocs_done++;
                b[0] = 0x5A;
            }
            r = mimir_realloc(b, 0);
            printf("OBSERVED mimir_realloc(live,0) -> %p "
                   "(U-12 unresolved; posture only)\n", r);
            if (r == NULL && b != NULL) {
                printf("OBSERVED original block byte0=0x%02X "
                       "after realloc(live,0)\n", b[0]);
                mimir_free(b);
                frees_done++;
            } else if (r != NULL) {
                mimir_free(r);
            }
        }
        p = mimir_realloc(NULL, 0);
        printf("OBSERVED mimir_realloc(NULL,0) -> %p "
               "(U-09 unresolved; posture only)\n", p);
        mimir_free(p);

        errno = 0;
        p = mimir_malloc(SIZE_MAX);
        saved = errno;
        printf("OBSERVED errno after failed mimir_malloc"
               "(SIZE_MAX) = %d (U-10: errno NOT promised "
               "for mimir_ symbols; value is incidental)\n", saved);
        errno = 0;
        {
            /* Via a volatile so the glibc header's
             * alloc-size attribute diagnostic (which
             * -Werror promotes) does not fire on the
             * constant: the run-time call is identical.
             * See INC-D008-03. */
            volatile size_t huge = SIZE_MAX;
            p = malloc(huge);
        }
        saved = errno;
        printf("OBSERVED errno after failed host malloc"
               "(SIZE_MAX) = %d ENOMEM=%d (host libc fact, "
               "this host only)\n", saved, ENOMEM);
        free(p);
        errno = 0;
        p = malloc(0);
        printf("OBSERVED host malloc(0) -> %s (host libc "
               "fact, this host only)\n",
               p != NULL ? "non-null" : "NULL");
        free(p);
    }

    printf("CHECKS: %d run, %d failed\n", checks_run, checks_failed);
    printf("ALLOCATIONS: %ld FREES: %ld\n", allocs_done, frees_done);
    printf("RESULT: %s\n", checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
