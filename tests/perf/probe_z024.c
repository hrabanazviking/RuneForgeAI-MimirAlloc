/* probe_z024.c - Z-024 Free-path anatomy (MimirAlloc Z13).
 *
 * Separates free cost by PATH and by THREAD GEOMETRY on the
 * banked core:
 *   path:  small (span path, <=1024 cutoff) vs mapping (large,
 *          one mapping per allocation, free = munmap whole)
 *   geometry:
 *     local        allocator thread == freeing thread
 *     remote       producer thread allocates, a DIFFERENT
 *                  consumer thread frees (same-CCD pair and
 *                  cross-CCD pair selected by CPU numbers)
 *     swap         T threads each allocate a batch, then each
 *                  frees the NEXT thread's batch concurrently
 *                  (remote free under lock contention)
 * Small frees are split two ways (the retention anatomy):
 *   steady   free every SECOND block of the batch: every span
 *            keeps live blocks, so no span is released and the
 *            timed cost is lock + registry lookup + free-list
 *            threading only.
 *   emptying free ALL blocks: spans empty out; all but the
 *            retention bound (2 per class, G-007 default) are
 *            released whole (munmap of one page) inside the
 *            timed region, so the delta emptying-minus-steady
 *            prices the span-release share.
 * Mapping frees always munmap; that cost is intrinsic to the
 * path and is reported as-is.
 *
 * Method (docs/Z13_MEASUREMENT_PROTOCOL.md, Z-004): the
 * runner runs each phase as separate PROCESSES (7 measured,
 * 3 warm-up discarded), pinned; inside a process one batch is
 * timed with CLOCK_MONOTONIC around ONLY the free loop.
 * Pattern-touch + checksum: the allocating thread writes a
 * per-block pattern (first and last byte); the freeing side
 * verifies the checksum BEFORE the timer starts (K-002
 * workload-equivalence discipline), so verification never
 * pollutes the timed region and a wrong checksum aborts.
 * Remote handoff: preallocated pointer array + padded atomic
 * sequence words; no allocation, no mutex, no condvar in the
 * handoff, and the timer starts only after the consumer
 * observes the publish flag, so handoff cost is excluded
 * from the free cost. Threads pin themselves with
 * pthread_setaffinity_np to the exact CPUs named on the
 * command line (CCD0 = CPUs 0-7, CCD1 = CPUs 8-15).
 *
 * Dual leg (bench_k002 / probe_z017 pattern): -DLEG_GLIBC
 * builds the identical probe against glibc malloc/free as a
 * comparator; the default leg links the banked mimir core.
 *
 * Output (stdout, one line):
 *   RESULT,leg,phase,size,n_freed,ns_total,ns_per_op,checksum
 * Exit 0 iff every allocation succeeded and the checksum
 * verified on the freeing side.
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <stdatomic.h>

#ifdef LEG_GLIBC
static void *leg_alloc(size_t n) { return malloc(n); }
static void leg_free(void *p) { free(p); }
static const char *LEG_NAME = "glibc";
#else
#include "mimir_alloc.h"
static void *leg_alloc(size_t n) { return mimir_malloc(n); }
static void leg_free(void *p) { mimir_free(p); }
static const char *LEG_NAME = "mimir";
#endif

static long long now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static int pin_cpu(int cpu)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return pthread_setaffinity_np(pthread_self(), sizeof set, &set);
}

static unsigned char pat(long i, size_t size)
{
    return (unsigned char)((i * 131u + (unsigned)size) & 0xFFu);
}

static uint64_t alloc_batch(void **ptrs, long n, size_t size)
{
    uint64_t chk = 0;
    long i;
    for (i = 0; i < n; i++) {
        unsigned char *p = (unsigned char *)leg_alloc(size);
        if (p == NULL) {
            fprintf(stderr, "alloc NULL at %ld size %zu\n", i, size);
            exit(1);
        }
        p[0] = pat(i, size);
        p[size - 1] = pat(i + 7, size);
        chk += (uint64_t)p[0] + ((uint64_t)p[size - 1] << 8);
        ptrs[i] = p;
    }
    return chk;
}

static uint64_t verify_batch(void **ptrs, long n, size_t size,
                             long start, long stride)
{
    uint64_t chk = 0;
    long i;
    for (i = start; i < n; i += stride) {
        const unsigned char *p = (const unsigned char *)ptrs[i];
        chk += (uint64_t)p[0] + ((uint64_t)p[size - 1] << 8);
    }
    return chk;
}

static uint64_t expect_chk(long n, size_t size, long start, long stride)
{
    uint64_t chk = 0;
    long i;
    for (i = start; i < n; i += stride) {
        chk += (uint64_t)pat(i, size) +
               ((uint64_t)pat(i + 7, size) << 8);
    }
    return chk;
}

static long long time_free(void **ptrs, long n, long start, long stride)
{
    long long t0 = now_ns();
    long i;
    for (i = start; i < n; i += stride) {
        leg_free(ptrs[i]);
    }
    return now_ns() - t0;
}

static void emit(const char *phase, size_t size, long nfreed,
                 long long ns, uint64_t chk)
{
    printf("RESULT,%s,%s,%zu,%ld,%lld,%.3f,%llu\n", LEG_NAME, phase,
           size, nfreed, ns, (double)ns / (double)nfreed,
           (unsigned long long)chk);
    fflush(stdout);
}

/* ---- local geometry ------------------------------------ */
static int phase_local(size_t size, long n, int emptying, int cpu)
{
    void **ptrs;
    uint64_t chk;
    long long ns;
    long nfreed;
    if (pin_cpu(cpu) != 0) { fprintf(stderr, "pin fail\n"); return 1; }
    ptrs = (void **)malloc((size_t)n * sizeof(void *));
    if (ptrs == NULL) return 1;
    chk = alloc_batch(ptrs, n, size);
    if (!emptying) {
        if (verify_batch(ptrs, n, size, 1, 2) !=
            expect_chk(n, size, 1, 2)) {
            fprintf(stderr, "checksum mismatch (steady)\n"); return 1;
        }
        ns = time_free(ptrs, n, 1, 2);
        nfreed = n / 2;
        emit("local_steady", size, nfreed, ns, chk);
        (void)time_free(ptrs, n, 0, 2);
    } else {
        if (verify_batch(ptrs, n, size, 0, 1) !=
            expect_chk(n, size, 0, 1)) {
            fprintf(stderr, "checksum mismatch (emptying)\n"); return 1;
        }
        ns = time_free(ptrs, n, 0, 1);
        emit("local_emptying", size, n, ns, chk);
    }
    free(ptrs);
    return 0;
}

