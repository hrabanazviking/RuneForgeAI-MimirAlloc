/* probe_z060.c - Z-060 pressure cycles on this host.
 *
 * Phase-9 pressure-cycle heritage re-run (docs/
 * PHASE_9_PRESSURE_CYCLES.md, slice I-008) on the Z13
 * host, with Z-050's trim policy (docs/Z13_TRIM_POLICY.md)
 * and the Z-048 mapping pool live. Modes (argv[1]):
 *
 *  cycles  - NCYC repeated pressure cycles in ONE process
 *            (the I-008 form): build span retention (the
 *            Z-050 shape) + mapping-pool retention (dense-
 *            touched larges), CALM holds, PARTIAL halves
 *            the spans and provably does NOT touch the
 *            pool (CP-3 evidence), FULL (injected HIGH PSI
 *            sample through the policy's apply form, the
 *            Z-050 input contract) releases BOTH pools,
 *            recovery allocations verified, quiescent
 *            reconciliation; the per-cycle counter tuple
 *            must be identical every cycle (no ratchet).
 *  caps    - the pool's hard bounds are the spend (CP-4):
 *            count cap 64, byte cap 32 MiB, single
 *            oversize mapping refused; overflow retires
 *            take the munmap path and are counted in
 *            Q-RELEASED (J-002 bytes_released).
 *  madv    - CP-5 pricing on raw mappings of this host:
 *            MADV_DONTNEED returns RSS at once;
 *            MADV_FREE does not (lazy; re-touch is
 *            fault-free while unreclaimed). Host-level
 *            pricing only; no allocator surface exists
 *            for either call on this tree.
 *  ceiling - the heritage ceiling leg: the probe sets
 *            its own RLIMIT_AS from its measured VmSize
 *            baseline + fixed headroom (I-008 form),
 *            fills large blocks to the clean NULL, probes
 *            responsiveness at the pressure point, frees,
 *            FULL-trims, restores the limit (pressure
 *            release) and proves recovery. The runner
 *            executes this mode inside a systemd-run
 *            --user scope with MemoryMax/MemorySwapMax
 *            (a cgroup ceiling scoped to this process
 *            only); the probe prints its cgroup path as
 *            evidence.
 *  hog     - raw-mmap resident hog for the runner's
 *            pure-cgroup containment leg: dense-touches
 *            32 MiB chunks until the ceiling stops it.
 *            Exit 0 only if a chunk mmap fails cleanly;
 *            being OOM-killed inside the scope (137) is
 *            the runner's expected containment outcome.
 *
 * Prints key=value lines; each mode's last line is
 * "PROBE_Z060_<MODE>: PASS" or "...: FAIL <reason>".
 * Exit 0 iff PASS (hog: see above). Standalone via
 * make probe-z060 / make test-z060.
 */
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#include "mimir_alloc.h"
#include "../../src/core/mimir_core.h"
#include "../core/mimir_trim_i003.h"
#include "../core/mimir_tune_g007.h"
#include "../core/mimir_pool_z048.h"

static long Pg; /* page size, discovered */

static long rss_kib(void)
{
    FILE *f = fopen("/proc/self/smaps_rollup", "r");
    char line[256];
    long v = -1;

    if (f != NULL) {
        while (fgets(line, sizeof line, f) != NULL) {
            if (strncmp(line, "Rss:", 4) == 0) {
                v = strtol(line + 4, NULL, 10);
                break;
            }
        }
        fclose(f);
        if (v >= 0) {
            return v;
        }
    }
    f = fopen("/proc/self/status", "r");
    if (f == NULL) {
        return -1;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            v = strtol(line + 6, NULL, 10);
            break;
        }
    }
    fclose(f);
    return v;
}

static long status_kib(const char *key)
{
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    size_t klen = strlen(key);
    long v = -1;

    if (f == NULL) {
        return -1;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        if (strncmp(line, key, klen) == 0) {
            v = strtol(line + klen, NULL, 10);
            break;
        }
    }
    fclose(f);
    return v;
}

