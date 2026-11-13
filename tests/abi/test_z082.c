/* test_z082.c - Z-082 static contract test for the
 * interposing variant: links the library OBJECTS plus
 * the interpose object (src/api/mimir_interpose.c)
 * directly, so this binary's own standard-name calls
 * bind to the interposed definitions in-process.
 *
 * What it pins:
 * S1 forwarding truth: every interposed name moves the
 *    matching E-005 L1 audit counter exactly once per
 *    call -- the forwarders pass through the L1 boundary
 *    (src/api/mimir_alloc.c), never a back door.
 * S2 lifecycle through standard names only: alloc /
 *    write / read-back / grow-preserve / shrink / free,
 *    calloc zeroing, reallocarray checked product.
 * S3 the D-013 contract under standard names: malloc(0)
 *    and zero-factor calloc exactly NULL (postures, not
 *    failures); realloc(p, 0) and reallocarray zero
 *    factor preserve the live block byte-for-byte;
 *    posix_memalign zero size is ENOMEM with *memptr
 *    unchanged; errno sentinel untouched on every
 *    checked refusal.
 * S4 aligned surface: aligned_alloc honours the
 *    alignment, rejects a non-power-of-two with NULL;
 *    posix_memalign EINVAL rows leave *memptr unchanged.
 * S5 usable size / trim standard names: malloc_usable_
 *    size answers the D-012 exact extent and 0 for
 *    NULL; malloc_trim(0) returns exactly 0 or 1 and
 *    the allocator stays fully usable after it.
 * S6 cfree retires a live block exactly as free does.
 *
 * Exit 0 iff every check passes. Standalone via
 * `make test-z082`; NOT part of `make test`. */
#include "mimir_alloc.h"
#include "mimir_audit_e005.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Standard names under test (declared locally, as in
 * the interpose TU, so the test does not depend on
 * feature-test macros for the legacy names). */
