/* probe_z112.c - Z-112 huge-page-only large-pool variant.
 * Host/kernel probe (no allocator under test, Z-049
 * pattern): prices the POOL-MISS shapes of the banked
 * Z-048 large pool (first / unique / over-bound sizes,
 * where the exact-length 64-entry / 32 MiB pool cannot
 * serve and every allocation pays the fresh-mapping
 * price) under two mapping strategies:
 *   banked  - the banked pooled shape: mmap(4096 + size),
 *             descriptor byte in page 0, usable extent at
 *             base + 4096 (one page off PMD alignment),
 *             no huge-page advise (the banked core issues
 *             no MADV_HUGEPAGE anywhere - grep-verified).
 *   variant - the huge-page-only variant shape: mapping
 *             padded so the USABLE extent is 2 MiB (PMD)
 *             aligned, descriptor in the page immediately
 *             before the usable extent, MADV_HUGEPAGE on
 *             the usable extent only. By construction the
 *             variant admits only usable sizes >= 2 MiB;
 *             the 64 KiB control size is measured banked-
 *             only as the no-regression witness (the
 *             variant never serves it, the banked pool
 *             keeps it unchanged).
 * Phases per (size, leg): untouched (alloc + free, no
 * touch - the Z-051 LOSS 1.10-1.22x shape) and touched
 * (alloc, one byte per 4 KiB page over the whole usable
 * extent timed + minor-fault delta + smaps AnonHugePages
 * of the usable VMA, read-sum checksum, free).
 * REPS reps per config, leg order alternating per rep;
 * A pool-hit witness phase (sub-PMD cycling reuse vs the
 * fresh price the variant would impose there) prices the
 * replacement regression. Raw rep lines then per-config
 * median lines. Exit 0 iff
 * every step succeeded and the read checksum is identical
 * across legs and reps for each size (K-002 discipline). */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#ifndef MADV_HUGEPAGE
