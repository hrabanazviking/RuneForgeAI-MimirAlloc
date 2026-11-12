/* probe_z059.c - Z-059: placement policy trial.
 *
 * First-fit vs best-fit vs the current policy, measured
 * on the Z-055 fragmentation oracle (tests/perf/
 * probe_z055.c; banked recorded run tests/perf/
 * recorded_run_z13_z055.txt). The policy under test is
 * compiled into the core objects by the runner
 * (tests/perf/run_z059.sh) under one of:
 *   (no flag)                  current policy: the class's
 *                              active span first, then a
 *                              first-fit scan, then fresh.
 *   -DMIMIR_PLACEMENT_FIRSTFIT pure first-fit: ignore the
 *                              active span, first can-serve
 *                              span in list order.
 *   -DMIMIR_PLACEMENT_BESTFIT  best-fit: can-serve span
 *                              with the fewest servable
 *                              slots, ties by list order.
 * This probe is policy-blind: it runs the identical
 * workload in all three builds and prints METRIC lines
 * the summariser (tests/perf/summarize_z059.py) tables.
 * Every check below is a policy INVARIANT (it must hold
 * under every policy); the policy-dependent quantities
 * are printed, never asserted.
 *
 * Leg A is the Z-055 standard fragmented state, verbatim
 * in construction: large mappings 20000/40000 kept live,
 * classes 64/256/1024 filled to 3 x capacity, odd indices
 * freed, quiesce, F-3 / tails / R-EXT / R-INT from the
 * measured per-class span deltas (I-002 formula). Leg A
 * is the control: filling fresh spans gives a policy no
 * hole to choose, so its graded numbers must reproduce
 * Z-055 exactly (F-3 32064, tails 3360, R-EXT 0.260937,
 * R-INT 0.091146, bearing 3/3/3, live 93/23/5).
 *
 * Leg B is the decision leg, per class 64 and 256, from
 * a clean state: fill 3 x capacity, quiesce, group the
 * blocks by span (span == one 4096 B page on this host,
 * Z-055 header, so the page of a pointer IS its span),
 * then free unevenly by group rank (page order): rank 0
 * all but 1, rank 1 half, rank 2 all but 2, rank >= 3
 * half. Quiesce. Then 24 follow-on allocations of the
 * class (24 > the TLS refill batch of 16, so the batch
 * cannot mask the policy; single thread throughout).
 * Metrics: distinct spans touched by the follow-ons,
 * new spans published during the follow-on phase, and
 * P2 = empty spans at quiesce after freeing ALL original
 * blocks with the follow-ons still live (retained-empty
 * count + spans sweep-released during that free phase).
 *
 * Pre-registered decision rule (filed before any run;
 * the ledger entry D-009 applies it exactly): best-fit
 * is CHOSEN (in the Z-039 form: flag-gated, production
 * default unchanged, adoption of the default reserved
 * to a later slice) iff ALL of: (a) Leg A reproduces
 * Z-055 exactly in every build; (b) follow-on touched:
 * best-fit < current, summed over classes 64+256, and
 * best-fit no worse than current in either class;
 * (c) P2: best-fit >= current summed, and no worse in
 * either class; (d) new spans == 0 for every policy;
 * (e) first-fit does NOT match best-fit on both (b) and
 * (c) (else the effect is the active pointer, not the
 * fit); (f) every check passes in every build.
 * Otherwise the decision is NO ADOPTION: the current
 * policy stays, the trial flags stay available.
 *
 * Quiescence (Z-029) is mimir_core_trim_retained_some(0)
 * before every snapshot, exactly as Z-055. All graded
 * quantities are exact allocator counters at quiesced
 * points; no wall time is measured or asserted. Stand-
 * alone: built by run_z059.sh against the library
 * OBJECTS, NOT part of make test. Exit 0 and
 * RESULT: PASS iff every check passed.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1

#include "mimir_alloc.h"
#include "mimir_small_g003.h"
#include "mimir_trim_i003.h"
#include "mimir_tune_g007.h"
#include "mimir_drain_h001.h"
#include "mimir_pool_z048.h"
#include "../abi/mimir_audit_e005.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(MIMIR_PLACEMENT_BESTFIT)
#define POLICY_NAME "best-fit"
#elif defined(MIMIR_PLACEMENT_FIRSTFIT)
#define POLICY_NAME "first-fit"
#else
#define POLICY_NAME "current"
#endif

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

static size_t page_bytes;

static void quiesce(void)
{
    (void)mimir_core_trim_retained_some(0);
}

static size_t page_round_up(size_t x)
{
    return (x + page_bytes - 1) / page_bytes * page_bytes;
}

#define MAXN 256
static void *b64[MAXN];
static void *b256[MAXN];
static void *b1024[MAXN];
static void *bl[MAXN];
static void *fol[64];

static int alloc_fill(void **arr, size_t n, size_t req,
                      unsigned char seed0)
{
    size_t i;
    for (i = 0; i < n; i++) {
        arr[i] = mimir_malloc(req);
        if (arr[i] == NULL) {
            return 0;
        }
        fill_pattern(arr[i], req,
                     (unsigned char)(seed0 + i));
    }
    return 1;
}

static int live_patterns_ok(void **arr, size_t n, size_t req,
                            unsigned char seed0,
                            const int *keep)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (keep != NULL && !keep[i]) {
            continue;
        }
        if (!pattern_intact(arr[i], req,
                            (unsigned char)(seed0 + i))) {
            return 0;
        }
    }
    return 1;
}

/* Leg B for one class. req/cap describe the class;
 * follow_n follow-on allocations. Prints one METRIC
 * legB line. Returns via out-params nothing; all graded
 * values are printed. */
