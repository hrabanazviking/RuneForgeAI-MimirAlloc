/* test_e003.c - E-003 link-route consumer (Phase 5, Wave 5).
 *
 * A small consumer program that uses ONLY the four public
 * symbols declared in include/mimir_alloc.h (mimir_malloc,
 * mimir_free, mimir_calloc, mimir_realloc) and exercises a
 * full allocation lifecycle through them. It is built two
 * ways by the slice evidence (see
 * tests/abi/recorded_run_e003.txt):
 *
 *   R1 static  - linked against build/libmimiralloc.a
 *                (the archive this slice adds). PROVEN in
 *                this slice: the binary carries the four
 *                symbols as defined (T) and has no
 *                DT_NEEDED for libmimiralloc.
 *   R2 dynamic - linked against build/libmimiralloc.so.
 *                OWED at this base: no shared-library
 *                target exists in this tree (E-002, the
 *                .so slice, is not banked yet). The exact
 *                commands that will prove R2 with this
 *                same source are recorded in
 *                docs/PHASE_5_LINK_ROUTES_E003.md and in
 *                the recorded run; no dynamic run is
 *                claimed here.
 *
 * The program deliberately calls no internal name
 * (mimir_core_* / mimir_platform_*) and no standard-name
 * allocator entry point for its lifecycle: binding to
 * MimirAlloc is by explicit mimir_* calls only (E-001
 * plan Section 5.2, Explicitly called finding).
 *
 * Checks (all by analogy to docs/contracts.md core rows,
 * via include/mimir_alloc.h header notes; U-03 remains
 * unresolved and nothing here settles it):
 *  - mimir_malloc succeeds, storage holds a pattern over
 *    its full extent, pointer is aligned to
 *    _Alignof(max_align_t) (non-numeric claim only, U-15);
 *  - mimir_calloc succeeds and reads zero at return;
 *  - mimir_realloc grows the calloc block preserving the
 *    written prefix byte-for-byte, and shrinks it
 *    preserving the surviving prefix;
 *  - mimir_realloc(NULL, n) acts as mimir_malloc;
 *  - mimir_malloc(SIZE_MAX) returns NULL (oversize
 *    refusal);
 *  - mimir_free(NULL) is a defined no-op;
 *  - every live block is freed exactly once and a fresh
 *    allocation after the frees still succeeds.
 *
 * Zero-size behaviour is NOT checked: it is
 * [UNRESOLVED: U-09] and this consumer must not be cited
 * as settling it. errno is NOT checked (not promised,
 * U-10).
 */
#include "mimir_alloc.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
    unsigned char *a;
    unsigned char *z;
    unsigned char *g;
    unsigned char *s;
    unsigned char *n;
    unsigned char *again;
    size_t i;
    int ok;

    printf("E-003 link-route consumer (4 public symbols only)\n");

    /* mimir_malloc: allocate, fill, verify. */
    a = mimir_malloc(64);
    check(a != NULL, "mimir_malloc(64) returns non-NULL");
    if (a != NULL) {
        check(((uintptr_t)a % _Alignof(max_align_t)) == 0,
              "mimir_malloc(64) aligned to _Alignof(max_align_t)");
        for (i = 0; i < 64; i++) {
            a[i] = (unsigned char)(0xA0u + (i & 0x0Fu));
        }
        ok = 1;
        for (i = 0; i < 64; i++) {
            if (a[i] != (unsigned char)(0xA0u + (i & 0x0Fu))) {
                ok = 0;
            }
        }
        check(ok, "mimir_malloc(64) pattern verifies over full extent");
    }

    /* mimir_calloc: allocate, require zeroed at return. */
    z = mimir_calloc(8, 16); /* 128 bytes */
    check(z != NULL, "mimir_calloc(8, 16) returns non-NULL");
    if (z != NULL) {
        ok = 1;
        for (i = 0; i < 128; i++) {
            if (z[i] != 0) {
                ok = 0;
            }
        }
        check(ok, "mimir_calloc(8, 16) storage reads zero at return");
        for (i = 0; i < 128; i++) {
            z[i] = (unsigned char)(i & 0xFFu);
        }

        /* mimir_realloc grow: prefix preserved. */
        g = mimir_realloc(z, 512);
        check(g != NULL, "mimir_realloc(block, 512) grow returns non-NULL");
        if (g != NULL) {
            ok = 1;
            for (i = 0; i < 128; i++) {
                if (g[i] != (unsigned char)(i & 0xFFu)) {
                    ok = 0;
                }
            }
            check(ok, "mimir_realloc grow preserves first 128 bytes");
            for (i = 128; i < 512; i++) {
                g[i] = 0x5A;
            }

            /* mimir_realloc shrink: surviving prefix preserved. */
            s = mimir_realloc(g, 32);
            check(s != NULL,
                  "mimir_realloc(block, 32) shrink returns non-NULL");
            if (s != NULL) {
                ok = 1;
                for (i = 0; i < 32; i++) {
                    if (s[i] != (unsigned char)(i & 0xFFu)) {
                        ok = 0;
                    }
                }
                check(ok, "mimir_realloc shrink preserves first 32 bytes");
                mimir_free(s);
            } else {
                mimir_free(g);
            }
        } else {
            mimir_free(z);
        }
    }

    /* mimir_realloc(NULL, n) acts as mimir_malloc. */
    n = mimir_realloc(NULL, 32);
    check(n != NULL, "mimir_realloc(NULL, 32) returns non-NULL");
    if (n != NULL) {
        n[0] = 0x3C;
        check(n[0] == 0x3C, "mimir_realloc(NULL, 32) storage writable");
        mimir_free(n);
    }

    /* Oversize refusal: exactly NULL. */
    check(mimir_malloc(SIZE_MAX) == NULL,
          "mimir_malloc(SIZE_MAX) returns NULL (oversize refusal)");

    /* mimir_free(NULL): defined no-op. */
    mimir_free(NULL);
    check(1, "mimir_free(NULL) returns normally (defined no-op)");

    /* Free the malloc block, then allocate again. */
    if (a != NULL) {
        mimir_free(a);
        check(1, "mimir_free(malloc block) returns normally");
    }
    again = mimir_malloc(48);
    check(again != NULL, "mimir_malloc(48) after frees returns non-NULL");
    if (again != NULL) {
        memset(again, 0x7E, 48);
        check(again[47] == 0x7E,
              "mimir_malloc(48) storage writable over full extent");
        mimir_free(again);
    }

    printf("E003 %d/%d checks passed\n", passed, passed + failed);
    printf("RESULT: %s\n", failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}
