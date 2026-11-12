/* probe_z053.c - Z-053 large-path fragmentation.
 *
 * One source, two legs (the K-002 discipline):
 *   (default)    mimir: this tree's banked allocator
 *                (Z-048 mapping cache + Z-049 THP live)
 *   -DLEG_GLIBC  glibc: host malloc/free
 * Usage: probe_z053 <pattern>
 *   pattern in {checker, uniform, pinwheel, churn}
 * Every pattern allocates ONLY large-path sizes (all
 * requests > the 1024-byte small cutoff: 16 KiB..2 MiB),
 * dense-touches every block (memset of the whole extent,
 * so RSS is real resident memory, not reservation), and
 * stops at defined phases. At each phase the probe reads
 * its OWN /proc/self/smaps_rollup (Rss + Pss - the Z-006
 * method; VmRSS spot readings are NOT used, per the
 * Z-047 census lesson) and /proc/self/maps (VMA count,
 * mapped bytes and unmapped holes inside the address
 * window spanned by the live blocks).
 *
 * Output lines:
 *   PROC,<leg>,<pattern>
 *   CKPT,<leg>,<pattern>,<phase>,<live_count>,
 *     <live_bytes>,<vma_count>,<vma_delta>,<rss_kib>,
 *     <pss_kib>,<avma_count>,<avma_kib>,<win_hole_count>,
 *     <win_hole_bytes>,<win_largest_hole>,<gap_bytes>,
 *     <cksum>
 * vma_delta is against the same process's BASE phase.
 * gap_bytes sums the positive address gaps between
 * consecutive live blocks (address order, requested
 * sizes) - address-space scattering between live blocks.
 * cksum sums the first+last stored byte of every live
 * block; the stored bytes are a deterministic function
 * of the operation sequence, so cksum must be IDENTICAL
 * across legs at every phase (workload equivalence).
 * Exit 0 iff every allocation succeeded and every phase
 * was read cleanly.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef LEG_GLIBC
static void *(*alloc_fn)(size_t) = malloc;
static void (*free_fn)(void *) = free;
static const char *leg_name = "glibc";
#else
#include "mimir_alloc.h"
static void *(*alloc_fn)(size_t) = mimir_malloc;
static void (*free_fn)(void *) = mimir_free;
static const char *leg_name = "mimir";
#endif

#define MAXLIVE 512
#define MAXVMA 16384

static void *g_ptr[MAXLIVE];
static size_t g_size[MAXLIVE];
static unsigned char g_seed[MAXLIVE];
static int g_n;
static long g_base_vma = -1;
static const char *g_pattern = "?";

static uint64_t rng_state = UINT64_C(0x9E3779B97F4A7C15);
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
    fprintf(stderr, "probe_z053 FAIL: %s\n", msg);
    exit(1);
}

static int reg_alloc(size_t n, unsigned char seed)
{
    if (g_n >= MAXLIVE) die("registry full");
    void *p = alloc_fn(n);
    if (!p) die("alloc returned NULL");
    memset(p, seed, n);
    g_ptr[g_n] = p;
    g_size[g_n] = n;
    g_seed[g_n] = seed;
    return g_n++;
}

static void reg_free(int i)
{
    if (i < 0 || i >= g_n || !g_ptr[i]) die("bad free");
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

static void checkpoint(const char *phase)
{
    long live_count = 0;
    unsigned long long live_bytes = 0;
    unsigned long long cksum = 0;
    uintptr_t lo = UINTPTR_MAX, hi = 0;
    /* address-sorted copy of live blocks for gap analysis */
    static void *sp[MAXLIVE];
    static size_t ss[MAXLIVE];
    int m = 0;
    for (int i = 0; i < g_n; i++) {
        if (!g_ptr[i]) continue;
        unsigned char *b = (unsigned char *)g_ptr[i];
        if (b[0] != g_seed[i] || b[g_size[i] - 1] != g_seed[i])
            die("stored byte mismatch (allocator corruption?)");
        cksum += (unsigned long long)b[0] +
                 (unsigned long long)b[g_size[i] - 1];
        live_count++;
        live_bytes += (unsigned long long)g_size[i];
        if ((uintptr_t)b < lo) lo = (uintptr_t)b;
        if ((uintptr_t)b + g_size[i] > hi)
            hi = (uintptr_t)b + g_size[i];
        sp[m] = g_ptr[i];
        ss[m] = g_size[i];
        m++;
    }
    /* insertion sort by address (m <= MAXLIVE, phases are rare) */
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
    int nvma = read_maps();
    if (nvma < 0) die("maps read");
    if (g_base_vma < 0) g_base_vma = nvma;
    /* Allocation VMAs: VMAs holding at least one live
     * block. Holes are the unmapped gaps BETWEEN two
     * allocation VMAs; gaps of 64 MiB or more separate
     * distinct regions (heap vs mmap area), not holes. */
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
                    /* unmapped part of this gap: walk every
                     * VMA strictly between the two alloc
                     * VMAs (pooled mappings live there). */
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
    printf("CKPT,%s,%s,%s,%ld,%llu,%d,%ld,%ld,%ld,%ld,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
           leg_name, g_pattern, phase, live_count, live_bytes,
           nvma, (long)nvma - g_base_vma, rss_kib, pss_kib, avma_count,
           avma_bytes / 1024ULL, hole_count, win_holes,
           largest_hole, win_unmapped, gap_bytes, cksum);
    fflush(stdout);
}

