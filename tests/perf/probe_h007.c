/* probe_h007.c - H-007 Phase 8 stress harness.
 * Stress at small thread counts and under oversubscription
 * (H-002 rows S6/S7 + carried O-05). Repeated bounded
 * scenarios at 1/2/4/8 threads plus one oversubscription
 * shape at 12 threads (4-core host: 8 is already 2x,
 * 12 is 3x oversubscription), REPS=3 per shape.
 * Mixes per rep: (A) cross-thread free ring, (B) own
 * alloc/fill/verify/free churn, (C) calloc/realloc mix.
 * Sizes mix span path (64/256/512) and mapping (2048).
 * Independent pattern checks (H-002 S7 method): the
 * verifying thread re-derives each byte from (owner,
 * index, phase) itself; the allocator never vouches.
 * Audit counters reconciled on the main thread at
 * quiescent points only (all workers joined): live
 * small and live mappings must return exactly to the
 * baseline delta after every rep. Samtal spot-checks:
 * a monitor thread issues a few HTTP GETs to
 * 127.0.0.1:8787 during the heaviest (12-thread) leg
 * only; recorded as a probe, not a bound, and never a
 * correctness oracle. Timings printed, never judged.
 * Exit 0 iff zero correctness failures. */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "mimir_alloc.h"
#include "../core/mimir_small_g003.h"
#include "../abi/mimir_audit_e005.h"

#define MAXT 16
#define NCROSS 256
#define NCHURN 256
#define NCR 128
#define REPS 3

static void *g_ptrs[MAXT][NCROSS];
static size_t g_sizes[MAXT][NCROSS];
static pthread_barrier_t g_bar;
static long g_pat[MAXT], g_null[MAXT], g_zero[MAXT];
static long g_rfail[MAXT];
static unsigned long base_small, base_map;

static uint8_t pat(int o, int i, int ph) {
    return (uint8_t)(0xA5u ^ (unsigned)(o * 31) ^
                     (unsigned)i ^ (unsigned)(ph * 67));
}
static size_t pick(int i) {
    if (i % 4 == 0) return 64;
    if (i % 4 == 1) return 256;
    if (i % 4 == 2) return 512;
    return 2048;
}
static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
static unsigned long live_small(void) {
    return mimir_audit_small_alloc - mimir_audit_small_free;
}
static unsigned long live_map(void) {
    return mimir_audit_alloc_published -
           mimir_audit_mapping_retired;
}

struct arg { int tid; int nt; };

static void *worker(void *p) {
    struct arg *a = (struct arg *)p;
    int t = a->tid, i;
    /* A: cross-thread free ring */
    for (i = 0; i < NCROSS; i++) {
        size_t sz = pick(i);
        void *q = mimir_malloc(sz);
        g_ptrs[t][i] = q; g_sizes[t][i] = sz;
        if (!q) { g_null[t]++; continue; }
        memset(q, pat(t, i, 1), sz);
    }
    pthread_barrier_wait(&g_bar);
    int src = (t + 1) % a->nt;
    for (i = 0; i < NCROSS; i++) {
        void *q = g_ptrs[src][i];
        if (!q) continue;
        size_t sz = g_sizes[src][i];
        const uint8_t *b = (const uint8_t *)q;
        for (size_t k = 0; k < sz; k++)
            if (b[k] != pat(src, i, 1)) { g_pat[t]++; break; }
        mimir_free(q);
    }
    /* B: own churn under contention */
    for (i = 0; i < NCHURN; i++) {
        size_t sz = (i % 10 == 9) ? 2048 :
                    (i % 2 ? 256 : 64);
        void *q = mimir_malloc(sz);
        if (!q) { g_null[t]++; continue; }
        memset(q, pat(t, i, 2), sz);
        const uint8_t *b = (const uint8_t *)q;
        for (size_t k = 0; k < sz; k++)
            if (b[k] != pat(t, i, 2)) { g_pat[t]++; break; }
        mimir_free(q);
    }
    /* C: calloc/realloc mix */
    for (i = 0; i < NCR; i++) {
        size_t n0 = (size_t)(i % 3 + 1) * 64;
        unsigned char *q = mimir_calloc(i % 3 + 1, 64);
        if (!q) { g_null[t]++; continue; }
        for (size_t k = 0; k < n0; k++)
            if (q[k] != 0) { g_zero[t]++; break; }
        memset(q, pat(t, i, 3), n0);
        size_t n1 = (i % 2 == 0) ? 2048 : 256;
        unsigned char *r = mimir_realloc(q, n1);
        if (!r) { g_rfail[t]++; mimir_free(q); continue; }
        size_t keep = n0 < n1 ? n0 : n1;
        for (size_t k = 0; k < keep; k++)
            if (r[k] != pat(t, i, 3)) { g_pat[t]++; break; }
        mimir_free(r);
    }
    return NULL;
}

