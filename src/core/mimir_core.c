/*
 * mimir_core.c -- MimirAlloc Phase 3 minimal allocator core.
 *
 * Slice C-003. This is the roadmap's explicitly experimental
 * mapping-per-allocation correctness baseline (Roadmap Phase 3;
 * B-006 S4, L5 degenerate form): every allocation is backed by
 * its own OS mapping. It is NOT a performance allocator: there
 * is no reuse, no splitting, no coalescing, no size classes,
 * no free lists and no caching (B-006 S8.5). A later allocation
 * is a fresh mapping or a failure.
 *
 * Clean-room: implemented only from the banked MimirAlloc
 * design documents (B-006, B-007, B-008) and the C-001/C-002
 * code in this repository. No existing allocator's code was
 * consulted or copied.
 *
 * Layer form (B-006 S4, collapsed Phase 3):
 *   L1 public boundary  -- src/api/mimir_alloc.c (thin forward)
 *   L2 admission        -- checked arithmetic below, before any
 *                          mapping request (B-007 Section A)
 *   L3 fast path        -- pass-through only; nothing here
 *   L4 central ownership-- registry discipline, counters, and
 *                          lock discipline (this file)
 *   L5 page/span manager-- descriptor layout and mapping layout
 *                          (this file); sole policy client of L6
 *   L6 OS abstraction   -- src/platform/ (C-002); consumed only
 *                          through mimir_platform.h
 *   L7 observability    -- compiled out; nothing here
 *
 * Metadata (B-006 S7.1 Option A, CHOSEN): each mapping carries
 * its own 96-byte descriptor (B-006 S7.3) in a reserve page of
 * exactly one system page at the start of the same mapping:
 *
 *   [ reserve page | usable region ...................... ]
 *   ^ mapping_base   ^ mapping_base + usable_offset = user ptr
 *
 * The descriptor lives exactly as long as its mapping and is
 * never accessed after the munmap that destroys both
 * (Invariant 2). Registry anchors and counters are fixed
 * static storage below L1 (B-006 S7.2); per-allocation metadata
 * is exclusively the in-mapping descriptor.
 *
 * Total mapping size (B-006 S6 / S7.1 / S11.1 step 5): the two
 * textual forms recorded as INC-B007-A1 coincide here because
 * usable_offset is exactly one whole system page, so
 * page_round_up(usable_offset + requested) equals
 * usable_offset + page_round_up(requested). This file computes
 * the checked sum usable_offset + requested (B-007 A3/A4) and
 * lets L6 apply its checked page round-up (B-007 A5/A6, inside
 * C-002's acquire). Recorded as INC-C003-01 in tests/core/
 * NOTES.md; the in-doc reconciliation of B-006 remains owed
 * by a design revision, not by this code.
 *
 * Lock (B-006 S9): the one simple global lock is consumed from
 * L6 (mimir_platform_global_lock/unlock); it guards only
 * allocator metadata (descriptors, registry, counters), never
 * user bytes (S9.2), and IS held across the mapping acquire /
 * release calls (S9.4 DECISION), so mapping, publication and
 * registry insertion are one locked step. It is never held
 * across a return to the caller. Lock failure: the operation
 * fails with no ownership passing and no unlocked progress
 * (S9.5 / F-07) -- malloc returns NULL, free leaves the block
 * LIVE and recorded as owned.
 *
 * Errors (B-007 D.0): the only caller-observable failure
 * channel of mimir_malloc is the NULL return. errno is NOT
 * promised (U-10). No error is reported while the lock is
 * held; this baseline reports nothing at all (L7 compiled out).
 *
 * Contents (B-007 C1): bytes of a successful allocation are
 * unspecified and NOT zero-guaranteed; the allocator never
 * reads or writes user bytes. That fresh anonymous mappings
 * happen to read as zero is incidental (C1.3), never promised.
 *
 * Initialization (B-007 C2 / B-006 F-08): clean by
 * construction -- registry anchors and counters are static
 * storage in their defined starting state (empty registry,
 * zero counters) and the L6 lock is statically initialised.
 * No allocation, no constructor, no init step is needed or
 * performed before the first call.
 *
 * Zero-size (U-09 SETTLED by Z-078, decision D-013): B-006
 * F-05 and B-007 A1/A4 gave mimir_malloc(0) no behaviour
 * and B-008 prescribed none; the implementation's NULL
 * answer, continuous with the C-001 stub's posture, is
 * adopted by D-013 as this target's contract: size == 0
 * is a defined, non-failure refusal (exactly NULL,
 * counted as a posture null, never a failure), stated in
 * the public header and proven by tests/abi/test_z078.c.
 *
 * C-004 addendum (B-007 completeness audit): two literal gaps
 * in this file were closed -- the B-007 A9 pointer-range check
 * is now an explicit code check before publication (it was
 * construction-only in C-003), and the B-007 A10 bookkeeping
 * check now also covers the live_mappings increment (C-003
 * checked only live_bytes). No other behaviour changed. The
 * full audit is tests/core/AUDIT_B007_C004.md.
 *
 * C-005 addendum (calloc/realloc per B-008): the C-003 alloc
 * body is refactored, behaviour unchanged, into a lock-held
 * construction helper (alloc_locked) shared by malloc,
 * calloc and realloc's move; registry owner determination
 * and the retire-exactly-once step (with its F-06 rollback)
 * are likewise shared helpers instead of free-private code.
 * calloc adds the checked nmemb*size product and the
 * explicit usable-extent zeroing step of B-008 S3; realloc
 * adds the B-008 S4 cases (NULL / zero posture / same-size /
 * shrink-in-place bookkeeping / grow-by-move with the
 * original preserved on any failure). Zero cases remain
 * contract since Z-078 (D-013), behaviour unchanged --
 * they began as postures only (U-09 / U-12 then
 * unresolved). The slice record is
 * tests/core/NOTES_C005.md.
 *
 * G-003 addendum (Phase 7 small-object path, per the
 * banked G-002 design): requests of 1..1024 bytes are
 * served from size-class spans (one system page each,
 * 96-byte header) with bounded reuse; requests above
 * the cutoff keep this file's mapping-per-allocation
 * path unchanged (G-002 R-1). The span machinery, its
 * header assignment (INC-G002-04), the two-registry
 * free path (R-4) and the span/object counters (R-5)
 * live in the G-003 block below, before
 * mimir_core_alloc. The slice record is
 * docs/PHASE_7_SMALL_OBJECT_PATH_G003.md.
 *
 * H-001 addendum (Phase 7 unload drain, commissioned fix
 * for INC-G008-02): this file now registers ONE image
 * destructor, which calls the internal drain entry point
 * mimir_core_drain_retained: under the global lock it
 * releases every retained EMPTY span whole through
 * span_release_locked / L6, so a dlclose no longer
 * orphans retained spans. Live blocks are deliberately
 * untouched (INC-G008-03 posture: consumer duty, see
 * docs/PHASE_7_UNLOAD_DRAIN_H001.md). The drain is
 * non-terminal and idempotent; no public symbol is
 * added and no constructor is registered --
 * initialisation remains clean by construction (above).
 *
 * J-005 addendum (Phase 10 debug-only poisoning +
 * quarantine of freed blocks): under MIMIR_DEBUG only,
 * a small-path free poisons the block's whole class
 * extent (byte 0xDD) and holds the block in a bounded
 * FIFO quarantine (capacity 32) inside its span before
 * reuse; verification happens at quarantine eviction
 * (the only road back to reuse) and on demand. The
 * production build contains none of this code. The
 * slice record is docs/PHASE_10_POISON_QUARANTINE.md.
 *
 * J-007 addendum (Phase 10 allocation-free diagnostics):
 * the core now owns ONE error-reporting channel,
 * mimir_core_diag_report (declared in mimir_core.h),
 * wired to the genuine failure NULL paths of the three
 * allocating entry points. Fixed stack buffer, fixed name
 * tables, write(2) to fd 2 -- the J-004/J-005 report shape
 * generalised from debug-only detection to production
 * error reporting (J-001 S5.4). It allocates nothing,
 * takes no lock, calls no L6 entry and cannot re-enter
 * itself (counted guard). Posture NULLs stay silent.
 */

#include "mimir_core.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h> /* J-007: getenv for the channel's
                       * enable gate (INC-J007-01). */
#include <stdio.h>  /* Z-050: fopen/fread for the live
                       * /proc/pressure/memory read only. */
#include <time.h>   /* Z-050: clock_gettime(CLOCK_MONOTONIC)
                       * for the retained-pool idle clock. */
#include <threads.h>
#include <errno.h> /* Z-074: EINVAL/ENOMEM return values of the posix_memalign-shaped core entry (ISO C header; the values, not the errno variable, are the channel) */
#include <unistd.h> /* J-007: write(2) / STDERR_FILENO for
                       * the diagnostic channel only (the
                       * INC-J005-05 layer deviation, now the
                       * channel's documented home). */

#ifdef MIMIR_DEBUG
/* J-004 debug-only reporting channel (see the J-004 block
 * below): write(2) + abort() are named HERE, in debug
 * builds only, because no L6 reporting primitive exists
 * yet -- J-007 owns the production reporting channel and
 * lifting this into L6 is recorded as INC-J004-03.
 * Production builds do not include these headers at all. */
#include <stdlib.h>
#include <unistd.h>
#endif

#include "../platform/mimir_platform.h"

/* ---- Z-019 lock hold/wait histograms --------------------
 * Everything in this section exists ONLY under
 * MIMIR_LOCK_HIST. A production build never defines it:
 * no code, no data, no symbol, no cost (proof: the
 * recorded run's nm/size/byte-identity checks on the
 * production object, tests/perf/recorded_run_z13_z019.txt).
 * Method: within this translation unit only, the two L6
 * lock entry points are shadowed by macros naming the
 * static wrappers below; the wrappers call the real L6
 * functions through the parenthesised-name form, which
 * the preprocessor does not expand, and they are defined
 * BEFORE the macros so their own bodies are unaffected.
 * Every lock acquisition in this file is timed:
 *   wait = stamp just before the real lock call to the
 *          stamp just after it returns;
 *   hold = stamp just after acquisition (taken AFTER the
 *          wait sample is recorded, so the wait update is
 *          not charged to this hold) to the stamp just
 *          before the real unlock call.
 * The wait sample is recorded immediately after
 * acquisition and the hold sample immediately before
 * release - both while this thread holds the one global
 * lock, so the shared histogram arrays are updated only
 * under the lock and need no atomics. A failed
 * acquisition records no wait/hold sample, only the
 * lock_failures counter (atomic: no lock is held then).
 * A sample whose delta is negative or at/above 2^48
 * cycles (TSC artefact) is dropped and counted in
 * anomalies instead of polluting a bucket.
 * Attribution: a thread-local current-op tag set at the
 * top of mimir_core_alloc / _calloc / _realloc / _free
 * and CLEARED back to OTHER by the unlock wrapper, so a
 * later internal locked surface on the same thread can
 * never inherit a stale entry-point tag. realloc(NULL)
 * delegates to the alloc entry before either takes the
 * lock; the inner entry overwrites the tag and owns the
 * sample - the same call accounting the E-005 counters
 * use. The histogram code allocates nothing, prints
 * nothing and takes no lock of its own.
 * Timing source: rdtsc on x86-64 / i386 (this line's
 * hosts), fenced with LFENCE; other hosts fall back to
 * CLOCK_MONOTONIC_RAW nanoseconds so the facility still
 * compiles and runs - the recorded run states the source
 * and the probe's calibrated cycles-per-ns per process. */
#ifdef MIMIR_LOCK_HIST
#include <time.h>
#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
static uint64_t z19_stamp(void)
{
    _mm_lfence();
    return (uint64_t)__rdtsc();
}
#else
static uint64_t z19_stamp(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) +
           (uint64_t)ts.tv_nsec;
}
#endif

static struct mimir_lock_hist z19_hist;
static _Thread_local int z19_current_op = MIMIR_LH_OP_OTHER;
static _Thread_local uint64_t z19_acquire_stamp;
static _Thread_local int z19_holding;

static unsigned z19_bucket(uint64_t v)
{
    unsigned e;
    uint64_t sub;
    if (v < 64) {
        return (unsigned)v;
    }
    e = 63u - (unsigned)__builtin_clzll(v);
    if (e >= 42u) {
        return MIMIR_LOCK_HIST_BUCKETS - 1;
    }
    sub = (v - (UINT64_C(1) << e)) >> (e - 2u);
    return 64u + (e - 6u) * 4u + (unsigned)sub;
}

static void z19_record(struct mimir_lock_hist_series *s,
                       uint64_t v)
{
    if (v >= (UINT64_C(1) << 48)) {
        s->overflow_samples += 1;
        __atomic_fetch_add(&z19_hist.anomalies, 1ULL,
                           __ATOMIC_RELAXED);
        return;
    }
    s->count += 1;
    s->sum_cycles += v;
    if (s->count == 1 || v < s->min_cycles) {
        s->min_cycles = v;
    }
    if (v > s->max_cycles) {
        s->max_cycles = v;
    }
    if (v >= (UINT64_C(1) << 42)) {
        s->overflow_samples += 1;
    }
    s->buckets[z19_bucket(v)] += 1;
}

static void z19_set_op(int op)
{
    z19_current_op = op;
}

static int z19_lock(void)
{
    uint64_t t0 = z19_stamp();
    int rc = (mimir_platform_global_lock)();
    uint64_t t1 = z19_stamp();
    if (rc != MIMIR_PLATFORM_OK) {
        __atomic_fetch_add(&z19_hist.lock_failures, 1ULL,
                           __ATOMIC_RELAXED);
        return rc;
    }
    if (t1 >= t0) {
        z19_record(&z19_hist.wait[z19_current_op], t1 - t0);
    } else {
        __atomic_fetch_add(&z19_hist.anomalies, 1ULL,
                           __ATOMIC_RELAXED);
    }
    z19_acquire_stamp = z19_stamp();
    z19_holding = 1;
    return rc;
}

static int z19_unlock(void)
{
    uint64_t t0 = z19_stamp();
    if (z19_holding) {
        if (t0 >= z19_acquire_stamp) {
            z19_record(&z19_hist.hold[z19_current_op],
                       t0 - z19_acquire_stamp);
        } else {
            __atomic_fetch_add(&z19_hist.anomalies, 1ULL,
                               __ATOMIC_RELAXED);
        }
    }
    z19_holding = 0;
    z19_current_op = MIMIR_LH_OP_OTHER;
    return (mimir_platform_global_unlock)();
}

#define mimir_platform_global_lock z19_lock
#define mimir_platform_global_unlock z19_unlock

int mimir_core_lock_hist_snapshot(struct mimir_lock_hist *out)
{
    if (out == NULL) {
        return -1;
    }
    *out = z19_hist;
    return 0;
}

void mimir_core_lock_hist_reset(void)
{
    memset(&z19_hist, 0, sizeof z19_hist);
}
#endif /* MIMIR_LOCK_HIST */
#ifdef MIMIR_DEBUG
/* J-005 only: abort() for the fail-clearly report path and
 * write(2) for its fixed-buffer fd 2 report (INC-J005-05). */
#include <stdlib.h>
#include <unistd.h>
#endif

/* Descriptor state values (B-006 S7.3: state is LIVE or
 * RETIRED; 0 means never published). */
#define MIMIR_STATE_UNPUBLISHED 0u
#define MIMIR_STATE_LIVE        1u
#define MIMIR_STATE_RETIRED     2u
#define MIMIR_STATE_POOLED      3u /* Z-048: in the mapping cache */

/* Descriptor magic (B-006 S7.3): a fixed constant identifying
 * a live descriptor of this layout. A sanity check only --
 * not a security boundary and not proof of provenance
 * (MA-6.2-005). The bytes spell "MIMIRALL" in ASCII. */
#define MIMIR_DESCRIPTOR_MAGIC UINT64_C(0x4D494D4952414C4C)

/*
 * Descriptor layout, B-006 S7.3 (design offsets, expressed and
 * compile-time-checked here; still not a stable ABI):
 *    0  8  magic
 *    8  8  mapping_base
 *   16  8  mapping_length
 *   24  8  usable_offset
 *   32  8  requested_size
 *   40  8  usable_size
 *   48  4  state
 *   52  4  flags
 *   56  8  list_next
 *   64  8  list_prev
 *   72 24  reserved (zero on publication)
 * Total 96 bytes, at the start of the reserve page.
 */
struct mimir_descriptor {
    uint64_t magic;
    void *mapping_base;
    size_t mapping_length;
    size_t usable_offset;
    size_t requested_size;
    size_t usable_size;
    uint32_t state;
    uint32_t flags;
    struct mimir_descriptor *list_next;
    struct mimir_descriptor *list_prev;
    unsigned char reserved[24];
};

_Static_assert(sizeof(struct mimir_descriptor) == 96,
               "descriptor must be exactly 96 bytes (B-006 S7.3)");
_Static_assert(offsetof(struct mimir_descriptor, mapping_base) == 8,
               "mapping_base offset (B-006 S7.3)");
_Static_assert(offsetof(struct mimir_descriptor, state) == 48,
               "state offset (B-006 S7.3)");
_Static_assert(offsetof(struct mimir_descriptor, list_next) == 56,
               "list_next offset (B-006 S7.3)");
_Static_assert(offsetof(struct mimir_descriptor, reserved) == 72,
               "reserved offset (B-006 S7.3)");
/* J-002: the allocating thread identity (ISO C11 thrd_t,
 * no OS facility named in this layer) is recorded for the
 * cross_thread_frees counter in the mapping's RESERVE
 * PAGE at offset 96, immediately past the 96-byte
 * descriptor -- deliberately NOT in the descriptor: its
 * flags and reserved[24] are zero-on-publication by
 * B-006 S7.3 and audited exactly by B-007 C1.4
 * (test_b007), and the span header's 96 bytes are fully
 * assigned (G-002 S7). The reserve page is one whole
 * system page (>= 4096 on every admitted host) and only
 * its first 96 bytes are the descriptor, so offset 96 is
 * allocator-owned scratch inside the reserve page; see
 * INC-J002-01. */
#define MIMIR_OWNER_THREAD_OFFSET ((size_t)96)
_Static_assert(sizeof(thrd_t) <= 4096 - 96,
               "thrd_t must fit the reserve page past the descriptor (J-002)");

/*
 * Registry anchors and counters (B-006 S8.1): fixed, small,
 * constant-size static storage, bootstrap-owned below L1
 * (S7.2). A descriptor is on this list if and only if its
 * state is LIVE. Counters are updated only in the same locked
 * section that inserts or removes the descriptor, and drive
 * no allocation decision (Invariant 8).
 */
static struct mimir_descriptor *mimir_registry_head; /* NULL: empty */
static size_t mimir_live_mappings;                   /* 0 at start  */
#ifdef MIMIR_LOCK_SHARD
static _Atomic size_t mimir_live_bytes;              /* 0 at start  */
#else
#ifdef MIMIR_LOCK_SHARD
static _Atomic size_t mimir_live_bytes;              /* 0 at start  */
#else
static size_t mimir_live_bytes;                      /* 0 at start  */
#endif
#endif

/*
 * Z-048 mapping cache (roadmap Wave 6; retention priced by
 * Z-046, VMA economics by Z-015): dead LARGE mappings are
 * retained in a size-exact reuse pool instead of being
 * returned to the OS at free. A pooled descriptor is on NO
 * live registry: its state is MIMIR_STATE_POOLED and it is
 * linked on mimir_pool_head through the descriptor's own
 * list_next/list_prev fields. The pool is bounded by count
 * and by total bytes; a retire that would exceed either
 * bound (or whose mapping alone exceeds the byte bound)
 * takes the banked munmap path instead, and the full
 * trim/drain surface releases the whole pool (the trim
 * trigger the roadmap names). Reuse is exact-length only:
 * a hit republishes the mapping through the ordinary
 * publication code in alloc_locked, so every publication
 * invariant (descriptor rewritten in full, zero-on-
 * publication fields, J-002 owner record, audit
 * publication) is identical for fresh and reused mappings.
 *
 * Accounting under the pool (Z-048 definitions):
 * - mimir_live_mappings / mimir_live_bytes count LIVE
 *   only, exactly as before; pooled mappings are neither.
 * - mimir_audit_mapping_retired increments when a
 *   descriptor leaves LIVE (pooled or unmapped), so
 *   published - retired == live mappings still holds.
 * - mimir_stats_bytes_released counts bytes ACTUALLY
 *   returned to the OS: a pooled retire adds nothing; a
 *   direct unmap at retire and every pool release at
 *   trim/drain add their mapping_length at the munmap.
 * - Q-MAPPED (mimir_core_mapped_bytes and the J-002
 *   snapshot's bytes_mapped) counts live mapping bytes +
 *   pooled mapping bytes + live span bytes: a pooled
 *   mapping is still mapped. Q-RETAINED / bytes_cached
 *   keep their span-only definitions; the pool has its
 *   own query pair (mimir_core_pool_count/_bytes).
 */
#define MIMIR_POOL_MAX_COUNT ((size_t)64)
#define MIMIR_POOL_MAX_BYTES ((size_t)32 * 1024 * 1024)
static struct mimir_descriptor *mimir_pool_head; /* NULL: empty */
static size_t mimir_pool_count;                  /* 0 at start  */
static size_t mimir_pool_bytes;                  /* 0 at start  */

/*
 * E-005 audit counters (docs/PHASE_5_AUDIT_HOOK_E005.md;
 * INC-E001-07 shape (b)): mimir_audit_alloc_published
 * increments exactly once when a new mapping is published
 * LIVE in alloc_locked (the single publication point, so
 * it covers malloc, calloc, and realloc's grow-by-move
 * construction); mimir_audit_mapping_retired increments
 * exactly once when retire_locked successfully releases a
 * mapping (the single retirement point, so it covers free
 * and realloc's transfer of the old block). Their
 * difference is the number of live MimirAlloc mappings.
 * Like the L1 call counters, these are INTERNAL audit
 * state: declared in no installed header, defined here
 * with default visibility in this object only so a
 * test-only static build can read them, and bound local
 * in the shared library (this object is compiled
 * -fvisibility=hidden for the .so and the version script
 * binds every non-public name local), so they never
 * appear in libmimiralloc.so's dynamic symbol table.
 * Contract status: NOT part of the contract -
 * Proposed-internal only. Increments happen with the
 * global lock held (__atomic_fetch_add, relaxed, is used
 * for uniformity with the L1 counters and is harmless).
 */
unsigned long mimir_audit_alloc_published;
unsigned long mimir_audit_mapping_retired;

/*
 * J-002 statistics state (docs/PHASE_10_STATISTICS.md;
 * roadmap Phase 10; I-002 S3.2 quantities). Fixed static
 * storage below L1, like every counter above. The byte
 * counters, the peak and the cross-thread counter are
 * updated only inside the one global lock, at the exact
 * publication / release points named in mimir_core.h.
 * alloc_failures and alloc_posture_nulls are also
 * incremented on pre-lock paths (posture gates, lock
 * failure itself), so those two use __atomic_fetch_add
 * (relaxed) exactly like the E-005 audit counters; the
 * snapshot reads them with __atomic_load_n under the lock.
 * Counters drive no allocation decision (Invariant 8).
 */
#ifdef MIMIR_LOCK_SHARD
static _Atomic uint64_t mimir_stats_bytes_requested;
static _Atomic uint64_t mimir_stats_bytes_allocated;
static _Atomic uint64_t mimir_stats_bytes_released;
#else
static uint64_t mimir_stats_bytes_requested;
static uint64_t mimir_stats_bytes_allocated;
static uint64_t mimir_stats_bytes_released;
#endif
static uint64_t mimir_stats_peak_mapped;
_Alignas(64) static uint64_t mimir_stats_alloc_failures; /* Z-040: own 64 B line - pre-lock atomic (Z-016 rule) */
_Alignas(64) static uint64_t mimir_stats_alloc_posture_nulls; /* Z-040: own 64 B line - pre-lock atomic (Z-016 rule) */
_Alignas(64) static uint64_t mimir_stats_unlock_failures; /* Z-064: own 64 B line - post-unlock atomic (Z-016 rule) */
_Alignas(64) static uint64_t mimir_stats_drain_lock_failures; /* Z-065: own 64 B line - pre-lock atomic (Z-016 rule) */
_Alignas(64) static uint64_t mimir_stats_trim_lock_failures; /* Z-065: own 64 B line - pre-lock atomic (Z-016 rule) */
_Alignas(64) static uint64_t mimir_stats_free_lock_failures; /* Z-065: own 64 B line - pre-lock atomic (Z-016 rule) */
static uint64_t mimir_stats_cross_thread_frees;
/* Z-076: small-path cross-thread half (INC-J002-01) and
 * the unrecorded residual. Both move only under the one
 * global lock, at the mimir_core_free courses named in
 * mimir_core.h; the snapshot sums the small half into
 * cross_thread_frees and reports both verbatim. */
static uint64_t mimir_stats_cross_thread_small;
static uint64_t mimir_stats_small_unrecorded;

static void mimir_stats_peak_update_locked(void);

static void mimir_stats_note_failure(void)
{
    __atomic_fetch_add(&mimir_stats_alloc_failures, 1ULL,
                       __ATOMIC_RELAXED);
}

static void mimir_stats_note_posture(void)
{
    __atomic_fetch_add(&mimir_stats_alloc_posture_nulls, 1ULL,
                       __ATOMIC_RELAXED);
}

/* Z-064 (D-01): record one failed central unlock. Called
 * from the unlock wrappers below at the unlock site, where
 * the lock state is by definition unknown (the unlock just
 * failed), so the recording path is a single atomic add and
 * takes no lock (Z-016 / Z-040 pre-lock atomic posture). */
static void mimir_stats_note_unlock_failure(void)
{
    __atomic_fetch_add(&mimir_stats_unlock_failures, 1ULL,
                       __ATOMIC_RELAXED);
}

/* ---- J-007 diagnostics channel ----------------------------
 * Everything below in this section uses ONLY: fixed static
 * tables, its arguments, the two J-002 atomic counters
 * (loaded, never allocated), one fixed stack buffer and
 * write(2). No malloc family call, no L6 call, no stdio,
 * no lock -- by construction, and proven by interposer in
 * tests/core/test_j007.c. mimir_diag_last_cause is written
 * by the locked construction paths (or a pre-lock admission
 * gate) immediately before they return NULL and read by
 * the same entry-point call after unlock, still on the
 * failing thread; it is a cause LABEL for the report, never
 * a decision input (Invariant 8 posture). */
static const char *const mimir_diag_op_names[] = {
    "malloc", "calloc", "realloc", "free", "drain", "trim"
};
static const char *const mimir_diag_cause_names[] = {
    "none", "backing", "arithmetic", "lock", "page-size",
    "internal", "metadata", "double-free", "poison"
};
#ifdef MIMIR_LOCK_SHARD
static _Thread_local int mimir_diag_last_cause;
#else
#ifdef MIMIR_LOCK_SHARD
static _Thread_local int mimir_diag_last_cause;
#else
static int mimir_diag_last_cause;
#endif
#endif
static int mimir_diag_guard; /* 0 idle, 1 reporting */
static uint64_t mimir_diag_emitted_count;
static uint64_t mimir_diag_suppressed_count;
static uint64_t mimir_diag_max_depth_seen;

/* ---- Z-070: double-free detection (poison + membership) ----
 * The cheap always-on subset, costed in
 * tests/perf/recorded_run_z13_z070.txt:
 *   MEMBER: a second free of a small block is refused.
 *     The central courses already meet the pointer on its
 *     span's free list inside find_span_mode_locked (the
 *     J-006 walk -- that cost predates this slice and is
 *     unchanged); this slice labels that meeting so the
 *     entry point can count and report it. The TLS local
 *     course, which bins a same-thread free WITHOUT
 *     consulting any list, gains the missing check: a
 *     scan of the calling thread's own per-class bin
 *     (at most MIMIR_TLS_BIN_CAPACITY pointer compares
 *     against a thread-local array) before the block is
 *     binned a second time.
 *   POISON: at every small free course, bytes
 *     [sizeof(void*), class_size) of the freed block are
 *     filled with MIMIR_DFREE_POISON_BYTE (the first 8
 *     bytes are the free-list link's home and are never
 *     poisoned or verified); at every reuse handout the
 *     same range is verified. A mismatch tallies and
 *     reports; the handout proceeds (the signal is about
 *     caller data, not allocator metadata).
 * Escape hatch: build with -DMIMIR_DFREE_GUARD=0 (or
 * -DMIMIR_DFREE_MEMBER=0 / -DMIMIR_DFREE_POISON=0 to
 * disable one half) and every per-op check compiles
 * out; the tallies then stay 0 and only the two stats
 * fields remain. Under MIMIR_DEBUG the J-005 poison /
 * quarantine owns detection and this guard is off;
 * under MIMIR_OOL_META the Z-034 trial's no-block-byte
 * invariant owns its spans and this guard is off. */
#ifndef MIMIR_DFREE_GUARD
#define MIMIR_DFREE_GUARD 1
#endif
#ifndef MIMIR_DFREE_MEMBER
#define MIMIR_DFREE_MEMBER MIMIR_DFREE_GUARD
#endif
#ifndef MIMIR_DFREE_POISON
#define MIMIR_DFREE_POISON MIMIR_DFREE_GUARD
#endif
#if defined(MIMIR_DEBUG) || defined(MIMIR_OOL_META)
#define MIMIR_DFREE_MEMBER_ON 0
#define MIMIR_DFREE_POISON_ON 0
#elif MIMIR_DFREE_GUARD && MIMIR_DFREE_MEMBER
#define MIMIR_DFREE_MEMBER_ON 1
#else
#define MIMIR_DFREE_MEMBER_ON 0
#endif
#if !defined(MIMIR_DEBUG) && !defined(MIMIR_OOL_META) && \
        MIMIR_DFREE_GUARD && MIMIR_DFREE_POISON
#define MIMIR_DFREE_POISON_ON 1
#elif !defined(MIMIR_DFREE_POISON_ON)
#define MIMIR_DFREE_POISON_ON 0
#endif
#define MIMIR_DFREE_POISON_BYTE 0xDF
/* Z-070 measured cap: the poisoned range is
 * [sizeof(void*), sizeof(void*) + MIMIR_DFREE_POISON_SPAN)
 * intersected with the class extent -- at most 32 bytes
 * past the link word. Full-extent poison was measured
 * first (recorded run Table A) and rejected above 16 B:
 * its cost grows linearly with class size (+222 ns/pair
 * at 1024 B, 8.8x the whole pair) while the detection
 * value does not -- use-after-free writes land at the
 * start of the block in the common case, and the capped
 * range covers the first 40 bytes of every class. */
#define MIMIR_DFREE_POISON_SPAN 32

/* ---- Z-066 (D-12): INC-J006-02 consistent-length detection ----
 * State for the detection wiring documented at
 * descriptor_lengths_audit_locked and span_length_is_true
 * below. The tallies are atomic (read lock-free by the
 * getters and the stats snapshot). mimir_z066_last_bad_sum
 * is touched only under the central lock: it remembers the
 * walked byte sum of the currently open descriptor
 * detection episode so repeat refusals of one episode
 * count as collateral, not as new detections (0 = no open
 * episode; a real walked sum is never 0 while any mapping
 * is live). mimir_z066_detected is the J-007 label pattern
 * carried to the free path: a locked detection site sets
 * it, and mimir_core_free consumes it AFTER unlocking and
 * reports through the diag channel (op free, cause
 * metadata); it is thread-local because the setter and the
 * consumer are the same call on the same thread. */
static _Atomic uint64_t mimir_metadata_detections;
static _Atomic uint64_t mimir_metadata_collateral_refusals;
static size_t mimir_z066_last_bad_sum;
static _Thread_local int mimir_z066_detected;

/* Z-070: detection tallies (atomic; the TLS course takes
 * no lock, the Z-064/Z-066 pre-lock atomic posture) and
 * the free-path label, the Z-066 pattern carried to the
 * J-006 free-list walk: a locked finder sets it when a
 * for-free walk meets the pointer on its own span's free
 * list, and mimir_core_free consumes it after unlocking,
 * once per refused free. */
static _Atomic uint64_t mimir_dfree_detections;
static _Atomic uint64_t mimir_poison_detections;
static _Thread_local int mimir_z070_df_detected;

/* Consume the detection label on the free path, after the
 * caller has unlocked. A span detection is tallied here,
 * once per refused central free (the descriptor audit
 * tallies itself, at its site, with episode semantics --
 * span_event is 0 for that course so it never counts
 * twice). The diag report is the channel's own: env-gated,
 * allocation-free, a plain no-op when the gate is closed. */
static void z066_free_report(int span_event)
{
    if (!mimir_z066_detected) {
        return;
    }
    mimir_z066_detected = 0;
    if (span_event) {
        __atomic_fetch_add(&mimir_metadata_detections, 1ULL,
                           __ATOMIC_RELAXED);
    }
    mimir_core_diag_report(MIMIR_DIAG_OP_FREE,
                           MIMIR_DIAG_CAUSE_METADATA, 0);
}

/* Z-070: consume the double-free label on the free
 * path, after the caller has unlocked (the z066_free_
 * report shape): tally once per refused central free and
 * report through the diag channel (op free, cause
 * double-free). */
static void z070_df_report(void)
{
    if (!mimir_z070_df_detected) {
        return;
    }
    mimir_z070_df_detected = 0;
    __atomic_fetch_add(&mimir_dfree_detections, 1ULL,
                       __ATOMIC_RELAXED);
    mimir_core_diag_report(MIMIR_DIAG_OP_FREE,
                           MIMIR_DIAG_CAUSE_DOUBLE_FREE, 0);
}

/* Z-070: record one double-free detection at a site that
 * reports immediately (the TLS bin course: no lock is
 * held and none is taken; the diag channel is env-gated
 * and allocation-free, so this is a plain atomic add
 * plus a normally-closed gate check). */
#if MIMIR_DFREE_MEMBER_ON
static void dfree_note_double_free(void)
{
    __atomic_fetch_add(&mimir_dfree_detections, 1ULL,
                       __ATOMIC_RELAXED);
    mimir_core_diag_report(MIMIR_DIAG_OP_FREE,
                           MIMIR_DIAG_CAUSE_DOUBLE_FREE, 0);
}
#endif /* MIMIR_DFREE_MEMBER_ON */

#if MIMIR_DFREE_POISON_ON
/* Z-070: poison bytes [sizeof(void*), class_size) of a
 * freshly freed block. The first 8 bytes are the free-
 * list link's home (or the caller's last bytes, in a
 * bin) and are never touched here. */
static void dfree_poison_fill(void *ptr, size_t class_size)
{
    if (class_size > sizeof(void *)) {
        size_t n = class_size - sizeof(void *);
        if (n > MIMIR_DFREE_POISON_SPAN) {
            n = MIMIR_DFREE_POISON_SPAN;
        }
        memset((unsigned char *)ptr + sizeof(void *),
               MIMIR_DFREE_POISON_BYTE, n);
    }
}

/* Z-070: verify the poison range at a reuse handout. A
 * mismatch tallies one poison detection and reports
 * (op = the serving entry); the handout proceeds. */
static void dfree_poison_verify(const void *ptr,
                                size_t class_size, int op)
{
    const unsigned char *p = (const unsigned char *)ptr;
    size_t end = class_size;
    size_t i;
    if (end > sizeof(void *) + MIMIR_DFREE_POISON_SPAN) {
        end = sizeof(void *) + MIMIR_DFREE_POISON_SPAN;
    }
    /* Word-wise over the capped range (the block is
     * 16-byte aligned and the range starts at offset 8,
     * so every word load is 8-aligned); the tail of a
     * non-multiple class is checked byte-wise. */
    for (i = sizeof(void *);
         i + sizeof(uint64_t) <= end; i += sizeof(uint64_t)) {
        uint64_t w;
        memcpy(&w, p + i, sizeof w);
        if (w != UINT64_C(0xDFDFDFDFDFDFDFDF)) {
            __atomic_fetch_add(&mimir_poison_detections,
                               1ULL, __ATOMIC_RELAXED);
            mimir_core_diag_report(
                op, MIMIR_DIAG_CAUSE_POISON,
                (uint64_t)class_size);
            return;
        }
    }
    for (; i < end; i++) {
        if (p[i] != MIMIR_DFREE_POISON_BYTE) {
            __atomic_fetch_add(&mimir_poison_detections,
                               1ULL, __ATOMIC_RELAXED);
            mimir_core_diag_report(
                op, MIMIR_DIAG_CAUSE_POISON,
                (uint64_t)class_size);
            return;
        }
    }
}
#endif /* MIMIR_DFREE_POISON_ON */

/* Record the cause and return NULL from a pointer-returning
 * locked construction path (alloc_locked /
 * span_acquire_locked). */
#define MIMIR_DIAG_FAIL(cause)     (mimir_diag_last_cause = (cause), NULL)

static size_t mimir_diag_append(char *buf, size_t cap,
                                size_t len, const char *text)
{
    while (*text != '\0' && len < cap) {
        buf[len++] = *text++;
    }
    return len;
}

