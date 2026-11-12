/* test_z050_trim_policy.c - Z-050 trim policy under pressure.
 *
 * Suites:
 *  1. PSI parser: the live kernel format, boundary values,
 *     and malformed inputs (missing line, missing field,
 *     missing avg300 marker, NULLs).
 *  2. Decision table: every band boundary of
 *     docs/Z13_TRIM_POLICY.md S2, both sides of each line.
 *  3. Apply against a real retained pool: CALM+young trims
 *     nothing (the conservative promise), PARTIAL releases
 *     exactly half the retained spans rounded up, HIGH
 *     pressure releases everything and Q-RETAINED falls to
 *     zero with byte reconciliation (spans x page size).
 *  4. Live tick: reads this host's real /proc/pressure/memory
 *     and files a self-consistent report.
 *
 * Standalone via `make test-z050`; NOT part of `make test`
 * (standing gate counts untouched). Prints one summary line:
 * Z050 CHECKS: <run> run, <failed> failed -- exit 0 iff 0.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mimir_alloc.h"
#include "mimir_trim_policy_z050.h"
#include "mimir_trim_i003.h"
#include "mimir_tune_g007.h"

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

static int dbl_eq(double a, double b)
{
    double d = a - b;
    if (d < 0) {
        d = -d;
    }
    return d < 0.0005;
}

/* ---- suite 1: parser ------------------------------------ */

static void test_parser(void)
{
    struct mimir_pressure_sample s;
    const char *live =
        "some avg10=0.12 avg60=0.34 avg300=0.56 total=123456\n"
        "full avg10=0.01 avg60=0.02 avg300=0.03 total=654321\n";
    const char *hot =
        "some avg10=27.50 avg60=9.25 avg300=3.00 total=999999999\n"
        "full avg10=18.25 avg60=5.00 avg300=1.50 total=888888888\n";

    memset(&s, 0, sizeof s);
    CHECK(mimir_pressure_parse(live, &s) == 0);
    CHECK(dbl_eq(s.some_avg10, 0.12));
    CHECK(dbl_eq(s.full_avg10, 0.01));
    CHECK(s.some_total_us == 123456);
    CHECK(s.full_total_us == 654321);

    CHECK(mimir_pressure_parse(hot, &s) == 0);
    CHECK(dbl_eq(s.some_avg10, 27.50));
    CHECK(dbl_eq(s.full_avg10, 18.25));
    CHECK(s.some_total_us == 999999999);

    CHECK(mimir_pressure_parse(NULL, &s) == -1);
    CHECK(mimir_pressure_parse(live, NULL) == -1);
    CHECK(mimir_pressure_parse("", &s) == -1);
    CHECK(mimir_pressure_parse("some avg10=0.12 avg60=0.34 "
                               "avg300=0.56 total=1\n", &s) == -1);
    CHECK(mimir_pressure_parse("full avg10=0.01 avg60=0.02 "
                               "avg300=0.03 total=2\n", &s) == -1);
    /* No avg300 marker anywhere: not PSI text. */
    CHECK(mimir_pressure_parse("some avg10=0.12 total=1\n"
                               "full avg10=0.01 total=2\n",
                               &s) == -1);
    /* Failure leaves *out untouched. */
    s.some_avg10 = 42.0;
    CHECK(mimir_pressure_parse("garbage", &s) == -1);
    CHECK(dbl_eq(s.some_avg10, 42.0));

    /* The live file on this host must parse. */
    CHECK(mimir_core_pressure_read(&s) == 0);
    CHECK(s.some_avg10 >= 0.0 && s.some_avg10 <= 100.0);
    CHECK(s.full_avg10 >= 0.0 && s.full_avg10 <= 100.0);
}

/* ---- suite 2: decision table ---------------------------- */

static struct mimir_pressure_sample sample(double some, double full)
{
    struct mimir_pressure_sample s;
    s.some_avg10 = some;
    s.full_avg10 = full;
    s.some_total_us = 0;
    s.full_total_us = 0;
    return s;
}

