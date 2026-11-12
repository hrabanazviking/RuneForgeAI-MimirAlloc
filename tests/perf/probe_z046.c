/* probe_z046.c - Z-046 the threshold study probe.
 *
 * Two studies in one binary, on this host only:
 *
 * A. glibc dynamic mmap-threshold behaviour, proven in
 *    FRESH processes from mallinfo2 counters taken WHILE a
 *    block is held (hblks = live mmapped chunks):
 *      threshone <size>
 *        Virgin-process verdict for one request size:
 *        MMAP (hblks 0->1) or ARENA (arena grows, hblks 0).
 *        One size per process, so the dynamic threshold
 *        (which a freed mmap chunk RAISES) can never
 *        pollute a later reading.
 *      threshdyn <free_size> <probe_size>
 *        Allocate+free free_size, then allocate probe_size
 *        and record its verdict: the dynamic rise, its
 *        reach, and its cap, pair by pair, fresh process
 *        per pair.
 *      threshstay <free_size> <probe_size>
 *        As threshdyn, but 2000 small alloc/free cycles
 *        run between the rise and the probe: does the
 *        raised threshold decay with ordinary traffic?
 *      threshmallopt <set> <free_size> <probe_size>
 *        mallopt(M_MMAP_THRESHOLD, set) first, then as
 *        threshdyn: an explicit setting's documented side
 *        effect is that dynamic adjustment stops.
 *
 * B. Per-size cost of MimirAlloc's fixed 1024-byte cutoff
 *    (MIMIR_SMALL_CUTOFF; every request above it takes a
 *    fresh mapping per allocation, alloc_locked) across
 *    the ladder 1 KiB..32 MiB, under the Z-004 protocol
 *    (runner pins CPU 2, warm-up processes discarded,
 *    legs interleaved, headline = median of processes):
 *      op <mimir|glibc|glmap|glarena|gltrim> <size> <iters>
 *        Single-size alloc + bench endpoint touch + free,
 *        one block live. Legs:
 *          mimir   : the banked core (mapping per alloc
 *                    above 1024; span path at/below).
 *          glibc   : host glibc, default dynamic threshold
 *                    (steady state: after warm-up the
 *                    threshold has risen over the whole
 *                    ladder, so this leg prices REUSE).
 *          glmap   : glibc with mallopt threshold 128:
 *                    every request in the ladder mmaps
 *                    fresh and munmaps on free - glibc's
 *                    own mapping-per-allocation price.
 *          glarena : glibc with mallopt mmap threshold
 *                    64 MiB AND trim threshold 64 MiB:
 *                    nothing in the ladder mmaps and the
 *                    arena RETAINS freed memory; pure reuse
 *                    price. Stands
 *                    as the measured proxy for what a
 *                    reuse-served size costs, i.e. what a
 *                    raised MimirAlloc cutoff would have
 *                    to approach (a future MimirAlloc
 *                    reuse path would pay its own
 *                    bookkeeping on top - proxy, not a
 *                    promise; labelled wherever used).
 *          gltrim  : glibc with ONLY the mmap threshold
 *                    raised (64 MiB); the default 128 KiB
 *                    trim threshold stays, so every large
 *                    free shrinks the arena top back to
 *                    the OS. The retention-policy control:
 *                    reuse WITHOUT retention, priced.
 *      raw <notouch|touch> <size> <iters>
 *        The bare mapping pair in EXACTLY MimirAlloc's
 *        geometry (round_up(4096+size,4096),
 *        PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,
 *        user at base+4096), as probe_z023: pair =
 *        raw/notouch; faults+populated teardown =
 *        raw/touch - raw/notouch.
 *      opfull <mimir|glibc> <size> <iters>
 *        Dense use: every user page written and read back
 *        each iteration. Prices the fault term a sparse
 *        bench hides (mimir re-faults every page on every
 *        allocation; a reuse allocator faults them once).
 *
 * Checksum discipline (K-002, protocol s.9): every
 * endpoint-touch leg at a size shares exactly one
 * checksum; opfull legs share their own per size.
 * Timing CLOCK_MONOTONIC_RAW; minor faults from
 * /proc/self/stat field 10 around the timed section.
 * CSV (cost lines): phase,leg,size,live,iters,total_ns,
 * ns_per_op,minflt_delta,checksum
 * Threshold lines: T1/TD/TS/TM records, see emitters.
 */
