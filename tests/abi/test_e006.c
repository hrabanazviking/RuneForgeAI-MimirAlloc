/*
 * test_e006.c - MimirAlloc slice E-006 alignment lifecycle
 * test (Wave 5, Phase 5; E-001 task T6).
 *
 * This program links ONLY against the shared library
 * (build/libmimiralloc.so, via `make test-e006`), never the
 * loose objects, so its evidence is lifecycle-through-the-
 * linked-library evidence in the sense of E-001 X3. It uses
 * only the four public symbols from include/mimir_alloc.h.
 * No aligned-allocation entry point is called, because none
 * exists: this slice is the decision record on that absence
 * (docs/PHASE_5_ALIGNED_SURFACE_E006.md), and the hard law of
 * the slice is no silent ABI expansion.
 *
 * What it measures, on this host, is the actual alignment
 * of pointers returned by the existing four symbols:
 *   - the CONTRACT FLOOR: (uintptr_t)ptr %
 *     _Alignof(max_align_t) == 0. The header's claim is the
 *     non-numeric "suitably aligned for any object type
 *     with a fundamental alignment requirement" (MA-6.1-
 *     MALLOC-01 by analogy; U-15: no numeric value is
 *     claimed). _Alignof(max_align_t) is its operational
 *     form on this target, as in D-002 / D-008. Floor
 *     results are CHECKS: a failure fails the test.
 *   - the CONSTRUCTION FACT: this mapping-per-allocation
 *     baseline returns mapping_base + one system page, so
 *     returned pointers are in fact page-aligned (D-008
 *     Section 2, INC-D008-08). Page-modulus results are
 *     printed as OBSERVED lines and counted in the final
 *     OBSERVED summary. They are NEVER checks and never
 *     promoted to claims: a future redesign may lawfully
 *     reduce delivered alignment to the floor.
 *   - the OVER-ALIGNMENT DEMAND LADDER (Phase B): moduli
 *     32..16384 checked against pointers obtained from
 *     plain mimir_malloc. On this host (page 16384) the
 *     construction satisfies all of them; the checks are
 *     recorded as CONSTRUCTION checks - they describe this
 *     build on this host, and the slice doc says so. The
 *     32768 modulus (2x page) is OBSERVED only: the
 *     construction does not deliver it by construction.
 *
 * Phases:
 *   ENV  host facts: runtime page size, _Alignof
 *        (max_align_t), pointer width.
 *   A    mimir_malloc ladder: 24 sizes (odd, boundary,
 *        page-crossing, large), all live at once, pairwise
 *        distinct, filled and verified over the full
 *        requested extent; floor checked per pointer,
 *        page modulus observed.
 *   B    over-alignment demand ladder via mimir_malloc.
 *   C    mimir_calloc paths: floor + zeroed extent.
 *   D    mimir_realloc lifecycle chain: grow / grow /
 *        shrink / same-size / realloc(NULL, n) / grow,
 *        floor + prefix preservation at every step.
 *   E    interleave: free one block, allocate another;
 *        the surviving block is undisturbed; free(NULL)
 *        is a defined no-op.
 *   F    defined failures (oversize malloc, calloc
 *        overflow) return exactly NULL, and the allocator
 *        still serves a normal request afterwards.
 *
 * Zero-size and errno are NOT exercised: U-09 / U-12 /
 * U-10 are unresolved and no posture is codified here.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. OBSERVED lines never affect the
 * result. Standalone on purpose: built and run via `make
 * test-e006`; NOT part of `make test`. See
 * docs/PHASE_5_ALIGNED_SURFACE_E006.md and the recorded
 * run tests/abi/recorded_run_e006.txt.
 */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mimir_alloc.h"

static int checks_run;
static int checks_failed;
static long obs_returns;      /* non-null mimir_ returns   */
static long obs_page_mod;     /* of those, 0 mod page size */
static long page_sz;

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

static int mod_ok(const void *ptr, size_t modulus)
{
    return ptr != NULL &&
           ((uintptr_t)ptr % (uintptr_t)modulus) == 0;
}

static void observe(const void *ptr)
{
    if (ptr != NULL) {
        obs_returns++;
        if (page_sz > 0 &&
            ((uintptr_t)ptr % (uintptr_t)page_sz) == 0)
            obs_page_mod++;
    }
}

static const size_t ladder[] = {
    1, 3, 7, 16, 17, 31, 64, 65, 100, 127, 256, 257, 1000,
    4096, 4097, 8192, 16383, 16384, 16385, 32768, 65536,
    100000, 262144, 1048576
};
#define LADDER_N (sizeof ladder / sizeof ladder[0])

