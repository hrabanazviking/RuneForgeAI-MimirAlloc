/*
 * probe_z043.c -- Z-043 perf probe: the producer/consumer
 * free storm priced, and worst-case remote-free latency
 * measured under flood.
 *
 * One phase per process (Z-004 protocol, as Z-024/Z-030):
 * the runner discards 3 warm-up processes and medians 7
 * measured ones. Threads pin 1:1 to logical CPUs 0..31
 * (CCD0 0-7, CCD1 8-15, SMT siblings 16-31).
 *
 * LIVE SET (stated, per the gate's condition): every leg
 * works on 64-byte blocks; the ring/pc legs hold 131,072
 * blocks live in flight per round (32 x 4,096); the queue
 * legs offer 63,488 blocks (31 x 2,048) against one
 * 256-slot queue. Free cost on this core is a function of
 * live-set size (Z-024 anatomy point 1); these numbers do
 * not transfer to other live sets.
 *
 * LATENCY METHOD (stated, not hidden): latency legs time
 * EVERY free/push individually with
 * clock_gettime(CLOCK_MONOTONIC) brackets on the freeing /
 * pushing thread, into per-thread sample arrays merged
 * after the flood. The bracket floor (an empty
 * back-to-back clock_gettime pair, measured in-process at
 * phase start) is printed in the RESULT line as
 * timer_floor_ns; raw latencies are reported, nothing is
 * subtracted. Under flood the frees price in microseconds
 * (lock convoy + registry walk), two orders above the
 * ~25 ns bracket, so the perturbation is small; for the
 * enqueue leg, whose uncontended cost is ~100 ns, the
 * floor is named beside the number instead. The recorded
 * max is the OBSERVED max over the sampled (here: every)
 * frees of the measured processes on a shared host - a
 * lower bound on the true worst case, never an upper
 * bound or a proof.
 *
 * Phases:
 *   local32_wall    - control: 32 threads each free their
 *                     OWN batch concurrently (no crossing);
 *                     wall ns/op. Prices the storm's
 *                     concurrency without cross-free.
 *   ring32_wall     - 32 threads ring cross-free (each
 *                     frees the next thread's batch) via
 *                     production mimir_free; wall ns/op.
 *   ring32_lat      - same geometry, every free timed:
 *                     ns_per_op = median free latency;
 *                     max_ns / p99_ns = worst / p99 free
 *                     latency. THE gate's worst-case leg
 *                     for the production path.
 *   pc16x16_lat     - 16 producers stream batches through
 *                     SPSC rings to 16 consumers who free
 *                     via mimir_free, every free timed.
 *   queue31_enq     - 31 producers push into one owner
 *                     queue (owner drains + frees
 *                     concurrently); every PUSH timed
 *                     (freer-side latency, Z-030's *_enq
 *                     definition); wall ns/op over offered
 *                     counts the whole system incl. bypass
 *                     direct frees and owner drain.
 *   queue31_total   - same flood; per-block end-to-end
 *                     latency (push timestamp -> owner
 *                     completes the real mimir_free),
 *                     tracked by the owner via a side table
 *                     indexed by (producer, seq). max_ns is
 *                     the worst end-to-end remote-free
 *                     latency through the queue.
 *
 * Output: one RESULT line per process:
 *   RESULT phase=<name> ns_per_op=<f> ops=<n>
 *          checksum=<n> max_ns=<n> p99_ns=<n>
 *          timer_floor_ns=<n> [bypassed=<n>]
 * Patterns are written at allocation and verified by the
 * freeing side BEFORE the timed free (K-002 discipline);
 * the checksum covers exactly the offered blocks (for the
 * queue phases: owner-drained checks plus producer bypass
 * checks, so the split cannot move it), so it is
 * identical across processes of a phase.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE 1

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

#include "mimir_alloc.h"
#include "mimir_remote_queue_z030.h"

#ifdef LEG_GLIBC
/* Comparator build (Z-024/Z-030 precedent): the same probe
 * source with production calls mapped to glibc. Only the
 * ring32_lat / pc16x16_lat phases are run in this build;
 * the queue legs require MimirAlloc blocks (the owner
 * performs mimir_free) and are never run under this leg. */
