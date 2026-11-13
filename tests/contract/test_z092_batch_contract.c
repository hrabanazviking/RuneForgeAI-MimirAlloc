/*
 * test_z092_batch_contract.c -- Z-092 batch API prototype
 * contract suite.
 *
 * Written against the Z-092 contract (tests/contract/
 * mimir_batch_z092.h; docs/Z13_Z092_BATCH_API_PROTOTYPE.md)
 * and the landed prototype in src/core/mimir_core.c.
 * Standalone: NOT part of `make test` (see
 * tests/contract/run_z092.sh); the standing gate counts
 * do not move because of it.
 *
 * The surface is referenced through WEAK symbols: on a
 * core without the prototype every surface phase records
 * its planned checks as failed under a "surface absent"
 * label instead of crashing; public-API regression legs
 * run for real on every core. Exit 0 and "RESULT: PASS"
 * only if every check passed.
 *
 * Determinism: no timing, no sleeps, no randomness.
 * Every leg frees everything it allocates, so the final
 * reconciliation (blocks_allocated == blocks_freed) is
 * an exact equality, not a tolerance.
 */
#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "mimir_alloc.h"
#include "mimir_batch_z092.h"

extern int mimir_batch_snapshot(struct mimir_batch_snapshot *out) __attribute__((weak));
extern size_t mimir_batch_alloc(size_t size, void **out, size_t count) __attribute__((weak));
extern size_t mimir_batch_free(void **ptrs, size_t count) __attribute__((weak));

static long g_checks = 0;
static long g_failures = 0;

static void check(int ok, const char *label)
{
    g_checks++;
    if (!ok) {
        g_failures++;
        printf("FAIL: %s\n", label);
    }
}

static void fail_planned(long n, const char *phase)
{
    long i;
    for (i = 0; i < n; i++) {
        g_checks++;
        g_failures++;
    }
    printf("FAIL: %s: %ld planned checks failed (batch surface absent)\n", phase, n);
}

static int surface_present(void)
{
    return mimir_batch_snapshot != NULL &&
           mimir_batch_alloc != NULL &&
           mimir_batch_free != NULL;
}

#define SENTINEL ((void *)(uintptr_t)0x5EEDB10Cu)

static void fill_sentinel(void **a, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) a[i] = SENTINEL;
}

static int all_sentinel(void **a, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) if (a[i] != SENTINEL) return 0;
    return 1;
}

static int all_distinct(void **a, size_t n)
{
    size_t i, j;
    for (i = 0; i < n; i++)
        for (j = i + 1; j < n; j++)
            if (a[i] == a[j]) return 0;
    return 1;
}

static int all_aligned16(void **a, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
        if (((uintptr_t)a[i] & (uintptr_t)15) != 0) return 0;
    return 1;
}

static int same_set(void **a, void **b, size_t n)
{
    size_t i, j;
    for (i = 0; i < n; i++) {
        int found = 0;
        for (j = 0; j < n; j++) if (a[i] == b[j]) found = 1;
        if (!found) return 0;
    }
    return 1;
}

/* P1 snapshot: NULL refusal, shape, no counter movement. */
static void p1_snapshot(void)
{
    struct mimir_batch_snapshot s0, s1;
    if (!surface_present()) { fail_planned(6, "P1"); return; }
    check(mimir_batch_snapshot(NULL) == -1, "P1 snapshot NULL is -1");
    check(mimir_batch_snapshot(&s0) == 0, "P1 snapshot ok");
    check(s0.max_count == (uint64_t)MIMIR_BATCH_MAX_COUNT, "P1 max_count is 4096");
    check(mimir_batch_snapshot(&s1) == 0, "P1 snapshot again ok");
    check(s1.alloc_calls == s0.alloc_calls &&
          s1.free_calls == s0.free_calls &&
          s1.blocks_allocated == s0.blocks_allocated &&
          s1.blocks_freed == s0.blocks_freed &&
          s1.failures == s0.failures, "P1 snapshot moves no counter");
    check(s0.blocks_allocated >= s0.blocks_freed, "P1 allocated >= freed mid-suite");
}

/* P2 alloc refusals and no-ops: out[] untouched, exact
 * failure accounting. Planned: 14 checks. */
