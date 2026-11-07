/*
 * test_j002.c -- evidence test for slice J-002 (Phase 10
 * statistics counters implementation; slice record:
 * docs/PHASE_10_STATISTICS.md; roadmap Phase 10; quantity
 * vocabulary of docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md
 * (I-002) Sections 3.2 and 4.1).
 *
 * Run standalone via `make test-j002` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-i003..test-i008 pattern).
 *
 * The binary links the library OBJECTS (LIBOBJS) plus the
 * I-004 fault-injection shim with --wrap on the four L6
 * platform entries (the I-004 form, reused unchanged), so
 * genuine backing failures can be injected for the
 * alloc_failures leg. It reads the J-002 snapshot
 * (tests/core/mimir_stats_j002.h), the I-003 trim surface,
 * the H-001 drain, the G-003 span/object counters and the
 * E-005 mapping counters -- all INC-E001-07 shape (b).
 *
 * Method: the test keeps its OWN expected accumulators
 * (requested / allocated / mapped / released / peak /
 * failures / posture NULLs / cross-thread frees), updated
 * by script arithmetic from the page size discovered at
 * runtime -- never copied from a snapshot -- and asserts
 * the locked snapshot equals them at every quiescent
 * point. Definitions were written before the run (slice
 * doc S2): a small publication grants its class size, a
 * large publication grants usable_size, staying/shrink
 * realloc publishes nothing, posture NULLs (U-09/U-12)
 * are counted separately from genuine failures
 * (Q-FAILED-OS), and cross_thread_frees is PARTIAL
 * (mapping path only, INC-J002-01).
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */

#define _POSIX_C_SOURCE 200809L

#include "mimir_alloc.h"
#include "mimir_stats_j002.h"
#include "mimir_trim_i003.h"
#include "mimir_drain_h001.h"
#include "mimir_small_g003.h"
#include "mimir_fault_i004.h"
#include "../abi/mimir_audit_e005.h"

#include <pthread.h>
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

static void check_u64(uint64_t got, uint64_t want,
                      const char *label)
{
    checks++;
    printf("  [%s] %s (got %llu, want %llu)\n",
           got == want ? "ok" : "FAIL", label,
           (unsigned long long)got,
           (unsigned long long)want);
    if (got != want) {
        failures++;
    }
}

static void fill_pattern(void *ptr, size_t len,
                         unsigned char seed)
{
    unsigned char *p = (unsigned char *)ptr;
    size_t i;
    for (i = 0; i < len; i++) {
        p[i] = (unsigned char)(seed + (unsigned char)i);
    }
}

static int pattern_intact(const void *ptr, size_t len,
                          unsigned char seed)
{
    const unsigned char *p = (const unsigned char *)ptr;
    size_t i;
    for (i = 0; i < len; i++) {
        if (p[i] != (unsigned char)(seed + (unsigned char)i)) {
            return 0;
        }
    }
    return 1;
}

static uint64_t P; /* system page size, discovered */
static const size_t CLASS_SIZES[20] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};

static uint64_t roundup_page(uint64_t n)
{
    return ((n + P - 1) / P) * P;
}
static uint64_t large_len(size_t req) /* mapping_length */
{
    return roundup_page(P + (uint64_t)req);
}
static uint64_t large_usable(size_t req)
{
    return large_len(req) - P;
}
static uint64_t span_capacity(size_t class_size)
{
    return (P - 96) / (uint64_t)class_size;
}

/* The test's own expected state, script arithmetic only. */
static uint64_t exp_req, exp_alloc, exp_mapped, exp_released;
static uint64_t exp_peak, exp_fail, exp_posture, exp_xtf;
static uint64_t exp_large_published_bytes;

static struct mimir_stats snap_now(const char *label)
{
    struct mimir_stats s;
    int rc = mimir_core_stats_snapshot(&s);
    checks++;
    if (rc != 0) {
        printf("  [FAIL] snapshot rc (%s, rc %d)\n", label, rc);
        failures++;
        memset(&s, 0, sizeof s);
    }
    return s;
}