void *malloc(size_t size);
void free(void *ptr);
void cfree(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *reallocarray(void *ptr, size_t nmemb, size_t size);
void *aligned_alloc(size_t alignment, size_t size);
int posix_memalign(void **memptr, size_t alignment,
                   size_t size);
size_t malloc_usable_size(void *ptr);
int malloc_trim(size_t pad);

static int checks = 0;
static int fails = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks++;                                                       \
        if (!(cond)) {                                                  \
            fails++;                                                    \
            printf("  [FAIL] %s (line %d)\n", #cond, __LINE__);         \
        }                                                               \
    } while (0)

#define SENTINEL 77

int main(void)
{
    unsigned long m0, c0, r0, f0, ra0, aa0, pm0, us0, tr0;
    unsigned char *p, *q;
    void *mv = (void *)0x1;
    size_t i;
    int rc;

    /* ---- S1 forwarding truth (E-005 counters) ---- */
    m0 = mimir_audit_call_malloc;
    c0 = mimir_audit_call_calloc;
    r0 = mimir_audit_call_realloc;
    f0 = mimir_audit_call_free;
    ra0 = mimir_audit_call_reallocarray;
    aa0 = mimir_audit_call_aligned_alloc;
    pm0 = mimir_audit_call_posix_memalign;
    us0 = mimir_audit_call_usable_size;
    tr0 = mimir_audit_call_trim;

    p = malloc(64);
    CHECK(p != NULL);
    q = calloc(8, 16);
    CHECK(q != NULL);
    p = realloc(p, 128);
    CHECK(p != NULL);
    q = reallocarray(q, 4, 16);
    CHECK(q != NULL);
    CHECK(malloc_usable_size(p) >= 128);
    {
        void *a = aligned_alloc(64, 100);
        CHECK(a != NULL);
        CHECK(((uintptr_t)a % 64) == 0);
        free(a);
    }
    rc = posix_memalign(&mv, 32, 200);
    CHECK(rc == 0);
    CHECK(mv != (void *)0x1);
    CHECK(((uintptr_t)mv % 32) == 0);
    free(mv);
    rc = malloc_trim(0);
    CHECK(rc == 0 || rc == 1);
    free(p);
    free(q);

    CHECK(mimir_audit_call_malloc - m0 == 1);
    CHECK(mimir_audit_call_calloc - c0 == 1);
    CHECK(mimir_audit_call_realloc - r0 == 1);
    CHECK(mimir_audit_call_reallocarray - ra0 == 1);
    CHECK(mimir_audit_call_aligned_alloc - aa0 == 1);
    CHECK(mimir_audit_call_posix_memalign - pm0 == 1);
    CHECK(mimir_audit_call_usable_size - us0 == 1);
    CHECK(mimir_audit_call_trim - tr0 == 1);
    /* free: S1 freed a, mv, p, q = 4 calls so far. */
    CHECK(mimir_audit_call_free - f0 == 4);

    /* ---- S2 lifecycle through standard names ---- */
    p = malloc(96);
    CHECK(p != NULL);
    for (i = 0; i < 96; i++) {
        p[i] = (unsigned char)(i * 3u + 1u);
    }
    p = realloc(p, 512);
    CHECK(p != NULL);
    for (i = 0; i < 96; i++) {
        CHECK(p[i] == (unsigned char)(i * 3u + 1u));
    }
    p = realloc(p, 40);
    CHECK(p != NULL);
    for (i = 0; i < 40; i++) {
        CHECK(p[i] == (unsigned char)(i * 3u + 1u));
    }
    free(p);

    q = calloc(32, 8);
    CHECK(q != NULL);
    for (i = 0; i < 256; i++) {
        CHECK(q[i] == 0);
    }
    free(q);

    /* realloc(NULL, n) acts as malloc(n). */
    p = realloc(NULL, 48);
    CHECK(p != NULL);
    free(p);

    /* ---- S3 D-013 contract under standard names ---- */
    {
        struct mimir_stats_v1 a, b;
        CHECK(mimir_stats_snapshot(&a) == 0);
        errno = SENTINEL; CHECK(malloc(0) == NULL); CHECK(errno == SENTINEL);
        errno = SENTINEL; CHECK(calloc(0, 16) == NULL); CHECK(errno == SENTINEL);
        errno = SENTINEL; CHECK(calloc(16, 0) == NULL); CHECK(errno == SENTINEL);
        errno = SENTINEL; CHECK(realloc(NULL, 0) == NULL); CHECK(errno == SENTINEL);
        CHECK(mimir_stats_snapshot(&b) == 0);
        CHECK(b.alloc_posture_nulls - a.alloc_posture_nulls == 4);
        CHECK(b.alloc_failures - a.alloc_failures == 0);
    }

    p = malloc(96);
    CHECK(p != NULL);
    for (i = 0; i < 96; i++) {
        p[i] = (unsigned char)(i * 5u);
    }
    /* realloc(p, 0): NULL, block LIVE and intact (D-013;
     * glibc would free it -- the named divergence). */
    errno = SENTINEL; CHECK(realloc(p, 0) == NULL); CHECK(errno == SENTINEL);
    for (i = 0; i < 96; i++) {
        CHECK(p[i] == (unsigned char)(i * 5u));
    }
    CHECK(malloc_usable_size(p) >= 96);
    errno = SENTINEL; CHECK(reallocarray(p, 0, 8) == NULL); CHECK(errno == SENTINEL);
    for (i = 0; i < 96; i++) {
        CHECK(p[i] == (unsigned char)(i * 5u));
    }
    free(p); /* freeable exactly as before */

    /* Checked refusals: overflow, errno untouched. */
    errno = SENTINEL; CHECK(malloc(SIZE_MAX) == NULL); CHECK(errno == SENTINEL);
    errno = SENTINEL; CHECK(calloc(SIZE_MAX, 2) == NULL); CHECK(errno == SENTINEL);
    errno = SENTINEL; CHECK(reallocarray(NULL, SIZE_MAX, 2) == NULL); CHECK(errno == SENTINEL);
    p = malloc(24);
    CHECK(p != NULL);
    errno = SENTINEL; CHECK(realloc(p, SIZE_MAX) == NULL); CHECK(errno == SENTINEL);
    CHECK(malloc_usable_size(p) >= 24); /* original intact */
    free(p);

    /* posix-shaped zero size: ENOMEM, *memptr unchanged. */
    mv = (void *)0x1;
    errno = SENTINEL; rc = posix_memalign(&mv, 8, 0);
    CHECK(rc == ENOMEM); CHECK(errno == SENTINEL);
    CHECK(mv == (void *)0x1);

    /* ---- S4 aligned surface ---- */
    errno = SENTINEL; CHECK(aligned_alloc(24, 100) == NULL); CHECK(errno == SENTINEL);
    errno = SENTINEL; CHECK(aligned_alloc(0, 100) == NULL); CHECK(errno == SENTINEL);
    /* Size need NOT be a multiple of alignment (Z-074). */
    p = aligned_alloc(128, 100);
    CHECK(p != NULL);
    CHECK(((uintptr_t)p % 128) == 0);
    free(p);
    mv = (void *)0x1;
    rc = posix_memalign(&mv, 24, 100);
    CHECK(rc == EINVAL); CHECK(mv == (void *)0x1);
    rc = posix_memalign(NULL, 32, 100);
    CHECK(rc == EINVAL);

    /* ---- S5 usable size / trim ---- */
    CHECK(malloc_usable_size(NULL) == 0);
    p = malloc(100);
    CHECK(p != NULL);
    CHECK(malloc_usable_size(p) == mimir_usable_size(p));
    CHECK(malloc_usable_size(p) >= 100);
    free(p);
    p = malloc(64);
    CHECK(p != NULL);
    free(p);
    rc = malloc_trim(0);
    CHECK(rc == 0 || rc == 1);
    /* Allocator fully usable after trim. */
    p = malloc(64);
    CHECK(p != NULL);
    free(p);

    /* ---- S6 cfree ---- */
    f0 = mimir_audit_call_free;
    p = malloc(32);
    CHECK(p != NULL);
    cfree(p);
    CHECK(mimir_audit_call_free - f0 == 1);

    free(NULL); /* defined no-op through the interposed name */

    printf("test_z082: %d checks, %d failed\n", checks, fails);
    printf("test_z082: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
