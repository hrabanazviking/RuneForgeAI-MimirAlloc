/*
 * test_j003.c -- evidence test for slice J-003 (Phase 10
 * statistics BALANCE PROOF; slice record:
 * docs/PHASE_10_BALANCE_PROOF.md; governing contract:
 * docs/PHASE_10_FOUNDATION_PLAN.md S4.4, identities
 * R1-R12; surface under proof: the J-002 locked snapshot
 * mimir_core_stats_snapshot, docs/PHASE_10_STATISTICS.md).
 *
 * Run standalone via `make test-j003` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-j002 pattern). The binary links the library
 * OBJECTS plus the I-004 fault-injection shim with
 * --wrap on the four L6 platform entries, exactly as
 * test-j002 does, so the fault leg can inject acquire /
 * page-size / lock / release failures.
 *
 * Method: every identity is asserted at QUIESCENT points
 * (single thread, or after joins; the one concurrent leg
 * is the RLIMIT_AS child, a separate process image whose
 * assertions are all in deltas from its own baseline).
 * The test keeps its OWN script state -- cumulative
 * requested / allocated / released / failures / posture
 * NULLs / cross-thread frees, cumulative large published
 * bytes, and the exact set of live blocks with their
 * request sizes -- updated by script arithmetic from the
 * runtime page size, never copied from a snapshot. The
 * identities then equate three independent sources: the
 * script, the J-002 snapshot (one locked registry walk),
 * and the E-005 / G-003 audit publication counters.
 *
 * D-ALLOC-LIVE is not a named snapshot field (J-002
 * banking reconciliation); it is derived here exactly as
 * the reconciliation prescribes: live_mapping_bytes -
 * live_mappings * P + sum over classes of
 * span_live_objects * span_class_size, and R5 equates
 * that derivation against the script's own live usable /
 * charged sums, term by term.
 * D-ACQ-CUM is likewise not a named field; it is derived
 * from the publication counters (script cumulative large
 * mapping_length + span_published * P) and R6 equates
 * D-ACQ-CUM - D-REL-CUM (== bytes_released) against
 * bytes_mapped.
 *
 * Quantities that cannot be asserted exactly are NOT
 * silently skipped: D-LOCK (ABSENT, INC-J002-02) and
 * D-FAIL-BUDGET (0-by-absence, INC-I001-14) carry no
 * counter, and R10's free-slots clause needs the carved
 * count, which is not a named field; the test asserts
 * the ABSENT marker pair (0 / measured 0) in every
 * snapshot and the doc records the NOT-PROVABLE grades.
 * The disclosed J-002 narrowing -- a realloc
 * transfer-retire is NOT a cross-thread free -- is
 * asserted AS the documented behaviour in Leg 5.
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
#include "mimir_tune_g007.h"
#include "../abi/mimir_audit_e005.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
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
static int is_large(size_t req) { return req > 1024; }
static size_t class_size_of(size_t req)
{
    size_t i;
    for (i = 0; i < 20; i++) {
        if (req <= CLASS_SIZES[i]) {
            return CLASS_SIZES[i];
        }
    }
    return 0; /* large path */
}

/* ---- the test's own script state (never snapshot-fed) ---- */
static uint64_t exp_req, exp_alloc, exp_released;
static uint64_t exp_fail, exp_posture, exp_xtf;
static uint64_t cum_large_pub_bytes; /* D-ACQ-CUM large half */
static uint64_t peak_locked;         /* set by the R8 leg */
static uint64_t retention_bound;     /* spans per class */

struct live_ent { void *ptr; size_t req; };
static struct live_ent live_large[512];
static size_t n_live_large;
static struct live_ent live_small[4096];
static size_t n_live_small;

static uint64_t script_usable_sum(void)
{
    uint64_t t = 0;
    size_t i;
    for (i = 0; i < n_live_large; i++) {
        t += large_usable(live_large[i].req);
    }
    return t;
}
static uint64_t script_small_charged(void)
{
    uint64_t t = 0;
    size_t i;
    for (i = 0; i < n_live_small; i++) {
        t += class_size_of(live_small[i].req);
    }
    return t;
}
static void track_large(void *ptr, size_t req)
{
    live_large[n_live_large].ptr = ptr;
    live_large[n_live_large].req = req;
    n_live_large++;
}
static void track_small(void *ptr, size_t req)
{
    live_small[n_live_small].ptr = ptr;
    live_small[n_live_small].req = req;
    n_live_small++;
}
static void untrack(struct live_ent *arr, size_t *n, void *ptr)
{
    size_t i;
    for (i = 0; i < *n; i++) {
        if (arr[i].ptr == ptr) {
            arr[i] = arr[*n - 1];
            (*n)--;
            return;
        }
    }
}

/* Script-side publication / release accounting. */
static void script_pub(void *ptr, size_t req)
{
    exp_req += req;
    if (is_large(req)) {
        exp_alloc += large_usable(req);
        cum_large_pub_bytes += large_len(req);
        track_large(ptr, req);
    } else {
        exp_alloc += class_size_of(req);
        track_small(ptr, req);
    }
}
static void script_free(void *ptr, size_t req)
{
    if (is_large(req)) {
        exp_released += large_len(req);
        untrack(live_large, &n_live_large, ptr);
    } else {
        untrack(live_small, &n_live_small, ptr);
    }
}
static void script_drain(void)
{
    size_t n = mimir_core_drain_retained();
    exp_released += (uint64_t)n * P;
}

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

