/*
 * test_h006.c -- evidence test for slice H-006 (Phase 8
 * thread cancellation + teardown, fork posture O-03/O-04;
 * slice record: docs/PHASE_8_CANCELLATION_TEARDOWN_H006.md;
 * governing plan: docs/PHASE_8_FOUNDATION_PLAN_H002.md
 * item S5 and Sections 4/5; predecessor proof:
 * docs/PHASE_7_THREAD_EXIT_G006.md, whose INC-G006-02/-04
 * left fork and cancellation inspection-only).
 *
 * Run standalone via `make test-h006` (NOT part of
 * `make test`). Links LIBOBJS (audit counters, drain) and
 * takes build/libmimiralloc.so as argv[1] for the dlclose
 * leg (test_h001 pattern).
 *
 * Phases (bounded: <= 8 threads at once):
 *  A cancel BETWEEN allocator calls: 4 workers hold
 *    16x64 small + 4x2048 mapping each, are cancelled at a
 *    nanosleep cancellation point; main (survivor)
 *    pattern-verifies and frees all 80 blocks; counters
 *    reconcile exactly.
 *  B cancel with consumer cleanup handlers: 3 workers
 *    hold 8x128 small + 2x4096 mapping each and push a
 *    pthread cleanup handler that verifies + frees their
 *    blocks; cancellation runs the handlers; counters
 *    reconcile exactly with frees done BY the handlers.
 *  C teardown shapes: leftovers of an exited thread and
 *    of a cancelled thread freed cross-thread; 4-thread
 *    churn; then mimir_core_drain_retained() releases
 *    exactly the retained empty spans (H-001 drain at
 *    teardown) and the allocator stays usable.
 *  D fork probes, all in disposable child processes:
 *    D1 quiescent fork (child allocates/frees, parent
 *    state untouched); D2 fork+exec /bin/true; D3 fork
 *    timed mid-churn (child allocator call under a 400 ms
 *    parent timeout -- a hang is the locked-mutex hazard
 *    observed, the child is SIGKILLed, evidence recorded);
 *    D4 async cancellation of a thread hammering allocator
 *    calls, in a child under a 5 s parent timeout (a hang
 *    is lock-death observed; never provoked in-process).
 *  E dlclose teardown: .so instance, 3 holders cancelled
 *    between calls, leftovers verified + freed via the
 *    .so, all freed, dlclose unmaps cleanly, twice.
 *
 * Deferred cancellation is the default and the allocator
 * contains no cancellation point (G-006 INC-G006-04), so
 * in-process cancellation is only ever delivered while a
 * worker waits OUTSIDE allocator calls. The inside-a-call
 * cases run only as D3/D4 disposable-child probes.
 * Exit 0 + RESULT: PASS only if every check passed.
 */
#define _POSIX_C_SOURCE 200809L
#include "mimir_alloc.h"
#include "mimir_small_g003.h"
#include "mimir_drain_h001.h"
#include "../abi/mimir_audit_e005.h"
#include <dlfcn.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int failures = 0;
static int checks = 0;
static void check(int cond, const char *label)
{
    checks++;
    printf("  [%s] %s\n", cond ? "ok" : "FAIL", label);
    if (!cond) failures++;
}
static uint8_t pat_byte(int salt, int tid, int i)
{
    return (uint8_t)(0x5Au ^ (unsigned)(salt * 67) ^
                     (unsigned)(tid * 31) ^ (unsigned)(i * 7));
}
static void fill_block(void *p, size_t sz, int salt, int tid, int i)
{ memset(p, pat_byte(salt, tid, i), sz); }
static int verify_block(const void *p, size_t sz, int salt,
                        int tid, int i)
{
    const uint8_t *b = (const uint8_t *)p;
    uint8_t want = pat_byte(salt, tid, i);
    size_t k;
    for (k = 0; k < sz; k++) if (b[k] != want) return 0;
    return 1;
}
static void wait_loop(void)
{
    for (;;) {
        struct timespec ts = { 0, 10000000L };
        nanosleep(&ts, NULL);
    }
}
static void msleep(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}
struct counters {
    unsigned long cm, cc, cr, cf, pub, ret, spub, sret,
                  salloc, sfree;
};
static struct counters snap(void)
{
    struct counters c;
    c.cm = mimir_audit_call_malloc;
    c.cc = mimir_audit_call_calloc;
    c.cr = mimir_audit_call_realloc;
    c.cf = mimir_audit_call_free;
    c.pub = mimir_audit_alloc_published;
    c.ret = mimir_audit_mapping_retired;
    c.spub = mimir_audit_span_published;
    c.sret = mimir_audit_span_retired;
    c.salloc = mimir_audit_small_alloc;
    c.sfree = mimir_audit_small_free;
    return c;
}

