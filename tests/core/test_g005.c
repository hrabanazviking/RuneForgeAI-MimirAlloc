/*
 * test_g005.c -- evidence test for slice G-005 (Phase 7
 * reclaim policy + idle-thread retained-memory measurement;
 * slice record: docs/PHASE_7_RECLAIM_POLICY_G005.md).
 *
 * Run standalone via `make test-g005` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-g003 pattern). It verifies the reclaim policy that
 * G-003 implemented (docs/PHASE_7_SMALL_OBJECT_PATH_G003.md)
 * and measures what that policy retains, through the four
 * public symbols plus the G-003 span/object audit counters
 * (tests/core/mimir_small_g003.h, INC-E001-07 shape (b):
 * readable only by linking the library OBJECTS directly).
 *
 * The policy under test (defined in the G-005 slice doc):
 *   RP-1 a span whose live_count returns to 0 is EMPTY.
 *   RP-2 at most 2 EMPTY spans per size class are retained
 *        (MIMIR_MAX_EMPTY_SPANS_PER_CLASS in the core).
 *   RP-3 the empty span that would exceed the bound is
 *        released whole to L6 (unmapped) in the same locked
 *        free operation that emptied it -- reclaim is
 *        synchronous at free time, never deferred to a
 *        background pass, a later allocation, or thread
 *        exit.
 *   RP-4 retention is GLOBAL per class, not per thread:
 *        the core keeps no thread-local cache (no
 *        _Thread_local / pthread-key state exists in the
 *        small-object path), so an idle thread retains
 *        nothing of its own; what an idle thread's earlier
 *        frees left behind is exactly the shared per-class
 *        bound, already counted.
 *   RP-5 retained-after-free memory is therefore at most
 *        20 classes x 2 spans x one system page, and live
 *        spans are otherwise bounded by live demand only.
 *
 * Measurement method (stated honestly): span/object counts
 * are EXACT (audit-counter deltas); mapped retained bytes
 * are live_spans x the runtime page size (each span is
 * exactly one page, G-002 C-2); VmRSS from
 * /proc/self/status is a process-level cross-check only --
 * it includes the test harness's own libc allocations and
 * is printed as a fact, never used as a pass/fail check.
 * Warm-up: one LARGE alloc/touch/free (2048, mapping path,
 * retained-after-free 0 per the G-001 baseline) before any
 * measurement, so first-call code/stack faulting does not
 * pollute the first phase's RSS reading -- and so the
 * warm-up itself leaves no span state behind (a small
 * warm-up would leave one retained empty span in its class
 * and falsify P1's per-class counts). All runs are
 * small and bounded (the largest single phase holds
 * 2*capacity+1 blocks of one class).
 *
 *   P1 per-class bound: for each of the 20 classes, hold
 *      2*capacity+1 blocks (forces exactly 3 spans), free
 *      all, and check exactly 2 spans of retention remain
 *      (live-span delta +2 per class, cumulative).
 *   P2 churn stability: 10 rounds of alloc/free of
 *      3*capacity blocks of class 128; after every round
 *      live small objects are 0 and total live spans are
 *      back at the P1 end state (no retention growth).
 *   P3 idle threads: 4 worker threads each allocate 200
 *      blocks of size 256, free them all, then sit IDLE
 *      (blocked, holding nothing) while the main thread
 *      measures: live small objects 0, total live spans
 *      exactly the global bound state -- the idle threads
 *      add NO retention. Threads are then released and
 *      joined; the post-join reading must be identical
 *      (nothing was waiting for thread exit to reclaim).
 *   P4 final accounting: live small objects 0, live spans
 *      exactly 40 (2 x 20 classes), retained mapped bytes
 *      exactly 40 x page size; RSS printed start vs end.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */

#define _POSIX_C_SOURCE 200809L

#include "mimir_alloc.h"
#include "mimir_small_g003.h"

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

static const size_t class_sizes[20] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};

static unsigned long live_spans(void)
{
    return mimir_audit_span_published - mimir_audit_span_retired;
}

static unsigned long live_small(void)
{
    return mimir_audit_small_alloc - mimir_audit_small_free;
}