#undef mimir_malloc
#undef mimir_free
#define mimir_malloc malloc
#define mimir_free free
#endif

#define NTHREADS 32
#define RING_PER 4096
#define SIZE 64

static void pin_cpu(int cpu)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
}

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static uint64_t timer_floor_ns(void)
{
    uint64_t best = UINT64_MAX;
    for (int i = 0; i < 200; i++) {
        uint64_t a = now_ns();
        uint64_t b = now_ns();
        if (b - a < best) {
            best = b - a;
        }
    }
    return best;
}

static void block_fill(void *p, uint32_t producer, uint32_t seq)
{
    uint64_t tag = ((uint64_t)producer << 32) | seq;
    memcpy(p, &tag, 8);
    unsigned char mid = (unsigned char)(tag & 0x5au);
    memset((unsigned char *)p + 8, mid, SIZE - 9);
    ((unsigned char *)p)[SIZE - 1] = (unsigned char)(mid ^ 0xffu);
}

static uint64_t block_check(const void *p, uint32_t producer,
                            uint32_t seq)
{
    uint64_t tag;
    memcpy(&tag, p, 8);
    if (tag != (((uint64_t)producer << 32) | seq)) {
        return UINT64_MAX;
    }
    const unsigned char *b = (const unsigned char *)p;
    return (uint64_t)b[0] + (uint64_t)b[SIZE - 1];
}

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

/* p99 + max from a merged sample array (sorted in place). */
static void lat_stats(uint64_t *samples, size_t n, uint64_t *p99,
                      uint64_t *max_out)
{
    qsort(samples, n, sizeof(uint64_t), cmp_u64);
    *max_out = samples[n - 1];
    size_t idx = (size_t)((n * 99ull) / 100ull);
    if (idx >= n) {
        idx = n - 1;
    }
    *p99 = samples[idx];
}

/* ---------------- ring geometries ------------------------- */

struct ring_job {
    int id;
    int timed;              /* 0 = wall, 1 = every free timed */
    int victim_id;          /* whose batch this thread frees */
    void **mine;
    void ***all;
    pthread_barrier_t *bar;
    uint64_t *samples;      /* timed mode: RING_PER entries */
    uint64_t checksum;
    uint64_t errors;
};

static void *ring_main(void *arg)
{
    struct ring_job *job = (struct ring_job *)arg;
    pin_cpu(job->id % 32);
    for (size_t i = 0; i < RING_PER; i++) {
        void *p = mimir_malloc(SIZE);
        if (p == NULL) {
            job->errors++;
            job->mine[i] = NULL;
            continue;
        }
        block_fill(p, (uint32_t)job->id, (uint32_t)i);
        job->mine[i] = p;
    }
    pthread_barrier_wait(job->bar);
    int victim = job->victim_id;
    void **batch = job->all[victim];
    uint64_t sum = 0;
    for (size_t i = 0; i < RING_PER; i++) {
        void *p = batch[i];
        if (p == NULL) {
            continue;
        }
        uint64_t c = block_check(p, (uint32_t)victim, (uint32_t)i);
        if (c == UINT64_MAX) {
            job->errors++;
            c = 0;
        }
        sum += c;
        if (job->timed) {
            uint64_t a = now_ns();
            mimir_free(p);
            uint64_t b = now_ns();
            job->samples[i] = b - a;
        } else {
            mimir_free(p);
        }
    }
    job->checksum = sum;
    return NULL;
}

/* mode: 0 = local wall (victim == self), 1 = ring wall,
 *       2 = ring timed */