static size_t mimir_diag_append_u64(char *buf, size_t cap,
                                    size_t len, uint64_t value)
{
    char digits[20];
    size_t n = 0;
    if (value == 0) {
        digits[n++] = '0';
    }
    while (value > 0) {
        digits[n++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (n > 0 && len < cap) {
        buf[len++] = digits[--n];
    }
    return len;
}

void mimir_core_diag_report(int op, int cause,
                            uint64_t requested)
{
    char buf[224];
    const char *gate;

    size_t len = 0;
    size_t off = 0;
    const char *op_name = "unknown";
    const char *cause_name = "unknown";
    uint64_t failures;
    uint64_t posture;

    /* INC-J007-01 enable gate: emission happens only when
     * the process environment carries MIMIRALLOC_DIAG=1.
     * The banked C-006 audit (MA-6.3-020..023 silence
     * battery, inside `make test`) grades the Phase 3
     * no-reporting baseline, and J-001 S5.4's default-on
     * reading cannot hold simultaneously with that banked
     * gate; the channel therefore ships in production,
     * wired and proven, with emission explicitly enabled.
     * The owed ruling was taken at Z-071: DECISION_LEDGER
     * D-011 keeps this gate as the permanent production
     * posture (INC-J007-01 discharged; evidence packet
     * tests/perf/recorded_run_z13_z071.txt). The
     * gate itself is allocation-free: getenv performs no
     * allocation (a bounded scan of environ), takes no
     * lock and allocates no storage, so every bound proven
     * for the channel covers the gated path whole. When
     * the gate is closed the call is a plain return: no
     * counter moves, nothing is written. */
    gate = getenv("MIMIRALLOC_DIAG");
    if (gate == NULL || gate[0] != '1' || gate[1] != '\0') {
        return;
    }
    if (__atomic_exchange_n(&mimir_diag_guard, 1,
                            __ATOMIC_ACQ_REL) != 0) {
        /* Nested attempt (a report triggered from inside a
         * report): counted and dropped. Depth never exceeds
         * 1, and nothing here can re-enter the allocator. */
        __atomic_fetch_add(&mimir_diag_suppressed_count, 1ULL,
                           __ATOMIC_RELAXED);
        return;
    }
    if (mimir_diag_max_depth_seen < 1) {
        mimir_diag_max_depth_seen = 1;
    }
    if (op >= 0 && op < (int)(sizeof mimir_diag_op_names /
                              sizeof mimir_diag_op_names[0])) {
        op_name = mimir_diag_op_names[op];
    }
    if (cause >= 0 &&
        cause < (int)(sizeof mimir_diag_cause_names /
                      sizeof mimir_diag_cause_names[0])) {
        cause_name = mimir_diag_cause_names[cause];
    }
    failures = __atomic_load_n(&mimir_stats_alloc_failures,
                               __ATOMIC_RELAXED);
    posture = __atomic_load_n(&mimir_stats_alloc_posture_nulls,
                              __ATOMIC_RELAXED);
    len = mimir_diag_append(buf, sizeof buf, len,
                            "mimiralloc: error op=");
    len = mimir_diag_append(buf, sizeof buf, len, op_name);
    len = mimir_diag_append(buf, sizeof buf, len, " cause=");
    len = mimir_diag_append(buf, sizeof buf, len, cause_name);
    len = mimir_diag_append(buf, sizeof buf, len, " req=");
    len = mimir_diag_append_u64(buf, sizeof buf, len, requested);
    len = mimir_diag_append(buf, sizeof buf, len, " failures=");
    len = mimir_diag_append_u64(buf, sizeof buf, len, failures);
    len = mimir_diag_append(buf, sizeof buf, len, " posture=");
    len = mimir_diag_append_u64(buf, sizeof buf, len, posture);
    len = mimir_diag_append(buf, sizeof buf, len, "\n");
    while (off < len) {
        ssize_t written = write(STDERR_FILENO, buf + off,
                                len - off);
        if (written <= 0) {
            break;
        }
        off += (size_t)written;
    }
    if (off == len) {
        __atomic_fetch_add(&mimir_diag_emitted_count, 1ULL,
                           __ATOMIC_RELAXED);
    } else {
        __atomic_fetch_add(&mimir_diag_suppressed_count, 1ULL,
                           __ATOMIC_RELAXED);
    }
    __atomic_store_n(&mimir_diag_guard, 0, __ATOMIC_RELEASE);
}

uint64_t mimir_core_diag_emitted(void)
{
    return __atomic_load_n(&mimir_diag_emitted_count,
                           __ATOMIC_RELAXED);
}

uint64_t mimir_core_diag_suppressed(void)
{
    return __atomic_load_n(&mimir_diag_suppressed_count,
                           __ATOMIC_RELAXED);
}

uint64_t mimir_core_diag_max_depth(void)
{
    return __atomic_load_n(&mimir_diag_max_depth_seen,
                           __ATOMIC_RELAXED);
}

uint64_t mimir_core_metadata_detections(void)
{
    return __atomic_load_n(&mimir_metadata_detections,
                           __ATOMIC_RELAXED);
}

uint64_t mimir_core_metadata_collateral_refusals(void)
{
    return __atomic_load_n(&mimir_metadata_collateral_refusals,
                           __ATOMIC_RELAXED);
}

uint64_t mimir_core_double_free_detections(void)
{
    return __atomic_load_n(&mimir_dfree_detections,
                           __ATOMIC_RELAXED);
}

uint64_t mimir_core_double_free_poison_detections(void)
{
    return __atomic_load_n(&mimir_poison_detections,
                           __ATOMIC_RELAXED);
}

/* Insert at the head of the registry. Caller holds the lock
 * and has fully written the descriptor (B-007 C1.4). */
static void registry_insert(struct mimir_descriptor *desc)
{
    desc->list_prev = NULL;
    desc->list_next = mimir_registry_head;
    if (mimir_registry_head != NULL) {
        mimir_registry_head->list_prev = desc;
    }
    mimir_registry_head = desc;
}

/* Remove from the registry. Caller holds the lock. Only the
 * links of still-mapped neighbours are touched. */
static void registry_remove(struct mimir_descriptor *desc)
{
    if (desc->list_prev != NULL) {
        desc->list_prev->list_next = desc->list_next;
    } else {
        mimir_registry_head = desc->list_next;
    }
    if (desc->list_next != NULL) {
        desc->list_next->list_prev = desc->list_prev;
    }
    desc->list_next = NULL;
    desc->list_prev = NULL;
}

/* Z-048 mapping-cache list helpers. Caller holds the lock.
 * The pool list reuses the descriptor's own list links; a
 * descriptor is on exactly one of {live registry, pool,
 * neither} at any instant. */
static void pool_insert_locked(struct mimir_descriptor *desc)
{
    desc->list_prev = NULL;
    desc->list_next = mimir_pool_head;
    if (mimir_pool_head != NULL) {
        mimir_pool_head->list_prev = desc;
    }
    mimir_pool_head = desc;
    mimir_pool_count += 1;
    mimir_pool_bytes += desc->mapping_length;
}

static void pool_remove_locked(struct mimir_descriptor *desc)
{
    if (desc->list_prev != NULL) {
        desc->list_prev->list_next = desc->list_next;
    } else {
        mimir_pool_head = desc->list_next;
    }
    if (desc->list_next != NULL) {
        desc->list_next->list_prev = desc->list_prev;
    }
    desc->list_next = NULL;
    desc->list_prev = NULL;
    mimir_pool_count -= 1;
    mimir_pool_bytes -= desc->mapping_length;
}

/* Take a pooled mapping of exactly want_length out of the
 * pool and return its base, or NULL on a miss. The caller
 * republishes it through alloc_locked's publication code. */
static void *pool_take_locked(size_t want_length)
{
    struct mimir_descriptor *desc = mimir_pool_head;
    while (desc != NULL) {
        if (desc->mapping_length == want_length) {
            void *base = desc->mapping_base;
            pool_remove_locked(desc);
            return base;
        }
        desc = desc->list_next;
    }
    return NULL;
}

/* Release every pooled mapping to the OS (the Z-048 trim
 * trigger). Caller holds the lock. Each entry is unlinked
 * and accounted BEFORE its munmap (after a successful
 * release its reserve page is gone); a failed release
 * re-pools the still-mapped entry, so the pool bookkeeping
 * always describes actual memory. Returns how many
 * mappings were actually released. */
static size_t pool_release_all_locked(void)
{
    size_t released = 0;
    struct mimir_descriptor *desc = mimir_pool_head;
    while (desc != NULL) {
        struct mimir_descriptor *next = desc->list_next;
        void *base = desc->mapping_base;
        size_t length = desc->mapping_length;
        pool_remove_locked(desc);
        if (mimir_platform_map_release(base, length) ==
            MIMIR_PLATFORM_OK) {
            mimir_stats_bytes_released += length;
            released += 1;
        } else {
            pool_insert_locked(desc);
        }
        desc = next;
    }
    return released;
}

/*
 * Owner determination (B-006 S11.2 step 4), shared by free
 * and realloc (C-005): find the descriptor whose recorded
 * user pointer is exactly ptr, walking only allocator-owned
 * registry links -- validation never dereferences memory
 * derived from the unvalidated ptr, so a foreign pointer's
 * bytes are never read here (the order recorded in
 * tests/core/NOTES.md item 2 for C-003). Caller holds the
 * lock and validates the returned descriptor's own fields
 * with descriptor_is_live before acting on it.
 */
static struct mimir_descriptor *find_candidate_locked(void *ptr)
{
    struct mimir_descriptor *desc = mimir_registry_head;
    size_t j006_steps = 0;

    while (desc != NULL) {
        /* J-006: bound the walk by the live counter. A valid
         * registry holds exactly mimir_live_mappings nodes,
         * so no valid walk is cut; a corrupted list_next
         * that cycles now ends in refusal instead of an
         * unbounded loop. A list_next pointing at unmapped
         * memory still faults on the dereference below --
         * no in-process check can validate a pointer without
         * reading it (INC-J006-01). */
        if (++j006_steps > mimir_live_mappings) {
            return NULL;
        }
        if ((void *)((unsigned char *)desc->mapping_base +
                     desc->usable_offset) == ptr) {
            break;
        }
        desc = desc->list_next;
    }
    return desc;
}

/* The S8.2 step-2 field validation, applied to a trusted
 * registry descriptor: magic, state == LIVE, and
 * mapping_base == the descriptor's own address (S7.1: the
 * descriptor sits at its mapping base). The pointer-equality
 * part of the validation is find_candidate_locked itself. */
static int descriptor_is_live(struct mimir_descriptor *desc)
{
    /* J-006 (docs/PHASE_10_METADATA_INTEGRITY.md): the S8.2
     * checks above plus the exact layout relations
     * publication writes in alloc_locked -- usable_offset
     * lies inside the mapping, usable_size is exactly the
     * mapping past that offset, and requested_size is a
     * non-zero size no larger than the usable extent (a
     * realloc shrink only ever lowers it). Valid metadata
     * always satisfies every relation, so no defined
     * behaviour changes; a descriptor whose length, offset
     * or size fields were corrupted after publication now
     * fails validation here and takes the non-destructive
     * refusal course, instead of driving a wrong-length
     * release in retire_locked or a wrong-length transfer
     * copy in realloc. Fields corrupted CONSISTENTLY with
     * these relations still pass (INC-J006-02). */
    return desc->magic == MIMIR_DESCRIPTOR_MAGIC &&
           desc->state == MIMIR_STATE_LIVE &&
           desc->mapping_base == (void *)desc &&
           desc->usable_offset != 0 &&
           desc->usable_offset <= desc->mapping_length &&
           desc->usable_size ==
               desc->mapping_length - desc->usable_offset &&
           desc->requested_size != 0 &&
           desc->requested_size <= desc->usable_size;
}

/*
 * Retire one LIVE descriptor exactly once (B-006 S11.2
 * steps 5-7), shared by free and by realloc's move transfer
 * (C-005). Caller holds the lock. The exact acquired pair
 * is captured in locals before release (S8.2 step 3): after
 * a successful release the descriptor is dead and never
 * read. On release failure F-06 governs: the retirement is
 * rolled back in full (descriptor restored to LIVE and
 * re-linked, counters restored) so the block remains
 * recorded as owned and the bookkeeping still describes
 * actual memory (Invariant 2). Returns 1 on success, 0 on
 * rolled-back failure.
 */
/*
 * Z-066 (D-12): the descriptor half of INC-J006-02 --
 * detection of CONSISTENT length corruption on the
 * release path. J-006 proved the integrity walk's
 * byte-sum catches this class (walked live lengths no
 * longer equal mimir_live_bytes) while every operational
 * path stayed silent: retire_locked released whatever
 * length the corrupted header claimed and Q-RELEASED
 * credited it. This audit is that same byte-sum,
 * promoted to the one choke point every descriptor
 * release passes through (free, realloc transfer,
 * realloc rollback): walk the live registry with the
 * J-006 step bound, sum mapping_length over the
 * descriptors that pass descriptor_is_live, and require
 * the walked count and byte sum to equal the static
 * counters exactly -- the invariant
 * mimir_core_metadata_check already proves of every
 * valid tree, so no defined behaviour changes.
 *
 * On mismatch the retirement is REFUSED before any
 * state moves: the caller reads it exactly like a
 * release failure (F-06 posture -- the block stays
 * LIVE, counters untouched, nothing unmapped on a
 * length nobody can trust). The refusal is fail-closed
 * and registry-wide by construction: the sum cannot
 * say WHICH descriptor is corrupt, so while any one
 * episode is open every retirement refuses; the first
 * refusal of an episode (a walked sum not seen before)
 * tallies one detection, later refusals of the same
 * episode tally as collateral, and any passing audit
 * closes the episode. Detection is reported on the
 * J-007 diag channel by the entry point (cause
 * metadata). Scope, stated plainly: compensating
 * corruption of two descriptors (+1 page / -1 page)
 * keeps the sum equal and is NOT detected here; a
 * descriptor corrupted while POOLED is off this
 * registry and outside this audit (the pool's own
 * byte-sum in the walk still flags it when a checker
 * runs). Caller holds the lock.
 */
static int descriptor_lengths_audit_locked(void)
{
    struct mimir_descriptor *desc = mimir_registry_head;
    size_t steps = 0;
    size_t bytes = 0;

    while (desc != NULL) {
        if (++steps > mimir_live_mappings) {
            return 0; /* cycling registry (INC-J006-07 kin) */
        }
        if (descriptor_is_live(desc)) {
            bytes += desc->mapping_length;
        }
        desc = desc->list_next;
    }
    if (steps != mimir_live_mappings ||
        bytes != mimir_live_bytes) {
        if (bytes != mimir_z066_last_bad_sum) {
            __atomic_fetch_add(&mimir_metadata_detections,
                               1ULL, __ATOMIC_RELAXED);
            mimir_z066_last_bad_sum = bytes;
        } else {
            __atomic_fetch_add(
                &mimir_metadata_collateral_refusals, 1ULL,
                __ATOMIC_RELAXED);
        }
        mimir_z066_detected = 1;
        return 0;
    }
    mimir_z066_last_bad_sum = 0;
    return 1;
}

static int retire_locked(struct mimir_descriptor *desc)
{
    /* Z-066: no retirement on unproven length accounting.
     * A refusal here precedes every mutation below, so
     * the F-06 rollback contract (block stays LIVE,
     * counters restored) holds trivially -- nothing was
     * ever moved. */
    if (!descriptor_lengths_audit_locked()) {
        mimir_diag_last_cause = MIMIR_DIAG_CAUSE_METADATA;
        return 0;
    }
    void *mapping_base = desc->mapping_base;
    size_t mapping_length = desc->mapping_length;
    int status;

    desc->state = MIMIR_STATE_RETIRED;
    registry_remove(desc);
    mimir_live_mappings -= 1;
    mimir_live_bytes -= mapping_length;

    /* Z-048: when the mapping cache admits this mapping
     * (count and byte bounds, checked overflow-free against
     * the pool's current bytes), the retirement completes
     * WITHOUT an munmap: the descriptor stays in its still-
     * mapped reserve page with state POOLED, off every live
     * registry, until a same-length allocation republishes
     * it or trim/drain releases it. The audit retirement
     * still increments here -- the block has left LIVE --
     * but Q-RELEASED does not move: nothing was returned
     * to the OS. Only a retire the pool cannot admit takes
     * the banked release path below. */
    if (mimir_pool_count < MIMIR_POOL_MAX_COUNT &&
        mapping_length <= MIMIR_POOL_MAX_BYTES - mimir_pool_bytes) {
        desc->state = MIMIR_STATE_POOLED;
        pool_insert_locked(desc);
        __atomic_fetch_add(&mimir_audit_mapping_retired, 1UL,
                           __ATOMIC_RELAXED);
        return 1;
    }

    status = mimir_platform_map_release(mapping_base,
                                        mapping_length);
    if (status != MIMIR_PLATFORM_OK) {
        desc->state = MIMIR_STATE_LIVE;
        registry_insert(desc);
        mimir_live_mappings += 1;
        mimir_live_bytes += mapping_length;
        return 0;
    }
    __atomic_fetch_add(&mimir_audit_mapping_retired, 1UL,
                       __ATOMIC_RELAXED);
    mimir_stats_bytes_released += mapping_length; /* J-002 Q-RELEASED */
    return 1;
}

/*
 * Allocation construction with the global lock already held
 * (C-005 refactor of the C-003 mimir_core_alloc body; the
 * checked-arithmetic, layout, publication and rollback
 * steps are unchanged). size is admitted by the caller
 * (size > 0). If zero_user is non-zero the usable extent is
 * zeroed after the mapping succeeds and before registry
 * publication -- the B-008 S3.3 explicit zeroing step for
 * calloc; the zeroing covers usable_size (the whole usable
 * extent), of which the requested extent is the
 * caller-promised part. On success *out_desc (when
 * non-NULL) receives the LIVE descriptor; on failure NULL
 * is returned, nothing is published, and registry and
 * counters are unchanged (A.3).
 */
static void *alloc_locked(size_t size, int zero_user,
                          struct mimir_descriptor **out_desc)
{
    size_t page_size;
    size_t total_unrounded;
    size_t usable_offset;
    size_t usable_size;
    void *base = NULL;
    size_t mapping_length = 0;
    struct mimir_descriptor *desc;
    int status;
    int from_pool = 0; /* Z-048: base was taken from the pool */

    /* L5 layout, via L6 discovery (B-006 S11.1 step 5).
     * Page-size discovery failure is a defined failure with
     * no mapping attempted and no fallback constant (F-11). */
    status = mimir_platform_page_size(&page_size);
    if (status != MIMIR_PLATFORM_OK || page_size == 0) {
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_PAGESIZE);
    }
    usable_offset = page_size; /* exactly one system page, S7.1 */

    /* Checked arithmetic, check before evaluate (B-007 A3/A4):
     * total = usable_offset + requested_size, tested first. */
    if (size > SIZE_MAX - usable_offset) {
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_ARITHMETIC);
    }
    total_unrounded = usable_offset + size;

    /* No bookkeeping check is evaluated before the mapping:
     * mapping_length is not yet known here (it is at least
     * total_unrounded). The exact B-007 A10 counter checks
     * run after acquire, before publication, below.
     * (C-004 / INC-C004-04: the C-003 heading of this comment
     * claimed a pre-acquire check that no code performed.) */

    /* Z-048: the mapping cache is consulted first. The
     * exact length a fresh acquire would return is the
     * checked page round-up of total_unrounded; a pooled
     * mapping of exactly that length is taken out of the
     * pool and republished by the identical publication
     * code below, so a hit and a fresh mapping differ only
     * in how base/mapping_length were obtained. A pooled
     * mapping that fails a publication check is returned
     * to the pool, never unmapped (see from_pool below). */
    {
        size_t want_length;
        if (total_unrounded <= SIZE_MAX - (page_size - 1)) {
            want_length = ((total_unrounded + page_size - 1) /
                           page_size) * page_size;
            base = pool_take_locked(want_length);
            if (base != NULL) {
                mapping_length = want_length;
                from_pool = 1;
            }
        }
    }
    if (base == NULL) {
        /* L6 acquire, lock held across the call (S9.4
         * DECISION). L6 applies the checked page round-up
         * (B-007 A5/A6) and is all-or-nothing: on failure
         * nothing is mapped (F-01, F-03, F-04), no
         * descriptor exists, registry unchanged. */
        status = mimir_platform_map_acquire(total_unrounded,
                                            &base,
                                            &mapping_length);
        if (status != MIMIR_PLATFORM_OK) {
            return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_BACKING);
        }
    }

    /* B-007 A8: subtraction checked before evaluate, and the
     * publication gate usable_size >= requested_size. */
    if (usable_offset > mapping_length) {
        if (from_pool) {
                pool_insert_locked(
                    (struct mimir_descriptor *)base);
            } else {
                (void)mimir_platform_map_release(base,
                                                  mapping_length);
            }
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_INTERNAL);
    }
    usable_size = mapping_length - usable_offset;
    if (usable_size < size) {
        if (from_pool) {
                pool_insert_locked(
                    (struct mimir_descriptor *)base);
            } else {
                (void)mimir_platform_map_release(base,
                                                  mapping_length);
            }
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_INTERNAL);
    }

    /* B-007 A9: pointer-range check, before the addition is
     * ever evaluated on the return path: base + usable_offset
     * must not wrap the address space. (C-004: this check was
     * discharged only by construction in C-003 -- A8 plus the
     * live-mapping extent; it is now stated in code, as A9
     * requires, before publication.) On failure the mapping
     * is released and nothing is published (A.3). */
    if (usable_offset > (size_t)(UINTPTR_MAX - (uintptr_t)base)) {
        if (from_pool) {
                pool_insert_locked(
                    (struct mimir_descriptor *)base);
            } else {
                (void)mimir_platform_map_release(base,
                                                  mapping_length);
            }
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_INTERNAL);
    }

    /* B-007 A10: exact counter checks, before publication. On
     * failure the mapping is released and nothing is
     * published (A.3: registry and counters unchanged).
     * (C-004: the live_mappings increment is now checked too;
     * C-003 checked only the live_bytes addition.) */
    if (mapping_length > SIZE_MAX - mimir_live_bytes ||
        mimir_live_mappings == SIZE_MAX) {
        if (from_pool) {
                pool_insert_locked(
                    (struct mimir_descriptor *)base);
            } else {
                (void)mimir_platform_map_release(base,
                                                  mapping_length);
            }
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_INTERNAL);
    }

    /* Publication (B-006 S11.1 step 8; B-007 C1.4): write the
     * descriptor in full, then link it and update counters in
     * this same locked section. B-007 A9 is discharged by the
     * explicit pointer-range check above (C-004), backed by
     * usable_offset <= mapping_length and the whole extent
     * being one live mapping. */
    desc = (struct mimir_descriptor *)base;
    desc->magic = MIMIR_DESCRIPTOR_MAGIC;
    desc->mapping_base = base;
    desc->mapping_length = mapping_length;
    desc->usable_offset = usable_offset;
    desc->requested_size = size;
    desc->usable_size = usable_size;
    desc->state = MIMIR_STATE_LIVE;
    desc->flags = 0;
    memset(desc->reserved, 0, sizeof desc->reserved);
    {   /* J-002: record the allocating thread in the
         * reserve page past the descriptor (INC-J002-01);
         * desc->flags and desc->reserved stay zero (C1.4). */
        thrd_t j002_self = thrd_current();
        memcpy((unsigned char *)base + MIMIR_OWNER_THREAD_OFFSET,
               &j002_self, sizeof j002_self);
    }

    /* Zeroing (calloc only; B-008 S3.2 step 5, S3.3): the
     * usable extent is zeroed explicitly after the mapping
     * succeeds and before publication -- the design does
     * not rest the zeroing promise on the host's
     * fresh-mapping property. The descriptor sits in the
     * reserve page, disjoint from the usable extent, so
     * this write cannot disturb it. For plain allocations
     * (zero_user == 0) user bytes are never touched
     * (B-006 S9.2, B-007 C1.2). */
    if (zero_user) {
        memset((void *)((unsigned char *)base + usable_offset),
               0, usable_size);
    }

    registry_insert(desc);
    mimir_live_mappings += 1;
    mimir_live_bytes += mapping_length;
    __atomic_fetch_add(&mimir_audit_alloc_published, 1UL,
                       __ATOMIC_RELAXED);
    /* J-002: one new block published -- requested bytes are
     * the admitted size, allocated bytes the usable extent
     * actually granted, and Q-MAPPED is re-sampled for the
     * peak in this same locked section. */
    mimir_stats_bytes_requested += size;
    mimir_stats_bytes_allocated += usable_size;
    mimir_stats_peak_update_locked();

    if (out_desc != NULL) {
        *out_desc = desc;
    }

    /* User pointer: mapping_base + usable_offset -- page
     * aligned by construction, hence suitably aligned by
     * analogy to MA-6.1-MALLOC-01 (B-006 S7.1). The usable
     * bytes are the caller's alone once the caller of this
     * helper returns; this layer never touches them while
     * caller-owned (B-006 S9.2, B-007 C1.2; the calloc
     * zeroing above and the realloc transfer copy are the
     * documented construction/transfer exceptions,
     * B-008 S3.3 / S4.5). */
    return (void *)((unsigned char *)base + usable_offset);
}

/*
 * Z-074: aligned construction with the global lock already
 * held, for alignments above the system page size (the plain
 * alloc_locked above already returns a page-aligned user
 * pointer, which satisfies every power-of-two alignment up
 * to the page size; mimir_core_aligned_alloc routes those
 * requests there and only larger alignments reach this
 * variant). The construction is alloc_locked's, with two
 * differences, both forced by the geometry:
 *   - the mapping is acquired for the worst-case total
 *     alignment + page_size + size (checked before evaluate,
 *     B-007 A3/A4 forms), because the aligned user address
 *     is round_up(base + page_size, alignment) and the
 *     offset it lands at is only known once the kernel has
 *     chosen base: usable_offset lies in
 *     [page_size, page_size + alignment) and the descriptor
 *     page (the first page of the mapping) is never inside
 *     the usable extent;
 *   - the Z-048 mapping cache is not consulted: a pooled
 *     mapping's length would have to match this call's
 *     alignment-dependent total exactly, and the publication
 *     below is the identical code either way. Retirement of
 *     an aligned block is the ordinary retire_locked course
 *     (whole mapping_length from mapping_base), so a freed
 *     aligned mapping may still ENTER the pool and be
 *     republished later by any path -- every publication
 *     rewrites usable_offset in full.
 * descriptor_is_live admits this shape unchanged: it
 * requires only usable_offset != 0, usable_offset <=
 * mapping_length, usable_size == mapping_length -
 * usable_offset and requested_size <= usable_size, all of
 * which the publication below writes exactly. On any
 * failure nothing is published and registry and counters
 * are unchanged (A.3), exactly as in alloc_locked.
 */
static void *alloc_aligned_locked(size_t alignment, size_t size)
{
    size_t page_size;
    size_t total_unrounded;
    size_t usable_offset;
    size_t usable_size;
    void *base = NULL;
    size_t mapping_length = 0;
    struct mimir_descriptor *desc;
    int status;
    uintptr_t first_usable;
    uintptr_t user_addr;

    status = mimir_platform_page_size(&page_size);
    if (status != MIMIR_PLATFORM_OK || page_size == 0) {
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_PAGESIZE);
    }
    /* Checked arithmetic, check before evaluate: the
     * worst-case total is alignment + page_size + size.
     * A syntactically valid alignment whose worst-case
     * total cannot be represented is an unsatisfiable
     * request (failure), never an invalid one. */
    if (alignment > SIZE_MAX - page_size ||
        size > SIZE_MAX - page_size - alignment) {
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_ARITHMETIC);
    }
    total_unrounded = alignment + page_size + size;

    status = mimir_platform_map_acquire(total_unrounded,
                                        &base,
                                        &mapping_length);
    if (status != MIMIR_PLATFORM_OK) {
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_BACKING);
    }

    /* The first address past the descriptor page, rounded
     * up to the requested alignment. alignment is a power
     * of two (admission checked it), so the mask form is
     * exact; the wrap guard is defensive -- a mapping of
     * the acquired length exists, so base + page_size is a
     * real address and its round-up stays inside the
     * mapping or the usable-size gate below refuses. */
    first_usable = (uintptr_t)base + page_size;
    user_addr = (first_usable + (alignment - 1)) &
                ~(uintptr_t)(alignment - 1);
    if (user_addr < first_usable) {
        (void)mimir_platform_map_release(base, mapping_length);
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_INTERNAL);
    }
    usable_offset = (size_t)(user_addr - (uintptr_t)base);

    if (usable_offset > mapping_length) {
        (void)mimir_platform_map_release(base, mapping_length);
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_INTERNAL);
    }
    usable_size = mapping_length - usable_offset;
    if (usable_size < size) {
        (void)mimir_platform_map_release(base, mapping_length);
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_INTERNAL);
    }
    if (mapping_length > SIZE_MAX - mimir_live_bytes ||
        mimir_live_mappings == SIZE_MAX) {
        (void)mimir_platform_map_release(base, mapping_length);
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_INTERNAL);
    }

    /* Publication, identical to alloc_locked (B-006 S11.1
     * step 8; B-007 C1.4) apart from the usable_offset. */
    desc = (struct mimir_descriptor *)base;
    desc->magic = MIMIR_DESCRIPTOR_MAGIC;
    desc->mapping_base = base;
    desc->mapping_length = mapping_length;
    desc->usable_offset = usable_offset;
    desc->requested_size = size;
    desc->usable_size = usable_size;
    desc->state = MIMIR_STATE_LIVE;
    desc->flags = 0;
    memset(desc->reserved, 0, sizeof desc->reserved);
    {
        thrd_t j002_self = thrd_current();
        memcpy((unsigned char *)base + MIMIR_OWNER_THREAD_OFFSET,
               &j002_self, sizeof j002_self);
    }

    registry_insert(desc);
    mimir_live_mappings += 1;
    mimir_live_bytes += mapping_length;
    __atomic_fetch_add(&mimir_audit_alloc_published, 1UL,
                       __ATOMIC_RELAXED);
    mimir_stats_bytes_requested += size;
    mimir_stats_bytes_allocated += usable_size;
    mimir_stats_peak_update_locked();

    return (void *)user_addr;
}

/*
 * G-003 addendum (Phase 7 small-object path, per the banked
 * G-002 design docs/PHASE_7_SIZE_CLASSES_G002.md): requests
 * of 1..1024 bytes are served from size-class spans carved
 * out of one system page each; every request above the
 * 1024-byte cutoff takes the banked mapping-per-allocation
 * path above, unchanged (G-002 R-1). The span machinery
 * below runs under the same one L6 global lock, in the same
 * locked sections as the mapping registry discipline, and
 * publishes/frees nothing outside them. L3 remains the
 * banked pass-through (G-002 C-5): class selection lives
 * here in L4, spans are owned by L5-in-this-file, and no
 * thread-local cache exists in this slice (INC-G003-04).
 *
 * Span header (G-002 S7, INC-G002-04 assignment made by
 * this slice): exactly 96 bytes at offset 0 of the span's
 * single system page, following the B-006 S7.3 descriptor
 * discipline and sharing its field offsets for the common
 * prefix, so the offsets the mapping path's validation
 * relies on never move:
 *    0  8  magic (span magic, distinct from the mapping
 *               descriptor magic so the two registries can
 *               never be confused for each other)
 *    8  8  mapping_base (the span's own address)
 *   16  8  mapping_length (exactly one system page)
 *   24  8  usable_offset (96: the first object offset)
 *   32  8  requested_size -- REUSED as the span's class
 *               size (the span serves exactly one class
 *               for its whole lifetime, G-002 S7)
 *   40  8  usable_size (mapping_length - 96, as in the
 *               mapping descriptor's own arithmetic)
 *   48  4  state (LIVE / RETIRED, same values as S7.3)
 *   52  4  flags -- ASSIGNED: the class index (0..19)
 *   56  8  list_next / 64 8 list_prev (span registry)
 *   72  8  free_head (first free object of this span, or
 *               NULL; free objects thread their own first
 *               8 bytes, G-002 C-8)
 *   80  8  live_count (objects currently handed out)
 *   88  8  carve_count (objects ever carved; objects are
 *               carved sequentially from offset 96 at the
 *               class stride until capacity, G-002 S7)
 * The header never grows: 96 bytes total, static-asserted.
 *
 * Bounded reuse (the Phase 7 element this slice owns):
 * a freed span object goes onto its span's free list and
 * is the first object its class hands out again; a span
 * whose live_count returns to 0 is an EMPTY span, and at
 * most mimir_retention_bound empty spans per class are
 * retained -- the empty span that would exceed
 * the bound is released whole via L6 with its exact
 * acquired pair in the same locked section (INC-G003-05).
 * Live spans are bounded by live demand, never by cache:
 * retained-after-free memory is therefore at most
 * 20 classes x 2 spans x one system page.
 *
 * Counters (G-002 R-5 / INC-G002-09): span-served objects
 * are NOT mappings and are never folded into the E-005
 * mapping counters above; the four counters below count
 * span publications/retirements and small-object
 * publications/retirements beside them, in the same
 * INC-E001-07 shape (b): internal audit state, declared
 * in no installed header, readable only by a test-only
 * static build (tests/core/mimir_small_g003.h), bound
 * local in the shared library. published - retired, for
 * each pair, is the live count of what it names.
 */

#define MIMIR_SMALL_CUTOFF ((size_t)1024)
#define MIMIR_SMALL_CLASS_COUNT 20
#define MIMIR_SPAN_HEADER_SIZE ((size_t)96)
/* G-007 tuning state (docs/PHASE_7_TUNING_PARAMETERS_G007.md):
 * the per-class empty-span retention bound of RP-2, formerly
 * the compile-time constant 2, is now a single size_t stored
 * under the same global-lock discipline as the span registry
 * it governs. Its DEFAULT is exactly the banked value 2, so
 * an unconfigured process behaves byte-for-byte as G-003 /
 * G-005 banked. The valid range is 0..16: 0 disables
 * retention (every empty span is released at free time);
 * 16 caps retained-after-free memory at 20 classes x 16
 * spans x one system page (INC-G007-02). The setter/getter
 * pair below (mimir_tune_set_retention_bound /
 * mimir_tune_get_retention_bound) is INC-E001-07 shape (b)
 * internal state: declared only in the test-only header
 * tests/core/mimir_tune_g007.h, bound local in the shared
 * library, never part of the public ABI. */
#define MIMIR_RETENTION_DEFAULT ((size_t)2)
#define MIMIR_RETENTION_MAX ((size_t)16)
static size_t mimir_retention_bound = MIMIR_RETENTION_DEFAULT;

/* Span magic: the bytes spell "MIMIRSPN" in ASCII. A sanity
 * check only, exactly as the descriptor magic is (S7.3). */
#define MIMIR_SPAN_MAGIC UINT64_C(0x4D494D495253504E)

static const size_t mimir_class_sizes[MIMIR_SMALL_CLASS_COUNT] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};

struct mimir_span_header {
    uint64_t magic;
    void *mapping_base;
    size_t mapping_length;
    size_t usable_offset;
    size_t class_size;
    size_t usable_size;
    uint32_t state;
    uint32_t class_index;
    struct mimir_span_header *list_next;
    struct mimir_span_header *list_prev;
    void *free_head;
    size_t live_count;
    size_t carve_count;
};

_Static_assert(sizeof(struct mimir_span_header) == 96,
               "span header must be exactly 96 bytes (G-002 S7)");
_Static_assert(offsetof(struct mimir_span_header, mapping_base) == 8,
               "span mapping_base offset (B-006 S7.3 discipline)");
_Static_assert(offsetof(struct mimir_span_header, state) == 48,
               "span state offset (B-006 S7.3 discipline)");
_Static_assert(offsetof(struct mimir_span_header, list_next) == 56,
               "span list_next offset (B-006 S7.3 discipline)");
_Static_assert(offsetof(struct mimir_span_header, free_head) == 72,
               "span free_head offset (G-003 assignment, INC-G002-04)");
_Static_assert(offsetof(struct mimir_span_header, live_count) == 80,
               "span live_count offset (G-003 assignment)");
_Static_assert(offsetof(struct mimir_span_header, carve_count) == 88,
               "span carve_count offset (G-003 assignment)");

/*
 * J-004 debug-only red zones + canaries (Phase 10 plan S5.2;
 * slice record docs/PHASE_10_DEBUG_REDZONES.md). Compiled
 * ONLY under MIMIR_DEBUG (make DEBUG=1, build-debug/); a
 * production build contains none of this code at all (S5.1:
 * no runtime flag, distinction by directory and macro).
 *
 * Debug small-slot layout (span object area, from offset 96):
 *   [ 16 B pre-zone ][ user extent: class_size B ][ 16 B post-zone ]
 * repeated at a stride of class_size + 32. Both zones are
 * filled with the pattern byte 0xA5 at publication, checked
 * on free and on realloc, and re-checked on demand by
 * mimir_core_debug_verify_all(). The pre-zone of slot 0 sits
 * immediately after the 96-byte span header (offset 96), so
 * the header-adjacent guard is the metadata-adjacent canary
 * the layout permits; verify_all additionally revalidates
 * every span header (magic/state/base/class) as the
 * metadata canary check. User pointers stay 16-byte aligned:
 * page base + 96 + 16 + k*(class+32), every term a multiple
 * of 16. J-002 counter semantics are untouched: requested /
 * allocated still count the admitted size / class extent;
 * only span CAPACITY changes, reported as overhead (S5.2).
 * The mapping (large) path carries NO red zones in this
 * slice -- scoped out, INC-J004-01.
 *
 * Detection posture (plan S9.2: fail clearly, never pretend
 * to repair): on a guard mismatch the allocator writes one
 * fixed-format line to fd 2 from a fixed static buffer via
 * write(2) -- no allocation, no stdio, no lock recursion
 * (the global lock is already held; write(2) never calls
 * back) -- then abort()s: a defined stop, in the calling
 * thread, with the corrupt block NOT freed, NOT reused and
 * NOT repaired. This pre-figures J-007's reporting contract
 * (allocation-free, recursion-proof) as far as J-001 S5
 * defines it; J-007 owns the production channel.
 */
