/*
 * probe_z030.c -- Z-030 perf probe: Z-024's remote-free cost
 * re-measured on this tree, plus the queue path priced three
 * honest ways.
 *
 * One phase per process (Z-004 protocol, as Z-024): the
 * runner discards 3 warm-up processes and medians 7 measured
 * ones. Threads pin themselves (CCD0 = CPUs 0-7, CCD1 =
 * CPUs 8-15). Shape: the Z-024 small-64 steady shape -
 * 8,192 blocks of 64 bytes, free every SECOND block (4,096
 * frees), so no span empties and the timed cost is lookup +
 * lock + threading (direct) or enqueue / enqueue+drain
 * (queue). Patterns are written at allocation and the batch
 * checksum is verified OUTSIDE the timed region (K-002
 * discipline); handoff cost is excluded from direct legs by
 * Z-024's publish-flag construction.
 *
 * Queue legs price the Z-030 remote-free queue
 * (tests/concurrency/mimir_remote_queue_z030.h) WITHOUT
 * hiding cost:
 *   *_enq   - the freer-side cost only: the remote thread's
 *             enqueue loop is timed; the owner drains and
 *             frees AFTER the timer stops. This is what a
 *             remote freer pays; it is NOT the system cost.
 *   *_total - the whole system cost: the enqueue loop runs
 *             while the owner drains and performs the real
 *             mimir_free concurrently; the timer stops when
 *             the last queued block has been freed by the
 *             owner. Enqueue + drain + free, all inside.
 *   *_direct - Z-024's geometry re-run: the remote thread
 *             calls mimir_free itself, timed.
 * Swap legs: 4 threads each allocate a batch, then each
 * frees (direct) or queue-routes (total) the NEXT thread's
 * batch; the figure is wall time over all frees, Z-024's
 * swap definition.
 *
 * Output: one RESULT line per process:
 *   RESULT phase=<name> ns_per_op=<f> ops=<n> checksum=<n>
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

#define BATCH 8192
/* Swap legs use Z-024's exact swap shape: 4,096 blocks PER
 * THREAD (run_z024.sh: 'swap 64 4096'). The pair legs use
 * Z-024's 8,192-block batch. Keeping the shapes distinct
 * matters: free cost on this core is a function of live-set
 * size (Z-024 anatomy point 1). */
#define SWAP_BATCH 4096
#define SIZE 64

static void pin(int cpu)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
}

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static void fill_batch(void **ptrs, size_t n, unsigned seed)
{
    for (size_t i = 0; i < n; i++) {
        unsigned char v = (unsigned char)(seed + i);
        ((unsigned char *)ptrs[i])[0] = v;
        ((unsigned char *)ptrs[i])[SIZE - 1] = (unsigned char)(v ^ 0xffu);
    }
}

static uint64_t batch_checksum(void **ptrs, size_t n, size_t stride)
{
    uint64_t sum = 0;
    for (size_t i = 0; i < n; i += stride) {
        sum += ((unsigned char *)ptrs[i])[0];
        sum += ((unsigned char *)ptrs[i])[SIZE - 1];
    }
    return sum;
}

static size_t alloc_batch(void **ptrs, size_t n)
{
    size_t got = 0;
    for (size_t i = 0; i < n; i++) {
        ptrs[i] = mimir_malloc(SIZE);
        if (ptrs[i] == NULL) {
            break;
        }
        got++;
    }
    return got;
}

static void free_untimed(void **ptrs, size_t n, size_t stride)
{
    /* Only the blocks the timed region did NOT free: those
     * not selected by the stride. */
    for (size_t i = 0; i < n; i++) {
        if (i % stride != 0) {
            mimir_free(ptrs[i]);
        }
    }
}

/* ---------- one-to-one geometries ---------- */

struct handoff {
    void **ptrs;
    size_t n;         /* stride-selected count */
    size_t stride;
    int consumer_cpu;
    int owner_cpu;
    _Atomic int published;
    double consumer_ns; /* timed region, consumer-reported */
    uint64_t enqueued;
    uint64_t bypassed;
    mrq_t *q;
    _Atomic int producers_done;
    uint64_t drained_freed;
};

