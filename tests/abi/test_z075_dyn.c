/* test_z075_dyn.c - Z-075 dynamic consumer (E-series
 * style). Linked ONLY against build/libmimiralloc.so,
 * never the static objects: it proves mimir_usable_size
 * resolves and answers exactly through the shared
 * library's C ABI (decision D-012 semantics: class extent
 * for class-served blocks, mapping usable extent for
 * mapping-served blocks, 0 for NULL / interior / foreign
 * / freed, shrink never reduces the answer). The export
 * list, DT_NEEDED and undefined-symbol proofs are in the
 * runner (tests/abi/run_z075.sh) and its recorded run.
 *
 * Standalone: built and run via `make test-z075`; NOT
 * part of `make test`.
 */
#include "mimir_alloc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

int main(void)
{
    size_t ps = (size_t)sysconf(_SC_PAGESIZE);
    unsigned char *p;
    unsigned char *q;

    printf("Z-075 dynamic consumer (libmimiralloc.so only)\n");
    CHECK(mimir_usable_size(NULL) == 0);

    p = mimir_malloc(100);
    CHECK(p != NULL && mimir_usable_size(p) == 112);
    if (p != NULL) {
        memset(p, 0x5A, 112);
        CHECK(p[0] == 0x5A && p[111] == 0x5A);
        CHECK(mimir_usable_size(p + 1) == 0);
    }
    q = mimir_realloc(p, 5000);
    CHECK(q != NULL &&
          mimir_usable_size(q) == (size_t)((5000 + ps - 1) & ~(ps - 1)));
    if (q != NULL) {
        q = mimir_realloc(q, 2000); /* shrink: extent kept */
        CHECK(q != NULL &&
              mimir_usable_size(q) ==
                  (size_t)((5000 + ps - 1) & ~(ps - 1)));
        mimir_free(q);
        CHECK(mimir_usable_size(q) == 0);
    } else {
        mimir_free(p);
    }

    p = mimir_aligned_alloc(8192, 100);
    CHECK(p != NULL && (uintptr_t)p % 8192 == 0);
    CHECK(p != NULL && mimir_usable_size(p) >= 100);
    mimir_free(p);

    printf("Z075 DYN: %d checks, %d failed\n", checks_run,
           checks_failed);
    printf("RESULT: %s\n", checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
