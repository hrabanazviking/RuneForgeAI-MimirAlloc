/* probe_z044.c - Z-044 Fork posture proof on this host.
 *
 * Re-runs the Phase 8 fork posture (H-006, docs/
 * PHASE_8_CANCELLATION_TEARDOWN_H006.md; limitation L-08)
 * on x86-64 against the CURRENT core, which since H-006
 * gained the Z-029 TLS freelist cache and Z-030 remote-free
 * routing. No pthread_atfork handler exists in src/ (grep-
 * verified for this slice); the core keeps ONE global mutex
 * (mimir_platform_global_lock in src/platform).
 *
 * Posture under proof (L-08): fork is supported ONLY (1)
 * quiescent - no thread inside an allocator call at the
 * fork instant - after which the child may allocate and
 * free normally, and (2) fork followed promptly by exec.
 * Fork-while-busy is UNSUPPORTED for allocator use in the
 * child.
 *
 * Phases (mimir leg; the glibc leg -DLEG_GLIBC runs only
 * the exec and busy shapes as a same-shape comparator and
 * prints SKIP for the posture phases):
 *  Q1 quiescent single-thread fork: main primes its TLS
 *     bins, holds patterned small+large blocks; the child
 *     verifies patterns, reads its inherited TLS snapshot
 *     (lock-free by contract), allocates (bin hit expected)
 *     and frees its own small+large blocks, frees one
 *     inherited block, _exit(0). Parent then proves its
 *     own blocks and its stats counters were not moved by
 *     the child (the child's increments die with its copy).
 *  Q2 quiescent multithread fork: 3 workers alive but
 *     parked OUTSIDE allocator calls (ready flag set,
 *     progress counters sampled frozen), holding patterned
 *     blocks with populated TLS bins. The child verifies
 *     every pattern, drains its OWN (forking thread's)
 *     bins, frees every inherited block incl. the vanished
 *     workers' blocks (cross-thread frees), then measures
 *     mimir_core_drain_retained(): spans whose binned
 *     tally belongs to vanished threads can never be
 *     released in the child - the stranding measurement
 *     (INC-Z044-01).
 *  E1 fork+exec /bin/true, quiescent, 16 tries.
 *  E2 fork+exec /bin/true while 4 churners hammer, 32
 *     tries; the child calls ONLY execve (async-safe road).
 *  B0 staged boundary (mimir only): a helper thread takes
 *     mimir_platform_global_lock() itself, so the lock is
 *     PROVABLY held at the fork instant (B0a, 3 tries), or
 *     provably released before it (B0b, 3 tries). Children
 *     run the staged B1 protocol. B0a is the deterministic
 *     form of the boundary: the small stage can be served
 *     lock-free from the forking thread's inherited bins
 *     (snapshot hit delta measured in the child), the
 *     large stage takes the central lock and cannot
 *     complete while the inherited copy stays locked.
 *  B1 fork-while-busy boundary measurement: 4 churners
 *     hammering; 24 disposable children. Each child writes
 *     'S' to a pipe after a small (64 B) alloc+touch+free,
 *     then the hit-evidence byte ('H' if the TLS snapshot
 *     hit counter moved, 'M' if not, '-' glibc leg), then
 *     'L' after a large (2048 B) alloc+touch+free, then
 *     _exit(0). Parent allows 500 ms per child (poll +
 *     waitpid WNOHANG watchdog), then SIGKILLs and reaps.
 *     Classes: BOTH_OK, SMALL_OK_LARGE_HUNG (S seen, no L,
 *     killed), SMALL_HUNG (no S, killed), CRASH (signalled
 *     other than by our SIGKILL, or nonzero exit), ANOMALY
 *     (exit 0 with missing stage bytes). HUNG means only:
 *     no completion within 500 ms (H-006 used 400 ms).
 *  B2 busy control: churners running, 8 children _exit(0)
 *     immediately with NO allocator call and NO exec -
 *     isolates "fork itself" from "allocator use in the
 *     child". Must be 8/8.
 *
 * Discipline (H-006 + risk review): anything that can
 * deadlock runs in a disposable child under a parent
 * timeout; children use only write() + _exit (no stdio -
 * the parent fflushes before every fork); every child
 * sets PR_SET_PDEATHSIG; kills target the exact fork()
 * pid and are reaped before the next try. Churners run a
 * bounded alloc/free loop (nothing retained). All
 * allocator calls go through volatile function pointers.
 * Patterns are a pure function of (salt, index); checksums
 * read only bytes the probe wrote. Exit 0 iff every
 * EXECUTED proof phase (Q1, Q2, E1, E2, B0b, B2) passed;
 * B1/B0a outcomes are measurements, never failures.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef LEG_GLIBC
static void *(*volatile vp_malloc)(size_t) = malloc;
static void (*volatile vp_free)(void *) = free;
static const char *LEG = "glibc";
#else
#include "mimir_alloc.h"
#include "core/mimir_core.h"
#include "platform/mimir_platform.h"
#include "../contract/mimir_tls_cache_z028.h"
static void *(*volatile vp_malloc)(size_t) = mimir_malloc;
static void (*volatile vp_free)(void *) = mimir_free;
static const char *LEG = "mimir";
#endif

#define SMALL_SZ 64u
#define LARGE_SZ 2048u
#define TIMEOUT_MS 500
#define E1_TRIES 16
#define E2_TRIES 32
#define B0_TRIES 3
#define B1_TRIES 24
#define B2_TRIES 8
#define NCHURN 4
#define NWORK 3

static int failures = 0;

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
}
static void msleep(long ms) {
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}
#ifndef LEG_GLIBC
static unsigned char pat(unsigned salt, size_t i) {
    return (unsigned char)((salt * 67u + (unsigned)i * 7u + 13u) & 0xffu);
}
static void fill_block(void *p, size_t sz, unsigned salt) {
    unsigned char *b = (unsigned char *)p;
    for (size_t i = 0; i < sz; i++) b[i] = pat(salt, i);
}
static int verify_block(const void *p, size_t sz, unsigned salt) {
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < sz; i++) if (b[i] != pat(salt, i)) return 0;
    return 1;
}
/* Child-side line output: stack buffer + write(), no stdio. */
static void child_fmt(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n > 0) {
        ssize_t w = write(STDOUT_FILENO, buf, (size_t)n);
        (void)w;
    }
}
#endif /* mimir-only pattern/child-output helpers */

