/*
 * test_z043_free_storm.c -- Z-043 producer/consumer free
 * storm: correctness at 32 threads.
 *
 * Slice Z-043 (docs/MIMIRALLOC_Z13_ROADMAP.md): "Producer/
 * consumer free storm - 32-thread cross-free patterns at
 * rates the banked core never faced; remote-free queue
 * behaviour under flood." Gate: storm green; worst-case
 * remote-free latency recorded (the latency half lives in
 * tests/perf/probe_z043.c; this file is the storm-green
 * half).
 *
 * POSITION (honest). Two paths are stormed:
 *  (a) The PRODUCTION path: mimir_malloc / mimir_free on the
 *      banked Wave-4 core (Z-029 TLS cache live; a small
 *      block freed by a non-owner thread routes to the
 *      central core under the one global lock and counts
 *      remote on the freeing thread's TLS snapshot). Prior
 *      cross-free evidence stops at 4 concurrent freers
 *      (Z-024 swap x4) and 8 producers into one queue
 *      (Z-030 Q5); no banked run has 32 threads freeing
 *      across ownership at once.
 *  (b) The Z-030 remote-free queue
 *      (tests/concurrency/mimir_remote_queue_z030.h),
 *      still test-only (NOT wired into mimir_free), driven
 *      explicitly at flood scale: 31 producers into one
 *      owner queue, and 32 queues cross-flooded at once -
 *      beyond Z-030's 8-producer / swap-x4 geometries.
 *
 * Phases (all deterministic: fixed counts, joins before
 * every accounting read, no timing assertions, no sleeps;
 * concurrent drain/consume loops poll a done flag plus the
 * work channel, and every assertion is made at a quiescent
 * point after pthread_join):
 *   S1 ring32 direct  - 32 threads; each allocates a batch,
 *      barrier, then each frees the NEXT thread's batch via
 *      production mimir_free (every free is a cross-thread
 *      free, all 32 freeing concurrently). 3 rounds.
 *      Per-thread remote_frees_out deltas are read by each
 *      freer from its own TLS snapshot before exit and
 *      summed after join: the identity summed == offered is
 *      asserted as MEASURED on this tree (both production
 *      remote routes - owner-record route and no-record
 *      fallback - count exactly one remote per cross free;
 *      see the recorded run for the derivation).
 *   S2 split16 direct - 16 producers publish batches into
 *      per-producer SPSC rings; 16 consumers (one per
 *      producer) free them via mimir_free as they appear:
 *      the classic producer/consumer shape on the
 *      production path, streaming, not barrier-released.
 *   S3 queue flood 31->1 - 31 producers self-allocate and
 *      push into ONE owner queue (capacity 256); the owner
 *      (main thread) drains bounded batches of 8 and frees
 *      concurrently. Refusals are counted FULL bypasses:
 *      the producer frees its own block directly.
 *      offered == queued + bypassed exactly; the owner
 *      verifies every drained block's pattern and proves
 *      exactly-once over the (producer, seq) space with a
 *      bitmap.
 *   S4 queue flood 32 owners - 32 threads each own a queue;
 *      each allocates a batch and pushes it into the NEXT
 *      thread's queue while draining and freeing its own
 *      queue concurrently. Bypass: the pusher frees its own
 *      block directly (a local free). Per-queue pushed ==
 *      drained and pending == 0 at quiescence.
 *   S5 ring32 mixed sizes - S1's ring at 32 threads over
 *      four small classes (32/64/128/512 B), one round:
 *      cross-free correctness is not a single-class fact.
 * Every phase ends with live_small_objects back at its
 * phase baseline and mimir_core_metadata_check() == 0.
 * Blocks carry the Z-030 self-describing (producer, seq)
 * tag pattern; whoever frees verifies before freeing.
 *
 * Counting discipline follows test_d004.c / the Z-030
 * suite: workers accumulate into their own structs; the
 * main thread sums after joins.
 * Standalone via `make test-z043`; NOT part of `make test`
 * (the standing gate's 4988/0/77 counts are untouched).
 *
 * RED-PATH PROOFS (the runner builds both; each MUST fail):
 *  -DMRQ_MUTANT_LOSSY: the Z-030 lossy queue mutant
 *   silently loses every 64th push; S3/S4 accounting and
 *   exactly-once cannot reconcile.
 *  -DZ043_MUTANT_DROP_FREE: this harness's own mutant -
 *   S1 consumers silently skip every 8192nd free (a leaked
 *   block); the end-of-phase live-object identity must
 *   catch it. A storm that cannot see a leak proves
 *   nothing.
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

#include "mimir_alloc.h"
#include "mimir_remote_queue_z030.h"
/* Test-only internal surfaces (the Z-030 suite's shape):
 * the J-002 stats snapshot, the J-006 metadata check and
 * the Z-029 TLS snapshot, reached by linking the library
 * objects directly. */
