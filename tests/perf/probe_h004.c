/* probe_h004.c - H-004 Phase 8 contention controls probe.
 * Test-only harness: NO src/ change. Three variants measured
 * with the H-002 Phase B method (alloc/fill/verify/free pairs,
 * sizes 64 B with every 10th 2048 B, ITERS per thread, medians
 * of REPS internal repetitions; timing is never a correctness
 * oracle - correctness is pattern checks + count reconcile).
 * V0 REAL: the banked allocator via the public API (status
 * quo single global lock) - directly comparable to H-002.
 * V1 SHARD MODEL: test-only fixed 64 B slot pool, 4 shards,
 * one mutex per shard; alloc pops own shard, free pushes to
 * the OWNING shard (cross-thread free = remote shard lock).
 * Measures striped-lock cost only; it is NOT an allocator.
 * V2 QUEUE MODEL: test-only fixed 64 B slot pool; each thread
 * owns a freelist (no lock for own alloc/free); a cross-thread
 * free pushes onto the owner's bounded queue (cap QUEUE_CAP)
 * under the owner's queue mutex; the owner drains its queue
 * at its next alloc. Measures deferred remote-free cost only.
 * V1/V2 use the H-002 Phase A ring shape for cross frees.
 * Exit 0 iff zero pattern/null/reconcile failures. */
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mimir_alloc.h"

#define MAX_THREADS 8
#define ITERS 1500
#define REPS 3
#define SHARDS 4
#define POOL_PER_SHARD 4096
#define QUEUE_CAP 512

