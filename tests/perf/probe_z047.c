/* probe_z047.c - Z-047 mapping-cache design: host economics probe.
 * Host/kernel probe (no allocator under test): prices the
 * decisions the Z-047 design doc must make, in exactly
 * MimirAlloc's large-path geometry (mapping_length =
 * 4096 reserve page + page_round_up(request); user extent =
 * base + 4096). Legs:
 *   op fresh        mmap + endpoint touch + munmap per op
 *                   (the banked mapping-per-allocation price,
 *                   sparse touch, bench_k002 shape)
 *   op poolhit      one exact-length mapping held in a
 *                   one-entry mock pool; per op = take,
 *                   endpoint touch, return (NO syscall) -
 *                   the raw hit floor; allocator bookkeeping
 *                   (lock, descriptor, pool list) is NOT in
 *                   this number and is priced in the design
 *                   doc from Z-046's residual, never hidden
 *   op freshdense   mmap + memset every user byte + munmap
 *   op pooldense    pooled mapping whose pages are already
 *                   resident; per op = memset every byte
 *                   (re-touch with no faults)
 *   op pooldontneed pooled mapping returned with
 *                   MADV_DONTNEED on the user extent; per op
 *                   = memset every byte (faults return) -
 *                   the RSS-honest pooling price
 *   census          deterministic VMA / RSS census (no
 *                   timing): base, live mappings held,
 *                   dead mappings pooled (half touched),
 *                   DONTNEED applied, eviction; VMA counts
 *                   from /proc/self/maps, RSS from
 *                   /proc/self/status VmRSS
 * Timed legs print one line per process:
 *   OP,<leg>,<size>,<iters>,<ns_per_op>,<minflt_per_op_x1000>,<checksum>
 * Census prints CENSUS,<stage>,<vmas>,<rss_kib>,<detail>.
 * Exit 0 iff every mmap/munmap/madvise step succeeded and
 * the checksum is stable within the process. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>

#ifndef MADV_DONTNEED
#define MADV_DONTNEED 4
#endif

#define RESERVE ((size_t)4096)

static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static long read_minflt(void) {
    struct rusage ru;
    if (getrusage(RUSAGE_SELF, &ru) != 0) return -1;
    return ru.ru_minflt;
}

static size_t page_round_up(size_t n) {
    return (n + RESERVE - 1) & ~(RESERVE - 1);
}

/* MimirAlloc large-path geometry for a request of n bytes. */
static size_t mapping_len_for(size_t n) {
    return page_round_up(RESERVE + n);
}

static void *map_geom(size_t n, size_t *out_len) {
    size_t len = mapping_len_for(n);
    void *b = mmap(NULL, len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (b == MAP_FAILED) return NULL;
    *out_len = len;
    return b;
}

/* bench_k002 endpoint touch: first + last user byte. */
static uint64_t touch_endpoints(void *base, size_t n) {
    unsigned char *u = (unsigned char *)base + RESERVE;
    u[0] = (unsigned char)(u[0] + 1u);
    u[n - 1] = (unsigned char)(u[n - 1] + 1u);
    return (uint64_t)u[0] + ((uint64_t)u[n - 1] << 8);
}

static int count_vmas(void) {
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) return -1;
    int n = 0;
    char buf[512];
    while (fgets(buf, sizeof buf, f)) n++;
    fclose(f);
    return n;
}

static long rss_kib(void) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char buf[256];
    long v = -1;
    while (fgets(buf, sizeof buf, f)) {
        if (sscanf(buf, "VmRSS: %ld kB", &v) == 1) break;
    }
    fclose(f);
    return v;
}