/* ---------- churners (busy phases) ---------- */
struct churner {
    volatile int stop;
    volatile unsigned long iters;
};
static struct churner g_churn[NCHURN];
static void *churn_main(void *arg) {
    struct churner *c = (struct churner *)arg;
    unsigned k = 0;
    while (!c->stop) {
        void *p = vp_malloc(SMALL_SZ);
        if (p) { ((volatile unsigned char *)p)[0] = (unsigned char)k; vp_free(p); }
        if ((k & 15u) == 0) {
            void *q = vp_malloc(LARGE_SZ);
            if (q) { ((volatile unsigned char *)q)[0] = (unsigned char)k; vp_free(q); }
        }
        k++;
        c->iters = k;
    }
    return NULL;
}
static unsigned long churn_total(void) {
    unsigned long t = 0;
    for (int i = 0; i < NCHURN; i++) t += g_churn[i].iters;
    return t;
}
static pthread_t g_churn_th[NCHURN];
static void churn_start(void) {
    for (int i = 0; i < NCHURN; i++) { g_churn[i].stop = 0; g_churn[i].iters = 0; }
    for (int i = 0; i < NCHURN; i++)
        if (pthread_create(&g_churn_th[i], NULL, churn_main, &g_churn[i]) != 0) {
            printf("Z044,%s,SYS,churn_create_failed=1\n", LEG); failures++;
        }
}
static void churn_stop(void) {
    for (int i = 0; i < NCHURN; i++) g_churn[i].stop = 1;
    for (int i = 0; i < NCHURN; i++) pthread_join(g_churn_th[i], NULL);
}

