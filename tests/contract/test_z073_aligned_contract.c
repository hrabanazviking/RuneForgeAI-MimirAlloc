/*
 * test_z073_aligned_contract.c -- Z-073 aligned-allocation
 * contract suite.
 *
 * Contract-before-code (docs/Z13_ALIGNED_ALLOCATION_
 * CONTRACT.md): this suite is written against the Z-073
 * contract for mimir_aligned_alloc and mimir_posix_memalign
 * BEFORE either exists. It is EXPECTED RED on the current
 * (banked Z-001..Z-070) core and GREEN on the Z-074
 * implementation, without modification. Standalone: NOT
 * part of `make test` (see tests/contract/run_z073.sh);
 * the standing gate counts do not move because of it.
 *
 * RED mechanism (honest, per-clause, Z-028 precedent): the
 * Z-074 surface (tests/contract/mimir_aligned_z073.h) is
 * referenced through WEAK symbols. On the old core the
 * symbols are absent, so each surface-dependent phase
 * records its planned checks as failed under a "surface
 * absent" label instead of crashing or failing to link;
 * behaviour that only needs the existing public API (P7)
 * runs for real on every core and must pass on both. On a
 * core that provides the surface, every phase executes its
 * full body. Exit 0 and "RESULT: PASS" only if every check
 * passed.
 *
 * Check accounting (fixed; the Z-074 green run must match
 * these totals exactly):
 *   P0 presence            3
 *   P1 aligned alignment  38   (13 valid x2 + 12 invalid)
 *   P2 posix domain       35   (6 valid x2 + 11 invalid x2
 *                               + 1 NULL-memptr)
 *   P3 size domain        35   (12 sizes x2 + 4 no-multiple
 *                               + 3 zero interim + 4 oversize)
 *   P4 lifecycle           5
 *   P5 distinctness        5
 *   P6 large alignment     4
 *   P7 public regression   5   (runs on every core)
 *   TOTAL                130
 * On the pre-Z-074 core: 130 run, 125 failed (P7 passes).
 *
 * Determinism: no timing, no sleeps, no randomness. Every
 * count is a fixed function of the contract domains.
 */
#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "mimir_alloc.h"
#include "mimir_aligned_z073.h"

/* Weak references to the Z-074 surface (see header note). */
extern void *mimir_aligned_alloc(size_t alignment, size_t size) __attribute__((weak));
extern int mimir_posix_memalign(void **memptr, size_t alignment, size_t size) __attribute__((weak));

static long g_checks = 0;
static long g_failures = 0;

static void check(int ok, const char *label)
{
    g_checks++;
    if (!ok) {
        g_failures++;
        printf("FAIL: %s\n", label);
    }
}

/* Record n planned checks as failed (surface absent). */
static void fail_planned(long n, const char *phase)
{
    long i;
    for (i = 0; i < n; i++) {
        g_checks++;
        g_failures++;
    }
    printf("FAIL: %s: %ld planned checks failed (aligned surface absent)\n", phase, n);
}

static int surface_present(void)
{
    return mimir_aligned_alloc != NULL && mimir_posix_memalign != NULL;
}

static int aligned_ok(const void *p, size_t alignment)
{
    size_t eff = alignment < 16 ? 16 : alignment;
    return p != NULL && ((uintptr_t)p % eff) == 0 && ((uintptr_t)p % alignment) == 0;
}

/* P0: presence - 3 checks. */
static void p0_presence(void)
{
    check(mimir_aligned_alloc != NULL, "P0 mimir_aligned_alloc symbol present");
    check(mimir_posix_memalign != NULL, "P0 mimir_posix_memalign symbol present");
    check(surface_present(), "P0 full aligned surface present");
}

/* P1: mimir_aligned_alloc alignment domain - 38 checks. */
static void p1_alignment_domain(void)
{
    static const size_t valid[] = {1,2,4,8,16,32,64,128,256,512,1024,2048,4096};
    static const size_t invalid[] = {0,3,5,6,7,9,12,24,48,100,1000,4095};
    size_t i;
    if (!surface_present()) { fail_planned(38, "P1 alignment domain"); return; }
    for (i = 0; i < sizeof(valid)/sizeof(valid[0]); i++) {
        void *p = mimir_aligned_alloc(valid[i], 64);
        check(p != NULL, "P1 valid alignment returns non-null");
        check(aligned_ok(p, valid[i]), "P1 valid alignment honoured (+16 floor)");
        if (p) mimir_free(p);
    }
    for (i = 0; i < sizeof(invalid)/sizeof(invalid[0]); i++) {
        void *p = mimir_aligned_alloc(invalid[i], 64);
        check(p == NULL, "P1 invalid alignment returns NULL");
        if (p) mimir_free(p);
    }
}