/* ---- local geometry, REVERSE free order ----------------
 * Same batch shape as local emptying, but frees run newest-
 * first: the newest blocks live in the HEAD span / the HEAD
 * mapping descriptor, so the registry walks are shortest.
 * Paired with local_emptying (oldest-first, deepest walk)
 * it isolates the walk-depth share of the free cost. */
static int phase_local_rev(size_t size, long n, int cpu)
{
    void **ptrs;
    uint64_t chk;
    long long t0, ns;
    long i;
    if (pin_cpu(cpu) != 0) { fprintf(stderr, "pin fail\n"); return 1; }
    ptrs = (void **)malloc((size_t)n * sizeof(void *));
    if (ptrs == NULL) return 1;
    chk = alloc_batch(ptrs, n, size);
    if (verify_batch(ptrs, n, size, 0, 1) != expect_chk(n, size, 0, 1)) {
        fprintf(stderr, "checksum mismatch (localrev)\n"); return 1;
    }
    t0 = now_ns();
    for (i = n - 1; i >= 0; i--) leg_free(ptrs[i]);
    ns = now_ns() - t0;
    emit("local_rev_emptying", size, n, ns, chk);
    free(ptrs);
    return 0;
}

/* ---- remote geometry (producer -> consumer) ------------ */
struct handoff {
    void **ptrs;
    long n;
    size_t size;
    int emptying;
    int alloc_cpu;
    int free_cpu;
    uint64_t chk_alloc;
    long long free_ns;
    long nfreed;
    int failed;
    _Atomic unsigned long seq;
    char pad[64];
    _Atomic unsigned long ack;
};