/* ---------------- Phase A: cancel between calls -------- */
#define A_THREADS 4
#define A_SMALL 16
#define A_LARGE 4
#define A_N (A_SMALL + A_LARGE)
static void *g_a[A_THREADS][A_N];
static size_t g_a_sz[A_THREADS][A_N];
static volatile int g_a_ready[A_THREADS];
static void *worker_a(void *p)
{
    int t = (int)(intptr_t)p;
    int i;
    for (i = 0; i < A_N; i++) {
        size_t sz = (i < A_SMALL) ? 64 : 2048;
        void *ptr = mimir_malloc(sz);
        g_a[t][i] = ptr;
        g_a_sz[t][i] = sz;
        if (ptr) fill_block(ptr, sz, 1, t, i);
    }
    g_a_ready[t] = 1;
    wait_loop();
    return NULL;
}
static int wait_ready(volatile int *flags, int n, int secs)
{
    int waited = 0, t, all;
    while (waited < secs * 100) {
        all = 1;
        for (t = 0; t < n; t++) if (!flags[t]) all = 0;
        if (all) return 1;
        msleep(10);
        waited++;
    }
    return 0;
}
static void phase_a(void)
{
    pthread_t th[A_THREADS];
    struct counters b0, b1, b2;
    int t, i, cancelled = 0, pat = 0, nulls = 0;
    char label[160];
    printf("phase A: cancel between allocator calls\n");
    b0 = snap();
    for (t = 0; t < A_THREADS; t++)
        pthread_create(&th[t], NULL, worker_a, (void *)(intptr_t)t);
    check(wait_ready(g_a_ready, A_THREADS, 10),
          "A: all 4 workers holding blocks and waiting");
    for (t = 0; t < A_THREADS; t++) pthread_cancel(th[t]);
    for (t = 0; t < A_THREADS; t++) {
        void *st = NULL;
        pthread_join(th[t], &st);
        if (st == PTHREAD_CANCELED) cancelled++;
    }
    snprintf(label, sizeof(label),
             "A: join status PTHREAD_CANCELED for %d/4", cancelled);
    check(cancelled == A_THREADS, label);
    b1 = snap();
    check(b1.salloc - b0.salloc == 64 && b1.pub - b0.pub == 16 &&
          b1.sfree - b0.sfree == 0 && b1.ret - b0.ret == 0,
          "A: cancelled blocks still live (64 small, 16 mappings)");
    for (t = 0; t < A_THREADS; t++) {
        for (i = 0; i < A_N; i++) {
            if (!g_a[t][i]) { nulls++; continue; }
            if (!verify_block(g_a[t][i], g_a_sz[t][i], 1, t, i))
                pat++;
        }
    }
    check(nulls == 0, "A: no null allocations");
    check(pat == 0, "A: all 80 patterns intact after cancel");
    for (t = 0; t < A_THREADS; t++)
        for (i = 0; i < A_N; i++) mimir_free(g_a[t][i]);
    b2 = snap();
    check(b2.cm - b0.cm == 80 && b2.cf - b0.cf == 80 &&
          b2.salloc - b0.salloc == 64 &&
          b2.sfree - b0.sfree == 64 &&
          b2.pub - b0.pub == 16 && b2.ret - b0.ret == 16,
          "A: counters reconcile exactly after survivor frees");
}

