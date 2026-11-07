/*
 * test_i004.c -- evidence test for slice I-004 (Phase 9
 * genuine allocation-failure handling; slice record:
 * docs/PHASE_9_FAILURE_HANDLING.md; contract fixed by
 * docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md Section 6.3,
 * FC-1..FC-6).
 *
 * Run standalone via `make test-i004` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-g003..test-i003 pattern).
 *
 * The binary links the library OBJECTS (LIBOBJS) plus the
 * test-only fault-injection shim
 * (tests/core/mimir_fault_shim_i004.c, declared in
 * tests/core/mimir_fault_i004.h) with the linker's --wrap
 * interposition on the four L6 platform entries, so the
 * core's mapping, release, lock and page-size calls pass
 * through the shim and the Nth one can be failed on demand
 * (roadmap Section 9.3). It also reads the E-005 mapping
 * counters (tests/abi/mimir_audit_e005.h), the G-003
 * span/object counters (tests/core/mimir_small_g003.h),
 * the G-007 retention pair (tests/core/mimir_tune_g007.h),
 * the H-001 drain and the I-003 trim queries
 * (tests/core/mimir_trim_i003.h) -- all INC-E001-07
 * shape (b). Single-threaded, so every reading is at a
 * quiescent point by construction (I-002 S5.3). It proves:
 *
 * - shim transparency: unarmed, every call forwards and
 *   only the attempt counters move (Leg 0);
 * - FC-6 separation: posture NULLs (U-09 zero-size, U-12
 *   realloc(ptr, 0)), calloc product overflow and
 *   oversize rejections return NULL with NO platform
 *   mapping attempt at all, while a representable
 *   unsatisfiable request reaches the OS and fails there
 *   as a REAL failure, counted separately (Leg 0);
 * - FC-1 / FAIL-001 sweep: failing the Nth mapping for
 *   N = 1..4 across six large mallocs fails exactly the
 *   Nth, returns exactly NULL, and the other five succeed
 *   pairwise non-overlapping (Leg 1); the same single-shot
 *   failure on the small path (fresh class per size),
 *   calloc on both paths and every realloc construction
 *   (Legs 2-4), each followed by a recovery allocation
 *   (FAIL-003);
 * - FC-2: every injected realloc failure leaves the
 *   original block LIVE and byte-identical and freeable
 *   exactly once -- grow on both paths, cross-class small
 *   grow, realloc(NULL, n), and the transfer-step release
 *   failure whose rollback branch rested on inspection
 *   since C-005 (INC-C005-03) (Legs 4-5);
 * - release failure (F-06, FAIL-051, INC-I003-03): a
 *   failed release on free of a large block rolls the
 *   retirement back in full; a failed release inside a
 *   trim leaves that span retained, is not counted, and
 *   the trim walk CONTINUES to the next class; a failed
 *   span release at a bound-0 free leaves the object freed
 *   and the span retained for reuse (Leg 5);
 * - F-07 lock failure (FAIL-061) and F-11 page-size
 *   failure (FAIL-101) through the shim's other two
 *   channels: NULL with no mapping attempted, original
 *   untouched, recovery immediate (Leg 6);
 * - FC-4 / FC-5: a 30-round injected failure storm with
 *   interleaved successes reconciles exactly -- injected
 *   counts, attempt counts and publication deltas all as
 *   scripted, budget refusals zero by construction (no
 *   budget exists, INC-I001-14) -- and after the storm
 *   every block frees, Q-MAPPED and Q-RETAINED return to
 *   0 and all three counter pairs reconcile (Leg 7);
 * - FC-3: no leg retries or waits; the whole run is a
 *   fixed script whose completion is the no-hang evidence
 *   (the recorded run also wraps the binary in timeout).
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */

#define _POSIX_C_SOURCE 200809L

#include "mimir_alloc.h"
#include "mimir_fault_i004.h"
#include "mimir_small_g003.h"
#include "mimir_trim_i003.h"
#include "mimir_tune_g007.h"
#include "mimir_drain_h001.h"
#include "../abi/mimir_audit_e005.h"

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

static void check_eq_ulong(unsigned long got, unsigned long want,
                           const char *label)
{
    checks++;
    printf("  [%s] %s (got %lu, want %lu)\n",
           got == want ? "ok" : "FAIL", label, got, want);
    if (got != want) {
        failures++;
    }
}