/* Compare the snapshot against every expected accumulator. */
static void expect_all(const char *label)
{
    struct mimir_stats s = snap_now(label);
    char buf[128];
    snprintf(buf, sizeof buf, "%s: bytes_requested", label);
    check_u64(s.bytes_requested, exp_req, buf);
    snprintf(buf, sizeof buf, "%s: bytes_allocated", label);
    check_u64(s.bytes_allocated, exp_alloc, buf);
    snprintf(buf, sizeof buf, "%s: bytes_mapped", label);
    check_u64(s.bytes_mapped, exp_mapped, buf);
    snprintf(buf, sizeof buf, "%s: bytes_released", label);
    check_u64(s.bytes_released, exp_released, buf);
    snprintf(buf, sizeof buf, "%s: peak_mapped", label);
    check_u64(s.peak_mapped, exp_peak, buf);
    snprintf(buf, sizeof buf, "%s: alloc_failures", label);
    check_u64(s.alloc_failures, exp_fail, buf);
    snprintf(buf, sizeof buf, "%s: alloc_posture_nulls", label);
    check_u64(s.alloc_posture_nulls, exp_posture, buf);
    snprintf(buf, sizeof buf, "%s: cross_thread_frees", label);
    check_u64(s.cross_thread_frees, exp_xtf, buf);
    snprintf(buf, sizeof buf, "%s: mapped == I-003 Q-MAPPED", label);
    check_u64(s.bytes_mapped,
              (uint64_t)mimir_core_mapped_bytes(), buf);
    snprintf(buf, sizeof buf, "%s: cached == I-003 Q-RETAINED", label);
    check_u64(s.bytes_cached,
              (uint64_t)mimir_core_retained_bytes(), buf);
    snprintf(buf, sizeof buf, "%s: lock_contention ABSENT", label);
    check(s.lock_contention == 0 &&
          s.lock_contention_measured == 0, buf);
}

/* Script helpers: each updates the expected accumulators
 * exactly as the slice doc definitions prescribe. */
static void script_large_publication(size_t req)
{
    exp_req += req;
    exp_alloc += large_usable(req);
    exp_mapped += large_len(req);
    exp_large_published_bytes += large_len(req);
    if (exp_mapped > exp_peak) {
        exp_peak = exp_mapped;
    }
}
static void script_large_free(size_t req)
{
    exp_released += large_len(req);
    exp_mapped -= large_len(req);
}
static void script_drain(void)
{
    size_t n = mimir_core_drain_retained();
    exp_released += (uint64_t)n * P;
    exp_mapped -= (uint64_t)n * P;
}

/* Cross-thread leg plumbing. */
static void *g_xt_free_ptr;
static void *g_xt_alloc_ptr;
static void *worker_free(void *arg)
{
    (void)arg;
    mimir_free(g_xt_free_ptr);
    return NULL;
}
static void *worker_alloc(void *arg)
{
    (void)arg;
    g_xt_alloc_ptr = mimir_malloc(5000);
    return NULL;
}
static unsigned long g_churn_large_ok;
static unsigned long g_churn_small_ok;
static void *worker_churn(void *arg)
{
    int i;
    (void)arg;
    for (i = 0; i < 200; i++) {
        void *a = mimir_malloc(2048);
        void *b = mimir_malloc(32);
        if (a != NULL) {
            g_churn_large_ok++;
        }
        if (b != NULL) {
            g_churn_small_ok++;
        }
        if (a != NULL && b != NULL) {
            ((unsigned char *)a)[0] = 0xA5;
            ((unsigned char *)b)[0] = 0x5A;
        }
        mimir_free(b);
        mimir_free(a);
    }
    return NULL;
}