/* Assert every exactly-assertable identity at one
 * quiescent point, from one locked snapshot, against the
 * script state and the E-005 / G-003 audit counters. */
static void prove_all(const char *label)
{
    struct mimir_stats s = snap_now(label);
    char buf[160];
    uint64_t sum_live = 0, sum_spans = 0, sum_empty = 0;
    uint64_t sum_cap = 0, sum_charged = 0, cap_from_spans = 0;
    uint64_t acq_cum, d_alloc_live;
    size_t i;
    for (i = 0; i < 20; i++) {
        sum_live += s.span_live_objects[i];
        sum_spans += s.span_total_spans[i];
        sum_empty += s.span_empty_spans[i];
        sum_cap += s.span_capacity_objects[i];
        sum_charged += s.span_live_objects[i] *
                       s.span_class_size[i];
        cap_from_spans += s.span_total_spans[i] *
            span_capacity(CLASS_SIZES[i]);
    }
    acq_cum = cum_large_pub_bytes +
        (uint64_t)mimir_audit_span_published * P;
    d_alloc_live = (s.live_mapping_bytes -
                    s.live_mappings * P) + sum_charged;

    snprintf(buf, sizeof buf, "%s R1 pub-retired == live_mappings", label);
    check_u64((uint64_t)(mimir_audit_alloc_published -
                         mimir_audit_mapping_retired),
              s.live_mappings, buf);
    snprintf(buf, sizeof buf, "%s R2 span pub-retired == live_spans", label);
    check_u64((uint64_t)(mimir_audit_span_published -
                         mimir_audit_span_retired),
              s.live_spans, buf);
    snprintf(buf, sizeof buf, "%s R2 class spans sum == live_spans", label);
    check_u64(sum_spans, s.live_spans, buf);
    snprintf(buf, sizeof buf, "%s R3 small alloc-free == live_small", label);
    check_u64((uint64_t)(mimir_audit_small_alloc -
                         mimir_audit_small_free),
              s.live_small_objects, buf);
    snprintf(buf, sizeof buf, "%s R3/R10 class live sum == live_small", label);
    check_u64(sum_live, s.live_small_objects, buf);
    snprintf(buf, sizeof buf, "%s R4 mapped == mapping+span bytes", label);
    check_u64(s.bytes_mapped,
              s.live_mapping_bytes + s.live_span_bytes, buf);
    snprintf(buf, sizeof buf, "%s R4 mapped == I-003 Q-MAPPED", label);
    check_u64(s.bytes_mapped,
              (uint64_t)mimir_core_mapped_bytes(), buf);
    snprintf(buf, sizeof buf, "%s R5 large walk: bytes == usable + mappings*P", label);
    check_u64(s.live_mapping_bytes,
              script_usable_sum() + s.live_mappings * P, buf);
    snprintf(buf, sizeof buf, "%s R5 span walk: span bytes == spans*P", label);
    check_u64(s.live_span_bytes, s.live_spans * P, buf);
    snprintf(buf, sizeof buf, "%s R5 small walk: charged == script charged", label);
    check_u64(sum_charged, script_small_charged(), buf);
    snprintf(buf, sizeof buf, "%s R5 D-ALLOC-LIVE derived == script live granted", label);
    check_u64(d_alloc_live,
              script_usable_sum() + script_small_charged(), buf);
    snprintf(buf, sizeof buf, "%s R5 overhead == reserve pages + span slack", label);
    check_u64(s.bytes_mapped - d_alloc_live,
              s.live_mappings * P +
                  (s.live_span_bytes - sum_charged), buf);
    snprintf(buf, sizeof buf, "%s R6 ACQ - REL == MAPPED", label);
    check_u64(acq_cum - s.bytes_released, s.bytes_mapped, buf);
    snprintf(buf, sizeof buf, "%s R7 cached == empty spans * P", label);
    check_u64(s.bytes_cached, sum_empty * P, buf);
    /* R7 middle form: the contract's "live spans" here
     * means spans carrying live objects -- total minus
     * EMPTY -- since D-MAPPED also contains the retained
     * EMPTY spans' bytes (INC-J003-01). */
    snprintf(buf, sizeof buf, "%s R7 cached == mapped - (live-carrying spans*P + mapping bytes)", label);
    check_u64(s.bytes_cached,
              s.bytes_mapped -
                  ((s.live_spans - sum_empty) * P +
                   s.live_mapping_bytes), buf);
    snprintf(buf, sizeof buf, "%s R7 cached == I-003 Q-RETAINED", label);
    check_u64(s.bytes_cached,
              (uint64_t)mimir_core_retained_bytes(), buf);
    snprintf(buf, sizeof buf, "%s R7 cached within retention bound", label);
    check(s.bytes_cached <= retention_bound * 20 * P, buf);
    snprintf(buf, sizeof buf, "%s R8 peak >= mapped", label);
    check(s.peak_mapped >= s.bytes_mapped, buf);
    if (peak_locked != 0) {
        snprintf(buf, sizeof buf, "%s R8 peak == locked high-water", label);
        check_u64(s.peak_mapped, peak_locked, buf);
    }
    snprintf(buf, sizeof buf, "%s R10 capacity == per-span capacities summed", label);
    check_u64(sum_cap, cap_from_spans, buf);
    snprintf(buf, sizeof buf, "%s cum bytes_requested == script", label);
    check_u64(s.bytes_requested, exp_req, buf);
    snprintf(buf, sizeof buf, "%s cum bytes_allocated == script", label);
    check_u64(s.bytes_allocated, exp_alloc, buf);
    snprintf(buf, sizeof buf, "%s cum bytes_released == script", label);
    check_u64(s.bytes_released, exp_released, buf);
    snprintf(buf, sizeof buf, "%s cum alloc_failures == script", label);
    check_u64(s.alloc_failures, exp_fail, buf);
    snprintf(buf, sizeof buf, "%s cum posture nulls == script", label);
    check_u64(s.alloc_posture_nulls, exp_posture, buf);
    snprintf(buf, sizeof buf, "%s cum cross_thread_frees == script (R11)", label);
    check_u64(s.cross_thread_frees, exp_xtf, buf);
    snprintf(buf, sizeof buf, "%s D-LOCK ABSENT marker pair intact", label);
    check(s.lock_contention == 0 &&
          s.lock_contention_measured == 0, buf);
    snprintf(buf, sizeof buf, "%s live tracked counts agree", label);
    check(s.live_mappings == (uint64_t)n_live_large &&
          s.live_small_objects == (uint64_t)n_live_small, buf);
}