static void check_eq_size(size_t got, size_t want,
                          const char *label)
{
    checks++;
    printf("  [%s] %s (got %zu, want %zu)\n",
           got == want ? "ok" : "FAIL", label, got, want);
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

static int all_zero(const void *ptr, size_t len)
{
    const unsigned char *p = (const unsigned char *)ptr;
    size_t i;
    for (i = 0; i < len; i++) {
        if (p[i] != 0) {
            return 0;
        }
    }
    return 1;
}

/* Pairwise non-overlap of [ptr, ptr+len) intervals. */
static int intervals_disjoint(void *const *ptrs,
                              const size_t *lens, size_t n)
{
    size_t i;
    size_t j;
    for (i = 0; i < n; i++) {
        uintptr_t a0 = (uintptr_t)ptrs[i];
        uintptr_t a1 = a0 + lens[i];
        for (j = i + 1; j < n; j++) {
            uintptr_t b0 = (uintptr_t)ptrs[j];
            uintptr_t b1 = b0 + lens[j];
            if (a0 < b1 && b0 < a1) {
                return 0;
            }
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Leg 0: shim transparency, then FC-6 posture separation.            */
/* ------------------------------------------------------------------ */
static void leg0_transparency_and_postures(void)
{
    void *big;
    void *small;
    void *p;
    unsigned long att;
    unsigned long rel;
    unsigned long pub;
    unsigned long ret;
    unsigned long spub;
    unsigned long sret;
    unsigned long salloc;
    unsigned long sfree;
    unsigned long inj;
    unsigned long realf;

    printf("Leg 0: shim transparency + posture separation (FC-6)\n");

    big = mimir_malloc(4096);
    check(big != NULL, "transparency: large malloc succeeds unarmed");
    small = mimir_malloc(64);
    check(small != NULL, "transparency: small malloc succeeds unarmed");
    if (big != NULL) {
        fill_pattern(big, 4096, 0x11);
        check(pattern_intact(big, 4096, 0x11),
              "transparency: large pattern verifies");
    }
    if (small != NULL) {
        fill_pattern(small, 64, 0x22);
        check(pattern_intact(small, 64, 0x22),
              "transparency: small pattern verifies");
    }
    mimir_free(big);
    mimir_free(small);
    (void)mimir_core_drain_retained();
    check_eq_ulong(mimir_fault_acquire_attempts, 2,
                   "transparency: exactly 2 acquire attempts (1 mapping + 1 span)");
    check_eq_ulong(mimir_fault_release_attempts, 2,
                   "transparency: exactly 2 release attempts (mapping + drained span)");
    check_eq_ulong(mimir_fault_acquire_injected, 0,
                   "transparency: 0 injected acquire failures");
    check_eq_ulong(mimir_fault_release_injected, 0,
                   "transparency: 0 injected release failures");
    check_eq_ulong(mimir_fault_acquire_real_failed, 0,
                   "transparency: 0 real acquire failures");
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "transparency: Q-MAPPED back to 0");
    check_eq_size(mimir_core_retained_spans(), 0,
                  "transparency: Q-RETAINED back to 0 spans");

    /* Posture / pre-acquire rejections: none of these may
     * reach the platform mapping entry at all. A live block
     * is held across the realloc(ptr, 0) posture. */
    p = mimir_malloc(32);
    check(p != NULL, "posture setup: small block allocated");
    if (p != NULL) {
        fill_pattern(p, 32, 0x33);
    }
    att = mimir_fault_acquire_attempts;
    rel = mimir_fault_release_attempts;
    pub = mimir_audit_alloc_published;
    ret = mimir_audit_mapping_retired;
    spub = mimir_audit_span_published;
    sret = mimir_audit_span_retired;
    salloc = mimir_audit_small_alloc;
    sfree = mimir_audit_small_free;
    inj = mimir_fault_acquire_injected;
    realf = mimir_fault_acquire_real_failed;

    check(mimir_malloc(0) == NULL,
          "FC-6: malloc(0) returns NULL (posture, U-09)");
    check(mimir_calloc(0, 8) == NULL,
          "FC-6: calloc(0, 8) returns NULL (posture, U-09)");
    check(mimir_calloc(8, 0) == NULL,
          "FC-6: calloc(8, 0) returns NULL (posture, U-09)");
    check(mimir_calloc(SIZE_MAX, 2) == NULL,
          "FC-6: calloc(SIZE_MAX, 2) overflow returns NULL");
    check(mimir_calloc(2, SIZE_MAX) == NULL,
          "FC-6: calloc(2, SIZE_MAX) overflow returns NULL");
    check(mimir_malloc(SIZE_MAX) == NULL,
          "FC-6: malloc(SIZE_MAX) rejected with NULL");
    check(mimir_malloc(SIZE_MAX - 8) == NULL,
          "FC-6: malloc(SIZE_MAX - 8) rejected with NULL");
    check(mimir_realloc(p, 0) == NULL,
          "FC-6: realloc(ptr, 0) returns NULL (posture, U-12)");
    check(p != NULL && pattern_intact(p, 32, 0x33),
          "FC-6: realloc(ptr, 0) left the original byte-identical");
    check_eq_ulong(mimir_fault_acquire_attempts, att,
                   "FC-6: posture/overflow/oversize made NO acquire attempt");
    check_eq_ulong(mimir_fault_release_attempts, rel,
                   "FC-6: posture set made NO release attempt");
    check_eq_ulong(mimir_fault_acquire_injected, inj,
                   "FC-6: posture NULLs are not injected failures");
    check_eq_ulong(mimir_fault_acquire_real_failed, realf,
                   "FC-6: posture NULLs are not OS failures");
    check_eq_ulong(mimir_audit_alloc_published, pub,
                   "FC-6: no mapping published by posture set");
    check_eq_ulong(mimir_audit_mapping_retired, ret,
                   "FC-6: no mapping retired by posture set");
    check_eq_ulong(mimir_audit_span_published, spub,
                   "FC-6: no span published by posture set");
    check_eq_ulong(mimir_audit_span_retired, sret,
                   "FC-6: no span retired by posture set");
    check_eq_ulong(mimir_audit_small_alloc, salloc,
                   "FC-6: no small object published by posture set");
    check_eq_ulong(mimir_audit_small_free, sfree,
                   "FC-6: no small object freed by posture set");
    mimir_free(p);

    /* A representable unsatisfiable request DOES reach the
     * OS and fails there for real: the shim must count it
     * as a real failure, never as an injected one. */
    att = mimir_fault_acquire_attempts;
    inj = mimir_fault_acquire_injected;
    realf = mimir_fault_acquire_real_failed;
    check(mimir_malloc(SIZE_MAX / 2) == NULL,
          "real failure: malloc(SIZE_MAX/2) returns NULL");
    check_eq_ulong(mimir_fault_acquire_attempts, att + 1,
                   "real failure: exactly 1 acquire attempt was made");
    check_eq_ulong(mimir_fault_acquire_real_failed, realf + 1,
                   "real failure: counted as a REAL OS failure");
    check_eq_ulong(mimir_fault_acquire_injected, inj,
                   "real failure: NOT counted as injected");
    p = mimir_malloc(128);
    check(p != NULL, "real failure: recovery allocation succeeds");
    mimir_free(p);
    (void)mimir_core_drain_retained();
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "Leg 0 end: Q-MAPPED 0 at quiescent point");
}

/* ------------------------------------------------------------------ */
/* Leg 1: fail-the-Nth-mapping sweep over large malloc (FAIL-001/002).*/
/* ------------------------------------------------------------------ */
static void leg1_nth_sweep_large(void)
{
    unsigned long n;

    printf("Leg 1: Nth-mapping sweep, large malloc (FC-1, FAIL-001/002/003)\n");
    for (n = 1; n <= 4; n++) {
        void *ptrs[6];
        size_t lens[6];
        unsigned long att0 = mimir_fault_acquire_attempts;
        unsigned long inj0 = mimir_fault_acquire_injected;
        unsigned long real0 = mimir_fault_acquire_real_failed;
        unsigned long pub0 = mimir_audit_alloc_published;
        size_t i;
        size_t nulls = 0;
        size_t null_at = 0;
        int patterns_ok = 1;

        mimir_fault_arm_acquire(n);
        for (i = 0; i < 6; i++) {
            ptrs[i] = mimir_malloc(2048);
            lens[i] = 2048;
            if (ptrs[i] == NULL) {
                nulls++;
                null_at = i;
            } else {
                fill_pattern(ptrs[i], 2048,
                             (unsigned char)(0x40 + i));
            }
        }
        check_eq_size(nulls, 1, "sweep: exactly one NULL in six mallocs");
        check_eq_size(null_at, (size_t)(n - 1),
                      "sweep: the NULL is exactly the Nth allocation");
        for (i = 0; i < 6; i++) {
            if (ptrs[i] != NULL &&
                !pattern_intact(ptrs[i], 2048,
                                (unsigned char)(0x40 + i))) {
                patterns_ok = 0;
            }
        }
        check(patterns_ok,
              "sweep: survivors byte-identical (failure corrupted nothing)");
        {
            void *live[6];
            size_t ll[6];
            size_t k = 0;
            for (i = 0; i < 6; i++) {
                if (ptrs[i] != NULL) {
                    live[k] = ptrs[i];
                    ll[k] = lens[i];
                    k++;
                }
            }
            check(intervals_disjoint(live, ll, k),
                  "FC-1: no overlapping blocks ever returned");
        }
        check_eq_ulong(mimir_fault_acquire_attempts, att0 + 6,
                       "sweep: 6 acquire attempts for 6 mallocs");
        check_eq_ulong(mimir_fault_acquire_injected, inj0 + 1,
                       "sweep: exactly 1 injected failure");
        check_eq_ulong(mimir_fault_acquire_real_failed, real0,
                       "sweep: 0 real failures");
        check_eq_ulong(mimir_audit_alloc_published, pub0 + 5,
                       "sweep: exactly 5 mappings published");
        for (i = 0; i < 6; i++) {
            mimir_free(ptrs[i]);
        }
        check_eq_size(mimir_core_mapped_bytes(), 0,
                      "sweep: Q-MAPPED 0 after freeing the five");
        {
            void *rec = mimir_malloc(2048);
            check(rec != NULL,
                  "FAIL-003: recovery allocation succeeds after the sweep round");
            mimir_free(rec);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Leg 2: small-path malloc failure, one fresh class per size.        */
/* ------------------------------------------------------------------ */
static void leg2_small_path(void)
{
    static const size_t sizes[6] = {16, 48, 80, 160, 320, 640};
    size_t k;

    printf("Leg 2: small-path malloc failure, fresh classes (FC-1)\n");
    for (k = 0; k < 6; k++) {
        unsigned long att0 = mimir_fault_acquire_attempts;
        unsigned long inj0 = mimir_fault_acquire_injected;
        unsigned long spub0 = mimir_audit_span_published;
        unsigned long salloc0 = mimir_audit_small_alloc;
        void *p;
        void *q;

        mimir_fault_arm_acquire(1);
        p = mimir_malloc(sizes[k]);
        check(p == NULL, "small: injected span failure returns exactly NULL");
        check_eq_ulong(mimir_fault_acquire_attempts, att0 + 1,
                       "small: exactly 1 acquire attempt");
        check_eq_ulong(mimir_fault_acquire_injected, inj0 + 1,
                       "small: exactly 1 injected failure");
        check_eq_ulong(mimir_audit_span_published, spub0,
                       "small: no span published by the failed call");
        check_eq_ulong(mimir_audit_small_alloc, salloc0,
                       "small: no object published by the failed call");
        q = mimir_malloc(sizes[k]);
        check(q != NULL, "small: recovery allocation succeeds (span acquired)");
        if (q != NULL) {
            fill_pattern(q, sizes[k], (unsigned char)(0x50 + k));
            check(pattern_intact(q, sizes[k],
                                 (unsigned char)(0x50 + k)),
                  "small: recovery block pattern verifies");
        }
        mimir_free(q);
    }
    /* A further object of a warmed class is served from the
     * retained/live span with NO mapping attempt at all. */
    {
        unsigned long att0 = mimir_fault_acquire_attempts;
        void *a = mimir_malloc(48);
        void *b = mimir_malloc(48);
        check(a != NULL && b != NULL && a != b,
              "small: two objects of one class are distinct");
        check_eq_ulong(mimir_fault_acquire_attempts, att0,
                       "small: serving from an existing span attempts no mapping");
        mimir_free(a);
        mimir_free(b);
    }
    (void)mimir_core_trim_retained();
    check_eq_size(mimir_core_retained_spans(), 0,
                  "Leg 2 end: trim leaves Q-RETAINED 0");
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "Leg 2 end: Q-MAPPED 0 at quiescent point");
}

/* ------------------------------------------------------------------ */
/* Leg 3: calloc failure on both paths.                               */
/* ------------------------------------------------------------------ */
static void leg3_calloc(void)
{
    unsigned long inj0;
    unsigned long pub0;
    unsigned long spub0;
    void *p;

    printf("Leg 3: calloc failure, large and small (FC-1)\n");
    inj0 = mimir_fault_acquire_injected;
    pub0 = mimir_audit_alloc_published;
    mimir_fault_arm_acquire(1);
    p = mimir_calloc(64, 64);
    check(p == NULL, "calloc large: injected failure returns exactly NULL");
    check_eq_ulong(mimir_fault_acquire_injected, inj0 + 1,
                   "calloc large: exactly 1 injected failure");
    check_eq_ulong(mimir_audit_alloc_published, pub0,
                   "calloc large: nothing published by the failed call");
    p = mimir_calloc(64, 64);
    check(p != NULL, "calloc large: recovery succeeds");
    check(p != NULL && all_zero(p, 4096),
          "calloc large: recovered block is zero over the product");
    mimir_free(p);

    inj0 = mimir_fault_acquire_injected;
    spub0 = mimir_audit_span_published;
    mimir_fault_arm_acquire(1);
    p = mimir_calloc(6, 16);
    check(p == NULL, "calloc small: injected failure returns exactly NULL");
    check_eq_ulong(mimir_fault_acquire_injected, inj0 + 1,
                   "calloc small: exactly 1 injected failure");
    check_eq_ulong(mimir_audit_span_published, spub0,
                   "calloc small: no span published by the failed call");
    p = mimir_calloc(6, 16);
    check(p != NULL, "calloc small: recovery succeeds");
    check(p != NULL && all_zero(p, 96),
          "calloc small: recovered block is zero over the product");
    mimir_free(p);
    (void)mimir_core_trim_retained();
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "Leg 3 end: Q-MAPPED 0 at quiescent point");
}

/* ------------------------------------------------------------------ */
/* Leg 4: realloc failure-atomicity under acquire failure (FC-2).     */
/* ------------------------------------------------------------------ */
static void leg4_realloc_atomic(void)
{
    void *p;
    void *r;
    unsigned long inj0;
    unsigned long pub0;
    unsigned long ret0;
    unsigned long salloc0;
    unsigned long sfree0;
    unsigned long att0;

    printf("Leg 4: realloc failure-atomicity, acquire failure (FC-2)\n");

    /* (a) large -> larger grow. */
    p = mimir_malloc(4096);
    check(p != NULL, "realloc(a): setup large block allocated");
    if (p != NULL) {
        fill_pattern(p, 4096, 0x61);
    }
    inj0 = mimir_fault_acquire_injected;
    pub0 = mimir_audit_alloc_published;
    ret0 = mimir_audit_mapping_retired;
    mimir_fault_arm_acquire(1);
    r = mimir_realloc(p, 8192);
    check(r == NULL, "realloc(a): injected grow failure returns exactly NULL");
    check(p != NULL && pattern_intact(p, 4096, 0x61),
          "FC-2(a): original LIVE and byte-identical after failure");
    check_eq_ulong(mimir_fault_acquire_injected, inj0 + 1,
                   "realloc(a): exactly 1 injected failure");
    check_eq_ulong(mimir_audit_alloc_published, pub0,
                   "realloc(a): new mapping was never published");
    check_eq_ulong(mimir_audit_mapping_retired, ret0,
                   "realloc(a): original was not retired");
    mimir_free(p);
    check_eq_ulong(mimir_audit_mapping_retired, ret0 + 1,
                   "FC-2(a): original freeable exactly once afterwards");

    /* (b) small -> large grow. */
    p = mimir_malloc(224);
    check(p != NULL, "realloc(b): setup small block allocated");
    if (p != NULL) {
        fill_pattern(p, 224, 0x62);
    }
    inj0 = mimir_fault_acquire_injected;
    salloc0 = mimir_audit_small_alloc;
    sfree0 = mimir_audit_small_free;
    mimir_fault_arm_acquire(1);
    r = mimir_realloc(p, 5000);
    check(r == NULL, "realloc(b): injected grow failure returns exactly NULL");
    check(p != NULL && pattern_intact(p, 224, 0x62),
          "FC-2(b): original LIVE and byte-identical after failure");
    check_eq_ulong(mimir_fault_acquire_injected, inj0 + 1,
                   "realloc(b): exactly 1 injected failure");
    check_eq_ulong(mimir_audit_small_alloc, salloc0,
                   "realloc(b): no new small object published");
    check_eq_ulong(mimir_audit_small_free, sfree0,
                   "realloc(b): original not freed by the failed call");
    mimir_free(p);

    /* (c) small -> small cross-class grow needing a fresh span. */
    p = mimir_malloc(200);
    check(p != NULL, "realloc(c): setup block in class 256 allocated");
    if (p != NULL) {
        fill_pattern(p, 200, 0x63);
    }
    inj0 = mimir_fault_acquire_injected;
    mimir_fault_arm_acquire(1);
    r = mimir_realloc(p, 300);
    check(r == NULL, "realloc(c): injected cross-class failure returns NULL");
    check(p != NULL && pattern_intact(p, 200, 0x63),
          "FC-2(c): original LIVE and byte-identical after failure");
    check_eq_ulong(mimir_fault_acquire_injected, inj0 + 1,
                   "realloc(c): exactly 1 injected failure");
    r = mimir_realloc(p, 300);
    check(r != NULL, "realloc(c): recovery grow succeeds");
    check(r != NULL && pattern_intact(r, 200, 0x63),
          "realloc(c): recovery preserves the requested range");
    mimir_free(r);

    /* (d) realloc(NULL, n) behaves as malloc under failure. */
    mimir_fault_arm_acquire(1);
    r = mimir_realloc(NULL, 4096);
    check(r == NULL, "realloc(d): realloc(NULL, 4096) fails exactly as malloc");
    r = mimir_realloc(NULL, 4096);
    check(r != NULL, "realloc(d): recovery succeeds");
    mimir_free(r);

    /* (e) same-size and shrink never touch the mapping entry:
     * an armed failure survives both and fires on the grow. */
    p = mimir_malloc(4096);
    check(p != NULL, "realloc(e): setup large block allocated");
    if (p != NULL) {
        fill_pattern(p, 4096, 0x64);
    }
    att0 = mimir_fault_acquire_attempts;
    mimir_fault_arm_acquire(1);
    r = mimir_realloc(p, 4096);
    check(r == p, "realloc(e): same-size returns the same pointer");
    r = mimir_realloc(p, 2048);
    check(r == p, "realloc(e): shrink returns the same pointer");
    check_eq_ulong(mimir_fault_acquire_attempts, att0,
                   "realloc(e): same-size and shrink attempt no mapping");
    check(p != NULL && pattern_intact(p, 2048, 0x64),
          "realloc(e): contents intact across same-size and shrink");
    r = mimir_realloc(p, 8192);
    check(r == NULL, "realloc(e): the armed failure fires on the grow");
    check(p != NULL && pattern_intact(p, 2048, 0x64),
          "FC-2(e): original LIVE and byte-identical after grow failure");
    mimir_free(p);
    (void)mimir_core_trim_retained();
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "Leg 4 end: Q-MAPPED 0 at quiescent point");
}

/* ------------------------------------------------------------------ */
/* Leg 5: release failure -- F-06 rollback and INC-I003-03.           */
/* ------------------------------------------------------------------ */
static void leg5_release_failure(void)
{
    void *p;
    void *q;
    void *r;
    unsigned long ret0;
    unsigned long pub0;
    unsigned long sret0;
    unsigned long sfree0;
    unsigned long rinj0;
    size_t released;
    size_t i;
    void *warm[2];

    printf("Leg 5: release failure, F-06 rollback (FAIL-051, INC-I003-03)\n");

    /* (a) free of a large block whose release fails: the
     * retirement rolls back in full and the block stays
     * LIVE, byte-identical and still caller-owned (the
     * F-06 state); a second free retires it exactly once. */
    p = mimir_malloc(4096);
    check(p != NULL, "release(a): setup large block allocated");
    if (p != NULL) {
        fill_pattern(p, 4096, 0x71);
    }
    ret0 = mimir_audit_mapping_retired;
    rinj0 = mimir_fault_release_injected;
    mimir_fault_arm_release(1);
    mimir_free(p);
    check_eq_ulong(mimir_fault_release_injected, rinj0 + 1,
                   "release(a): exactly 1 injected release failure");
    check_eq_ulong(mimir_audit_mapping_retired, ret0,
                   "release(a): retirement rolled back (retired count unmoved)");
    check(p != NULL && pattern_intact(p, 4096, 0x71),
          "release(a): block still LIVE and byte-identical (never unmapped)");
    mimir_free(p);
    check_eq_ulong(mimir_audit_mapping_retired, ret0 + 1,
                   "release(a): second free retires the block exactly once");

    /* (b) trim continuation (INC-I003-03): two warmed
     * classes hold one retained span each. A PARTIAL trim
     * asked for 1 span, with the first release (lowest
     * class, class-fair order) failed by injection,
     * counts only the span it actually released, leaves
     * the failed span retained, and CONTINUES to the next
     * class inside the same call. Then the classes are
     * re-warmed and a FULL trim under the same single-shot
     * failure releases both spans: the shared walk makes
     * further passes while any class progresses, so the
     * transiently failed span is retried on the next pass
     * of the same call and released then -- the count
     * still reflects only actual releases (2). */
    for (i = 0; i < 2; i++) {
        warm[i] = mimir_malloc(384);
    }
    check(warm[0] != NULL && warm[1] != NULL,
          "release(b): class 384 warmed");
    mimir_free(warm[0]);
    mimir_free(warm[1]);
    for (i = 0; i < 2; i++) {
        warm[i] = mimir_malloc(448);
    }
    check(warm[0] != NULL && warm[1] != NULL,
          "release(b): class 448 warmed");
    mimir_free(warm[0]);
    mimir_free(warm[1]);
    check_eq_size(mimir_core_retained_spans(), 2,
                  "release(b): exactly 2 retained spans before trim");
    sret0 = mimir_audit_span_retired;
    rinj0 = mimir_fault_release_injected;
    mimir_fault_arm_release(1);
    released = mimir_core_trim_retained_some(1);
    check_eq_size(released, 1,
                  "INC-I003-03: partial trim counts only the release that succeeded");
    check_eq_ulong(mimir_fault_release_injected, rinj0 + 1,
                   "INC-I003-03: exactly 1 injected release failure in partial trim");
    check_eq_ulong(mimir_audit_span_retired, sret0 + 1,
                   "INC-I003-03: span_retired delta is exactly the counted 1");
    check_eq_size(mimir_core_retained_spans(), 1,
                  "INC-I003-03: the failed span stays retained (rolled back)");
    for (i = 0; i < 2; i++) {
        warm[i] = mimir_malloc(448);
    }
    check(warm[0] != NULL && warm[1] != NULL,
          "release(b): class 448 re-warmed");
    mimir_free(warm[0]);
    mimir_free(warm[1]);
    check_eq_size(mimir_core_retained_spans(), 2,
                  "release(b): 2 retained spans again before full trim");
    sret0 = mimir_audit_span_retired;
    rinj0 = mimir_fault_release_injected;
    mimir_fault_arm_release(1);
    released = mimir_core_trim_retained();
    check_eq_size(released, 2,
                  "INC-I003-03: full trim releases both (failed span retried next pass)");
    check_eq_ulong(mimir_fault_release_injected, rinj0 + 1,
                   "INC-I003-03: exactly 1 injected release failure in full trim");
    check_eq_ulong(mimir_audit_span_retired, sret0 + 2,
                   "INC-I003-03: span_retired delta is exactly the counted 2");
    check_eq_size(mimir_core_retained_spans(), 0,
                  "release(b): Q-RETAINED 0 after the full trim");

    /* (c) realloc transfer-step release failure (the branch
     * INC-C005-03 left to inspection): the new mapping is
     * acquired, the copy made, the OLD release fails and
     * rolls back, the new block is retired as unpublished
     * partial work, and NULL returns with the original
     * preserved. Net counter effect: published +1 and
     * retired +1 (the new block only). */
    p = mimir_malloc(4096);
    check(p != NULL, "release(c): setup large block allocated");
    if (p != NULL) {
        fill_pattern(p, 4096, 0x72);
    }
    pub0 = mimir_audit_alloc_published;
    ret0 = mimir_audit_mapping_retired;
    rinj0 = mimir_fault_release_injected;
    mimir_fault_arm_release(1);
    r = mimir_realloc(p, 8192);
    check(r == NULL, "release(c): transfer failure returns exactly NULL");
    check(p != NULL && pattern_intact(p, 4096, 0x72),
          "FC-2(f): original LIVE and byte-identical after transfer failure");
    check_eq_ulong(mimir_fault_release_injected, rinj0 + 1,
                   "release(c): exactly 1 injected release failure");
    check_eq_ulong(mimir_audit_alloc_published, pub0 + 1,
                   "release(c): the new mapping WAS published then rolled back");
    check_eq_ulong(mimir_audit_mapping_retired, ret0 + 1,
                   "release(c): only the new mapping was retired");
    mimir_free(p);
    check_eq_ulong(mimir_audit_mapping_retired, ret0 + 2,
                   "FC-2(f): original freeable exactly once afterwards");

    /* (d) small free at retention bound 0 whose span
     * release fails: the object stays freed, the span
     * stays retained and serves the next allocation with
     * no mapping attempt; the next emptying free releases
     * it for real. */
    check(mimir_tune_set_retention_bound(0) == 0,
          "release(d): retention bound set to 0");
    p = mimir_malloc(500);
    check(p != NULL, "release(d): setup block in class 512 allocated");
    if (p != NULL) {
        fill_pattern(p, 500, 0x73);
    }
    sfree0 = mimir_audit_small_free;
    sret0 = mimir_audit_span_retired;
    rinj0 = mimir_fault_release_injected;
    mimir_fault_arm_release(1);
    mimir_free(p);
    check_eq_ulong(mimir_audit_small_free, sfree0 + 1,
                   "release(d): the object itself was freed");
    check_eq_ulong(mimir_fault_release_injected, rinj0 + 1,
                   "release(d): exactly 1 injected release failure");
    check_eq_ulong(mimir_audit_span_retired, sret0,
                   "release(d): span retirement rolled back (unmoved)");
    check_eq_size(mimir_core_retained_spans(), 1,
                  "release(d): the emptied span stays retained");
    {
        unsigned long att0 = mimir_fault_acquire_attempts;
        q = mimir_malloc(500);
        check(q != NULL, "release(d): retained span serves the next allocation");
        check_eq_ulong(mimir_fault_acquire_attempts, att0,
                       "release(d): reuse attempts no mapping");
        mimir_free(q);
    }
    check_eq_ulong(mimir_audit_span_retired, sret0 + 1,
                   "release(d): next emptying free releases the span for real");
    check_eq_size(mimir_core_retained_spans(), 0,
                  "release(d): Q-RETAINED 0 afterwards");
    check(mimir_tune_set_retention_bound(2) == 0,
          "release(d): retention bound restored to the default 2");
    check_eq_size(mimir_tune_get_retention_bound(), 2,
                  "release(d): bound readback is 2");
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "Leg 5 end: Q-MAPPED 0 at quiescent point");
}

/* ------------------------------------------------------------------ */
/* Leg 6: lock failure (F-07) and page-size failure (F-11).           */
/* ------------------------------------------------------------------ */
static void leg6_lock_and_pagesize(void)
{
    void *p;
    void *q;
    unsigned long att0;
    unsigned long linj0;
    unsigned long pinj0;
    unsigned long ret0;

    printf("Leg 6: lock failure (FAIL-061) + page-size failure (FAIL-101)\n");

    p = mimir_malloc(4096);
    check(p != NULL, "lock: setup large block allocated");
    if (p != NULL) {
        fill_pattern(p, 4096, 0x75);
    }
    att0 = mimir_fault_acquire_attempts;
    linj0 = mimir_fault_lock_injected;
    mimir_fault_arm_lock(1);
    q = mimir_malloc(4096);
    check(q == NULL, "F-07: lock failure on malloc returns exactly NULL");
    check_eq_ulong(mimir_fault_lock_injected, linj0 + 1,
                   "F-07: exactly 1 injected lock failure");
    check_eq_ulong(mimir_fault_acquire_attempts, att0,
                   "F-07: no mapping attempted when the lock cannot be taken");
    ret0 = mimir_audit_mapping_retired;
    mimir_fault_arm_lock(1);
    mimir_free(p);
    check_eq_ulong(mimir_audit_mapping_retired, ret0,
                   "F-07: failed-lock free retires nothing (block stays LIVE)");
    check(p != NULL && pattern_intact(p, 4096, 0x75),
          "F-07: block byte-identical after the failed-lock free");
    mimir_free(p);
    check_eq_ulong(mimir_audit_mapping_retired, ret0 + 1,
                   "F-07: free succeeds once the lock is healthy again");
    q = mimir_malloc(2048);
    check(q != NULL, "lock: setup block for realloc allocated");
    if (q != NULL) {
        fill_pattern(q, 2048, 0x76);
    }
    mimir_fault_arm_lock(1);
    check(mimir_realloc(q, 8192) == NULL,
          "F-07: lock failure on realloc returns exactly NULL");
    check(q != NULL && pattern_intact(q, 2048, 0x76),
          "F-07: realloc original LIVE and byte-identical");
    mimir_free(q);

    att0 = mimir_fault_acquire_attempts;
    pinj0 = mimir_fault_pagesize_injected;
    mimir_fault_arm_pagesize(1);
    p = mimir_malloc(4096);
    check(p == NULL, "F-11: page-size failure on large malloc returns NULL");
    check_eq_ulong(mimir_fault_pagesize_injected, pinj0 + 1,
                   "F-11: exactly 1 injected page-size failure");
    check_eq_ulong(mimir_fault_acquire_attempts, att0,
                   "F-11: no mapping attempted when discovery fails");
    mimir_fault_arm_pagesize(1);
    p = mimir_malloc(112);
    check(p == NULL, "F-11: page-size failure on small malloc returns NULL");
    check_eq_ulong(mimir_fault_acquire_attempts, att0,
                   "F-11: small path also attempts no mapping");
    p = mimir_malloc(112);
    check(p != NULL, "F-11: recovery allocation succeeds");
    mimir_free(p);
    (void)mimir_core_trim_retained();
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "Leg 6 end: Q-MAPPED 0 at quiescent point");
}