static void *producer_main(void *arg)
{
    struct handoff *h = (struct handoff *)arg;
    if (pin_cpu(h->alloc_cpu) != 0) { h->failed = 1; return NULL; }
    h->chk_alloc = alloc_batch(h->ptrs, h->n, h->size);
    atomic_store_explicit(&h->seq, 1ul, memory_order_release);
    while (atomic_load_explicit(&h->seq, memory_order_acquire) != 2ul) {
    }
    return NULL;
}

static void *consumer_main(void *arg)
{
    struct handoff *h = (struct handoff *)arg;
    long start, stride;
    if (pin_cpu(h->free_cpu) != 0) { h->failed = 1; return NULL; }
    while (atomic_load_explicit(&h->seq, memory_order_acquire) != 1ul) {
    }
    start = h->emptying ? 0 : 1;
    stride = h->emptying ? 1 : 2;
    if (verify_batch(h->ptrs, h->n, h->size, start, stride) !=
        expect_chk(h->n, h->size, start, stride)) {
        h->failed = 2;
        atomic_store_explicit(&h->seq, 2ul, memory_order_release);
        return NULL;
    }
    h->free_ns = time_free(h->ptrs, h->n, start, stride);
    h->nfreed = h->emptying ? h->n : h->n / 2;
    atomic_store_explicit(&h->seq, 2ul, memory_order_release);
    return NULL;
}

static int phase_remote(size_t size, long n, int emptying,
                        int alloc_cpu, int free_cpu, const char *label)
{
    struct handoff h;
    pthread_t prod, cons;
    memset(&h, 0, sizeof h);
    h.ptrs = (void **)malloc((size_t)n * sizeof(void *));
    if (h.ptrs == NULL) return 1;
    h.n = n; h.size = size; h.emptying = emptying;
    h.alloc_cpu = alloc_cpu; h.free_cpu = free_cpu;
    atomic_init(&h.seq, 0ul);
    atomic_init(&h.ack, 0ul);
    if (pthread_create(&cons, NULL, consumer_main, &h) != 0) return 1;
    if (pthread_create(&prod, NULL, producer_main, &h) != 0) return 1;
    pthread_join(prod, NULL);
    pthread_join(cons, NULL);
    if (h.failed) {
        fprintf(stderr, "remote phase failed code=%d\n", h.failed);
        return 1;
    }
    emit(label, size, h.nfreed, h.free_ns, h.chk_alloc);
    if (!emptying) {
        long i;
        for (i = 0; i < n; i += 2) leg_free(h.ptrs[i]);
    }
    free(h.ptrs);
    return 0;
}

/* ---- swap geometry (concurrent remote frees) ----------- */
#define MAXSWAP 8
struct swap_worker {
    void **mine;
    void **victim;
    long n;
    size_t size;
    int cpu;
    int failed;
    long long free_ns;
    uint64_t chk;
};
static _Atomic unsigned long swap_go;
static _Atomic unsigned long swap_done;
static _Atomic unsigned long swap_verified;

static void *swap_main(void *arg)
{
    struct swap_worker *w = (struct swap_worker *)arg;
    if (pin_cpu(w->cpu) != 0) { w->failed = 1; return NULL; }
    w->chk = alloc_batch(w->mine, w->n, w->size);
    atomic_fetch_add_explicit(&swap_done, 1ul, memory_order_acq_rel);
    /* Stage 1 (go==1): verify the victim batch BEFORE the
     * timed window opens (go==2), so verification cost is
     * never inside the swap wall time. */
    while (atomic_load_explicit(&swap_go, memory_order_acquire) < 1ul) {
    }
    if (verify_batch(w->victim, w->n, w->size, 1, 2) !=
        expect_chk(w->n, w->size, 1, 2)) {
        w->failed = 2;
        atomic_fetch_add_explicit(&swap_verified, 1ul, memory_order_acq_rel);
        return NULL;
    }
    atomic_fetch_add_explicit(&swap_verified, 1ul, memory_order_acq_rel);
    while (atomic_load_explicit(&swap_go, memory_order_acquire) < 2ul) {
    }
    w->free_ns = time_free(w->victim, w->n, 1, 2);
    return NULL;
}

