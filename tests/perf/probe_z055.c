/* probe_z055.c - Z-055: the fragmentation oracle on this host.
 *
 * The Phase-9 F-series fragmentation legs (I-006;
 * docs/PHASE_9_FRAGMENTATION_AND_RECLAIM.md; definitions
 * fixed by docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md
 * Sections 3.2 / 4.1 / 6.2, F-1..F-6) ported to this host:
 * x86-64, page = 4096 B (measured, asserted as this host's
 * header), span = 1 page, 96 B header. Every expectation
 * the old test hardcoded from the 16 KiB-page host (page,
 * capacities, mapping lengths, span counts, F-3) is here
 * DERIVED from the measured page size and the stated
 * workload, so the oracle grades this host's allocator
 * against this host's arithmetic.
 *
 * Two adaptations the current core forces, both part of
 * the measurement, not workarounds:
 * - QUIESCENCE (Z-029): frees bin in the freeing thread's
 *   TLS bins and core counters do not move until a drain.
 *   I-006's quiescent points are therefore established
 *   explicitly by quiesce(): mimir_core_trim_retained_
 *   some(0) - the trim surface flushes the TLS bins and
 *   drains them to the central core (applying the free-
 *   path retention sweep) while releasing at most 0 spans
 *   itself (mimir_trim_i003.h: max == 0 releases nothing).
 *   Every snapshot and every operation delta in this probe
 *   is taken at such a point.
 * - RETENTION (Z-038): the retention bound is enforced
 *   per class at drain/sweep time (default build: global
 *   bound, mimir_retention_bound per class). The probe
 *   files the retained counts it measures at quiescence
 *   against the bound-2-per-class prediction.
 * - DISTRIBUTION (Z-029/Z-037/Z-038): allocation on the
 *   current core distributes a class's blocks across
 *   spans (TLS batch refills, arena assignment), so the
 *   I-006 construction (3 spans per class, no span
 *   emptied by the odd frees) does NOT reproduce as-is:
 *   the pilot run measured 14 spans published for the
 *   same workload, 4 spans empty at the fragmented point
 *   and 1 empty span sweep-released. The fragmented state
 *   is therefore built CLASS BY CLASS with counter deltas
 *   measured per class, and F-3 / the tails total are
 *   computed from those measured per-class span counts -
 *   exact, from allocator counters, on this host's shape.
 * - POOL (Z-048): Q-MAPPED includes pooled (dead but
 *   still mapped) large mappings; a freed large mapping
 *   stays in Q-MAPPED until a trim or drain releases the
 *   pool. The F-6 child A leg files that trajectory
 *   explicitly via mimir_core_pool_bytes().
 *
 * Legs: F-1(b) span tails + F-1(a) workload-exact leg,
 * F-2 mapping overhead, F-3 free-but-unusable + R-EXT /
 * R-INT at the stated fragmented state, the cross-class
 * proof, F-4 reclaim overhead (free-path release past the
 * bound, partial / full / no-op trims with the OS anon
 * rw-p cross-check, timed trials, hit-vs-miss follow-on
 * cost; wall times timing-never-an-oracle), F-6 this-
 * probe Q-MAPPED trajectory (TRAJ lines at every phase
 * point) AND the F-6 cross-slice leg owed by O-08 /
 * INC-I006-02: Q-MAPPED / Q-RETAINED sampled inside
 * forked RLIMIT_AS-constrained children run in I-005's
 * method (child measures its own VmSize baseline, sets
 * baseline + headroom, parent enforces a 30 s bound).
 * F-5 remains unmeasurable (no budget enforcement,
 * INC-I006-01) and is reported absent, never approximated.
 *
 * Standalone: built by tests/perf/run_z055.sh against the
 * library OBJECTS (the test_i006 link shape), NOT part of
 * make test. Exit 0 and RESULT: PASS iff every check in
 * the main process and both F-6 children passed.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1

#include "mimir_alloc.h"
#include "mimir_small_g003.h"
#include "mimir_trim_i003.h"
#include "mimir_tune_g007.h"
#include "mimir_drain_h001.h"
#include "mimir_pool_z048.h"
#include "../abi/mimir_audit_e005.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>

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

/* True quiescence under the Z-029 TLS free path: flush +
 * drain to the central core, releasing nothing itself. */
static void quiesce(void)
{
    (void)mimir_core_trim_retained_some(0);
}