static long minflt(void)
{
    FILE *f = fopen("/proc/self/stat", "r");
    char buf[4096];
    size_t n;
    char *rp, *p;
    long fields[12];
    int i;

    if (f == NULL) {
        return -1;
    }
    n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    if (n == 0) {
        return -1;
    }
    buf[n] = '\0';
    rp = strrchr(buf, ')');
    if (rp == NULL) {
        return -1;
    }
    p = rp + 2; /* field 3 (state, one char) starts here */
    while (*p != ' ' && *p != '\0') {
        p++; /* skip the state char */
    }
    while (*p == ' ') {
        p++; /* field 4 (ppid) starts here */
    }
    for (i = 0; i < 12; i++) {
        char *end = NULL;
        fields[i] = strtol(p, &end, 10);
        if (end == p) {
            return -1;
        }
        p = end;
    }
    /* fields[0] is field 4; minflt is field 10. */
    return fields[10 - 4];
}

static void print_cgroup(void)
{
    FILE *f = fopen("/proc/self/cgroup", "r");
    char line[512];

    if (f == NULL) {
        printf("cgroup_path=UNREADABLE\n");
        return;
    }
    while (fgets(line, sizeof line, f) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        printf("cgroup_path=%s\n", line);
    }
    fclose(f);
}

static void dense_touch(unsigned char *p, size_t n)
{
    size_t off;
    for (off = 0; off < n; off += (size_t)Pg) {
        p[off] = (unsigned char)(0x5Au ^ (off >> 12));
    }
    p[n - 1] = (unsigned char)0xC3u;
}

static int dense_check(const unsigned char *p, size_t n)
{
    size_t off;
    for (off = 0; off < n; off += (size_t)Pg) {
        if (p[off] != (unsigned char)(0x5Au ^ (off >> 12))) {
            return 0;
        }
    }
    if (p[n - 1] != (unsigned char)0xC3u) {
        return 0;
    }
    return 1;
}

static size_t large_len(size_t req)
{
    size_t total = (size_t)Pg + req;
    return ((total + (size_t)Pg - 1) / (size_t)Pg) * (size_t)Pg;
}

static struct mimir_stats snap(void)
{
    struct mimir_stats s;
    memset(&s, 0, sizeof s);
    if (mimir_core_stats_snapshot(&s) != 0) {
        printf("PROBE_Z060: FAIL stats snapshot rc != 0\n");
        exit(1);
    }
    return s;
}

/* ---- shared workload shape (Z-050 span shape + pool) -- */
#define NCLASS 6
#define PER_CLASS 3000
static const size_t small_sizes[NCLASS] =
    {32, 64, 128, 256, 512, 1024};
static void *small_blocks[NCLASS][PER_CLASS];

#define NLARGE 8
static const size_t large_sizes[NLARGE] =
    {65536, 262144, 1048576, 2097152,
     65536, 262144, 1048576, 2097152};
static void *large_blocks[NLARGE];

#define EXPECT_SPANS ((size_t)96)
#define EXPECT_SPAN_BYTES ((size_t)393216)
#define EXPECT_POOL_COUNT ((size_t)8)
#define TOL_KIB 512L

static size_t expect_pool_bytes(void)
{
    size_t sum = 0;
    int i;
    for (i = 0; i < NLARGE; i++) {
        sum += large_len(large_sizes[i]);
    }
    return sum;
}

static void build_smalls(void)
{
    size_t c, i;
    for (c = 0; c < NCLASS; c++) {
        for (i = 0; i < PER_CLASS; i++) {
            small_blocks[c][i] = mimir_malloc(small_sizes[c]);
            if (small_blocks[c][i] == NULL) {
                printf("PROBE_Z060: FAIL small alloc NULL\n");
                exit(1);
            }
            memset(small_blocks[c][i], 0xA5, small_sizes[c]);
        }
    }
}

static void free_smalls(void)
{
    size_t c, i;
    for (c = 0; c < NCLASS; c++) {
        for (i = 0; i < PER_CLASS; i++) {
            mimir_free(small_blocks[c][i]);
            small_blocks[c][i] = NULL;
        }
    }
}

static void build_larges(void)
{
    int i;
    for (i = 0; i < NLARGE; i++) {
        large_blocks[i] = mimir_malloc(large_sizes[i]);
        if (large_blocks[i] == NULL) {
            printf("PROBE_Z060: FAIL large alloc NULL\n");
            exit(1);
        }
        dense_touch((unsigned char *)large_blocks[i],
                    large_sizes[i]);
        if (!dense_check((const unsigned char *)large_blocks[i],
                         large_sizes[i])) {
            printf("PROBE_Z060: FAIL large pattern\n");
            exit(1);
        }
    }
}

static void free_larges(void)
{
    int i;
    for (i = 0; i < NLARGE; i++) {
        mimir_free(large_blocks[i]);
        large_blocks[i] = NULL;
    }
}

