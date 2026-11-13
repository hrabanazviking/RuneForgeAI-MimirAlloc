/*
 * test_z079_budget.c -- Z-079 budget enforcement evidence
 * (roadmap Z-079: Phase-9 soft budgets enforced; L-03 /
 * D-04; INC-I001-14 discharged; decision D-014).
 *
 * Standalone via `make test-z079`; NOT part of `make test`
 * (the Z-052/Z-064 pattern), so the standing gate's counts
 * are untouched. Links the library OBJECTS directly.
 *
 * The design under test is I-002 S3.3: the budget caps
 * Q-MAPPED (live mapping bytes + live span bytes + pooled
 * mapping-cache bytes); growth is judged BEFORE backing
 * is acquired; over budget the allocator first reclaims
 * what it may release (retained EMPTY spans through the
 * one trim walk, then the mapping pool) and only then
 * refuses, with exactly NULL, the original state
 * untouched, and the refusal counted in Q-REFUSED-BUDGET
 * (snapshot field budget_failures / the lock-free getter)
 * -- never in alloc_failures (Q-FAILED-OS), never in
 * alloc_posture_nulls, and with no J-007 line.
 *
 * Legs:
 *  0. Quiesced baseline: budget unset (0) by default,
 *     both failure counters at their process-start values.
 *  1. SET/GET round-trip, incl. the snapshot's
 *     budget_mapped_bytes and unset-by-zero.
 *  2. Unset means unbounded: a 1 MiB mapping succeeds and
 *     no refusal is counted.
 *  3. OVER-BUDGET, LARGE PATH (the headline proof): a
 *     budget admitting exactly one 2048-byte mapping
 *     admits the first (equality), refuses the second
 *     (NULL, budget counter +1, OS-failure counter
 *     unmoved, Q-MAPPED unmoved); a freed mapping is then
 *     re-served from the pool under the same budget
 *     (reuse grows nothing); and a different-length fresh
 *     mapping under a below-current budget reclaims the
 *     pool before refusing (Q-MAPPED falls by the pooled
 *     bytes even though the call fails).
 *  4. RECLAIM BEFORE REFUSAL (spans): a retained EMPTY
 *     span (warmed by a worker thread so its exit drain
 *     leaves it centrally EMPTY) is released by the
 *     gate's reclaim step before the refusal lands.
 *  5. SERVE WITHOUT NEW BACKING: with the budget at 1
 *     byte, a TLS bin hit still succeeds (no growth, no
 *     refusal counted), while a bin miss of a fresh class
 *     is refused at its span acquire (+1).
 *  6. CALLOC AND REALLOC: a calloc over budget refuses
 *     (+1); a realloc grow over budget returns NULL with
 *     the original block live, byte-intact and freeable
 *     (+1, FC-2).
 *  7. COUNTER SEPARATION (FC-4): a zero-size contract
 *     NULL moves only alloc_posture_nulls; an oversize
 *     request moves only alloc_failures; neither moves
 *     the budget counter.
 *  8. ALIGNED SURFACE: mimir_aligned_alloc over budget
 *     refuses (+1); mimir_posix_memalign answers ENOMEM
 *     with *memptr unchanged (+1).
 *  9. UNSET RESTORES + FINAL IDENTITY: unsetting admits
 *     again; after all frees and a drain, Q-MAPPED is
 *     back at the leg-0 baseline and the budget counter
 *     equals the exact number of refusals the legs
 *     performed (8), with alloc_failures moved only by
 *     the one genuine oversize failure of leg 7.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mimir_alloc.h"
#include "mimir_budget_z079.h"
#include "mimir_stats_j002.h"

static int checks = 0;
static int failures = 0;

#define CHECK(cond, msg) do { \
        checks++; \
        if (!(cond)) { failures++; \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } \
    } while (0)

static struct mimir_stats snap(void)
{
    struct mimir_stats s;

    if (mimir_core_stats_snapshot(&s) != 0) {
        printf("FAIL: stats snapshot refused\n");
        failures++;
        memset(&s, 0, sizeof s);
    }
    return s;
}

static void *warm_worker(void *arg)
{
    void *p = mimir_malloc(24);

    (void)arg;
    if (p != NULL) {
        memset(p, 0x5A, 24);
        mimir_free(p);
    }
    return NULL;
}

int main(void)
{
    struct mimir_stats s0, s;
    size_t page, m0, m_live;
    uint64_t bf0, af0, pn0;
    void *p1, *p2, *p3, *q, *a, *b, *c, *d, *e, *f, *g;
    void *pv;
    pthread_t th;
    size_t retained;

    setvbuf(stdout, NULL, _IOLBF, 0);

    /* Leg 0: quiesce (drain releases retained spans and
     * the whole pool), then take the baseline. */
    (void)mimir_core_drain_retained();
    s0 = snap();
    page = (size_t)s0.page_size_bytes;
    CHECK(page == 4096, "leg0: this host is a 4 KiB-page host");
    CHECK(mimir_budget_get_mapped_bytes() == 0,
          "leg0: budget unset by default");
    CHECK(s0.budget_mapped_bytes == 0,
          "leg0: snapshot budget_mapped_bytes is 0 (unset)");
    m0 = mimir_core_mapped_bytes();
    bf0 = s0.budget_failures;
    af0 = s0.alloc_failures;
    pn0 = s0.alloc_posture_nulls;
    CHECK(mimir_core_budget_failures() == bf0,
          "leg0: lock-free getter agrees with the snapshot");
    printf("Z079 baseline: page=%zu mapped0=%zu "
           "budget_failures=%llu alloc_failures=%llu\n",
           page, m0, (unsigned long long)bf0,
           (unsigned long long)af0);

    /* Leg 1: set/get round-trip. */
    CHECK(mimir_budget_set_mapped_bytes(123456) == 0,
          "leg1: setter returns 0");
    CHECK(mimir_budget_get_mapped_bytes() == 123456,
          "leg1: getter reads the set value back exactly");
    s = snap();
    CHECK(s.budget_mapped_bytes == 123456,
          "leg1: snapshot carries the set budget");
    CHECK(mimir_budget_set_mapped_bytes(0) == 0,
          "leg1: unset returns 0");
    CHECK(mimir_budget_get_mapped_bytes() == 0,
          "leg1: zero unsets the budget");

    /* Leg 2: unset means unbounded. */
    p1 = mimir_malloc(1024 * 1024);
    CHECK(p1 != NULL, "leg2: 1 MiB mapping succeeds unset");
    if (p1 != NULL) {
        memset(p1, 0xA5, 4096);
    }
    mimir_free(p1);
    s = snap();
    CHECK(s.budget_failures == bf0,
          "leg2: no refusal is counted while unset");

    /* Leg 3: over-budget, large path. Drain first so the
     * leg-2 pooled mapping cannot be reclaimed into a
     * surprise admission; M is measured, never assumed. */
    (void)mimir_core_drain_retained();
    m_live = mimir_core_mapped_bytes();
    CHECK(mimir_budget_set_mapped_bytes(m_live + 2 * page) == 0,
          "leg3: budget set to exactly one 2048 mapping");
    p1 = mimir_malloc(2048);
    CHECK(p1 != NULL,
          "leg3: first mapping admitted (equality admitted)");
    CHECK(mimir_core_mapped_bytes() == m_live + 2 * page,
          "leg3: Q-MAPPED grew by exactly one mapping");
    p2 = mimir_malloc(2048);
    CHECK(p2 == NULL, "leg3: second mapping refused NULL");
    s = snap();
    CHECK(s.budget_failures == bf0 + 1,
          "leg3: budget counter moved by exactly 1");
    CHECK(s.alloc_failures == af0,
          "leg3: OS-failure counter unmoved by a refusal");
    CHECK(mimir_core_mapped_bytes() == m_live + 2 * page,
          "leg3: the refusal acquired nothing");
    if (p1 != NULL) {
        memset(p1, 0x3C, 2048);
        CHECK(((unsigned char *)p1)[0] == 0x3C &&
              ((unsigned char *)p1)[2047] == 0x3C,
              "leg3: the admitted block is fully usable");
    }
    mimir_free(p1); /* retires into the Z-048 pool */
    p3 = mimir_malloc(2048);
    CHECK(p3 != NULL,
          "leg3: pooled reuse is served under the budget");
    s = snap();
    CHECK(s.budget_failures == bf0 + 1,
          "leg3: reuse counted no refusal");
    mimir_free(p3);
    /* Below-current budget, fresh length: the gate must
     * reclaim the pooled mapping before refusing. The
     * budget is m_live + page, NOT m_live itself: the
     * drained baseline m_live can be 0, and 0 is the
     * unset sentinel, not a budget (finishing-lead fix;
     * the first draft set exactly m_live and the call
     * below was admitted by the unset budget). One page
     * of headroom still refuses the 3-page fresh mapping
     * after the pool (2 pages) is reclaimed. */
    CHECK(mimir_budget_set_mapped_bytes(m_live + page) == 0,
          "leg3: budget lowered below the pooled total");
    q = mimir_malloc(8192);
    CHECK(q == NULL, "leg3: fresh over-budget mapping refused");
    s = snap();
    CHECK(s.budget_failures == bf0 + 2,
          "leg3: second refusal counted");
    CHECK(mimir_core_mapped_bytes() == m_live,
          "leg3: the pool was reclaimed before the refusal");

    /* Leg 4: reclaim of a retained EMPTY span. */
    (void)mimir_core_drain_retained();
    m_live = mimir_core_mapped_bytes();
    CHECK(pthread_create(&th, NULL, warm_worker, NULL) == 0,
          "leg4: worker thread created");
    CHECK(pthread_join(th, NULL) == 0, "leg4: worker joined");
    retained = mimir_core_retained_bytes();
    CHECK(retained == page,
          "leg4: the warmed span is retained EMPTY (1 page)");
    CHECK(mimir_budget_set_mapped_bytes(m_live + page) == 0,
          "leg4: budget set at current Q-MAPPED");
    p1 = mimir_malloc(2048);
    CHECK(p1 == NULL,
          "leg4: mapping refused once reclaim is exhausted");
    CHECK(mimir_core_retained_bytes() == 0,
          "leg4: the retained span was reclaimed first");
    CHECK(mimir_core_mapped_bytes() == m_live,
          "leg4: Q-MAPPED fell by the reclaimed span");
    s = snap();
    CHECK(s.budget_failures == bf0 + 3,
          "leg4: third refusal counted");
    CHECK(mimir_budget_set_mapped_bytes(0) == 0,
          "leg4: budget unset again");

    /* Leg 5: serve without new backing vs fresh class. */
    a = mimir_malloc(32);
    CHECK(a != NULL, "leg5: class-32 warm allocation succeeds");
    CHECK(mimir_budget_set_mapped_bytes(1) == 0,
          "leg5: budget set to 1 byte");
    b = mimir_malloc(32);
    CHECK(b != NULL,
          "leg5: a TLS bin hit is served at a 1-byte budget");
    s = snap();
    CHECK(s.budget_failures == bf0 + 3,
          "leg5: the bin hit counted no refusal");
    mimir_free(b);
    c = mimir_malloc(48);
    CHECK(c == NULL,
          "leg5: a fresh class at a 1-byte budget is refused");
    s = snap();
    CHECK(s.budget_failures == bf0 + 4,
          "leg5: the span-acquire refusal is counted");
    CHECK(s.alloc_failures == af0,
          "leg5: OS-failure counter still unmoved");
    mimir_free(a);
    CHECK(mimir_budget_set_mapped_bytes(0) == 0,
          "leg5: budget unset");
    c = mimir_malloc(48);
    CHECK(c != NULL, "leg5: the same class succeeds unset");
    mimir_free(c);

    /* Leg 6: calloc and realloc refusals. */
    m_live = mimir_core_mapped_bytes();
    CHECK(mimir_budget_set_mapped_bytes(m_live) == 0,
          "leg6: budget set at current Q-MAPPED");
    d = mimir_calloc(3000, 2);
    CHECK(d == NULL, "leg6: over-budget calloc refused NULL");
    s = snap();
    CHECK(s.budget_failures == bf0 + 5,
          "leg6: calloc refusal counted");
    CHECK(mimir_budget_set_mapped_bytes(0) == 0,
          "leg6: budget unset for the realloc setup");
    e = mimir_malloc(2048);
    CHECK(e != NULL, "leg6: realloc source block allocated");
    if (e != NULL) {
        memset(e, 0x77, 2048);
    }
    m_live = mimir_core_mapped_bytes();
    CHECK(mimir_budget_set_mapped_bytes(m_live) == 0,
          "leg6: budget set at current Q-MAPPED");
    f = mimir_realloc(e, 100000);
    CHECK(f == NULL, "leg6: over-budget realloc grow refused");
    s = snap();
    CHECK(s.budget_failures == bf0 + 6,
          "leg6: realloc refusal counted exactly once");
    CHECK(e != NULL && ((unsigned char *)e)[0] == 0x77 &&
          ((unsigned char *)e)[2047] == 0x77,
          "leg6: the original block is byte-intact (FC-2)");
    mimir_free(e); /* must free exactly as before */
    CHECK(mimir_budget_set_mapped_bytes(0) == 0,
          "leg6: budget unset");

    /* Leg 7: counter separation. */
    p1 = mimir_malloc(0);
    CHECK(p1 == NULL, "leg7: zero-size is the contract NULL");
    s = snap();
    CHECK(s.alloc_posture_nulls == pn0 + 1,
          "leg7: the contract NULL moved only posture count");
    CHECK(s.budget_failures == bf0 + 6,
          "leg7: posture NULL is not a budget refusal");
    p1 = mimir_malloc(SIZE_MAX);
    CHECK(p1 == NULL, "leg7: oversize refused NULL");
    s = snap();
    CHECK(s.alloc_failures == af0 + 1,
          "leg7: oversize moved only the OS-failure counter");
    CHECK(s.budget_failures == bf0 + 6,
          "leg7: a genuine failure is not a budget refusal");

    /* Leg 8: aligned surface. The requests are 64 KiB,
     * larger than every reclaimable byte (retained spans
     * + pool) plus the budget headroom, so the gate must
     * refuse even after reclaiming everything it may
     * release (finishing-lead fix: the first draft's
     * 2048-byte request fit the budget once the gate
     * reclaimed the retained spans and the pool, and was
     * correctly admitted -- reclaim-before-refusal is
     * the design, not a defect). */
    m_live = mimir_core_mapped_bytes();
    CHECK(mimir_budget_set_mapped_bytes(m_live) == 0,
          "leg8: budget set at current Q-MAPPED");
    g = mimir_aligned_alloc(4096, 65536);
    CHECK(g == NULL, "leg8: over-budget aligned alloc refused");
    pv = (void *)(uintptr_t)0x1;
    CHECK(mimir_posix_memalign(&pv, 4096, 65536) == ENOMEM,
          "leg8: posix shape answers ENOMEM on a refusal");
    CHECK(pv == (void *)(uintptr_t)0x1,
          "leg8: memptr left unchanged by the refusal");
    s = snap();
    CHECK(s.budget_failures == bf0 + 8,
          "leg8: both aligned refusals counted");
    CHECK(mimir_budget_set_mapped_bytes(0) == 0,
          "leg8: budget unset");

    /* Leg 9: unset restores; final identity. */
    p1 = mimir_malloc(1024 * 1024);
    CHECK(p1 != NULL, "leg9: unset admits the 1 MiB mapping");
    mimir_free(p1);
    (void)mimir_core_drain_retained();
    s = snap();
    CHECK(s.budget_failures == bf0 + 8,
          "leg9: exactly the 8 leg refusals were counted");
    CHECK(mimir_core_budget_failures() == bf0 + 8,
          "leg9: the lock-free getter agrees at the end");
    CHECK(s.alloc_failures == af0 + 1,
          "leg9: OS failures moved only by the oversize leg");
    CHECK(mimir_core_mapped_bytes() == m0,
          "leg9: Q-MAPPED reconciles to the baseline");
    printf("Z079 totals: budget_failures=%llu (expected %llu) "
           "alloc_failures=%llu mapped_end=%zu mapped0=%zu\n",
           (unsigned long long)s.budget_failures,
           (unsigned long long)(bf0 + 8),
           (unsigned long long)s.alloc_failures,
           mimir_core_mapped_bytes(), m0);

    printf("CHECKS: %d run, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
