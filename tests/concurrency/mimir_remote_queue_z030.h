/* mimir_remote_queue_z030.h - Z-030 remote-free queue.
 *
 * Slice Z-030 (docs/MIMIRALLOC_Z13_ROADMAP.md): "Cross-thread
 * frees into TLS-owned blocks - Remote-free queue per owner
 * thread with bounded drain; concurrency suite extended for
 * producer/consumer geometries."
 *
 * DESIGN SOURCE AND HONEST POSITION. This queue is designed
 * from the banked Z-028 TLS cache contract
 * (docs/Z13_TLS_CACHE_CONTRACT.md, slice commit 7dfffa9,
 * queued for banking in team-05's worktree at the hour this
 * slice was built): contract S4/TLS-XFREE names the per-owner
 * remote-free queue as Z-030's design, and contract S5 rules
 * that any per-owner queue another thread writes falls under
 * the Z-016 false-sharing law in full - head/tail fields on
 * their own 64-byte lines. Z-029 (the TLS cache
 * implementation this queue will serve) is being built in
 * parallel by team-06 and does NOT exist in this tree: this
 * header invents no Z-029 surface, no Z-029 result, and no
 * claim that the queue is wired into mimir_free. What Z-030
 * proves is the queue itself, in isolation, driven explicitly
 * by the extended concurrency suite
 * (tests/concurrency/test_z030_remote_queue.c): a remote
 * freer enqueues a dead block's pointer instead of freeing
 * it; the owner thread drains bounded batches and performs
 * the real mimir_free itself. Wiring mimir_free's non-owner
 * detection to this queue is Z-029-integration work, recorded
 * as next work in the slice's recorded run.
 *
 * STRUCTURE. Bounded MPSC ring, capacity MRQ_CAPACITY slots:
 * - Producers (any thread) claim the slot at the head index
 *   by CAS, but only after observing that slot free for this
 *   cycle (its sequence stamp equals the head index). A
 *   producer that finds the head slot occupied reports FULL
 *   and keeps ownership of its block: overflow is a counted,
 *   visible outcome, never a silent drop (a dropped pointer
 *   would be a leaked block).
 * - Publication is a release store of the slot's sequence
 *   stamp (index + 1) after the pointer store.
 * - The owner alone advances the tail: it reads a slot only
 *   when the stamp shows published (acquire), copies the
 *   pointer out, and re-stamps the slot free for the next
 *   cycle (stamp = index + MRQ_CAPACITY). Sequence stamps
 *   are monotone cycle counters, so the design is immune to
 *   ABA on slot reuse by construction.
 * - Bounded drain: mrq_drain_batch moves at most
 *   MRQ_DRAIN_BATCH (8, the Z-028 batch constant) pointers
 *   per call. Draining is explicit and polling by design:
 *   there is no wakeup channel, so there is no lost-wakeup
 *   race to test or to have.
 * - Close: mrq_close stops new pushes (counted as closed
 *   rejects; the caller keeps its block). Items already
 *   queued remain drainable by the owner after close, which
 *   is the owner-exit protocol: close first, drain to empty,
 *   then the queue storage may be retired. Retiring storage
 *   with producers still pushing is a caller error, exactly
 *   as freeing any shared structure early would be.
 *
 * FALSE SHARING (Z-016 applied, contract S5): the head
 * index + producer-written counters occupy their own
 * 64-byte line; the tail index + owner-written counters
 * occupy another. Slot sequence stamps are written by one
 * producer and the owner per cycle - the unavoidable MPSC
 * hand-off word, one per slot, 16 bytes apart at most one
 * transfer per block, which is the queue's payload cost,
 * not metadata contention.
 *
 * MEMORY ORDERING. Push: pointer store, then release stamp
 * store; the claim CAS is acq_rel. Drain: acquire stamp
 * load before the pointer load; free-stamp store is release
 * after the pointer copy. No lock is taken anywhere.
 *
 * MUTANT (red-path proof): compiled with MRQ_MUTANT_LOSSY,
 * mrq_push silently drops every 64th successful push (it
 * reports success without publishing). The Z-030 suite must
 * FAIL against that build; a suite that cannot fail proves
 * nothing. The mutant exists only for that proof and is
 * never the shipped build.
 */
#ifndef MIMIR_REMOTE_QUEUE_Z030_H
#define MIMIR_REMOTE_QUEUE_Z030_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Capacity: 256 queued remote frees per owner. Ground: a
 * Z-028 bin holds 16 blocks per class; a remote burst in the
 * banked mixed_batch shape cycles 256 live blocks, so one
 * full workload burst per owner queue fits without overflow;
 * 256 is also small enough that the suite's exact-fill test
 * is cheap and deterministic. A bound chosen for testability
 * and burst coverage, not a claim of optimality. */
#define MRQ_CAPACITY ((size_t)256)
#define MRQ_DRAIN_BATCH ((size_t)8) /* Z-028 batch constant */

enum mrq_push_result {
    MRQ_PUSH_OK = 0,     /* ownership moved to the queue */
    MRQ_PUSH_FULL = 1,   /* queue full; caller keeps block */
    MRQ_PUSH_CLOSED = 2  /* queue closed; caller keeps block */
};

struct mrq_slot {
    _Atomic uint64_t seq; /* cycle stamp: see header notes */
    void *ptr;
};

