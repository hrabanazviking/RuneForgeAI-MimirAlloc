/*
 * test_c005.c -- C-005 calloc/realloc evidence program.
 *
 * Slice C-005. Exercises, through the public L1 boundary only
 * (include/mimir_alloc.h), the behaviours that
 * docs/PHASE_3_CALLOC_REALLOC_AND_UNSUPPORTED_BEHAVIOR.md
 * (B-008) designs for mimir_calloc and mimir_realloc, mapped
 * by analogy under U-03 to the contract rows of
 * docs/contracts.md (A-008) and to the B-009 derivation cases
 * of docs/PHASE_3_TEST_SUITE_PLAN.md S8.3 (cited per section).
 * The slice record, including every posture and inconsistency
 * below, is tests/core/NOTES_C005.md.
 *
 * What is proved here (Defined cases, B-008 S5 / S7.2):
 *   - calloc success (MA-6.1-CALLOC-01/-05; hook C-002, named
 *     hook MA-CALLOC-ZEROING; B-009 P3-T-CAL-001/-002/-003):
 *     for a sweep of (nmemb, size) pairs -- including products
 *     that are not multiples of the alignment, a product of
 *     exactly two system pages, and one byte over a page --
 *     the block is non-null, aligned to max_align_t, and
 *     EVERY byte of the requested product extent reads zero
 *     at the point of return, verified byte by byte before
 *     any caller write; the extent is then written with a
 *     pattern and verified, proving owner usability, and the
 *     block is freed. No byte beyond the requested product is
 *     inspected as an oracle (B-009 CAL-003 rule); the
 *     usable-extent zeroing of B-008 S3.3 is a construction
 *     property resting on code inspection (INC-C005-02).
 *   - calloc overflow (MA-6.1-CALLOC-02; hook C-001, named
 *     hook MA-CALLOC-OVERFLOW; P3-T-CAL-011): products that
 *     overflow size_t in either factor order return NULL,
 *     with a live sentinel block verified undisturbed after
 *     the batch and the allocator verified healthy after.
 *   - calloc unsatisfiable (MA-6.1-CALLOC-03; named hook
 *     MA-CALLOC-FAILURE; P3-T-CAL-012 form): representable
 *     products that cannot be satisfied (product SIZE_MAX in
 *     either factor order) return NULL; sentinel undisturbed.
 *   - realloc grow (MA-6.1-REALLOC-01/-06; hook C-004, named
 *     hook MA-REALLOC-SUCCESS-COPY-MIN; P3-T-REA-001/-002):
 *     the first min(old, new) == old bytes are preserved
 *     byte-for-byte, including a grow across multiple pages.
 *     The grow tail is NEVER asserted -- neither preserved,
 *     nor zeroed, nor any other value (MA-6.1-REALLOC-06;
 *     P3-T-REA-031 rule): after the preserved prefix is
 *     checked, the test writes its own pattern over the full
 *     new extent and verifies only its own writes.
 *   - realloc shrink and same size (MA-6.1-REALLOC-01;
 *     P3-T-REA-001): shrink preserves the first new_size
 *     bytes; a shrink-then-grow preserves exactly the
 *     shrunken extent, proving the recorded requested size
 *     (not the retained mapping length) governs preservation
 *     (B-008 S4.4 two-records rule); a block shrunk and then
 *     freed retires normally and later allocations succeed.
 *     Same-size realloc preserves the full extent.
 *   - realloc failure preservation (MA-6.1-REALLOC-02,
 *     MA-6.2-022; hook C-003, named hook
 *     MA-REALLOC-PRESERVE-ON-FAILURE; P3-T-REA-011/-012):
 *     realloc to SIZE_MAX (fails the L2 checked arithmetic)
 *     and to SIZE_MAX / 2 (representable, mapping cannot be
 *     satisfied) both return NULL, the original contents are
 *     verified byte-for-byte intact, and the original remains
 *     caller-owned: it can be grown successfully afterwards,
 *     and in a second block it is freed exactly as before the
 *     failed call with later allocations succeeding. The
 *     B-008 S8 E-4 fail-the-Nth-mapping injection sweep
 *     cannot be run: no fault-injection hook exists in Phase
 *     3 scope (INC-C005-03).
 *   - realloc(NULL, n) (MA-6.1-REALLOC-03; named hook
 *     MA-REALLOC-NULL; P3-T-REA-021/-022): for n > 0 it
 *     behaves as mimir_malloc(n) -- success returns a usable,
 *     aligned, freeable block; an unsatisfiable n returns
 *     NULL.
 *   - Family pairing (B-008 S6.2 U-1 boundary): a block from
 *     mimir_calloc may be resized by mimir_realloc and freed
 *     by mimir_free; the preserved range of a grown calloc
 *     block still reads zero (it was preserved, not zeroed
 *     by realloc -- the check is over the preserved range
 *     only).
 *
 * Postures recorded, NOT contract checks (labelled in every
 * label and in NOTES_C005.md INC-C005-01):
 *   - calloc with a zero factor returns NULL in this
 *     implementation. The case is [UNRESOLVED: U-09]
 *     (MA-6.1-CALLOC-04); B-009 P3-T-CAL-031 writes no pass
 *     criterion for it and hook C-006 is blocked. These
 *     checks record the posture only and must not be cited
 *     as settling U-09.
 *   - realloc(ptr, 0) returns NULL in this implementation
 *     and leaves the original block LIVE, unchanged and
 *     freeable exactly as before the call (the zero size is
 *     refused at L2 admission before any state is touched,
 *     B-007 A1). The case is [UNRESOLVED: U-12]
 *     (MA-6.1-REALLOC-04); B-009 P3-T-REA-041 writes no pass
 *     criterion -- in particular it asserts neither the
 *     return value nor the ownership of p -- and hook C-006
 *     is blocked. These checks record the posture only and
 *     must not be cited as settling U-12.
 *   - realloc(NULL, 0) returns NULL, inheriting the
 *     mimir_malloc(0) posture (U-09, by inheritance per
 *     B-008 S4.7).
 *   - Shrink and same-size return the input pointer, and a
 *     grow returns a different pointer, in this design
 *     (B-008 S4.3/S4.4 design-level facts). The checks that
 *     observe this are labelled design observations: the
 *     contract permits a move in every case (MA-6.1-REALLOC-01)
 *     and no caller may rely on pointer identity (B-008
 *     S6.3 X-6).
 *
 * What is deliberately NOT checked, and why:
 *   - any errno value (not promised -- U-10 / V-02);
 *   - realloc of an invalid, foreign, interior or
 *     already-released pointer, double free, use after free
 *     (undefined behaviour -- MA-6.1-REALLOC-05; B-009 mints
 *     no case for it, NO-TEST-UB);
 *   - the grow tail's contents (MA-6.1-REALLOC-06 -- see
 *     above; asserting any value would fail the case's own
 *     specification);
 *   - bytes beyond any requested extent (no usable-size
 *     contract exists -- B-008 S6.1 N-3, S6.2 U-7);
 *   - performance of any kind (experimental correctness
 *     baseline, B-008 S6.3 X-5).
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
 * verify it byte for byte over the full extent (the same
 * helper pair as tests/core/test_core.c). */
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

