/* probe_z010.c - Z-010 Latency ladder (MimirAlloc Z13, Wave 2).
 *
 * Pointer-chase latency by working-set size, pinned to one CPU.
 * A single-cycle random permutation chain over 64-byte elements
 * (one element per cache line) makes every load dependent on the
 * previous one and defeats hardware prefetch, so the measured
 * ns/access is the true load-to-use latency of whichever level
 * of the hierarchy holds the working set on this silicon.
 *
 * Sizes span 4 KiB..512 MiB with extra points around the known
 * hardware boundaries of the Ryzen AI MAX+ 395 (Strix Halo):
 * L1d 48 KiB/core, L2 1 MiB/core, L3 32 MiB per CCD (2 CCDs,
 * 64 MiB total). Boundaries are NAMED from the measured jumps,
 * not assumed: the probe only reports numbers.
 *
 * Usage: probe_z010 <cpu>
 *   cpu - logical CPU to pin to (0 = CCD0 core 0, 8 = CCD1 core 0;
 *         sysfs cache topology verified at run time by the runner)
 *
 * Per size: chain built once (which also first-touches every
 * page), one warm-up pass, then 5 timed repetitions; the median
 * ns/access is the reported value, min is printed as a cross-check.
 * Timing: clock_gettime(CLOCK_MONOTONIC) around the whole chase.
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sched.h>
#include <time.h>

struct elem { struct elem *next; char pad[56]; };
_Static_assert(sizeof(struct elem) == 64, "one element per 64B line");

static uint64_t rng_state;
static uint64_t rng_next(void)
{
    uint64_t x = rng_state;
    x ^= x << 13; x ^= x >> 7; x ^= x << 17;
    rng_state = x;
    return x;
}

static long long now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static int cmp_d(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static const size_t sizes[] = {
    4ULL<<10, 8ULL<<10, 16ULL<<10, 32ULL<<10, 48ULL<<10,
    64ULL<<10, 96ULL<<10, 128ULL<<10, 192ULL<<10, 256ULL<<10,
    384ULL<<10, 512ULL<<10, 768ULL<<10, 1ULL<<20, 1536ULL<<10,
    2ULL<<20, 3ULL<<20, 4ULL<<20, 6ULL<<20, 8ULL<<20,
    12ULL<<20, 16ULL<<20, 24ULL<<20, 32ULL<<20, 48ULL<<20,
    64ULL<<20, 96ULL<<20, 128ULL<<20, 192ULL<<20, 256ULL<<20,
    384ULL<<20, 512ULL<<20
};

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: probe_z010 <cpu>\n"); return 2; }
    int cpu = atoi(argv[1]);
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0) {
        perror("sched_setaffinity"); return 1;
    }
    int actual = sched_getcpu();
    printf("PROBE cpu_arg=%d cpu_running=%d elem_bytes=64 timing=CLOCK_MONOTONIC reps=5\n", cpu, actual);
    fflush(stdout);
    for (size_t si = 0; si < sizeof(sizes)/sizeof(sizes[0]); si++) {
        size_t bytes = sizes[si];
        size_t n = bytes / sizeof(struct elem);
        struct elem *buf = NULL;
        if (posix_memalign((void **)&buf, 4096, bytes) != 0 || !buf) {
            printf("SIZE bytes=%zu FAIL alloc\n", bytes); fflush(stdout); continue;
        }
        size_t *order = malloc(n * sizeof(size_t));
        if (!order) { printf("SIZE bytes=%zu FAIL order\n", bytes); free(buf); return 1; }
        for (size_t i = 0; i < n; i++) order[i] = i;
        rng_state = 0x9E3779B97F4A7C15ULL ^ (uint64_t)bytes;
        for (size_t i = n - 1; i > 0; i--) {
            size_t j = (size_t)(rng_next() % (uint64_t)(i + 1));
            size_t t = order[i]; order[i] = order[j]; order[j] = t;
        }
        for (size_t i = 0; i < n; i++) {
            size_t cur = order[i];
            size_t nxt = order[(i + 1) % n];
            buf[cur].next = &buf[nxt];
        }
        free(order);
        struct elem *p = buf; /* any start: the single cycle covers every element */
        /* warm-up: one full pass */
        for (size_t i = 0; i < n; i++) p = p->next;
        uint64_t accesses = (uint64_t)n > 4000000ULL ? (uint64_t)n : 4000000ULL;
        double vals[5];
        for (int rep = 0; rep < 5; rep++) {
            long long t0 = now_ns();
            uint64_t done = 0;
            while (done < accesses) { p = p->next; done++; }
            long long t1 = now_ns();
            vals[rep] = (double)(t1 - t0) / (double)accesses;
        }
        double sorted[5]; memcpy(sorted, vals, sizeof(vals));
        qsort(sorted, 5, sizeof(double), cmp_d);
        printf("LAT bytes=%zu kib=%zu median_ns=%.2f min_ns=%.2f reps_ns=%.2f,%.2f,%.2f,%.2f,%.2f sink=%ld\n",
               bytes, bytes/1024, sorted[2], sorted[0],
               vals[0], vals[1], vals[2], vals[3], vals[4],
               (long)((char *)p - (char *)buf));
        fflush(stdout);
        free(buf);
    }
    printf("PROBE DONE cpu=%d\n", cpu);
    return 0;
}
