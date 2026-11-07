/*
 * test_h005.c -- evidence test for slice H-005 (Phase 8
 * owner death handling; slice record:
 * docs/PHASE_8_OWNER_DEATH_H005.md).
 *
 * Run standalone via `make test-h005` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-g006 / test-h003 pattern).
 *
 * What G-006 proved in calm shapes: a thread may exit
 * while holding blocks; the blocks stay live in the
 * process-global registries and another thread frees
 * them cleanly. What H-002 Section 5 item 2 left
 * unexercised, and this test runs: death MID-WORKLOAD
 * UNDER CONTENTION, with death timed against other
 * threads' frees of the dead threads' blocks.
 *
 * Shape, repeated for ROUNDS=3 rounds:
 *   D1 death under contention: 3 churner threads run
 *      alloc/fill/verify/free loops while 4 victim
 *      threads allocate blocks in two batches (with
 *      their own alloc/free churn between the batches,
 *      so death is mid-workload, not at a quiescent
 *      edge), pattern-fill every block, then DIE still
 *      holding all of them -- victims 0/2 by return,
 *      victims 1/3 by pthread_exit, identical shapes so
 *      the two death forms must leave identical deltas.
 *      Max concurrent threads: 3 + 4 = 7 (<= 8 bound).
 *   D2 quiescent reconciliation while dead blocks are
 *      held: after every thread joins, live counters
 *      exceed baseline by EXACTLY the dead blocks'
 *      counts (span small + mapping), and the main
 *      thread verifies every dead block's pattern.
 *   D3 cross-thread free after death, under renewed
 *      contention: 2 freer threads verify each dead
 *      block's pattern independently (re-derived seed)
 *      and free it, while 2 fresh churners churn.
 *      Ownership is process-global: survivors freeing
 *      a dead owner's blocks is the ordinary free path.
 *   D4 quiescent reconciliation: counters return
 *      EXACTLY to baseline; no block lost, none freed
 *      twice (a double free or lost block would break
 *      the exact equality or corrupt a pattern).
 *   D5 final accounting (once, after all rounds): live
 *   small == 0, live mappings == 0, spans within the
 *   RP-5 bound (<= 40), small_alloc == small_free and
 *   mapping published == retired exactly, and a fresh
 *   small + large pair still allocates (health).
 *
 * Per victim per round: 8 blocks of 64 B + 4 blocks of
 * 256 B (span path, <= 1024) + 3 blocks of 2048 B
 * (mapping path, > 1024). Per round: 48 small + 12
 * mapping dead blocks; 3 rounds: 144 small + 36 mapping
 * blocks die with their owners and are freed by
 * survivors. Churners: 300 iterations/round-leg of
 * 128 B / 4096 B self alloc/free.
 *
 * Counter discipline (E-005, as G-006/H-003): audit
 * counters are read only on the main thread at
 * quiescent points (all threads of the leg joined;
 * pthread_join is the synchronisation edge). No counter
 * is sampled mid-flight for a verdict.
 *
 * Exit status 0 and a final "RESULT: PASS" line only
 * if every check passed. Prints only facts observed.
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

static void fill_pattern(unsigned char *p, size_t n,
                         unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)((i * 31u + seed * 17u +
                                (i >> 8)) & 0xffu);
    }
}

static int pattern_ok(const unsigned char *p, size_t n,
                      unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        unsigned char want =
            (unsigned char)((i * 31u + seed * 17u +
                             (i >> 8)) & 0xffu);
        if (p[i] != want) {
            return 0;
        }
    }
    return 1;
}

static unsigned long live_spans(void)
{
    return mimir_audit_span_published -
           mimir_audit_span_retired;
}

static unsigned long live_small(void)
{
    return mimir_audit_small_alloc - mimir_audit_small_free;
}

static unsigned long live_mappings(void)
{
    return mimir_audit_alloc_published -
           mimir_audit_mapping_retired;
}

/* ---- churner: self-contained alloc/fill/verify/free - */
struct churn_req {
    int iters;
    unsigned int seed;
    int fail;
    long ops;
};

