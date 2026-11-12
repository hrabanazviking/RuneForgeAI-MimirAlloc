/* probe_z013.c - Z-013 THP under madvise.
 * Host/kernel probe (no allocator under test): measures what
 * MADV_HUGEPAGE on/off does to large anonymous mappings on
 * this machine, per size decade:
 *   - first-touch fault latency (total + per 4 KiB page) and
 *     minor-fault count delta from /proc/self/stat;
 *   - RSS honesty from /proc/self/smaps (Rss + AnonHugePages
 *     for the touched VMA), incl. a non-PMD-multiple size;
 *   - write (memset) and read (sequential sum) bandwidth.
 * Mappings are 2 MiB-aligned (reserve + trim) in BOTH modes
 * so alignment never confounds the comparison. "off" is an
 * explicit MADV_NOHUGEPAGE; "on" is MADV_HUGEPAGE (the host
 * THP mode is madvise, so no huge pages arrive without it).
 * REPS repetitions per config, median reported; raw rep
 * lines printed first. Exit 0 iff every madvise/mmap/smaps
 * step succeeded and checksums are stable per size. */
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
#ifndef MADV_NOHUGEPAGE
#define MADV_NOHUGEPAGE 15
#endif

#define PMD ((size_t)2 * 1024 * 1024)
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
    buf[n] = '\0';
    char *rp = strrchr(buf, ')');
    if (!rp) return -1;
    /* tokens after ')': state ppid pgrp session tty_nr tpgid
     * flags minflt -> minflt is token 8 (1-based). */
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

/* Find the VMA containing addr in /proc/self/smaps; report
 * its Rss and AnonHugePages in KiB. 0 ok, -1 not found. */
static int smaps_vma(const volatile void *addr, long *rss_kib,
                     long *huge_kib) {
    FILE *f = fopen("/proc/self/smaps", "r");
    if (!f) return -1;
    char line[512];
    unsigned long target = (unsigned long)(uintptr_t)addr;
    int in_vma = 0, found = 0;
    long rss = -1, huge = -1;
    while (fgets(line, sizeof line, f)) {
        unsigned long lo, hi;
        char extra;
        if (sscanf(line, "%lx-%lx %c", &lo, &hi, &extra) == 3 &&
            line[0] >= '0' && line[0] <= '9') {
            if (in_vma && found) break;
            in_vma = (target >= lo && target < hi);
            if (in_vma) { found = 1; rss = -1; huge = -1; }
            continue;
        }
        if (in_vma) {
            long v;
            if (sscanf(line, "Rss: %ld kB", &v) == 1) rss = v;
            if (sscanf(line, "AnonHugePages: %ld kB", &v) == 1)
                huge = v;
        }
    }
    fclose(f);
    if (!found || rss < 0 || huge < 0) return -1;
    *rss_kib = rss;
    *huge_kib = huge;
    return 0;
}

struct result {
    long long fault_ns;
    long minflt;
    long rss_kib;
    long huge_kib;
    double write_mibs;
    double read_mibs;
    unsigned long long cksum;
};

static int run_once(size_t size, int huge_on, struct result *r) {
    size_t reserve = size + PMD;
    char *raw = mmap(NULL, reserve, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) return -1;
    uintptr_t base = ((uintptr_t)raw + PMD - 1) & ~(uintptr_t)(PMD - 1);
    size_t head = (size_t)(base - (uintptr_t)raw);
    char *p = (char *)base;
    if (head) munmap(raw, head);
    if (reserve - head - size) munmap(p + size, reserve - head - size);
    if (madvise(p, size, huge_on ? MADV_HUGEPAGE : MADV_NOHUGEPAGE) != 0) {
        munmap(p, size);
        return -1;
    }
    long mf0 = read_minflt();
    long long t0 = now_ns();
    for (size_t off = 0; off < size; off += 4096)
        *(volatile char *)(p + off) = (char)1;
    long long t1 = now_ns();
    long mf1 = read_minflt();
    long rss, huge;
    if (smaps_vma(p, &rss, &huge) != 0) { munmap(p, size); return -1; }
    /* Bandwidth passes repeat to a fixed byte target: a
     * single pass over a small region is microseconds and
     * its timing is noise (seen flipping sign between runs
     * at 2 MiB); ~256 MiB of passes per rep is stable. */
    size_t passes = ((size_t)256 * 1024 * 1024) / size;
    if (passes == 0) passes = 1;
    long long t2 = now_ns();
    for (size_t k = 0; k < passes; k++) memset(p, 0x5A, size);
    long long t3 = now_ns();
    unsigned long long sum = 0;
    const uint64_t *q = (const uint64_t *)p;
    for (size_t k = 0; k < passes; k++)
        for (size_t i = 0; i < size / 8; i++) sum += q[i];
    long long t4 = now_ns();
    r->fault_ns = t1 - t0;
    r->minflt = (mf0 >= 0 && mf1 >= 0) ? (mf1 - mf0) : -1;
    r->rss_kib = rss;
    r->huge_kib = huge;
    r->write_mibs = (double)size * (double)passes / (double)(t3 - t2) * 1e9 / 1048576.0;
    r->read_mibs = (double)size * (double)passes / (double)(t4 - t3) * 1e9 / 1048576.0;
    r->cksum = sum;
    munmap(p, size);
    return 0;
}

static int cmp_ll(const void *a, const void *b) {
    long long x = *(const long long *)a, y = *(const long long *)b;
    return (x > y) - (x < y);
}
static int cmp_d(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}
static int cmp_l(const void *a, const void *b) {
    long x = *(const long *)a, y = *(const long *)b;
    return (x > y) - (x < y);
}