#include "../../src/core/mimir_core.h"
#include "../../tests/contract/mimir_tls_cache_z028.h"

static unsigned long g_checks = 0;
static unsigned long g_failures = 0;

static void check(int ok, const char *label)
{
    g_checks++;
    if (!ok) {
        g_failures++;
        fprintf(stderr, "FAIL: %s\n", label);
    }
}

/* Per-block pattern: the Z-030 suite's scheme - an 8-byte
 * tag at offset 0 encoding (producer, sequence); the byte
 * (tag & 0x5a) repeated over the middle; its complement at
 * the last byte. Verified by whoever frees, before freeing.
 * Requires size >= 16. */
static void block_fill(void *p, size_t size, uint32_t producer,
                       uint32_t seq)
{
    uint64_t tag = ((uint64_t)producer << 32) | seq;
    memcpy(p, &tag, 8);
    unsigned char mid = (unsigned char)(tag & 0x5au);
    memset((unsigned char *)p + 8, mid, size - 9);
    ((unsigned char *)p)[size - 1] = (unsigned char)(mid ^ 0xffu);
}

static int block_verify(const void *p, size_t size, uint32_t producer,
                        uint32_t seq)
{
    uint64_t tag;
    memcpy(&tag, p, 8);
    if (tag != (((uint64_t)producer << 32) | seq)) {
        return 0;
    }
    unsigned char mid = (unsigned char)(tag & 0x5au);
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 8; i + 1 < size; i++) {
        if (b[i] != mid) {
            return 0;
        }
    }
    return b[size - 1] == (unsigned char)(mid ^ 0xffu);
}

static uint64_t live_small_now(void)
{
    struct mimir_stats st;
    if (mimir_core_stats_snapshot(&st) != 0) {
        return UINT64_MAX;
    }
    return st.live_small_objects;
}

static uint64_t remote_out_now(void)
{
    struct mimir_tls_cache_snapshot snap;
    if (mimir_tls_cache_snapshot(&snap) != 0) {
        return UINT64_MAX;
    }
    return snap.remote_frees_out;
}

static void pin_cpu(int cpu)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
}

static void phase_end(const char *name, uint64_t baseline)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "%s live_small back at baseline", name);
    check(live_small_now() == baseline, buf);
    snprintf(buf, sizeof(buf), "%s metadata check clean", name);
    check(mimir_core_metadata_check() == 0, buf);
}

/* ---------------- S1/S5: ring cross-free, production ------ */

#define RING_THREADS 32

struct ring_job {
    int id;
    int rounds;
    const size_t *sizes;      /* per-block sizes (len count) */
    size_t count;
    void **mine;              /* this thread's current batch */
    void ***all;              /* all threads' batch arrays */
    pthread_barrier_t *bar;
    uint64_t verified;
    uint64_t freed;
    uint64_t pattern_failures;
    uint64_t alloc_failures;
    uint64_t remote_delta;
};