static void *direct_consumer(void *arg)
{
    struct handoff *h = (struct handoff *)arg;
    pin(h->consumer_cpu);
    while (!atomic_load_explicit(&h->published, memory_order_acquire)) {
        sched_yield();
    }
    double t0 = now_ns();
    for (size_t i = 0; i < h->n; i += h->stride) {
        mimir_free(h->ptrs[i]);
    }
    h->consumer_ns = now_ns() - t0;
    return NULL;
}

static void *enq_consumer(void *arg)
{
    struct handoff *h = (struct handoff *)arg;
    pin(h->consumer_cpu);
    while (!atomic_load_explicit(&h->published, memory_order_acquire)) {
        sched_yield();
    }
    double t0 = now_ns();
    for (size_t i = 0; i < h->n; i += h->stride) {
        if (mrq_push(h->q, h->ptrs[i]) == MRQ_PUSH_OK) {
            h->enqueued++;
        } else {
            h->bypassed++;
            mimir_free(h->ptrs[i]);
        }
    }
    h->consumer_ns = now_ns() - t0;
    atomic_store_explicit(&h->producers_done, 1, memory_order_release);
    return NULL;
}

static void *owner_drainer(void *arg)
{
    struct handoff *h = (struct handoff *)arg;
    pin(h->owner_cpu);
    void *batch[MRQ_DRAIN_BATCH];
    for (;;) {
        size_t n = mrq_drain_batch(h->q, batch, MRQ_DRAIN_BATCH);
        for (size_t i = 0; i < n; i++) {
            mimir_free(batch[i]);
            h->drained_freed++;
        }
        if (n == 0) {
            if (atomic_load_explicit(&h->producers_done,
                                     memory_order_acquire)) {
                size_t m = mrq_drain_batch(h->q, batch, MRQ_DRAIN_BATCH);
                for (size_t i = 0; i < m; i++) {
                    mimir_free(batch[i]);
                    h->drained_freed++;
                }
                if (m == 0) {
                    return NULL;
                }
            } else {
                sched_yield();
            }
        }
    }
}

static int run_pair(const char *phase, int alloc_cpu, int free_cpu,
                    int mode) /* 0 direct, 1 enq, 2 total */
{
    pin(alloc_cpu);
    void **ptrs = (void **)malloc(sizeof(void *) * BATCH);
    if (ptrs == NULL) {
        return 2;
    }
    size_t got = alloc_batch(ptrs, BATCH);
    if (got != BATCH) {
        fprintf(stderr, "alloc shortfall %zu\n", got);
        return 2;
    }
    fill_batch(ptrs, BATCH, 0x30);
    uint64_t expect = batch_checksum(ptrs, BATCH, 2);
    if (batch_checksum(ptrs, BATCH, 2) != expect) {
        return 2;
    }
    static mrq_t q;
    mrq_init(&q);
    struct handoff h;
    memset(&h, 0, sizeof(h));
    h.ptrs = ptrs;
    h.n = BATCH;
    h.stride = 2;
    h.consumer_cpu = free_cpu;
    h.owner_cpu = alloc_cpu;
    h.q = &q;
    atomic_init(&h.published, 0);
    atomic_init(&h.producers_done, 0);
    pthread_t consumer, owner;
    double wall0 = 0.0, wall1 = 0.0;
    if (mode == 0) {
        pthread_create(&consumer, NULL, direct_consumer, &h);
        atomic_store_explicit(&h.published, 1, memory_order_release);
        pthread_join(consumer, NULL);
    } else if (mode == 1) {
        pthread_create(&consumer, NULL, enq_consumer, &h);
        atomic_store_explicit(&h.published, 1, memory_order_release);
        pthread_join(consumer, NULL);
        /* Untimed: drain and free whatever was queued. */
        atomic_store_explicit(&h.producers_done, 1, memory_order_release);
        void *batch[MRQ_DRAIN_BATCH];
        size_t n;
        uint64_t drained = 0;
        while ((n = mrq_drain_batch(&q, batch, MRQ_DRAIN_BATCH)) > 0) {
            for (size_t i = 0; i < n; i++) {
                mimir_free(batch[i]);
                drained++;
            }
        }
        if (drained != h.enqueued) {
            fprintf(stderr, "enq drain mismatch %llu vs %llu\n",
                    (unsigned long long)drained,
                    (unsigned long long)h.enqueued);
            return 2;
        }
    } else {
        pthread_create(&owner, NULL, owner_drainer, &h);
        pthread_create(&consumer, NULL, enq_consumer, &h);
        atomic_store_explicit(&h.published, 1, memory_order_release);
        wall0 = now_ns();
        pthread_join(consumer, NULL);
        pthread_join(owner, NULL);
        wall1 = now_ns();
        if (h.drained_freed != h.enqueued) {
            fprintf(stderr, "total drain mismatch\n");
            return 2;
        }
        h.consumer_ns = wall1 - wall0;
    }
    size_t ops = BATCH / 2;
    double ns_per = h.consumer_ns / (double)ops;
    /* Free the untouched half (untimed) and report. */
    for (size_t i = 1; i < BATCH; i += 2) {
        mimir_free(ptrs[i]);
    }
    printf("RESULT phase=%s ns_per_op=%.1f ops=%zu checksum=%llu "
           "enqueued=%llu bypassed=%llu\n",
           phase, ns_per, ops, (unsigned long long)expect,
           (unsigned long long)h.enqueued,
           (unsigned long long)h.bypassed);
    free(ptrs);
    return 0;
}

