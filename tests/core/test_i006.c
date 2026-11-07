/*
 * test_i006.c -- evidence test for slice I-006 (Phase 9
 * fragmentation and reclaim-overhead measurement; slice
 * record: docs/PHASE_9_FRAGMENTATION_AND_RECLAIM.md;
 * definitions fixed by docs/PHASE_9_BUDGET_AND_
 * TELEMETRY_DESIGN.md Sections 3.2 / 4.1 / 6.2 (F-1..F-6)
 * BEFORE this run; trim mechanism per I-003).
 *
 * Run standalone via `make test-i006` (NOT part of
 * `make test`, whose composition is unchanged).
 *
 * The binary links the library OBJECTS (LIBOBJS), so it
 * can read the G-003 span/object counters, the E-005
 * mapping counters, the G-007 retention getter, the
 * H-001 drain and the I-003 trim surface, all
 * INC-E001-07 shape (b). At quiescent points only it:
 *
 * - builds a stated fragmented state (classes 64 / 256 /
 *   1024, exactly 3 spans each, odd-indexed blocks freed:
 *   interleaved live/free holes, no span emptied) plus two
 *   live large mappings, and measures Q-MAPPED, Q-RETAINED
 *   and Q-LIVESPAN exactly from the allocator's counters;
 * - computes F-3 (free-but-unusable bytes), the F-1(b)
 *   span-tail total and the R-EXT / R-INT ratios of those
 *   measured quantities, and F-2 for the large mappings;
 * - proves free slots cannot serve a different class
 *   (fresh span for class 128) but do serve their own
 *   (hole reuse for class 64, span_published +0);
 * - measures reclaim overhead (F-4): free-path immediate
 *   release past the retention bound, partial trim, full
 *   trim (cross-checked byte-exactly against the OS-side
 *   anonymous rw-p drop in /proc/self/maps), no-op trim,
 *   5 full-trim and 3 partial-trim timed trials, and the
 *   first-allocation-after-trim vs retained-hit latency
 *   legs (7 samples each, span_published delta as the
 *   exact path evidence). Wall times are reported as
 *   timing-never-an-oracle: printed, never asserted;
 * - measures the F-1(a) term on a 1000-byte leg exactly
 *   from the workload's own request record (allocator
 *   state cannot hold it, INC-I002-02);
 * - verifies every live block byte-identical after every
 *   reclaim step, and ends with every counter pair
 *   reconciled exactly (published == retired,
 *   alloc == free, Q-MAPPED == Q-RETAINED == 0).
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */

#define _POSIX_C_SOURCE 200809L

#include "mimir_alloc.h"
#include "mimir_small_g003.h"
#include "mimir_trim_i003.h"
#include "mimir_tune_g007.h"
#include "mimir_drain_h001.h"
#include "../abi/mimir_audit_e005.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int failures = 0;
static int checks = 0;

static void check(int condition, const char *label)
{
    checks++;
    printf("  [%s] %s\n", condition ? "ok" : "FAIL", label);
    if (!condition) {
        failures++;
    }
}

static void check_eq_size(size_t got, size_t want,
                          const char *label)
{
    checks++;
    printf("  [%s] %s (got %zu, want %zu)\n",
           got == want ? "ok" : "FAIL", label, got, want);
    if (got != want) {
        failures++;
    }
}

static void fill_pattern(void *ptr, size_t len,
                         unsigned char seed)
{
    unsigned char *p = (unsigned char *)ptr;
    size_t i;
    for (i = 0; i < len; i++) {
        p[i] = (unsigned char)(seed + (unsigned char)i);
    }
}

static int pattern_intact(const void *ptr, size_t len,
                          unsigned char seed)
{
    const unsigned char *p = (const unsigned char *)ptr;
    size_t i;
    for (i = 0; i < len; i++) {
        if (p[i] != (unsigned char)(seed + (unsigned char)i)) {
            return 0;
        }
    }
    return 1;
}

static long long now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static int cmp_ll(const void *a, const void *b)
{
    long long x = *(const long long *)a;
    long long y = *(const long long *)b;
    return (x > y) - (x < y);
}

