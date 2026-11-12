/* probe_z042.c - Z-042 thread-churn storm probe.
 *
 * Slice Z-042 (docs/MIMIRALLOC_Z13_ROADMAP.md, Wave 5):
 * "10,000 short-lived allocating threads: TLS/arena teardown
 * correctness, drain cost, and leak accounting to the byte.
 * Gate: storm green; retained-bytes-after-storm figure
 * recorded."
 *
 * No arena variant exists in this tree (Z-037/Z-038 are the
 * arena slices and are not banked here), so the teardown
 * surface under test is the Wave-4 TLS freelist cache
 * (Z-029): its C11 tss destructor must drain every churn
 * thread's bins exactly once at thread exit. The probe proves
 * that with the allocator's own accounting surfaces, to the
 * byte, at quiescent points only:
 *
 *   - J-002 stats snapshot (tests/core/mimir_stats_j002.h):
 *     bytes_mapped, bytes_cached (Q-RETAINED), live counts,
 *     per-class span tallies.
 *   - I-003 trim surface: mimir_core_retained_bytes(),
 *     mimir_core_retained_spans(), mimir_core_trim_retained().
 *   - G-007 retention bound getter (the policy that makes a
 *     nonzero retained figure legitimate, not a leak).
 *   - J-006 metadata walk: mimir_core_metadata_check() == 0.
 *   - Z-028/Z-029 TLS surface
 *     (tests/contract/mimir_tls_cache_z028.h):
 *     mimir_tls_cache_exit_drains() /
 *     mimir_tls_cache_exit_drain_blocks() process-global
 *     counters, and a per-thread calibration snapshot.
 *
 * Phases in the mimir leg (one process, sequential, every
 * snapshot taken after all churn threads of the phase have
 * been joined - quiescent points only):
 *   S0  pristine baseline (process start, nothing allocated
 *       through mimir by this process yet).
 *   CAL one calibration thread running the exact storm
 *       shape, capturing its own TLS snapshot just before
 *       exit: exit_drains must move by exactly 1 and
 *       exit_drain_blocks by exactly its cached_blocks.
 *   NUL 10,000 threads that never touch the allocator:
 *       create/join control for the drain-cost arithmetic;
 *       exit counters and stats must not move at all.
 *   DRN 256 threads that drain_thread() explicitly before
 *       exit: their exit destructors must then return
 *       exactly 0 blocks (exactly-once, no double-return).
 *   STM 10,000 churn threads in waves of 32 concurrent
 *       (= the host's hardware-thread count).
 *       Per thread: 8 blocks in each of classes 16/64/256/
 *       1024, one 2048-byte large block, plus two blocks
 *       (64 and 512) left LIVE at exit and handed to main
 *       through the thread's argument slot. Every block is
 *       pattern-touched and verified before free (Law 3).
 *       The 33 non-held blocks are freed by their owner, so
 *       each thread exits with a deterministic bin state -
 *       the calibration thread's - and the exit counters
 *       must move by exactly 10,000 drains and
 *       10,000 x calibration cached_blocks.
 *   HELD main verifies and frees the 20,000 held blocks of
 *       the dead threads (post-exit remote frees), returning
 *       live_small_objects to baseline.
 *   TRIM main drains its own (empty) TLS cache, then
 *       mimir_core_trim_retained() must release exactly the
 *       retained spans counted at the pre-trim snapshot,
 *       and bytes_mapped must return to the S0 figure to
 *       the byte: zero leak, zero unexplained retention.
 *
 * Drain cost (honest form): the tss destructor runs inside
 * pthread teardown, so no per-drain timer exists that does
 * not time thread creation too. The probe therefore reports
 * the storm wall, the null-thread wall for the same 10,000
 * create/joins, their difference as the whole
 * alloc+free+teardown cost per thread, and that difference
 * divided by the exact number of exit-drained blocks as an
 * upper-bound drain-inclusive figure. It is labelled as
 * such, never as a pure drain price.
 *
 * The glibc leg (-DUSE_GLIBC) runs the identical thread
 * shape and timing with malloc/free and no mimir surfaces:
 * a wall-time comparator only. No verdict vs glibc is
 * claimed by this slice's gate; the numbers are recorded
 * side by side for Wave 5's verdict slice (Z-045).
 *
 * Exit status: 0 iff every printed CHECK passes. Any failed
 * invariant makes the storm RED, honestly.
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

#ifndef USE_GLIBC
#include "mimir_alloc.h"
#include "../../tests/core/mimir_stats_j002.h"
#include "../../tests/contract/mimir_tls_cache_z028.h"
#define P_MALLOC(sz) mimir_malloc(sz)
#define P_FREE(p) mimir_free(p)
#else
#define P_MALLOC(sz) malloc(sz)
#define P_FREE(p) free(p)
#endif

#define THREADS_TOTAL 10000
#define WAVE 32
#define DRN_THREADS 256
#define NCHURN_CLASS 4
#define PER_CLASS 8
#define LARGE_SIZE 2048u
/* Block sequence numbers inside one thread (pattern seeds):
 * 0..31 churn classes in order, 32 large, 33 held-64,
 * 34 held-512. */
