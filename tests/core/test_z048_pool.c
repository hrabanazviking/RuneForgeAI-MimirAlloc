/* test_z048_pool.c - Z-048 mapping-cache churn cases.
 *
 * The standing gate's extension for the Z-048 slice
 * (roadmap: "Standing gate extended with large-path churn
 * cases"). Standalone via make test-z048; NOT part of
 * make test (standing gate composition untouched, the
 * test-z043 precedent).
 *
 * Proves the Z-048 semantics against the production core:
 *  1. a freed large mapping is POOLED, not unmapped
 *     (pool queries move; Q-RELEASED does not; Q-MAPPED
 *     is unchanged by the free - the bytes stay mapped);
 *  2. a same-length allocation republishes the SAME
 *     mapping (exact-length reuse), and calloc served
 *     from the pool is zero over the whole extent (the
 *     publication zeroing, not the fresh-mmap property);
 *  3. reuse is size-segregated: a different length misses;
 *  4. the pool is bounded by count (64) and by bytes
 *     (32 MiB): the retire that would cross a bound takes
 *     the munmap path, counted in Q-RELEASED;
 *  5. trim and drain release the whole pool (the roadmap's
 *     trim trigger), counted in Q-RELEASED;
 *  6. realloc grow-by-move republishes a pooled mapping;
 *  7. the J-006 metadata walk stays at 0 violations with
 *     the pool populated, after reuse, and after trim;
 *  8. cross-thread and single-thread churn leave the
 *     books exact: published == retired, live == 0.
 *
 * All expectations are computed from the host page size
 * discovered at runtime; nothing is hardcoded to 4096.
 */
#define _POSIX_C_SOURCE 200809L
#include "mimir_alloc.h"
#include "mimir_stats_j002.h"
#include "mimir_trim_i003.h"
#include "mimir_drain_h001.h"
#include "mimir_integrity_j006.h"
#include "mimir_pool_z048.h"
#include "../abi/mimir_audit_e005.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int checks_run;
static int checks_failed;

#define CHECK(cond, label) do { \
    checks_run++; \
    if (!(cond)) { \
        checks_failed++; \
        printf("  [FAIL] %s\n", label); \
    } \
} while (0)

static uint64_t P; /* system page size, discovered */

static uint64_t large_len(size_t req)
{
    return ((P + (uint64_t)req + P - 1) / P) * P;
}

static struct mimir_stats snap(void)
{
    struct mimir_stats s;
    if (mimir_core_stats_snapshot(&s) != 0) {
        checks_run++;
        checks_failed++;
        printf("  [FAIL] stats snapshot rc != 0\n");
        memset(&s, 0, sizeof s);
    }
    return s;
}

static unsigned long live_mappings(void)
{
    return mimir_audit_alloc_published -
           mimir_audit_mapping_retired;
}

static void fill(unsigned char *p, size_t n, unsigned char v)
{
    size_t i;
    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)(v + (i & 0x3Fu));
    }
}

static int filled(const unsigned char *p, size_t n, unsigned char v)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (p[i] != (unsigned char)(v + (i & 0x3Fu))) {
            return 0;
        }
    }
    return 1;
}

/* Cross-thread churn worker: alternate two large sizes. */
static void *churn_worker(void *arg)
{
    long id = (long)(intptr_t)arg;
    int i;
    for (i = 0; i < 200; i++) {
        size_t n = (i & 1) ? 16384 : 8192;
        unsigned char *p = mimir_malloc(n);
        if (p == NULL) {
            return (void *)(intptr_t)1;
        }
        p[0] = (unsigned char)(id + i);
        p[n - 1] = (unsigned char)(id + i + 1);
        if (p[0] != (unsigned char)(id + i) ||
            p[n - 1] != (unsigned char)(id + i + 1)) {
            return (void *)(intptr_t)2;
        }
        mimir_free(p);
    }
    return NULL;
}

