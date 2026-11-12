/* probe_z049.c - Z-049 THP for cached mappings.
 * Host/kernel probe (no allocator under test): measures what
 * MADV_HUGEPAGE does to POOLED (cached, reused) large mappings
 * in the exact shape the MimirAlloc large path creates them:
 * one descriptor page followed by the usable extent
 * (usable_offset == one system page, mapping_length ==
 * page + usable rounded to pages; see alloc_locked in
 * src/core/mimir_core.c). Z-013 measured ideally 2 MiB-aligned
 * raw mappings; this probe measures the pooled shape, whose
 * usable extent is offset one page from the mapping base, and
 * the pool's two reuse modes:
 *   fresh    - first touch of a newly mapped pooled mapping
 *              (descriptor byte written first, as the
 *              allocator writes its descriptor), fault cost,
 *              minor faults, smaps RSS / AnonHugePages, then
 *              memset write + read-sum bandwidth;
 *   resident - pool hit with pages kept resident: one full
 *              memset pass timed + minor-fault delta
 *              (expected ~0 faults in both modes);
 *   dontneed - pool hit after the pooled mapping's usable
 *              extent was returned with MADV_DONTNEED while
 *              the VMA stayed cached: RSS after DONTNEED,
 *              re-fault cost, minor faults, huge backing
 *              re-formed or not.
 * Modes: off = explicit MADV_NOHUGEPAGE on the whole mapping,
 * on = MADV_HUGEPAGE on the whole mapping (host THP mode is
 * madvise per the Z-002 header). REPS repetitions per config,
 * mode order alternating per rep; medians reported; raw rep
 * lines printed first. Exit 0 iff every step succeeded and
 * the read checksum is identical across modes and reps for
 * each size (workload equivalence, K-002 discipline). */
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
#ifndef MADV_DONTNEED
#define MADV_DONTNEED 4
#endif

#define PMD ((size_t)2 * 1024 * 1024)
#define PAGE_SZ ((size_t)4096)
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
    long long reuse_ns;
    long reuse_minflt;
    long rss_after_dn_kib;
    long long refault_ns;
    long refault_minflt;
    long rss2_kib;
    long huge2_kib;
    unsigned long base_mod;
};

static int run_once(size_t size, int huge_on, struct result *r) {
    size_t total = PAGE_SZ + size;
    char *base = mmap(NULL, total, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) return -1;
    r->base_mod = (unsigned long)((uintptr_t)base % PMD);
    if (madvise(base, total,
                huge_on ? MADV_HUGEPAGE : MADV_NOHUGEPAGE) != 0) {
        munmap(base, total);
        return -1;
    }
    char *u = base + PAGE_SZ;
    /* The allocator writes its descriptor in page 0 when the
     * mapping is published; do the same before touching the
     * usable extent. */
    *(volatile char *)base = (char)1;
    /* fresh: first touch, one byte per 4 KiB page */
    long mf0 = read_minflt();
    long long t0 = now_ns();
    for (size_t off = 0; off < size; off += PAGE_SZ)
        *(volatile char *)(u + off) = (char)1;
    long long t1 = now_ns();
    long mf1 = read_minflt();
    long rss, huge;
    if (smaps_vma(u, &rss, &huge) != 0) {
        munmap(base, total);
        return -1;
    }
    /* bandwidth: passes repeat to a ~256 MiB byte target */
    size_t passes = ((size_t)256 * 1024 * 1024) / size;
    if (passes == 0) passes = 1;
    long long t2 = now_ns();
    for (size_t k = 0; k < passes; k++) memset(u, 0x5A, size);
    long long t3 = now_ns();
    unsigned long long sum = 0;
    const uint64_t *q = (const uint64_t *)u;
    for (size_t k = 0; k < passes; k++)
        for (size_t i = 0; i < size / 8; i++) sum += q[i];
    long long t4 = now_ns();
    r->fault_ns = t1 - t0;
    r->minflt = (mf0 >= 0 && mf1 >= 0) ? (mf1 - mf0) : -1;
    r->rss_kib = rss;
    r->huge_kib = huge;
    r->write_mibs = (double)size * (double)passes /
                    (double)(t3 - t2) * 1e9 / 1048576.0;
    r->read_mibs = (double)size * (double)passes /
                   (double)(t4 - t3) * 1e9 / 1048576.0;
    r->cksum = sum;
    /* resident pool hit: pages kept; one full memset pass */
    mf0 = read_minflt();
    t0 = now_ns();
    memset(u, 0x5A, size);
    t1 = now_ns();
    mf1 = read_minflt();
    r->reuse_ns = t1 - t0;
    r->reuse_minflt = (mf0 >= 0 && mf1 >= 0) ? (mf1 - mf0) : -1;
    /* pooled, pages returned: MADV_DONTNEED on usable only */
    if (madvise(u, size, MADV_DONTNEED) != 0) {
        munmap(base, total);
        return -1;
    }
    long rss_dn, huge_dn;
    if (smaps_vma(u, &rss_dn, &huge_dn) != 0) {
        munmap(base, total);
        return -1;
    }
    r->rss_after_dn_kib = rss_dn;
    /* re-fault on reuse */
    mf0 = read_minflt();
    t0 = now_ns();
    for (size_t off = 0; off < size; off += PAGE_SZ)
        *(volatile char *)(u + off) = (char)1;
    t1 = now_ns();
    mf1 = read_minflt();
    long rss2, huge2;
    if (smaps_vma(u, &rss2, &huge2) != 0) {
        munmap(base, total);
        return -1;
    }
    r->refault_ns = t1 - t0;
    r->refault_minflt = (mf0 >= 0 && mf1 >= 0) ? (mf1 - mf0) : -1;
    r->rss2_kib = rss2;
    r->huge2_kib = huge2;
    munmap(base, total);
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
    {1u * 1024u * 1024u, "1MiB-subPMD", 1},
    {2u * 1024u * 1024u, "2MiB", 1},
    {20u * 1024u * 1024u, "20MiB", 1},
    {200u * 1024u * 1024u, "200MiB", 1},
};
#define NCFG (sizeof CFGS / sizeof CFGS[0])