/* P2: mimir_posix_memalign domain - 35 checks. */
static void p2_posix_domain(void)
{
    static const size_t valid[] = {8,16,32,64,256,4096};
    static const size_t invalid[] = {0,1,2,3,4,5,6,7,12,24,100};
    size_t i;
    if (!surface_present()) { fail_planned(35, "P2 posix domain"); return; }
    for (i = 0; i < sizeof(valid)/sizeof(valid[0]); i++) {
        void *p = NULL;
        int rc = mimir_posix_memalign(&p, valid[i], 64);
        check(rc == 0, "P2 valid alignment returns 0");
        check(aligned_ok(p, valid[i]), "P2 valid alignment pointer honoured");
        if (rc == 0 && p) mimir_free(p);
    }
    for (i = 0; i < sizeof(invalid)/sizeof(invalid[0]); i++) {
        void *p = (void *)(uintptr_t)0x1;
        int rc = mimir_posix_memalign(&p, invalid[i], 64);
        check(rc == EINVAL, "P2 invalid alignment returns EINVAL");
        check(p == (void *)(uintptr_t)0x1, "P2 invalid alignment leaves memptr unchanged");
    }
    check(mimir_posix_memalign(NULL, 64, 64) == EINVAL, "P2 NULL memptr returns EINVAL");
}

/* P3: size domain - 35 checks. */
static void p3_size_domain(void)
{
    static const size_t sizes[] = {1,2,15,16,17,100,1000,1024,1025,4096,65536,1048576};
    static const size_t nomult_a[] = {256,4096,1024,512};
    static const size_t nomult_s[] = {100,37,1025,511};
    size_t i;
    if (!surface_present()) { fail_planned(35, "P3 size domain"); return; }
    for (i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
        void *p = mimir_aligned_alloc(64, sizes[i]);
        check(p != NULL, "P3 size returns non-null");
        check(aligned_ok(p, 64), "P3 size pointer 64-aligned");
        if (p) { memset(p, 0xA5, sizes[i]); mimir_free(p); }
    }
    /* Decision D-Z073-2: size need NOT be a multiple of alignment. */
    for (i = 0; i < 4; i++) {
        void *p = mimir_aligned_alloc(nomult_a[i], nomult_s[i]);
        check(aligned_ok(p, nomult_a[i]), "P3 non-multiple size succeeds, aligned");
        if (p) mimir_free(p);
    }
    /* Interim zero-size posture (U-09; final closure is Z-078). */
    check(mimir_aligned_alloc(64, 0) == NULL, "P3 interim zero size returns NULL");
    {
        void *p = (void *)(uintptr_t)0x1;
        int rc = mimir_posix_memalign(&p, 64, 0);
        check(rc == ENOMEM, "P3 interim zero size returns ENOMEM");
        check(p == (void *)(uintptr_t)0x1, "P3 interim zero size leaves memptr unchanged");
    }
    /* Oversize / overflow: clean failure, never a wrap. */
    check(mimir_aligned_alloc(16, SIZE_MAX) == NULL, "P3 SIZE_MAX returns NULL");
    check(mimir_aligned_alloc(4096, SIZE_MAX - 7) == NULL, "P3 SIZE_MAX-7 at 4096 returns NULL");
    {
        void *p = (void *)(uintptr_t)0x1;
        int rc = mimir_posix_memalign(&p, 16, SIZE_MAX);
        check(rc == ENOMEM, "P3 posix SIZE_MAX returns ENOMEM");
        check(p == (void *)(uintptr_t)0x1, "P3 posix SIZE_MAX leaves memptr unchanged");
    }
}

/* P4: lifecycle interop - 5 checks. Alignment does NOT
 * survive mimir_realloc (only the 16-byte floor does):
 * the contract states this, the suite pins the floor. */
static void p4_lifecycle(void)
{
    unsigned char *p, *r;
    size_t i;
    int preserved = 1;
    if (!surface_present()) { fail_planned(5, "P4 lifecycle"); return; }
    p = (unsigned char *)mimir_aligned_alloc(128, 128);
    check(aligned_ok(p, 128), "P4 aligned block honoured");
    if (p) { memset(p, 0x5A, 128); mimir_free(p); }
    p = (unsigned char *)mimir_malloc(128);
    check(p != NULL, "P4 plain malloc works after aligned free");
    if (p) mimir_free(p);
    p = (unsigned char *)mimir_aligned_alloc(256, 200);
    if (p) { for (i = 0; i < 200; i++) p[i] = (unsigned char)(i * 7u); }
    r = (unsigned char *)mimir_realloc(p, 400);
    check(r != NULL, "P4 realloc of aligned block succeeds");
    if (r) { for (i = 0; i < 200; i++) if (r[i] != (unsigned char)(i * 7u)) preserved = 0; }
    check(preserved && r != NULL, "P4 realloc preserves first min(old,new) bytes");
    check(r != NULL && ((uintptr_t)r % 16) == 0, "P4 realloc result keeps 16-byte floor only");
    if (r) mimir_free(r);
}

