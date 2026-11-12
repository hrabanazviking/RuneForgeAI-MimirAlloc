/* test_z077.c - Z-077 trim and reallocarray contract test
 * (Wave 9). Static link against the production objects
 * (LIBOBJS), so the E-005 audit counters appended for the
 * two new entry points are readable through
 * tests/abi/mimir_audit_e005.h, and the internal I-003 /
 * Z-048 / J-002 query surfaces are readable through the
 * tests/core test-only headers.
 *
 * What is proven (the Z-077 contract as written in
 * include/mimir_alloc.h and docs/Z13_Z077_TRIM_
 * REALLOCARRAY.md; behaviour by analogy under
 * [UNRESOLVED: U-03] exactly as the header states it):
 *  - mimir_reallocarray success paths: null ptr as
 *    mimir_malloc(product); grow / shrink / same-product
 *    with min(old, new) bytes preserved byte-for-byte;
 *    the large (mapping-path) product likewise.
 *  - Overflow edges, honoured exactly: the product is
 *    checked before the multiplication is relied on --
 *    (SIZE_MAX, 2), (2, SIZE_MAX), (SIZE_MAX/2 + 1, 2),
 *    (SIZE_MAX, SIZE_MAX) and (2^32, 2^32) (whose product
 *    wraps to 0) are all exactly NULL, counted as genuine
 *    failures (J-002 alloc_failures), never as posture
 *    nulls, and no wrapped-around size is allocated.
 *    With a live ptr, overflow and an unsatisfiable
 *    representable product are exactly NULL with the
 *    original block byte-intact, still owned, and
 *    freeable exactly as before.
 *  - Zero factors are posture only (U-09 / U-12 NOT
 *    settled; Z-078 owns the closure): NULL with a live
 *    ptr left live and intact, counted as posture nulls
 *    (J-002 alloc_posture_nulls), never as failures.
 *  - mimir_trim over the I-003 machinery: pad == 0 is
 *    the full trim (retained empty spans AND the Z-048
 *    mapping pool); pad > 0 keeps pad bytes of retained
 *    spans rounded UP to whole spans and never touches
 *    the pool; the return is exactly 1 iff something was
 *    actually released, else exactly 0; repeated calls
 *    are idempotent and the allocator stays usable.
 *  - Audit counters: each new L1 counter moves exactly
 *    once per call of its entry point, failures and
 *    postures included (every public call in this file
 *    goes through a counting wrapper and the totals are
 *    reconciled at the end).
 *
 * Standalone on purpose: built and run via
 * `make test-z077`; NOT part of `make test`, so the
 * standing gate count is unchanged. The dynamic and C++
 * consumers live in test_z077_dyn.c / test_z077_cpp.cpp;
 * the ABI recorded run is tests/abi/run_z077.sh, filed as
 * tests/abi/recorded_run_z13_z077.txt.
 */
#include "mimir_alloc.h"
#include "mimir_audit_e005.h"
#include "mimir_pool_z048.h"
#include "mimir_stats_j002.h"
#include "mimir_trim_i003.h"

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

static unsigned long my_trim_calls = 0;
static unsigned long my_ra_calls = 0;

static int t_trim(size_t pad)
{
    my_trim_calls++;
    return mimir_trim(pad);
}

static void *t_ra(void *ptr, size_t nmemb, size_t size)
{
    my_ra_calls++;
    return mimir_reallocarray(ptr, nmemb, size);
}

static void fill(unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)(seed + i * 31u);
    }
}

static int intact(const unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (p[i] != (unsigned char)(seed + i * 31u)) {
            return 0;
        }
    }
    return 1;
}

struct stats_pair {
    uint64_t failures;
    uint64_t postures;
};

static struct stats_pair snap(void)
{
    struct mimir_stats st;
    struct stats_pair out;
    if (mimir_core_stats_snapshot(&st) != 0) {
        out.failures = (uint64_t)-1;
        out.postures = (uint64_t)-1;
        return out;
    }
    out.failures = st.alloc_failures;
    out.postures = st.alloc_posture_nulls;
    return out;
}

static void *worker_ra(void *arg)
{
    unsigned char *p;
    (void)arg;
    p = mimir_reallocarray(NULL, 8, 32);
    if (p == NULL) {
        return (void *)1;
    }
    fill(p, 256, 77u);
    if (!intact(p, 256, 77u)) {
        return (void *)2;
    }
    mimir_free(p);
    return NULL;
}