static void leg_b(size_t req, size_t cap,
                  unsigned char seed0, size_t follow_n)
{
    size_t n = 3 * cap;
    size_t i, g, r;
    /* groups: page -> list of indices into bl[] */
    uintptr_t pages[MAXN];
    size_t gidx[MAXN][MAXN];
    size_t gcnt[MAXN];
    size_t ngroups = 0;
    int keep[MAXN];
    size_t live_after_free = 0;
    size_t f3class;
    unsigned long pub0, ret0;
    size_t new_spans, touched;
    uintptr_t tpages[64];
    size_t ntouched = 0;
    size_t empty_ret, empty_rel;

    printf("== leg B class %zu (cap %zu, follow %zu) ==\n",
           req, cap, follow_n);
    for (i = 0; i < MAXN; i++) {
        keep[i] = 1;
        gcnt[i] = 0;
    }
    check(alloc_fill(bl, n, req, seed0), "leg B fill");
    quiesce();
    /* group by page (span == page on this host) */
    for (i = 0; i < n; i++) {
        uintptr_t pg = (uintptr_t)bl[i] &
                       ~(uintptr_t)(page_bytes - 1);
        for (g = 0; g < ngroups; g++) {
            if (pages[g] == pg) {
                break;
            }
        }
        if (g == ngroups) {
            pages[ngroups] = pg;
            gcnt[ngroups] = 0;
            ngroups++;
        }
        gidx[g][gcnt[g]++] = i;
    }
    /* sort groups by page ascending (insertion sort) */
    for (g = 1; g < ngroups; g++) {
        uintptr_t pg = pages[g];
        size_t tmp[MAXN];
        size_t tc = gcnt[g];
        size_t k;
        memcpy(tmp, gidx[g], tc * sizeof(size_t));
        r = g;
        while (r > 0 && pages[r - 1] > pg) {
            pages[r] = pages[r - 1];
            gcnt[r] = gcnt[r - 1];
            memcpy(gidx[r], gidx[r - 1],
                   gcnt[r] * sizeof(size_t));
            r--;
        }
        pages[r] = pg;
        gcnt[r] = tc;
        for (k = 0; k < tc; k++) {
            gidx[r][k] = tmp[k];
        }
    }
    printf("  groups (bearing spans) = %zu:", ngroups);
    for (g = 0; g < ngroups; g++) {
        printf(" %zu", gcnt[g]);
    }
    printf("\n");
    check(ngroups >= 3, "leg B at least 3 bearing spans");
    /* uneven free by rank */
    for (g = 0; g < ngroups; g++) {
        size_t cnt = gcnt[g];
        size_t nfree, k;
        if (g == 0) {
            nfree = cnt - 1;          /* keep 1 */
        } else if (g == 2) {
            nfree = cnt >= 2 ? cnt - 2 : 0; /* keep 2 */
        } else {
            nfree = cnt / 2;          /* free half */
        }
        for (k = 0; k < nfree; k++) {
            size_t idx = gidx[g][k];
            mimir_free(bl[idx]);
            keep[idx] = 0;
        }
    }
    quiesce();
    for (i = 0; i < n; i++) {
        if (keep[i]) {
            live_after_free++;
        }
    }
    f3class = 0;
    for (g = 0; g < ngroups; g++) {
        size_t live_g = 0;
        for (i = 0; i < gcnt[g]; i++) {
            if (keep[gidx[g][i]]) {
                live_g++;
            }
        }
        f3class += (cap - live_g) * req;
    }
    f3class += mimir_core_retained_bytes();
    printf("  leg B fragmented: live=%zu f3class=%zu "
           "retained_spans=%zu\n", live_after_free, f3class,
           mimir_core_retained_spans());
    check(live_patterns_ok(bl, n, req, seed0, keep),
          "leg B kept patterns intact at fragmented point");
    /* follow-on phase */
    pub0 = mimir_audit_span_published;
    for (i = 0; i < follow_n; i++) {
        fol[i] = mimir_malloc(req);
        if (fol[i] == NULL) {
            break;
        }
        fill_pattern(fol[i], req, (unsigned char)(0xE0 + i));
    }
    check_eq_size(i, follow_n, "leg B follow-on all served");
    quiesce();
    new_spans = (size_t)(mimir_audit_span_published - pub0);
    check_eq_size(new_spans, 0,
                  "leg B follow-on acquires no new span "
                  "(holes existed for every policy)");
    for (i = 0; i < follow_n; i++) {
        uintptr_t pg = (uintptr_t)fol[i] &
                       ~(uintptr_t)(page_bytes - 1);
        size_t k;
        for (k = 0; k < ntouched; k++) {
            if (tpages[k] == pg) {
                break;
            }
        }
        if (k == ntouched) {
            tpages[ntouched++] = pg;
        }
    }
    touched = ntouched;
    {
        int ok = 1;
        for (i = 0; i < follow_n; i++) {
            if (!pattern_intact(fol[i], req,
                                (unsigned char)(0xE0 + i))) {
                ok = 0;
            }
        }
        check(ok, "leg B follow-on patterns intact");
    }
    check(live_patterns_ok(bl, n, req, seed0, keep),
          "leg B kept originals intact after follow-on");
    /* free ALL originals; follow-ons stay live */
    ret0 = mimir_audit_span_retired;
    for (i = 0; i < n; i++) {
        if (keep[i]) {
            mimir_free(bl[i]);
            keep[i] = 0;
        }
    }
    quiesce();
    empty_ret = mimir_core_retained_spans();
    empty_rel = (size_t)(mimir_audit_span_retired - ret0);
    printf("METRIC legB req=%zu groups=%zu f3class=%zu "
           "follow=%zu touched=%zu newspans=%zu "
           "empty_retained=%zu empty_released=%zu "
           "empty_total=%zu\n", req, ngroups, f3class,
           follow_n, touched, new_spans, empty_ret,
           empty_rel, empty_ret + empty_rel);
    /* cleanup: free follow-ons, trim to large-only */
    for (i = 0; i < follow_n; i++) {
        mimir_free(fol[i]);
    }
    quiesce();
    (void)mimir_core_trim_retained();
    check_eq_size(mimir_core_retained_spans(), 0,
                  "leg B cleanup: no retained spans");
}

