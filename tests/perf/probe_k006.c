/* probe_k006.c - K-006 Phase 11 false-sharing and
 * concurrency-scaling probe.
 *
 * ONE source, TWO binaries (K-002 pattern):
 *   -DLEG_GLIBC : stdlib malloc/free (host glibc 2.36)
 *   (default)   : mimir_malloc/mimir_free from the banked
 *                 production objects (shipped -O2 build,
 *                 K-004), exactly as make test builds them.
 *
 * Workloads (argv[1]), thread count argv[2] in {1,2,4,8}:
 *   scale       H-002 Phase B replica: every worker loops
 *               ITERS times alloc / fill with a seeded byte
 *               pattern / verify byte-for-byte / free its
 *               OWN blocks against the shared allocator.
 *               Sizes: 64, except every 10th is 2048 (the
 *               H-002 Phase B mix verbatim). Reported
 *               ns_per_op is the H-002 figure: the SUM of
 *               per-thread elapsed times divided by total
 *               ops, so a flat line means perfect scaling
 *               and a rising line is contention loss.
 *   fs_packed   False-sharing control, no allocator in the
 *               loop: T counters packed 8 bytes apart in
 *               one shared array (same cache line(s));
 *               each thread increments only its own.
 *   fs_padded   Same, but each counter sits in its own
 *               128-byte slot (own cache line on this Pi:
 *               64-byte lines, getconf-verified in the
 *               recorded run).
 *   fs_alloc16  Each thread allocates ONE 16-byte object
 *               through the leg allocator before timing,
 *               then increments the first word in the timed
 *               loop. Neighbouring small allocations are
 *               the sharing suspect; the addresses are
 *               printed on ADDR lines so adjacency is
 *               checkable from the recorded run itself.
 *   fs_alloc128 Same with 128-byte objects (the padded-by-
 *               size control for the allocator legs).
 *
 * Every process runs an untimed warm-up at 1/10 scale
 * before its timed pass (K-001 M-6). Repetitions are
 * separate processes driven by run_bench_k006.sh;
 * distributions are computed by summarize_k006.py. A
 * single run of this binary is never a verdict.
 * Timing: CLOCK_MONOTONIC. Exit 0 iff zero failures:
 * pattern mismatches (scale), NULL allocations, or a
 * counter that does not reach its exact final count
 * (fs legs) all fail the process.
 * CSV line: leg,workload,threads,ops,total_ns,ns_per_op,
 *           wall_ns,fail,sink
 * For fs legs total_ns is the wall time (the counters are
 * cooperative, there is no per-thread sum to report);
 * ns_per_op = wall_ns / ops there.
 */
#define _POSIX_C_SOURCE 200809L
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef LEG_GLIBC
static void *(*alloc_fn)(size_t) = malloc;
static void (*free_fn)(void *) = free;
static const char *leg_name = "glibc";
#else
#include "mimir_alloc.h"
static void *(*alloc_fn)(size_t) = mimir_malloc;
static void (*free_fn)(void *) = mimir_free;
static const char *leg_name = "mimiralloc";
#endif

#define MAX_THREADS 8
#define SCALE_ITERS 1500
#define FS_ITERS 60000

static pthread_barrier_t g_bar;
static long long now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
static uint8_t pat_byte(int tid, int i)
{
    return (uint8_t)(0xA5u ^ (unsigned)(tid * 31) ^ (unsigned)i);
}

/* ---------------- scale (H-002 Phase B replica) -------- */
struct sarg { int tid; long long elapsed; long fail; uint64_t sink; };

static void scale_loop(struct sarg *a, int iters)
{
    int t = a->tid;
    for (int i = 0; i < iters; i++) {
        size_t sz = (i % 10 == 9) ? 2048 : 64;
        void *ptr = alloc_fn(sz);
        if (!ptr) { a->fail++; continue; }
        memset(ptr, pat_byte(t, i), sz);
        const uint8_t *b = (const uint8_t *)ptr;
        for (size_t k = 0; k < sz; k++)
            if (b[k] != pat_byte(t, i)) { a->fail++; break; }
        a->sink += b[0];
        free_fn(ptr);
    }
}
static void *scale_worker(void *p)
{
    struct sarg *a = (struct sarg *)p;
    scale_loop(a, SCALE_ITERS / 10);          /* warm-up */
    a->fail = 0; a->sink = 0;
    pthread_barrier_wait(&g_bar);
    long long s = now_ns();
    scale_loop(a, SCALE_ITERS);
    a->elapsed = now_ns() - s;
    return NULL;
}
static int run_scale(int nt)
{
    pthread_t th[MAX_THREADS];
    struct sarg args[MAX_THREADS];
    memset(args, 0, sizeof args);
    pthread_barrier_init(&g_bar, NULL, (unsigned)nt);
    long long w0 = now_ns();
    for (int t = 0; t < nt; t++) {
        args[t].tid = t;
        if (pthread_create(&th[t], NULL, scale_worker, &args[t]) != 0) {
            printf("%s,scale,%d,0,0,0,0,1,0\n", leg_name, nt);
            return 1;
        }
    }
    for (int t = 0; t < nt; t++) pthread_join(th[t], NULL);
    long long wall = now_ns() - w0;
    pthread_barrier_destroy(&g_bar);
    long fail = 0; long long sum = 0; uint64_t sink = 0;
    for (int t = 0; t < nt; t++) {
        fail += args[t].fail; sum += args[t].elapsed; sink += args[t].sink;
    }
    long ops = (long)nt * SCALE_ITERS;
    printf("%s,scale,%d,%ld,%lld,%lld,%lld,%ld,%llu\n",
           leg_name, nt, ops, sum, ops ? sum / ops : 0, wall, fail,
           (unsigned long long)sink);
    return fail ? 1 : 0;
}