/* ------------------------------------------------------------------ */
/* Leg 7: failure storm with exact accounting (FC-3/FC-4/FC-5).       */
/* ------------------------------------------------------------------ */
static void leg7_storm(void)
{
    static const size_t fresh[6] = {112, 128, 192, 768, 896, 1024};
    unsigned long att0;
    unsigned long inj0;
    unsigned long real0;
    unsigned long pub0;
    unsigned long ret0;
    unsigned long spub0;
    unsigned long sret0;
    unsigned long salloc0;
    unsigned long sfree0;
    unsigned long round;
    size_t fi = 0;
    int nulls_ok = 1;
    int successes_ok = 1;

    printf("Leg 7: 30-round injected failure storm (FC-3, FC-4, FC-5)\n");
    (void)mimir_core_trim_retained();
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "storm: starts at a quiescent Q-MAPPED 0");
    att0 = mimir_fault_acquire_attempts;
    inj0 = mimir_fault_acquire_injected;
    real0 = mimir_fault_acquire_real_failed;
    pub0 = mimir_audit_alloc_published;
    ret0 = mimir_audit_mapping_retired;
    spub0 = mimir_audit_span_published;
    salloc0 = mimir_audit_small_alloc;
    sfree0 = mimir_audit_small_free;

    for (round = 1; round <= 30; round++) {
        void *p;
        mimir_fault_arm_acquire(1);
        p = mimir_malloc(2048);
        if (p != NULL) {
            nulls_ok = 0;
            mimir_free(p);
        }
        if (round == 15) {
            unsigned long a_mid = mimir_fault_acquire_attempts;
            unsigned long i_mid = mimir_fault_acquire_injected;
            check(mimir_malloc(0) == NULL,
                  "storm: mid-storm malloc(0) is a posture NULL");
            check_eq_ulong(mimir_fault_acquire_attempts, a_mid,
                           "FC-4: posture NULL inside the storm attempts no mapping");
            check_eq_ulong(mimir_fault_acquire_injected, i_mid,
                           "FC-4: posture NULL inside the storm is not an OS failure");
        }
        if (round % 5 == 0) {
            void *big = mimir_malloc(2048);
            void *sm = mimir_malloc(fresh[fi]);
            fi++;
            if (big == NULL || sm == NULL) {
                successes_ok = 0;
            }
            if (big != NULL) {
                fill_pattern(big, 2048, 0x77);
                if (!pattern_intact(big, 2048, 0x77)) {
                    successes_ok = 0;
                }
            }
            if (sm != NULL) {
                fill_pattern(sm, 16, 0x78);
                if (!pattern_intact(sm, 16, 0x78)) {
                    successes_ok = 0;
                }
            }
            mimir_free(big);
            mimir_free(sm);
        }
    }
    check(nulls_ok, "storm: all 30 injected mallocs returned exactly NULL");
    check(successes_ok,
          "storm: all 12 interleaved success allocations verified");
    check_eq_ulong(mimir_fault_acquire_attempts, att0 + 42,
                   "FC-4: storm attempts = 30 injected + 6 large + 6 span acquires");
    check_eq_ulong(mimir_fault_acquire_injected, inj0 + 30,
                   "FC-4: storm injected failures = exactly 30");
    check_eq_ulong(mimir_fault_acquire_real_failed, real0,
                   "FC-4: storm real OS failures = 0 (separation holds)");
    check_eq_ulong(mimir_audit_alloc_published, pub0 + 6,
                   "FC-4: storm mappings published = the 6 successes");
    check_eq_ulong(mimir_audit_mapping_retired, ret0 + 6,
                   "FC-4: storm mappings retired = the 6 frees");
    check_eq_ulong(mimir_audit_span_published, spub0 + 6,
                   "FC-4: storm spans published = the 6 fresh classes");
    check_eq_ulong(mimir_audit_small_alloc, salloc0 + 6,
                   "FC-4: storm small objects = the 6 successes");
    check_eq_ulong(mimir_audit_small_free, sfree0 + 6,
                   "FC-4: storm small frees = the 6 successes");
    printf("  budget refusals in storm: 0 (no budget exists in this tree; "
           "INC-I001-14 -- caller-indistinguishable NULLs are all OS-kind "
           "by construction, and the counters above account for every one)\n");

    sret0 = mimir_audit_span_retired;
    (void)mimir_core_trim_retained();
    check_eq_ulong(mimir_audit_span_retired, sret0 + 6,
                   "FC-5: closing trim retires exactly the 6 retained spans");
    check_eq_size(mimir_core_retained_spans(), 0,
                  "FC-5: Q-RETAINED 0 after the storm (failures leaked nothing)");
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "FC-5: Q-MAPPED 0 after the storm");
    mimir_free(NULL);
    check(1, "FC-5: free(NULL) after the storm returns normally");
    {
        void *p = mimir_malloc(2048);
        check(p != NULL, "FC-5: allocator fully usable after the storm");
        mimir_free(p);
    }
}

