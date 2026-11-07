/* test_j006.c -- J-006 metadata integrity + corrupted-
 * metadata posture evidence (docs/PHASE_10_METADATA_
 * INTEGRITY.md). EVERY probe runs in a disposable child
 * process (fork): the child builds its own allocator
 * state, corrupts a COPY of live metadata in place,
 * exercises the allocator, and reports a class code;
 * the parent only waits (with a 10 s deadline, so a
 * hang is itself a recorded outcome) and compares.
 * No corruption ever happens in the parent or in any
 * long-lived process.
 * Class codes: 0 = clean/benign, 10 = DETECTED (lookup
 * validation refusal and/or the J-006 integrity walk),
 * 42 = SILENT misbehaviour proven in the child.
 * Crash-expected probes are graded on the signal. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "mimir_alloc.h"
#include "mimir_stats_j002.h"
#include "mimir_drain_h001.h"
#include "mimir_integrity_j006.h"

#define DETECTED 10
#define SILENT   42
#define WILD ((void *)(uintptr_t)0x0000dead00000000ULL)

static size_t PAGE;
static uint64_t rd64(const void *b, size_t o)
{ uint64_t v; memcpy(&v, (const char *)b + o, 8); return v; }
static void wr64(void *b, size_t o, uint64_t v)
{ memcpy((char *)b + o, &v, 8); }
static void wr32(void *b, size_t o, uint32_t v)
{ memcpy((char *)b + o, &v, 4); }
static void wrptr(void *b, size_t o, void *v)
{ memcpy((char *)b + o, &v, sizeof v); }
/* descriptor of a large (mapping-path) block: B-006 S7.1 */
static void *desc_of(const void *user)
{ return (void *)((const char *)user - PAGE); }
/* span header of a small block: its page base (G-002 S7) */
static void *span_of(const void *p)
{ return (void *)((uintptr_t)p & ~(uintptr_t)(PAGE - 1)); }
static int snap(struct mimir_stats *s)
{ return mimir_core_stats_snapshot(s); }

#define REQ(cond) do { if (!(cond)) { \
    printf("    REQ-FAIL %s:%d: %s\n", __func__, __LINE__, #cond); \
    fflush(stdout); return 90; } } while (0)

static int p_baseline(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(32);
    void *b = mimir_malloc(4096);
    void *c = mimir_calloc(10, 10);
    REQ(a && b && c);
    REQ(mimir_core_metadata_check() == 0);
    memset(a, 0xA5, 32); memset(b, 0x5A, 4096);
    REQ(((unsigned char *)a)[31] == 0xA5);
    REQ(((unsigned char *)b)[4095] == 0x5A);
    mimir_free(a); mimir_free(b); mimir_free(c);
    REQ(mimir_core_metadata_check() == 0);
    REQ(snap(&s) == 0 && s.live_mappings == 0 &&
        s.live_small_objects == 0);
    printf("    valid metadata: check=0 before/after, all freed\n");
    return 0;
}

static int p_desc_magic(void)
{
    struct mimir_stats s;
    void *b = mimir_malloc(4096); void *d;
    uint64_t m;
    REQ(b); d = desc_of(b); m = rd64(d, 0);
    wr64(d, 0, m ^ 0xFF);
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(b);
    REQ(snap(&s) == 0 && s.live_mappings == 1);
    wr64(d, 0, m); mimir_free(b);
    REQ(snap(&s) == 0 && s.live_mappings == 0);
    printf("    desc magic corrupt: free refused (leak, no crash), checker flagged, restore+free ok\n");
    return DETECTED;
}

static int p_desc_state(void)
{
    struct mimir_stats s;
    void *b = mimir_malloc(4096); void *d;
    REQ(b); d = desc_of(b);
    wr32(d, 48, 99);
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(b);
    REQ(snap(&s) == 0 && s.live_mappings == 1);
    wr32(d, 48, 1); mimir_free(b);
    REQ(snap(&s) == 0 && s.live_mappings == 0);
    printf("    desc state corrupt: free refused, checker flagged, restore+free ok\n");
    return DETECTED;
}