/* ------------- Phase B: cleanup handlers free ---------- */
#define B_THREADS 3
#define B_SMALL 8
#define B_LARGE 2
#define B_N (B_SMALL + B_LARGE)
struct b_slot {
    void *ptrs[B_N];
    size_t szs[B_N];
    int tid, handler_ran, handler_freed, handler_pat;
    volatile int ready;
};
static struct b_slot g_b[B_THREADS];
static void cleanup_b(void *p)
{
    struct b_slot *s = (struct b_slot *)p;
    int i;
    s->handler_ran = 1;
    for (i = 0; i < B_N; i++) {
        if (!s->ptrs[i]) continue;
        if (!verify_block(s->ptrs[i], s->szs[i], 2, s->tid, i))
            s->handler_pat++;
        mimir_free(s->ptrs[i]);
        s->ptrs[i] = NULL;
        s->handler_freed++;
    }
}
static void *worker_b(void *p)
{
    struct b_slot *s = (struct b_slot *)p;
    int i;
    for (i = 0; i < B_N; i++) {
        size_t sz = (i < B_SMALL) ? 128 : 4096;
        void *ptr = mimir_malloc(sz);
        s->ptrs[i] = ptr;
        s->szs[i] = sz;
        if (ptr) fill_block(ptr, sz, 2, s->tid, i);
    }
    pthread_cleanup_push(cleanup_b, s);
    s->ready = 1;
    wait_loop();
    pthread_cleanup_pop(0);
    return NULL;
}
static void phase_b(void)
{
    pthread_t th[B_THREADS];
    struct counters b0, b1;
    volatile int *flags[B_THREADS];
    int t, cancelled = 0, ran = 0, freed = 0, pat = 0, allr = 1;
    char label[160];
    printf("phase B: cancel with cleanup handlers freeing\n");
    (void)flags;
    b0 = snap();
    for (t = 0; t < B_THREADS; t++) {
        g_b[t].tid = t;
        pthread_create(&th[t], NULL, worker_b, &g_b[t]);
    }
    {
        int waited = 0;
        while (waited < 1000) {
            allr = 1;
            for (t = 0; t < B_THREADS; t++)
                if (!g_b[t].ready) allr = 0;
            if (allr) break;
            msleep(10);
            waited++;
        }
    }
    check(allr, "B: all 3 workers holding blocks, handlers pushed");
    for (t = 0; t < B_THREADS; t++) pthread_cancel(th[t]);
    for (t = 0; t < B_THREADS; t++) {
        void *st = NULL;
        pthread_join(th[t], &st);
        if (st == PTHREAD_CANCELED) cancelled++;
        ran += g_b[t].handler_ran;
        freed += g_b[t].handler_freed;
        pat += g_b[t].handler_pat;
    }
    snprintf(label, sizeof(label),
             "B: PTHREAD_CANCELED %d/3, handlers ran %d/3", cancelled, ran);
    check(cancelled == B_THREADS && ran == B_THREADS, label);
    snprintf(label, sizeof(label), "B: handlers freed %d/30 blocks", freed);
    check(freed == B_THREADS * B_N, label);
    check(pat == 0, "B: handler-side pattern failures == 0");
    b1 = snap();
    check(b1.cm - b0.cm == 30 && b1.cf - b0.cf == 30 &&
          b1.salloc - b0.salloc == 24 && b1.sfree - b0.sfree == 24 &&
          b1.pub - b0.pub == 6 && b1.ret - b0.ret == 6,
          "B: counters reconcile exactly (frees by handlers)");
    check(b1.salloc - b1.sfree == b0.salloc - b0.sfree &&
          b1.pub - b1.ret == b0.pub - b0.ret,
          "B: nothing live after handler frees");
}