static void *churner_thread(void *arg)
{
    struct churn_req *r = (struct churn_req *)arg;
    int i;

    r->fail = 0;
    r->ops = 0;
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
        r->ops++;
    }
    return NULL;
}

/* ---- victim: allocates in 2 batches, churns between,
 * then dies holding every block it kept. ------------- */
enum { NS64 = 8, NS256 = 4, NL = 3, NSMALL = NS64 + NS256 };

struct victim_req {
    unsigned int seed;
    int use_pthread_exit;
    void *small_out[NSMALL];
    size_t small_sz[NSMALL];
    void *large_out[NL];
    int alloc_fail;
    int churn_fail;
};

static void victim_fill_batch(struct victim_req *r,
                              int from, int to, int batch)
{
    int i;
    for (i = from; i < to; i++) {
        size_t sz = (i < NS64) ? (size_t)64 : (size_t)256;
        r->small_sz[i] = sz;
        r->small_out[i] = mimir_malloc(sz);
        if (r->small_out[i] == NULL) {
            r->alloc_fail++;
        } else {
            fill_pattern((unsigned char *)r->small_out[i], sz,
                         r->seed + (unsigned int)(batch * 40 + i));
        }
    }
}

static void *victim_thread(void *arg)
{
    struct victim_req *r = (struct victim_req *)arg;
    int i;

    r->alloc_fail = 0;
    r->churn_fail = 0;
    /* Batch 1: half the small blocks. */
    victim_fill_batch(r, 0, NSMALL / 2, 0);
    /* Mid-workload churn: own alloc/free cycles so the
     * death below is not at a quiescent edge. */
    for (i = 0; i < 20; i++) {
        size_t sz = (i % 2 == 0) ? (size_t)128 : (size_t)2048;
        unsigned int seed = r->seed + 900u + (unsigned int)i;
        void *p = mimir_malloc(sz);
        if (p == NULL) {
            r->churn_fail++;
            continue;
        }
        fill_pattern((unsigned char *)p, sz, seed);
        if (!pattern_ok((const unsigned char *)p, sz, seed)) {
            r->churn_fail++;
        }
        mimir_free(p);
    }
    /* Batch 2: rest of the small blocks + the large. */
    victim_fill_batch(r, NSMALL / 2, NSMALL, 1);
    for (i = 0; i < NL; i++) {
        r->large_out[i] = mimir_malloc(2048);
        if (r->large_out[i] == NULL) {
            r->alloc_fail++;
        } else {
            fill_pattern((unsigned char *)r->large_out[i],
                         2048, r->seed + 100u + (unsigned int)i);
        }
    }
    /* DIE holding every kept block. */
    if (r->use_pthread_exit) {
        pthread_exit(NULL);
    }
    return NULL;
}

static int verify_victim(const struct victim_req *r)
{
    int bad = 0;
    int i;

    for (i = 0; i < NSMALL; i++) {
        int batch = (i < NSMALL / 2) ? 0 : 1;
        if (r->small_out[i] == NULL ||
            !pattern_ok((const unsigned char *)r->small_out[i],
                        r->small_sz[i],
                        r->seed + (unsigned int)(batch * 40 + i))) {
            bad++;
        }
    }
    for (i = 0; i < NL; i++) {
        if (r->large_out[i] == NULL ||
            !pattern_ok((const unsigned char *)r->large_out[i],
                        2048, r->seed + 100u + (unsigned int)i)) {
            bad++;
        }
    }
    return bad;
}

/* ---- freer: verifies + frees a slice of dead blocks - */
struct freer_req {
    struct victim_req *victims; /* array of 4 */
    int first_victim;
    int n_victims;
    int bad;
    int freed;
};

