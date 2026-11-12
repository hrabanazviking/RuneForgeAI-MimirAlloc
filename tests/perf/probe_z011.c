/* probe_z011.c - Z-011 Bandwidth saturation (MimirAlloc Z13, Wave 2).
 *
 * STREAM-style triad a[i] = b[i] + s*c[i] over three double arrays
 * of 256 MiB each (768 MiB total footprint, 12x the 64 MiB total L3),
 * run at 1/2/4/8/16/32 threads to find where UMA bandwidth saturates
 * on the Ryzen AI MAX+ 395 (Strix Halo, 16C/32T, 2 CCDs, 128 GiB UMA)
 * and what a single core can pull alone.
 *
 * Method: pthreads, static contiguous partitioning; each thread is
 * pinned to logical CPU == its index (CPUs 0..15 are the physical
 * cores, 16..31 the SMT siblings, per the sysfs L3 shared lists) and
 * first-touches (initialises) its own chunk so pages land under the
 * thread that streams them; one warm-up pass; 5 timed repetitions,
 * best (minimum) time reported - STREAM convention. Bandwidth uses
 * the STREAM byte count: 3 arrays x 8 B x N per pass (read b, read c,
 * write a); any write-allocate traffic is extra DRAM traffic the
 * convention does not count, which is named here so the number is
 * never misread. Timing: clock_gettime(CLOCK_MONOTONIC) by thread 0
 * across barrier-synchronised passes. A value check on a[] after the
 * last pass and a printed sink defeat dead-code elimination.
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <pthread.h>
#include <time.h>

static const size_t NELEM = 33554432ULL; /* 256 MiB per array */
static const double SCALAR = 3.0;
static const int REPS = 5;
static const int THREAD_COUNTS[] = {1, 2, 4, 8, 16, 32};

static double *ga, *gb, *gc;
static pthread_barrier_t bar;
static long long rep_t0, rep_t1;
static double best_s;
static double sink_sum;

static long long now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

struct targ { int tid; int nthreads; };

static void *worker(void *arg)
{
    struct targ *t = (struct targ *)arg;
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(t->tid, &set);
    if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) != 0) {
        fprintf(stderr, "pin failed tid=%d\n", t->tid);
    }
    size_t n = NELEM;
    size_t chunk = n / (size_t)t->nthreads;
    size_t lo = chunk * (size_t)t->tid;
    size_t hi = (t->tid == t->nthreads - 1) ? n : lo + chunk;
    /* first-touch: the streaming thread initialises its own chunk */
    for (size_t i = lo; i < hi; i++) { gb[i] = 2.0; gc[i] = 1.5; ga[i] = 0.0; }
    pthread_barrier_wait(&bar);
    /* warm-up pass */
    for (size_t i = lo; i < hi; i++) ga[i] = gb[i] + SCALAR * gc[i];
    for (int rep = 0; rep < REPS; rep++) {
        pthread_barrier_wait(&bar);
        if (t->tid == 0) rep_t0 = now_ns();
        pthread_barrier_wait(&bar);
        for (size_t i = lo; i < hi; i++) ga[i] = gb[i] + SCALAR * gc[i];
        pthread_barrier_wait(&bar);
        if (t->tid == 0) {
            rep_t1 = now_ns();
            double s = (double)(rep_t1 - rep_t0) / 1e9;
            if (rep == 0 || s < best_s) best_s = s;
        }
    }
    if (t->tid == 0) sink_sum = ga[0] + ga[n / 2] + ga[n - 1];
    return NULL;
}

int main(void)
{
    if (posix_memalign((void **)&ga, 4096, NELEM * sizeof(double)) != 0 ||
        posix_memalign((void **)&gb, 4096, NELEM * sizeof(double)) != 0 ||
        posix_memalign((void **)&gc, 4096, NELEM * sizeof(double)) != 0) {
        fprintf(stderr, "alloc failed\n");
        return 1;
    }
    double bytes = 3.0 * 8.0 * (double)NELEM;
    printf("PROBE triad a=b+s*c elem=double nelem=%zu mib_per_array=%zu bytes_per_pass=%.0f reps=%d timing=CLOCK_MONOTONIC report=best\n",
           NELEM, NELEM * sizeof(double) / (1024 * 1024), bytes, REPS);
    fflush(stdout);
    for (size_t ci = 0; ci < sizeof(THREAD_COUNTS) / sizeof(THREAD_COUNTS[0]); ci++) {
        int nt = THREAD_COUNTS[ci];
        pthread_t th[32];
        struct targ args[32];
        best_s = 0.0;
        if (pthread_barrier_init(&bar, NULL, (unsigned)nt) != 0) return 1;
        for (int i = 0; i < nt; i++) {
            args[i].tid = i; args[i].nthreads = nt;
            if (pthread_create(&th[i], NULL, worker, &args[i]) != 0) return 1;
        }
        for (int i = 0; i < nt; i++) pthread_join(th[i], NULL);
        pthread_barrier_destroy(&bar);
        double gbs = bytes / best_s / 1e9;
        printf("BW threads=%d best_s=%.4f gb_s=%.2f check_a=%.1f sink=%.1f\n",
               nt, best_s, gbs, ga[NELEM / 2], sink_sum);
        fflush(stdout);
    }
    printf("PROBE DONE\n");
    free(ga); free(gb); free(gc);
    return 0;
}
