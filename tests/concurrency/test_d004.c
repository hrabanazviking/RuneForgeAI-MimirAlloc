/*
 * test_d004.c -- D-004 physical multithread allocate/fill/free
 * test (Wave 4, Phase 4 exit evidence, Pi half).
 *
 * Runs against the real public mimir_ API (include/mimir_alloc.h)
 * on this host with a bounded POSIX thread pool. Design bounds
 * (Roadmap Section 12 item 7: start with low concurrency and
 * bounded allocations): 4 worker threads (host nproc is 4; the
 * assignment cap is 8), fixed iteration counts, request sizes
 * at most 8192 bytes, at most 64 blocks live at once outside a
 * single thread's own churn, and a deterministic schedule of
 * 2,088 allocations, 2,088 frees and 32 realloc grows across
 * all phases -- seconds, never minutes, on the Pi.
 *
 * What is exercised, in phase order:
 *
 *   PHASE A [private churn, concurrent]: each thread performs
 *   250 iterations of mimir_malloc / fill / verify / mimir_free
 *   with per-thread, per-iteration sizes and patterns, plus a
 *   mimir_calloc zero-check every 50 iterations (5 per thread).
 *   No block is shared in this phase; it proves concurrent
 *   callers do not disturb each other's private blocks.
 *
 *   PHASE B [simultaneous live set + cross-verification]: each
 *   thread allocates 16 blocks and fills them with a pattern
 *   unique to (thread, slot). After a barrier, each thread
 *   verifies the NEXT thread's 16 live blocks read-only, then
 *   after a second barrier re-verifies and frees its own.
 *   The main thread afterwards checks that all 64 recorded
 *   pointers were pairwise distinct while simultaneously live.
 *   Every live block is pattern-checked before its free.
 *
 *   PHASE C [cross-thread free, hook C-009 / MA-6.2-054 by
 *   analogy]: each producer thread allocates 50 blocks, fills
 *   and verifies them, and publishes them; after a barrier the
 *   NEXT thread verifies each block again and frees it --
 *   blocks allocated on one thread are freed on another,
 *   exactly the C-009 pass criterion. Each consumer then makes
 *   one fresh allocation of its own (fill / verify / free) to
 *   show subsequent allocations remain valid (same criterion).
 *
 *   PHASE D [shared concurrent hammer]: all threads run 200
 *   further iterations concurrently against the shared
 *   allocator with per-thread fixed-seed xorshift sizes (16..
 *   4096 bytes); every 25th iteration is a mimir_realloc grow
 *   (128 -> 512) with the preserved prefix verified. Any
 *   allocator-visible corruption (pattern mismatch, duplicate
 *   live pointer in Phase B, lost realloc prefix, unexpected
 *   null) is a counted failure.
 *
 * Counting discipline: each worker accumulates checks,
 * failures, allocations and frees in its OWN result struct;
 * the main thread sums them after pthread_join. No counter is
 * shared while threads run, and every loop bound, size and
 * pattern is a fixed function of (phase, thread, iteration),
 * so the printed totals are deterministic for a given build:
 * the same numbers must appear on every passing run.
 *
 * Contract position (honesty): the namespaced mimir_ API has
 * NO direct contract rows (U-03 open; docs/contracts.md S7.1(e)
 * records that Roadmap S6 does not even name core-function
 * thread-safety). This test is therefore implementation
 * evidence BY ANALOGY to MA-6.2-054 / hook C-009 (cross-thread
 * ownership) and to the Phase 3 C-007 cases ERR-211/ERR-212 --
 * it proves what THIS build did under concurrency on THIS
 * host, and settles no contract question. Thread-safety of
 * this implementation rests on the one global lock consumed
 * from L6 (B-006 S9; src/platform/mimir_platform.c). See
 * INC-D004-01 in docs/PHASE_4_MULTITHREAD_RUN.md.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if every
 * check passed. Prints only facts it observed or counted.
 */

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "mimir_alloc.h"

#define NTHREADS   4
#define ITER_A     250
#define CALLOC_EVERY 50
#define LIVE_B     16
#define CROSS_C    50
#define ITER_D     200
#define REALLOC_EVERY 25