#define SEQ_LARGE 32
#define SEQ_HELD0 33
#define SEQ_HELD1 34
#define CLASS_IDX_64 3
#define CLASS_IDX_512 15
#define PAGE_BYTES 4096u

static const size_t churn_classes[NCHURN_CLASS] = {16, 64, 256, 1024};

static _Atomic uint64_t g_alloc_fail;
static _Atomic uint64_t g_pattern_fail;
static _Atomic uint64_t g_create_fail;
static _Atomic uint64_t g_checksum;

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

struct worker_arg {
    int tid;
    int capture; /* mimir leg: snapshot own TLS before exit */
    int predrain; /* mimir leg: drain_thread() before exit */
    void *held[2];
#ifndef USE_GLIBC
    struct mimir_tls_cache_snapshot cap;
#endif
};

static struct worker_arg g_args[THREADS_TOTAL];
static struct worker_arg g_cal_arg __attribute__((unused));
static struct worker_arg g_drn_args[DRN_THREADS] __attribute__((unused));

static void *churn_worker(void *p)
{
    struct worker_arg *arg = (struct worker_arg *)p;
    /* Fixed slots indexed by sequence number, never
     * compressed: a failed allocation leaves its slot NULL
     * and cannot shift the pattern seeds of later blocks. */
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
                atomic_fetch_add_explicit(&g_alloc_fail, 1, memory_order_relaxed);
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
            atomic_fetch_add_explicit(&g_alloc_fail, 1, memory_order_relaxed);
        } else {
            touch(b, LARGE_SIZE, arg->tid, SEQ_LARGE);
            ptrs[SEQ_LARGE] = b;
            sizes[SEQ_LARGE] = LARGE_SIZE;
        }
    }
    /* Held blocks: allocated last, never freed by this
     * thread; ownership passes to main via the arg slot. */
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

    /* Verify every block this thread owns, then free all
     * but the held pair (owner frees, into this thread's
     * own TLS bins for the small classes). Verified blocks
     * fold into the checksum: the sum of each block's first
     * and last pattern bytes, a deterministic function of
     * (tid, seq, size) only, so the total must be identical
     * across legs and reps (K-002 workload equivalence). */
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
        atomic_fetch_add_explicit(&g_checksum, lsum,
                                  memory_order_relaxed);
    }
    for (seq = 0; seq < 33; seq++) {
        if (ptrs[seq] != NULL) {
            P_FREE(ptrs[seq]);
        }
    }
#ifndef USE_GLIBC
    if (arg->predrain) {
        /* DRN phase: return every binned block explicitly,
         * so the thread-exit destructor that follows must
         * find an empty cache and return exactly 0 blocks -
         * the exactly-once proof against double-return. */
        mimir_tls_cache_drain_thread();
    }
    if (arg->capture) {
        if (mimir_tls_cache_snapshot(&arg->cap) != 0) {
            atomic_fetch_add_explicit(&g_pattern_fail, 1, memory_order_relaxed);
        }
    }
#endif
    return NULL;
}

static void *null_worker(void *p)
{
    (void)p;
    return NULL;
}

/* Run COUNT threads of fn in waves of WAVE; returns wall ns
 * for the whole phase and the worst single wave ns. */
