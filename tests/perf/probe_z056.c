/* probe_z056.c - Z-056 soak harness probe.
 *
 * One source, two legs (the K-002 discipline):
 *   (default)    mimir: this tree's banked allocator
 *                (Z-048 mapping cache + Z-049 THP live)
 *   -DLEG_GLIBC  glibc: host malloc/free/calloc/realloc
 * Usage: probe_z056 <seconds> <interval_s>
 *
 * The soak: a 320-slot registry under a mixed lifecycle.
 * Sizes: 55% small (16..1024 B), 25% medium (4 KiB..
 * 64 KiB), 20% large (128 KiB..2 MiB), drawn from one
 * fixed-seed xorshift PRNG. Each slot carries a TTL in
 * operations (100..20000); the loop free-runs: an expired
 * or empty slot is (re)filled, live slots are also hit by
 * random realloc (grow/shrink) and free operations, small
 * fills occasionally use calloc (verified zero). Every
 * block is dense-touched (full memset; calloc blocks are
 * verified zero then stamped) so RSS is real resident
 * memory, not reservation.
 *
 * Every interval_s of wall clock (and at t=0 and at the
 * deadline) the probe SAMPLES: it verifies the first,
 * middle and last stored byte of every live block (any
 * mismatch aborts - allocator corruption evidence), reads
 * its own /proc/self/smaps_rollup (Rss + Pss, the Z-006
 * method) and /proc/self/maps (VMA count, allocation-VMA
 * holes - the Z-053 window definition), and runs a timed
 * latency batch (256 x 64 B alloc/free + 32 x 64 KiB
 * alloc/free, clock_gettime MONOTONIC) reported as ns/op.
 *
 * Output lines:
 *   SOAK,<leg>,<seconds>,<interval_s>
 *   SAMPLE,<leg>,<t_s>,<ops>,<live_count>,<live_bytes>,
 *     <rss_kib>,<pss_kib>,<vma_count>,<vma_delta>,
 *     <avma_count>,<avma_kib>,<hole_count>,<hole_bytes>,
 *     <largest_hole>,<unmapped_bytes>,<gap_bytes>,
 *     <lat_small_ns_op>,<lat_large_ns_op>,<cksum>
 *   DONE,<leg>,<total_ops>,<samples>,<peak_rss_kib>
 * After the deadline the probe frees everything and emits
 * a DRAINED line (rss_kib, pss_kib, vma_count, vma_delta
 * after the full drain) before DONE.
 *
 * Time-boxed, not op-boxed: the two legs run the same
 * duration and the same workload DISTRIBUTION, not the
 * identical op sequence, so checksums are per-process
 * integrity evidence only and are NOT compared across
 * legs (stated plainly, per the honesty law).
 * Exit 0 iff the full duration ran, every scheduled
 * sample was emitted, and no integrity check failed.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef LEG_GLIBC
static void *(*alloc_fn)(size_t) = malloc;
static void *(*calloc_fn)(size_t, size_t) = calloc;
static void *(*realloc_fn)(void *, size_t) = realloc;
static void (*free_fn)(void *) = free;
static const char *leg_name = "glibc";
#else
#include "mimir_alloc.h"
static void *(*alloc_fn)(size_t) = mimir_malloc;
static void *(*calloc_fn)(size_t, size_t) = mimir_calloc;
static void *(*realloc_fn)(void *, size_t) = mimir_realloc;
static void (*free_fn)(void *) = mimir_free;
static const char *leg_name = "mimir";
#endif

#define SLOTS 320
#define MAXVMA 16384

static void *g_ptr[SLOTS];
static size_t g_size[SLOTS];
static unsigned char g_seed[SLOTS];
static long g_ttl[SLOTS];
static long g_base_vma = -1;
static long g_peak_rss = 0;

static uint64_t rng_state = UINT64_C(0x243F6A8885A308D3);
static uint64_t rng_next(void)
{
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return x;
}

static void die(const char *msg)
{
    fprintf(stderr, "probe_z056 FAIL: %s\n", msg);
    exit(1);
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static size_t pick_size(void)
{
    uint64_t r = rng_next() % 100;
    if (r < 55) return (size_t)(16 + (rng_next() % 1009));
    if (r < 80) return (size_t)(4096 + (rng_next() % 61441));
    return (size_t)(131072 + (rng_next() % 1966081));
}

static unsigned char seed_for(long k)
{
    return (unsigned char)((k * 31 + 7) & 0xFF);
}

static void slot_fill(int i, long opno)
{
    size_t n = pick_size();
    unsigned char sd = seed_for(opno % 251);
    void *p;
    if (n <= 1024 && (rng_next() % 8) == 0) {
        p = calloc_fn(1, n);
        if (!p) die("calloc returned NULL");
        const unsigned char *b = (const unsigned char *)p;
        for (size_t k = 0; k < n; k++)
            if (b[k] != 0) die("calloc block not zeroed");
        memset(p, sd, n);
    } else {
        p = alloc_fn(n);
        if (!p) die("alloc returned NULL");
        memset(p, sd, n);
    }
    g_ptr[i] = p;
    g_size[i] = n;
    g_seed[i] = sd;
    g_ttl[i] = 100 + (long)(rng_next() % 19901);
}

static void slot_free(int i)
{
    free_fn(g_ptr[i]);
    g_ptr[i] = NULL;
    g_size[i] = 0;
}

/* smaps_rollup totals (Z-006 method): returns 0 on success. */
static int read_smaps(long *rss_kib, long *pss_kib)
{
    FILE *f = fopen("/proc/self/smaps_rollup", "r");
    if (!f) return -1;
    char buf[256];
    long r = -1, p = -1;
    while (fgets(buf, sizeof buf, f)) {
        if (sscanf(buf, "Rss: %ld kB", &r) == 1) continue;
        if (sscanf(buf, "Pss: %ld kB", &p) == 1) continue;
    }
    fclose(f);
    if (r < 0 || p < 0) return -1;
    *rss_kib = r;
    *pss_kib = p;
    return 0;
}

