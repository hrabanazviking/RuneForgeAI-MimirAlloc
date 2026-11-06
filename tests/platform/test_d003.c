/*
 * test_d003.c -- D-003 page-size portability proof (Wave 4).
 *
 * Two clearly labelled evidence classes:
 *
 *   PART A [MEASURED ON-DEVICE]: runs against the real L6
 *   platform layer and the real allocator on this host. Every
 *   page quantity in Part A is the runtime-discovered system
 *   page size (mimir_platform_page_size / sysconf(_SC_PAGESIZE),
 *   failure-checked). Nothing in Part A names a literal page
 *   size as an expectation: expected mapping lengths are
 *   computed from the discovered value.
 *
 *   PART B [ARITHMETIC-LEVEL ONLY]: a reference implementation
 *   of the documented checked page-round-up (B-007 A5/A6 forms,
 *   re-derived here from the design text, NOT linked against
 *   the allocator's private static helper) is exercised
 *   against BOTH admitted target page sizes, 4096 and 16384,
 *   with expected values computed independently via
 *   unsigned __int128 arithmetic. Part B proves the rounding
 *   and layout FORMULAS are page-size-generic. It does NOT
 *   prove the allocator was run on a 4096-byte-page host: no
 *   such host exists in this campaign (the Pi kernel is a
 *   16 KiB-page kernel), and no such claim is made
 *   (INC-D003-02 in docs/PAGE_SIZE_PORTABILITY.md).
 *
 * Part B also carries the arithmetic proof for the
 * INC-D003-01 fix in tests/core/test_c008.c (RLIMIT_AS loop
 * cap raised 8192 -> 32768): see section B3.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if every
 * check passed. Prints only facts it observed or computed.
 */

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mimir_alloc.h"
#include "../../src/platform/mimir_platform.h"

/* Descriptor field offsets (B-006 S7.3), as used by the
 * banked tests/core/test_b007.c and test_c006.c readers. */
#define DESC_OFF_MAPPING_LENGTH 16u
#define DESC_OFF_USABLE_OFFSET  24u
#define DESC_OFF_REQUESTED_SIZE 32u
#define DESC_OFF_USABLE_SIZE    40u

static int checks = 0;
static int failures = 0;

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
        if (p[i] != (unsigned char)((i * 31u + seed * 17u +
                                     (i >> 8)) & 0xffu)) {
            return 0;
        }
    }
    return 1;
}

static size_t rd_size(const unsigned char *d, size_t off)
{
    size_t v;

    memcpy(&v, d + off, sizeof v);
    return v;
}

/* ------------------------------------------------------------------ */
/* Reference checked round-up (B-007 A5/A6), re-derived in this test. */
/* Returns 0 on success, -1 on overflow (refusal before evaluation).  */
/* ------------------------------------------------------------------ */
static int ref_round_up_checked(size_t x, size_t page_size,
                                size_t *out_rounded)
{
    size_t page_count;

    if (page_size == 0) {
        return -1;
    }
    if (x > SIZE_MAX - (page_size - 1)) {           /* A5 pre-check */
        return -1;
    }
    page_count = (x + (page_size - 1)) / page_size; /* A6 division  */
    if (page_count > SIZE_MAX / page_size) {        /* A6 pre-check */
        return -1;
    }
    *out_rounded = page_count * page_size;
    return 0;
}

/* Independent oracle: exact result via unsigned __int128. */
static int oracle_round_up(size_t x, size_t page_size, size_t *out)
{
    unsigned __int128 pages;

    if (x == 0) {
        *out = 0;
        return 0;
    }
    pages = ((unsigned __int128)x + (page_size - 1)) / page_size;
    if (pages * page_size > (unsigned __int128)SIZE_MAX) {
        return -1;
    }
    *out = (size_t)(pages * page_size);
    return 0;
}