#ifdef MIMIR_DEBUG
#define MIMIR_DBG_GUARD_SIZE ((size_t)16)
#define MIMIR_DBG_PATTERN 0xA5

static size_t mimir_dbg_stride(size_t class_size)
{
    return class_size + 2 * MIMIR_DBG_GUARD_SIZE;
}

static void mimir_dbg_fill(void *obj, size_t class_size)
{
    memset((unsigned char *)obj - MIMIR_DBG_GUARD_SIZE,
           MIMIR_DBG_PATTERN, MIMIR_DBG_GUARD_SIZE);
    memset((unsigned char *)obj + class_size,
           MIMIR_DBG_PATTERN, MIMIR_DBG_GUARD_SIZE);
}

static int mimir_dbg_intact(const void *obj, size_t class_size)
{
    const unsigned char *pre =
        (const unsigned char *)obj - MIMIR_DBG_GUARD_SIZE;
    const unsigned char *post =
        (const unsigned char *)obj + class_size;
    size_t i;

    for (i = 0; i < MIMIR_DBG_GUARD_SIZE; i++) {
        if (pre[i] != MIMIR_DBG_PATTERN ||
            post[i] != MIMIR_DBG_PATTERN) {
            return 0;
        }
    }
    return 1;
}

/* One fixed-format line, fixed static storage, write(2)
 * only; then the defined stop. Never returns. */
static _Noreturn void mimir_dbg_report_abort(const char *where,
                                             const void *obj)
{
    static const char head[] =
        "MIMIR_DEBUG J-004 CORRUPTION red-zone mismatch at ";
    static const char mid[] = " obj=0x";
    static char buf[160];
    static const char hexd[] = "0123456789abcdef";
    uintptr_t v = (uintptr_t)obj;
    size_t n = 0;
    size_t i;
    int shift;

    for (i = 0; head[i] != '\0' && n < sizeof buf; i++) {
        buf[n++] = head[i];
    }
    for (i = 0; where[i] != '\0' && n < sizeof buf; i++) {
        buf[n++] = where[i];
    }
    for (i = 0; mid[i] != '\0' && n < sizeof buf; i++) {
        buf[n++] = mid[i];
    }
    for (shift = (int)(sizeof(uintptr_t) * 8) - 4;
         shift >= 0 && n < sizeof buf; shift -= 4) {
        buf[n++] = hexd[(v >> shift) & 0xF];
    }
    if (n < sizeof buf) {
        buf[n++] = '\n';
    }
    (void)!write(2, buf, n);
    abort();
}

static void mimir_dbg_check_or_abort(const void *obj,
                                     size_t class_size,
                                     const char *where)
{
    if (!mimir_dbg_intact(obj, class_size)) {
        mimir_dbg_report_abort(where, obj);
    }
}
#endif /* MIMIR_DEBUG */

/*
 * Z-039 geometry B -- lock-sharding of the central core
 * (MIMIRALLOC_Z13_ROADMAP.md Wave 5, slice Z-039 follow-up;
 * decision D-008 in docs/DECISION_LEDGER.md). Everything in
 * this section exists ONLY under MIMIR_LOCK_SHARD. A
 * production build never defines it: no code, no data, no
 * symbol, no cost (proof in
 * tests/perf/recorded_run_z13_z039_followup.txt, the
 * Z-019/Z-034/Z-037 zero-cost pattern). Combination refusals
 * (below): MIMIR_DEBUG (the debug layout owns its own
 * serving paths and bypasses the TLS cache this geometry
 * shards around), MIMIR_ARENA_CCD (two partition geometries
 * at once are future work, exactly as Z-037 recorded for
 * its own combinations), MIMIR_OOL_META (the trial's free
 * path is its own), MIMIR_LOCK_HIST (the histograms
 * instrument the ONE lock; a sharded instrument is future
 * work).
 *
 * Design (the Z-039 recorded run's paper geometry B,
 * applied by decision rule R2 to Z-038's banked ladder):
 * the singleton core keeps ONE set of registries, but the
 * span registry is split into one list per shard and the
 * one global lock is joined by K = 4 shard locks keyed by
 * size class: shard 0 = class 16 B, shard 1 = class 32 B,
 * shard 2 = classes 48 B + 64 B, shard 3 = classes
 * 80 B..1024 B (the hot small classes each own a shard;
 * the cold classes share). A class's spans, its active
 * hint, its TLS tally table and its free lists are guarded
 * by that class's shard ONLY. The mapping registry keeps
 * the global lock. FIXED LOCK ORDER, audited in L6: shards
 * in ascending index, the global lock LAST -- never the
 * reverse, never a shard while a higher shard is held.
 * Operations that genuinely span shards (the caller's TLS
 * pending/handed flush, realloc, the large path, stats /
 * metadata / drain / trim snapshots) acquire ALL shards
 * ascending and then the global lock: in this build the
 * existing entry points' global-lock calls resolve to
 * exactly that acquisition (shard_lock_global_all below),
 * so the banked global sections stay correct by superset,
 * and the two-class rule of the paper design is honoured
 * by the same ascending order. The paths the ladder
 * measures take exactly one shard: a TLS refill/drain
 * batch for one class, a central small alloc/free for one
 * class, and a cross-thread free (which searches the shard
 * lists in ascending order and keeps only the found
 * span's shard). Shared counters that shard-held sections
 * update (live spans/bytes/objects, the J-002 byte totals)
 * are _Atomic in this build; the peak sampler is a CAS
 * max. mimir_shard_hint is the serving thread's current
 * shard, a _Thread_local scratch set by every serving
 * helper before it touches the registry macro below --
 * under sharding there is no single lock to serialise a
 * shared hint, so the hint is per-thread by construction.
 */
#ifdef MIMIR_LOCK_SHARD
#if defined(MIMIR_DEBUG) || defined(MIMIR_ARENA_CCD) || \
    defined(MIMIR_OOL_META) || defined(MIMIR_LOCK_HIST)
#error "MIMIR_LOCK_SHARD is not defined in combination with MIMIR_DEBUG, MIMIR_ARENA_CCD, MIMIR_OOL_META or MIMIR_LOCK_HIST (Z-039 follow-up)"
#endif
_Static_assert(MIMIR_SHARD_COUNT == MIMIR_PLATFORM_SHARD_COUNT,
               "core and platform shard counts must agree (Z-039)");

static _Thread_local size_t mimir_shard_hint;

static size_t shard_of_class(size_t class_index)
{
    if (class_index < 2) {
        return class_index; /* 16 B -> 0, 32 B -> 1 */
    }
    if (class_index < 4) {
        return 2; /* 48 B, 64 B */
    }
    return 3; /* 80 B..1024 B */
}

size_t mimir_shard_of_class(size_t class_index)
{
    return shard_of_class(class_index);
}

int mimir_shard_snapshot(struct mimir_shard_snapshot *out)
{
    size_t s;

    if (out == NULL) {
        return -1;
    }
    for (s = 0; s < MIMIR_SHARD_COUNT; s++) {
        out->acquisitions[s] =
            mimir_platform_shard_acquisitions(s);
    }
    out->order_violations =
        mimir_platform_shard_order_violations();
    return 0;
}

/* Z-064 (D-01): the counted per-shard unlock. Every
 * per-shard unlock in this file goes through here, so a
 * failed shard unlock moves the same tally as a failed
 * global unlock. Defined before shard_lock_all, whose
 * rollback path is one of its call sites. */
static int core_shard_unlock(size_t shard)
{
    int rc = (mimir_platform_shard_unlock)(shard);

    if (rc != MIMIR_PLATFORM_OK) {
        mimir_stats_note_unlock_failure();
    }
    return rc;
}

static int shard_lock_all(void)
{
    size_t s;

    for (s = 0; s < MIMIR_SHARD_COUNT; s++) {
        if (mimir_platform_shard_lock(s) != MIMIR_PLATFORM_OK) {
            while (s > 0) {
                s--;
                (void)core_shard_unlock(s);
            }
            return -1;
        }
    }
    return 0;
}

static void shard_unlock_all(void)
{
    size_t s = MIMIR_SHARD_COUNT;

    while (s > 0) {
        s--;
        (void)core_shard_unlock(s);
    }
}

/* The all-shards-then-global acquisition: what every
 * banked global-lock call site means in this build. The
 * two helpers are defined BEFORE the macro pair below so
 * their bodies still name the real L6 primitives. */
static int shard_lock_global_all(void)
{
    if (shard_lock_all() != 0) {
        return MIMIR_PLATFORM_ERR_LOCK;
    }
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        shard_unlock_all();
        return MIMIR_PLATFORM_ERR_LOCK;
    }
    return MIMIR_PLATFORM_OK;
}

static int shard_unlock_global_all(void)
{
    int rc = mimir_platform_global_unlock();

    shard_unlock_all();
    return rc;
}

#define mimir_platform_global_lock shard_lock_global_all
#define mimir_platform_global_unlock shard_unlock_global_all
#endif /* MIMIR_LOCK_SHARD */

/* ---- Z-064 lock-failure observability (D-01) ------------
 * Every central global unlock in this file goes through
 * core_global_unlock: it returns the L6 status to callers
 * that want it and, on failure, increments the Z-064 tally
 * at the unlock site, before any other action. Call sites
 * keep their (void) posture -- counting is the
 * observability D-01 owed (INC-M002-01: every unlock
 * discarded its status), not a new failure mode; no caller
 * gains a wedge it did not already have. The name it calls
 * is the file's current macro meaning of the global unlock:
 * the real L6 entry in a production build,
 * shard_unlock_global_all in a MIMIR_LOCK_SHARD build (the
 * composite's own body still names the real primitives and
 * leaves counting to its callers, so a composite failure is
 * counted exactly once, here), and z19_unlock in a
 * MIMIR_LOCK_HIST build (which forwards to the real entry
 * and returns its status, so a failure is likewise counted
 * exactly once, here). Defined after the shard section and
 * before every ordinary call site. */
static int core_global_unlock(void)
{
    int rc = mimir_platform_global_unlock();

    if (rc != MIMIR_PLATFORM_OK) {
        mimir_stats_note_unlock_failure();
    }
    return rc;
}

/* Span registry anchors and counters: fixed static storage
 * below L1, exactly as the mapping registry's are (B-006
 * S7.2 / S8.1). A span header is on this list if and only
 * if its state is LIVE. mimir_class_active is a per-class
 * hint for the span to try first; correctness never rests
 * on it -- the registry walk is authoritative. */
#ifndef MIMIR_LOCK_SHARD
static struct mimir_span_header *mimir_span_head; /* NULL: empty */
#else
/* Z-039: one span list per shard; every pre-Z-039 use of
 * mimir_span_head resolves through this macro to the list
 * of the shard named by the serving thread's hint, which
 * the serving helpers set before touching the registry. */
static struct mimir_span_header
    *mimir_shard_span_head[MIMIR_SHARD_COUNT];
#define mimir_span_head (mimir_shard_span_head[mimir_shard_hint])
#endif
#ifndef MIMIR_ARENA_CCD
static struct mimir_span_header *mimir_class_active[MIMIR_SMALL_CLASS_COUNT];
#else
/* Z-037: the active-span hint is per arena (see the arena
 * section below, beside span_release_locked). Every
 * pre-Z-037 use of mimir_class_active[class] resolves
 * through this macro to the slot of the arena named by
 * the lock-held scratch mimir_arena_hint, which the
 * serving entry points set before touching the hint.
 * span_release_locked overrides the hint with the span's
 * owner arena around its own active-slot writes. */
static struct mimir_span_header
    *mimir_arena_active[MIMIR_ARENA_COUNT][MIMIR_SMALL_CLASS_COUNT];
static size_t mimir_arena_hint;
#define mimir_class_active (mimir_arena_active[mimir_arena_hint])
#endif
#ifdef MIMIR_LOCK_SHARD
static _Atomic size_t mimir_live_spans;
static _Atomic size_t mimir_live_span_bytes;
static _Atomic size_t mimir_live_small_objects;
#else
#ifdef MIMIR_LOCK_SHARD
static _Atomic size_t mimir_live_spans;
static _Atomic size_t mimir_live_span_bytes;
static _Atomic size_t mimir_live_small_objects;
#else
static size_t mimir_live_spans;
static size_t mimir_live_span_bytes;
static size_t mimir_live_small_objects;
#endif
#endif

unsigned long mimir_audit_span_published;
unsigned long mimir_audit_span_retired;
_Alignas(64) unsigned long mimir_audit_small_alloc; /* Z-040: own 64 B line - lock-free on the TLS hit path */
_Alignas(64) unsigned long mimir_audit_small_free; /* Z-040: own 64 B line - lock-free on the free-to-bin path */

/* Smallest class index whose class size is >= size.
 * Caller guarantees 1 <= size <= MIMIR_SMALL_CUTOFF, so the
 * search always succeeds. */
static size_t class_index_for(size_t size)
{
    size_t i;

    for (i = 0; i < MIMIR_SMALL_CLASS_COUNT; i++) {
        if (size <= mimir_class_sizes[i]) {
            return i;
        }
    }
    return MIMIR_SMALL_CLASS_COUNT - 1; /* unreachable */
}

/* Objects a span of this header's class can ever hold:
 * the G-002 Part C packing arithmetic, computed from the
 * header's own recorded mapping length. */
static size_t span_capacity(const struct mimir_span_header *span)
{
#ifdef MIMIR_DEBUG
    /* J-004: debug slots occupy stride (class + 32 guard
     * bytes), so a debug span holds fewer objects -- the
     * capacity overhead reported in the slice doc. */
    return (span->mapping_length - MIMIR_SPAN_HEADER_SIZE) /
           mimir_dbg_stride(span->class_size);
#else
    return (span->mapping_length - MIMIR_SPAN_HEADER_SIZE) /
           span->class_size;
#endif
}

/*
 * Z-034 out-of-line metadata trial (MIMIRALLOC_Z13_ROADMAP.md
 * Wave 4, slice Z-034 "Metadata diet"). Everything in this
 * section exists ONLY under MIMIR_OOL_META. A production
 * build never defines it: no code, no data, no symbol, no
 * cost (proof in tests/perf/recorded_run_z13_z034.txt, the
 * Z-019 zero-cost pattern). MIMIR_DEBUG builds are refused:
 * the debug red-zone/quarantine layout owns its own free
 * path and the trial does not re-derive it.
 *
 * Trial design: for the four smallest classes (indices
 * 0..3: 16/32/48/64 bytes) the free list leaves the freed
 * blocks entirely. Liveness moves to a bitmap in ONE side
 * page acquired per span at span creation (bit set = live;
 * words = ceil(capacity/64), at the side page base). Free
 * writes NO byte of the block; alloc reads NO byte of any
 * block -- the in-band link read/write pair is replaced by
 * a bitmap scan in the side page. The span header stays
 * exactly 96 bytes, object offsets and capacities are
 * unchanged, free_head stays NULL for trial spans, and all
 * other classes keep the banked in-band path untouched.
 * The side page is the trial's own metadata bill: one
 * extra page per trial-class span, recorded in the slice's
 * header-cost table, never hidden.
 */
#ifdef MIMIR_OOL_META
#ifdef MIMIR_DEBUG
#error "MIMIR_OOL_META trial is not defined for MIMIR_DEBUG builds (Z-034)"
#endif

#define MIMIR_OOL_MAX_CLASS_INDEX ((size_t)3) /* 16/32/48/64 B */
#define MIMIR_OOL_SLOTS ((size_t)512)

struct mimir_ool_slot {
    struct mimir_span_header *span; /* NULL empty, (void*)-1 tombstone */
    void *page_base;
    size_t page_len;
};

static struct mimir_ool_slot mimir_ool_table[MIMIR_OOL_SLOTS];

static int ool_class_is(size_t class_index)
{
    return class_index <= MIMIR_OOL_MAX_CLASS_INDEX;
}

static int ool_span_is(const struct mimir_span_header *span)
{
    return span != NULL && ool_class_is(span->class_index);
}

static size_t ool_hash(const struct mimir_span_header *span)
{
    return ((uintptr_t)span >> 12) & (MIMIR_OOL_SLOTS - 1);
}

/* Side-page bitmap for span, or NULL when unregistered. */
static uint64_t *ool_bits(const struct mimir_span_header *span)
{
    size_t i;
    size_t h = ool_hash(span);

    for (i = 0; i < MIMIR_OOL_SLOTS; i++) {
        struct mimir_ool_slot *s =
            &mimir_ool_table[(h + i) & (MIMIR_OOL_SLOTS - 1)];
        if (s->span == span) {
            return (uint64_t *)s->page_base;
        }
        if (s->span == NULL) {
            return NULL;
        }
    }
    return NULL;
}

static int ool_register(struct mimir_span_header *span,
                        void *page_base, size_t page_len)
{
    size_t i;
    size_t h = ool_hash(span);

    for (i = 0; i < MIMIR_OOL_SLOTS; i++) {
        struct mimir_ool_slot *s =
            &mimir_ool_table[(h + i) & (MIMIR_OOL_SLOTS - 1)];
        if (s->span == NULL || s->span == (void *)(uintptr_t)-1) {
            s->span = span;
            s->page_base = page_base;
            s->page_len = page_len;
            memset(page_base, 0, page_len);
            return 1;
        }
    }
    return 0;
}

/* Remove span's registration; returns the side page pair
 * (NULL base when unregistered). */
static void ool_unregister(struct mimir_span_header *span,
                           void **out_base, size_t *out_len)
{
    size_t i;
    size_t h = ool_hash(span);

    *out_base = NULL;
    *out_len = 0;
    for (i = 0; i < MIMIR_OOL_SLOTS; i++) {
        struct mimir_ool_slot *s =
            &mimir_ool_table[(h + i) & (MIMIR_OOL_SLOTS - 1)];
        if (s->span == span) {
            *out_base = s->page_base;
            *out_len = s->page_len;
            s->span = (void *)(uintptr_t)-1;
            s->page_base = NULL;
            s->page_len = 0;
            return;
        }
        if (s->span == NULL) {
            return;
        }
    }
}

static int ool_bit_test(const struct mimir_span_header *span,
                        size_t idx)
{
    uint64_t *bits = ool_bits(span);

    if (bits == NULL) {
        return 0;
    }
    return (int)((bits[idx >> 6] >> (idx & 63)) & UINT64_C(1));
}

/* Take one object index of a trial span (lock held): the
 * lowest free carved index, else a fresh carve. Sets the
 * bit (and advances carve_count on the carve road).
 * Returns (size_t)-1 when the span cannot serve. */
static size_t ool_take_locked(struct mimir_span_header *span)
{
    uint64_t *bits = ool_bits(span);
    size_t idx;

    if (bits == NULL) {
        return (size_t)-1;
    }
    for (idx = 0; idx < span->carve_count; idx++) {
        if (!((bits[idx >> 6] >> (idx & 63)) & UINT64_C(1))) {
            bits[idx >> 6] |= UINT64_C(1) << (idx & 63);
            return idx;
        }
    }
    if (span->carve_count < span_capacity(span)) {
        idx = span->carve_count;
        span->carve_count += 1;
        bits[idx >> 6] |= UINT64_C(1) << (idx & 63);
        return idx;
    }
    return (size_t)-1;
}

static void ool_clear_locked(struct mimir_span_header *span,
                             const void *ptr)
{
    uint64_t *bits = ool_bits(span);
    size_t idx;

    if (bits == NULL) {
        return;
    }
    idx = (size_t)((const unsigned char *)ptr -
                   (const unsigned char *)span->mapping_base -
                   MIMIR_SPAN_HEADER_SIZE) /
          span->class_size;
    bits[idx >> 6] &= ~(UINT64_C(1) << (idx & 63));
}

static size_t ool_popcount_locked(
    const struct mimir_span_header *span)
{
    uint64_t *bits = ool_bits(span);
    size_t idx;
    size_t n = 0;

    if (bits == NULL) {
        return 0;
    }
    for (idx = 0; idx < span->carve_count; idx++) {
        n += (size_t)((bits[idx >> 6] >> (idx & 63)) & UINT64_C(1));
    }
    return n;
}
#endif /* MIMIR_OOL_META */

static void span_registry_insert(struct mimir_span_header *span)
{
    span->list_prev = NULL;
    span->list_next = mimir_span_head;
    if (mimir_span_head != NULL) {
        mimir_span_head->list_prev = span;
    }
    mimir_span_head = span;
}

static void span_registry_remove(struct mimir_span_header *span)
{
    if (span->list_prev != NULL) {
        span->list_prev->list_next = span->list_next;
    } else {
        mimir_span_head = span->list_next;
    }
    if (span->list_next != NULL) {
        span->list_next->list_prev = span->list_prev;
    }
    span->list_next = NULL;
    span->list_prev = NULL;
}

/* Z-066 (D-12): the span half of INC-J006-02. A span's
 * mapping is ALWAYS exactly one system page:
 * span_acquire_locked acquires page_size and refuses
 * (mapping_length != page_size) before publication, and
 * no valid code ever writes a span's mapping_length
 * afterwards. The acquired page size is therefore an
 * independent record of every live span's true length,
 * kept here (written once, at the first span
 * publication) so the validators can hold the header to
 * it: a mapping_length/usable_size pair corrupted
 * CONSISTENTLY (the J-006 span-len-consist probe doubled
 * both and every J-006 relation still passed) now fails
 * validation, and the span is bypassed for allocation
 * and refused for free exactly like a magic-corrupt
 * span. A zero cache means no span has been published
 * yet in this process, in which case no live span
 * exists to validate and the check is permissive. */
static _Atomic size_t mimir_span_page_size;

static int span_length_is_true(const struct mimir_span_header *span)
{
    size_t page_size = __atomic_load_n(&mimir_span_page_size,
                                       __ATOMIC_RELAXED);
    return page_size == 0 || span->mapping_length == page_size;
}

static int span_header_is_live(const struct mimir_span_header *span)
{
    if (!(span->magic == MIMIR_SPAN_MAGIC &&
          span->state == MIMIR_STATE_LIVE &&
          span->mapping_base == (const void *)span &&
          span->usable_offset == MIMIR_SPAN_HEADER_SIZE &&
          span->class_index < MIMIR_SMALL_CLASS_COUNT &&
          span->class_size == mimir_class_sizes[span->class_index])) {
        return 0;
    }
    /* J-006 (docs/PHASE_10_METADATA_INTEGRITY.md): the exact
     * relations publication writes in span_acquire_locked --
     * the mapping holds the header plus at least one object,
     * usable_size is exactly the mapping past the header,
     * and the carve/live counts are ordered and within the
     * capacity the header's own fields imply
     * (live <= carve <= capacity). Valid spans always
     * satisfy these, so no defined behaviour changes; a
     * span whose length or count fields were corrupted
     * inconsistently after publication now fails here and
     * is refused or bypassed, instead of serving, freeing
     * or releasing on false counts. Consistent corruption
     * still passes (INC-J006-05, INC-J006-06). */
    if (!span_length_is_true(span) ||
        span->mapping_length <
            MIMIR_SPAN_HEADER_SIZE + span->class_size ||
        span->usable_size !=
            span->mapping_length - MIMIR_SPAN_HEADER_SIZE ||
        span->live_count > span->carve_count ||
        span->carve_count > span_capacity(span)) {
        return 0;
    }
    return 1;
}

/*
 * Z-043: the same relations minus the live_count bound,
 * for the free path only. A small block published from a
 * bin hit is accounted by a PENDING +1 in the allocating
 * thread (Z-029 implementation doc section 3), applied
 * to live_count only at that thread's next flush; a
 * cross-thread free of such a block reaches
 * small_free_locked first and applies its -1 immediately.
 * Between the two, live_count reads as a small negative
 * number in size_t wrap, and the strict relation above
 * refuses the span: the Z-043 storm measured 41-92 of
 * ~391k cross frees silently doing nothing that way -
 * the block stayed live forever and the span could never
 * drain. The free path does not need the bound: ownership
 * of the exact block is proven by the containment,
 * stride, carve and free-list checks in the finder below,
 * which are all kept; and the counter arithmetic is
 * exact in the mod-2^64 ring once every transition is
 * applied exactly once, which refusing the free broke.
 * Allocation-side callers keep the strict form.
 */
static int span_header_is_live_for_free(
    const struct mimir_span_header *span)
{
    if (!(span->magic == MIMIR_SPAN_MAGIC &&
          span->state == MIMIR_STATE_LIVE &&
          span->mapping_base == (const void *)span &&
          span->usable_offset == MIMIR_SPAN_HEADER_SIZE &&
          span->class_index < MIMIR_SMALL_CLASS_COUNT &&
          span->class_size == mimir_class_sizes[span->class_index])) {
        return 0;
    }
    if (!span_length_is_true(span) ||
        span->mapping_length <
            MIMIR_SPAN_HEADER_SIZE + span->class_size ||
        span->usable_size !=
            span->mapping_length - MIMIR_SPAN_HEADER_SIZE ||
        span->carve_count > span_capacity(span)) {
        return 0;
    }
    return 1;
}

/* J-006: shape validation for one free-list node of a span,
 * using only the (already validated) header's own fields:
 * a free node is a carved object start inside this span's
 * object area. The node's own bytes are never read here,
 * so a corrupt link value can be rejected without
 * dereferencing it.
 *
 * Banking fix (2026-11-07): the shape is layout-relative.
 * Production carves object k at first + k * class_size;
 * a MIMIR_DEBUG build (J-004, banked on a sibling line)
 * carves it at first + MIMIR_DBG_GUARD_SIZE +
 * k * mimir_dbg_stride(class_size), so the production
 * arithmetic rejected every legitimate debug free node
 * (DEBUG test-j005 reuse leg, 81 checks / 1 failure).
 * The debug branch below validates against the debug
 * placement exactly as the J-004 code defines it, with
 * the same strength: a node must sit exactly on a debug
 * slot start, below carve_count, inside the mapping --
 * guard bytes, slot interiors, uncarved slots and wild
 * values are refused in both layouts. The production
 * path is unchanged. In this slice tree alone there is
 * no MIMIR_DEBUG flavour (J-004 owns it, INC-J006-08);
 * the debug branch compiles where that flavour exists,
 * i.e. in the banked merged tree. */
static int span_free_node_valid(const struct mimir_span_header *span,
                                const void *node)
{
    uintptr_t base;
    uintptr_t addr;
    uintptr_t first;

    if (node == NULL) {
        return 0;
    }
    base = (uintptr_t)span->mapping_base;
    addr = (uintptr_t)node;
#ifdef MIMIR_DEBUG
    first = base + MIMIR_SPAN_HEADER_SIZE + MIMIR_DBG_GUARD_SIZE;
    if (addr < first || addr >= base + span->mapping_length) {
        return 0;
    }
    if ((addr - first) % mimir_dbg_stride(span->class_size) != 0) {
        return 0;
    }
    return (addr - first) / mimir_dbg_stride(span->class_size) <
           span->carve_count;
#else
    first = base + MIMIR_SPAN_HEADER_SIZE;
    if (addr < first || addr >= base + span->mapping_length) {
        return 0;
    }
    if ((addr - first) % span->class_size != 0) {
        return 0;
    }
    return (addr - first) / span->class_size < span->carve_count;
#endif
}

/* J-002 peak sampler: Q-MAPPED only ever grows at the two
 * publication points (alloc_locked above, span publication
 * below), so sampling exactly there under the lock yields
 * the exact high-water mark (I-002 Q-PEAKMAPPED). */
static void mimir_stats_peak_update_locked(void)
{
#ifdef MIMIR_LOCK_SHARD
    /* Z-039: the sampler runs under whichever shard (or the
     * global set) published; the two totals are atomic and
     * the high-water moves by CAS, so concurrent publishers
     * on different shards cannot lose a peak. The value is
     * still only ever sampled at the two publication points. */
    uint64_t mapped =
        (uint64_t)(__atomic_load_n(&mimir_live_bytes,
                                   __ATOMIC_RELAXED) +
                   __atomic_load_n(&mimir_live_span_bytes,
                                   __ATOMIC_RELAXED) +
                   __atomic_load_n(&mimir_pool_bytes,
                                   __ATOMIC_RELAXED)); /* Z-048 */
    uint64_t cur = __atomic_load_n(&mimir_stats_peak_mapped,
                                   __ATOMIC_RELAXED);
    while (mapped > cur &&
           !__atomic_compare_exchange_n(
               &mimir_stats_peak_mapped, &cur, mapped, 0,
               __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
    }
#else
    uint64_t mapped =
        (uint64_t)(mimir_live_bytes + mimir_live_span_bytes +
                   mimir_pool_bytes); /* Z-048 */

    if (mapped > mimir_stats_peak_mapped) {
        mimir_stats_peak_mapped = mapped;
    }
#endif
}

/*
 * Acquire and publish one fresh span for class_index, with
 * the lock held. The mapping is exactly one system page,
 * discovered at runtime through L6 on this call (never a
 * constant, G-002 C-2) and acquired through L6's checked
 * all-or-nothing acquire. Publication mirrors
 * alloc_locked: counter checks before publication, header
 * written in full, then linked and counted in this same
 * locked section. On any failure nothing is published and
 * the registry and counters are unchanged (G-002 C-7).
 */
#ifdef MIMIR_ARENA_CCD
/* Z-037 forward declaration: the arena section proper
 * sits below, beside span_release_locked; span creation
 * registers ownership from here. */
static int arena_register(struct mimir_span_header *span,
                          size_t arena);
#endif
static struct mimir_span_header *span_acquire_locked(size_t class_index)
{
#ifdef MIMIR_LOCK_SHARD
    mimir_shard_hint = shard_of_class(class_index);
#endif
    size_t page_size;
    void *base = NULL;
    size_t mapping_length = 0;
    struct mimir_span_header *span;
    int status;

    status = mimir_platform_page_size(&page_size);
    if (status != MIMIR_PLATFORM_OK || page_size == 0) {
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_PAGESIZE);
    }
    /* A span must hold its header plus at least one object
     * of its class; on every admitted host (page >= 4096,
     * class <= 1024) this holds by the G-002 S6 arithmetic,
     * and the check keeps the invariant explicit rather
     * than assumed. */
    if (page_size <= MIMIR_SPAN_HEADER_SIZE ||
        page_size - MIMIR_SPAN_HEADER_SIZE <
            mimir_class_sizes[class_index]) {
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_INTERNAL);
    }
    status = mimir_platform_map_acquire(page_size, &base,
                                        &mapping_length);
    if (status != MIMIR_PLATFORM_OK) {
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_BACKING);
    }
    if (mapping_length != page_size) {
        (void)mimir_platform_map_release(base, mapping_length);
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_INTERNAL);
    }
    if (mapping_length > SIZE_MAX - mimir_live_span_bytes ||
        mimir_live_spans == SIZE_MAX) {
        (void)mimir_platform_map_release(base, mapping_length);
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_INTERNAL);
    }

    span = (struct mimir_span_header *)base;
    span->magic = MIMIR_SPAN_MAGIC;
    span->mapping_base = base;
    span->mapping_length = mapping_length;
    /* Z-066: record the one true span length (see
     * span_length_is_true). mapping_length == page_size
     * was proven immediately above. */
    __atomic_store_n(&mimir_span_page_size, page_size,
                     __ATOMIC_RELAXED);
    span->usable_offset = MIMIR_SPAN_HEADER_SIZE;
    span->class_size = mimir_class_sizes[class_index];
    span->usable_size = mapping_length - MIMIR_SPAN_HEADER_SIZE;
    span->state = MIMIR_STATE_LIVE;
    span->class_index = (uint32_t)class_index;
    span->free_head = NULL;
    span->live_count = 0;
    span->carve_count = 0;
#ifdef MIMIR_OOL_META
    /* Z-034: trial classes acquire their out-of-line
     * metadata page beside the span. Failure publishes
     * nothing, exactly like a backing failure above. */
    if (ool_class_is(class_index)) {
        void *side = NULL;
        size_t side_len = 0;

        status = mimir_platform_map_acquire(page_size, &side,
                                            &side_len);
        if (status != MIMIR_PLATFORM_OK ||
            !ool_register(span, side, side_len)) {
            if (side != NULL) {
                (void)mimir_platform_map_release(side, side_len);
            }
            (void)mimir_platform_map_release(base,
                                             mapping_length);
            return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_BACKING);
        }
    }
#endif

#ifdef MIMIR_ARENA_CCD
    /* Z-037: the new span is owned by the acquiring
     * thread's arena (the hint the serving entry point
     * set). Registration failure publishes nothing,
     * exactly like a backing failure above. */
    if (!arena_register(span, mimir_arena_hint)) {
        (void)mimir_platform_map_release(base, mapping_length);
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_BACKING);
    }
#endif
    span_registry_insert(span);
    mimir_live_spans += 1;
    mimir_live_span_bytes += mapping_length;
    __atomic_fetch_add(&mimir_audit_span_published, 1UL,
                       __ATOMIC_RELAXED);
    mimir_stats_peak_update_locked(); /* J-002 */
    mimir_class_active[class_index] = span;
    return span;
}

/*
 * Z-037 per-CCD arena prototype (MIMIRALLOC_Z13_ROADMAP.md
 * Wave 5, slice Z-037 "Arena prototype behind a flag").
 * Everything in this section exists ONLY under
 * MIMIR_ARENA_CCD. A production build never defines it: no
 * code, no data, no symbol, no cost (proof in
 * tests/perf/recorded_run_z13_z037.txt, the Z-019/Z-034
 * zero-cost pattern). MIMIR_DEBUG and MIMIR_OOL_META builds
 * are refused: the debug layout and the OOL trial own their
 * own serving paths and the prototype does not re-derive
 * them (combinations are future work, recorded in the
 * slice's CAMPAIGN_STATE entry).
 *
 * Design: TWO arenas, one per CCD, per the Z-012 constants
 * of record (tests/perf/recorded_run_z012.txt;
 * docs/Z13_DESIGN_CONSTANTS.md S4): CCD0 = logical CPUs
 * 0-7 and their SMT siblings 16-23; CCD1 = logical 8-15
 * and siblings 24-31; cross-CCD shared-line transfer
 * 103 ns vs 32.5 ns intra-CCD (penalty 3.2x), so a span is
 * served ONLY to threads running on its owner CCD. A
 * span's owner arena is fixed at span creation (the
 * acquiring thread's arena) and recorded in the side table
 * below -- the span header stays exactly 96 bytes, object
 * offsets and capacities are unchanged. Serving (the
 * small_alloc_locked road and the Z-029 TLS refill road
 * through tls_take_one_locked, i.e. the Wave-4 cache
 * machinery shares this partition unchanged) selects the
 * caller's arena from its CURRENT cpu at each central
 * transaction; blocks already in a thread's TLS bins stay
 * valid across a migration, and every free routes by the
 * block's span (never by the freeing thread's arena), so
 * cross-CCD and cross-thread frees are exactly as correct
 * as on the banked core. The one global lock is unchanged:
 * this prototype partitions span OWNERSHIP and SELECTION,
 * not locking -- the lock question is Z-039's road, and
 * Z-038 owns the ladder that judges whether the partition
 * pays. Retention is per arena: an arena's empty spans of
 * a class are bounded by the G-007 bound SPLIT across
 * the arenas (per-arena bound = bound / 2, Dossier S4.2
 * C5: split, not duplicated), so the retained-bytes
 * total never exceeds the banked global bound.
 */
#ifdef MIMIR_ARENA_CCD
#ifdef MIMIR_DEBUG
#error "MIMIR_ARENA_CCD prototype is not defined for MIMIR_DEBUG builds (Z-037)"
#endif
#ifdef MIMIR_OOL_META
#error "MIMIR_ARENA_CCD prototype is not defined with MIMIR_OOL_META (Z-037)"
#endif

/* Owner-table slots: 65536 x 16 B = 1 MiB of BSS in
 * flag-on builds only. Live spans are demand-unbounded
 * in principle; at table-full arena_register fails and
 * span_acquire fails the allocation cleanly (publishes
 * nothing, the OOL precedent) - a defined ceiling at
 * 65536 live spans (which needs at least ~196k live
 * objects of the largest class), never a silent
 * misroute. */
#define MIMIR_ARENA_SLOTS ((size_t)65536)

struct mimir_arena_slot {
    struct mimir_span_header *span; /* NULL empty, (void*)-1 tombstone */
    uint32_t arena;
    uint32_t reserved;
};

static struct mimir_arena_slot mimir_arena_table[MIMIR_ARENA_SLOTS];
static uint64_t mimir_arena_selections[MIMIR_ARENA_COUNT];

static size_t arena_hash(const struct mimir_span_header *span)
{
    return ((uintptr_t)span >> 12) & (MIMIR_ARENA_SLOTS - 1);
}