struct result {
    long checks;
    long failures;
    long allocs;   /* successful mimir_malloc/calloc returns  */
    long frees;    /* mimir_free calls on live blocks         */
};

struct worker_arg {
    int tid;
    struct result res;
};

/* Shared Phase B / Phase C publication arrays. Written by the
 * owner thread before a barrier, read by other threads only
 * after that barrier -- the barrier is the synchronisation,
 * no extra lock is needed or used. */
static void *g_live_ptr[NTHREADS][LIVE_B];
static size_t g_live_size[NTHREADS][LIVE_B];
static void *g_cross_ptr[NTHREADS][CROSS_C];
static size_t g_cross_size[NTHREADS][CROSS_C];

static pthread_barrier_t g_barrier;

/* Per-block verifiable pattern: the byte at offset i is a
 * fixed function of the block's seed and i, so any thread can
 * verify any block from its seed alone, and two blocks with
 * different seeds hold different bytes at almost every offset. */
static unsigned int block_seed(int phase, int tid, int slot)
{
    return 0x01000193u ^ ((unsigned int)(phase + 1) * 0x9E3779B9u) ^
           ((unsigned int)(tid + 1) * 0x85EBCA6Bu) ^
           ((unsigned int)(slot + 1) * 0x27D4EB2Fu);
}

static unsigned char pattern_byte(unsigned int seed, size_t i)
{
    return (unsigned char)(((seed >> ((i & 3u) * 8u)) +
                            (unsigned int)(i * 31u) +
                            (unsigned int)(i >> 8)) & 0xffu);
}

static void fill_pattern(unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;

    for (i = 0; i < n; i++) {
        p[i] = pattern_byte(seed, i);
    }
}

static int pattern_ok(const unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (p[i] != pattern_byte(seed, i)) {
            return 0;
        }
    }
    return 1;
}

static void tally(struct result *r, int ok)
{
    r->checks++;
    if (!ok) {
        r->failures++;
    }
}

/* One full private cycle: allocate n bytes, fill, verify,
 * free. Counts: alloc check + pattern check; alloc/free
 * counters move only on success. */
static void private_cycle(struct result *r, size_t n, unsigned int seed)
{
    unsigned char *p = (unsigned char *)mimir_malloc(n);

    tally(r, p != NULL);
    if (p == NULL) {
        return;
    }
    r->allocs++;
    fill_pattern(p, n, seed);
    tally(r, pattern_ok(p, n, seed));
    mimir_free(p);
    r->frees++;
}