/* ================= cycles mode ================= */
#define NCYC 5

struct cycle_tuple {
    size_t ret_spans;
    size_t ret_bytes;
    size_t pool_count;
    size_t pool_bytes;
    int calm_dec;
    size_t calm_rel;
    int part_dec;
    size_t part_rel;
    size_t part_pool_delta;
    size_t rebuilt_spans;
    int full_dec;
    size_t full_rel;
    size_t after_retained;
    size_t after_pool_count;
    size_t after_pool_bytes;
    size_t recovery_ok;
};

static int tuple_eq(const struct cycle_tuple *a,
                    const struct cycle_tuple *b)
{
    return a->ret_spans == b->ret_spans &&
           a->ret_bytes == b->ret_bytes &&
           a->pool_count == b->pool_count &&
           a->pool_bytes == b->pool_bytes &&
           a->calm_dec == b->calm_dec &&
           a->calm_rel == b->calm_rel &&
           a->part_dec == b->part_dec &&
           a->part_rel == b->part_rel &&
           a->part_pool_delta == b->part_pool_delta &&
           a->rebuilt_spans == b->rebuilt_spans &&
           a->full_dec == b->full_dec &&
           a->full_rel == b->full_rel &&
           a->after_retained == b->after_retained &&
           a->after_pool_count == b->after_pool_count &&
           a->after_pool_bytes == b->after_pool_bytes &&
           a->recovery_ok == b->recovery_ok;
}

static void cfail(const char *mode, const char *why,
                  long a, long b)
{
    printf("PROBE_Z060_%s: FAIL %s (%ld vs %ld)\n",
           mode, why, a, b);
    exit(1);
}