static void *ring_main(void *arg)
{
    struct ring_job *job = (struct ring_job *)arg;
    pin_cpu(job->id % 32);
    uint64_t remote0 = remote_out_now();
    for (int round = 0; round < job->rounds; round++) {
        for (size_t i = 0; i < job->count; i++) {
            void *p = mimir_malloc(job->sizes[i]);
            if (p == NULL) {
                job->alloc_failures++;
                job->mine[i] = NULL;
                continue;
            }
            block_fill(p, job->sizes[i], (uint32_t)job->id,
                       (uint32_t)i);
            job->mine[i] = p;
        }
        pthread_barrier_wait(job->bar);
        /* Free the NEXT thread's batch: every free crosses
         * ownership, all 32 threads freeing at once. */
        int victim = (job->id + 1) % RING_THREADS;
        void **batch = job->all[victim];
        for (size_t i = 0; i < job->count; i++) {
            void *p = batch[i];
            if (p == NULL) {
                continue;
            }
            if (block_verify(p, job->sizes[i], (uint32_t)victim,
                             (uint32_t)i)) {
                job->verified++;
            } else {
                job->pattern_failures++;
            }
#ifdef Z043_MUTANT_DROP_FREE
            /* MUTANT: silently leak every 1024th free in
             * round 0. The phase-end live identity MUST
             * catch this. */
            if (round == 0 && (job->freed + 1) % 1024 == 0) {
                continue;
            }
#endif
            mimir_free(p);
            job->freed++;
        }
        pthread_barrier_wait(job->bar);
    }
    job->remote_delta = remote_out_now() - remote0;
    return NULL;
}

static void phase_ring(const char *name, const size_t *sizes,
                       size_t count, int rounds, int assert_remote)
{
    uint64_t baseline = live_small_now();
    static void *storage[RING_THREADS][4096];
    static void **all[RING_THREADS];
    static struct ring_job jobs[RING_THREADS];
    static pthread_t th[RING_THREADS];
    pthread_barrier_t bar;
    pthread_barrier_init(&bar, NULL, RING_THREADS);
    for (int t = 0; t < RING_THREADS; t++) {
        all[t] = storage[t];
        memset(&jobs[t], 0, sizeof(jobs[t]));
        jobs[t].id = t;
        jobs[t].rounds = rounds;
        jobs[t].sizes = sizes;
        jobs[t].count = count;
        jobs[t].mine = storage[t];
        jobs[t].all = all;
        jobs[t].bar = &bar;
        pthread_create(&th[t], NULL, ring_main, &jobs[t]);
    }
    uint64_t offered = 0, freed = 0, verified = 0, remote = 0;
    uint64_t patfail = 0, allocfail = 0;
    for (int t = 0; t < RING_THREADS; t++) {
        pthread_join(th[t], NULL);
        offered += jobs[t].count * (uint64_t)rounds;
        freed += jobs[t].freed;
        verified += jobs[t].verified;
        remote += jobs[t].remote_delta;
        patfail += jobs[t].pattern_failures;
        allocfail += jobs[t].alloc_failures;
    }
    pthread_barrier_destroy(&bar);
    char buf[128];
    snprintf(buf, sizeof(buf), "%s no alloc failures", name);
    check(allocfail == 0, buf);
    snprintf(buf, sizeof(buf), "%s patterns verified == offered", name);
    check(verified == offered, buf);
    snprintf(buf, sizeof(buf), "%s zero pattern failures", name);
    check(patfail == 0, buf);
    snprintf(buf, sizeof(buf), "%s freed == offered", name);
    check(freed == offered, buf);
    if (assert_remote) {
        snprintf(buf, sizeof(buf),
                 "%s summed remote_frees_out == offered", name);
        check(remote == offered, buf);
    }
    printf("%s: offered=%llu freed=%llu remote_sum=%llu\n", name,
           (unsigned long long)offered, (unsigned long long)freed,
           (unsigned long long)remote);
    phase_end(name, baseline);
}

/* ---------------- S2: 16x16 producer/consumer, production - */

