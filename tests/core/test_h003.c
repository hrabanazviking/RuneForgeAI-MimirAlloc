/*
 * test_h003.c -- evidence test for slice H-003 (Phase 8
 * cross-thread frees, verify-and-harden; slice record:
 * docs/PHASE_8_CROSS_THREAD_FREES_H003.md; governing plan:
 * docs/PHASE_8_FOUNDATION_PLAN_H002.md item S1 and
 * INC-H002-03, which reframed H-003 from "implement" to
 * "verify and harden" on the measured baseline of 67,500
 * cross-thread frees with 0 failures).
 *
 * Run standalone via `make test-h003` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-g003..test-h002 pattern).
 *
 * This binary links the library OBJECTS (LIBOBJS), so it
 * can read the E-005 audit counters
 * (tests/abi/mimir_audit_e005.h), the G-003 span counters
 * (tests/core/mimir_small_g003.h) and call the H-001 drain
 * entry point (tests/core/mimir_drain_h001.h). It proves,
 * at bounded scale (2, 4 and 8 threads; 8 is 2x
 * oversubscription on this 4-core host), over repeated
 * rounds:
 *
 * Phase A (cross-thread free, both paths): every thread
 * allocates N blocks whose sizes cycle over six small
 * classes (16..1024, span path) and three large sizes
 * (2048/4096/8192, mapping path), fills each with a seeded
 * byte pattern, then -- after a barrier -- each thread
 * verifies byte-for-byte and frees the blocks of the NEXT
 * thread (ring). The freeing thread re-derives the seed
 * from (owner, index) itself; the allocator is never asked
 * to vouch for its own blocks (H-002 Section 1, S7 method).
 *
 * Phase B (cross-thread calloc): blocks allocated with
 * mimir_calloc on one thread (small product 256 and large
 * product 4096), zero-verified by the owner, pattern-filled,
 * then verified and freed by the next thread.
 *
 * Phase C (cross-thread realloc): three shapes, each
 * realloc'd by a different thread than the allocator:
 *   C1 grow small -> large (256 -> 2048): must move; the
 *      reallocating thread verifies the preserved prefix,
 *      fills the new extent; the owner then verifies the
 *      full new pattern and frees it.
 *   C2 grow large -> large (2048 -> 8192): same shape.
 *   C3 stay within class (100 -> 112, class 112): the
 *      banked span rule returns the SAME pointer with no
 *      counter movement; verified explicitly.
 *
 * Phase D (retained spans + drain, H-001 interplay): after
 * every round above, live small objects and live mappings
 * must be exactly 0 while retained EMPTY spans remain; the
 * main thread then allocates one block in each of four
 * small classes, a single worker thread frees them all
 * cross-thread, and mimir_core_drain_retained() must
 * release exactly the spans live at that point, bringing
 * span published == retired.
 *
 * Reconciliation: after EVERY phase of every round the
 * audit-counter deltas must equal the exact expected counts
 * the test computes from the operations it performed
 * (E-005 discipline). At the end, cumulative counters must
 * match the cumulative expectation exactly: L1 call counts,
 * small alloc == small free, mapping published == retired,
 * and (after the final drain) span published == retired.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */

#define _POSIX_C_SOURCE 200809L

#include "mimir_alloc.h"
#include "mimir_small_g003.h"
#include "mimir_drain_h001.h"
#include "../abi/mimir_audit_e005.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* ---------------- shared state ---------------- */

#define MAX_THREADS 8
#define ROUNDS 3

/* Phase A: 234 = 26 full cycles of the 9-size ring:
 * 6 small + 3 large per cycle, per thread. */
#define A_N 234
#define A_CYCLE_SMALL 6
#define A_CYCLE_LARGE 3

/* Phase B: 24 per thread, alternating small/large. */
#define B_N 24

/* Phase C: 16 per thread for C1/C2 shapes (alternating),
 * plus C3_N stay-case blocks per thread. */
#define C_N 16
#define C3_N 16

static const size_t a_sizes[9] = {
    16, 64, 128, 256, 512, 1024, 2048, 4096, 8192
};