int main(void)
{
    size_t cap64, cap256, cap512, cap1024;
    size_t tail64, tail256, tail1024;
    size_t n64, n256, n1024;
    void *large1, *large2;
    size_t large1_len, large2_len, large_sum;
    size_t i;
    unsigned long sa0;

    page_bytes = (size_t)sysconf(_SC_PAGESIZE);
    printf("Z-059 placement policy trial (Z-055 oracle)\n");
    printf("METRIC policy=%s\n", POLICY_NAME);
    printf("page=%zu retention_bound=%zu\n", page_bytes,
           mimir_tune_get_retention_bound());
    check_eq_size(page_bytes, 4096,
                  "host header: page size 4096 on this host");
    check_eq_size(mimir_tune_get_retention_bound(), 2,
                  "retention bound is the default 2");
    cap64 = (page_bytes - 96) / 64;
    cap256 = (page_bytes - 96) / 256;
    cap512 = (page_bytes - 96) / 512;
    cap1024 = (page_bytes - 96) / 1024;
    tail64 = page_bytes - 96 - cap64 * 64;
    tail256 = page_bytes - 96 - cap256 * 256;
    tail1024 = page_bytes - 96 - cap1024 * 1024;
    (void)cap512;
    printf("capacities: 64->%zu 256->%zu 1024->%zu\n",
           cap64, cap256, cap1024);
    check_eq_size(cap64, 62, "capacity class 64");
    check_eq_size(cap256, 15, "capacity class 256");
    check_eq_size(cap1024, 3, "capacity class 1024");
    n64 = 3 * cap64;
    n256 = 3 * cap256;
    n1024 = 3 * cap1024;

    printf("== baseline ==\n");
    quiesce();
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "baseline Q-MAPPED 0");
    check_eq_size(mimir_core_retained_bytes(), 0,
                  "baseline Q-RETAINED 0");

    printf("== large mappings, kept live ==\n");
    large1 = mimir_malloc(20000);
    large2 = mimir_malloc(40000);
    check(large1 != NULL && large2 != NULL,
          "large blocks allocated");
    fill_pattern(large1, 20000, 0xA1);
    fill_pattern(large2, 40000, 0xB2);
    large1_len = page_round_up(page_bytes + 20000);
    large2_len = page_round_up(page_bytes + 40000);
    large_sum = large1_len + large2_len;
    quiesce();
    check_eq_size(mimir_core_mapped_bytes(), large_sum,
                  "Q-MAPPED = sum of mapping lengths");

    printf("== leg A: Z-055 standard fragmented state ==\n");
    {
        struct phase { size_t acq, rel, ret, bear; };
        struct phase ph[3];
        void **arrs[3];
        size_t ns[3], reqs[3], caps[3], tailv[3], live[3];
        size_t bear_total = 0;
        size_t f3 = 0, tails = 0;
        size_t retained_frag;
        int k;
        arrs[0] = b64; arrs[1] = b256; arrs[2] = b1024;
        ns[0] = n64; ns[1] = n256; ns[2] = n1024;
        reqs[0] = 64; reqs[1] = 256; reqs[2] = 1024;
        caps[0] = cap64; caps[1] = cap256; caps[2] = cap1024;
        tailv[0] = tail64; tailv[1] = tail256;
        tailv[2] = tail1024;
        sa0 = mimir_audit_small_alloc;
        for (k = 0; k < 3; k++) {
            unsigned long p0 = mimir_audit_span_published;
            unsigned long r0 = mimir_audit_span_retired;
            size_t ret_before = mimir_core_retained_spans();
            check(alloc_fill(arrs[k], ns[k], reqs[k],
                             (unsigned char)(0x10 + 0x30 * k)),
                  "leg A alloc fragmented class");
            for (i = 1; i < ns[k]; i += 2) {
                mimir_free(arrs[k][i]);
            }
            quiesce();
            ph[k].acq = (size_t)(mimir_audit_span_published - p0);
            ph[k].rel = (size_t)(mimir_audit_span_retired - r0);
            ph[k].ret = mimir_core_retained_spans() - ret_before;
            ph[k].bear = ph[k].acq - ph[k].rel - ph[k].ret;
            live[k] = (ns[k] + 1) / 2;
            bear_total += ph[k].bear;
            printf("METRIC legA_class req=%zu acq=%zu rel=%zu "
                   "ret=%zu bear=%zu live=%zu\n", reqs[k],
                   ph[k].acq, ph[k].rel, ph[k].ret, ph[k].bear,
                   live[k]);
            check_eq_size(ph[k].bear, 3,
                          "leg A bearing spans == Z-055 (3)");
            check_eq_size(live[k], k == 0 ? 93 : k == 1 ? 23 : 5,
                          "leg A live == Z-055");
        }
        check_eq_size((size_t)(mimir_audit_small_alloc - sa0),
                      n64 + n256 + n1024,
                      "leg A objects handed out");
        retained_frag = mimir_core_retained_spans();
        {
            size_t span_total = bear_total + retained_frag;
            check_eq_size(mimir_core_mapped_bytes(),
                          large_sum + span_total * page_bytes,
                          "leg A fragmented Q-MAPPED exact");
        }
        for (k = 0; k < 3; k++) {
            f3 += (ph[k].bear * caps[k] - live[k]) * reqs[k];
            tails += ph[k].bear * tailv[k];
        }
        f3 += mimir_core_retained_bytes();
        printf("METRIC legA f3=%zu tails=%zu rext=%.6f "
               "rint=%.6f qmapped=%zu qretained=%zu\n", f3,
               tails, (double)f3 / (double)mimir_core_mapped_bytes(),
               (double)tails / (double)(bear_total * page_bytes),
               mimir_core_mapped_bytes(),
               mimir_core_retained_bytes());
        check_eq_size(f3, 32064, "leg A F-3 == Z-055 (32064)");
        check_eq_size(tails, 3360,
                      "leg A tails == Z-055 (3360)");
        check_eq_size(mimir_core_mapped_bytes(), 122880,
                      "leg A Q-MAPPED == Z-055 (122880)");
        {
            int odds_keep[MAXN];
            size_t j;
            for (j = 0; j < MAXN; j++) {
                odds_keep[j] = (j % 2) == 0;
            }
            check(live_patterns_ok(b64, n64, 64, 0x10, odds_keep) &&
                  live_patterns_ok(b256, n256, 256, 0x40, odds_keep) &&
                  live_patterns_ok(b1024, n1024, 1024, 0x70,
                                   odds_keep),
                  "leg A live patterns intact");
        }
        check(pattern_intact(large1, 20000, 0xA1) &&
              pattern_intact(large2, 40000, 0xB2),
              "leg A large patterns intact");
        /* free the surviving evens; back to large-only */
        for (i = 0; i < n64; i += 2) {
            mimir_free(b64[i]);
        }
        for (i = 0; i < n256; i += 2) {
            mimir_free(b256[i]);
        }
        for (i = 0; i < n1024; i += 2) {
            mimir_free(b1024[i]);
        }
        quiesce();
        (void)mimir_core_trim_retained();
        check_eq_size(mimir_core_mapped_bytes(), large_sum,
                      "leg A cleanup: Q-MAPPED large-only");
        check_eq_size(mimir_core_retained_spans(), 0,
                      "leg A cleanup: no retained spans");
    }

    leg_b(64, cap64, 0x10, 24);
    check_eq_size(mimir_core_mapped_bytes(), large_sum,
                  "after leg B 64: Q-MAPPED large-only");
    leg_b(256, cap256, 0x40, 24);
    check_eq_size(mimir_core_mapped_bytes(), large_sum,
                  "after leg B 256: Q-MAPPED large-only");

    printf("== final reconciliation ==\n");
    mimir_free(large1);
    mimir_free(large2);
    quiesce();
    (void)mimir_core_trim_retained();
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "final Q-MAPPED 0");
    check_eq_size(mimir_core_retained_bytes(), 0,
                  "final Q-RETAINED 0");
    check(mimir_audit_span_published == mimir_audit_span_retired,
          "span counters reconcile (published == retired)");
    check(mimir_audit_small_alloc == mimir_audit_small_free,
          "object counters reconcile (alloc == free)");
    printf("METRIC totals span_published=%lu span_retired=%lu "
           "small_alloc=%lu small_free=%lu\n",
           mimir_audit_span_published, mimir_audit_span_retired,
           mimir_audit_small_alloc, mimir_audit_small_free);
    printf("checks=%d failures=%d\n", checks, failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