static int run_cycles(void)
{
    struct mimir_pressure_sample calm, raised, high;
    struct cycle_tuple ref, cur;
    long rss_base;
    int cyc;
    int have_ref = 0;

    memset(&calm, 0, sizeof calm);
    memset(&raised, 0, sizeof raised);
    memset(&high, 0, sizeof high);
    raised.some_avg10 = 2.0;  /* RAISED band, idle 0 -> PARTIAL */
    high.some_avg10 = 25.0;   /* HIGH band -> FULL */
    high.full_avg10 = 10.0;

    if (mimir_tune_set_retention_bound(16) != 0) {
        cfail("CYCLES", "retention bound refused", 0, 0);
    }
    rss_base = rss_kib();
    printf("rss_base_kib=%ld\n", rss_base);
    printf("expect_pool_bytes=%zu\n", expect_pool_bytes());

    for (cyc = 1; cyc <= NCYC; cyc++) {
        long rss_free, rss_pre_full, rss_full;
        size_t pool_before_partial, pool_after_partial;
        int decision = -1;
        size_t released;
        struct mimir_stats st;
        void *rec[4];
        size_t i;
        int rec_ok = 1;

        memset(&cur, 0, sizeof cur);

        build_smalls();
        build_larges();
        free_smalls();
        (void)mimir_core_trim_retained_some(0); /* TLS drain */
        cur.ret_spans = mimir_core_retained_spans();
        cur.ret_bytes = mimir_core_retained_bytes();
        free_larges();
        cur.pool_count = mimir_core_pool_count();
        cur.pool_bytes = mimir_core_pool_bytes();
        rss_free = rss_kib();
        printf("cycle=%d ret_spans=%zu ret_bytes=%zu "
               "pool_count=%zu pool_bytes=%zu rss_free_kib=%ld\n",
               cyc, cur.ret_spans, cur.ret_bytes,
               cur.pool_count, cur.pool_bytes, rss_free);
        if (cur.ret_spans != EXPECT_SPANS ||
            cur.ret_bytes != EXPECT_SPAN_BYTES) {
            cfail("CYCLES", "retained shape drifted",
                  (long)cur.ret_spans, (long)EXPECT_SPANS);
        }
        if (cur.pool_count != EXPECT_POOL_COUNT ||
            cur.pool_bytes != expect_pool_bytes()) {
            cfail("CYCLES", "pool shape drifted",
                  (long)cur.pool_count, (long)EXPECT_POOL_COUNT);
        }
        if (rss_free <
            rss_base + (long)((cur.ret_bytes + cur.pool_bytes)
                              / 1024) - TOL_KIB) {
            cfail("CYCLES", "retained bytes not visible in RSS",
                  rss_free, rss_base);
        }

        /* CALM leg: nothing moves (policy promise P-1). */
        released = mimir_core_trim_policy_apply(&calm, 0,
                                                &decision);
        cur.calm_dec = decision;
        cur.calm_rel = released;
        if (decision != MIMIR_TRIM_DECISION_NONE ||
            released != 0) {
            cfail("CYCLES", "calm leg trimmed",
                  (long)decision, (long)released);
        }
        if (mimir_core_retained_spans() != EXPECT_SPANS ||
            mimir_core_pool_count() != EXPECT_POOL_COUNT) {
            cfail("CYCLES", "calm leg moved a pool", 0, 0);
        }

        /* PARTIAL leg: spans halve; the mapping pool must
         * NOT move at all (CP-3: no DONTNEED step exists;
         * PARTIAL is span-budget only on this tree). */
        pool_before_partial = mimir_core_pool_bytes();
        released = mimir_core_trim_policy_apply(&raised, 0,
                                                &decision);
        cur.part_dec = decision;
        cur.part_rel = released;
        pool_after_partial = mimir_core_pool_bytes();
        cur.part_pool_delta =
            pool_after_partial - pool_before_partial;
        printf("cycle=%d partial_dec=%d partial_rel=%zu "
               "partial_pool_delta=%zu\n",
               cyc, decision, released, cur.part_pool_delta);
        if (decision != MIMIR_TRIM_DECISION_PARTIAL ||
            released != EXPECT_SPANS / 2) {
            cfail("CYCLES", "partial leg wrong",
                  (long)decision, (long)released);
        }
        if (cur.part_pool_delta != 0 ||
            mimir_core_pool_count() != EXPECT_POOL_COUNT) {
            cfail("CYCLES", "partial leg touched the pool",
                  (long)cur.part_pool_delta, 0);
        }
        if (mimir_core_retained_spans() != EXPECT_SPANS / 2) {
            cfail("CYCLES", "partial leg span count wrong",
                  (long)mimir_core_retained_spans(),
                  (long)(EXPECT_SPANS / 2));
        }

        /* Rebuild the span pool to the full shape. */
        build_smalls();
        free_smalls();
        (void)mimir_core_trim_retained_some(0);
        cur.rebuilt_spans = mimir_core_retained_spans();
        if (cur.rebuilt_spans != EXPECT_SPANS) {
            cfail("CYCLES", "rebuild did not restore shape",
                  (long)cur.rebuilt_spans, (long)EXPECT_SPANS);
        }

        /* FULL leg: one HIGH evaluation returns BOTH
         * pools (CP-1) and RSS follows down. */
        rss_pre_full = rss_kib();
        released = mimir_core_trim_policy_apply(&high, 0,
                                                &decision);
        cur.full_dec = decision;
        cur.full_rel = released;
        cur.after_retained = mimir_core_retained_bytes();
        cur.after_pool_count = mimir_core_pool_count();
        cur.after_pool_bytes = mimir_core_pool_bytes();
        rss_full = rss_kib();
        printf("cycle=%d full_dec=%d full_rel=%zu "
               "rss_pre_full_kib=%ld rss_full_kib=%ld\n",
               cyc, decision, released, rss_pre_full, rss_full);
        if (decision != MIMIR_TRIM_DECISION_FULL ||
            released != EXPECT_SPANS) {
            cfail("CYCLES", "full leg wrong",
                  (long)decision, (long)released);
        }
        if (cur.after_retained != 0 ||
            cur.after_pool_count != 0 ||
            cur.after_pool_bytes != 0) {
            cfail("CYCLES", "full leg left retention",
                  (long)cur.after_pool_bytes, 0);
        }
        if (rss_full >
            rss_pre_full -
                (long)((EXPECT_SPAN_BYTES +
                        expect_pool_bytes()) / 1024) +
                TOL_KIB) {
            cfail("CYCLES", "RSS did not follow FULL down",
                  rss_full, rss_pre_full);
        }

        /* Recovery: allocator fully usable after the
         * pressure release; then reconcile quiescent. */
        for (i = 0; i < 4; i++) {
            rec[i] = mimir_malloc(large_sizes[i]);
            if (rec[i] == NULL) {
                rec_ok = 0;
                break;
            }
            dense_touch((unsigned char *)rec[i],
                        large_sizes[i]);
            if (!dense_check((const unsigned char *)rec[i],
                             large_sizes[i])) {
                rec_ok = 0;
            }
        }
        cur.recovery_ok = (size_t)rec_ok;
        if (!rec_ok) {
            cfail("CYCLES", "recovery allocation failed", 0, 0);
        }
        for (i = 0; i < 4; i++) {
            mimir_free(rec[i]);
        }
        (void)mimir_core_trim_retained();
        st = snap();
        if (st.live_mappings != 0 || st.live_small_objects != 0 ||
            st.bytes_mapped != 0 || st.bytes_cached != 0) {
            cfail("CYCLES", "quiescent reconciliation failed",
                  (long)st.bytes_mapped, 0);
        }
        if (mimir_core_pool_count() != 0 ||
            mimir_core_retained_spans() != 0) {
            cfail("CYCLES", "quiescent pools nonzero", 0, 0);
        }

        if (!have_ref) {
            ref = cur;
            have_ref = 1;
        } else if (!tuple_eq(&ref, &cur)) {
            cfail("CYCLES", "cycle tuple drift (ratchet)",
                  (long)cyc, 1);
        }
        printf("cycle=%d tuple=STABLE\n", cyc);
    }
    printf("cycles_run=%d\n", NCYC);
    printf("PROBE_Z060_CYCLES: PASS\n");
    return 0;
}

