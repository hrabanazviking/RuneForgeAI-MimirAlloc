/* test_e008_first.c - E-008 startup ordering (a): allocation
 * as the very first allocator call in a program.
 *
 * This translation unit contains NO constructor and NO
 * other MimirAlloc call site, and the executable links no
 * module that allocates before main (it links only
 * build/libmimiralloc.so, whose banked sources contain no
 * constructor, no pthread_once and no init function -
 * grep evidence in the recorded run). The first statement
 * of main is the mimir_malloc call itself, executed before
 * any printf (stdio may allocate through libc, never
 * through MimirAlloc; the claim proven here is exactly
 * "first MimirAlloc call", stated honestly). If the
 * allocator required any explicit or implicit init step
 * before first use, this call would be the one to fail.
 *
 * The banked design fact under test (B-007 C2 / B-006
 * F-08, restated in docs/PHASE_5_FOUNDATION_PLAN.md
 * Section 2): initialisation is clean by construction -
 * registry anchors and counters are static storage in
 * their defined starting state and the L6 lock is
 * statically initialised (PTHREAD_MUTEX_INITIALIZER).
 * This program is the first-call evidence for that fact
 * through the linked shared library.
 *
 * Defined cases only (U-09 / U-10 / U-12 not exercised).
 * Standalone via make test-e008; NOT part of make test.
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
    /* VERY FIRST MimirAlloc call in this program. Nothing
     * above this line in main, no constructor in this TU,
     * and no earlier call site exists in the process. */
    unsigned char *p = (unsigned char *)mimir_malloc(128);
    unsigned char *z;
    unsigned char *g;
    size_t i;
    int ok;

    printf("E-008 first-call consumer (first statement of main "
           "is mimir_malloc)\n");
    check(p != NULL, "first-ever mimir_malloc(128) returns non-NULL with no prior init");
    if (p != NULL) {
        check(((uintptr_t)p % _Alignof(max_align_t)) == 0,
              "first block aligned to _Alignof(max_align_t)");
        memset(p, 0xA6, 128);
        ok = 1;
        for (i = 0; i < 128; i++) { if (p[i] != 0xA6) { ok = 0; } }
        check(ok, "first block writable and readable over full extent");
        g = (unsigned char *)mimir_realloc(p, 300);
        check(g != NULL, "realloc grow of the first block returns non-NULL");
        if (g != NULL) {
            ok = 1;
            for (i = 0; i < 128; i++) { if (g[i] != 0xA6) { ok = 0; } }
            check(ok, "realloc preserves the first block's 128 bytes");
            mimir_free(g);
        } else {
            mimir_free(p);
        }
    }
    z = (unsigned char *)mimir_calloc(16, 8);
    check(z != NULL, "mimir_calloc after first malloc returns non-NULL");
    if (z != NULL) {
        ok = 1;
        for (i = 0; i < 128; i++) { if (z[i] != 0) { ok = 0; } }
        check(ok, "calloc storage zeroed at return");
        mimir_free(z);
    }
    mimir_free(NULL);
    check(1, "mimir_free(NULL) returns normally");
    p = (unsigned char *)mimir_malloc(32);
    check(p != NULL, "allocator healthy after the first-call lifecycle");
    if (p != NULL) {
        mimir_free(p);
    }
    printf("E008-FIRST %d/%d checks passed\n", passed, passed + failed);
    printf("RESULT: %s\n", failed == 0 ? "PASS" : "FAIL");
    return failed == 0 ? 0 : 1;
}