/* ------------------------------------------------------------------ */
/* PHASE A -- private churn, concurrent                               */
/* ------------------------------------------------------------------ */
static void phase_a(struct worker_arg *a)
{
    static const size_t sizes[8] =
        {16u, 32u, 64u, 128u, 256u, 512u, 1024u, 2048u};
    int it;

    for (it = 0; it < ITER_A; it++) {
        size_t n = sizes[(size_t)(it + a->tid) % 8u];

        private_cycle(&a->res, n, block_seed(0, a->tid, it));
        if (it % CALLOC_EVERY == 0) {
            unsigned char *z =
                (unsigned char *)mimir_calloc(32u, 4u); /* 128 bytes */
            size_t i;
            int allzero = 1;

            tally(&a->res, z != NULL);
            if (z != NULL) {
                a->res.allocs++;
                for (i = 0; i < 128u; i++) {
                    if (z[i] != 0) {
                        allzero = 0;
                    }
                }
                tally(&a->res, allzero);
                fill_pattern(z, 128u, block_seed(0, a->tid, 900 + it));
                tally(&a->res,
                      pattern_ok(z, 128u, block_seed(0, a->tid, 900 + it)));
                mimir_free(z);
                a->res.frees++;
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* PHASE B -- simultaneous live set, cross-verified                   */
/* ------------------------------------------------------------------ */
static void phase_b(struct worker_arg *a)
{
    int t = a->tid;
    int slot, other;

    for (slot = 0; slot < LIVE_B; slot++) {
        size_t n = 64u + (size_t)((slot * 37 + t * 11) % 960);
        unsigned char *p = (unsigned char *)mimir_malloc(n);

        g_live_size[t][slot] = n;
        g_live_ptr[t][slot] = p;
        tally(&a->res, p != NULL);
        if (p != NULL) {
            a->res.allocs++;
            fill_pattern(p, n, block_seed(1, t, slot));
            tally(&a->res, pattern_ok(p, n, block_seed(1, t, slot)));
        }
    }
    pthread_barrier_wait(&g_barrier);

    /* Cross-verify the NEXT thread's live blocks, read-only. */
    other = (t + 1) % NTHREADS;
    for (slot = 0; slot < LIVE_B; slot++) {
        const unsigned char *p =
            (const unsigned char *)g_live_ptr[other][slot];

        if (p == NULL) {
            tally(&a->res, 0);
            continue;
        }
        tally(&a->res, pattern_ok(p, g_live_size[other][slot],
                                  block_seed(1, other, slot)));
    }
    pthread_barrier_wait(&g_barrier);

    /* Re-verify own blocks (pattern-checked before free), free. */
    for (slot = 0; slot < LIVE_B; slot++) {
        unsigned char *p = (unsigned char *)g_live_ptr[t][slot];

        if (p == NULL) {
            continue;
        }
        tally(&a->res, pattern_ok(p, g_live_size[t][slot],
                                  block_seed(1, t, slot)));
        mimir_free(p);
        a->res.frees++;
    }
}

/* ------------------------------------------------------------------ */
/* PHASE C -- cross-thread free (C-009 / MA-6.2-054 by analogy)       */
/* ------------------------------------------------------------------ */
static void phase_c(struct worker_arg *a)
{
    int t = a->tid;
    int slot, producer;

    for (slot = 0; slot < CROSS_C; slot++) {
        size_t n = 32u + (size_t)((slot * 53 + t * 7) % 480);
        unsigned char *p = (unsigned char *)mimir_malloc(n);

        g_cross_size[t][slot] = n;
        g_cross_ptr[t][slot] = p;
        tally(&a->res, p != NULL);
        if (p != NULL) {
            a->res.allocs++;
            fill_pattern(p, n, block_seed(2, t, slot));
            tally(&a->res, pattern_ok(p, n, block_seed(2, t, slot)));
        }
    }
    pthread_barrier_wait(&g_barrier);

    /* Verify and free the PREVIOUS thread's blocks: every block
     * freed in this step was allocated on a different thread. */
    producer = (t + NTHREADS - 1) % NTHREADS;
    for (slot = 0; slot < CROSS_C; slot++) {
        unsigned char *p = (unsigned char *)g_cross_ptr[producer][slot];

        if (p == NULL) {
            tally(&a->res, 0);
            continue;
        }
        tally(&a->res, pattern_ok(p, g_cross_size[producer][slot],
                                  block_seed(2, producer, slot)));
        mimir_free(p);
        a->res.frees++;
    }

    /* Subsequent allocations remain valid after cross frees. */
    private_cycle(&a->res, 256u, block_seed(2, t, 999));
}

/* ------------------------------------------------------------------ */
/* PHASE D -- shared concurrent hammer                                */
/* ------------------------------------------------------------------ */
static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void phase_d(struct worker_arg *a)
{
    uint32_t rng = 0x9E3779B9u ^ ((uint32_t)(a->tid + 1) * 0x85EBCA6Bu);
    int it;

    for (it = 0; it < ITER_D; it++) {
        if (it % REALLOC_EVERY == REALLOC_EVERY - 1) {
            unsigned char *p = (unsigned char *)mimir_malloc(128u);
            unsigned char *q;

            tally(&a->res, p != NULL);
            if (p == NULL) {
                continue;
            }
            a->res.allocs++;
            fill_pattern(p, 128u, block_seed(3, a->tid, it));
            q = (unsigned char *)mimir_realloc(p, 512u);
            tally(&a->res, q != NULL);
            if (q == NULL) {
                /* Failure posture: original still owned; verify
                 * it is intact, then free it (B-008 S4 by
                 * analogy). Counts as a failure above already:
                 * this run expects no allocation failures. */
                tally(&a->res, pattern_ok(p, 128u,
                                          block_seed(3, a->tid, it)));
                mimir_free(p);
                a->res.frees++;
                continue;
            }
            tally(&a->res, pattern_ok(q, 128u, block_seed(3, a->tid, it)));
            fill_pattern(q, 512u, block_seed(3, a->tid, 500 + it));
            tally(&a->res, pattern_ok(q, 512u,
                                      block_seed(3, a->tid, 500 + it)));
            mimir_free(q);
            a->res.frees++;
        } else {
            size_t n = 16u + (size_t)(xorshift32(&rng) % 4081u);

            private_cycle(&a->res, n, block_seed(3, a->tid, it));
        }
    }
}

static void *worker_main(void *vp)
{
    struct worker_arg *a = (struct worker_arg *)vp;

    phase_a(a);
    phase_b(a);
    phase_c(a);
    phase_d(a);
    return NULL;
}

int main(void)
{
    pthread_t th[NTHREADS];
    struct worker_arg args[NTHREADS];
    struct result per_phase_note;
    long checks = 0, failures = 0, allocs = 0, frees = 0;
    long page = sysconf(_SC_PAGESIZE);
    int t, i, j, created = 0;

    (void)per_phase_note;
    printf("D-004 multithread allocate/fill/free test\n");
    printf("CONFIG: threads=%d iter_a=%d live_b=%d cross_c=%d "
           "iter_d=%d host_page_size=%ld\n",
           NTHREADS, ITER_A, LIVE_B, CROSS_C, ITER_D, page);

    pthread_barrier_init(&g_barrier, NULL, NTHREADS);
    for (t = 0; t < NTHREADS; t++) {
        memset(&args[t], 0, sizeof args[t]);
        args[t].tid = t;
        if (pthread_create(&th[t], NULL, worker_main, &args[t]) != 0) {
            printf("  [FAIL] pthread_create for thread %d\n", t);
            failures++;
            continue;
        }
        created++;
    }
    for (t = 0; t < created; t++) {
        pthread_join(th[t], NULL);
    }
    pthread_barrier_destroy(&g_barrier);

    for (t = 0; t < NTHREADS; t++) {
        printf("THREAD %d: checks=%ld failures=%ld allocs=%ld frees=%ld\n",
               t, args[t].res.checks, args[t].res.failures,
               args[t].res.allocs, args[t].res.frees);
        checks += args[t].res.checks;
        failures += args[t].res.failures;
        allocs += args[t].res.allocs;
        frees += args[t].res.frees;
    }

    /* Phase B distinctness: the 64 simultaneously live pointers
     * recorded before any of them was freed must be pairwise
     * distinct (allocator-visible overlap would break this). */
    {
        long distinct_fail = 0;

        for (i = 0; i < NTHREADS * LIVE_B; i++) {
            for (j = i + 1; j < NTHREADS * LIVE_B; j++) {
                void *pi = g_live_ptr[i / LIVE_B][i % LIVE_B];
                void *pj = g_live_ptr[j / LIVE_B][j % LIVE_B];

                if (pi != NULL && pi == pj) {
                    distinct_fail++;
                }
            }
        }
        checks++;
        if (distinct_fail != 0) {
            failures++;
        }
        printf("PHASE B distinct live pointers: %s (%d recorded)\n",
               distinct_fail == 0 ? "ok" : "FAIL", NTHREADS * LIVE_B);
    }

    /* Balance: every successful allocation was freed exactly
     * once by construction of the schedule; realloc keeps the
     * count balanced (one alloc, one free of the result). */
    checks++;
    if (allocs != frees) {
        failures++;
    }
    printf("ALLOC/FREE BALANCE: allocs=%ld frees=%ld %s\n",
           allocs, frees, allocs == frees ? "ok" : "FAIL");
    printf("CHECKS: %ld run, %ld failed\n", checks, failures);
    printf("ALLOCATIONS: %ld successful, %ld frees\n", allocs, frees);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
