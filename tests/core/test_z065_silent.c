/*
 * test_z065_silent.c -- Z-065 silent courses made
 * distinguishable (D-02) contract evidence.
 *
 * Slice: Z-065 (MIMIRALLOC_Z13_ROADMAP.md, Wave 8).
 * Standalone via `make test-z065`; NOT part of `make test`
 * (standing gate counts untouched). Links the library
 * OBJECTS directly plus the I-004 fault-injection shim
 * (--wrap on the four L6 entries; only the lock channel is
 * armed here), with the leaf platform object built
 * -fno-lto so --wrap can interpose (the Z-064 build fact).
 *
 * The defect (register D-02 / INC-M002-02): drain/trim
 * return 0 on lock failure, indistinguishable from
 * nothing-retained, and free on lock failure returns
 * silently with the block live. The decision of record
 * (docs/DECISION_LEDGER.md D-010) is COUNTER, not sentinel:
 * returns and the F-07 posture are unchanged, and three
 * monotone tallies (drain/trim/free lock failures) make
 * each course distinguishable.
 *
 * Contract legs:
 *  1. CONTROL, nothing retained: drain and both trim forms
 *     return 0 and every tally stays 0 -- the quiet no-op
 *     course, proven silent in the tallies.
 *  2. ARMED, nothing retained: the same calls return the
 *     SAME 0, but the drain tally and the trim tally each
 *     move by exactly 1. Return-identical, tally-distinct:
 *     that is the whole slice in one comparison.
 *  3. RETAINED leg: with retained spans present, an armed
 *     trim releases nothing and the retained count is
 *     unchanged (tally +1); an unarmed trim then releases
 *     the retained spans. Same pair for drain.
 *  4. FREE leg (legacy course): an armed free of a large
 *     block leaves the block LIVE and writable (F-07) and
 *     moves the free tally by exactly 1; the unarmed retry
 *     frees it and the tally does not move again.
 *  5. FREE leg (remote course): a cross-thread free of a
 *     small block under an armed lock failure likewise
 *     leaves the block live and moves the free tally by
 *     exactly 1; the owner then frees it normally.
 *  6. NON-COURSES: an allocation lock failure still takes
 *     its pre-existing channel (NULL + diagnostics) and
 *     moves NONE of the three tallies.
 *  7. SNAPSHOT agreement: the stats snapshot fields equal
 *     the lock-free getters at the end, at the exact totals
 *     the legs above predict.
 * The test prints TALLY evidence lines for the recorded
 * run in addition to its CHECKS summary.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mimir_alloc.h"
#include "mimir_drain_h001.h"
#include "mimir_fault_i004.h"
#include "mimir_stats_j002.h"
#include "mimir_trim_i003.h"

static int checks_run;
static int checks_failed;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks_run++;                                                   \
        if (!(cond)) {                                                  \
            checks_failed++;                                            \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
        }                                                               \
    } while (0)

static uint64_t tally_drain(void) { return mimir_core_drain_lock_failures(); }
static uint64_t tally_trim(void) { return mimir_core_trim_lock_failures(); }
static uint64_t tally_free(void) { return mimir_core_free_lock_failures(); }

/* Allocate n small blocks of one class, free them all, and
 * flush the TLS bins with a zero-budget trim so the emptied
 * spans stand RETAINED. Returns the retained span count. */
static size_t build_retained(void)
{
    void *ptrs[256];
    size_t i;
    for (i = 0; i < 256; i++) {
        ptrs[i] = mimir_malloc(32);
        CHECK(ptrs[i] != NULL);
        memset(ptrs[i], 0xA5, 32);
    }
    for (i = 0; i < 256; i++) {
        mimir_free(ptrs[i]);
    }
    (void)mimir_core_trim_retained_some(0);
    return mimir_core_retained_spans();
}

struct remote_arg {
    void *ptr;
};

static void *remote_free_main(void *v)
{
    struct remote_arg *a = (struct remote_arg *)v;
    mimir_free(a->ptr);
    return NULL;
}