#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif
#include <time.h>
#include <unistd.h>

#include "mimir_alloc.h"

/* touch(): the bench_k002.c endpoint touch, copied
 * byte-for-byte so checksums are comparable with
 * every earlier slice (K-002 discipline): writes
 * the first and last byte, returns their xor. */
static unsigned char touch(void *ptr, size_t n, unsigned seed)
{
    unsigned char *b = (unsigned char *)ptr;
    b[0] = (unsigned char)(seed & 0xFFu);
    b[n - 1] = (unsigned char)((seed >> 3) & 0xFFu);
    return (unsigned char)(b[0] ^ b[n - 1]);
}

static void *(*const MIMIR_AF)(size_t) = mimir_malloc;
static void (*const MIMIR_FF)(void *) = mimir_free;
static void *(*const GLIBC_AF)(size_t) = malloc;
static void (*const GLIBC_FF)(void *) = free;

static long long z46_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (long long)ts.tv_sec * 1000000000LL +
           (long long)ts.tv_nsec;
}

static long read_minflt(void)
{
    FILE *f = fopen("/proc/self/stat", "r");
    if (!f) return -1;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    if (n == 0) return -1;
    buf[n] = 0;
    char *rp = strrchr(buf, ')');
    if (!rp) return -1;
    char *p = rp + 1;
    long val = -1;
    for (int tok = 1; tok <= 8; tok++) {
        while (*p == ' ') p++;
        if (!*p) return -1;
        if (tok == 8) val = strtol(p, NULL, 10);
        while (*p && *p != ' ') p++;
    }
    return val;
}

static size_t map_len_for(size_t size)
{
    size_t total = 4096u + size;
    return (total + 4095u) & ~(size_t)4095u;
}

static void emit(const char *phase, const char *leg, size_t size,
                 long live, long iters, long long total_ns,
                 long minflt_delta, uint64_t chk)
{
    printf("%s,%s,%zu,%ld,%ld,%lld,%.2f,%ld,%llu\n",
           phase, leg, size, live, iters, total_ns,
           iters > 0 ? (double)total_ns / (double)iters : 0.0,
           minflt_delta, (unsigned long long)chk);
}

/* ---- cost legs ---- */

/* Apply a leg's mallopt posture; returns 0 on success. */
static int leg_setup(const char *leg)
{
    if (strcmp(leg, "glmap") == 0)
        return mallopt(M_MMAP_THRESHOLD, 128) == 1 ? 0 : 1;
    if (strcmp(leg, "glarena") == 0)
        return (mallopt(M_MMAP_THRESHOLD, 67108864) == 1 &&
                mallopt(M_TRIM_THRESHOLD, 67108864) == 1) ? 0 : 1;
    if (strcmp(leg, "gltrim") == 0)
        return mallopt(M_MMAP_THRESHOLD, 67108864) == 1 ? 0 : 1;
    return 0;
}

static int phase_op(void *(*af)(size_t), void (*ff)(void *),
                    const char *leg, size_t size, long iters)
{
    long warm = iters / 10, i;
    uint64_t chk = 0;
    if (leg_setup(leg) != 0) return 1;
    for (i = 0; i < warm; i++) {
        void *p = af(size);
        if (!p) return 1;
        (void)touch(p, size, (unsigned)i);
        ff(p);
    }
    long f0 = read_minflt();
    long long t0 = z46_now_ns();
    for (i = 0; i < iters; i++) {
        void *p = af(size);
        if (!p) return 1;
        chk += touch(p, size, (unsigned)i);
        ff(p);
    }
    long long t1 = z46_now_ns();
    long f1 = read_minflt();
    if (f0 < 0 || f1 < 0) return 1;
    emit("op", leg, size, 1, iters, t1 - t0, f1 - f0, chk);
    return 0;
}

