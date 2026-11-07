/* test_e005.c - MimirAlloc slice E-005 audit-counter proof,
 * static/test-only leg (INC-E001-07 shape (b)).
 *
 * This program links the library OBJECTS (via `make
 * test-e005`), never the shared library, precisely so it
 * can read the internal audit counters declared in
 * tests/abi/mimir_audit_e005.h. Its claim is the Phase 5
 * task T5 claim, stated exactly: through the intended
 * call path (the four public entry points), the counters
 * move exactly per call / per published mapping / per
 * retired mapping, and calls that bypass MimirAlloc
 * (libc malloc/calloc/realloc/free in the same process,
 * Phase 2) move none of them. The dynamic (.so) leg,
 * where the counters are deliberately unreadable, is
 * tests/abi/test_e005_dyn.c.
 *
 * The expected counts below are derived from the phase
 * script itself, not read back from the counters:
 *   Phase 1: 5 mimir_malloc, all succeed
 *            -> call_malloc 5, published 5
 *   Phase 2: libc bypass (3 malloc-family allocations,
 *            1 realloc, 3 frees) -> no counter moves
 *   Phase 3: 3 mimir_calloc, all succeed, extents zeroed
 *            -> call_calloc 3, published 8
 *   Phase 4: realloc grow x2 (each publishes a new mapping
 *            and retires the old), shrink x1, same-size x1
 *            -> call_realloc 4, published 10, retired 2
 *   Phase 5: oversize mimir_malloc fails NULL, overflow
 *            mimir_calloc fails NULL
 *            -> call_malloc 6, call_calloc 4, published 10
 *   Phase 6: mimir_free(NULL) (call only), free 4 live
 *            -> call_free 5, retired 6
 *   Phase 7: free the remaining 4 live blocks
 *            -> call_free 9, retired 10 = published (live 0)
 *
 * Defined-case behaviour only is exercised for the
 * counters' exactness; the Phase 5 failure calls are the
 * header's defined failures (null return). Zero-size and
 * realloc-to-zero postures are NOT exercised: no behaviour
 * is promised for them (U-09 / U-12 unresolved).
 *
 * Standalone on purpose: built and run via `make
 * test-e005`; NOT part of `make test`. See
 * docs/PHASE_5_AUDIT_HOOK_E005.md and the recorded run
 * tests/abi/recorded_run_e005.txt.
 */