#define SPLIT_PRODUCERS 16
#define SPLIT_PER 4096
#define SPLIT_RING 1024  /* slots per producer-consumer ring */

struct split_ring {
    _Atomic uint64_t head; /* consumer-owned */
    _Atomic uint64_t tail; /* producer-owned */
    void *slots[SPLIT_RING];
};

struct split_prod {
    int id;
    struct split_ring *ring;
    uint64_t produced;
    uint64_t alloc_failures;
};

struct split_cons {
    int id;
    struct split_ring *ring;
    _Atomic int *producer_done;
    uint64_t freed;
    uint64_t verified;
    uint64_t pattern_failures;
    uint64_t remote_delta;
};

static void *split_prod_main(void *arg)
{
    struct split_prod *job = (struct split_prod *)arg;
    pin_cpu(job->id % 32);
    for (uint32_t i = 0; i < SPLIT_PER; i++) {
        void *p = mimir_malloc(64);
        if (p == NULL) {
            job->alloc_failures++;
            continue;
        }
        block_fill(p, 64, (uint32_t)job->id, i);
        uint64_t tail = atomic_load_explicit(&job->ring->tail,
                                             memory_order_relaxed);
        while (tail - atomic_load_explicit(&job->ring->head,
                                           memory_order_acquire) >=
               SPLIT_RING) {
            sched_yield(); /* ring full: consumer is behind */
        }
        job->ring->slots[tail % SPLIT_RING] = p;
        atomic_store_explicit(&job->ring->tail, tail + 1,
                              memory_order_release);
        job->produced++;
    }
    return NULL;
}

static void *split_cons_main(void *arg)
{
    struct split_cons *job = (struct split_cons *)arg;
    pin_cpu((16 + job->id) % 32);
    uint64_t remote0 = remote_out_now();
    uint64_t head = 0;
    for (;;) {
        uint64_t tail = atomic_load_explicit(&job->ring->tail,
                                             memory_order_acquire);
        if (head < tail) {
            void *p = job->ring->slots[head % SPLIT_RING];
            if (block_verify(p, 64, (uint32_t)job->id,
                             (uint32_t)head)) {
                job->verified++;
            } else {
                job->pattern_failures++;
            }
            mimir_free(p);
            job->freed++;
            head++;
            atomic_store_explicit(&job->ring->head, head,
                                  memory_order_release);
        } else if (atomic_load_explicit(job->producer_done,
                                        memory_order_acquire)) {
            /* Re-read tail AFTER seeing done: the tail read
             * above may predate the producer's final store,
             * which happens-before done. Only an empty
             * re-read proves the ring is drained. */
            uint64_t tail2 = atomic_load_explicit(
                &job->ring->tail, memory_order_acquire);
            if (head == tail2) {
                break;
            }
        } else {
            sched_yield();
        }
    }
    job->remote_delta = remote_out_now() - remote0;
    return NULL;
}

static void phase_split(void)
{
    const char *name = "S2 split16";
    uint64_t baseline = live_small_now();
    static struct split_ring rings[SPLIT_PRODUCERS];
    static struct split_prod prods[SPLIT_PRODUCERS];
    static struct split_cons conss[SPLIT_PRODUCERS];
    static _Atomic int done[SPLIT_PRODUCERS];
    static pthread_t pth[SPLIT_PRODUCERS], cth[SPLIT_PRODUCERS];
    for (int t = 0; t < SPLIT_PRODUCERS; t++) {
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
        conss[t].producer_done = &done[t];
        pthread_create(&cth[t], NULL, split_cons_main, &conss[t]);
        pthread_create(&pth[t], NULL, split_prod_main, &prods[t]);
    }
    uint64_t produced = 0, freed = 0, verified = 0, remote = 0;
    uint64_t patfail = 0, allocfail = 0;
    for (int t = 0; t < SPLIT_PRODUCERS; t++) {
        pthread_join(pth[t], NULL);
        atomic_store_explicit(&done[t], 1, memory_order_release);
        pthread_join(cth[t], NULL);
        produced += prods[t].produced;
        allocfail += prods[t].alloc_failures;
        freed += conss[t].freed;
        verified += conss[t].verified;
        remote += conss[t].remote_delta;
        patfail += conss[t].pattern_failures;
    }
    uint64_t offered = (uint64_t)SPLIT_PRODUCERS * SPLIT_PER;
    check(allocfail == 0, "S2 no alloc failures");
    check(produced == offered, "S2 produced == offered");
    check(freed == offered, "S2 freed == offered");
    check(verified == offered, "S2 verified == offered");
    check(patfail == 0, "S2 zero pattern failures");
    check(remote == offered, "S2 summed remote_frees_out == offered");
    printf("S2 split16: offered=%llu freed=%llu remote_sum=%llu\n",
           (unsigned long long)offered, (unsigned long long)freed,
           (unsigned long long)remote);
    phase_end(name, baseline);
}