/* ------------- Phase C: teardown shapes ---------------- */
static void *g_c_ptrs[2][8];
static size_t g_c_szs[2][8];
static volatile int g_c_ready[1];
static void fill_c(int slot, int salt)
{
    int i;
    for (i = 0; i < 8; i++) {
        size_t sz = (i < 6) ? 96 : 2048;
        void *ptr = mimir_malloc(sz);
        g_c_ptrs[slot][i] = ptr;
        g_c_szs[slot][i] = sz;
        if (ptr) fill_block(ptr, sz, salt, slot, i);
    }
}
static void *worker_c_exit(void *unused)
{ (void)unused; fill_c(0, 3); return NULL; }
static void *worker_c_cancel(void *unused)
{ (void)unused; fill_c(1, 4); g_c_ready[0] = 1; wait_loop(); return NULL; }
static volatile int g_churn_stop;
static long g_churn_fail[4];
static void *worker_churn(void *p)
{
    int t = (int)(intptr_t)p;
    int i = 0;
    while (!g_churn_stop) {
        size_t sz = (i % 2 == 0) ? 128 : 2048;
        void *ptr = mimir_malloc(sz);
        if (!ptr) { g_churn_fail[t]++; break; }
        fill_block(ptr, sz, 5, t, i % 64);
        if (!verify_block(ptr, sz, 5, t, i % 64)) g_churn_fail[t]++;
        mimir_free(ptr);
        i++;
    }
    return NULL;
}
static void phase_c(void)
{
    pthread_t th, ch[4];
    struct counters b0, b1, b2;
    int t, i, pat = 0;
    printf("phase C: teardown (leftovers, churn, drain)\n");
    b0 = snap();
    pthread_create(&th, NULL, worker_c_exit, NULL);
    pthread_join(th, NULL);
    pthread_create(&th, NULL, worker_c_cancel, NULL);
    check(wait_ready(g_c_ready, 1, 10), "C: cancel worker holding");
    pthread_cancel(th);
    {
        void *st = NULL;
        pthread_join(th, &st);
        check(st == PTHREAD_CANCELED, "C: leftover owner cancelled");
    }
    for (t = 0; t < 2; t++)
        for (i = 0; i < 8; i++)
            if (g_c_ptrs[t][i] &&
                !verify_block(g_c_ptrs[t][i], g_c_szs[t][i],
                              t == 0 ? 3 : 4, t, i))
                pat++;
    check(pat == 0, "C: exited+cancelled leftovers intact (16)");
    for (t = 0; t < 2; t++)
        for (i = 0; i < 8; i++) mimir_free(g_c_ptrs[t][i]);
    b1 = snap();
    check(b1.cm - b0.cm == 16 && b1.cf - b0.cf == 16 &&
          b1.salloc - b0.salloc == 12 && b1.sfree - b0.sfree == 12 &&
          b1.pub - b0.pub == 4 && b1.ret - b0.ret == 4,
          "C: leftover counters reconcile exactly");
    g_churn_stop = 0;
    memset((void *)g_churn_fail, 0, sizeof(g_churn_fail));
    for (t = 0; t < 4; t++)
        pthread_create(&ch[t], NULL, worker_churn, (void *)(intptr_t)t);
    msleep(300);
    g_churn_stop = 1;
    for (t = 0; t < 4; t++) pthread_join(ch[t], NULL);
    {
        long f = 0;
        for (t = 0; t < 4; t++) f += g_churn_fail[t];
        check(f == 0, "C: churn failures == 0");
    }
    b2 = snap();
    check(b2.salloc - b2.sfree == b0.salloc - b0.sfree &&
          b2.pub - b2.ret == b0.pub - b0.ret,
          "C: nothing live after churn");
    {
        unsigned long live = b2.spub - b2.sret;
        size_t drained = mimir_core_drain_retained();
        struct counters b3 = snap();
        char label[128];
        snprintf(label, sizeof(label),
                 "C: drain released %zu of %lu retained spans",
                 drained, live);
        check(drained == (size_t)live, label);
        check(b3.spub == b3.sret, "C: span pub == ret after drain");
        check(mimir_core_drain_retained() == 0,
              "C: second drain no-op");
    }
    {
        void *p = mimir_malloc(64);
        check(p != NULL, "C: allocator usable after drain");
        if (p) { fill_block(p, 64, 6, 0, 0);
                 check(verify_block(p, 64, 6, 0, 0),
                       "C: post-drain pattern intact");
                 mimir_free(p); }
    }
}