static void traj(const char *label)
{
    printf("  TRAJ %-28s Q-MAPPED=%zu Q-RETAINED=%zu "
           "live_spans=%lu\n", label,
           mimir_core_mapped_bytes(),
           mimir_core_retained_bytes(),
           mimir_audit_span_published -
           mimir_audit_span_retired);
}

static size_t page_round_up(size_t x)
{
    return (x + page_bytes - 1) / page_bytes * page_bytes;
}

/* The fragmented workload: counts are set in main from
 * the measured capacities (3 spans per class). */
#define MAXN 256
static void *b64[MAXN];
static void *b256[MAXN];
static void *b1024[MAXN];
static void *warm[MAXN];
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

/* ---------------- F-6 cross-slice leg (I-005 method) --- */

static size_t vm_size_bytes(void)
{
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    size_t kb = 0;
    if (!f) {
        return 0;
    }
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "VmSize:", 7) == 0) {
            kb = (size_t)strtoul(line + 7, NULL, 10);
            break;
        }
    }
    fclose(f);
    return kb * 1024;
}

static void child_traj(const char *legname, const char *label)
{
    printf("  TRAJ F6-%s %-22s Q-MAPPED=%zu Q-RETAINED=%zu "
           "live_spans=%lu\n", legname, label,
           mimir_core_mapped_bytes(),
           mimir_core_retained_bytes(),
           mimir_audit_span_published -
           mimir_audit_span_retired);
    fflush(stdout);
}

/* Child A: large-fill under a 64-page headroom. */
static int child_a(void)
{
    void *ptrs[128];
    size_t n = 0, i;
    size_t base = vm_size_bytes();
    struct rlimit rl;
    size_t large_len;
    size_t base_mapped;

    failures = 0;
    checks = 0;
    if (base == 0) {
        printf("  CHILD A: no VmSize baseline\n");
        return 1;
    }
    rl.rlim_cur = base + 64 * page_bytes;
    rl.rlim_max = rl.rlim_cur;
    if (setrlimit(RLIMIT_AS, &rl) != 0) {
        printf("  CHILD A: setrlimit failed\n");
        return 1;
    }
    large_len = page_round_up(page_bytes + 4096);
    printf("== F-6 child A: large-fill, headroom 64 pages "
           "(baseline=%zu limit=%zu large_len=%zu) ==\n",
           base, base + 64 * page_bytes, large_len);
    quiesce();
    child_traj("A", "baseline");
    /* The child is forked from the main probe, which at
     * fork time holds its two large mappings live: the
     * child's baseline Q-MAPPED is that inherited figure,
     * measured here, and every child expectation is
     * relative to it. */
    base_mapped = mimir_core_mapped_bytes();
    printf("  F6-A inherited baseline Q-MAPPED=%zu "
           "(parent state at fork)\n", base_mapped);
    while (n < 128) {
        void *p = mimir_malloc(4096);
        if (p == NULL) {
            break;
        }
        fill_pattern(p, 4096, (unsigned char)(0x30 + n));
        ptrs[n++] = p;
    }
    printf("  F6-A large successes before NULL: %zu "
           "(headroom bound %zu)\n", n,
           (64 * page_bytes) / large_len);
    check(n >= 4, "F6-A: at least 4 large blocks served "
          "under limit");
    check(n <= (64 * page_bytes) / large_len,
          "F6-A: successes bounded by headroom arithmetic");
    {
        int ok = 1;
        for (i = 0; i < n; i++) {
            if (!pattern_intact(ptrs[i], 4096,
                                (unsigned char)(0x30 + i))) {
                ok = 0;
            }
        }
        check(ok, "F6-A: live patterns intact at the limit");
    }
    child_traj("A", "at-NULL");
    check_eq_size(mimir_core_mapped_bytes(),
                  base_mapped + n * large_len,
                  "F6-A: Q-MAPPED at limit == baseline + "
                  "successes x mapping_length");
    for (i = 0; i < n; i++) {
        mimir_free(ptrs[i]);
    }
    quiesce();
    child_traj("A", "after free-all");
    /* Z-048: freed large mappings are pooled, not
     * unmapped - they stay inside Q-MAPPED until a trim
     * or drain releases the pool. Filed as measured. */
    printf("  F6-A pool after free-all: count=%zu "
           "bytes=%zu\n", mimir_core_pool_count(),
           mimir_core_pool_bytes());
    check_eq_size(mimir_core_pool_bytes(), n * large_len,
                  "F6-A: freed mappings pooled byte-exact "
                  "(Z-048)");
    check_eq_size(mimir_core_mapped_bytes(),
                  base_mapped + n * large_len,
                  "F6-A: Q-MAPPED unchanged by free-all "
                  "(pool holds the bytes)");
    (void)mimir_core_trim_retained();
    child_traj("A", "after trim (pool released)");
    check_eq_size(mimir_core_mapped_bytes(), base_mapped,
                  "F6-A: Q-MAPPED back to baseline after "
                  "trim releases the pool");
    {
        void *p = mimir_malloc(4096);
        check(p != NULL, "F6-A: allocator serves again "
              "after freeing all");
        if (p) {
            fill_pattern(p, 4096, 0x77);
            check(pattern_intact(p, 4096, 0x77),
                  "F6-A: re-served block usable");
            mimir_free(p);
        }
    }
    quiesce();
    printf("  CHILD A checks=%d failures=%d\n", checks,
           failures);
    fflush(stdout);
    return failures ? 1 : 0;
}