static int phase_opfull(void *(*af)(size_t), void (*ff)(void *),
                        const char *leg, size_t size, long iters)
{
    long warm = iters / 10 > 0 ? iters / 10 : 1, i;
    size_t pages = size / 4096u, pg;
    uint64_t chk = 0;
    if (leg_setup(leg) != 0) return 1;
    for (i = 0; i < warm; i++) {
        unsigned char *b = af(size);
        if (!b) return 1;
        for (pg = 0; pg < pages; pg++)
            b[pg * 4096u] = (unsigned char)(pg + (size_t)i);
        ff(b);
    }
    long f0 = read_minflt();
    long long t0 = z46_now_ns();
    for (i = 0; i < iters; i++) {
        unsigned char *b = af(size);
        uint64_t s = 0;
        if (!b) return 1;
        for (pg = 0; pg < pages; pg++)
            b[pg * 4096u] = (unsigned char)(pg + (size_t)i);
        for (pg = 0; pg < pages; pg++)
            s += b[pg * 4096u];
        chk += s;
        ff(b);
    }
    long long t1 = z46_now_ns();
    long f1 = read_minflt();
    if (f0 < 0 || f1 < 0) return 1;
    emit("opfull", leg, size, 1, iters, t1 - t0, f1 - f0, chk);
    return 0;
}
static int phase_raw(const char *mode, size_t size, long iters)
{
    size_t len = map_len_for(size);
    long warm = iters / 10, i;
    uint64_t chk = 0;
    int do_touch = strcmp(mode, "notouch") != 0;
    for (i = 0; i < warm; i++) {
        char *base = mmap(NULL, len, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) return 1;
        if (do_touch)
            (void)touch(base + 4096u, size, (unsigned)i);
        if (munmap(base, len) != 0) return 1;
    }
    long f0 = read_minflt();
    long long t0 = z46_now_ns();
    for (i = 0; i < iters; i++) {
        char *base = mmap(NULL, len, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) return 1;
        if (do_touch)
            chk += touch(base + 4096u, size, (unsigned)i);
        if (munmap(base, len) != 0) return 1;
    }
    long long t1 = z46_now_ns();
    long f1 = read_minflt();
    if (f0 < 0 || f1 < 0) return 1;
    emit("raw", mode, size, 1, iters, t1 - t0, f1 - f0, chk);
    return 0;
}

/* ---- threshold legs (glibc only, fresh processes) ---- */

static volatile unsigned char thresh_sink;

/* Allocate size, touch both ends through volatile (the
 * allocation must not be elided), snapshot mallinfo2
 * WHILE held, free. Returns hblks during, or -1. Outputs
 * hblkhd/arena/uordblks during via pointers when given. */
static long held_hblks(size_t size, size_t *hd_out,
                       size_t *arena_out, size_t *uord_out)
{
    unsigned char *p = malloc(size);
    struct mallinfo2 mi;
    if (!p) return -1;
    p[0] = 1;
    p[size - 1] = 2;
    thresh_sink = (unsigned char)(p[0] + p[size - 1]);
    mi = mallinfo2();
    if (hd_out) *hd_out = mi.hblkhd;
    if (arena_out) *arena_out = mi.arena;
    if (uord_out) *uord_out = mi.uordblks;
    long hb = (long)mi.hblks;
    free(p);
    return hb;
}

static int phase_threshone(size_t size)
{
    size_t hd = 0, ar = 0, uo = 0;
    long hb = held_hblks(size, &hd, &ar, &uo);
    if (hb < 0) return 1;
    printf("T1,%zu,%ld,%zu,%zu,%zu,%s\n", size, hb, hd, ar, uo,
           hb > 0 ? "MMAP" : "ARENA");
    return 0;
}