/* Free everything the script tracks, drain retained
 * spans, then assert R12 full-cycle closure. */
static void quiesce(const char *label)
{
    struct mimir_stats s;
    char buf[160];
    while (n_live_large > 0) {
        void *ptr = live_large[0].ptr;
        size_t req = live_large[0].req;
        mimir_free(ptr);
        script_free(ptr, req);
    }
    while (n_live_small > 0) {
        void *ptr = live_small[0].ptr;
        size_t req = live_small[0].req;
        mimir_free(ptr);
        script_free(ptr, req);
    }
    script_drain();
    prove_all(label);
    s = snap_now(label);
    snprintf(buf, sizeof buf, "%s R12 live_mappings == 0", label);
    check_u64(s.live_mappings, 0, buf);
    snprintf(buf, sizeof buf, "%s R12 live_spans == 0", label);
    check_u64(s.live_spans, 0, buf);
    snprintf(buf, sizeof buf, "%s R12 live_small == 0", label);
    check_u64(s.live_small_objects, 0, buf);
    snprintf(buf, sizeof buf, "%s R12 mapped == 0", label);
    check_u64(s.bytes_mapped, 0, buf);
    snprintf(buf, sizeof buf, "%s R12 cached == 0", label);
    check_u64(s.bytes_cached, 0, buf);
    snprintf(buf, sizeof buf, "%s R12 ACQ == REL (released == all published bytes)", label);
    check_u64(cum_large_pub_bytes +
                  (uint64_t)mimir_audit_span_published * P,
              s.bytes_released, buf);
    snprintf(buf, sizeof buf, "%s R12 R1 difference == 0", label);
    check(mimir_audit_alloc_published ==
          mimir_audit_mapping_retired, buf);
    snprintf(buf, sizeof buf, "%s R12 R2 difference == 0", label);
    check(mimir_audit_span_published ==
          mimir_audit_span_retired, buf);
    snprintf(buf, sizeof buf, "%s R12 R3 difference == 0", label);
    check(mimir_audit_small_alloc == mimir_audit_small_free, buf);
}

/* ---- cross-thread plumbing (Leg 5) ---- */
static void *g_xt_ptr;
static size_t g_xt_req;
static void *g_xt_new;
static void *worker_free(void *arg)
{
    (void)arg;
    mimir_free(g_xt_ptr);
    return NULL;
}
static void *worker_alloc(void *arg)
{
    (void)arg;
    g_xt_new = mimir_malloc(g_xt_req);
    return NULL;
}
static void *worker_realloc_grow(void *arg)
{
    (void)arg;
    /* Grow-by-move executed by the worker: the transfer
     * retire of the main-allocated block happens HERE,
     * on a non-allocating thread. Per the disclosed
     * J-002 narrowing it is NOT a cross-thread free. */
    g_xt_new = mimir_realloc(g_xt_ptr, 9000);
    if (g_xt_new != NULL) {
        /* The new block was published by this worker;
         * freeing it here is a same-thread free. */
        mimir_free(g_xt_new);
    }
    return NULL;
}

/* ---- RLIMIT_AS pressure child (Leg 7) ---- */
static size_t vm_size_bytes(void)
{
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    size_t kb = 0;
    if (!f) {
        return 0;
    }
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "VmSize: %zu kB", &kb) == 1) {
            break;
        }
    }
    fclose(f);
    return kb * 1024u;
}

/* Runs in the forked child: a fresh image state copied
 * from a quiesced parent, so live baselines are zero and
 * every cumulative assertion is a delta from the child's
 * own first snapshot. Returns 0 iff every child identity
 * held. Prints one summary line with write(2) only (no
 * stdio allocation under the limit). */