/* ---------- staged child runner (B0/B1) ---------- */
enum stage_class { SC_BOTH_OK, SC_SMALL_OK_LARGE_HUNG, SC_SMALL_HUNG,
                   SC_CRASH, SC_ANOMALY };
static const char *sc_name(enum stage_class c) {
    switch (c) {
    case SC_BOTH_OK: return "BOTH_OK";
    case SC_SMALL_OK_LARGE_HUNG: return "SMALL_OK_LARGE_HUNG";
    case SC_SMALL_HUNG: return "SMALL_HUNG";
    case SC_CRASH: return "CRASH";
    default: return "ANOMALY";
    }
}
/* The staged child body: runs in the forked child, writes
 * stage bytes to wfd, _exit(0) at the end. Never returns. */
static void staged_child(int wfd) {
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    char b;
#ifndef LEG_GLIBC
    struct mimir_tls_cache_snapshot s0, s1;
    int have0 = (mimir_tls_cache_snapshot(&s0) == 0);
#endif
    void *p = vp_malloc(SMALL_SZ);
    if (!p) _exit(43);
    ((volatile unsigned char *)p)[0] = 0xA5u;
    if (((volatile unsigned char *)p)[0] != 0xA5u) _exit(44);
    vp_free(p);
    b = 'S'; if (write(wfd, &b, 1) < 0) _exit(45);
#ifndef LEG_GLIBC
    char hb = '-';
    if (have0 && mimir_tls_cache_snapshot(&s1) == 0)
        hb = (s1.hits > s0.hits) ? 'H' : 'M';
#else
    char hb = '-';
#endif
    if (write(wfd, &hb, 1) < 0) _exit(45);
    void *q = vp_malloc(LARGE_SZ);
    if (!q) _exit(43);
    ((volatile unsigned char *)q)[0] = 0x5Au;
    if (((volatile unsigned char *)q)[0] != 0x5Au) _exit(44);
    vp_free(q);
    b = 'L'; if (write(wfd, &b, 1) < 0) _exit(45);
    _exit(0);
}
struct stage_result {
    enum stage_class cls;
    int got_s, got_l, hitbyte; /* hitbyte: 'H','M','-' or 0 */
    char state_at_kill;        /* /proc state char or '-' */
};
/* Fork one staged child, watch it for TIMEOUT_MS, classify. */
static void run_staged(struct stage_result *r) {
    int fds[2];
    r->got_s = r->got_l = r->hitbyte = 0; r->state_at_kill = '-';
    if (pipe(fds) != 0) { r->cls = SC_ANOMALY; return; }
    fflush(NULL);
    pid_t pid = fork();
    if (pid == 0) {
        close(fds[0]);
        staged_child(fds[1]);
        _exit(127);
    }
    close(fds[1]);
    if (pid < 0) { close(fds[0]); r->cls = SC_ANOMALY; return; }
    char buf[8]; size_t nbuf = 0;
    int status = 0, killed = 0;
    uint64_t deadline = now_ms() + TIMEOUT_MS;
    for (;;) {
        struct pollfd pfd = { fds[0], POLLIN, 0 };
        int pr = poll(&pfd, 1, 10);
        if (pr > 0 && (pfd.revents & (POLLIN | POLLHUP))) {
            ssize_t n = read(fds[0], buf + nbuf, sizeof buf - nbuf);
            if (n > 0) nbuf += (size_t)n;
        }
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) { break; }
        if (now_ms() >= deadline) {
            char sp[64];
            snprintf(sp, sizeof sp, "/proc/%d/stat", (int)pid);
            FILE *f = fopen(sp, "r");
            if (f) {
                char sbuf[256];
                if (fgets(sbuf, sizeof sbuf, f)) {
                    char *rp = strrchr(sbuf, ')');
                    if (rp && rp[1] == ' ' && rp[2]) r->state_at_kill = rp[2];
                }
                fclose(f);
            }
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            killed = 1;
            /* drain whatever arrived before the kill */
            struct pollfd pfd2 = { fds[0], POLLIN, 0 };
            if (poll(&pfd2, 1, 20) > 0) {
                ssize_t n = read(fds[0], buf + nbuf, sizeof buf - nbuf);
                if (n > 0) nbuf += (size_t)n;
            }
            break;
        }
    }
    if (!killed) {
        /* the child may have written its last bytes and
         * exited between our final read and waitpid: drain
         * whatever the pipe still holds before classifying */
        for (int round = 0; round < 3; round++) {
            struct pollfd pfd3 = { fds[0], POLLIN, 0 };
            if (poll(&pfd3, 1, 20) <= 0) break;
            ssize_t n = read(fds[0], buf + nbuf, sizeof buf - nbuf);
            if (n <= 0) break;
            nbuf += (size_t)n;
        }
    }
    close(fds[0]);
    for (size_t i = 0; i < nbuf; i++) {
        if (buf[i] == 'S') r->got_s = 1;
        else if (buf[i] == 'L') r->got_l = 1;
        else if (buf[i] == 'H' || buf[i] == 'M' || buf[i] == '-') r->hitbyte = buf[i];
    }
    if (killed) {
        r->cls = r->got_s ? SC_SMALL_OK_LARGE_HUNG : SC_SMALL_HUNG;
    } else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        r->cls = (r->got_s && r->got_l) ? SC_BOTH_OK : SC_ANOMALY;
    } else {
        r->cls = SC_CRASH;
    }
}