/* ================================================================== */
/* PART B [ARITHMETIC-LEVEL ONLY]                                     */
/* ================================================================== */
static void part_b(void)
{
    static const size_t pages[2] = {4096u, 16384u};
    static const size_t xs[] = {
        0u, 1u, 100u, 4095u, 4096u, 4097u, 8191u, 8192u, 8193u,
        16383u, 16384u, 16385u, 32768u, 32769u, 65536u,
        1048576u, 1048577u
    };
    size_t pi, xi;
    char label[160];

    printf("PART B [ARITHMETIC-LEVEL ONLY] checked round-up vs "
           "independent __int128 oracle, page sizes 4096 and 16384\n");

    /* B1: formula agreement + whole-page invariants. */
    for (pi = 0; pi < 2; pi++) {
        for (xi = 0; xi < sizeof xs / sizeof xs[0]; xi++) {
            size_t got = 0, want = 0;
            int grc = ref_round_up_checked(xs[xi], pages[pi], &got);
            int orc = oracle_round_up(xs[xi], pages[pi], &want);

            snprintf(label, sizeof label,
                     "B1 page=%zu x=%zu: ref==oracle (%zu)",
                     pages[pi], xs[xi], want);
            check(grc == orc && (grc != 0 || got == want), label);
            if (grc == 0) {
                snprintf(label, sizeof label,
                         "B1 page=%zu x=%zu: result is a whole "
                         "number of pages and >= x",
                         pages[pi], xs[xi]);
                check(got % pages[pi] == 0 && got >= xs[xi], label);
            }
        }
    }

    /* B1 overflow refusals at both page sizes. */
    {
        size_t out = 0;

        check(ref_round_up_checked(SIZE_MAX, 4096u, &out) != 0,
              "B1 page=4096 x=SIZE_MAX: overflow refused");
        check(ref_round_up_checked(SIZE_MAX, 16384u, &out) != 0,
              "B1 page=16384 x=SIZE_MAX: overflow refused");
        check(ref_round_up_checked(SIZE_MAX - 100u, 16384u, &out) != 0,
              "B1 page=16384 x=SIZE_MAX-100: overflow refused");
    }

    /* B2: allocator layout formula at both page sizes.
     * Layout (B-006 S7.1): usable_offset = one system page;
     * mapping = round_up(usable_offset + request). Expected
     * values below are hand-computed constants, stated here
     * so the formula is checked against arithmetic done
     * outside the code under test. */
    {
        static const struct { size_t page, req, want; } cases[] = {
            {4096u,  1u,     8192u},
            {4096u,  4096u,  8192u},
            {4096u,  4097u,  12288u},
            {4096u,  32769u, 40960u},
            {16384u, 1u,     32768u},
            {16384u, 4096u,  32768u},
            {16384u, 16385u, 49152u},
            {16384u, 32769u, 65536u}
        };
        size_t i;

        for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
            size_t got = 0;
            int rc = ref_round_up_checked(cases[i].page + cases[i].req,
                                          cases[i].page, &got);

            snprintf(label, sizeof label,
                     "B2 page=%zu req=%zu: mapping length == %zu",
                     cases[i].page, cases[i].req, cases[i].want);
            check(rc == 0 && got == cases[i].want, label);
        }
    }

    /* B3: INC-D003-01 arithmetic (test_c008 RLIMIT_AS case).
     * Minimum VA cost of one mimir_malloc(4096) is one reserve
     * page + round_up(4096) of usable space. Under a 96 MiB
     * RLIMIT_AS, a loop capped at 8192 blocks can consume at
     * most 8192 * 8192 = 64 MiB of allocator mappings on a
     * 4096-page host -- below the limit, so the refusal the
     * case exists to observe is not guaranteed there. The
     * raised cap 32768 guarantees it at both target sizes. */
    {
        size_t min_map_4k = 0, min_map_16k = 0;
        const size_t limit = (size_t)96u * 1024u * 1024u;

        (void)ref_round_up_checked(4096u + 4096u, 4096u, &min_map_4k);
        (void)ref_round_up_checked(16384u + 4096u, 16384u, &min_map_16k);
        check(min_map_4k == 8192u && min_map_16k == 32768u,
              "B3 min VA per malloc(4096): 8192 @4K pages, "
              "32768 @16K pages");
        check((unsigned __int128)8192u * min_map_4k <
              (unsigned __int128)limit,
              "B3 old cap 8192 @4K pages: 64 MiB < 96 MiB limit "
              "(refusal NOT guaranteed -- the INC-D003-01 defect)");
        check((unsigned __int128)32768u * min_map_4k >
              (unsigned __int128)limit &&
              (unsigned __int128)32768u * min_map_16k >
              (unsigned __int128)limit,
              "B3 fixed cap 32768: refusal guaranteed at both "
              "4096 and 16384 page sizes");
    }
}

