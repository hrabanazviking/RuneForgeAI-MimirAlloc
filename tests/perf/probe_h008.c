/* probe_h008.c - H-008 Phase 8 latency + memory reports.
 * Latency leg: H-002 Phase B shape exactly (every worker
 * alloc/fill/verify/free its own blocks against the one
 * global lock; sizes 64 B, every 10th 2048 B), but timed
 * PER OP so the distribution H-002 named unknown (its
 * Section 5 item 5) is measured: for each iteration the
 * malloc call and the free call are timed separately with
 * CLOCK_MONOTONIC; the fill/verify between them is NOT
 * timed (pattern check only, H-002 S7 method: the worker
 * re-derives each byte itself). Per thread count
 * (1/2/4/8) all samples are pooled, sorted, and reported
 * as mean / p50 / p90 / p99 / max for malloc, free and
 * the malloc+free pair. ITERS=2000 per thread, bounded
 * in the H-002 (1500) / H-007 scale. Timing overhead of
 * two extra clock reads per op is part of the measured
 * number and is stated in the slice doc, never hidden.
 * Memory leg (H-002 Section 5 item 1): per thread count,
 * each worker allocates a held set of 128 blocks (sizes
 * cycle 64/256/512 span path, 2048 mapping path), fills
 * them, then waits at a barrier whose count includes the
 * main thread. While every worker is blocked at the
 * second barrier (no allocator call in flight - a true
 * quiescent point), the main thread reads the E-005 /
 * G-003 audit counters and VmRSS and reconciles: live
 * small objects and live mappings must equal the held
 * set exactly, requested live bytes are the deterministic
 * workload sum, mapped bytes are derived per live
 * mapping (one header page + payload pages, S7.1 shape)
 * and retained span bytes are live spans x page size
 * (one span is exactly one system page, G-003). After
 * release the workers verify + free cross-thread (ring),
 * and the main thread re-checks at the next quiescent
 * point that live small / live mappings return exactly
 * to baseline. Live SPANS are not required to return:
 * warmed classes retain their spans for process life
 * (G-005, no idle reaper) - the retained count is the
 * report, not a failure. Exit 0 iff zero failures. */
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "mimir_alloc.h"
#include "../core/mimir_small_g003.h"
#include "../abi/mimir_audit_e005.h"

#define MAXT 8
#define ITERS 2000
#define HOLD 128

static long long g_alloc_ns[MAXT][ITERS];
static long long g_free_ns[MAXT][ITERS];
static long g_pat[MAXT], g_null[MAXT];
static pthread_barrier_t g_bar1, g_bar2;
static void *g_hold[MAXT][HOLD];
static size_t g_hsize[MAXT][HOLD];
static unsigned long base_small, base_map, base_span;
static long g_pagesz;

static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
static uint8_t pat(int t, int i, int ph) {
    return (uint8_t)(0xA5u ^ (unsigned)(t * 31) ^
                     (unsigned)i ^ (unsigned)(ph * 67));
}
static unsigned long live_small(void) {
    return mimir_audit_small_alloc - mimir_audit_small_free;
}
static unsigned long live_map(void) {
    return mimir_audit_alloc_published -
           mimir_audit_mapping_retired;
}
static unsigned long live_span(void) {
    return mimir_audit_span_published -
           mimir_audit_span_retired;
}
static long rss_kb(void) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256]; long kb = -1;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "VmRSS: %ld kB", &kb) == 1) break;
    fclose(f);
    return kb;
}
static int cmp_ll(const void *a, const void *b) {
    long long x = *(const long long *)a;
    long long y = *(const long long *)b;
    return (x > y) - (x < y);
}
static void report(const char *what, long long *v, long n) {
    qsort(v, (size_t)n, sizeof *v, cmp_ll);
    long long sum = 0;
    for (long i = 0; i < n; i++) sum += v[i];
    printf("LAT %s n=%ld mean_ns=%lld p50_ns=%lld "
           "p90_ns=%lld p99_ns=%lld max_ns=%lld\n",
           what, n, n ? sum / n : 0,
           n ? v[n / 2] : 0, n ? v[(n * 9) / 10] : 0,
           n ? v[(n * 99) / 100] : 0, n ? v[n - 1] : 0);
}