/* ---------- fork+exec one try; 1 iff child exit 0 ---------- */
static int exec_one(void) {
    fflush(NULL);
    pid_t pid = fork();
    if (pid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGKILL);
        execl("/bin/true", "true", (char *)NULL);
        _exit(127);
    }
    if (pid < 0) return 0;
    int status = 0;
    if (waitpid(pid, &status, 0) != pid) return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* ---------- Q2 workers (mimir leg only) ---------- */
#ifndef LEG_GLIBC
struct worker {
    void *small[4];
    void *mid[2];
    volatile int ready;
    volatile unsigned long progress;
    volatile int stop;
};
static struct worker g_work[NWORK];
static void *worker_main(void *arg) {
    struct worker *w = (struct worker *)arg;
    int idx = (int)(w - g_work);
    for (int i = 0; i < 4; i++) {
        w->small[i] = vp_malloc(SMALL_SZ);
        if (w->small[i]) fill_block(w->small[i], SMALL_SZ, (unsigned)(20 + idx * 4 + i));
    }
    for (int i = 0; i < 2; i++) {
        w->mid[i] = vp_malloc(128);
        if (w->mid[i]) fill_block(w->mid[i], 128, (unsigned)(40 + idx * 2 + i));
    }
    /* populate own TLS bins: 8 alloc/free cycles at 64 B */
    for (int i = 0; i < 8; i++) {
        void *p = vp_malloc(SMALL_SZ);
        if (p) { ((volatile unsigned char *)p)[0] = 1; vp_free(p); }
    }
    w->ready = 1;
    while (!w->stop) { msleep(5); w->progress++; }
    return NULL;
}
#endif /* mimir-only Q2 workers */

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("Z044,%s,START,timeout_ms=%d\n", LEG, TIMEOUT_MS);

#ifdef LEG_GLIBC
    printf("Z044,glibc,Q1,result=SKIP posture is mimir-only\n");
    printf("Z044,glibc,Q2,result=SKIP posture is mimir-only\n");
    printf("Z044,glibc,B0,result=SKIP no platform lock surface\n");