static void *g_ptrs[MAX_THREADS][A_N > B_N ? A_N : B_N];
static size_t g_sizes_arr[MAX_THREADS][A_N > B_N ? A_N : B_N];
static void *g_c_ptrs[MAX_THREADS][C_N];
static size_t g_c_sizes[MAX_THREADS][C_N];
static void *g_c3_ptrs[MAX_THREADS][C3_N];

static pthread_barrier_t g_bar;
static int g_nthreads;
static int g_round;

static long g_pat_fail[MAX_THREADS];
static long g_null_fail[MAX_THREADS];
static long g_zero_fail[MAX_THREADS];
static long g_stay_fail[MAX_THREADS];

static uint8_t pat_byte(int salt, int tid, int i)
{
    return (uint8_t)(0x5Au ^ (unsigned)(salt * 67) ^
                     (unsigned)(tid * 31) ^ (unsigned)(i * 7));
}

static void fill_block(void *ptr, size_t sz, int salt, int tid, int i)
{
    memset(ptr, pat_byte(salt, tid, i), sz);
}

/* Verify every byte; return 1 if intact, 0 on first mismatch. */
static int verify_block(const void *ptr, size_t sz, int salt,
                        int tid, int i)
{
    const uint8_t *b = (const uint8_t *)ptr;
    uint8_t want = pat_byte(salt, tid, i);
    size_t k;
    for (k = 0; k < sz; k++) {
        if (b[k] != want) {
            return 0;
        }
    }
    return 1;
}

/* ---------------- counter snapshots ---------------- */

struct counters {
    unsigned long call_malloc, call_calloc, call_realloc, call_free;
    unsigned long published, retired;
    unsigned long span_pub, span_ret;
    unsigned long small_alloc, small_free;
};

static struct counters snap(void)
{
    struct counters c;
    c.call_malloc = mimir_audit_call_malloc;
    c.call_calloc = mimir_audit_call_calloc;
    c.call_realloc = mimir_audit_call_realloc;
    c.call_free = mimir_audit_call_free;
    c.published = mimir_audit_alloc_published;
    c.retired = mimir_audit_mapping_retired;
    c.span_pub = mimir_audit_span_published;
    c.span_ret = mimir_audit_span_retired;
    c.small_alloc = mimir_audit_small_alloc;
    c.small_free = mimir_audit_small_free;
    return c;
}

static void check_delta(const char *phase, struct counters a,
                        struct counters b,
                        unsigned long d_malloc, unsigned long d_calloc,
                        unsigned long d_realloc, unsigned long d_free,
                        unsigned long d_pub, unsigned long d_ret,
                        unsigned long d_salloc, unsigned long d_sfree)
{
    char label[192];
    snprintf(label, sizeof(label),
             "%s: L1 deltas malloc=%lu calloc=%lu realloc=%lu free=%lu",
             phase, b.call_malloc - a.call_malloc,
             b.call_calloc - a.call_calloc,
             b.call_realloc - a.call_realloc,
             b.call_free - a.call_free);
    check(b.call_malloc - a.call_malloc == d_malloc &&
          b.call_calloc - a.call_calloc == d_calloc &&
          b.call_realloc - a.call_realloc == d_realloc &&
          b.call_free - a.call_free == d_free, label);
    snprintf(label, sizeof(label),
             "%s: mapping deltas published=%lu retired=%lu",
             phase, b.published - a.published,
             b.retired - a.retired);
    check(b.published - a.published == d_pub &&
          b.retired - a.retired == d_ret, label);
    snprintf(label, sizeof(label),
             "%s: small deltas alloc=%lu free=%lu",
             phase, b.small_alloc - a.small_alloc,
             b.small_free - a.small_free);
    check(b.small_alloc - a.small_alloc == d_salloc &&
          b.small_free - a.small_free == d_sfree, label);
}

/* ---------------- Phase A ---------------- */

static void *worker_a(void *p)
{
    int t = (int)(intptr_t)p;
    int i;
    int src = (t + 1) % g_nthreads;

    for (i = 0; i < A_N; i++) {
        size_t sz = a_sizes[i % 9];
        void *ptr = mimir_malloc(sz);
        g_ptrs[t][i] = ptr;
        g_sizes_arr[t][i] = sz;
        if (!ptr) {
            g_null_fail[t]++;
            continue;
        }
        fill_block(ptr, sz, 1 + g_round, t, i);
    }
    pthread_barrier_wait(&g_bar);
    /* Free the NEXT thread's blocks: verify, then free. */
    for (i = 0; i < A_N; i++) {
        void *ptr = g_ptrs[src][i];
        size_t sz = g_sizes_arr[src][i];
        if (!ptr) {
            continue;
        }
        if (!verify_block(ptr, sz, 1 + g_round, src, i)) {
            g_pat_fail[t]++;
        }
        mimir_free(ptr);
        g_ptrs[src][i] = NULL;
    }
    return NULL;
}