/* ---- latency leg ---- */
struct larg { int tid; };
static void *lat_worker(void *p) {
    struct larg *a = (struct larg *)p;
    int t = a->tid;
    for (int i = 0; i < ITERS; i++) {
        size_t sz = (i % 10 == 9) ? 2048 : 64;
        long long t0 = now_ns();
        void *q = mimir_malloc(sz);
        long long t1 = now_ns();
        g_alloc_ns[t][i] = t1 - t0;
        if (!q) { g_null[t]++; g_free_ns[t][i] = 0; continue; }
        memset(q, pat(t, i, 1), sz);
        const uint8_t *b = (const uint8_t *)q;
        for (size_t k = 0; k < sz; k++)
            if (b[k] != pat(t, i, 1)) { g_pat[t]++; break; }
        long long t2 = now_ns();
        mimir_free(q);
        g_free_ns[t][i] = now_ns() - t2;
    }
    return NULL;
}
static int run_lat(int nt) {
    static long long pool_a[MAXT * ITERS];
    static long long pool_f[MAXT * ITERS];
    static long long pool_p[MAXT * ITERS];
    pthread_t th[MAXT];
    struct larg args[MAXT];
    memset(g_pat, 0, sizeof g_pat);
    memset(g_null, 0, sizeof g_null);
    for (int t = 0; t < nt; t++) {
        args[t].tid = t;
        if (pthread_create(&th[t], NULL, lat_worker, &args[t])) {
            printf("LAT threads=%d CREATE_FAIL\n", nt);
            return 1;
        }
    }
    for (int t = 0; t < nt; t++) pthread_join(th[t], NULL);
    long pf = 0, nf = 0, n = 0;
    for (int t = 0; t < nt; t++) {
        pf += g_pat[t]; nf += g_null[t];
        for (int i = 0; i < ITERS; i++) {
            pool_a[n] = g_alloc_ns[t][i];
            pool_f[n] = g_free_ns[t][i];
            pool_p[n] = g_alloc_ns[t][i] + g_free_ns[t][i];
            n++;
        }
    }
    char tag[64];
    snprintf(tag, sizeof tag, "threads=%d malloc", nt);
    report(tag, pool_a, n);
    snprintf(tag, sizeof tag, "threads=%d free", nt);
    report(tag, pool_f, n);
    snprintf(tag, sizeof tag, "threads=%d pair", nt);
    report(tag, pool_p, n);
    printf("LAT threads=%d pattern_fail=%ld null_fail=%ld %s\n",
           nt, pf, nf, (pf == 0 && nf == 0) ? "OK" : "FAIL");
    return (pf == 0 && nf == 0) ? 0 : 1;
}

