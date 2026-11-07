/*
 * test_g006.c -- evidence test for slice G-006 (Phase 7
 * thread exit handling; slice record:
 * docs/PHASE_7_THREAD_EXIT_G006.md).
 *
 * Run standalone via `make test-g006` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-g005 pattern). It verifies the thread-exit posture
 * that INC-G005-04 left to this slice: the core holds NO
 * per-thread state (no _Thread_local, no __thread, no
 * pthread-key state anywhere in src/ -- proven by source
 * inspection in the recorded run, not by this program), so
 * thread exit has nothing to drain, and an allocation's
 * lifetime is bound to the PROCESS registries, never to
 * the allocating thread. What this program proves BY
 * EXECUTION, through the four public symbols plus the
 * E-005 mapping counters and the G-003 span/object
 * counters (INC-E001-07 shape (b): readable only by
 * linking the library OBJECTS directly):
 *
 *   T1 (task case a) exit-while-holding: 4 threads each
 *      allocate small (span-path) and large (mapping-path)
 *      blocks, fill them with patterns, and EXIT still
 *      holding them. After join, the blocks are still
 *      counted live by exactly the allocated amounts, the
 *      patterns are intact, and the MAIN thread frees
 *      every block through the two-registry free path
 *      (G-002 R-4). Counters then reconcile exactly to
 *      the pre-T1 baseline.
 *   T2 (task case b) exit while others work: 3 churner
 *      threads allocate/touch/free continuously while 3
 *      holder threads allocate, fill and exit holding
 *      blocks. No churner observes a failure, the held
 *      blocks verify after the holders have exited, the
 *      main thread frees them, and the counters reconcile
 *      exactly once every thread has joined.
 *   T3 (task case c) many short-lived threads: 48 thread
 *      lifecycles in 6 batches of 8 concurrent threads;
 *      each thread allocates small + large blocks, touches
 *      them, frees ALL of them itself and exits. After
 *      every batch the live counters are exactly back at
 *      baseline and live spans stay within the RP-5 bound
 *      (40). Bounded on purpose: the Pi is small and
 *      carries Samtal; at most 8 threads are ever live.
 *   T4 (task case d) pthread_exit vs return: the design
 *      registers no thread-exit hook of any kind, so it
 *      cannot distinguish the two exit forms; the test
 *      proves the observable equivalence -- a holder that
 *      returns and a holder that calls pthread_exit leave
 *      IDENTICAL counter deltas, identical pattern
 *      survival, and both sets of blocks free cleanly
 *      from the main thread with exact reconciliation.
 *   T5 final accounting: live small objects 0, live
 *      mappings 0, live spans within the RP-5 bound, and
 *      a fresh small + large allocation pair still works
 *      after all the exits (allocator healthy).
 *
 * Counter discipline: audit counters are read only on
 * the main thread, and for pass/fail only at quiescent
 * points (after every thread of the phase has been
 * joined -- pthread_join is the synchronisation edge).
 * No counter is sampled mid-flight for a verdict. Span
 * retention is the G-005 global per-class bound (RP-2 /
 * RP-5): freed spans may stay mapped as retained empty
 * spans, so span checks are bound checks (<= 40), while
 * small-object and mapping checks are EXACT (delta == 0
 * at reconciliation).
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */

#define _POSIX_C_SOURCE 200809L

#include "mimir_alloc.h"
#include "mimir_small_g003.h"
#include "../abi/mimir_audit_e005.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

static void fill_pattern(unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)((i * 31u + seed * 17u + (i >> 8)) & 0xffu);
    }
}

static int pattern_ok(const unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        unsigned char want =
            (unsigned char)((i * 31u + seed * 17u + (i >> 8)) & 0xffu);
        if (p[i] != want) {
            return 0;
        }
    }
    return 1;
}

static unsigned long live_spans(void)
{
    return mimir_audit_span_published - mimir_audit_span_retired;
}

static unsigned long live_small(void)
{
    return mimir_audit_small_alloc - mimir_audit_small_free;
}

static unsigned long live_mappings(void)
{
    return mimir_audit_alloc_published - mimir_audit_mapping_retired;
}

/* ---- generic holder thread (T1, T2, T4) ----
 * Allocates n_small blocks of small_size and n_large
 * blocks of large_size, fills each with a pattern seeded
 * by (seed, index), stores the pointers in caller-owned
 * arrays, then EXITS STILL HOLDING every block. The
 * blocks are harness-owned after the exit; the allocating
 * thread never touches them again (it no longer exists).
 */
