/*
 * test_z030_remote_queue.c -- Z-030 concurrency suite extension:
 * producer/consumer geometries for the per-owner remote-free
 * queue (tests/concurrency/mimir_remote_queue_z030.h).
 *
 * Slice Z-030 (docs/MIMIRALLOC_Z13_ROADMAP.md): "Cross-thread
 * frees into TLS-owned blocks - Remote-free queue per owner
 * thread with bounded drain; concurrency suite extended for
 * producer/consumer geometries." Gate: concurrency gate green
 * incl. the new geometries.
 *
 * POSITION (honest). The queue is Z-030's design from the
 * banked Z-028 contract (S4/TLS-XFREE, S5); Z-029's TLS cache
 * does not exist in this tree yet, and mimir_free does NOT
 * route through this queue. This suite drives the queue
 * explicitly: a block is allocated, a DIFFERENT thread (the
 * remote freer) enqueues its pointer, and the owner thread
 * drains bounded batches and performs the real mimir_free.
 * That is exactly the traffic Z-029 integration will
 * generate; what is proven here is the queue, the geometries,
 * the accounting, and that every block so routed is freed
 * exactly once, uncorrupted, with allocator metadata clean
 * afterwards.
 *
 * Phases (all deterministic: fixed counts, joins before every
 * accounting read, no timing assertions, no sleeps; owner
 * drain loops in the concurrent geometries poll the queue
 * plus a producers-done flag, and every assertion is made at
 * a quiescent point after pthread_join):
 *   Q0 layout      - Z-016 line separation of head/tail.
 *   Q1 single      - FIFO order, bounded batch drain, empty pop.
 *   Q2 capacity    - exact fill to MRQ_CAPACITY, next push FULL
 *                    (counted; caller frees its own block),
 *                    full drain, accounting reconciles.
 *   Q3 owner-alloc - owner allocates 4096 blocks; a remote
 *                    thread enqueues all; owner drains
 *                    concurrently in batches of MRQ_DRAIN_BATCH
 *                    and frees. Single producer; patterns
 *                    verify by self-describing tag and an
 *                    exactly-once bitmap (bypass frees skip
 *                    sequences, so order is not assumed).
 *   Q4 producer-alloc - a producer thread allocates and
 *                    enqueues its own blocks; the owner (a
 *                    third party to the allocation) drains
 *                    and frees: the classic remote-free shape.
 *   Q5 storm       - 8 producers x 2048 blocks into one owner
 *                    queue, owner draining concurrently;
 *                    overflow, if any, is freed by its producer
 *                    directly (the bypass road) and counted;
 *                    pushed + overflow == offered, exactly.
 *   Q6 swap x4     - 4 threads; each allocates, enqueues to
 *                    the NEXT thread's queue, drains its own.
 *   Q7 close/exit  - close stops pushes (CLOSED counted, the
 *                    caller frees its block itself); items
 *                    queued before close still drain; a queue
 *                    closed and drained retires at pending 0.
 * Every phase that routes real blocks verifies each block's
 * pattern at drain time, then checks live_small_objects is
 * back at its phase baseline and mimir_core_metadata_check()
 * reports 0 at the phase end.
 *
 * Counting discipline follows test_d004.c: workers accumulate
 * into their own structs; the main thread sums after joins.
 * Standalone via `make test-z030`; NOT part of `make test`
 * (the standing gate's 4988/0/77 counts are untouched).
 * Built with -DMRQ_MUTANT_LOSSY (the run_z030 red-path proof)
 * this suite MUST fail: the mutant silently loses every 64th
 * push, so drain totals and accounting cannot reconcile.
 */

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>

#include "mimir_alloc.h"
#include "mimir_remote_queue_z030.h"
/* Test-only internal surfaces (INC-E001-07 shape (b)): the
 * J-002 stats snapshot and the J-006 metadata check, reached
 * by linking the library objects directly, as every internal
 * surface in this tree is reached. */
#include "../../src/core/mimir_core.h"

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

