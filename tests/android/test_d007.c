/*
 * test_d007.c -- D-007 Android physical-device test program
 * (Wave 4, Phase 4 exit evidence, Android half). SOURCE ONLY
 * in this slice: ready to run, NOT yet run on any Android
 * device. See docs/PHASE_4_ANDROID_DEVICE_PACKET.md.
 *
 * Purpose: the small native Android program Roadmap Section 8
 * Phase 4 task T11 requires - it exercises the four public
 * mimir_ symbols (mimir_malloc, mimir_calloc, mimir_realloc,
 * mimir_free) through the allocate / fill / verify / free
 * pattern of the Research 03 device acceptance gate, at the
 * low concurrency and bounded allocations Roadmap Section 12
 * item 7 requires on the flagship tablet: at most 2 threads,
 * request sizes at most 8192 bytes, at most 8 blocks live at
 * once outside the cross-thread hand-off (25 blocks), fixed
 * iteration counts, seconds of runtime.
 *
 * The program is portable C11 + POSIX (stdio, string,
 * unistd sysconf, pthread) against include/mimir_alloc.h
 * only - the same surface the native suite uses - so the
 * identical source builds (a) natively on the Pi, which is
 * how this slice logic-checked it (a Linux/glibc run, NOT
 * Android evidence), and (b) with the Android NDK clang
 * driver per scripts/android/build_android.sh once an NDK
 * exists (D-006), for the on-device run that is still OWED.
 *
 * What it does, in phase order:
 *
 *   ENV   [run record]: prints the page size from
 *         sysconf(_SC_PAGESIZE) measured IN THIS RUN (the
 *         Phase 4 exit E3 record - a value copied from an
 *         inventory is not that record, D-001), the online
 *         processor count from sysconf(_SC_NPROCESSORS_ONLN)
 *         where it succeeds, and the pointer width. A failed
 *         sysconf is printed as FAILED, never guessed.
 *   A     [allocate/fill/verify/free]: 3 rounds over the
 *         size ladder {1, 16, 64, 256, 1024, 4096, 8192}:
 *         allocate, fill with a per-(round,size) pattern,
 *         verify every byte, free. In-process repetition
 *         only - repeated STARTUP/EXIT evidence is produced
 *         by running the binary itself repeatedly (packet
 *         Section 6), not by this loop.
 *   B     [live set]: 8 blocks allocated and filled, all
 *         verified while simultaneously live, pairwise
 *         distinct, each address aligned to
 *         _Alignof(max_align_t) (the non-numeric suitable-
 *         alignment claim of the header, U-15: no numeric
 *         alignment value is claimed), then freed.
 *   C     [calloc/realloc]: mimir_calloc(16, 32) reads zero
 *         at return over the full extent; a 128-byte block
 *         filled, grown by mimir_realloc to 512 with the
 *         preserved prefix verified byte-for-byte, freed;
 *         mimir_realloc(NULL, 64) acts as mimir_malloc.
 *   D     [failure]: mimir_malloc(SIZE_MAX) returns NULL
 *         (oversize refusal); mimir_free(NULL) returns
 *         normally (defined no-op). Zero-size behaviour is
 *         NOT checked: it is [UNRESOLVED: U-09] and this
 *         program must not be cited as settling it.
 *   E     [cross-thread free, 2 threads]: the producer
 *         thread allocates 25 blocks, fills and verifies
 *         them, and publishes them; the consumer thread
 *         verifies each again and frees it - blocks
 *         allocated on one thread freed on another - then
 *         makes one fresh allocation of its own (fill /
 *         verify / free). Bounded per Section 12 item 7.
 *
 * Contract position (honesty): the namespaced mimir_ API
 * has NO direct contract rows (U-03 open). Every check is
 * BY ANALOGY to the core-symbol rows cited in
 * include/mimir_alloc.h, Proposed only. A PASS on the
 * Samsung SM-X400 would be device evidence for the Android
 * ARM64 + bionic row; a PASS anywhere else (this Pi
 * included) is evidence for that host only and promotes
 * no Android claim.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed or
 * counted: deterministic CHECKS / ALLOCATIONS / FREES
 * totals for a given build.
 */

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "mimir_alloc.h"