static long rss_kb(void)
{
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    long v = -1;

    if (f == NULL) {
        return -1;
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        if (sscanf(line, "VmRSS: %ld kB", &v) == 1) {
            break;
        }
    }
    fclose(f);
    return v;
}

/* ---- P3 idle-thread machinery ---- */
static pthread_mutex_t idle_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t idle_cv = PTHREAD_COND_INITIALIZER;
static int threads_idle = 0;
static int threads_release = 0;
#define IDLE_THREADS 4
#define IDLE_BLOCKS 200
#define IDLE_SIZE ((size_t)256)

static void *idle_worker(void *arg)
{
    void *blocks[IDLE_BLOCKS];
    size_t i;
    (void)arg;

    for (i = 0; i < IDLE_BLOCKS; i++) {
        blocks[i] = mimir_malloc(IDLE_SIZE);
        if (blocks[i] != NULL) {
            memset(blocks[i], 0xA5, IDLE_SIZE);
        }
    }
    for (i = 0; i < IDLE_BLOCKS; i++) {
        mimir_free(blocks[i]);
    }
    /* Now idle: hold nothing, block until released. */
    pthread_mutex_lock(&idle_mu);
    threads_idle++;
    pthread_cond_broadcast(&idle_cv);
    while (!threads_release) {
        pthread_cond_wait(&idle_cv, &idle_mu);
    }
    pthread_mutex_unlock(&idle_mu);
    return NULL;
}

