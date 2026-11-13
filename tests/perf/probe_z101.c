/* probe_z101.c - Z-101 storm battery, final form.
 *
 * Slice Z-101 (docs/MIMIRALLOC_Z13_ROADMAP.md, Wave 12):
 * "Thread-churn (Z-042) + free-storm (Z-043) + fork storm
 * combined at machine scale against the near-final core.
 * Gate: battery green; retained-bytes figures recorded."
 *
 * One process per rep; the runner (run_z101.sh) follows
 * the Z-004 protocol as in Z-042: one warm-up process per
 * leg discarded, then 3 recorded processes per leg,
 * alternating legs. Two legs from one source:
 *   mimir  - the battery against the near-final core,
 *            with the allocator's own accounting surfaces
 *            (J-002 stats, I-003 trim, G-007 bound, J-006
 *            metadata walk, Z-028/Z-029 TLS exit counters)
 *            asserted at quiescent snapshots.
 *   glibc  - (-DUSE_GLIBC) the identical shapes and fork
 *            counts with malloc/free: wall-time comparator
 *            only. No verdict vs glibc is claimed by this
 *            slice's gate; walls are recorded side by side
 *            for the Wave-12 verdict slices.
 *
 * Phases (mimir leg; glibc leg runs the same shapes
 * without the mimir surfaces):
 *   CAL  one calibration thread running the exact Z-042
 *        churn shape, capturing its own TLS snapshot just
 *        before exit: the exit destructor must return
 *        exactly its cached blocks (Z-042 identity).
 *   CMB  THE COMBINED STORM - the three storms OVERLAP:
 *        16 churn looper threads run short-lived
 *        alloc/touch/verify/free cycles continuously;
 *        16 ring workers run fixed cross-free rounds
 *        (16 x 20 rounds x 256 blocks of 64 B = 81,920
 *        remote frees, every block pattern-verified by
 *        its freer); while both run, main performs 256
 *        fork+exec(/bin/true) children (the Z-044 E2
 *        proven shape: the child only execs, it never
 *        touches the allocator). Churn loopers run for
 *        the whole fork window (stop flag set only after
 *        the last child is reaped). Asserted: every fork
 *        child exits 0, every looper iterated, cross-free
 *        count exact, zero alloc/pattern/create failures,
 *        live_small back at baseline afterwards.
 *   STM  thread-churn at Z-042 scale: 10,000 one-shot
 *        churn threads in waves of 32 (the host's
 *        hardware-thread count), each 35 allocations
 *        (8 in each of classes 16/64/256/1024, one
 *        2048 B large, two held blocks handed to main).
 *        Asserted exactly as Z-042: exit drains +10,000,
 *        exit-drained blocks +10,000 x calibration,
 *        counters settled after a 100 ms quiesce, live
 *        exactly baseline + 20,000 held, then main frees
 *        the held blocks and live returns to baseline.
 *   RNG  free-storm at Z-043 S1 scale: 32 threads ring
 *        cross-free, 3 rounds x 2,048 blocks of 64 B =
 *        196,608 remote frees, pattern-verified by the
 *        freer before the free (K-002 discipline).
 *   FRK  fork storm, supported shapes only (Z-044):
 *        16 quiescent fork+alloc children (parent single-
 *        threaded at the fork instant; child allocates,
 *        verifies, frees, _exit(0/42)) + 500 sequential
 *        fork+exec(/bin/true) children. The fork-while-
 *        busy ALLOC shape stays measured-unsupported per
 *        Z-044 B1 and is NOT exercised as a correctness
 *        leg; under load this battery forks only to exec
 *        (CMB), exactly the shape Z-044 proved.
 *   TRIM byte-exact close (Z-042): main drains its own
 *        TLS cache, mimir_core_trim_retained() releases
 *        the retained spans, and bytes_mapped must return
 *        to the pristine S0 figure to the byte.
 *
 * Retained bytes are printed at every quiescent snapshot
 * (RETAINED lines in the summary): after CAL, after CMB,
 * after STM (the Z-042 gate figure), after RNG, after
 * FRK, pre-trim, and final (0 after trim).
 *
 * Accounting note (near-final core): the mapped-bytes
 * identity carries the Z-048 pool term - Q-MAPPED is
 * live span bytes + live mapping bytes + pooled mapping
 * bytes (tests/core/mimir_pool_z048.h query pair). The
 * two-term Z-042 form predates the mapping pool; the
 * first trial run of this probe measured the difference
 * exactly (8,192 B early, 131,072 B after the churn
 * storm) and the core's own Q-MAPPED definition names
 * the third term. The trim close releases the pool too
 * (pool_release_all in mimir_core_trim_retained), so
 * the byte-exact return to S0 still arbitrates leaks.
 *
 * Exit status: 0 iff every printed CHECK passes and the
 * failure counters are all zero. Any failure makes the
 * battery RED, honestly.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#ifndef USE_GLIBC
#include "mimir_alloc.h"
#include "../../tests/core/mimir_stats_j002.h"
#include "../../tests/core/mimir_pool_z048.h"
#include "../../tests/contract/mimir_tls_cache_z028.h"
#define P_MALLOC(sz) mimir_malloc(sz)
#define P_FREE(p) mimir_free(p)
#else
#define P_MALLOC(sz) malloc(sz)
#define P_FREE(p) free(p)
#endif

/* ---- churn shape (identical to Z-042) ---- */
#define THREADS_TOTAL 10000
#define WAVE 32
#define NCHURN_CLASS 4
#define PER_CLASS 8
#define LARGE_SIZE 2048u
#define SEQ_LARGE 32
#define SEQ_HELD0 33
#define SEQ_HELD1 34
#define PAGE_BYTES 4096u