static int passed = 0;
static int failed = 0;
static long allocs = 0; /* successful mimir_malloc/calloc/realloc-NULL returns */
static long frees = 0;  /* mimir_free calls on live blocks */

static void check(int condition, const char *name)
{
    if (condition) {
        passed++;
        printf("PASS %s\n", name);
    } else {
        failed++;
        printf("FAIL %s\n", name);
    }
}

static unsigned char pattern_byte(unsigned int seed, size_t i)
{
    return (unsigned char)((seed * 131u + (unsigned int)i * 7u + 3u) & 0xFFu);
}

static void fill_block(unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        p[i] = pattern_byte(seed, i);
    }
}

static int verify_block(const unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (p[i] != pattern_byte(seed, i)) {
            return 0;
        }
    }
    return 1;
}

/* Phase E hand-off: written by the producer before it exits,
 * read by the consumer only after pthread_join - the join is
 * the synchronisation, no extra lock is needed or used. */
#define CROSS_N 25
static void *g_cross_ptr[CROSS_N];
static size_t g_cross_size[CROSS_N];
static int g_cross_bad = 0; /* producer-side verify failures */

static void *producer_main(void *unused)
{
    int k;
    (void)unused;
    for (k = 0; k < CROSS_N; k++) {
        size_t n = 64u + (size_t)k * 16u;
        void *p = mimir_malloc(n);
        g_cross_ptr[k] = p;
        g_cross_size[k] = n;
        if (p == NULL) {
            g_cross_bad++;
            continue;
        }
        fill_block((unsigned char *)p, n, 900u + (unsigned int)k);
        if (!verify_block((const unsigned char *)p, n, 900u + (unsigned int)k)) {
            g_cross_bad++;
        }
    }
    return NULL;
}