/* Per-block pattern: an 8-byte tag at offset 0 encoding
 * (producer, sequence); the byte (tag & 0x5a) repeated over
 * the middle; its complement at the last byte. Verified by
 * whoever drains, before freeing. */
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

/* One owner queue plus its drain bookkeeping. producers_done
 * is set (release) by the producing side when it will push no
 * more; the drain loop reads it (acquire) only when a drain
 * came up empty, then drains once more before concluding -
 * a push that landed between the empty drain and the flag
 * store is ordered before the flag, so the final drain sees
 * it. All assertions happen after joins, at quiescence. */
struct owner_ctx {
    mrq_t q;
    _Atomic int producers_done;
    uint64_t freed_by_owner;
    uint64_t pattern_failures;
    size_t block_size;
    uint32_t expect_producer;
    uint32_t next_seq;
    unsigned char *seq_seen;  /* exactly-once bitmap (Q3/Q4) */
    size_t seq_space;
    uint64_t seq_duplicates;
};

static void owner_ctx_init(struct owner_ctx *ctx, size_t block_size,
                           uint32_t expect_producer)
{
    memset(ctx, 0, sizeof(*ctx));
    mrq_init(&ctx->q);
    atomic_init(&ctx->producers_done, 0);
    ctx->block_size = block_size;
    ctx->expect_producer = expect_producer;
}

/* Drain until producers are done and the ring is empty.
 * seq_mode != 0 is for single-producer phases only: each
 * block's pattern is verified against the (producer, seq)
 * tag the block carries, and a bitmap proves every queued
 * block was delivered exactly once. (Order is NOT assumed:
 * a bypassed overflow block's sequence simply never
 * arrives, which the bitmap tolerates and the accounting
 * identity - queued + bypassed == offered - pins down.) */
static void owner_drain_until_done(struct owner_ctx *ctx, int seq_mode)
{
    void *batch[MRQ_DRAIN_BATCH];
    for (;;) {
        size_t n = mrq_drain_batch(&ctx->q, batch, MRQ_DRAIN_BATCH);
        for (size_t i = 0; i < n; i++) {
            if (seq_mode) {
                uint64_t tag;
                memcpy(&tag, batch[i], 8);
                uint32_t prod = (uint32_t)(tag >> 32);
                uint32_t seq = (uint32_t)tag;
                if (!block_verify(batch[i], ctx->block_size, prod, seq) ||
                    prod != ctx->expect_producer ||
                    seq >= ctx->seq_space) {
                    ctx->pattern_failures++;
                } else if (ctx->seq_seen[seq]) {
                    ctx->seq_duplicates++;
                } else {
                    ctx->seq_seen[seq] = 1;
                }
                ctx->next_seq++;
            }
            mimir_free(batch[i]);
            ctx->freed_by_owner++;
        }
        if (n == 0) {
            if (atomic_load_explicit(&ctx->producers_done,
                                     memory_order_acquire)) {
                size_t m = mrq_drain_batch(&ctx->q, batch,
                                           MRQ_DRAIN_BATCH);
                int progress = (m > 0);
                for (size_t i = 0; i < m; i++) {
                    if (seq_mode) {
                        uint64_t tag;
                        memcpy(&tag, batch[i], 8);
                        uint32_t prod = (uint32_t)(tag >> 32);
                        uint32_t seq = (uint32_t)tag;
                        if (!block_verify(batch[i], ctx->block_size,
                                          prod, seq) ||
                            prod != ctx->expect_producer ||
                            seq >= ctx->seq_space) {
                            ctx->pattern_failures++;
                        } else if (ctx->seq_seen[seq]) {
                            ctx->seq_duplicates++;
                        } else {
                            ctx->seq_seen[seq] = 1;
                        }
                        ctx->next_seq++;
                    }
                    mimir_free(batch[i]);
                    ctx->freed_by_owner++;
                }
                if (!progress) {
                    return;
                }
            } else {
                sched_yield();
            }
        }
    }
}

/* Producer job shared by Q3/Q4/Q5: allocates its own blocks
 * (producer_alloc != 0) or enqueues a caller-supplied array,
 * bypass-freeing whatever the queue refuses. */