#else
    /* ================= Q1 ================= */
    {
        for (int i = 0; i < 12; i++) {
            void *p = vp_malloc(SMALL_SZ);
            if (p) { fill_block(p, SMALL_SZ, 90); vp_free(p); }
        }
        for (int i = 0; i < 4; i++) {
            void *p = vp_malloc(128);
            if (p) { fill_block(p, 128, 91); vp_free(p); }
        }
        struct mimir_tls_cache_snapshot pre;
        int snap_ok = (mimir_tls_cache_snapshot(&pre) == 0);
        void *s1 = vp_malloc(SMALL_SZ); if (s1) fill_block(s1, SMALL_SZ, 1);
        void *s2 = vp_malloc(128);      if (s2) fill_block(s2, 128, 2);
        void *l1 = vp_malloc(LARGE_SZ); if (l1) fill_block(l1, LARGE_SZ, 3);
        void *l2 = vp_malloc(4096);     if (l2) fill_block(l2, 4096, 4);
        struct mimir_stats st0;
        int st_ok = (mimir_core_stats_snapshot(&st0) == 0);
        printf("Z044,mimir,Q1,pre,snap_ok=%d cached=%llu hits=%llu "
               "stats_ok=%d live_small=%llu live_mappings=%llu\n",
               snap_ok, (unsigned long long)pre.cached_blocks,
               (unsigned long long)pre.hits, st_ok,
               (unsigned long long)st0.live_small_objects,
               (unsigned long long)st0.live_mappings);
        if (!s1 || !s2 || !l1 || !l2 || !snap_ok || !st_ok) {
            printf("Z044,mimir,Q1,result=FAIL setup\n"); failures++;
        } else {
            fflush(NULL);
            pid_t pid = fork();
            if (pid == 0) {
                prctl(PR_SET_PDEATHSIG, SIGKILL);
                if (!verify_block(s1, SMALL_SZ, 1) || !verify_block(s2, 128, 2) ||
                    !verify_block(l1, LARGE_SZ, 3) || !verify_block(l2, 4096, 4))
                    _exit(41);
                struct mimir_tls_cache_snapshot cs0, cs1;
                if (mimir_tls_cache_snapshot(&cs0) != 0) _exit(46);
                void *cp = mimir_malloc(SMALL_SZ);
                if (!cp) _exit(42);
                fill_block(cp, SMALL_SZ, 7);
                if (!verify_block(cp, SMALL_SZ, 7)) _exit(41);
                if (mimir_tls_cache_snapshot(&cs1) != 0) _exit(46);
                mimir_free(cp);
                void *cq = mimir_malloc(LARGE_SZ);
                if (!cq) _exit(42);
                fill_block(cq, LARGE_SZ, 8);
                if (!verify_block(cq, LARGE_SZ, 8)) _exit(41);
                mimir_free(cq);
                mimir_free(s2); /* inherited own-thread block */
                if (!verify_block(s1, SMALL_SZ, 1) || !verify_block(l1, LARGE_SZ, 3))
                    _exit(41);
                child_fmt("Z044,mimir,Q1,child,cached=%llu hit=%d verify=ok\n",
                          (unsigned long long)cs0.cached_blocks,
                          (cs1.hits > cs0.hits) ? 1 : 0);
                struct mimir_stats cst;
                if (mimir_core_stats_snapshot(&cst) == 0)
                    child_fmt("Z044,mimir,Q1,childstats,live_small=%llu "
                              "live_mappings=%llu (expect 1/2: s1,l1,l2 "
                              "still live in the child copy)\n",
                              (unsigned long long)cst.live_small_objects,
                              (unsigned long long)cst.live_mappings);
                _exit(0);
            }
            int status = 0;
            waitpid(pid, &status, 0);
            int child_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
            int parent_ok = verify_block(s1, SMALL_SZ, 1) && verify_block(s2, 128, 2) &&
                            verify_block(l1, LARGE_SZ, 3) && verify_block(l2, 4096, 4);
            struct mimir_stats st1;
            int st1_ok = (mimir_core_stats_snapshot(&st1) == 0);
            int counters_ok = st1_ok &&
                st1.live_small_objects == st0.live_small_objects &&
                st1.live_mappings == st0.live_mappings;
            printf("Z044,mimir,Q1,result=%s child_exit_ok=%d parent_patterns=%d "
                   "parent_counters_unmoved=%d\n",
                   (child_ok && parent_ok && counters_ok) ? "PASS" : "FAIL",
                   child_ok, parent_ok, counters_ok);
            if (!(child_ok && parent_ok && counters_ok)) failures++;
            vp_free(s1); vp_free(s2); vp_free(l1); vp_free(l2);
        }
    }

    /* ================= Q2 ================= */
    {
        pthread_t wth[NWORK];
        for (int i = 0; i < NWORK; i++) {
            g_work[i].ready = 0; g_work[i].stop = 0; g_work[i].progress = 0;
            pthread_create(&wth[i], NULL, worker_main, &g_work[i]);
        }
        int all_ready = 0;
        for (int t = 0; t < 200 && !all_ready; t++) {
            all_ready = 1;
            for (int i = 0; i < NWORK; i++) if (!g_work[i].ready) all_ready = 0;
            if (!all_ready) msleep(5);
        }
        /* quiescence evidence: workers parked, progress is a
         * sleep-loop counter; allocator progress is frozen
         * because workers make no calls after ready. Sample
         * the held pointers' patterns twice, 50 ms apart. */
        void *held = vp_malloc(SMALL_SZ); if (held) fill_block(held, SMALL_SZ, 55);
        int pat_ok = 1;
        for (int i = 0; i < NWORK; i++) {
            for (int j = 0; j < 4; j++)
                if (!g_work[i].small[j] || !verify_block(g_work[i].small[j], SMALL_SZ, (unsigned)(20 + i * 4 + j))) pat_ok = 0;
            for (int j = 0; j < 2; j++)
                if (!g_work[i].mid[j] || !verify_block(g_work[i].mid[j], 128, (unsigned)(40 + i * 2 + j))) pat_ok = 0;
        }
        printf("Z044,mimir,Q2,pre,workers_ready=%d patterns_ok=%d held=%p\n",
               all_ready, pat_ok, held);
        if (!all_ready || !pat_ok || !held) {
            printf("Z044,mimir,Q2,result=FAIL setup\n"); failures++;
        } else {
            fflush(NULL);
            pid_t pid = fork();
            if (pid == 0) {
                prctl(PR_SET_PDEATHSIG, SIGKILL);
                struct mimir_stats cst0;
                int cst0_ok = (mimir_core_stats_snapshot(&cst0) == 0);
                for (int i = 0; i < NWORK; i++) {
                    for (int j = 0; j < 4; j++)
                        if (!verify_block(g_work[i].small[j], SMALL_SZ, (unsigned)(20 + i * 4 + j))) _exit(41);
                    for (int j = 0; j < 2; j++)
                        if (!verify_block(g_work[i].mid[j], 128, (unsigned)(40 + i * 2 + j))) _exit(41);
                }
                if (!verify_block(held, SMALL_SZ, 55)) _exit(41);
                void *cp = mimir_malloc(SMALL_SZ);
                if (!cp) _exit(42);
                fill_block(cp, SMALL_SZ, 9);
                if (!verify_block(cp, SMALL_SZ, 9)) _exit(41);
                mimir_free(cp);
                void *cq = mimir_malloc(LARGE_SZ);
                if (!cq) _exit(42);
                mimir_free(cq);
                /* return the forking thread's own bins to the
                 * core so only vanished threads' tally can
                 * strand spans below */
                mimir_tls_cache_drain_thread();
                for (int i = 0; i < NWORK; i++) {
                    for (int j = 0; j < 4; j++) mimir_free(g_work[i].small[j]);
                    for (int j = 0; j < 2; j++) mimir_free(g_work[i].mid[j]);
                }
                mimir_free(held);
                struct mimir_stats cst;
                int cst_ok = (mimir_core_stats_snapshot(&cst) == 0);
                size_t rb = mimir_core_retained_spans();
                size_t rel = mimir_core_drain_retained();
                size_t ra = mimir_core_retained_spans();
                child_fmt("Z044,mimir,Q2,child,stats_ok=%d live_small_before=%llu "
                          "live_small_after=%llu "
                          "retained_before=%zu drain_released=%zu retained_after=%zu\n",
                          cst_ok,
                          cst0_ok ? (unsigned long long)cst0.live_small_objects : 0ull,
                          cst_ok ? (unsigned long long)cst.live_small_objects : 0ull,
                          rb, rel, ra);
                _exit(0);
            }
            int status = 0;
            waitpid(pid, &status, 0);
            int child_ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
            printf("Z044,mimir,Q2,result=%s child_exit_ok=%d "
                   "(stranding counts on the child line above)\n",
                   child_ok ? "PASS" : "FAIL", child_ok);
            if (!child_ok) failures++;
        }
        for (int i = 0; i < NWORK; i++) g_work[i].stop = 1;
        for (int i = 0; i < NWORK; i++) {
            pthread_join(wth[i], NULL);
            for (int j = 0; j < 4; j++) vp_free(g_work[i].small[j]);
            for (int j = 0; j < 2; j++) vp_free(g_work[i].mid[j]);
        }
        vp_free(held);
        {
            struct mimir_stats pst;
            if (mimir_core_stats_snapshot(&pst) == 0)
                printf("Z044,mimir,Q2,parent,live_small=%llu live_mappings=%llu "
                       "(expect 0/0: parent freed everything itself)\n",
                       (unsigned long long)pst.live_small_objects,
                       (unsigned long long)pst.live_mappings);
        }
    }