static int run_ring(int mode, const char *phase)
{
    static void *storage[NTHREADS][RING_PER];
    static void **all[NTHREADS];
    static struct ring_job jobs[NTHREADS];
    static pthread_t th[NTHREADS];
    static uint64_t samples[NTHREADS][RING_PER];
    pthread_barrier_t bar;
    pthread_barrier_init(&bar, NULL, NTHREADS);
    uint64_t floor_ns = timer_floor_ns();
    for (int t = 0; t < NTHREADS; t++) {
        all[t] = storage[t];
        memset(&jobs[t], 0, sizeof(jobs[t]));
        jobs[t].id = t;
        jobs[t].timed = (mode == 2);
        jobs[t].victim_id = (mode == 0) ? t : (t + 1) % NTHREADS;
        jobs[t].mine = storage[t];
        jobs[t].all = all;
        jobs[t].bar = &bar;
        jobs[t].samples = samples[t];
    }
    uint64_t wall0 = now_ns();
    for (int t = 0; t < NTHREADS; t++) {
        pthread_create(&th[t], NULL, ring_main, &jobs[t]);
    }
    uint64_t checksum = 0, errors = 0;
    for (int t = 0; t < NTHREADS; t++) {
        pthread_join(th[t], NULL);
        checksum += jobs[t].checksum;
        errors += jobs[t].errors;
    }
    uint64_t wall1 = now_ns();
    pthread_barrier_destroy(&bar);
    if (errors != 0) {
        fprintf(stderr, "%s: %llu errors\n", phase,
                (unsigned long long)errors);
        return 1;
    }
    uint64_t ops = (uint64_t)NTHREADS * RING_PER;
    if (mode == 2) {
        static uint64_t merged[NTHREADS * RING_PER];
        memcpy(merged, samples, sizeof(merged));
        uint64_t p99, max_ns;
        lat_stats(merged, ops, &p99, &max_ns);
        uint64_t median = merged[ops / 2];
        printf("RESULT phase=%s ns_per_op=%llu ops=%llu "
               "checksum=%llu max_ns=%llu p99_ns=%llu "
               "timer_floor_ns=%llu\n",
               phase, (unsigned long long)median,
               (unsigned long long)ops,
               (unsigned long long)checksum,
               (unsigned long long)max_ns,
               (unsigned long long)p99,
               (unsigned long long)floor_ns);
    } else {
        double ns_per_op = (double)(wall1 - wall0) / (double)ops;
        printf("RESULT phase=%s ns_per_op=%.1f ops=%llu "
               "checksum=%llu max_ns=0 p99_ns=0 "
               "timer_floor_ns=%llu\n",
               phase, ns_per_op, (unsigned long long)ops,
               (unsigned long long)checksum,
               (unsigned long long)floor_ns);
    }
    return 0;
}

/* ---------------- pc16x16 latency ------------------------- */

#define PC_PRODUCERS 16
#define PC_PER 4096
#define PC_RING 1024

struct pc_ring {
    _Atomic uint64_t head;
    _Atomic uint64_t tail;
    void *slots[PC_RING];
};

struct pc_prod {
    int id;
    struct pc_ring *ring;
    uint64_t errors;
};

struct pc_cons {
    int id;
    struct pc_ring *ring;
    _Atomic int *done;
    uint64_t *samples;
    uint64_t checksum;
    uint64_t errors;
    uint64_t freed;
};

static void *pc_prod_main(void *arg)
{
    struct pc_prod *job = (struct pc_prod *)arg;
    pin_cpu(job->id % 32);
    for (uint32_t i = 0; i < PC_PER; i++) {
        void *p = mimir_malloc(SIZE);
        if (p == NULL) {
            job->errors++;
            continue;
        }
        block_fill(p, (uint32_t)job->id, i);
        uint64_t tail = atomic_load_explicit(&job->ring->tail,
                                             memory_order_relaxed);
        while (tail - atomic_load_explicit(&job->ring->head,
                                           memory_order_acquire) >=
               PC_RING) {
            sched_yield();
        }
        job->ring->slots[tail % PC_RING] = p;
        atomic_store_explicit(&job->ring->tail, tail + 1,
                              memory_order_release);
    }
    return NULL;
}