/* ---------------- Phase B ---------------- */

static void *worker_b(void *p)
{
    int t = (int)(intptr_t)p;
    int i;
    int src = (t + 1) % g_nthreads;

    for (i = 0; i < B_N; i++) {
        void *ptr;
        size_t sz;
        if (i % 2 == 0) {
            ptr = mimir_calloc(16, 16); /* 256, small */
            sz = 256;
        } else {
            ptr = mimir_calloc(256, 16); /* 4096, large */
            sz = 4096;
        }
        g_ptrs[t][i] = ptr;
        g_sizes_arr[t][i] = sz;
        if (!ptr) {
            g_null_fail[t]++;
            continue;
        }
        /* Owner verifies the calloc zero promise. */
        {
            const uint8_t *b = (const uint8_t *)ptr;
            size_t k;
            for (k = 0; k < sz; k++) {
                if (b[k] != 0) {
                    g_zero_fail[t]++;
                    break;
                }
            }
        }
        fill_block(ptr, sz, 11 + g_round, t, i);
    }
    pthread_barrier_wait(&g_bar);
    for (i = 0; i < B_N; i++) {
        void *ptr = g_ptrs[src][i];
        size_t sz = g_sizes_arr[src][i];
        if (!ptr) {
            continue;
        }
        if (!verify_block(ptr, sz, 11 + g_round, src, i)) {
            g_pat_fail[t]++;
        }
        mimir_free(ptr);
        g_ptrs[src][i] = NULL;
    }
    return NULL;
}

/* ---------------- Phase C ---------------- */

static void *worker_c(void *p)
{
    int t = (int)(intptr_t)p;
    int i;
    int src = (t + 1) % g_nthreads;

    /* Allocate: C1/C2 grow shapes alternate by index. */
    for (i = 0; i < C_N; i++) {
        size_t sz = (i % 2 == 0) ? 256 : 2048;
        void *ptr = mimir_malloc(sz);
        g_c_ptrs[t][i] = ptr;
        g_c_sizes[t][i] = sz;
        if (!ptr) {
            g_null_fail[t]++;
            continue;
        }
        fill_block(ptr, sz, 21 + g_round, t, i);
    }
    /* C3 stay-case blocks: requested 100 (class 112). */
    for (i = 0; i < C3_N; i++) {
        void *ptr = mimir_malloc(100);
        g_c3_ptrs[t][i] = ptr;
        if (!ptr) {
            g_null_fail[t]++;
            continue;
        }
        fill_block(ptr, 100, 31 + g_round, t, i);
    }
    pthread_barrier_wait(&g_bar);

    /* Realloc the NEXT thread's blocks cross-thread. */
    for (i = 0; i < C_N; i++) {
        void *ptr = g_c_ptrs[src][i];
        size_t oldsz = g_c_sizes[src][i];
        size_t newsz = (i % 2 == 0) ? 2048 : 8192;
        void *np;
        if (!ptr) {
            g_c_ptrs[src][i] = NULL;
            continue;
        }
        np = mimir_realloc(ptr, newsz);
        if (!np) {
            g_null_fail[t]++;
            g_c_ptrs[src][i] = ptr; /* original still owned */
            g_c_sizes[src][i] = oldsz;
            continue;
        }
        /* Preserved prefix: min(old,new) requested bytes;
         * both shapes grow, so the full old pattern must
         * survive at the head of the new block. */
        if (!verify_block(np, oldsz, 21 + g_round, src, i)) {
            g_pat_fail[t]++;
        }
        fill_block(np, newsz, 41 + g_round, src, i);
        g_c_ptrs[src][i] = np;
        g_c_sizes[src][i] = newsz;
    }
    /* C3: realloc next thread's 100 -> 112 stays in class
     * 112: same pointer, no counter movement, bytes kept. */
    for (i = 0; i < C3_N; i++) {
        void *ptr = g_c3_ptrs[src][i];
        void *np;
        if (!ptr) {
            continue;
        }
        np = mimir_realloc(ptr, 112);
        if (np != ptr) {
            g_stay_fail[t]++;
        }
        if (np && !verify_block(np, 100, 31 + g_round, src, i)) {
            g_pat_fail[t]++;
        }
        g_c3_ptrs[src][i] = np ? np : ptr;
    }
    pthread_barrier_wait(&g_bar);

    /* Owner verifies the full post-realloc pattern of its
     * own (cross-thread realloc'd) blocks and frees them:
     * every free here retires a block whose last writer was
     * a different thread. C3 blocks are freed by their
     * owner too (allocated by owner, realloc'd by next). */
    for (i = 0; i < C_N; i++) {
        void *ptr = g_c_ptrs[t][i];
        size_t sz = g_c_sizes[t][i];
        if (!ptr) {
            continue;
        }
        if (!verify_block(ptr, sz, 41 + g_round, t, i)) {
            g_pat_fail[t]++;
        }
        mimir_free(ptr);
        g_c_ptrs[t][i] = NULL;
    }
    for (i = 0; i < C3_N; i++) {
        void *ptr = g_c3_ptrs[t][i];
        if (!ptr) {
            continue;
        }
        mimir_free(ptr);
        g_c3_ptrs[t][i] = NULL;
    }
    return NULL;
}

