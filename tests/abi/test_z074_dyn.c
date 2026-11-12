/* test_z074_dyn.c - Z-074 dynamic consumer (E-series style).
 *
 * Linked ONLY against build/libmimiralloc.so, never the
 * static objects: it proves the two Wave 9 entry points
 * resolve and behave through the shared library's C ABI,
 * and that blocks they return are freed -- and realloc'd
 * -- through the same .so (cross-entry compatibility over
 * the shared ABI, not allocation-only use). The export
 * list, DT_NEEDED and undefined-symbol proofs are in the
 * runner (tests/abi/run_z074.sh) and its recorded run.
 *
 * Standalone: built and run via `make test-z074`; NOT
 * part of `make test`.
 */
#include "mimir_alloc.h"

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

int main(void)
{
    static const size_t aligns[] = { 16, 32, 64, 256, 4096,
                                     8192, 65536 };
    size_t i;

    printf("Z-074 dynamic consumer (libmimiralloc.so only)\n");
    for (i = 0; i < sizeof aligns / sizeof aligns[0]; i++) {
        unsigned char *p = mimir_aligned_alloc(aligns[i], 300);
        unsigned char *q;
        CHECK(p != NULL);
        if (p == NULL) {
            continue;
        }
        CHECK((uintptr_t)p % aligns[i] == 0);
        memset(p, 0xA5, 300);
        CHECK(p[0] == 0xA5 && p[299] == 0xA5);
        /* Cross-entry over the .so: realloc, then free. */
        q = mimir_realloc(p, 900);
        CHECK(q != NULL);
        if (q != NULL) {
            CHECK(q[0] == 0xA5 && q[299] == 0xA5);
            mimir_free(q);
        } else {
            mimir_free(p);
        }
    }
    {
        void *out = NULL;
        CHECK(mimir_posix_memalign(&out, 4096, 128) == 0);
        CHECK(out != NULL && (uintptr_t)out % 4096 == 0);
        mimir_free(out);
        out = (void *)(uintptr_t)0x1;
        CHECK(mimir_posix_memalign(&out, 24, 128) == EINVAL);
        CHECK(out == (void *)(uintptr_t)0x1);
        CHECK(mimir_aligned_alloc(3, 128) == NULL);
        CHECK(mimir_aligned_alloc(0, 128) == NULL);
    }
    printf("Z074 DYN: %d checks, %d failed\n", checks_run,
           checks_failed);
    printf("RESULT: %s\n", checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
