/*
 * test_core.c -- exercise of the C-003 mapping-backed core.
 *
 * Slice C-003 evidence program, run on the Pi via `make test`
 * (see recorded_run.txt beside this file for the recorded
 * output). It checks only behaviour the banked design
 * documents promise by analogy for the namespaced pair
 * (U-03 caveat: analogy only, Proposed):
 *
 *   1. allocations of several sizes succeed with non-null,
 *      suitably aligned pointers (MA-6.1-MALLOC-01 by
 *      analogy) -- including a size that exercises page
 *      round-up (one byte over a whole usable page) and a
 *      multi-page size;
 *   2. patterns written across the full requested extent of
 *      each allocation read back exactly;
 *   3. several simultaneously live blocks have pairwise
 *      non-overlapping requested extents and hold distinct
 *      patterns without disturbing each other (Invariant 1);
 *   4. every block is freed exactly once and free(NULL) is a
 *      defined no-op (MA-6.1-FREE-01/-02 by analogy);
 *   5. oversize requests fail with NULL and no wrapped or
 *      partial allocation (MA-6.1-MALLOC-04 by analogy):
 *      SIZE_MAX and SIZE_MAX - 8;
 *   6. mimir_malloc(0) returns NULL -- recorded here as this
 *      implementation's posture ONLY. Zero-size is
 *      [UNRESOLVED: U-09]; this check does not settle it and
 *      must not be cited as doing so;
 *   7. after frees, a fresh allocation succeeds and works
 *      (each allocation is a fresh mapping, B-006 S8.5).
 *
 * It deliberately does NOT check: allocation contents before
 * writing (unspecified, never zero-promised -- B-007 C1), any
 * errno value (not promised, U-10), performance of any kind
 * (this is an experimental correctness baseline, not a
 * performance allocator), or invalid frees (undefined
 * behaviour, B-006 S8.3 -- no defined outcome exists to
 * check).
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */

#include "mimir_alloc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
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

/* Write a position-dependent pattern seeded per block, then
 * verify it byte for byte over the full requested extent. */
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

int main(void)
{
    long queried = sysconf(_SC_PAGESIZE);
    size_t page = queried > 0 ? (size_t)queried : 0;

    /* Sizes exercised one at a time (alloc, pattern, free):
     * small, sub-page, exactly one usable page of payload
     * (page bytes fit the usable region exactly when the
     * usable region is one system page), one byte over --
     * which exercises the page round-up -- and a multi-page
     * size. Expressed via the runtime page size, never a
     * hardcoded constant (MA-6.2-041). */
    size_t sizes[6];
    size_t nsizes = 0;
    size_t i;
    int j;

    printf("C-003 core allocator test\n");
    printf("runtime system page size: %zu bytes\n", page);
    check(page > 0, "runtime page size discovered");

    sizes[nsizes++] = 1;
    sizes[nsizes++] = 64;
    sizes[nsizes++] = 1000;
    if (page > 0) {
        sizes[nsizes++] = page;         /* one full usable page */
        sizes[nsizes++] = page + 1;     /* exercises round-up   */
        sizes[nsizes++] = 3 * page + 123; /* multi-page           */
    }

    for (i = 0; i < nsizes; i++) {
        size_t n = sizes[i];
        unsigned char *p = (unsigned char *)mimir_malloc(n);
        char label[96];

        snprintf(label, sizeof label, "malloc(%zu) non-null", n);
        check(p != NULL, label);
        if (p == NULL) {
            continue;
        }
        snprintf(label, sizeof label,
                 "malloc(%zu) aligned to max_align_t", n);
        check((uintptr_t)p % _Alignof(max_align_t) == 0, label);
        fill_pattern(p, n, (unsigned int)(i + 1));
        snprintf(label, sizeof label,
                 "malloc(%zu) pattern verifies over full extent", n);
        check(pattern_ok(p, n, (unsigned int)(i + 1)), label);
        mimir_free(p);
    }

    /* Simultaneously live blocks: distinct, non-overlapping,
     * and pattern-stable while all are live. */
    {
        enum { LIVE_COUNT = 4 };
        static const size_t live_sizes[LIVE_COUNT] = {16, 500, 5000, 20000};
        unsigned char *live[LIVE_COUNT];
        int all_nonnull = 1;
        int disjoint = 1;
        int stable = 1;

        for (j = 0; j < LIVE_COUNT; j++) {
            live[j] = (unsigned char *)mimir_malloc(live_sizes[j]);
            if (live[j] == NULL) {
                all_nonnull = 0;
            }
        }
        check(all_nonnull, "four simultaneous allocations succeed");
        for (j = 0; j < LIVE_COUNT; j++) {
            int k;

            if (live[j] == NULL) {
                continue;
            }
            fill_pattern(live[j], live_sizes[j], (unsigned int)(40 + j));
            for (k = j + 1; k < LIVE_COUNT; k++) {
                if (live[k] != NULL &&
                    ranges_overlap(live[j], live_sizes[j],
                                   live[k], live_sizes[k])) {
                    disjoint = 0;
                }
            }
        }
        check(disjoint, "live blocks pairwise non-overlapping");
        for (j = 0; j < LIVE_COUNT; j++) {
            if (live[j] != NULL &&
                !pattern_ok(live[j], live_sizes[j],
                            (unsigned int)(40 + j))) {
                stable = 0;
            }
        }
        check(stable, "live blocks hold distinct patterns undisturbed");
        for (j = 0; j < LIVE_COUNT; j++) {
            mimir_free(live[j]); /* free(NULL) among them is a no-op */
        }
        check(1, "all live blocks freed exactly once, no crash");
    }

    /* free(NULL): defined no-op (MA-6.1-FREE-02 by analogy). */
    mimir_free(NULL);
    check(1, "mimir_free(NULL) returns normally");

    /* Oversize: defined failure, NULL only (MA-6.1-MALLOC-04
     * by analogy) -- no wrapped or partial allocation. */
    check(mimir_malloc(SIZE_MAX) == NULL,
          "malloc(SIZE_MAX) returns NULL (oversize refusal)");
    check(mimir_malloc(SIZE_MAX - 8) == NULL,
          "malloc(SIZE_MAX - 8) returns NULL (oversize refusal)");

    /* Zero-size: implementation posture ONLY (U-09 open). */
    check(mimir_malloc(0) == NULL,
          "malloc(0) returns NULL (posture only, U-09 unresolved)");

    /* A fresh allocation after all the frees still works:
     * every allocation is a fresh mapping (B-006 S8.5). */
    {
        unsigned char *p = (unsigned char *)mimir_malloc(4096);

        check(p != NULL, "allocation after frees succeeds");
        if (p != NULL) {
            fill_pattern(p, 4096, 99);
            check(pattern_ok(p, 4096, 99),
                  "post-free allocation pattern verifies");
            mimir_free(p);
        }
    }

    printf("CHECKS: %d run, %d failed\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