static int phase_swap(size_t size, long n, int nthreads,
                      const int *cpus, const char *label)
{
    struct swap_worker wk[MAXSWAP];
    pthread_t th[MAXSWAP];
    long long wall0, wall;
    uint64_t chk = 0;
    int t;
    if (nthreads < 2 || nthreads > MAXSWAP) return 2;
    memset(wk, 0, sizeof wk);
    atomic_store(&swap_go, 0ul);
    atomic_store(&swap_done, 0ul);
    atomic_store(&swap_verified, 0ul);
    for (t = 0; t < nthreads; t++) {
        wk[t].mine = (void **)malloc((size_t)n * sizeof(void *));
        if (wk[t].mine == NULL) return 1;
        wk[t].n = n; wk[t].size = size; wk[t].cpu = cpus[t];
    }
    for (t = 0; t < nthreads; t++) {
        wk[t].victim = wk[(t + 1) % nthreads].mine;
    }
    for (t = 0; t < nthreads; t++) {
        if (pthread_create(&th[t], NULL, swap_main, &wk[t]) != 0)
            return 1;
    }
    while (atomic_load_explicit(&swap_done, memory_order_acquire) !=
           (unsigned long)nthreads) {
    }
    atomic_store_explicit(&swap_go, 1ul, memory_order_release);
    while (atomic_load_explicit(&swap_verified, memory_order_acquire) !=
           (unsigned long)nthreads) {
    }
    for (t = 0; t < nthreads; t++) {
        if (wk[t].failed) {
            fprintf(stderr, "swap verify failed t=%d\n", t);
            return 1;
        }
    }
    wall0 = now_ns();
    atomic_store_explicit(&swap_go, 2ul, memory_order_release);
    for (t = 0; t < nthreads; t++) pthread_join(th[t], NULL);
    wall = now_ns() - wall0;
    for (t = 0; t < nthreads; t++) {
        if (wk[t].failed) {
            fprintf(stderr, "swap worker failed t=%d code=%d\n",
                    t, wk[t].failed);
            return 1;
        }
        chk += wk[t].chk;
    }
    {
        long total = (long)nthreads * (n / 2);
        printf("RESULT,%s,%s,%zu,%ld,%lld,%.3f,%llu\n", LEG_NAME,
               label, size, total, wall, (double)wall / (double)total,
               (unsigned long long)chk);
        fflush(stdout);
    }
    for (t = 0; t < nthreads; t++) {
        long i;
        for (i = 0; i < n; i += 2) leg_free(wk[t].mine[i]);
        free(wk[t].mine);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr,
            "usage:\n"
            "  probe_z024 local <size> <n> <emptying 0|1> <cpu>\n"
            "  probe_z024 remote <size> <n> <emptying 0|1>"
            " <alloc_cpu> <free_cpu> <label>\n"
            "  probe_z024 swap <size> <n> <label> <cpu...> \n");
        return 2;
    }
    if (strcmp(argv[1], "local") == 0 && argc == 6) {
        return phase_local((size_t)strtoul(argv[2], NULL, 10),
                           strtol(argv[3], NULL, 10),
                           atoi(argv[4]), atoi(argv[5]));
    }
    if (strcmp(argv[1], "localrev") == 0 && argc == 5) {
        return phase_local_rev((size_t)strtoul(argv[2], NULL, 10),
                               strtol(argv[3], NULL, 10), atoi(argv[4]));
    }
    if (strcmp(argv[1], "remote") == 0 && argc == 8) {
        return phase_remote((size_t)strtoul(argv[2], NULL, 10),
                            strtol(argv[3], NULL, 10), atoi(argv[4]),
                            atoi(argv[5]), atoi(argv[6]), argv[7]);
    }
    if (strcmp(argv[1], "swap") == 0 && argc >= 6) {
        int cpus[MAXSWAP];
        int nt = argc - 5;
        int i;
        if (nt > MAXSWAP) return 2;
        for (i = 0; i < nt; i++) cpus[i] = atoi(argv[5 + i]);
        return phase_swap((size_t)strtoul(argv[2], NULL, 10),
                          strtol(argv[3], NULL, 10), nt, cpus, argv[4]);
    }
    fprintf(stderr, "bad arguments\n");
    return 2;
}