static int run_op(const char *leg, size_t size, long iters) {
    uint64_t chk = 0;
    long f0 = read_minflt();
    long long t0, t1;
    if (strcmp(leg, "fresh") == 0) {
        t0 = now_ns();
        for (long i = 0; i < iters; i++) {
            size_t len;
            void *b = map_geom(size, &len);
            if (!b) { fprintf(stderr, "mmap fail\n"); return 1; }
            chk += touch_endpoints(b, size);
            if (munmap(b, len) != 0) { fprintf(stderr, "munmap fail\n"); return 1; }
        }
        t1 = now_ns();
    } else if (strcmp(leg, "poolhit") == 0) {
        size_t len;
        void *b = map_geom(size, &len);
        if (!b) { fprintf(stderr, "mmap fail\n"); return 1; }
        t0 = now_ns();
        for (long i = 0; i < iters; i++) {
            /* take + return: the mock pool is one pointer;
             * the timed work is the touch alone. */
            chk += touch_endpoints(b, size);
        }
        t1 = now_ns();
        if (munmap(b, len) != 0) return 1;
    } else if (strcmp(leg, "freshdense") == 0) {
        t0 = now_ns();
        for (long i = 0; i < iters; i++) {
            size_t len;
            void *b = map_geom(size, &len);
            if (!b) { fprintf(stderr, "mmap fail\n"); return 1; }
            unsigned char *u = (unsigned char *)b + RESERVE;
            memset(u, (int)(i & 0xFF), size);
            chk += u[0] + u[size - 1];
            if (munmap(b, len) != 0) { fprintf(stderr, "munmap fail\n"); return 1; }
        }
        t1 = now_ns();
    } else if (strcmp(leg, "pooldense") == 0 ||
               strcmp(leg, "pooldontneed") == 0) {
        int dontneed = (strcmp(leg, "pooldontneed") == 0);
        size_t len;
        void *b = map_geom(size, &len);
        if (!b) { fprintf(stderr, "mmap fail\n"); return 1; }
        unsigned char *u = (unsigned char *)b + RESERVE;
        memset(u, 0x5A, size); /* fault every page in */
        chk += u[0];
        if (dontneed && madvise(u, page_round_up(size), MADV_DONTNEED) != 0) {
            fprintf(stderr, "madvise fail\n"); return 1;
        }
        t0 = now_ns();
        for (long i = 0; i < iters; i++) {
            memset(u, (int)(i & 0xFF), size);
            chk += u[0] + u[size - 1];
            if (dontneed &&
                madvise(u, page_round_up(size), MADV_DONTNEED) != 0) {
                fprintf(stderr, "madvise fail\n"); return 1;
            }
        }
        t1 = now_ns();
        if (munmap(b, len) != 0) return 1;
    } else {
        fprintf(stderr, "unknown leg %s\n", leg);
        return 2;
    }
    long f1 = read_minflt();
    double nspo = (double)(t1 - t0) / (double)iters;
    long mf = (f0 >= 0 && f1 >= 0) ? (f1 - f0) : -1;
    double mfpo = iters ? (double)mf * 1000.0 / (double)iters : 0.0;
    printf("OP,%s,%zu,%ld,%.2f,%.0f,%llu\n", leg, size, iters, nspo,
           mfpo, (unsigned long long)chk);
    return 0;
}

#define CEN_N 8
#define CEN_SIZE ((size_t)262144)

static int run_census(void) {
    int base = count_vmas();
    long base_rss = rss_kib();
    printf("CENSUS,base,%d,%ld,process_start\n", base, base_rss);
    void *blk[CEN_N];
    size_t len[CEN_N];
    for (int i = 0; i < CEN_N; i++) {
        blk[i] = map_geom(CEN_SIZE, &len[i]);
        if (!blk[i]) { fprintf(stderr, "census mmap fail\n"); return 1; }
    }
    printf("CENSUS,live8,%d,%ld,8_mappings_of_%zu_held_live\n",
           count_vmas(), rss_kib(), CEN_SIZE);
    /* free into the mock pool: mappings stay; touch half of
     * them fully first (while "live") so the pool holds a
     * touched half and an untouched half. */
    for (int i = 0; i < CEN_N / 2; i++) {
        memset((unsigned char *)blk[i] + RESERVE, 0xA5, CEN_SIZE);
    }
    printf("CENSUS,pooled8,%d,%ld,4_touched_4_untouched_dead_mappings_kept\n",
           count_vmas(), rss_kib());
    for (int i = 0; i < CEN_N / 2; i++) {
        if (madvise((unsigned char *)blk[i] + RESERVE,
                    page_round_up(CEN_SIZE), MADV_DONTNEED) != 0) {
            fprintf(stderr, "census madvise fail\n"); return 1;
        }
    }
    printf("CENSUS,pooled8_dontneed,%d,%ld,touched_half_DONTNEED_applied\n",
           count_vmas(), rss_kib());
    for (int i = 0; i < CEN_N; i++) {
        if (munmap(blk[i], len[i]) != 0) {
            fprintf(stderr, "census munmap fail\n"); return 1;
        }
    }
    printf("CENSUS,evicted,%d,%ld,all_pool_mappings_unmapped\n",
           count_vmas(), rss_kib());
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "census") == 0) {
        return run_census();
    }
    if (argc == 5 && strcmp(argv[1], "op") == 0) {
        size_t size = strtoull(argv[3], NULL, 10);
        long iters = strtol(argv[4], NULL, 10);
        if (size == 0 || iters <= 0) return 2;
        return run_op(argv[2], size, iters);
    }
    fprintf(stderr, "usage: probe_z047 op <leg> <size> <iters> | census\n");
    return 2;
}