static int p_desc_base(void)
{
    struct mimir_stats s;
    void *b = mimir_malloc(4096); void *d;
    REQ(b); d = desc_of(b);
    wrptr(d, 8, (char *)d + 8);
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(b);
    REQ(snap(&s) == 0 && s.live_mappings == 1);
    wrptr(d, 8, d); mimir_free(b);
    REQ(snap(&s) == 0 && s.live_mappings == 0);
    printf("    desc mapping_base corrupt: candidate miss, free refused, checker flagged\n");
    return DETECTED;
}

static int p_desc_len_incons(void)
{
    struct mimir_stats s;
    void *b = mimir_malloc(4096); void *d; uint64_t L;
    REQ(b); d = desc_of(b); L = rd64(d, 16);
    wr64(d, 16, L / 2); /* usable_size now inconsistent */
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(b);
    REQ(snap(&s) == 0 && s.live_mappings == 1);
    wr64(d, 16, L); mimir_free(b);
    REQ(snap(&s) == 0 && s.live_mappings == 0);
    printf("    desc mapping_length halved alone (J-006 relation): free refused -- previously a wrong-length munmap\n");
    return DETECTED;
}

static int p_desc_len_consist(void)
{
    struct mimir_stats s0, s1;
    void *b = mimir_malloc(20000); void *d;
    REQ(b); d = desc_of(b);
    REQ(rd64(d, 16) == 3 * (uint64_t)PAGE); /* host fact */
    wr64(d, 16, 4 * (uint64_t)PAGE);        /* length grown */
    wr64(d, 40, 3 * (uint64_t)PAGE);        /* usable, consistent */
    REQ(mimir_core_metadata_check() > 0);   /* byte-sum catches it */
    REQ(snap(&s0) == 0);
    mimir_free(b); /* releases a 4-page length never mapped */
    REQ(snap(&s1) == 0 && s1.live_mappings == 0);
    REQ(s1.bytes_released - s0.bytes_released == 4 * (uint64_t)PAGE);
    printf("    desc length+usable consistently grown: free 'succeeded' and Q-RELEASED credited 4 pages for a 3-page mapping -- allocator silent, checker byte-sum flagged it first\n");
    return SILENT;
}

static int p_desc_requested(void)
{
    struct mimir_stats s;
    void *b = mimir_malloc(4096); void *d; void *r;
    REQ(b); d = desc_of(b);
    wr64(d, 32, rd64(d, 40) + 4096); /* requested > usable */
    REQ(mimir_core_metadata_check() > 0);
    r = mimir_realloc(b, 8192);
    REQ(r == NULL);
    REQ(snap(&s) == 0 && s.live_mappings == 1);
    wr64(d, 32, 4096);
    r = mimir_realloc(b, 2048);
    REQ(r == b);
    mimir_free(b);
    printf("    desc requested_size corrupt (J-006 relation): realloc refused NULL, original live; restore+shrink+free ok\n");
    return DETECTED;
}

static int p_desc_uoff(void)
{
    struct mimir_stats s;
    void *b = mimir_malloc(4096); void *d;
    REQ(b); d = desc_of(b);
    wr64(d, 24, 0); /* usable_offset */
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(b);
    REQ(snap(&s) == 0 && s.live_mappings == 1);
    wr64(d, 24, (uint64_t)PAGE); mimir_free(b);
    REQ(snap(&s) == 0 && s.live_mappings == 0);
    printf("    desc usable_offset=0: candidate miss, free refused, checker flagged\n");
    return DETECTED;
}