static const size_t churn_classes[NCHURN_CLASS] = {16, 64, 256, 1024};

/* ---- ring shape ---- */
#define RING_MAX 32
#define CMB_RING_THREADS 16
#define CMB_RING_PER 256
#define CMB_RING_ROUNDS 20
#define RNG_THREADS 32
#define RNG_PER 2048
#define RNG_ROUNDS 3
#define RING_SIZE 64u

/* ---- combined churn loopers ---- */
#define CMB_CHURN_THREADS 16

/* ---- fork shape ---- */
#define CMB_FORKS 256
#define FRK_ALLOC_CHILDREN 16
#define FRK_EXEC_CHILDREN 500

static _Atomic uint64_t g_alloc_fail;
static _Atomic uint64_t g_pattern_fail;
static _Atomic uint64_t g_create_fail;
static _Atomic uint64_t g_checksum;
static _Atomic uint64_t g_ring_checksum;
static _Atomic uint64_t g_cross_frees;
static _Atomic int g_stop;
static _Atomic uint64_t g_churn_iters;

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static unsigned char seed_of(int tid, int seq, size_t size)
{
    return (unsigned char)(((unsigned)tid * 37u + (unsigned)seq * 11u +
                             (unsigned)size) & 0xffu);
}

static void touch(void *ptr, size_t size, int tid, int seq)
{
    unsigned char *b = (unsigned char *)ptr;
    unsigned char s = seed_of(tid, seq, size);
    b[0] = s;
    b[size - 1] = (unsigned char)(s ^ 0xa5u);
    if (size >= 64) {
        b[size / 2] = (unsigned char)(s ^ 0x3cu);
    }
}

static int verify_pat(const void *ptr, size_t size, int tid, int seq)
{
    const unsigned char *b = (const unsigned char *)ptr;
    unsigned char s = seed_of(tid, seq, size);
    if (b[0] != s) {
        return 0;
    }
    if (b[size - 1] != (unsigned char)(s ^ 0xa5u)) {
        return 0;
    }
    if (size >= 64 && b[size / 2] != (unsigned char)(s ^ 0x3cu)) {
        return 0;
    }
    return 1;
}

/* ================= churn (Z-042 shape) ================= */

struct worker_arg {
    int tid;
    int capture;
    void *held[2];
#ifndef USE_GLIBC
    struct mimir_tls_cache_snapshot cap;
#endif
};

static struct worker_arg g_args[THREADS_TOTAL];
#ifndef USE_GLIBC
static struct worker_arg g_cal_arg;
#endif

static void *churn_worker(void *p)
{
    struct worker_arg *arg = (struct worker_arg *)p;
    void *ptrs[33];
    size_t sizes[33];
    int ci, i, seq;

    for (seq = 0; seq < 33; seq++) {
        ptrs[seq] = NULL;
        sizes[seq] = 0;
    }
    for (ci = 0; ci < NCHURN_CLASS; ci++) {
        for (i = 0; i < PER_CLASS; i++) {
            size_t sz = churn_classes[ci];
            void *b = P_MALLOC(sz);
            seq = ci * PER_CLASS + i;
            if (b == NULL) {
                atomic_fetch_add_explicit(&g_alloc_fail, 1,
                                          memory_order_relaxed);
                continue;
            }
            touch(b, sz, arg->tid, seq);
            ptrs[seq] = b;
            sizes[seq] = sz;
        }
    }
    {
        void *b = P_MALLOC(LARGE_SIZE);
        if (b == NULL) {
            atomic_fetch_add_explicit(&g_alloc_fail, 1,
                                      memory_order_relaxed);
        } else {
            touch(b, LARGE_SIZE, arg->tid, SEQ_LARGE);
            ptrs[SEQ_LARGE] = b;
            sizes[SEQ_LARGE] = LARGE_SIZE;
        }
    }
    arg->held[0] = P_MALLOC(64);
    if (arg->held[0] == NULL) {
        atomic_fetch_add_explicit(&g_alloc_fail, 1, memory_order_relaxed);
    } else {
        touch(arg->held[0], 64, arg->tid, SEQ_HELD0);
    }
    arg->held[1] = P_MALLOC(512);
    if (arg->held[1] == NULL) {
        atomic_fetch_add_explicit(&g_alloc_fail, 1, memory_order_relaxed);
    } else {
        touch(arg->held[1], 512, arg->tid, SEQ_HELD1);
    }
    {
        uint64_t lsum = 0;
        for (seq = 0; seq < 33; seq++) {
            if (ptrs[seq] != NULL) {
                unsigned char s = seed_of(arg->tid, seq, sizes[seq]);
                if (!verify_pat(ptrs[seq], sizes[seq], arg->tid, seq)) {
                    atomic_fetch_add_explicit(&g_pattern_fail, 1,
                                              memory_order_relaxed);
                } else {
                    lsum += (uint64_t)s + (uint64_t)(s ^ 0xa5u);
                }
            }
        }
        if (arg->held[0] != NULL) {
            unsigned char s = seed_of(arg->tid, SEQ_HELD0, 64);
            if (!verify_pat(arg->held[0], 64, arg->tid, SEQ_HELD0)) {
                atomic_fetch_add_explicit(&g_pattern_fail, 1,
                                          memory_order_relaxed);
            } else {
                lsum += (uint64_t)s + (uint64_t)(s ^ 0xa5u);
            }
        }
        if (arg->held[1] != NULL) {
            unsigned char s = seed_of(arg->tid, SEQ_HELD1, 512);
            if (!verify_pat(arg->held[1], 512, arg->tid, SEQ_HELD1)) {
                atomic_fetch_add_explicit(&g_pattern_fail, 1,
                                          memory_order_relaxed);
            } else {
                lsum += (uint64_t)s + (uint64_t)(s ^ 0xa5u);
            }
        }
        atomic_fetch_add_explicit(&g_checksum, lsum, memory_order_relaxed);
    }
    for (seq = 0; seq < 33; seq++) {
        if (ptrs[seq] != NULL) {
            P_FREE(ptrs[seq]);
        }
    }
#ifndef USE_GLIBC
    if (arg->capture) {
        if (mimir_tls_cache_snapshot(&arg->cap) != 0) {
            atomic_fetch_add_explicit(&g_pattern_fail, 1,
                                      memory_order_relaxed);
        }
    }
#endif
    return NULL;
}