int main(void)
{
    long ps = sysconf(_SC_PAGESIZE);
    struct mimir_stats s;
    size_t i;

    printf("test_j002: Phase 10 statistics counters (J-002)\n");
    check(ps > 0, "page size discovered via sysconf");
    P = (uint64_t)ps;
    printf("  page size = %llu bytes\n", (unsigned long long)P);
    check(mimir_core_stats_snapshot(NULL) == -1,
          "snapshot(NULL) returns -1");

    printf("Leg 0: baseline is exactly zero\n");
    s = snap_now("leg0");
    check_u64(s.bytes_requested, 0, "leg0 requested");
    check_u64(s.bytes_allocated, 0, "leg0 allocated");
    check_u64(s.bytes_mapped, 0, "leg0 mapped");
    check_u64(s.bytes_released, 0, "leg0 released");
    check_u64(s.peak_mapped, 0, "leg0 peak");
    check_u64(s.page_size_bytes, P, "leg0 snapshot page size");
    {
        int table_ok = 1;
        for (i = 0; i < 20; i++) {
            if (s.span_class_size[i] !=
                (uint64_t)CLASS_SIZES[i]) {
                table_ok = 0;
            }
        }
        check(table_ok,
              "leg0 snapshot class table == banked G-002 table");
    }
    expect_all("leg0");

    printf("Leg A: small + large publications, trim, frees\n");
    {
        void *small[10];
        void *large;
        int distinct = 1;
        for (i = 0; i < 10; i++) {
            small[i] = mimir_malloc(100);
            check(small[i] != NULL, "legA small alloc non-NULL");
            fill_pattern(small[i], 100, (unsigned char)(0x10 + i));
            for (size_t j = 0; j < i; j++) {
                if (small[i] == small[j]) {
                    distinct = 0;
                }
            }
        }
        check(distinct, "legA small blocks pairwise distinct");
        exp_req += 1000;
        exp_alloc += 10 * 112;
        exp_mapped += P; /* one span of class 112 */
        if (exp_mapped > exp_peak) {
            exp_peak = exp_mapped;
        }
        s = snap_now("legA1");
        check_u64(s.live_small_objects, 10, "legA live small");
        check_u64(s.span_live_objects[6], 10,
                  "legA class 112 live objects");
        check_u64(s.span_total_spans[6], 1, "legA class 112 spans");
        check_u64(s.span_empty_spans[6], 0, "legA class 112 empty");
        check_u64(s.span_capacity_objects[6], span_capacity(112),
                  "legA class 112 capacity");
        check_u64(s.span_class_size[6], 112, "legA class size table");
        check_u64(s.bytes_cached, 0, "legA cached 0 while live");
        expect_all("legA1");

        large = mimir_malloc(2000);
        check(large != NULL, "legA large alloc non-NULL");
        fill_pattern(large, 2000, 0x77);
        script_large_publication(2000);
        s = snap_now("legA2");
        check_u64(s.live_mappings, 1, "legA live mappings");
        check_u64(s.live_mapping_bytes, large_len(2000),
                  "legA live mapping bytes");
        expect_all("legA2");

        for (i = 0; i < 10; i++) {
            check(pattern_intact(small[i], 100,
                                 (unsigned char)(0x10 + i)),
                  "legA small pattern intact before free");
            mimir_free(small[i]);
        }
        s = snap_now("legA3");
        check_u64(s.live_small_objects, 0, "legA small all freed");
        check_u64(s.span_empty_spans[6], 1,
                  "legA span retained empty");
        check_u64(s.bytes_cached, P, "legA cached = one page");
        expect_all("legA3");

        check(mimir_core_trim_retained() == 1,
              "legA trim releases exactly the retained span");
        exp_released += P;
        exp_mapped -= P;
        s = snap_now("legA4");
        check_u64(s.bytes_cached, 0, "legA cached 0 after trim");
        expect_all("legA4");

        check(pattern_intact(large, 2000, 0x77),
              "legA large pattern intact before free");
        mimir_free(large);
        script_large_free(2000);
        expect_all("legA5");
    }

    printf("Leg B: calloc, staying realloc, grow-by-move\n");
    {
        void *c = mimir_calloc(4, 50); /* product 200, class 224 */
        void *c2;
        int zeroed = 1;
        check(c != NULL, "legB calloc non-NULL");
        for (i = 0; i < 200; i++) {
            if (((unsigned char *)c)[i] != 0) {
                zeroed = 0;
            }
        }
        check(zeroed, "legB calloc extent zeroed");
        exp_req += 200;
        exp_alloc += 224;
        exp_mapped += P;
        if (exp_mapped > exp_peak) {
            exp_peak = exp_mapped;
        }
        expect_all("legB1");

        fill_pattern(c, 200, 0x42);
        c2 = mimir_realloc(c, 100); /* staying: within class 224 */
        check(c2 == c, "legB staying realloc returns same pointer");
        check(pattern_intact(c, 200, 0x42),
              "legB staying realloc preserves bytes");
        expect_all("legB2 staying realloc moves no counter");

        c = mimir_realloc(c, 3000); /* grow-by-move to mapping */
        check(c != NULL && c != c2, "legB grow realloc moves");
        check(pattern_intact(c, 200, 0x42),
              "legB grow preserves the requested prefix");
        exp_req += 3000;
        exp_alloc += large_usable(3000);
        exp_large_published_bytes += large_len(3000);
        exp_mapped += large_len(3000);
        if (exp_mapped > exp_peak) {
            exp_peak = exp_mapped;
        }
        /* the old small object was retired by the transfer:
         * its span is now EMPTY and retained, still mapped */
        s = snap_now("legB3");
        check_u64(s.live_small_objects, 0, "legB old object retired");
        check_u64(s.bytes_cached, P, "legB old span retained empty");
        expect_all("legB3");

        mimir_free(c);
        script_large_free(3000);
        script_drain();
        s = snap_now("legB4");
        check_u64(s.bytes_mapped, 0, "legB quiesced to zero mapped");
        expect_all("legB4");
    }

    printf("Leg C: genuine failures vs posture NULLs (I-004 shim)\n");
    {
        void *r = mimir_malloc(5000);
        void *t;
        unsigned long acq_before;
        check(r != NULL, "legC anchor large alloc non-NULL");
        fill_pattern(r, 5000, 0x99);
        script_large_publication(5000);
        expect_all("legC0");

        /* C1: injected mapping failure on a large request */
        mimir_fault_arm_acquire(1);
        t = mimir_malloc(6000);
        check(t == NULL, "legC injected acquire failure is NULL");
        check_u64(mimir_fault_acquire_injected, 1,
                  "legC exactly one injected acquire failure");
        exp_fail += 1;
        expect_all("legC1 failure counted, nothing else moved");
        t = mimir_malloc(6000); /* recovery */
        check(t != NULL, "legC recovery alloc succeeds");
        script_large_publication(6000);
        mimir_free(t);
        script_large_free(6000);
        expect_all("legC1 recovery reconciles");

        /* C2: injected page-size failure reaches no mapping */
        acq_before = mimir_fault_acquire_attempts;
        mimir_fault_arm_pagesize(1);
        t = mimir_malloc(6000);
        check(t == NULL, "legC page-size failure is NULL");
        check_u64(mimir_fault_acquire_attempts, acq_before,
                  "legC page-size failure attempted no mapping");
        exp_fail += 1;
        expect_all("legC2");

        /* C3: injected lock failure */
        mimir_fault_arm_lock(1);
        t = mimir_malloc(6000);
        check(t == NULL, "legC lock failure is NULL");
        exp_fail += 1;
        expect_all("legC3");

        /* C4: posture NULLs are NOT failures */
        check(mimir_malloc(0) == NULL, "legC malloc(0) posture NULL");
        exp_posture += 1;
        check(mimir_calloc(0, 8) == NULL,
              "legC calloc(0,8) posture NULL");
        exp_posture += 1;
        check(mimir_calloc(3, 0) == NULL,
              "legC calloc(3,0) posture NULL");
        exp_posture += 1;
        check(mimir_realloc(r, 0) == NULL,
              "legC realloc(ptr,0) posture NULL");
        exp_posture += 1;
        check(pattern_intact(r, 5000, 0x99),
              "legC posture realloc leaves original intact");
        expect_all("legC4 postures counted apart from failures");

        /* C5: pre-admission arithmetic rejections are genuine
         * Q-FAILED-OS failures, with no mapping attempted */
        check(mimir_calloc(SIZE_MAX / 2 + 1, 2) == NULL,
              "legC calloc overflow is NULL");
        exp_fail += 1;
        acq_before = mimir_fault_acquire_attempts;
        check(mimir_malloc(SIZE_MAX) == NULL,
              "legC oversize malloc is NULL");
        check_u64(mimir_fault_acquire_attempts, acq_before,
                  "legC oversize attempted no mapping");
        exp_fail += 1;
        expect_all("legC5");

        mimir_free(r);
        script_large_free(5000);
        script_drain();
        expect_all("legC6 quiesced");
    }

    printf("Leg D: cross-thread frees (PARTIAL: mapping path)\n");
    {
        pthread_t th;
        void *p;

        /* D1: allocated on main, freed by a worker -> +1 */
        p = mimir_malloc(5000);
        check(p != NULL, "legD1 alloc non-NULL");
        script_large_publication(5000);
        g_xt_free_ptr = p;
        check(pthread_create(&th, NULL, worker_free, NULL) == 0,
              "legD1 worker created");
        check(pthread_join(th, NULL) == 0, "legD1 worker joined");
        exp_xtf += 1;
        /* the worker's free already released the mapping;
         * script the release exactly once */
        script_large_free(5000);
        expect_all("legD1 cross-thread free counted once");

        /* D2: same-thread alloc + free -> +0 */
        p = mimir_malloc(5000);
        check(p != NULL, "legD2 alloc non-NULL");
        script_large_publication(5000);
        mimir_free(p);
        script_large_free(5000);
        expect_all("legD2 same-thread free not counted");

        /* D3: small object freed cross-thread -> +0 (PARTIAL,
         * INC-J002-01: no per-object owner record exists) */
        p = mimir_malloc(64);
        check(p != NULL, "legD3 small alloc non-NULL");
        exp_req += 64;
        exp_alloc += 64;
        exp_mapped += P;
        if (exp_mapped > exp_peak) {
            exp_peak = exp_mapped;
        }
        g_xt_free_ptr = p;
        check(pthread_create(&th, NULL, worker_free, NULL) == 0,
              "legD3 worker created");
        check(pthread_join(th, NULL) == 0, "legD3 worker joined");
        s = snap_now("legD3");
        check_u64(s.bytes_cached, P,
                  "legD3 small span retained empty after free");
        expect_all("legD3 small cross-thread free uncounted");
        script_drain();
        expect_all("legD3 drained");

        /* D4: allocated by a worker, freed on main -> +1 */
        g_xt_alloc_ptr = NULL;
        check(pthread_create(&th, NULL, worker_alloc, NULL) == 0,
              "legD4 worker created");
        check(pthread_join(th, NULL) == 0, "legD4 worker joined");
        check(g_xt_alloc_ptr != NULL, "legD4 worker alloc non-NULL");
        script_large_publication(5000);
        mimir_free(g_xt_alloc_ptr);
        exp_xtf += 1;
        script_large_free(5000);
        expect_all("legD4 worker-alloc main-free counted");
    }

    printf("Leg E: span occupancy across classes\n");
    {
        void *a[3], *b[2], *c[5];
        for (i = 0; i < 3; i++) {
            a[i] = mimir_malloc(16);
            check(a[i] != NULL, "legE class 16 alloc");
        }
        for (i = 0; i < 2; i++) {
            b[i] = mimir_malloc(1024);
            check(b[i] != NULL, "legE class 1024 alloc");
        }
        for (i = 0; i < 5; i++) {
            c[i] = mimir_malloc(256);
            check(c[i] != NULL, "legE class 256 alloc");
        }
        exp_req += 3 * 16 + 2 * 1024 + 5 * 256;
        exp_alloc += 3 * 16 + 2 * 1024 + 5 * 256;
        exp_mapped += 3 * P;
        if (exp_mapped > exp_peak) {
            exp_peak = exp_mapped;
        }
        s = snap_now("legE1");
        check_u64(s.span_live_objects[0], 3, "legE class 16 live");
        check_u64(s.span_live_objects[19], 2, "legE class 1024 live");
        check_u64(s.span_live_objects[11], 5, "legE class 256 live");
        check_u64(s.span_total_spans[0], 1, "legE class 16 spans");
        check_u64(s.span_total_spans[19], 1, "legE class 1024 spans");
        check_u64(s.span_total_spans[11], 1, "legE class 256 spans");
        check_u64(s.span_capacity_objects[0], span_capacity(16),
                  "legE class 16 capacity");
        check_u64(s.span_capacity_objects[19], span_capacity(1024),
                  "legE class 1024 capacity");
        check_u64(s.live_small_objects, 10, "legE live small total");
        check_u64(s.live_small_objects,
                  (uint64_t)(mimir_audit_small_alloc -
                             mimir_audit_small_free),
                  "legE occupancy == G-003 alloc-free identity");
        expect_all("legE1");

        for (i = 0; i < 3; i++) {
            mimir_free(a[i]);
        }
        for (i = 0; i < 5; i++) {
            mimir_free(c[i]);
        }
        s = snap_now("legE2");
        check_u64(s.bytes_cached, 2 * P,
                  "legE two emptied classes retained");
        check_u64(s.span_empty_spans[0], 1, "legE class 16 empty");
        check_u64(s.span_empty_spans[11], 1, "legE class 256 empty");
        check_u64(s.span_empty_spans[19], 0, "legE class 1024 live");
        for (i = 0; i < 2; i++) {
            mimir_free(b[i]);
        }
        script_drain();
        s = snap_now("legE3");
        check_u64(s.bytes_mapped, 0, "legE drained to zero");
        check_u64(s.live_small_objects, 0, "legE no live small");
        expect_all("legE3");
    }

    printf("Leg F: snapshot invariants under concurrent churn\n");
    {
        pthread_t th;
        int bad_mapped = 0, bad_cached = 0, bad_peak = 0;
        int bad_occ = 0, bad_spans = 0, bad_lock = 0;
        check(pthread_create(&th, NULL, worker_churn, NULL) == 0,
              "legF churn worker created");
        for (i = 0; i < 100; i++) {
            uint64_t occ = 0, spans = 0;
            size_t k;
            s = snap_now("legF");
            if (s.bytes_mapped !=
                s.live_mapping_bytes + s.live_span_bytes) {
                bad_mapped++;
            }
            if (s.bytes_cached > s.live_span_bytes) {
                bad_cached++;
            }
            if (s.peak_mapped < s.bytes_mapped) {
                bad_peak++;
            }
            for (k = 0; k < 20; k++) {
                occ += s.span_live_objects[k];
                spans += s.span_total_spans[k];
            }
            if (occ != s.live_small_objects) {
                bad_occ++;
            }
            if (spans != s.live_spans) {
                bad_spans++;
            }
            if (s.lock_contention != 0 ||
                s.lock_contention_measured != 0) {
                bad_lock++;
            }
        }
        check(pthread_join(th, NULL) == 0, "legF churn worker joined");
        check_u64(g_churn_large_ok, 200,
                  "legF all 200 churn large allocs succeeded");
        check_u64(g_churn_small_ok, 200,
                  "legF all 200 churn small allocs succeeded");
        /* Script the churn exactly: 200 large publications
         * and frees, 200 small publications (one span,
         * reused), peak unchanged at the scripted 3 pages. */
        exp_req += 200 * (2048 + 32);
        exp_alloc += 200 * (large_usable(2048) + 32);
        exp_large_published_bytes += 200 * large_len(2048);
        exp_released += 200 * large_len(2048);
        exp_mapped += P; /* the one churn span (class 32),
                            acquired once, still mapped here;
                            the drain below releases it */
        check(bad_mapped == 0,
              "legF invariant: mapped == mapping+span bytes (100 snaps)");
        check(bad_cached == 0,
              "legF invariant: cached <= span bytes (100 snaps)");
        check(bad_peak == 0,
              "legF invariant: peak >= mapped (100 snaps)");
        check(bad_occ == 0,
              "legF invariant: occupancy sums to live small (100 snaps)");
        check(bad_spans == 0,
              "legF invariant: class spans sum to live spans (100 snaps)");
        check(bad_lock == 0,
              "legF invariant: contention ABSENT in every snapshot");
        script_drain();
    }

    printf("Final reconciliation\n");
    s = snap_now("final");
    check_u64(s.bytes_mapped, 0, "final mapped 0");
    check_u64(s.bytes_cached, 0, "final cached 0");
    check_u64(s.live_mappings, 0, "final live mappings 0");
    check_u64(s.live_spans, 0, "final live spans 0");
    check_u64((uint64_t)mimir_audit_alloc_published,
              (uint64_t)mimir_audit_mapping_retired,
              "final E-005 published == retired");
    check_u64((uint64_t)mimir_audit_span_published,
              (uint64_t)mimir_audit_span_retired,
              "final G-003 spans published == retired");
    check_u64((uint64_t)mimir_audit_small_alloc,
              (uint64_t)mimir_audit_small_free,
              "final G-003 small alloc == free");
    /* Q-RELEASED identity handed to J-003: every published
     * byte was either still mapped or released; nothing is
     * still mapped, and no failed-publication release
     * occurred in this run (injected failures were acquire
     * failures), so released == all published bytes. */
    check_u64(s.bytes_released,
              exp_large_published_bytes +
                  (uint64_t)mimir_audit_span_published * P,
              "final released == all published bytes identity");
    check_u64(s.peak_mapped, 4 * P,
              "final peak is the scripted 4 pages (Leg C: anchor + recovery live together)");
    expect_all("final");

    printf("checks: %d, failures: %d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