static unsigned char seed_for(int k)
{
    return (unsigned char)((k * 7 + 1) & 0xFF);
}

/* checker: alternating 64 KiB / 256 KiB, free the 256 KiB
 * blocks (size-mismatched holes), refill same sizes. */
static void pat_checker(void)
{
    enum { N = 192 };
    for (int i = 0; i < N; i++)
        reg_alloc((i % 2 == 0) ? (size_t)65536 : (size_t)262144,
                  seed_for(i));
    checkpoint("FULL");
    for (int i = 1; i < N; i += 2) reg_free(i);
    checkpoint("HOLED");
    for (int i = 1; i < N; i += 2)
        reg_alloc((size_t)262144, seed_for(1000 + i));
    checkpoint("REFILL");
    for (int i = 0; i < g_n; i++)
        if (g_ptr[i]) reg_free(i);
    checkpoint("DRAINED");
}

/* uniform: 256 x 128 KiB, free odd (exact-size holes), refill. */
static void pat_uniform(void)
{
    enum { N = 256 };
    for (int i = 0; i < N; i++)
        reg_alloc((size_t)131072, seed_for(i));
    checkpoint("FULL");
    for (int i = 1; i < N; i += 2) reg_free(i);
    checkpoint("HOLED");
    int added = 0;
    for (int i = 1; i < N; i += 2) {
        reg_alloc((size_t)131072, seed_for(2000 + i));
        added++;
    }
    (void)added;
    checkpoint("REFILL");
    for (int i = 0; i < g_n; i++)
        if (g_ptr[i]) reg_free(i);
    checkpoint("DRAINED");
}

/* pinwheel: 128 blocks cycling 16 KiB / 1 MiB / 32 KiB /
 * 512 KiB (small-large interleave), released in thirds. */
static void pat_pinwheel(void)
{
    static const size_t cyc[4] = {
        (size_t)16384, (size_t)1048576,
        (size_t)32768, (size_t)524288
    };
    enum { N = 128 };
    for (int i = 0; i < N; i++)
        reg_alloc(cyc[i % 4], seed_for(i));
    checkpoint("FULL");
    for (int i = 0; i < N; i++)
        if (i % 3 == 0) reg_free(i);
    checkpoint("THIRD1");
    for (int i = 0; i < N; i++)
        if (i % 3 == 1) reg_free(i);
    checkpoint("THIRD2");
    for (int i = 0; i < N; i++)
        if (g_ptr[i]) reg_free(i);
    checkpoint("DRAINED");
}

/* churn: long run - a 96-slot live window, 6000 replace
 * operations (free one slot, allocate a fresh large size
 * into it, dense-touch). Sizes and slot choices come from
 * one fixed-seed PRNG, so both legs execute the identical
 * operation sequence. Checkpoints every 1500 ops record
 * the VMA / RSS trajectory over the long run. */
static void pat_churn(void)
{
    static const size_t set[8] = {
        (size_t)16384, (size_t)32768, (size_t)65536,
        (size_t)131072, (size_t)262144, (size_t)524288,
        (size_t)1048576, (size_t)2097152
    };
    enum { W = 96, OPS = 6000 };
    for (int i = 0; i < W; i++)
        reg_alloc(set[rng_next() % 8], seed_for(i));
    checkpoint("OP0");
    for (int op = 1; op <= OPS; op++) {
        int slot = (int)(rng_next() % W);
        size_t n = set[rng_next() % 8];
        unsigned char sd = seed_for(op % 251);
        free_fn(g_ptr[slot]);
        void *p = alloc_fn(n);
        if (!p) die("churn alloc returned NULL");
        memset(p, sd, n);
        g_ptr[slot] = p;
        g_size[slot] = n;
        g_seed[slot] = sd;
        if (op == 1500) checkpoint("OP1500");
        if (op == 3000) checkpoint("OP3000");
        if (op == 4500) checkpoint("OP4500");
        if (op == 6000) checkpoint("OP6000");
    }
    for (int i = 0; i < W; i++) reg_free(i);
    checkpoint("DRAINED");
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: probe_z053 <pattern>\n");
        return 2;
    }
    g_pattern = argv[1];
    printf("PROC,%s,%s\n", leg_name, g_pattern);
    fflush(stdout);
    checkpoint("BASE");
    if (strcmp(g_pattern, "checker") == 0) pat_checker();
    else if (strcmp(g_pattern, "uniform") == 0) pat_uniform();
    else if (strcmp(g_pattern, "pinwheel") == 0) pat_pinwheel();
    else if (strcmp(g_pattern, "churn") == 0) pat_churn();
    else {
        fprintf(stderr, "unknown pattern %s\n", g_pattern);
        return 2;
    }
    return 0;
}