/* ---------------- false-sharing legs ------------------- */
static volatile uint64_t g_packed[MAX_THREADS];
struct padded_slot { volatile uint64_t v; char pad[120]; };
static struct padded_slot g_padded[MAX_THREADS];
static void *g_alloc_ptrs[MAX_THREADS];

struct farg { int tid; int mode; long fail; };
enum { M_PACKED, M_PADDED, M_ALLOC16, M_ALLOC128 };

static void fs_loop(struct farg *a, long iters)
{
    int t = a->tid;
    volatile uint64_t *ctr;
    if (a->mode == M_PACKED) ctr = &g_packed[t];
    else if (a->mode == M_PADDED) ctr = &g_padded[t].v;
    else ctr = (volatile uint64_t *)g_alloc_ptrs[t];
    for (long i = 0; i < iters; i++) (*ctr)++;
}
static void *fs_worker(void *p)
{
    struct farg *a = (struct farg *)p;
    fs_loop(a, FS_ITERS / 10);               /* warm-up */
    /* reset own counter after warm-up */
    if (a->mode == M_PACKED) g_packed[a->tid] = 0;
    else if (a->mode == M_PADDED) g_padded[a->tid].v = 0;
    else *(volatile uint64_t *)g_alloc_ptrs[a->tid] = 0;
    pthread_barrier_wait(&g_bar);
    fs_loop(a, FS_ITERS);
    return NULL;
}
static uint64_t fs_final(struct farg *a)
{
    if (a->mode == M_PACKED) return g_packed[a->tid];
    if (a->mode == M_PADDED) return g_padded[a->tid].v;
    return *(volatile uint64_t *)g_alloc_ptrs[a->tid];
}
static int run_fs(int nt, int mode, const char *name)
{
    pthread_t th[MAX_THREADS];
    struct farg args[MAX_THREADS];
    memset(args, 0, sizeof args);
    memset((void *)g_packed, 0, sizeof g_packed);
    memset(g_padded, 0, sizeof g_padded);
    if (mode == M_ALLOC16 || mode == M_ALLOC128) {
        size_t sz = (mode == M_ALLOC16) ? 16 : 128;
        for (int t = 0; t < nt; t++) {
            g_alloc_ptrs[t] = alloc_fn(sz);
            if (!g_alloc_ptrs[t]) {
                printf("%s,%s,%d,0,0,0,0,1,0\n", leg_name, name, nt);
                return 1;
            }
            *(volatile uint64_t *)g_alloc_ptrs[t] = 0;
        }
        printf("#ADDR %s,%s,%d", leg_name, name, nt);
        for (int t = 0; t < nt; t++)
            printf(" %lx", (unsigned long)(uintptr_t)g_alloc_ptrs[t]);
        printf("\n");
        fflush(stdout);
    }
    pthread_barrier_init(&g_bar, NULL, (unsigned)nt);
    long long w0 = now_ns();
    for (int t = 0; t < nt; t++) {
        args[t].tid = t; args[t].mode = mode;
        if (pthread_create(&th[t], NULL, fs_worker, &args[t]) != 0) {
            printf("%s,%s,%d,0,0,0,0,1,0\n", leg_name, name, nt);
            return 1;
        }
    }
    for (int t = 0; t < nt; t++) pthread_join(th[t], NULL);
    long long wall = now_ns() - w0;
    pthread_barrier_destroy(&g_bar);
    long fail = 0; uint64_t sink = 0;
    for (int t = 0; t < nt; t++) {
        uint64_t fin = fs_final(&args[t]);
        sink += fin;
        if (fin != (uint64_t)FS_ITERS) fail++;
    }
    if (mode == M_ALLOC16 || mode == M_ALLOC128)
        for (int t = 0; t < nt; t++) free_fn(g_alloc_ptrs[t]);
    long ops = (long)nt * FS_ITERS;
    const char *leg = (mode == M_ALLOC16 || mode == M_ALLOC128)
                      ? leg_name : "raw";
    printf("%s,%s,%d,%ld,%lld,%lld,%lld,%ld,%llu\n",
           leg, name, nt, ops, wall, ops ? wall / ops : 0, wall, fail,
           (unsigned long long)sink);
    return fail ? 1 : 0;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <scale|fs_packed|fs_padded|"
                        "fs_alloc16|fs_alloc128> <threads>\n", argv[0]);
        return 2;
    }
    int nt = atoi(argv[2]);
    if (nt < 1 || nt > MAX_THREADS) {
        fprintf(stderr, "threads out of range\n");
        return 2;
    }
    if (strcmp(argv[1], "scale") == 0) return run_scale(nt);
    if (strcmp(argv[1], "fs_packed") == 0)
        return run_fs(nt, M_PACKED, "fs_packed");
    if (strcmp(argv[1], "fs_padded") == 0)
        return run_fs(nt, M_PADDED, "fs_padded");
    if (strcmp(argv[1], "fs_alloc16") == 0)
        return run_fs(nt, M_ALLOC16, "fs_alloc16");
    if (strcmp(argv[1], "fs_alloc128") == 0)
        return run_fs(nt, M_ALLOC128, "fs_alloc128");
    fprintf(stderr, "unknown workload\n");
    return 2;
}