/* ---------------- S3: queue flood 31 -> 1 ----------------- */

#define FLOOD_PRODUCERS 31
#define FLOOD_PER 2048

struct flood_owner {
    mrq_t q;
    _Atomic int producers_done;
    uint64_t freed_by_owner;
    uint64_t pattern_failures;
    uint64_t duplicates;
    unsigned char *seq_seen; /* (producer * FLOOD_PER + seq) */
};

struct flood_prod {
    int id;
    mrq_t *q;
    uint64_t ok;
    uint64_t bypassed;
    uint64_t alloc_failures;
};

static void *flood_prod_main(void *arg)
{
    struct flood_prod *job = (struct flood_prod *)arg;
    pin_cpu((job->id + 1) % 32);
    for (uint32_t i = 0; i < FLOOD_PER; i++) {
        void *p = mimir_malloc(64);
        if (p == NULL) {
            job->alloc_failures++;
            continue;
        }
        block_fill(p, 64, (uint32_t)job->id, i);
        if (mrq_push(job->q, p) == MRQ_PUSH_OK) {
            job->ok++;
        } else {
            job->bypassed++;
            mimir_free(p); /* bypass road: producer frees own */
        }
    }
    return NULL;
}

static void phase_flood31(void)
{
    const char *name = "S3 queue flood31";
    uint64_t baseline = live_small_now();
    static struct flood_owner owner;
    static struct flood_prod prods[FLOOD_PRODUCERS];
    static pthread_t th[FLOOD_PRODUCERS];
    memset(&owner, 0, sizeof(owner));
    mrq_init(&owner.q);
    atomic_init(&owner.producers_done, 0);
    size_t space = (size_t)FLOOD_PRODUCERS * FLOOD_PER;
    owner.seq_seen = calloc(space, 1);
    check(owner.seq_seen != NULL, "S3 bitmap allocated");
    for (int t = 0; t < FLOOD_PRODUCERS; t++) {
        memset(&prods[t], 0, sizeof(prods[t]));
        prods[t].id = t;
        prods[t].q = &owner.q;
        pthread_create(&th[t], NULL, flood_prod_main, &prods[t]);
    }
    /* Owner (main thread) drains concurrently in bounded
     * batches of MRQ_DRAIN_BATCH and frees. */
    void *batch[MRQ_DRAIN_BATCH];
    int joined = 0;
    for (;;) {
        size_t n = mrq_drain_batch(&owner.q, batch, MRQ_DRAIN_BATCH);
        for (size_t i = 0; i < n; i++) {
            uint64_t tag;
            memcpy(&tag, batch[i], 8);
            uint32_t prod = (uint32_t)(tag >> 32);
            uint32_t seq = (uint32_t)tag;
            if (prod >= FLOOD_PRODUCERS || seq >= FLOOD_PER ||
                !block_verify(batch[i], 64, prod, seq)) {
                owner.pattern_failures++;
            } else if (owner.seq_seen[(size_t)prod * FLOOD_PER + seq]) {
                owner.duplicates++;
            } else {
                owner.seq_seen[(size_t)prod * FLOOD_PER + seq] = 1;
            }
            mimir_free(batch[i]);
            owner.freed_by_owner++;
        }
        if (n == 0) {
            if (!joined) {
                for (int t = 0; t < FLOOD_PRODUCERS; t++) {
                    pthread_join(th[t], NULL);
                }
                joined = 1;
                atomic_store_explicit(&owner.producers_done, 1,
                                      memory_order_release);
            } else {
                break; /* joined and a full drain came up empty */
            }
        }
    }
    uint64_t ok = 0, bypassed = 0, allocfail = 0;
    for (int t = 0; t < FLOOD_PRODUCERS; t++) {
        ok += prods[t].ok;
        bypassed += prods[t].bypassed;
        allocfail += prods[t].alloc_failures;
    }
    uint64_t offered = (uint64_t)FLOOD_PRODUCERS * FLOOD_PER;
    struct mrq_stats s;
    mrq_snapshot(&owner.q, &s);
    check(allocfail == 0, "S3 no alloc failures");
    check(ok + bypassed == offered, "S3 queued + bypassed == offered");
    check(s.pushed == ok, "S3 queue pushed == producer ok sum");
    check(s.overflow == bypassed, "S3 queue overflow == bypassed sum");
    check(s.drained == ok, "S3 queue drained == pushed");
    check(s.pending == 0, "S3 queue pending == 0");
    check(owner.freed_by_owner == ok, "S3 owner freed == pushed");
    check(owner.pattern_failures == 0, "S3 zero pattern failures");
    check(owner.duplicates == 0, "S3 zero duplicate deliveries");
    uint64_t bits = 0;
    for (size_t i = 0; i < space; i++) {
        bits += owner.seq_seen[i];
    }
    check(bits == ok, "S3 bitmap bits == pushed (exactly-once)");
    printf("S3 queue flood31: offered=%llu queued=%llu bypassed=%llu\n",
           (unsigned long long)offered, (unsigned long long)ok,
           (unsigned long long)bypassed);
    free(owner.seq_seen);
    phase_end(name, baseline);
}

