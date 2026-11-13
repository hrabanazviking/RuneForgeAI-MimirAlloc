/* test_z083.c - Z-083 memalign-family contract test
 * (Wave 10). Static link against the production objects
 * (LIBOBJS), so the E-005 audit counters appended for the
 * three new entry points are readable through
 * tests/abi/mimir_audit_e005.h, and the internal J-002
 * stats snapshot is readable through
 * tests/core/mimir_stats_j002.h.
 *
 * What is proven (the Z-083 contract as written in
 * include/mimir_alloc.h and docs/Z13_Z083_GAP_CLOSURE.md;
 * behaviour by analogy under [UNRESOLVED: U-03], decision
 * D-016):
 *  - mimir_memalign over the Z-074 domain: every non-zero
 *    power-of-two alignment succeeds at assorted sizes,
 *    the pointer satisfies the alignment exactly, the
 *    requested extent is writable/readable, the usable
 *    extent covers the request, and mimir_free retires
 *    the block. Invalid alignments (0 and non-powers of
 *    two, SIZE_MAX included) are exactly NULL, counted
 *    as genuine failures, never as posture nulls.
 *  - Zero size at all three entries is the D-013
 *    contract null: exactly NULL, counted as a posture
 *    null, never as a failure -- pvalloc(0) included (it
 *    is NOT rounded into a one-page allocation).
 *  - mimir_valloc: page-aligned pointer at unrounded
 *    sizes, usable extent covers the request.
 *  - mimir_pvalloc: page-aligned pointer, usable extent
 *    covers the size rounded UP to a page multiple; a
 *    size whose round-up overflows is exactly NULL,
 *    counted as a failure, with no wrapped allocation.
 *  - Oversize requests are exactly NULL (failures) and
 *    the allocator stays fully usable afterwards.
 *  - errno (sentinel 77) is left exactly as on entry by
 *    every checked refusal above (D-013 / U-10).
 *  - Audit counters: each new L1 counter moves exactly
 *    once per call of its entry point, failures and
 *    postures included (every public call in this file
 *    goes through a counting wrapper and the totals are
 *    reconciled at the end).
 *
 * Standalone on purpose: built and run via
 * `make test-z083`; NOT part of `make test`, so the
 * standing gate count is unchanged. The dynamic and C++
 * consumers live in test_z083_dyn.c / test_z083_cpp.cpp;
 * the ABI recorded run is tests/abi/run_z083.sh, filed as
 * tests/abi/recorded_run_z13_z083.txt.
 */
#include "mimir_alloc.h"
#include "mimir_audit_e005.h"
#include "mimir_stats_j002.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks_run++;                                                   \
        if (!(cond)) {                                                  \
            checks_failed++;                                            \
            printf("  [FAIL] %s (line %d)\n", #cond, __LINE__);         \
        }                                                               \
    } while (0)

static unsigned long load(unsigned long *counter)
{
    return __atomic_load_n(counter, __ATOMIC_RELAXED);
}

static unsigned long my_memalign_calls = 0;
static unsigned long my_valloc_calls = 0;
static unsigned long my_pvalloc_calls = 0;

static void *call_memalign(size_t alignment, size_t size)
{
    my_memalign_calls++;
    return mimir_memalign(alignment, size);
}
static void *call_valloc(size_t size)
{
    my_valloc_calls++;
    return mimir_valloc(size);
}
static void *call_pvalloc(size_t size)
{
    my_pvalloc_calls++;
    return mimir_pvalloc(size);
}

struct ledger { uint64_t failures; uint64_t postures; };
static struct ledger ledger_now(void)
{
    struct mimir_stats st;
    struct ledger out;
    if (mimir_core_stats_snapshot(&st) != 0) {
        out.failures = UINT64_MAX;
        out.postures = UINT64_MAX;
        return out;
    }
    out.failures = st.alloc_failures;
    out.postures = st.alloc_posture_nulls;
    return out;
}

static void fill_check(void *p, size_t n, unsigned seed)
{
    unsigned char *b = (unsigned char *)p;
    size_t i;
    int ok = 1;
    for (i = 0; i < n; i++) b[i] = (unsigned char)((i * 31u + seed) & 0xffu);
    for (i = 0; i < n; i++) {
        if (b[i] != (unsigned char)((i * 31u + seed) & 0xffu)) ok = 0;
    }
    CHECK(ok);
}