int main(void) {
    printf("probe_z049: THP for cached (pooled) mappings, "
           "MimirAlloc large-path shape (1 descriptor page + "
           "usable extent)\n");
    printf("pmd_bytes=%zu reps=%d order=alternating\n", PMD, REPS);
    int fail = 0;
    double med_ffpp[NCFG][2], med_fwr[NCFG][2], med_frd[NCFG][2];
    double med_fhuge[NCFG][2], med_rfpp[NCFG][2], med_rhuge[NCFG][2];
    for (size_t c = 0; c < NCFG; c++) {
        size_t size = CFGS[c].size;
        unsigned long long ck0 = 0;
        int have_ck = 0;
        for (int mode = 0; mode < 2; mode++) {
            long long fpp[REPS], faults[REPS], reuse[REPS];
            long long rfpp[REPS], refaults[REPS];
            long mfs[REPS], rsss[REPS], huges[REPS];
            long rmfs[REPS], dnrss[REPS], rmfs2[REPS];
            long rss2s[REPS], huge2s[REPS];
            double wrs[REPS], rds[REPS];
            for (int rep = 0; rep < REPS; rep++) {
                struct result r;
                if (run_once(size, mode, &r) != 0) {
                    printf("ERROR size=%zu mode=%s rep=%d\n", size,
                           mode ? "on" : "off", rep);
                    fail = 1;
                    break;
                }
                if (!have_ck) { ck0 = r.cksum; have_ck = 1; }
                if (r.cksum != ck0) {
                    fail = 1;
                    printf("CKSUM-MISMATCH size=%zu\n", size);
                }
                faults[rep] = r.fault_ns;
                fpp[rep] = r.fault_ns / (long long)(size / PAGE_SZ);
                mfs[rep] = r.minflt;
                rsss[rep] = r.rss_kib;
                huges[rep] = r.huge_kib;
                wrs[rep] = r.write_mibs;
                rds[rep] = r.read_mibs;
                reuse[rep] = r.reuse_ns;
                rmfs[rep] = r.reuse_minflt;
                dnrss[rep] = r.rss_after_dn_kib;
                refaults[rep] = r.refault_ns;
                rfpp[rep] = r.refault_ns / (long long)(size / PAGE_SZ);
                rmfs2[rep] = r.refault_minflt;
                rss2s[rep] = r.rss2_kib;
                huge2s[rep] = r.huge2_kib;
                printf("raw,size=%zu,mode=%s,rep=%d,phase=fresh,"
                       "base_mod_pmd=%lu,fault_ns=%lld,"
                       "fault_ns_per_page=%lld,minflt_delta=%ld,"
                       "rss_kib=%ld,anon_huge_kib=%ld,"
                       "write_mibs=%.1f,read_mibs=%.1f,cksum=%llu\n",
                       size, mode ? "on" : "off", rep, r.base_mod,
                       r.fault_ns, fpp[rep], r.minflt, r.rss_kib,
                       r.huge_kib, r.write_mibs, r.read_mibs,
                       r.cksum);
                printf("raw,size=%zu,mode=%s,rep=%d,phase=resident,"
                       "reuse_memset_ns=%lld,minflt_delta=%ld\n",
                       size, mode ? "on" : "off", rep, r.reuse_ns,
                       r.reuse_minflt);
                printf("raw,size=%zu,mode=%s,rep=%d,phase=dontneed,"
                       "rss_after_dontneed_kib=%ld,refault_ns=%lld,"
                       "refault_ns_per_page=%lld,minflt_delta=%ld,"
                       "rss_kib=%ld,anon_huge_kib=%ld\n",
                       size, mode ? "on" : "off", rep,
                       r.rss_after_dn_kib, r.refault_ns, rfpp[rep],
                       r.refault_minflt, r.rss2_kib, r.huge2_kib);
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
            qsort(reuse, REPS, sizeof reuse[0], cmp_ll);
            qsort(rmfs, REPS, sizeof rmfs[0], cmp_l);
            qsort(dnrss, REPS, sizeof dnrss[0], cmp_l);
            qsort(refaults, REPS, sizeof refaults[0], cmp_ll);
            qsort(rfpp, REPS, sizeof rfpp[0], cmp_ll);
            qsort(rmfs2, REPS, sizeof rmfs2[0], cmp_l);
            qsort(rss2s, REPS, sizeof rss2s[0], cmp_l);
            qsort(huge2s, REPS, sizeof huge2s[0], cmp_l);
            int m = REPS / 2;
            med_ffpp[c][mode] = (double)fpp[m];
            med_fwr[c][mode] = wrs[m];
            med_frd[c][mode] = rds[m];
            med_fhuge[c][mode] = (double)huges[m];
            med_rfpp[c][mode] = (double)rfpp[m];
            med_rhuge[c][mode] = (double)huge2s[m];
            printf("median,size=%zu,label=%s,mode=%s,phase=fresh,"
                   "fault_ns=%lld,fault_ns_per_page=%lld,"
                   "minflt_delta=%ld,rss_kib=%ld,anon_huge_kib=%ld,"
                   "write_mibs=%.1f,read_mibs=%.1f\n",
                   size, CFGS[c].label, mode ? "on" : "off",
                   faults[m], fpp[m], mfs[m], rsss[m], huges[m],
                   wrs[m], rds[m]);
            printf("median,size=%zu,label=%s,mode=%s,phase=resident,"
                   "reuse_memset_ns=%lld,minflt_delta=%ld\n",
                   size, CFGS[c].label, mode ? "on" : "off",
                   reuse[m], rmfs[m]);
            printf("median,size=%zu,label=%s,mode=%s,phase=dontneed,"
                   "rss_after_dontneed_kib=%ld,refault_ns=%lld,"
                   "refault_ns_per_page=%lld,minflt_delta=%ld,"
                   "rss_kib=%ld,anon_huge_kib=%ld\n",
                   size, CFGS[c].label, mode ? "on" : "off",
                   dnrss[m], refaults[m], rfpp[m], rmfs2[m],
                   rss2s[m], huge2s[m]);
            fflush(stdout);
        }
        if (fail) break;
    }
    if (!fail) {
        for (size_t c = 0; c < NCFG; c++) {
            double fimp = (med_ffpp[c][0] - med_ffpp[c][1]) /
                          med_ffpp[c][0] * 100.0;
            double wimp = (med_fwr[c][1] - med_fwr[c][0]) /
                          med_fwr[c][0] * 100.0;
            double rimp = (med_frd[c][1] - med_frd[c][0]) /
                          med_frd[c][0] * 100.0;
            double rimpf = (med_rfpp[c][0] - med_rfpp[c][1]) /
                           med_rfpp[c][0] * 100.0;
            const char *verdict;
            if (med_fhuge[c][1] <= 0 && med_rhuge[c][1] <= 0)
                verdict = "LOSS-no-huge-backing";
            else if ((fimp >= 10 || wimp >= 10 || rimp >= 10 ||
                      rimpf >= 10) &&
                     fimp >= -10 && wimp >= -10 && rimp >= -10 &&
                     rimpf >= -10)
                verdict = "WIN";
            else if ((fimp <= -10 || wimp <= -10 || rimp <= -10 ||
                      rimpf <= -10) &&
                     fimp < 10 && wimp < 10 && rimp < 10 &&
                     rimpf < 10)
                verdict = "LOSS";
            else if ((fimp >= 10 || rimpf >= 10) &&
                     (wimp <= -10 || rimp <= -10))
                verdict = "MIXED-fault-win-bandwidth-loss";
            else
                verdict = "TIE";
            printf("verdict,size=%zu,label=%s,"
                   "fresh_fault_improve_pct=%.1f,"
                   "write_improve_pct=%.1f,read_improve_pct=%.1f,"
                   "refault_improve_pct=%.1f,"
                   "anon_huge_kib_fresh_on=%.0f,"
                   "anon_huge_kib_refault_on=%.0f,verdict=%s\n",
                   CFGS[c].size, CFGS[c].label, fimp, wimp, rimp,
                   rimpf, med_fhuge[c][1], med_rhuge[c][1], verdict);
        }
    }
    printf("RESULT: %s\n", fail ? "FAIL" : "PASS");
    return fail ? 1 : 0;
}
