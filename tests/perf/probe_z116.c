/* probe_z116.c - Z-116 energy-proportional idle probe.
 *
 * One source, two legs:
 *   (default)    mimir: this tree's allocator (public API)
 *   -DLEG_GLIBC  glibc: host malloc family + malloc_trim
 * Usage: probe_z116 <idle_seconds> <sample_s>
 *   idle_seconds must be a positive multiple of sample_s.
 *
 * Shape: BASELINE sample (nothing live) -> LOAD phase
 * (832 blocks, deterministic mix: 512 small 64..1024 B,
 * 256 medium 4..64 KiB, 64 large 256 KiB..1 MiB, every
 * page dense-touched and pattern-verified) -> PEAK sample
 * -> free everything -> DRAINED sample (idle t=0) -> IDLE
 * window: one sample every sample_s for idle_seconds with
 * NO allocator traffic in between (the only calls are the
 * measurement reads themselves: smaps_rollup, maps, and
 * for the mimir leg mimir_stats_snapshot, which takes the
 * allocator lock and flushes the caller's TLS cache but
 * allocates nothing) -> TRIM sample after an explicit
 * full trim (mimir_trim(0) / malloc_trim(0)) -> two
 * post-trim idle samples -> SANITY phase (128 blocks
 * allocated, verified, freed, trimmed) -> FINAL sample.
 *
 * The question of the slice: after load ends, does the
 * retained footprint fall back toward baseline on its own
 * over idle time (energy-proportional idle), or does it
 * sit at the drained level until an explicit trim? The
 * Z-050 policy layer is caller-driven by design (no
 * background thread exists in this allocator), so the
 * no-traffic idle window is the honest test of the
 * allocator's own idle behaviour.
 *
 * Output lines:
 *   IDLEPROBE,<leg>,<idle_seconds>,<sample_s>
 *   SAMPLE,<leg>,<phase>,<t_ms>,<rss_kib>,<pss_kib>,
 *     <vma_count>,<cached_B>,<mapped_B>,<live_B>
 *     mimir: cached=bytes_cached (retained EMPTY spans),
 *            mapped=bytes_mapped (stats snapshot)
 *     glibc: cached=fordblks (free arena bytes),
 *            mapped=arena+hblkhd (mallinfo2)
 *   TRIM,<leg>,<ret>,<cached_before>,<cached_after>
 *   DONE,<leg>,<samples>,<cksum>,<integrity_ok>
 * Exit 0 iff every scheduled sample was emitted and no
 * integrity check failed.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef LEG_GLIBC
#include <malloc.h>
static void *(*alloc_fn)(size_t) = malloc;
static void (*free_fn)(void *) = free;
static int trim_fn(size_t pad) { return malloc_trim(pad); }
static const char *leg_name = "glibc";
#else
#include "mimir_alloc.h"
static void *(*alloc_fn)(size_t) = mimir_malloc;
static void (*free_fn)(void *) = mimir_free;
static int trim_fn(size_t pad) { return mimir_trim(pad); }
static const char *leg_name = "mimir";
#endif

#define NB 832
static void *g_ptr[NB];
static size_t g_size[NB];
static unsigned char g_seed[NB];
static long g_samples = 0;
static unsigned long long g_cksum = 0;
static int g_integrity = 1;

static uint64_t rng_state = UINT64_C(0x9E3779B97F4A7C15);
static uint64_t rng_next(void)
{
    uint64_t x = rng_state;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    rng_state = x;
    return x;
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void die(const char *msg)
{
    fprintf(stderr, "probe_z116 FAIL: %s\n", msg);
    exit(1);
}

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
    *rss_kib = r; *pss_kib = p;
    return 0;
}

static int read_vma_count(void)
{
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return -1;
    char buf[512];
    int n = 0;
    while (fgets(buf, sizeof buf, f)) n++;
    fclose(f);
    return n;
}

static void alloc_state(unsigned long long *cached,
                        unsigned long long *mapped)
{
#ifdef LEG_GLIBC
    struct mallinfo2 mi = mallinfo2();
    *cached = (unsigned long long)mi.fordblks;
    *mapped = (unsigned long long)(mi.arena + mi.hblkhd);
#else
    struct mimir_stats_v1 st;
    if (mimir_stats_snapshot(&st) != 0) die("stats snapshot");
    *cached = (unsigned long long)st.bytes_cached;
    *mapped = (unsigned long long)st.bytes_mapped;
#endif
}

static double g_t0;
static void sample(const char *phase, unsigned long long live_B)
{
    long rss = -1, pss = -1;
    if (read_smaps(&rss, &pss) != 0) die("smaps read");
    int nvma = read_vma_count();
    if (nvma < 0) die("maps read");
    unsigned long long cached = 0, mapped = 0;
    alloc_state(&cached, &mapped);
    long t_ms = (long)((now_s() - g_t0) * 1000.0);
    printf("SAMPLE,%s,%s,%ld,%ld,%ld,%d,%llu,%llu,%llu\n",
           leg_name, phase, t_ms, rss, pss, nvma,
           cached, mapped, live_B);
    fflush(stdout);
    g_samples++;
}

static size_t pick_size(int i)
{
    if (i < 512) return (size_t)(64 + (rng_next() % 961));
    if (i < 768) return (size_t)(4096 + (rng_next() % 61441));
    return (size_t)(262144 + (rng_next() % 786433));
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: probe_z116 <idle_s> <sample_s>\n");
        return 2;
    }
    long idle_s = atol(argv[1]);
    long sample_s = atol(argv[2]);
    if (idle_s <= 0 || sample_s <= 0 || idle_s % sample_s != 0) {
        fprintf(stderr, "probe_z116: idle_s must be a positive "
                "multiple of sample_s\n");
        return 2;
    }
    printf("IDLEPROBE,%s,%ld,%ld\n", leg_name, idle_s, sample_s);
    fflush(stdout);
    g_t0 = now_s();
    sample("baseline", 0);

    unsigned long long live_B = 0;
    for (int i = 0; i < NB; i++) {
        size_t n = pick_size(i);
        unsigned char sd = (unsigned char)((i * 31 + 7) & 0xFF);
        void *p = alloc_fn(n);
        if (!p) die("load alloc returned NULL");
        memset(p, sd, n);
        g_ptr[i] = p; g_size[i] = n; g_seed[i] = sd;
        live_B += (unsigned long long)n;
    }
    for (int i = 0; i < NB; i++) {
        unsigned char *b = (unsigned char *)g_ptr[i];
        size_t n = g_size[i];
        if (b[0] != g_seed[i] || b[n / 2] != g_seed[i] ||
            b[n - 1] != g_seed[i]) {
            g_integrity = 0;
            die("stored byte mismatch at peak");
        }
        g_cksum += (unsigned long long)b[0] + b[n / 2] + b[n - 1];
    }
    sample("peak", live_B);

    for (int i = 0; i < NB; i++) { free_fn(g_ptr[i]); g_ptr[i] = NULL; }
    sample("drained", 0);

    long steps = idle_s / sample_s;
    for (long k = 0; k < steps; k++) {
        sleep((unsigned int)sample_s);
        sample("idle", 0);
    }

    unsigned long long cb = 0, ca = 0, mb = 0, ma = 0;
    alloc_state(&cb, &mb);
    int tret = trim_fn(0);
    alloc_state(&ca, &ma);
    printf("TRIM,%s,%d,%llu,%llu\n", leg_name, tret, cb, ca);
    fflush(stdout);
    sample("post_trim", 0);

    for (int k = 0; k < 2; k++) {
        sleep((unsigned int)sample_s);
        sample("post_trim_idle", 0);
    }

    unsigned long long san_B = 0;
    static void *sp[128];
    static size_t ss[128];
    for (int i = 0; i < 128; i++) {
        size_t n = (size_t)(64 + (rng_next() % 4097));
        void *p = alloc_fn(n);
        if (!p) die("sanity alloc returned NULL");
        memset(p, (unsigned char)(i & 0xFF), n);
        sp[i] = p; ss[i] = n; san_B += (unsigned long long)n;
    }
    for (int i = 0; i < 128; i++) {
        unsigned char *b = (unsigned char *)sp[i];
        if (b[0] != (unsigned char)(i & 0xFF) ||
            b[ss[i] - 1] != (unsigned char)(i & 0xFF)) {
            g_integrity = 0;
            die("sanity byte mismatch");
        }
        g_cksum += b[0];
    }
    for (int i = 0; i < 128; i++) free_fn(sp[i]);
    (void)trim_fn(0);
    sample("final", 0);

    printf("DONE,%s,%ld,%llu,%d\n", leg_name, g_samples,
           g_cksum, g_integrity);
    fflush(stdout);
    return g_integrity ? 0 : 1;
}
