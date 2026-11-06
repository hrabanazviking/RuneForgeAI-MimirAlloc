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

void *mimir_core_alloc(size_t size)
{
    size_t page_size;
    size_t total_unrounded;
    size_t usable_offset;
    size_t usable_size;
    void *base = NULL;
    size_t mapping_length = 0;
    struct mimir_descriptor *desc;
    int status;

    /* L2 admission (B-007 A1): zero-size is not admitted as a
     * supported call. Posture only: NULL, U-09 unresolved. */
    if (size == 0) {
        return NULL;
    }

    /* L4: take the one global lock via L6 (B-006 S9.1). On
     * lock failure fail with no ownership passing (F-07). */
    if (mimir_platform_global_lock() != MIMIR_PLATFORM_OK) {
        return NULL;
    }

    /* L5 layout, via L6 discovery (B-006 S11.1 step 5).
     * Page-size discovery failure is a defined failure with
     * no mapping attempted and no fallback constant (F-11). */
    status = mimir_platform_page_size(&page_size);
    if (status != MIMIR_PLATFORM_OK || page_size == 0) {
        (void)mimir_platform_global_unlock();
        return NULL;
    }
    usable_offset = page_size; /* exactly one system page, S7.1 */

    /* Checked arithmetic, check before evaluate (B-007 A3/A4):
     * total = usable_offset + requested_size, tested first. */
    if (size > SIZE_MAX - usable_offset) {
        (void)mimir_platform_global_unlock();
        return NULL;
    }
    total_unrounded = usable_offset + size;

    /* Bookkeeping check before any mapping (B-007 A10 form):
     * the eventual live_bytes addition must be representable.
     * mapping_length >= total_unrounded, so testing against
     * total_unrounded is necessary but not sufficient; the
     * exact test runs after acquire, before publication. */

    /* L6 acquire, lock held across the call (S9.4 DECISION).
     * L6 applies the checked page round-up (B-007 A5/A6) and
     * is all-or-nothing: on failure nothing is mapped (F-01,
     * F-03, F-04), no descriptor exists, registry unchanged. */
    status = mimir_platform_map_acquire(total_unrounded,
                                        &base, &mapping_length);
    if (status != MIMIR_PLATFORM_OK) {
        (void)mimir_platform_global_unlock();
        return NULL;
    }

    /* B-007 A8: subtraction checked before evaluate, and the
     * publication gate usable_size >= requested_size. */
    if (usable_offset > mapping_length) {
        (void)mimir_platform_map_release(base, mapping_length);
        (void)mimir_platform_global_unlock();
        return NULL;
    }
    usable_size = mapping_length - usable_offset;
    if (usable_size < size) {
        (void)mimir_platform_map_release(base, mapping_length);
        (void)mimir_platform_global_unlock();
        return NULL;
    }

    /* B-007 A10: exact counter check, before publication. On
     * failure the mapping is released and nothing is
     * published (A.3: registry and counters unchanged). */
    if (mapping_length > SIZE_MAX - mimir_live_bytes) {
        (void)mimir_platform_map_release(base, mapping_length);
        (void)mimir_platform_global_unlock();
        return NULL;
    }

    /* Publication (B-006 S11.1 step 8; B-007 C1.4): write the
     * descriptor in full, then link it and update counters in
     * this same locked section. B-007 A9: base + usable_offset
     * cannot wrap -- usable_offset < mapping_length and the
     * whole extent is one live mapping. */
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
    registry_insert(desc);
    mimir_live_mappings += 1;
    mimir_live_bytes += mapping_length;

    /* The lock is never held across the return (S9.4 item 1).
     * If unlock itself fails there is no safe report channel
     * under the design (no reporting under lock, S9.4 item 3)
     * and the allocation is already soundly published; the
     * pointer is returned and the failure is not hidden by
     * returning overlapping or unpublished memory (S9.5). */
    (void)mimir_platform_global_unlock();

    /* User pointer: mapping_base + usable_offset -- page
     * aligned by construction, hence suitably aligned by
     * analogy to MA-6.1-MALLOC-01 (B-006 S7.1). The usable
     * bytes are now the caller's alone; this layer never
     * touches them (B-006 S9.2, B-007 C1.2). */
    return (void *)((unsigned char *)base + usable_offset);
}

void mimir_core_free(void *ptr)
{
    struct mimir_descriptor *desc;
    void *mapping_base;
    size_t mapping_length;
    int status;

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
    desc = mimir_registry_head;
    while (desc != NULL) {
        if ((void *)((unsigned char *)desc->mapping_base +
                     desc->usable_offset) == ptr) {
            break;
        }
        desc = desc->list_next;
    }

    if (desc == NULL || desc->magic != MIMIR_DESCRIPTOR_MAGIC ||
        desc->state != MIMIR_STATE_LIVE ||
        desc->mapping_base != (void *)desc) {
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

    /* Capture the exact acquired pair in locals (S8.2 step 3):
     * after release the descriptor is dead and never read. */
    mapping_base = desc->mapping_base;
    mapping_length = desc->mapping_length;

    /* Retire exactly once (B-006 S11.2 steps 5-7): mark
     * RETIRED, unlink, decrement counters -- then release the
     * WHOLE mapping via L6, lock held across the call (S9.4
     * DECISION). If the release fails, F-06 governs: the
     * retirement is rolled back in full (descriptor restored
     * to LIVE and re-linked, counters restored) so the block
     * remains recorded as owned and the bookkeeping still
     * describes actual memory (Invariant 2). */
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
