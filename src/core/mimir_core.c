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
 * Zero-size (U-09 UNRESOLVED): B-006 F-05 and B-007 A1/A4 give
 * mimir_malloc(0) NO behaviour, and B-008 prescribes none for
 * it either. This implementation returns NULL for size == 0
 * as an implementation posture only, continuous with the
 * C-001 stub's posture. It does NOT settle U-09 and no caller
 * or test may cite it as doing so.
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
 * postures only (U-09 / U-12 unresolved). The slice record
 * is tests/core/NOTES_C005.md.
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
#include <threads.h>
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
static size_t mimir_live_bytes;                      /* 0 at start  */

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
static uint64_t mimir_stats_bytes_requested;
static uint64_t mimir_stats_bytes_allocated;
static uint64_t mimir_stats_bytes_released;
static uint64_t mimir_stats_peak_mapped;
static uint64_t mimir_stats_alloc_failures;
static uint64_t mimir_stats_alloc_posture_nulls;
static uint64_t mimir_stats_cross_thread_frees;

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
    "malloc", "calloc", "realloc"
};
static const char *const mimir_diag_cause_names[] = {
    "none", "backing", "arithmetic", "lock", "page-size",
    "internal"
};
static int mimir_diag_last_cause;
static int mimir_diag_guard; /* 0 idle, 1 reporting */
static uint64_t mimir_diag_emitted_count;
static uint64_t mimir_diag_suppressed_count;
static uint64_t mimir_diag_max_depth_seen;

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
     * wired and proven, with emission explicitly enabled
     * until the banking reconciliation rules on flipping
     * the default (docs/PHASE_10_DIAGNOSTICS.md S5). The
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
static int retire_locked(struct mimir_descriptor *desc)
{
    void *mapping_base = desc->mapping_base;
    size_t mapping_length = desc->mapping_length;
    int status;

    desc->state = MIMIR_STATE_RETIRED;
    registry_remove(desc);
    mimir_live_mappings -= 1;
    mimir_live_bytes -= mapping_length;

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

    /* L6 acquire, lock held across the call (S9.4 DECISION).
     * L6 applies the checked page round-up (B-007 A5/A6) and
     * is all-or-nothing: on failure nothing is mapped (F-01,
     * F-03, F-04), no descriptor exists, registry unchanged. */
    status = mimir_platform_map_acquire(total_unrounded,
                                        &base, &mapping_length);
    if (status != MIMIR_PLATFORM_OK) {
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_BACKING);
    }

    /* B-007 A8: subtraction checked before evaluate, and the
     * publication gate usable_size >= requested_size. */
    if (usable_offset > mapping_length) {
        (void)mimir_platform_map_release(base, mapping_length);
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_INTERNAL);
    }
    usable_size = mapping_length - usable_offset;
    if (usable_size < size) {
        (void)mimir_platform_map_release(base, mapping_length);
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
        (void)mimir_platform_map_release(base, mapping_length);
        return MIMIR_DIAG_FAIL(MIMIR_DIAG_CAUSE_INTERNAL);
    }

    /* B-007 A10: exact counter checks, before publication. On
     * failure the mapping is released and nothing is
     * published (A.3: registry and counters unchanged).
     * (C-004: the live_mappings increment is now checked too;
     * C-003 checked only the live_bytes addition.) */
    if (mapping_length > SIZE_MAX - mimir_live_bytes ||
        mimir_live_mappings == SIZE_MAX) {
        (void)mimir_platform_map_release(base, mapping_length);
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

/* Span registry anchors and counters: fixed static storage
 * below L1, exactly as the mapping registry's are (B-006
 * S7.2 / S8.1). A span header is on this list if and only
 * if its state is LIVE. mimir_class_active is a per-class
 * hint for the span to try first; correctness never rests
 * on it -- the registry walk is authoritative. */
static struct mimir_span_header *mimir_span_head; /* NULL: empty */
static struct mimir_span_header *mimir_class_active[MIMIR_SMALL_CLASS_COUNT];
static size_t mimir_live_spans;
static size_t mimir_live_span_bytes;
static size_t mimir_live_small_objects;

unsigned long mimir_audit_span_published;
unsigned long mimir_audit_span_retired;
unsigned long mimir_audit_small_alloc;
unsigned long mimir_audit_small_free;

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
    if (span->mapping_length <
            MIMIR_SPAN_HEADER_SIZE + span->class_size ||
        span->usable_size !=
            span->mapping_length - MIMIR_SPAN_HEADER_SIZE ||
        span->live_count > span->carve_count ||
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
    uint64_t mapped =
        (uint64_t)(mimir_live_bytes + mimir_live_span_bytes);

    if (mapped > mimir_stats_peak_mapped) {
        mimir_stats_peak_mapped = mapped;
    }
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
static struct mimir_span_header *span_acquire_locked(size_t class_index)
{
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
    span->usable_offset = MIMIR_SPAN_HEADER_SIZE;
    span->class_size = mimir_class_sizes[class_index];
    span->usable_size = mapping_length - MIMIR_SPAN_HEADER_SIZE;
    span->state = MIMIR_STATE_LIVE;
    span->class_index = (uint32_t)class_index;
    span->free_head = NULL;
    span->live_count = 0;
    span->carve_count = 0;

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
        return 0;
    }
    __atomic_fetch_add(&mimir_audit_span_retired, 1UL,
                       __ATOMIC_RELAXED);
    mimir_stats_bytes_released += mapping_length; /* J-002 Q-RELEASED */
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

/* Count retained EMPTY spans of one class (lock held). */
static size_t empty_spans_of_class_locked(size_t class_index)
{
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
            ) {
            count++;
        }
        span = span->list_next;
    }
    return count;
}

