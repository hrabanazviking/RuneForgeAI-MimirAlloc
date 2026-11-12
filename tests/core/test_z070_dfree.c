/* test_z070_dfree.c -- Z-070 double-free detection
 * evidence (docs/MIMIRALLOC_Z13_ROADMAP.md slice Z-070;
 * mechanism + costs in
 * tests/perf/recorded_run_z13_z070.txt).
 *
 * Legs, each in a forked child so detection counters
 * start at a known baseline per leg (the z066 pattern):
 *   0 control: valid churn -- same-thread and cross-
 *     thread alloc/free cycles, realloc growth -- moves
 *     NEITHER detection tally NOR the diag emitted
 *     counter. Detection never fires on a valid tree.
 *   1 tls-double-free: alloc/free/free on one thread.
 *     The second free is refused by bin membership:
 *     tally +1, one diag emission; the block is then
 *     handed out exactly once (two further allocs are
 *     distinct, patterns independent), further cycles
 *     clean, tally unmoved.
 *   2 central-double-free: a block allocated in main,
 *     freed in a worker thread (cross-thread course:
 *     owner record cleared by that free), freed again
 *     in main (no owner record: legacy central course).
 *     The J-006 walk refuses it; tally +1, one diag
 *     emission; allocator unharmed.
 *   3 poison: free a block (poisoned in the bin), write
 *     one byte into it (use-after-free write), realloc
 *     the class: the handout verifies the poison --
 *     tally +1, one diag emission -- and proceeds with
 *     the caller's byte intact (telemetry posture).
 *     A clean free/alloc cycle moves nothing.
 *   4 large-scope: a mapping-path block freed twice.
 *     Out of scope BY DESIGN (recorded in the run): the
 *     descriptor is retired and the mapping unmapped by
 *     the first free, so the second free is
 *     indistinguishable from a foreign pointer (the
 *     S8.3 posture) and tallies nothing. The allocator
 *     keeps working.
 *
 * Standalone: `make test-z070`; NOT part of `make test`
 * (the Z-064 standalone precedent).
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "mimir_alloc.h"
#include "mimir_stats_j002.h"
#include "mimir_diag_j007.h"

static int checks;
static int failures;

#define REQ(cond) do { \
        checks++; \
        if (!(cond)) { \
            failures++; \
            printf("    FAIL %s:%d: %s\n", \
                   __FILE__, __LINE__, #cond); \
            fflush(stdout); \
        } \
    } while (0)

static void *xalloc(size_t n)
{
    void *p = mimir_malloc(n);
    if (p == NULL) {
        printf("    FAIL: mimir_malloc(%zu) NULL\n", n);
        checks++;
        failures++;
    }
    return p;
}

/* Cross-thread free helper (leg 2). */
static void *g_free_target;
static void *free_in_thread(void *unused)
{
    (void)unused;
    mimir_free(g_free_target);
    return NULL;
}

static int leg_control(void)
{
    int i;
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    REQ(mimir_core_double_free_detections() == 0);
    REQ(mimir_core_double_free_poison_detections() == 0);
    for (i = 0; i < 2000; i++) {
        size_t n = 16 + (size_t)(i % 8) * 16; /* 16..128 */
        unsigned char *p = xalloc(n);
        if (p == NULL) {
            return 1;
        }
        memset(p, 0x5A, n);
        mimir_free(p);
    }
    /* Cross-thread frees of valid blocks: no tally. */
    for (i = 0; i < 64; i++) {
        pthread_t th;
        g_free_target = xalloc(48);
        if (g_free_target == NULL) {
            return 1;
        }
        memset(g_free_target, 0x33, 48);
        REQ(pthread_create(&th, NULL, free_in_thread,
                           NULL) == 0);
        REQ(pthread_join(th, NULL) == 0);
    }
    /* Realloc growth across classes: no tally. */
    {
        unsigned char *p = xalloc(24);
        if (p == NULL) {
            return 1;
        }
        memset(p, 0x77, 24);
        p = mimir_realloc(p, 200);
        REQ(p != NULL);
        if (p != NULL) {
            REQ(p[0] == 0x77 && p[23] == 0x77);
            mimir_free(p);
        }
    }
    REQ(mimir_core_double_free_detections() == 0);
    REQ(mimir_core_double_free_poison_detections() == 0);
    REQ(mimir_core_diag_emitted() == 0);
    printf("    control: 2000 same-thread + 64 cross-thread"
           " cycles + realloc, tallies 0/0 emitted 0\n");
    return failures ? 1 : 0;
}

static int leg_tls_double_free(void)
{
    unsigned char *p, *q1, *q2;
    struct mimir_stats st;
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    p = xalloc(32);
    if (p == NULL) {
        return 1;
    }
    memset(p, 0xAB, 32);
    mimir_free(p);
    REQ(mimir_core_double_free_detections() == 0);
    mimir_free(p); /* the double free: refused */
    REQ(mimir_core_double_free_detections() == 1);
    REQ(mimir_core_diag_emitted() == 1);
    REQ(mimir_core_stats_snapshot(&st) == 0);
    REQ(st.double_free_detections == 1);
    /* The block is in circulation exactly once. */
    q1 = xalloc(32);
    q2 = xalloc(32);
    REQ(q1 != NULL && q2 != NULL);
    REQ(q1 != q2);
    if (q1 != NULL && q2 != NULL) {
        memset(q1, 0x11, 32);
        memset(q2, 0x22, 32);
        REQ(q1[0] == 0x11 && q1[31] == 0x11);
        REQ(q2[0] == 0x22 && q2[31] == 0x22);
    }
    mimir_free(q1);
    mimir_free(q2);
    REQ(mimir_core_double_free_detections() == 1);
    REQ(mimir_core_double_free_poison_detections() == 0);
    printf("    tls-double-free: refused once, tally=1"
           " emitted=1, block handed out exactly once\n");
    return failures ? 1 : 0;
}

