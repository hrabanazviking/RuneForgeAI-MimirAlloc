/* test_smoke.c - contract smoke test (C-001, updated C-003).
 *
 * Links against the L1 boundary in src/api/mimir_alloc.c,
 * backed since C-003 by the real mapping-backed core in
 * src/core/mimir_core.c (the C-001 stub is gone).
 *
 * C-003 UPDATE, stated plainly as the slice requires: the
 * stub-era expectations for mimir_malloc(1) and
 * mimir_malloc(1024) genuinely changed -- the stub returned
 * NULL for every request because it implemented nothing; the
 * real allocator succeeds for these ordinary requests. Those
 * two checks now assert success (non-null), touch the storage
 * the caller owns, and free it. Every other expectation is
 * unchanged: SIZE_MAX is still refused with NULL (now the
 * real oversize refusal, MA-6.1-MALLOC-04 by analogy),
 * mimir_malloc(0) still returns NULL as a posture only
 * (U-09 unresolved, NOT settled by this test), and
 * mimir_free(NULL) is still the defined no-op.
 *
 * Checks:
 *  - the header is self-contained (it is the first include
 *    here) and its include guard is in effect;
 *  - the two declared prototypes have the exact C ABI
 *    signatures from include/mimir_alloc.h (compile-time
 *    function-pointer checks, plus a runtime identity
 *    check);
 *  - mimir_malloc succeeds for a small and a larger ordinary
 *    request, the returned storage is writable and readable
 *    by its owner, and mimir_free retires it;
 *  - the oversize case SIZE_MAX returns NULL
 *    (MA-6.1-MALLOC-04 by analogy);
 *  - mimir_malloc(0) returns NULL -- an implementation
 *    posture only, recorded here explicitly as NOT a
 *    contract check, because zero-size is [UNRESOLVED:
 *    U-09] and this test must not be cited as settling it;
 *  - mimir_free(NULL) is a defined no-op that returns
 *    normally (MA-6.1-FREE-02 by analogy), including when
 *    handed the NULL result of a failed mimir_malloc call.
 *
 * This test deliberately does NOT check: allocation contents
 * before the caller writes them (unspecified, never
 * zero-guaranteed, B-007 C1), any errno value (not promised,
 * U-10), any numeric alignment value (not claimed, U-15), or
 * any absent symbol (absence of mimir_calloc, mimir_realloc,
 * and the other B-008 S6.1 not-present facilities is a
 * link-time fact verified with nm on the built objects,
 * recorded in the slice evidence, not a runtime check). The
 * thorough allocator exercise lives in tests/core/test_core.c.
 */
#include "mimir_alloc.h"
#include "mimir_alloc.h" /* second inclusion must be a no-op */

#include <stdint.h>
#include <stdio.h>

/* Compile-time prototype checks: these assignments fail to
 * compile if the header's signatures ever drift. */
static void *(*const check_malloc_sig)(size_t) = mimir_malloc;
static void (*const check_free_sig)(void *) = mimir_free;

static int passed = 0;
static int failed = 0;

static void check(int condition, const char *name)
{
    if (condition) {
        passed++;
        printf("PASS %s\n", name);
    } else {
        failed++;
        printf("FAIL %s\n", name);
    }
}

int main(void)
{
    void *small;
    void *larger;

#ifdef MIMIR_ALLOC_H
    check(1, "header include guard MIMIR_ALLOC_H defined");
#else
    check(0, "header include guard MIMIR_ALLOC_H defined");
#endif
    check(check_malloc_sig == &mimir_malloc,
          "mimir_malloc prototype matches void *(*)(size_t)");
    check(check_free_sig == &mimir_free,
          "mimir_free prototype matches void (*)(void *)");

    /* C-003: real allocator -- ordinary requests succeed. */
    small = mimir_malloc(1);
    check(small != NULL,
          "mimir_malloc(1) returns non-NULL (real allocator)");
    if (small != NULL) {
        ((unsigned char *)small)[0] = 0xA5;
        check(((unsigned char *)small)[0] == 0xA5,
              "mimir_malloc(1) storage writable/readable by owner");
        mimir_free(small);
    }
    larger = mimir_malloc(1024);
    check(larger != NULL,
          "mimir_malloc(1024) returns non-NULL (real allocator)");
    if (larger != NULL) {
        ((unsigned char *)larger)[1023] = 0x5A;
        check(((unsigned char *)larger)[1023] == 0x5A,
              "mimir_malloc(1024) storage writable/readable by owner");
        mimir_free(larger);
    }

    check(mimir_malloc(SIZE_MAX) == NULL,
          "mimir_malloc(SIZE_MAX) returns NULL (oversize refusal)");
    check(mimir_malloc(0) == NULL,
          "mimir_malloc(0) returns NULL (posture only, U-09 unresolved)");

    mimir_free(NULL);
    check(1, "mimir_free(NULL) returns normally (defined no-op)");

    mimir_free(mimir_malloc(8));
    check(1, "mimir_free(mimir_malloc(8) result) returns normally");

    printf("SMOKE %d/%d checks passed\n", passed, passed + failed);
    return failed == 0 ? 0 : 1;
}
