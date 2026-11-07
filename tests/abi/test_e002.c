/* test_e002.c - MimirAlloc slice E-002 consumer evidence test.
 *
 * This program is a CONSUMER of the shared library: it is
 * linked against build/libmimiralloc.so (never against the
 * static library objects) and exercises the four exported
 * entry points exactly as an outside caller would, through
 * the public header only. It proves the .so is loadable,
 * linkable and functional, complementing the .dynsym
 * inspection in tests/abi/recorded_run_e002.txt, which proves
 * the export list is exactly the four public symbols.
 *
 * Standalone on purpose: built and run via `make test-e002`;
 * it is NOT part of `make test` (E-002 is additive wiring
 * only). See docs/SHARED_LIBRARY_BUILD.md.
 *
 * Behaviour checked is the public header's defined-case list,
 * by analogy under [UNRESOLVED: U-03] exactly as the header
 * states it; zero-size and realloc-to-zero postures are NOT
 * exercised here because no behaviour is promised for them
 * (U-09 / U-12 remain unresolved).
 */
#include "mimir_alloc.h"

#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks_run++;                                                   \
        if (cond) {                                                     \
            printf("  [ok] %s\n", #cond);                               \
        } else {                                                        \
            checks_failed++;                                            \
            printf("  [FAIL] %s (line %d)\n", #cond, __LINE__);         \
        }                                                               \
    } while (0)

int main(void)
{
    printf("E-002 consumer test (linked against libmimiralloc.so)\n");

    /* mimir_malloc: non-null, suitably aligned, writable,
     * distinct storage across live blocks. */
    unsigned char *a = mimir_malloc(64);
    CHECK(a != NULL);
    CHECK(((uintptr_t)a % alignof(max_align_t)) == 0);
    unsigned char *b = mimir_malloc(64);
    CHECK(b != NULL);
    CHECK(a != b);
    if (a != NULL && b != NULL) {
        memset(a, 0xA5, 64);
        memset(b, 0x5A, 64);
        int intact = 1;
        for (size_t i = 0; i < 64; i++) {
            if (a[i] != 0xA5 || b[i] != 0x5A) {
                intact = 0;
            }
        }
        CHECK(intact);
    }

    /* mimir_calloc: requested extent zero at return, then
     * writable over the full extent. */
    unsigned char *c = mimir_calloc(16, 8);
    CHECK(c != NULL);
    if (c != NULL) {
        int zeroed = 1;
        for (size_t i = 0; i < 128; i++) {
            if (c[i] != 0) {
                zeroed = 0;
            }
        }
        CHECK(zeroed);
        memset(c, 0x3C, 128);
        CHECK(c[0] == 0x3C && c[127] == 0x3C);
    }

    /* mimir_realloc grow: first min(old, new) bytes preserved
     * byte-for-byte at the returned pointer. */
    unsigned char *g = mimir_malloc(32);
    CHECK(g != NULL);
    if (g != NULL) {
        for (size_t i = 0; i < 32; i++) {
            g[i] = (unsigned char)(i + 1);
        }
        unsigned char *g2 = mimir_realloc(g, 96);
        CHECK(g2 != NULL);
        if (g2 != NULL) {
            int preserved = 1;
            for (size_t i = 0; i < 32; i++) {
                if (g2[i] != (unsigned char)(i + 1)) {
                    preserved = 0;
                }
            }
            CHECK(preserved);
            g = g2;
        }
    }

    /* mimir_realloc shrink: preserved prefix, returned pointer
     * is the one to use and to free. */
    if (g != NULL) {
        unsigned char *g3 = mimir_realloc(g, 16);
        CHECK(g3 != NULL);
        if (g3 != NULL) {
            CHECK(g3[0] == 1 && g3[15] == 16);
            g = g3;
        }
    }

    /* mimir_free(NULL) is a defined no-op; live blocks free
     * exactly once; the allocator stays healthy afterwards. */
    mimir_free(NULL);
    CHECK(1); /* reached: free(NULL) returned normally */
    mimir_free(a);
    mimir_free(b);
    mimir_free(c);
    mimir_free(g);
    unsigned char *d = mimir_malloc(48);
    CHECK(d != NULL);
    if (d != NULL) {
        memset(d, 0x77, 48);
        CHECK(d[0] == 0x77 && d[47] == 0x77);
        mimir_free(d);
    }

    printf("CHECKS: %d run, %d failed\n", checks_run, checks_failed);
    if (checks_failed == 0) {
        printf("RESULT: PASS\n");
        return 0;
    }
    printf("RESULT: FAIL\n");
    return 1;
}