static uintptr_t vma_lo[MAXVMA];
static uintptr_t vma_hi[MAXVMA];

static int read_maps(void)
{
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return -1;
    char buf[512];
    int n = 0;
    while (fgets(buf, sizeof buf, f)) {
        unsigned long long lo = 0, hi = 0;
        if (sscanf(buf, "%llx-%llx", &lo, &hi) != 2) continue;
        if (n < MAXVMA) {
            vma_lo[n] = (uintptr_t)lo;
            vma_hi[n] = (uintptr_t)hi;
        }
        n++;
    }
    fclose(f);
    return n;
}

/* Timed latency batch: returns ns/op for alloc+free pairs
 * of one fixed size. Runs on scratch allocations only;
 * the soak registry is untouched. */
static long lat_batch(size_t n, int reps)
{
    static void *tmp[256];
    if (reps > 256) reps = 256;
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (int i = 0; i < reps; i++) {
        tmp[i] = alloc_fn(n);
        if (!tmp[i]) die("latency batch alloc NULL");
        ((unsigned char *)tmp[i])[0] = (unsigned char)i;
    }
    for (int i = 0; i < reps; i++) free_fn(tmp[i]);
    clock_gettime(CLOCK_MONOTONIC, &b);
    double ns = ((double)b.tv_sec - (double)a.tv_sec) * 1e9 +
                ((double)b.tv_nsec - (double)a.tv_nsec);
    return (long)(ns / (2.0 * reps));
}