/* ---------------- Phase D ---------------- */

static void *g_d_ptrs[4];
static const size_t d_sizes[4] = { 32, 256, 768, 1024 };

static void *worker_d_free(void *unused)
{
    int i;
    (void)unused;
    for (i = 0; i < 4; i++) {
        if (g_d_ptrs[i] &&
            !verify_block(g_d_ptrs[i], d_sizes[i], 51, 0, i)) {
            g_pat_fail[0]++;
        }
        mimir_free(g_d_ptrs[i]);
        g_d_ptrs[i] = NULL;
    }
    return NULL;
}

/* ---------------- driver ---------------- */

static void run_phase(void *(*worker)(void *), const char *name,
                      struct counters before,
                      unsigned long d_malloc, unsigned long d_calloc,
                      unsigned long d_realloc, unsigned long d_free,
                      unsigned long d_pub, unsigned long d_ret,
                      unsigned long d_salloc, unsigned long d_sfree)
{
    pthread_t th[MAX_THREADS];
    int t;
    char label[160];

    memset(g_pat_fail, 0, sizeof(g_pat_fail));
    memset(g_null_fail, 0, sizeof(g_null_fail));
    memset(g_zero_fail, 0, sizeof(g_zero_fail));
    memset(g_stay_fail, 0, sizeof(g_stay_fail));

    pthread_barrier_init(&g_bar, NULL, (unsigned)g_nthreads);
    for (t = 0; t < g_nthreads; t++) {
        pthread_create(&th[t], NULL, worker, (void *)(intptr_t)t);
    }
    for (t = 0; t < g_nthreads; t++) {
        pthread_join(th[t], NULL);
    }
    pthread_barrier_destroy(&g_bar);

    {
        long pat = 0, nullf = 0, zero = 0, stay = 0;
        struct counters after;
        for (t = 0; t < g_nthreads; t++) {
            pat += g_pat_fail[t];
            nullf += g_null_fail[t];
            zero += g_zero_fail[t];
            stay += g_stay_fail[t];
        }
        snprintf(label, sizeof(label),
                 "%s: pattern failures == 0 (saw %ld)", name, pat);
        check(pat == 0, label);
        snprintf(label, sizeof(label),
                 "%s: null failures == 0 (saw %ld)", name, nullf);
        check(nullf == 0, label);
        snprintf(label, sizeof(label),
                 "%s: zero failures == 0 (saw %ld)", name, zero);
        check(zero == 0, label);
        snprintf(label, sizeof(label),
                 "%s: stay-pointer failures == 0 (saw %ld)",
                 name, stay);
        check(stay == 0, label);
        after = snap();
        check_delta(name, before, after, d_malloc, d_calloc,
                    d_realloc, d_free, d_pub, d_ret,
                    d_salloc, d_sfree);
    }
}