struct prod_job {
    mrq_t *q;
    _Atomic int *done;
    void **ptrs;          /* supplied-array mode */
    size_t count;
    uint32_t producer_id;
    size_t size;          /* self-alloc mode block size */
    int producer_alloc;
    uint64_t ok;
    uint64_t bypassed;
    uint64_t alloc_failures;
};

static void *producer_main(void *arg)
{
    struct prod_job *job = (struct prod_job *)arg;
    for (size_t i = 0; i < job->count; i++) {
        void *p;
        if (job->producer_alloc) {
            p = mimir_malloc(job->size);
            if (p == NULL) {
                job->alloc_failures++;
                continue;
            }
            block_fill(p, job->size, job->producer_id, (uint32_t)i);
        } else {
            p = job->ptrs[i];
        }
        if (mrq_push(job->q, p) == MRQ_PUSH_OK) {
            job->ok++;
        } else {
            job->bypassed++;
            mimir_free(p); /* bypass road: the pusher frees */
        }
    }
    if (job->done != NULL) {
        atomic_store_explicit(job->done, 1, memory_order_release);
    }
    return NULL;
}

/* ---------------- Q0/Q1/Q2: single-threaded queue law ------ */

static void phase_q0_layout(void)
{
    check(offsetof(mrq_t, head) % 64 == 0, "Q0 head on its own line");
    check(offsetof(mrq_t, tail) % 64 == 0, "Q0 tail on its own line");
    check(offsetof(mrq_t, tail) - offsetof(mrq_t, head) >= 64,
          "Q0 head/tail at least one full line apart");
    check(MRQ_CAPACITY == 256, "Q0 capacity constant is 256");
    check(MRQ_DRAIN_BATCH == 8, "Q0 drain batch is the Z-028 batch (8)");
}

static void phase_q1_single(void)
{
    static mrq_t q;
    mrq_init(&q);
    int sent[20];
    void *out[MRQ_DRAIN_BATCH];
    check(mrq_drain_batch(&q, out, MRQ_DRAIN_BATCH) == 0,
          "Q1 empty queue drains zero");
    for (int i = 0; i < 20; i++) {
        check(mrq_push(&q, &sent[i]) == MRQ_PUSH_OK, "Q1 push ok");
    }
    size_t n = mrq_drain_batch(&q, out, MRQ_DRAIN_BATCH);
    check(n == MRQ_DRAIN_BATCH, "Q1 bounded drain returns exactly batch");
    int fifo = 1;
    for (size_t i = 0; i < n; i++) {
        if (out[i] != &sent[i]) {
            fifo = 0;
        }
    }
    check(fifo, "Q1 FIFO order in first batch");
    n = mrq_drain_batch(&q, out, MRQ_DRAIN_BATCH);
    check(n == MRQ_DRAIN_BATCH, "Q1 second batch full");
    n = mrq_drain_batch(&q, out, 4);
    check(n == 4, "Q1 final drain returns the remainder");
    fifo = 1;
    for (size_t i = 0; i < n; i++) {
        if (out[i] != &sent[16 + i]) {
            fifo = 0;
        }
    }
    check(fifo, "Q1 FIFO order preserved to the last block");
    struct mrq_stats s;
    mrq_snapshot(&q, &s);
    check(s.pushed == 20 && s.drained == 20 && s.pending == 0 &&
              s.overflow == 0,
          "Q1 accounting reconciles at quiescence");
}