/* ------------------------------------------------------------------ */
/* Final reconciliation + the C-007 / C-008 case ledger.              */
/* ------------------------------------------------------------------ */
static void final_reconciliation(void)
{
    printf("Final reconciliation at a quiescent point\n");
    (void)mimir_core_drain_retained();
    check_eq_size(mimir_core_retained_spans(), 0,
                  "final: Q-RETAINED 0 spans");
    check_eq_size(mimir_core_retained_bytes(), 0,
                  "final: Q-RETAINED 0 bytes");
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "final: Q-MAPPED 0");
    check_eq_ulong(mimir_audit_alloc_published,
                   mimir_audit_mapping_retired,
                   "final: mapping counters reconcile (published == retired)");
    check_eq_ulong(mimir_audit_span_published,
                   mimir_audit_span_retired,
                   "final: span counters reconcile (published == retired)");
    check_eq_ulong(mimir_audit_small_alloc, mimir_audit_small_free,
                   "final: object counters reconcile (alloc == free)");
    check_eq_ulong(mimir_fault_acquire_real_failed, 1,
                   "final: exactly 1 real OS failure in the whole run (Leg 0)");
    check_eq_ulong(mimir_fault_release_real_failed, 0,
                   "final: 0 real release failures in the whole run");
    check_eq_ulong(mimir_fault_lock_real_failed, 0,
                   "final: 0 real lock failures in the whole run");
    check_eq_ulong(mimir_fault_pagesize_real_failed, 0,
                   "final: 0 real page-size failures in the whole run");
    printf("  fault totals: acquire attempts=%lu injected=%lu real=%lu; "
           "release attempts=%lu injected=%lu real=%lu; "
           "lock attempts=%lu injected=%lu real=%lu; "
           "pagesize attempts=%lu injected=%lu real=%lu\n",
           mimir_fault_acquire_attempts, mimir_fault_acquire_injected,
           mimir_fault_acquire_real_failed,
           mimir_fault_release_attempts, mimir_fault_release_injected,
           mimir_fault_release_real_failed,
           mimir_fault_lock_attempts, mimir_fault_lock_injected,
           mimir_fault_lock_real_failed,
           mimir_fault_pagesize_attempts, mimir_fault_pagesize_injected,
           mimir_fault_pagesize_real_failed);
    printf("  audit totals: map published=%lu retired=%lu; "
           "span published=%lu retired=%lu; small alloc=%lu free=%lu\n",
           mimir_audit_alloc_published, mimir_audit_mapping_retired,
           mimir_audit_span_published, mimir_audit_span_retired,
           mimir_audit_small_alloc, mimir_audit_small_free);
}