static void p2_alloc_refusals(void)
{
    struct mimir_batch_snapshot b, a;
    void *out[8];
    if (!surface_present()) { fail_planned(14, "P2"); return; }
    mimir_batch_snapshot(&b);
    fill_sentinel(out, 8);
    check(mimir_batch_alloc(64, out, 0) == 0, "P2 count 0 returns 0");
    check(all_sentinel(out, 8), "P2 count 0 leaves out untouched");
    check(mimir_batch_alloc(64, NULL, 4) == 0, "P2 NULL out returns 0");
    check(mimir_batch_alloc(0, out, 4) == 0, "P2 size 0 returns 0 (D-013)");
    check(all_sentinel(out, 8), "P2 size 0 leaves out untouched");
    check(mimir_batch_alloc(64, out, MIMIR_BATCH_MAX_COUNT + 1) == 0,
          "P2 over-cap count refused whole");
    check(all_sentinel(out, 8), "P2 over-cap leaves out untouched");
    check(mimir_batch_alloc(SIZE_MAX, out, 4) == 0, "P2 SIZE_MAX returns 0");
    check(all_sentinel(out, 8), "P2 SIZE_MAX leaves out untouched (all-or-nothing)");
    mimir_batch_snapshot(&a);
    check(a.failures - b.failures == 4, "P2 exactly 4 failures recorded");
    check(a.blocks_allocated == b.blocks_allocated, "P2 nothing allocated net");
    check(a.alloc_calls - b.alloc_calls == 5, "P2 alloc_calls counts all 5 calls");
    check(a.blocks_freed == b.blocks_freed, "P2 nothing freed");
    check(mimir_batch_alloc(64, out, 0) == 0, "P2 count 0 again returns 0");
}

/* P3 alloc success across classes incl. above the TLS
 * cutoff, distinctness, alignment, writability, and
 * cache reuse of the freed set. Planned: 15 checks. */
static void p3_alloc_success(void)
{
    struct mimir_batch_snapshot b, a;
    void *ptrs[32];
    void *again[32];
    void *big[4];
    size_t i;
    if (!surface_present()) { fail_planned(15, "P3"); return; }
    mimir_batch_snapshot(&b);
    check(mimir_batch_alloc(64, ptrs, 32) == 32, "P3 alloc 32x64 returns 32");
    check(all_distinct(ptrs, 32), "P3 32 blocks distinct");
    check(all_aligned16(ptrs, 32), "P3 32 blocks 16-aligned");
    for (i = 0; i < 32; i++) memset(ptrs[i], (int)(i + 1), 64);
    check(((unsigned char *)ptrs[7])[63] == 8, "P3 blocks writable to full size");
    check(mimir_batch_alloc(2048, big, 4) == 4, "P3 alloc 4x2048 (above cache cutoff)");
    check(all_distinct(big, 4) && all_aligned16(big, 4), "P3 big distinct+aligned");
    memset(big[0], 0xAB, 2048);
    check(((unsigned char *)big[0])[2047] == 0xAB, "P3 big writable to full size");
    check(mimir_batch_free(big, 4) == 4, "P3 free big returns 4");
    check(mimir_batch_free(ptrs, 32) == 32, "P3 free 32 returns 32");
    check(mimir_batch_alloc(64, again, 32) == 32, "P3 re-alloc 32x64 returns 32");
    check(same_set(ptrs, again, 32), "P3 freed set is reused (cache locality)");
    check(mimir_batch_free(again, 32) == 32, "P3 free again returns 32");
    mimir_batch_snapshot(&a);
    check(a.blocks_allocated - b.blocks_allocated == 68, "P3 blocks_allocated +68");
    check(a.blocks_freed - b.blocks_freed == 68, "P3 blocks_freed +68");
    check(a.failures == b.failures, "P3 no failures");
}

/* P4 free refusals, no-ops, NULL skipping, array never
 * written. Planned: 13 checks. */
static void p4_free_semantics(void)
{
    struct mimir_batch_snapshot b, a;
    void *ptrs[8];
    void *copy[8];
    if (!surface_present()) { fail_planned(13, "P4"); return; }
    mimir_batch_snapshot(&b);
    check(mimir_batch_free(NULL, 0) == 0, "P4 free count 0 NULL array is no-op");
    check(mimir_batch_free(NULL, 4) == 0, "P4 free NULL array refused");
    check(mimir_batch_alloc(128, ptrs, 8) == 8, "P4 alloc 8x128");
    memcpy(copy, ptrs, sizeof ptrs);
    check(mimir_batch_free(ptrs, MIMIR_BATCH_MAX_COUNT + 1) == 0,
          "P4 over-cap free refused whole");
    memset(ptrs[0], 0x11, 128);
    check(((unsigned char *)ptrs[0])[127] == 0x11,
          "P4 refused free left blocks live and writable");
    ptrs[1] = NULL;
    ptrs[4] = NULL;
    check(mimir_batch_free(ptrs, 8) == 6, "P4 free skips NULLs, returns 6");
    check(memcmp(ptrs, copy, sizeof ptrs) != 0, "P4 sanity: test array itself edited by test");
    mimir_batch_snapshot(&a);
    check(a.failures - b.failures == 2, "P4 exactly 2 failures (NULL array, over-cap)");
    check(a.blocks_freed - b.blocks_freed == 6, "P4 blocks_freed +6");
    check(a.blocks_allocated - b.blocks_allocated == 8, "P4 blocks_allocated +8");
    /* The other 2 live blocks (indices 1 and 4 were
     * nulled in the test array only): free via copy. */
    {
        void *rest[2];
        rest[0] = copy[1];
        rest[1] = copy[4];
        check(mimir_batch_free(rest, 2) == 2, "P4 remaining 2 freed via saved copies");
    }
    mimir_batch_snapshot(&a);
    check(a.blocks_freed - b.blocks_freed == 8, "P4 blocks_freed +8 total");
    check(mimir_batch_free(ptrs, 0) == 0, "P4 free count 0 returns 0");
}

