/* test_z074.c - Z-074 aligned allocation contract test
 * (Wave 9). Static link against the production objects
 * (LIBOBJS), so the E-005 audit counters appended for the
 * two new entry points are readable through
 * tests/abi/mimir_audit_e005.h.
 *
 * What is proven (the Z-073 contract as implemented by
 * Z-074; behaviour by analogy under [UNRESOLVED: U-03]
 * exactly as the public header states it):
 *  - mimir_aligned_alloc over the alignment ladder
 *    1..1048576 x a size ladder: non-null, the exact
 *    modulus (uintptr_t)ptr % alignment == 0, the full
 *    requested extent writable and readable back, all
 *    blocks distinct while live, every block releasable
 *    by mimir_free.
 *  - mimir_posix_memalign over a subset ladder: return 0,
 *    *memptr set and aligned as above.
 *  - Invalid domains: alignment 0 and non-powers of two
 *    are exactly NULL / EINVAL (with *memptr unchanged);
 *    for the posix shape, powers of two below
 *    sizeof(void *) are EINVAL too (the POSIX multiple
 *    rule). A null memptr is EINVAL (recorded choice).
 *  - Valid but unsatisfiable is failure, never
 *    invalidity: alignment 2^63 and oversize sizes are
 *    NULL / ENOMEM with *memptr unchanged.
 *  - Validation precedence (posix shape): memptr, then
 *    alignment, then the zero-size posture -- a bad
 *    alignment with size 0 is EINVAL, not success.
 *  - Zero-size postures (U-09 unresolved; the Z-073
 *    contract's interim pin): mimir_aligned_alloc(a, 0)
 *    is NULL; the posix shape returns ENOMEM with
 *    *memptr unchanged.
 *  - Free / realloc interplay: aligned blocks free with
 *    mimir_free; realloc same-size and shrink return the
 *    original pointer, grow preserves the prefix (the
 *    result's alignment is NOT asserted -- the contract
 *    promises only the family floor there); the allocator
 *    stays healthy across aligned free + re-alloc cycles
 *    (the Z-048 pool republish course).
 *  - Cross-thread: an aligned block allocated on the main
 *    thread is freed on a worker thread; a worker thread
 *    allocates and frees its own aligned blocks.
 *  - Audit counters: each new L1 counter moves exactly
 *    once per call of its entry point, failures included.
 *
 * Standalone on purpose: built and run via
 * `make test-z074`; NOT part of `make test`, so the
 * standing gate count is unchanged. The dynamic and C++
 * consumers live in test_z074_dyn.c / test_z074_cpp.cpp;
 * the ABI recorded run is tests/abi/run_z074.sh, filed as
 * tests/abi/recorded_run_z13_z074.txt.
 */
#include "mimir_alloc.h"
#include "mimir_audit_e005.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks_run++;                                                   \
        if (!(cond)) {                                                  \
            checks_failed++;                                            \
            printf("  [FAIL] %s (line %d)\n", #cond, __LINE__);         \
        }                                                               \
    } while (0)

static unsigned long load(unsigned long *counter)
{
    return __atomic_load_n(counter, __ATOMIC_RELAXED);
}

static void fill(unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)(seed + (i * 31u));
    }
}

static int verify(const unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (p[i] != (unsigned char)(seed + (i * 31u))) {
            return 0;
        }
    }
    return 1;
}

static void *worker_free_arg;
static void *worker_free_main(void *unused)
{
    (void)unused;
    mimir_free(worker_free_arg);
    return NULL;
}

static int worker_alloc_ok;
static void *worker_alloc_main(void *unused)
{
    int i;
    (void)unused;
    worker_alloc_ok = 1;
    for (i = 0; i < 32; i++) {
        unsigned char *p = mimir_aligned_alloc(128, 200);
        if (p == NULL || (uintptr_t)p % 128 != 0) {
            worker_alloc_ok = 0;
            mimir_free(p);
            return NULL;
        }
        fill(p, 200, (unsigned int)i);
        if (!verify(p, 200, (unsigned int)i)) {
            worker_alloc_ok = 0;
            mimir_free(p);
            return NULL;
        }
        mimir_free(p);
    }
    return NULL;
}