static double run_waves(int count, double *worst_wave_ns)
{
    pthread_t th[WAVE];
    double t0 = now_ns();
    double worst = 0.0;
    int base, i, k;

    for (base = 0; base < count; base += WAVE) {
        double w0 = now_ns();
        k = count - base;
        if (k > WAVE) {
            k = WAVE;
        }
        {
            int created[WAVE];
            for (i = 0; i < k; i++) {
                created[i] =
                    (pthread_create(&th[i], NULL, churn_worker,
                                    &g_args[base + i]) == 0);
                if (!created[i]) {
                    atomic_fetch_add_explicit(&g_create_fail, 1,
                                              memory_order_relaxed);
                }
            }
            for (i = 0; i < k; i++) {
                if (created[i]) {
                    (void)pthread_join(th[i], NULL);
                }
            }
        }
        {
            double w = now_ns() - w0;
            if (w > worst) {
                worst = w;
            }
        }
    }
    if (worst_wave_ns != NULL) {
        *worst_wave_ns = worst;
    }
    return now_ns() - t0;
}

/* ================= combined churn looper ================= */

static void *churn_looper(void *p)
{
    int tid = (int)(intptr_t)p;
    uint64_t iters = 0;
    static const size_t sizes[4] = {64, 64, 256, 1024};

    while (!atomic_load_explicit(&g_stop, memory_order_relaxed)) {
        void *ptrs[4];
        int i;
        for (i = 0; i < 4; i++) {
            ptrs[i] = P_MALLOC(sizes[i]);
            if (ptrs[i] == NULL) {
                atomic_fetch_add_explicit(&g_alloc_fail, 1,
                                          memory_order_relaxed);
            } else {
                touch(ptrs[i], sizes[i], tid, i);
            }
        }
        for (i = 0; i < 4; i++) {
            if (ptrs[i] != NULL) {
                if (!verify_pat(ptrs[i], sizes[i], tid, i)) {
                    atomic_fetch_add_explicit(&g_pattern_fail, 1,
                                              memory_order_relaxed);
                }
                P_FREE(ptrs[i]);
            }
        }
        iters++;
    }
    atomic_fetch_add_explicit(&g_churn_iters, iters, memory_order_relaxed);
    return NULL;
}

/* ================= ring cross-free (Z-043 shape) ================= */

static void block_fill(void *ptr, uint32_t producer, uint32_t seq)
{
    uint64_t tag = ((uint64_t)producer << 32) | seq;
    unsigned char mid;
    memcpy(ptr, &tag, 8);
    mid = (unsigned char)(tag & 0x5au);
    memset((unsigned char *)ptr + 8, mid, RING_SIZE - 9);
    ((unsigned char *)ptr)[RING_SIZE - 1] = (unsigned char)(mid ^ 0xffu);
}

static int block_check(const void *ptr, uint32_t producer, uint32_t seq,
                       uint64_t *contrib)
{
    uint64_t tag;
    const unsigned char *b = (const unsigned char *)ptr;
    memcpy(&tag, ptr, 8);
    if (tag != (((uint64_t)producer << 32) | seq)) {
        return 0;
    }
    *contrib = (uint64_t)b[0] + (uint64_t)b[RING_SIZE - 1];
    return 1;
}

struct ring_job {
    int id;
    int nthreads;
    int per;
    int rounds;
    pthread_barrier_t *bar;
};

static void **g_ring_all[RING_MAX];