/* ================= caps mode ================= */
static int run_caps(void)
{
    struct mimir_stats s0, s1;
    void *ptrs[65];
    size_t expect_first64 = 0;
    size_t i;

    if (mimir_core_pool_count() != 0) {
        cfail("CAPS", "pool not empty at start", 0, 0);
    }
    /* Count cap: 65 distinct lengths; first 64 pool, the
     * 65th retire takes the munmap path (Q-RELEASED). */
    for (i = 0; i < 65; i++) {
        size_t req = 2048 + i * (size_t)Pg;
        ptrs[i] = mimir_malloc(req);
        if (ptrs[i] == NULL) {
            cfail("CAPS", "count-cap alloc NULL", (long)i, 0);
        }
        ((unsigned char *)ptrs[i])[0] = (unsigned char)i;
        if (i < 64) {
            expect_first64 += large_len(req);
        }
    }
    s0 = snap();
    for (i = 0; i < 65; i++) {
        mimir_free(ptrs[i]);
    }
    s1 = snap();
    printf("count_cap_pool_count=%zu\n",
           mimir_core_pool_count());
    printf("count_cap_pool_bytes=%zu\n",
           mimir_core_pool_bytes());
    printf("count_cap_expect_bytes=%zu\n", expect_first64);
    printf("count_cap_released_delta=%llu\n",
           (unsigned long long)(s1.bytes_released -
                                s0.bytes_released));
    if (mimir_core_pool_count() != 64 ||
        mimir_core_pool_bytes() != expect_first64) {
        cfail("CAPS", "count cap wrong",
              (long)mimir_core_pool_count(), 64);
    }
    if (s1.bytes_released - s0.bytes_released !=
        (unsigned long long)large_len(2048 + 64 * (size_t)Pg)) {
        cfail("CAPS", "overflow retire not in Q-RELEASED",
              (long)(s1.bytes_released - s0.bytes_released),
              (long)large_len(2048 + 64 * (size_t)Pg));
    }
    s0 = snap();
    (void)mimir_core_trim_retained();
    s1 = snap();
    if (mimir_core_pool_count() != 0 ||
        s1.bytes_released - s0.bytes_released !=
            (unsigned long long)expect_first64) {
        cfail("CAPS", "trim did not release count pool",
              (long)mimir_core_pool_count(), 0);
    }

    /* Byte cap: two 20 MiB mappings; the second retire
     * would take the pool past 32 MiB and munmaps. */
    {
        void *a = mimir_malloc(20u * 1024u * 1024u);
        void *b = mimir_malloc(20u * 1024u * 1024u);
        size_t len = large_len(20u * 1024u * 1024u);
        if (a == NULL || b == NULL) {
            cfail("CAPS", "byte-cap alloc NULL", 0, 0);
        }
        ((unsigned char *)a)[0] = 1;
        ((unsigned char *)b)[0] = 2;
        s0 = snap();
        mimir_free(a);
        if (mimir_core_pool_count() != 1 ||
            mimir_core_pool_bytes() != len) {
            cfail("CAPS", "first 20 MiB not pooled",
                  (long)mimir_core_pool_count(), 1);
        }
        mimir_free(b);
        s1 = snap();
        printf("byte_cap_pool_count=%zu byte_cap_len=%zu\n",
               mimir_core_pool_count(), len);
        if (mimir_core_pool_count() != 1 ||
            mimir_core_pool_bytes() != len ||
            s1.bytes_released - s0.bytes_released !=
                (unsigned long long)len) {
            cfail("CAPS", "byte cap wrong",
                  (long)mimir_core_pool_count(), 1);
        }
        (void)mimir_core_trim_retained();
    }

    /* Single oversize: a 40 MiB mapping can never pool. */
    {
        void *big = mimir_malloc(40u * 1024u * 1024u);
        size_t len = large_len(40u * 1024u * 1024u);
        if (big == NULL) {
            cfail("CAPS", "oversize alloc NULL", 0, 0);
        }
        ((unsigned char *)big)[0] = 3;
        s0 = snap();
        mimir_free(big);
        s1 = snap();
        printf("oversize_len=%zu pool_after=%zu\n",
               len, mimir_core_pool_count());
        if (mimir_core_pool_count() != 0 ||
            s1.bytes_released - s0.bytes_released !=
                (unsigned long long)len) {
            cfail("CAPS", "oversize mapping pooled",
                  (long)mimir_core_pool_count(), 0);
        }
    }
    printf("pool_max_count=64\n");
    printf("pool_max_bytes=33554432\n");
    printf("PROBE_Z060_CAPS: PASS\n");
    return 0;
}