static int arena_register(struct mimir_span_header *span,
                          size_t arena)
{
    size_t i;
    size_t h = arena_hash(span);

    for (i = 0; i < MIMIR_ARENA_SLOTS; i++) {
        struct mimir_arena_slot *s =
            &mimir_arena_table[(h + i) & (MIMIR_ARENA_SLOTS - 1)];
        if (s->span == NULL || s->span == (void *)(uintptr_t)-1) {
            s->span = span;
            s->arena = (uint32_t)arena;
            s->reserved = 0;
            return 1;
        }
    }
    return 0;
}

/* Owner arena of a registered span; arena 0 when
 * unregistered (a span that predates registration cannot
 * exist in this build -- acquire registers before
 * publishing -- so the default is a defensive posture). */
static size_t arena_of(const struct mimir_span_header *span)
{
    size_t i;
    size_t h = arena_hash(span);

    if (span == NULL) {
        return 0;
    }
    for (i = 0; i < MIMIR_ARENA_SLOTS; i++) {
        struct mimir_arena_slot *s =
            &mimir_arena_table[(h + i) & (MIMIR_ARENA_SLOTS - 1)];
        if (s->span == span) {
            return (size_t)s->arena;
        }
        if (s->span == NULL) {
            return 0;
        }
    }
    return 0;
}

/* 1 when span has a live owner entry (J-006 support). */
static int arena_registered(const struct mimir_span_header *span)
{
    size_t i;
    size_t h = arena_hash(span);

    for (i = 0; i < MIMIR_ARENA_SLOTS; i++) {
        struct mimir_arena_slot *s =
            &mimir_arena_table[(h + i) & (MIMIR_ARENA_SLOTS - 1)];
        if (s->span == span) {
            return 1;
        }
        if (s->span == NULL) {
            return 0;
        }
    }
    return 0;
}

/* The G-007 retention bound, split across the arenas
 * (Dossier S4.2 C5): per-arena bound = bound / 2 (floor),
 * so at the default bound 2 each arena retains at most
 * 1 empty span per class (2 spans = 8192 B per class in
 * total, exactly the banked global figure); at bound 16,
 * 8 per arena (16 spans = 64 KiB per class in total).
 * An odd bound rounds DOWN (bound 1 -> 0 per arena):
 * the split never lets the total exceed the global
 * bound the operator set. */
static size_t arena_retention_bound(void)
{
    return mimir_retention_bound / MIMIR_ARENA_COUNT;
}

static void arena_unregister(struct mimir_span_header *span)
{
    size_t i;
    size_t h = arena_hash(span);

    for (i = 0; i < MIMIR_ARENA_SLOTS; i++) {
        struct mimir_arena_slot *s =
            &mimir_arena_table[(h + i) & (MIMIR_ARENA_SLOTS - 1)];
        if (s->span == span) {
            s->span = (void *)(uintptr_t)-1;
            s->arena = 0;
            return;
        }
        if (s->span == NULL) {
            return;
        }
    }
}
/* The calling thread's arena under the Z-012 CCD map:
 * physical core = logical % 16 (SMT sibling of n is n+16);
 * cores 0-7 are CCD0, 8-15 are CCD1. A failed CPU query
 * falls back to arena 0. Lock-free: one L6 query, no
 * allocator state touched. */
static size_t arena_current_raw(void)
{
    size_t cpu;

    if (mimir_platform_current_cpu(&cpu) != MIMIR_PLATFORM_OK) {
        return 0;
    }
    return (size_t)(((cpu % 16u) >= 8u) ? 1u : 0u);
}

/* Set the lock-held serving hint to the caller's arena
 * and count the selection. Serving entry points only. */
static size_t arena_select_locked(void)
{
    size_t arena = arena_current_raw();

    mimir_arena_hint = arena;
    mimir_arena_selections[arena] += 1;
    return arena;
}

size_t mimir_arena_current(void)
{
    return arena_current_raw();
}

int mimir_arena_snapshot(struct mimir_arena_snapshot *out)
{
    struct mimir_span_header *span;
    size_t steps = 0;
    size_t a;

    if (out == NULL) {
        return -1;
    }
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return -1;
    }
    for (a = 0; a < MIMIR_ARENA_COUNT; a++) {
        out->spans[a] = 0;
        out->live_objects[a] = 0;
        out->selections[a] = mimir_arena_selections[a];
    }
    span = mimir_span_head;
    while (span != NULL) {
        if (++steps > mimir_live_spans) {
            break;
        }
        if (span_header_is_live(span)) {
            size_t owner = arena_of(span);

            out->spans[owner] += 1;
            out->live_objects[owner] +=
                (uint64_t)span->live_count;
        }
        span = span->list_next;
    }
    (void)core_global_unlock();
    return 0;
}
#endif /* MIMIR_ARENA_CCD */

/*
 * Release one LIVE span whole, with the lock held: the
 * exact acquired pair is captured before release and the
 * header is never read after a successful release
 * (Invariant 2). On release failure the retirement is
 * rolled back in full (header restored LIVE and
 * re-linked, counters restored), the F-06 discipline of
 * retire_locked carried to spans. Returns 1 on success,
 * 0 on rolled-back failure.
 */
static int span_release_locked(struct mimir_span_header *span)
{
    void *mapping_base = span->mapping_base;
    size_t mapping_length = span->mapping_length;
    size_t class_index = span->class_index;
    int status;
#ifdef MIMIR_LOCK_SHARD
    /* Z-039: the registry/active writes below name this
     * span's shard list; the hint is set, not saved -- every
     * caller either holds every shard or re-sets the hint
     * before its next registry touch. */
    mimir_shard_hint = shard_of_class(class_index);
#endif
#ifdef MIMIR_ARENA_CCD
    /* Z-037: the active-slot writes in this function name
     * the span's OWNER arena, never the serving hint: a
     * release is routinely triggered by a thread of the
     * other CCD (cross-CCD free, TLS drain). Save and
     * override the hint for the duration; the caller's
     * hint is restored on every exit path below. */
    size_t arena_saved_hint = mimir_arena_hint;
    size_t arena_owner = arena_of(span);

    mimir_arena_hint = arena_owner;
#endif
#ifdef MIMIR_OOL_META
    /* Z-034: the trial side page is captured now and
     * released only after the span release succeeds, so a
     * rolled-back retirement keeps its metadata. */
    void *ool_base = NULL;
    size_t ool_len = 0;
    int ool_had = 0;

    if (ool_span_is(span)) {
        ool_unregister(span, &ool_base, &ool_len);
        ool_had = ool_base != NULL;
        if (ool_had) {
            /* Registration returns at rollback below. */
            (void)0;
        }
    }
#endif

    span->state = MIMIR_STATE_RETIRED;
    span_registry_remove(span);
    if (mimir_class_active[class_index] == span) {
        mimir_class_active[class_index] = NULL;
    }
    mimir_live_spans -= 1;
    mimir_live_span_bytes -= mapping_length;

    status = mimir_platform_map_release(mapping_base,
                                        mapping_length);
    if (status != MIMIR_PLATFORM_OK) {
        span->state = MIMIR_STATE_LIVE;
        span_registry_insert(span);
        mimir_class_active[class_index] = span;
        mimir_live_spans += 1;
        mimir_live_span_bytes += mapping_length;
#ifdef MIMIR_ARENA_CCD
        mimir_arena_hint = arena_saved_hint;
#endif
#ifdef MIMIR_OOL_META
        if (ool_had) {
            (void)ool_register(span, ool_base, ool_len);
        }
#endif
        return 0;
    }
#ifdef MIMIR_ARENA_CCD
    arena_unregister(span);
    mimir_arena_hint = arena_saved_hint;
#endif
    __atomic_fetch_add(&mimir_audit_span_retired, 1UL,
                       __ATOMIC_RELAXED);
    mimir_stats_bytes_released += mapping_length; /* J-002 Q-RELEASED */
#ifdef MIMIR_OOL_META
    if (ool_had) {
        (void)mimir_platform_map_release(ool_base, ool_len);
        mimir_stats_bytes_released += ool_len; /* Z-034 side page */
    }
#endif
    return 1;
}

#ifdef MIMIR_DEBUG
/* J-005 forward declaration: the quarantine block below
 * defines this after this point in the file; the EMPTY
 * tests here and in the trim/retained/stats walks all
 * consult it (S5.3 guards). */
static size_t debug_span_quarantine_count_locked(
    const struct mimir_span_header *span);
#endif

/* Z-029: the TLS cache block further down owns this; the
 * span release decisions here consult the binned tally so
 * a span holding thread-cached blocks is never counted
 * EMPTY for release. In a MIMIR_DEBUG build the cache is
 * bypassed and the tally is always 0 (Z-029 slice doc). */
static size_t tls_binned_count_locked(
    const struct mimir_span_header *span);

/* Count retained EMPTY spans of one class (lock held).
 * Z-037: in an arena build the production callers use the
 * per-arena variant below and the only remaining caller is
 * the MIMIR_DEBUG free path, a combination the prototype
 * refuses; the definition is compiled out there rather
 * than left unused. */
#if !defined(MIMIR_ARENA_CCD) || defined(MIMIR_DEBUG)
static size_t empty_spans_of_class_locked(size_t class_index)
{
#ifdef MIMIR_LOCK_SHARD
    mimir_shard_hint = shard_of_class(class_index);
#endif
    struct mimir_span_header *span = mimir_span_head;
    size_t count = 0;

    while (span != NULL) {
        if (span->class_index == class_index &&
            span->live_count == 0
#ifdef MIMIR_DEBUG
            /* J-005: not EMPTY while quarantine holds
             * blocks in it (S5.3). */
            && debug_span_quarantine_count_locked(span) == 0
#endif
            /* Z-029: not EMPTY for release while a bin
             * holds blocks of this span. */
            && tls_binned_count_locked(span) == 0) {
            count++;
        }
        span = span->list_next;
    }
    return count;
}
#endif /* !MIMIR_ARENA_CCD || MIMIR_DEBUG */

#ifdef MIMIR_ARENA_CCD
/* Z-037: retained EMPTY spans of one class in ONE arena
 * (lock held). The G-007 retention bound applies per
 * arena, so one CCD's churn cannot evict the other CCD's
 * retained spans. */
static size_t empty_spans_of_class_arena_locked(
    size_t class_index, size_t arena)
{
    struct mimir_span_header *span = mimir_span_head;
    size_t count = 0;

    while (span != NULL) {
        if (span->class_index == class_index &&
            span->live_count == 0 &&
            tls_binned_count_locked(span) == 0 &&
            arena_of(span) == arena) {
            count++;
        }
        span = span->list_next;
    }
    return count;
}
#endif /* MIMIR_ARENA_CCD */

static int span_can_serve(const struct mimir_span_header *span)
{
    /* J-006: a corrupt free_head (wild, misaligned, or
     * pointing outside the carved area) does not make the
     * span servable and is never followed; the span may
     * still serve by carving, which hands out only memory
     * the header's own validated fields describe. */
#ifdef MIMIR_OOL_META
    /* Z-034: a trial span serves iff some slot is not live
     * (a cleared bit below carve_count, or carve room). */
    if (ool_span_is(span)) {
        return span->live_count < span_capacity(span);
    }
#endif
    return (span->free_head != NULL &&
            span_free_node_valid(span, span->free_head)) ||
           span->carve_count < span_capacity(span);
}

/* ---- Z-067 (D-12): INC-J006-06 -- live_count trusted by
 * release paths ------------------------------------------
 * J-006 proved the integrity walk's span relation
 * (free-list nodes + live_count + TLS tally == carve_count,
 * the walked live sum equalling mimir_live_small_objects)
 * flags a live_count corrupted to 0 under live objects,
 * while every operational release path trusted the field:
 * the drain unmapped the span under its live blocks
 * (docs/PHASE_10_METADATA_INTEGRITY.md, INC-J006-06; the
 * doc's smallest remedy is this verification on the cold
 * release paths). The audit below is the per-span half of
 * that relation, consulted by exactly the paths that
 * RELEASE a span whole on the strength of live_count == 0:
 * the shared trim walk (drain and trim enter there), the
 * retention-overflow release in small_free_locked (both
 * flavours), the TLS retention sweep, and the tally free
 * course -- the sweep and the tally course wired after a
 * reviewer pass caught the thread-exit drain releasing a
 * live_count-corrupted span whole through the sweep (the
 * experiment is recorded in the slice's recorded run). On
 * a proven mismatch the release is REFUSED -- the span
 * stays retained and mapped, its objects stay live, and
 * nothing is unmapped on a count nobody can trust -- the
 * refusal is tallied here and reported on the J-007 diag
 * channel by the entry point (op free / drain / trim,
 * cause metadata). The audit runs only on spans already
 * selected as release candidates (live_count == 0, no TLS
 * tally, retention exceeded or a trim/drain walk), never
 * on the alloc/free hot path.
 *
 * Episode semantics (the Z-066 pattern): the first
 * refusal for a distinct (span, walked-nodes / live_count
 * / carve_count) signature tallies one detection; repeats
 * of the same signature tally as collateral; a later
 * audit of the same span that PASSES closes the episode.
 * The tallies are atomic and ungated; the diag emission
 * is env-gated exactly as the channel is. Scope, stated
 * plainly: the audit proves the count relation of the
 * candidate span only, so a corruption that preserves the
 * relation (live_count lowered while the free list grows
 * by the same amount -- which needs a second, list write)
 * is outside it, and the registry-wide live sum stays the
 * integrity walk's job. INC-J006-04 (free_head naming a
 * live object) is NOT detected here or anywhere in this
 * slice: it preserves this relation exactly; its
 * undetectable-by-design record is in
 * tests/perf/recorded_run_z13_z067.txt. Caller holds the
 * lock. */
static _Atomic uint64_t mimir_live_release_detections;
static _Atomic uint64_t mimir_live_release_collateral_refusals;
static const struct mimir_span_header *mimir_z067_bad_span;
static size_t mimir_z067_bad_sig;
static _Thread_local int mimir_z067_detected;

static void z067_note_refusal_locked(
    const struct mimir_span_header *span, size_t nodes)
{
    size_t sig = nodes ^ span->live_count ^
                 (span->carve_count << 1);

    if (mimir_z067_bad_span == span &&
        mimir_z067_bad_sig == sig) {
        __atomic_fetch_add(
            &mimir_live_release_collateral_refusals, 1ULL,
            __ATOMIC_RELAXED);
    } else {
        __atomic_fetch_add(&mimir_live_release_detections,
                           1ULL, __ATOMIC_RELAXED);
        mimir_z067_bad_span = span;
        mimir_z067_bad_sig = sig;
    }
    mimir_z067_detected = 1;
}

/* The release audit: 1 = the span's count relation is
 * proven, so a release may trust live_count == 0; 0 =
 * proven mismatch (or a free list whose shape cannot be
 * walked to a count), tallied at z067_note_refusal_locked.
 * An OOL trial span is judged by its own bitmap recount,
 * exactly as the J-006 walk judges it. */
static int span_live_audit_locked(
    const struct mimir_span_header *span)
{
    const void *node;
    size_t nodes = 0;

#ifdef MIMIR_OOL_META
    if (ool_span_is(span)) {
        size_t live = ool_popcount_locked(span);

        if (live == span->live_count) {
            if (mimir_z067_bad_span == span) {
                mimir_z067_bad_span = NULL;
                mimir_z067_bad_sig = 0;
            }
            return 1;
        }
        z067_note_refusal_locked(span, live);
        return 0;
    }
#endif
    node = span->free_head;
    while (node != NULL) {
        if (!span_free_node_valid(span, node)) {
            z067_note_refusal_locked(span,
                                     span->carve_count + 1);
            return 0;
        }
        nodes += 1;
        if (nodes > span->carve_count) {
            z067_note_refusal_locked(span,
                                     span->carve_count + 1);
            return 0;
        }
        node = *(void *const *)node;
    }
    if (nodes + span->live_count +
            tls_binned_count_locked(span) ==
        span->carve_count) {
        if (mimir_z067_bad_span == span) {
            mimir_z067_bad_span = NULL;
            mimir_z067_bad_sig = 0;
        }
        return 1;
    }
    z067_note_refusal_locked(span, nodes);
    return 0;
}

/* Consume the detection label on an entry point that ran
 * a release path, after the caller has unlocked (the
 * Z-066 pattern): the diag report is the channel's own --
 * env-gated, allocation-free, a plain no-op when the gate
 * is closed. The tallies moved at the audit site, under
 * the lock; only the emission is deferred to here. */
static void z067_release_report(int op)
{
    if (!mimir_z067_detected) {
        return;
    }
    mimir_z067_detected = 0;
    mimir_core_diag_report(op, MIMIR_DIAG_CAUSE_METADATA, 0);
}

uint64_t mimir_core_live_release_detections(void)
{
    return __atomic_load_n(&mimir_live_release_detections,
                           __ATOMIC_RELAXED);
}

uint64_t mimir_core_live_release_collateral_refusals(void)
{
    return __atomic_load_n(
        &mimir_live_release_collateral_refusals,
        __ATOMIC_RELAXED);
}

/*
 * J-005 (Phase 10 debug-only poisoning + quarantine of
 * freed blocks; docs/PHASE_10_POISON_QUARANTINE.md; plan:
 * docs/PHASE_10_FOUNDATION_PLAN.md S5.2/S5.3). Compiled
 * ONLY under MIMIR_DEBUG (the one debug flag of S5.1);
 * production objects contain none of this code at all.
 *
 * On a small-path free the block's whole class extent is
 * filled with the poison pattern (byte 0xDD, stated here
 * and in the slice doc) and the block is held in the
 * bounded quarantine below instead of being threaded onto
 * its span's free list. A quarantined block is verified
 * (a) at eviction from the quarantine -- eviction is the
 * only road back to reuse, so every reused block has
 * passed verification first -- and (b) on demand through
 * mimir_debug_quarantine_verify. A byte that no longer
 * matches the pattern is a use-after-free write (or
 * external corruption of allocator-held memory): the
 * allocator-triggered path reports on fd 2 from a fixed
 * stack buffer with write(2) and calls abort(); it
 * allocates nothing and recurses into nothing (the
 * Phase 10 rule). The on-demand verify reports by return
 * value only and never aborts, so a test parent can prove
 * detection without dying; the abort shape is exercised
 * only in disposable child processes (slice doc S3).
 *
 * Capacity: exactly MIMIR_DEBUG_QUARANTINE_CAPACITY
 * blocks, one global FIFO across classes, fixed static
 * storage. No per-block metadata growth: the span header
 * stays 96 bytes and class extents are unchanged, so
 * debug layout overhead is 0 bytes per block; the table
 * itself is CAPACITY entries of static storage.
 *
 * S5.3 honoured by construction + guards: quarantine
 * lives INSIDE spans; a span holding quarantined blocks
 * is never selected for release (retention release in
 * small_free_locked, the shared trim/drain walk) and is
 * never counted EMPTY by the retained/stats walks, so
 * D-CACHED keeps its production definition (retention
 * cache only) and quarantined bytes are reported as
 * their own quantity, mimir_debug_quarantined_bytes
 * (bytes_quarantined), with their own name.
 */
#ifdef MIMIR_DEBUG

#define MIMIR_DEBUG_POISON_BYTE 0xDDu
#define MIMIR_DEBUG_QUARANTINE_CAPACITY ((size_t)32)

struct mimir_debug_quarantine_entry {
    void *ptr;
    struct mimir_span_header *span;
};

static struct mimir_debug_quarantine_entry
    mimir_debug_quarantine[MIMIR_DEBUG_QUARANTINE_CAPACITY];
static size_t mimir_debug_quarantine_len;

static void debug_poison_fill(void *ptr, size_t extent)
{
    memset(ptr, (int)MIMIR_DEBUG_POISON_BYTE, extent);
}

static int debug_block_is_poisoned(const void *ptr, size_t extent)
{
    const unsigned char *p = (const unsigned char *)ptr;
    size_t i;

    for (i = 0; i < extent; i++) {
        if (p[i] != (unsigned char)MIMIR_DEBUG_POISON_BYTE) {
            return 0;
        }
    }
    return 1;
}

static int debug_quarantine_contains_locked(const void *ptr)
{
    size_t i;

    for (i = 0; i < mimir_debug_quarantine_len; i++) {
        if (mimir_debug_quarantine[i].ptr == ptr) {
            return 1;
        }
    }
    return 0;
}

static size_t debug_span_quarantine_count_locked(
    const struct mimir_span_header *span)
{
    size_t i;
    size_t n = 0;

    for (i = 0; i < mimir_debug_quarantine_len; i++) {
        if (mimir_debug_quarantine[i].span == span) {
            n++;
        }
    }
    return n;
}

/* Count quarantined blocks whose bytes no longer match
 * the poison pattern (lock held). Reports by count only:
 * never aborts, allocates nothing. */
static size_t debug_quarantine_verify_locked(void)
{
    size_t i;
    size_t bad = 0;

    for (i = 0; i < mimir_debug_quarantine_len; i++) {
        struct mimir_debug_quarantine_entry *e =
            &mimir_debug_quarantine[i];

        if (!debug_block_is_poisoned(e->ptr,
                                     e->span->class_size)) {
            bad++;
        }
    }
    return bad;
}

/* Report + fail clearly (S5.2's stated exception; the
 * Phase 10 fail-clearly posture): fixed stack buffer,
 * write(2) to fd 2, abort(). No allocation, no stdio, no
 * lock operation, no recursion. INC-J005-05: this
 * debug-only code names write(2) directly because L6
 * publishes no diagnostic channel; J-007 owns the
 * production reporting channel and this call is to be
 * re-homed to it at reconciliation. Called with the
 * global lock held; abort() does not return. */
static void debug_report_and_abort_locked(const void *ptr)
{
    static const char head[] =
        "mimiralloc debug: poison check failed for quarantined block 0x";
    static const char tail[] =
        " (use-after-free write detected)\n";
    char buf[160];
    size_t n = 0;
    size_t i;
    uintptr_t v = (uintptr_t)ptr;
    int shift;
    ssize_t ignored;

    for (i = 0; head[i] != '\0'; i++) {
        buf[n++] = head[i];
    }
    for (shift = (int)(sizeof(uintptr_t) * 8) - 4;
         shift >= 0; shift -= 4) {
        unsigned digit = (unsigned)((v >> shift) & 0xFu);
        buf[n++] = (char)(digit < 10 ? '0' + (int)digit
                                     : 'a' + (int)(digit - 10));
    }
    for (i = 0; tail[i] != '\0'; i++) {
        buf[n++] = tail[i];
    }
    ignored = write(2, buf, n);
    (void)ignored;
    abort();
}

/* Evict the oldest quarantined block (lock held): verify
 * its poison first -- a corrupted block fails clearly
 * here and never returns to reuse -- then thread it onto
 * its span's free list, the production reuse home. Its
 * first 8 bytes become the free-list link, exactly as a
 * production free writes them (G-002 S7). */
static void debug_quarantine_evict_oldest_locked(void)
{
    struct mimir_debug_quarantine_entry e =
        mimir_debug_quarantine[0];

    if (!debug_block_is_poisoned(e.ptr, e.span->class_size)) {
        debug_report_and_abort_locked(e.ptr);
    }
    memmove(&mimir_debug_quarantine[0],
            &mimir_debug_quarantine[1],
            (mimir_debug_quarantine_len - 1) *
                sizeof mimir_debug_quarantine[0]);
    mimir_debug_quarantine_len -= 1;
    *(void **)e.ptr = e.span->free_head;
    e.span->free_head = e.ptr;
}

static void debug_quarantine_insert_locked(
    void *ptr, struct mimir_span_header *span)
{
    if (mimir_debug_quarantine_len ==
        MIMIR_DEBUG_QUARANTINE_CAPACITY) {
        debug_quarantine_evict_oldest_locked();
    }
    mimir_debug_quarantine[mimir_debug_quarantine_len].ptr = ptr;
    mimir_debug_quarantine[mimir_debug_quarantine_len].span = span;
    mimir_debug_quarantine_len += 1;
}

/* Test-only surface (INC-E001-07 shape (b), exactly like
 * the G-007 tuning pair): declared in src/core/
 * mimir_core.h under MIMIR_DEBUG and re-declared for
 * tests only in tests/core/mimir_quarantine_j005.h;
 * bound local in the shared library (nm -D stays
 * exactly 4 in both flavours). Getter posture is the
 * G-007 one: (size_t)-1 / -1 when the lock cannot be
 * taken, with nothing changed. */
size_t mimir_debug_quarantine_count(void)
{
    size_t n;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return (size_t)-1;
    }
    n = mimir_debug_quarantine_len;
    (void)core_global_unlock();
    return n;
}

size_t mimir_debug_quarantined_bytes(void)
{
    size_t i;
    size_t bytes = 0;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return (size_t)-1;
    }
    for (i = 0; i < mimir_debug_quarantine_len; i++) {
        bytes += mimir_debug_quarantine[i].span->class_size;
    }
    (void)core_global_unlock();
    return bytes;
}

int mimir_debug_quarantine_verify(void)
{
    size_t bad;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return -1;
    }
    bad = debug_quarantine_verify_locked();
    (void)core_global_unlock();
    return (int)bad;
}

#endif /* MIMIR_DEBUG */

/*
 * Ownership determination for the span registry (G-002
 * R-4), with the lock held: find the LIVE span whose
 * object area contains ptr at its class stride, computed
 * only from the trusted span header's own fields -- bytes
 * at the candidate pointer are never read to decide
 * ownership. A carved object is LIVE for this path if and
 * only if it is not on its span's free list (the free-list
 * walk reads allocator-written links inside allocator-
 * owned free objects, never user bytes of a live object).
 * Returns the span, or NULL when ptr is not a live span
 * object (foreign, interior, uncarved, already-freed, or
 * a pointer at the span header itself -- INC-G002-05's
 * sharpest case, which lands outside the object area by
 * construction and is refused here).
 */
static struct mimir_span_header *find_span_mode_locked(
    void *ptr, int for_free)
{
    struct mimir_span_header *span = mimir_span_head;
    uintptr_t target = (uintptr_t)ptr;
    size_t j006_steps = 0;

#ifdef MIMIR_DEBUG
    /* J-005: a quarantined block is already freed. It is
     * not on its span's free list (the quarantine table is
     * its only home), so without this guard it would pass
     * the free-list walk below as if live and a double
     * free would retire it twice. */
    if (debug_quarantine_contains_locked(ptr)) {
        return NULL;
    }
#endif

    while (span != NULL) {
        /* J-006: registry-walk bound, as in
         * find_candidate_locked: a valid registry holds
         * exactly mimir_live_spans nodes; a cycling
         * list_next ends in refusal, not a loop. A wild
         * list_next still faults (INC-J006-01). */
        if (++j006_steps > mimir_live_spans) {
            return NULL;
        }
        uintptr_t base = (uintptr_t)span->mapping_base;
        uintptr_t first = base + MIMIR_SPAN_HEADER_SIZE;

        /* Z-066: on the free path, a span that is ours
         * (magic / state / self-base all valid) whose
         * mapping_length disagrees with the one true span
         * length, and whose REAL page holds the target, is
         * a detection: label it for the entry point's diag
         * report. The validator below refuses the span
         * regardless; this only names the cause. */
        if (for_free && !span_length_is_true(span) &&
            span->magic == MIMIR_SPAN_MAGIC &&
            span->state == MIMIR_STATE_LIVE &&
            span->mapping_base == (const void *)span &&
            target >= first &&
            target < base + __atomic_load_n(
                                &mimir_span_page_size,
                                __ATOMIC_RELAXED)) {
            mimir_z066_detected = 1;
        }

        if ((for_free ? span_header_is_live_for_free(span)
                      : span_header_is_live(span)) &&
            target >= first &&
            target < base + span->mapping_length) {
            size_t offset = (size_t)(target - first);
            size_t index;
            void *node;

#ifdef MIMIR_DEBUG
            /* J-004: user extents start one guard in, at
             * the debug stride. */
            if (offset < MIMIR_DBG_GUARD_SIZE) {
                return NULL; /* in slot 0 pre-zone */
            }
            offset -= MIMIR_DBG_GUARD_SIZE;
            if (offset % mimir_dbg_stride(span->class_size) != 0) {
                return NULL; /* interior: not an object start */
            }
            index = offset / mimir_dbg_stride(span->class_size);
#else
            if (offset % span->class_size != 0) {
                return NULL; /* interior: not an object start */
            }
            index = offset / span->class_size;
#endif
            if (index >= span->carve_count) {
                return NULL; /* uncarved tail area */
            }
#ifdef MIMIR_OOL_META
            /* Z-034: trial-span liveness IS the bitmap bit;
             * no free-list walk exists for these spans. */
            if (ool_span_is(span)) {
                return ool_bit_test(span, index) ? span : NULL;
            }
#endif
            node = span->free_head;
            {
                /* J-006: the free-list walk range-checks
                 * each node against the span's own mapped
                 * object area BEFORE dereferencing it (the
                 * bounds are hoisted into locals: full
                 * shape validation per node measurably
                 * taxed this hot walk, bench in the slice
                 * doc S6), and the walk is bounded by
                 * carve_count (a valid list holds
                 * carve - live nodes). A corrupt link that
                 * leaves the span, or cycles, now ends in
                 * refusal of this free instead of a fault
                 * or an unbounded loop; an in-area corrupt
                 * link is followed at most carve_count
                 * steps within mapped memory and then
                 * refused or exhausted. */
                size_t j006_nodes = 0;
                const uintptr_t j006_lo =
                    (uintptr_t)span->mapping_base +
                    MIMIR_SPAN_HEADER_SIZE;
                const uintptr_t j006_hi =
                    (uintptr_t)span->mapping_base +
                    span->mapping_length;

                while (node != NULL) {
                    if (node == ptr) {
#if MIMIR_DFREE_MEMBER_ON
                        /* Z-070: a for-free walk meeting
                         * the pointer on its own span's
                         * free list has detected a double
                         * free; label it for the entry
                         * point's report (counted once
                         * there, never here). */
                        if (for_free) {
                            mimir_z070_df_detected = 1;
                        }
#endif
                        return NULL; /* already freed */
                    }
                    /* The bound is j006_hi - 8: the whole
                     * 8-byte link read must lie inside the
                     * span page, never straddle its end. */
                    if ((uintptr_t)node < j006_lo ||
                        (uintptr_t)node >
                            j006_hi - sizeof(void *) ||
                        ++j006_nodes > span->carve_count) {
                        return NULL; /* corrupt free list */
                    }
                    node = *(void **)node;
                }
            }
            return span;
        }
        span = span->list_next;
    }
    return NULL;
}

static struct mimir_span_header *find_span_live_locked(void *ptr)
{
    return find_span_mode_locked(ptr, 0);
}

/* Z-043: free-path finder; see span_header_is_live_for_free. */
static struct mimir_span_header *find_span_for_free_locked(void *ptr)
{
    return find_span_mode_locked(ptr, 1);
}


#ifdef MIMIR_LOCK_SHARD
/* Z-039: find_span_live_locked walks ONE shard list (the
 * hint's). This variant finds a live span across all lists:
 * the hint's list first (the single-shard callers' span is
 * always there), then the others ascending; on success the
 * hint names the found span's shard. List stability is the
 * CALLER's lock set, never this function's. */
static struct mimir_span_header *find_span_live_any_locked(void *ptr)
{
    size_t first = mimir_shard_hint;
    size_t s;
    struct mimir_span_header *span;

    span = find_span_live_locked(ptr);
    if (span != NULL) {
        return span;
    }
    for (s = 0; s < MIMIR_SHARD_COUNT; s++) {
        if (s == first) {
            continue;
        }
        mimir_shard_hint = s;
        span = find_span_live_locked(ptr);
        if (span != NULL) {
            return span;
        }
    }
    mimir_shard_hint = first;
    return NULL;
}
/* Keeper banking composition (Z-043 banked onto the Z-039
 * tree): the free-path twin of find_span_live_any_locked
 * above -- same walk, strict free-path predicate. */
static struct mimir_span_header *find_span_for_free_any_locked(void *ptr)
{
    size_t first = mimir_shard_hint;
    size_t s;
    struct mimir_span_header *span;

    span = find_span_for_free_locked(ptr);
    if (span != NULL) {
        return span;
    }
    for (s = 0; s < MIMIR_SHARD_COUNT; s++) {
        if (s == first) {
            continue;
        }
        mimir_shard_hint = s;
        span = find_span_for_free_locked(ptr);
        if (span != NULL) {
            return span;
        }
    }
    mimir_shard_hint = first;
    return NULL;
}
#endif /* MIMIR_LOCK_SHARD */
/* Z-059 placement policy trial (docs/MIMIRALLOC_Z13_ROADMAP.md
 * slice Z-059; decision D-009 in docs/DECISION_LEDGER.md).
 * Two trial span-selection policies behind build flags,
 * measured against the banked Z-055 fragmentation oracle
 * by tests/perf/probe_z059.c:
 *   MIMIR_PLACEMENT_FIRSTFIT - ignore the class's active
 *     span; serve from the first span in list order that
 *     can serve (pure first-fit).
 *   MIMIR_PLACEMENT_BESTFIT - serve from the can-serve
 *     span with the fewest servable slots (capacity minus
 *     live minus TLS-binned: the tightest fit), ties
 *     broken by list order.
 * With neither flag defined this helper is not compiled
 * at all and both selection sites run the banked code
 * verbatim (active span first, then first-fit scan, then
 * a fresh span). The production default is unchanged by
 * this slice; see D-009. The two flags are mutually
 * exclusive. */
#if defined(MIMIR_PLACEMENT_FIRSTFIT) && defined(MIMIR_PLACEMENT_BESTFIT)
#error "Z-059: placement trial flags are mutually exclusive"
#endif

#if defined(MIMIR_PLACEMENT_FIRSTFIT) || defined(MIMIR_PLACEMENT_BESTFIT)
static struct mimir_span_header *placement_select_locked(
    size_t class_index
#ifdef MIMIR_ARENA_CCD
    , size_t arena
#endif
    )
{
    struct mimir_span_header *span = mimir_span_head;
#ifdef MIMIR_PLACEMENT_BESTFIT
    struct mimir_span_header *best = NULL;
    size_t best_free = (size_t)-1;

    while (span != NULL) {
        if (span->class_index == class_index &&
            span_header_is_live(span) && span_can_serve(span)
#ifdef MIMIR_ARENA_CCD
            && arena_of(span) == arena
#endif
            ) {
            /* Servable slots, from the TLS invariant
             * (free-list nodes + live_count + tally ==
             * carve_count): servable = free nodes + carve
             * room = capacity - live - tally. Saturated
             * at 0: a transient tally skew must never
             * wrap the size_t. */
            size_t used = span->live_count +
                tls_binned_count_locked(span);
            size_t servable = used < span_capacity(span)
                ? span_capacity(span) - used : 0;
            if (servable < best_free) {
                best_free = servable;
                best = span;
            }
        }
        span = span->list_next;
    }
    span = best;
#else /* MIMIR_PLACEMENT_FIRSTFIT */
    while (span != NULL) {
        if (span->class_index == class_index &&
            span_header_is_live(span) && span_can_serve(span)
#ifdef MIMIR_ARENA_CCD
            && arena_of(span) == arena
#endif
            ) {
            break;
        }
        span = span->list_next;
    }
#endif
    if (span == NULL) {
        span = span_acquire_locked(class_index);
        if (span == NULL) {
            return NULL;
        }
    }
    mimir_class_active[class_index] = span;
    return span;
}
#endif /* Z-059 placement trial */

/*
 * Serve one small object from the class path, with the
 * lock held. size is admitted by the caller (1..1024).
 * Reuse order (bounded local reuse): the class's active
 * span, then any registered span of the class that can
 * serve, then one fresh span. A backing failure is the
 * defined NULL failure -- a small request is NEVER
 * silently re-routed to a private mapping (G-002 R-1).
 * If zero_user is non-zero the whole CLASS extent is
 * zeroed before publication (G-002 R-2: the slack bytes
 * are the free-list link's home and must never carry a
 * previous owner's data forward). On success *out_span
 * (when non-NULL) receives the serving span's header.
 */
static void *small_alloc_locked(size_t size, int zero_user,
                                struct mimir_span_header **out_span)
{
    size_t class_index = class_index_for(size);
#ifdef MIMIR_LOCK_SHARD
    mimir_shard_hint = shard_of_class(class_index);
#endif
    size_t class_size = mimir_class_sizes[class_index];
#ifdef MIMIR_ARENA_CCD
    /* Z-037: serve from the calling thread's arena only. */
    size_t arena = arena_select_locked();
#endif
#if defined(MIMIR_PLACEMENT_FIRSTFIT) || defined(MIMIR_PLACEMENT_BESTFIT)
    /* Z-059 trial: policy selection replaces the banked
     * active-then-first-fit choice below. */
    struct mimir_span_header *span = placement_select_locked(
        class_index
#ifdef MIMIR_ARENA_CCD
        , arena
#endif
        );
    void *obj;

    if (span == NULL) {
        return NULL;
    }
#else
    struct mimir_span_header *span = mimir_class_active[class_index];
    void *obj;