static int p_desc_next_cycle(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(4096);
    void *b = mimir_malloc(4096);
    void *c = mimir_malloc(4096);
    REQ(a && b && c); /* registry: c -> b -> a */
    wrptr(desc_of(b), 56, desc_of(b)); /* b next = b: cycle */
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(a); /* J-006 bounded walk: refuses, no hang */
    REQ(snap(&s) == 0 && s.live_mappings == 3);
    wrptr(desc_of(b), 56, desc_of(a));
    mimir_free(a); mimir_free(b); mimir_free(c);
    REQ(snap(&s) == 0 && s.live_mappings == 0);
    printf("    desc list_next cycle: free refused in bounded walk (parent deadline is the hang oracle), checker flagged\n");
    return DETECTED;
}

static int p_desc_next_wild(void)
{
    void *a = mimir_malloc(4096);
    void *b = mimir_malloc(4096);
    REQ(a && b);
    wrptr(desc_of(b), 56, WILD); /* head's next = unmapped */
    printf("    desc list_next wild: about to free(a); walk dereferences the wild link\n");
    fflush(stdout);
    mimir_free(a); /* expected: SIGSEGV in find_candidate */
    printf("    SURVIVED unexpectedly\n");
    return 90;
}

static int p_desc_flags(void)
{
    struct mimir_stats s;
    void *b = mimir_malloc(4096); void *d;
    REQ(b); d = desc_of(b);
    wr32(d, 52, 0xFFFFFFFFu);              /* flags */
    memset((char *)d + 72, 0xAA, 24);      /* reserved */
    REQ(mimir_core_metadata_check() > 0);  /* C1.4 zero check */
    mimir_free(b);                         /* free path ignores them */
    REQ(snap(&s) == 0 && s.live_mappings == 0);
    printf("    desc flags/reserved corrupt: no operational effect (free succeeds), checker flagged the C1.4 violation\n");
    return DETECTED;
}

static int p_desc_owner(void)
{
    struct mimir_stats s0, s1;
    void *b = mimir_malloc(4096); void *d;
    REQ(b); d = desc_of(b);
    REQ(snap(&s0) == 0);
    memset((char *)d + 96, 0xFF, 8); /* J-002 owner-thread scratch */
    mimir_free(b);
    REQ(snap(&s1) == 0);
    REQ(s1.cross_thread_frees == s0.cross_thread_frees + 1);
    printf("    owner-thread scratch corrupt: free succeeds, cross_thread_frees falsely +1 -- statistics lie, memory unaffected\n");
    return SILENT;
}

static int p_span_magic(void)
{
    struct mimir_stats s0, s1;
    void *a = mimir_malloc(32); void *S; void *b;
    uint64_t m;
    REQ(a); S = span_of(a); m = rd64(S, 0);
    REQ(snap(&s0) == 0);
    wr64(S, 0, m ^ 0xFF);
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(a);
    REQ(snap(&s1) == 0 && s1.live_small_objects == 1);
    b = mimir_malloc(32); /* corrupt span bypassed, fresh span */
    REQ(b && span_of(b) != S);
    REQ(snap(&s1) == 0 && s1.live_spans == s0.live_spans + 1);
    memset(b, 0x3C, 32);
    wr64(S, 0, m); mimir_free(a); mimir_free(b);
    REQ(mimir_core_metadata_check() == 0);
    printf("    span magic corrupt: free refused, span bypassed for new alloc (leak, no overlap), checker flagged\n");
    return DETECTED;
}

static int p_span_class_mismatch(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(32); void *S;
    REQ(a); S = span_of(a);
    wr64(S, 32, 999); /* class_size disagrees with index */
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(a);
    REQ(snap(&s) == 0 && s.live_small_objects == 1);
    wr64(S, 32, 32); mimir_free(a);
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    printf("    span class_size corrupt alone: agreement check refuses free, checker flagged\n");
    return DETECTED;
}