int main(void)
{
    static const size_t aligns[] = {
        1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048,
        4096, 8192, 16384, 65536, 1048576
    };
    static const size_t sizes[] = { 1, 37, 100, 1024, 1025,
                                    4096, 65536 };
    void *live[sizeof aligns / sizeof aligns[0] *
               (sizeof sizes / sizeof sizes[0])];
    size_t nlive = 0;
    size_t ai, si, i, j;

    printf("Z-074 aligned allocation contract test (static link)\n");

    /* Ladder: every (alignment, size) pair live at once. */
    for (ai = 0; ai < sizeof aligns / sizeof aligns[0]; ai++) {
        for (si = 0; si < sizeof sizes / sizeof sizes[0]; si++) {
            unsigned char *p =
                mimir_aligned_alloc(aligns[ai], sizes[si]);
            CHECK(p != NULL);
            if (p == NULL) {
                continue;
            }
            CHECK((uintptr_t)p % aligns[ai] == 0);
            fill(p, sizes[si], (unsigned int)(ai * 7 + si));
            live[nlive++] = p;
        }
    }
    for (i = 0; i < nlive; i++) {
        for (j = i + 1; j < nlive; j++) {
            CHECK(live[i] != live[j]);
        }
    }
    /* Contents survived the whole ladder being live. */
    {
        size_t k = 0;
        for (ai = 0; ai < sizeof aligns / sizeof aligns[0]; ai++) {
            for (si = 0; si < sizeof sizes / sizeof sizes[0]; si++) {
                CHECK(verify(live[k], sizes[si],
                             (unsigned int)(ai * 7 + si)));
                k++;
            }
        }
    }
    for (i = 0; i < nlive; i++) {
        mimir_free(live[i]);
    }

    /* Many small over-aligned blocks at once. */
    {
        void *many[200];
        int ok = 1;
        for (i = 0; i < 200; i++) {
            many[i] = mimir_aligned_alloc(64, 64);
            if (many[i] == NULL ||
                (uintptr_t)many[i] % 64 != 0) {
                ok = 0;
            }
        }
        CHECK(ok);
        for (i = 0; i < 200; i++) {
            for (j = i + 1; j < 200; j++) {
                if (many[i] == many[j]) {
                    ok = 0;
                }
            }
        }
        CHECK(ok);
        for (i = 0; i < 200; i++) {
            mimir_free(many[i]);
        }
    }

    /* posix shape: subset ladder. */
    {
        static const size_t pa[] = { 8, 16, 64, 4096, 8192,
                                     1048576 };
        static const size_t ps[] = { 1, 100, 5000 };
        for (ai = 0; ai < sizeof pa / sizeof pa[0]; ai++) {
            for (si = 0; si < sizeof ps / sizeof ps[0]; si++) {
                void *out = NULL;
                int rc = mimir_posix_memalign(&out, pa[ai],
                                              ps[si]);
                CHECK(rc == 0);
                CHECK(out != NULL);
                if (out == NULL) {
                    continue;
                }
                CHECK((uintptr_t)out % pa[ai] == 0);
                fill(out, ps[si], 3u);
                CHECK(verify(out, ps[si], 3u));
                mimir_free(out);
            }
        }
    }

    /* Invalid domains: mimir_aligned_alloc. */
    {
        static const size_t bad[] = { 0, 3, 5, 24, 48, 100,
                                      SIZE_MAX };
        for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            CHECK(mimir_aligned_alloc(bad[i], 64) == NULL);
        }
    }

    /* Valid but unsatisfiable: failure, never invalidity
     * (the posix shape proves the distinction: ENOMEM,
     * not EINVAL, and *memptr unchanged). */
    CHECK(mimir_aligned_alloc((size_t)1 << 63, 64) == NULL);
    CHECK(mimir_aligned_alloc(4096, SIZE_MAX - 100) == NULL);
    {
        void *sentinel = (void *)(uintptr_t)0x1;
        void *out = sentinel;
        CHECK(mimir_posix_memalign(&out, (size_t)1 << 63,
                                   64) == ENOMEM);
        CHECK(out == sentinel);
        out = sentinel;
        CHECK(mimir_posix_memalign(&out, 4096,
                                   SIZE_MAX - 100) == ENOMEM);
        CHECK(out == sentinel);
    }

    /* Invalid domains: posix shape, incl. precedence. */
    {
        static const size_t badp[] = { 0, 1, 2, 4, 3, 24,
                                       SIZE_MAX };
        void *sentinel = (void *)(uintptr_t)0x1;
        CHECK(mimir_posix_memalign(NULL, 64, 64) == EINVAL);
        for (i = 0; i < sizeof badp / sizeof badp[0]; i++) {
            void *out = sentinel;
            CHECK(mimir_posix_memalign(&out, badp[i], 64) ==
                  EINVAL);
            CHECK(out == sentinel);
        }
        /* Precedence: bad alignment + zero size is EINVAL;
         * null memptr + zero size is EINVAL. */
        {
            void *out = sentinel;
            CHECK(mimir_posix_memalign(&out, 3, 0) == EINVAL);
            CHECK(out == sentinel);
        }
        CHECK(mimir_posix_memalign(NULL, 64, 0) == EINVAL);
    }

    /* Zero-size postures (U-09; the Z-073 contract's
     * interim pin for this surface: aligned NULL; posix
     * ENOMEM with *memptr unchanged). */
    CHECK(mimir_aligned_alloc(64, 0) == NULL);
    {
        void *out = (void *)(uintptr_t)0x1;
        CHECK(mimir_posix_memalign(&out, 64, 0) == ENOMEM);
        CHECK(out == (void *)(uintptr_t)0x1);
    }
    /* The contract's named oversize cases (P3). */
    CHECK(mimir_aligned_alloc(16, SIZE_MAX) == NULL);
    CHECK(mimir_aligned_alloc(4096, SIZE_MAX - 7) == NULL);

    /* Free / realloc interplay on a mapping-routed block. */
    {
        unsigned char *p = mimir_aligned_alloc(256, 100);
        unsigned char *q;
        CHECK(p != NULL);
        fill(p, 100, 9u);
        q = mimir_realloc(p, 100);
        CHECK(q == p); /* same size: original pointer */
        q = mimir_realloc(p, 50);
        CHECK(q == p); /* shrink: original pointer */
        CHECK(verify(p, 50, 9u));
        q = mimir_realloc(p, 5000);
        CHECK(q != NULL);
        if (q != NULL) {
            CHECK(verify(q, 50, 9u)); /* prefix preserved */
            mimir_free(q);
        } else {
            mimir_free(p);
        }
    }
    /* Realloc interplay on a class-routed block. */
    {
        unsigned char *p = mimir_aligned_alloc(16, 32);
        unsigned char *q;
        CHECK(p != NULL);
        fill(p, 32, 11u);
        q = mimir_realloc(p, 3000);
        CHECK(q != NULL);
        if (q != NULL) {
            CHECK(verify(q, 32, 11u));
            mimir_free(q);
        } else {
            mimir_free(p);
        }
    }
    /* Aligned free + re-alloc cycles (pool republish). */
    {
        int ok = 1;
        for (i = 0; i < 50; i++) {
            unsigned char *p = mimir_aligned_alloc(4096, 8192);
            if (p == NULL || (uintptr_t)p % 4096 != 0) {
                ok = 0;
                mimir_free(p);
                break;
            }
            fill(p, 8192, (unsigned int)i);
            if (!verify(p, 8192, (unsigned int)i)) {
                ok = 0;
            }
            mimir_free(p);
        }
        CHECK(ok);
    }
    /* The family still works after all of the above. */
    {
        unsigned char *p = mimir_malloc(48);
        unsigned char *c = mimir_calloc(10, 10);
        CHECK(p != NULL && c != NULL);
        CHECK(c == NULL || c[0] == 0);
        mimir_free(p);
        mimir_free(c);
    }

    /* Cross-thread courses. */
    {
        pthread_t th;
        worker_free_arg = mimir_aligned_alloc(512, 300);
        CHECK(worker_free_arg != NULL);
        CHECK(pthread_create(&th, NULL, worker_free_main,
                             NULL) == 0);
        CHECK(pthread_join(th, NULL) == 0);
        CHECK(pthread_create(&th, NULL, worker_alloc_main,
                             NULL) == 0);
        CHECK(pthread_join(th, NULL) == 0);
        CHECK(worker_alloc_ok);
    }

    /* Audit counters: exactly one increment per call. */
    {
        unsigned long a0 = load(&mimir_audit_call_aligned_alloc);
        unsigned long p0 = load(&mimir_audit_call_posix_memalign);
        void *x1 = mimir_aligned_alloc(64, 64);
        void *x2 = mimir_aligned_alloc(64, 64);
        void *x3 = mimir_aligned_alloc(8192, 64);
        (void)mimir_aligned_alloc(3, 64);
        (void)mimir_aligned_alloc(0, 64);
        (void)mimir_aligned_alloc(64, 0);
        CHECK(load(&mimir_audit_call_aligned_alloc) - a0 == 6);
        mimir_free(x1);
        mimir_free(x2);
        mimir_free(x3);
        {
            void *o1 = NULL, *o2 = NULL;
            CHECK(mimir_posix_memalign(&o1, 64, 64) == 0);
            CHECK(mimir_posix_memalign(&o2, 64, 64) == 0);
            mimir_free(o1);
            mimir_free(o2);
            (void)mimir_posix_memalign(NULL, 64, 64);
            o1 = (void *)(uintptr_t)0x1;
            (void)mimir_posix_memalign(&o1, 3, 64);
            (void)mimir_posix_memalign(&o1, (size_t)1 << 63, 64);
            o1 = NULL;
            (void)mimir_posix_memalign(&o1, 64, 0);
            CHECK(load(&mimir_audit_call_posix_memalign) - p0 == 6);
        }
    }

    printf("Z074 STATIC: %d checks, %d failed\n", checks_run,
           checks_failed);
    printf("RESULT: %s\n", checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