    if (span == NULL || !span_header_is_live(span) ||
        !span_can_serve(span)) {
        span = mimir_span_head;
        while (span != NULL) {
            if (span->class_index == class_index &&
                span_header_is_live(span) && span_can_serve(span)
#ifdef MIMIR_ARENA_CCD
                && arena_of(span) == arena
#endif
                ) {
                break;
            }
            span = span->list_next;
        }
        if (span == NULL) {
            span = span_acquire_locked(class_index);
            if (span == NULL) {
                return NULL;
            }
        }
        mimir_class_active[class_index] = span;
    }
#endif /* Z-059 placement trial */

#ifdef MIMIR_OOL_META
    if (ool_span_is(span)) {
        /* Z-034: serve from the bitmap; no block byte is
         * read or written on this road. */
        size_t ool_idx = ool_take_locked(span);

        if (ool_idx == (size_t)-1) {
            return NULL;
        }
        obj = (void *)((unsigned char *)span->mapping_base +
                       MIMIR_SPAN_HEADER_SIZE +
                       ool_idx * class_size);
    } else
#endif
    if (span->free_head != NULL &&
        span_free_node_valid(span, span->free_head)) {
        obj = span->free_head;
        span->free_head = *(void **)obj;
#if MIMIR_DFREE_POISON_ON
        /* Z-070: a reused block's poison verifies before
         * publication (carved blocks never pass here). */
        dfree_poison_verify(obj, class_size,
                            MIMIR_DIAG_OP_MALLOC);
#endif
#ifdef MIMIR_DEBUG
        /* J-004: a reused slot's guards were allocator-owned
         * while the slot was free; verify before refilling. */
        mimir_dbg_check_or_abort(obj, class_size, "alloc-reuse");
#endif
    } else {
#ifdef MIMIR_DEBUG
        obj = (void *)((unsigned char *)span->mapping_base +
                       MIMIR_SPAN_HEADER_SIZE +
                       MIMIR_DBG_GUARD_SIZE +
                       span->carve_count *
                           mimir_dbg_stride(class_size));
#else
        /* J-006: a free_head that fails shape validation is
         * never handed out and never followed; the span
         * serves by carving (span_can_serve has already
         * established carve room on this path). The corrupt
         * head stays in place for the metadata checker to
         * flag (mimir_core_metadata_check); the free nodes
         * behind it are stranded, not trusted. A VALID-
         * looking head that names a live object still
         * passes -- shape cannot tell live from free
         * (INC-J006-04). */
        obj = (void *)((unsigned char *)span->mapping_base +
                       MIMIR_SPAN_HEADER_SIZE +
                       span->carve_count * class_size);
#endif
        span->carve_count += 1;
    }
#ifdef MIMIR_DEBUG
    mimir_dbg_fill(obj, class_size);
#endif

    if (zero_user) {
        memset(obj, 0, class_size);
    }

    span->live_count += 1;
    mimir_live_small_objects += 1;
    /* J-002: requested = admitted size, allocated = the
     * class extent actually granted. */
    mimir_stats_bytes_requested += size;
    mimir_stats_bytes_allocated += class_size;
    __atomic_fetch_add(&mimir_audit_small_alloc, 1UL,
                       __ATOMIC_RELAXED);
    if (out_span != NULL) {
        *out_span = span;
    }
    return obj;
}

/*
 * Retire one live span object, with the lock held (the
 * span form of retire exactly once): the object is
 * threaded onto its span's free list -- its first 8 bytes
 * become the link, which is allocator metadata written
 * only as ownership leaves the caller (G-002 S7) -- and
 * if the span thereby becomes EMPTY beyond the per-class
 * retention bound it is released whole in this same
 * locked section. A release failure leaves the span
 * retained as an empty span (span_release_locked has
 * already rolled itself back); the object stays freed
 * either way, so this helper has no failure return.
 */
static void small_free_locked(void *ptr)
{
#ifdef MIMIR_LOCK_SHARD
    struct mimir_span_header *span = find_span_for_free_any_locked(ptr);
#else
    struct mimir_span_header *span = find_span_for_free_locked(ptr);
#endif

    if (span == NULL) {
        return;
    }
#ifdef MIMIR_DEBUG
    /* J-004: verify BEFORE the free-list link overwrites the
     * user extent's first 8 bytes; on mismatch the block is
     * NOT freed (report + defined stop inside). */
    mimir_dbg_check_or_abort(ptr, span->class_size, "free");
#endif
#ifdef MIMIR_DEBUG
    /* J-005: poison the whole class extent and hold the
     * block in the bounded quarantine instead of threading
     * it onto the free list. live_count and the audit
     * counters move exactly as in production: ownership
     * left the caller at this instant; quarantine is an
     * allocator-internal hold before reuse, not caller
     * liveness. The retention release below additionally
     * requires that the span hold no quarantined blocks:
     * a span is never released out from under its
     * quarantine (S5.3; INC-J005-02 records the retention
     * convergence this implies). */
    debug_poison_fill(ptr, span->class_size);
    span->live_count -= 1;
    mimir_live_small_objects -= 1;
    __atomic_fetch_add(&mimir_audit_small_free, 1UL,
                       __ATOMIC_RELAXED);
    debug_quarantine_insert_locked(ptr, span);
    if (span->live_count == 0 &&
        debug_span_quarantine_count_locked(span) == 0 &&
        tls_binned_count_locked(span) == 0 &&
        empty_spans_of_class_locked(span->class_index) >
            mimir_retention_bound) {
        /* Z-067: the retention course releases only on a
         * proven count relation (INC-J006-06). */
        if (span_live_audit_locked(span)) {
            (void)span_release_locked(span);
        }
    }
#else
#ifdef MIMIR_OOL_META
    if (ool_span_is(span)) {
        /* Z-034: clear the bit; the block keeps every byte
         * the caller left in it. */
        ool_clear_locked(span, ptr);
    } else {
        *(void **)ptr = span->free_head;
        span->free_head = ptr;
    }
#else
    *(void **)ptr = span->free_head;
    span->free_head = ptr;
#endif
#if MIMIR_DFREE_POISON_ON
    /* Z-070: poison the freed extent past the link. */
    dfree_poison_fill(ptr, span->class_size);
#endif
    span->live_count -= 1;
    mimir_live_small_objects -= 1;
    __atomic_fetch_add(&mimir_audit_small_free, 1UL,
                       __ATOMIC_RELAXED);

    /* Z-043: the release trigger must also require that no
     * bin holds a block of THIS span. The tally counts both
     * bin-resident blocks and blocks published from a bin
     * whose live transition is still pending in the
     * allocating thread (the tally unit converts to a live
     * unit only at that thread's flush, tls_flush_locked),
     * so while the tally is non-zero this span can hold
     * blocks that are live in callers' hands with
     * live_count not yet reflecting them - and the
     * live_count == 0 above can be reached by this free
     * while such blocks exist. Releasing here unlinked and
     * unmapped the span out from under those blocks: their
     * later cross-thread frees found an owner record but no
     * registered span and silently did nothing (the Z-043
     * storm: 76 of 391,232 cross frees lost, plus a crash
     * in a bin drain touching the unmapped span). The
     * retention sweep (tls_retention_sweep_locked) and the
     * trim path already carry this exact guard; the
     * empty-span count alone does not imply it, because it
     * counts only tally-free spans and can be satisfied by
     * OTHER spans of the class. */
    if (span->live_count == 0 &&
#ifdef MIMIR_ARENA_CCD
        empty_spans_of_class_arena_locked(span->class_index,
                                          arena_of(span)) >
            arena_retention_bound()
#else
        /* Z-029: never release a span a bin still holds
         * blocks of; the class-wide empty count alone
         * does not prove THIS span is unreferenced. */
        tls_binned_count_locked(span) == 0 &&
        empty_spans_of_class_locked(span->class_index) >
            mimir_retention_bound
#endif
            ) {
        /* Z-067 (D-12): the retention course is a release
         * path that trusts live_count == 0; the span is
         * released only when the release audit proves its
         * count relation (INC-J006-06). A refused span
         * stays retained, tallied at the audit. */
        if (span_live_audit_locked(span)) {
            (void)span_release_locked(span);
        }
    }
#endif
}

#ifdef MIMIR_DEBUG
/*
 * J-004 on-demand verification (plan S5.2: canaries verified
 * at retire AND on demand). Walks every registered span
 * under the one global lock: a span whose header fails
 * span_header_is_live counts as one metadata corruption
 * (its object area is not trusted or walked); every carved
 * slot not on its span's free list has both guards checked.
 * Returns the number of corrupted units found (0 = clean).
 * Reports nothing and never aborts: the caller decides;
 * the free/realloc paths keep the abort posture. On lock
 * failure returns -1 (the G-007 getter posture). Internal
 * only, INC-E001-07 shape (b) via mimir_core.h under
 * MIMIR_DEBUG + tests/core/mimir_debug_j004.h.
 */
int mimir_core_debug_verify_all(void)
{
    struct mimir_span_header *span;
    int bad = 0;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return -1;
    }
    span = mimir_span_head;
    while (span != NULL) {
        size_t idx;
        size_t cap;

        if (!span_header_is_live(span)) {
            bad += 1;
            span = span->list_next;
            continue;
        }
        cap = span_capacity(span);
        if (cap > span->carve_count) {
            cap = span->carve_count;
        }
        for (idx = 0; idx < cap; idx++) {
            void *obj = (void *)(
                (unsigned char *)span->mapping_base +
                MIMIR_SPAN_HEADER_SIZE + MIMIR_DBG_GUARD_SIZE +
                idx * mimir_dbg_stride(span->class_size));
            void *node = span->free_head;
            int on_free = 0;

            while (node != NULL) {
                if (node == obj) {
                    on_free = 1;
                    break;
                }
                node = *(void **)node;
            }
            if (!on_free &&
                !mimir_dbg_intact(obj, span->class_size)) {
                bad += 1;
            }
        }
        span = span->list_next;
    }
    (void)core_global_unlock();
    return bad;
}
#endif /* MIMIR_DEBUG */

/* ==== Z-029 TLS cache =====================================
 * Per-thread freelist cache over the banked size classes,
 * implementing the Z-028 contract
 * (docs/Z13_TLS_CACHE_CONTRACT.md; slice doc
 * docs/Z13_TLS_CACHE_IMPLEMENTATION.md). Design summary:
 *
 * - Bins live in static _Thread_local storage: 20 classes x
 *   MIMIR_TLS_BIN_CAPACITY (16) block pointers plus this
 *   thread's counters and pending-accounting list. Cache
 *   storage is never allocated through mimir_malloc; first
 *   touch is zero-initialised static TLS plus a call_once
 *   tss registration, so initialisation is recursion-safe
 *   and never runs inside the central lock or through a
 *   public entry point (contract S9 risk addendum).
 * - A block held in a bin is FREE at core level (contract
 *   S9 TLS-ACCT): free-to-bin and cache hits move the core
 *   accounting (span live_count, mimir_live_small_objects,
 *   the J-002 byte counters) through a per-thread PENDING
 *   list, flushed under the central lock at the start of
 *   every locked core entry in this file (the allocating
 *   entries, stats snapshot, metadata check, trim / drain /
 *   retained queries, the tuning pair, drain_thread and
 *   the thread-exit drain). A locked per-span BINNED TALLY
 *   counts blocks held off the span free lists: refill
 *   increments it, drain / batch-drain decrements it, and
 *   a flush converts pending transitions (a hit moves one
 *   unit from tally to live_count; a free-to-bin moves one
 *   from live_count to tally). At every instant, for every
 *   span: free-list nodes + live_count + tally ==
 *   carve_count, and a span is releasable only when
 *   live_count == 0 AND tally == 0, so no span is ever
 *   released from under a bin or a pending transition.
 * - The hit path takes no lock and touches no shared-
 *   written state besides the pre-existing E-005-style
 *   atomic audit counter (the L1 boundary already counts
 *   every public call with the same relaxed atomic).
 *   Refill / drain take the central lock once per batch,
 *   never per block, and use only the internal locked
 *   helpers of this file (never a public entry point: the
 *   core lock is non-recursive).
 * - Ownership (contract S2): a lock-free atomic hash maps
 *   each live or binned small block to its allocating
 *   thread (thrd_t bits) and its span. It is written at
 *   refill and at locked small publications, read on the
 *   free path. A free whose recorded owner is another
 *   thread routes to the central core and counts
 *   remote_frees_out; the Z-028 suite (P7) pins this for
 *   an already-EXITED owner too, which is the reading
 *   implemented here (the contract doc S2 ownerless
 *   sentence is recorded as superseded by the suite in
 *   the Z-029 slice doc). If the table cannot record a
 *   block (probe exhaustion), its later free falls back
 *   to the legacy two-registry path and is accounted
 *   there (slice doc S7).
 * - Thread exit: the tss destructor (registered through
 *   call_once) flushes and drains the exiting thread's
 *   bins to the central core exactly once (a flag defeats
 *   destructor re-invocation) and bumps the process-global
 *   exit counters under the central lock. The main thread
 *   is covered by the image destructor, which drains the
 *   calling thread's cache (counted as an exit drain)
 *   before the unload drain releases retained spans
 *   (contract S6 ordering).
 * - MIMIR_DEBUG: the cache is BYPASSED entirely (contract
 *   S9's recorded road): every small operation takes the
 *   banked locked core path, so the J-004 red zones and
 *   the J-005 poison / quarantine transitions are
 *   byte-for-byte the banked ones. The surface below
 *   still links, with the constants exact and the
 *   counters zero, because no cache exists to count.
 * ========================================================== */
#include "../../tests/contract/mimir_tls_cache_z028.h"

_Static_assert(MIMIR_TLS_CLASS_COUNT == MIMIR_SMALL_CLASS_COUNT,
               "TLS class count must match the core class table (Z-029)");
_Static_assert(sizeof(thrd_t) == sizeof(uint64_t),
               "owner record stores a thrd_t as uint64_t bits (Z-029)");

/* Process-global thread-exit drain counters (contract S6):
 * written only under the central lock, read through atomic
 * loads at quiescent points. Defined in both flavours; a
 * MIMIR_DEBUG build never moves them (no cache exists). */
static uint64_t tls_exit_drains_count;
static uint64_t tls_exit_drain_blocks_count;

uint64_t mimir_tls_cache_exit_drains(void)
{
    return __atomic_load_n(&tls_exit_drains_count,
                           __ATOMIC_RELAXED);
}

uint64_t mimir_tls_cache_exit_drain_blocks(void)
{
    return __atomic_load_n(&tls_exit_drain_blocks_count,
                           __ATOMIC_RELAXED);
}

size_t mimir_tls_cache_bin_capacity(size_t class_index)
{
    return class_index < MIMIR_TLS_CLASS_COUNT
               ? MIMIR_TLS_BIN_CAPACITY
               : (size_t)0;
}

size_t mimir_tls_cache_batch(size_t class_index)
{
    return class_index < MIMIR_TLS_CLASS_COUNT
               ? MIMIR_TLS_BATCH
               : (size_t)0;
}

#ifdef MIMIR_DEBUG
/* Debug flavour: cache bypassed (see the block heading).
 * The tally is always 0, the flush is a no-op, the unload
 * hook does nothing, snapshots read as an empty cache and
 * drain_thread is a no-op: the banked debug core paths
 * below serve every operation exactly as before Z-029. */
static size_t tls_binned_count_locked(
    const struct mimir_span_header *span)
{
    (void)span;
    return 0;
}

static void tls_flush_locked(void)
{
}

static void tls_unload_drain(void)
{
}

int mimir_tls_cache_snapshot(struct mimir_tls_cache_snapshot *out)
{
    if (out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    return 0;
}

void mimir_tls_cache_drain_thread(void)
{
}
#else /* !MIMIR_DEBUG */
/* ---- owner record (contract S2) ------------------------
 * Fixed open-addressing table in static storage, updated
 * with __atomic builtins only (the E-005 discipline): no
 * lock is taken to read or write a record. key is 0 for an
 * empty slot, TLS_OWNER_TOMBSTONE for a cleared record
 * (probe chains stay intact), otherwise the block pointer
 * itself. owner / span are meaningful only while key is a
 * block pointer. Publication order on set: claim the key
 * with a CAS, then store span and owner; a reader that
 * finds its key re-reads the pair (a block's record is
 * stable from publication until its free: the allocating
 *   thread writes it before the block can be handed on). */
#define TLS_OWNER_SLOTS ((size_t)65536)
#define TLS_OWNER_PROBE ((size_t)256)
#define TLS_OWNER_TOMBSTONE ((uintptr_t)1)

struct tls_owner_entry { /* Z-040: one entry per 64 B line (Z-016 rule) */
    _Alignas(64) uintptr_t key;
    uint64_t owner;
    uintptr_t span;
    unsigned char z040_pad[40]; /* 24 B fields + 40 B pad = one 64 B line */
};
_Static_assert(offsetof(struct tls_owner_entry, span) == 16, "owner span offset unchanged (Z-040)");
_Static_assert(offsetof(struct tls_owner_entry, owner) == 8, "owner owner offset unchanged (Z-040)");
_Static_assert(sizeof(struct tls_owner_entry) == 64, "owner entry is exactly one 64 B line (Z-040)");
static struct tls_owner_entry tls_owner_table[TLS_OWNER_SLOTS];

static uint64_t tls_self_bits(void)
{
    thrd_t self = thrd_current();
    uint64_t bits;
    memcpy(&bits, &self, sizeof bits);
    return bits;
}

static size_t tls_owner_index(uintptr_t ptr)
{
    return (size_t)(((ptr >> 4) *
                     UINT64_C(0x9E3779B97F4A7C15)) >>
                    48) &
           (TLS_OWNER_SLOTS - 1);
}

static int tls_owner_set(void *ptr, uint64_t owner,
                         const struct mimir_span_header *span)
{
    uintptr_t key = (uintptr_t)ptr;
    size_t i;
    size_t claim = TLS_OWNER_SLOTS;
    /* Scan the chain for an existing record FIRST: a block
     * has at most one record, ever. Claiming the first
     * tombstone met without completing the scan would
     * duplicate a record that sits further down the chain
     * (placed there when earlier slots were occupied);
     * after the nearer record is cleared, readers would
     * find the stale one -- a zombie with an old owner,
     * span, and handed flag -- and account the block a
     * second time. An empty slot ends the search: no
     * record can exist beyond it. */
    for (i = 0; i < TLS_OWNER_PROBE; i++) {
        struct tls_owner_entry *e =
            &tls_owner_table[(tls_owner_index(key) + i) &
                             (TLS_OWNER_SLOTS - 1)];
        uintptr_t seen =
            __atomic_load_n(&e->key, __ATOMIC_ACQUIRE);
        if (seen == key) {
            __atomic_store_n(&e->span, (uintptr_t)span,
                             __ATOMIC_RELAXED);
            __atomic_store_n(&e->owner, owner,
                             __ATOMIC_RELEASE);
            return 1;
        }
        if (seen == TLS_OWNER_TOMBSTONE) {
            if (claim == TLS_OWNER_SLOTS) {
                claim = i;
            }
            continue;
        }
        if (seen == 0) {
            break;
        }
    }
    if (claim == TLS_OWNER_SLOTS) {
        if (i >= TLS_OWNER_PROBE) {
            return 0; /* probe exhausted (S7) */
        }
        claim = i; /* the empty slot that ended the scan */
    }
    {
        struct tls_owner_entry *e =
            &tls_owner_table[(tls_owner_index(key) + claim) &
                             (TLS_OWNER_SLOTS - 1)];
        uintptr_t expected =
            __atomic_load_n(&e->key, __ATOMIC_RELAXED);
        if (expected != 0 && expected != TLS_OWNER_TOMBSTONE &&
            expected != key) {
            return 0;
        }
        if (!__atomic_compare_exchange_n(
                &e->key, &expected, key, 0,
                __ATOMIC_ACQ_REL, __ATOMIC_RELAXED) &&
            expected != key) {
            return 0;
        }
        __atomic_store_n(&e->span, (uintptr_t)span,
                         __ATOMIC_RELAXED);
        __atomic_store_n(&e->owner, owner,
                         __ATOMIC_RELEASE);
        return 1;
    }
}

static int tls_owner_get(const void *ptr, uint64_t *out_owner,
                         struct mimir_span_header **out_span)
{
    uintptr_t key = (uintptr_t)ptr;
    size_t i;
    for (i = 0; i < TLS_OWNER_PROBE; i++) {
        struct tls_owner_entry *e =
            &tls_owner_table[(tls_owner_index(key) + i) &
                             (TLS_OWNER_SLOTS - 1)];
        uintptr_t seen =
            __atomic_load_n(&e->key, __ATOMIC_ACQUIRE);
        if (seen == 0) {
            return 0;
        }
        if (seen == key) {
            *out_owner =
                __atomic_load_n(&e->owner, __ATOMIC_RELAXED);
            *out_span = (struct mimir_span_header *)
                (__atomic_load_n(&e->span, __ATOMIC_RELAXED) &
                 ~(uintptr_t)1);
            return 1;
        }
    }
    return 0;
}

static void tls_owner_clear(const void *ptr)
{
    uintptr_t key = (uintptr_t)ptr;
    size_t i;
    for (i = 0; i < TLS_OWNER_PROBE; i++) {
        struct tls_owner_entry *e =
            &tls_owner_table[(tls_owner_index(key) + i) &
                             (TLS_OWNER_SLOTS - 1)];
        uintptr_t seen =
            __atomic_load_n(&e->key, __ATOMIC_ACQUIRE);
        if (seen == 0) {
            return;
        }
        if (seen == key) {
            __atomic_store_n(&e->owner, 0, __ATOMIC_RELAXED);
            __atomic_store_n(&e->span, 0, __ATOMIC_RELAXED);
            __atomic_store_n(&e->key, TLS_OWNER_TOMBSTONE,
                             __ATOMIC_RELEASE);
            return;
        }
    }
}

/* The handed flag: bit 0 of the record's span word (span
 * headers are page-aligned, so the bit is free). It marks
 * a block handed out by a cache HIT whose live accounting
 * is still listed in the owner's handed list: the binned
 * tally still counts the block and the locked live count
 * does not yet. The flag has exactly one writer per
 * lifecycle phase -- the owner sets it at the hit and
 * clears it when the block is binned, retired or flushed;
 * a cross-thread freer clears the whole record when it
 * returns the block against the tally. */
static int tls_owner_is_handed(const void *ptr)
{
    uintptr_t key = (uintptr_t)ptr;
    size_t i;
    for (i = 0; i < TLS_OWNER_PROBE; i++) {
        struct tls_owner_entry *e =
            &tls_owner_table[(tls_owner_index(key) + i) &
                             (TLS_OWNER_SLOTS - 1)];
        uintptr_t seen =
            __atomic_load_n(&e->key, __ATOMIC_ACQUIRE);
        if (seen == 0) {
            return 0;
        }
        if (seen == key) {
            return (__atomic_load_n(&e->span,
                                    __ATOMIC_RELAXED) &
                    (uintptr_t)1) != 0;
        }
    }
    return 0;
}

static void tls_owner_set_handed(void *ptr, int handed)
{
    uintptr_t key = (uintptr_t)ptr;
    size_t i;
    for (i = 0; i < TLS_OWNER_PROBE; i++) {
        struct tls_owner_entry *e =
            &tls_owner_table[(tls_owner_index(key) + i) &
                             (TLS_OWNER_SLOTS - 1)];
        uintptr_t seen =
            __atomic_load_n(&e->key, __ATOMIC_ACQUIRE);
        if (seen == 0) {
            return;
        }
        if (seen == key) {
            uintptr_t raw =
                __atomic_load_n(&e->span, __ATOMIC_RELAXED);
            raw = (raw & ~(uintptr_t)1) |
                  (handed ? (uintptr_t)1 : (uintptr_t)0);
            __atomic_store_n(&e->span, raw, __ATOMIC_RELEASE);
            return;
        }
    }
}

/* ---- per-thread cache state -----------------------------
 * Static _Thread_local storage only (contract S9): no
 * allocation, no tid-indexed shared array (a recycled tid
 * can never alias a dead thread's bin). The struct is
 * cache-line aligned so one thread's hot counters never
 * share a line with a neighbour thread's TLS block. */
#define TLS_PENDING_SPANS ((size_t)64)

struct tls_pending_entry {
    struct mimir_span_header *span;
    int32_t delta; /* - frees-to-bin of settled blocks */
};

/* One unconverted cache-hit handout. The flush converts
 * exactly the listed blocks whose records still carry the
 * handed flag; entries whose flag was cleared meanwhile
 * (binned, retired, or returned by a cross-thread freer)
 * are already accounted and are skipped. */
#define TLS_HANDED_CAP ((size_t)64)
struct tls_handed_entry {
    void *ptr;
    struct mimir_span_header *span;
    uint32_t req;
    uint32_t alloc;
};

struct tls_state {
    void *bins[MIMIR_TLS_CLASS_COUNT][MIMIR_TLS_BIN_CAPACITY];
    uint8_t bin_len[MIMIR_TLS_CLASS_COUNT];
    /* Per-slot settled bit: 1 while the binned block's
     * accounting unit still sits in its span's live count
     * with a pending -1 in this thread's pending list (a
     * settled free-to-bin not yet flushed). A hit that
     * pops a bit-1 block cancels the pending unit and
     * leaves the block live-counted; a hit that pops a
     * bit-0 block takes the handed path (its unit sits in
     * the binned tally). The flush clears the bit on the
     * blocks whose pending units it converts. */
    uint8_t bin_settled[MIMIR_TLS_CLASS_COUNT]
                       [MIMIR_TLS_BIN_CAPACITY];
    uint64_t hits;
    uint64_t misses;
    uint64_t refills;
    uint64_t refill_blocks;
    uint64_t drains;
    uint64_t drain_blocks;
    uint64_t local_frees;
    uint64_t local_frees_to_core;
    uint64_t remote_frees_out;
    uint64_t calloc_served;
    struct tls_pending_entry pending[TLS_PENDING_SPANS];
    size_t pending_len;
    struct tls_handed_entry handed[TLS_HANDED_CAP];
    size_t handed_len;
    uint64_t pending_req;
    uint64_t pending_alloc;
    int initialized;
    int exit_drained;
};
static _Thread_local _Alignas(64) struct tls_state tls;

static void tls_tss_destructor(void *value);
static tss_t tls_key;
static once_flag tls_once = ONCE_FLAG_INIT;

static void tls_register_key(void)
{
    (void)tss_create(&tls_key, tls_tss_destructor);
}

/* First-touch initialisation: zero-initialised static TLS
 * IS the empty cache; the only act is registering for the
 * thread-exit drain, through call_once, never under the
 * central lock and never through a public entry point. */
static void tls_ensure_init(void)
{
    if (tls.initialized) {
        return;
    }
    tls.initialized = 1;
    call_once(&tls_once, tls_register_key);
    (void)tss_set(tls_key, (void *)&tls);
}

/* ---- binned tally (locked) ------------------------------
 * Blocks held in bins, per span, maintained only under
 * the central lock. Together with the pending list this
 * keeps free_nodes + live_count + tally == carve_count
 * true at every instant (block heading), and a span with
 * a nonzero tally is never released. */
#define TLS_TALLY_SLOTS ((size_t)4096)
#ifndef MIMIR_LOCK_SHARD
static struct {
    struct mimir_span_header *span;
    uint32_t count;
} tls_tally[TLS_TALLY_SLOTS];
#endif
#ifdef MIMIR_LOCK_SHARD
/* Z-039: one tally table per shard. The macro selects by
 * the span argument's own class, so every tally call site
 * (parameter named span in both tally functions) lands in
 * its span's shard table under its span's shard lock. */
static struct {
    struct mimir_span_header *span;
    uint32_t count;
} tls_shard_tally[MIMIR_SHARD_COUNT][TLS_TALLY_SLOTS];
#define tls_tally (tls_shard_tally[shard_of_class((size_t)span->class_index)])
#endif

static void tally_add(struct mimir_span_header *span,
                      int32_t delta)
{
    size_t i;
    size_t empty = TLS_TALLY_SLOTS;
    for (i = 0; i < TLS_TALLY_SLOTS; i++) {
        if (tls_tally[i].span == span) {
            int64_t next =
                (int64_t)tls_tally[i].count + (int64_t)delta;
            if (next <= 0) {
                tls_tally[i].span = NULL;
                tls_tally[i].count = 0;
            } else {
                tls_tally[i].count = (uint32_t)next;
            }
            return;
        }
        if (tls_tally[i].span == NULL &&
            empty == TLS_TALLY_SLOTS) {
            empty = i;
        }
    }
    if (delta > 0 && empty != TLS_TALLY_SLOTS) {
        tls_tally[empty].span = span;
        tls_tally[empty].count = (uint32_t)delta;
    }
    /* No slot (4096 spans hold binned blocks at once): the
     * increment is lost and the span may look releasable;
     * the bound is two orders past any suite geometry and
     * the loss is recorded in the slice doc (S7). */
}

static size_t tls_binned_count_locked(
    const struct mimir_span_header *span)
{
    size_t i;
    for (i = 0; i < TLS_TALLY_SLOTS; i++) {
        if (tls_tally[i].span == span) {
            return (size_t)tls_tally[i].count;
        }
    }
    return 0;
}

/* ---- pending accounting ---------------------------------
 * A hit records (span, +1) and the J-002 byte pair; a
 * free-to-bin records (span, -1). Entries merge by span,
 * so the list holds distinct spans only. */
static int tls_pending_add(struct mimir_span_header *span,
                           int32_t delta)
{
    size_t i;
    for (i = 0; i < tls.pending_len; i++) {
        if (tls.pending[i].span == span) {
            tls.pending[i].delta += delta;
            return 0;
        }
    }
    if (tls.pending_len >= TLS_PENDING_SPANS) {
        return -1;
    }
    tls.pending[tls.pending_len].span = span;
    tls.pending[tls.pending_len].delta = delta;
    tls.pending_len += 1;
    return 0;
}

/* Apply this thread's pending transitions. Caller holds
 * the central lock. A hit converts one tally unit into a
 * live_count unit; a free-to-bin converts one live_count
 * unit into a tally unit; the byte counters are monotone
 * and simply accumulate. */
static void tls_flush_locked(void)
{
    size_t i;
    if (tls.pending_len == 0 && tls.pending_req == 0 &&
        tls.pending_alloc == 0 && tls.handed_len == 0) {
        return;
    }
    for (i = 0; i < tls.pending_len; i++) {
        struct mimir_span_header *span = tls.pending[i].span;
        int32_t delta = tls.pending[i].delta;
        if (delta == 0) {
            continue;
        }
        span->live_count =
            (size_t)((int64_t)span->live_count + delta);
        mimir_live_small_objects =
            (size_t)((int64_t)mimir_live_small_objects +
                     delta);
        tally_add(span, -delta);
        if (delta < 0) {
            /* The converted units are tally-counted now:
             * clear the settled bit on that many of this
             * span's binned blocks (pending units are
             * fungible across the bin). */
            size_t ci = (size_t)span->class_index;
            int32_t left = -delta;
            size_t j;
            for (j = 0; j < tls.bin_len[ci] && left > 0;
                 j++) {
                uint64_t bo;
                struct mimir_span_header *bs = NULL;
                if (!tls.bin_settled[ci][j]) {
                    continue;
                }
                if (tls_owner_get(tls.bins[ci][j], &bo,
                                  &bs) &&
                    bs == span) {
                    tls.bin_settled[ci][j] = 0;
                    left--;
                }
            }
        }
    }
    tls.pending_len = 0;
    mimir_stats_bytes_requested += tls.pending_req;
    mimir_stats_bytes_allocated += tls.pending_alloc;
    tls.pending_req = 0;
    tls.pending_alloc = 0;
    /* Convert this thread's unconverted hit handouts:
     * each block still flagged handed moves one unit from
     * its span's binned tally to its live count, with its
     * byte pair. Blocks whose flag was cleared meanwhile
     * were accounted at that moment and are skipped. */
    for (i = 0; i < tls.handed_len; i++) {
        void *block = tls.handed[i].ptr;
        uint64_t o;
        struct mimir_span_header *s = NULL;
        if (block == NULL) {
            continue;
        }
        if (tls_owner_get(block, &o, &s) &&
            o == tls_self_bits() &&
            tls_owner_is_handed(block)) {
            struct mimir_span_header *span =
                tls.handed[i].span;
            span->live_count += 1;
            mimir_live_small_objects += 1;
            tally_add(span, -1);
            mimir_stats_bytes_requested +=
                (uint64_t)tls.handed[i].req;
            mimir_stats_bytes_allocated +=
                (uint64_t)tls.handed[i].alloc;
            tls_owner_set_handed(block, 0);
        }
    }
    tls.handed_len = 0;
}

#ifdef MIMIR_LOCK_SHARD
/* Z-039: convert the calling thread's pending/handed TLS
 * accounting, if any. The pending and handed lists are
 * thread-local, so the emptiness check needs no lock; a
 * non-empty conversion spans classes and therefore takes
 * the all-shards-then-global acquisition. Entries that
 * take a single shard call this FIRST, so the flush call
 * inside their locked section finds empty lists and is a
 * no-op there. On lock failure the lists stay pending --
 * the next central entry retries the conversion, exactly
 * the banked posture for a failed flush. */
static void shard_flush_phase(void)
{
    if (tls.pending_len == 0 && tls.pending_req == 0 &&
        tls.pending_alloc == 0 && tls.handed_len == 0) {
        return;
    }
    if (shard_lock_global_all() != MIMIR_PLATFORM_OK) {
        return;
    }
    tls_flush_locked();
    (void)core_global_unlock();
}

/* Z-039: search the shard lists in ascending order for a
 * live span containing ptr, keeping ONLY the found span's
 * shard locked (the caller unlocks it via *out_shard).
 * Returns NULL with *out_shard == SIZE_MAX when no list
 * holds the pointer, and NULL with *out_shard == (size_t)-2
 * when a shard lock failed (the F-07 posture: the caller
 * leaves the block LIVE). */
static struct mimir_span_header *shard_find_span_live(
    void *ptr, size_t *out_shard)
{
    size_t s;

    *out_shard = SIZE_MAX;
    for (s = 0; s < MIMIR_SHARD_COUNT; s++) {
        struct mimir_span_header *span;

        if (mimir_platform_shard_lock(s) != MIMIR_PLATFORM_OK) {
            *out_shard = (size_t)-2;
            return NULL;
        }
        mimir_shard_hint = s;
        span = find_span_live_locked(ptr);
        if (span != NULL) {
            *out_shard = s;
            return span;
        }
        (void)core_shard_unlock(s);
    }
    return NULL;
}

/* The unlock matching a one-class central section: the
 * class's shard in a shard build, the global lock in a
 * production build (the #else body is the only one a
 * production build compiles, and vice versa). */
static void core_central_unlock(size_t class_index)
{
    (void)core_shard_unlock(
        shard_of_class(class_index));
}
#else
static void core_central_unlock(size_t class_index)
{
    (void)class_index;
    (void)core_global_unlock();
}
#endif /* MIMIR_LOCK_SHARD */
/* ---- locked core helpers --------------------------------
 * Everything in this section runs with the central lock
 * held and uses only this file's internal locked machinery.
 */

/* Containment lookup for a bin block whose owner record
 * is unavailable: walk the span registry and validate the
 * block as a carved object start of the named class. */
static struct mimir_span_header *tls_find_span_locked(
    const void *ptr, size_t class_index)
{
#ifdef MIMIR_LOCK_SHARD
    mimir_shard_hint = shard_of_class(class_index);
#endif
    struct mimir_span_header *span = mimir_span_head;
    size_t steps = 0;
    while (span != NULL) {
        if (++steps > mimir_live_spans) {
            return NULL;
        }
        if ((size_t)span->class_index == class_index &&
            span_header_is_live(span) &&
            span_free_node_valid(span, ptr)) {
            return span;
        }
        span = span->list_next;
    }
    return NULL;
}

/* Take one FREE block of a class from the central core:
 * pop the serving span's free-list head, or carve the
 * next object. No live accounting moves: the block is
 * free at core level before and after the take (it moves
 * from the span free list into a bin). Mirrors the span
 * choice of small_alloc_locked. */
static void *tls_take_one_locked(
    size_t class_index, struct mimir_span_header **out_span)
{
#ifdef MIMIR_ARENA_CCD
    /* Z-037: the Wave-4 cache refills from the calling
     * thread's arena only (the partition is shared with
     * the direct small path by construction). */
    size_t arena = arena_select_locked();
#endif
#ifdef MIMIR_LOCK_SHARD
    mimir_shard_hint = shard_of_class(class_index);
#endif
#if defined(MIMIR_PLACEMENT_FIRSTFIT) || defined(MIMIR_PLACEMENT_BESTFIT)
    /* Z-059 trial: policy selection; mirrors small_alloc_locked. */
    struct mimir_span_header *span = placement_select_locked(
        class_index
#ifdef MIMIR_ARENA_CCD
        , arena
#endif
        );
    void *obj;
    if (span == NULL) {
        return NULL;
    }
#else
    struct mimir_span_header *span =
        mimir_class_active[class_index];
    void *obj;
    if (span == NULL || !span_header_is_live(span) ||
        !span_can_serve(span)) {
        span = mimir_span_head;
        while (span != NULL) {
            if (span->class_index == class_index &&
                span_header_is_live(span) &&
                span_can_serve(span)
#ifdef MIMIR_ARENA_CCD
                && arena_of(span) == arena
#endif
                ) {
                break;
            }
            span = span->list_next;
        }
        if (span == NULL) {
            span = span_acquire_locked(class_index);
            if (span == NULL) {
                return NULL;
            }
        }
        mimir_class_active[class_index] = span;
    }
#endif /* Z-059 placement trial */
    if (span->free_head != NULL &&
        span_free_node_valid(span, span->free_head)) {
        obj = span->free_head;
        span->free_head = *(void **)obj;
    } else {
        obj = (void *)((unsigned char *)span->mapping_base +
                       MIMIR_SPAN_HEADER_SIZE +
                       span->carve_count * span->class_size);
        span->carve_count += 1;
    }
#if MIMIR_DFREE_POISON_ON
    /* Z-070: every block that enters a bin is poisoned
     * here if it was not already (a block taken from the
     * free list was filled at its free; a FRESH CARVE was
     * never freed, so its poison is established now --
     * without this, a carved block sitting in a bin
     * would fail its handout verify on bytes no free
     * ever wrote). */
    dfree_poison_fill(obj, span->class_size);
#endif
    *out_span = span;
    return obj;
}

/* Refill the calling thread's (empty) bin of one class
 * with one batch from the central core. Taken blocks are
 * pushed in REVERSE take order, so the first block taken
 * (the span free-list head) is the bin top: the miss
 * handout observes central LIFO order (Z-028 P7). */
static size_t tls_refill_locked(size_t class_index)
{
    void *taken[MIMIR_TLS_BATCH];
    size_t moved = 0;
    size_t i;
    uint64_t self = tls_self_bits();
    while (moved < MIMIR_TLS_BATCH) {
        struct mimir_span_header *span = NULL;
        void *obj = tls_take_one_locked(class_index, &span);
        if (obj == NULL) {
            break;
        }
        tally_add(span, 1);
        (void)tls_owner_set(obj, self, span);
        taken[moved++] = obj;
    }
    for (i = moved; i-- > 0;) {
        tls.bins[class_index][tls.bin_len[class_index]] =
            taken[i];
        tls.bin_settled[class_index]
                       [tls.bin_len[class_index]] = 0;
        tls.bin_len[class_index]++;
    }
    if (moved > 0) {
        tls.refills += 1;
        tls.refill_blocks += (uint64_t)moved;
    }
    return moved;
}

/* Return one binned block to its span's free list (the
 * block stays FREE at core level: no live counter moves). */
static void tls_return_one_locked(
    void *block, struct mimir_span_header *span)
{
    *(void **)block = span->free_head;
    span->free_head = block;
    tally_add(span, -1);
}

/* Release touched spans that the returns just made EMPTY
 * beyond the retention bound (the small_free_locked tail
 * discipline, applied per drain batch). */
static void tls_retention_sweep_locked(
    struct mimir_span_header **spans, size_t n)
{
    size_t i;
    size_t j;
    for (i = 0; i < n; i++) {
        struct mimir_span_header *span = spans[i];
        int dup = 0;
        if (span == NULL) {
            continue;
        }
        for (j = 0; j < i; j++) {
            if (spans[j] == span) {
                dup = 1;
                break;
            }
        }
        if (dup) {
            continue;
        }
        if (span_header_is_live(span) &&
            span->live_count == 0 &&
            tls_binned_count_locked(span) == 0 &&
#ifdef MIMIR_ARENA_CCD
            empty_spans_of_class_arena_locked(span->class_index,
                                              arena_of(span)) >
                arena_retention_bound()
#else
            empty_spans_of_class_locked(span->class_index) >
                mimir_retention_bound
#endif
                ) {
            /* Z-067: the sweep is a release path trusting
             * live_count == 0 like any other; release only
             * on a proven count relation (INC-J006-06). */
            if (span_live_audit_locked(span)) {
                (void)span_release_locked(span);
            }
        }
    }
}

/* Drain one batch (the bottom, oldest blocks) of one
 * class bin to the central core: the free-to-full-bin
 * rule of contract S3. */
static void tls_drain_batch_locked(size_t class_index)
{
    struct mimir_span_header *touched[MIMIR_TLS_BATCH];
    size_t i;
    for (i = 0; i < MIMIR_TLS_BATCH; i++) {
        void *block = tls.bins[class_index][i];
        struct mimir_span_header *span = NULL;
        uint64_t owner;
        if (!tls_owner_get(block, &owner, &span) ||
            span == NULL) {
            span = tls_find_span_locked(block, class_index);
        }
        touched[i] = span;
        if (span != NULL) {
            tls_return_one_locked(block, span);
        }
        tls_owner_clear(block);
    }
    memmove(&tls.bins[class_index][0],
            &tls.bins[class_index][MIMIR_TLS_BATCH],
            (size_t)(tls.bin_len[class_index] -
                     (uint8_t)MIMIR_TLS_BATCH) *
                sizeof(void *));
    memmove(&tls.bin_settled[class_index][0],
            &tls.bin_settled[class_index][MIMIR_TLS_BATCH],
            (size_t)(tls.bin_len[class_index] -
                     (uint8_t)MIMIR_TLS_BATCH) *
                sizeof(uint8_t));
    tls.bin_len[class_index] -= (uint8_t)MIMIR_TLS_BATCH;
    tls.drains += 1;
    tls.drain_blocks += (uint64_t)MIMIR_TLS_BATCH;
    tls_retention_sweep_locked(touched, MIMIR_TLS_BATCH);
}

/* Drain every bin of the calling thread to the central
 * core. count_as_drain = 1 for an explicit drain (the
 * drains / drain_blocks counters move); = 0 at thread
 * exit, where the caller moves the exit counters. The
 * scratch list is static because the central lock
 * serialises every drain in the process. */
static struct mimir_span_header *
    tls_touched[MIMIR_TLS_CLASS_COUNT * MIMIR_TLS_BIN_CAPACITY];

static uint64_t tls_drain_all_locked(int count_as_drain)
{
    uint64_t returned = 0;
    size_t nt = 0;
    size_t ci;
    for (ci = 0; ci < MIMIR_TLS_CLASS_COUNT; ci++) {
        while (tls.bin_len[ci] > 0) {
            void *block =
                tls.bins[ci][--tls.bin_len[ci]];
            struct mimir_span_header *span = NULL;
            uint64_t owner;
            if (!tls_owner_get(block, &owner, &span) ||
                span == NULL) {
                span = tls_find_span_locked(block, ci);
            }
            if (span != NULL) {
                tls_return_one_locked(block, span);
                tls_touched[nt++] = span;
                returned += 1;
            }
            tls_owner_clear(block);
        }
    }
    if (count_as_drain && returned > 0) {
        tls.drains += 1;
        tls.drain_blocks += returned;
    }
    tls_retention_sweep_locked(tls_touched, nt);
    return returned;
}

/* Account one block handed out from a bin or a refill as
 * LIVE, under the lock (a miss handout, a realloc take):
 * the J-002 publication pair plus the live counters; the
 * block's tally unit is consumed. The caller performs the
 * E-005 audit increment. */
static void tls_account_publication_locked(
    struct mimir_span_header *span, size_t size,
    size_t class_size)
{
    span->live_count += 1;
    mimir_live_small_objects += 1;
    mimir_stats_bytes_requested += size;
    mimir_stats_bytes_allocated += class_size;
    tally_add(span, -1);
}

/* Return one HANDED block to its span's free list, with
 * the lock held: the block's live accounting still sits
 * in the owner's handed list, so the unit comes out of
 * the binned tally (which counts unconverted handouts),
 * never out of the live count. Used by the cross-thread
 * free and realloc paths for blocks whose owner record
 * carries the handed flag. Mirrors small_free_locked's
 * non-debug tail otherwise (audit, retention release
 * additionally requires an empty tally). */
static void small_free_tally_locked(void *ptr)
{
#ifdef MIMIR_LOCK_SHARD
    struct mimir_span_header *span =
        find_span_live_any_locked(ptr);
#else
    struct mimir_span_header *span =
        find_span_live_locked(ptr);
#endif
    if (span == NULL) {
        return;
    }
    *(void **)ptr = span->free_head;
    span->free_head = ptr;
#if MIMIR_DFREE_POISON_ON
    dfree_poison_fill(ptr, span->class_size);
#endif
    tally_add(span, -1);
    __atomic_fetch_add(&mimir_audit_small_free, 1UL,
                       __ATOMIC_RELAXED);
    if (span->live_count == 0 &&
        tls_binned_count_locked(span) == 0 &&
#ifdef MIMIR_ARENA_CCD
        empty_spans_of_class_arena_locked(span->class_index,
                                          arena_of(span)) >
            arena_retention_bound()
#else
        empty_spans_of_class_locked(span->class_index) >
            mimir_retention_bound
#endif
            ) {
        /* Z-067: the tally course's retention release
         * trusts live_count == 0 too; release only on a
         * proven count relation (INC-J006-06). */
        if (span_live_audit_locked(span)) {
            (void)span_release_locked(span);
        }
    }
}

/* Realloc's construction step, with the lock held: serve
 * the new block from the calling thread's bin (refilling
 * once if empty), accounting it exactly as
 * small_alloc_locked would, or return NULL so the caller
 * can fall back to alloc_any_locked. Hit / miss counters
 * do NOT move here: those count mimir_malloc / _calloc
 * events; the refill counters do move, as a refill is a
 * refill whichever entry asked for it. */
static void *tls_take_locked(size_t size, int zero_user)
{
    size_t class_index = class_index_for(size);
    void *block;
    struct mimir_span_header *span = NULL;
    uint64_t owner;
    if (tls.bin_len[class_index] == 0 &&
        tls_refill_locked(class_index) == 0) {
        return NULL;
    }
    block = tls.bins[class_index]
                   [--tls.bin_len[class_index]];
#if MIMIR_DFREE_POISON_ON
    dfree_poison_verify(block,
                        mimir_class_sizes[class_index],
                        MIMIR_DIAG_OP_REALLOC);
#endif
    {
        int was_settled =
            tls.bin_settled[class_index]
                           [tls.bin_len[class_index]];
        if (!tls_owner_get(block, &owner, &span) ||
            span == NULL) {
            span = tls_find_span_locked(block, class_index);
        }
        if (span == NULL) {
            tls.bins[class_index]
                [tls.bin_len[class_index]++] = block;
            return NULL;
        }
        if (was_settled &&
            tls_pending_add(span, 1) == 0) {
            /* The block's unit never left the live count;
             * cancel its pending -1 and book only the
             * byte pair (the lock is held). */
            mimir_stats_bytes_requested += (uint64_t)size;
            mimir_stats_bytes_allocated +=
                (uint64_t)mimir_class_sizes[class_index];
        } else {
            if (was_settled) {
                /* Defensive (the cancel cannot fail while
                 * the bit invariant holds): settle the
                 * pending unit into the tally first, then
                 * publish against it. */
                tls_flush_locked();
            }
            tls_account_publication_locked(
                span, size, mimir_class_sizes[class_index]);
        }
    }
    __atomic_fetch_add(&mimir_audit_small_alloc, 1UL,
                       __ATOMIC_RELAXED);
    if (zero_user) {
        memset(block, 0, mimir_class_sizes[class_index]);
    }
    return block;
}

/* Retire a realloc'd-away live block into the calling
 * thread's bin, with the lock held (accounting is direct
 * here, never pending): the S3 full-bin batch-drain rule
 * applies exactly as on the free path. The E-005 audit
 * increment matches the small_free_locked it replaces;
 * local_frees does NOT move (that counter counts
 * mimir_free calls). */
static void tls_retire_locked(
    void *ptr, struct mimir_span_header *span)
{
    size_t class_index = (size_t)span->class_index;
    /* Settle this thread's pending and handed accounting
     * first (the caller holds the central lock): after
     * the flush, a hit handout being retired is a settled
     * live block, and every binned block is tally-
     * counted, so the batch drain and the direct
     * accounting below are exact. */
    tls_flush_locked();
    if (tls.bin_len[class_index] >= MIMIR_TLS_BIN_CAPACITY) {
        tls_drain_batch_locked(class_index);
    }
    tls.bins[class_index][tls.bin_len[class_index]] = ptr;
    tls.bin_settled[class_index][tls.bin_len[class_index]] =
        0;
    tls.bin_len[class_index] += 1;
#if MIMIR_DFREE_POISON_ON
    /* Z-070: a retired block enters the bin freed; it
     * carries the same poison as a tls_free_local block. */
    dfree_poison_fill(ptr, span->class_size);
#endif
    span->live_count -= 1;
    mimir_live_small_objects -= 1;
    tally_add(span, 1);
    __atomic_fetch_add(&mimir_audit_small_free, 1UL,
                       __ATOMIC_RELAXED);
}

/* Thread-exit drain (contract S6): flush, return every
 * binned block to the central core exactly once, and move
 * the process-global exit counters under the lock. The
 * exit_drained flag is set BEFORE the drain so destructor
 * re-invocation (the tss value stays non-NULL) and the
 * unload hook can never double-drain or double-count. */
static void tls_tss_destructor(void *value)
{
    uint64_t returned;
    (void)value;
    if (!tls.initialized || tls.exit_drained) {
        return;
    }
    tls.exit_drained = 1;
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        /* Z-065 (D-02): an exit drain that cannot take the
         * lock is a drain lock failure, counted like the
         * drain entry point's own (below). */
        __atomic_fetch_add(&mimir_stats_drain_lock_failures,
                           1ULL, __ATOMIC_RELAXED);
        return;
    }
    tls_flush_locked();
    returned = tls_drain_all_locked(0);
    tls_exit_drains_count += 1;
    tls_exit_drain_blocks_count += returned;
    (void)core_global_unlock();
    /* Z-067: a retention-sweep refusal during the exit
     * drain is reported on the trim course, after the
     * unlock, on the exiting thread (whose detection
     * label this is). */
    z067_release_report(MIMIR_DIAG_OP_TRIM);
}