static int leg_central_double_free(void)
{
    pthread_t th;
    unsigned char *p;
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    p = xalloc(48);
    if (p == NULL) {
        return 1;
    }
    memset(p, 0x44, 48);
    g_free_target = p;
    REQ(pthread_create(&th, NULL, free_in_thread,
                       NULL) == 0);
    REQ(pthread_join(th, NULL) == 0);
    /* First free legitimately crossed to the central
     * course and cleared the owner record. */
    REQ(mimir_core_double_free_detections() == 0);
    mimir_free(p); /* second free: legacy central course */
    REQ(mimir_core_double_free_detections() == 1);
    REQ(mimir_core_diag_emitted() == 1);
    /* Allocator unharmed. */
    p = xalloc(48);
    REQ(p != NULL);
    if (p != NULL) {
        memset(p, 0x55, 48);
        REQ(p[47] == 0x55);
        mimir_free(p);
    }
    REQ(mimir_core_double_free_detections() == 1);
    printf("    central-double-free: refused once,"
           " tally=1 emitted=1, allocator unharmed\n");
    return failures ? 1 : 0;
}

static int leg_poison(void)
{
    unsigned char *p, *q;
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    p = xalloc(64);
    if (p == NULL) {
        return 1;
    }
    memset(p, 0x66, 64);
    mimir_free(p);
    /* Clean re-handout first: poison verifies, no tally. */
    q = xalloc(64);
    REQ(q == p);
    REQ(mimir_core_double_free_poison_detections() == 0);
    if (q == NULL) {
        return 1;
    }
    memset(q, 0x77, 64);
    mimir_free(q);
    q[39] = 0x41; /* UAF write: last byte of the capped
     * poison range [8, 40) -- see the boundary below */
    q = xalloc(64);
    REQ(q != NULL);
    REQ(mimir_core_double_free_poison_detections() == 1);
    REQ(mimir_core_diag_emitted() == 1);
    if (q != NULL) {
        /* Telemetry posture: the handout proceeded and
         * the caller's byte is exactly as written. */
        REQ(q[39] == 0x41);
        mimir_free(q);
    }
    /* Cap boundary, pinned as tested scope: q is back
     * in the bin (freed above); a UAF write at offset
     * 40 -- the first byte PAST the capped poison range
     * -- is NOT detected by design (the Z-070 economics
     * ruling: full-extent poison failed its adoption
     * bars, the capped range passed). */
    q[40] = 0x42;
    q = xalloc(64);
    REQ(q != NULL);
    REQ(mimir_core_double_free_poison_detections() == 1);
    REQ(mimir_core_diag_emitted() == 1);
    if (q != NULL) {
        REQ(q[40] == 0x42);
        mimir_free(q);
    }
    REQ(mimir_core_double_free_detections() == 0);
    printf("    poison: UAF write detected at handout,"
           " tally=1 emitted=1, byte intact, handout"
           " proceeded\n");
    return failures ? 1 : 0;
}

static int leg_large_scope(void)
{
    unsigned char *p;
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    p = xalloc(20000);
    if (p == NULL) {
        return 1;
    }
    memset(p, 0x88, 20000);
    mimir_free(p);
    mimir_free(p); /* indistinguishable from foreign */
    REQ(mimir_core_double_free_detections() == 0);
    REQ(mimir_core_diag_emitted() == 0);
    p = xalloc(20000);
    REQ(p != NULL);
    if (p != NULL) {
        memset(p, 0x99, 20000);
        REQ(p[19999] == 0x99);
        mimir_free(p);
    }
    printf("    large-scope: mapping double free tallies"
           " nothing (S8.3 posture, recorded), allocator"
           " unharmed\n");
    return failures ? 1 : 0;
}

struct leg {
    const char *name;
    int (*fn)(void);
};

static const struct leg LEGS[] = {
    { "control", leg_control },
    { "tls-double-free", leg_tls_double_free },
    { "central-double-free", leg_central_double_free },
    { "poison", leg_poison },
    { "large-scope", leg_large_scope },
};

int main(void)
{
    size_t i;
    int bad = 0;
    int total_checks = 0;
    printf("test_z070_dfree: Z-070 double-free detection,"
           " %zu legs, each in a disposable child\n",
           sizeof LEGS / sizeof LEGS[0]);
    fflush(stdout);
    for (i = 0; i < sizeof LEGS / sizeof LEGS[0]; i++) {
        pid_t pid = fork();
        int status = 0;
        if (pid == 0) {
            int r = LEGS[i].fn();
            printf("    leg %s: CHECKS: %d run, %d failed\n",
                   LEGS[i].name, checks, failures);
            fflush(stdout);
            _exit(r ? 1 : 0);
        }
        if (pid < 0 || waitpid(pid, &status, 0) != pid ||
            !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            bad++;
            printf("  [FAIL] leg %s (status=%d)\n",
                   LEGS[i].name, status);
        } else {
            printf("  [ok] leg %s\n", LEGS[i].name);
        }
        fflush(stdout);
    }
    (void)total_checks;
    printf("test_z070_dfree: legs=%zu failed=%d\n",
           sizeof LEGS / sizeof LEGS[0], bad);
    printf("RESULT: %s\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
