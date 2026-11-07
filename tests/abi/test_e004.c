/* test_e004.c - E-004 independent binding-proof consumer (Phase 5, Wave 5).
 *
 * This program is INDEPENDENT of test_e002.c and test_e003.c:
 * it is a new consumer, written for this slice, that links
 * ONLY against build/libmimiralloc.so (the banked E-002
 * shared library, via make lib) and exercises the full
 * allocation lifecycle through the four public symbols
 * declared in include/mimir_alloc.h:
 * mimir_malloc, mimir_free, mimir_calloc, mimir_realloc.
 *
 * Its purpose is the Phase 5 exit evidence named by
 * E-001 Section 5.4 and E-003 Section 5: an independent
 * test executable whose undefined mimir_* references are
 * bound at load/run time to libmimiralloc.so (proven by
 * nm/readelf on this executable, ldd resolution, and
 * LD_DEBUG=bindings output recorded in
 * tests/abi/recorded_run_e004.txt), and which passes
 * lifecycle tests through that dynamically bound library.
 *
 * It calls no internal name (mimir_core_* /
 * mimir_platform_*) and no standard-name allocator entry
 * point for its lifecycle: binding is by explicit mimir_*
 * calls only. Zero-size behaviour is NOT checked
 * ([UNRESOLVED: U-09]); errno is NOT checked (U-10).
 * Behaviour is by analogy under [UNRESOLVED: U-03],
 * exactly as the public header states it.
 *
 * Standalone on purpose: built and run via make test-e004;
 * NOT part of make test. See
 * docs/PHASE_5_BINDING_PROOF_E004.md.
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
    unsigned char *b;
    unsigned char *z;
    unsigned char *g;
    unsigned char *s;
    unsigned char *n;
    unsigned char *again;
    size_t i;
    int ok;

    printf("E-004 independent binding-proof consumer (dynamic, libmimiralloc.so)\n");

    /* Two live malloc blocks: distinct, aligned, patterns intact. */
    a = mimir_malloc(96);
    check(a != NULL, "mimir_malloc(96) returns non-NULL");
    b = mimir_malloc(96);
    check(b != NULL, "second mimir_malloc(96) returns non-NULL");
    if (a != NULL) {
        check(((uintptr_t)a % _Alignof(max_align_t)) == 0,
              "mimir_malloc(96) aligned to _Alignof(max_align_t)");
    }
    if (a != NULL && b != NULL) {
        check(a != b, "two live malloc blocks are distinct");
        memset(a, 0xC1, 96);
        memset(b, 0x3E, 96);
        ok = 1;
        for (i = 0; i < 96; i++) {
            if (a[i] != 0xC1 || b[i] != 0x3E) { ok = 0; }
        }
        check(ok, "live malloc blocks hold distinct patterns undisturbed");
    }

    /* calloc: zeroed at return, then writable. */
    z = mimir_calloc(12, 16); /* 192 bytes */
    check(z != NULL, "mimir_calloc(12, 16) returns non-NULL");
    if (z != NULL) {
        ok = 1;
        for (i = 0; i < 192; i++) { if (z[i] != 0) { ok = 0; } }
        check(ok, "mimir_calloc(12, 16) storage reads zero at return");
        for (i = 0; i < 192; i++) { z[i] = (unsigned char)(0xFFu - (i & 0xFFu)); }

        /* realloc grow: prefix preserved byte-for-byte. */
        g = mimir_realloc(z, 640);
        check(g != NULL, "mimir_realloc(block, 640) grow returns non-NULL");
        if (g != NULL) {
            ok = 1;
            for (i = 0; i < 192; i++) {
                if (g[i] != (unsigned char)(0xFFu - (i & 0xFFu))) { ok = 0; }
            }
            check(ok, "mimir_realloc grow preserves first 192 bytes");
            for (i = 192; i < 640; i++) { g[i] = 0x6B; }

            /* realloc shrink: surviving prefix preserved. */
            s = mimir_realloc(g, 40);
            check(s != NULL, "mimir_realloc(block, 40) shrink returns non-NULL");
            if (s != NULL) {
                ok = 1;
                for (i = 0; i < 40; i++) {
                    if (s[i] != (unsigned char)(0xFFu - (i & 0xFFu))) { ok = 0; }
                }
                check(ok, "mimir_realloc shrink preserves first 40 bytes");
                mimir_free(s);
            } else {
                mimir_free(g);
            }
        } else {
            mimir_free(z);
        }
    }

    /* realloc(NULL, n) acts as malloc. */
    n = mimir_realloc(NULL, 56);
    check(n != NULL, "mimir_realloc(NULL, 56) returns non-NULL");
    if (n != NULL) {
        memset(n, 0x91, 56);
        check(n[0] == 0x91 && n[55] == 0x91,
              "mimir_realloc(NULL, 56) storage writable over full extent");
        mimir_free(n);
    }

    /* Oversize and calloc-overflow refusals: exactly NULL. */
    check(mimir_malloc(SIZE_MAX) == NULL,
          "mimir_malloc(SIZE_MAX) returns NULL (oversize refusal)");
    check(mimir_calloc(SIZE_MAX, 2) == NULL,
          "mimir_calloc(SIZE_MAX, 2) returns NULL (overflow refusal)");

    /* free(NULL) no-op; free live blocks; allocator healthy after. */
    mimir_free(NULL);
    check(1, "mimir_free(NULL) returns normally (defined no-op)");
    if (a != NULL) { mimir_free(a); }
    if (b != NULL) { mimir_free(b); }
    check(1, "mimir_free of live malloc blocks returns normally");
    again = mimir_malloc(72);
    check(again != NULL, "mimir_malloc(72) after frees returns non-NULL");
    if (again != NULL) {
        memset(again, 0x2D, 72);
        check(again[0] == 0x2D && again[71] == 0x2D,
              "mimir_malloc(72) storage writable over full extent");
        mimir_free(again);
    }

    printf("E004 %d/%d checks passed\n", passed, passed + failed);
    printf("RESULT: %s\n", failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}