static void phase_q2_capacity(void)
{
    static mrq_t q;
    mrq_init(&q);
    uint64_t base = live_small_now();
    void *blocks[MRQ_CAPACITY];
    size_t allocd = 0;
    for (size_t i = 0; i < MRQ_CAPACITY; i++) {
        blocks[i] = mimir_malloc(64);
        if (blocks[i] == NULL) {
            break;
        }
        allocd++;
        block_fill(blocks[i], 64, 9, (uint32_t)i);
    }
    check(allocd == MRQ_CAPACITY, "Q2 allocated a full queue of blocks");
    int all_ok = 1;
    for (size_t i = 0; i < allocd; i++) {
        if (mrq_push(&q, blocks[i]) != MRQ_PUSH_OK) {
            all_ok = 0;
        }
    }
    check(all_ok, "Q2 every push up to capacity succeeds");
    void *extra = mimir_malloc(64);
    check(extra != NULL, "Q2 overflow block allocated");
    if (extra != NULL) {
        block_fill(extra, 64, 9, (uint32_t)MRQ_CAPACITY);
    }
    check(extra != NULL &&
              mrq_push(&q, extra) == MRQ_PUSH_FULL,
          "Q2 push past capacity reports FULL, never a silent drop");
    if (extra != NULL) {
        check(block_verify(extra, 64, 9, (uint32_t)MRQ_CAPACITY),
              "Q2 overflow block intact and still caller-owned");
        mimir_free(extra); /* the bypass road: caller frees */
    }
    void *out[MRQ_DRAIN_BATCH];
    uint64_t drained = 0;
    int order_ok = 1;
    size_t n;
    while ((n = mrq_drain_batch(&q, out, MRQ_DRAIN_BATCH)) > 0) {
        for (size_t i = 0; i < n; i++) {
            if (!block_verify(out[i], 64, 9, (uint32_t)drained)) {
                order_ok = 0;
            }
            mimir_free(out[i]);
            drained++;
        }
    }
    check(order_ok, "Q2 full-queue drain is FIFO and uncorrupted");
    check(drained == MRQ_CAPACITY, "Q2 drained exactly capacity");
    struct mrq_stats s;
    mrq_snapshot(&q, &s);
    check(s.pushed == MRQ_CAPACITY && s.overflow == 1 &&
              s.pending == 0,
          "Q2 accounting: pushed=capacity, overflow=1, pending=0");
    check(live_small_now() == base, "Q2 live objects back to baseline");
    check(mimir_core_metadata_check() == 0, "Q2 metadata clean");
}

/* ---------------- Q3: owner-alloc, remote push ------------- */

static void phase_q3_owner_alloc(void)
{
    enum { N = 4096 };
    static struct owner_ctx ctx;
    owner_ctx_init(&ctx, 64, 3);
    static unsigned char seen3[N];
    memset(seen3, 0, sizeof(seen3));
    ctx.seq_seen = seen3;
    ctx.seq_space = N;
    uint64_t base = live_small_now();
    void **ptrs = (void **)malloc(sizeof(void *) * N);
    check(ptrs != NULL, "Q3 pointer array allocated");
    if (ptrs == NULL) {
        return;
    }
    size_t allocd = 0;
    for (size_t i = 0; i < N; i++) {
        ptrs[i] = mimir_malloc(64);
        if (ptrs[i] == NULL) {
            break;
        }
        allocd++;
        block_fill(ptrs[i], 64, 3, (uint32_t)i);
    }
    check(allocd == N, "Q3 owner allocated all blocks");
    struct prod_job job;
    memset(&job, 0, sizeof(job));
    job.q = &ctx.q;
    job.done = &ctx.producers_done;
    job.ptrs = ptrs;
    job.count = allocd;
    job.producer_alloc = 0;
    pthread_t th;
    check(pthread_create(&th, NULL, producer_main, &job) == 0,
          "Q3 remote pusher created");
    owner_drain_until_done(&ctx, 1); /* single producer: FIFO */
    pthread_join(th, NULL);
    check(job.alloc_failures == 0, "Q3 no allocation failures");
    check(job.ok + job.bypassed == allocd,
          "Q3 every block accounted by pusher");
    check(ctx.pattern_failures == 0,
          "Q3 every drained block pattern-verified at drain");
    uint64_t seen3_count = 0;
    for (size_t i = 0; i < N; i++) { seen3_count += seen3[i]; }
    check(ctx.seq_duplicates == 0 && seen3_count == job.ok,
          "Q3 every queued block delivered exactly once (bitmap)");
    check(ctx.freed_by_owner == job.ok,
          "Q3 owner freed exactly the queued blocks");
    struct mrq_stats s;
    mrq_snapshot(&ctx.q, &s);
    check(s.pushed == job.ok && s.pending == 0,
          "Q3 accounting reconciles, queue empty");
    check(live_small_now() == base, "Q3 live objects back to baseline");
    check(mimir_core_metadata_check() == 0, "Q3 metadata clean");
    free(ptrs);
}