/* Samtal monitor: plain HTTP GETs, info only. */
static int g_samtal[8];
static void *monitor(void *unused) {
    (void)unused;
    for (int j = 0; j < 5; j++) {
        int code = 0;
        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s >= 0) {
            struct sockaddr_in ad;
            memset(&ad, 0, sizeof ad);
            ad.sin_family = AF_INET;
            ad.sin_port = htons(8787);
            ad.sin_addr.s_addr = htonl(0x7f000001u);
            struct timeval tv; tv.tv_sec = 3; tv.tv_usec = 0;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv,
                       sizeof tv);
            setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv,
                       sizeof tv);
            if (connect(s, (struct sockaddr *)&ad,
                        sizeof ad) == 0) {
                const char *req =
                    "GET / HTTP/1.0\r\nHost: x\r\n\r\n";
                if (send(s, req, strlen(req), 0) > 0) {
                    char buf[64]; ssize_t n =
                        recv(s, buf, sizeof buf - 1, 0);
                    if (n > 0) { buf[n] = 0;
                        if (sscanf(buf, "HTTP/%*s %d",
                                   &code) != 1) code = -1; }
                }
            }
            close(s);
        }
        g_samtal[j] = code;
        { struct timespec d; d.tv_sec=0; d.tv_nsec=200000000L; nanosleep(&d,NULL); }
    }
    return NULL;
}

static int run_rep(int nt, int rep, int with_mon) {
    pthread_t th[MAXT], mon;
    struct arg args[MAXT];
    memset(g_pat, 0, sizeof g_pat);
    memset(g_null, 0, sizeof g_null);
    memset(g_zero, 0, sizeof g_zero);
    memset(g_rfail, 0, sizeof g_rfail);
    pthread_barrier_init(&g_bar, NULL, (unsigned)nt);
    if (with_mon) {
        for (int j = 0; j < 5; j++) g_samtal[j] = 0;
        pthread_create(&mon, NULL, monitor, NULL);
    }
    long long w0 = now_ns();
    for (int t = 0; t < nt; t++) {
        args[t].tid = t; args[t].nt = nt;
        if (pthread_create(&th[t], NULL, worker, &args[t])) {
            printf("SCENARIO threads=%d rep=%d CREATE_FAIL\n",
                   nt, rep);
            return 1;
        }
    }
    for (int t = 0; t < nt; t++) pthread_join(th[t], NULL);
    long long wall = now_ns() - w0;
    if (with_mon) pthread_join(mon, NULL);
    pthread_barrier_destroy(&g_bar);
    long pf = 0, nf = 0, zf = 0, rf = 0;
    for (int t = 0; t < nt; t++) {
        pf += g_pat[t]; nf += g_null[t];
        zf += g_zero[t]; rf += g_rfail[t];
    }
    long cross = (long)nt * NCROSS;
    long churn = (long)nt * NCHURN;
    long crops = (long)nt * NCR;
    unsigned long ls = live_small() - base_small;
    unsigned long lm = live_map() - base_map;
    int audit_ok = (ls == 0 && lm == 0);
    printf("SCENARIO threads=%d rep=%d cross_ops=%ld "
           "churn_ops=%ld calloc_realloc_ops=%ld "
           "pattern_fail=%ld null_fail=%ld zero_fail=%ld "
           "realloc_fail=%ld live_small_delta=%lu "
           "live_map_delta=%lu audit=%s wall_ns=%lld %s\n",
           nt, rep, cross, churn, crops, pf, nf, zf, rf,
           ls, lm, audit_ok ? "OK" : "FAIL", wall,
           (pf == 0 && nf == 0 && zf == 0 && rf == 0 &&
            audit_ok) ? "OK" : "FAIL");
    if (with_mon)
        printf("SAMTAL threads=%d rep=%d get_codes=%d,%d,"
               "%d,%d,%d (probe, not a bound)\n", nt, rep,
               g_samtal[0], g_samtal[1], g_samtal[2],
               g_samtal[3], g_samtal[4]);
    return (pf == 0 && nf == 0 && zf == 0 && rf == 0 &&
            audit_ok) ? 0 : 1;
}

int main(void) {
    int rc = 0;
    base_small = live_small(); base_map = live_map();
    const int cfgs[5] = {1, 2, 4, 8, 12};
    for (int c = 0; c < 5; c++) {
        int fails = 0;
        for (int r = 0; r < REPS; r++)
            fails += run_rep(cfgs[c], r, cfgs[c] == 12);
        printf("SUMMARY threads=%d reps=%d failed_reps=%d %s\n",
               cfgs[c], REPS, fails, fails ? "FAIL" : "OK");
        if (fails) rc = 1;
    }
    printf("RESULT: %s\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}
