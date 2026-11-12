/* probe_z051.c - Z-051 huge allocations (256 MiB..8 GiB).
 *
 * Two parts, on this host only, under the Z-004 protocol
 * (docs/Z13_MEASUREMENT_PROTOCOL.md): the runner pins every
 * process with taskset -c 2, discards warm-up processes,
 * interleaves legs, and headlines medians of processes.
 *
 * cost <mimir|glibc> <size> <touch>
 *   One allocation per process. Times alloc, optional
 *   full first-touch (one byte written per 4 KiB page,
 *   ascending, then read back summed as the checksum),
 *   and free separately. Reports minor-fault delta around
 *   the whole op, RSS/VmSize from /proc/self/statm at
 *   baseline / after alloc / after touch / after free,
 *   and (mimir leg) the core stats bytes_mapped delta.
 *   CSV: leg,size,touch,alloc_ns,touch_ns,free_ns,
 *        minflt_delta,rss_base,rss_alloc,rss_touch,
 *        rss_free,vm_alloc,mapped_delta,checksum,status
 *   status is OK or FAILNULL. rss/vm/mapped in bytes.
 *   touch=0 leaves checksum 0 in both legs.
 *
 * fail <case>
 *   Failure-cleanliness audit, one case per process.
 *   Prints FAILCASE lines and exits 0 iff every check in
 *   the case passes. Cases:
 *     overflow   SIZE_MAX-family malloc refusals +
 *                calloc product overflows; allocator
 *                proven live afterwards (small + huge).
 *     beyondva   malloc(2^48) on both allocators: the
 *                user VA cannot satisfy it; clean NULL,
 *                no crash, allocator live afterwards.
 *     overcommit malloc(32 GiB) and malloc(64 GiB)
 *                WITHOUT touching: records success or
 *                clean failure under this host's
 *                overcommit posture (mode 0, ratio 50);
 *                a success must show near-zero RSS
 *                growth (lazy commitment) and free
 *                cleanly. Never touched: no OOM risk.
 *     calloczero calloc(256 MiB): succeeds, fully zero
 *                (every byte scanned), timing printed;
 *                the banked core zeroes eagerly, so this
 *                also prices eager fault-in.
 *     reallocfail alloc 256 MiB, page-pattern it,
 *                realloc to SIZE_MAX must return NULL
 *                with the original live and byte-intact;
 *                then realloc-grow to 1 GiB succeeds and
 *                preserves the pattern.
 *     sequence   partial-failure sequence: A=2 GiB live,
 *                a SIZE_MAX attempt fails between two
 *                successes (B=2 GiB), stats bytes_mapped
 *                and /proc/self/maps line count tracked
 *                at every step and required to return
 *                EXACTLY to baseline after all frees and
 *                a trim (the small path is warmed before
 *                the baseline; its retained empty span is
 *                by-design retention, released by trim);
 *                a small allocation must succeed after
 *                every failure.
 *     pair       two 8 GiB blocks live at once, sparse
 *                touch, both freed, maps count and
 *                bytes_mapped back to baseline.
 */
#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "mimir_alloc.h"
#include "core/mimir_core.h"

static long long now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static long read_minflt(void)
{
    FILE *f = fopen("/proc/self/stat", "r");
    if (!f) return -1;
    char buf[8192];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    if (n == 0) return -1;
    buf[n] = '\0';
    char *rp = strrchr(buf, ')');
    if (!rp) return -1;
    /* fields after comm: field3 is token 1; minflt is field 10 */
    char *sp = strchr(rp + 2, ' '); /* skip field 3 (state) */
    if (!sp) return -1;
    char *p = sp + 1;
    long vals[7];
    for (int i = 0; i < 7; i++) {
        vals[i] = strtol(p, &p, 10);
    }
    return vals[6]; /* fields 4..10: minflt is index 6 */
}

static void read_statm(long long *vm, long long *rss)
{
    *vm = -1; *rss = -1;
    FILE *f = fopen("/proc/self/statm", "r");
    if (!f) return;
    long long total = 0, resident = 0;
    if (fscanf(f, "%lld %lld", &total, &resident) == 2) {
        long ps = sysconf(_SC_PAGESIZE);
        *vm = total * ps;
        *rss = resident * ps;
    }
    fclose(f);
}

