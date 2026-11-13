/* probe_z115.c - Z-115 KV-cache-shaped allocation probe.
 * A SYNTHETIC pattern modelled on transformer KV-cache
 * allocation behaviour, measured on THIS allocator. It is
 * not Aesir and does not run Aesir: the Z-087 boundary
 * blockers (B-087-1/2/3, re-checked in this slice) still
 * stand, so the KV shape is modelled, per the WAVE_PLAN
 * Wave 13 pool choice for Z-115. Three shapes:
 *   kv_append - per-sequence append growth: each sequence
 *     grows by fixed token blocks via realloc (the classic
 *     KV append path), every step touched.
 *   kv_blocks - a long-lived resident block pool (the
 *     cached prefix) held live while small per-token
 *     blocks churn through a FIFO ring.
 *   kv_mixed  - resident large blocks plus append growth
 *     plus eviction rotation (free oldest residents,
 *     allocate fresh ones), the serving mix.
 * One shape per process; CSV out in the bench_k002 format:
 *   leg,workload,ops,elapsed_ns,ops_per_sec,ns_per_op,checksum
 * Leg: -DLEG_GLIBC builds glibc malloc/realloc/free, the
 * default build binds the mimir_* namespaced API. The
 * runner relabels the glibc tuned run's leg to glibc_tuned.
 * Work per shape is fixed, so ops and checksum must be
 * identical across legs; the summariser asserts both.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mimir_alloc.h"
#ifdef LEG_GLIBC
static void *a_alloc(size_t n) { return malloc(n); }
static void *a_realloc(void *p, size_t n) { return realloc(p, n); }
static void a_free(void *p) { free(p); }
static const char *leg_name = "glibc";
#else
static void *a_alloc(size_t n) { return mimir_malloc(n); }
static void *a_realloc(void *p, size_t n) { return mimir_realloc(p, n); }
static void a_free(void *p) { mimir_free(p); }
static const char *leg_name = "mimiralloc";
#endif
static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}
static volatile unsigned char sink;
/* touch first/mid/last byte; returns byte sum for checksum */
static uint64_t touch(void *p, size_t n, unsigned char v)
{
    unsigned char *b = (unsigned char *)p;
    b[0] = v; b[n / 2] = (unsigned char)(v + 1); b[n - 1] = (unsigned char)(v + 2);
    sink = b[0];
    return (uint64_t)b[0] + b[n / 2] + b[n - 1];
}
static uint64_t shape_append(long reps, uint64_t *ops_out)
{
    /* 32 sequences x 64 appends of 4 KiB -> 256 KiB each */
    uint64_t chk = 0, ops = 0;
    long r;
    for (r = 0; r < reps; r++) {
        void *seq[32] = {0};
        int s, t;
        for (s = 0; s < 32; s++) {
            size_t cur = 0;
            for (t = 0; t < 64; t++) {
                cur += 4096;
                seq[s] = a_realloc(seq[s], cur);
                if (!seq[s]) { fprintf(stderr, "alloc fail append\n"); exit(1); }
                ops++;
                chk += touch(seq[s], cur, (unsigned char)(s + t));
            }
        }
        for (s = 0; s < 32; s++) a_free(seq[s]);
    }
    *ops_out = ops;
    return chk;
}
static uint64_t shape_blocks(long reps, uint64_t *ops_out)
{
    /* resident 256 x 64 KiB held live; ring churn 2048 x 4 KiB */
    uint64_t chk = 0, ops = 0;
    void *res[256];
    void *ring[128] = {0};
    int i;
    long r;
    for (i = 0; i < 256; i++) {
        res[i] = a_alloc(65536);
        if (!res[i]) { fprintf(stderr, "alloc fail res\n"); exit(1); }
        chk += touch(res[i], 65536, (unsigned char)i);
    }
    for (r = 0; r < reps; r++) {
        int c;
        for (c = 0; c < 2048; c++) {
            int slot = c % 128;
            a_free(ring[slot]);
            ring[slot] = a_alloc(4096);
            if (!ring[slot]) { fprintf(stderr, "alloc fail churn\n"); exit(1); }
            ops++;
            chk += touch(ring[slot], 4096, (unsigned char)c);
        }
        chk += touch(res[r % 256], 65536, (unsigned char)r);
    }
    for (i = 0; i < 128; i++) a_free(ring[i]);
    for (i = 0; i < 256; i++) a_free(res[i]);
    *ops_out = ops;
    return chk;
}
static uint64_t shape_mixed(long reps, uint64_t *ops_out)
{
    /* resident 128 x 256 KiB; per pass: 16 seqs append-grow
     * 32 x 8 KiB, then evict oldest 32 residents, refill */
    uint64_t chk = 0, ops = 0;
    long r;
    for (r = 0; r < reps; r++) {
        void *res[128];
        void *seq[16] = {0};
        int i, s, t;
        for (i = 0; i < 128; i++) {
            res[i] = a_alloc(262144);
            if (!res[i]) { fprintf(stderr, "alloc fail mixed res\n"); exit(1); }
            chk += touch(res[i], 262144, (unsigned char)i);
        }
        for (s = 0; s < 16; s++) {
            size_t cur = 0;
            for (t = 0; t < 32; t++) {
                cur += 8192;
                seq[s] = a_realloc(seq[s], cur);
                if (!seq[s]) { fprintf(stderr, "alloc fail mixed grow\n"); exit(1); }
                ops++;
                chk += touch(seq[s], cur, (unsigned char)(s + t));
            }
        }
        for (i = 0; i < 32; i++) { a_free(res[i]); res[i] = a_alloc(262144); if (!res[i]) { fprintf(stderr, "alloc fail evict\n"); exit(1); } ops++; chk += touch(res[i], 262144, (unsigned char)(i + r)); }
        for (s = 0; s < 16; s++) a_free(seq[s]);
        for (i = 0; i < 128; i++) a_free(res[i]);
    }
    *ops_out = ops;
    return chk;
}
int main(int argc, char **argv)
{
    long reps;
    uint64_t ops = 0, chk, t0, t1;
    if (argc < 3) { fprintf(stderr, "usage: probe_z115 <shape> <reps>\n"); return 2; }
    reps = strtol(argv[2], NULL, 10);
    if (reps < 1) return 2;
    t0 = now_ns();
    if (strcmp(argv[1], "kv_append") == 0) chk = shape_append(reps, &ops);
    else if (strcmp(argv[1], "kv_blocks") == 0) chk = shape_blocks(reps, &ops);
    else if (strcmp(argv[1], "kv_mixed") == 0) chk = shape_mixed(reps, &ops);
    else { fprintf(stderr, "unknown shape %s\n", argv[1]); return 2; }
    t1 = now_ns();
    printf("%s,%s,%llu,%llu,%.1f,%.2f,%llu\n", leg_name, argv[1],
           (unsigned long long)ops, (unsigned long long)(t1 - t0),
           (t1 - t0) ? (double)ops * 1e9 / (double)(t1 - t0) : 0.0,
           ops ? (double)(t1 - t0) / (double)ops : 0.0,
           (unsigned long long)chk);
    return 0;
}
