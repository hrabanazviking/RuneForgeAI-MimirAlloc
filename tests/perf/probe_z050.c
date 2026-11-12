/* probe_z050.c - Z-050 pressure-cycle RSS probe.
 *
 * One process, Z-006's smaps-rollup method: baseline RSS,
 * build a retained span pool (alloc + touch + free across
 * six small classes, retention bound raised to 16), read
 * RSS again -- the retained bytes must be VISIBLE in RSS
 * (RSS honesty, Z-006's leg). Then run the policy twice:
 *
 *  CALM leg:  sample (0,0), idle 0 -> decision NONE, zero
 *             released, RSS unchanged within tolerance.
 *  PRESSURE leg: an injected HIGH sample (some 25.00,
 *             full 10.00 -- the policy's input contract is
 *             a PSI sample; the live-file leg is covered by
 *             the unit suite's tick test) -> decision FULL,
 *             every retained span released, and RSS falls
 *             by at least the retained bytes minus the
 *             tolerance. RSS FOLLOWS THE POLICY'S PROMISE.
 *
 * Prints key=value lines; the last line is
 * "PROBE_Z050: PASS" or "PROBE_Z050: FAIL <reason>".
 * Exit 0 iff PASS. Standalone via make probe-z050.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mimir_alloc.h"
#include "../core/mimir_trim_policy_z050.h"
#include "../core/mimir_trim_i003.h"
#include "../core/mimir_tune_g007.h"

#define NCLASS 6
#define PER_CLASS 3000
#define TOL_KIB 64

static const size_t sizes[NCLASS] = {32, 64, 128, 256, 512, 1024};
static void *blocks[NCLASS][PER_CLASS];

static long rss_kib(void)
{
    FILE *f = fopen("/proc/self/smaps_rollup", "r");
    char line[256];
    long v = -1;

    if (f != NULL) {
        while (fgets(line, sizeof line, f) != NULL) {
            if (strncmp(line, "Rss:", 4) == 0) {
                v = strtol(line + 4, NULL, 10);
                break;
            }
        }
        fclose(f);
        if (v >= 0) {
            return v;
        }
    }
    f = fopen("/proc/self/status", "r");
    if (f == NULL) {
        return -1;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            v = strtol(line + 6, NULL, 10);
            break;
        }
    }
    fclose(f);
    return v;
}

static void fail(const char *why, long a, long b)
{
    printf("PROBE_Z050: FAIL %s (%ld vs %ld)\n", why, a, b);
    exit(1);
}

int main(void)
{
    struct mimir_pressure_sample calm, high;
    long rss_base, rss_free, rss_calm, rss_press;
    size_t retained, spans, released;
    int decision = -1;
    size_t c, i;

    memset(&calm, 0, sizeof calm);
    memset(&high, 0, sizeof high);
    high.some_avg10 = 25.0;
    high.full_avg10 = 10.0;

    if (mimir_tune_set_retention_bound(16) != 0) {
        fail("retention bound refused", 0, 0);
    }

    rss_base = rss_kib();
    for (c = 0; c < NCLASS; c++) {
        for (i = 0; i < PER_CLASS; i++) {
            blocks[c][i] = mimir_malloc(sizes[c]);
            if (blocks[c][i] == NULL) {
                fail("alloc NULL", (long)c, (long)i);
            }
            memset(blocks[c][i], 0xA5, sizes[c]);
        }
    }
    for (c = 0; c < NCLASS; c++) {
        for (i = 0; i < PER_CLASS; i++) {
            mimir_free(blocks[c][i]);
            blocks[c][i] = NULL;
        }
    }
    (void)mimir_core_trim_retained_some(0); /* TLS drain: see
                     * the unit suite's note -- the measured
                     * pool must be the pool the policy walks */
    spans = mimir_core_retained_spans();
    retained = mimir_core_retained_bytes();
    rss_free = rss_kib();
    printf("rss_base_kib=%ld\n", rss_base);
    printf("retained_spans=%zu\n", spans);
    printf("retained_bytes=%zu\n", retained);
    printf("rss_after_free_kib=%ld\n", rss_free);
    if (retained < 131072) {
        fail("pool too small to measure", (long)retained, 131072);
    }
    if (rss_free < rss_base + (long)(retained / 1024) - TOL_KIB) {
        fail("retained bytes not visible in RSS after free",
             rss_free, rss_base);
    }

    /* CALM leg: the conservative promise -- nothing moves. */
    released = mimir_core_trim_policy_apply(&calm, 0, &decision);
    rss_calm = rss_kib();
    printf("calm_decision=%d\n", decision);
    printf("calm_released=%zu\n", released);
    printf("rss_after_calm_kib=%ld\n", rss_calm);
    if (decision != MIMIR_TRIM_DECISION_NONE || released != 0) {
        fail("calm leg trimmed", (long)decision, (long)released);
    }
    if (rss_calm < rss_free - TOL_KIB) {
        fail("calm leg RSS fell", rss_calm, rss_free);
    }

    /* PRESSURE leg: RSS must follow the promise down. */
    released = mimir_core_trim_policy_apply(&high, 0, &decision);
    rss_press = rss_kib();
    printf("pressure_decision=%d\n", decision);
    printf("pressure_released=%zu\n", released);
    printf("retained_after_pressure=%zu\n",
           mimir_core_retained_bytes());
    printf("rss_after_pressure_kib=%ld\n", rss_press);
    if (decision != MIMIR_TRIM_DECISION_FULL) {
        fail("pressure leg not FULL", (long)decision, 0);
    }
    if (released != spans) {
        fail("pressure leg released != retained spans",
             (long)released, (long)spans);
    }
    if (mimir_core_retained_bytes() != 0) {
        fail("pool not empty after FULL", 0, 0);
    }
    /* Compare against rss_calm, the state immediately
     * before the pressure leg: between rss_free and the
     * pressure apply, the calm leg's own stdio activity
     * grew RSS by ~132 KiB in the runs of record, which
     * would mask the drop if rss_free were the baseline.
     * The promise is about the pressure leg's own delta. */
    if (rss_press > rss_calm - (long)(retained / 1024) + TOL_KIB) {
        fail("RSS did not follow the policy promise",
             rss_press, rss_calm);
    }

    printf("rss_drop_kib=%ld\n", rss_calm - rss_press);
    printf("PROBE_Z050: PASS\n");
    return 0;
}