#endif /* mimir-only Q phases */

    /* ================= E1: fork+exec quiescent ================= */
    {
        int ok = 0;
        for (int i = 0; i < E1_TRIES; i++) ok += exec_one();
        printf("Z044,%s,E1,ok=%d tries=%d result=%s\n", LEG, ok, E1_TRIES,
               ok == E1_TRIES ? "PASS" : "FAIL");
        if (ok != E1_TRIES) failures++;
    }

    /* ================= staged boundary (quiescent) ================= */
#ifndef LEG_GLIBC
    /* B0a: lock PROVABLY held at the fork instant */
    {
        /* prime main-thread bins BEFORE the lock is taken:
         * after main takes the lock, a refill in main
         * would block in the parent itself */
        for (int i = 0; i < 8; i++) {
            void *p = vp_malloc(SMALL_SZ);
            if (p) { fill_block(p, SMALL_SZ, 92); vp_free(p); }
        }
        if (mimir_platform_global_lock() != 0) {
            printf("Z044,mimir,B0,result=FAIL could not take platform lock\n");
            failures++;
        } else {
            /* main itself holds the lock across the forks:
             * the child inherits it locked; main must not
             * call the allocator until it unlocks */
            int n_both = 0, n_slh = 0, n_sh = 0, n_crash = 0, n_anom = 0, n_hit = 0;
            for (int i = 0; i < B0_TRIES; i++) {
                struct stage_result r;
                run_staged(&r);
                printf("Z044,mimir,B0a,try=%d class=%s got_s=%d got_l=%d hit=%c state=%c\n",
                       i, sc_name(r.cls), r.got_s, r.got_l,
                       r.hitbyte ? r.hitbyte : '?', r.state_at_kill);
                switch (r.cls) {
                case SC_BOTH_OK: n_both++; break;
                case SC_SMALL_OK_LARGE_HUNG: n_slh++; break;
                case SC_SMALL_HUNG: n_sh++; break;
                case SC_CRASH: n_crash++; break;
                default: n_anom++; break;
                }
                if (r.hitbyte == 'H') n_hit++;
            }
            printf("Z044,mimir,B0a,counts,both_ok=%d small_ok_large_hung=%d "
                   "small_hung=%d crash=%d anomaly=%d hit_evidence=%d tries=%d\n",
                   n_both, n_slh, n_sh, n_crash, n_anom, n_hit, B0_TRIES);
            mimir_platform_global_unlock();
        }
    }
    /* B0b: lock taken and RELEASED before the fork (control) */
    {
        int taken = (mimir_platform_global_lock() == 0);
        if (taken) mimir_platform_global_unlock();
        int n_both = 0, n_other = 0;
        for (int i = 0; i < B0_TRIES; i++) {
            struct stage_result r;
            run_staged(&r);
            printf("Z044,mimir,B0b,try=%d class=%s got_s=%d got_l=%d hit=%c\n",
                   i, sc_name(r.cls), r.got_s, r.got_l, r.hitbyte ? r.hitbyte : '?');
            if (r.cls == SC_BOTH_OK) n_both++; else n_other++;
        }
        printf("Z044,mimir,B0b,counts,both_ok=%d other=%d tries=%d result=%s\n",
               n_both, n_other, B0_TRIES,
               (taken && n_both == B0_TRIES) ? "PASS" : "FAIL");
        if (!(taken && n_both == B0_TRIES)) failures++;
    }