struct cfg { size_t size; const char *label; int decade; };
static const struct cfg CFGS[] = {
    {512u * 1024u, "512KiB-control", 0},
    {2u * 1024u * 1024u, "2MiB", 1},
    {3u * 1024u * 1024u, "3MiB-nonmultiple", 0},
    {20u * 1024u * 1024u, "20MiB", 1},
    {200u * 1024u * 1024u, "200MiB", 1},
};

int main(void) {
    printf("probe_z013: THP under madvise (host THP mode per header)\n");
    printf("pmd_bytes=%zu reps=%d order=alternating\n", PMD, REPS);
    int fail = 0;
    /* medians[config][mode]: fault_per_page, write, read, huge */
    double med_fpp[5][2], med_wr[5][2], med_rd[5][2], med_huge[5][2];
    for (size_t c = 0; c < sizeof CFGS / sizeof CFGS[0]; c++) {
        size_t size = CFGS[c].size;
        for (int mode = 0; mode < 2; mode++) {
            long long fpp[REPS], faults[REPS];
            long mfs[REPS], rsss[REPS], huges[REPS];
            double wrs[REPS], rds[REPS];
            unsigned long long ck0 = 0;
            for (int rep = 0; rep < REPS; rep++) {
                struct result r;
                if (run_once(size, mode, &r) != 0) {
                    printf("ERROR size=%zu mode=%s rep=%d\n", size,
                           mode ? "on" : "off", rep);
                    fail = 1;
                    break;
                }
                if (rep == 0) ck0 = r.cksum;
                if (r.cksum != ck0) { fail = 1; printf("CKSUM-MISMATCH\n"); }
                faults[rep] = r.fault_ns;
                fpp[rep] = r.fault_ns / (long long)(size / 4096);
                mfs[rep] = r.minflt;
                rsss[rep] = r.rss_kib;
                huges[rep] = r.huge_kib;
                wrs[rep] = r.write_mibs;
                rds[rep] = r.read_mibs;
                printf("raw,size=%zu,mode=%s,rep=%d,fault_ns=%lld,"
                       "fault_ns_per_page=%lld,minflt_delta=%ld,"
                       "rss_kib=%ld,anon_huge_kib=%ld,write_mibs=%.1f,"
                       "read_mibs=%.1f,cksum=%llu\n",
                       size, mode ? "on" : "off", rep, r.fault_ns,
                       fpp[rep], r.minflt, r.rss_kib, r.huge_kib,
                       r.write_mibs, r.read_mibs, r.cksum);
                fflush(stdout);
            }
            if (fail) break;
            qsort(faults, REPS, sizeof faults[0], cmp_ll);
            qsort(fpp, REPS, sizeof fpp[0], cmp_ll);
            qsort(mfs, REPS, sizeof mfs[0], cmp_l);
            qsort(rsss, REPS, sizeof rsss[0], cmp_l);
            qsort(huges, REPS, sizeof huges[0], cmp_l);
            qsort(wrs, REPS, sizeof wrs[0], cmp_d);
            qsort(rds, REPS, sizeof rds[0], cmp_d);
            int m = REPS / 2;
            med_fpp[c][mode] = (double)fpp[m];
            med_wr[c][mode] = wrs[m];
            med_rd[c][mode] = rds[m];
            med_huge[c][mode] = (double)huges[m];
            printf("median,size=%zu,label=%s,mode=%s,fault_ns=%lld,"
                   "fault_ns_per_page=%lld,minflt_delta=%ld,rss_kib=%ld,"
                   "anon_huge_kib=%ld,write_mibs=%.1f,read_mibs=%.1f\n",
                   size, CFGS[c].label, mode ? "on" : "off", faults[m],
                   fpp[m], mfs[m], rsss[m], huges[m], wrs[m], rds[m]);
            fflush(stdout);
        }
        if (fail) break;
    }
    if (!fail) {
        for (size_t c = 0; c < sizeof CFGS / sizeof CFGS[0]; c++) {
            if (!CFGS[c].decade) continue;
            double fimp = (med_fpp[c][0] - med_fpp[c][1]) /
                          med_fpp[c][0] * 100.0;
            double wimp = (med_wr[c][1] - med_wr[c][0]) /
                          med_wr[c][0] * 100.0;
            double rimp = (med_rd[c][1] - med_rd[c][0]) /
                          med_rd[c][0] * 100.0;
            const char *verdict;
            if (med_huge[c][1] <= 0) verdict = "LOSS-no-huge-backing";
            else if ((fimp >= 10 || wimp >= 10 || rimp >= 10) &&
                     fimp >= -10 && wimp >= -10 && rimp >= -10)
                verdict = "WIN";
            else if ((fimp <= -10 || wimp <= -10 || rimp <= -10) &&
                     fimp < 10 && wimp < 10 && rimp < 10)
                verdict = "LOSS";
            else if (fimp >= 10 && (wimp <= -10 || rimp <= -10))
                verdict = "MIXED-fault-win-bandwidth-loss";
            else verdict = "TIE";
            printf("verdict,size=%zu,label=%s,fault_improve_pct=%.1f,"
                   "write_improve_pct=%.1f,read_improve_pct=%.1f,"
                   "anon_huge_kib_on=%.0f,verdict=%s\n",
                   CFGS[c].size, CFGS[c].label, fimp, wimp, rimp,
                   med_huge[c][1], verdict);
        }
    }
    printf("RESULT: %s\n", fail ? "FAIL" : "PASS");
    return fail ? 1 : 0;
}
