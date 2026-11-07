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
 */

#include "mimir_core.h"

#include <stdint.h>
#include <string.h>

#include "../platform/mimir_platform.h"

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

    while (desc != NULL) {
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
    return desc->magic == MIMIR_DESCRIPTOR_MAGIC &&
           desc->state == MIMIR_STATE_LIVE &&
           desc->mapping_base == (void *)desc;
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
        return NULL;
    }
    usable_offset = page_size; /* exactly one system page, S7.1 */

    /* Checked arithmetic, check before evaluate (B-007 A3/A4):
     * total = usable_offset + requested_size, tested first. */
    if (size > SIZE_MAX - usable_offset) {
        return NULL;
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
        return NULL;
    }

    /* B-007 A8: subtraction checked before evaluate, and the
     * publication gate usable_size >= requested_size. */
    if (usable_offset > mapping_length) {
        (void)mimir_platform_map_release(base, mapping_length);
        return NULL;
    }
    usable_size = mapping_length - usable_offset;
    if (usable_size < size) {
        (void)mimir_platform_map_release(base, mapping_length);
        return NULL;
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
        return NULL;
    }

    /* B-007 A10: exact counter checks, before publication. On
     * failure the mapping is released and nothing is
     * published (A.3: registry and counters unchanged).
     * (C-004: the live_mappings increment is now checked too;
     * C-003 checked only the live_bytes addition.) */
    if (mapping_length > SIZE_MAX - mimir_live_bytes ||
        mimir_live_mappings == SIZE_MAX) {
        (void)mimir_platform_map_release(base, mapping_length);
        return NULL;
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
    return (span->mapping_length - MIMIR_SPAN_HEADER_SIZE) /
           span->class_size;
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
    return span->magic == MIMIR_SPAN_MAGIC &&
           span->state == MIMIR_STATE_LIVE &&
           span->mapping_base == (const void *)span &&
           span->usable_offset == MIMIR_SPAN_HEADER_SIZE &&
           span->class_index < MIMIR_SMALL_CLASS_COUNT &&
           span->class_size == mimir_class_sizes[span->class_index];
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
        return NULL;
    }
    /* A span must hold its header plus at least one object
     * of its class; on every admitted host (page >= 4096,
     * class <= 1024) this holds by the G-002 S6 arithmetic,
     * and the check keeps the invariant explicit rather
     * than assumed. */
    if (page_size <= MIMIR_SPAN_HEADER_SIZE ||
        page_size - MIMIR_SPAN_HEADER_SIZE <
            mimir_class_sizes[class_index]) {
        return NULL;
    }
    status = mimir_platform_map_acquire(page_size, &base,
                                        &mapping_length);
    if (status != MIMIR_PLATFORM_OK) {
        return NULL;
    }
    if (mapping_length != page_size) {
        (void)mimir_platform_map_release(base, mapping_length);
        return NULL;
    }
    if (mapping_length > SIZE_MAX - mimir_live_span_bytes ||
        mimir_live_spans == SIZE_MAX) {
        (void)mimir_platform_map_release(base, mapping_length);
        return NULL;
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
    return 1;
}

/* Count retained EMPTY spans of one class (lock held). */
static size_t empty_spans_of_class_locked(size_t class_index)
{
    struct mimir_span_header *span = mimir_span_head;
    size_t count = 0;

    while (span != NULL) {
        if (span->class_index == class_index &&
            span->live_count == 0) {
            count++;
        }
        span = span->list_next;
    }
    return count;
}

static int span_can_serve(const struct mimir_span_header *span)
{
    return span->free_head != NULL ||
           span->carve_count < span_capacity(span);
}

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

    while (span != NULL) {
        uintptr_t base = (uintptr_t)span->mapping_base;
        uintptr_t first = base + MIMIR_SPAN_HEADER_SIZE;

        if (span_header_is_live(span) && target >= first &&
            target < base + span->mapping_length) {
            size_t offset = (size_t)(target - first);
            size_t index;
            void *node;

            if (offset % span->class_size != 0) {
                return NULL; /* interior: not an object start */
            }
            index = offset / span->class_size;
            if (index >= span->carve_count) {
                return NULL; /* uncarved tail area */
            }
            node = span->free_head;
            while (node != NULL) {
                if (node == ptr) {
                    return NULL; /* already freed */
                }
                node = *(void **)node;
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

    if (span->free_head != NULL) {
        obj = span->free_head;
        span->free_head = *(void **)obj;
    } else {
        obj = (void *)((unsigned char *)span->mapping_base +
                       MIMIR_SPAN_HEADER_SIZE +
                       span->carve_count * class_size);
        span->carve_count += 1;
    }

    if (zero_user) {
        memset(obj, 0, class_size);
    }

    span->live_count += 1;
    mimir_live_small_objects += 1;
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
}

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
    void *result;

    if (size == 0) {
        return NULL;
    }
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return NULL;
    }
    result = alloc_any_locked(size, 0, NULL, NULL);
    (void)mimir_platform_global_unlock();
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
    size_t product;
    void *result;

    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    if (nmemb > SIZE_MAX / size) {
        return NULL;
    }
    product = nmemb * size;

    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return NULL;
    }
    result = alloc_any_locked(product, 1, NULL, NULL);
    (void)mimir_platform_global_unlock();
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
    struct mimir_descriptor *desc;
    struct mimir_descriptor *new_desc = NULL;
    void *new_ptr;
    size_t old_size;

    if (ptr == NULL) {
        return mimir_core_alloc(new_size);
    }
    if (new_size == 0) {
        return NULL;
    }
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return NULL;
    }

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
            (void)mimir_platform_global_unlock();
            return NULL;
        }
        memcpy(new_ptr, ptr, old_size);

        if (!retire_locked(desc)) {
            retire_any_locked(new_ptr, new_desc);
            (void)mimir_platform_global_unlock();
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

        if (new_size <= span_class) {
            (void)mimir_platform_global_unlock();
            return ptr;
        }

        new_ptr = alloc_any_locked(new_size, 0, &new_desc,
                                   &new_span);
        if (new_ptr == NULL) {
            (void)mimir_platform_global_unlock();
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