int main(void)
{
    long page = sysconf(_SC_PAGESIZE);
    size_t header = 96;
    long rss_start, rss_end;
    unsigned long spans_bound_state = 0;
    size_t i;

    printf("G-005 reclaim policy + idle-thread retention test\n");
    printf("page_size=%ld (runtime, via sysconf)\n", page);
    if (page <= 0) {
        printf("RESULT: FAIL (no page size)\n");
        return 1;
    }

    /* Warm-up: one LARGE alloc/touch/free before measuring
     * (mapping path; leaves no span state behind). */
    {
        void *w = mimir_malloc(2048);
        if (w != NULL) {
            memset(w, 0x5A, 2048);
            mimir_free(w);
        }
    }
    rss_start = rss_kb();
    printf("rss_start=%ld kB (after warm-up, process-level)\n",
           rss_start);
    printf("live_spans_start=%lu live_small_start=%lu\n",
           live_spans(), live_small());

    /* ---------- P1: per-class retention bound ---------- */
    printf("P1 per-class bound (hold 2*cap+1, free all, "
           "expect +2 retained spans per class)\n");
    for (i = 0; i < 20; i++) {
        size_t cs = class_sizes[i];
        size_t cap = ((size_t)page - header) / cs;
        size_t n = 2 * cap + 1;
        void **blocks = (void **)malloc(n * sizeof(void *));
        unsigned long before = live_spans();
        unsigned long pub_before = mimir_audit_span_published;
        unsigned long ret_before = mimir_audit_span_retired;
        size_t j, got = 0;
        char label[160];

        if (blocks == NULL) {
            check(0, "P1 harness allocation");
            continue;
        }
        for (j = 0; j < n; j++) {
            blocks[j] = mimir_malloc(cs);
            if (blocks[j] != NULL) {
                memset(blocks[j], 0x3C, cs);
                got++;
            }
        }
        snprintf(label, sizeof(label),
                 "P1 class %zu: all %zu blocks allocated", cs, n);
        check(got == n, label);
        snprintf(label, sizeof(label),
                 "P1 class %zu: spans published in phase == 3 "
                 "(observed %lu)",
                 cs, mimir_audit_span_published - pub_before);
        check(mimir_audit_span_published - pub_before == 3, label);
        for (j = 0; j < n; j++) {
            mimir_free(blocks[j]);
        }
        free(blocks);
        snprintf(label, sizeof(label),
                 "P1 class %zu: retained delta == +2 spans "
                 "(observed %+ld), retired in phase == 1 "
                 "(observed %lu)",
                 cs, (long)live_spans() - (long)before,
                 mimir_audit_span_retired - ret_before);
        check(live_spans() - before == 2 &&
              mimir_audit_span_retired - ret_before == 1, label);
        snprintf(label, sizeof(label),
                 "P1 class %zu: live small objects back to phase "
                 "start", cs);
        check(live_small() == 0, label);
        printf("  class %5zu cap %4zu blocks %5zu -> "
               "live_spans_total %lu\n",
               cs, cap, n, live_spans());
    }
    spans_bound_state = live_spans();
    printf("P1 end: live_spans=%lu (expect 40 = 2 x 20 classes), "
           "retained mapped bytes=%lu\n",
           spans_bound_state,
           spans_bound_state * (unsigned long)page);
    check(spans_bound_state == 40,
          "P1 total retained spans == 40 (2 per class x 20)");

    /* ---------- P2: churn stability ---------- */
    printf("P2 churn (10 rounds, class 128, 3*cap blocks)\n");
    {
        size_t cs = 128;
        size_t cap = ((size_t)page - header) / cs;
        size_t n = 3 * cap;
        void **blocks = (void **)malloc(n * sizeof(void *));
        int round;

        check(blocks != NULL, "P2 harness allocation");
        if (blocks != NULL) {
            for (round = 0; round < 10; round++) {
                size_t j;
                char label[160];

                for (j = 0; j < n; j++) {
                    blocks[j] = mimir_malloc(cs);
                    if (blocks[j] != NULL) {
                        memset(blocks[j], 0xC3, cs);
                    }
                }
                for (j = 0; j < n; j++) {
                    mimir_free(blocks[j]);
                }
                snprintf(label, sizeof(label),
                         "P2 round %d: live_spans == %lu "
                         "(observed %lu), live_small == 0",
                         round, spans_bound_state, live_spans());
                check(live_spans() == spans_bound_state &&
                      live_small() == 0, label);
            }
            free(blocks);
        }
        printf("P2 end: rss=%ld kB (fact only)\n", rss_kb());
    }

    /* ---------- P3: idle threads retain nothing extra --- */
    printf("P3 idle threads (%d threads x %d blocks of %zu, "
           "freed, then idle)\n",
           IDLE_THREADS, IDLE_BLOCKS, IDLE_SIZE);
    {
        pthread_t th[IDLE_THREADS];
        int created = 0;
        unsigned long spans_while_idle, spans_after_join;
        long rss_while_idle;
        char label[160];

        for (i = 0; i < IDLE_THREADS; i++) {
            if (pthread_create(&th[i], NULL, idle_worker,
                               NULL) == 0) {
                created++;
            }
        }
        check(created == IDLE_THREADS, "P3 all threads created");

        pthread_mutex_lock(&idle_mu);
        while (threads_idle < created) {
            pthread_cond_wait(&idle_cv, &idle_mu);
        }
        pthread_mutex_unlock(&idle_mu);

        spans_while_idle = live_spans();
        rss_while_idle = rss_kb();
        printf("P3 while idle: live_spans=%lu live_small=%lu "
               "rss=%ld kB\n",
               spans_while_idle, live_small(), rss_while_idle);
        snprintf(label, sizeof(label),
                 "P3 idle threads add no retention: live_spans "
                 "== bound state %lu (observed %lu)",
                 spans_bound_state, spans_while_idle);
        check(spans_while_idle == spans_bound_state &&
              live_small() == 0, label);

        pthread_mutex_lock(&idle_mu);
        threads_release = 1;
        pthread_cond_broadcast(&idle_cv);
        pthread_mutex_unlock(&idle_mu);
        for (i = 0; i < (size_t)created; i++) {
            pthread_join(th[i], NULL);
        }
        spans_after_join = live_spans();
        printf("P3 after join: live_spans=%lu\n",
               spans_after_join);
        check(spans_after_join == spans_while_idle,
              "P3 thread exit reclaims nothing further "
              "(reclaim already happened at free time, RP-3)");
    }

    /* ---------- P4: final accounting ---------- */
    rss_end = rss_kb();
    printf("P4 final: live_spans=%lu live_small=%lu "
           "retained_mapped_bytes=%lu rss_end=%ld kB "
           "rss_delta=%+ld kB (process-level fact)\n",
           live_spans(), live_small(),
           live_spans() * (unsigned long)page,
           rss_end, rss_end - rss_start);
    check(live_small() == 0, "P4 live small objects == 0");
    check(live_spans() == 40,
          "P4 retained spans == 40 (the RP-5 worst case, "
          "reached exactly)");
    printf("checks=%d failures=%d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