/* P5: distinctness and full-extent writability - 5 checks. */
static void p5_distinct(void)
{
    void *blocks[32];
    size_t i, j;
    int all_nonnull = 1, all_aligned = 1, distinct = 1, pattern = 1;
    void *again;
    if (!surface_present()) { fail_planned(5, "P5 distinctness"); return; }
    for (i = 0; i < 32; i++) {
        blocks[i] = mimir_aligned_alloc(128, 128);
        if (!blocks[i]) all_nonnull = 0;
        else if (!aligned_ok(blocks[i], 128)) all_aligned = 0;
    }
    for (i = 0; i < 32; i++) for (j = i + 1; j < 32; j++)
        if (blocks[i] && blocks[i] == blocks[j]) distinct = 0;
    for (i = 0; i < 32; i++) if (blocks[i]) memset(blocks[i], (int)(0x30 + i), 128);
    for (i = 0; i < 32; i++) if (blocks[i]) {
        unsigned char *b = (unsigned char *)blocks[i];
        for (j = 0; j < 128; j++) if (b[j] != (unsigned char)(0x30 + i)) pattern = 0;
    }
    check(all_nonnull, "P5 32 aligned blocks all non-null");
    check(all_aligned, "P5 32 aligned blocks all 128-aligned");
    check(distinct, "P5 32 aligned blocks pairwise distinct");
    check(pattern, "P5 full requested extent writable, patterns intact");
    for (i = 0; i < 32; i++) if (blocks[i]) mimir_free(blocks[i]);
    again = mimir_aligned_alloc(128, 128);
    check(again != NULL, "P5 allocation works after freeing all");
    if (again) mimir_free(again);
}

/* P6: large alignment - 4 checks. Page alignment at a
 * large size is GUARANTEED; beyond the page, success is
 * optional but a misaligned success never is. */
static void p6_large_alignment(void)
{
    static const size_t beyond[] = {8192, 65536};
    size_t i;
    void *p;
    if (!surface_present()) { fail_planned(4, "P6 large alignment"); return; }
    p = mimir_aligned_alloc(4096, 1048576);
    check(p != NULL, "P6 page-aligned 1 MiB returns non-null");
    check(aligned_ok(p, 4096), "P6 page-aligned 1 MiB honoured");
    if (p) mimir_free(p);
    for (i = 0; i < 2; i++) {
        p = mimir_aligned_alloc(beyond[i], 8192);
        check(p == NULL || aligned_ok(p, beyond[i]), "P6 beyond-page: clean failure or honoured");
        if (p) mimir_free(p);
    }
}

/* P7: public regression - 5 checks. Runs on every core:
 * the existing four symbols keep their banked behaviour
 * whether or not the aligned surface exists. */
static void p7_public_regression(void)
{
    unsigned char *p = (unsigned char *)mimir_malloc(64);
    unsigned char *z;
    size_t i;
    int zeroed = 1;
    check(p != NULL, "P7 mimir_malloc non-null");
    check(p != NULL && ((uintptr_t)p % 16) == 0, "P7 mimir_malloc 16-aligned");
    if (p) mimir_free(p);
    z = (unsigned char *)mimir_calloc(16, 8);
    check(z != NULL, "P7 mimir_calloc non-null");
    if (z) { for (i = 0; i < 128; i++) if (z[i] != 0) zeroed = 0; }
    check(z != NULL && zeroed, "P7 mimir_calloc zeroed over requested extent");
    if (z) mimir_free(z);
    mimir_free(NULL);
    check(1, "P7 mimir_free(NULL) is a no-op (reached)");
}

int main(void)
{
    p0_presence();
    p1_alignment_domain();
    p2_posix_domain();
    p3_size_domain();
    p4_lifecycle();
    p5_distinct();
    p6_large_alignment();
    p7_public_regression();
    printf("CHECKS: %ld run, %ld failed\n", g_checks, g_failures);
    printf("RESULT: %s\n", g_failures == 0 ? "PASS" : "FAIL");
    return g_failures == 0 ? 0 : 1;
}