#include "mimir_alloc.h"
#include "mimir_audit_e005.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks_run++;                                                   \
        if (cond) {                                                     \
            printf("  [ok] %s\n", #cond);                               \
        } else {                                                        \
            checks_failed++;                                            \
            printf("  [FAIL] %s (line %d)\n", #cond, __LINE__);         \
        }                                                               \
    } while (0)

static unsigned long load(const unsigned long *ctr)
{
    return __atomic_load_n(ctr, __ATOMIC_RELAXED);
}

static void audit_line(const char *phase)
{
    printf("AUDIT %s call_malloc=%lu call_calloc=%lu "
           "call_realloc=%lu call_free=%lu published=%lu "
           "retired=%lu live=%lu\n",
           phase,
           load(&mimir_audit_call_malloc),
           load(&mimir_audit_call_calloc),
           load(&mimir_audit_call_realloc),
           load(&mimir_audit_call_free),
           load(&mimir_audit_alloc_published),
           load(&mimir_audit_mapping_retired),
           load(&mimir_audit_alloc_published) -
               load(&mimir_audit_mapping_retired));
}

static int expect(const char *phase, unsigned long cm,
                  unsigned long cc, unsigned long cr,
                  unsigned long cf, unsigned long pub,
                  unsigned long ret)
{
    int ok = load(&mimir_audit_call_malloc) == cm &&
             load(&mimir_audit_call_calloc) == cc &&
             load(&mimir_audit_call_realloc) == cr &&
             load(&mimir_audit_call_free) == cf &&
             load(&mimir_audit_alloc_published) == pub &&
             load(&mimir_audit_mapping_retired) == ret;
    audit_line(phase);
    return ok;
}

int main(void)
{
    printf("E-005 audit-counter test (static leg, library "
           "objects linked directly)\n");

    /* Start state: every counter zero before any call. */
    CHECK(expect("start", 0, 0, 0, 0, 0, 0));

    /* Phase 1: five mimir_malloc through the public path. */
    static const size_t msz[5] = {32, 64, 128, 256, 512};
    unsigned char *m[5];
    for (int i = 0; i < 5; i++) {
        m[i] = mimir_malloc(msz[i]);
        CHECK(m[i] != NULL);
        if (m[i] != NULL) {
            memset(m[i], 0x40 + i, msz[i]);
        }
    }
    CHECK(expect("phase1-mimir-malloc-x5", 5, 0, 0, 0, 5, 0));

    /* Phase 2 (NEGATIVE): libc calls in the same process
     * bypass MimirAlloc entirely; no counter may move. */
    unsigned char *x = malloc(64);
    unsigned char *y = calloc(16, 8);
    unsigned char *z = malloc(32);
    CHECK(x != NULL && y != NULL && z != NULL);
    if (z != NULL) {
        unsigned char *z2 = realloc(z, 96);
        CHECK(z2 != NULL);
        if (z2 != NULL) {
            z = z2;
        }
    }
    free(x);
    free(y);
    free(z);
    CHECK(expect("phase2-libc-bypass", 5, 0, 0, 0, 5, 0));

    /* Phase 3: three mimir_calloc; extents read zero. */
    unsigned char *c[3];
    c[0] = mimir_calloc(4, 16);
    c[1] = mimir_calloc(8, 8);
    c[2] = mimir_calloc(1, 100);
    static const size_t csz[3] = {64, 64, 100};
    for (int i = 0; i < 3; i++) {
        CHECK(c[i] != NULL);
        if (c[i] != NULL) {
            int zeroed = 1;
            for (size_t j = 0; j < csz[i]; j++) {
                if (c[i][j] != 0) {
                    zeroed = 0;
                }
            }
            CHECK(zeroed);
        }
    }
    CHECK(expect("phase3-mimir-calloc-x3", 5, 3, 0, 0, 8, 0));

    /* Phase 4: realloc - grow x2 moves (publish + retire),
     * shrink x1 and same-size x1 move no mapping counter. */
    if (m[0] != NULL) {
        for (size_t j = 0; j < 32; j++) {
            m[0][j] = (unsigned char)(j + 1);
        }
        unsigned char *g = mimir_realloc(m[0], 96);
        CHECK(g != NULL);
        if (g != NULL) {
            int preserved = 1;
            for (size_t j = 0; j < 32; j++) {
                if (g[j] != (unsigned char)(j + 1)) {
                    preserved = 0;
                }
            }
            CHECK(preserved);
            m[0] = g;
        }
    }
    if (m[1] != NULL) {
        unsigned char *g = mimir_realloc(m[1], 128);
        CHECK(g != NULL);
        if (g != NULL) {
            m[1] = g;
        }
    }
    CHECK(expect("phase4a-realloc-grow-x2", 5, 3, 2, 0, 10, 2));
    if (m[4] != NULL) {
        unsigned char *s = mimir_realloc(m[4], 128);
        CHECK(s == m[4]);
    }
    if (m[3] != NULL) {
        unsigned char *s = mimir_realloc(m[3], 256);
        CHECK(s == m[3]);
    }
    CHECK(expect("phase4b-realloc-shrink-same", 5, 3, 4, 0, 10, 2));

    /* Phase 5: defined failures. The call counters move
     * (the call reached MimirAlloc); the mapping counters
     * do not (nothing was published). */
    void *big = mimir_malloc(SIZE_MAX);
    CHECK(big == NULL);
    void *ovf = mimir_calloc(SIZE_MAX, 2);
    CHECK(ovf == NULL);
    CHECK(expect("phase5-defined-failures", 6, 4, 4, 0, 10, 2));

    /* Phase 6: free(NULL) counts as a call and retires
     * nothing; four live frees retire four mappings. */
    mimir_free(NULL);
    CHECK(expect("phase6a-free-null", 6, 4, 4, 1, 10, 2));
    mimir_free(m[2]);
    mimir_free(m[3]);
    mimir_free(c[0]);
    mimir_free(c[1]);
    CHECK(expect("phase6b-free-x4", 6, 4, 4, 5, 10, 6));

    /* Phase 7: free the remaining four live blocks; the
     * books balance exactly: published == retired. */
    mimir_free(m[0]);
    mimir_free(m[1]);
    mimir_free(m[4]);
    mimir_free(c[2]);
    CHECK(expect("phase7-free-rest", 6, 4, 4, 9, 10, 10));
    CHECK(load(&mimir_audit_alloc_published) ==
          load(&mimir_audit_mapping_retired));

    printf("CHECKS: %d run, %d failed\n", checks_run,
           checks_failed);
    if (checks_failed == 0) {
        printf("RESULT: PASS\n");
        return 0;
    }
    printf("RESULT: FAIL\n");
    return 1;
}