/* ================================================================== */
/* PART A [MEASURED ON-DEVICE]                                        */
/* ================================================================== */
static void part_a(void)
{
    size_t page = 0;
    long queried;
    int status;
    char label[192];
    size_t sizes[10];
    size_t nsizes = 0, i;

    printf("PART A [MEASURED ON-DEVICE] runtime page size and "
           "allocator behaviour on this host\n");

    /* A1: L6 discovery agrees with an independent sysconf. */
    status = mimir_platform_page_size(&page);
    queried = sysconf(_SC_PAGESIZE);
    printf("runtime system page size: L6 reports %zu bytes, "
           "independent sysconf(_SC_PAGESIZE) reports %ld\n",
           page, queried);
    check(status == MIMIR_PLATFORM_OK && page > 0,
          "A1 L6 page-size discovery succeeds");
    check(queried > 0 && page == (size_t)queried,
          "A1 L6 value == independent sysconf value");
    check(page > 0 && (page & (page - 1)) == 0,
          "A1 discovered page size is a nonzero power of two");

    if (page == 0) {
        check(0, "A2/A3 skipped: no discovered page size");
        return;
    }

    /* A2: L6 mapping acquire: exact whole-page lengths,
     * page-aligned bases, whole-mapping pattern, exact release. */
    {
        size_t reqs[5];

        reqs[0] = 1;
        reqs[1] = page - 1;
        reqs[2] = page;
        reqs[3] = page + 1;
        reqs[4] = 2 * page + 1;
        for (i = 0; i < 5; i++) {
            void *base = NULL;
            size_t mlen = 0, want = 0;

            status = mimir_platform_map_acquire(reqs[i], &base, &mlen);
            (void)oracle_round_up(reqs[i], page, &want);
            snprintf(label, sizeof label,
                     "A2 acquire(%zu): length == round_up(req, "
                     "runtime page) == %zu", reqs[i], want);
            check(status == MIMIR_PLATFORM_OK && base != NULL &&
                  mlen == want, label);
            if (base != NULL) {
                unsigned char *b = (unsigned char *)base;

                snprintf(label, sizeof label,
                         "A2 acquire(%zu): base is runtime-page "
                         "aligned", reqs[i]);
                check((uintptr_t)base % page == 0, label);
                fill_pattern(b, mlen, 7u);
                check(pattern_ok(b, mlen, 7u),
                      "A2 whole mapped extent pattern verifies");
                check(mimir_platform_map_release(base, mlen) ==
                      MIMIR_PLATFORM_OK,
                      "A2 release with exact acquired pair succeeds");
            }
        }
    }

    /* A3: allocator level -- user pointer, descriptor fields
     * and pattern, for runtime-derived sizes (plus the fixed
     * ordinary request sizes 4095/4096/4097, which are byte
     * counts, never page claims: MA-6.2-041). */
    sizes[nsizes++] = 1;
    sizes[nsizes++] = 100;
    sizes[nsizes++] = 4095;
    sizes[nsizes++] = 4096;
    sizes[nsizes++] = 4097;
    sizes[nsizes++] = page - 1;
    sizes[nsizes++] = page;
    sizes[nsizes++] = page + 1;
    sizes[nsizes++] = 32769;
    for (i = 0; i < nsizes; i++) {
        size_t n = sizes[i];
        unsigned char *p = (unsigned char *)mimir_malloc(n);

        snprintf(label, sizeof label,
                 "A3 malloc(%zu) succeeds", n);
        check(p != NULL, label);
        if (p == NULL) {
            continue;
        }
        snprintf(label, sizeof label,
                 "A3 malloc(%zu): user pointer is runtime-page "
                 "aligned", n);
        check((uintptr_t)p % page == 0, label);
        {
            const unsigned char *d = p - page;
            size_t want_mlen = 0;

            (void)oracle_round_up(page + n, page, &want_mlen);
            snprintf(label, sizeof label,
                     "A3 malloc(%zu): descriptor usable_offset == "
                     "runtime page, mapping_length == %zu, "
                     "usable_size == mapping_length - page",
                     n, want_mlen);
            check(rd_size(d, DESC_OFF_USABLE_OFFSET) == page &&
                  rd_size(d, DESC_OFF_MAPPING_LENGTH) == want_mlen &&
                  rd_size(d, DESC_OFF_REQUESTED_SIZE) == n &&
                  rd_size(d, DESC_OFF_USABLE_SIZE) == want_mlen - page,
                  label);
        }
        fill_pattern(p, n, 99u);
        snprintf(label, sizeof label,
                 "A3 malloc(%zu): requested extent pattern "
                 "verifies", n);
        check(pattern_ok(p, n, 99u), label);
        mimir_free(p);
    }
}

int main(void)
{
    printf("D-003 page-size portability test\n");
    part_b();
    part_a();
    printf("CHECKS: %d run, %d failed\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