static int all_zero(const unsigned char *p, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (p[i] != 0) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    long queried = sysconf(_SC_PAGESIZE);
    size_t page = queried > 0 ? (size_t)queried : 0;
    char label[128];

    printf("C-005 calloc/realloc test\n");
    printf("runtime system page size: %zu bytes\n", page);
    check(page > 0, "runtime page size discovered");

    /* ---- calloc success + zeroing (C-002; CAL-001/-002/-003).
     * Products include non-multiples of the alignment (1, 7,
     * 28, 3000), an odd product (3 x 1000), one byte over a
     * usable page, and exactly two system pages. ---- */
    {
        size_t pairs[8][2];
        size_t npairs = 0;
        size_t i;

        pairs[npairs][0] = 1; pairs[npairs][1] = 1; npairs++;
        pairs[npairs][0] = 1; pairs[npairs][1] = 7; npairs++;
        pairs[npairs][0] = 7; pairs[npairs][1] = 1; npairs++;
        pairs[npairs][0] = 4; pairs[npairs][1] = 16; npairs++;
        pairs[npairs][0] = 10; pairs[npairs][1] = 100; npairs++;
        pairs[npairs][0] = 3; pairs[npairs][1] = 1000; npairs++;
        if (page > 0) {
            pairs[npairs][0] = 1; pairs[npairs][1] = page + 1; npairs++;
            pairs[npairs][0] = 2; pairs[npairs][1] = page; npairs++;
        }

        for (i = 0; i < npairs; i++) {
            size_t nmemb = pairs[i][0];
            size_t size = pairs[i][1];
            size_t product = nmemb * size;
            unsigned char *p =
                (unsigned char *)mimir_calloc(nmemb, size);

            snprintf(label, sizeof label,
                     "calloc(%zu, %zu) non-null", nmemb, size);
            check(p != NULL, label);
            if (p == NULL) {
                continue;
            }
            snprintf(label, sizeof label,
                     "calloc(%zu, %zu) aligned to max_align_t",
                     nmemb, size);
            check((uintptr_t)p % _Alignof(max_align_t) == 0, label);
            snprintf(label, sizeof label,
                     "calloc(%zu, %zu) every requested byte zero "
                     "(byte-by-byte)", nmemb, size);
            check(all_zero(p, product), label);
            fill_pattern(p, product, (unsigned int)(11 + i));
            snprintf(label, sizeof label,
                     "calloc(%zu, %zu) extent owner-writable, "
                     "pattern verifies", nmemb, size);
            check(pattern_ok(p, product, (unsigned int)(11 + i)),
                  label);
            mimir_free(p);
        }
    }

    /* ---- calloc overflow + unsatisfiable (C-001; CAL-011),
     * with a live sentinel verified after each batch
     * (CAL-012 form for the unsatisfiable products). ---- */
    {
        unsigned char *sentinel =
            (unsigned char *)mimir_malloc(256);

        check(sentinel != NULL, "sentinel allocation succeeds");
        if (sentinel != NULL) {
            fill_pattern(sentinel, 256, 7);
        }
        check(mimir_calloc(SIZE_MAX, 2) == NULL,
              "calloc(SIZE_MAX, 2) returns NULL (overflow refusal)");
        check(mimir_calloc(2, SIZE_MAX) == NULL,
              "calloc(2, SIZE_MAX) returns NULL (overflow refusal)");
        check(mimir_calloc(SIZE_MAX / 2 + 1, 2) == NULL,
              "calloc(SIZE_MAX/2+1, 2) returns NULL (overflow refusal)");
        check(mimir_calloc(2, SIZE_MAX / 2 + 1) == NULL,
              "calloc(2, SIZE_MAX/2+1) returns NULL (overflow refusal)");
        check(mimir_calloc(SIZE_MAX / 16 + 1, 16) == NULL,
              "calloc(SIZE_MAX/16+1, 16) returns NULL (overflow refusal)");
        if (sentinel != NULL) {
            check(pattern_ok(sentinel, 256, 7),
                  "sentinel undisturbed after calloc overflow refusals");
        }
        check(mimir_calloc(1, SIZE_MAX) == NULL,
              "calloc(1, SIZE_MAX) returns NULL (representable, unsatisfiable)");
        check(mimir_calloc(SIZE_MAX, 1) == NULL,
              "calloc(SIZE_MAX, 1) returns NULL (representable, unsatisfiable)");
        if (sentinel != NULL) {
            check(pattern_ok(sentinel, 256, 7),
                  "sentinel undisturbed after unsatisfiable calloc");
            mimir_free(sentinel);
        }
        {
            unsigned char *p = (unsigned char *)mimir_calloc(6, 7);

            check(p != NULL,
                  "calloc succeeds after overflow/unsatisfiable failures");
            if (p != NULL) {
                check(all_zero(p, 42),
                      "post-failure calloc extent reads zero");
                mimir_free(p);
            }
        }
    }

    /* ---- calloc zero factor: POSTURE ONLY (U-09 unresolved;
     * MA-6.1-CALLOC-04; P3-T-CAL-031 BLOCKED, no criterion). ---- */
    check(mimir_calloc(0, 16) == NULL,
          "calloc(0, 16) returns NULL (posture only, U-09 unresolved)");
    check(mimir_calloc(16, 0) == NULL,
          "calloc(16, 0) returns NULL (posture only, U-09 unresolved)");
    check(mimir_calloc(0, 0) == NULL,
          "calloc(0, 0) returns NULL (posture only, U-09 unresolved)");

    /* ---- Family pairing: calloc block grown by realloc.
     * The preserved range still reads zero because it was
     * preserved byte-for-byte (REALLOC-01), not because
     * realloc zeroes anything (REALLOC-06). ---- */
    {
        unsigned char *p = (unsigned char *)mimir_calloc(8, 8);
        unsigned char *r;

        check(p != NULL, "calloc(8, 8) non-null (pairing setup)");
        if (p != NULL) {
            check(all_zero(p, 64), "calloc(8, 8) extent reads zero");
            r = (unsigned char *)mimir_realloc(p, 256);
            check(r != NULL, "realloc of calloc block to 256 non-null");
            if (r != NULL) {
                check(all_zero(r, 64),
                      "grown calloc block: preserved 64 bytes still zero");
                mimir_free(r);
            } else {
                mimir_free(p);
            }
        }
    }

    /* ---- realloc(NULL, n) (REALLOC-03; REA-021/-022). ---- */
    {
        unsigned char *p = (unsigned char *)mimir_realloc(NULL, 128);

        check(p != NULL, "realloc(NULL, 128) non-null (acts as malloc)");
        if (p != NULL) {
            check((uintptr_t)p % _Alignof(max_align_t) == 0,
                  "realloc(NULL, 128) aligned to max_align_t");
            fill_pattern(p, 128, 31);
            check(pattern_ok(p, 128, 31),
                  "realloc(NULL, 128) extent owner-writable, pattern verifies");
            mimir_free(p);
            check(1, "realloc(NULL, 128) block freed exactly once, no crash");
        }
        check(mimir_realloc(NULL, SIZE_MAX) == NULL,
              "realloc(NULL, SIZE_MAX) returns NULL (malloc failure case)");
        check(mimir_realloc(NULL, 0) == NULL,
              "realloc(NULL, 0) returns NULL (posture only, U-09 unresolved)");
    }

    /* ---- realloc grow (C-004; REA-001/-002). The tail is
     * never asserted: after the prefix check the test writes
     * its own pattern over the full new extent. ---- */
    {
        unsigned char *p = (unsigned char *)mimir_malloc(64);
        unsigned char *r;

        check(p != NULL, "malloc(64) non-null (grow setup)");
        if (p != NULL) {
            fill_pattern(p, 64, 21);
            r = (unsigned char *)mimir_realloc(p, 5000);
            check(r != NULL, "realloc grow 64 -> 5000 non-null");
            if (r != NULL) {
                check(pattern_ok(r, 64, 21),
                      "grow preserves first min(old, new) = 64 bytes");
                check(r != p,
                      "grow moved to a new block (design observation, "
                      "B-008 S4.4; not a contract claim)");
                fill_pattern(r, 5000, 22);
                check(pattern_ok(r, 5000, 22),
                      "grown extent fully owner-writable, pattern verifies");
                mimir_free(r);
            } else {
                mimir_free(p);
            }
        }
    }
    if (page > 0) {
        unsigned char *p = (unsigned char *)mimir_malloc(100);
        unsigned char *r;
        size_t big = 3 * page + 123;

        check(p != NULL, "malloc(100) non-null (multi-page grow setup)");
        if (p != NULL) {
            fill_pattern(p, 100, 23);
            r = (unsigned char *)mimir_realloc(p, big);
            check(r != NULL, "realloc grow 100 -> 3*page+123 non-null");
            if (r != NULL) {
                check(pattern_ok(r, 100, 23),
                      "multi-page grow preserves first 100 bytes");
                mimir_free(r);
            } else {
                mimir_free(p);
            }
        }
    }

    /* ---- realloc same size (B-008 S4.3 design decision). ---- */
    {
        unsigned char *p = (unsigned char *)mimir_malloc(200);
        unsigned char *r;

        check(p != NULL, "malloc(200) non-null (same-size setup)");
        if (p != NULL) {
            fill_pattern(p, 200, 25);
            r = (unsigned char *)mimir_realloc(p, 200);
            check(r != NULL, "realloc same size 200 -> 200 non-null");
            if (r != NULL) {
                check(pattern_ok(r, 200, 25),
                      "same-size realloc preserves full extent");
                check(r == p,
                      "same-size returns input pointer (design decision, "
                      "B-008 S4.3; not a contract claim)");
                mimir_free(r);
            } else {
                mimir_free(p);
            }
        }
    }

    /* ---- realloc shrink (REA-001; B-008 S4.4 two records). ---- */
    {
        unsigned char *p = (unsigned char *)mimir_malloc(5000);
        unsigned char *r;
        unsigned char *r2;

        check(p != NULL, "malloc(5000) non-null (shrink setup)");
        if (p != NULL) {
            fill_pattern(p, 5000, 24);
            r = (unsigned char *)mimir_realloc(p, 100);
            check(r != NULL, "realloc shrink 5000 -> 100 non-null");
            if (r != NULL) {
                check(pattern_ok(r, 100, 24),
                      "shrink preserves prefix of first 100 bytes");
                check(r == p,
                      "shrink returns input pointer (design-level fact, "
                      "B-008 S4.4; not a contract claim)");
                fill_pattern(r, 100, 27);
                check(pattern_ok(r, 100, 27),
                      "shrunken extent owner-writable, pattern verifies");
                /* Grow back from the shrunken record: only the
                 * recorded 100 bytes may be preserved. The
                 * pattern just written seeds those 100 bytes. */
                r2 = (unsigned char *)mimir_realloc(r, 2000);
                check(r2 != NULL, "realloc grow after shrink 100 -> 2000 non-null");
                if (r2 != NULL) {
                    check(pattern_ok(r2, 100, 27),
                          "grow after shrink preserves recorded 100 bytes");
                    mimir_free(r2);
                } else {
                    mimir_free(r);
                }
            } else {
                mimir_free(p);
            }
        }
    }
    if (page > 0) {
        unsigned char *p = (unsigned char *)mimir_malloc(3 * page);
        unsigned char *r;
        unsigned char *fresh;

        check(p != NULL, "malloc(3*page) non-null (shrink+free setup)");
        if (p != NULL) {
            r = (unsigned char *)mimir_realloc(p, 50);
            check(r != NULL, "realloc shrink 3*page -> 50 non-null");
            if (r != NULL) {
                /* Retires the WHOLE original mapping (the
                 * recorded mapping length, B-008 S4.4), not a
                 * length derived from the shrunken size. */
                mimir_free(r);
                fresh = (unsigned char *)mimir_malloc(1024);
                check(fresh != NULL,
                      "allocation after shrink+free succeeds");
                if (fresh != NULL) {
                    mimir_free(fresh);
                }
            } else {
                mimir_free(p);
            }
        }
    }

    /* ---- realloc failure preservation (C-003; REA-011/-012):
     * the original stays LIVE, intact and caller-owned. ---- */
    {
        unsigned char *p = (unsigned char *)mimir_malloc(256);
        unsigned char *r;

        check(p != NULL, "malloc(256) non-null (failure-preservation setup)");
        if (p != NULL) {
            fill_pattern(p, 256, 26);
            check(mimir_realloc(p, SIZE_MAX) == NULL,
                  "realloc(ptr, SIZE_MAX) returns NULL (failure)");
            check(pattern_ok(p, 256, 26),
                  "original intact after SIZE_MAX realloc failure");
            check(mimir_realloc(p, SIZE_MAX / 2) == NULL,
                  "realloc(ptr, SIZE_MAX/2) returns NULL (unsatisfiable)");
            check(pattern_ok(p, 256, 26),
                  "original intact after unsatisfiable realloc failure");
            r = (unsigned char *)mimir_realloc(p, 512);
            check(r != NULL,
                  "original still usable: grow succeeds after failures");
            if (r != NULL) {
                check(pattern_ok(r, 256, 26),
                      "grow after failures preserves first 256 bytes");
                mimir_free(r);
            } else {
                mimir_free(p);
            }
        }
    }
    {
        unsigned char *q = (unsigned char *)mimir_malloc(128);
        unsigned char *fresh;

        check(q != NULL, "malloc(128) non-null (freeable-after-failure setup)");
        if (q != NULL) {
            fill_pattern(q, 128, 29);
            check(mimir_realloc(q, SIZE_MAX) == NULL,
                  "realloc(ptr, SIZE_MAX) returns NULL (second block)");
            check(pattern_ok(q, 128, 29),
                  "second original intact after failed realloc");
            mimir_free(q);
            fresh = (unsigned char *)mimir_malloc(64);
            check(fresh != NULL,
                  "allocation after freeing failure-preserved block succeeds");
            if (fresh != NULL) {
                mimir_free(fresh);
            }
        }
    }

    /* ---- realloc(ptr, 0): POSTURE ONLY (U-12 unresolved;
     * MA-6.1-REALLOC-04; P3-T-REA-041 BLOCKED, no criterion --
     * neither the return value nor the ownership of p is
     * asserted as contract). This implementation refuses zero
     * at L2 admission before touching any state, so the
     * recorded posture is: NULL return, original LIVE,
     * unchanged, and freeable exactly as before. ---- */
    {
        unsigned char *p = (unsigned char *)mimir_malloc(300);
        unsigned char *fresh;

        check(p != NULL, "malloc(300) non-null (realloc(ptr, 0) posture setup)");
        if (p != NULL) {
            fill_pattern(p, 300, 28);
            check(mimir_realloc(p, 0) == NULL,
                  "realloc(ptr, 0) returns NULL (posture only, U-12 unresolved)");
            check(pattern_ok(p, 300, 28),
                  "original intact after realloc(ptr, 0) (posture record)");
            mimir_free(p);
            fresh = (unsigned char *)mimir_malloc(64);
            check(fresh != NULL,
                  "original was freeable after realloc(ptr, 0); later allocation succeeds (posture record)");
            if (fresh != NULL) {
                mimir_free(fresh);
            }
        }
    }

    printf("CHECKS: %d run, %d failed\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
