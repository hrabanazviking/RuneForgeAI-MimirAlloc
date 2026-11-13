/* test_z103_chaos.c -- Z-103 (Wave 12) Corruption chaos.
 * Randomised metadata flips under load: the Wave-8
 * detection surface (Z-066 length audit, Z-067 live-count
 * release audit, J-006 integrity walk, J-007 tallies)
 * measured AT SCALE, plus the false-positive rate on
 * clean runs under the same load.
 *
 * Every trial runs in a forked child (the test_j006
 * precedent): the child builds its own allocator state,
 * starts 3 churn threads (the load), flips ONE randomly
 * chosen metadata field (splitmix64, master seed fixed
 * and recorded in the recorded run), then exercises the
 * detection surfaces: integrity walk, free of the target
 * (cross-thread for span targets, so the central path --
 * not the TLS bin -- judges it), drain + trim, and the
 * detection tallies. The child prints ONE result line;
 * the parent only waits (10 s deadline: a hang is a
 * recorded outcome class) and aggregates.
 *
 * Outcome classes per trial:
 *   DETECTED  walk > 0, or any detection tally moved,
 *             or the target free was refused.
 *   SILENT    none of the above and the child completed
 *             its exercise cycle normally.
 *   CRASH     child died by signal.
 *   HANG      child exceeded the 10 s deadline.
 *   ERR       child exited abnormally without a result.
 * Clean trials (family 0) use DETECTED-criteria as the
 * false-positive test: any walk violation or tally
 * movement on an unflipped tree is a false positive.
 *
 * Families:
 *   0 clean (no flip)                 n=60
 *   1 desc single-bit flip            n=60
 *   2 desc consistent length pair     n=30
 *   3 span single-bit flip            n=60
 *   4 span consistent pair            n=30
 *   5 desc reserve-scratch flip       n=20
 * Standalone: `make test-z103`; NOT part of `make test`
 * (the Z-064 standalone precedent).
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "mimir_alloc.h"
#include "mimir_stats_j002.h"
#include "mimir_diag_j007.h"
#include "mimir_integrity_j006.h"
#include "mimir_drain_h001.h"
#include "mimir_trim_i003.h"

static size_t PAGE;
static volatile int churn_stop;

static uint64_t sm64(uint64_t *s)
{
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static uint64_t rd64(const void *b, size_t o)
{ uint64_t v; memcpy(&v, (const char *)b + o, 8); return v; }
static void wr64(void *b, size_t o, uint64_t v)
{ memcpy((char *)b + o, &v, 8); }
static void wr32(void *b, size_t o, uint32_t v)
{ memcpy((char *)b + o, &v, 4); }
static void *desc_of(const void *u)
{ return (void *)((const char *)u - PAGE); }
static void *span_of(const void *p)
{ return (void *)((uintptr_t)p & ~(uintptr_t)(PAGE - 1)); }
static int snap(struct mimir_stats *s)
{ return mimir_core_stats_snapshot(s); }

static uint64_t tally_sum(void)
{
    return mimir_core_metadata_detections()
         + mimir_core_metadata_collateral_refusals()
         + mimir_core_live_release_detections()
         + mimir_core_live_release_collateral_refusals();
}

static void *churn(void *arg)
{
    uint64_t s = 0xC0FFEEULL + (uint64_t)(uintptr_t)arg * 7919ULL;
    static const size_t sz[4] = {16, 64, 256, 1024};
    while (!churn_stop) {
        void *p = mimir_malloc(sz[sm64(&s) & 3]);
        if (p) { memset(p, 0xA5, 16); mimir_free(p); }
    }
    return NULL;
}

struct xfree { void *p; };
static void *xfree_main(void *a)
{ mimir_free(((struct xfree *)a)->p); return NULL; }
static void free_cross(void *p)
{
    struct xfree x = { p }; pthread_t t;
    if (pthread_create(&t, NULL, xfree_main, &x) == 0)
        pthread_join(t, NULL);
    else mimir_free(p);
}

/* Run one trial in-process (already the child). Prints
 * the result line; returns 0. */
