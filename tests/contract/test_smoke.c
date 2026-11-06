/* test_smoke.c - C-001 foundation smoke test.
 *
 * Links against the stub in src/api/mimir_alloc.c and
 * checks only what the stub honestly offers:
 *  - the header is self-contained (it is the first include
 *    here) and its include guard is in effect;
 *  - the two declared prototypes have the exact C ABI
 *    signatures from include/mimir_alloc.h (compile-time
 *    function-pointer checks, plus a runtime identity
 *    check);
 *  - the stub returns the documented not-implemented /
 *    defined-refusal result: NULL from mimir_malloc, for a
 *    small request, a larger request, and SIZE_MAX (the
 *    oversize case, MA-6.1-MALLOC-04 by analogy);
 *  - mimir_malloc(0) also returns NULL in the stub - a
 *    stub posture only, recorded here explicitly as NOT a
 *    contract check, because zero-size is [UNRESOLVED:
 *    U-09] and this test must not be cited as settling it;
 *  - mimir_free(NULL) is a defined no-op that returns
 *    normally (MA-6.1-FREE-02 by analogy), including when
 *    handed the NULL result of a stub mimir_malloc call.
 *
 * This test deliberately does NOT check: any successful
 * allocation (the stub performs none, and faking one is
 * forbidden), any errno value (not promised, U-10), any
 * numeric alignment (not claimed, U-15), or any absent
 * symbol (absence of mimir_calloc, mimir_realloc, and the
 * other B-008 S6.1 not-present facilities is a link-time
 * fact verified with nm on the built objects, recorded in
 * the slice evidence, not a runtime check).
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
#ifdef MIMIR_ALLOC_H
    check(1, "header include guard MIMIR_ALLOC_H defined");
#else
    check(0, "header include guard MIMIR_ALLOC_H defined");
#endif
    check(check_malloc_sig == &mimir_malloc,
          "mimir_malloc prototype matches void *(*)(size_t)");
    check(check_free_sig == &mimir_free,
          "mimir_free prototype matches void (*)(void *)");
    check(mimir_malloc(1) == NULL,
          "stub mimir_malloc(1) returns NULL (not implemented)");
    check(mimir_malloc(1024) == NULL,
          "stub mimir_malloc(1024) returns NULL (not implemented)");
    check(mimir_malloc(SIZE_MAX) == NULL,
          "stub mimir_malloc(SIZE_MAX) returns NULL (oversize refusal)");
    check(mimir_malloc(0) == NULL,
          "stub mimir_malloc(0) returns NULL (stub posture, U-09 unresolved)");

    mimir_free(NULL);
    check(1, "mimir_free(NULL) returns normally (defined no-op)");

    mimir_free(mimir_malloc(8));
    check(1, "mimir_free(stub mimir_malloc result) returns normally");

    printf("SMOKE %d/%d checks passed\n", passed, passed + failed);
    return failed == 0 ? 0 : 1;
}
