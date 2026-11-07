/*
 * test_g007.c -- evidence test for slice G-007 (Phase 7
 * tuning parameters: configurable, validated and visible;
 * slice record: docs/PHASE_7_TUNING_PARAMETERS_G007.md).
 *
 * Run standalone via `make test-g007` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-g003/test-g005 pattern). It verifies the tuning
 * surface for the G-005 RP-2 per-class empty-span retention
 * bound through the four public symbols, the G-003
 * span/object audit counters (tests/core/mimir_small_g003.h)
 * and the G-007 test-only tuning functions
 * (tests/core/mimir_tune_g007.h) -- all INC-E001-07
 * shape (b): readable/settable only by linking the library
 * OBJECTS directly. The shared library exports none of it
 * (nm -D stays exactly 4; recorded run).
 *
 * The mechanism under test (G-007 design decision): ONE
 * explicit setter/getter pair on the internal/test surface.
 * Properties proven here, in order:
 *   A visibility + default: before any setter call the
 *     getter reads back exactly 2, the banked default.
 *   P1 default-behaviour equivalence: with nothing
 *     configured, the G-005 P1 shape reproduces EXACTLY for
 *     all 20 classes (hold 2*cap+1, free all: 3 spans
 *     published, 1 retired, retained delta +2 per class,
 *     cumulative 40 spans = the RP-5 worst case).
 *   P2 invalid-value refusal: out-of-range values (17, 100,
 *     SIZE_MAX) are each refused with return -1, the getter
 *     still reads 2 after each refusal, and a churn phase
 *     afterwards behaves exactly as the default bound
 *     dictates (no partial application, no state damage).
 *   P3 bound 1 takes effect exactly: for classes 512, 640
 *     and 768 (each holding 2 retained spans from P1), a
 *     2*cap+1 hold/free cycle publishes exactly 1 new span,
 *     retires exactly 2, and the class converges to exactly
 *     1 retained span (total 40 -> 37).
 *   P4 bound 0 takes effect exactly: for classes 1024 and
 *     320, a 2*cap+1 hold/free cycle publishes exactly 1
 *     new span, retires exactly 3, and the class converges
 *     to exactly 0 retained spans (total 37 -> 33).
 *   P5 restore: setting 2 again succeeds, the getter reads
 *     2, and re-running the P1 shape on the two zeroed
 *     classes restores exactly +2 retained spans each
 *     (total 33 -> 37; the three bound-1 classes keep their
 *     converged 1 each -- the bound applies at free time,
 *     it does not retroactively re-warm classes).
 *   P6 final accounting: live small objects 0, live spans
 *     exactly 37 (15 classes x 2 + 3 classes x 1 + 2
 *     classes x 2 restored), retained mapped bytes exactly
 *     37 x the runtime page size.
 *
 * Setter semantics relied on (documented in the slice
 * doc): the bound is applied at free time when a span
 * empties (RP-3 discipline); lowering it never eagerly
 * unmaps already-retained spans, so convergence is
 * observed exactly at the next hold/free cycle of a class.
 *
 * Warm-up: one LARGE alloc/touch/free (2048, mapping path)
 * before any measurement, exactly as in test_g005, so the
 * warm-up leaves no span state behind. All phases are
 * small and bounded (the largest holds 2*cap+1 blocks of
 * one class, as in G-005 P1).
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */

#define _POSIX_C_SOURCE 200809L

#include "mimir_alloc.h"
#include "mimir_small_g003.h"
#include "mimir_tune_g007.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

static const size_t class_sizes[20] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};

static unsigned long live_spans(void)
{
    return mimir_audit_span_published - mimir_audit_span_retired;
}

static unsigned long live_small(void)
{
    return mimir_audit_small_alloc - mimir_audit_small_free;
}

/* One hold/free cycle of 2*cap+1 blocks of class size cs,
 * checking the exact span-counter deltas the caller states.
 * Returns nothing; every expectation is a check. */