int main(void)
{
    long ps = sysconf(_SC_PAGESIZE);
    struct mimir_stats s0, s1;
    void *p, *q;
    size_t i;

    printf("test_z048_pool: Z-048 mapping cache churn cases\n");
    CHECK(ps > 0, "page size discovered via sysconf");
    P = (uint64_t)ps;
    printf("  page size = %llu bytes\n", (unsigned long long)P);

    /* ---- 1. a freed large mapping is pooled, not unmapped */
    printf("Case 1: free pools the mapping\n");
    CHECK(mimir_core_pool_count() == 0, "pool empty at start");
    CHECK(mimir_core_pool_bytes() == 0, "pool bytes 0 at start");
    s0 = snap();
    p = mimir_malloc(8192);
    CHECK(p != NULL, "malloc 8192 non-NULL");
    fill(p, 8192, 0x30);
    s1 = snap();
    CHECK(s1.live_mappings == 1, "one live mapping while held");
    mimir_free(p);
    CHECK(mimir_core_pool_count() == 1, "pool count 1 after free");
    CHECK(mimir_core_pool_bytes() == large_len(8192),
          "pool bytes == mapping_length(8192)");
    CHECK(live_mappings() == 0, "published == retired after free");
    s1 = snap();
    CHECK(s1.live_mappings == 0, "stats live_mappings 0 after free");
    CHECK(s1.bytes_released == s0.bytes_released,
          "Q-RELEASED unmoved by a pooled free");
    CHECK(s1.bytes_mapped == s0.bytes_mapped + large_len(8192),
          "Q-MAPPED still carries the pooled mapping");
    CHECK(mimir_core_metadata_check() == 0,
          "metadata check 0 with pool populated");

    /* ---- 2. exact-length reuse + calloc cleanliness */
    printf("Case 2: exact reuse, calloc zeroed from the pool\n");
    q = mimir_malloc(8192);
    CHECK(q == p, "same-length malloc republishes the pooled mapping");
    CHECK(mimir_core_pool_count() == 0, "pool drained by the hit");
    fill(q, 8192, 0xAB);
    CHECK(filled(q, 8192, 0xAB), "reused block writable over extent");
    mimir_free(q);
    q = mimir_calloc(1, 8192);
    CHECK(q == p, "calloc also served from the pool");
    {
        int zeroed = 1;
        for (i = 0; i < 8192; i++) {
            if (((unsigned char *)q)[i] != 0) {
                zeroed = 0;
            }
        }
        CHECK(zeroed, "calloc extent zero over a dirty pooled mapping");
    }
    mimir_free(q);
    CHECK(mimir_core_pool_count() == 1, "pool 1 after calloc free");

    /* ---- 3. size segregation: a different length misses */
    printf("Case 3: size-segregated reuse\n");
    {
        void *a = mimir_malloc(16384);
        void *b;
        CHECK(a != NULL && a != p, "different length misses the pool");
        CHECK(mimir_core_pool_count() == 1,
              "missed pool entry untouched");
        mimir_free(a); /* pools the 16384 length beside the 8192 */
        CHECK(mimir_core_pool_count() == 2, "two lengths pooled");
        CHECK(mimir_core_pool_bytes() ==
              large_len(8192) + large_len(16384),
              "pool bytes == both mapping lengths");
        b = mimir_malloc(16384);
        CHECK(b == a, "16384 request takes the 16384 entry");
        q = mimir_malloc(8192);
        CHECK(q == p, "8192 request takes the 8192 entry");
        mimir_free(b);
        mimir_free(q);
    }

    /* ---- 4. trim releases the whole pool */
    printf("Case 4: trim is the pool release trigger\n");
    s0 = snap();
    {
        uint64_t pooled = mimir_core_pool_bytes();
        CHECK(pooled == large_len(8192) + large_len(16384),
              "pool holds both lengths before trim");
        (void)mimir_core_trim_retained();
        s1 = snap();
        CHECK(mimir_core_pool_count() == 0, "pool empty after trim");
        CHECK(s1.bytes_released - s0.bytes_released == pooled,
              "trim moved Q-RELEASED by exactly the pooled bytes");
        CHECK(s0.bytes_mapped - s1.bytes_mapped == pooled,
              "trim dropped Q-MAPPED by exactly the pooled bytes");
    }
    CHECK(mimir_core_metadata_check() == 0,
          "metadata check 0 after trim");

    /* ---- 5. drain releases the pool too */
    printf("Case 5: drain releases the pool\n");
    p = mimir_malloc(8192);
    mimir_free(p);
    CHECK(mimir_core_pool_count() == 1, "pool 1 before drain");
    (void)mimir_core_drain_retained();
    CHECK(mimir_core_pool_count() == 0, "pool empty after drain");

    /* ---- 6. count bound: 65 distinct lengths, 64 pooled */
    printf("Case 6: count bound 64\n");
    {
        void *blocks[65];
        uint64_t expect_pool_bytes = 0;
        int all_ok = 1;
        for (i = 0; i < 65; i++) {
            blocks[i] = mimir_malloc(2048 + i * 4096);
            if (blocks[i] == NULL) {
                all_ok = 0;
            }
        }
        CHECK(all_ok, "65 distinct-length blocks allocated");
        s0 = snap();
        for (i = 0; i < 65; i++) {
            mimir_free(blocks[i]);
            if (i < 64) {
                expect_pool_bytes += large_len(2048 + i * 4096);
            }
        }
        s1 = snap();
        CHECK(mimir_core_pool_count() == 64, "pool capped at 64");
        CHECK(mimir_core_pool_bytes() == expect_pool_bytes,
              "pool bytes == the first 64 mapping lengths");
        CHECK(s1.bytes_released - s0.bytes_released ==
              large_len(2048 + 64 * 4096),
              "the 65th retire unmapped, counted in Q-RELEASED");
        (void)mimir_core_trim_retained();
        CHECK(mimir_core_pool_count() == 0, "pool cleared by trim");
    }

    /* ---- 7. byte bound: a second 20 MiB retire cannot pool */
    printf("Case 7: byte bound 32 MiB\n");
    {
        unsigned char *big1 = mimir_malloc(20u * 1024u * 1024u);
        unsigned char *big2 = mimir_malloc(20u * 1024u * 1024u + 8192u);
        CHECK(big1 != NULL && big2 != NULL, "two 20 MiB blocks live");
        big1[0] = 1; big1[20u * 1024u * 1024u - 1] = 2;
        big2[0] = 3; big2[20u * 1024u * 1024u + 8191u] = 4;
        mimir_free(big1);
        CHECK(mimir_core_pool_count() == 1,
              "first 20 MiB retire pooled");
        s0 = snap();
        mimir_free(big2);
        s1 = snap();
        CHECK(mimir_core_pool_count() == 1,
              "second 20 MiB retire exceeds the byte bound: not pooled");
        CHECK(s1.bytes_released - s0.bytes_released ==
              large_len(20u * 1024u * 1024u + 8192u),
              "overflow retire unmapped, counted in Q-RELEASED");
        (void)mimir_core_trim_retained();
        CHECK(mimir_core_pool_count() == 0, "pool cleared by trim");
    }

    /* ---- 8. realloc grow-by-move republishes a pooled mapping */
    printf("Case 8: realloc grow-by-move pool hit\n");
    p = mimir_malloc(8192);
    mimir_free(p);
    CHECK(mimir_core_pool_count() == 1, "pool primed for realloc");
    {
        unsigned char *small = mimir_malloc(100);
        unsigned char *grown;
        CHECK(small != NULL, "small block for realloc");
        fill(small, 100, 0x51);
        grown = mimir_realloc(small, 8192);
        CHECK(grown == p, "grow-by-move landed on the pooled mapping");
        CHECK(grown != NULL && filled(grown, 100, 0x51),
              "grow preserved the requested prefix");
        mimir_free(grown);
    }
    (void)mimir_core_trim_retained();

    /* ---- 9. single-thread churn over the K-002 large sizes */
    printf("Case 9: churn, 3000 iterations, K-002 large sizes\n");
    {
        static const size_t sizes[6] = {
            2048, 8192, 16384, 65536, 262144, 1048576
        };
        int ok = 1;
        for (i = 0; i < 3000; i++) {
            size_t n = sizes[i % 6];
            unsigned char *b = mimir_malloc(n);
            if (b == NULL) {
                ok = 0;
                break;
            }
            b[0] = (unsigned char)i;
            b[n - 1] = (unsigned char)(i + 1);
            if (b[0] != (unsigned char)i ||
                b[n - 1] != (unsigned char)(i + 1)) {
                ok = 0;
            }
            mimir_free(b);
        }
        CHECK(ok, "3000 churn iterations pattern-exact");
        CHECK(live_mappings() == 0, "churn leaves no live mapping");
        CHECK(mimir_core_pool_count() <= 64 &&
              mimir_core_pool_bytes() <= 32u * 1024u * 1024u,
              "pool within bounds after churn");
        CHECK(mimir_core_metadata_check() == 0,
              "metadata check 0 after churn");
    }

    /* ---- 10. cross-thread churn */
    printf("Case 10: cross-thread churn, 4 threads x 200\n");
    {
        pthread_t th[4];
        int created = 0;
        int joined_ok = 1;
        long t;
        for (t = 0; t < 4; t++) {
            if (pthread_create(&th[t], NULL, churn_worker,
                               (void *)(intptr_t)t) == 0) {
                created++;
            }
        }
        CHECK(created == 4, "4 churn threads created");
        for (t = 0; t < created; t++) {
            void *ret = NULL;
            if (pthread_join(th[t], &ret) != 0 || ret != NULL) {
                joined_ok = 0;
            }
        }
        CHECK(joined_ok, "all churn threads pattern-exact");
        CHECK(live_mappings() == 0,
              "cross-thread churn leaves no live mapping");
        CHECK(mimir_core_metadata_check() == 0,
              "metadata check 0 after cross-thread churn");
    }

    /* ---- final: quiesce and reconcile */
    (void)mimir_core_trim_retained();
    (void)mimir_core_drain_retained();
    CHECK(mimir_core_pool_count() == 0, "pool empty at quiesce");
    CHECK(mimir_core_pool_bytes() == 0, "pool bytes 0 at quiesce");
    CHECK(live_mappings() == 0, "published == retired at quiesce");
    CHECK(mimir_core_metadata_check() == 0,
          "metadata check 0 at quiesce");

    printf("CHECKS: %d run, %d failed\n", checks_run, checks_failed);
    if (checks_failed == 0) {
        printf("RESULT: PASS\n");
        return 0;
    }
    printf("RESULT: FAIL\n");
    return 1;
}