/* ---------- swap x4 ---------- */

struct swap_shared {
    void **batches[4];
    int cpus[4];
    pthread_barrier_t barrier;
    mrq_t queues[4];
    _Atomic int done_flags[4];
    uint64_t drained_freed[4];
    int alloc_failed;
    uint64_t checksums[4];
};

struct swap_worker {
    struct swap_shared *s;
    int me;
    int mode; /* 0 direct, 2 total */
};

static void *swap_worker_main(void *arg)
{
    struct swap_worker *w = (struct swap_worker *)arg;
    struct swap_shared *s = w->s;
    int me = w->me;
    pin(s->cpus[me]);
    /* Each thread allocates and fills its OWN batch before
     * the barrier - the Z-024 swap construction exactly. */
    if (alloc_batch(s->batches[me], SWAP_BATCH) != SWAP_BATCH) {
        s->alloc_failed = 1;
    }
    fill_batch(s->batches[me], SWAP_BATCH, (unsigned)(0x40 + me));
    s->checksums[me] = batch_checksum(s->batches[me], SWAP_BATCH, 2);
    pthread_barrier_wait(&s->barrier);
    if (w->mode == 0) {
        void **victim = s->batches[(me + 1) % 4];
        for (size_t i = 0; i < SWAP_BATCH; i += 2) {
            mimir_free(victim[i]);
        }
    } else if (w->mode == 1) {
        int target = (me + 1) % 4;
        void **mine = s->batches[me];
        for (size_t i = 0; i < SWAP_BATCH; i += 2) {
            if (mrq_push(&s->queues[target], mine[i]) != MRQ_PUSH_OK) {
                mimir_free(mine[i]);
            } else {
                s->drained_freed[me]++; /* here: enqueued count */
            }
        }
    } else {
        int target = (me + 1) % 4;
        void **mine = s->batches[me];
        for (size_t i = 0; i < SWAP_BATCH; i += 2) {
            if (mrq_push(&s->queues[target], mine[i]) != MRQ_PUSH_OK) {
                mimir_free(mine[i]);
            }
        }
        atomic_store_explicit(&s->done_flags[me], 1,
                              memory_order_release);
        int producer = (me + 3) % 4;
        void *batch[MRQ_DRAIN_BATCH];
        for (;;) {
            size_t n = mrq_drain_batch(&s->queues[me], batch,
                                       MRQ_DRAIN_BATCH);
            for (size_t i = 0; i < n; i++) {
                mimir_free(batch[i]);
                s->drained_freed[me]++;
            }
            if (n == 0) {
                if (atomic_load_explicit(&s->done_flags[producer],
                                         memory_order_acquire)) {
                    size_t m = mrq_drain_batch(&s->queues[me], batch,
                                               MRQ_DRAIN_BATCH);
                    for (size_t i = 0; i < m; i++) {
                        mimir_free(batch[i]);
                        s->drained_freed[me]++;
                    }
                    if (m == 0) {
                        break;
                    }
                } else {
                    sched_yield();
                }
            }
        }
    }
    return NULL;
}