static long maps_lines(void)
{
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return -1;
    long c = 0; int ch;
    while ((ch = fgetc(f)) != EOF) if (ch == '\n') c++;
    fclose(f);
    return c;
}

static uint64_t mapped_now(void)
{
    struct mimir_stats s;
    if (mimir_core_stats_snapshot(&s) != 0) return UINT64_MAX;
    return s.bytes_mapped;
}

static int use_mimir;
static void *xalloc(size_t n)
{
    return use_mimir ? mimir_malloc(n) : malloc(n);
}
static void xfree(void *p)
{
    if (use_mimir) mimir_free(p); else free(p);
}

/* write one byte per page, ascending; return byte sum */
static uint64_t touch_pages(unsigned char *b, size_t n)
{
    uint64_t sum = 0;
    size_t pages = n / 4096;
    for (size_t i = 0; i < pages; i++) {
        b[i * 4096] = (unsigned char)(i & 0xFFu);
    }
    for (size_t i = 0; i < pages; i++) sum += b[i * 4096];
    return sum;
}

static int mode_cost(const char *leg, size_t size, int touch)
{
    use_mimir = (strcmp(leg, "mimir") == 0);
    long long vm0, rss0, vm1, rss1, vm2, rss2, vm3, rss3;
    read_statm(&vm0, &rss0);
    uint64_t map0 = use_mimir ? mapped_now() : 0;
    long f0 = read_minflt();
    long long t0 = now_ns();
    void *p = xalloc(size);
    long long t1 = now_ns();
    if (!p) {
        printf("%s,%zu,%d,%lld,0,0,0,%lld,%lld,%lld,%lld,0,FAILNULL\n",
               leg, size, touch, t1 - t0, rss0, rss0, rss0, vm0);
        return 2;
    }
    read_statm(&vm1, &rss1);
    uint64_t sum = 0;
    long long t2 = t1;
    if (touch) {
        sum = touch_pages((unsigned char *)p, size);
        t2 = now_ns();
    }
    read_statm(&vm2, &rss2);
    long long t3 = now_ns();
    xfree(p);
    long long t4 = now_ns();
    read_statm(&vm3, &rss3);
    long f1 = read_minflt();
    uint64_t map1 = use_mimir ? mapped_now() : 0;
    long long mapped_delta = use_mimir
        ? (long long)map1 - (long long)map0 : -1;
    printf("%s,%zu,%d,%lld,%lld,%lld,%ld,%lld,%lld,%lld,%lld,%lld,%lld,%llu,OK\n",
           leg, size, touch, t1 - t0, touch ? t2 - t1 : 0LL, t4 - t3,
           f1 - f0, rss0, rss1, rss2, rss3, vm1, mapped_delta,
           (unsigned long long)sum);
    return 0;
}

static int small_works(void)
{
    void *p = mimir_malloc(64);
    if (!p) return 0;
    memset(p, 0xA5, 64);
    mimir_free(p);
    return 1;
}

static int fail_overflow(void)
{
    int ok = 1;
    void *a = mimir_malloc(SIZE_MAX);
    void *b = mimir_malloc(SIZE_MAX - 4095);
    void *c = mimir_malloc(SIZE_MAX - 4096);
    void *d = mimir_calloc((size_t)1 << 32, (size_t)1 << 32);
    void *e = mimir_calloc(SIZE_MAX, 2);
    printf("FAILCASE overflow malloc_SIZE_MAX=%s malloc_SIZE_MAX-4095=%s malloc_SIZE_MAX-4096=%s calloc_2^32x2^32=%s calloc_SIZE_MAXx2=%s\n",
           a ? "NONNULL" : "NULL", b ? "NONNULL" : "NULL",
           c ? "NONNULL" : "NULL", d ? "NONNULL" : "NULL",
           e ? "NONNULL" : "NULL");
    if (a || b || c || d || e) ok = 0;
    if (a) mimir_free(a);
    if (b) mimir_free(b);
    if (c) mimir_free(c);
    if (d) mimir_free(d);
    if (e) mimir_free(e);
    int sw = small_works();
    void *h = mimir_malloc((size_t)256 * 1024 * 1024);
    int huge = (h != NULL);
    if (h) { ((unsigned char *)h)[0] = 1; mimir_free(h); }
    printf("FAILCASE overflow small_after=%d huge256MiB_after=%d result=%s\n",
           sw, huge, (ok && sw && huge) ? "PASS" : "FAIL");
    return (ok && sw && huge) ? 0 : 1;
}