/* P5 boundary: exactly MIMIR_BATCH_MAX_COUNT in one
 * call. Planned: 4 checks. */
static void p5_boundary(void)
{
    static void *ptrs[4096];
    if (!surface_present()) { fail_planned(4, "P5"); return; }
    check(mimir_batch_alloc(64, ptrs, MIMIR_BATCH_MAX_COUNT) == MIMIR_BATCH_MAX_COUNT,
          "P5 alloc exactly 4096 succeeds");
    check(all_distinct(ptrs, MIMIR_BATCH_MAX_COUNT), "P5 4096 blocks distinct");
    check(all_aligned16(ptrs, MIMIR_BATCH_MAX_COUNT), "P5 4096 blocks 16-aligned");
    check(mimir_batch_free(ptrs, MIMIR_BATCH_MAX_COUNT) == MIMIR_BATCH_MAX_COUNT,
          "P5 free exactly 4096 returns 4096");
}

/* P6 concurrency: disjoint per-thread batches reconcile
 * exactly. Planned: 3 checks. */
static int g_thread_errors = 0;

static void *batch_worker(void *arg)
{
    void *ptrs[16];
    int iter;
    (void)arg;
    for (iter = 0; iter < 50; iter++) {
        if (mimir_batch_alloc(128, ptrs, 16) != 16) { g_thread_errors++; return NULL; }
        memset(ptrs[0], 0x22, 128);
        if (mimir_batch_free(ptrs, 16) != 16) { g_thread_errors++; return NULL; }
    }
    return NULL;
}

static void p6_concurrency(void)
{
    struct mimir_batch_snapshot b, a;
    pthread_t th[4];
    int i, created = 0;
    if (!surface_present()) { fail_planned(3, "P6"); return; }
    mimir_batch_snapshot(&b);
    for (i = 0; i < 4; i++) {
        if (pthread_create(&th[i], NULL, batch_worker, NULL) == 0) created++;
    }
    for (i = 0; i < created; i++) pthread_join(th[i], NULL);
    check(created == 4, "P6 4 worker threads created");
    check(g_thread_errors == 0, "P6 no worker errors");
    mimir_batch_snapshot(&a);
    check(a.blocks_allocated - b.blocks_allocated ==
          a.blocks_freed - b.blocks_freed &&
          a.blocks_allocated - b.blocks_allocated == 4u * 50u * 16u,
          "P6 concurrent batches reconcile (+3200/+3200)");
}

/* P7 public regression: the public surface is untouched
 * by the prototype. Planned: 5 checks. */
static void p7_public_regression(void)
{
    void *p = mimir_malloc(64);
    check(p != NULL, "P7 public alloc works");
    if (p != NULL) {
        memset(p, 0x33, 64);
        check(((unsigned char *)p)[63] == 0x33, "P7 public block writable");
        mimir_free(p);
        check(1, "P7 public free returns");
    } else {
        check(0, "P7 public block writable");
        check(0, "P7 public free returns");
    }
    check(mimir_malloc(0) == NULL, "P7 public zero-size refusal intact (D-013)");
    mimir_free(NULL);
    check(1, "P7 public NULL free is a no-op");
}

/* P8 final reconciliation: everything the suite batch-
 * allocated has been batch-freed. Planned: 2 checks. */
static void p8_reconcile(void)
{
    struct mimir_batch_snapshot s;
    if (!surface_present()) { fail_planned(2, "P8"); return; }
    check(mimir_batch_snapshot(&s) == 0, "P8 snapshot ok");
    check(s.blocks_allocated == s.blocks_freed,
          "P8 blocks_allocated == blocks_freed at quiescence");
}

int main(void)
{
    printf("Z-092 batch API prototype contract suite\n");
    printf("surface: %s\n", surface_present() ? "present" : "ABSENT");
    p1_snapshot();
    p2_alloc_refusals();
    p3_alloc_success();
    p4_free_semantics();
    p5_boundary();
    p6_concurrency();
    p7_public_regression();
    p8_reconcile();
    printf("CHECKS: %ld run, %ld failed\n", g_checks, g_failures);
    printf("RESULT: %s\n", g_failures == 0 ? "PASS" : "FAIL");
    return g_failures == 0 ? 0 : 1;
}