int main(void)
{
    const size_t ps = 4096; /* this host: 4 KiB pages */
    unsigned long trim0 = load(&mimir_audit_call_trim);
    unsigned long ra0 = load(&mimir_audit_call_reallocarray);
    struct stats_pair s0, s1;
    unsigned char *p, *q;
    void *blocks[1600];
    size_t i, n;
    int r;

    printf("=== test_z077: trim + reallocarray contract ===\n");

    /* ---- S1 reallocarray success paths (small) ---- */
    printf("-- S1 success paths --\n");
    p = t_ra(NULL, 4, 16); /* 64 bytes, as malloc(64) */
    CHECK(p != NULL);
    fill(p, 64, 3u);
    CHECK(intact(p, 64, 3u));
    q = t_ra(p, 8, 16); /* grow to 128 */
    CHECK(q != NULL);
    CHECK(intact(q, 64, 3u)); /* min(old, new) preserved */
    fill(q + 64, 64, 9u);
    p = t_ra(q, 2, 16); /* shrink to 32 */
    CHECK(p != NULL);
    CHECK(intact(p, 32, 3u));
    q = t_ra(p, 4, 8); /* same product 32, other factors */
    CHECK(q != NULL);
    CHECK(intact(q, 32, 3u));
    mimir_free(q);
    p = t_ra(NULL, 1000, 1000); /* exact 1,000,000 product */
    CHECK(p != NULL);
    fill(p, 1000000, 5u);
    CHECK(intact(p, 1000000, 5u));
    mimir_free(p);

    /* ---- S2 overflow edges, null ptr ---- */
    printf("-- S2 overflow edges (null ptr) --\n");
    s0 = snap();
    CHECK(t_ra(NULL, SIZE_MAX, 2) == NULL);
    CHECK(t_ra(NULL, 2, SIZE_MAX) == NULL);
    CHECK(t_ra(NULL, SIZE_MAX / 2 + 1, 2) == NULL);
    CHECK(t_ra(NULL, SIZE_MAX, SIZE_MAX) == NULL);
    /* 2^32 x 2^32 wraps to exactly 0 in 64-bit size_t:
     * the checked form must refuse it, never allocate 0. */
    CHECK(t_ra(NULL, (size_t)1 << 32, (size_t)1 << 32) == NULL);
    s1 = snap();
    CHECK(s1.failures - s0.failures == 5);
    CHECK(s1.postures - s0.postures == 0);
    /* Representable but unsatisfiable: also a genuine
     * failure NULL, with no allocation and no posture. */
    CHECK(t_ra(NULL, SIZE_MAX, 1) == NULL);
    s0 = snap();
    CHECK(s0.failures - s1.failures == 1);
    CHECK(s0.postures - s1.postures == 0);
    p = mimir_malloc(64); /* allocator unharmed */
    CHECK(p != NULL);
    mimir_free(p);

    /* ---- S3 overflow / failure with a live ptr ---- */
    printf("-- S3 failure preservation (live ptr) --\n");
    p = mimir_malloc(1000);
    CHECK(p != NULL);
    fill(p, 1000, 21u);
    s0 = snap();
    CHECK(t_ra(p, SIZE_MAX / 2 + 1, 2) == NULL); /* overflow */
    CHECK(intact(p, 1000, 21u));
    CHECK(t_ra(p, 1, SIZE_MAX / 2) == NULL); /* unsatisfiable */
    CHECK(intact(p, 1000, 21u));
    s1 = snap();
    CHECK(s1.failures - s0.failures == 2);
    CHECK(s1.postures - s0.postures == 0);
    q = t_ra(p, 25, 40); /* same product 1000: still owned */
    CHECK(q != NULL);
    CHECK(intact(q, 1000, 21u));
    mimir_free(q); /* freeable exactly as before */

    /* ---- S4 zero factors: posture only ---- */
    printf("-- S4 zero factors (posture) --\n");
    s0 = snap();
    CHECK(t_ra(NULL, 0, 16) == NULL);
    CHECK(t_ra(NULL, 16, 0) == NULL);
    CHECK(t_ra(NULL, 0, 0) == NULL);
    p = mimir_malloc(256);
    CHECK(p != NULL);
    fill(p, 256, 33u);
    CHECK(t_ra(p, 0, 16) == NULL); /* live ptr, product 0 */
    CHECK(intact(p, 256, 33u));    /* original live, intact */
    mimir_free(p);
    s1 = snap();
    CHECK(s1.postures - s0.postures == 4);
    CHECK(s1.failures - s0.failures == 0);

    /* ---- S5 worker-thread reallocarray ---- */
    printf("-- S5 worker thread --\n");
    {
        pthread_t th;
        void *res = NULL;
        CHECK(pthread_create(&th, NULL, worker_ra, NULL) == 0);
        CHECK(pthread_join(th, &res) == 0);
        CHECK(res == NULL);
    }
    /* NOTE: the worker called mimir_reallocarray directly,
     * outside the counting wrappers: account for its one
     * call so the S8 reconciliation stays exact. */
    my_ra_calls += 1;

    /* ---- S6 trim over retained spans ---- */
    printf("-- S6 trim: retained spans --\n");
    n = 0;
    for (i = 0; i < 1600; i++) {
        /* spread across many small size classes */
        blocks[n] = mimir_malloc(16 + (i % 60) * 16);
        if (blocks[n] != NULL) {
            n++;
        }
    }
    CHECK(n >= 1200);
    for (i = 0; i < n; i++) {
        mimir_free(blocks[i]);
    }
    {
        size_t spans_before;
        size_t bytes_before;
        /* Settle first: every I-003 trim form (and this
         * one) opens with the TLS flush + drain preamble,
         * which can itself retire drained empty spans
         * beyond the retention bound -- the same side
         * effect mimir_core_trim_retained has. One
         * keep-everything call absorbs it; the pad-budget
         * assertions below are then exact. */
        r = t_trim(SIZE_MAX);
        CHECK(r == 0);
        spans_before = mimir_core_retained_spans();
        bytes_before = mimir_core_retained_bytes();
        CHECK(spans_before >= 1);
        CHECK(bytes_before == spans_before * ps);
        /* A pad that covers everything releases nothing. */
        r = t_trim(SIZE_MAX);
        CHECK(r == 0);
        CHECK(mimir_core_retained_bytes() == bytes_before);
        r = t_trim(bytes_before);
        CHECK(r == 0);
        CHECK(mimir_core_retained_bytes() == bytes_before);
        /* Keep exactly one page: surplus spans released. */
        r = t_trim(ps);
        CHECK(mimir_core_retained_bytes() == ps);
        CHECK(r == (spans_before > 1 ? 1 : 0));
        /* Full trim releases the rest; then idempotent. */
        r = t_trim(0);
        CHECK(r == 1);
        CHECK(mimir_core_retained_bytes() == 0);
        r = t_trim(0);
        CHECK(r == 0);
    }
    p = mimir_malloc(128); /* allocator unharmed */
    CHECK(p != NULL);
    mimir_free(p);

    /* ---- S7 large reallocarray + pool trim ---- */
    printf("-- S7 large path + mapping pool --\n");
    p = t_ra(NULL, 1024, 1024); /* 1 MiB, mapping path */
    CHECK(p != NULL);
    fill(p, 1048576, 51u);
    q = t_ra(p, 2048, 1024); /* grow to 2 MiB */
    CHECK(q != NULL);
    CHECK(intact(q, 1048576, 51u));
    mimir_free(q);
    CHECK(mimir_core_pool_count() >= 1); /* Z-048 cache */
    {
        size_t pool_before = mimir_core_pool_count();
        r = t_trim(ps); /* pad > 0 never touches the pool */
        CHECK(mimir_core_pool_count() == pool_before);
        r = t_trim(0); /* full trim releases the pool */
        CHECK(r == 1);
        CHECK(mimir_core_pool_count() == 0);
        CHECK(mimir_core_pool_bytes() == 0);
    }

    /* ---- S8 audit counter reconciliation ---- */
    printf("-- S8 audit counters --\n");
    CHECK(load(&mimir_audit_call_trim) - trim0 == my_trim_calls);
    CHECK(load(&mimir_audit_call_reallocarray) - ra0 ==
          my_ra_calls);
    printf("trim calls: %lu, reallocarray calls: %lu\n",
           my_trim_calls, my_ra_calls);

    printf("test_z077: %d checks, %d failed\n", checks_run,
           checks_failed);
    printf("RESULT: %s\n", checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