static int p_span_class_consist(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(32);
    void *b = mimir_malloc(32); void *S;
    REQ(a && b); S = span_of(a);
    REQ(span_of(b) == S && (char *)b == (char *)a + 32);
    wr32(S, 52, 3);   /* class_index of 64 */
    wr64(S, 32, 64);  /* class_size, consistent */
    mimir_free(a);    /* offset 0 still a valid 64-stride start */
    REQ(snap(&s) == 0 && s.live_small_objects == 1);
    mimir_free(b);    /* offset 32 is 'interior' at stride 64 */
    REQ(snap(&s) == 0 && s.live_small_objects == 1);
    REQ(mimir_core_metadata_check() > 0); /* via class_active hint */
    wr32(S, 52, 1); wr64(S, 32, 32); mimir_free(b);
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    printf("    span class index+size consistently corrupt: a freed under the false class, b misread as interior and unfreeable; operational paths blind, checker catches it only via the class_active cross-check\n");
    return SILENT;
}

static int p_span_live_zero(void)
{
    unsigned char vec = 0;
    void *a = mimir_malloc(32);
    void *b = mimir_malloc(32); void *S;
    size_t rel;
    REQ(a && b); S = span_of(a);
    wr64(S, 80, 0); /* live_count = 0 while 2 objects live */
    REQ(mimir_core_metadata_check() > 0); /* counter cross-sum */
    rel = mimir_core_drain_retained();    /* trusts live_count */
    REQ(rel >= 1);
    REQ(mincore(S, PAGE, &vec) != 0);     /* span unmapped */
    printf("    span live_count=0 with live objects: drain released the span under live blocks (mincore ENOMEM) -- checker cross-sum flagged it, no operational path did\n");
    return SILENT;
}

static int p_span_carve_reduced(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(32);
    void *b = mimir_malloc(32); void *S;
    REQ(a && b); S = span_of(a);
    wr64(S, 88, 0); /* carve_count below live_count */
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(a);
    REQ(snap(&s) == 0 && s.live_small_objects == 2);
    wr64(S, 88, 2); mimir_free(a); mimir_free(b);
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    printf("    span carve_count=0 under 2 live (J-006 relation live<=carve): free refused -- previously re-carve could re-hand live offsets\n");
    return DETECTED;
}

static int p_span_carve_inflated(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(32); void *S; uint64_t c0;
    REQ(a); S = span_of(a); c0 = rd64(S, 88);
    wr64(S, 88, 100000); /* carve_count beyond capacity */
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(a);
    REQ(snap(&s) == 0 && s.live_small_objects == 1);
    wr64(S, 88, c0); mimir_free(a);
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    printf("    span carve_count inflated past capacity (J-006 relation): free refused, checker flagged\n");
    return DETECTED;
}

static int p_span_len_incons(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(32); void *S; uint64_t L;
    REQ(a); S = span_of(a); L = rd64(S, 16);
    wr64(S, 16, L / 2); /* usable_size now inconsistent */
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(a);
    REQ(snap(&s) == 0 && s.live_small_objects == 1);
    wr64(S, 16, L); mimir_free(a);
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    printf("    span mapping_length halved alone (J-006 relation): free refused, checker flagged\n");
    return DETECTED;
}

static int p_span_len_consist(void)
{
    void *arr[1200]; (void)arr;
    void *a = mimir_malloc(16); void *S; void *p;
    int i, found = 0;
    REQ(a); S = span_of(a);
    wr64(S, 16, 2 * (uint64_t)PAGE);       /* length doubled */
    wr64(S, 40, 2 * (uint64_t)PAGE - 96);  /* usable, consistent */
    printf("    checker on consistently-doubled span length: %d violation(s) (byte-sum)\n",
           mimir_core_metadata_check());
    for (i = 0; i < 1199; i++) {
        p = mimir_malloc(16);
        REQ(p);
        arr[i] = p;
        if ((uintptr_t)p >= (uintptr_t)S + PAGE) { found = 1; break; }
    }
    REQ(found);
    printf("    span length+usable consistently doubled: allocator carved object %d at/past the real page end -- out-of-mapping handout, no write attempted\n", i);
    return SILENT;
}