/* ------------- Phase D: fork probes -------------------- */
/* Wait up to ms for pid; return 1 if exited (status set), 0 on timeout. */
static int wait_timeout(pid_t pid, int ms, int *status)
{
    int waited = 0;
    while (waited < ms) {
        pid_t r = waitpid(pid, status, WNOHANG);
        if (r == pid) return 1;
        if (r < 0) return 1;
        msleep(10);
        waited += 10;
    }
    return 0;
}
static void *worker_hammer(void *unused)
{
    int old;
    (void)unused;
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &old);
    for (;;) {
        void *p = mimir_malloc(64);
        if (p) { fill_block(p, 64, 7, 0, 0); mimir_free(p); }
    }
    return NULL;
}
static int g_d3_hung, g_d3_ok, g_d3_crash, g_d3_tries;
static int g_d4_hung, g_d4_ok, g_d4_other;
static void phase_d(void)
{
    struct counters b0, b1;
    printf("phase D: fork probes (disposable children)\n");
    /* D1: quiescent fork. */
    b0 = snap();
    {
        void *mine[6];
        int i, pat = 0;
        pid_t pid;
        int status = 0;
        for (i = 0; i < 6; i++) {
            size_t sz = (i < 4) ? 64 : 2048;
            mine[i] = mimir_malloc(sz);
            if (mine[i]) fill_block(mine[i], sz, 8, 0, i);
        }
        pid = fork();
        if (pid == 0) {
            void *own[6];
            int j;
            for (j = 0; j < 6; j++) {
                size_t sz = (j < 4) ? 64 : 2048;
                if (!mine[j] ||
                    !verify_block(mine[j], sz, 8, 0, j)) _exit(11);
                own[j] = mimir_malloc(sz);
                if (!own[j]) _exit(12);
                fill_block(own[j], sz, 9, 0, j);
                if (!verify_block(own[j], sz, 9, 0, j)) _exit(11);
            }
            for (j = 0; j < 6; j++) mimir_free(own[j]);
            _exit(0);
        }
        check(pid > 0, "D1: fork succeeded");
        if (pid > 0) {
            check(wait_timeout(pid, 5000, &status) &&
                  WIFEXITED(status) && WEXITSTATUS(status) == 0,
                  "D1: quiescent child allocated/freed, exit 0");
        }
        for (i = 0; i < 6; i++) {
            size_t sz = (i < 4) ? 64 : 2048;
            if (!mine[i] || !verify_block(mine[i], sz, 8, 0, i))
                pat++;
        }
        check(pat == 0, "D1: parent blocks intact after fork");
        for (i = 0; i < 6; i++) mimir_free(mine[i]);
        b1 = snap();
        check(b1.cm - b0.cm == 6 && b1.cf - b0.cf == 6 &&
              b1.salloc - b0.salloc == 4 &&
              b1.sfree - b0.sfree == 4 &&
              b1.pub - b0.pub == 2 && b1.ret - b0.ret == 2,
              "D1: parent counters moved only by parent ops");
    }
    /* D2: fork + exec only. */
    {
        pid_t pid = fork();
        int status = 0;
        if (pid == 0) {
            execl("/bin/true", "true", (char *)NULL);
            _exit(127);
        }
        check(pid > 0 && wait_timeout(pid, 5000, &status) &&
              WIFEXITED(status) && WEXITSTATUS(status) == 0,
              "D2: fork+exec /bin/true exit 0");
    }
    /* D3: fork timed mid-churn; child call under timeout. */
    {
        pthread_t ch[4];
        int t, attempt;
        g_churn_stop = 0;
        for (t = 0; t < 4; t++)
            pthread_create(&ch[t], NULL, worker_churn,
                           (void *)(intptr_t)t);
        for (attempt = 0; attempt < 24; attempt++) {
            pid_t pid;
            int status = 0;
            g_d3_tries++;
            pid = fork();
            if (pid == 0) {
                void *p = mimir_malloc(64);
                if (!p) _exit(13);
                fill_block(p, 64, 10, 0, 0);
                if (!verify_block(p, 64, 10, 0, 0)) _exit(11);
                mimir_free(p);
                _exit(0);
            }
            if (pid < 0) { g_d3_crash++; break; }
            if (!wait_timeout(pid, 400, &status)) {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                g_d3_hung++;
                break;
            }
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
                g_d3_ok++;
            else
                g_d3_crash++;
        }
        g_churn_stop = 1;
        for (t = 0; t < 4; t++) pthread_join(ch[t], NULL);
        printf("  D3: tries=%d ok=%d hung=%d crash=%d\n",
               g_d3_tries, g_d3_ok, g_d3_hung, g_d3_crash);
        check(g_d3_tries > 0, "D3: mid-churn fork probe ran");
        check(g_d3_crash == 0, "D3: no child crashed");
    }
    /* D4: async cancel inside allocator calls, in child. */
    {
        int trial;
        for (trial = 0; trial < 3; trial++) {
            pid_t pid = fork();
            int status = 0;
            if (pid == 0) {
                pthread_t w;
                void *p;
                pthread_create(&w, NULL, worker_hammer, NULL);
                msleep(50);
                pthread_cancel(w);
                pthread_join(w, NULL);
                p = mimir_malloc(64);
                if (!p) _exit(14);
                fill_block(p, 64, 11, 0, 0);
                if (!verify_block(p, 64, 11, 0, 0)) _exit(11);
                mimir_free(p);
                _exit(0);
            }
            if (pid < 0) { g_d4_other++; continue; }
            if (!wait_timeout(pid, 5000, &status)) {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                g_d4_hung++;
            } else if (WIFEXITED(status) &&
                       WEXITSTATUS(status) == 0) {
                g_d4_ok++;
            } else {
                g_d4_other++;
            }
        }
        printf("  D4: trials=3 ok=%d hung=%d other=%d\n",
               g_d4_ok, g_d4_hung, g_d4_other);
        check(g_d4_ok + g_d4_hung + g_d4_other == 3,
              "D4: async-cancel probe completed 3 trials");
    }
    {
        void *p = mimir_malloc(64);
        check(p != NULL, "D: test process allocator healthy after probes");
        if (p) mimir_free(p);
    }
}