int main(void)
{
    size_t r;
    uint64_t d0, t0, f0;
    void *big;
    void *small;
    pthread_t th;
    struct remote_arg arg;

    /* Fresh process: every tally at 0. */
    CHECK(tally_drain() == 0);
    CHECK(tally_trim() == 0);
    CHECK(tally_free() == 0);

    /* ---- leg 1: control, nothing retained ------------- */
    (void)mimir_core_drain_retained();
    CHECK(mimir_core_retained_spans() == 0);
    r = mimir_core_trim_retained();
    CHECK(r == 0);
    r = mimir_core_trim_retained_some(4);
    CHECK(r == 0);
    r = mimir_core_drain_retained();
    CHECK(r == 0);
    CHECK(tally_trim() == 0);
    CHECK(tally_drain() == 0);
    printf("TALLY control nothing-retained drain=%llu trim=%llu "
           "free=%llu\n",
           (unsigned long long)tally_drain(),
           (unsigned long long)tally_trim(),
           (unsigned long long)tally_free());

    /* ---- leg 2: armed, nothing retained --------------- */
    mimir_fault_arm_lock(1);
    r = mimir_core_trim_retained();
    CHECK(r == 0); /* return identical to the quiet course */
    CHECK(tally_trim() == 1);
    mimir_fault_arm_lock(1);
    r = mimir_core_trim_retained_some(4);
    CHECK(r == 0);
    CHECK(tally_trim() == 2);
    mimir_fault_arm_lock(1);
    r = mimir_core_drain_retained();
    CHECK(r == 0);
    CHECK(tally_drain() == 1);
    CHECK(tally_free() == 0);
    printf("TALLY armed nothing-retained drain=%llu trim=%llu "
           "free=%llu\n",
           (unsigned long long)tally_drain(),
           (unsigned long long)tally_trim(),
           (unsigned long long)tally_free());

    /* ---- leg 3: retained present ---------------------- */
    {
        size_t kept = build_retained();
        CHECK(kept >= 1);
        mimir_fault_arm_lock(1);
        r = mimir_core_trim_retained();
        CHECK(r == 0);
        CHECK(tally_trim() == 3);
        CHECK(mimir_core_retained_spans() == kept);
        r = mimir_core_trim_retained();
        CHECK(r == kept);
        CHECK(mimir_core_retained_spans() == 0);
        CHECK(tally_trim() == 3);

        kept = build_retained();
        CHECK(kept >= 1);
        mimir_fault_arm_lock(1);
        r = mimir_core_drain_retained();
        CHECK(r == 0);
        CHECK(tally_drain() == 2);
        CHECK(mimir_core_retained_spans() == kept);
        r = mimir_core_drain_retained();
        CHECK(r >= kept);
        CHECK(mimir_core_retained_spans() == 0);
        CHECK(tally_drain() == 2);
    }
    printf("TALLY retained legs drain=%llu trim=%llu free=%llu\n",
           (unsigned long long)tally_drain(),
           (unsigned long long)tally_trim(),
           (unsigned long long)tally_free());

    /* ---- leg 4: free, legacy course ------------------- */
    big = mimir_malloc(300000);
    CHECK(big != NULL);
    memset(big, 0x5A, 300000);
    f0 = tally_free();
    mimir_fault_arm_lock(1);
    mimir_free(big);
    CHECK(tally_free() == f0 + 1);
    /* F-07: the block stayed LIVE -- still writable. */
    memset(big, 0xC3, 300000);
    mimir_free(big);
    CHECK(tally_free() == f0 + 1);
    printf("TALLY free legacy f0=%llu after=%llu\n",
           (unsigned long long)f0,
           (unsigned long long)tally_free());

    /* ---- leg 5: free, remote course ------------------- */
    small = mimir_malloc(48);
    CHECK(small != NULL);
    memset(small, 0x3C, 48);
    arg.ptr = small;
    f0 = tally_free();
    mimir_fault_arm_lock(1);
    CHECK(pthread_create(&th, NULL, remote_free_main, &arg) == 0);
    CHECK(pthread_join(th, NULL) == 0);
    CHECK(tally_free() == f0 + 1);
    memset(small, 0x96, 48); /* still live, still ours */
    mimir_free(small);
    CHECK(tally_free() == f0 + 1);
    printf("TALLY free remote f0=%llu after=%llu\n",
           (unsigned long long)f0,
           (unsigned long long)tally_free());

    /* ---- leg 6: allocation lock failure is not silent,
     * and is NOT one of these courses -------------------- */
    d0 = tally_drain();
    t0 = tally_trim();
    f0 = tally_free();
    mimir_fault_arm_lock(1);
    {
        void *p = mimir_malloc(300000);
        CHECK(p == NULL); /* pre-existing NULL + diag channel */
    }
    CHECK(tally_drain() == d0);
    CHECK(tally_trim() == t0);
    CHECK(tally_free() == f0);

    /* ---- leg 7: snapshot agreement --------------------- */
    {
        struct mimir_stats st;
        CHECK(mimir_core_stats_snapshot(&st) == 0);
        CHECK(st.drain_lock_failures == tally_drain());
        CHECK(st.trim_lock_failures == tally_trim());
        CHECK(st.free_lock_failures == tally_free());
        CHECK(st.drain_lock_failures == 2);
        CHECK(st.trim_lock_failures == 3);
        CHECK(st.free_lock_failures == 2);
        printf("TALLY final snapshot drain=%llu trim=%llu "
               "free=%llu shim_lock_injected=%lu\n",
               (unsigned long long)st.drain_lock_failures,
               (unsigned long long)st.trim_lock_failures,
               (unsigned long long)st.free_lock_failures,
               mimir_fault_lock_injected);
    }

    printf("CHECKS: %d run, %d failed\n", checks_run, checks_failed);
    printf("RESULT: %s\n", checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