int main(void)
{
    size_t i, j;
    void *live[LADDER_N];

    printf("E-006 alignment lifecycle test (linked against "
           "libmimiralloc.so only)\n");

    /* ENV: host facts measured in this run. */
    page_sz = sysconf(_SC_PAGESIZE);
    printf("ENV pagesize=%ld alignof_max_align_t=%zu "
           "sizeof_max_align_t=%zu ptr_bits=%zu\n",
           page_sz, (size_t)_Alignof(max_align_t),
           sizeof(max_align_t), sizeof(void *) * 8);
    CHECK(page_sz > 0);
    CHECK(_Alignof(max_align_t) == 16);
    CHECK(sizeof(void *) * 8 == 64);

    /* Phase A: mimir_malloc ladder, all live at once. */
    printf("PHASE A mimir_malloc ladder (%zu sizes)\n",
           LADDER_N);
    for (i = 0; i < LADDER_N; i++) {
        int verified = 1;
        live[i] = mimir_malloc(ladder[i]);
        observe(live[i]);
        CHECK(live[i] != NULL);
        CHECK(mod_ok(live[i], _Alignof(max_align_t)));
        if (live[i] != NULL) {
            unsigned char pat =
                (unsigned char)(i * 31u + 7u);
            memset(live[i], pat, ladder[i]);
            for (j = 0; j < ladder[i]; j++)
                if (((unsigned char *)live[i])[j] != pat)
                    verified = 0;
        }
        CHECK(live[i] != NULL && verified);
    }
    for (i = 0; i < LADDER_N; i++) {
        int distinct = 1;
        for (j = i + 1; j < LADDER_N; j++)
            if (live[i] != NULL && live[i] == live[j])
                distinct = 0;
        CHECK(distinct);
    }
    for (i = 0; i < LADDER_N; i++)
        mimir_free(live[i]);

    /* Phase B: over-alignment demand ladder. Every modulus
     * up to the page size is a CONSTRUCTION check on this
     * host (see header): plain mimir_malloc(4096) returns
     * a fresh mapping + one page, so the pointer is page-
     * aligned by construction. 2x page is OBSERVED only. */
    printf("PHASE B over-alignment demand via mimir_malloc\n");
    {
        static const size_t demands[] = {
            32, 64, 128, 256, 1024, 4096, 16384
        };
        size_t k;
        for (k = 0; k < sizeof demands / sizeof demands[0];
             k++) {
            void *p = mimir_malloc(4096);
            observe(p);
            printf("  demand %zu: ptr mod demand = %zu\n",
                   demands[k],
                   p != NULL ? (size_t)((uintptr_t)p %
                                        (uintptr_t)demands[k])
                             : (size_t)0 - 1);
            CHECK(p != NULL);
            CHECK(mod_ok(p, demands[k]));
            mimir_free(p);
        }
        {
            void *p = mimir_malloc(4096);
            observe(p);
            printf("OBSERVED demand 32768 (2x page): "
                   "ptr mod 32768 = %zu (observation only; "
                   "not delivered by construction)\n",
                   p != NULL ? (size_t)((uintptr_t)p %
                                        (uintptr_t)32768)
                             : (size_t)0 - 1);
            mimir_free(p);
        }
    }

    /* Phase C: mimir_calloc floor + zeroed extent. */
    printf("PHASE C mimir_calloc paths\n");
    {
        static const struct { size_t n, s; } cases[] = {
            {1, 1}, {3, 7}, {16, 32}, {100, 100},
            {1, 65537}, {7, 9999}
        };
        size_t k;
        for (k = 0; k < sizeof cases / sizeof cases[0]; k++) {
            size_t total = cases[k].n * cases[k].s;
            size_t t;
            int zeroed = 1;
            unsigned char *p =
                mimir_calloc(cases[k].n, cases[k].s);
            observe(p);
            CHECK(p != NULL);
            CHECK(mod_ok(p, _Alignof(max_align_t)));
            if (p != NULL)
                for (t = 0; t < total; t++)
                    if (p[t] != 0)
                        zeroed = 0;
            CHECK(p != NULL && zeroed);
            mimir_free(p);
        }
    }

    /* Phase D: mimir_realloc lifecycle chain. */
    printf("PHASE D mimir_realloc lifecycle chain\n");
    {
        unsigned char *p = mimir_malloc(37);
        unsigned char *q;
        size_t t;
        int preserved;
        observe(p);
        CHECK(p != NULL);
        CHECK(mod_ok(p, _Alignof(max_align_t)));
        if (p != NULL)
            for (t = 0; t < 37; t++)
                p[t] = (unsigned char)(t + 1);
        q = mimir_realloc(p, 1000);
        observe(q);
        CHECK(q != NULL);
        CHECK(mod_ok(q, _Alignof(max_align_t)));
        preserved = 1;
        if (q != NULL)
            for (t = 0; t < 37; t++)
                if (q[t] != (unsigned char)(t + 1))
                    preserved = 0;
        CHECK(q != NULL && preserved);
        p = q;
        if (p != NULL)
            for (t = 0; t < 1000; t++)
                p[t] = (unsigned char)(t * 7 + 3);
        q = mimir_realloc(p, 70000);
        observe(q);
        CHECK(q != NULL);
        CHECK(mod_ok(q, _Alignof(max_align_t)));
        preserved = 1;
        if (q != NULL)
            for (t = 0; t < 1000; t++)
                if (q[t] != (unsigned char)(t * 7 + 3))
                    preserved = 0;
        CHECK(q != NULL && preserved);
        p = q;
        q = mimir_realloc(p, 13);
        observe(q);
        CHECK(q != NULL);
        CHECK(mod_ok(q, _Alignof(max_align_t)));
        preserved = 1;
        if (q != NULL)
            for (t = 0; t < 13; t++)
                if (q[t] != (unsigned char)(t * 7 + 3))
                    preserved = 0;
        CHECK(q != NULL && preserved);
        p = q;
        q = mimir_realloc(p, 13);
        observe(q);
        CHECK(q != NULL);
        CHECK(mod_ok(q, _Alignof(max_align_t)));
        preserved = 1;
        if (q != NULL)
            for (t = 0; t < 13; t++)
                if (q[t] != (unsigned char)(t * 7 + 3))
                    preserved = 0;
        CHECK(q != NULL && preserved);
        mimir_free(q);
        q = mimir_realloc(NULL, 41);
        observe(q);
        CHECK(q != NULL);
        CHECK(mod_ok(q, _Alignof(max_align_t)));
        if (q != NULL)
            for (t = 0; t < 41; t++)
                q[t] = (unsigned char)(0xC0 + t);
        p = mimir_realloc(q, 20000);
        observe(p);
        CHECK(p != NULL);
        CHECK(mod_ok(p, _Alignof(max_align_t)));
        preserved = 1;
        if (p != NULL)
            for (t = 0; t < 41; t++)
                if (p[t] != (unsigned char)(0xC0 + t))
                    preserved = 0;
        CHECK(p != NULL && preserved);
        mimir_free(p);
    }

    /* Phase E: interleave + free(NULL) no-op. */
    printf("PHASE E interleave lifecycle\n");
    {
        unsigned char *a = mimir_malloc(100);
        unsigned char *b = mimir_malloc(200);
        unsigned char *c;
        size_t t;
        int intact = 1;
        observe(a);
        observe(b);
        if (b != NULL)
            memset(b, 0x5A, 200);
        mimir_free(a);
        c = mimir_malloc(300);
        observe(c);
        CHECK(c != NULL);
        CHECK(mod_ok(c, _Alignof(max_align_t)));
        CHECK(c != b);
        if (b != NULL)
            for (t = 0; t < 200; t++)
                if (b[t] != 0x5A)
                    intact = 0;
        CHECK(b != NULL && intact);
        mimir_free(b);
        mimir_free(c);
        mimir_free(NULL);
        CHECK(1);
    }

    /* Phase F: defined failures, then recovery. */
    printf("PHASE F defined failures + recovery\n");
    {
        void *bad = mimir_malloc(SIZE_MAX);
        void *ovf = mimir_calloc(SIZE_MAX / 2 + 1, 2);
        void *ok;
        CHECK(bad == NULL);
        CHECK(ovf == NULL);
        ok = mimir_malloc(48);
        observe(ok);
        CHECK(ok != NULL);
        CHECK(mod_ok(ok, _Alignof(max_align_t)));
        mimir_free(ok);
    }

    printf("OBSERVED page-modulus: %ld of %ld non-null "
           "mimir_ returns were 0 mod pagesize=%ld "
           "(construction observation, NOT a claim; "
           "INC-D008-08)\n",
           obs_page_mod, obs_returns, page_sz);
    printf("CHECKS: %d run, %d failed\n",
           checks_run, checks_failed);
    printf("RESULT: %s\n",
           checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