static void *pc_cons_main(void *arg)
{
    struct pc_cons *job = (struct pc_cons *)arg;
    pin_cpu((16 + job->id) % 32);
    uint64_t head = 0, sum = 0;
    for (;;) {
        uint64_t tail = atomic_load_explicit(&job->ring->tail,
                                             memory_order_acquire);
        if (head < tail) {
            void *p = job->ring->slots[head % PC_RING];
            uint64_t c = block_check(p, (uint32_t)job->id,
                                     (uint32_t)head);
            if (c == UINT64_MAX) {
                job->errors++;
                c = 0;
            }
            sum += c;
            uint64_t a = now_ns();
            mimir_free(p);
            uint64_t b = now_ns();
            job->samples[head] = b - a;
            job->freed++;
            head++;
            atomic_store_explicit(&job->ring->head, head,
                                  memory_order_release);
        } else if (atomic_load_explicit(job->done,
                                        memory_order_acquire)) {
            uint64_t tail2 = atomic_load_explicit(
                &job->ring->tail, memory_order_acquire);
            if (head == tail2) {
                break;
            }
        } else {
            sched_yield();
        }
    }
    job->checksum = sum;
    return NULL;
}

static int run_pc(const char *phase)
{
    static struct pc_ring rings[PC_PRODUCERS];
    static struct pc_prod prods[PC_PRODUCERS];
    static struct pc_cons conss[PC_PRODUCERS];
    static _Atomic int done[PC_PRODUCERS];
    static pthread_t pth[PC_PRODUCERS], cth[PC_PRODUCERS];
    static uint64_t samples[PC_PRODUCERS][PC_PER];
    uint64_t floor_ns = timer_floor_ns();
    for (int t = 0; t < PC_PRODUCERS; t++) {
        memset(&rings[t], 0, sizeof(rings[t]));
        atomic_init(&rings[t].head, 0);
        atomic_init(&rings[t].tail, 0);
        atomic_init(&done[t], 0);
        memset(&prods[t], 0, sizeof(prods[t]));
        prods[t].id = t;
        prods[t].ring = &rings[t];
        memset(&conss[t], 0, sizeof(conss[t]));
        conss[t].id = t;
        conss[t].ring = &rings[t];
        conss[t].done = &done[t];
        conss[t].samples = samples[t];
        pthread_create(&cth[t], NULL, pc_cons_main, &conss[t]);
        pthread_create(&pth[t], NULL, pc_prod_main, &prods[t]);
    }
    uint64_t checksum = 0, errors = 0, freed = 0;
    for (int t = 0; t < PC_PRODUCERS; t++) {
        pthread_join(pth[t], NULL);
        atomic_store_explicit(&done[t], 1, memory_order_release);
        pthread_join(cth[t], NULL);
        checksum += conss[t].checksum;
        errors += prods[t].errors + conss[t].errors;
        freed += conss[t].freed;
    }
    if (errors != 0 || freed != (uint64_t)PC_PRODUCERS * PC_PER) {
        fprintf(stderr, "%s: errors=%llu freed=%llu\n", phase,
                (unsigned long long)errors,
                (unsigned long long)freed);
        return 1;
    }
    static uint64_t merged[PC_PRODUCERS * PC_PER];
    memcpy(merged, samples, sizeof(merged));
    uint64_t p99, max_ns;
    lat_stats(merged, freed, &p99, &max_ns);
    printf("RESULT phase=%s ns_per_op=%llu ops=%llu "
           "checksum=%llu max_ns=%llu p99_ns=%llu "
           "timer_floor_ns=%llu\n",
           phase, (unsigned long long)merged[freed / 2],
           (unsigned long long)freed,
           (unsigned long long)checksum,
           (unsigned long long)max_ns,
           (unsigned long long)p99,
           (unsigned long long)floor_ns);
    return 0;
}

/* ---------------- queue31 legs ---------------------------- */