/* ---------------- S4: queue flood, 32 owners -------------- */

#define OWNERS 32
#define OWNERS_PER 1024

struct owner_job {
    int id;
    mrq_t *mine;
    mrq_t *next;
    _Atomic int *pushers_done; /* count of owners done pushing */
    uint64_t pushed_ok;
    uint64_t bypassed;
    uint64_t drained_freed;
    uint64_t pattern_failures;
    uint64_t alloc_failures;
};

static void owner_drain_some(struct owner_job *job)
{
    void *batch[MRQ_DRAIN_BATCH];
    size_t n = mrq_drain_batch(job->mine, batch, MRQ_DRAIN_BATCH);
    for (size_t i = 0; i < n; i++) {
        uint64_t tag;
        memcpy(&tag, batch[i], 8);
        uint32_t prod = (uint32_t)(tag >> 32);
        uint32_t seq = (uint32_t)tag;
        int expect = (job->id + OWNERS - 1) % OWNERS;
        if ((int)prod != expect || seq >= OWNERS_PER ||
            !block_verify(batch[i], 64, prod, seq)) {
            job->pattern_failures++;
        }
        mimir_free(batch[i]);
        job->drained_freed++;
    }
}

static void *owner_main(void *arg)
{
    struct owner_job *job = (struct owner_job *)arg;
    pin_cpu(job->id % 32);
    for (uint32_t i = 0; i < OWNERS_PER; i++) {
        void *p = mimir_malloc(64);
        if (p == NULL) {
            job->alloc_failures++;
            continue;
        }
        block_fill(p, 64, (uint32_t)job->id, i);
        if (mrq_push(job->next, p) == MRQ_PUSH_OK) {
            job->pushed_ok++;
        } else {
            job->bypassed++;
            mimir_free(p); /* bypass: pusher frees its own */
        }
        if ((i & 63u) == 0) {
            owner_drain_some(job); /* keep own queue moving */
        }
    }
    atomic_fetch_add_explicit(job->pushers_done, 1,
                              memory_order_acq_rel);
    /* Drain own queue to empty: peers may still be pushing;
     * conclude only when every owner has finished pushing
     * AND a full drain came up empty (the Z-030 drain
     * discipline: a push that landed before the counter
     * increment is visible to the final drain). */
    for (;;) {
        uint64_t before = job->drained_freed;
        owner_drain_some(job);
        if (job->drained_freed == before) {
            if (atomic_load_explicit(job->pushers_done,
                                     memory_order_acquire) ==
                OWNERS) {
                owner_drain_some(job);
                if (job->drained_freed == before) {
                    break;
                }
            } else {
                sched_yield();
            }
        }
    }
    return NULL;
}