static void *ring_worker(void *p)
{
    struct ring_job *job = (struct ring_job *)p;
    uint64_t lsum = 0;
    int round, i;

    for (round = 0; round < job->rounds; round++) {
        void **mine = (void **)calloc((size_t)job->per, sizeof(void *));
        if (mine == NULL) {
            atomic_fetch_add_explicit(&g_alloc_fail, 1,
                                      memory_order_relaxed);
            mine = NULL;
        }
        if (mine != NULL) {
            for (i = 0; i < job->per; i++) {
                void *b = P_MALLOC(RING_SIZE);
                if (b == NULL) {
                    atomic_fetch_add_explicit(&g_alloc_fail, 1,
                                              memory_order_relaxed);
                } else {
                    block_fill(b, (uint32_t)job->id, (uint32_t)i);
                    mine[i] = b;
                }
            }
        }
        g_ring_all[job->id] = mine;
        (void)pthread_barrier_wait(job->bar);
        {
            int vid = (job->id + 1) % job->nthreads;
            void **victim = g_ring_all[vid];
            if (victim != NULL) {
                for (i = 0; i < job->per; i++) {
                    if (victim[i] != NULL) {
                        uint64_t contrib = 0;
                        if (!block_check(victim[i], (uint32_t)vid,
                                         (uint32_t)i, &contrib)) {
                            atomic_fetch_add_explicit(
                                &g_pattern_fail, 1, memory_order_relaxed);
                        } else {
                            lsum += contrib;
                        }
                        atomic_fetch_add_explicit(&g_cross_frees, 1,
                                                  memory_order_relaxed);
                        P_FREE(victim[i]);
                    }
                }
            }
        }
        (void)pthread_barrier_wait(job->bar);
        free(mine);
    }
    atomic_fetch_add_explicit(&g_ring_checksum, lsum, memory_order_relaxed);
    return NULL;
}

static double run_ring(int nthreads, int per, int rounds)
{
    pthread_barrier_t bar;
    pthread_t th[RING_MAX];
    struct ring_job jobs[RING_MAX];
    int created[RING_MAX];
    int i;
    double t0;

    (void)pthread_barrier_init(&bar, NULL, (unsigned)nthreads);
    for (i = 0; i < nthreads; i++) {
        jobs[i].id = i;
        jobs[i].nthreads = nthreads;
        jobs[i].per = per;
        jobs[i].rounds = rounds;
        jobs[i].bar = &bar;
        g_ring_all[i] = NULL;
    }
    t0 = now_ns();
    for (i = 0; i < nthreads; i++) {
        created[i] = (pthread_create(&th[i], NULL, ring_worker,
                                     &jobs[i]) == 0);
        if (!created[i]) {
            atomic_fetch_add_explicit(&g_create_fail, 1,
                                      memory_order_relaxed);
        }
    }
    for (i = 0; i < nthreads; i++) {
        if (created[i]) {
            (void)pthread_join(th[i], NULL);
        }
    }
    (void)pthread_barrier_destroy(&bar);
    return now_ns() - t0;
}

/* ================= fork helpers ================= */

/* fork+exec /bin/true; the child only execs (Z-044 E2
 * shape). Returns 1 iff the child exited 0. */