#define Q_PRODUCERS 31
#define Q_PER 2048

struct q_prod {
    int id;
    mrq_t *q;
    int total_mode;
    uint64_t *push_samples;  /* enq mode: Q_PER entries */
    uint64_t *push_stamps;   /* total mode: side table */
    uint64_t ok;
    uint64_t bypassed;
    uint64_t bypass_sum;
    uint64_t errors;
};

static void *q_prod_main(void *arg)
{
    struct q_prod *job = (struct q_prod *)arg;
    pin_cpu((job->id + 1) % 32);
    for (uint32_t i = 0; i < Q_PER; i++) {
        void *p = mimir_malloc(SIZE);
        if (p == NULL) {
            job->errors++;
            continue;
        }
        block_fill(p, (uint32_t)job->id, i);
        if (job->total_mode) {
            job->push_stamps[i] = now_ns();
            if (mrq_push(job->q, p) == MRQ_PUSH_OK) {
                job->ok++;
            } else {
                job->push_stamps[i] = 0;
                job->bypassed++;
                job->bypass_sum +=
                    block_check(p, (uint32_t)job->id, i);
                mimir_free(p);
            }
        } else {
            uint64_t a = now_ns();
            int rc = mrq_push(job->q, p);
            uint64_t b = now_ns();
            job->push_samples[i] = b - a;
            if (rc == MRQ_PUSH_OK) {
                job->ok++;
            } else {
                job->bypassed++;
                job->bypass_sum +=
                    block_check(p, (uint32_t)job->id, i);
                mimir_free(p);
            }
        }
    }
    return NULL;
}