static double run_waves(void *(*fn)(void *), struct worker_arg *args,
                        int count, int use_args, double *worst_wave_ns)
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
                void *a = use_args ? (void *)&args[base + i] : NULL;
                created[i] =
                    (pthread_create(&th[i], NULL, fn, a) == 0);
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
           (unsigned long long)empty_spans,
           (unsigned long long)total_spans,
           (unsigned long long)s->exit_drains,
           (unsigned long long)s->exit_blocks, s->metadata,
           (unsigned long long)s->retention_bound,
           (unsigned long long)s->st.peak_mapped);
    /* Cross-surface identities that must hold at EVERY
     * quiescent snapshot (leak-accounting review, Z-042):
     * cached == retained_bytes (two surfaces, one quantity);
     * retained_bytes == empty-span sum x page (byte-exact);
     * mapped == live span bytes + live mapping bytes. */
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
        snprintf(d, sizeof(d), "mapped=%llu parts=%llu",
                 (unsigned long long)s->st.bytes_mapped,
                 (unsigned long long)(s->st.live_span_bytes +
                                      s->st.live_mapping_bytes));
        check("snap_mapped_eq_parts",
              s->st.bytes_mapped ==
                  s->st.live_span_bytes + s->st.live_mapping_bytes,
              d);
        check("snap_metadata_clean", s->metadata == 0, "J-006 walk");
    }
}
#endif /* !USE_GLIBC */

