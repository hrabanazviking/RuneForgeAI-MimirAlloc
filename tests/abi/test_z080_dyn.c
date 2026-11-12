/* test_z080_dyn.c - Z-080 dynamic consumer: the whole
 * current public surface (ten symbols) exercised through a
 * VERSIONED shared library only.
 *
 * This consumer is linked against the versioned library
 * file (build/libmimiralloc.so.<VERSION>, SONAME
 * libmimiralloc.so.0) by `make test-z080`, and against a
 * staged install of that library by tests/abi/run_z080.sh.
 * In both routes the link records DT_NEEDED =
 * libmimiralloc.so.0 -- the SONAME, the versioned identity
 * -- never the unversioned build-tree name the E-002 route
 * records. It never links the static objects.
 *
 * Standalone: built and run via `make test-z080`; NOT
 * part of `make test`.
 */
#include "mimir_alloc.h"

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
    unsigned char *p, *q, *a;
    void *out = NULL;
    struct mimir_stats_v1 st;
    size_t i;
    int r;

    printf("Z-080 dynamic consumer (versioned libmimiralloc only)\n");

    /* The original four (E-002 surface). */
    p = mimir_malloc(100);
    CHECK(p != NULL);
    CHECK(mimir_usable_size(NULL) == 0);
    if (p != NULL) {
        CHECK(mimir_usable_size(p) >= 100);
        for (i = 0; i < 100; i++) {
            p[i] = (unsigned char)(i * 3u);
        }
        q = mimir_realloc(p, 640);
        CHECK(q != NULL);
        if (q != NULL) {
            for (i = 0; i < 100; i++) {
                CHECK(q[i] == (unsigned char)(i * 3u));
            }
            p = q;
        }
    }
    q = mimir_calloc(12, 16);
    CHECK(q != NULL);
    if (q != NULL) {
        int zeroed = 1;
        for (i = 0; i < 192; i++) {
            if (q[i] != 0) {
                zeroed = 0;
            }
        }
        CHECK(zeroed);
        mimir_free(q);
    }

    /* The Z-074 aligned pair. */
    a = mimir_aligned_alloc(64, 128);
    CHECK(a != NULL);
    if (a != NULL) {
        CHECK((uintptr_t)a % 64 == 0);
        mimir_free(a);
    }
    CHECK(mimir_posix_memalign(&out, 64, 128) == 0);
    CHECK(out != NULL && (uintptr_t)out % 64 == 0);
    mimir_free(out);

    /* Z-076 public statistics. */
    memset(&st, 0, sizeof st);
    CHECK(mimir_stats_snapshot(&st) == 0);
    CHECK(st.struct_version == 1);
    CHECK(st.page_size_bytes == 4096);

    /* Z-077 pair: reallocarray round-trip + overflow refusal,
     * trim posture returns. */
    q = mimir_reallocarray(NULL, 4, 16);
    CHECK(q != NULL);
    if (q != NULL) {
        for (i = 0; i < 64; i++) {
            q[i] = (unsigned char)(i * 7u);
        }
        q = mimir_reallocarray(q, 16, 16);
        CHECK(q != NULL);
        if (q != NULL) {
            for (i = 0; i < 64; i++) {
                CHECK(q[i] == (unsigned char)(i * 7u));
            }
        }
        CHECK(mimir_reallocarray(q, SIZE_MAX, 2) == NULL);
        mimir_free(q);
    }
    if (p != NULL) {
        mimir_free(p);
        p = NULL;
    }
    r = mimir_trim(SIZE_MAX); /* keep everything */
    CHECK(r == 0);
    r = mimir_trim(0);
    CHECK(r == 0 || r == 1);

    /* Post-trim health through the same .so. */
    p = mimir_malloc(72);
    CHECK(p != NULL);
    mimir_free(p);
    mimir_free(NULL); /* defined no-op */

    printf("Z080 DYN: %d checks, %d failed\n", checks_run,
           checks_failed);
    printf("RESULT: %s\n", checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