static void cycle(long page, size_t cs, long expect_pub,
                  long expect_ret, long expect_delta,
                  const char *phase)
{
    size_t cap = ((size_t)page - 96) / cs;
    size_t n = 2 * cap + 1;
    void **blocks = (void **)malloc(n * sizeof(void *));
    unsigned long before = live_spans();
    unsigned long pub_before = mimir_audit_span_published;
    unsigned long ret_before = mimir_audit_span_retired;
    size_t j, got = 0;
    char label[192];

    if (blocks == NULL) {
        check(0, "harness allocation");
        return;
    }
    for (j = 0; j < n; j++) {
        blocks[j] = mimir_malloc(cs);
        if (blocks[j] != NULL) {
            memset(blocks[j], 0x3C, cs);
            got++;
        }
    }
    snprintf(label, sizeof(label),
             "%s class %zu: all %zu blocks allocated", phase, cs, n);
    check(got == n, label);
    snprintf(label, sizeof(label),
             "%s class %zu: spans published == %ld (observed %lu)",
             phase, cs, expect_pub,
             mimir_audit_span_published - pub_before);
    check((long)(mimir_audit_span_published - pub_before) ==
          expect_pub, label);
    for (j = 0; j < n; j++) {
        mimir_free(blocks[j]);
    }
    free(blocks);
    snprintf(label, sizeof(label),
             "%s class %zu: spans retired == %ld (observed %lu), "
             "retained delta == %+ld (observed %+ld)",
             phase, cs, expect_ret,
             mimir_audit_span_retired - ret_before, expect_delta,
             (long)live_spans() - (long)before);
    check((long)(mimir_audit_span_retired - ret_before) ==
              expect_ret &&
          (long)live_spans() - (long)before == expect_delta,
          label);
    snprintf(label, sizeof(label),
             "%s class %zu: live small objects back to 0", phase, cs);
    check(live_small() == 0, label);
    printf("  %s class %5zu cap %4zu blocks %5zu -> "
           "live_spans_total %lu\n",
           phase, cs, cap, n, live_spans());
}