/* ================= madv mode ================= */
static int run_madv(void)
{
    size_t n = 16u * 1024u * 1024u;
    unsigned char *a, *b;
    long rss0, rss1, rss2, flt0, flt_b, flt_a;

    a = (unsigned char *)mmap(NULL, n, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    b = (unsigned char *)mmap(NULL, n, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (a == MAP_FAILED || b == MAP_FAILED) {
        cfail("MADV", "raw mmap failed", 0, 0);
    }
    dense_touch(a, n);
    dense_touch(b, n);
    if (!dense_check(a, n) || !dense_check(b, n)) {
        cfail("MADV", "raw pattern failed", 0, 0);
    }
    rss0 = rss_kib();
    if (madvise(b, n, MADV_DONTNEED) != 0) {
        cfail("MADV", "MADV_DONTNEED failed", 0, 0);
    }
    rss1 = rss_kib();
    flt0 = minflt();
    if (madvise(a, n, MADV_FREE) != 0) {
        cfail("MADV", "MADV_FREE failed", 0, 0);
    }
    rss2 = rss_kib();
    printf("madv_rss_touched_kib=%ld\n", rss0);
    printf("madv_rss_after_dontneed_kib=%ld\n", rss1);
    printf("madv_rss_after_free_kib=%ld\n", rss2);
    printf("madv_dontneed_drop_kib=%ld\n", rss0 - rss1);
    printf("madv_free_drop_kib=%ld\n", rss1 - rss2);
    if (rss0 - rss1 < 15360L) {
        cfail("MADV", "DONTNEED did not return RSS",
              rss0 - rss1, 15360L);
    }
    if (rss1 - rss2 > 1024L) {
        cfail("MADV", "FREE returned RSS eagerly",
              rss1 - rss2, 1024L);
    }
    dense_touch(b, n); /* re-fault the DONTNEED mapping */
    flt_b = minflt();
    dense_touch(a, n); /* FREE pages still resident: no faults */
    flt_a = minflt();
    printf("madv_refault_dontneed=%ld\n", flt_b - flt0);
    printf("madv_refault_free=%ld\n", flt_a - flt_b);
    if (flt_b - flt0 < 4000L) {
        cfail("MADV", "DONTNEED re-touch did not re-fault",
              flt_b - flt0, 4000L);
    }
    if (flt_a - flt_b > 100L) {
        cfail("MADV", "FREE re-touch faulted (pages reclaimed)",
              flt_a - flt_b, 100L);
    }
    printf("madv_free_verdict=LAZY_RSS_RETAINED\n");
    (void)munmap(a, n);
    (void)munmap(b, n);
    printf("PROBE_Z060_MADV: PASS\n");
    return 0;
}

/* ================= ceiling mode ================= */
static int run_ceiling(void)
{
    struct rlimit saved, work;
    long vmsize_kib;
    size_t headroom = 40u * 1024u * 1024u;
    void *fill[256];
    size_t nfill = 0;
    struct timespec t0, t1;
    void *probe_small, *rec;
    long health_ns;
    int stable_null = 0;

    print_cgroup();
    vmsize_kib = status_kib("VmSize:");
    if (vmsize_kib < 0) {
        cfail("CEILING", "VmSize unreadable", 0, 0);
    }
    if (getrlimit(RLIMIT_AS, &saved) != 0) {
        cfail("CEILING", "getrlimit failed", 0, 0);
    }
    printf("ceiling_vmsize_base_kib=%ld\n", vmsize_kib);
    printf("ceiling_headroom_bytes=%zu\n", headroom);
    work = saved;
    work.rlim_cur = (rlim_t)((size_t)vmsize_kib * 1024u) +
                    (rlim_t)headroom;
    if (saved.rlim_max != RLIM_INFINITY &&
        work.rlim_cur > saved.rlim_max) {
        cfail("CEILING", "headroom exceeds hard limit",
              (long)work.rlim_cur, (long)saved.rlim_max);
    }
    if (mimir_tune_set_retention_bound(16) != 0) {
        cfail("CEILING", "retention bound refused", 0, 0);
    }
    /* Build retention BEFORE the ceiling descends, so the
     * pressure point is the fill's, not the build's. */
    build_smalls();
    build_larges();
    free_smalls();
    (void)mimir_core_trim_retained_some(0);
    free_larges();
    if (mimir_core_retained_spans() != EXPECT_SPANS ||
        mimir_core_pool_count() != EXPECT_POOL_COUNT) {
        cfail("CEILING", "pre-ceiling shape wrong", 0, 0);
    }
    if (setrlimit(RLIMIT_AS, &work) != 0) {
        cfail("CEILING", "setrlimit failed", 0, 0);
    }
    printf("ceiling_soft_bytes=%llu\n",
           (unsigned long long)work.rlim_cur);

    /* Fill to the clean NULL (real OS failure class). */
    while (nfill < 256) {
        void *p = mimir_malloc(1048576);
        if (p == NULL) {
            break;
        }
        ((unsigned char *)p)[0] = 0x11;
        ((unsigned char *)p)[1048575] = 0x22;
        fill[nfill++] = p;
    }
    if (nfill == 0 || nfill >= 256) {
        cfail("CEILING", "fill did not reach a clean NULL",
              (long)nfill, 0);
    }
    if (mimir_malloc(1048576) == NULL) {
        stable_null = 1;
    }
    printf("ceiling_fill_ok=%zu stable_null=%d\n",
           nfill, stable_null);
    if (!stable_null) {
        cfail("CEILING", "failure not stable at pressure point",
              0, 0);
    }

    /* Responsiveness at the pressure point (heritage:
     * reported, no bound claimed): a small allocation
     * served from a retained span needs no fresh VA; a
     * local health read must complete. */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    probe_small = mimir_malloc(64);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    if (probe_small == NULL) {
        cfail("CEILING", "retained-span probe NULL under "
              "pressure", 0, 0);
    }
    memset(probe_small, 0x7E, 64);
    if (((unsigned char *)probe_small)[63] != 0x7E) {
        cfail("CEILING", "probe pattern wrong", 0, 0);
    }
    mimir_free(probe_small);
    printf("ceiling_probe_alloc_ns=%lld\n",
           (long long)(t1.tv_sec - t0.tv_sec) * 1000000000LL +
               (long long)(t1.tv_nsec - t0.tv_nsec));
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (status_kib("VmSize:") < 0) {
        cfail("CEILING", "health read failed", 0, 0);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    health_ns = (long)((t1.tv_sec - t0.tv_sec) * 1000000000LL +
                       (t1.tv_nsec - t0.tv_nsec));
    printf("ceiling_health_ns=%ld\n", health_ns);

    /* Pressure release: free the fill, FULL-trim both
     * pools, restore the soft limit, prove recovery. */
    while (nfill > 0) {
        mimir_free(fill[--nfill]);
    }
    (void)mimir_core_trim_retained();
    if (mimir_core_pool_count() != 0 ||
        mimir_core_retained_spans() != 0) {
        cfail("CEILING", "trim under ceiling left retention",
              0, 0);
    }
    if (setrlimit(RLIMIT_AS, &saved) != 0) {
        cfail("CEILING", "limit restore failed", 0, 0);
    }
    rec = mimir_malloc(1048576);
    if (rec == NULL) {
        cfail("CEILING", "no recovery after release", 0, 0);
    }
    dense_touch((unsigned char *)rec, 1048576);
    if (!dense_check((const unsigned char *)rec, 1048576)) {
        cfail("CEILING", "recovery pattern wrong", 0, 0);
    }
    mimir_free(rec);
    (void)mimir_core_trim_retained();
    printf("ceiling_recovery_ok=1\n");
    printf("PROBE_Z060_CEILING: PASS\n");
    return 0;
}

/* ================= hog mode ================= */
static int run_hog(void)
{
    size_t chunk = 32u * 1024u * 1024u;
    void *keep[64];
    size_t n = 0;

    print_cgroup();
    while (n < 64) {
        unsigned char *p = (unsigned char *)mmap(
            NULL, chunk, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) {
            size_t k;
            printf("hog_result=MMAP_NULL chunks=%zu\n", n);
            printf("PROBE_Z060_HOG: PASS\n");
            for (k = 0; k < n; k++) {
                (void)munmap(keep[k], chunk);
            }
            return 0;
        }
        memset(p, 0xA5, chunk);
        keep[n++] = p;
        printf("hog_chunks=%zu rss_kib=%ld\n", n, rss_kib());
        fflush(stdout);
    }
    printf("hog_result=NO_CEILING_EFFECT chunks=%zu\n", n);
    printf("PROBE_Z060_HOG: FAIL no ceiling effect at 2 GiB\n");
    return 3;
}

/* ================ hogfree mode ================
 * CP-5 under pressure: dense-touch 6 x 32 MiB raw
 * chunks (192 MiB), MADV_FREE the first four (128 MiB),
 * then keep touching fresh 32 MiB chunks. Without the
 * FREE, the runner's hog died during its 8th chunk at
 * this ceiling; if FREE pages are the cgroup's first
 * reclaim source, this process survives materially
 * further on the same ceiling. PASS (printed before
 * any kill) iff it completes >= 10 chunks. */
static int run_hogfree(void)
{
    size_t chunk = 32u * 1024u * 1024u;
    unsigned char *keep[16];
    size_t n = 0;
    size_t i;

    print_cgroup();
    for (i = 0; i < 6; i++) {
        unsigned char *p = (unsigned char *)mmap(
            NULL, chunk, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) {
            printf("PROBE_Z060_HOGFREE: FAIL early mmap\n");
            return 1;
        }
        memset(p, 0x5A, chunk);
        keep[n++] = p;
    }
    printf("hogfree_touched_chunks=%zu rss_kib=%ld\n",
           n, rss_kib());
    fflush(stdout);
    for (i = 0; i < 4; i++) {
        if (madvise(keep[i], chunk, MADV_FREE) != 0) {
            printf("PROBE_Z060_HOGFREE: FAIL madvise\n");
            return 1;
        }
    }
    printf("hogfree_free_applied=4 rss_kib=%ld\n", rss_kib());
    fflush(stdout);
    while (n < 16) {
        unsigned char *p = (unsigned char *)mmap(
            NULL, chunk, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (p == MAP_FAILED) {
            break;
        }
        memset(p, 0xA5, chunk);
        keep[n++] = p;
        printf("hogfree_chunks=%zu rss_kib=%ld\n",
               n, rss_kib());
        fflush(stdout);
        if (n == 10) {
            printf("hogfree_result=FREE_PAGES_RECLAIMED "
                   "chunks=%zu\n", n);
            printf("PROBE_Z060_HOGFREE: PASS\n");
            fflush(stdout);
        }
    }
    printf("hogfree_final_chunks=%zu\n", n);
    if (n >= 10) {
        printf("PROBE_Z060_HOGFREE: PASS\n");
        return 0;
    }
    printf("PROBE_Z060_HOGFREE: FAIL only %zu chunks\n", n);
    return 1;
}

int main(int argc, char **argv)
{
    long ps = sysconf(_SC_PAGESIZE);
    const char *mode = (argc > 1) ? argv[1] : "cycles";

    if (ps <= 0) {
        printf("PROBE_Z060: FAIL page size undiscovered\n");
        return 1;
    }
    Pg = ps;
    printf("probe_z060_mode=%s page_size=%ld\n", mode, Pg);
    if (strcmp(mode, "cycles") == 0) {
        return run_cycles();
    }
    if (strcmp(mode, "caps") == 0) {
        return run_caps();
    }
    if (strcmp(mode, "madv") == 0) {
        return run_madv();
    }
    if (strcmp(mode, "ceiling") == 0) {
        return run_ceiling();
    }
    if (strcmp(mode, "hog") == 0) {
        return run_hog();
    }
    if (strcmp(mode, "hogfree") == 0) {
        return run_hogfree();
    }
    printf("PROBE_Z060: FAIL unknown mode %s\n", mode);
    return 1;
}