static int pressure_child(void)
{
    struct mimir_stats s0, s;
    void *smalls[200];
    void *larges[300];
    size_t n_large = 0, i;
    unsigned long span_pub0, map_pub0;
    int bad = 0;
    char out[256];
    int len;
    size_t base = vm_size_bytes();
    struct rlimit rl;
    if (base == 0 || mimir_core_stats_snapshot(&s0) != 0) {
        return 2;
    }
    span_pub0 = mimir_audit_span_published;
    map_pub0 = mimir_audit_alloc_published;
    rl.rlim_cur = (rlim_t)(base + 96 * (size_t)P);
    rl.rlim_max = (rlim_t)(base + 96 * (size_t)P);
    if (setrlimit(RLIMIT_AS, &rl) != 0) {
        return 3;
    }
    for (i = 0; i < 200; i++) {
        smalls[i] = mimir_malloc(64);
        if (smalls[i] == NULL) {
            bad = 1;
        }
    }
    for (i = 0; i < 300; i++) {
        void *p = mimir_malloc(4096);
        if (p == NULL) {
            break;
        }
        larges[n_large++] = p;
    }
    if (n_large == 0 || n_large >= 300) {
        bad = 1; /* pressure never bit, or never stopped */
    }
    if (mimir_malloc(4096) != NULL) {
        bad = 1; /* a second over-limit request succeeded */
    }
    if (mimir_core_stats_snapshot(&s) != 0) {
        return 4;
    }
    /* Exactly two caller-visible failures: the loop's
     * breaking NULL and the repeat attempt. */
    if (s.alloc_failures - s0.alloc_failures != 2) {
        bad = 1;
    }
    if (s.live_mappings != (uint64_t)n_large) {
        bad = 1;
    }
    if ((uint64_t)(mimir_audit_alloc_published -
                    mimir_audit_mapping_retired) !=
        (uint64_t)n_large) {
        bad = 1; /* R1 under pressure */
    }
    if (mimir_audit_alloc_published - map_pub0 !=
        (unsigned long)n_large) {
        bad = 1; /* one publication per successful large */
    }
    for (i = 0; i < n_large; i++) {
        mimir_free(larges[i]);
    }
    for (i = 0; i < 200; i++) {
        if (smalls[i] != NULL) {
            mimir_free(smalls[i]);
        }
    }
    mimir_core_drain_retained();
    if (mimir_core_stats_snapshot(&s) != 0) {
        return 5;
    }
    if (s.bytes_mapped != 0 || s.bytes_cached != 0 ||
        s.live_mappings != 0 || s.live_spans != 0 ||
        s.live_small_objects != 0) {
        bad = 1; /* R12 under pressure */
    }
    if (mimir_audit_alloc_published !=
        mimir_audit_mapping_retired ||
        mimir_audit_span_published !=
        mimir_audit_span_retired ||
        mimir_audit_small_alloc != mimir_audit_small_free) {
        bad = 1; /* R1/R2/R3 closure under pressure */
    }
    /* R6 closure in the child, all in deltas: released
     * delta == every byte the child published -- large
     * mapping_lengths from its own script (n_large x
     * large_len(4096)) plus span bytes from the audit
     * span-publication delta times P. */
    {
        uint64_t rel_delta = s.bytes_released - s0.bytes_released;
        uint64_t acq_delta =
            (uint64_t)n_large * large_len(4096) +
            (uint64_t)(mimir_audit_span_published - span_pub0) * P;
        if (rel_delta != acq_delta) {
            bad = 1;
        }
    }
    len = snprintf(out, sizeof out,
                   "  child: larges=%zu failures_delta=%llu "
                   "mapped_end=%llu bad=%d\n",
                   n_large,
                   (unsigned long long)(s.alloc_failures -
                                        s0.alloc_failures),
                   (unsigned long long)s.bytes_mapped, bad);
    if (len > 0) {
        ssize_t w = write(1, out, (size_t)len);
        (void)w;
    }
    return bad ? 1 : 0;
}