/* mode: 0 = enqueue latency, 1 = end-to-end total latency */
static int run_queue31(int mode, const char *phase)
{
    static mrq_t q;
    static struct q_prod prods[Q_PRODUCERS];
    static pthread_t th[Q_PRODUCERS];
    static uint64_t push_samples[Q_PRODUCERS][Q_PER];
    static uint64_t push_stamps[Q_PRODUCERS][Q_PER];
    static uint64_t lat_samples[Q_PRODUCERS * Q_PER];
    mrq_init(&q);
    uint64_t floor_ns = timer_floor_ns();
    for (int t = 0; t < Q_PRODUCERS; t++) {
        memset(&prods[t], 0, sizeof(prods[t]));
        prods[t].id = t;
        prods[t].q = &q;
        prods[t].total_mode = mode;
        prods[t].push_samples = push_samples[t];
        prods[t].push_stamps = push_stamps[t];
        memset(push_stamps[t], 0, sizeof(push_stamps[t]));
    }
    uint64_t wall0 = now_ns();
    for (int t = 0; t < Q_PRODUCERS; t++) {
        pthread_create(&th[t], NULL, q_prod_main, &prods[t]);
    }
    /* Owner = main thread: drain + free concurrently. */
    void *batch[MRQ_DRAIN_BATCH];
    uint64_t checksum = 0, errors = 0, lat_n = 0, owner_freed = 0;
    uint64_t drain_calls = 0, drain_empty = 0;
    int joined = 0;
    pin_cpu(0);
    for (;;) {
        size_t n = mrq_drain_batch(&q, batch, MRQ_DRAIN_BATCH);
        drain_calls++;
        if (n == 0) {
            drain_empty++;
        }
        for (size_t i = 0; i < n; i++) {
            uint64_t tag;
            memcpy(&tag, batch[i], 8);
            uint32_t prod = (uint32_t)(tag >> 32);
            uint32_t seq = (uint32_t)tag;
            uint64_t c = (prod < Q_PRODUCERS && seq < Q_PER)
                             ? block_check(batch[i], prod, seq)
                             : UINT64_MAX;
            if (c == UINT64_MAX) {
                errors++;
                c = 0;
            }
            checksum += c;
            mimir_free(batch[i]);
            owner_freed++;
            if (mode == 1 && prod < Q_PRODUCERS && seq < Q_PER) {
                uint64_t stamp = push_stamps[prod][seq];
                if (stamp != 0) {
                    lat_samples[lat_n++] = now_ns() - stamp;
                }
            }
        }
        if (n == 0) {
            if (!joined) {
                for (int t = 0; t < Q_PRODUCERS; t++) {
                    pthread_join(th[t], NULL);
                }
                joined = 1;
            } else {
                break;
            }
        }
    }
    uint64_t wall1 = now_ns();
    uint64_t ok = 0, bypassed = 0;
    for (int t = 0; t < Q_PRODUCERS; t++) {
        ok += prods[t].ok;
        bypassed += prods[t].bypassed;
        checksum += prods[t].bypass_sum;
        errors += prods[t].errors;
    }
    uint64_t offered = (uint64_t)Q_PRODUCERS * Q_PER;
    if (errors != 0 || ok + bypassed != offered ||
        owner_freed != ok) {
        fprintf(stderr, "%s: errors=%llu ok=%llu bypassed=%llu "
                        "owner_freed=%llu\n",
                phase, (unsigned long long)errors,
                (unsigned long long)ok,
                (unsigned long long)bypassed,
                (unsigned long long)owner_freed);
        return 1;
    }
    double wall_per_op = (double)(wall1 - wall0) / (double)offered;
    if (mode == 0) {
        static uint64_t merged[Q_PRODUCERS * Q_PER];
        memcpy(merged, push_samples, sizeof(merged));
        uint64_t p99, max_ns;
        lat_stats(merged, offered, &p99, &max_ns);
        printf("RESULT phase=%s ns_per_op=%.1f ops=%llu "
               "checksum=%llu max_ns=%llu p99_ns=%llu "
               "timer_floor_ns=%llu bypassed=%llu "
               "push_median_ns=%llu queued=%llu "
               "drain_calls=%llu drain_empty=%llu\n",
               phase, wall_per_op, (unsigned long long)offered,
               (unsigned long long)checksum,
               (unsigned long long)max_ns,
               (unsigned long long)p99,
               (unsigned long long)floor_ns,
               (unsigned long long)bypassed,
               (unsigned long long)merged[offered / 2],
               (unsigned long long)ok,
               (unsigned long long)drain_calls,
               (unsigned long long)drain_empty);
    } else {
        uint64_t p99, max_ns;
        lat_stats(lat_samples, lat_n, &p99, &max_ns);
        printf("RESULT phase=%s ns_per_op=%.1f ops=%llu "
               "checksum=%llu max_ns=%llu p99_ns=%llu "
               "timer_floor_ns=%llu bypassed=%llu "
               "e2e_median_ns=%llu queued=%llu "
               "drain_calls=%llu drain_empty=%llu\n",
               phase, wall_per_op, (unsigned long long)offered,
               (unsigned long long)checksum,
               (unsigned long long)max_ns,
               (unsigned long long)p99,
               (unsigned long long)floor_ns,
               (unsigned long long)bypassed,
               (unsigned long long)lat_samples[lat_n / 2],
               (unsigned long long)ok,
               (unsigned long long)drain_calls,
               (unsigned long long)drain_empty);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: probe_z043 <phase>\n");
        return 2;
    }
    const char *phase = argv[1];
    if (strcmp(phase, "local32_wall") == 0) {
        return run_ring(0, phase);
    }
    if (strcmp(phase, "ring32_wall") == 0) {
        return run_ring(1, phase);
    }
    if (strcmp(phase, "ring32_lat") == 0) {
        return run_ring(2, phase);
    }
    if (strcmp(phase, "pc16x16_lat") == 0) {
        return run_pc(phase);
    }
    if (strcmp(phase, "queue31_enq") == 0) {
        return run_queue31(0, phase);
    }
    if (strcmp(phase, "queue31_total") == 0) {
        return run_queue31(1, phase);
    }
    fprintf(stderr, "unknown phase %s\n", phase);
    return 2;
}