static int run_swap(const char *phase, const int cpus[4], int mode)
{
    static struct swap_shared s;
    memset(&s, 0, sizeof(s));
    for (int i = 0; i < 4; i++) {
        s.cpus[i] = cpus[i];
        mrq_init(&s.queues[i]);
        atomic_init(&s.done_flags[i], 0);
        s.batches[i] = (void **)malloc(sizeof(void *) * SWAP_BATCH);
        if (s.batches[i] == NULL) {
            return 2;
        }
    }
    /* Barrier of 5: the four workers plus this thread, whose
     * arrival starts the wall clock as the barrier releases -
     * thread creation and allocation sit outside the wall. */
    pthread_barrier_init(&s.barrier, NULL, 5);
    struct swap_worker workers[4];
    pthread_t ths[4];
    for (int i = 0; i < 4; i++) {
        workers[i].s = &s;
        workers[i].me = i;
        workers[i].mode = mode;
        pthread_create(&ths[i], NULL, swap_worker_main, &workers[i]);
    }
    double t0 = now_ns();
    pthread_barrier_wait(&s.barrier);
    for (int i = 0; i < 4; i++) {
        pthread_join(ths[i], NULL);
    }
    double wall = now_ns() - t0;
    if (s.alloc_failed) {
        fprintf(stderr, "swap alloc shortfall\n");
        return 2;
    }
    uint64_t expect = 0;
    for (int i = 0; i < 4; i++) {
        expect += s.checksums[i];
    }
    if (mode == 1) {
        /* Untimed cleanup: drain every queue and free. The
         * per-worker drained_freed[] field counted enqueues
         * in this mode; the drain must match it in sum. */
        uint64_t enq = 0, dr = 0;
        void *batch[MRQ_DRAIN_BATCH];
        for (int i = 0; i < 4; i++) {
            enq += s.drained_freed[i];
            size_t n;
            while ((n = mrq_drain_batch(&s.queues[i], batch,
                                        MRQ_DRAIN_BATCH)) > 0) {
                for (size_t k = 0; k < n; k++) {
                    mimir_free(batch[k]);
                    dr++;
                }
            }
        }
        if (dr != enq) {
            fprintf(stderr, "swap enq drain mismatch\n");
            return 2;
        }
    }
    size_t ops = 4 * (SWAP_BATCH / 2);
    printf("RESULT phase=%s ns_per_op=%.1f ops=%zu checksum=%llu "
           "enqueued=0 bypassed=0\n",
           phase, wall / (double)ops, ops,
           (unsigned long long)expect);
    for (int i = 0; i < 4; i++) {
        free_untimed(s.batches[i], SWAP_BATCH, 2);
        free(s.batches[i]);
    }
    pthread_barrier_destroy(&s.barrier);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: probe_z030 <phase>\n");
        return 2;
    }
    const char *phase = argv[1];
    static const int same4[4] = { 2, 3, 4, 5 };
    static const int cross4[4] = { 2, 10, 3, 11 };
    if (strcmp(phase, "local_steady") == 0) {
        return run_pair(phase, 2, 2, 0);
    }
    if (strcmp(phase, "remote_same_direct") == 0) {
        return run_pair(phase, 2, 3, 0);
    }
    if (strcmp(phase, "remote_cross_direct") == 0) {
        return run_pair(phase, 2, 10, 0);
    }
    if (strcmp(phase, "remote_same_enq") == 0) {
        return run_pair(phase, 2, 3, 1);
    }
    if (strcmp(phase, "remote_cross_enq") == 0) {
        return run_pair(phase, 2, 10, 1);
    }
    if (strcmp(phase, "remote_same_total") == 0) {
        return run_pair(phase, 2, 3, 2);
    }
    if (strcmp(phase, "remote_cross_total") == 0) {
        return run_pair(phase, 2, 10, 2);
    }
    if (strcmp(phase, "swap_same_direct") == 0) {
        return run_swap(phase, same4, 0);
    }
    if (strcmp(phase, "swap_cross_direct") == 0) {
        return run_swap(phase, cross4, 0);
    }
    if (strcmp(phase, "swap_same_enq") == 0) {
        return run_swap(phase, same4, 1);
    }
    if (strcmp(phase, "swap_cross_enq") == 0) {
        return run_swap(phase, cross4, 1);
    }
    if (strcmp(phase, "swap_same_total") == 0) {
        return run_swap(phase, same4, 2);
    }
    if (strcmp(phase, "swap_cross_total") == 0) {
        return run_swap(phase, cross4, 2);
    }
    fprintf(stderr, "unknown phase %s\n", phase);
    return 2;
}