/* Child D: trim under pressure, 128-page headroom. */
static int child_d(void)
{
    void *small[2048];
    void *ptrs[256];
    size_t cap64, want, n = 0, i;
    size_t base = vm_size_bytes();
    struct rlimit rl;
    size_t retained0, r;

    failures = 0;
    checks = 0;
    if (base == 0) {
        printf("  CHILD D: no VmSize baseline\n");
        return 1;
    }
    rl.rlim_cur = base + 128 * page_bytes;
    rl.rlim_max = rl.rlim_cur;
    if (setrlimit(RLIMIT_AS, &rl) != 0) {
        printf("  CHILD D: setrlimit failed\n");
        return 1;
    }
    cap64 = (page_bytes - 96) / 64;
    want = 20 * cap64;
    printf("== F-6 child D: trim under pressure, headroom "
           "128 pages (baseline=%zu limit=%zu warm=%zu) ==\n",
           base, base + 128 * page_bytes, want);
    check(alloc_fill(small, want, 64, 0x21),
          "F6-D: warm class 64 under limit");
    for (i = 0; i < want; i++) {
        mimir_free(small[i]);
    }
    quiesce();
    retained0 = mimir_core_retained_spans();
    printf("  F6-D retained spans after free-all + "
           "quiesce: %zu\n", retained0);
    check(retained0 >= 1,
          "F6-D: at least 1 retained span held under "
          "pressure setup");
    check_eq_size(mimir_core_retained_bytes(),
                  retained0 * page_bytes,
                  "F6-D: retained bytes == spans x page");
    child_traj("D", "retained-held");
    while (n < 256) {
        void *p = mimir_malloc(4096);
        if (p == NULL) {
            break;
        }
        fill_pattern(p, 4096, (unsigned char)(0x50 + n));
        ptrs[n++] = p;
    }
    printf("  F6-D large successes before NULL: %zu\n", n);
    check(n >= 1, "F6-D: clean NULL reached with retained "
          "spans held");
    child_traj("D", "at-NULL");
    r = mimir_core_trim_retained();
    printf("  F6-D trim released=%zu\n", r);
    check_eq_size(r, retained0,
                  "F6-D: trim released exactly the retained "
                  "spans");
    check_eq_size(mimir_core_retained_bytes(), 0,
                  "F6-D: retained bytes 0 after trim");
    child_traj("D", "after trim");
    {
        void *p = mimir_malloc(4096);
        check(p != NULL, "F6-D: large request served after "
              "trim freed VA under the same limit");
        if (p) {
            mimir_free(p);
        }
    }
    for (i = 0; i < n; i++) {
        mimir_free(ptrs[i]);
    }
    quiesce();
    (void)mimir_core_trim_retained();
    printf("  CHILD D checks=%d failures=%d\n", checks,
           failures);
    fflush(stdout);
    return failures ? 1 : 0;
}