int main(void)
{
    const size_t page = 4096;
    unsigned long base_memalign = load(&mimir_audit_call_memalign);
    unsigned long base_valloc = load(&mimir_audit_call_valloc);
    unsigned long base_pvalloc = load(&mimir_audit_call_pvalloc);
    struct ledger before, after;
    void *p;
    size_t ai, si;
    static const size_t aligns[] =
        {1, 2, 4, 8, 16, 32, 64, 128, 256, 1024, 4096, 8192, 65536};
    static const size_t sizes[] = {1, 24, 100, 1000};
    static const size_t bad_aligns[] = {0, 3, 24, 48, 100, SIZE_MAX};
    static const size_t vsizes[] = {1, 100, 4095, 4096, 4097, 65536};

    printf("Z-083 memalign-family contract test\n");

    /* A: memalign success matrix over the Z-074 domain. */
    for (ai = 0; ai < sizeof(aligns) / sizeof(aligns[0]); ai++) {
        for (si = 0; si < sizeof(sizes) / sizeof(sizes[0]); si++) {
            p = call_memalign(aligns[ai], sizes[si]);
            CHECK(p != NULL);
            if (p == NULL) continue;
            CHECK(((uintptr_t)p % aligns[ai]) == 0);
            CHECK(mimir_usable_size(p) >= sizes[si]);
            fill_check(p, sizes[si], (unsigned)(aligns[ai] & 0xffu));
            mimir_free(p);
        }
    }

    /* B: invalid alignments are failures, never postures. */
    for (ai = 0; ai < sizeof(bad_aligns) / sizeof(bad_aligns[0]); ai++) {
        before = ledger_now();
        errno = 77;
        p = call_memalign(bad_aligns[ai], 100);
        after = ledger_now();
        CHECK(p == NULL);
        CHECK(after.failures == before.failures + 1);
        CHECK(after.postures == before.postures);
        CHECK(errno == 77);
    }

    /* C: zero size is the D-013 posture null at memalign. */
    before = ledger_now();
    errno = 77;
    p = call_memalign(64, 0);
    after = ledger_now();
    CHECK(p == NULL);
    CHECK(after.postures == before.postures + 1);
    CHECK(after.failures == before.failures);
    CHECK(errno == 77);
    p = call_memalign(16, 0);
    CHECK(p == NULL);

    /* D: oversize memalign fails; allocator stays usable. */
    before = ledger_now();
    p = call_memalign(64, SIZE_MAX);
    after = ledger_now();
    CHECK(p == NULL);
    CHECK(after.failures == before.failures + 1);
    p = call_memalign(64, 100);
    CHECK(p != NULL);
    mimir_free(p);

    /* E: valloc -- page-aligned pointer, unrounded size. */
    for (si = 0; si < sizeof(vsizes) / sizeof(vsizes[0]); si++) {
        p = call_valloc(vsizes[si]);
        CHECK(p != NULL);
        if (p == NULL) continue;
        CHECK(((uintptr_t)p % page) == 0);
        CHECK(mimir_usable_size(p) >= vsizes[si]);
        fill_check(p, vsizes[si], 0x5au);
        mimir_free(p);
    }
    before = ledger_now();
    errno = 77;
    p = call_valloc(0);
    after = ledger_now();
    CHECK(p == NULL);
    CHECK(after.postures == before.postures + 1);
    CHECK(after.failures == before.failures);
    CHECK(errno == 77);
    before = ledger_now();
    p = call_valloc(SIZE_MAX);
    after = ledger_now();
    CHECK(p == NULL);
    CHECK(after.failures == before.failures + 1);

    /* F: pvalloc -- size rounded UP to page multiples. */
    p = call_pvalloc(1);
    CHECK(p != NULL);
    if (p != NULL) {
        CHECK(((uintptr_t)p % page) == 0);
        CHECK(mimir_usable_size(p) >= page);
        fill_check(p, page, 0xa5u);
        mimir_free(p);
    }
    p = call_pvalloc(100);
    CHECK(p != NULL);
    if (p != NULL) {
        CHECK(mimir_usable_size(p) >= page);
        mimir_free(p);
    }
    p = call_pvalloc(4096);
    CHECK(p != NULL);
    if (p != NULL) {
        CHECK(mimir_usable_size(p) >= page);
        mimir_free(p);
    }
    p = call_pvalloc(4097);
    CHECK(p != NULL);
    if (p != NULL) {
        CHECK(((uintptr_t)p % page) == 0);
        CHECK(mimir_usable_size(p) >= 2 * page);
        fill_check(p, 4097, 0x3cu);
        mimir_free(p);
    }
    p = call_pvalloc(8192);
    CHECK(p != NULL);
    if (p != NULL) {
        CHECK(mimir_usable_size(p) >= 2 * page);
        mimir_free(p);
    }
    /* pvalloc(0): posture null, NOT rounded into a page. */
    before = ledger_now();
    errno = 77;
    p = call_pvalloc(0);
    after = ledger_now();
    CHECK(p == NULL);
    CHECK(after.postures == before.postures + 1);
    CHECK(after.failures == before.failures);
    CHECK(errno == 77);
    /* Round-up overflow: exactly NULL, a genuine failure. */
    before = ledger_now();
    errno = 77;
    p = call_pvalloc(SIZE_MAX);
    after = ledger_now();
    CHECK(p == NULL);
    CHECK(after.failures == before.failures + 1);
    CHECK(after.postures == before.postures);
    CHECK(errno == 77);
    before = ledger_now();
    p = call_pvalloc(SIZE_MAX - 100);
    after = ledger_now();
    CHECK(p == NULL);
    CHECK(after.failures == before.failures + 1);

    /* G: family coherence -- a memalign block reallocs
     * under the family rules (contents preserved on
     * shrink; no alignment promise is asserted for the
     * result, per the Z-074 contract), and a pvalloc
     * block is writable over its whole rounded extent. */
    p = call_memalign(256, 512);
    CHECK(p != NULL);
    if (p != NULL) {
        void *q;
        fill_check(p, 512, 0x11u);
        q = mimir_realloc(p, 128);
        CHECK(q != NULL);
        if (q != NULL) {
            const unsigned char *b = (const unsigned char *)q;
            size_t i; int ok = 1;
            for (i = 0; i < 128; i++) {
                if (b[i] != (unsigned char)((i * 31u + 0x11u) & 0xffu)) ok = 0;
            }
            CHECK(ok);
            mimir_free(q);
        } else {
            mimir_free(p);
        }
    }

    /* Audit reconciliation: one count per call, exactly. */
    CHECK(load(&mimir_audit_call_memalign) - base_memalign == my_memalign_calls);
    CHECK(load(&mimir_audit_call_valloc) - base_valloc == my_valloc_calls);
    CHECK(load(&mimir_audit_call_pvalloc) - base_pvalloc == my_pvalloc_calls);

    printf("test_z083: %d checks / %d failed - %s\n",
           checks_run, checks_failed,
           checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