int main(void)
{
    static const size_t ladder[] = {1, 16, 64, 256, 1024, 4096, 8192};
    const size_t nladder = sizeof(ladder) / sizeof(ladder[0]);
    long ps;
    size_t s;
    int round;

    printf("D007-ANDROID-TEST allocate/fill/verify/free (4 public mimir_ symbols)\n");
    printf("NOTE: a run of this binary is evidence for the host it runs on only.\n");

    /* ENV: the in-run record (Phase 4 E3). */
    ps = sysconf(_SC_PAGESIZE);
    if (ps > 0) {
        printf("ENV PAGESIZE=%ld (sysconf, measured in this run)\n", ps);
        check(1, "ENV sysconf(_SC_PAGESIZE) succeeded");
    } else {
        printf("ENV PAGESIZE=FAILED (sysconf returned %ld)\n", ps);
        check(0, "ENV sysconf(_SC_PAGESIZE) succeeded");
    }
    {
        long np = sysconf(_SC_NPROCESSORS_ONLN);
        if (np > 0) {
            printf("ENV NPROCESSORS_ONLN=%ld\n", np);
        } else {
            printf("ENV NPROCESSORS_ONLN=FAILED\n");
        }
    }
    printf("ENV POINTER_BITS=%d\n", (int)(sizeof(void *) * 8));

    /* Phase A: allocate / fill / verify / free, 3 rounds. */
    for (round = 0; round < 3; round++) {
        for (s = 0; s < nladder; s++) {
            size_t n = ladder[s];
            unsigned int seed = 100u + (unsigned int)round * 10u + (unsigned int)s;
            void *p = mimir_malloc(n);
            check(p != NULL, "A mimir_malloc(ladder size) non-NULL");
            if (p == NULL) {
                continue;
            }
            allocs++;
            fill_block((unsigned char *)p, n, seed);
            check(verify_block((const unsigned char *)p, n, seed),
                  "A fill/verify round-trip intact");
            mimir_free(p);
            frees++;
        }
    }

    /* Phase B: 8 simultaneously live blocks. */
    {
        void *live[8];
        int distinct = 1;
        int aligned = 1;
        int intact = 1;
        int i, j;
        for (i = 0; i < 8; i++) {
            size_t n = 128u + (size_t)i * 96u;
            live[i] = mimir_malloc(n);
            if (live[i] == NULL) {
                intact = 0;
                continue;
            }
            allocs++;
            fill_block((unsigned char *)live[i], n, 500u + (unsigned int)i);
            if ((uintptr_t)live[i] % _Alignof(max_align_t) != 0) {
                aligned = 0;
            }
        }
        for (i = 0; i < 8; i++) {
            if (live[i] == NULL) {
                continue;
            }
            if (!verify_block((const unsigned char *)live[i],
                              128u + (size_t)i * 96u, 500u + (unsigned int)i)) {
                intact = 0;
            }
            for (j = i + 1; j < 8; j++) {
                if (live[i] == live[j]) {
                    distinct = 0;
                }
            }
        }
        check(intact, "B 8 live blocks filled and verified while live");
        check(distinct, "B 8 live blocks pairwise distinct");
        check(aligned, "B live blocks suitably aligned (max_align_t, non-numeric U-15)");
        for (i = 0; i < 8; i++) {
            if (live[i] != NULL) {
                mimir_free(live[i]);
                frees++;
            }
        }
    }

    /* Phase C: calloc zeroing + realloc preservation. */
    {
        unsigned char *z = (unsigned char *)mimir_calloc(16, 32);
        int zeroed = 1;
        int i;
        check(z != NULL, "C mimir_calloc(16, 32) non-NULL");
        if (z != NULL) {
            allocs++;
            for (i = 0; i < 16 * 32; i++) {
                if (z[i] != 0) {
                    zeroed = 0;
                }
            }
            check(zeroed, "C calloc extent reads zero at return");
            mimir_free(z);
            frees++;
        }
    }
    {
        unsigned char *b = (unsigned char *)mimir_malloc(128);
        check(b != NULL, "C mimir_malloc(128) non-NULL (realloc source)");
        if (b != NULL) {
            unsigned char *g;
            allocs++;
            fill_block(b, 128, 700u);
            g = (unsigned char *)mimir_realloc(b, 512);
            check(g != NULL, "C mimir_realloc grow 128 -> 512 non-NULL");
            if (g != NULL) {
                check(verify_block(g, 128, 700u),
                      "C realloc grow preserves first 128 bytes");
                mimir_free(g);
                frees++;
            } else {
                mimir_free(b);
                frees++;
            }
        }
    }
    {
        void *n = mimir_realloc(NULL, 64);
        check(n != NULL, "C mimir_realloc(NULL, 64) acts as malloc");
        if (n != NULL) {
            allocs++;
            mimir_free(n);
            frees++;
        }
    }

    /* Phase D: defined failure behaviour. */
    check(mimir_malloc(SIZE_MAX) == NULL,
          "D mimir_malloc(SIZE_MAX) returns NULL (oversize refusal)");
    mimir_free(NULL);
    check(1, "D mimir_free(NULL) returns normally (defined no-op)");

    /* Phase E: cross-thread free with 2 threads. */
    {
        pthread_t prod;
        int rc = pthread_create(&prod, NULL, producer_main, NULL);
        check(rc == 0, "E producer thread created");
        if (rc == 0) {
            int intact = 1;
            int k;
            pthread_join(prod, NULL);
            check(g_cross_bad == 0, "E producer fill/verify clean (25 blocks)");
            for (k = 0; k < CROSS_N; k++) {
                if (g_cross_ptr[k] == NULL) {
                    intact = 0;
                    continue;
                }
                allocs++;
                if (!verify_block((const unsigned char *)g_cross_ptr[k],
                                  g_cross_size[k], 900u + (unsigned int)k)) {
                    intact = 0;
                }
                mimir_free(g_cross_ptr[k]); /* freed on a different thread */
                frees++;
            }
            check(intact, "E 25 blocks verified and freed cross-thread");
        }
    }
    {
        void *own = mimir_malloc(256);
        check(own != NULL, "E fresh allocation after cross-thread frees non-NULL");
        if (own != NULL) {
            allocs++;
            fill_block((unsigned char *)own, 256, 999u);
            check(verify_block((const unsigned char *)own, 256, 999u),
                  "E fresh allocation fill/verify intact");
            mimir_free(own);
            frees++;
        }
    }

    printf("CHECKS %d run, %d failed\n", passed + failed, failed);
    printf("ALLOCATIONS %ld FREES %ld\n", allocs, frees);
    printf("RESULT: %s\n", failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}