int main(void)
{
    double null_wall, storm_wall, worst_wave = 0.0;
    int t;

    /* Line-buffered even when redirected to a file, so a
     * mid-storm crash can never hide the phase output that
     * was already produced (probe-audit point, Z-042). */
    setvbuf(stdout, NULL, _IOLBF, 0);

    for (t = 0; t < THREADS_TOTAL; t++) {
        g_args[t].tid = t;
        g_args[t].capture = 0;
        g_args[t].held[0] = NULL;
        g_args[t].held[1] = NULL;
    }

#ifndef USE_GLIBC
    struct snap s0, scal, sbase, sdrn, snull, s1, s2, s3, s4;
    uint64_t calib_blocks = 0;

    take_snap("S0_pristine", &s0);

    /* CAL: one thread, exact storm shape, snapshot captured
     * inside the thread just before it returns. */
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
    printf("CALIB cached_blocks=%llu cached_bytes=%llu hits=%llu "
           "misses=%llu refills=%llu drains=%llu "
           "drain_blocks=%llu local_frees=%llu\n",
           (unsigned long long)g_cal_arg.cap.cached_blocks,
           (unsigned long long)g_cal_arg.cap.cached_bytes,
           (unsigned long long)g_cal_arg.cap.hits,
           (unsigned long long)g_cal_arg.cap.misses,
           (unsigned long long)g_cal_arg.cap.refills,
           (unsigned long long)g_cal_arg.cap.drains,
           (unsigned long long)g_cal_arg.cap.drain_blocks,
           (unsigned long long)g_cal_arg.cap.local_frees);
    {
        char d[160];
        snprintf(d, sizeof(d), "drains_delta=%llu blocks_delta=%llu "
                              "calib_cached=%llu",
                 (unsigned long long)(scal.exit_drains - s0.exit_drains),
                 (unsigned long long)(scal.exit_blocks - s0.exit_blocks),
                 (unsigned long long)calib_blocks);
        check("calibration_exit_drain_exact",
              scal.exit_drains - s0.exit_drains == 1 &&
                  scal.exit_blocks - s0.exit_blocks == calib_blocks &&
                  calib_blocks > 0,
              d);
    }
    /* The calibration thread's held blocks are main's now. */
    if (g_cal_arg.held[0] != NULL) {
        if (!verify_pat(g_cal_arg.held[0], 64, 0, SEQ_HELD0)) {
            atomic_fetch_add_explicit(&g_pattern_fail, 1, memory_order_relaxed);
        }
        mimir_free(g_cal_arg.held[0]);
    }
    if (g_cal_arg.held[1] != NULL) {
        if (!verify_pat(g_cal_arg.held[1], 512, 0, SEQ_HELD1)) {
            atomic_fetch_add_explicit(&g_pattern_fail, 1, memory_order_relaxed);
        }
        mimir_free(g_cal_arg.held[1]);
    }
    take_snap("S_base", &sbase);

    /* DRN: 256 threads running the same shape but calling
     * mimir_tls_cache_drain_thread() before returning. Each
     * post-drain snapshot must show 0 cached blocks, the
     * exit counters must move by exactly +256 drains and
     * +0 blocks: an explicit drain and the exit destructor
     * together return every block exactly once. */
    {
        int i;
        uint64_t cap_sum = 0;
        for (i = 0; i < DRN_THREADS; i++) {
            memset(&g_drn_args[i], 0, sizeof(g_drn_args[i]));
            g_drn_args[i].tid = i;
            g_drn_args[i].capture = 1;
            g_drn_args[i].predrain = 1;
        }
        (void)run_waves(churn_worker, g_drn_args, DRN_THREADS, 1, NULL);
        for (i = 0; i < DRN_THREADS; i++) {
            cap_sum += g_drn_args[i].cap.cached_blocks;
            if (g_drn_args[i].held[0] != NULL) {
                if (!verify_pat(g_drn_args[i].held[0], 64, i, SEQ_HELD0)) {
                    atomic_fetch_add_explicit(&g_pattern_fail, 1,
                                              memory_order_relaxed);
                }
                mimir_free(g_drn_args[i].held[0]);
            }
            if (g_drn_args[i].held[1] != NULL) {
                if (!verify_pat(g_drn_args[i].held[1], 512, i, SEQ_HELD1)) {
                    atomic_fetch_add_explicit(&g_pattern_fail, 1,
                                              memory_order_relaxed);
                }
                mimir_free(g_drn_args[i].held[1]);
            }
        }
        take_snap("S_drn", &sdrn);
        {
            char d[160];
            snprintf(d, sizeof(d),
                     "drains_delta=%llu blocks_delta=%llu cap_sum=%llu",
                     (unsigned long long)(sdrn.exit_drains -
                                          sbase.exit_drains),
                     (unsigned long long)(sdrn.exit_blocks -
                                          sbase.exit_blocks),
                     (unsigned long long)cap_sum);
            check("predrain_exit_returns_zero_blocks",
                  sdrn.exit_drains - sbase.exit_drains ==
                          (uint64_t)DRN_THREADS &&
                      sdrn.exit_blocks == sbase.exit_blocks &&
                      cap_sum == 0,
                  d);
            check("predrain_live_back_to_base",
                  sdrn.st.live_small_objects ==
                      sbase.st.live_small_objects,
                  "held blocks of DRN threads freed by main");
        }
    }
#endif

    /* NUL: 10,000 threads that never touch the allocator. */
    null_wall = run_waves(null_worker, g_args, THREADS_TOTAL, 0, NULL);

#ifndef USE_GLIBC
    take_snap("S_null", &snull);
    {
        char d[160];
        snprintf(d, sizeof(d), "drains_delta=%llu blocks_delta=%llu",
                 (unsigned long long)(snull.exit_drains -
                                      sdrn.exit_drains),
                 (unsigned long long)(snull.exit_blocks -
                                      sdrn.exit_blocks));
        check("null_threads_no_drain",
              snull.exit_drains == sdrn.exit_drains &&
                  snull.exit_blocks == sdrn.exit_blocks,
              d);
        check("null_threads_no_state_change",
              snull.st.bytes_mapped == sdrn.st.bytes_mapped &&
                  snull.st.live_small_objects ==
                      sdrn.st.live_small_objects,
              "mapped/live unchanged by allocator-free threads");
    }
#endif

    /* STM: the storm itself. The checksum is reset here so
     * the STORM line carries the storm-only total in both
     * legs: identical across legs and reps proves the two
     * legs did the same work (K-002 discipline). */
    atomic_store_explicit(&g_checksum, 0, memory_order_relaxed);
    storm_wall = run_waves(churn_worker, g_args, THREADS_TOTAL, 1,
                           &worst_wave);

#ifndef USE_GLIBC
    take_snap("S1_after_storm", &s1);
    {
        /* Settle proof: pthread_join orders the churn
         * threads' tss destructors before the joins return
         * on this host's C library; prove it by re-reading
         * the exit counters after a 100 ms quiesce - they
         * must be identical to the S1 values, i.e. no drain
         * was still in flight when S1 was taken. */
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
        uint64_t expect_blocks =
            (uint64_t)THREADS_TOTAL * calib_blocks;
        snprintf(d, sizeof(d),
                 "drains_delta=%llu blocks_delta=%llu expect_blocks=%llu",
                 (unsigned long long)(s1.exit_drains -
                                      snull.exit_drains),
                 (unsigned long long)(s1.exit_blocks -
                                      snull.exit_blocks),
                 (unsigned long long)expect_blocks);
        check("storm_exit_drains_exact",
              s1.exit_drains - snull.exit_drains ==
                      (uint64_t)THREADS_TOTAL &&
                  s1.exit_blocks - snull.exit_blocks == expect_blocks,
              d);
        snprintf(d, sizeof(d), "live_small=%llu base=%llu expect=+20000",
                 (unsigned long long)s1.st.live_small_objects,
                 (unsigned long long)snull.st.live_small_objects);
        check("storm_live_is_held_only",
              s1.st.live_small_objects ==
                  snull.st.live_small_objects + 20000,
              d);
        snprintf(d, sizeof(d), "live64=%llu live512=%llu",
                 (unsigned long long)s1.st.span_live_objects[CLASS_IDX_64],
                 (unsigned long long)
                     s1.st.span_live_objects[CLASS_IDX_512]);
        check("storm_held_per_class",
              s1.st.span_live_objects[CLASS_IDX_64] ==
                      snull.st.span_live_objects[CLASS_IDX_64] + 10000 &&
                  s1.st.span_live_objects[CLASS_IDX_512] ==
                      snull.st.span_live_objects[CLASS_IDX_512] + 10000,
              d);
        snprintf(d, sizeof(d),
                 "retained=%llu bound_cap=%llu",
                 (unsigned long long)s1.st.bytes_cached,
                 (unsigned long long)(20ull *
                                      (uint64_t)s1.retention_bound *
                                      PAGE_BYTES));
        check("storm_retained_within_policy",
              s1.st.bytes_cached <=
                  20ull * (uint64_t)s1.retention_bound * PAGE_BYTES,
              d);
    }
    printf("RETAINED_AFTER_STORM_BYTES %llu\n",
           (unsigned long long)s1.st.bytes_cached);
#endif

    /* HELD: main verifies and frees every held block of the
     * dead churn threads (post-exit remote frees). */
    {
        uint64_t held_seen = 0;
        for (t = 0; t < THREADS_TOTAL; t++) {
            if (g_args[t].held[0] != NULL) {
                held_seen++;
                if (!verify_pat(g_args[t].held[0], 64, t, SEQ_HELD0)) {
                    atomic_fetch_add_explicit(&g_pattern_fail, 1,
                                              memory_order_relaxed);
                }
                P_FREE(g_args[t].held[0]);
                g_args[t].held[0] = NULL;
            }
            if (g_args[t].held[1] != NULL) {
                held_seen++;
                if (!verify_pat(g_args[t].held[1], 512, t, SEQ_HELD1)) {
                    atomic_fetch_add_explicit(&g_pattern_fail, 1,
                                              memory_order_relaxed);
                }
                P_FREE(g_args[t].held[1]);
                g_args[t].held[1] = NULL;
            }
        }
        printf("HELD blocks_verified_and_freed=%llu\n",
               (unsigned long long)held_seen);
#ifndef USE_GLIBC
        {
            char d[96];
            snprintf(d, sizeof(d), "held_seen=%llu",
                     (unsigned long long)held_seen);
            check("held_count_exact", held_seen == 20000, d);
        }
#endif
    }

#ifndef USE_GLIBC
    take_snap("S2_after_held_free", &s2);
    {
        char d[160];
        snprintf(d, sizeof(d), "live_small=%llu base=%llu",
                 (unsigned long long)s2.st.live_small_objects,
                 (unsigned long long)sbase.st.live_small_objects);
        check("held_free_returns_live_to_base",
              s2.st.live_small_objects == sbase.st.live_small_objects,
              d);
        check("no_exit_drains_during_main_free",
              s2.exit_drains == s1.exit_drains &&
                  s2.exit_blocks == s1.exit_blocks,
              "exit counters frozen while main frees");
    }
    /* Main's own TLS cache must be empty (its frees were all
     * remote, routed to the core); drain it for the record. */
    {
        struct mimir_tls_cache_snapshot msnap;
        if (mimir_tls_cache_snapshot(&msnap) == 0) {
            char d[96];
            snprintf(d, sizeof(d), "main_cached_blocks=%llu",
                     (unsigned long long)msnap.cached_blocks);
            check("main_cache_empty", msnap.cached_blocks == 0, d);
        } else {
            check("main_cache_empty", 0, "snapshot failed");
        }
        mimir_tls_cache_drain_thread();
    }
    take_snap("S3_pre_trim", &s3);
    {
        size_t released = mimir_core_trim_retained();
        char d[160];
        snprintf(d, sizeof(d), "released_spans=%llu pre_retained_spans=%llu",
                 (unsigned long long)released,
                 (unsigned long long)s3.retained_spans);
        check("trim_releases_exactly_retained",
              (uint64_t)released == (uint64_t)s3.retained_spans &&
                  (uint64_t)released * PAGE_BYTES == s3.st.bytes_cached,
              d);
        printf("TRIM released_spans=%llu released_bytes=%llu\n",
               (unsigned long long) released,
               (unsigned long long)((uint64_t) released * PAGE_BYTES));
    }
    take_snap("S4_final", &s4);
    {
        char d[192];
        snprintf(d, sizeof(d),
                 "final_mapped=%llu pristine_mapped=%llu "
                 "final_live_spans=%llu pristine_live_spans=%llu",
                 (unsigned long long)s4.st.bytes_mapped,
                 (unsigned long long)s0.st.bytes_mapped,
                 (unsigned long long)s4.st.live_spans,
                 (unsigned long long)s0.st.live_spans);
        check("final_mapped_eq_pristine_to_the_byte",
              s4.st.bytes_mapped == s0.st.bytes_mapped &&
                  s4.st.live_spans == s0.st.live_spans &&
                  s4.st.live_mappings == s0.st.live_mappings &&
                  s4.retained_bytes == 0 && s4.st.bytes_cached == 0,
              d);
    }
#endif

    {
        uint64_t af = atomic_load_explicit(&g_alloc_fail, memory_order_relaxed);
        uint64_t pf = atomic_load_explicit(&g_pattern_fail, memory_order_relaxed);
        uint64_t cf = atomic_load_explicit(&g_create_fail, memory_order_relaxed);
        char d[128];
        snprintf(d, sizeof(d), "alloc=%llu pattern=%llu create=%llu",
                 (unsigned long long)af, (unsigned long long)pf,
                 (unsigned long long)cf);
        check("zero_failures", af == 0 && pf == 0 && cf == 0, d);
        printf("STORM threads=%d waves_of=%d null_wall_s=%.3f "
               "storm_wall_s=%.3f worst_wave_ms=%.1f "
               "alloc_teardown_ns_per_thread=%.0f checksum=%llu\n",
               THREADS_TOTAL, WAVE, null_wall / 1e9, storm_wall / 1e9,
               worst_wave / 1e6,
               (storm_wall - null_wall) / (double)THREADS_TOTAL,
               (unsigned long long)atomic_load_explicit(
                   &g_checksum, memory_order_relaxed));
#ifndef USE_GLIBC
        {
            uint64_t eb = s1.exit_blocks - snull.exit_blocks;
            if (eb > 0) {
                printf("DRAIN exit_blocks=%llu "
                       "drain_inclusive_ns_per_exit_block=%.0f "
                       "(upper bound: (storm-null) wall over "
                       "exit-drained blocks; includes alloc/free "
                       "work, labelled not pure drain)\n",
                       (unsigned long long)eb,
                       (storm_wall - null_wall) / (double)eb);
            }
        }
#endif
    }

    printf("STORM RESULT: %s (%d checks failed)\n",
           g_checks_failed == 0 ? "PASS" : "FAIL", g_checks_failed);
    return g_checks_failed == 0 ? 0 : 1;
}
