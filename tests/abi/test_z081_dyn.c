/* test_z081_dyn.c - Z-081 dynamic consumer: links ONLY
 * against build/libmimiralloc.so (the E-002/E-007 route),
 * proving the whole audited ten-symbol surface is
 * reachable and behaves through the shared library
 * alone -- one exercised call per public symbol, read
 * back through the public statistics struct where a
 * cross-check exists. Exit 0 iff every check passes. */
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
    struct mimir_stats_v1 st;
    unsigned char *p, *q;
    void *m = NULL;
    size_t i;
    int t;

    p = mimir_malloc(96);
    CHECK(p != NULL);
    for (i = 0; i < 96; i++) {
        p[i] = (unsigned char)(i * 5u);
    }
    q = mimir_calloc(4, 32);
    CHECK(q != NULL);
    for (i = 0; i < 128; i++) {
        CHECK(q[i] == 0);
    }
    mimir_free(q);
    p = mimir_realloc(p, 200);
    CHECK(p != NULL);
    for (i = 0; i < 96; i++) {
        CHECK(p[i] == (unsigned char)(i * 5u));
    }
    CHECK(mimir_usable_size(p) >= 200);
    p = mimir_reallocarray(p, 4, 64);
    CHECK(p != NULL);
    CHECK(mimir_posix_memalign(&m, 64, 100) == 0);
    CHECK(m != NULL && ((uintptr_t)m % 64) == 0);
    mimir_free(m);
    q = mimir_aligned_alloc(128, 50);
    CHECK(q != NULL && ((uintptr_t)q % 128) == 0);
    mimir_free(q);
    CHECK(mimir_stats_snapshot(&st) == 0);
    CHECK(st.struct_version == MIMIR_STATS_VERSION);
    mimir_free(p);
    mimir_free(NULL);
    t = mimir_trim(0);
    CHECK(t == 0 || t == 1);

    printf("test_z081_dyn: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