static void phase_owners32(void)
{
    const char *name = "S4 queue owners32";
    uint64_t baseline = live_small_now();
    static mrq_t queues[OWNERS];
    static struct owner_job jobs[OWNERS];
    static pthread_t th[OWNERS];
    static _Atomic int pushers_done;
    atomic_init(&pushers_done, 0);
    for (int t = 0; t < OWNERS; t++) {
        mrq_init(&queues[t]);
        memset(&jobs[t], 0, sizeof(jobs[t]));
        jobs[t].id = t;
        jobs[t].mine = &queues[t];
        jobs[t].next = &queues[(t + 1) % OWNERS];
        jobs[t].pushers_done = &pushers_done;
        pthread_create(&th[t], NULL, owner_main, &jobs[t]);
    }
    for (int t = 0; t < OWNERS; t++) {
        pthread_join(th[t], NULL);
    }
    uint64_t ok = 0, bypassed = 0, drained = 0, patfail = 0;
    uint64_t allocfail = 0;
    int per_queue_ok = 1;
    for (int t = 0; t < OWNERS; t++) {
        ok += jobs[t].pushed_ok;
        bypassed += jobs[t].bypassed;
        drained += jobs[t].drained_freed;
        patfail += jobs[t].pattern_failures;
        allocfail += jobs[t].alloc_failures;
        struct mrq_stats s;
        mrq_snapshot(&queues[t], &s);
        if (s.pushed != s.drained || s.pending != 0) {
            per_queue_ok = 0;
        }
    }
    uint64_t offered = (uint64_t)OWNERS * OWNERS_PER;
    check(allocfail == 0, "S4 no alloc failures");
    check(ok + bypassed == offered, "S4 queued + bypassed == offered");
    check(drained == ok, "S4 drained-and-freed == pushed");
    check(per_queue_ok, "S4 every queue pushed == drained, pending 0");
    check(patfail == 0, "S4 zero pattern failures");
    printf("S4 queue owners32: offered=%llu queued=%llu bypassed=%llu\n",
           (unsigned long long)offered, (unsigned long long)ok,
           (unsigned long long)bypassed);
    phase_end(name, baseline);
}

/* ---------------- main ------------------------------------ */

int main(void)
{
    static size_t sizes64[2048];
    static size_t sizes_mixed[1024];
    for (size_t i = 0; i < 2048; i++) {
        sizes64[i] = 64;
    }
    static const size_t mix[4] = {32, 64, 128, 512};
    for (size_t i = 0; i < 1024; i++) {
        sizes_mixed[i] = mix[i % 4];
    }
    printf("Z-043 free storm suite (32 threads)\n");
    phase_ring("S1 ring32", sizes64, 2048, 3, 1);
    phase_split();
    phase_flood31();
    phase_owners32();
    phase_ring("S5 ring32 mixed", sizes_mixed, 1024, 1, 1);
    printf("CHECKS: %lu run, %lu failed\n", g_checks, g_failures);
    printf("RESULT: %s\n", g_failures == 0 ? "PASS" : "FAIL");
    return g_failures == 0 ? 0 : 1;
}