static void test_decisions(void)
{
    struct mimir_pressure_sample s;

    /* HIGH band: FULL at any idle, from either gauge. */
    s = sample(5.00, 0.0);
    CHECK(mimir_trim_decision_for(&s, 0) == MIMIR_TRIM_DECISION_FULL);
    s = sample(27.5, 0.0);
    CHECK(mimir_trim_decision_for(&s, 0) == MIMIR_TRIM_DECISION_FULL);
    s = sample(0.0, 2.50);
    CHECK(mimir_trim_decision_for(&s, 0) == MIMIR_TRIM_DECISION_FULL);
    s = sample(0.0, 40.0);
    CHECK(mimir_trim_decision_for(&s, 3600) == MIMIR_TRIM_DECISION_FULL);
    /* Just below HIGH on both gauges is not HIGH. */
    s = sample(4.99, 2.49);
    CHECK(mimir_trim_decision_for(&s, 0) == MIMIR_TRIM_DECISION_PARTIAL);

    /* RAISED band: PARTIAL young, FULL at idle >= 10 s. */
    s = sample(1.00, 0.0);
    CHECK(mimir_trim_decision_for(&s, 0) == MIMIR_TRIM_DECISION_PARTIAL);
    CHECK(mimir_trim_decision_for(&s, 9) == MIMIR_TRIM_DECISION_PARTIAL);
    CHECK(mimir_trim_decision_for(&s, 10) == MIMIR_TRIM_DECISION_FULL);
    CHECK(mimir_trim_decision_for(&s, 600) == MIMIR_TRIM_DECISION_FULL);
    s = sample(3.25, 0.0);
    CHECK(mimir_trim_decision_for(&s, 10) == MIMIR_TRIM_DECISION_FULL);

    /* CALM band: NONE young, PARTIAL at idle >= 60 s, never
     * FULL from idle alone -- the conservative promise. */
    s = sample(0.99, 0.0);
    CHECK(mimir_trim_decision_for(&s, 0) == MIMIR_TRIM_DECISION_NONE);
    CHECK(mimir_trim_decision_for(&s, 59) == MIMIR_TRIM_DECISION_NONE);
    CHECK(mimir_trim_decision_for(&s, 60) == MIMIR_TRIM_DECISION_PARTIAL);
    s = sample(0.0, 0.0);
    CHECK(mimir_trim_decision_for(&s, 86400) ==
          MIMIR_TRIM_DECISION_PARTIAL);

    /* full alone (below HIGH) never raises the band. */
    s = sample(0.0, 2.49);
    CHECK(mimir_trim_decision_for(&s, 0) == MIMIR_TRIM_DECISION_NONE);

    /* NULL sample: NONE (fail-calm). */
    CHECK(mimir_trim_decision_for(NULL, 9999) == MIMIR_TRIM_DECISION_NONE);
}

/* ---- suite 3: apply against a real pool ----------------- */

#define Z050_CLASSES 6
#define Z050_PER_CLASS 3000

static const size_t z050_sizes[Z050_CLASSES] = {
    32, 64, 128, 256, 512, 1024
};
static void *z050_blocks[Z050_CLASSES][Z050_PER_CLASS];

static size_t build_retained_pool(void)
{
    size_t c, i;

    for (c = 0; c < Z050_CLASSES; c++) {
        for (i = 0; i < Z050_PER_CLASS; i++) {
            z050_blocks[c][i] = mimir_malloc(z050_sizes[c]);
            if (z050_blocks[c][i] != NULL) {
                memset(z050_blocks[c][i], 0xA5, z050_sizes[c]);
            }
        }
    }
    for (c = 0; c < Z050_CLASSES; c++) {
        for (i = 0; i < Z050_PER_CLASS; i++) {
            mimir_free(z050_blocks[c][i]);
            z050_blocks[c][i] = NULL;
        }
    }
    return mimir_core_retained_spans();
}