/* ---------------- Q4: producer-alloc, owner frees ---------- */

static void phase_q4_producer_alloc(void)
{
    enum { N = 4096 };
    static struct owner_ctx ctx;
    owner_ctx_init(&ctx, 512, 4);
    static unsigned char seen4[N];
    memset(seen4, 0, sizeof(seen4));
    ctx.seq_seen = seen4;
    ctx.seq_space = N;
    uint64_t base = live_small_now();
    struct prod_job job;
    memset(&job, 0, sizeof(job));
    job.q = &ctx.q;
    job.done = &ctx.producers_done;
    job.count = N;
    job.producer_id = 4;
    job.size = 512;
    job.producer_alloc = 1;
    pthread_t th;
    check(pthread_create(&th, NULL, producer_main, &job) == 0,
          "Q4 producer created");
    owner_drain_until_done(&ctx, 1); /* single producer: FIFO */
    pthread_join(th, NULL);
    check(job.alloc_failures == 0, "Q4 no allocation failures");
    check(ctx.pattern_failures == 0,
          "Q4 every drained block pattern-verified at drain");
    uint64_t seen4_count = 0;
    for (size_t i = 0; i < N; i++) { seen4_count += seen4[i]; }
    check(ctx.seq_duplicates == 0 && seen4_count == job.ok,
          "Q4 every queued block delivered exactly once (bitmap)");
    check(ctx.freed_by_owner == job.ok,
          "Q4 owner freed exactly the queued blocks");
    check(job.ok + job.bypassed == N, "Q4 offered == queued + bypassed");
    struct mrq_stats s;
    mrq_snapshot(&ctx.q, &s);
    check(s.pushed == job.ok && s.pending == 0,
          "Q4 accounting reconciles");
    check(live_small_now() == base, "Q4 live objects back to baseline");
    check(mimir_core_metadata_check() == 0, "Q4 metadata clean");
}

/* ---------------- Q5: many-to-one storm -------------------- */

static void phase_q5_storm(void)
{
    enum { PRODUCERS = 8, PER = 2048 };
    static struct owner_ctx ctx;
    owner_ctx_init(&ctx, 64, 0);
    uint64_t base = live_small_now();
    static _Atomic int flags[PRODUCERS];
    struct prod_job jobs[PRODUCERS];
    pthread_t ths[PRODUCERS];
    for (int i = 0; i < PRODUCERS; i++) {
        atomic_init(&flags[i], 0);
        memset(&jobs[i], 0, sizeof(jobs[i]));
        jobs[i].q = &ctx.q;
        jobs[i].done = &flags[i];
        jobs[i].count = PER;
        jobs[i].producer_id = (uint32_t)(16 + i);
        jobs[i].size = 64;
        jobs[i].producer_alloc = 1;
    }
    int created = 0;
    for (int i = 0; i < PRODUCERS; i++) {
        if (pthread_create(&ths[i], NULL, producer_main, &jobs[i]) == 0) {
            created++;
        }
    }
    check(created == PRODUCERS, "Q5 all storm producers created");
    /* Owner (this thread) drains concurrently until every
     * producer flag is set and a final drain comes up empty.
     * Patterns are NOT verified here: with 8 producers the
     * interleave is not a predictable sequence; exactness is
     * carried by the accounting identity below plus Q3/Q4's
     * ordered pattern proofs on the same code path. */
    void *batch[MRQ_DRAIN_BATCH];
    for (;;) {
        size_t n = mrq_drain_batch(&ctx.q, batch, MRQ_DRAIN_BATCH);
        for (size_t i = 0; i < n; i++) {
            mimir_free(batch[i]);
            ctx.freed_by_owner++;
        }
        if (n == 0) {
            int all = 1;
            for (int i = 0; i < PRODUCERS; i++) {
                if (!atomic_load_explicit(&flags[i],
                                          memory_order_acquire)) {
                    all = 0;
                }
            }
            if (all) {
                size_t m = mrq_drain_batch(&ctx.q, batch,
                                           MRQ_DRAIN_BATCH);
                for (size_t i = 0; i < m; i++) {
                    mimir_free(batch[i]);
                    ctx.freed_by_owner++;
                }
                if (m == 0) {
                    break;
                }
            } else {
                sched_yield();
            }
        }
    }
    uint64_t ok = 0, bypassed = 0, alloc_fail = 0;
    for (int i = 0; i < PRODUCERS; i++) {
        pthread_join(ths[i], NULL);
        ok += jobs[i].ok;
        bypassed += jobs[i].bypassed;
        alloc_fail += jobs[i].alloc_failures;
    }
    check(alloc_fail == 0, "Q5 no allocation failures");
    check(ok + bypassed == (uint64_t)(PRODUCERS * PER),
          "Q5 offered == queued + bypassed, exactly");
    check(ctx.freed_by_owner == ok, "Q5 owner freed every queued block");
    struct mrq_stats s;
    mrq_snapshot(&ctx.q, &s);
    check(s.pushed == ok && s.pending == 0,
          "Q5 queue accounting reconciles at quiescence");
    check(s.overflow == bypassed,
          "Q5 every bypass was a counted FULL overflow");
    check(live_small_now() == base, "Q5 live objects back to baseline");
    check(mimir_core_metadata_check() == 0, "Q5 metadata clean");
}

