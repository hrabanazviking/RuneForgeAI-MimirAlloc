/* probe_z014.c - Z-014 page-fault cost curve probe.
 * Host/kernel probe (no allocator under test): measures
 * first-touch page-fault cost of fresh anonymous mappings
 * by size (4 KiB..1 GiB) against two recycled variants:
 *   fresh      - mmap + first touch + munmap, one measured
 *                mapping per process (the process IS the
 *                fresh context; runner uses median-of-
 *                processes per Z-004 protocol section 6).
 *   recyc_map  - mmap/touch/munmap cycles inside one
 *                process; only the LAST cycle is timed.
 *                Physical pages come from the kernel free
 *                pool the earlier cycles just released
 *                (cache-warm, recently zeroed pool), VMA
 *                and page tables are rebuilt each cycle.
 *   recyc_keep - the pooled-mapping case (what a large-path
 *                allocator that retains VA would do): mmap
 *                once, touch, then MADV_DONTNEED + re-touch
 *                repeats; only the LAST re-touch is timed.
 *                VMA retained, pages dropped and re-faulted.
 * All legs take MADV_NOHUGEPAGE so the curve is the 4 KiB
 * fault cost with no THP confound (THP-on is Z-013, banked;
 * host THP mode is madvise anyway). Touch writes one byte
 * per 4 KiB page in ascending order; an untimed read-back
 * pass sums the written bytes as the checksum, which must
 * be identical across every process and mode for a size.
 * Timing is CLOCK_MONOTONIC_RAW around the touch loop only;
 * mmap / MADV_DONTNEED / munmap times are printed as
 * separate columns. Minor-fault delta around the timed
 * touch comes from /proc/self/stat field minflt.
 * CSV: size,mode,pages,touch_ns,ns_per_page,map_ns,
 *      aux_ns,unmap_ns,minflt_delta,checksum
 * (aux_ns = munmap time of earlier cycles in recyc_map,
 *  MADV_DONTNEED time in recyc_keep, 0 in fresh.)
 * Exit 0 iff every syscall step succeeded and every
 * cycle checksum equals the expected pattern sum. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#ifndef MADV_NOHUGEPAGE
#define MADV_NOHUGEPAGE 15
#endif
#ifndef MADV_DONTNEED
#define MADV_DONTNEED 4
#endif
static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
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
static unsigned long long expected_cksum(size_t pages) {
    unsigned long long s = 0;
    for (size_t i = 0; i < pages; i++)
        s += (unsigned char)(i * 7u + 1u);
    return s;
}
/* Touch every page once, ascending; returns checksum from
 * an untimed read-back pass (proves the writes landed). */
static unsigned long long touch(char *base, size_t pages,
                                  long long *touch_ns) {
    long long t0 = now_ns();
    for (size_t i = 0; i < pages; i++)
        base[i * 4096u] = (char)(i * 7u + 1u);
    long long t1 = now_ns();
    *touch_ns = t1 - t0;
    unsigned long long s = 0;
    for (size_t i = 0; i < pages; i++)
        s += (unsigned char)base[i * 4096u];
    return s;
}
static char *map_nohuge(size_t size, long long *map_ns) {
    long long t0 = now_ns();
    char *p = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    long long t1 = now_ns();
    if (p == MAP_FAILED) return NULL;
    *map_ns = t1 - t0;
    if (madvise(p, size, MADV_NOHUGEPAGE) != 0) {
        munmap(p, size);
        return NULL;
    }
    return p;
}
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: probe_z014 <size> <mode>\n");
        return 2;
    }
    size_t size = (size_t)strtoull(argv[1], NULL, 10);
    const char *mode = argv[2];
    long ps = sysconf(_SC_PAGESIZE);
    if (ps != 4096 || size == 0 || size % 4096u != 0) {
        fprintf(stderr, "bad size/pagesize\n");
        return 2;
    }
    size_t pages = size / 4096u;
    unsigned long long want = expected_cksum(pages);
    long long map_ns = 0, aux_ns = 0, unmap_ns = 0, touch_ns = 0;
    long f0 = 0, f1 = 0;
    unsigned long long ck = 0;
    if (strcmp(mode, "fresh") == 0) {
        char *p = map_nohuge(size, &map_ns);
        if (!p) return 1;
        f0 = read_minflt();
        ck = touch(p, pages, &touch_ns);
        f1 = read_minflt();
        long long t0 = now_ns();
        if (munmap(p, size) != 0) return 1;
        unmap_ns = now_ns() - t0;
    } else if (strcmp(mode, "recyc_map") == 0) {
        int cycles = (size >= (size_t)256*1024*1024) ? 3 : 5;
        for (int c = 0; c < cycles; c++) {
            char *p = map_nohuge(size, &map_ns);
            if (!p) return 1;
            int last = (c == cycles - 1);
            if (last) f0 = read_minflt();
            ck = touch(p, pages, &touch_ns);
            if (last) f1 = read_minflt();
            if (ck != want) return 1;
            long long t0 = now_ns();
            if (munmap(p, size) != 0) return 1;
            if (last) unmap_ns = now_ns() - t0;
            else aux_ns += now_ns() - t0;
        }
    } else if (strcmp(mode, "recyc_keep") == 0) {
        int reps = (size >= (size_t)256*1024*1024) ? 2 : 3;
        char *p = map_nohuge(size, &map_ns);
        if (!p) return 1;
        ck = touch(p, pages, &touch_ns);
        if (ck != want) return 1;
        for (int r = 0; r < reps; r++) {
            long long t0 = now_ns();
            if (madvise(p, size, MADV_DONTNEED) != 0) return 1;
            long long dn = now_ns() - t0;
            int last = (r == reps - 1);
            if (last) { aux_ns = dn; f0 = read_minflt(); }
            ck = touch(p, pages, &touch_ns);
            if (last) f1 = read_minflt();
            if (ck != want) return 1;
        }
        long long t0 = now_ns();
        if (munmap(p, size) != 0) return 1;
        unmap_ns = now_ns() - t0;
    } else {
        fprintf(stderr, "unknown mode: %s\n", mode);
        return 2;
    }
    if (ck != want || f0 < 0 || f1 < 0) return 1;
    printf("%zu,%s,%zu,%lld,%.2f,%lld,%lld,%lld,%ld,%llu\n",
           size, mode, pages, touch_ns,
           (double)touch_ns / (double)pages, map_ns, aux_ns,
           unmap_ns, f1 - f0, ck);
    return 0;
}