/* ---- memory leg ---- */
struct marg { int tid; int nt; };
static size_t hold_size(int i) {
    if (i % 4 == 0) return 64;
    if (i % 4 == 1) return 256;
    if (i % 4 == 2) return 512;
    return 2048;
}
static void *mem_worker(void *p) {
    struct marg *a = (struct marg *)p;
    int t = a->tid;
    for (int i = 0; i < HOLD; i++) {
        size_t sz = hold_size(i);
        void *q = mimir_malloc(sz);
        g_hold[t][i] = q; g_hsize[t][i] = sz;
        if (!q) { g_null[t]++; continue; }
        memset(q, pat(t, i, 2), sz);
    }
    pthread_barrier_wait(&g_bar1);
    pthread_barrier_wait(&g_bar2);
    int src = (t + 1) % a->nt;
    for (int i = 0; i < HOLD; i++) {
        void *q = g_hold[src][i];
        if (!q) continue;
        size_t sz = g_hsize[src][i];
        const uint8_t *b = (const uint8_t *)q;
        for (size_t k = 0; k < sz; k++)
            if (b[k] != pat(src, i, 2)) { g_pat[t]++; break; }
        mimir_free(q);
    }
    return NULL;
}
static int run_mem(int nt) {
    pthread_t th[MAXT];
    struct marg args[MAXT];
    memset(g_pat, 0, sizeof g_pat);
    memset(g_null, 0, sizeof g_null);
    pthread_barrier_init(&g_bar1, NULL, (unsigned)nt + 1);
    pthread_barrier_init(&g_bar2, NULL, (unsigned)nt + 1);
    for (int t = 0; t < nt; t++) {
        args[t].tid = t; args[t].nt = nt;
        if (pthread_create(&th[t], NULL, mem_worker, &args[t])) {
            printf("MEM threads=%d CREATE_FAIL\n", nt);
            return 1;
        }
    }
    pthread_barrier_wait(&g_bar1); /* held set live, quiescent */
    unsigned long ls = live_small() - base_small;
    unsigned long lm = live_map() - base_map;
    unsigned long lsp = live_span() - base_span;
    long held_small = 0, held_map = 0;
    unsigned long req = 0;
    for (int t = 0; t < nt; t++)
        for (int i = 0; i < HOLD; i++) {
            size_t sz = hold_size(i);
            req += (unsigned long)sz;
            if (sz == 2048) held_map++; else held_small++;
        }
    /* each 2048 mapping: 1 header page + 1 payload page */
    unsigned long mapped = lm * (unsigned long)(2 * g_pagesz);
    unsigned long spanb = lsp * (unsigned long)g_pagesz;
    int recon = ((long)ls == held_small && (long)lm == held_map);
    printf("MEM threads=%d held_small=%ld held_map=%ld "
           "live_small=%lu live_map=%lu live_spans=%lu "
           "requested_bytes=%lu mapped_bytes=%lu "
           "span_bytes=%lu rss_kb=%ld reconcile=%s\n",
           nt, held_small, held_map, ls, lm, lsp, req,
           mapped, spanb, rss_kb(), recon ? "OK" : "FAIL");
    pthread_barrier_wait(&g_bar2); /* release verify+free */
    for (int t = 0; t < nt; t++) pthread_join(th[t], NULL);
    pthread_barrier_destroy(&g_bar1);
    pthread_barrier_destroy(&g_bar2);
    unsigned long ds = live_small() - base_small;
    unsigned long dm = live_map() - base_map;
    unsigned long dsp = live_span() - base_span;
    long pf = 0, nf = 0;
    for (int t = 0; t < nt; t++) { pf += g_pat[t]; nf += g_null[t]; }
    int back = (ds == 0 && dm == 0);
    printf("MEM threads=%d after_free live_small_delta=%lu "
           "live_map_delta=%lu retained_spans=%lu "
           "retained_span_bytes=%lu rss_kb=%ld "
           "pattern_fail=%ld null_fail=%ld %s\n",
           nt, ds, dm, dsp, dsp * (unsigned long)g_pagesz,
           rss_kb(), pf, nf,
           (back && pf == 0 && nf == 0) ? "OK" : "FAIL");
    return (recon && back && pf == 0 && nf == 0) ? 0 : 1;
}

int main(void) {
    int rc = 0;
    g_pagesz = sysconf(_SC_PAGESIZE);
    base_small = live_small();
    base_map = live_map();
    base_span = live_span();
    printf("BASE page_size=%ld rss_kb=%ld live_small=%lu "
           "live_map=%lu live_spans=%lu\n",
           g_pagesz, rss_kb(), base_small, base_map, base_span);
    const int cfgs[4] = {1, 2, 4, 8};
    for (int c = 0; c < 4; c++) rc |= run_mem(cfgs[c]);
    for (int c = 0; c < 4; c++) rc |= run_lat(cfgs[c]);
    printf("FINAL retained_spans=%lu retained_span_bytes=%lu "
           "rss_kb=%ld\n", live_span() - base_span,
           (live_span() - base_span) * (unsigned long)g_pagesz,
           rss_kb());
    printf("RESULT: %s\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}