int main(void)
{
    static const int configs[3] = { 2, 4, 8 };
    struct counters start, now;
    unsigned long exp_malloc = 0, exp_calloc = 0, exp_realloc = 0;
    unsigned long exp_free = 0, exp_pub = 0, exp_ret = 0;
    unsigned long exp_salloc = 0, exp_sfree = 0;
    unsigned long total_cross_frees = 0;
    int ci, r, t, i;

    printf("test_h003: Phase 8 cross-thread frees "
           "(verify-and-harden, H-003)\n");

    start = snap();
    check(start.small_alloc == start.small_free &&
          start.published == start.retired,
          "start: no live small objects or mappings");

    for (ci = 0; ci < 3; ci++) {
        g_nthreads = configs[ci];
        for (r = 0; r < ROUNDS; r++) {
            unsigned long T = (unsigned long)g_nthreads;
            struct counters before;
            char pname[96];

            g_round = r;
            printf("config threads=%d round=%d\n", g_nthreads, r);

            /* Phase A expectations: per thread A_N mallocs
             * (156 small, 78 large) and A_N frees. */
            before = snap();
            snprintf(pname, sizeof(pname),
                     "A t=%d r=%d", g_nthreads, r);
            run_phase(worker_a, pname, before,
                      T * A_N, 0, 0, T * A_N,
                      T * (A_N / 9) * A_CYCLE_LARGE,
                      T * (A_N / 9) * A_CYCLE_LARGE,
                      T * (A_N / 9) * A_CYCLE_SMALL,
                      T * (A_N / 9) * A_CYCLE_SMALL);
            exp_malloc += T * A_N;
            exp_free += T * A_N;
            exp_pub += T * (A_N / 9) * A_CYCLE_LARGE;
            exp_ret += T * (A_N / 9) * A_CYCLE_LARGE;
            exp_salloc += T * (A_N / 9) * A_CYCLE_SMALL;
            exp_sfree += T * (A_N / 9) * A_CYCLE_SMALL;
            total_cross_frees += T * A_N;

            /* Phase B expectations: per thread B_N callocs
             * (half small, half large) and B_N frees. */
            before = snap();
            snprintf(pname, sizeof(pname),
                     "B t=%d r=%d", g_nthreads, r);
            run_phase(worker_b, pname, before,
                      0, T * B_N, 0, T * B_N,
                      T * (B_N / 2), T * (B_N / 2),
                      T * (B_N / 2), T * (B_N / 2));
            exp_calloc += T * B_N;
            exp_free += T * B_N;
            exp_pub += T * (B_N / 2);
            exp_ret += T * (B_N / 2);
            exp_salloc += T * (B_N / 2);
            exp_sfree += T * (B_N / 2);
            total_cross_frees += T * B_N;

            /* Phase C expectations per thread:
             * mallocs: C_N grow blocks + C3_N stay blocks.
             * reallocs: C_N grows + C3_N stays.
             * Grow accounting: the initial C2 mallocs publish
             * one large mapping each; each grow publishes one
             * new large mapping; C1 retires one small object,
             * C2 retires one large mapping; the final free
             * retires one large mapping per grow block.
             * Per thread C_N/2 of each shape.
             * frees: C_N grow blocks + C3_N stay blocks.
             * Stay blocks: small alloc at malloc, small free
             * at free, realloc moves nothing. */
            before = snap();
            snprintf(pname, sizeof(pname),
                     "C t=%d r=%d", g_nthreads, r);
            run_phase(worker_c, pname, before,
                      T * (C_N + C3_N), 0, T * (C_N + C3_N),
                      T * (C_N + C3_N),
                      /* published: initial large mallocs
                       * plus one per grow */
                      T * (C_N / 2) + T * C_N,
                      /* retired: C2 old mappings + all grow
                       * blocks at final free */
                      T * (C_N / 2) + T * C_N,
                      /* small alloc: C1 blocks + stay blocks */
                      T * (C_N / 2) + T * C3_N,
                      /* small free: C1 olds + stay blocks */
                      T * (C_N / 2) + T * C3_N);
            exp_malloc += T * (C_N + C3_N);
            exp_realloc += T * (C_N + C3_N);
            exp_free += T * (C_N + C3_N);
            exp_pub += T * (C_N / 2) + T * C_N;
            exp_ret += T * (C_N / 2) + T * C_N;
            exp_salloc += T * (C_N / 2) + T * C3_N;
            exp_sfree += T * (C_N / 2) + T * C3_N;
            total_cross_frees += T * (C_N + C3_N);
            (void)t;
            (void)i;
        }
    }

    /* After all rounds: nothing live but retained spans. */
    now = snap();
    check(now.small_alloc - now.small_free == 0,
          "final: live small objects == 0");
    check(now.published - now.retired == 0,
          "final: live mappings == 0");
    check(now.call_malloc - start.call_malloc == exp_malloc &&
          now.call_calloc - start.call_calloc == exp_calloc &&
          now.call_realloc - start.call_realloc == exp_realloc &&
          now.call_free - start.call_free == exp_free,
          "final: cumulative L1 call counters reconcile exactly");
    check(now.published - start.published == exp_pub &&
          now.retired - start.retired == exp_ret &&
          now.small_alloc - start.small_alloc == exp_salloc &&
          now.small_free - start.small_free == exp_sfree,
          "final: cumulative core counters reconcile exactly");
    printf("  totals: cross-thread-phase frees=%lu "
           "malloc=%lu calloc=%lu realloc=%lu free=%lu\n",
           total_cross_frees, exp_malloc, exp_calloc,
           exp_realloc, exp_free);

    /* Phase D: retained spans + cross-thread free + drain. */
    printf("phase D: retained spans, cross-thread free, drain\n");
    {
        pthread_t th;
        struct counters d_before, d_mid, d_after;
        unsigned long spans_live;
        size_t drained;
        int ok = 1;

        memset(g_pat_fail, 0, sizeof(g_pat_fail));
        d_before = snap();
        for (i = 0; i < 4; i++) {
            g_d_ptrs[i] = mimir_malloc(d_sizes[i]);
            if (!g_d_ptrs[i]) {
                ok = 0;
                continue;
            }
            fill_block(g_d_ptrs[i], d_sizes[i], 51, 0, i);
        }
        check(ok, "D: main-thread allocations non-null");
        exp_malloc += 4;
        exp_free += 4;
        exp_salloc += 4;
        exp_sfree += 4;

        pthread_create(&th, NULL, worker_d_free, NULL);
        pthread_join(th, NULL);
        check(g_pat_fail[0] == 0,
              "D: worker verified all 4 patterns before free");

        d_mid = snap();
        check(d_mid.small_alloc - d_mid.small_free == 0 &&
              d_mid.published - d_mid.retired == 0,
              "D: nothing live after cross-thread frees");
        spans_live = d_mid.span_pub - d_mid.span_ret;
        printf("  D: retained empty spans live before drain: "
               "%lu\n", spans_live);
        check(spans_live > 0,
              "D: at least one retained empty span exists");
        drained = mimir_core_drain_retained();
        printf("  D: drain released %zu spans\n", drained);
        check(drained == (size_t)spans_live,
              "D: drain released exactly the retained spans");
        d_after = snap();
        check(d_after.span_pub == d_after.span_ret,
              "D: span published == retired after drain");
        check(d_after.small_alloc - d_before.small_alloc == 4 &&
              d_after.small_free - d_before.small_free == 4,
              "D: small counter deltas == 4 alloc / 4 free");
        check(mimir_core_drain_retained() == 0,
              "D: second drain is a no-op (idempotent)");
        /* Allocator still usable after the drain. */
        {
            void *p = mimir_malloc(64);
            check(p != NULL, "D: allocator usable after drain");
            if (p) {
                fill_block(p, 64, 61, 0, 0);
                check(verify_block(p, 64, 61, 0, 0),
                      "D: post-drain block pattern intact");
                mimir_free(p);
                exp_malloc += 1;
                exp_free += 1;
                exp_salloc += 1;
                exp_sfree += 1;
            }
        }
        now = snap();
        check(now.call_malloc - start.call_malloc == exp_malloc &&
              now.call_free - start.call_free == exp_free &&
              now.small_alloc - start.small_alloc == exp_salloc &&
              now.small_free - start.small_free == exp_sfree,
              "D: cumulative counters still reconcile exactly");
    }

    printf("checks=%d failures=%d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