static int fail_beyondva(void)
{
    void *m = mimir_malloc((size_t)1 << 48);
    void *g = malloc((size_t)1 << 48);
    int sw = small_works();
    printf("FAILCASE beyondva mimir_2^48=%s glibc_2^48=%s small_after=%d result=%s\n",
           m ? "NONNULL" : "NULL", g ? "NONNULL" : "NULL", sw,
           (!m && !g && sw) ? "PASS" : "FAIL");
    if (m) mimir_free(m);
    if (g) free(g);
    return (!m && !g && sw) ? 0 : 1;
}

static int fail_overcommit(void)
{
    int ok = 1;
    const size_t sizes[2] = { (size_t)32 << 30, (size_t)64 << 30 };
    for (int i = 0; i < 2; i++) {
        long long vm0, rss0, vm1, rss1, vm2, rss2;
        read_statm(&vm0, &rss0);
        void *p = mimir_malloc(sizes[i]);
        read_statm(&vm1, &rss1);
        if (p) {
            /* success without touch: RSS must stay lazy */
            long long growth = rss1 - rss0;
            printf("FAILCASE overcommit size=%zu result=SUCCESS rss_growth=%lld vm_growth=%lld\n",
                   sizes[i], growth, vm1 - vm0);
            if (growth > (long long)64 * 1024 * 1024) ok = 0;
            mimir_free(p);
            read_statm(&vm2, &rss2);
            printf("FAILCASE overcommit size=%zu after_free vm_delta=%lld rss_delta=%lld\n",
                   sizes[i], vm2 - vm0, rss2 - rss0);
            if (vm2 - vm0 > (long long)16 * 1024 * 1024) ok = 0;
        } else {
            printf("FAILCASE overcommit size=%zu result=CLEAN_FAILURE\n",
                   sizes[i]);
        }
        if (!small_works()) ok = 0;
    }
    printf("FAILCASE overcommit result=%s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

static int fail_calloczero(void)
{
    size_t n = (size_t)256 * 1024 * 1024;
    long long t0 = now_ns();
    unsigned char *p = mimir_calloc(1, n);
    long long t1 = now_ns();
    if (!p) {
        printf("FAILCASE calloczero alloc=NULL result=FAIL\n");
        return 1;
    }
    uint64_t sum = 0;
    for (size_t i = 0; i < n; i++) sum += p[i];
    long long t2 = now_ns();
    mimir_free(p);
    printf("FAILCASE calloczero alloc_ns=%lld scan_ns=%lld byte_sum=%llu result=%s\n",
           t1 - t0, t2 - t1, (unsigned long long)sum,
           sum == 0 ? "PASS" : "FAIL");
    return sum == 0 ? 0 : 1;
}

static int pattern_ok(unsigned char *b, size_t n)
{
    size_t pages = n / 4096;
    for (size_t i = 0; i < pages; i++)
        if (b[i * 4096] != (unsigned char)(i & 0xFFu)) return 0;
    return 1;
}

static int fail_realloc(void)
{
    size_t n0 = (size_t)256 * 1024 * 1024;
    unsigned char *p = mimir_malloc(n0);
    if (!p) { printf("FAILCASE reallocfail alloc=NULL result=FAIL\n"); return 1; }
    touch_pages(p, n0);
    uint64_t m0 = mapped_now();
    void *r = mimir_realloc(p, SIZE_MAX);
    int intact = (r == NULL) && pattern_ok(p, n0);
    uint64_t m1 = mapped_now();
    printf("FAILCASE reallocfail realloc_SIZE_MAX=%s pattern_intact=%d mapped_unchanged=%d\n",
           r ? "NONNULL" : "NULL", intact, m0 == m1);
    if (r) { mimir_free(r); printf("FAILCASE realloc result=FAIL\n"); return 1; }
    unsigned char *q = mimir_realloc(p, (size_t)1 << 30);
    if (!q) { mimir_free(p); printf("FAILCASE reallocgrow grow=NULL result=FAIL\n"); return 1; }
    int kept = pattern_ok(q, n0);
    touch_pages(q, (size_t)1 << 30);
    mimir_free(q);
    int ok = intact && kept && (m0 == m1) && small_works();
    printf("FAILCASE reallocgrow preserved=%d result=%s\n", kept,
           ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

static int fail_sequence(void)
{
    /* Warm the small path BEFORE the baseline: its first
     * allocation creates a span that the core RETAINS when
     * emptied (by-design bounded retention, G-007). With
     * the warm-up done first, any residue in the huge
     * path itself is measured exactly. */
    if (!small_works()) return 1;
    /* Release the warmed span too: the baseline then
     * holds NO retained span, so the residue measured
     * at the end is exactly what this sequence retained. */
    (void)mimir_core_trim_retained();
    long maps0 = maps_lines();
    uint64_t m0 = mapped_now();
    int ok = 1;
    void *A = mimir_malloc((size_t)2 << 30);
    if (!A) ok = 0; else ((unsigned char *)A)[0] = 7;
    void *F = mimir_malloc(SIZE_MAX);
    if (F) ok = 0;
    if (!small_works()) ok = 0;
    void *B = mimir_malloc((size_t)2 << 30);
    if (!B) ok = 0; else ((unsigned char *)B)[0] = 9;
    uint64_t mLive = mapped_now();
    long long expect = (long long)((size_t)2 << 30) * 2 + 8192;
    printf("FAILCASE sequence live_mapped_delta=%lld expected=%lld fail_between=%s small_after_fail=%d\n",
           (long long)mLive - (long long)m0, expect,
           F ? "NONNULL" : "NULL", small_works());
    if (llabs((long long)mLive - (long long)m0 - expect) > 4096) ok = 0;
    if (A) mimir_free(A);
    void *C = mimir_malloc((size_t)1 << 30);
    if (!C) ok = 0; else ((unsigned char *)C)[0] = 3;
    if (B) mimir_free(B);
    if (C) mimir_free(C);
    uint64_t mPre = mapped_now();
    size_t trimmed = mimir_core_trim_retained();
    uint64_t mEnd = mapped_now();
    long maps1 = maps_lines();
    int clean = (mEnd == m0) && (maps1 == maps0) && small_works();
    printf("FAILCASE sequence retained_before_trim=%lld trimmed_spans=%zu mapped_restored=%d maps_restored=%d (%ld->%ld) result=%s\n",
           (long long)mPre - (long long)m0, trimmed,
           mEnd == m0, maps1 == maps0, maps0, maps1,
           (ok && clean) ? "PASS" : "FAIL");
    return (ok && clean) ? 0 : 1;
}

static int fail_pair(void)
{
    long maps0 = maps_lines();
    uint64_t m0 = mapped_now();
    void *A = mimir_malloc((size_t)8 << 30);
    void *B = mimir_malloc((size_t)8 << 30);
    int ok = (A != NULL) && (B != NULL);
    if (A) ((unsigned char *)A)[0] = 1;
    if (B) { ((unsigned char *)B)[0] = 2; ((unsigned char *)B)[((size_t)8 << 30) - 4096] = 3; }
    if (A) mimir_free(A);
    if (B) mimir_free(B);
    uint64_t mEnd = mapped_now();
    long maps1 = maps_lines();
    int clean = (mEnd == m0) && (maps1 == maps0) && small_works();
    printf("FAILCASE pair both_live=%d mapped_restored=%d maps_restored=%d result=%s\n",
           ok, mEnd == m0, maps1 == maps0,
           (ok && clean) ? "PASS" : "FAIL");
    return (ok && clean) ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc >= 5 && strcmp(argv[1], "cost") == 0)
        return mode_cost(argv[2], strtoull(argv[3], NULL, 10),
                         atoi(argv[4]));
    if (argc >= 3 && strcmp(argv[1], "fail") == 0) {
        if (strcmp(argv[2], "overflow") == 0) return fail_overflow();
        if (strcmp(argv[2], "beyondva") == 0) return fail_beyondva();
        if (strcmp(argv[2], "overcommit") == 0) return fail_overcommit();
        if (strcmp(argv[2], "calloczero") == 0) return fail_calloczero();
        if (strcmp(argv[2], "reallocfail") == 0) return fail_realloc();
        if (strcmp(argv[2], "sequence") == 0) return fail_sequence();
        if (strcmp(argv[2], "pair") == 0) return fail_pair();
    }
    fprintf(stderr, "usage: probe_z051 cost <leg> <size> <touch> | fail <case>\n");
    return 64;
}
