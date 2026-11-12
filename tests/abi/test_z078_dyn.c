/* test_z078_dyn.c - Z-078 dynamic consumer: links ONLY
 * against build/libmimiralloc.so (the E-002/E-007
 * route), proving the D-013 zero-size and errno
 * contract holds through the shared library alone:
 * the zero-size rows, realloc-to-zero preservation,
 * the posix-shaped ENOMEM row, the checked-failure
 * errno sentinels, and the ledger deltas read through
 * the PUBLIC statistics struct (postures move,
 * failures do not). Exit 0 iff every check passes. */
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

#define SENTINEL 77

int main(void)
{
    struct mimir_stats_v1 a, b;
    unsigned char *p;
    void *m = (void *)0x1;
    size_t i;
    int rc;

    CHECK(mimir_stats_snapshot(&a) == 0);
    errno = SENTINEL; CHECK(mimir_malloc(0) == NULL); CHECK(errno == SENTINEL);
    errno = SENTINEL; CHECK(mimir_calloc(0, 16) == NULL); CHECK(errno == SENTINEL);
    errno = SENTINEL; CHECK(mimir_realloc(NULL, 0) == NULL); CHECK(errno == SENTINEL);
    CHECK(mimir_stats_snapshot(&b) == 0);
    CHECK(b.alloc_posture_nulls - a.alloc_posture_nulls == 3);
    CHECK(b.alloc_failures - a.alloc_failures == 0);

    p = mimir_malloc(96);
    CHECK(p != NULL);
    for (i = 0; i < 96; i++) {
        p[i] = (unsigned char)(i * 5u);
    }
    errno = SENTINEL; CHECK(mimir_realloc(p, 0) == NULL); CHECK(errno == SENTINEL);
    for (i = 0; i < 96; i++) {
        CHECK(p[i] == (unsigned char)(i * 5u)); /* block live, intact */
    }
    CHECK(mimir_usable_size(p) >= 96);
    errno = SENTINEL; CHECK(mimir_reallocarray(p, 0, 8) == NULL); CHECK(errno == SENTINEL);
    mimir_free(p); /* freeable exactly as before */

    errno = SENTINEL; CHECK(mimir_aligned_alloc(64, 0) == NULL); CHECK(errno == SENTINEL);
    errno = SENTINEL; rc = mimir_posix_memalign(&m, 8, 0); CHECK(rc == ENOMEM); CHECK(errno == SENTINEL);
    CHECK(m == (void *)0x1);
    errno = SENTINEL; CHECK(mimir_malloc(SIZE_MAX) == NULL); CHECK(errno == SENTINEL);
    errno = SENTINEL; CHECK(mimir_calloc(SIZE_MAX, 2) == NULL); CHECK(errno == SENTINEL);

    printf("test_z078_dyn: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