static long long median_ll(long long *v, size_t n)
{
    qsort(v, n, sizeof(long long), cmp_ll);
    return v[n / 2];
}

/* Sum of anonymous rw-p bytes in /proc/self/maps (the
 * I-003 helper, unchanged): raw open/read into a static
 * buffer, no allocation between the two readings of a
 * trim, so the only anonymous mappings that can move
 * across the measurement are the spans the trim itself
 * releases. OS-side figure, kept separated from the
 * allocator quantities per I-002 S4. */
static char maps_buf[1 << 19];

static long maps_anon_rw_bytes(void)
{
    int fd = open("/proc/self/maps", O_RDONLY);
    ssize_t total = 0;
    char *line;
    long sum = 0;

    if (fd < 0) {
        return -1;
    }
    for (;;) {
        ssize_t n = read(fd, maps_buf + total,
                         sizeof(maps_buf) - 1 - (size_t)total);
        if (n <= 0) {
            break;
        }
        total += n;
        if ((size_t)total >= sizeof(maps_buf) - 1) {
            break;
        }
    }
    close(fd);
    maps_buf[total] = '\0';
    line = maps_buf;
    while (*line != '\0') {
        char *nl = strchr(line, '\n');
        unsigned long lo = 0, hi = 0;
        char perms[8] = {0};
        char tail[512];
        int fields;

        if (nl != NULL) {
            *nl = '\0';
        }
        tail[0] = '\0';
        fields = sscanf(line, "%lx-%lx %7s %*s %*s %*s %511s",
                        &lo, &hi, perms, tail);
        if (fields == 3 && strcmp(perms, "rw-p") == 0) {
            sum += (long)(hi - lo);
        }
        if (nl == NULL) {
            break;
        }
        line = nl + 1;
    }
    return sum;
}

static size_t page_bytes;

static void traj(const char *label)
{
    printf("  TRAJ %-28s Q-MAPPED=%zu Q-RETAINED=%zu "
           "live_spans=%lu\n", label,
           mimir_core_mapped_bytes(),
           mimir_core_retained_bytes(),
           mimir_audit_span_published -
           mimir_audit_span_retired);
}

/* The fragmented workload: static storage so the test
 * itself allocates nothing through the measured path
 * beyond the scripted calls. */
#define N64 762
#define N256 189
#define N1024 45
static void *b64[N64];
static void *b256[N256];
static void *b1024[N1024];
static void *warm[256];
static void *leg[30];

static int alloc_fill(void **arr, size_t n, size_t req,
                      unsigned char seed0)
{
    size_t i;
    for (i = 0; i < n; i++) {
        arr[i] = mimir_malloc(req);
        if (arr[i] == NULL) {
            return 0;
        }
        fill_pattern(arr[i], req,
                     (unsigned char)(seed0 + i));
    }
    return 1;
}