static void sample(long t_s, long ops)
{
    long live_count = 0;
    unsigned long long live_bytes = 0, cksum = 0;
    static void *sp[SLOTS];
    static size_t ss[SLOTS];
    int m = 0;
    for (int i = 0; i < SLOTS; i++) {
        if (!g_ptr[i]) continue;
        unsigned char *b = (unsigned char *)g_ptr[i];
        size_t n = g_size[i];
        if (b[0] != g_seed[i] || b[n / 2] != g_seed[i] ||
            b[n - 1] != g_seed[i])
            die("stored byte mismatch (allocator corruption?)");
        cksum += (unsigned long long)b[0] + b[n / 2] + b[n - 1];
        live_count++;
        live_bytes += (unsigned long long)n;
        sp[m] = g_ptr[i];
        ss[m] = n;
        m++;
    }
    for (int i = 1; i < m; i++) {
        void *tp = sp[i];
        size_t ts = ss[i];
        int j = i - 1;
        while (j >= 0 && (uintptr_t)sp[j] > (uintptr_t)tp) {
            sp[j + 1] = sp[j];
            ss[j + 1] = ss[j];
            j--;
        }
        sp[j + 1] = tp;
        ss[j + 1] = ts;
    }
    unsigned long long gap_bytes = 0;
    for (int i = 1; i < m; i++) {
        uintptr_t prev_end = (uintptr_t)sp[i - 1] + ss[i - 1];
        if ((uintptr_t)sp[i] > prev_end &&
            (uintptr_t)sp[i] - prev_end < ((uintptr_t)64 << 20))
            gap_bytes += (unsigned long long)((uintptr_t)sp[i] - prev_end);
    }
    long rss_kib = -1, pss_kib = -1;
    if (read_smaps(&rss_kib, &pss_kib) != 0) die("smaps read");
    if (rss_kib > g_peak_rss) g_peak_rss = rss_kib;
    int nvma = read_maps();
    if (nvma < 0) die("maps read");
    if (g_base_vma < 0) g_base_vma = nvma;
    /* Allocation-VMA window holes: the Z-053 definition. */
    unsigned long long avma_bytes = 0, win_holes = 0;
    unsigned long long win_unmapped = 0;
    unsigned long long hole_count = 0, largest_hole = 0;
    long avma_count = 0;
    if (m > 0) {
        int lim = nvma < MAXVMA ? nvma : MAXVMA;
        uintptr_t prev_hi = 0;
        int have_prev = 0;
        for (int i = 0; i < lim; i++) {
            int holds = 0;
            for (int j = 0; j < m; j++) {
                uintptr_t b = (uintptr_t)sp[j];
                if (b >= vma_lo[i] && b < vma_hi[i]) {
                    holds = 1;
                    break;
                }
            }
            if (!holds) continue;
            avma_count++;
            avma_bytes += (unsigned long long)(vma_hi[i] - vma_lo[i]);
            if (have_prev && vma_lo[i] > prev_hi) {
                unsigned long long h =
                    (unsigned long long)(vma_lo[i] - prev_hi);
                if (h < (64ULL << 20)) {
                    win_holes += h;
                    hole_count++;
                    if (h > largest_hole) largest_hole = h;
                    uintptr_t cur2 = prev_hi;
                    for (int k = 0; k < lim; k++) {
                        if (vma_hi[k] <= prev_hi ||
                            vma_lo[k] >= vma_lo[i])
                            continue;
                        if (vma_lo[k] > cur2)
                            win_unmapped +=
                                (unsigned long long)(vma_lo[k] - cur2);
                        if (vma_hi[k] > cur2) cur2 = vma_hi[k];
                    }
                    if (vma_lo[i] > cur2)
                        win_unmapped +=
                            (unsigned long long)(vma_lo[i] - cur2);
                }
            }
            prev_hi = vma_hi[i];
            have_prev = 1;
        }
    }
    long lat_s = lat_batch(64, 256);
    long lat_l = lat_batch(65536, 32);
    printf("SAMPLE,%s,%ld,%ld,%ld,%llu,%ld,%ld,%d,%ld,%ld,%llu,%llu,%llu,%llu,%llu,%llu,%ld,%ld,%llu\n",
           leg_name, t_s, ops, live_count, live_bytes, rss_kib,
           pss_kib, nvma, (long)nvma - g_base_vma, avma_count,
           avma_bytes / 1024ULL, hole_count, win_holes,
           largest_hole, win_unmapped, gap_bytes, lat_s, lat_l,
           cksum);
    fflush(stdout);
}