static int span_can_serve(const struct mimir_span_header *span)
{
    /* J-006: a corrupt free_head (wild, misaligned, or
     * pointing outside the carved area) does not make the
     * span servable and is never followed; the span may
     * still serve by carving, which hands out only memory
     * the header's own validated fields describe. */
    return (span->free_head != NULL &&
            span_free_node_valid(span, span->free_head)) ||
           span->carve_count < span_capacity(span);
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
    (void)mimir_platform_global_unlock();
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
    (void)mimir_platform_global_unlock();
    return bytes;
}

int mimir_debug_quarantine_verify(void)
{
    size_t bad;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return -1;
    }
    bad = debug_quarantine_verify_locked();
    (void)mimir_platform_global_unlock();
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
static struct mimir_span_header *find_span_live_locked(void *ptr)
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

        if (span_header_is_live(span) && target >= first &&
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
    size_t class_size = mimir_class_sizes[class_index];
    struct mimir_span_header *span = mimir_class_active[class_index];
    void *obj;

    if (span == NULL || !span_header_is_live(span) ||
        !span_can_serve(span)) {
        span = mimir_span_head;
        while (span != NULL) {
            if (span->class_index == class_index &&
                span_header_is_live(span) && span_can_serve(span)) {
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

    if (span->free_head != NULL &&
        span_free_node_valid(span, span->free_head)) {
        obj = span->free_head;
        span->free_head = *(void **)obj;
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
    struct mimir_span_header *span = find_span_live_locked(ptr);

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
        empty_spans_of_class_locked(span->class_index) >
            mimir_retention_bound) {
        (void)span_release_locked(span);
    }
#else
    *(void **)ptr = span->free_head;
    span->free_head = ptr;
    span->live_count -= 1;
    mimir_live_small_objects -= 1;
    __atomic_fetch_add(&mimir_audit_small_free, 1UL,
                       __ATOMIC_RELAXED);

    if (span->live_count == 0 &&
        empty_spans_of_class_locked(span->class_index) >
            mimir_retention_bound) {
        (void)span_release_locked(span);
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
    (void)mimir_platform_global_unlock();
    return bad;
}
#endif /* MIMIR_DEBUG */

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
    mimir_retention_bound = value;
    (void)mimir_platform_global_unlock();
    return 0;
}

size_t mimir_tune_get_retention_bound(void)
{
    size_t value;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return (size_t)-1;
    }
    value = mimir_retention_bound;
    (void)mimir_platform_global_unlock();
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
            struct mimir_span_header *span = mimir_span_head;
            struct mimir_span_header *victim = NULL;

            while (span != NULL) {
                if (span->class_index == class_index &&
                    span->live_count == 0
#ifdef MIMIR_DEBUG
                    /* J-005: never a quarantine holder. */
                    && debug_span_quarantine_count_locked(span) == 0
#endif
                    ) {
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
    struct mimir_span_header *span = mimir_span_head;
    size_t spans = 0;
    size_t bytes = 0;

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
 * before dlclose. Large mappings are not drained: the
 * mapping path retains nothing after free (the Phase 3
 * baseline), so there is nothing retained for it here.
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
        return 0;
    }
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
    (void)mimir_platform_global_unlock();
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
        return 0;
    }
    released = trim_some_locked(SIZE_MAX);
    (void)mimir_platform_global_unlock();
    return released;
}

size_t mimir_core_trim_retained_some(size_t max_spans)
{
    size_t released;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return 0;
    }
    released = trim_some_locked(max_spans);
    (void)mimir_platform_global_unlock();
    return released;
}

size_t mimir_core_retained_spans(void)
{
    size_t spans;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return (size_t)-1;
    }
    retained_snapshot_locked(&spans, NULL);
    (void)mimir_platform_global_unlock();
    return spans;
}