static int trial(int fam, uint64_t seed)
{
    uint64_t rs = seed;
    pthread_t th[3];
    int i, walk = 0, refused = 0;
    uint64_t t0, t1;
    struct mimir_stats s0, s1;
    void *target = NULL, *target2 = NULL;
    int is_desc = (fam == 1 || fam == 2 || fam == 5);

    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    churn_stop = 0;
    for (i = 0; i < 3; i++)
        (void)pthread_create(&th[i], NULL, churn,
                             (void *)(uintptr_t)(i + 1));

    if (is_desc) {
        target = mimir_malloc(20000);
        if (!target) { printf("Z103R fam=%d out=ERR\n", fam); goto out; }
        memset(target, 0x5A, 20000);
    } else if (fam != 0) {
        target = mimir_malloc(32);
        target2 = mimir_malloc(32);
        if (!target || !target2) {
            printf("Z103R fam=%d out=ERR\n", fam); goto out;
        }
        memset(target, 0x5A, 32); memset(target2, 0x3C, 32);
    } else {
        target = mimir_malloc(20000);
        target2 = mimir_malloc(32);
        if (!target || !target2) {
            printf("Z103R fam=%d out=ERR\n", fam); goto out;
        }
        memset(target, 0x5A, 20000); memset(target2, 0x3C, 32);
    }

    t0 = tally_sum();
    (void)snap(&s0);

    /* ---- the flip (families 1-5) ---- */
    if (fam == 1) {
        static const size_t off[8] = {0, 8, 16, 24, 32, 40, 48, 56};
        void *d = desc_of(target);
        size_t o = off[sm64(&rs) % 8];
        if (o == 48)
            wr32(d, o, (uint32_t)rd64(d, o) ^ (1u << (sm64(&rs) % 32)));
        else
            wr64(d, o, rd64(d, o) ^ (1ULL << (sm64(&rs) % 64)));
    } else if (fam == 2) {
        void *d = desc_of(target);
        uint64_t L = rd64(d, 16), U = rd64(d, 40);
        if ((sm64(&rs) & 1) && L > 2 * (uint64_t)PAGE) {
            wr64(d, 16, L - (uint64_t)PAGE);
            wr64(d, 40, U - (uint64_t)PAGE);
        } else {
            wr64(d, 16, L + (uint64_t)PAGE);
            wr64(d, 40, U + (uint64_t)PAGE);
        }
    } else if (fam == 3) {
        static const size_t off[9] = {0, 16, 32, 40, 52, 56, 72, 80, 88};
        void *S = span_of(target);
        size_t o = off[sm64(&rs) % 9];
        unsigned bit = (unsigned)(sm64(&rs) % 32);
        if (o == 52) wr32(S, o, (uint32_t)rd64(S, o) ^ (1u << (bit % 32)));
        else wr64(S, o, rd64(S, o) ^ (1ULL << (bit % 63)));
    } else if (fam == 4) {
        void *S = span_of(target);
        if (sm64(&rs) & 1) { /* length pair (Z-066 span form) */
            wr64(S, 16, 2 * (uint64_t)PAGE);
            wr64(S, 40, 2 * (uint64_t)PAGE - 96);
        } else {             /* class pair (Z-068 by-design) */
            wr32(S, 52, 3);
            wr64(S, 32, 64);
        }
    } else if (fam == 5) {
        void *d = desc_of(target);
        wr64(d, 96, rd64(d, 96) ^ (1ULL << (sm64(&rs) % 63)));
    }

    /* ---- exercise the detection surfaces under load ---- */
    walk = mimir_core_metadata_check();
    if (is_desc || fam == 0) {
        uint64_t before;
        (void)snap(&s1); before = s1.live_mappings;
        mimir_free(target); target = NULL;
        (void)snap(&s1);
        if (fam != 0 && s1.live_mappings == before && before > 0)
            refused = 1;
        if (target2) { mimir_free(target2); target2 = NULL; }
    }
    if (!is_desc && fam != 0) {
        free_cross(target); target = NULL;
        mimir_free(target2); target2 = NULL;
    }
    (void)mimir_core_drain_retained();
    (void)mimir_core_trim_retained();
    if (walk == 0) walk = mimir_core_metadata_check();
    t1 = tally_sum();

    churn_stop = 1;
    for (i = 0; i < 3; i++) pthread_join(th[i], NULL);
    printf("Z103R fam=%d out=%s walk=%d det=%llu refused=%d\n",
           fam,
           (walk > 0 || t1 > t0 || refused) ? "DETECTED" : "SILENT",
           walk, (unsigned long long)(t1 - t0), refused);
    fflush(stdout);
    return 0;
out:
    churn_stop = 1;
    for (i = 0; i < 3; i++) pthread_join(th[i], NULL);
    fflush(stdout);
    return 0;
}

int main(void)
{
    static const int counts[6] = {60, 60, 30, 60, 30, 20};
    int fam, i;
    int det[6] = {0}, sil[6] = {0}, cra[6] = {0},
        han[6] = {0}, err[6] = {0};
    PAGE = (size_t)sysconf(_SC_PAGESIZE);
    for (fam = 0; fam < 6; fam++) {
        for (i = 0; i < counts[fam]; i++) {
            int fds[2]; pid_t pid; int st = 0, done = 0, waited = 0;
            char buf[256]; ssize_t n = 0;
            if (pipe(fds) != 0) { err[fam]++; continue; }
            pid = fork();
            if (pid == 0) {
                (void)dup2(fds[1], 1);
                close(fds[0]); close(fds[1]);
                trial(fam, 0x1030000ULL
                      + (uint64_t)fam * 100003ULL + (uint64_t)i);
                _exit(0);
            }
            close(fds[1]);
            while (waited < 10000) {
                pid_t r = waitpid(pid, &st, WNOHANG);
                if (r == pid) { done = 1; break; }
                (void)usleep(10000); waited += 10;
            }
            if (!done) {
                (void)kill(pid, SIGKILL);
                (void)waitpid(pid, &st, 0);
                han[fam]++;
            } else if (WIFSIGNALED(st)) {
                cra[fam]++;
            } else {
                n = read(fds[0], buf, sizeof buf - 1);
                if (n > 0) {
                    buf[n] = 0;
                    if (strstr(buf, "out=DETECTED")) det[fam]++;
                    else if (strstr(buf, "out=SILENT")) sil[fam]++;
                    else err[fam]++;
                } else err[fam]++;
            }
            close(fds[0]);
        }
        printf("Z103 fam=%d n=%d detected=%d silent=%d crash=%d "
               "hang=%d err=%d\n", fam, counts[fam], det[fam],
               sil[fam], cra[fam], han[fam], err[fam]);
        fflush(stdout);
    }
    printf("test_z103_chaos: families=6 trials=260 done\n");
    return 0;
}