static void test_apply(void)
{
    struct mimir_pressure_sample calm = sample(0.0, 0.0);
    struct mimir_pressure_sample raised = sample(2.0, 0.0);
    struct mimir_pressure_sample high = sample(25.0, 10.0);
    size_t spans0, spans1, released;
    size_t bytes0;
    int decision = -1;

    CHECK(mimir_tune_set_retention_bound(16) == 0);

    /* trim_retained_some(0) releases nothing but performs
     * the trim surface's TLS drain, so the pool measured
     * below is the same pool the policy walks (the query
     * alone only flushes; the drain empties TLS bins and
     * their spans become EMPTY-retained). */
    (void)mimir_core_trim_retained_some(0);
    spans0 = build_retained_pool();
    (void)mimir_core_trim_retained_some(0);
    spans0 = mimir_core_retained_spans();
    bytes0 = mimir_core_retained_bytes();
    CHECK(spans0 >= 24); /* 6 classes, several spans each */
    CHECK(bytes0 == spans0 * 4096);

    /* CALM + young: nothing moves -- promise P-1. */
    released = mimir_core_trim_policy_apply(&calm, 0, &decision);
    CHECK(decision == MIMIR_TRIM_DECISION_NONE);
    CHECK(released == 0);
    CHECK(mimir_core_retained_spans() == spans0);

    /* RAISED + young: PARTIAL = half rounded up. */
    released = mimir_core_trim_policy_apply(&raised, 0, &decision);
    CHECK(decision == MIMIR_TRIM_DECISION_PARTIAL);
    CHECK(released == (spans0 + 1) / 2);
    spans1 = mimir_core_retained_spans();
    CHECK(spans1 == spans0 - released);
    CHECK(mimir_core_retained_bytes() == spans1 * 4096);

    /* HIGH at any idle: FULL, pool empties, bytes reconcile. */
    released = mimir_core_trim_policy_apply(&high, 0, &decision);
    CHECK(decision == MIMIR_TRIM_DECISION_FULL);
    CHECK(released == spans1);
    CHECK(mimir_core_retained_spans() == 0);
    CHECK(mimir_core_retained_bytes() == 0);

    /* FULL on an empty pool is a no-op (idempotent). */
    released = mimir_core_trim_policy_apply(&high, 0, &decision);
    CHECK(released == 0);

    /* The allocator is fully usable after policy trims. */
    {
        void *p = mimir_malloc(256);
        CHECK(p != NULL);
        if (p != NULL) {
            memset(p, 0x5A, 256);
            mimir_free(p);
        }
    }
    (void)mimir_core_trim_retained();
}

/* ---- suite 4: live tick --------------------------------- */

static void test_tick(void)
{
    struct mimir_trim_policy_report rep;
    struct mimir_pressure_sample now;
    size_t before = mimir_core_retained_spans();

    CHECK(mimir_core_trim_policy_tick(NULL) == -1);
    CHECK(mimir_core_pressure_read(&now) == 0);
    CHECK(mimir_core_trim_policy_tick(&rep) == 0);
    /* The tick's sample is a real PSI read: same band as a
     * direct read taken alongside it (values may drift a
     * hair between reads; the DECISION must agree with the
     * table applied to the tick's own sample). */
    CHECK(rep.decision ==
          mimir_trim_decision_for(&rep.sample, rep.idle_seconds));
    CHECK(rep.retained_after <= rep.retained_before);
    CHECK(rep.retained_before - rep.retained_after ==
          rep.released_spans * 4096);
    if (rep.decision == MIMIR_TRIM_DECISION_NONE) {
        CHECK(rep.released_spans == 0);
        CHECK(mimir_core_retained_spans() == before);
    }
    printf("tick: some_avg10=%.2f full_avg10=%.2f idle=%llus "
           "decision=%d released=%zu retained=%zu->%zu\n",
           rep.sample.some_avg10, rep.sample.full_avg10,
           (unsigned long long)rep.idle_seconds, rep.decision,
           rep.released_spans, rep.retained_before,
           rep.retained_after);
    (void)mimir_core_trim_retained();
}

int main(void)
{
    test_parser();
    test_decisions();
    test_apply();
    test_tick();
    printf("Z050 CHECKS: %d run, %d failed\n",
           checks_run, checks_failed);
    return checks_failed == 0 ? 0 : 1;
}