/* The image destructor's TLS step: the calling (main)
 * thread's cache drains as an exit drain before the
 * unload drain releases retained spans. */
static void tls_unload_drain(void)
{
    tls_tss_destructor(NULL);
}
/* ---- entry-point paths ---------------------------------- */

/* Small allocation through the cache (malloc / calloc).
 * Handles the full failure reporting of the entry points
 * it serves, exactly as the legacy bodies do (J-002 note
 * plus the J-007 report after unlock), so the dispatchers
 * in mimir_core_alloc / mimir_core_calloc return its
 * result directly. */
static void *tls_alloc(size_t size, int zero_user,
                       int is_calloc)
{
    size_t class_index = class_index_for(size);
    size_t class_size = mimir_class_sizes[class_index];
    int op = is_calloc ? MIMIR_DIAG_OP_CALLOC
                       : MIMIR_DIAG_OP_MALLOC;
    tls_ensure_init();
    if (tls.bin_len[class_index] > 0) {
        /* Cache hit: no lock, no shared-written state
         * beyond the audit atomic. */
        void *block =
            tls.bins[class_index]
                    [--tls.bin_len[class_index]];
        int was_settled =
            tls.bin_settled[class_index]
                           [tls.bin_len[class_index]];
        struct mimir_span_header *span = NULL;
        uint64_t owner;
        tls.hits += 1;
#if MIMIR_DFREE_POISON_ON
        /* Z-070: verify before the calloc memset below
         * can erase the evidence. */
        dfree_poison_verify(block, class_size, op);
#endif
        if (zero_user) {
            memset(block, 0, class_size);
        }
        if (is_calloc) {
            tls.calloc_served += 1;
        }
        (void)tls_owner_get(block, &owner, &span);
        if (span != NULL && was_settled &&
            tls_pending_add(span, 1) == 0) {
            /* The block's unit never left the live count:
             * the pending -1 from its free-to-bin is
             * cancelled by this handout, so the block is
             * simply live again -- no handed flag, no
             * entry; the byte pair rides the pending
             * scalars to the next flush. */
            tls.pending_req += (uint64_t)size;
            tls.pending_alloc += (uint64_t)class_size;
        } else if (span != NULL && !was_settled &&
                   tls.handed_len < TLS_HANDED_CAP) {
            /* Fast path: flag the block handed in its
             * owner record and list the handout; the
             * tally keeps counting it until this thread's
             * next flush converts the listing. */
            tls_owner_set_handed(block, 1);
            tls.handed[tls.handed_len].ptr = block;
            tls.handed[tls.handed_len].span = span;
            tls.handed[tls.handed_len].req = (uint32_t)size;
            tls.handed[tls.handed_len].alloc =
                (uint32_t)class_size;
            tls.handed_len += 1;
        } else if (mimir_platform_global_lock() ==
                   MIMIR_PLATFORM_OK) {
            /* Cold path (owner record lost or the handed
             * list full): flush, then account directly. */
            tls_flush_locked();
            if (span == NULL) {
                span = tls_find_span_locked(block,
                                            class_index);
            }
            if (span != NULL) {
                tls_account_publication_locked(span, size,
                                               class_size);
            } else {
                mimir_live_small_objects += 1;
                mimir_stats_bytes_requested +=
                    (uint64_t)size;
                mimir_stats_bytes_allocated +=
                    (uint64_t)class_size;
            }
            (void)core_global_unlock();
        }
        __atomic_fetch_add(&mimir_audit_small_alloc, 1UL,
                           __ATOMIC_RELAXED);
        return block;
    }
#ifndef MIMIR_LOCK_SHARD
    /* Cache miss: one locked refill, then the handout is
     * accounted under the same lock. */
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        mimir_stats_note_failure();
        mimir_core_diag_report(op, MIMIR_DIAG_CAUSE_LOCK,
                               (uint64_t)size);
        return NULL;
    }
    mimir_diag_last_cause = MIMIR_DIAG_CAUSE_INTERNAL;
    tls_flush_locked();
#else /* MIMIR_LOCK_SHARD */
    /* Z-039: the caller's pending accounting converts
     * first (all shards, briefly, only when non-empty);
     * the refill itself runs under this class's shard
     * alone. */
    shard_flush_phase();
    if (mimir_platform_shard_lock(
            shard_of_class(class_index)) != MIMIR_PLATFORM_OK) {
        mimir_stats_note_failure();
        mimir_core_diag_report(op, MIMIR_DIAG_CAUSE_LOCK,
                               (uint64_t)size);
        return NULL;
    }
    mimir_shard_hint = shard_of_class(class_index);
    mimir_diag_last_cause = MIMIR_DIAG_CAUSE_INTERNAL;
    tls_flush_locked();
#endif /* MIMIR_LOCK_SHARD */
    if (tls_refill_locked(class_index) == 0) {
        int cause = mimir_diag_last_cause;
        mimir_stats_note_failure();
        (void)core_central_unlock(class_index);
        mimir_core_diag_report(op, cause, (uint64_t)size);
        return NULL;
    }
    {
        void *block =
            tls.bins[class_index]
                    [--tls.bin_len[class_index]];
        struct mimir_span_header *span = NULL;
        uint64_t owner;
        (void)tls_owner_get(block, &owner, &span);
        if (span == NULL) {
            span = tls_find_span_locked(block, class_index);
        }
        tls.misses += 1;
        if (span != NULL) {
            tls_account_publication_locked(span, size,
                                           class_size);
        } else {
            mimir_live_small_objects += 1;
            mimir_stats_bytes_requested += (uint64_t)size;
            mimir_stats_bytes_allocated +=
                (uint64_t)class_size;
        }
        __atomic_fetch_add(&mimir_audit_small_alloc, 1UL,
                           __ATOMIC_RELAXED);
        if (zero_user) {
            memset(block, 0, class_size);
        }
        if (is_calloc) {
            tls.calloc_served += 1;
        }
        (void)core_central_unlock(class_index);
        return block;
    }
}

/* Free of a block the owner record assigns to the calling
 * thread: the block enters the calling thread's bin (S3:
 * a free to a full bin first drains one batch to the
 * central core, so the bin never exceeds capacity and the
 * triggering block is always cached). On lock failure
 * for that drain the block stays LIVE and nothing is
 * counted (the F-07 posture of the legacy free). */
static void tls_free_local(void *ptr,
                           struct mimir_span_header *span)
{
    size_t class_index = (size_t)span->class_index;
    int handed = tls_owner_is_handed(ptr);
    tls_ensure_init();
#if MIMIR_DFREE_MEMBER_ON
    /* Z-070: bin membership -- the double-free check
     * this course never had. A pointer already in this
     * thread's bin for its class is already freed;
     * binning it again would hand the same block out
     * twice. Refuse BEFORE any accounting moves: the
     * block stays exactly as freed, no counter moves,
     * the detection tallies and reports. The scan is
     * bounded by MIMIR_TLS_BIN_CAPACITY compares
     * against a thread-local array (no lock, no shared
     * state). A block drained out of the bin to the
     * central free list has lost its owner record with
     * the drain, so a later second free of it routes
     * to the central course and is refused by the
     * J-006 walk there -- every residence is covered
     * exactly once. */
    {
        size_t df_i;
        for (df_i = 0; df_i < tls.bin_len[class_index];
             df_i++) {
            if (tls.bins[class_index][df_i] == ptr) {
                dfree_note_double_free();
                return;
            }
        }
    }
#endif
    if (handed) {
        /* A hit handout returning before its conversion:
         * the tally already counts the block and the live
         * count never did, so binning it moves no locked
         * counter. Clear the flag, move its byte pair to
         * the pending scalars, and void its handed entry
         * so the flush skips it. */
        size_t i;
        tls_owner_set_handed(ptr, 0);
        for (i = 0; i < tls.handed_len; i++) {
            if (tls.handed[i].ptr == ptr) {
                tls.pending_req +=
                    (uint64_t)tls.handed[i].req;
                tls.pending_alloc +=
                    (uint64_t)tls.handed[i].alloc;
                tls.handed[i].ptr = NULL;
                break;
            }
        }
    }
#ifndef MIMIR_LOCK_SHARD
    if (tls.bin_len[class_index] >= MIMIR_TLS_BIN_CAPACITY) {
        if (mimir_platform_global_lock() !=
            MIMIR_PLATFORM_OK) {
            /* Z-065 (D-02): free-on-lock-failure -- the
             * block stays LIVE (F-07) and the course is
             * counted, never silent. */
            __atomic_fetch_add(&mimir_stats_free_lock_failures,
                               1ULL, __ATOMIC_RELAXED);
            return;
        }
        tls_flush_locked();
        tls_drain_batch_locked(class_index);
        (void)core_global_unlock();
        tls.local_frees_to_core += 1;
    }
#else /* MIMIR_LOCK_SHARD */
    if (tls.bin_len[class_index] >= MIMIR_TLS_BIN_CAPACITY) {
        shard_flush_phase();
        if (mimir_platform_shard_lock(
                shard_of_class(class_index)) !=
            MIMIR_PLATFORM_OK) {
            /* Z-065 (D-02): free-on-lock-failure, counted
             * (the block stays LIVE, F-07). */
            __atomic_fetch_add(&mimir_stats_free_lock_failures,
                               1ULL, __ATOMIC_RELAXED);
            return;
        }
        mimir_shard_hint = shard_of_class(class_index);
        tls_flush_locked();
        tls_drain_batch_locked(class_index);
        (void)core_shard_unlock(
            shard_of_class(class_index));
        tls.local_frees_to_core += 1;
    }
#endif /* MIMIR_LOCK_SHARD */
    {
        int settled_pend = 0;
        if (!handed) {
            if (tls_pending_add(span, -1) == 0) {
                settled_pend = 1;
#ifndef MIMIR_LOCK_SHARD
            } else if (mimir_platform_global_lock() ==
                       MIMIR_PLATFORM_OK) {
                /* Pending list full: account directly; the
                 * block is tally-counted from this moment. */
                tls_flush_locked();
                span->live_count -= 1;
                mimir_live_small_objects -= 1;
                tally_add(span, 1);
                (void)core_global_unlock();
            }
#else /* MIMIR_LOCK_SHARD */
            } else {
                shard_flush_phase();
                if (mimir_platform_shard_lock(
                        shard_of_class(class_index)) ==
                    MIMIR_PLATFORM_OK) {
                    mimir_shard_hint =
                        shard_of_class(class_index);
                    tls_flush_locked();
                    span->live_count -= 1;
                    mimir_live_small_objects -= 1;
                    tally_add(span, 1);
                    (void)core_shard_unlock(
                        shard_of_class(class_index));
                }
            }
#endif /* MIMIR_LOCK_SHARD */
        }
        tls.bins[class_index][tls.bin_len[class_index]] =
            ptr;
        tls.bin_settled[class_index]
                       [tls.bin_len[class_index]] =
            (uint8_t)settled_pend;
        tls.bin_len[class_index] += 1;
#if MIMIR_DFREE_POISON_ON
        dfree_poison_fill(ptr, span->class_size);
#endif
    }
    tls.local_frees += 1;
    __atomic_fetch_add(&mimir_audit_small_free, 1UL,
                       __ATOMIC_RELAXED);
}

/* ---- the Z-028 surface ---------------------------------- */

/* The calling thread's snapshot only. Reads thread-local
 * state exclusively: no allocation, no central lock, no
 * counter movement, no cache initialisation (contract
 * S2); a thread that never allocated reads its zero-
 * initialised static TLS as an empty cache. */
int mimir_tls_cache_snapshot(struct mimir_tls_cache_snapshot *out)
{
    size_t i;
    if (out == NULL) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    for (i = 0; i < MIMIR_TLS_CLASS_COUNT; i++) {
        out->per_class_cached[i] =
            (uint64_t)tls.bin_len[i];
        out->cached_blocks += (uint64_t)tls.bin_len[i];
        out->cached_bytes += (uint64_t)tls.bin_len[i] *
                             (uint64_t)mimir_class_sizes[i];
    }
    out->hits = tls.hits;
    out->misses = tls.misses;
    out->refills = tls.refills;
    out->refill_blocks = tls.refill_blocks;
    out->drains = tls.drains;
    out->drain_blocks = tls.drain_blocks;
    out->local_frees = tls.local_frees;
    out->local_frees_to_core = tls.local_frees_to_core;
    out->remote_frees_out = tls.remote_frees_out;
    out->calloc_served = tls.calloc_served;
    return 0;
}

void mimir_tls_cache_drain_thread(void)
{
    tls_ensure_init();
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return;
    }
    tls_flush_locked();
    (void)tls_drain_all_locked(1);
    (void)core_global_unlock();
    /* Z-067: a sweep refusal during the drain is reported
     * on the trim course (the sweep is a retention
     * release); the tallies moved at the audit. */
    z067_release_report(MIMIR_DIAG_OP_TRIM);
}
#endif /* !MIMIR_DEBUG */

/*
 * G-007 tuning surface (docs/PHASE_7_TUNING_PARAMETERS_G007.md).
 * Validation happens BEFORE any state is touched: an
 * out-of-range value is refused with -1 and the previous
 * effective value is kept exactly -- no partial application
 * is possible, as the whole surface is this one scalar,
 * stored once under the global lock. A lock failure is -2,
 * likewise with nothing changed. The getter reads the same
 * scalar under the same lock, so what a test reads back is
 * exactly what free-time decisions will use. The bound
 * applies at free time (RP-3): changing it never eagerly
 * releases already-retained spans; each class converges at
 * its next emptying free.
 */
int mimir_tune_set_retention_bound(size_t value)
{
    if (value > MIMIR_RETENTION_MAX) {
        return -1;
    }
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return -2;
    }
    tls_flush_locked(); /* Z-029 */
    mimir_retention_bound = value;
    (void)core_global_unlock();
    return 0;
}

size_t mimir_tune_get_retention_bound(void)
{
    size_t value;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return (size_t)-1;
    }
    tls_flush_locked(); /* Z-029 */
    value = mimir_retention_bound;
    (void)core_global_unlock();
    return value;
}

/*
 * I-003 trim core (docs/PHASE_9_TRIM_IMPLEMENTATION.md;
 * semantics fixed by I-002 Section 5). With the lock held,
 * release EMPTY retained spans whole through
 * span_release_locked, at most max_spans of them, in
 * class-fair round-robin order: each pass visits classes
 * 0..19 in index order and releases at most one span per
 * class, so a partial trim never strips one class while
 * another class cache survives untouched (I-002 S5.2.2).
 * Within a class the span released is the deepest EMPTY
 * span of that class in the registry: the registry inserts
 * at the head, so the deepest is the earliest ACQUIRED
 * still-registered empty span of the class. I-002 asked
 * for the earliest-EMPTIED span first; no became-empty
 * order is recorded anywhere (the 96-byte span header is
 * fully assigned, G-002 S7), so earliest-acquired is the
 * deterministic proxy implemented here and INC-I003-01
 * records the deviation. A span with live_count > 0 is
 * never selected (I-002 S5.2.1). On an L6 release failure
 * span_release_locked has already rolled the span back to
 * retained LIVE; the walk does not count it and continues
 * with the next class (I-002 S5.2.4). Returns the number
 * of spans actually released.
 */
static size_t trim_some_locked(size_t max_spans)
{
    size_t released = 0;

    while (released < max_spans) {
        int progress = 0;
        size_t class_index;

        for (class_index = 0;
             class_index < MIMIR_SMALL_CLASS_COUNT &&
             released < max_spans;
             class_index++) {
#ifdef MIMIR_LOCK_SHARD
            mimir_shard_hint = shard_of_class(class_index);
#endif
            struct mimir_span_header *span = mimir_span_head;
            struct mimir_span_header *victim = NULL;

            while (span != NULL) {
                if (span->class_index == class_index &&
                    span->live_count == 0
#ifdef MIMIR_DEBUG
                    /* J-005: never a quarantine holder. */
                    && debug_span_quarantine_count_locked(span) == 0
#endif
                    /* Z-029: never a bin holder. */
                    && tls_binned_count_locked(span) == 0 &&
                    /* Z-067: never a span whose live_count
                     * the release audit cannot prove
                     * (INC-J006-06); a corrupt candidate is
                     * skipped and tallied at the audit. */
                    span_live_audit_locked(span)) {
                    victim = span;
                }
                span = span->list_next;
            }
            if (victim != NULL &&
                span_release_locked(victim)) {
                released += 1;
                progress = 1;
            }
        }
        if (!progress) {
            break;
        }
    }
    return released;
}

/* Counted registry walk (lock held): the EMPTY spans and
 * their total mapping bytes -- Q-RETAINED of I-002 S3.2,
 * computed exactly from the registry under the lock,
 * never estimated. Every span is one page, so the byte
 * total is spans x that span mapping_length summed
 * exactly as recorded per span. */
static void retained_snapshot_locked(size_t *out_spans,
                                     size_t *out_bytes)
{
    struct mimir_span_header *span;
    size_t spans = 0;
    size_t bytes = 0;

#ifdef MIMIR_LOCK_SHARD
    size_t sh;
    for (sh = 0; sh < MIMIR_SHARD_COUNT; sh++) {
        mimir_shard_hint = sh;
#endif
    span = mimir_span_head;

    while (span != NULL) {
        if (span->live_count == 0
#ifdef MIMIR_DEBUG
            /* J-005: Q-RETAINED counts the retention cache
             * only; quarantine-held spans are reported by
             * their own quantity (S5.3). */
            && debug_span_quarantine_count_locked(span) == 0
#endif
            ) {
            spans += 1;
            bytes += span->mapping_length;
        }
        span = span->list_next;
    }
#ifdef MIMIR_LOCK_SHARD
    } /* for sh */
#endif
    if (out_spans != NULL) {
        *out_spans = spans;
    }
    if (out_bytes != NULL) {
        *out_bytes = bytes;
    }
}

/*
 * H-001 unload drain (docs/PHASE_7_UNLOAD_DRAIN_H001.md;
 * commissioned fix for INC-G008-02). The G-003 span path
 * retains up to mimir_retention_bound EMPTY spans per class
 * after their last object is freed (G-005 RP-2/RP-5); those
 * spans are separate L6 mappings, disjoint from the library
 * image, so before this slice a dlclose unmapped the image
 * and orphaned every retained span until process exit
 * (G-008: exactly 49,152 B = 3 pages per cycle for the
 * G-008 workload's 3 warmed classes). The drain below is
 * the single entry point that releases them: it takes the
 * one global lock, walks the span registry once, and
 * releases every span whose live_count is 0 whole through
 * span_release_locked (the banked L5/L6 release path, with
 * its F-06 rollback), counting the spans actually
 * released. A span holding even one live block is never
 * touched (INC-G008-03 posture, H-001 slice doc Section
 * 3): live blocks at unload stay exactly as live as they
 * were, their span stays mapped, and freeing them after
 * the image has unmapped remains undefined behaviour per
 * the public header -- the consumer duty is to free
 * before dlclose. Live large mappings are not drained
 * either; since Z-048 the drain DOES release the mapping
 * cache's pooled (dead) large mappings, through
 * pool_release_all_locked in this same locked section --
 * they are retained memory exactly like the empty spans,
 * and leaving them mapped would orphan them at dlclose
 * the way INC-G008-02 orphaned spans.
 *
 * The drain is NON-TERMINAL and IDEMPOTENT (the
 * INC-G006-03 safety property, re-examined in the H-001
 * slice doc Section 4): it changes no registry, counter,
 * or tuning state besides releasing the empty spans
 * themselves (span_release_locked keeps the span counters
 * exact), the lock is released before return, and the
 * allocator is fully usable afterwards -- a later
 * allocation simply acquires a fresh span. A second call
 * with nothing retained releases nothing and returns 0.
 * On lock failure nothing is released and 0 is returned:
 * no unlocked progress is ever made (B-006 S9.5 / F-07).
 * The walk captures each span's list_next before any
 * release, because span_release_locked unlinks (and on
 * its rollback path re-links) the span it is given; no
 * other span's link is written by a release.
 *
 * The entry point itself is INC-E001-07 shape (b)
 * internal state, exactly like the G-007 tuning pair:
 * declared in src/core/mimir_core.h for the core's own
 * translation units and re-declared for tests only in
 * tests/core/mimir_drain_h001.h; in the shared library
 * the core object is compiled -fvisibility=hidden and
 * the version script binds every non-public name local,
 * so it never appears in libmimiralloc.so's dynamic
 * symbol table (nm -D stays exactly 4) and NO public
 * symbol is added. Promotion to a public shutdown entry
 * point would require a future contract slice
 * (INC-G007-01 pattern).
 */
size_t mimir_core_drain_retained(void)
{
    size_t released;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        /* Z-065 (D-02): the return stays 0 (nothing was
         * released -- the safe posture is unchanged), but
         * the course is no longer indistinguishable from
         * nothing-retained: the drain lock-failure counter
         * moves. See docs/DECISION_LEDGER.md D-010. */
        __atomic_fetch_add(&mimir_stats_drain_lock_failures,
                           1ULL, __ATOMIC_RELAXED);
        return 0;
    }
    tls_flush_locked(); /* Z-029 */
#ifndef MIMIR_DEBUG
    /* Z-029: the calling thread's cached blocks return to
     * the central core first, so their spans become
     * releasable by the walk below. */
    (void)tls_drain_all_locked(1);
#endif
    /* I-003: the drain now shares the one trim walk
     * (trim_some_locked with no span limit), so drain and
     * on-demand trim are the same release operation with
     * different triggers, as I-002 S5.1 requires (one
     * release path, not a budget-flavoured variant). The
     * set of spans released is unchanged: every retained
     * EMPTY span, live spans and mappings never touched;
     * only the walk order changed (class-fair instead of
     * registry order), which cannot change how many bytes
     * a full release returns (I-002 S5.2 preamble). */
    released = trim_some_locked(SIZE_MAX);
    (void)pool_release_all_locked(); /* Z-048 trim trigger */
    (void)core_global_unlock();
    z067_release_report(MIMIR_DIAG_OP_DRAIN); /* Z-067 */
    return released;
}

/*
 * I-003 on-demand trim (docs/PHASE_9_TRIM_IMPLEMENTATION.md;
 * semantics fixed by I-002 Section 5). These entry points
 * are the H-001 drain operation invoked on demand: under
 * the one global lock they release retained EMPTY spans
 * whole through the shared trim walk and return how many
 * spans were actually released; bytes released are spans x
 * page size exactly, because every span is one page, and a
 * test reconciles them at a quiescent point against
 * mimir_core_retained_bytes before/after and the G-003
 * span_retired counter delta (I-002 S5.3). Live spans,
 * live small objects and live large mappings are never
 * touched (I-002 S5.2.1). Both forms are non-terminal and
 * idempotent exactly as the drain is: a call with nothing
 * retained returns 0, a lock failure releases nothing and
 * returns 0, and the allocator stays fully usable. The
 * _some form is the stop-at-need mechanism of I-002
 * S5.2.3: it releases at most max_spans spans in
 * class-fair order and then stops, without acquiring or
 * refusing anything -- budget enforcement itself remains
 * unimplemented (INC-I001-14 stands; see the slice doc).
 * The queries below read Q-RETAINED and Q-MAPPED (I-002
 * S3.2) as single locked snapshots; on lock failure they
 * return (size_t)-1 (the G-007 getter posture) rather than
 * a torn or invented figure. All five entry points are
 * INC-E001-07 shape (b) internal state, exactly like the
 * drain and the G-007 pair: declared in
 * src/core/mimir_core.h and re-declared for tests only in
 * tests/core/mimir_trim_i003.h; bound local in the shared
 * library, nm -D stays exactly 4, and NO public symbol is
 * added (the public-surface decision is recorded in the
 * slice doc: internal-only; a public trim API is owed to
 * a later ABI/contract slice).
 */
size_t mimir_core_trim_retained(void)
{
    size_t released;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        /* Z-065 (D-02): counted, per D-010 -- return 0 is
         * no longer the only evidence of this course. */
        __atomic_fetch_add(&mimir_stats_trim_lock_failures,
                           1ULL, __ATOMIC_RELAXED);
        return 0;
    }
    tls_flush_locked(); /* Z-029 */