struct hold_req {
    int n_small;
    size_t small_size;
    int n_large;
    size_t large_size;
    unsigned int seed;
    void **small_out;
    void **large_out;
    int alloc_fail;
    int use_pthread_exit;
};

static void *holder_thread(void *arg)
{
    struct hold_req *r = (struct hold_req *)arg;
    int i;

    r->alloc_fail = 0;
    for (i = 0; i < r->n_small; i++) {
        r->small_out[i] = mimir_malloc(r->small_size);
        if (r->small_out[i] == NULL) {
            r->alloc_fail++;
        } else {
            fill_pattern((unsigned char *)r->small_out[i],
                         r->small_size, r->seed + (unsigned int)i);
        }
    }
    for (i = 0; i < r->n_large; i++) {
        r->large_out[i] = mimir_malloc(r->large_size);
        if (r->large_out[i] == NULL) {
            r->alloc_fail++;
        } else {
            fill_pattern((unsigned char *)r->large_out[i],
                         r->large_size,
                         r->seed + 100u + (unsigned int)i);
        }
    }
    if (r->use_pthread_exit) {
        pthread_exit(NULL);
    }
    return NULL;
}

/* Verify every held block's pattern; returns the number
 * of blocks whose pattern is wrong. */
static int verify_held(const struct hold_req *r)
{
    int bad = 0;
    int i;

    for (i = 0; i < r->n_small; i++) {
        if (r->small_out[i] == NULL ||
            !pattern_ok((const unsigned char *)r->small_out[i],
                        r->small_size, r->seed + (unsigned int)i)) {
            bad++;
        }
    }
    for (i = 0; i < r->n_large; i++) {
        if (r->large_out[i] == NULL ||
            !pattern_ok((const unsigned char *)r->large_out[i],
                        r->large_size,
                        r->seed + 100u + (unsigned int)i)) {
            bad++;
        }
    }
    return bad;
}

static void free_held(const struct hold_req *r)
{
    int i;

    for (i = 0; i < r->n_small; i++) {
        mimir_free(r->small_out[i]);
    }
    for (i = 0; i < r->n_large; i++) {
        mimir_free(r->large_out[i]);
    }
}

/* ---- churner thread (T2): self-contained alloc/free -- */
struct churn_req {
    int iters;
    unsigned int seed;
    int fail;
};

static void *churner_thread(void *arg)
{
    struct churn_req *r = (struct churn_req *)arg;
    int i;

    r->fail = 0;
    for (i = 0; i < r->iters; i++) {
        size_t sz = (i % 2 == 0) ? (size_t)128 : (size_t)4096;
        unsigned int seed = r->seed + (unsigned int)i;
        void *p = mimir_malloc(sz);

        if (p == NULL) {
            r->fail++;
            continue;
        }
        fill_pattern((unsigned char *)p, sz, seed);
        if (!pattern_ok((const unsigned char *)p, sz, seed)) {
            r->fail++;
        }
        mimir_free(p);
    }
    return NULL;
}

/* ---- short-lived thread (T3): alloc, touch, free all -- */
struct short_req {
    unsigned int seed;
    int fail;
};

static void *short_thread(void *arg)
{
    static const size_t sizes[3] = {32, 96, 512};
    struct short_req *r = (struct short_req *)arg;
    void *small[20];
    void *large[2];
    int i;

    r->fail = 0;
    for (i = 0; i < 20; i++) {
        size_t sz = sizes[i % 3];
        small[i] = mimir_malloc(sz);
        if (small[i] == NULL) {
            r->fail++;
        } else {
            fill_pattern((unsigned char *)small[i], sz,
                         r->seed + (unsigned int)i);
        }
    }
    for (i = 0; i < 2; i++) {
        large[i] = mimir_malloc(2048);
        if (large[i] == NULL) {
            r->fail++;
        } else {
            fill_pattern((unsigned char *)large[i], 2048,
                         r->seed + 50u + (unsigned int)i);
        }
    }
    for (i = 0; i < 20; i++) {
        size_t sz = sizes[i % 3];
        if (small[i] != NULL &&
            !pattern_ok((const unsigned char *)small[i], sz,
                        r->seed + (unsigned int)i)) {
            r->fail++;
        }
    }
    for (i = 0; i < 2; i++) {
        if (large[i] != NULL &&
            !pattern_ok((const unsigned char *)large[i], 2048,
                        r->seed + 50u + (unsigned int)i)) {
            r->fail++;
        }
    }
    for (i = 0; i < 20; i++) {
        mimir_free(small[i]);
    }
    for (i = 0; i < 2; i++) {
        mimir_free(large[i]);
    }
    return NULL;
}

