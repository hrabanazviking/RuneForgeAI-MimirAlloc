/* test_z076.c - Z-076 public statistics test (static link).
 *
 * Links the library OBJECTS (LIBOBJS) like the J-002 proof,
 * so it can cross-read the public surface
 * (mimir_stats_snapshot / struct mimir_stats_v1, include/
 * mimir_alloc.h) against the internal snapshot
 * (mimir_core_stats_snapshot, tests/core/mimir_stats_j002.h)
 * at the same quiescent points, read the L1 audit counter
 * (tests/abi/mimir_audit_e005.h), and drive the L6 lock
 * directly for the deterministic contention leg (the
 * platform entry points are re-declared below, test-only,
 * in the I-003 re-declaration pattern; the .so binds them
 * local, which the dynamic legs prove separately).
 *
 * Legs: L0 posture/version; L1 public==internal agreement;
 * L2 audit counter; L3 same-thread small free (+0); L4
 * cross-thread small free (+1, INC-J002-01 closure); L5
 * cross-thread large free (+1 total, +0 small); L6
 * same-thread large free (+0); L7 deterministic lock
 * contention (+1 exactly, INC-J002-02 closure); L8 churn
 * contention (>0); L9 unrecorded-residual invariants (no
 * positive leg exists in a production build: classifying
 * a free needs its Z-029 owner record, and exhausting the
 * 65536-slot / 256-probe table is not a unit-test act --
 * the residual is bounded by the monotone counter and the
 * bracket invariant asserted here); L10 exact small byte
 * accounting through the public surface.
 *
 * Standalone: built and run via `make test-z076`; NOT
 * part of `make test` (standing gate counts untouched).
 */
#include "mimir_alloc.h"
#include "mimir_audit_e005.h"
#include "../core/mimir_stats_j002.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Test-only re-declarations (I-003 pattern): L6 lock. */
int mimir_platform_global_lock(void);
int mimir_platform_global_unlock(void);
uint64_t mimir_platform_lock_contention(void);

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

static struct mimir_stats_v1 snap(void)
{
    struct mimir_stats_v1 v;
    memset(&v, 0, sizeof v);
    if (mimir_stats_snapshot(&v) != 0) {
        printf("  [FAIL] mimir_stats_snapshot returned -1\n");
        checks_run++;
        checks_failed++;
    }
    return v;
}

static void *g_ptr;
static void *free_in_worker(void *arg)
{
    (void)arg;
    mimir_free(g_ptr);
    return NULL;
}

static volatile int g_attempted;
static void *lock_in_worker(void *arg)
{
    (void)arg;
    g_attempted = 1;
    (void)mimir_platform_global_lock();
    (void)mimir_platform_global_unlock();
    return NULL;
}

static void *churn(void *arg)
{
    long i;
    (void)arg;
    for (i = 0; i < 2000; i++) {
        void *p = mimir_malloc(5000); /* mapping path: long holds */
        if (p != NULL) {
            memset(p, 1, 16);
            mimir_free(p);
        }
    }
    return NULL;
}