int main(void)
{
    long page = sysconf(_SC_PAGESIZE);
    size_t i;

    printf("G-007 tuning parameters test (retention bound: "
           "configurable, validated, visible)\n");
    printf("page_size=%ld (runtime, via sysconf)\n", page);
    if (page <= 0) {
        printf("RESULT: FAIL (no page size)\n");
        return 1;
    }

    /* Warm-up: one LARGE alloc/touch/free (mapping path;
     * leaves no span state behind), as in test_g005. */
    {
        void *w = mimir_malloc(2048);
        if (w != NULL) {
            memset(w, 0x5A, 2048);
            mimir_free(w);
        }
    }

    /* ---------- A: visibility + default ---------- */
    printf("A visibility: getter before any setter call\n");
    check(mimir_tune_get_retention_bound() ==
              MIMIR_TUNE_RETENTION_DEFAULT,
          "A default effective bound == 2 (banked RP-2 value)");
    check(live_spans() == 0 && live_small() == 0,
          "A no span state after warm-up");

    /* ---------- P1: default equivalence, G-005 P1 shape -- */
    printf("P1 default equivalence (nothing configured; hold "
           "2*cap+1, free all, expect +2 retained per class)\n");
    for (i = 0; i < 20; i++) {
        cycle(page, class_sizes[i], 3, 1, 2, "P1");
    }
    check(live_spans() == 40,
          "P1 total retained spans == 40 (2 per class x 20, "
          "the G-005 end state)");
    check(mimir_tune_get_retention_bound() == 2,
          "P1 getter still reads the default 2");

    /* ---------- P2: invalid values refused ---------- */
    printf("P2 invalid-value refusal (17, 100, SIZE_MAX)\n");
    {
        static const size_t bad[3] = {17, 100, SIZE_MAX};
        char label[160];

        for (i = 0; i < 3; i++) {
            int rc = mimir_tune_set_retention_bound(bad[i]);

            snprintf(label, sizeof(label),
                     "P2 set(%zu) refused, return == -1 "
                     "(observed %d)", bad[i], rc);
            check(rc == -1, label);
            snprintf(label, sizeof(label),
                     "P2 after refusal of %zu: getter still == 2 "
                     "(observed %zu)",
                     bad[i], mimir_tune_get_retention_bound());
            check(mimir_tune_get_retention_bound() == 2, label);
        }
    }
    printf("P2 behaviour after refusals: churn class 128 "
           "(3*cap hold/free) must keep the default bound\n");
    {
        size_t cs = 128;
        size_t cap = ((size_t)page - 96) / cs;
        size_t n = 3 * cap;
        void **blocks = (void **)malloc(n * sizeof(void *));
        unsigned long before = live_spans();
        size_t j;

        check(blocks != NULL, "P2 harness allocation");
        if (blocks != NULL) {
            for (j = 0; j < n; j++) {
                blocks[j] = mimir_malloc(cs);
                if (blocks[j] != NULL) {
                    memset(blocks[j], 0xC3, cs);
                }
            }
            for (j = 0; j < n; j++) {
                mimir_free(blocks[j]);
            }
            free(blocks);
        }
        check(live_spans() == before && live_small() == 0,
              "P2 churn leaves live_spans unchanged at the "
              "default bound (no partial application)");
    }

    /* ---------- P3: bound 1 takes effect exactly ---------- */
    printf("P3 bound 1 (classes 512, 640, 768 converge "
           "2 -> 1 retained each)\n");
    check(mimir_tune_set_retention_bound(1) == 0,
          "P3 set(1) accepted, return == 0");
    check(mimir_tune_get_retention_bound() == 1,
          "P3 getter reads back exactly 1");
    {
        static const size_t cs3[3] = {512, 640, 768};

        for (i = 0; i < 3; i++) {
            cycle(page, cs3[i], 1, 2, -1, "P3");
        }
    }
    check(live_spans() == 37,
          "P3 total retained spans == 37 (40 - 3 classes x 1)");

    /* ---------- P4: bound 0 takes effect exactly ---------- */
    printf("P4 bound 0 (classes 1024, 320 converge "
           "2 -> 0 retained each)\n");
    check(mimir_tune_set_retention_bound(0) == 0,
          "P4 set(0) accepted, return == 0");
    check(mimir_tune_get_retention_bound() == 0,
          "P4 getter reads back exactly 0");
    {
        static const size_t cs4[2] = {1024, 320};

        for (i = 0; i < 2; i++) {
            cycle(page, cs4[i], 1, 3, -2, "P4");
        }
    }
    check(live_spans() == 33,
          "P4 total retained spans == 33 (37 - 2 classes x 2)");

    /* ---------- P5: restore the default ---------- */
    printf("P5 restore bound 2 (classes 1024, 320 re-warm "
           "0 -> 2 retained each)\n");
    check(mimir_tune_set_retention_bound(2) == 0,
          "P5 set(2) accepted, return == 0");
    check(mimir_tune_get_retention_bound() == 2,
          "P5 getter reads back exactly 2");
    {
        static const size_t cs5[2] = {1024, 320};

        for (i = 0; i < 2; i++) {
            cycle(page, cs5[i], 3, 1, 2, "P5");
        }
    }
    check(live_spans() == 37,
          "P5 total retained spans == 37 (33 + 2 classes x 2; "
          "bound-1 classes keep their converged 1)");

    /* ---------- P6: final accounting ---------- */
    printf("P6 final: live_spans=%lu live_small=%lu "
           "retained_mapped_bytes=%lu\n",
           live_spans(), live_small(),
           live_spans() * (unsigned long)page);
    check(live_small() == 0, "P6 live small objects == 0");
    check(live_spans() == 37, "P6 retained spans == 37");
    check(live_spans() * (unsigned long)page ==
              37UL * (unsigned long)page,
          "P6 retained mapped bytes == 37 x page size");

    printf("checks=%d failures=%d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