static int live_patterns_ok(void **arr, size_t n, size_t req,
                            unsigned char seed0, int odds_freed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (odds_freed && (i % 2) == 1) {
            continue;
        }
        if (!pattern_intact(arr[i], req,
                            (unsigned char)(seed0 + i))) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    size_t cap64, cap256, cap1024;
    size_t tail64, tail256, tail1024;
    unsigned long pub0, ret0, sa0, mp0, mr0;
    size_t mapped0, f3, tails, i;
    long long t0, t1;
    void *large1, *large2, *probe;
    long anon_before, anon_after;
    long long full_trials[5], part_trials[3];
    long long hit[7], miss[7];
    size_t r;
    int t;

    page_bytes = (size_t)sysconf(_SC_PAGESIZE);
    printf("I-006 fragmentation + reclaim-overhead probe\n");
    printf("page=%zu retention_bound=%zu\n", page_bytes,
           mimir_tune_get_retention_bound());
    check_eq_size(page_bytes, 16384, "page size 16384");
    check_eq_size(mimir_tune_get_retention_bound(), 2,
                  "retention bound is the default 2");

    cap64 = (page_bytes - 96) / 64;
    cap256 = (page_bytes - 96) / 256;
    cap1024 = (page_bytes - 96) / 1024;
    tail64 = page_bytes - 96 - cap64 * 64;
    tail256 = page_bytes - 96 - cap256 * 256;
    tail1024 = page_bytes - 96 - cap1024 * 1024;
    printf("capacities: 64->%zu (tail %zu) 256->%zu "
           "(tail %zu) 1024->%zu (tail %zu)\n",
           cap64, tail64, cap256, tail256, cap1024, tail1024);
    check_eq_size(cap64, 254, "capacity class 64");
    check_eq_size(cap256, 63, "capacity class 256");
    check_eq_size(cap1024, 15, "capacity class 1024");
    check_eq_size(tail1024, 928,
                  "F-1(b) tail class 1024 = I-002 example");

    printf("== baseline ==\n");
    traj("baseline");
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "baseline Q-MAPPED 0");
    check_eq_size(mimir_core_retained_bytes(), 0,
                  "baseline Q-RETAINED 0");

    printf("== large mappings (F-2), kept live ==\n");
    large1 = mimir_malloc(20000);
    large2 = mimir_malloc(40000);
    check(large1 != NULL && large2 != NULL,
          "large blocks allocated");
    fill_pattern(large1, 20000, 0xA1);
    fill_pattern(large2, 40000, 0xB2);
    traj("after large");
    /* mapping_length = page_round_up(page + requested):
     * 20000 -> 49152, 40000 -> 65536 (derived, I-002 F-2;
     * the Q-MAPPED reading itself is the measurement). */
    check_eq_size(mimir_core_mapped_bytes(), 49152 + 65536,
                  "Q-MAPPED = sum of mapping lengths");
    printf("  F-2: 20000 req -> overhead %zu; 40000 req "
           "-> overhead %zu; total %zu\n",
           (size_t)49152 - 20000, (size_t)65536 - 40000,
           (size_t)(49152 - 20000) + (65536 - 40000));

    printf("== fragmented state ==\n");
    pub0 = mimir_audit_span_published;
    sa0 = mimir_audit_small_alloc;
    check(alloc_fill(b64, N64, 64, 0x10), "alloc class 64");
    check(alloc_fill(b256, N256, 256, 0x40),
          "alloc class 256");
    check(alloc_fill(b1024, N1024, 1024, 0x70),
          "alloc class 1024");
    check_eq_size(mimir_audit_span_published - pub0, 9,
                  "exactly 9 spans published");
    check_eq_size(mimir_audit_small_alloc - sa0,
                  N64 + N256 + N1024, "996 objects handed out");
    for (i = 1; i < N64; i += 2) {
        mimir_free(b64[i]);
    }
    for (i = 1; i < N256; i += 2) {
        mimir_free(b256[i]);
    }
    for (i = 1; i < N1024; i += 2) {
        mimir_free(b1024[i]);
    }
    traj("fragmented");
    check_eq_size(mimir_core_retained_spans(), 0,
                  "no span emptied by the odd frees");
    check_eq_size(mimir_core_mapped_bytes(),
                  49152 + 65536 + 9 * page_bytes,
                  "fragmented Q-MAPPED exact");
    /* F-3 from the workload record: free slots per class =
     * 3 spans x capacity - live objects of the class. */
    {
        size_t live64 = 0, live256 = 0, live1024 = 0;
        for (i = 0; i < N64; i += 2) {
            live64++;
        }
        for (i = 0; i < N256; i += 2) {
            live256++;
        }
        for (i = 0; i < N1024; i += 2) {
            live1024++;
        }
        f3 = (3 * cap64 - live64) * 64 +
             (3 * cap256 - live256) * 256 +
             (3 * cap1024 - live1024) * 1024 +
             mimir_core_retained_bytes();
        tails = 3 * (tail64 + tail256 + tail1024);
        printf("  live objects: 64:%zu 256:%zu 1024:%zu\n",
               live64, live256, live1024);
        printf("  F-3 free-but-unusable = %zu B\n", f3);
        printf("  F-1(b) span tails total = %zu B\n", tails);
        printf("  R-EXT = F-3/Q-MAPPED = %.6f\n",
               (double)f3 /
               (double)mimir_core_mapped_bytes());
        printf("  R-INT = tails/Q-LIVESPAN = %.6f\n",
               (double)tails / (double)(9 * page_bytes));
        check_eq_size(f3, 70976, "F-3 exact value");
        check_eq_size(tails, 3360, "F-1(b) total exact");
        check_eq_size((mimir_audit_small_alloc -
                       mimir_audit_small_free), 499,
                      "live small objects reconcile: 499");
    }
    check(live_patterns_ok(b64, N64, 64, 0x10, 1) &&
          live_patterns_ok(b256, N256, 256, 0x40, 1) &&
          live_patterns_ok(b1024, N1024, 1024, 0x70, 1),
          "live small patterns intact in fragmented state");
    check(pattern_intact(large1, 20000, 0xA1) &&
          pattern_intact(large2, 40000, 0xB2),
          "large patterns intact in fragmented state");
    mapped0 = mimir_core_mapped_bytes();

    printf("== cross-class proof ==\n");
    pub0 = mimir_audit_span_published;
    probe = mimir_malloc(128);
    check(probe != NULL, "class 128 allocation succeeds");
    check_eq_size(mimir_audit_span_published - pub0, 1,
                  "new class acquires a fresh span despite F-3 free bytes");
    pub0 = mimir_audit_span_published;
    {
        void *reuse = mimir_malloc(64);
        check(reuse != NULL, "class 64 allocation succeeds");
        check_eq_size(mimir_audit_span_published - pub0, 0,
                      "own class reuses a hole (no new span)");
        mimir_free(reuse);
    }
    mimir_free(probe);
    check_eq_size(mimir_core_retained_spans(), 1,
                  "emptied 128 span retained");
    ret0 = mimir_audit_span_retired;
    t0 = now_ns();
    r = mimir_core_trim_retained();
    t1 = now_ns();
    printf("  TRIM frag-state full: released=%zu wall_ns=%lld "
           "per_span_ns=%lld\n", r, t1 - t0,
           r ? (t1 - t0) / (long long)r : 0);
    check_eq_size(r, 1, "trim in fragmented state releases only the retained span");
    check_eq_size(mimir_audit_span_retired - ret0, 1,
                  "one span release call");
    check_eq_size(mimir_core_mapped_bytes(), mapped0,
                  "Q-MAPPED restored after frag-state trim");
    check(live_patterns_ok(b64, N64, 64, 0x10, 1) &&
          live_patterns_ok(b256, N256, 256, 0x40, 1) &&
          live_patterns_ok(b1024, N1024, 1024, 0x70, 1) &&
          pattern_intact(large1, 20000, 0xA1) &&
          pattern_intact(large2, 40000, 0xB2),
          "all live blocks byte-identical after frag-state trim");

    printf("== free-path reclaim (bound 2 per class) ==\n");
    ret0 = mimir_audit_span_retired;
    for (i = 0; i < N64; i += 2) {
        mimir_free(b64[i]);
    }
    for (i = 0; i < N256; i += 2) {
        mimir_free(b256[i]);
    }
    for (i = 0; i < N1024; i += 2) {
        mimir_free(b1024[i]);
    }
    traj("after free-all small");
    check_eq_size(mimir_audit_span_retired - ret0, 3,
                  "free path itself released 3 spans (3rd empty span per class past bound 2)");
    check_eq_size(mimir_core_retained_spans(), 6,
                  "6 retained spans (2 per class x 3 classes)");
    check_eq_size(mimir_core_retained_bytes(), 6 * page_bytes,
                  "Q-RETAINED = 98304");
    check(pattern_intact(large1, 20000, 0xA1) &&
          pattern_intact(large2, 40000, 0xB2),
          "large patterns intact after free-all");

    printf("== partial trim ==\n");
    ret0 = mimir_audit_span_retired;
    t0 = now_ns();
    r = mimir_core_trim_retained_some(2);
    t1 = now_ns();
    printf("  TRIM partial(2): released=%zu wall_ns=%lld "
           "per_span_ns=%lld\n", r, t1 - t0,
           r ? (t1 - t0) / (long long)r : 0);
    check_eq_size(r, 2, "partial trim releases exactly 2");
    check_eq_size(mimir_audit_span_retired - ret0, 2,
                  "two span release calls");
    check_eq_size(mimir_core_retained_spans(), 4,
                  "4 retained spans remain");
    traj("after partial trim");

    printf("== full trim (OS cross-check) ==\n");
    anon_before = maps_anon_rw_bytes();
    ret0 = mimir_audit_span_retired;
    t0 = now_ns();
    r = mimir_core_trim_retained();
    t1 = now_ns();
    anon_after = maps_anon_rw_bytes();
    printf("  TRIM full: released=%zu wall_ns=%lld "
           "per_span_ns=%lld anon_drop=%ld\n", r, t1 - t0,
           r ? (t1 - t0) / (long long)r : 0,
           anon_before - anon_after);
    check_eq_size(r, 4, "full trim releases exactly 4");
    check_eq_size(mimir_audit_span_retired - ret0, 4,
                  "four span release calls");
    check_eq_size(mimir_core_retained_bytes(), 0,
                  "Q-RETAINED 0 after full trim");
    check_eq_size(mimir_core_mapped_bytes(), 49152 + 65536,
                  "Q-MAPPED back to large-only");
    check(anon_before >= 0 && anon_after >= 0 &&
          anon_before - anon_after == (long)(4 * page_bytes),
          "OS anonymous rw-p drop == 4 pages exactly");
    check(pattern_intact(large1, 20000, 0xA1) &&
          pattern_intact(large2, 40000, 0xB2),
          "large patterns intact after full trim");
    traj("after full trim");

    printf("== no-op trim ==\n");
    t0 = now_ns();
    r = mimir_core_trim_retained();
    t1 = now_ns();
    printf("  TRIM noop full: released=%zu wall_ns=%lld\n",
           r, t1 - t0);
    check_eq_size(r, 0, "trim on empty cache is a no-op");
    t0 = now_ns();
    r = mimir_core_trim_retained_some(3);
    t1 = now_ns();
    printf("  TRIM noop partial(3): released=%zu wall_ns=%lld\n",
           r, t1 - t0);
    check_eq_size(r, 0, "partial trim on empty cache is a no-op");

    printf("== trim overhead trials (class 256, 2 spans) ==\n");
    for (t = 0; t < 5; t++) {
        check(alloc_fill(warm, 126, 256, 0x21),
              "trial warm alloc");
        for (i = 0; i < 126; i++) {
            mimir_free(warm[i]);
        }
        check_eq_size(mimir_core_retained_spans(), 2,
                      "trial: 2 retained spans warmed");
        t0 = now_ns();
        r = mimir_core_trim_retained();
        t1 = now_ns();
        full_trials[t] = t1 - t0;
        printf("  TRIAL full #%d: released=%zu wall_ns=%lld "
               "per_span_ns=%lld\n", t, r, t1 - t0,
               r ? (t1 - t0) / (long long)r : 0);
        check_eq_size(r, 2, "trial full trim releases 2");
    }
    printf("  full-trim trials median wall_ns=%lld "
           "(timing-never-an-oracle)\n",
           median_ll(full_trials, 5));
    for (t = 0; t < 3; t++) {
        check(alloc_fill(warm, 126, 256, 0x31),
              "partial trial warm alloc");
        for (i = 0; i < 126; i++) {
            mimir_free(warm[i]);
        }
        t0 = now_ns();
        r = mimir_core_trim_retained_some(1);
        t1 = now_ns();
        part_trials[t] = t1 - t0;
        printf("  TRIAL partial #%d: released=%zu wall_ns=%lld\n",
               t, r, t1 - t0);
        check_eq_size(r, 1, "partial trial releases 1");
        check_eq_size(mimir_core_trim_retained(), 1,
                      "partial trial cleanup trim releases 1");
    }
    printf("  partial-trim trials median wall_ns=%lld "
           "(timing-never-an-oracle)\n",
           median_ll(part_trials, 3));
    check_eq_size(mimir_core_mapped_bytes(), 49152 + 65536,
                  "Q-MAPPED large-only after trials");

    printf("== F-4 follow-on cost: hit vs miss (class 512) ==\n");
    probe = mimir_malloc(512);
    check(probe != NULL, "class 512 warm alloc");
    mimir_free(probe);
    check_eq_size(mimir_core_retained_spans(), 1,
                  "one retained 512 span for hit leg");
    for (t = 0; t < 7; t++) {
        pub0 = mimir_audit_span_published;
        t0 = now_ns();
        probe = mimir_malloc(512);
        t1 = now_ns();
        hit[t] = t1 - t0;
        check(probe != NULL, "hit alloc succeeds");
        check_eq_size(mimir_audit_span_published - pub0, 0,
                      "hit sample: no span acquired");
        mimir_free(probe);
    }
    for (t = 0; t < 7; t++) {
        check_eq_size(mimir_core_trim_retained(), 1,
                      "miss setup trim releases the retained span");
        pub0 = mimir_audit_span_published;
        t0 = now_ns();
        probe = mimir_malloc(512);
        t1 = now_ns();
        miss[t] = t1 - t0;
        check(probe != NULL, "miss alloc succeeds");
        check_eq_size(mimir_audit_span_published - pub0, 1,
                      "miss sample: exactly one fresh span acquired");
        mimir_free(probe);
    }
    check_eq_size(mimir_core_trim_retained(), 1,
                  "miss leg cleanup trim");
    printf("  HIT  samples ns:");
    for (t = 0; t < 7; t++) {
        printf(" %lld", hit[t]);
    }
    printf("\n  MISS samples ns:");
    for (t = 0; t < 7; t++) {
        printf(" %lld", miss[t]);
    }
    printf("\n  HIT median=%lld MISS median=%lld "
           "(timing-never-an-oracle)\n",
           median_ll(hit, 7), median_ll(miss, 7));

    printf("== F-1(a) leg: 30 x 1000 B (class 1024) ==\n");
    pub0 = mimir_audit_span_published;
    check(alloc_fill(leg, 30, 1000, 0x51), "leg alloc");
    check_eq_size(mimir_audit_span_published - pub0, 2,
                  "leg spans exactly 2");
    for (i = 1; i < 30; i += 2) {
        mimir_free(leg[i]);
    }
    printf("  F-1(a) live term: 15 live x (1024-1000) = %zu B "
           "(workload-record exact; allocator state cannot "
           "hold it, INC-I002-02)\n", (size_t)(15 * 24));
    check(live_patterns_ok(leg, 30, 1000, 0x51, 1),
          "leg live patterns intact");
    for (i = 0; i < 30; i += 2) {
        mimir_free(leg[i]);
    }
    check_eq_size(mimir_core_trim_retained(), 2,
                  "leg cleanup trim releases 2");
    traj("after F-1(a) leg");

    printf("== final reconciliation ==\n");
    mimir_free(large1);
    mimir_free(large2);
    check_eq_size(mimir_core_trim_retained(), 0,
                  "final trim no-op");
    check_eq_size(mimir_core_drain_retained(), 0,
                  "final drain no-op (H-001 unregressed)");
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "final Q-MAPPED 0");
    check_eq_size(mimir_core_retained_bytes(), 0,
                  "final Q-RETAINED 0");
    check(mimir_audit_span_published ==
          mimir_audit_span_retired,
          "span counters reconcile (published == retired)");
    check(mimir_audit_small_alloc == mimir_audit_small_free,
          "object counters reconcile (alloc == free)");
    check(mimir_audit_alloc_published ==
          mimir_audit_mapping_retired,
          "mapping counters reconcile (published == retired)");
    mp0 = mimir_audit_alloc_published;
    mr0 = mimir_audit_mapping_retired;
    (void)mp0;
    (void)mr0;
    printf("  totals: span_published=%lu span_retired=%lu "
           "small_alloc=%lu small_free=%lu "
           "map_published=%lu map_retired=%lu\n",
           mimir_audit_span_published,
           mimir_audit_span_retired,
           mimir_audit_small_alloc, mimir_audit_small_free,
           mimir_audit_alloc_published,
           mimir_audit_mapping_retired);
    printf("checks=%d failures=%d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