static int fork_exec_one(void)
{
    pid_t pid;
    int st = 0;
    pid_t w;

    fflush(stdout);
    pid = fork();
    if (pid < 0) {
        return 0;
    }
    if (pid == 0) {
        execl("/bin/true", "true", (char *)NULL);
        _exit(127);
    }
    do {
        w = waitpid(pid, &st, 0);
    } while (w < 0 && errno == EINTR);
    return (w == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
}

/* Quiescent fork+alloc child: allocates through the leg's
 * allocator, verifies, frees, _exit(0); 42 on any failure.
 * Called only while no other thread is inside the
 * allocator (Z-044 Q shape). No stdio in the child. */
static int fork_alloc_one(int salt)
{
    pid_t pid;
    int st = 0;
    pid_t w;

    fflush(stdout);
    pid = fork();
    if (pid < 0) {
        return 0;
    }
    if (pid == 0) {
        static const size_t csz[5] = {64, 128, 512, 2048, 64};
        void *cp[5];
        int i, ok = 1;
        for (i = 0; i < 5; i++) {
            cp[i] = NULL;
        }
        for (i = 0; i < 5; i++) {
            unsigned char *b = (unsigned char *)P_MALLOC(csz[i]);
            if (b == NULL) {
                _exit(42);
            }
            cp[i] = b;
            b[0] = (unsigned char)(salt + i * 31);
            b[csz[i] / 2] = (unsigned char)(salt + i * 17 + 3);
            b[csz[i] - 1] = (unsigned char)(salt + i * 7 + 5);
        }
        for (i = 0; i < 5; i++) {
            unsigned char *b = (unsigned char *)cp[i];
            if (b[0] != (unsigned char)(salt + i * 31) ||
                b[csz[i] / 2] != (unsigned char)(salt + i * 17 + 3) ||
                b[csz[i] - 1] != (unsigned char)(salt + i * 7 + 5)) {
                ok = 0;
            }
        }
        for (i = 0; i < 5; i++) {
            P_FREE(cp[i]);
        }
        _exit(ok ? 0 : 42);
    }
    do {
        w = waitpid(pid, &st, 0);
    } while (w < 0 && errno == EINTR);
    return (w == pid && WIFEXITED(st) && WEXITSTATUS(st) == 0);
}

/* ================= checks + snapshots ================= */

static int g_checks_failed;

static void check(const char *name, int ok, const char *detail)
{
    printf("CHECK %s %s %s\n", name, ok ? "PASS" : "FAIL", detail);
    if (!ok) {
        g_checks_failed++;
    }
}

#ifndef USE_GLIBC
struct snap {
    struct mimir_stats st;
    size_t retained_bytes;
    size_t retained_spans;
    size_t pool_bytes;
    uint64_t exit_drains;
    uint64_t exit_blocks;
    int metadata;
    size_t retention_bound;
};

static void take_snap(const char *label, struct snap *s)
{
    uint64_t empty_spans = 0;
    uint64_t total_spans = 0;
    int ci;

    memset(s, 0, sizeof(*s));
    if (mimir_core_stats_snapshot(&s->st) != 0) {
        printf("SNAP %s ERROR stats snapshot failed\n", label);
        g_checks_failed++;
        return;
    }
    s->retained_bytes = mimir_core_retained_bytes();
    s->retained_spans = mimir_core_retained_spans();
    s->pool_bytes = mimir_core_pool_bytes();
    s->exit_drains = mimir_tls_cache_exit_drains();
    s->exit_blocks = mimir_tls_cache_exit_drain_blocks();
    s->metadata = mimir_core_metadata_check();
    s->retention_bound = mimir_tune_get_retention_bound();
    for (ci = 0; ci < MIMIR_STATS_CLASS_COUNT; ci++) {
        empty_spans += s->st.span_empty_spans[ci];
        total_spans += s->st.span_total_spans[ci];
    }
    printf("SNAP %s mapped=%llu cached=%llu released=%llu "
           "live_small=%llu live_spans=%llu live_mappings=%llu "
           "live_span_bytes=%llu live_mapping_bytes=%llu "
           "retained_bytes=%llu retained_spans=%llu "
           "pool_bytes=%llu "
           "empty_spans_sum=%llu total_spans_sum=%llu "
           "exit_drains=%llu exit_blocks=%llu metadata=%d "
           "retention_bound=%llu peak_mapped=%llu\n",
           label,
           (unsigned long long)s->st.bytes_mapped,
           (unsigned long long)s->st.bytes_cached,
           (unsigned long long)s->st.bytes_released,
           (unsigned long long)s->st.live_small_objects,
           (unsigned long long)s->st.live_spans,
           (unsigned long long)s->st.live_mappings,
           (unsigned long long)s->st.live_span_bytes,
           (unsigned long long)s->st.live_mapping_bytes,
           (unsigned long long)s->retained_bytes,
           (unsigned long long)s->retained_spans,
           (unsigned long long)s->pool_bytes,
           (unsigned long long)empty_spans,
           (unsigned long long)total_spans,
           (unsigned long long)s->exit_drains,
           (unsigned long long)s->exit_blocks, s->metadata,
           (unsigned long long)s->retention_bound,
           (unsigned long long)s->st.peak_mapped);
    {
        char d[128];
        snprintf(d, sizeof(d), "cached=%llu retained=%llu",
                 (unsigned long long)s->st.bytes_cached,
                 (unsigned long long)s->retained_bytes);
        check("snap_cached_eq_retained",
              s->st.bytes_cached == (uint64_t)s->retained_bytes, d);
        snprintf(d, sizeof(d), "empty=%llu cached=%llu",
                 (unsigned long long)(empty_spans * PAGE_BYTES),
                 (unsigned long long)s->st.bytes_cached);
        check("snap_cached_eq_empty_spans",
              (uint64_t)(empty_spans * PAGE_BYTES) ==
                  s->st.bytes_cached,
              d);
        /* Z-048 term: Q-MAPPED on the near-final core is
         * live span bytes + live mapping bytes + POOLED
         * mapping bytes (a pooled mapping is still
         * mapped; src/core/mimir_core.c Q-MAPPED comment).
         * The Z-042 two-term form predates the pool. */
        snprintf(d, sizeof(d), "mapped=%llu parts=%llu pool=%llu",
                 (unsigned long long)s->st.bytes_mapped,
                 (unsigned long long)(s->st.live_span_bytes +
                                      s->st.live_mapping_bytes +
                                      (uint64_t)s->pool_bytes),
                 (unsigned long long)s->pool_bytes);
        check("snap_mapped_eq_parts",
              s->st.bytes_mapped ==
                  s->st.live_span_bytes + s->st.live_mapping_bytes +
                      (uint64_t)s->pool_bytes,
              d);
        check("snap_metadata_clean", s->metadata == 0, "J-006 walk");
    }
}
#endif /* !USE_GLIBC */

int main(void)
{
    double cmb_wall = 0.0, stm_wall = 0.0, worst_wave = 0.0;
    double rng_wall = 0.0, frk_wall = 0.0;
    uint64_t cmb_fork_ok = 0, frk_alloc_ok = 0, frk_exec_ok = 0;
    uint64_t cross_before, cross_cmb_expect, cross_rng_expect;
    int t;

    setvbuf(stdout, NULL, _IOLBF, 0);

    for (t = 0; t < THREADS_TOTAL; t++) {
        g_args[t].tid = t;
        g_args[t].capture = 0;
        g_args[t].held[0] = NULL;
        g_args[t].held[1] = NULL;
    }

#ifndef USE_GLIBC
    struct snap s0, scal, sbase, scmb, s1, s2, s3, s4, spre, s5;
    uint64_t calib_blocks = 0;

    take_snap("S0_pristine", &s0);

    /* CAL: one thread, exact churn shape, own snapshot. */
    memset(&g_cal_arg, 0, sizeof(g_cal_arg));
    g_cal_arg.tid = 0;
    g_cal_arg.capture = 1;
    {
        pthread_t th;
        if (pthread_create(&th, NULL, churn_worker, &g_cal_arg) != 0) {
            check("calibration_create", 0, "pthread_create failed");
        } else {
            (void)pthread_join(th, NULL);
        }
    }
    take_snap("S_cal", &scal);
    calib_blocks = g_cal_arg.cap.cached_blocks;
    printf("CALIB cached_blocks=%llu cached_bytes=%llu\n",
           (unsigned long long)g_cal_arg.cap.cached_blocks,
           (unsigned long long)g_cal_arg.cap.cached_bytes);
    {
        char d[160];
        snprintf(d, sizeof(d),
                 "drains_delta=%llu blocks_delta=%llu calib_cached=%llu",
                 (unsigned long long)(scal.exit_drains - s0.exit_drains),
                 (unsigned long long)(scal.exit_blocks - s0.exit_blocks),
                 (unsigned long long)calib_blocks);
        check("calibration_exit_drain_exact",
              scal.exit_drains - s0.exit_drains == 1 &&
                  scal.exit_blocks - s0.exit_blocks == calib_blocks &&
                  calib_blocks > 0,
              d);
    }
    if (g_cal_arg.held[0] != NULL) {
        if (!verify_pat(g_cal_arg.held[0], 64, 0, SEQ_HELD0)) {
            atomic_fetch_add_explicit(&g_pattern_fail, 1,
                                      memory_order_relaxed);
        }
        mimir_free(g_cal_arg.held[0]);
    }
    if (g_cal_arg.held[1] != NULL) {
        if (!verify_pat(g_cal_arg.held[1], 512, 0, SEQ_HELD1)) {
            atomic_fetch_add_explicit(&g_pattern_fail, 1,
                                      memory_order_relaxed);
        }
        mimir_free(g_cal_arg.held[1]);
    }
    take_snap("S_base", &sbase);
#endif

    /* ============ CMB: the combined storm ============ */
    {
        pthread_t cth[CMB_CHURN_THREADS];
        pthread_t rth[CMB_RING_THREADS];
        struct ring_job rjobs[CMB_RING_THREADS];
        pthread_barrier_t bar;
        int ccreated[CMB_CHURN_THREADS];
        int rcreated[CMB_RING_THREADS];
        int i;
        double t0;
        struct timespec pause_ts;

        cross_before =
            atomic_load_explicit(&g_cross_frees, memory_order_relaxed);
        atomic_store_explicit(&g_stop, 0, memory_order_relaxed);
        atomic_store_explicit(&g_churn_iters, 0, memory_order_relaxed);
        (void)pthread_barrier_init(&bar, NULL, CMB_RING_THREADS);
        for (i = 0; i < CMB_RING_THREADS; i++) {
            rjobs[i].id = i;
            rjobs[i].nthreads = CMB_RING_THREADS;
            rjobs[i].per = CMB_RING_PER;
            rjobs[i].rounds = CMB_RING_ROUNDS;
            rjobs[i].bar = &bar;
            g_ring_all[i] = NULL;
        }
        t0 = now_ns();
        for (i = 0; i < CMB_CHURN_THREADS; i++) {
            ccreated[i] =
                (pthread_create(&cth[i], NULL, churn_looper,
                                (void *)(intptr_t)(1000 + i)) == 0);
            if (!ccreated[i]) {
                atomic_fetch_add_explicit(&g_create_fail, 1,
                                          memory_order_relaxed);
            }
        }
        for (i = 0; i < CMB_RING_THREADS; i++) {
            rcreated[i] = (pthread_create(&rth[i], NULL, ring_worker,
                                          &rjobs[i]) == 0);
            if (!rcreated[i]) {
                atomic_fetch_add_explicit(&g_create_fail, 1,
                                          memory_order_relaxed);
            }
        }
        /* Let the storms reach full speed before forking. */
        pause_ts.tv_sec = 0;
        pause_ts.tv_nsec = 20000000L;
        (void)nanosleep(&pause_ts, NULL);
        for (i = 0; i < CMB_FORKS; i++) {
            if (fork_exec_one()) {
                cmb_fork_ok++;
            }
        }
        atomic_store_explicit(&g_stop, 1, memory_order_relaxed);
        for (i = 0; i < CMB_CHURN_THREADS; i++) {
            if (ccreated[i]) {
                (void)pthread_join(cth[i], NULL);
            }
        }
        for (i = 0; i < CMB_RING_THREADS; i++) {
            if (rcreated[i]) {
                (void)pthread_join(rth[i], NULL);
            }
        }
        (void)pthread_barrier_destroy(&bar);
        cmb_wall = now_ns() - t0;
    }
    cross_cmb_expect =
        (uint64_t)CMB_RING_THREADS * CMB_RING_ROUNDS * CMB_RING_PER;
    {
        char d[192];
        uint64_t got = atomic_load_explicit(&g_cross_frees,
                                            memory_order_relaxed) -
                       cross_before;
        snprintf(d, sizeof(d), "fork_ok=%llu/%d cross=%llu expect=%llu "
                               "churn_iters=%llu",
                 (unsigned long long)cmb_fork_ok, CMB_FORKS,
                 (unsigned long long)got,
                 (unsigned long long)cross_cmb_expect,
                 (unsigned long long)atomic_load_explicit(
                     &g_churn_iters, memory_order_relaxed));
        check("combined_fork_exec_all_ok",
              cmb_fork_ok == (uint64_t)CMB_FORKS, d);
        check("combined_cross_frees_exact", got == cross_cmb_expect, d);
        check("combined_churn_progressed",
              atomic_load_explicit(&g_churn_iters,
                                   memory_order_relaxed) > 0,
              d);
    }
#ifndef USE_GLIBC
    take_snap("S_cmb", &scmb);
    {
        char d[128];
        snprintf(d, sizeof(d), "live=%llu base=%llu",
                 (unsigned long long)scmb.st.live_small_objects,
                 (unsigned long long)sbase.st.live_small_objects);
        check("combined_live_back_to_base",
              scmb.st.live_small_objects == sbase.st.live_small_objects,
              d);
    }
    printf("RETAINED_AFTER_COMBINED_BYTES %llu\n",
           (unsigned long long)scmb.retained_bytes);
#endif

    /* ============ STM: thread-churn at Z-042 scale ============ */
    atomic_store_explicit(&g_checksum, 0, memory_order_relaxed);
    stm_wall = run_waves(THREADS_TOTAL, &worst_wave);
#ifndef USE_GLIBC
    take_snap("S1_after_storm", &s1);
    {
        struct timespec ts;
        uint64_t d2, b2;
        ts.tv_sec = 0;
        ts.tv_nsec = 100000000L;
        (void)nanosleep(&ts, NULL);
        d2 = mimir_tls_cache_exit_drains();
        b2 = mimir_tls_cache_exit_drain_blocks();
        check("exit_counters_settled_after_join",
              d2 == s1.exit_drains && b2 == s1.exit_blocks,
              "counter re-read after 100 ms quiesce equals S1");
    }
    {
        char d[192];
        uint64_t expect_blocks = (uint64_t)THREADS_TOTAL * calib_blocks;
        snprintf(d, sizeof(d),
                 "drains_delta=%llu blocks_delta=%llu expect_blocks=%llu",
                 (unsigned long long)(s1.exit_drains -
                                      scmb.exit_drains),
                 (unsigned long long)(s1.exit_blocks - scmb.exit_blocks),
                 (unsigned long long)expect_blocks);
        check("storm_exit_drains_exact",
              s1.exit_drains - scmb.exit_drains ==
                      (uint64_t)THREADS_TOTAL &&
                  s1.exit_blocks - scmb.exit_blocks == expect_blocks,
              d);
        snprintf(d, sizeof(d), "live=%llu base_plus_held=%llu",
                 (unsigned long long)s1.st.live_small_objects,
                 (unsigned long long)(sbase.st.live_small_objects +
                                      2u * (unsigned)THREADS_TOTAL));
        check("storm_live_is_base_plus_held",
              s1.st.live_small_objects ==
                  sbase.st.live_small_objects +
                      (uint64_t)(2 * THREADS_TOTAL),
              d);
    }
    printf("RETAINED_AFTER_STORM_BYTES %llu\n",
           (unsigned long long)s1.retained_bytes);
    /* HELD: main verifies and frees the 20,000 held blocks. */
    {
        int i;
        uint64_t held = 0;
        for (i = 0; i < THREADS_TOTAL; i++) {
            if (g_args[i].held[0] != NULL) {
                held++;
                if (!verify_pat(g_args[i].held[0], 64, i, SEQ_HELD0)) {
                    atomic_fetch_add_explicit(&g_pattern_fail, 1,
                                              memory_order_relaxed);
                }
                mimir_free(g_args[i].held[0]);
                g_args[i].held[0] = NULL;
            }
            if (g_args[i].held[1] != NULL) {
                held++;
                if (!verify_pat(g_args[i].held[1], 512, i, SEQ_HELD1)) {
                    atomic_fetch_add_explicit(&g_pattern_fail, 1,
                                              memory_order_relaxed);
                }
                mimir_free(g_args[i].held[1]);
                g_args[i].held[1] = NULL;
            }
        }
        {
            char d[96];
            snprintf(d, sizeof(d), "held=%llu",
                     (unsigned long long)held);
            check("held_count_exact",
                  held == (uint64_t)(2 * THREADS_TOTAL), d);
        }
    }
    take_snap("S2_after_held", &s2);
    check("held_live_back_to_base",
          s2.st.live_small_objects == sbase.st.live_small_objects,
          "live_small returns to baseline after held frees");
#endif

    /* ============ RNG: free-storm at Z-043 S1 scale ============ */
    cross_before =
        atomic_load_explicit(&g_cross_frees, memory_order_relaxed);
    rng_wall = run_ring(RNG_THREADS, RNG_PER, RNG_ROUNDS);
    cross_rng_expect = (uint64_t)RNG_THREADS * RNG_ROUNDS * RNG_PER;
    {
        char d[128];
        uint64_t got = atomic_load_explicit(&g_cross_frees,
                                            memory_order_relaxed) -
                       cross_before;
        snprintf(d, sizeof(d), "cross=%llu expect=%llu",
                 (unsigned long long)got,
                 (unsigned long long)cross_rng_expect);
        check("ring_cross_frees_exact", got == cross_rng_expect, d);
    }
#ifndef USE_GLIBC
    take_snap("S3_after_ring", &s3);
    check("ring_live_back_to_base",
          s3.st.live_small_objects == sbase.st.live_small_objects,
          "live_small at baseline after ring storm");
    printf("RETAINED_AFTER_RING_BYTES %llu\n",
           (unsigned long long)s3.retained_bytes);
#endif

    /* ============ FRK: fork storm (supported shapes) ============ */
    {
        double t0 = now_ns();
        int i;
        for (i = 0; i < FRK_ALLOC_CHILDREN; i++) {
            if (fork_alloc_one(1000 + i)) {
                frk_alloc_ok++;
            }
        }
        for (i = 0; i < FRK_EXEC_CHILDREN; i++) {
            if (fork_exec_one()) {
                frk_exec_ok++;
            }
        }
        frk_wall = now_ns() - t0;
    }
    {
        char d[128];
        snprintf(d, sizeof(d), "alloc_ok=%llu/%d exec_ok=%llu/%d",
                 (unsigned long long)frk_alloc_ok, FRK_ALLOC_CHILDREN,
                 (unsigned long long)frk_exec_ok, FRK_EXEC_CHILDREN);
        check("fork_alloc_children_all_ok",
              frk_alloc_ok == (uint64_t)FRK_ALLOC_CHILDREN, d);
        check("fork_exec_children_all_ok",
              frk_exec_ok == (uint64_t)FRK_EXEC_CHILDREN, d);
    }
#ifndef USE_GLIBC
    take_snap("S4_after_fork", &s4);
    check("fork_live_unchanged",
          s4.st.live_small_objects == sbase.st.live_small_objects,
          "fork children run in their own copies; parent live set "
          "unchanged");
    printf("RETAINED_AFTER_FORK_BYTES %llu\n",
           (unsigned long long)s4.retained_bytes);

    /* ============ TRIM: byte-exact close ============ */
    mimir_tls_cache_drain_thread();
    take_snap("S_pre_trim", &spre);
    printf("RETAINED_PRE_TRIM_BYTES %llu\n",
           (unsigned long long)spre.retained_bytes);
    {
        size_t released = mimir_core_trim_retained();
        char d[160];
        snprintf(d, sizeof(d),
                 "released_spans=%llu pre_retained_spans=%llu",
                 (unsigned long long)released,
                 (unsigned long long)spre.retained_spans);
        check("trim_releases_exactly_retained",
              (uint64_t)released == (uint64_t)spre.retained_spans &&
                  (uint64_t)released * PAGE_BYTES == spre.st.bytes_cached,
              d);
        printf("TRIM released_spans=%llu released_bytes=%llu\n",
               (unsigned long long)released,
               (unsigned long long)((uint64_t)released * PAGE_BYTES));
    }
    take_snap("S5_final", &s5);
    {
        char d[160];
        snprintf(d, sizeof(d), "mapped=%llu s0_mapped=%llu retained=%llu",
                 (unsigned long long)s5.st.bytes_mapped,
                 (unsigned long long)s0.st.bytes_mapped,
                 (unsigned long long)s5.retained_bytes);
        check("trim_returns_mapped_to_s0",
              s5.st.bytes_mapped == s0.st.bytes_mapped &&
                  s5.retained_bytes == 0 && s5.st.bytes_cached == 0,
              d);
        check("final_live_at_s0",
              s5.st.live_small_objects == s0.st.live_small_objects &&
                  s5.st.live_mappings == s0.st.live_mappings,
              d);
    }
    printf("RETAINED_FINAL_BYTES %llu\n",
           (unsigned long long)s5.retained_bytes);
#endif

    {
        uint64_t af = atomic_load_explicit(&g_alloc_fail,
                                           memory_order_relaxed);
        uint64_t pf = atomic_load_explicit(&g_pattern_fail,
                                           memory_order_relaxed);
        uint64_t cf = atomic_load_explicit(&g_create_fail,
                                           memory_order_relaxed);
        check("no_alloc_failures", af == 0, "alloc_fail counter");
        check("no_pattern_failures", pf == 0, "pattern_fail counter");
        check("no_create_failures", cf == 0, "create_fail counter");
        printf("STORM combined_wall_s=%.3f churn_wall_s=%.3f "
               "worst_wave_ms=%.1f ring_wall_s=%.3f fork_wall_s=%.3f "
               "cmb_forks=%llu frk_alloc=%llu frk_exec=%llu "
               "cross_total=%llu checksum=%llu ring_checksum=%llu "
               "alloc_fail=%llu pattern_fail=%llu create_fail=%llu\n",
               cmb_wall / 1e9, stm_wall / 1e9, worst_wave / 1e6,
               rng_wall / 1e9, frk_wall / 1e9,
               (unsigned long long)cmb_fork_ok,
               (unsigned long long)frk_alloc_ok,
               (unsigned long long)frk_exec_ok,
               (unsigned long long)atomic_load_explicit(
                   &g_cross_frees, memory_order_relaxed),
               (unsigned long long)atomic_load_explicit(
                   &g_checksum, memory_order_relaxed),
               (unsigned long long)atomic_load_explicit(
                   &g_ring_checksum, memory_order_relaxed),
               (unsigned long long)af, (unsigned long long)pf,
               (unsigned long long)cf);
        printf("BATTERY RESULT: %s\n",
               (g_checks_failed == 0 && af == 0 && pf == 0 && cf == 0)
                   ? "PASS"
                   : "FAIL");
    }
    return (g_checks_failed == 0) ? 0 : 1;
}