#endif

    /* ================= busy phases ================= */
    churn_start();
    msleep(150);
    unsigned long it0 = churn_total();
    msleep(100);
    unsigned long it1 = churn_total();
    printf("Z044,%s,BUSY,churn_iters_delta=%lu churners=%d\n",
           LEG, it1 - it0, NCHURN);
    if (it1 == it0) { printf("Z044,%s,BUSY,result=FAIL churners not running\n", LEG); failures++; }

    /* E2: fork+exec under churn */
    {
        int ok = 0;
        for (int i = 0; i < E2_TRIES; i++) ok += exec_one();
        printf("Z044,%s,E2,ok=%d tries=%d result=%s\n", LEG, ok, E2_TRIES,
               ok == E2_TRIES ? "PASS" : "FAIL");
        if (ok != E2_TRIES) failures++;
    }


    /* B1: fork-while-busy boundary measurement */
    {
        int n_both = 0, n_slh = 0, n_sh = 0, n_crash = 0, n_anom = 0, n_hit = 0;
        for (int i = 0; i < B1_TRIES; i++) {
            struct stage_result r;
            run_staged(&r);
            printf("Z044,%s,B1,try=%d class=%s got_s=%d got_l=%d hit=%c state=%c\n",
                   LEG, i, sc_name(r.cls), r.got_s, r.got_l,
                   r.hitbyte ? r.hitbyte : '?', r.state_at_kill);
            switch (r.cls) {
            case SC_BOTH_OK: n_both++; break;
            case SC_SMALL_OK_LARGE_HUNG: n_slh++; break;
            case SC_SMALL_HUNG: n_sh++; break;
            case SC_CRASH: n_crash++; break;
            default: n_anom++; break;
            }
            if (r.hitbyte == 'H') n_hit++;
        }
        printf("Z044,%s,B1,counts,both_ok=%d small_ok_large_hung=%d small_hung=%d "
               "crash=%d anomaly=%d hit_evidence=%d tries=%d\n",
               LEG, n_both, n_slh, n_sh, n_crash, n_anom, n_hit, B1_TRIES);
    }

    /* B2: busy control - child _exit(0), no allocator, no exec */
    {
        int ok = 0;
        for (int i = 0; i < B2_TRIES; i++) {
            fflush(NULL);
            pid_t pid = fork();
            if (pid == 0) { prctl(PR_SET_PDEATHSIG, SIGKILL); _exit(0); }
            if (pid < 0) continue;
            int status = 0;
            if (waitpid(pid, &status, 0) == pid &&
                WIFEXITED(status) && WEXITSTATUS(status) == 0) ok++;
        }
        printf("Z044,%s,B2,ok=%d tries=%d result=%s\n", LEG, ok, B2_TRIES,
               ok == B2_TRIES ? "PASS" : "FAIL");
        if (ok != B2_TRIES) failures++;
    }

    churn_stop();
    printf("Z044,%s,END,failures=%d\n", LEG, failures);
    return failures ? 1 : 0;
}
