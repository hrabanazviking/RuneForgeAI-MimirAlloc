/* test_z068.c -- Z-068 treatment evidence for the two remaining
 * INC-J006 silent classes on this host: desc-owner-scratch
 * (INC-J006-03) and span-class-consist (INC-J006-05).
 * Standalone (the test-j006 / test-z066 precedent): NOT part of
 * make test. Each leg corrupts live metadata in place in this
 * disposable process, exercises the operational paths, restores,
 * and prints a verdict line. Exit 0 iff every leg's observed
 * behaviour matches the recorded Z-068 standing. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mimir_alloc.h"
#include "mimir_stats_j002.h"
#include "mimir_integrity_j006.h"

static size_t PAGE;
static uint64_t rd64(const void *b, size_t o)
{ uint64_t v; memcpy(&v, (const char *)b + o, 8); return v; }
static void wr64(void *b, size_t o, uint64_t v)
{ memcpy((char *)b + o, &v, 8); }
static void wr32(void *b, size_t o, uint32_t v)
{ memcpy((char *)b + o, &v, 4); }
static void *desc_of(const void *user)
{ return (void *)((const char *)user - PAGE); }
static void *span_of(const void *p)
{ return (void *)((uintptr_t)p & ~(uintptr_t)(PAGE - 1)); }
static int snap(struct mimir_stats *s)
{ return mimir_core_stats_snapshot(s); }

static int fails;
#define CHK(cond, msg) do { if (!(cond)) { fails++; \
    printf("  [FAIL] %s:%d: %s\n", __func__, __LINE__, msg); } \
    else { printf("  [ok] %s\n", msg); } fflush(stdout); } while (0)

/* Leg A: owner-thread scratch (reserve page offset 96, NOT in
 * the descriptor). Claim under test: operationally SILENT and
 * statistics-only -- the free succeeds, memory is unaffected,
 * the integrity walk does not flag it, and the sole effect is
 * cross_thread_frees falsely +1 on a same-thread free. */
static void leg_owner_scratch(void)
{
    struct mimir_stats s0, s1;
    void *b, *d, *c;
    printf("leg A: desc-owner-scratch (INC-J006-03)\n");
    CHK(snap(&s0) == 0, "baseline snapshot");
    b = mimir_malloc(4096);
    CHK(b != NULL, "malloc(4096) on the mapping path");
    if (!b) return;
    d = desc_of(b);
    memset(b, 0x5A, 4096);
    memset((char *)d + 96, 0xFF, 8); /* J-002 owner scratch */
    CHK(mimir_core_metadata_check() == 0,
        "integrity walk does NOT flag the scratch (outside the descriptor)");
    mimir_free(b);
    CHK(snap(&s1) == 0 && s1.live_mappings == s0.live_mappings,
        "free succeeds, mapping retired normally");
    CHK(s1.cross_thread_frees == s0.cross_thread_frees + 1,
        "sole effect: cross_thread_frees falsely +1 (statistics lie)");
    c = mimir_malloc(4096);
    CHK(c != NULL, "allocator healthy afterwards");
    if (c) { memset(c, 0xA5, 4096);
        CHK(((unsigned char *)c)[4095] == 0xA5, "memory unaffected");
        mimir_free(c); }
    CHK(mimir_core_metadata_check() == 0, "walk clean at leg end");
    printf("  verdict A: SILENT, statistics-only (undetectable by design:\n"
           "  the scratch is a diagnostics-grade record no allocator\n"
           "  decision reads; see recorded run S3)\n");
}

/* Leg B: span class_index + class_size corrupted consistently
 * (INC-J006-05). Claim under test: the operational free path is
 * blind (the agreement check passes by construction; a block is
 * freed under the false class with no refusal), and the ONLY
 * detection surface is the integrity walk's class_active
 * cross-check. */
static void leg_class_consist(void)
{
    struct mimir_stats s;
    void *a, *b, *S;
    printf("leg B: span-class-consist (INC-J006-05)\n");
    a = mimir_malloc(32); b = mimir_malloc(32);
    CHK(a && b, "two 32-byte blocks");
    if (!a || !b) return;
    S = span_of(a);
    CHK(span_of(b) == S && (char *)b == (char *)a + 32,
        "same span, stride 32");
    CHK(snap(&s) == 0 && s.live_small_objects == 2, "live == 2");
    wr32(S, 52, 3);   /* class_index of 64 */
    wr64(S, 32, 64);  /* class_size, consistent */
    CHK(rd64(S, 32) == 64, "corruption in place");
    CHK(mimir_core_metadata_check() > 0,
        "integrity walk flags it (class_active cross-check) -- walk-only detection");
    mimir_free(a);    /* offset 0 still a valid 64-stride start */
    CHK(snap(&s) == 0 && s.live_small_objects == 1,
        "free of a ACCEPTED under the false class (no operational refusal)");
    wr32(S, 52, 1); wr64(S, 32, 32); /* restore */
    mimir_free(b);
    CHK(snap(&s) == 0 && s.live_small_objects == 0,
        "restore + free of b clean, live == 0");
    CHK(mimir_core_metadata_check() == 0, "walk clean at leg end");
    printf("  verdict B: operationally SILENT (undetectable by design on\n"
           "  the free path: header-only trust, agreement passes by\n"
           "  construction; compensating control = the walk, S3)\n");
}

int main(void)
{
    long ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) { printf("test_z068: no page size\n"); return 1; }
    PAGE = (size_t)ps;
    printf("test_z068: Z-068 remaining silent classes, page=%zu\n", PAGE);
    leg_owner_scratch();
    leg_class_consist();
    printf("test_z068: fails=%d\n", fails);
    return fails == 0 ? 0 : 1;
}