static int p_span_freehead_live(void)
{
    void *a = mimir_malloc(32);
    void *b = mimir_malloc(32); void *S; void *c;
    REQ(a && b); S = span_of(a);
    mimir_free(b);                 /* free_head = b */
    wrptr(S, 72, a);               /* head now names LIVE a */
    REQ(mimir_core_metadata_check() == 0); /* shape-valid: blind */
    ((unsigned char *)a)[0] = 0x11;
    c = mimir_malloc(32);
    REQ(c == a);                   /* overlap handout */
    ((unsigned char *)c)[0] = 0x22;
    REQ(((unsigned char *)a)[0] == 0x22);
    printf("    span free_head repointed at a live object: next alloc returned the SAME live object (write via one seen via other) -- silent overlap\n");
    return SILENT;
}

static int p_span_freehead_wild(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(32);
    void *b = mimir_malloc(32); void *S; void *c;
    REQ(a && b); S = span_of(a);
    mimir_free(b);
    wrptr(S, 72, WILD);            /* free_head wild */
    c = mimir_malloc(32);          /* J-006: never follows it */
    REQ(c && span_of(c) == S && (char *)c == (char *)a + 64);
    memset(c, 0x7E, 32);
    mimir_free(a);                 /* walk validates node first */
    REQ(snap(&s) == 0 && s.live_small_objects == 2);
    REQ(mimir_core_metadata_check() > 0);
    wrptr(S, 72, b);
    mimir_free(a); mimir_free(c);
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    printf("    span free_head wild: alloc carved fresh instead of following it, free refused without deref, checker flagged -- previously a wild handout/crash\n");
    return DETECTED;
}

static int p_span_freelist_cycle(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(32);
    void *b = mimir_malloc(32);
    void *c = mimir_malloc(32);
    REQ(a && b && c); REQ(span_of(a) == span_of(c));
    mimir_free(a); mimir_free(b);  /* list: b -> a -> NULL */
    wrptr(a, 0, b);                /* a -> b: cycle */
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(c); /* J-006 bounded walk: refuses, no hang */
    REQ(snap(&s) == 0 && s.live_small_objects == 1);
    wrptr(a, 0, NULL);
    mimir_free(c);
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    printf("    in-object free-list link cycle: free refused in bounded walk, checker flagged\n");
    return DETECTED;
}

static int p_span_next_cycle(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(32); void *S1;
    void *d = mimir_malloc(64); void *S2;
    REQ(a && d); S1 = span_of(a); S2 = span_of(d);
    REQ(S1 != S2); /* registry: S2 -> S1 */
    wrptr(S2, 56, S2);             /* head next = itself */
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(a); /* J-006 bounded walk: refuses, no hang */
    wrptr(S2, 56, S1);             /* restore BEFORE snap: the */
    /* statistics walk is unbounded (INC-J006-07) */
    REQ(snap(&s) == 0 && s.live_small_objects == 2);
    mimir_free(a); mimir_free(d);
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    printf("    span list_next cycle: free refused in bounded walk, checker flagged\n");
    return DETECTED;
}

static int p_span_next_wild(void)
{
    void *a = mimir_malloc(32);
    void *d = mimir_malloc(64); void *S2;
    REQ(a && d); S2 = span_of(d);
    wrptr(S2, 56, WILD);
    printf("    span list_next wild: about to free(a); walk dereferences the wild link\n");
    fflush(stdout);
    mimir_free(a); /* expected: SIGSEGV in span walk */
    printf("    SURVIVED unexpectedly\n");
    return 90;
}

struct probe { const char *name; int (*fn)(void);
               int expect_exit; int expect_signal; };