int main(void)
{
    printf("Z-076 public statistics test (static link)\n");

    /* L0: posture, version pair, priced-gap markers. */
    CHECK(mimir_stats_snapshot(NULL) == -1);
    {
        struct mimir_stats_v1 v = snap();
        CHECK(v.struct_version == MIMIR_STATS_VERSION);
        CHECK(v.struct_version == 1);
        CHECK(v.struct_size == sizeof(struct mimir_stats_v1));
        CHECK(v.lock_contention_measured == 1);
        CHECK(v.live_requested_measured == 0);
        CHECK(v.live_requested_bytes == 0);
        CHECK(v.page_size_bytes == 4096);
    }

    /* L1: public == internal at a quiescent point. */
    {
        struct mimir_stats in;
        struct mimir_stats_v1 v = snap();
        size_t i;
        CHECK(mimir_core_stats_snapshot(&in) == 0);
        CHECK(v.bytes_requested == in.bytes_requested);
        CHECK(v.bytes_allocated == in.bytes_allocated);
        CHECK(v.bytes_cached == in.bytes_cached);
        CHECK(v.bytes_mapped == in.bytes_mapped);
        CHECK(v.bytes_released == in.bytes_released);
        CHECK(v.peak_mapped == in.peak_mapped);
        CHECK(v.alloc_failures == in.alloc_failures);
        CHECK(v.alloc_posture_nulls == in.alloc_posture_nulls);
        CHECK(v.cross_thread_frees == in.cross_thread_frees);
        CHECK(v.lock_contention == in.lock_contention);
        CHECK(v.lock_contention_measured ==
              in.lock_contention_measured);
        CHECK(v.live_mappings == in.live_mappings);
        CHECK(v.live_spans == in.live_spans);
        CHECK(v.live_small_objects == in.live_small_objects);
        CHECK(v.live_mapping_bytes == in.live_mapping_bytes);
        CHECK(v.live_span_bytes == in.live_span_bytes);
        CHECK(v.page_size_bytes == in.page_size_bytes);
        CHECK(v.unlock_failures == in.unlock_failures);
        CHECK(v.drain_lock_failures == in.drain_lock_failures);
        CHECK(v.trim_lock_failures == in.trim_lock_failures);
        CHECK(v.free_lock_failures == in.free_lock_failures);
        CHECK(v.metadata_detections == in.metadata_detections);
        CHECK(v.metadata_collateral_refusals ==
              in.metadata_collateral_refusals);
        CHECK(v.live_release_detections ==
              in.live_release_detections);
        CHECK(v.live_release_collateral_refusals ==
              in.live_release_collateral_refusals);
        CHECK(v.double_free_detections ==
              in.double_free_detections);
        CHECK(v.poison_detections == in.poison_detections);
        CHECK(v.cross_thread_small_frees ==
              in.cross_thread_small_frees);
        CHECK(v.small_frees_unrecorded ==
              in.small_frees_unrecorded);
        CHECK(v.live_requested_bytes == in.live_requested_bytes);
        CHECK(v.live_requested_measured ==
              in.live_requested_measured);
        for (i = 0; i < MIMIR_PUBLIC_STATS_CLASS_COUNT; i++) {
            CHECK(v.span_class_size[i] == in.span_class_size[i]);
            CHECK(v.span_live_objects[i] ==
                  in.span_live_objects[i]);
            CHECK(v.span_total_spans[i] == in.span_total_spans[i]);
            CHECK(v.span_empty_spans[i] == in.span_empty_spans[i]);
            CHECK(v.span_capacity_objects[i] ==
                  in.span_capacity_objects[i]);
        }
        CHECK(v.span_class_size[0] == 16);
        CHECK(v.span_class_size[19] == 1024);
    }

    /* L2: the L1 audit counter moves once per public call. */
    {
        unsigned long before = mimir_audit_call_stats_snapshot;
        (void)snap();
        (void)snap();
        (void)snap();
        CHECK(mimir_audit_call_stats_snapshot - before == 3);
    }

    /* L3: same-thread small free counts nothing. */
    {
        struct mimir_stats_v1 a = snap();
        void *p = mimir_malloc(32);
        struct mimir_stats_v1 b;
        CHECK(p != NULL);
        mimir_free(p);
        b = snap();
        CHECK(b.cross_thread_small_frees ==
              a.cross_thread_small_frees);
        CHECK(b.cross_thread_frees == a.cross_thread_frees);
    }

    /* L4: cross-thread SMALL free counts exactly once. */
    {
        struct mimir_stats_v1 a = snap();
        struct mimir_stats_v1 b;
        pthread_t th;
        g_ptr = mimir_malloc(32);
        CHECK(g_ptr != NULL);
        CHECK(pthread_create(&th, NULL, free_in_worker, NULL) == 0);
        CHECK(pthread_join(th, NULL) == 0);
        b = snap();
        CHECK(b.cross_thread_small_frees ==
              a.cross_thread_small_frees + 1);
        CHECK(b.cross_thread_frees == a.cross_thread_frees + 1);
        CHECK(b.small_frees_unrecorded == a.small_frees_unrecorded);
    }

    /* L5: cross-thread LARGE free: total +1, small +0. */
    {
        struct mimir_stats_v1 a = snap();
        struct mimir_stats_v1 b;
        pthread_t th;
        g_ptr = mimir_malloc(100000);
        CHECK(g_ptr != NULL);
        CHECK(pthread_create(&th, NULL, free_in_worker, NULL) == 0);
        CHECK(pthread_join(th, NULL) == 0);
        b = snap();
        CHECK(b.cross_thread_frees == a.cross_thread_frees + 1);
        CHECK(b.cross_thread_small_frees ==
              a.cross_thread_small_frees);
    }

    /* L6: same-thread large free counts nothing. */
    {
        struct mimir_stats_v1 a = snap();
        void *p = mimir_malloc(100000);
        struct mimir_stats_v1 b;
        CHECK(p != NULL);
        mimir_free(p);
        b = snap();
        CHECK(b.cross_thread_frees == a.cross_thread_frees);
    }

    /* L7: deterministic contention: main holds the L6 lock,
     * the worker's acquisition finds it held -- exactly +1. */
    {
        uint64_t c0 = mimir_platform_lock_contention();
        struct mimir_stats_v1 a = snap();
        pthread_t th;
        struct timespec ts = { 0, 100000000L };
        CHECK(a.lock_contention == c0);
        CHECK(mimir_platform_global_lock() == 0);
        g_attempted = 0;
        CHECK(pthread_create(&th, NULL, lock_in_worker, NULL) == 0);
        while (!g_attempted) {
            nanosleep(&ts, NULL);
            ts.tv_nsec = 1000000L;
        }
        nanosleep(&(struct timespec){ 0, 100000000L }, NULL);
        CHECK(mimir_platform_global_unlock() == 0);
        CHECK(pthread_join(th, NULL) == 0);
        {
            struct mimir_stats_v1 b = snap();
            CHECK(mimir_platform_lock_contention() == c0 + 1);
            CHECK(b.lock_contention == a.lock_contention + 1);
        }
    }

    /* L8: churn contention through the public surface. */
    {
        struct mimir_stats_v1 a = snap();
        struct mimir_stats_v1 b;
        pthread_t th[4];
        int i;
        for (i = 0; i < 4; i++) {
            CHECK(pthread_create(&th[i], NULL, churn, NULL) == 0);
        }
        for (i = 0; i < 4; i++) {
            CHECK(pthread_join(th[i], NULL) == 0);
        }
        b = snap();
        CHECK(b.lock_contention > a.lock_contention);
    }

    /* L9: residual invariants. */
    {
        struct mimir_stats_v1 v = snap();
        CHECK(v.cross_thread_small_frees <= v.cross_thread_frees);
        CHECK(mimir_core_cross_thread_small_frees() ==
              v.cross_thread_small_frees);
        CHECK(mimir_core_small_frees_unrecorded() ==
              v.small_frees_unrecorded);
    }

    /* L10: exact small byte accounting (100 -> class 112). */
    {
        struct mimir_stats_v1 a = snap();
        void *p = mimir_malloc(100);
        struct mimir_stats_v1 b;
        CHECK(p != NULL);
        b = snap();
        CHECK(b.bytes_requested == a.bytes_requested + 100);
        CHECK(b.bytes_allocated == a.bytes_allocated + 112);
        mimir_free(p);
    }

    printf("Z076 STATIC: %d checks, %d failed\n", checks_run,
           checks_failed);
    printf("RESULT: %s\n", checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
