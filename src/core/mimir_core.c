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
    result = alloc_locked(size, 0, NULL);
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
    result = alloc_locked(product, 1, NULL);
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
    if (desc == NULL || !descriptor_is_live(desc)) {
        (void)mimir_platform_global_unlock();
        return NULL;
    }
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
    new_ptr = alloc_locked(new_size, 0, &new_desc);
    if (new_ptr == NULL) {
        (void)mimir_platform_global_unlock();
        return NULL;
    }
    memcpy(new_ptr, ptr, old_size);

    if (!retire_locked(desc)) {
        (void)retire_locked(new_desc);
        (void)mimir_platform_global_unlock();
        return NULL;
    }
    (void)mimir_platform_global_unlock();
    return new_ptr;
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