static void case_ledger(void)
{
    printf("C-007 / C-008 owed injection cases -- disposition in this run\n");
    printf("  CASE P3-T-FAIL-001 (F-01): RUN -- Leg 1 N=1 sweep and Legs 2-4 per entry point\n");
    printf("  CASE P3-T-FAIL-002 (F-01 sweep): RUN -- Leg 1, N = 1..4 over six mallocs\n");
    printf("  CASE P3-T-FAIL-003 (F-01 recovery): RUN -- recovery allocation after every injected failure, Legs 1-4 and 6\n");
    printf("  CASE P3-T-FAIL-011 (F-02 no-attempt aspect): RUN -- Leg 0 posture set and Leg 6: rejections attempt no mapping (shim attempt counter is the missing instrument)\n");
    printf("  CASE P3-T-FAIL-021 (F-03 EMFILE/ENFILE class): RUN-BY-CLASS -- the platform layer collapses every mmap failure to ERR_MAP, so the injected ERR_MAP is the exact allocator-visible failure of that class; the errno flavour is not observable by design (U-10)\n");
    printf("  CASE P3-T-FAIL-031 (F-04 EAGAIN class): RUN-BY-CLASS -- same ERR_MAP collapse as FAIL-021\n");
    printf("  CASE P3-T-FAIL-051 (F-06 release failure): RUN -- Leg 5 (a) free rollback, (b) trim continuation, (c) realloc transfer, (d) span release at bound 0\n");
    printf("  CASE P3-T-FAIL-061 (F-07 lock failure): RUN -- Leg 6 lock channel\n");
    printf("  CASE P3-T-FAIL-101 (F-11 discovery failure): RUN -- Leg 6 page-size channel\n");
    printf("  CASE P3-T-FAIL-111 (F-12 reporting): STILL SKIPPED -- no reporting path exists in this tree (L7 compiled out); there is nothing for a shim to instrument\n");
    printf("  CASE P3-T-FAIL-041 (F-05): STILL BLOCKED-by-U-09 -- zero-size has no pass criterion; not an injection case\n");
    printf("  CASE P3-T-FAIL-081: STILL SKIPPED -- DIAGNOSTIC-ONLY, needs a debug/internal-check build, which this shim is not\n");
    printf("  CASE P3-T-ARITH-031 (metadata-allocation failure): SUPERSEDED-BY-CONSTRUCTION -- this allocator has no separate metadata allocation (descriptor in the mapping reserve page, span header in the span page); the mapping failure IS the metadata failure, run in Legs 1-4\n");
    printf("  CASE P3-T-ARITH-032 (partial descriptor): RUN-BY-CONSTRUCTION -- Leg 1: after each injected failure nothing is published and the next allocations behave exactly as before (no partial state exists to observe)\n");
    printf("  CASE P3-T-ARITH-042 (before-any-mapping aspect): RUN for size+reserve overflow (Leg 0, zero attempts); the alignment-parameter half still has no executable instance (no alignment entry point, INC-C007-05)\n");
    printf("  CASE S93-INJ-1 (fail-the-Nth sweep): RUN -- Leg 1 (same instrument as FAIL-001/002)\n");
    printf("  CASE S93-INJ-2 (metadata-allocation failure): SUPERSEDED-BY-CONSTRUCTION -- as ARITH-031\n");
    printf("  CASE S93-INJ-3 (EMFILE/EAGAIN class): RUN-BY-CLASS -- as FAIL-021/031\n");
    printf("  NOTE: the banked test_c007 / test_c008 binaries are unchanged and still print their SKIPPED lines; this slice supersedes the owed cases with the runs above rather than rewriting banked suites\n");
}

int main(void)
{
    long page = sysconf(_SC_PAGESIZE);

    printf("I-004 failure-handling evidence test (Phase 9)\n");
    printf("runtime system page size: %ld bytes\n", page);
    check(page > 0, "runtime page size discovered");

    leg0_transparency_and_postures();
    leg1_nth_sweep_large();
    leg2_small_path();
    leg3_calloc();
    leg4_realloc_atomic();
    leg5_release_failure();
    leg6_lock_and_pagesize();
    leg7_storm();
    final_reconciliation();
    case_ledger();

    printf("checks=%d failures=%d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