/* ------------- Phase E: dlclose teardown --------------- */
typedef void *(*malloc_fn)(size_t);
typedef void (*free_fn)(void *);
static malloc_fn g_dm;
static free_fn g_df;
static void *g_e[3][10];
static size_t g_e_sz[3][10];
static volatile int g_e_ready[3];
static void *worker_e(void *p)
{
    int t = (int)(intptr_t)p;
    int i;
    for (i = 0; i < 10; i++) {
        size_t sz = (i < 8) ? 256 : 2048;
        void *ptr = g_dm(sz);
        g_e[t][i] = ptr;
        g_e_sz[t][i] = sz;
        if (ptr) fill_block(ptr, sz, 12, t, i);
    }
    g_e_ready[t] = 1;
    wait_loop();
    return NULL;
}
static int count_image(const char *path)
{
    FILE *f = fopen("/proc/self/maps", "r");
    char line[512];
    int n = 0;
    (void)path;
    if (!f) return -1;
    while (fgets(line, sizeof(line), f))
        if (strstr(line, "libmimiralloc.so")) n++;
    fclose(f);
    return n;
}
static void phase_e(const char *path)
{
    int cycle;
    printf("phase E: dlclose teardown of .so instance\n");
    check(count_image(path) == 0, "E: no .so image before dlopen");
    for (cycle = 0; cycle < 2; cycle++) {
        void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        pthread_t th[3];
        int t, i, cancelled = 0, pat = 0;
        char label[128];
        check(h != NULL, "E: dlopen succeeds");
        if (!h) return;
        g_dm = (malloc_fn)dlsym(h, "mimir_malloc");
        g_df = (free_fn)dlsym(h, "mimir_free");
        check(g_dm != NULL && g_df != NULL, "E: dlsym x2 succeeds");
        if (cycle == 0) {
            memset((void *)g_e_ready, 0, sizeof(g_e_ready));
            for (t = 0; t < 3; t++)
                pthread_create(&th[t], NULL, worker_e,
                               (void *)(intptr_t)t);
            check(wait_ready(g_e_ready, 3, 10),
                  "E: 3 .so holders waiting");
            for (t = 0; t < 3; t++) pthread_cancel(th[t]);
            for (t = 0; t < 3; t++) {
                void *st = NULL;
                pthread_join(th[t], &st);
                if (st == PTHREAD_CANCELED) cancelled++;
            }
            check(cancelled == 3, "E: all 3 holders cancelled");
            for (t = 0; t < 3; t++)
                for (i = 0; i < 10; i++)
                    if (!g_e[t][i] ||
                        !verify_block(g_e[t][i], g_e_sz[t][i],
                                      12, t, i))
                        pat++;
            check(pat == 0, "E: cancelled leftovers intact (30)");
            for (t = 0; t < 3; t++)
                for (i = 0; i < 10; i++) g_df(g_e[t][i]);
        } else {
            void *p = g_dm(256);
            check(p != NULL, "E: cycle-2 alloc succeeds");
            if (p) { fill_block(p, 256, 13, 0, 0);
                     check(verify_block(p, 256, 13, 0, 0),
                           "E: cycle-2 pattern intact");
                     g_df(p); }
        }
        snprintf(label, sizeof(label),
                 "E: dlclose cycle %d returns 0", cycle);
        check(dlclose(h) == 0, label);
        snprintf(label, sizeof(label),
                 "E: image unmapped after dlclose cycle %d", cycle);
        check(count_image(path) == 0, label);
    }
}
int main(int argc, char **argv)
{
    struct counters start, now;
    alarm(240);
    printf("test_h006: Phase 8 cancellation + teardown (H-006)\n");
    if (argc < 2) {
        printf("usage: test_h006 <path to libmimiralloc.so>\n");
        return 2;
    }
    start = snap();
    check(start.salloc == start.sfree && start.pub == start.ret,
          "start: no live small objects or mappings");
    phase_a();
    phase_b();
    phase_c();
    phase_d();
    phase_e(argv[1]);
    now = snap();
    check(now.salloc - now.sfree == 0 && now.pub - now.ret == 0,
          "final: no live small objects or mappings");
    printf("D3 fork-mid-churn: tries=%d ok=%d hung=%d crash=%d\n",
           g_d3_tries, g_d3_ok, g_d3_hung, g_d3_crash);
    printf("D4 async-cancel: ok=%d hung=%d other=%d of 3\n",
           g_d4_ok, g_d4_hung, g_d4_other);
    printf("checks=%d failures=%d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
