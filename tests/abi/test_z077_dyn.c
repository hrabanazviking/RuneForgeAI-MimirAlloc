/* test_z077_dyn.c - Z-077 dynamic consumer: links ONLY
 * against build/libmimiralloc.so (the E-002/E-007 route),
 * proving mimir_trim and mimir_reallocarray are real
 * dynamic exports with their public behaviour: a
 * reallocarray round-trip with preservation, an overflow
 * refusal, the zero-factor posture return, and trim
 * returns in {0, 1} with the keep-everything pad exactly
 * 0. Exit 0 iff every check passes. */
#include "mimir_alloc.h"

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
    unsigned char *p, *q;
    size_t i;
    int r;

    p = mimir_reallocarray(NULL, 4, 16);
    CHECK(p != NULL);
    for (i = 0; i < 64; i++) {
        p[i] = (unsigned char)(i * 7u);
    }
    q = mimir_reallocarray(p, 16, 16);
    CHECK(q != NULL);
    for (i = 0; i < 64; i++) {
        CHECK(q[i] == (unsigned char)(i * 7u));
    }
    CHECK(mimir_reallocarray(q, SIZE_MAX, 2) == NULL);
    CHECK(mimir_reallocarray(NULL, 0, 8) == NULL);
    r = mimir_trim(SIZE_MAX); /* keep everything */
    CHECK(r == 0);
    r = mimir_trim(0);
    CHECK(r == 0 || r == 1);
    mimir_free(q);
    printf("test_z077_dyn: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