static int run_child(int (*fn)(void), const char *name)
{
    pid_t pid = fork();
    int status = 0;
    long long deadline;

    if (pid < 0) {
        check(0, "F-6 fork");
        return 0;
    }
    if (pid == 0) {
        _exit(fn() ? 1 : 0);
    }
    deadline = now_ns() + 30000000000LL;
    for (;;) {
        pid_t w = waitpid(pid, &status, WNOHANG);
        if (w == pid) {
            break;
        }
        if (now_ns() > deadline) {
            kill(pid, SIGKILL);
            (void)waitpid(pid, &status, 0);
            printf("  F-6 child %s exceeded 30 s bound\n",
                   name);
            check(0, "F-6 child inside 30 s wall bound");
            return 0;
        }
        {
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 10000000L;
            nanosleep(&ts, NULL);
        }
    }
    {
        int ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        char label[128];
        snprintf(label, sizeof label,
                 "F-6 child %s RESULT PASS", name);
        check(ok, label);
        return ok;
    }
}

/* ------------------------------ main legs ------------- */

int main(void)
{
    size_t cap64, cap256, cap1024, cap512;
    size_t tail64, tail256, tail1024;
    size_t n64, n256, n1024;
    unsigned long pub0, ret0, sa0;
    size_t mapped0, f3, tails, i;
    size_t large1_len, large2_len, large_sum;
    size_t retained_fa;
    size_t retained_frag = 0;
    size_t frag_bearing = 0;
    long long t0, t1;
    void *large1, *large2, *probe;
    long anon_before, anon_after;
    long long full_trials[5], part_trials[3];
    long long hit[7], miss[7];
    size_t r;
    int t;

    page_bytes = (size_t)sysconf(_SC_PAGESIZE);
    printf("Z-055 fragmentation oracle (I-006 F-series, "
           "this host)\n");
    printf("page=%zu retention_bound=%zu\n", page_bytes,
           mimir_tune_get_retention_bound());
    check_eq_size(page_bytes, 4096,
                  "host header: page size 4096 on this host");
    check_eq_size(mimir_tune_get_retention_bound(), 2,
                  "retention bound is the default 2");

    cap64 = (page_bytes - 96) / 64;
    cap256 = (page_bytes - 96) / 256;
    cap512 = (page_bytes - 96) / 512;
    cap1024 = (page_bytes - 96) / 1024;
    tail64 = page_bytes - 96 - cap64 * 64;
    tail256 = page_bytes - 96 - cap256 * 256;
    tail1024 = page_bytes - 96 - cap1024 * 1024;
    printf("capacities: 64->%zu (tail %zu) 256->%zu "
           "(tail %zu) 512->%zu 1024->%zu (tail %zu)\n",
           cap64, tail64, cap256, tail256, cap512,
           cap1024, tail1024);
    check_eq_size(cap64, 62, "capacity class 64 (derived)");
    check_eq_size(cap256, 15, "capacity class 256 (derived)");
    check_eq_size(cap1024, 3, "capacity class 1024 (derived)");
    check_eq_size(tail1024, 928, "F-1(b) tail class 1024");

    n64 = 3 * cap64;
    n256 = 3 * cap256;
    n1024 = 3 * cap1024;

    printf("== baseline ==\n");
    quiesce();
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
    /* mapping_length = page_round_up(page + requested),
     * derived from the measured page (I-002 F-2); the
     * Q-MAPPED reading itself is the measurement. */
    large1_len = page_round_up(page_bytes + 20000);
    large2_len = page_round_up(page_bytes + 40000);
    large_sum = large1_len + large2_len;
    quiesce();
    traj("after large");
    check_eq_size(mimir_core_mapped_bytes(), large_sum,
                  "Q-MAPPED = sum of mapping lengths");
    printf("  F-2: 20000 req -> len %zu overhead %zu; "
           "40000 req -> len %zu overhead %zu; total %zu\n",
           large1_len, large1_len - 20000,
           large2_len, large2_len - 40000,
           (large1_len - 20000) + (large2_len - 40000));

    printf("== fragmented state (per class, deltas "
           "measured) ==\n");
    {
        /* Per-class phase record: spans acquired, spans
         * released by the sweep, spans retained empty,
         * spans still bearing live objects - all read from
         * counter deltas across the class's own phase, so
         * F-3 and the tails total are exact on this host's
         * distributed span shape. */
        struct phase { size_t acq, rel, ret, bear; };
        struct phase ph[3];
        void **arrs[3];
        size_t ns[3], reqs[3], caps[3], tailv[3], live[3];
        size_t bear_total = 0;
        int k;
        arrs[0] = b64; arrs[1] = b256; arrs[2] = b1024;
        ns[0] = n64; ns[1] = n256; ns[2] = n1024;
        reqs[0] = 64; reqs[1] = 256; reqs[2] = 1024;
        caps[0] = cap64; caps[1] = cap256; caps[2] = cap1024;
        tailv[0] = tail64; tailv[1] = tail256;
        tailv[2] = tail1024;
        sa0 = mimir_audit_small_alloc;
        for (k = 0; k < 3; k++) {
            unsigned long p0 = mimir_audit_span_published;
            unsigned long r0 = mimir_audit_span_retired;
            size_t ret_before = mimir_core_retained_spans();
            check(alloc_fill(arrs[k], ns[k], reqs[k],
                             (unsigned char)(0x10 + 0x30 * k)),
                  "alloc fragmented class");
            for (i = 1; i < ns[k]; i += 2) {
                mimir_free(arrs[k][i]);
            }
            quiesce();
            ph[k].acq = mimir_audit_span_published - p0;
            ph[k].rel = mimir_audit_span_retired - r0;
            ph[k].ret = mimir_core_retained_spans() -
                        ret_before;
            ph[k].bear = ph[k].acq - ph[k].rel - ph[k].ret;
            live[k] = (ns[k] + 1) / 2;
            bear_total += ph[k].bear;
            printf("  class %zu: spans acquired=%zu "
                   "sweep-released=%zu retained-empty=%zu "
                   "bearing=%zu live=%zu\n",
                   reqs[k], ph[k].acq, ph[k].rel, ph[k].ret,
                   ph[k].bear, live[k]);
        }
        check_eq_size(mimir_audit_small_alloc - sa0,
                      n64 + n256 + n1024,
                      "objects handed out (workload record)");
        traj("fragmented");
        retained_frag = mimir_core_retained_spans();
        {
            size_t span_total = bear_total + retained_frag;
            check_eq_size(mimir_core_mapped_bytes(),
                          large_sum + span_total * page_bytes,
                          "fragmented Q-MAPPED exact "
                          "(measured span total)");
            /* I-002 F-3, literally: free slots inside
             * BEARING spans x class size, plus Q-RETAINED
             * (the retained empty spans enter whole, by
             * the definition's own second term). On the
             * I-006 host retained was 0 at this point and
             * the formula reduced to the slot term. */
            f3 = 0;
            tails = 0;
            for (k = 0; k < 3; k++) {
                f3 += (ph[k].bear * caps[k] - live[k]) *
                      reqs[k];
                tails += ph[k].bear * tailv[k];
            }
            f3 += mimir_core_retained_bytes();
            printf("  live objects: 64:%zu 256:%zu "
                   "1024:%zu\n", live[0], live[1], live[2]);
            printf("  F-3 free-but-unusable = %zu B "
                   "(bearing-span free slots + Q-RETAINED, "
                   "I-002 formula)\n", f3);
            printf("  F-1(b) span tails total (bearing "
                   "spans) = %zu B\n", tails);
            printf("  R-EXT = F-3/Q-MAPPED = %.6f\n",
                   (double)f3 /
                   (double)mimir_core_mapped_bytes());
            printf("  R-INT = tails/Q-LIVESPAN = %.6f "
                   "(Q-LIVESPAN = bearing spans x page)\n",
                   (double)tails /
                   (double)(bear_total * page_bytes));
            check(f3 > 0, "F-3 positive at fragmented "
                  "point");
            check_eq_size((mimir_audit_small_alloc -
                           mimir_audit_small_free),
                          live[0] + live[1] + live[2],
                          "live small objects reconcile");
        }
        frag_bearing = bear_total;
    }
    check(live_patterns_ok(b64, n64, 64, 0x10, 1) &&
          live_patterns_ok(b256, n256, 256, 0x40, 1) &&
          live_patterns_ok(b1024, n1024, 1024, 0x70, 1),
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
                  "new class acquires a fresh span despite "
                  "F-3 free bytes");
    pub0 = mimir_audit_span_published;
    {
        void *reuse = mimir_malloc(64);
        check(reuse != NULL, "class 64 allocation succeeds");
        check_eq_size(mimir_audit_span_published - pub0, 0,
                      "own class reuses a hole (no new span)");
        mimir_free(reuse);
    }
    mimir_free(probe);
    quiesce();
    check_eq_size(mimir_core_retained_spans(),
                  retained_frag + 1,
                  "emptied 128 span retained (frag "
                  "retained + 1)");
    ret0 = mimir_audit_span_retired;
    t0 = now_ns();
    r = mimir_core_trim_retained();
    t1 = now_ns();
    printf("  TRIM frag-state full: released=%zu wall_ns=%lld "
           "per_span_ns=%lld\n", r, t1 - t0,
           r ? (t1 - t0) / (long long)r : 0);
    check_eq_size(r, retained_frag + 1,
                  "trim in fragmented state releases "
                  "exactly the retained spans");
    check_eq_size(mimir_audit_span_retired - ret0,
                  retained_frag + 1,
                  "span release calls == released");
    check_eq_size(mimir_core_mapped_bytes(),
                  mapped0 - retained_frag * page_bytes,
                  "Q-MAPPED after frag-state trim == "
                  "fragmented minus retained pages");
    check(live_patterns_ok(b64, n64, 64, 0x10, 1) &&
          live_patterns_ok(b256, n256, 256, 0x40, 1) &&
          live_patterns_ok(b1024, n1024, 1024, 0x70, 1) &&
          pattern_intact(large1, 20000, 0xA1) &&
          pattern_intact(large2, 40000, 0xB2),
          "all live blocks byte-identical after frag-state "
          "trim");

    printf("== free-path reclaim (bound 2 per class) ==\n");
    ret0 = mimir_audit_span_retired;
    for (i = 0; i < n64; i += 2) {
        mimir_free(b64[i]);
    }
    for (i = 0; i < n256; i += 2) {
        mimir_free(b256[i]);
    }
    for (i = 0; i < n1024; i += 2) {
        mimir_free(b1024[i]);
    }
    /* The free-path release is applied at drain/sweep;
     * quiesce() performs that drain without releasing any
     * retained span itself, so the retired delta across
     * free-all + quiesce IS the free-path release count. */
    quiesce();
    traj("after free-all small");
    retained_fa = mimir_core_retained_spans();
    check_eq_size(mimir_audit_span_retired - ret0,
                  frag_bearing - retained_fa,
                  "free path released every bearing span "
                  "past the retained bound");
    check_eq_size(retained_fa, 6,
                  "6 retained spans (bound 2 per class x "
                  "3 classes)");
    check_eq_size(mimir_core_retained_bytes(),
                  retained_fa * page_bytes,
                  "Q-RETAINED = retained spans x page");
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
    check_eq_size(mimir_core_retained_spans(),
                  retained_fa - 2,
                  "retained spans after partial trim");
    traj("after partial trim");

    printf("== full trim (OS cross-check) ==\n");
    {
        size_t remain = mimir_core_retained_spans();
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
        check_eq_size(r, remain,
                      "full trim releases exactly the "
                      "retained spans");
        check_eq_size(mimir_audit_span_retired - ret0, remain,
                      "span release calls == released");
        check_eq_size(mimir_core_retained_bytes(), 0,
                      "Q-RETAINED 0 after full trim");
        check_eq_size(mimir_core_mapped_bytes(), large_sum,
                      "Q-MAPPED back to large-only");
        check(anon_before >= 0 && anon_after >= 0 &&
              anon_before - anon_after ==
              (long)(remain * page_bytes),
              "OS anonymous rw-p drop == retained pages "
              "exactly");
    }
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
    printf("  TRIM noop partial(3): released=%zu "
           "wall_ns=%lld\n", r, t1 - t0);
    check_eq_size(r, 0, "partial trim on empty cache is a "
                  "no-op");

    printf("== trim overhead trials (class 256, 2 spans) "
           "==\n");
    for (t = 0; t < 5; t++) {
        size_t held;
        check(alloc_fill(warm, 2 * cap256, 256, 0x21),
              "trial warm alloc");
        for (i = 0; i < 2 * cap256; i++) {
            mimir_free(warm[i]);
        }
        quiesce();
        held = mimir_core_retained_spans();
        check_eq_size(held, 2,
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
        check(alloc_fill(warm, 2 * cap256, 256, 0x31),
              "partial trial warm alloc");
        for (i = 0; i < 2 * cap256; i++) {
            mimir_free(warm[i]);
        }
        quiesce();
        t0 = now_ns();
        r = mimir_core_trim_retained_some(1);
        t1 = now_ns();
        part_trials[t] = t1 - t0;
        printf("  TRIAL partial #%d: released=%zu "
               "wall_ns=%lld\n", t, r, t1 - t0);
        check_eq_size(r, 1, "partial trial releases 1");
        check_eq_size(mimir_core_trim_retained(), 1,
                      "partial trial cleanup trim releases 1");
    }
    printf("  partial-trim trials median wall_ns=%lld "
           "(timing-never-an-oracle)\n",
           median_ll(part_trials, 3));
    check_eq_size(mimir_core_mapped_bytes(), large_sum,
                  "Q-MAPPED large-only after trials");

    printf("== F-4 follow-on cost: hit vs miss (class 512) "
           "==\n");
    {
        /* On this host a fresh class-512 acquisition
         * publishes W spans and the warm free leaves R
         * retained (pilot: W=2, R=2 - the TLS batch /
         * arena distribution of the current core). Both
         * are measured at warm-up and the legs are graded
         * against the measured values, never assumed. */
        size_t w512, r512;
        pub0 = mimir_audit_span_published;
        probe = mimir_malloc(512);
        check(probe != NULL, "class 512 warm alloc");
        w512 = mimir_audit_span_published - pub0;
        mimir_free(probe);
        quiesce();
        r512 = mimir_core_retained_spans();
        printf("  class 512 warm: fresh acquisition "
               "published %zu spans, retained after free "
               "= %zu\n", w512, r512);
        check(w512 >= 1, "warm acquisition published at "
              "least one span");
        check(r512 >= 1, "at least one retained 512 span "
              "for hit leg");
        for (t = 0; t < 7; t++) {
            pub0 = mimir_audit_span_published;
            t0 = now_ns();
            probe = mimir_malloc(512);
            t1 = now_ns();
            hit[t] = t1 - t0;
            check(probe != NULL, "hit alloc succeeds");
            check_eq_size(mimir_audit_span_published -
                          pub0, 0,
                          "hit sample: no span acquired");
            mimir_free(probe);
        }
        quiesce();
        for (t = 0; t < 7; t++) {
            check_eq_size(mimir_core_trim_retained(), r512,
                          "miss setup trim releases the "
                          "retained spans");
            pub0 = mimir_audit_span_published;
            t0 = now_ns();
            probe = mimir_malloc(512);
            t1 = now_ns();
            miss[t] = t1 - t0;
            check(probe != NULL, "miss alloc succeeds");
            check_eq_size(mimir_audit_span_published -
                          pub0, w512,
                          "miss sample: fresh acquisition "
                          "repeats the warm span count");
            mimir_free(probe);
            quiesce();
        }
        check_eq_size(mimir_core_trim_retained(), r512,
                      "miss leg cleanup trim");
    }
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
    {
        size_t leg_min = (30 + cap1024 - 1) / cap1024;
        size_t leg_spans, held;
        pub0 = mimir_audit_span_published;
        check(alloc_fill(leg, 30, 1000, 0x51), "leg alloc");
        leg_spans = mimir_audit_span_published - pub0;
        printf("  leg spans published: %zu (capacity "
               "minimum %zu; distribution surplus filed "
               "as measured)\n", leg_spans, leg_min);
        check(leg_spans >= leg_min,
              "leg spans at least the capacity minimum");
        for (i = 1; i < 30; i += 2) {
            mimir_free(leg[i]);
        }
        printf("  F-1(a) live term: 15 live x (1024-1000) "
               "= %zu B (workload-record exact; allocator "
               "state cannot hold it, INC-I002-02)\n",
               (size_t)(15 * 24));
        check(live_patterns_ok(leg, 30, 1000, 0x51, 1),
              "leg live patterns intact");
        for (i = 0; i < 30; i += 2) {
            mimir_free(leg[i]);
        }
        quiesce();
        held = mimir_core_retained_spans();
        check_eq_size(mimir_core_trim_retained(), held,
                      "leg cleanup trim releases the "
                      "retained spans");
        traj("after F-1(a) leg");
    }

    printf("== F-5 ==\n");
    printf("  F-5 NOT MEASURED: no budget enforcement "
           "exists at this base (INC-I006-01, carried from "
           "INC-I001-14); reported absent, not "
           "approximated.\n");

    printf("== F-6 cross-slice leg: I-005 constrained runs "
           "==\n");
    run_child(child_a, "A");
    run_child(child_d, "D");
    quiesce();

    printf("== final reconciliation ==\n");
    mimir_free(large1);
    mimir_free(large2);
    quiesce();
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
          "mapping counters reconcile (published == "
          "retired)");
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