static void *freer_thread(void *arg)
{
    struct freer_req *r = (struct freer_req *)arg;
    int v, i;

    r->bad = 0;
    r->freed = 0;
    for (v = r->first_victim;
         v < r->first_victim + r->n_victims; v++) {
        struct victim_req *vic = &r->victims[v];
        if (verify_victim(vic) != 0) {
            r->bad++;
        }
        for (i = 0; i < NSMALL; i++) {
            mimir_free(vic->small_out[i]);
            r->freed++;
        }
        for (i = 0; i < NL; i++) {
            mimir_free(vic->large_out[i]);
            r->freed++;
        }
    }
    return NULL;
}

int main(void)
{
    enum { ROUNDS = 3, NV = 4, NC1 = 3, NF = 2, NC2 = 2 };
    long page = sysconf(_SC_PAGESIZE);
    unsigned long base_small, base_map;
    int round;

    printf("H-005 owner-death handling test\n");
    printf("page_size=%ld (runtime, via sysconf)\n", page);
    if (page <= 0) {
        printf("RESULT: FAIL (no page size)\n");
        return 1;
    }

    /* Warm-up: one LARGE alloc/touch/free (mapping path;
     * leaves no span state behind, G-005/G-006 method). */
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
    check(base_small == 0, "baseline live small == 0");
    check(base_map == 0, "baseline live mappings == 0");

    for (round = 0; round < ROUNDS; round++) {
        static struct victim_req vic[NV];
        static struct churn_req ch1[NC1];
        static struct churn_req ch2[NC2];
        static struct freer_req fr[NF];
        pthread_t vth[NV], cth[NC1], fth[NF], c2th[NC2];
        int vc = 0, cc = 0, fc = 0, c2c = 0;
        int i;
        char label[224];
        long churn_ops = 0;
        int churn_fail = 0, victim_bad = 0;

        printf("ROUND %d: D1 death under contention "
               "(%d churners + %d victims)\n", round, NC1, NV);
        memset(vic, 0, sizeof(vic));
        for (i = 0; i < NC1; i++) {
            ch1[i].iters = 300;
            ch1[i].seed = 1000u + (unsigned int)(round * 5000 + i * 400);
            if (pthread_create(&cth[i], NULL, churner_thread,
                               &ch1[i]) == 0) {
                cc++;
            }
        }
        for (i = 0; i < NV; i++) {
            vic[i].seed = 100u + (unsigned int)(round * 2000 + i * 200);
            vic[i].use_pthread_exit = (i % 2 == 1);
            if (pthread_create(&vth[i], NULL, victim_thread,
                               &vic[i]) == 0) {
                vc++;
            }
        }
        snprintf(label, sizeof(label),
                 "R%d all threads created (churners %d/%d, "
                 "victims %d/%d)", round, cc, NC1, vc, NV);
        check(cc == NC1 && vc == NV, label);
        for (i = 0; i < vc; i++) {
            pthread_join(vth[i], NULL);
        }
        for (i = 0; i < cc; i++) {
            pthread_join(cth[i], NULL);
        }
        for (i = 0; i < NC1; i++) {
            churn_ops += ch1[i].ops;
            churn_fail += ch1[i].fail;
        }
        snprintf(label, sizeof(label),
                 "R%d churners worked through the deaths: "
                 "%ld ops, %d failures", round, churn_ops,
                 churn_fail);
        check(churn_ops > 0 && churn_fail == 0, label);
        for (i = 0; i < NV; i++) {
            snprintf(label, sizeof(label),
                     "R%d victim %d (%s): alloc_fail=%d "
                     "churn_fail=%d", round, i,
                     vic[i].use_pthread_exit ?
                     "pthread_exit" : "return",
                     vic[i].alloc_fail, vic[i].churn_fail);
            check(vic[i].alloc_fail == 0 &&
                  vic[i].churn_fail == 0, label);
        }

        printf("ROUND %d: D2 held-by-the-dead reconciliation\n",
               round);
        snprintf(label, sizeof(label),
                 "R%d after death: live_small delta == %d "
                 "(observed %lu), live_mappings delta == %d "
                 "(observed %lu)", round, NV * NSMALL,
                 live_small() - base_small, NV * NL,
                 live_mappings() - base_map);
        check(live_small() - base_small ==
                  (unsigned long)(NV * NSMALL) &&
              live_mappings() - base_map ==
                  (unsigned long)(NV * NL), label);
        /* Death-form equivalence: return victims (0,2) and
         * pthread_exit victims (1,3) have identical shapes;
         * D2's exact total plus per-victim pattern checks
         * below pin both forms to the same delta. */
        for (i = 0; i < NV; i++) {
            victim_bad += verify_victim(&vic[i]);
        }
        snprintf(label, sizeof(label),
                 "R%d all %d dead blocks' patterns intact "
                 "after owner death (bad=%d)", round,
                 NV * (NSMALL + NL), victim_bad);
        check(victim_bad == 0, label);

        printf("ROUND %d: D3 survivors free the dead "
               "blocks under renewed contention\n", round);
        for (i = 0; i < NC2; i++) {
            ch2[i].iters = 300;
            ch2[i].seed = 7000u + (unsigned int)(round * 5000 + i * 400);
            if (pthread_create(&c2th[i], NULL, churner_thread,
                               &ch2[i]) == 0) {
                c2c++;
            }
        }
        for (i = 0; i < NF; i++) {
            fr[i].victims = vic;
            fr[i].first_victim = i * (NV / NF);
            fr[i].n_victims = NV / NF;
            if (pthread_create(&fth[i], NULL, freer_thread,
                               &fr[i]) == 0) {
                fc++;
            }
        }
        check(c2c == NC2 && fc == NF,
              "R-round freers + churners created");
        for (i = 0; i < fc; i++) {
            pthread_join(fth[i], NULL);
        }
        for (i = 0; i < c2c; i++) {
            pthread_join(c2th[i], NULL);
        }
        {
            int freed = 0, bad = 0, cf = 0;
            for (i = 0; i < NF; i++) {
                freed += fr[i].freed;
                bad += fr[i].bad;
            }
            for (i = 0; i < NC2; i++) {
                cf += ch2[i].fail;
            }
            snprintf(label, sizeof(label),
                     "R%d freers freed %d dead blocks, "
                     "pattern-bad victims=%d, churn "
                     "failures=%d", round, freed, bad, cf);
            check(freed == NV * (NSMALL + NL) && bad == 0 &&
                  cf == 0, label);
        }

        printf("ROUND %d: D4 reconciliation to baseline\n",
               round);
        snprintf(label, sizeof(label),
                 "R%d after survivor frees: live_small=%lu "
                 "live_mappings=%lu (baseline %lu/%lu), "
                 "spans=%lu <= 40", round, live_small(),
                 live_mappings(), base_small, base_map,
                 live_spans());
        check(live_small() == base_small &&
              live_mappings() == base_map &&
              live_spans() <= 40, label);
    }

    /* ---------- D5: final accounting ------------------ */
    printf("D5 final accounting\n");
    {
        void *s = mimir_malloc(64);
        void *l = mimir_malloc(2048);
        check(s != NULL && l != NULL,
              "D5 fresh small + large allocation after "
              "all deaths succeeds");
        if (s != NULL) {
            memset(s, 0x7E, 64);
        }
        if (l != NULL) {
            memset(l, 0x7E, 2048);
        }
        mimir_free(s);
        mimir_free(l);
    }
    printf("final: live_spans=%lu live_small=%lu "
           "live_mappings=%lu small_alloc=%lu "
           "small_free=%lu map_published=%lu "
           "map_retired=%lu\n",
           live_spans(), live_small(), live_mappings(),
           mimir_audit_small_alloc, mimir_audit_small_free,
           mimir_audit_alloc_published,
           mimir_audit_mapping_retired);
    check(live_small() == 0, "D5 live small == 0");
    check(live_mappings() == 0, "D5 live mappings == 0");
    check(live_spans() <= 40, "D5 spans within RP-5 (<=40)");
    check(mimir_audit_small_alloc == mimir_audit_small_free,
          "D5 small alloc == small free exactly");
    check(mimir_audit_alloc_published ==
          mimir_audit_mapping_retired,
          "D5 mapping published == retired exactly");

    printf("checks=%d failures=%d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