int main(void)
{
    long page = sysconf(_SC_PAGESIZE);
    unsigned long base_small, base_map;

    printf("G-006 thread-exit handling test\n");
    printf("page_size=%ld (runtime, via sysconf)\n", page);
    if (page <= 0) {
        printf("RESULT: FAIL (no page size)\n");
        return 1;
    }

    /* Warm-up: one LARGE alloc/touch/free (mapping path;
     * leaves no span state behind, G-005 method). */
    {
        void *w = mimir_malloc(2048);
        if (w != NULL) {
            memset(w, 0x5A, 2048);
            mimir_free(w);
        }
    }
    base_small = live_small();
    base_map = live_mappings();
    printf("baseline: live_small=%lu live_mappings=%lu "
           "live_spans=%lu\n",
           base_small, base_map, live_spans());
    check(base_small == 0, "baseline live small objects == 0");
    check(base_map == 0, "baseline live mappings == 0");

    /* ---------- T1: exit while holding (case a) ---------- */
    printf("T1 exit-while-holding (4 threads: 2x 32-of-64 + "
           "2x 16-of-256 small, each 4x2048 large, exit holding all)\n");
    {
        enum { NT = 4, NS64 = 32, NS256 = 16, NL = 4 };
        static void *s64[NT][NS64];
        static void *s256[NT][NS256];
        static void *lg[NT][NL];
        static struct hold_req rq64[NT];
        pthread_t th[NT];
        int created = 0;
        int t;
        char label[192];

        /* One holder invocation per thread (a thread exits
         * once): threads 0-1 hold the size-64 shape,
         * threads 2-3 the size-256 shape; every thread also
         * holds large blocks. The aggregate live deltas
         * below are computed from these actual shapes. */
        for (t = 0; t < NT; t++) {
            if (t < 2) {
                rq64[t].n_small = NS64;
                rq64[t].small_size = 64;
                rq64[t].seed = 10u + (unsigned int)t * 40u;
                rq64[t].small_out = s64[t];
            } else {
                rq64[t].n_small = NS256;
                rq64[t].small_size = 256;
                rq64[t].seed = 10u + (unsigned int)t * 40u;
                rq64[t].small_out = s256[t];
            }
            rq64[t].n_large = NL;
            rq64[t].large_size = 2048;
            rq64[t].large_out = lg[t];
            rq64[t].use_pthread_exit = 0;
            if (pthread_create(&th[t], NULL, holder_thread,
                               &rq64[t]) == 0) {
                created++;
            }
        }
        check(created == NT, "T1 all 4 holder threads created");
        for (t = 0; t < created; t++) {
            pthread_join(th[t], NULL);
        }
        /* Aggregate expectations: small = 2*32 + 2*16 = 96,
         * large = 4*4 = 16. */
        for (t = 0; t < NT; t++) {
            snprintf(label, sizeof(label),
                     "T1 thread %d: zero allocation failures "
                     "(observed %d)", t, rq64[t].alloc_fail);
            check(rq64[t].alloc_fail == 0, label);
        }
        snprintf(label, sizeof(label),
                 "T1 after exit: live_small delta == 96 "
                 "(observed %lu), live_mappings delta == 16 "
                 "(observed %lu)",
                 live_small() - base_small, live_mappings() - base_map);
        check(live_small() - base_small == 96 &&
              live_mappings() - base_map == 16, label);
        for (t = 0; t < NT; t++) {
            snprintf(label, sizeof(label),
                     "T1 thread %d: all held patterns intact "
                     "after its exit", t);
            check(verify_held(&rq64[t]) == 0, label);
        }
        for (t = 0; t < NT; t++) {
            free_held(&rq64[t]);
        }
        snprintf(label, sizeof(label),
                 "T1 freed from main: live_small == baseline "
                 "(observed %lu), live_mappings == baseline "
                 "(observed %lu)",
                 live_small(), live_mappings());
        check(live_small() == base_small &&
              live_mappings() == base_map, label);
        snprintf(label, sizeof(label),
                 "T1 live spans within RP-5 bound: %lu <= 40",
                 live_spans());
        check(live_spans() <= 40, label);
    }

    /* ---------- T2: exit while others work (case b) ----- */
    printf("T2 exit-while-others-work (3 churners x 200 "
           "iterations concurrent with 3 exiting holders)\n");
    {
        enum { NC = 3, NH = 3, NS = 16, NL = 2 };
        static void *sm[NH][NS];
        static void *lg[NH][NL];
        static struct hold_req hr[NH];
        static struct churn_req cr[NC];
        pthread_t cth[NC], hth[NH];
        int cc = 0, hc = 0;
        int i;
        char label[192];

        for (i = 0; i < NC; i++) {
            cr[i].iters = 200;
            cr[i].seed = 500u + (unsigned int)i * 300u;
            if (pthread_create(&cth[i], NULL, churner_thread,
                               &cr[i]) == 0) {
                cc++;
            }
        }
        check(cc == NC, "T2 all churner threads created");
        for (i = 0; i < NH; i++) {
            hr[i].n_small = NS;
            hr[i].small_size = 64;
            hr[i].small_out = sm[i];
            hr[i].n_large = NL;
            hr[i].large_size = 2048;
            hr[i].large_out = lg[i];
            hr[i].seed = 900u + (unsigned int)i * 40u;
            hr[i].use_pthread_exit = 0;
            if (pthread_create(&hth[i], NULL, holder_thread,
                               &hr[i]) == 0) {
                hc++;
            }
        }
        check(hc == NH, "T2 all holder threads created");
        for (i = 0; i < hc; i++) {
            pthread_join(hth[i], NULL);
        }
        for (i = 0; i < cc; i++) {
            pthread_join(cth[i], NULL);
        }
        for (i = 0; i < NC; i++) {
            snprintf(label, sizeof(label),
                     "T2 churner %d: zero failures over %d "
                     "iterations (observed %d)",
                     i, cr[i].iters, cr[i].fail);
            check(cr[i].fail == 0, label);
        }
        for (i = 0; i < NH; i++) {
            snprintf(label, sizeof(label),
                     "T2 holder %d: zero alloc failures, held "
                     "patterns intact after exit", i);
            check(hr[i].alloc_fail == 0 && verify_held(&hr[i]) == 0,
                  label);
        }
        snprintf(label, sizeof(label),
                 "T2 while held: live_small delta == %d "
                 "(observed %lu), live_mappings delta == %d "
                 "(observed %lu)",
                 NH * NS, live_small() - base_small,
                 NH * NL, live_mappings() - base_map);
        check(live_small() - base_small == (unsigned long)(NH * NS) &&
              live_mappings() - base_map == (unsigned long)(NH * NL),
              label);
        for (i = 0; i < NH; i++) {
            free_held(&hr[i]);
        }
        check(live_small() == base_small &&
              live_mappings() == base_map,
              "T2 counters reconcile exactly after main frees");
    }

    /* ---------- T3: many short-lived threads (case c) --- */
    printf("T3 short-lived churn (48 lifecycles, 6 batches "
           "of 8 concurrent, each 20 small + 2 large, all "
           "freed by the thread itself before exit)\n");
    {
        enum { BATCHES = 6, PER = 8 };
        int b, i;
        int total_fail = 0;
        char label[192];

        for (b = 0; b < BATCHES; b++) {
            static struct short_req sr[PER];
            pthread_t th[PER];
            int created = 0;

            for (i = 0; i < PER; i++) {
                sr[i].seed = 2000u + (unsigned int)(b * PER + i) * 40u;
                sr[i].fail = 0;
                if (pthread_create(&th[i], NULL, short_thread,
                                   &sr[i]) == 0) {
                    created++;
                }
            }
            for (i = 0; i < created; i++) {
                pthread_join(th[i], NULL);
            }
            for (i = 0; i < PER; i++) {
                total_fail += sr[i].fail;
            }
            snprintf(label, sizeof(label),
                     "T3 batch %d: %d threads ran, counters "
                     "reconciled (live_small %lu, live_mappings "
                     "%lu), spans %lu <= 40",
                     b, created, live_small(), live_mappings(),
                     live_spans());
            check(created == PER && live_small() == base_small &&
                  live_mappings() == base_map && live_spans() <= 40,
                  label);
        }
        snprintf(label, sizeof(label),
                 "T3 total per-thread failures == 0 (observed %d)",
                 total_fail);
        check(total_fail == 0, label);
    }

    /* ---------- T4: pthread_exit vs return (case d) ----- */
    printf("T4 pthread_exit vs return equivalence (identical "
           "holder shape, 8x64 small + 2x2048 large each)\n");
    {
        enum { NS = 8, NL = 2 };
        static void *sm_a[NS], *lg_a[NL], *sm_b[NS], *lg_b[NL];
        struct hold_req a, b;
        unsigned long d_small_a, d_map_a, d_small_b, d_map_b;
        pthread_t th;
        char label[192];

        memset(&a, 0, sizeof(a));
        a.n_small = NS; a.small_size = 64; a.small_out = sm_a;
        a.n_large = NL; a.large_size = 2048; a.large_out = lg_a;
        a.seed = 5000u; a.use_pthread_exit = 0;
        check(pthread_create(&th, NULL, holder_thread, &a) == 0,
              "T4 return-form holder created");
        pthread_join(th, NULL);
        d_small_a = live_small() - base_small;
        d_map_a = live_mappings() - base_map;
        check(a.alloc_fail == 0 && verify_held(&a) == 0,
              "T4 return-form: allocs ok, patterns intact");
        free_held(&a);
        check(live_small() == base_small && live_mappings() == base_map,
              "T4 return-form: reconciles after main free");

        memset(&b, 0, sizeof(b));
        b.n_small = NS; b.small_size = 64; b.small_out = sm_b;
        b.n_large = NL; b.large_size = 2048; b.large_out = lg_b;
        b.seed = 6000u; b.use_pthread_exit = 1;
        check(pthread_create(&th, NULL, holder_thread, &b) == 0,
              "T4 pthread_exit-form holder created");
        pthread_join(th, NULL);
        d_small_b = live_small() - base_small;
        d_map_b = live_mappings() - base_map;
        check(b.alloc_fail == 0 && verify_held(&b) == 0,
              "T4 pthread_exit-form: allocs ok, patterns intact");
        free_held(&b);
        check(live_small() == base_small && live_mappings() == base_map,
              "T4 pthread_exit-form: reconciles after main free");

        snprintf(label, sizeof(label),
                 "T4 equivalence: deltas identical (return: "
                 "small %lu map %lu; pthread_exit: small %lu "
                 "map %lu)",
                 d_small_a, d_map_a, d_small_b, d_map_b);
        check(d_small_a == d_small_b && d_map_a == d_map_b &&
              d_small_a == (unsigned long)NS &&
              d_map_a == (unsigned long)NL, label);
    }

    /* ---------- T5: final accounting -------------------- */
    printf("T5 final accounting\n");
    {
        void *s = mimir_malloc(64);
        void *l = mimir_malloc(2048);
        int ok = (s != NULL && l != NULL);

        if (s != NULL) {
            memset(s, 0x7E, 64);
        }
        if (l != NULL) {
            memset(l, 0x7E, 2048);
        }
        check(ok, "T5 fresh small + large allocation after "
                  "all exits succeeds");
        mimir_free(s);
        mimir_free(l);
    }
    printf("final: live_spans=%lu live_small=%lu "
           "live_mappings=%lu span_published=%lu "
           "span_retired=%lu small_alloc=%lu small_free=%lu "
           "map_published=%lu map_retired=%lu\n",
           live_spans(), live_small(), live_mappings(),
           mimir_audit_span_published, mimir_audit_span_retired,
           mimir_audit_small_alloc, mimir_audit_small_free,
           mimir_audit_alloc_published,
           mimir_audit_mapping_retired);
    check(live_small() == 0, "T5 live small objects == 0");
    check(live_mappings() == 0, "T5 live mappings == 0");
    check(live_spans() <= 40,
          "T5 live spans within RP-5 bound (<= 40)");
    check(mimir_audit_small_alloc == mimir_audit_small_free,
          "T5 small alloc counter == small free counter exactly");
    check(mimir_audit_alloc_published == mimir_audit_mapping_retired,
          "T5 mapping published == mapping retired exactly");

    printf("checks=%d failures=%d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