static int thresh_pair(const char *tag, long set_thr,
                       size_t fsize, size_t psize, int stay)
{
    size_t fhd = 0, phd = 0;
    long fhb, phb;
    int i;
    if (set_thr >= 0 &&
        mallopt(M_MMAP_THRESHOLD, (int)set_thr) != 1)
        return 1;
    fhb = held_hblks(fsize, &fhd, NULL, NULL);
    if (fhb < 0) return 1;
    if (stay) {
        for (i = 0; i < 2000; i++) {
            void *q = malloc(4096);
            if (!q) return 1;
            ((volatile unsigned char *)q)[0] = (unsigned char)i;
            free(q);
        }
    }
    phb = held_hblks(psize, &phd, NULL, NULL);
    if (phb < 0) return 1;
    if (set_thr >= 0)
        printf("%s,%ld,%zu,%ld,%zu,%ld,%zu,%s\n", tag, set_thr,
               fsize, fhb, fhd, phb, phd,
               phb > 0 ? "MMAP" : "ARENA");
    else
        printf("%s,%zu,%ld,%zu,%ld,%zu,%s\n", tag, fsize, fhb,
               fhd, phb, phd, phb > 0 ? "MMAP" : "ARENA");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "op") == 0 && argc == 5) {
        size_t size = (size_t)strtoull(argv[3], NULL, 10);
        long iters = strtol(argv[4], NULL, 10);
        if (strcmp(argv[2], "mimir") == 0)
            return phase_op(MIMIR_AF, MIMIR_FF, "mimir", size, iters);
        if (strcmp(argv[2], "glibc") == 0)
            return phase_op(GLIBC_AF, GLIBC_FF, "glibc", size, iters);
        if (strcmp(argv[2], "glmap") == 0)
            return phase_op(GLIBC_AF, GLIBC_FF, "glmap", size, iters);
        if (strcmp(argv[2], "glarena") == 0)
            return phase_op(GLIBC_AF, GLIBC_FF, "glarena", size, iters);
        if (strcmp(argv[2], "gltrim") == 0)
            return phase_op(GLIBC_AF, GLIBC_FF, "gltrim", size, iters);
    } else if (argc >= 2 && strcmp(argv[1], "opfull") == 0 &&
               argc == 5) {
        size_t size = (size_t)strtoull(argv[3], NULL, 10);
        long iters = strtol(argv[4], NULL, 10);
        if (strcmp(argv[2], "mimir") == 0)
            return phase_opfull(MIMIR_AF, MIMIR_FF, "mimir", size, iters);
        if (strcmp(argv[2], "glibc") == 0)
            return phase_opfull(GLIBC_AF, GLIBC_FF, "glibc", size, iters);
    } else if (argc >= 2 && strcmp(argv[1], "raw") == 0 &&
               argc == 5) {
        size_t size = (size_t)strtoull(argv[3], NULL, 10);
        long iters = strtol(argv[4], NULL, 10);
        return phase_raw(argv[2], size, iters);
    } else if (argc >= 2 && strcmp(argv[1], "threshone") == 0 &&
               argc == 3) {
        return phase_threshone((size_t)strtoull(argv[2], NULL, 10));
    } else if (argc >= 2 && strcmp(argv[1], "threshdyn") == 0 &&
               argc == 4) {
        return thresh_pair("TD", -1,
                           (size_t)strtoull(argv[2], NULL, 10),
                           (size_t)strtoull(argv[3], NULL, 10), 0);
    } else if (argc >= 2 && strcmp(argv[1], "threshstay") == 0 &&
               argc == 4) {
        return thresh_pair("TS", -1,
                           (size_t)strtoull(argv[2], NULL, 10),
                           (size_t)strtoull(argv[3], NULL, 10), 1);
    } else if (argc >= 2 && strcmp(argv[1], "threshmallopt") == 0 &&
               argc == 5) {
        return thresh_pair("TM", strtol(argv[2], NULL, 10),
                           (size_t)strtoull(argv[3], NULL, 10),
                           (size_t)strtoull(argv[4], NULL, 10), 0);
    }
    fprintf(stderr,
            "usage: probe_z046 op <mimir|glibc|glmap|glarena|gltrim> <size> <iters>\n"
            "       probe_z046 opfull <mimir|glibc> <size> <iters>\n"
            "       probe_z046 raw <notouch|touch> <size> <iters>\n"
            "       probe_z046 threshone <size>\n"
            "       probe_z046 threshdyn <free_size> <probe_size>\n"
            "       probe_z046 threshstay <free_size> <probe_size>\n"
            "       probe_z046 threshmallopt <set> <free_size> <probe_size>\n");
    return 2;
}