struct mrq {
    /* Producer line (written by every pushing thread). */
    _Alignas(64) _Atomic uint64_t head;         /* next claim */
    _Atomic uint64_t pushed;                    /* OK pushes */
    _Atomic uint64_t overflow;                  /* FULL pushes */
    _Atomic uint64_t closed_rejects;            /* CLOSED pushes */
    _Atomic int closed;
    char producer_pad[64 - 4 * 8 - 4];
    /* Owner line (written by the owner thread only, except
     * drained/pending reads at quiescent points). */
    _Alignas(64) uint64_t tail;                 /* next drain */
    uint64_t drained;                           /* owner pops */
    char owner_pad[64 - 2 * 8];
    /* Slots. */
    struct mrq_slot slots[MRQ_CAPACITY];
};

typedef struct mrq mrq_t;

static inline void mrq_init(mrq_t *q)
{
    memset(q, 0, sizeof(*q));
    atomic_init(&q->head, 0);
    atomic_init(&q->pushed, 0);
    atomic_init(&q->overflow, 0);
    atomic_init(&q->closed_rejects, 0);
    atomic_init(&q->closed, 0);
    for (size_t i = 0; i < MRQ_CAPACITY; i++) {
        atomic_init(&q->slots[i].seq, (uint64_t)i);
        q->slots[i].ptr = NULL;
    }
    q->tail = 0;
    q->drained = 0;
}

static inline void mrq_close(mrq_t *q)
{
    atomic_store_explicit(&q->closed, 1, memory_order_release);
}

/* Enqueue one dead block's pointer for the owner. On any
 * non-OK result the caller still owns the block and must
 * free it itself (the Z-029 integration road: straight to
 * the central core, counted as a bypass). */
static inline int mrq_push(mrq_t *q, void *ptr)
{
    if (atomic_load_explicit(&q->closed, memory_order_acquire)) {
        atomic_fetch_add_explicit(&q->closed_rejects, 1,
                                  memory_order_relaxed);
        return MRQ_PUSH_CLOSED;
    }
    for (;;) {
        uint64_t head = atomic_load_explicit(&q->head,
                                             memory_order_acquire);
        struct mrq_slot *slot = &q->slots[head & (MRQ_CAPACITY - 1)];
        uint64_t seq = atomic_load_explicit(&slot->seq,
                                            memory_order_acquire);
        if (seq != head) {
            /* Head slot not free for this cycle: the queue is
             * full at the head. Bounded and visible. */
            atomic_fetch_add_explicit(&q->overflow, 1,
                                      memory_order_relaxed);
            return MRQ_PUSH_FULL;
        }
        if (!atomic_compare_exchange_weak_explicit(
                &q->head, &head, head + 1,
                memory_order_acq_rel, memory_order_acquire)) {
            continue; /* another producer claimed; re-observe */
        }
        /* Slot claimed exclusively for cycle `head`. */
#ifdef MRQ_MUTANT_LOSSY
        uint64_t n = atomic_fetch_add_explicit(&q->pushed, 1,
                                               memory_order_relaxed);
        if ((n + 1) % 64 == 0) {
            /* MUTANT: report success, publish nothing. The
             * slot stamp is still advanced past publication
             * state into a never-readable value so the owner
             * can never see this slot; the block is lost. */
            atomic_store_explicit(&slot->seq, head + MRQ_CAPACITY,
                                  memory_order_release);
            return MRQ_PUSH_OK;
        }
#else
        atomic_fetch_add_explicit(&q->pushed, 1,
                                  memory_order_relaxed);
#endif
        slot->ptr = ptr;
        atomic_store_explicit(&slot->seq, head + 1,
                              memory_order_release);
        return MRQ_PUSH_OK;
    }
}

/* Owner-only: move up to `max` (and never more than
 * MRQ_DRAIN_BATCH when max > MRQ_DRAIN_BATCH is passed as
 * the slice's bounded-drain discipline - callers pass the
 * batch they mean; the suite pins the constant) queued
 * pointers into out[], in FIFO order. Returns the count
 * moved. Each returned pointer is the owner's to free. */
static inline size_t mrq_drain_batch(mrq_t *q, void **out, size_t max)
{
    size_t n = 0;
    while (n < max) {
        uint64_t tail = q->tail;
        struct mrq_slot *slot = &q->slots[tail & (MRQ_CAPACITY - 1)];
        uint64_t seq = atomic_load_explicit(&slot->seq,
                                            memory_order_acquire);
        if (seq != tail + 1) {
            break; /* nothing published at the tail */
        }
        out[n++] = slot->ptr;
        slot->ptr = NULL;
        atomic_store_explicit(&slot->seq, tail + MRQ_CAPACITY,
                              memory_order_release);
        q->tail = tail + 1;
        q->drained++;
    }
    return n;
}

/* Quiescent-point accounting (exact only when no push or
 * drain is in flight - the suite reads it after joins). */
struct mrq_stats {
    uint64_t pushed;
    uint64_t overflow;
    uint64_t closed_rejects;
    uint64_t drained;
    uint64_t pending; /* pushed - drained, at quiescence */
};

static inline void mrq_snapshot(const mrq_t *q, struct mrq_stats *out)
{
    out->pushed = atomic_load_explicit(&q->pushed,
                                       memory_order_acquire);
    out->overflow = atomic_load_explicit(&q->overflow,
                                         memory_order_acquire);
    out->closed_rejects =
        atomic_load_explicit(&q->closed_rejects, memory_order_acquire);
    out->drained = q->drained;
    out->pending = out->pushed - out->drained;
}

#endif /* MIMIR_REMOTE_QUEUE_Z030_H */