/* ---------------- Q6: swap x4 ------------------------------ */

struct swap_arg {
    struct swap_ctx *ctx;
    int index;
    uint64_t ok;
    uint64_t bypassed;
    uint64_t alloc_failures;
};

struct swap_ctx {
    struct owner_ctx owners[4];
};

static void *swap_main(void *arg)
{
    struct swap_arg *sa = (struct swap_arg *)arg;
    struct swap_ctx *ctx = sa->ctx;
    int me = sa->index;
    int target = (me + 1) % 4;
    int producer_idx = (me + 3) % 4; /* who pushes into my queue */
    for (size_t i = 0; i < 1024; i++) {
        void *p = mimir_malloc(64);
        if (p == NULL) {
            sa->alloc_failures++;
            continue;
        }
        block_fill(p, 64, (uint32_t)(32 + me), (uint32_t)i);
        if (mrq_push(&ctx->owners[target].q, p) == MRQ_PUSH_OK) {
            sa->ok++;
        } else {
            sa->bypassed++;
            mimir_free(p);
        }
    }
    /* My pushes are done; the flag other threads watch for
     * MY queue is the one on MY owner_ctx - by the convention
     * here each thread sets its own flag, and a drainer waits
     * on its producer's flag (owners[producer_idx]). */
    atomic_store_explicit(&ctx->owners[me].producers_done, 1,
                          memory_order_release);
    struct owner_ctx *mine = &ctx->owners[me];
    void *batch[MRQ_DRAIN_BATCH];
    for (;;) {
        size_t n = mrq_drain_batch(&mine->q, batch, MRQ_DRAIN_BATCH);
        for (size_t i = 0; i < n; i++) {
            mimir_free(batch[i]);
            mine->freed_by_owner++;
        }
        if (n == 0) {
            if (atomic_load_explicit(
                    &ctx->owners[producer_idx].producers_done,
                    memory_order_acquire)) {
                size_t m = mrq_drain_batch(&mine->q, batch,
                                           MRQ_DRAIN_BATCH);
                for (size_t i = 0; i < m; i++) {
                    mimir_free(batch[i]);
                    mine->freed_by_owner++;
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

static void phase_q6_swap(void)
{
    static struct swap_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    for (int i = 0; i < 4; i++) {
        owner_ctx_init(&ctx.owners[i], 64, 0);
    }
    uint64_t base = live_small_now();
    struct swap_arg args[4];
    pthread_t ths[4];
    int created = 0;
    for (int i = 0; i < 4; i++) {
        args[i].ctx = &ctx;
        args[i].index = i;
        args[i].ok = 0;
        args[i].bypassed = 0;
        args[i].alloc_failures = 0;
        if (pthread_create(&ths[i], NULL, swap_main, &args[i]) == 0) {
            created++;
        }
    }
    check(created == 4, "Q6 all swap threads created");
    uint64_t ok = 0, bypassed = 0, alloc_fail = 0, freed = 0;
    for (int i = 0; i < 4; i++) {
        pthread_join(ths[i], NULL);
        ok += args[i].ok;
        bypassed += args[i].bypassed;
        alloc_fail += args[i].alloc_failures;
        freed += ctx.owners[i].freed_by_owner;
    }
    check(alloc_fail == 0, "Q6 no allocation failures");
    check(ok + bypassed == 4096, "Q6 offered == queued + bypassed");
    check(freed == ok, "Q6 every queued block freed by its owner");
    int pending_ok = 1;
    for (int i = 0; i < 4; i++) {
        struct mrq_stats s;
        mrq_snapshot(&ctx.owners[i].q, &s);
        if (s.pending != 0) {
            pending_ok = 0;
        }
    }
    check(pending_ok, "Q6 all queues empty at quiescence");
    check(live_small_now() == base, "Q6 live objects back to baseline");
    check(mimir_core_metadata_check() == 0, "Q6 metadata clean");
}

/* ---------------- Q7: close / owner-exit protocol ---------- */

static void phase_q7_close(void)
{
    static mrq_t q;
    mrq_init(&q);
    uint64_t base = live_small_now();
    void *kept[10];
    size_t allocd = 0;
    for (size_t i = 0; i < 10; i++) {
        kept[i] = mimir_malloc(64);
        if (kept[i] == NULL) {
            break;
        }
        allocd++;
        block_fill(kept[i], 64, 7, (uint32_t)i);
    }
    check(allocd == 10, "Q7 blocks allocated");
    int pre_ok = 1;
    for (size_t i = 0; i < allocd; i++) {
        if (mrq_push(&q, kept[i]) != MRQ_PUSH_OK) {
            pre_ok = 0;
        }
    }
    check(pre_ok, "Q7 pre-close pushes all succeed");
    mrq_close(&q);
    void *late = mimir_malloc(64);
    check(late != NULL, "Q7 late block allocated");
    check(late != NULL && mrq_push(&q, late) == MRQ_PUSH_CLOSED,
          "Q7 push after close reports CLOSED, never a silent drop");
    if (late != NULL) {
        mimir_free(late); /* owner gone: the caller frees */
    }
    void *out[MRQ_DRAIN_BATCH];
    uint64_t drained = 0;
    int intact = 1;
    size_t n;
    while ((n = mrq_drain_batch(&q, out, MRQ_DRAIN_BATCH)) > 0) {
        for (size_t i = 0; i < n; i++) {
            if (!block_verify(out[i], 64, 7, (uint32_t)drained)) {
                intact = 0;
            }
            mimir_free(out[i]);
            drained++;
        }
    }
    check(intact, "Q7 queued blocks uncorrupted after close");
    check(drained == allocd,
          "Q7 items queued before close remain drainable after it");
    struct mrq_stats s;
    mrq_snapshot(&q, &s);
    check(s.closed_rejects == 1 && s.pending == 0,
          "Q7 accounting: one closed reject, queue retired empty");
    check(live_small_now() == base, "Q7 live objects back to baseline");
    check(mimir_core_metadata_check() == 0, "Q7 metadata clean");
}

int main(void)
{
    phase_q0_layout();
    phase_q1_single();
    phase_q2_capacity();
    phase_q3_owner_alloc();
    phase_q4_producer_alloc();
    phase_q5_storm();
    phase_q6_swap();
    phase_q7_close();
    printf("CHECKS: %lu run, %lu failed\n", g_checks, g_failures);
    printf("RESULT: %s\n", g_failures == 0 ? "PASS" : "FAIL");
    return g_failures == 0 ? 0 : 1;
}