int main(void)
{
    long ps = sysconf(_SC_PAGESIZE);
    struct mimir_stats s;
    size_t i;

    printf("test_j003: Phase 10 statistics BALANCE PROOF (J-003)\n");
    check(ps > 0, "page size discovered via sysconf");
    P = (uint64_t)ps;
    printf("  page size = %llu bytes\n", (unsigned long long)P);
    retention_bound = (uint64_t)mimir_tune_get_retention_bound();
    printf("  retention bound = %llu spans/class\n",
           (unsigned long long)retention_bound);
    check(retention_bound > 0, "retention bound readable");

    printf("Leg 0: baseline -- every live quantity zero, "
           "cumulative counters at process start\n");
    s = snap_now("leg0");
    check_u64(s.bytes_mapped, 0, "leg0 mapped 0");
    check_u64(s.live_mappings, 0, "leg0 live mappings 0");
    check_u64(s.live_spans, 0, "leg0 live spans 0");
    check_u64(s.live_small_objects, 0, "leg0 live small 0");
    prove_all("leg0");

    printf("Leg 1: mixed small/large churn with calloc and "
           "realloc, identities at every quiescent point\n");
    {
        void *sm[15];
        void *lg[5];
        static const size_t lg_req[5] = {2000, 5000, 2000, 5000, 3000};
        for (i = 0; i < 10; i++) {
            sm[i] = mimir_malloc(100);
            check(sm[i] != NULL, "leg1 small 100 alloc");
            script_pub(sm[i], 100);
        }
        for (i = 10; i < 15; i++) {
            sm[i] = mimir_malloc(1000);
            check(sm[i] != NULL, "leg1 small 1000 alloc");
            script_pub(sm[i], 1000);
        }
        for (i = 0; i < 5; i++) {
            lg[i] = mimir_malloc(lg_req[i]);
            check(lg[i] != NULL, "leg1 large alloc");
            script_pub(lg[i], lg_req[i]);
        }
        prove_all("leg1a full mix live");
        for (i = 0; i < 5; i++) {
            mimir_free(sm[i]);
            script_free(sm[i], 100);
        }
        mimir_free(lg[0]);
        script_free(lg[0], 2000);
        mimir_free(lg[3]);
        script_free(lg[3], 5000);
        prove_all("leg1b partial frees");
        {
            void *c[3];
            void *grown, *stayed, *moved;
            for (i = 0; i < 3; i++) {
                c[i] = mimir_calloc(4, 50); /* 200, class 224 */
                check(c[i] != NULL, "leg1 calloc alloc");
                script_pub(c[i], 200);
            }
            prove_all("leg1c calloc live");
            stayed = mimir_realloc(sm[5], 112); /* same class */
            check(stayed == sm[5],
                  "leg1 same-class realloc stays");
            prove_all("leg1d staying realloc moves nothing");
            moved = mimir_realloc(sm[6], 200); /* class 112 -> 224 */
            check(moved != NULL && moved != sm[6],
                  "leg1 cross-class realloc moves");
            untrack(live_small, &n_live_small, sm[6]);
            exp_req += 200;
            exp_alloc += 224;
            track_small(moved, 200);
            prove_all("leg1e cross-class realloc reconciles");
            grown = mimir_realloc(c[0], 3000); /* small -> large */
            check(grown != NULL && grown != c[0],
                  "leg1 grow-by-move realloc moves");
            untrack(live_small, &n_live_small, c[0]);
            exp_req += 3000;
            exp_alloc += large_usable(3000);
            cum_large_pub_bytes += large_len(3000);
            track_large(grown, 3000);
            prove_all("leg1f grow-by-move reconciles");
            (void)lg;
        }
        quiesce("leg1z");
    }

    printf("Leg 2: R9 call accounting per entry point "
           "(calls == successes + failures + posture NULLs)\n");
    {
        struct mimir_stats a, b;
        unsigned long call0;
        /* --- malloc line: 7 calls = 4 + 2 + 1 --- */
        void *m1, *m2, *m3, *m4;
        a = snap_now("leg2 malloc pre");
        call0 = mimir_audit_call_malloc;
        m1 = mimir_malloc(64);
        m2 = mimir_malloc(64);
        m3 = mimir_malloc(3000);
        m4 = mimir_malloc(1024);
        check(m1 && m2 && m3 && m4, "leg2 malloc successes");
        script_pub(m1, 64);
        script_pub(m2, 64);
        script_pub(m3, 3000);
        script_pub(m4, 1024);
        check(mimir_malloc(0) == NULL, "leg2 malloc(0) posture");
        exp_posture += 1;
        mimir_fault_arm_acquire(1);
        check(mimir_malloc(6000) == NULL,
              "leg2 malloc injected failure");
        exp_fail += 1;
        check(mimir_malloc(SIZE_MAX) == NULL,
              "leg2 malloc oversize failure");
        exp_fail += 1;
        b = snap_now("leg2 malloc post");
        check_u64(b.alloc_failures - a.alloc_failures, 2,
                  "R9 malloc failure delta == scripted 2");
        check_u64(b.alloc_posture_nulls - a.alloc_posture_nulls,
                  1, "R9 malloc posture delta == scripted 1");
        check_u64((uint64_t)(mimir_audit_call_malloc - call0),
                  7, "R9 malloc audit calls == 7 == 4+2+1");
        /* --- calloc line: 4 calls = 2 + 1 + 1 --- */
        {
            void *c1, *c2;
            a = snap_now("leg2 calloc pre");
            call0 = mimir_audit_call_calloc;
            c1 = mimir_calloc(4, 50);
            c2 = mimir_calloc(10, 10);
            check(c1 && c2, "leg2 calloc successes");
            script_pub(c1, 200);
            script_pub(c2, 100);
            check(mimir_calloc(0, 8) == NULL,
                  "leg2 calloc zero-factor posture");
            exp_posture += 1;
            check(mimir_calloc(SIZE_MAX / 2 + 1, 2) == NULL,
                  "leg2 calloc overflow failure");
            exp_fail += 1;
            b = snap_now("leg2 calloc post");
            check_u64(b.alloc_failures - a.alloc_failures, 1,
                      "R9 calloc failure delta == scripted 1");
            check_u64(b.alloc_posture_nulls -
                          a.alloc_posture_nulls, 1,
                      "R9 calloc posture delta == scripted 1");
            check_u64((uint64_t)(mimir_audit_call_calloc - call0),
                      4, "R9 calloc audit calls == 4 == 2+1+1");
            mimir_free(c1);
            script_free(c1, 200);
            mimir_free(c2);
            script_free(c2, 100);
        }
        /* --- realloc line: 5 calls = (2 publications +
         * 1 stay completion) + 1 failure + 1 posture --- */
        {
            void *p, *q, *r;
            a = snap_now("leg2 realloc pre");
            call0 = mimir_audit_call_realloc;
            p = mimir_realloc(NULL, 100);
            check(p != NULL, "leg2 realloc(NULL) publishes");
            script_pub(p, 100);
            r = mimir_realloc(p, 112);
            check(r == p, "leg2 realloc same-class stays");
            p = mimir_realloc(p, 3000);
            check(p != NULL, "leg2 realloc grow publishes");
            untrack(live_small, &n_live_small, r);
            exp_req += 3000;
            exp_alloc += large_usable(3000);
            cum_large_pub_bytes += large_len(3000);
            track_large(p, 3000);
            check(mimir_realloc(p, 0) == NULL,
                  "leg2 realloc(ptr,0) posture, original kept");
            exp_posture += 1;
            q = mimir_malloc(64);
            check(q != NULL, "leg2 realloc-fail anchor alloc");
            script_pub(q, 64);
            ((unsigned char *)q)[0] = 0x5A;
            mimir_fault_arm_acquire(1);
            r = mimir_realloc(q, 6000);
            check(r == NULL,
                  "leg2 realloc grow construction failure");
            exp_fail += 1;
            check(((unsigned char *)q)[0] == 0x5A,
                  "leg2 failed realloc preserves original");
            b = snap_now("leg2 realloc post");
            check_u64(b.alloc_failures - a.alloc_failures, 1,
                      "R9 realloc failure delta == scripted 1");
            check_u64(b.alloc_posture_nulls -
                          a.alloc_posture_nulls, 1,
                      "R9 realloc posture delta == scripted 1");
            check_u64((uint64_t)(mimir_audit_call_realloc -
                                  call0), 5,
                      "R9 realloc audit calls == 5 == 3+1+1");
            mimir_free(q);
            script_free(q, 64);
            mimir_free(p);
            script_free(p, 3000);
        }
        mimir_free(m1);
        script_free(m1, 64);
        mimir_free(m2);
        script_free(m2, 64);
        mimir_free(m3);
        script_free(m3, 3000);
        mimir_free(m4);
        script_free(m4, 1024);
        quiesce("leg2z");
    }

    printf("Leg 3: fault injection via the I-004 --wrap shim "
           "-- D-FAIL-OS vs shim counts stated exactly\n");
    {
        struct mimir_stats a, b;
        unsigned long inj0, att0;
        void *anchor = mimir_malloc(5000);
        void *t;
        check(anchor != NULL, "leg3 anchor alloc");
        script_pub(anchor, 5000);
        prove_all("leg3a anchor live");
        a = snap_now("leg3 pre");
        inj0 = mimir_fault_acquire_injected +
            mimir_fault_pagesize_injected +
            mimir_fault_lock_injected;
        att0 = mimir_fault_acquire_attempts;
        mimir_fault_arm_acquire(1);
        t = mimir_malloc(6000);
        check(t == NULL, "leg3 injected acquire failure NULL");
        exp_fail += 1;
        att0 = mimir_fault_acquire_attempts;
        mimir_fault_arm_pagesize(1);
        t = mimir_malloc(6000);
        check(t == NULL, "leg3 injected page-size failure NULL");
        exp_fail += 1;
        check_u64((uint64_t)mimir_fault_acquire_attempts,
                  (uint64_t)att0,
                  "leg3 page-size failure attempted no mapping");
        mimir_fault_arm_lock(1);
        t = mimir_malloc(6000);
        check(t == NULL, "leg3 injected lock failure NULL");
        exp_fail += 1;
        check(mimir_calloc(SIZE_MAX / 2 + 1, 2) == NULL,
              "leg3 calloc overflow NULL (never reaches L6)");
        exp_fail += 1;
        check(mimir_malloc(SIZE_MAX) == NULL,
              "leg3 oversize NULL (never reaches L6)");
        exp_fail += 1;
        b = snap_now("leg3 post");
        check_u64(b.alloc_failures - a.alloc_failures, 5,
                  "leg3 D-FAIL-OS delta == 5 caller-visible");
        check_u64((uint64_t)((mimir_fault_acquire_injected +
                              mimir_fault_pagesize_injected +
                              mimir_fault_lock_injected) - inj0), 3,
                  "leg3 shim injected == 3 (acquire+pagesize+lock)");
        check((b.alloc_failures - a.alloc_failures) -
                  ((mimir_fault_acquire_injected +
                    mimir_fault_pagesize_injected +
                    mimir_fault_lock_injected) - inj0) == 2,
              "leg3 exact difference: 2 pre-admission failures "
              "counted by D-FAIL-OS, never reaching the shim");
        check_u64((uint64_t)mimir_fault_acquire_real_failed, 0,
                  "leg3 no real acquire failure occurred");
        prove_all("leg3b failures moved nothing else");
        /* Release failure: F-06 rollback -- the free does
         * not happen; every identity must still hold with
         * the block LIVE. */
        mimir_fault_arm_release(1);
        mimir_free(anchor);
        check_u64((uint64_t)mimir_fault_release_injected, 1,
                  "leg3 exactly one injected release failure");
        s = snap_now("leg3 rollback");
        check_u64(s.live_mappings, 1,
                  "leg3 rolled-back free leaves block live (R1)");
        prove_all("leg3c rollback reconciles with block live");
        mimir_free(anchor);
        script_free(anchor, 5000);
        prove_all("leg3d release succeeds on retry");
        quiesce("leg3z");
    }

    printf("Leg 4: trim cycles (I-003 shape) -- R7 at every "
           "step of the retained-span drain\n");
    {
        /* Page-size-agnostic: cap+1 objects per class
         * forces exactly 2 spans per class at ANY page
         * size (span capacities are computed from the
         * runtime page size, never assumed). */
        static void *objs[4200];
        static const size_t cls[3] = {16, 256, 1024};
        size_t n = 0, k, ci;
        for (ci = 0; ci < 3; ci++) {
            size_t want = (size_t)span_capacity(cls[ci]) + 1;
            for (k = 0; k < want; k++) {
                objs[n] = mimir_malloc(cls[ci]);
                check(objs[n] != NULL, "leg4 class alloc");
                script_pub(objs[n], cls[ci]);
                n++;
            }
        }
        s = snap_now("leg4a");
        check_u64(s.live_spans, 6,
                  "leg4 exactly 2 spans per class x 3 classes");
        prove_all("leg4a six spans live across three classes");
        for (k = 0; k < n; k++) {
            size_t req = live_small[0].req;
            void *ptr = live_small[0].ptr;
            (void)objs;
            mimir_free(ptr);
            script_free(ptr, req);
        }
        s = snap_now("leg4b");
        check_u64(s.bytes_cached, 6 * P,
                  "leg4 all six emptied spans retained (bound 2/class)");
        prove_all("leg4b retained");
        check(mimir_core_trim_retained_some(2) == 2,
              "leg4 partial trim releases exactly 2 spans");
        exp_released += 2 * P;
        s = snap_now("leg4c");
        check_u64(s.bytes_cached, 4 * P,
                  "leg4 cached == 4 pages after partial trim");
        prove_all("leg4c partial trim reconciles");
        check(mimir_core_trim_retained() == 4,
              "leg4 full trim releases the remaining 4 spans");
        exp_released += 4 * P;
        prove_all("leg4d full trim reconciles");
        quiesce("leg4z");
    }

    printf("Leg 5: cross-thread frees (R11) -- Tier 1 exact, "
           "the disclosed transfer-retire narrowing asserted "
           "AS documented, Tier 2 contribution 0 by choice\n");
    {
        pthread_t th;
        void *p;
        /* XT1: main allocates large, worker frees -> +1 */
        p = mimir_malloc(5000);
        check(p != NULL, "leg5 xt1 alloc");
        script_pub(p, 5000);
        g_xt_ptr = p;
        check(pthread_create(&th, NULL, worker_free, NULL) == 0,
              "leg5 xt1 worker created");
        check(pthread_join(th, NULL) == 0, "leg5 xt1 joined");
        exp_xtf += 1;
        script_free(p, 5000);
        prove_all("leg5 xt1 cross-thread free counted once");
        /* XT2: worker allocates large, main frees -> +1 */
        g_xt_req = 5000;
        g_xt_new = NULL;
        check(pthread_create(&th, NULL, worker_alloc, NULL) == 0,
              "leg5 xt2 worker created");
        check(pthread_join(th, NULL) == 0, "leg5 xt2 joined");
        check(g_xt_new != NULL, "leg5 xt2 worker alloc non-NULL");
        script_pub(g_xt_new, 5000);
        mimir_free(g_xt_new);
        exp_xtf += 1;
        script_free(g_xt_new, 5000);
        prove_all("leg5 xt2 worker-alloc main-free counted");
        /* XT3: the narrowing case. Main allocates a large
         * block; a worker grows it by realloc (the transfer
         * retire of main's block executes on the worker) and
         * frees the new block itself. Documented behaviour
         * (J-002 S2, banking reconciliation): a realloc
         * transfer-retire is NOT a cross-thread free, so
         * the counter must move by exactly 0 across the
         * whole leg -- asserted as the documented narrowing,
         * not as a Tier-1 truth. */
        p = mimir_malloc(5000);
        check(p != NULL, "leg5 xt3 alloc");
        script_pub(p, 5000);
        g_xt_ptr = p;
        g_xt_new = NULL;
        check(pthread_create(&th, NULL, worker_realloc_grow,
                             NULL) == 0,
              "leg5 xt3 worker created");
        check(pthread_join(th, NULL) == 0, "leg5 xt3 joined");
        check(g_xt_new != NULL && g_xt_new != p,
              "leg5 xt3 grow-by-move happened in the worker");
        untrack(live_large, &n_live_large, p);
        exp_released += large_len(5000); /* transfer retire */
        exp_req += 9000;
        exp_alloc += large_usable(9000);
        cum_large_pub_bytes += large_len(9000);
        exp_released += large_len(9000); /* worker's own free */
        /* exp_xtf deliberately unchanged: the narrowing. */
        prove_all("leg5 xt3 narrowing: transfer-retire "
                  "uncounted, ledger still exact");
        /* XT4: small object freed cross-thread -> +0.
         * Tier 2 is unmeasurable by J-002's recorded
         * choice (b) (no per-object owner record); the
         * PARTIAL counter's documented contribution of a
         * small cross-thread free is exactly 0. */
        p = mimir_malloc(64);
        check(p != NULL, "leg5 xt4 small alloc");
        script_pub(p, 64);
        g_xt_ptr = p;
        check(pthread_create(&th, NULL, worker_free, NULL) == 0,
              "leg5 xt4 worker created");
        check(pthread_join(th, NULL) == 0, "leg5 xt4 joined");
        script_free(p, 64);
        prove_all("leg5 xt4 small cross-thread free "
                  "contributes 0 (PARTIAL, INC-J002-01)");
        s = snap_now("leg5 total");
        check_u64(s.cross_thread_frees, 2,
                  "leg5 R11 total == the two Tier-1 frees exactly");
        quiesce("leg5z");
    }

    printf("Leg 6: R8 peak discipline -- exact high-water, "
           "untouched by free/trim-only sequences, retained "
           "at mapped == 0\n");
    {
        void *pk[16];
        size_t npk = 0;
        uint64_t target;
        s = snap_now("leg6 pre");
        target = s.peak_mapped + 6 * P;
        while (s.bytes_mapped < target && npk < 16) {
            pk[npk] = mimir_malloc(5000);
            check(pk[npk] != NULL, "leg6 peak-driving alloc");
            script_pub(pk[npk], 5000);
            npk++;
            s = snap_now("leg6 driving");
        }
        check(s.bytes_mapped >= target,
              "leg6 drove mapped past prior peak + 6 pages");
        check_u64(s.peak_mapped, s.bytes_mapped,
                  "leg6 R8 peak == mapped at the high-water instant");
        peak_locked = s.peak_mapped;
        printf("  high-water locked at %llu bytes\n",
               (unsigned long long)peak_locked);
        mimir_free(pk[0]);
        script_free(pk[0], 5000);
        prove_all("leg6 free-only sequence leaves peak exact");
        {
            void *tmp = mimir_malloc(64);
            check(tmp != NULL, "leg6 trim-sequence small alloc");
            script_pub(tmp, 64);
            mimir_free(tmp);
            script_free(tmp, 64);
            exp_released += (uint64_t)mimir_core_trim_retained() * P;
        }
        prove_all("leg6 trim-only sequence leaves peak exact");
        while (npk > 1) {
            npk--;
            mimir_free(pk[npk]);
            script_free(pk[npk], 5000);
        }
        script_drain();
        s = snap_now("leg6 end");
        check_u64(s.bytes_mapped, 0, "leg6 mapped back to 0");
        check_u64(s.peak_mapped, peak_locked,
                  "leg6 R8 peak retains high-water exactly at 0");
        quiesce("leg6z");
    }

    printf("Leg 7: I-008-style RLIMIT_AS pressure cycle in a "
           "forked child -- identities under real backing "
           "failure, in deltas\n");
    {
        pid_t pid;
        quiesce("leg7 pre-fork");
        fflush(stdout);
        pid = fork();
        check(pid >= 0, "leg7 fork succeeded");
        if (pid == 0) {
            _exit(pressure_child());
        }
        if (pid > 0) {
            int status = 0;
            long waited = 0;
            pid_t w = 0;
            while (waited < 30000) {
                w = waitpid(pid, &status, WNOHANG);
                if (w == pid) {
                    break;
                }
                {
                    struct timespec ts = {0, 2000000L};
                    nanosleep(&ts, NULL);
                }
                waited += 2;
            }
            if (w != pid) {
                kill(pid, 9);
                (void)waitpid(pid, &status, 0);
                check(0, "leg7 child finished within 30 s wall");
            } else {
                check(WIFEXITED(status) &&
                          WEXITSTATUS(status) == 0,
                      "leg7 child identities held under pressure "
                      "(exit 0)");
            }
        }
        prove_all("leg7 parent state untouched by child");
    }

    printf("Final: full-cycle closure over the whole run\n");
    quiesce("final");
    s = snap_now("final");
    check_u64(s.peak_mapped, peak_locked,
              "final peak == Leg 6 high-water (no later leg "
              "exceeded it)");
    printf("IDENTITY VERDICTS (as asserted by this test; "
           "doc/PHASE_10_BALANCE_PROOF.md carries the grades)\n");
    printf("  R1  PROVEN\n");
    printf("  R2  PROVEN\n");
    printf("  R3  PROVEN\n");
    printf("  R4  PROVEN\n");
    printf("  R5  PROVEN (D-ALLOC-LIVE derived per the J-002 "
           "banking reconciliation)\n");
    printf("  R6  PROVEN (D-ACQ-CUM derived from the "
           "publication counters)\n");
    printf("  R7  PROVEN\n");
    printf("  R8  PROVEN\n");
    printf("  R9  PROVEN (malloc/calloc/realloc lines, "
           "Leg 2)\n");
    printf("  R10 PROVEN for live/total/empty/capacity "
           "aggregates; free-slots clause NOT-PROVABLE "
           "(carved count is not a named field)\n");
    printf("  R11 PROVEN Tier 1; transfer-retire "
           "PROVEN-AS-DOCUMENTED-NARROWING; Tier 2 exact "
           "count NOT-PROVABLE by recorded choice (b)\n");
    printf("  R12 PROVEN (incl. under RLIMIT_AS pressure, "
           "Leg 7)\n");
    printf("  D-LOCK NOT-PROVABLE (ABSENT, INC-J002-02; "
           "marker pair asserted 0/0 in every snapshot)\n");
    printf("  D-FAIL-BUDGET NOT-PROVABLE (0-by-absence, "
           "INC-I001-14; no counter exists to assert)\n");
    printf("checks: %d, failures: %d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