size_t mimir_core_retained_bytes(void)
{
    size_t bytes;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return (size_t)-1;
    }
    retained_snapshot_locked(NULL, &bytes);
    (void)mimir_platform_global_unlock();
    return bytes;
}

size_t mimir_core_mapped_bytes(void)
{
    size_t mapped;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return (size_t)-1;
    }
    mapped = mimir_live_bytes + mimir_live_span_bytes;
    (void)mimir_platform_global_unlock();
    return mapped;
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
    memset(out, 0, sizeof *out);
    out->bytes_requested = mimir_stats_bytes_requested;
    out->bytes_allocated = mimir_stats_bytes_allocated;
    out->bytes_released = mimir_stats_bytes_released;
    out->peak_mapped = mimir_stats_peak_mapped;
    out->alloc_failures = __atomic_load_n(
        &mimir_stats_alloc_failures, __ATOMIC_RELAXED);
    out->alloc_posture_nulls = __atomic_load_n(
        &mimir_stats_alloc_posture_nulls, __ATOMIC_RELAXED);
    out->cross_thread_frees = mimir_stats_cross_thread_frees;
    out->lock_contention = 0;          /* ABSENT: INC-J002-02 */
    out->lock_contention_measured = 0; /* ABSENT: INC-J002-02 */
    out->live_mappings = (uint64_t)mimir_live_mappings;
    out->live_spans = (uint64_t)mimir_live_spans;
    out->live_small_objects = (uint64_t)mimir_live_small_objects;
    out->live_mapping_bytes = (uint64_t)mimir_live_bytes;
    out->live_span_bytes = (uint64_t)mimir_live_span_bytes;
    out->bytes_mapped = (uint64_t)(mimir_live_bytes +
                                   mimir_live_span_bytes);
    out->page_size_bytes = (uint64_t)page_size;
    for (i = 0; i < MIMIR_SMALL_CLASS_COUNT; i++) {
        out->span_class_size[i] = (uint64_t)mimir_class_sizes[i];
    }
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
    (void)mimir_platform_global_unlock();
    return 0;
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
    span = mimir_span_head;
    count = 0;
    bytes = 0;
    while (span != NULL) {
        count++;
        if (count > mimir_live_spans) {
            violations++; /* cycling registry */
            break;
        }
        if (span_header_is_live(span)) {
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
                nodes + span->live_count != span->carve_count) {
                violations++;
            }
        } else {
            violations++;
        }
        if (span->list_prev != sprev) {
            violations++;
        }
        sprev = span;
        span = span->list_next;
    }
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
        struct mimir_span_header *want = mimir_class_active[i];
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
    }
    (void)mimir_platform_global_unlock();
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
 * call (posture only: NULL, U-09 unresolved). On lock
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
    result = alloc_any_locked(size, 0, NULL, NULL);
    if (result == NULL) {
        mimir_stats_note_failure(); /* J-002: Q-FAILED-OS */
    }
    (void)mimir_platform_global_unlock();
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
 *     zero factor is [UNRESOLVED: U-09] (MA-6.1-CALLOC-04)
 *     and is NOT routed through admission as a product of
 *     zero. This implementation returns NULL as a posture
 *     only, continuous with mimir_core_alloc(0); it does
 *     not settle U-09 and no caller or test may cite it as
 *     doing so (tests/core/NOTES_C005.md, INC-C005-01).
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

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        mimir_stats_note_failure(); /* J-002: Q-FAILED-OS */
        mimir_core_diag_report(MIMIR_DIAG_OP_CALLOC,
                               MIMIR_DIAG_CAUSE_LOCK,
                               (uint64_t)product);
        return NULL;
    }
    mimir_diag_last_cause = MIMIR_DIAG_CAUSE_INTERNAL;
    result = alloc_any_locked(product, 1, NULL, NULL);
    if (result == NULL) {
        mimir_stats_note_failure(); /* J-002: Q-FAILED-OS */
    }
    (void)mimir_platform_global_unlock();
    if (result == NULL) {
        mimir_core_diag_report(MIMIR_DIAG_OP_CALLOC,
                               mimir_diag_last_cause,
                               (uint64_t)product);
    }
    return result;
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
 *   - new_size == 0 with a non-null ptr: [UNRESOLVED:
 *     U-12] (MA-6.1-REALLOC-04). Like the calloc
 *     zero-factor gate it is decided before any allocator
 *     state is touched: zero is not admitted at L2 (B-007
 *     A1), so this implementation returns NULL and leaves
 *     the original block LIVE, unchanged and caller-owned,
 *     exactly as an admission failure would. Posture only
 *     -- it does not settle U-12, it is outside the B-008
 *     S7.2 supported list, and no caller or test may cite
 *     it as passing hook C-006 (tests/core/NOTES_C005.md,
 *     INC-C005-01).
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
 *   - grow: always a move (B-008 S4.4: no in-place grow
 *     exists in this machine). The B-008 S4.5 sequence runs
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
            (void)mimir_platform_global_unlock();
            return ptr;
        }
        if (new_size < old_size) {
            desc->requested_size = new_size;
            (void)mimir_platform_global_unlock();
            return ptr;
        }

        /* Grow: construct the new block completely first
         * (S4.5 steps 2-3), copy (step 4), then transfer and
         * retire the old block exactly once (step 5). */
        new_ptr = alloc_any_locked(new_size, 0, &new_desc,
                                   &new_span);
        if (new_ptr == NULL) {
            int cause = mimir_diag_last_cause; /* J-007 */
            mimir_stats_note_failure(); /* J-002 */
            (void)mimir_platform_global_unlock();
            mimir_core_diag_report(MIMIR_DIAG_OP_REALLOC,
                                   cause, (uint64_t)new_size);
            return NULL;
        }
        memcpy(new_ptr, ptr, old_size);

        if (!retire_locked(desc)) {
            mimir_stats_note_failure(); /* J-002: release failure */
            retire_any_locked(new_ptr, new_desc);
            (void)mimir_platform_global_unlock();
            /* J-007: the transfer step's L6 release failed:
             * cause class backing (an OS release refusal). */
            mimir_core_diag_report(MIMIR_DIAG_OP_REALLOC,
                                   MIMIR_DIAG_CAUSE_BACKING,
                                   (uint64_t)new_size);
            return NULL;
        }
        (void)mimir_platform_global_unlock();
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
        struct mimir_span_header *span = find_span_live_locked(ptr);
        struct mimir_span_header *new_span = NULL;
        size_t span_class;

        if (span == NULL) {
            /* Not a defined realloc (B-008 S4, invalid ptr):
             * nothing allocated, unmapped, unlinked or
             * counted on suspicion; NULL, no promise. */
            (void)mimir_platform_global_unlock();
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
            (void)mimir_platform_global_unlock();
            return ptr;
        }

        new_ptr = alloc_any_locked(new_size, 0, &new_desc,
                                   &new_span);
        if (new_ptr == NULL) {
            int cause = mimir_diag_last_cause; /* J-007 */
            mimir_stats_note_failure(); /* J-002 */
            (void)mimir_platform_global_unlock();
            mimir_core_diag_report(MIMIR_DIAG_OP_REALLOC,
                                   cause, (uint64_t)new_size);
            return NULL;
        }
        memcpy(new_ptr, ptr, span_class);
        small_free_locked(ptr);
        (void)mimir_platform_global_unlock();
        return new_ptr;
    }
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

    /* L4: take the one global lock via L6. On lock failure the
     * block is NOT freed unlocked; it stays LIVE (F-07). */
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return;
    }

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
        if (find_span_live_locked(ptr) != NULL) {
            small_free_locked(ptr);
        }
        (void)mimir_platform_global_unlock();
        return;
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
    {   /* J-002 cross_thread_frees (PARTIAL, INC-J002-01):
         * mapping-path blocks only -- the reserve page
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
        (void)mimir_platform_global_unlock();
        return;
    }

    /* Success: the descriptor ceased to exist with its
     * mapping. No field of it is read or written from here
     * (Invariant 2). ptr and every pointer into the former
     * mapping are dangling; later use is use-after-free UB
     * and a second free is double-free UB (S11.2 step 9). */
    (void)mimir_platform_global_unlock();
}