static const struct probe PROBES[] = {
    { "baseline-valid",      p_baseline,           0,        0 },
    { "desc-magic",          p_desc_magic,         DETECTED, 0 },
    { "desc-state",          p_desc_state,         DETECTED, 0 },
    { "desc-base",           p_desc_base,          DETECTED, 0 },
    { "desc-len-incons",     p_desc_len_incons,    DETECTED, 0 },
    { "desc-len-consist",    p_desc_len_consist,   SILENT,   0 },
    { "desc-requested",      p_desc_requested,     DETECTED, 0 },
    { "desc-uoff",           p_desc_uoff,          DETECTED, 0 },
    { "desc-next-cycle",     p_desc_next_cycle,    DETECTED, 0 },
    { "desc-next-wild",      p_desc_next_wild,     -1,       SIGSEGV },
    { "desc-flags-reserved", p_desc_flags,         DETECTED, 0 },
    { "desc-owner-scratch",  p_desc_owner,         SILENT,   0 },
    { "span-magic",          p_span_magic,         DETECTED, 0 },
    { "span-class-mismatch", p_span_class_mismatch,DETECTED, 0 },
    { "span-class-consist",  p_span_class_consist, SILENT,   0 },
    { "span-live-zero",      p_span_live_zero,     SILENT,   0 },
    { "span-carve-reduced",  p_span_carve_reduced, DETECTED, 0 },
    { "span-carve-inflated", p_span_carve_inflated,DETECTED, 0 },
    { "span-len-incons",     p_span_len_incons,    DETECTED, 0 },
    { "span-len-consist",    p_span_len_consist,   SILENT,   0 },
    { "span-freehead-live",  p_span_freehead_live, SILENT,   0 },
    { "span-freehead-wild",  p_span_freehead_wild, DETECTED, 0 },
    { "span-freelist-cycle", p_span_freelist_cycle,DETECTED, 0 },
    { "span-next-cycle",     p_span_next_cycle,    DETECTED, 0 },
    { "span-next-wild",      p_span_next_wild,     -1,       SIGSEGV },
};

int main(void)
{
    size_t i, n = sizeof PROBES / sizeof PROBES[0];
    int checks = 0, failed = 0;
    long ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) { printf("test_j006: no page size\n"); return 1; }
    PAGE = (size_t)ps;
    printf("test_j006: Phase 10 metadata integrity (J-006), page=%zu, %zu probes, each in a disposable child (10 s deadline)\n", PAGE, n);
    for (i = 0; i < n; i++) {
        pid_t pid = fork();
        int status = 0, waited = 0, timed_out = 0, ok;
        if (pid < 0) { printf("  [FAIL] %s: fork failed\n", PROBES[i].name); failed++; checks++; continue; }
        if (pid == 0) {
            int rc = PROBES[i].fn();
            fflush(stdout);
            _exit(rc);
        }
        for (;;) {
            pid_t w = waitpid(pid, &status, WNOHANG);
            if (w == pid) break;
            if (w < 0) break;
            if (waited >= 100) { timed_out = 1;
                kill(pid, SIGKILL);
                (void)waitpid(pid, &status, 0); break; }
            { struct timespec ts = { 0, 100000000L };
              nanosleep(&ts, NULL); }
            waited++;
        }
        checks++;
        if (timed_out) ok = 0;
        else if (PROBES[i].expect_signal != 0)
            ok = WIFSIGNALED(status) &&
                 WTERMSIG(status) == PROBES[i].expect_signal;
        else
            ok = WIFEXITED(status) &&
                 WEXITSTATUS(status) == PROBES[i].expect_exit;
        if (!ok) failed++;
        printf("  [%s] probe %02zu %-22s exit=%d signal=%d timeout=%d (expected exit=%d signal=%d)\n",
               ok ? "ok" : "FAIL", i, PROBES[i].name,
               WIFEXITED(status) ? WEXITSTATUS(status) : -1,
               WIFSIGNALED(status) ? WTERMSIG(status) : 0,
               timed_out, PROBES[i].expect_exit,
               PROBES[i].expect_signal);
        fflush(stdout);
    }
    printf("test_j006: checks=%d failed=%d\n", checks, failed);
    return failed == 0 ? 0 : 1;
}