#define MADV_HUGEPAGE 14
#endif
#define PMD ((size_t)2 * 1024 * 1024)
#define PG ((size_t)4096)
#define REPS 5
static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
static long read_minflt(void) {
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
static int smaps_huge(const volatile void *addr, long *huge_kib) {
    FILE *f = fopen("/proc/self/smaps", "r");
    if (!f) return -1;
    char line[512];
    unsigned long target = (unsigned long)(uintptr_t)addr;
    int in_vma = 0, found = 0;
    long huge = -1;
    while (fgets(line, sizeof line, f)) {
        unsigned long lo, hi;
        char extra;
        if (sscanf(line, "%lx-%lx %c", &lo, &hi, &extra) == 3 &&
            line[0] >= '0' && line[0] <= '9') {
            if (in_vma && found) break;
            in_vma = (target >= lo && target < hi);
            if (in_vma) { found = 1; huge = -1; }
            continue;
        }
        if (in_vma) {
            long v;
            if (sscanf(line, "AnonHugePages: %ld kB", &v) == 1)
                huge = v;
        }
    }
    fclose(f);
    if (!found || huge < 0) return -1;
    *huge_kib = huge;
    return 0;
}
struct meas {
    long long pair_ns;    /* untouched alloc+free */
    long long total_ns;   /* touched alloc+touch+free */
    long long touch_ns;
    long minflt;
    long huge_kib;
    unsigned long long cksum;
};
/* One measurement. variant=0 banked shape, 1 variant shape.
 * Returns 0 ok, -1 fail. */
static int measure(size_t size, int variant, struct meas *m) {
    size_t maplen;
    unsigned char *base, *usable;
    long long ta = now_ns();
    if (!variant) {
        maplen = PG + ((size + PG - 1) / PG) * PG;
        base = mmap(NULL, maplen, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) return -1;
        usable = base + PG;
    } else {
        maplen = PMD + ((size + PG - 1) / PG) * PG;
        base = mmap(NULL, maplen, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) return -1;
        uintptr_t u = ((uintptr_t)base + PG + PMD - 1) &
                      ~(uintptr_t)(PMD - 1);
        usable = (unsigned char *)u;
        if (madvise(usable, size, MADV_HUGEPAGE) != 0)
            return -1;
    }
    /* descriptor byte, as the allocator publishes it */
    usable[-4096] = 0x5a;
    /* untouched pair: a second fresh mapping, alloc+free */
    {
        size_t l2 = maplen;
        long long t0 = now_ns();
        void *b2 = mmap(NULL, l2, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (b2 == MAP_FAILED) return -1;
        if (munmap(b2, l2) != 0) return -1;
        m->pair_ns = now_ns() - t0;
    }
    long f0 = read_minflt();
    long long t0 = now_ns();
    unsigned long long sum = 0;
    for (size_t off = 0; off < size; off += PG) {
        usable[off] = (unsigned char)(off / PG);
    }
    long long t1 = now_ns();
    for (size_t off = 0; off < size; off += PG)
        sum += usable[off];
    long hk = -1;
    if (smaps_huge(usable, &hk) != 0) return -1;
    long f1 = read_minflt();
    if (munmap(base, maplen) != 0) return -1;
    long long t2 = now_ns();
    m->touch_ns = t1 - t0;
    m->total_ns = t2 - ta;
    m->minflt = (f0 >= 0 && f1 >= 0) ? (f1 - f0) : -1;
    m->huge_kib = hk;
    m->cksum = sum;
    return 0;
}
/* Pool-hit witness: cycling exact-length reuse at
 * sub-PMD sizes (the banked pool's home shape, K-002
 * large_batch class). banked leg: one fresh mapping then
 * CYCLES-1 reuses of the cached mapping (endpoint touch,
 * K-002 shape). variant leg: the huge-page-only pool
 * admits nothing below 2 MiB, so every cycle pays the
 * plain fresh-mapping price in the banked shape. Prices
 * the regression a REPLACEMENT adoption would cause. */
#define CYCLES 200
static int witness(size_t size, long long *hit_ns, long long *miss_ns) {
    size_t maplen = PG + ((size + PG - 1) / PG) * PG;
    unsigned char *base = mmap(NULL, maplen, PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) return -1;
    unsigned char *usable = base + PG;
    base[0] = 0x5a;
    long long t0 = now_ns();
    for (int i = 0; i < CYCLES; i++) {
        usable[0] = (unsigned char)i;
        usable[size - 1] = (unsigned char)i;
        if (usable[0] + usable[size - 1] != 2 * (unsigned char)i)
            return -1;
    }
    *hit_ns = (now_ns() - t0) / CYCLES;
    if (munmap(base, maplen) != 0) return -1;
    t0 = now_ns();
    for (int i = 0; i < CYCLES; i++) {
        unsigned char *b = mmap(NULL, maplen, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (b == MAP_FAILED) return -1;
        b[0] = 0x5a;
        b[PG] = (unsigned char)i;
        b[PG + size - 1] = (unsigned char)i;
        if (munmap(b, maplen) != 0) return -1;
    }
    *miss_ns = (now_ns() - t0) / CYCLES;
    return 0;
}
static int cmp_ll(const void *a, const void *b) {
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}
static int cmp_l(const void *a, const void *b) {
    long x = *(const long *)a, y = *(const long *)b;
    return (x > y) - (x < y);
}
int main(void) {
    static const size_t sizes[] = {
        65536, 2097152, 4194304, 20971520, 41943040
    };
    int fail = 0;
    for (size_t si = 0; si < sizeof sizes / sizeof sizes[0]; si++) {
        size_t size = sizes[si];
        int legs = (size >= PMD) ? 2 : 1;
        unsigned long long ref = 0;
        int have_ref = 0;
        struct meas rs[2][REPS];
        for (int r = 0; r < REPS; r++) {
            for (int li = 0; li < legs; li++) {
                int leg = (r % 2 == 0) ? li : (legs - 1 - li);
                if (measure(size, leg, &rs[leg][r]) != 0) {
                    printf("FAIL size=%zu leg=%d rep=%d\n",
                           size, leg, r);
                    return 1;
                }
                printf("raw,size=%zu,leg=%s,pair_ns=%lld,"
                       "total_ns=%lld,touch_ns=%lld,minflt=%ld,"
                       "huge_kib=%ld,cksum=%llu\n",
                       size, leg ? "variant" : "banked",
                       rs[leg][r].pair_ns, rs[leg][r].total_ns,
                       rs[leg][r].touch_ns, rs[leg][r].minflt,
                       rs[leg][r].huge_kib, rs[leg][r].cksum);
                fflush(stdout);
                if (!have_ref) { ref = rs[leg][r].cksum; have_ref = 1; }
                else if (rs[leg][r].cksum != ref) fail = 1;
            }
        }
        for (int leg = 0; leg < legs; leg++) {
            long long pair[REPS], tot[REPS], tch[REPS];
            long mf[REPS], hg[REPS];
            for (int r = 0; r < REPS; r++) {
                pair[r] = rs[leg][r].pair_ns;
                tot[r] = rs[leg][r].total_ns;
                tch[r] = rs[leg][r].touch_ns;
                mf[r] = rs[leg][r].minflt;
                hg[r] = rs[leg][r].huge_kib;
            }
            qsort(pair, REPS, sizeof *pair, cmp_ll);
            qsort(tot, REPS, sizeof *tot, cmp_ll);
            qsort(tch, REPS, sizeof *tch, cmp_ll);
            qsort(mf, REPS, sizeof *mf, cmp_l);
            qsort(hg, REPS, sizeof *hg, cmp_l);
            printf("median,size=%zu,leg=%s,pair_ns=%lld,"
                   "total_ns=%lld,touch_ns=%lld,minflt=%ld,"
                   "huge_kib=%ld\n",
                   size, leg ? "variant" : "banked",
                   pair[REPS/2], tot[REPS/2], tch[REPS/2],
                   mf[REPS/2], hg[REPS/2]);
            fflush(stdout);
        }
    }
        static const size_t wsizes[] = { 262144, 1048576 };
        for (size_t wi = 0; wi < 2; wi++) {
            long long hit = 0, miss = 0;
            if (witness(wsizes[wi], &hit, &miss) != 0) {
                printf("FAIL witness size=%zu\n", wsizes[wi]);
                return 1;
            }
            printf("median,witness_size=%zu,banked_pool_hit_ns=%lld,variant_fresh_ns=%lld\n", wsizes[wi], hit, miss);
            fflush(stdout);
        }
    printf("verdict,checksum_equivalence=%s\n",
           fail ? "FAIL" : "PASS");
    printf("RESULT: %s\n", fail ? "FAIL" : "PASS");
    return fail ? 1 : 0;
}