#ifndef MIMIR_DEBUG
    (void)tls_drain_all_locked(1);
#endif
    released = trim_some_locked(SIZE_MAX);
    (void)pool_release_all_locked(); /* Z-048 trim trigger */
    (void)core_global_unlock();
    z067_release_report(MIMIR_DIAG_OP_TRIM); /* Z-067 */
    return released;
}

size_t mimir_core_trim_retained_some(size_t max_spans)
{
    size_t released;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        /* Z-065 (D-02): counted, per D-010. */
        __atomic_fetch_add(&mimir_stats_trim_lock_failures,
                           1ULL, __ATOMIC_RELAXED);
        return 0;
    }
    tls_flush_locked(); /* Z-029 */
#ifndef MIMIR_DEBUG
    (void)tls_drain_all_locked(1);
#endif
    released = trim_some_locked(max_spans);
    (void)core_global_unlock();
    z067_release_report(MIMIR_DIAG_OP_TRIM); /* Z-067 */
    return released;
}

/*
 * Z-077 public-trim core (roadmap Z-077; the composition
 * behind public mimir_trim in include/mimir_alloc.h, over
 * the I-003 machinery above and the Z-048 pool). One lock,
 * the same steps as mimir_core_trim_retained (TLS flush and
 * drain, Z-065 lock-failure tally, Z-067 release report):
 * the retained-span snapshot is taken under the lock, pad
 * is priced in whole spans (pad rounded UP -- a partial
 * span cannot be kept, and the rounding never keeps less
 * than pad), and trim_some_locked releases exactly the
 * surplus. pad == 0 keeps nothing and additionally
 * releases the whole mapping pool (the full-trim
 * composition of mimir_core_trim_retained); pad > 0 never
 * touches the pool (the _some contract). If the page size
 * cannot be read, pad cannot be priced and nothing is
 * released -- fail-calm, never fail-trim. Returns 1 iff at
 * least one span or pooled mapping was actually released.
 */
int mimir_core_trim(size_t pad)
{
    size_t spans;
    size_t keep;
    size_t released;
    size_t pool_released = 0;
    size_t page_size;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        /* Z-065 (D-02): counted, as in the forms above. */
        __atomic_fetch_add(&mimir_stats_trim_lock_failures,
                           1ULL, __ATOMIC_RELAXED);
        return 0;
    }
    tls_flush_locked(); /* Z-029 */
#ifndef MIMIR_DEBUG
    (void)tls_drain_all_locked(1);
#endif
    retained_snapshot_locked(&spans, NULL);
    if (pad == 0) {
        keep = 0;
    } else if (mimir_platform_page_size(&page_size) !=
                   MIMIR_PLATFORM_OK ||
               page_size == 0) {
        keep = spans; /* unpriceable pad: release nothing */
    } else {
        keep = pad / page_size + (pad % page_size != 0);
    }
    released = (spans > keep) ? trim_some_locked(spans - keep)
                              : 0;
    if (pad == 0) {
        pool_released = pool_release_all_locked(); /* Z-048 */
    }
    (void)core_global_unlock();
    z067_release_report(MIMIR_DIAG_OP_TRIM); /* Z-067 */
    return (released > 0 || pool_released > 0) ? 1 : 0;
}

size_t mimir_core_retained_spans(void)
{
    size_t spans;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return (size_t)-1;
    }
    tls_flush_locked(); /* Z-029 */
    retained_snapshot_locked(&spans, NULL);
    (void)core_global_unlock();
    return spans;
}

size_t mimir_core_retained_bytes(void)
{
    size_t bytes;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return (size_t)-1;
    }
    tls_flush_locked(); /* Z-029 */
    retained_snapshot_locked(NULL, &bytes);
    (void)core_global_unlock();
    return bytes;
}

size_t mimir_core_mapped_bytes(void)
{
    size_t mapped;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return (size_t)-1;
    }
    tls_flush_locked(); /* Z-029 */
    mapped = mimir_live_bytes + mimir_live_span_bytes +
             mimir_pool_bytes; /* Z-048: pooled is still mapped */
    (void)core_global_unlock();
    return mapped;
}

/* Z-048 mapping-cache queries: the pool's occupancy as a
 * single locked snapshot, G-007 getter posture ((size_t)-1
 * on lock failure). Internal only, INC-E001-07 shape (b)
 * exactly like the trim queries: declared in
 * src/core/mimir_core.h, bound local in the shared
 * library, nm -D stays exactly 4. */
size_t mimir_core_pool_count(void)
{
    size_t n;
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return (size_t)-1;
    }
    tls_flush_locked(); /* Z-029 */
    n = mimir_pool_count;
    (void)core_global_unlock();
    return n;
}

size_t mimir_core_pool_bytes(void)
{
    size_t n;
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return (size_t)-1;
    }
    tls_flush_locked(); /* Z-029 */
    n = mimir_pool_bytes;
    (void)core_global_unlock();
    return n;
}
/* ---- Z-050 trim policy under pressure --------------------
 * docs/Z13_TRIM_POLICY.md. The policy layer over the I-003
 * trim surface above: it decides WHEN retained EMPTY spans
 * go back to the OS, from two inputs only -- a PSI sample
 * (/proc/pressure/memory) and the retained pool's idle time
 * -- and it executes exclusively through the existing trim
 * entry points, so there is still exactly ONE release path
 * (span_release_locked via trim_some_locked, I-002 S5.1).
 * Conservative by construction (policy doc S2): with no
 * pressure and a young pool it does nothing; a read failure
 * does nothing (fail-calm: a broken pressure input must
 * never cause a trim, and never blocks one the caller
 * drives explicitly through mimir_core_trim_retained).
 * Thresholds are the policy of record, named here once:
 *   HIGH   some_avg10 >= 5.00 or full_avg10 >= 2.50 (PSI %)
 *   RAISED some_avg10 >= 1.00
 *   under RAISED, idle >= 10 s escalates PARTIAL -> FULL
 *   CALM (below RAISED): idle >= 60 s earns PARTIAL only
 * PARTIAL releases half the retained spans (rounded up),
 * class-fair, through mimir_core_trim_retained_some.
 * The idle clock is pool-level, derived at tick time from
 * the J-002 activity counters: while retained bytes exist
 * and bytes_requested + bytes_released has not moved since
 * the previous tick, the pool is idle. The policy's own
 * releases move bytes_released, so a trim restarts the
 * idle clock -- deliberate: a pool that just paid a trim
 * is never immediately re-trimmed by the calm band.
 * The tick statics below are advisory policy state, not
 * allocator state: the tick is a caller-driven evaluation
 * (no background thread exists in this allocator), and a
 * racing pair of ticks can at worst agree on a decision
 * the table would have taken anyway. */
#define MIMIR_Z050_PSI_HIGH_SOME 5.0
#define MIMIR_Z050_PSI_HIGH_FULL 2.5
#define MIMIR_Z050_PSI_RAISED_SOME 1.0
#define MIMIR_Z050_IDLE_RAISED_S ((uint64_t)10)
#define MIMIR_Z050_IDLE_CALM_S ((uint64_t)60)

int mimir_pressure_parse(const char *text,
                         struct mimir_pressure_sample *out)
{
    struct mimir_pressure_sample parsed;
    const char *p;

    if (text == NULL || out == NULL) {
        return -1;
    }
    memset(&parsed, 0, sizeof parsed);
    p = strstr(text, "some avg10=");
    if (p == NULL) {
        return -1;
    }
    parsed.some_avg10 = strtod(p + strlen("some avg10="), NULL);
    /* The total= field sits at the END of each PSI line
     * ("some avg10=.. avg60=.. avg300=.. total=.."): there
     * is no "some total=" substring in the kernel format.
     * Anchor each total= search at its own line start. */
    p = strstr(text, " total=");
    if (p == NULL) {
        return -1;
    }
    parsed.some_total_us =
        strtoull(p + strlen(" total="), NULL, 10);
    p = strstr(text, "full avg10=");
    if (p == NULL) {
        return -1;
    }
    parsed.full_avg10 = strtod(p + strlen("full avg10="), NULL);
    p = strstr(p, " total=");
    if (p == NULL) {
        return -1;
    }
    parsed.full_total_us =
        strtoull(p + strlen(" total="), NULL, 10);
    /* strtod/strtoull accept empty fields as 0; the kernel
     * format always carries digits, and a text lacking the
     * avg300 marker between avg10 and total is not PSI. */
    if (strstr(text, "avg300=") == NULL) {
        return -1;
    }
    *out = parsed;
    return 0;
}

int mimir_core_pressure_read(struct mimir_pressure_sample *out)
{
    char buf[512];
    size_t n;
    FILE *f;

    if (out == NULL) {
        return -1;
    }
    f = fopen("/proc/pressure/memory", "r");
    if (f == NULL) {
        return -1;
    }
    n = fread(buf, 1, sizeof buf - 1, f);
    (void)fclose(f);
    if (n == 0) {
        return -1;
    }
    buf[n] = '\0';
    return mimir_pressure_parse(buf, out);
}

int mimir_trim_decision_for(const struct mimir_pressure_sample *sample,
                            uint64_t idle_seconds)
{
    if (sample == NULL) {
        return MIMIR_TRIM_DECISION_NONE;
    }
    if (sample->some_avg10 >= MIMIR_Z050_PSI_HIGH_SOME ||
        sample->full_avg10 >= MIMIR_Z050_PSI_HIGH_FULL) {
        return MIMIR_TRIM_DECISION_FULL;
    }
    if (sample->some_avg10 >= MIMIR_Z050_PSI_RAISED_SOME) {
        return idle_seconds >= MIMIR_Z050_IDLE_RAISED_S
                   ? MIMIR_TRIM_DECISION_FULL
                   : MIMIR_TRIM_DECISION_PARTIAL;
    }
    return idle_seconds >= MIMIR_Z050_IDLE_CALM_S
               ? MIMIR_TRIM_DECISION_PARTIAL
               : MIMIR_TRIM_DECISION_NONE;
}

size_t mimir_core_trim_policy_apply(
    const struct mimir_pressure_sample *sample,
    uint64_t idle_seconds, int *out_decision)
{
    int decision = mimir_trim_decision_for(sample, idle_seconds);
    size_t spans;

    if (out_decision != NULL) {
        *out_decision = decision;
    }
    if (decision == MIMIR_TRIM_DECISION_FULL) {
        return mimir_core_trim_retained();
    }
    if (decision == MIMIR_TRIM_DECISION_PARTIAL) {
        spans = mimir_core_retained_spans();
        if (spans == (size_t)-1 || spans == 0) {
            return 0;
        }
        return mimir_core_trim_retained_some((spans + 1) / 2);
    }
    return 0;
}

static uint64_t z050_policy_last_activity;
static uint64_t z050_policy_idle_since_s;
static int z050_policy_idle_valid;

static uint64_t z050_policy_now_s(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec;
}

int mimir_core_trim_policy_tick(struct mimir_trim_policy_report *report)
{
    struct mimir_stats stats;
    uint64_t activity;
    uint64_t now_s;

    if (report == NULL) {
        return -1;
    }
    memset(report, 0, sizeof *report);
    if (mimir_core_pressure_read(&report->sample) != 0) {
        return -1;
    }
    if (mimir_core_stats_snapshot(&stats) != 0) {
        return -1;
    }
    activity = stats.bytes_requested + stats.bytes_released;
    now_s = z050_policy_now_s();
    if (!z050_policy_idle_valid ||
        activity != z050_policy_last_activity) {
        z050_policy_last_activity = activity;
        z050_policy_idle_since_s = now_s;
        z050_policy_idle_valid = 1;
        report->idle_seconds = 0;
    } else {
        report->idle_seconds = now_s - z050_policy_idle_since_s;
    }
    report->retained_before = mimir_core_retained_bytes();
    if (report->retained_before == (size_t)-1) {
        return -1;
    }
    report->released_spans = mimir_core_trim_policy_apply(
        &report->sample, report->idle_seconds, &report->decision);
    report->retained_after = mimir_core_retained_bytes();
    return 0;
}

/*
 * J-002 statistics snapshot (docs/PHASE_10_STATISTICS.md).
 * One locked snapshot: the global lock is taken once, every
 * field is copied or walked inside that single locked
 * section, and the lock is released before return, so a
 * snapshot never mixes instants. bytes_cached and the
 * span_* occupancy fields come from one registry walk in
 * the same section (the retained_snapshot_locked
 * discipline, generalised per class). On a NULL out or a
 * lock failure it returns -1 with *out untouched (G-007
 * getter posture); the page size is discovered through L6
 * BEFORE the lock is taken (it needs no lock) and is 0 if
 * discovery failed. lock_contention is reported ABSENT
 * (INC-J002-02): the fields are the constant pair 0 / 0,
 * never a fabricated measure.
 */
/* Z-064 (D-01): the tally's lock-free read (declared in
 * mimir_core.h). One atomic load; takes no lock, so it
 * stays usable in exactly the state D-01 feared -- after a
 * genuinely failed unlock has wedged the mutex, where the
 * locked snapshot can no longer be taken. */
uint64_t mimir_core_unlock_failures(void)
{
    return __atomic_load_n(&mimir_stats_unlock_failures,
                           __ATOMIC_RELAXED);
}

_Static_assert(MIMIR_STATS_CLASS_COUNT == MIMIR_SMALL_CLASS_COUNT,
               "stats class count must match the core class table (J-002)");

int mimir_core_stats_snapshot(struct mimir_stats *out)
{
    struct mimir_span_header *span;
    size_t page_size = 0;
    size_t i;

    if (out == NULL) {
        return -1;
    }
    (void)mimir_platform_page_size(&page_size);
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return -1;
    }
    tls_flush_locked(); /* Z-029: caller's pending first */
    memset(out, 0, sizeof *out);
    out->bytes_requested = mimir_stats_bytes_requested;
    out->bytes_allocated = mimir_stats_bytes_allocated;
    out->bytes_released = mimir_stats_bytes_released;
    out->peak_mapped = mimir_stats_peak_mapped;
    out->alloc_failures = __atomic_load_n(
        &mimir_stats_alloc_failures, __ATOMIC_RELAXED);
    out->alloc_posture_nulls = __atomic_load_n(
        &mimir_stats_alloc_posture_nulls, __ATOMIC_RELAXED);
    out->drain_lock_failures = __atomic_load_n(
        &mimir_stats_drain_lock_failures, __ATOMIC_RELAXED);
    out->trim_lock_failures = __atomic_load_n(
        &mimir_stats_trim_lock_failures, __ATOMIC_RELAXED);
    out->free_lock_failures = __atomic_load_n(
        &mimir_stats_free_lock_failures, __ATOMIC_RELAXED);
    out->cross_thread_frees = mimir_stats_cross_thread_frees +
        mimir_stats_cross_thread_small; /* Z-076: both paths */
    out->cross_thread_small_frees =
        mimir_stats_cross_thread_small;
    out->small_frees_unrecorded = mimir_stats_small_unrecorded;
    out->live_requested_bytes = 0;     /* PRICED: INC-J002-05 */
    out->live_requested_measured = 0;  /* PRICED: INC-J002-05 */
    out->unlock_failures = __atomic_load_n(
        &mimir_stats_unlock_failures, __ATOMIC_RELAXED);
    out->lock_contention =
        mimir_platform_lock_contention(); /* Z-076: INC-J002-02 */
    out->lock_contention_measured = 1;    /* Z-076: INC-J002-02 */
    out->live_mappings = (uint64_t)mimir_live_mappings;
    out->live_spans = (uint64_t)mimir_live_spans;
    out->live_small_objects = (uint64_t)mimir_live_small_objects;
    out->live_mapping_bytes = (uint64_t)mimir_live_bytes;
    out->live_span_bytes = (uint64_t)mimir_live_span_bytes;
    out->bytes_mapped = (uint64_t)(mimir_live_bytes +
                                   mimir_live_span_bytes +
                                   mimir_pool_bytes);
    out->page_size_bytes = (uint64_t)page_size;
    out->metadata_detections = __atomic_load_n(
        &mimir_metadata_detections, __ATOMIC_RELAXED);
    out->metadata_collateral_refusals = __atomic_load_n(
        &mimir_metadata_collateral_refusals, __ATOMIC_RELAXED);
    out->live_release_detections = __atomic_load_n(
        &mimir_live_release_detections, __ATOMIC_RELAXED);
    out->live_release_collateral_refusals = __atomic_load_n(
        &mimir_live_release_collateral_refusals,
        __ATOMIC_RELAXED);
    out->double_free_detections = __atomic_load_n(
        &mimir_dfree_detections, __ATOMIC_RELAXED);
    out->poison_detections = __atomic_load_n(
        &mimir_poison_detections, __ATOMIC_RELAXED);
    for (i = 0; i < MIMIR_SMALL_CLASS_COUNT; i++) {
        out->span_class_size[i] = (uint64_t)mimir_class_sizes[i];
    }
#ifdef MIMIR_LOCK_SHARD
    { size_t sh; for (sh = 0; sh < MIMIR_SHARD_COUNT; sh++) { mimir_shard_hint = sh;
#endif
    span = mimir_span_head;
    while (span != NULL) {
        size_t idx = (size_t)span->class_index;
        if (idx < MIMIR_SMALL_CLASS_COUNT) {
            out->span_total_spans[idx] += 1;
            out->span_live_objects[idx] +=
                (uint64_t)span->live_count;
            out->span_capacity_objects[idx] +=
                (uint64_t)span_capacity(span);
            if (span->live_count == 0
#ifdef MIMIR_DEBUG
                /* J-005: D-CACHED keeps its production
                 * definition; a quarantine-held span is
                 * not EMPTY here (S5.3). */
                && debug_span_quarantine_count_locked(span) == 0
#endif
                ) {
                out->span_empty_spans[idx] += 1;
                out->bytes_cached +=
                    (uint64_t)span->mapping_length;
            }
        }
        span = span->list_next;
    }
#ifdef MIMIR_LOCK_SHARD
    } }
#endif
    (void)core_global_unlock();
    return 0;
}

/* Z-065 (D-02): lock-free reads of the three silent-course
 * tallies above. The stats snapshot is the reporting surface
 * of record, but a snapshot takes the very lock whose
 * failure is being counted; these getters are single atomic
 * loads, so each tally stays readable (and provable) even
 * then. Internal only, same INC-E001-07 shape (b) as the
 * snapshot: reached by tests through
 * tests/core/mimir_stats_j002.h's inclusion of
 * src/core/mimir_core.h; bound local in the shared library
 * (nm -D stays exactly 4). */
/* Z-076: lock-free reads of the two small-free tallies
 * (single atomic loads, the Z-064 getter pattern). */
uint64_t mimir_core_cross_thread_small_frees(void)
{
    return __atomic_load_n(&mimir_stats_cross_thread_small,
                           __ATOMIC_RELAXED);
}

uint64_t mimir_core_small_frees_unrecorded(void)
{
    return __atomic_load_n(&mimir_stats_small_unrecorded,
                           __ATOMIC_RELAXED);
}

uint64_t mimir_core_drain_lock_failures(void)
{
    return __atomic_load_n(&mimir_stats_drain_lock_failures,
                           __ATOMIC_RELAXED);
}

uint64_t mimir_core_trim_lock_failures(void)
{
    return __atomic_load_n(&mimir_stats_trim_lock_failures,
                           __ATOMIC_RELAXED);
}

uint64_t mimir_core_free_lock_failures(void)
{
    return __atomic_load_n(&mimir_stats_free_lock_failures,
                           __ATOMIC_RELAXED);
}

/*
 * J-006 metadata integrity check
 * (docs/PHASE_10_METADATA_INTEGRITY.md; J-001 S5.2 "extra
 * metadata consistency walks", the debug half of P10-9).
 * Under the one global lock, walk both registries with
 * bounded steps and verify, from the structures' own
 * fields: every descriptor and span header passes its
 * strengthened is_live validation; list_prev agrees with
 * the walk order; walked counts and byte sums equal the
 * static counters (live mappings / spans / bytes / small
 * objects); every span's free list consists only of
 * valid carved object starts and holds exactly
 * carve_count - live_count nodes; each class's active
 * hint is NULL or names a walked span of that class; and
 * descriptor flags / reserved bytes are still the zero
 * publication wrote (B-007 C1.4 -- the operational paths
 * never read them, so only this walk detects their
 * corruption). Returns the number of violations found
 * (0 = consistent), or -1 if the lock cannot be taken.
 * Nothing in the production entry points calls this
 * walk: it exists for test and debug use through the
 * INC-E001-07 shape (b) surface (declared here for the
 * core, re-declared for tests only in
 * tests/core/mimir_integrity_j006.h; bound local in the
 * shared library, nm -D stays exactly 4). The walk
 * follows registry links, so a list_next corrupted to
 * unmapped memory faults the CHECKER exactly as it
 * faults the operational walks (INC-J006-01); run it in
 * a disposable process when corruption is suspected.
 * No MIMIR_DEBUG build flavour exists at this base
 * (J-004 owns it, on a sibling branch): gating automatic
 * invocation under that flavour is INC-J006-08.
 */
int mimir_core_metadata_check(void)
{
    int violations = 0;
    struct mimir_descriptor *desc;
    struct mimir_descriptor *dprev = NULL;
    struct mimir_span_header *span;
    struct mimir_span_header *sprev = NULL;
    size_t count = 0;
    size_t bytes = 0;
    size_t live_sum = 0;
    size_t i;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return -1;
    }
    tls_flush_locked(); /* Z-029 */
    desc = mimir_registry_head;
    count = 0;
    bytes = 0;
    while (desc != NULL) {
        count++;
        if (count > mimir_live_mappings) {
            violations++; /* cycling registry */
            break;
        }
        if (descriptor_is_live(desc)) {
            bytes += desc->mapping_length;
        } else {
            violations++;
        }
        if (desc->list_prev != dprev) {
            violations++;
        }
        if (desc->flags != 0) {
            violations++;
        }
        for (i = 0; i < sizeof desc->reserved; i++) {
            if (desc->reserved[i] != 0) {
                violations++;
                break;
            }
        }
        dprev = desc;
        desc = desc->list_next;
    }
    if (count != mimir_live_mappings) {
        violations++;
    }
    if (bytes != mimir_live_bytes) {
        violations++;
    }
    /* Z-048: the mapping pool is a third list. Every entry
     * must be POOLED, self-based (descriptor at its mapping
     * base), zero-fielded exactly as publication wrote it,
     * with list_prev agreeing with the walk order; the
     * walked count and byte sum must equal the pool
     * counters. */
    {
        struct mimir_descriptor *pd = mimir_pool_head;
        struct mimir_descriptor *pprev = NULL;
        size_t pcount = 0;
        size_t pbytes = 0;
        while (pd != NULL) {
            pcount++;
            if (pcount > mimir_pool_count) {
                violations++; /* cycling pool list */
                break;
            }
            if (pd->magic == MIMIR_DESCRIPTOR_MAGIC &&
                pd->state == MIMIR_STATE_POOLED &&
                pd->mapping_base == (void *)pd &&
                pd->mapping_length != 0 &&
                pd->usable_offset != 0 &&
                pd->usable_offset <= pd->mapping_length) {
                pbytes += pd->mapping_length;
            } else {
                violations++;
            }
            if (pd->list_prev != pprev) {
                violations++;
            }
            if (pd->flags != 0) {
                violations++;
            }
            for (i = 0; i < sizeof pd->reserved; i++) {
                if (pd->reserved[i] != 0) {
                    violations++;
                    break;
                }
            }
            pprev = pd;
            pd = pd->list_next;
        }
        if (pcount != mimir_pool_count) {
            violations++;
        }
        if (pbytes != mimir_pool_bytes) {
            violations++;
        }
    }
#ifndef MIMIR_LOCK_SHARD
    span = mimir_span_head;
    count = 0;
    bytes = 0;
#else
    count = 0;
    bytes = 0;
    { size_t sh; for (sh = 0; sh < MIMIR_SHARD_COUNT; sh++) { mimir_shard_hint = sh; sprev = NULL;
    span = mimir_span_head;
#endif
    while (span != NULL) {
        count++;
        if (count > mimir_live_spans) {
            violations++; /* cycling registry */
            break;
        }
        if (span_header_is_live(span)) {
#ifdef MIMIR_ARENA_CCD
            /* Z-037: every live span must carry exactly
             * one owner entry; an unregistered live span
             * means the arena table drifted from the
             * registry, which no other check would see. */
            if (!arena_registered(span)) {
                violations++;
            }
#endif
#ifdef MIMIR_OOL_META
            /* Z-034: a trial span's free structure is its
             * bitmap: free_head must stay NULL and the set
             * bits must recount live_count exactly. */
            if (ool_span_is(span)) {
                bytes += span->mapping_length;
                live_sum += span->live_count;
                if (span->free_head != NULL ||
                    ool_popcount_locked(span) !=
                        span->live_count) {
                    violations++;
                }
            } else {
#endif
            void *node = span->free_head;
            size_t nodes = 0;
            int list_ok = 1;

            bytes += span->mapping_length;
            live_sum += span->live_count;
            while (node != NULL) {
                if (!span_free_node_valid(span, node) ||
                    ++nodes > span->carve_count) {
                    violations++;
                    list_ok = 0;
                    break;
                }
                node = *(void **)node;
            }
            if (list_ok &&
                nodes + span->live_count +
                        tls_binned_count_locked(span) !=
                    span->carve_count) {
                violations++;
            }
#ifdef MIMIR_OOL_META
            }
#endif
        } else {
            violations++;
        }
        if (span->list_prev != sprev) {
            violations++;
        }
        sprev = span;
        span = span->list_next;
    }
#ifdef MIMIR_LOCK_SHARD
    } }
#endif
    if (count != mimir_live_spans) {
        violations++;
    }
    if (bytes != mimir_live_span_bytes) {
        violations++;
    }
    if (live_sum != mimir_live_small_objects) {
        violations++;
    }
    for (i = 0; i < MIMIR_SMALL_CLASS_COUNT; i++) {
#ifdef MIMIR_ARENA_CCD
      /* Z-037: every arena's active hint must name a
       * registered span of this class OWNED by that arena. */
      size_t arena_i;
      for (arena_i = 0; arena_i < MIMIR_ARENA_COUNT; arena_i++) {
        struct mimir_span_header *want =
            mimir_arena_active[arena_i][i];
        struct mimir_span_header *walk = mimir_span_head;
        size_t steps = 0;
        int found = 0;

        if (want == NULL) {
            continue;
        }
        while (walk != NULL && ++steps <= mimir_live_spans) {
            if (walk == want) {
                found =
                    (size_t)walk->class_index == i &&
                    arena_of(walk) == arena_i;
                break;
            }
            walk = walk->list_next;
        }
        if (!found) {
            violations++;
        }
      }
#else
        struct mimir_span_header *want = mimir_class_active[i];
#ifdef MIMIR_LOCK_SHARD
        mimir_shard_hint = shard_of_class(i);
#endif
        struct mimir_span_header *walk = mimir_span_head;
        size_t steps = 0;
        int found = 0;

        if (want == NULL) {
            continue;
        }
        while (walk != NULL && ++steps <= mimir_live_spans) {
            if (walk == want) {
                found =
                    (size_t)walk->class_index == i;
                break;
            }
            walk = walk->list_next;
        }
        if (!found) {
            violations++;
        }
#endif
    }
    (void)core_global_unlock();
    return violations;
}

/*
 * The image destructor (H-001): registered in this
 * translation unit, it runs when this image is finalised
 * -- at dlclose, and at process exit for a statically
 * linked or never-unloaded image -- in each case before
 * the image unmaps. It does exactly one thing: call the
 * drain above, so every retained EMPTY span is released
 * through L6 while the code and the lock that own it are
 * still mapped. It allocates nothing, reports nothing
 * (no reporting channel exists at unload; the count is
 * deliberately discarded), and touches no live block.
 * Before this slice src/ registered no destructor at all
 * (G-006, INC-G006-03); the surface this one creates,
 * and why it stays safe, is re-examined in the H-001
 * slice doc Section 4. There is deliberately NO
 * constructor: initialisation stays clean by construction
 * (B-007 C2), exactly as before.
 */
__attribute__((destructor))
static void mimir_core_unload_drain(void)
{
    /* Z-029 (contract S6 ordering): the calling thread's
     * TLS cache drains before the retained spans release. */
    tls_unload_drain();
    (void)mimir_core_drain_retained();
}

/*
 * The one construction dispatcher (G-002 R-1), with the
 * lock held: 1..1024 goes to the class path, anything
 * larger to the banked alloc_locked mapping path,
 * byte-for-byte as before. Exactly one of *out_desc /
 * *out_span (when non-NULL) is set on success, so a
 * realloc rollback can retire the new block through
 * whichever registry published it.
 */
static void *alloc_any_locked(size_t size, int zero_user,
                              struct mimir_descriptor **out_desc,
                              struct mimir_span_header **out_span)
{
    if (out_desc != NULL) {
        *out_desc = NULL;
    }
    if (out_span != NULL) {
        *out_span = NULL;
    }
    if (size <= MIMIR_SMALL_CUTOFF) {
        return small_alloc_locked(size, zero_user, out_span);
    }
    return alloc_locked(size, zero_user, out_desc);
}

/* Retire a block constructed by alloc_any_locked through
 * the registry that published it (realloc rollback). */
static void retire_any_locked(void *ptr,
                              struct mimir_descriptor *desc)
{
    if (desc != NULL) {
        (void)retire_locked(desc);
    } else {
        small_free_locked(ptr);
    }
}

/* L2 admission (B-007 A1) + L4 lock wrapper over
 * alloc_locked: zero-size is not admitted as a supported
 * call (contract NULL since Z-078: U-09 settled by
 * D-013). On lock
 * failure fail with no ownership passing (F-07). The lock
 * is never held across the return (S9.4 item 1); if unlock
 * itself fails there is no safe report channel under the
 * design (no reporting under lock, S9.4 item 3) and the
 * allocation is already soundly published, so the pointer
 * is returned and the failure is not hidden by returning
 * overlapping or unpublished memory (S9.5) -- the C-003
 * wording, unchanged. */
void *mimir_core_alloc(size_t size)
{
#ifdef MIMIR_LOCK_HIST
    z19_set_op(MIMIR_LH_OP_MALLOC);
#endif
    void *result;

    if (size == 0) {
        mimir_stats_note_posture(); /* J-002: U-09 posture */
        return NULL;
    }
#ifndef MIMIR_DEBUG
    /* Z-029: small requests are served by the TLS cache. */
    if (size <= MIMIR_SMALL_CUTOFF) {
        return tls_alloc(size, 0, 0);
    }
#endif
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        mimir_stats_note_failure(); /* J-002: Q-FAILED-OS */
        /* J-007: report AFTER the failed lock attempt, no
         * lock held; the channel itself takes no lock. */
        mimir_core_diag_report(MIMIR_DIAG_OP_MALLOC,
                               MIMIR_DIAG_CAUSE_LOCK,
                               (uint64_t)size);
        return NULL;
    }
    mimir_diag_last_cause = MIMIR_DIAG_CAUSE_INTERNAL;
    tls_flush_locked(); /* Z-029 */
    result = alloc_any_locked(size, 0, NULL, NULL);
    if (result == NULL) {
        mimir_stats_note_failure(); /* J-002: Q-FAILED-OS */
    }
    (void)core_global_unlock();
    if (result == NULL) {
        /* J-007: emitted after unlock, from the cause the
         * failing construction path recorded. */
        mimir_core_diag_report(MIMIR_DIAG_OP_MALLOC,
                               mimir_diag_last_cause,
                               (uint64_t)size);
    }
    return result;
}

/*
 * mimir_core_calloc -- C-005, per B-008 S3 (design, by
 * analogy under U-03; contract IDs MA-6.1-CALLOC-*).
 *
 * Order of operations is B-008 S3.2 exactly:
 *   step 0: zero-factor gate before any arithmetic -- a
 *     zero factor is the D-013 contract null (MA-6.1-
 *     CALLOC-04; U-09 settled by Z-078) and is NOT routed
 *     through admission as a product of zero. The NULL
 *     answer is continuous with mimir_core_alloc(0); it
 *     began as a C-005 posture (tests/core/NOTES_C005.md,
 *     INC-C005-01) and is contract since Z-078.
 *   step 1: the product is checked before the
 *     multiplication is relied on (MA-6.1-CALLOC-02;
 *     B-007 A6 multiply form: nmemb <= SIZE_MAX / size,
 *     tested first, with size > 0 guaranteed by step 0).
 *   step 2: overflow is a Defined failure -- NULL, no
 *     wrapped allocation, nothing published. It is
 *     pre-admission: no mapping or descriptor exists, so
 *     the MA-6.2-024 rollback is satisfied vacuously.
 *   steps 3-6: the checked product is admitted exactly as
 *     a mimir_core_alloc(product) request, the usable
 *     extent is zeroed explicitly inside alloc_locked
 *     after the mapping succeeds and before publication,
 *     and the block is published LIVE and returned.
 *     Construction failure is the MA-6.1-CALLOC-03
 *     failure: NULL, no ownership passes. errno is not
 *     promised (U-10).
 */
void *mimir_core_calloc(size_t nmemb, size_t size)
{
#ifdef MIMIR_LOCK_HIST
    z19_set_op(MIMIR_LH_OP_CALLOC);
#endif
    size_t product;
    void *result;

    if (nmemb == 0 || size == 0) {
        mimir_stats_note_posture(); /* J-002: U-09 posture */
        return NULL;
    }
    if (nmemb > SIZE_MAX / size) {
        mimir_stats_note_failure(); /* J-002: Q-FAILED-OS */
        /* J-007: checked-arithmetic refusal. req=SIZE_MAX
         * marks the unrepresentable product (no product
         * value exists to report). */
        mimir_core_diag_report(MIMIR_DIAG_OP_CALLOC,
                               MIMIR_DIAG_CAUSE_ARITHMETIC,
                               (uint64_t)SIZE_MAX);
        return NULL;
    }
    product = nmemb * size;

#ifndef MIMIR_DEBUG
    /* Z-029: small callocs are routed through the cache. */
    if (product <= MIMIR_SMALL_CUTOFF) {
        return tls_alloc(product, 1, 1);
    }
#endif
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        mimir_stats_note_failure(); /* J-002: Q-FAILED-OS */
        mimir_core_diag_report(MIMIR_DIAG_OP_CALLOC,
                               MIMIR_DIAG_CAUSE_LOCK,
                               (uint64_t)product);
        return NULL;
    }
    mimir_diag_last_cause = MIMIR_DIAG_CAUSE_INTERNAL;
    tls_flush_locked(); /* Z-029 */
    result = alloc_any_locked(product, 1, NULL, NULL);
    if (result == NULL) {
        mimir_stats_note_failure(); /* J-002: Q-FAILED-OS */
    }
    (void)core_global_unlock();
    if (result == NULL) {
        mimir_core_diag_report(MIMIR_DIAG_OP_CALLOC,
                               mimir_diag_last_cause,
                               (uint64_t)product);
    }
    return result;
}

/*
 * Z-074 -- aligned allocation (Wave 9; roadmap Z-074,
 * implementing the contract-first slice Z-073's surface:
 * mimir_aligned_alloc and a posix_memalign-shaped entry,
 * in docs/contracts.md style under the namespaced U-03
 * by-analogy posture).
 *
 * Validity domain (this host's recorded rules, the V-03 /
 * U-11 debt the Z-073 contract takes up): an alignment is
 * VALID iff it is a non-zero power of two. The
 * posix_memalign-shaped entry additionally requires the
 * alignment to be a multiple of sizeof(void *), the POSIX
 * rule. Validity is a property of the value alone: a valid
 * alignment too large to satisfy is a FAILURE (NULL /
 * ENOMEM), never an invalidity. The size need NOT be a
 * multiple of the alignment (a recorded deviation from
 * C11 aligned_alloc, in the glibc posture). Zero size is
 * the family contract null (U-09 settled by D-013 at
 * Z-078): NULL from mimir_core_aligned_alloc; the posix
 * shape is ENOMEM with *memptr unchanged -- the Z-073
 * contract's interim posture, made final by D-013.
 *
 * Routing (over the core's machinery, by measurement):
 *   - alignment <= _Alignof(max_align_t) (16 on this
 *     host): the ordinary dispatch. Every pointer this
 *     allocator returns is aligned to the fundamental
 *     alignment -- small objects sit at multiples of 16
 *     from a 96-byte span offset with class sizes all
 *     multiples of 16, and mapping user pointers are
 *     page-aligned -- so the request is already served,
 *     the class machinery included.
 *   - 16 < alignment <= page size: the mapping path
 *     (alloc_locked) at any size. Its user pointer is
 *     page-aligned and a power-of-two alignment no larger
 *     than the page divides the page size, so the pointer
 *     is aligned as requested. The class machinery is
 *     deliberately NOT used here: the banked Z-017 audit
 *     measured it unable to promise these alignments
 *     (the 64 B class returns every pointer at
 *     ptr mod 64 == 32; the 48 B class splits 49.41%).
 *     The price is recorded honestly: such a request pays
 *     a whole mapping, exactly as a request above the
 *     1024-byte cutoff already does.
 *   - alignment > page size: alloc_aligned_locked, the
 *     mapping construction with a computed usable_offset.
 * Every aligned block is therefore a mapping-registry
 * block whose user pointer is exactly mapping_base +
 * usable_offset: mimir_core_free retires it through the
 * untouched mapping course, and mimir_core_realloc treats
 * it as any mapping block (same-size and shrink keep the
 * pointer and hence its alignment as a construction fact;
 * a grow move publishes through the ordinary dispatcher
 * and makes NO alignment promise -- the contract states
 * the default-alignment floor for realloc results).
 *
 * Failure behaviour: mimir_core_aligned_alloc fails with
 * exactly NULL (the family channel; errno is not promised,
 * U-10). mimir_core_posix_memalign returns 0 on success,
 * EINVAL for a null memptr or an invalid alignment (with
 * *memptr left unchanged in the alignment case; the null
 * memptr case is POSIX-undefined and is DEFINED here as
 * EINVAL, a recorded choice), and ENOMEM for a valid
 * request that cannot be satisfied (with *memptr left
 * unchanged). Validation precedence is fixed: memptr,
 * then alignment, then the zero-size posture, then
 * construction. The posix entry's return value IS its
 * error channel (the V-06 model); it does not set errno.
 */
