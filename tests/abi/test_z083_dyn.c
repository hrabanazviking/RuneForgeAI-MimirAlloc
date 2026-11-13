/* test_z083_dyn.c - Z-083 dynamic consumer: links ONLY
 * against build/libmimiralloc.so (the E-002/E-007
 * route), proving the memalign-family contract holds
 * through the shared library alone: memalign alignment
 * exactness and its invalid/zero refusals, valloc page
 * alignment at an unrounded size, pvalloc's page-multiple
 * usable extent and its zero/overflow refusals, and the
 * ledger deltas read through the PUBLIC statistics
 * struct. Exit 0 iff every check passes. */
#include "mimir_alloc.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int fails = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fails++;                                                    \
            printf("  [FAIL] %s (line %d)\n", #cond, __LINE__);         \
        }                                                               \
    } while (0)

int main(void)
{
    struct mimir_stats_v1 a, b;
    unsigned char *p;
    size_t i;

    CHECK(mimir_stats_snapshot(&a) == 0);
    errno = 77; CHECK(mimir_memalign(24, 100) == NULL); CHECK(errno == 77);
    errno = 77; CHECK(mimir_memalign(64, 0) == NULL); CHECK(errno == 77);
    errno = 77; CHECK(mimir_valloc(0) == NULL); CHECK(errno == 77);
    errno = 77; CHECK(mimir_pvalloc(0) == NULL); CHECK(errno == 77);
    errno = 77; CHECK(mimir_pvalloc(SIZE_MAX) == NULL); CHECK(errno == 77);
    CHECK(mimir_stats_snapshot(&b) == 0);
    /* failures: invalid memalign + pvalloc overflow = 2;
     * postures: the three zero-size calls = 3. */
    CHECK(b.alloc_failures - a.alloc_failures == 2);
    CHECK(b.alloc_posture_nulls - a.alloc_posture_nulls == 3);

    p = mimir_memalign(256, 300);
    CHECK(p != NULL);
    if (p != NULL) {
        CHECK(((uintptr_t)p % 256) == 0);
        CHECK(mimir_usable_size(p) >= 300);
        for (i = 0; i < 300; i++) p[i] = (unsigned char)(i * 7u);
        for (i = 0; i < 300; i++) CHECK(p[i] == (unsigned char)(i * 7u));
        mimir_free(p);
    }
    p = mimir_valloc(100);
    CHECK(p != NULL);
    if (p != NULL) {
        CHECK(((uintptr_t)p % 4096) == 0);
        CHECK(mimir_usable_size(p) >= 100);
        mimir_free(p);
    }
    p = mimir_pvalloc(4097);
    CHECK(p != NULL);
    if (p != NULL) {
        CHECK(((uintptr_t)p % 4096) == 0);
        CHECK(mimir_usable_size(p) >= 8192);
        mimir_free(p);
    }

    printf("test_z083_dyn: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