static long long now_ns(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
static uint8_t pat_byte(int tid, int i) {
    return (uint8_t)(0xA5u ^ (unsigned)(tid * 31) ^ (unsigned)i);
}
static int cmp_ll(const void *a, const void *b) {
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}
static long long median_ll(long long *v, int n) {
    qsort(v, (size_t)n, sizeof *v, cmp_ll); return v[n / 2];
}

/* ---------- V0: real allocator (status quo) ---------- */
/* Reduction matches H-002 exactly: sum of per-thread elapsed
 * over the leg, divided by total pairs. */
static long g_v0_pat, g_v0_null;
struct v0arg { int tid; long long elapsed; };
static void *v0_worker(void *p) {
    struct v0arg *a = (struct v0arg *)p; int t = a->tid;
    long long s = now_ns();
    for (int i = 0; i < ITERS; i++) {
        size_t sz = (i % 10 == 9) ? 2048 : 64;
        void *ptr = mimir_malloc(sz);
        if (!ptr) { __atomic_add_fetch(&g_v0_null, 1, __ATOMIC_RELAXED); continue; }
        memset(ptr, pat_byte(t, i), sz);
        const uint8_t *b = (const uint8_t *)ptr;
        for (size_t k = 0; k < sz; k++)
            if (b[k] != pat_byte(t, i)) { __atomic_add_fetch(&g_v0_pat, 1, __ATOMIC_RELAXED); break; }
        mimir_free(ptr);
    }
    a->elapsed = now_ns() - s;
    return NULL;
}
static long long run_v0(int nt) {
    pthread_t th[MAX_THREADS]; struct v0arg args[MAX_THREADS];
    long long per[REPS];
    for (int r = 0; r < REPS; r++) {
        g_v0_pat = 0; g_v0_null = 0;
        for (int t = 0; t < nt; t++) { args[t].tid = t; args[t].elapsed = 0; pthread_create(&th[t], NULL, v0_worker, &args[t]); }
        for (int t = 0; t < nt; t++) pthread_join(th[t], NULL);
        long long sum = 0; for (int t = 0; t < nt; t++) sum += args[t].elapsed;
        per[r] = sum / ((long long)nt * ITERS);
        if (g_v0_pat || g_v0_null) { printf("V0 threads=%d rep=%d FAIL pat=%ld null=%ld\n", nt, r, g_v0_pat, g_v0_null); return -1; }
    }
    return median_ll(per, REPS);
}

/* ---------- shared slot pool for V1/V2 models ---------- */
struct slot { struct slot *next; int owner; uint8_t data[64]; };
static struct slot g_pool[SHARDS][POOL_PER_SHARD];

/* ---------- V1: sharded (striped) locks model ---------- */
static struct slot *g_sh_free[SHARDS];
static pthread_mutex_t g_sh_lock[SHARDS];
static long g_v1_pat, g_v1_null, g_v1_leak;
static void *g_v1_ptrs[MAX_THREADS][ITERS];
static pthread_barrier_t g_bar1;
static void v1_init(void) {
    for (int s = 0; s < SHARDS; s++) {
        pthread_mutex_init(&g_sh_lock[s], NULL); g_sh_free[s] = NULL;
        for (int i = POOL_PER_SHARD - 1; i >= 0; i--) {
            g_pool[s][i].owner = s; g_pool[s][i].next = g_sh_free[s]; g_sh_free[s] = &g_pool[s][i];
        }
    }
}
static struct slot *v1_alloc(int shard) {
    pthread_mutex_lock(&g_sh_lock[shard]);
    struct slot *sl = g_sh_free[shard];
    if (sl) g_sh_free[shard] = sl->next;
    pthread_mutex_unlock(&g_sh_lock[shard]);
    return sl;
}
static void v1_free(struct slot *sl) {
    pthread_mutex_lock(&g_sh_lock[sl->owner]);
    sl->next = g_sh_free[sl->owner]; g_sh_free[sl->owner] = sl;
    pthread_mutex_unlock(&g_sh_lock[sl->owner]);
}
struct v1arg { int tid; int nt; long long elapsed; };
static void *v1_worker(void *p) {
    struct v1arg *a = (struct v1arg *)p; int t = a->tid;
    long long s0 = now_ns();
    int shard = t % SHARDS;
    for (int i = 0; i < ITERS; i++) {
        struct slot *sl = v1_alloc(shard);
        g_v1_ptrs[t][i] = sl;
        if (!sl) { __atomic_add_fetch(&g_v1_null, 1, __ATOMIC_RELAXED); continue; }
        memset(sl->data, pat_byte(t, i), 64);
    }
    pthread_barrier_wait(&g_bar1);
    int src = (t + 1) % a->nt;
    for (int i = 0; i < ITERS; i++) {
        struct slot *sl = (struct slot *)g_v1_ptrs[src][i];
        if (!sl) continue;
        for (int k = 0; k < 64; k++)
            if (sl->data[k] != pat_byte(src, i)) { __atomic_add_fetch(&g_v1_pat, 1, __ATOMIC_RELAXED); break; }
        v1_free(sl);
    }
    a->elapsed = now_ns() - s0;
    return NULL;
}
static long long run_v1(int nt, int cross_only_note) {
    (void)cross_only_note;
    pthread_t th[MAX_THREADS]; struct v1arg args[MAX_THREADS];
    long long per[REPS];
    for (int r = 0; r < REPS; r++) {
        v1_init(); g_v1_pat = 0; g_v1_null = 0;
        pthread_barrier_init(&g_bar1, NULL, (unsigned)nt);
        for (int t = 0; t < nt; t++) { args[t].tid = t; args[t].nt = nt; args[t].elapsed = 0; pthread_create(&th[t], NULL, v1_worker, &args[t]); }
        for (int t = 0; t < nt; t++) pthread_join(th[t], NULL);
        long long sum = 0; for (int t = 0; t < nt; t++) sum += args[t].elapsed;
        pthread_barrier_destroy(&g_bar1);
        per[r] = sum / ((long long)nt * ITERS);
        long live = 0;
        for (int sh = 0; sh < SHARDS; sh++) { struct slot *c = g_sh_free[sh]; while (c) { live++; c = c->next; } }
        g_v1_leak = (long)(SHARDS * POOL_PER_SHARD) - live;
        if (g_v1_pat || g_v1_null || g_v1_leak) { printf("V1 threads=%d rep=%d FAIL pat=%ld null=%ld unreturned=%ld\n", nt, r, g_v1_pat, g_v1_null, g_v1_leak); return -1; }
    }
    return median_ll(per, REPS);
}

/* ---------- V2: bounded remote-free queue model ---------- */
static struct slot *g_q_free[MAX_THREADS];
static struct slot *g_q_queue[MAX_THREADS][QUEUE_CAP];
static int g_q_head[MAX_THREADS], g_q_tail[MAX_THREADS], g_q_count[MAX_THREADS];
static pthread_mutex_t g_q_lock[MAX_THREADS];
static long g_v2_pat, g_v2_null, g_v2_full, g_v2_leak;
static void *g_v2_ptrs[MAX_THREADS][ITERS];
static pthread_barrier_t g_bar2;
static void v2_init(int nt) {
    for (int t = 0; t < nt; t++) {
        pthread_mutex_init(&g_q_lock[t], NULL);
        g_q_free[t] = NULL; g_q_head[t] = g_q_tail[t] = g_q_count[t] = 0;
        int shard = t % SHARDS;
        for (int i = 0; i < ITERS; i++) {
            struct slot *sl = &g_pool[shard][(i * 7 + t * 131) % POOL_PER_SHARD];
            (void)sl;
        }
        /* per-thread private stock: carve disjoint slots */
        for (int i = ITERS - 1; i >= 0; i--) {
            struct slot *sl = &g_pool[t % SHARDS][((t * ITERS + i) % POOL_PER_SHARD)];
            sl->owner = t; sl->next = g_q_free[t]; g_q_free[t] = sl;
        }
    }
}
static void v2_drain(int t) {
    pthread_mutex_lock(&g_q_lock[t]);
    while (g_q_count[t] > 0) {
        struct slot *sl = g_q_queue[t][g_q_head[t]];
        g_q_head[t] = (g_q_head[t] + 1) % QUEUE_CAP; g_q_count[t]--;
        sl->next = g_q_free[t]; g_q_free[t] = sl;
    }
    pthread_mutex_unlock(&g_q_lock[t]);
}
static struct slot *v2_alloc(int t) {
    if (!g_q_free[t]) v2_drain(t);
    struct slot *sl = g_q_free[t];
    if (sl) g_q_free[t] = sl->next;
    return sl;
}
static void v2_free(struct slot *sl, int freer) {
    int owner = sl->owner;
    if (owner == freer) { sl->next = g_q_free[freer]; g_q_free[freer] = sl; return; }
    pthread_mutex_lock(&g_q_lock[owner]);
    if (g_q_count[owner] >= QUEUE_CAP) {
        /* Bounded queue full: freer falls back to a direct
         * return into the owner's freelist under the same
         * lock (counted, never dropped, never blocking). */
        __atomic_add_fetch(&g_v2_full, 1, __ATOMIC_RELAXED);
        sl->next = g_q_free[owner]; g_q_free[owner] = sl;
    }
    else { g_q_queue[owner][g_q_tail[owner]] = sl; g_q_tail[owner] = (g_q_tail[owner] + 1) % QUEUE_CAP; g_q_count[owner]++; }
    pthread_mutex_unlock(&g_q_lock[owner]);
}
struct v2arg { int tid; int nt; long long elapsed; };
static void *v2_worker(void *p) {
    struct v2arg *a = (struct v2arg *)p; int t = a->tid;
    long long s0 = now_ns();
    for (int i = 0; i < ITERS; i++) {
        struct slot *sl = v2_alloc(t);
        g_v2_ptrs[t][i] = sl;
        if (!sl) { __atomic_add_fetch(&g_v2_null, 1, __ATOMIC_RELAXED); continue; }
        memset(sl->data, pat_byte(t, i), 64);
    }
    pthread_barrier_wait(&g_bar2);
    int src = (t + 1) % a->nt;
    for (int i = 0; i < ITERS; i++) {
        struct slot *sl = (struct slot *)g_v2_ptrs[src][i];
        if (!sl) continue;
        for (int k = 0; k < 64; k++)
            if (sl->data[k] != pat_byte(src, i)) { __atomic_add_fetch(&g_v2_pat, 1, __ATOMIC_RELAXED); break; }
        v2_free(sl, t);
    }
    v2_drain(t);
    a->elapsed = now_ns() - s0;
    return NULL;
}
static long long run_v2(int nt) {
    pthread_t th[MAX_THREADS]; struct v2arg args[MAX_THREADS];
    long long per[REPS];
    for (int r = 0; r < REPS; r++) {
        v2_init(nt); g_v2_pat = 0; g_v2_null = 0; g_v2_full = 0;
        pthread_barrier_init(&g_bar2, NULL, (unsigned)nt);
        for (int t = 0; t < nt; t++) { args[t].tid = t; args[t].nt = nt; args[t].elapsed = 0; pthread_create(&th[t], NULL, v2_worker, &args[t]); }
        for (int t = 0; t < nt; t++) pthread_join(th[t], NULL);
        long long sum = 0; for (int t = 0; t < nt; t++) sum += args[t].elapsed;
        pthread_barrier_destroy(&g_bar2);
        per[r] = sum / ((long long)nt * ITERS);
        long held = 0;
        for (int t = 0; t < nt; t++) { struct slot *c = g_q_free[t]; while (c) { held++; c = c->next; } held += g_q_count[t]; }
        g_v2_leak = (long)(nt * ITERS) - held;
        if (g_v2_pat || g_v2_null || g_v2_leak) { printf("V2 threads=%d rep=%d FAIL pat=%ld null=%ld unreturned=%ld\n", nt, r, g_v2_pat, g_v2_null, g_v2_leak); return -1; }
        printf("V2 threads=%d rep=%d queue_fallbacks=%ld\n", nt, r, g_v2_full);
    }
    return median_ll(per, REPS);
}

int main(void) {
    const int cfgs[4] = {1, 2, 4, 8};
    int rc = 0;
    printf("H004 probe: ITERS=%d REPS=%d SHARDS=%d QUEUE_CAP=%d\n", ITERS, REPS, SHARDS, QUEUE_CAP);
    printf("NOTE V1/V2 are lock/queue MODELS over a fixed 64B slot pool (alloc+crossfree legs combined, per-op over both legs); V0 is the real allocator Phase-B shape. Cross-variant comparison is lock-cost evidence, not an end-to-end allocator claim.\n");
    for (int i = 0; i < 4; i++) {
        int nt = cfgs[i];
        long long v0 = run_v0(nt);
        long long v1 = run_v1(nt, 0);
        long long v2 = run_v2(nt);
        printf("CONFIG threads=%d v0_real_ns_per_op=%lld v1_shard_model_ns_per_op=%lld v2_queue_model_ns_per_op=%lld %s\n",
               nt, v0, v1, v2, (v0 >= 0 && v1 >= 0 && v2 >= 0) ? "OK" : "FAIL");
        if (v0 < 0 || v1 < 0 || v2 < 0) rc = 1;
    }
    printf("RESULT: %s\n", rc == 0 ? "PASS" : "FAIL");
    return rc;
}