static int z074_alignment_valid(size_t alignment)
{
    return alignment != 0 && (alignment & (alignment - 1)) == 0;
}

void *mimir_core_aligned_alloc(size_t alignment, size_t size)
{
    void *result;

    if (!z074_alignment_valid(alignment)) {
        mimir_stats_note_failure();
        mimir_core_diag_report(MIMIR_DIAG_OP_MALLOC,
                               MIMIR_DIAG_CAUSE_ARITHMETIC,
                               (uint64_t)alignment);
        return NULL;
    }
    if (size == 0) {
        mimir_stats_note_posture();
        return NULL;
    }
    if (alignment <= _Alignof(max_align_t)) {
        return mimir_core_alloc(size);
    }
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        mimir_stats_note_failure();
        mimir_core_diag_report(MIMIR_DIAG_OP_MALLOC,
                               MIMIR_DIAG_CAUSE_LOCK,
                               (uint64_t)size);
        return NULL;
    }
    mimir_diag_last_cause = MIMIR_DIAG_CAUSE_INTERNAL;
    tls_flush_locked();
    {
        size_t page_size = 0;
        if (mimir_platform_page_size(&page_size) !=
                MIMIR_PLATFORM_OK ||
            page_size == 0) {
            mimir_diag_last_cause = MIMIR_DIAG_CAUSE_PAGESIZE;
            result = NULL;
        } else if (alignment <= page_size) {
            result = alloc_locked(size, 0, NULL);
        } else {
            result = alloc_aligned_locked(alignment, size);
        }
    }
    if (result == NULL) {
        mimir_stats_note_failure();
    }
    (void)core_global_unlock();
    if (result == NULL) {
        mimir_core_diag_report(MIMIR_DIAG_OP_MALLOC,
                               mimir_diag_last_cause,
                               (uint64_t)size);
    }
    return result;
}

int mimir_core_posix_memalign(void **memptr, size_t alignment,
                              size_t size)
{
    void *result;

    if (memptr == NULL) {
        return EINVAL;
    }
    if (!z074_alignment_valid(alignment) ||
        alignment % sizeof(void *) != 0) {
        return EINVAL;
    }
    if (size == 0) {
        /* D-013 contract (Z-078 closed U-09, making the
         * Z-073 contract P3 interim posture final): a
         * zero-size posix-shaped request is ENOMEM with
         * *memptr left unchanged -- NOT the 0/NULL outcome
         * an earlier draft of this slice used; reconciled
         * to the landed Z-073 contract before banking. */
        return ENOMEM;
    }
    result = mimir_core_aligned_alloc(alignment, size);
    if (result == NULL) {
        return ENOMEM;
    }
    *memptr = result;
    return 0;
}

/*
 * Z-052 mremap growth (roadmap Wave 6): attempt to grow a
 * LIVE mapping-registry block through the L6 mremap
 * primitive, caller holds the lock and has already
 * validated the descriptor LIVE. Returns the grown block's
 * user pointer on success (the input pointer when the
 * kernel extended in place, a relocated pointer when it
 * moved the mapping), or NULL when the attempt is refused
 * -- in which case the block is completely untouched and
 * the caller runs the banked S4.5 move, whose own failure
 * semantics are unchanged. A growth that cannot be
 * satisfied either way is therefore exactly the banked
 * MA-6.1-REALLOC-02 failure: NULL, original preserved.
 *
 * The attempt is made ONLY when all of these hold:
 *   (a) the request outgrows the mapping itself
 *       (new_size > usable_size). A growth the recorded
 *       mapping already serves keeps the banked by-record
 *       move semantics (test_g004 F6 pins them: growth is
 *       judged by the requested-size record, not by usable
 *       slack), so no mremap shortcut exists there.
 *   (b) the grown mapping length exceeds the Z-048 mapping
 *       cache's byte bound MIMIR_POOL_MAX_BYTES. Inside the
 *       bound the banked move is the cache's own road (the
 *       retired mapping is pooled for exact-length reuse,
 *       and the I-004 fault legs pin the acquire/copy/
 *       retire behaviour there); beyond the bound a
 *       retired mapping can never be pooled, so the move
 *       pays a full acquire + copy + unmap for nothing the
 *       cache can reuse -- that is the growth mremap
 *       exists for. (The bound is also glibc's own
 *       mmap-threshold ceiling, 32 MiB: above it glibc
 *       likewise grows its mmapped chunks by mremap.)
 *
 * Why MAYMOVE and not in-place-only: measured on this
 * host (docs/Z13_MREMAP_GROWTH.md), the kernel's top-down
 * placement packs each fresh mapping directly against the
 * VMA above it (a fresh 34 MiB mapping ends exactly where
 * libc begins in /proc/self/maps), so an in-place-only
 * mremap of a fresh allocator mapping is refused ENOMEM
 * essentially always. MREMAP_MAYMOVE keeps the win in
 * both shapes: extension in place when the room above is
 * free (e.g. a freed neighbour), and a kernel page-table
 * relocation -- zero bytes copied -- when it is not.
 *
 * Mechanics under the lock: the descriptor is unlinked
 * from the live registry BEFORE the call (the neighbours
 * hold its address, which a relocation invalidates; its
 * own links are the only other registry pointers to it),
 * and re-linked at its resulting base after. The
 * descriptor itself rides inside the mapping's reserve
 * page, so its contents -- including the J-002 owner
 * record past it -- arrive intact at the new base; only
 * mapping_base, mapping_length, usable_size and
 * requested_size are rewritten, together, so every
 * relation descriptor_is_live checks stays exact. Live
 * bytes move by the mapping delta, the J-002
 * requested/allocated counters by the admitted deltas,
 * and the peak sampler re-runs. The E-005 audit counters
 * do NOT move: nothing was published and nothing retired
 * (published - retired still equals the live count). On
 * a kernel refusal the descriptor is re-linked at its
 * old base with every field untouched (mremap failure
 * leaves the old mapping intact), and NULL returns.
 * The arithmetic is the checked page round-up of
 * alloc_locked (check before evaluate, B-007 A5 form),
 * all of it tested before the registry is touched.
 */
static void *grow_mremap_locked(struct mimir_descriptor *desc,
                                void *user_ptr, size_t new_size)
{
    size_t page_size;
    size_t total_unrounded;
    size_t want_length;
    size_t old_length = desc->mapping_length;
    size_t old_usable = desc->usable_size;
    size_t old_requested = desc->requested_size;
    size_t usable_offset = desc->usable_offset;
    void *old_base = desc->mapping_base;
    void *new_base = NULL;
    size_t delta;

    if (new_size <= old_usable) {
        return NULL; /* (a): by-record move, banked semantics */
    }
    if (mimir_platform_page_size(&page_size) != MIMIR_PLATFORM_OK ||
        page_size == 0) {
        return NULL;
    }
    /* Checked arithmetic, check before evaluate (B-007
     * A3/A5 forms, as in alloc_locked): total =
     * usable_offset + new_size, then its page round-up. */
    if (new_size > SIZE_MAX - usable_offset) {
        return NULL;
    }
    total_unrounded = usable_offset + new_size;
    if (total_unrounded > SIZE_MAX - (page_size - 1)) {
        return NULL;
    }
    want_length = ((total_unrounded + page_size - 1) /
                   page_size) * page_size;
    if (want_length <= MIMIR_POOL_MAX_BYTES) {
        return NULL; /* (b): the mapping cache's road */
    }
    if (want_length <= old_length) {
        return NULL; /* unreachable given (a); never shrink */
    }
    delta = want_length - old_length;
    if (delta > SIZE_MAX - mimir_live_bytes) {
        return NULL; /* counter-overflow guard, pre-change */
    }

    registry_remove(desc);
    if (mimir_platform_map_grow(old_base, old_length,
                                want_length, &new_base) !=
        MIMIR_PLATFORM_OK) {
        registry_insert(desc); /* old base, fields untouched */
        return NULL;
    }
    /* The descriptor rode the mapping to new_base (the
     * same address when the kernel extended in place). */
    desc = (struct mimir_descriptor *)new_base;
    desc->mapping_base = new_base;
    desc->mapping_length = want_length;
    desc->usable_size = want_length - usable_offset;
    desc->requested_size = new_size;
    registry_insert(desc);
    mimir_live_bytes += delta;
    /* J-002: the grow admits the requested delta and
     * grants the usable delta; no other counter moves
     * (see the header: publication/retirement counts are
     * exact). */
    mimir_stats_bytes_requested += (uint64_t)(new_size - old_requested);
    mimir_stats_bytes_allocated +=
        (uint64_t)(desc->usable_size - old_usable);
    mimir_stats_peak_update_locked();
    (void)user_ptr; /* the caller returns what we return */
    return (void *)((unsigned char *)new_base + usable_offset);
}

/*
 * mimir_core_realloc -- C-005, per B-008 S4 (design, by
 * analogy under U-03; contract IDs MA-6.1-REALLOC-*).
 *
 * Case order:
 *   - ptr == NULL: behaves as mimir_core_alloc(new_size)
 *     (MA-6.1-REALLOC-03), including its failure cases and
 *     its zero-size posture (new_size == 0 inherits the
 *     U-09 posture by inheritance, B-008 S4.7).
 *   - new_size == 0 with a non-null ptr: the D-013
 *     contract case (MA-6.1-REALLOC-04; U-12 settled by
 *     Z-078). Like the calloc zero-factor gate it is
 *     decided before any allocator state is touched: zero
 *     is not admitted at L2 (B-007 A1), so the call
 *     returns NULL and leaves the original block LIVE,
 *     unchanged and caller-owned, exactly as an admission
 *     failure would -- the block is NOT freed. It began
 *     as a C-005 posture outside the B-008 S7.2 supported
 *     list (tests/core/NOTES_C005.md, INC-C005-01); hook
 *     C-006's per-target policy for this target is D-013.
 *   - invalid ptr (not a live in-scope block): UB
 *     (MA-6.1-REALLOC-05). This baseline takes the same
 *     non-destructive course as free: nothing is allocated,
 *     unmapped, unlinked or counted on suspicion, and NULL
 *     is returned. No behaviour is promised for such calls.
 *   - same size: the input pointer is returned unchanged,
 *     contents untouched (B-008 S4.3 design decision).
 *   - shrink: the original mapping is retained whole and
 *     the same pointer is returned; only the descriptor's
 *     requested_size record changes -- mapping_length (and
 *     usable_size, the physical extent) are never derived
 *     from the shrunken size, so a later free unmaps
 *     exactly the recorded mapping length (B-008 S4.4). A
 *     shrink frees no system memory and allocates nothing,
 *     so it cannot enter the null-return failure path.
 *   - grow: a move (B-008 S4.4: no in-place grow existed
 *     in this machine), EXCEPT the Z-052 mremap case: a
 *     growth whose new mapping length exceeds the Z-048
 *     mapping cache's byte bound is first attempted via
 *     the L6 mremap primitive (grow_mremap_locked below);
 *     only a refused attempt runs the S4.5 sequence. The
 *     B-008 S4.5 sequence runs
 *     under the one lock: validate, admit, construct the
 *     new block completely (alloc_locked; its registry
 *     publication is the internal step of MA-6.2-024 --
 *     the block is not yet owned by or addressable by the
 *     caller), copy min(old, new) == the old requested
 *     extent, then transfer: retire the old block exactly
 *     once via retire_locked. Any failure before the
 *     transfer is the MA-6.1-REALLOC-02 / MA-6.2-022
 *     failure: NULL, and the original block remains valid,
 *     unchanged in contents and ownership, and freeable
 *     exactly as before. If the old block's release itself
 *     fails at the transfer step, retire_locked has
 *     already restored it LIVE (F-06); the new block is
 *     then retired again as unpublished partial work
 *     (MA-6.2-024) and NULL is returned with the original
 *     preserved. (If that rollback release also failed,
 *     retire_locked leaves the new block LIVE and tracked
 *     rather than losing a mapping; NULL is still returned
 *     and the original is still preserved. Neither
 *     release-failure branch can be triggered through the
 *     public API on a healthy host -- no fault-injection
 *     hook exists in Phase 3 scope; they rest on code
 *     inspection, NOTES_C005.md INC-C005-03.)
 *   - the copy reads and writes user bytes under the lock:
 *     the transfer step of B-008 S4.5, read with the
 *     temporal boundary of S3.3 (the allocator touches
 *     user bytes only as part of moving ownership, never
 *     while a block is simply caller-owned). Recorded as
 *     INC-C005-06 in NOTES_C005.md.
 *   - lock failure at entry: NULL, original untouched (the
 *     F-07 posture). errno is not promised (U-10).
 */
void *mimir_core_realloc(void *ptr, size_t new_size)
{
#ifdef MIMIR_LOCK_HIST
    z19_set_op(MIMIR_LH_OP_REALLOC);
#endif
    struct mimir_descriptor *desc;
    struct mimir_descriptor *new_desc = NULL;
    void *new_ptr;
    size_t old_size;

    if (ptr == NULL) {
        return mimir_core_alloc(new_size);
    }
    if (new_size == 0) {
        mimir_stats_note_posture(); /* J-002: U-12 posture */
        return NULL;
    }
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        mimir_stats_note_failure(); /* J-002: Q-FAILED-OS */
        mimir_core_diag_report(MIMIR_DIAG_OP_REALLOC,
                               MIMIR_DIAG_CAUSE_LOCK,
                               (uint64_t)new_size);
        return NULL;
    }
    mimir_diag_last_cause = MIMIR_DIAG_CAUSE_INTERNAL;
    tls_flush_locked(); /* Z-029 */

    desc = find_candidate_locked(ptr);
    if (desc != NULL && descriptor_is_live(desc)) {
        /* Mapping-registry block: the banked B-008 S4 cases,
         * unchanged, except that the grow construction goes
         * through the R-1 dispatcher (a mapping block's
         * requested size is always above the cutoff, so a
         * grow always routes large in practice; the
         * dispatcher keeps the rule in exactly one place). */
        struct mimir_span_header *new_span = NULL;

        old_size = desc->requested_size;

        if (new_size == old_size) {
            (void)core_global_unlock();
            return ptr;
        }
        if (new_size < old_size) {
            desc->requested_size = new_size;
            (void)core_global_unlock();
            return ptr;
        }

        /* Z-052: an uncacheable growth is attempted via
         * mremap first (grow_mremap_locked names its exact
         * bounds and its rollback); on success it returns
         * the grown block's user pointer -- the SAME pointer
         * when the kernel extended the mapping in place, a
         * kernel-relocated one when it did not -- with no
         * copy, publication or retirement having happened. */
        {
            void *grown = grow_mremap_locked(desc, ptr, new_size);
            if (grown != NULL) {
                (void)core_global_unlock();
                return grown;
            }
        }

        /* Grow: construct the new block completely first
         * (S4.5 steps 2-3), copy (step 4), then transfer and
         * retire the old block exactly once (step 5). */
        new_ptr = alloc_any_locked(new_size, 0, &new_desc,
                                   &new_span);
        if (new_ptr == NULL) {
            int cause = mimir_diag_last_cause; /* J-007 */
            mimir_stats_note_failure(); /* J-002 */
            (void)core_global_unlock();
            mimir_core_diag_report(MIMIR_DIAG_OP_REALLOC,
                                   cause, (uint64_t)new_size);
            return NULL;
        }
        memcpy(new_ptr, ptr, old_size);

        if (!retire_locked(desc)) {
            mimir_stats_note_failure(); /* J-002: release failure */
            retire_any_locked(new_ptr, new_desc);
            (void)core_global_unlock();
            /* J-007: the transfer step's L6 release failed:
             * cause class backing (an OS release refusal) --
             * or, Z-066, the length audit refused the
             * retirement on detected corruption: cause
             * class metadata, and the label is consumed
             * here so no later free can inherit it. */
            mimir_core_diag_report(MIMIR_DIAG_OP_REALLOC,
                                   mimir_z066_detected
                                       ? MIMIR_DIAG_CAUSE_METADATA
                                       : MIMIR_DIAG_CAUSE_BACKING,
                                   (uint64_t)new_size);
            mimir_z066_detected = 0;
            return NULL;
        }
        (void)core_global_unlock();
        return new_ptr;
    }

    /* Span-registry block (G-003, G-002 R-3): the old
     * block's recorded size is its class size -- no
     * per-object requested-size record exists under the
     * G-002 span layout (INC-G003-03). Staying cases need
     * no record: any new_size the old class can serve
     * returns the same pointer with every byte physically
     * preserved, which covers same-size, shrink (the B-008
     * shrink posture carried down: the larger class is
     * retained) and grow-within-class in one rule. Moving
     * cases follow the banked S4.5 sequence through the
     * R-1 dispatcher for the NEW size -- construct
     * completely, copy, retire the old exactly once,
     * original preserved on any failure. The copy length
     * on a move is the old class extent: it covers the
     * caller's whole preserved range (requested <= class)
     * and is in-bounds on both sides; the bytes it copies
     * beyond the requested extent land in the new block's
     * unspecified range, where B-008 promises nothing
     * (INC-G003-03). Retiring a span object cannot fail
     * observably (small_free_locked), so no rollback
     * branch exists on this path. */
    {
#ifdef MIMIR_LOCK_SHARD
        struct mimir_span_header *span = find_span_live_any_locked(ptr);
#else
        struct mimir_span_header *span = find_span_live_locked(ptr);
#endif
        struct mimir_span_header *new_span = NULL;
        size_t span_class;

        if (span == NULL) {
            /* Not a defined realloc (B-008 S4, invalid ptr):
             * nothing allocated, unmapped, unlinked or
             * counted on suspicion; NULL, no promise. */
            (void)core_global_unlock();
            return NULL;
        }
        span_class = span->class_size;

#ifdef MIMIR_DEBUG
        /* J-004: realloc verifies the old block's guards
         * before ANY case (stay, shrink or move); on
         * mismatch the original is left exactly as found
         * and the defined stop fires inside. */
        mimir_dbg_check_or_abort(ptr, span_class, "realloc");
#endif
        if (new_size <= span_class) {
            (void)core_global_unlock();
            return ptr;
        }

#ifndef MIMIR_DEBUG
        /* Z-029: a small new block is served from the
         * calling thread's bin (refilling once if empty),
         * falling back to the locked dispatcher. */
        if (new_size <= MIMIR_SMALL_CUTOFF) {
            tls_ensure_init();
            new_ptr = tls_take_locked(new_size, 0);
            if (new_ptr == NULL) {
                new_ptr = alloc_any_locked(new_size, 0,
                                           &new_desc,
                                           &new_span);
                if (new_ptr != NULL && new_span != NULL) {
                    (void)tls_owner_set(new_ptr,
                                        tls_self_bits(),
                                        new_span);
                }
            }
        } else {
            new_ptr = alloc_any_locked(new_size, 0, &new_desc,
                                       &new_span);
        }
#else
        new_ptr = alloc_any_locked(new_size, 0, &new_desc,
                                   &new_span);
#endif
        if (new_ptr == NULL) {
            int cause = mimir_diag_last_cause; /* J-007 */
            mimir_stats_note_failure(); /* J-002 */
            (void)core_global_unlock();
            mimir_core_diag_report(MIMIR_DIAG_OP_REALLOC,
                                   cause, (uint64_t)new_size);
            return NULL;
        }
        memcpy(new_ptr, ptr, span_class);
#ifndef MIMIR_DEBUG
        {
            uint64_t ob;
            struct mimir_span_header *ospan = NULL;
            if (tls_owner_get(ptr, &ob, &ospan) &&
                ospan != NULL) {
                if (ob == tls_self_bits()) {
                    tls_retire_locked(ptr, ospan);
                } else if (tls_owner_is_handed(ptr)) {
                    small_free_tally_locked(ptr);
                    tls_owner_clear(ptr);
                } else {
                    small_free_locked(ptr);
                }
            } else {
                small_free_locked(ptr);
            }
        }
#else
        small_free_locked(ptr);
#endif
        (void)core_global_unlock();
        return new_ptr;
    }
}

/*
 * mimir_core_reallocarray -- Z-077 (Wave 9; roadmap Z-077),
 * per the contracts.md S2.8 conditional rows
 * MA-COND-REALLOCARRAY-01/-02/-03 (the namespaced surface
 * under U-03 takes them up directly, as Z-073/Z-074 did for
 * the aligned rows). The requested size is the product
 * nmemb * size, contracted exactly as in
 * mimir_core_calloc:
 *   step 0: zero-factor gate before any arithmetic -- a
 *     zero factor makes the product zero, which is the
 *     U-09 case with a null ptr and the U-12 case with a
 *     live one; BOTH are the D-013 contract null since
 *     Z-078 (NULL, original block live and untouched,
 *     counted as a posture null, never routed into
 *     mimir_core_realloc).
 *   step 1: the product is checked before the
 *     multiplication is relied on (nmemb <= SIZE_MAX / size,
 *     with size > 0 guaranteed by step 0).
 *   step 2: overflow is a Defined genuine failure -- NULL,
 *     no wrapped allocation, nothing published, the
 *     original block (if any) live, unchanged in contents
 *     and ownership and freeable exactly as before
 *     (MA-COND-REALLOCARRAY-02/-03 with MA-6.1-REALLOC-02).
 *     It is counted as a genuine failure (Q-FAILED-OS) and
 *     reported on the J-007 channel as op REALLOC / cause
 *     ARITHMETIC with req = SIZE_MAX, the calloc overflow
 *     shape verbatim (no product value exists to report).
 *   step 3: any other call is exactly
 *     mimir_core_realloc(ptr, product): NULL ptr as alloc,
 *     same-size / shrink / grow, and failure preservation
 *     all inherit the realloc case order above.
 * errno is not promised (U-10).
 */
void *mimir_core_reallocarray(void *ptr, size_t nmemb,
                              size_t size)
{
#ifdef MIMIR_LOCK_HIST
    z19_set_op(MIMIR_LH_OP_REALLOC);
#endif
    if (nmemb == 0 || size == 0) {
        mimir_stats_note_posture(); /* J-002: U-09/U-12 */
        return NULL;
    }
    if (nmemb > SIZE_MAX / size) {
        mimir_stats_note_failure(); /* J-002: Q-FAILED-OS */
        mimir_core_diag_report(MIMIR_DIAG_OP_REALLOC,
                               MIMIR_DIAG_CAUSE_ARITHMETIC,
                               (uint64_t)SIZE_MAX);
        return NULL;
    }
    return mimir_core_realloc(ptr, nmemb * size);
}

void mimir_core_free(void *ptr)
{
#ifdef MIMIR_LOCK_HIST
    z19_set_op(MIMIR_LH_OP_FREE);
#endif
    struct mimir_descriptor *desc;

    /* Defined no-op, before any locking or descriptor access
     * (B-006 S8.2 step 1; MA-6.1-FREE-02 by analogy). */
    if (ptr == NULL) {
        return;
    }
    /* Z-066: the detection label is per-call; a stale label
     * left by a realloc course on this thread must never be
     * reported as this call's detection. */
    mimir_z066_detected = 0;
    /* Z-067: the detection label is per-call; a stale label
     * left by another course on this thread must never be
     * reported as this call's detection (the Z-066
     * discipline). */
    mimir_z067_detected = 0;
    mimir_z070_df_detected = 0; /* Z-070: same discipline */

#ifndef MIMIR_DEBUG
    /* Z-029: the owner record routes the free first: the
     * calling thread's own small blocks enter its bin;
     * another thread's small block returns to the central
     * core under the lock and counts remote. A record that
     * disagrees with the span registry falls through to
     * the legacy path below. */
    {
        uint64_t ob;
        struct mimir_span_header *ospan = NULL;
        if (tls_owner_get(ptr, &ob, &ospan) && ospan != NULL) {
            if (ob == tls_self_bits()) {
                tls_free_local(ptr, ospan);
                z067_release_report(MIMIR_DIAG_OP_FREE);
                return;
            }
#ifndef MIMIR_LOCK_SHARD
            if (mimir_platform_global_lock() ==
                MIMIR_PLATFORM_OK) {
                tls_flush_locked();
                if (find_span_for_free_locked(ptr) != NULL) {
                    if (tls_owner_is_handed(ptr)) {
                        small_free_tally_locked(ptr);
                    } else {
                        small_free_locked(ptr);
                    }
                    /* Z-076 (INC-J002-01): this free's
                     * recorded owner is another thread and
                     * the block was a live span object --
                     * the small-path cross-thread event,
                     * counted exactly once, under the lock. */
                    mimir_stats_cross_thread_small += 1;
                    tls_owner_clear(ptr);
                    (void)core_global_unlock();
                    z067_release_report(MIMIR_DIAG_OP_FREE);
                    tls.remote_frees_out += 1;
                    return;
                }
                (void)core_global_unlock();
            } else {
                /* Z-065 (D-02): the F-07 posture is kept
                 * (the block stays LIVE) and the course
                 * gains its counter. */
                __atomic_fetch_add(
                    &mimir_stats_free_lock_failures, 1ULL,
                    __ATOMIC_RELAXED);
                return; /* F-07: the block stays LIVE */
            }
#else /* MIMIR_LOCK_SHARD */
            {
                size_t fshard = 0;
                struct mimir_span_header *fspan;

                shard_flush_phase();
                fspan = shard_find_span_live(ptr, &fshard);
                if (fshard == (size_t)-2) {
                    /* Z-065 (D-02): counted; block stays
                     * LIVE (F-07). */
                    __atomic_fetch_add(
                        &mimir_stats_free_lock_failures, 1ULL,
                        __ATOMIC_RELAXED);
                    return; /* F-07: the block stays LIVE */
                }
                if (fspan != NULL) {
                    if (tls_owner_is_handed(ptr)) {
                        small_free_tally_locked(ptr);
                    } else {
                        small_free_locked(ptr);
                    }
                    /* Z-076 (INC-J002-01): as the
                     * non-shard course above. */
                    mimir_stats_cross_thread_small += 1;
                    tls_owner_clear(ptr);
                    (void)core_shard_unlock(fshard);
                    z067_release_report(MIMIR_DIAG_OP_FREE);
                    tls.remote_frees_out += 1;
                    return;
                }
                /* Not a live span object: fall through to
                 * the legacy path below (a mapping block,
                 * or a free with no defined behaviour). */
            }
#endif /* MIMIR_LOCK_SHARD */
        }
    }
#endif
    /* L4: take the one global lock via L6. On lock failure the
     * block is NOT freed unlocked; it stays LIVE (F-07).
     * Z-065 (D-02): that safe posture is unchanged, and the
     * course now gains the free lock-failure counter. */
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        __atomic_fetch_add(&mimir_stats_free_lock_failures,
                           1ULL, __ATOMIC_RELAXED);
        return;
    }

    tls_flush_locked(); /* Z-029 */
    /*
     * Owner determination (B-006 S11.2 step 4): find the LIVE
     * descriptor whose user pointer is exactly ptr, walking
     * only allocator-owned registry links -- validation never
     * dereferences memory derived from the unvalidated ptr,
     * so a foreign pointer's bytes are never read here. The
     * found descriptor is then validated on its own fields:
     * magic, state == LIVE, mapping_base + usable_offset ==
     * ptr, and mapping_base == the descriptor's own address
     * (S7.1: the descriptor sits at its mapping base). This is
     * the S8.2 step-2 validation plus the registry-membership
     * check of S11.2 step 4, in an order that reads only
     * trusted memory.
     */
    desc = find_candidate_locked(ptr);

    if (desc == NULL || !descriptor_is_live(desc)) {
        /* Not a defined free (B-006 S8.3): foreign, interior
         * or already-freed pointer -- undefined behaviour, no
         * defined error or recovery. This baseline takes the
         * only non-destructive course: nothing is unmapped,
         * unlinked or counted on suspicion (S8.2 step 4,
         * F-09), and the call simply returns. No behaviour is
         * promised for such calls. */
        /* G-003 two-registry free path (G-002 R-4): not a
         * mapping block -- consult the span registry
         * before taking the non-destructive course. A
         * live span object is retired exactly once onto
         * its span's free list (small_free_locked); any
         * other pointer keeps the S8.3 posture below. */
#ifndef MIMIR_LOCK_SHARD
        if (find_span_for_free_locked(ptr) != NULL) {
            small_free_locked(ptr);
            /* Z-076 (INC-J002-01 residual): no owner
             * record classified this free, so its thread
             * relation is unknown -- counted as unrecorded,
             * never as cross-thread. */
            mimir_stats_small_unrecorded += 1;
#ifndef MIMIR_DEBUG
            /* Z-029 fallback accounting: a small block
             * with no owner record (table pressure; slice
             * doc S7) freed through the central core counts
             * as routed to the core, never as a bin free. */
            tls.remote_frees_out += 1;
#endif
        }
        (void)core_global_unlock();
        /* Z-066: a central free refused on a length-corrupt
         * span reports here (tallied once, diag emitted). */
        z066_free_report(1);
        z067_release_report(MIMIR_DIAG_OP_FREE);
        z070_df_report(); /* Z-070 */
        return;
#else /* MIMIR_LOCK_SHARD */
        {
            size_t sh;
            int found = 0;

            for (sh = 0; sh < MIMIR_SHARD_COUNT && !found;
                 sh++) {
                mimir_shard_hint = sh;
                if (find_span_for_free_locked(ptr) != NULL) {
                    small_free_locked(ptr);
                    /* Z-076: unrecorded residual, as the
                     * non-shard course above. */
                    mimir_stats_small_unrecorded += 1;
                    tls.remote_frees_out += 1;
                    found = 1;
                }
            }
        }
        (void)core_global_unlock();
        /* Z-066: as the non-shard branch above. */
        z066_free_report(1);
        z067_release_report(MIMIR_DIAG_OP_FREE);
        z070_df_report(); /* Z-070 */
        return;
#endif /* MIMIR_LOCK_SHARD */
    }

    /* Retire exactly once (B-006 S11.2 steps 5-7) via the
     * shared retire_locked (C-005): it captures the exact
     * acquired pair in locals before release (S8.2 step 3 --
     * after release the descriptor is dead and never read),
     * marks RETIRED, unlinks, decrements counters, then
     * releases the WHOLE mapping via L6, lock held across
     * the call (S9.4 DECISION). If the release fails, F-06
     * governs: retire_locked rolls the retirement back in
     * full so the block remains recorded as owned and the
     * bookkeeping still describes actual memory
     * (Invariant 2), and this call simply returns with the
     * block still LIVE. */
    {   /* J-002 cross_thread_frees, mapping half (the
         * small half is counted at the Z-076 courses above;
         * INC-J002-01) -- the reserve page
         * past the descriptor recorded the allocating
         * thread at publication; the realloc transfer retire does NOT
         * pass through here and is never counted as a free. */
        thrd_t j002_owner;
        thrd_t j002_self = thrd_current();
        memcpy(&j002_owner,
               (const unsigned char *)desc +
                   MIMIR_OWNER_THREAD_OFFSET,
               sizeof j002_owner);
        if (!thrd_equal(j002_owner, j002_self)) {
            mimir_stats_cross_thread_frees += 1;
        }
    }
    if (!retire_locked(desc)) {
        (void)core_global_unlock();
        /* Z-066: a Z-066 audit refusal reports through the
         * diag channel (cause metadata); an ordinary F-06
         * release failure set no label and reports nothing,
         * exactly as before. */
        z066_free_report(0);
        return;
    }

    /* Success: the descriptor ceased to exist with its
     * mapping. No field of it is read or written from here
     * (Invariant 2). ptr and every pointer into the former
     * mapping are dangling; later use is use-after-free UB
     * and a second free is double-free UB (S11.2 step 9). */
    (void)core_global_unlock();
}

/*
 * Z-075 -- usable size (Wave 9; roadmap Z-075, decision
 * D-012 in docs/DECISION_LEDGER.md): the exact usable
 * extent of a live block, by analogy under U-03 to
 * contracts.md MA-COND-USABLE-01/-02, with the V-05
 * per-target semantics DEFINED by D-012 as follows --
 * the answer is read from the allocator's own records,
 * never computed from the request and never guessed:
 *   - a span-registry block (any request the class
 *     machinery served, 1..1024 bytes at publication):
 *     exactly its span's class size. No per-object
 *     requested-size record exists under the G-002 span
 *     layout (INC-G003-03), and the class extent is the
 *     block's whole usable extent -- every byte of it is
 *     the caller's to use, which the slice's tests prove
 *     by filling and re-reading the full class extent.
 *   - a mapping-registry block (requests above the
 *     cutoff, and every aligned block above the family
 *     floor): exactly the descriptor's usable_size,
 *     which publication wrote as mapping_length -
 *     usable_offset and descriptor_is_live revalidates
 *     on this very lookup. A realloc shrink lowers only
 *     the requested-size record, so the answer after a
 *     shrink is still the mapping's full usable extent
 *     (those bytes remain the caller's; the answer is
 *     the extent, not the last request).
 *   - NULL: 0 (MA-COND-USABLE-02's null case, defined
 *     for this target by D-012; glibc agrees).
 *   - a pointer the registries can determine is not a
 *     live block -- foreign, interior, or already freed
 *     (on a span's free list, retired or pooled in the
 *     mapping registry, or resident in the CALLING
 *     thread's TLS bin): 0, by the non-destructive
 *     course of free's S8.3 posture, with the defined
 *     return a query can give. The lookup never
 *     dereferences the argument: ownership is decided
 *     by walking allocator-owned registry links and
 *     comparing pointer values, exactly as in free
 *     (the span walk's free-list reads are of
 *     allocator-owned span pages, under the J-006
 *     bounds discipline).
 *   - lock failure at entry: 0 (the F-07 posture
 *     carried to a query: nothing can be proven, so
 *     nothing is claimed). This branch cannot be
 *     triggered through the public API on a healthy
 *     host -- no fault-injection hook is wired into
 *     this suite -- and rests on code inspection, the
 *     INC-C005-03 posture for such branches.
 * The one residence no other thread can distinguish
 * from live is a block sitting in ANOTHER thread's TLS
 * bin: the cache holds no cross-thread identity index
 * by design (Z-028 contract S9: no tid-indexed shared
 * array), so such a pointer is answered from the span
 * registry as its class extent. Asking after a block
 * another thread has freed is use-after-free -- UB
 * across the whole family (MA-COND-USABLE-03 states no
 * behaviour for a released argument) -- and D-012
 * records this boundary rather than papering over it;
 * the calling thread's own bin IS checked by identity
 * below, so a same-thread free is always answered 0.
 * The call modifies nothing: no counter moves, no
 * record changes; the only state it touches is the
 * calling thread's own pending TLS accounting, which
 * it flushes under the lock first, exactly as every
 * other locked entry point does.
 */
#ifndef MIMIR_DEBUG
/* Identity scan of the calling thread's own TLS bins:
 * a block resident there has been freed by this thread
 * and not yet drained to its span's free list, so the
 * span registry alone would still call it live. The
 * bins are thread-local static storage (zeroed before
 * first use, so the scan is safe on a thread that has
 * never allocated); no lock and no cache
 * initialisation are needed to read them. */
static int z075_own_bin_contains(const void *ptr)
{
    size_t ci;
    size_t i;

    for (ci = 0; ci < MIMIR_TLS_CLASS_COUNT; ci++) {
        for (i = 0; i < tls.bin_len[ci]; i++) {
            if (tls.bins[ci][i] == ptr) {
                return 1;
            }
        }
    }
    return 0;
}
#endif /* !MIMIR_DEBUG */

size_t mimir_core_usable_size(const void *ptr)
{
    struct mimir_descriptor *desc;
    struct mimir_span_header *span;
    size_t result = 0;

    if (ptr == NULL) {
        return 0;
    }
#ifndef MIMIR_DEBUG
    if (z075_own_bin_contains(ptr)) {
        return 0; /* freed into this thread's own bin */
    }
#endif
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return 0; /* F-07 posture: nothing can be proven */
    }
    tls_flush_locked(); /* Z-029: as every locked entry */
    desc = find_candidate_locked((void *)ptr);
    if (desc != NULL && descriptor_is_live(desc)) {
        result = desc->usable_size;
        (void)core_global_unlock();
        return result;
    }
#ifdef MIMIR_LOCK_SHARD
    span = find_span_live_any_locked((void *)ptr);
#else
    span = find_span_live_locked((void *)ptr);
#endif
    if (span != NULL) {
        result = span->class_size;
    }
    (void)core_global_unlock();
    return result;
}