static long g_exp[SLOTS];
static long g_samples = 0;

static void sample_counted(long t_s, long ops)
{
    sample(t_s, ops);
    g_samples++;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: probe_z056 <seconds> <interval_s>\n");
        return 2;
    }
    long seconds = atol(argv[1]);
    long interval = atol(argv[2]);
    if (seconds <= 0 || interval <= 0 || seconds % interval != 0) {
        fprintf(stderr, "probe_z056: seconds must be a positive "
                "multiple of interval_s\n");
        return 2;
    }
    printf("SOAK,%s,%ld,%ld\n", leg_name, seconds, interval);
    fflush(stdout);
    sample_counted(0, 0);
    for (int i = 0; i < SLOTS; i++) slot_fill(i, i);
    for (int i = 0; i < SLOTS; i++)
        g_exp[i] = 100 + (long)(rng_next() % 19901);
    double start = now_s();
    long next_t = interval;
    long ops = 0;
    for (;;) {
        for (int b = 0; b < 256; b++) {
            ops++;
            int i = (int)(rng_next() % SLOTS);
            uint64_t act = rng_next() % 100;
            if (!g_ptr[i] || ops >= g_exp[i]) {
                if (g_ptr[i]) slot_free(i);
                slot_fill(i, ops);
                g_exp[i] = ops + 100 + (long)(rng_next() % 19901);
            } else if (act < 15) {
                /* realloc: grow or shrink in place class;
                 * contents preserved up to the smaller size
                 * (public contract), then re-stamped. */
                size_t oldn = g_size[i];
                unsigned char sd = g_seed[i];
                size_t newn = pick_size();
                void *p = realloc_fn(g_ptr[i], newn);
                if (!p) die("realloc returned NULL");
                unsigned char *bb = (unsigned char *)p;
                size_t keep = oldn < newn ? oldn : newn;
                if (bb[0] != sd || bb[keep / 2] != sd ||
                    bb[keep - 1] != sd)
                    die("realloc contents not preserved");
                unsigned char nsd = seed_for(ops % 251);
                memset(p, nsd, newn);
                g_ptr[i] = p;
                g_size[i] = newn;
                g_seed[i] = nsd;
                g_exp[i] = ops + 100 + (long)(rng_next() % 19901);
            } else if (act < 30) {
                slot_free(i);
                slot_fill(i, ops);
                g_exp[i] = ops + 100 + (long)(rng_next() % 19901);
            }
            /* else: slot survives this visit untouched */
        }
        double elapsed = now_s() - start;
        while (next_t <= seconds && (double)next_t <= elapsed) {
            sample_counted(next_t, ops);
            next_t += interval;
        }
        if (elapsed >= (double)seconds) break;
    }
    while (next_t <= seconds) {
        sample_counted(next_t, ops);
        next_t += interval;
    }
    for (int i = 0; i < SLOTS; i++)
        if (g_ptr[i]) slot_free(i);
    long rss_kib = -1, pss_kib = -1;
    if (read_smaps(&rss_kib, &pss_kib) != 0) die("smaps read");
    int nvma = read_maps();
    if (nvma < 0) die("maps read");
    printf("DRAINED,%s,%ld,%ld,%d,%ld\n", leg_name, rss_kib,
           pss_kib, nvma, (long)nvma - g_base_vma);
    printf("DONE,%s,%ld,%ld,%ld\n", leg_name, ops, g_samples,
           g_peak_rss);
    fflush(stdout);
    return 0;
}
