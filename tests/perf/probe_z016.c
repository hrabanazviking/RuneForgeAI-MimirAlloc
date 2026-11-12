/* probe_z016.c - Z-016 False-sharing ladder (MimirAlloc Z13, Wave 2).
 *
 * Contended cache-line microbench. N threads each hammer ONLY their
 * own atomic counter; nothing is logically shared. The sole variable
 * is the counter LAYOUT, i.e. how counters map onto 64-byte lines:
 *   same-line : stride   8 B - 8 counters share ONE 64 B line
 *               (true false sharing: every increment needs exclusive
 *               ownership of a line 7 other threads are writing)
 *   neighbour : stride  64 B - one counter per line, lines adjacent
 *               (no shared line; tests adjacent-line interference)
 *   padded128 : stride 128 B - one counter per line + 1 guard line
 *   padded256 : stride 256 B - one counter per line + 3 guard lines
 * Thread counts 2, 4, 8, 16, 32. Pinning fills physical cores first
 * per the Z-004 protocol (CCD law) using the Z-012 sysfs topology:
 *   order = 0-7 (CCD0 physical), 8-15 (CCD1 physical),
 *           16-23 (CCD0 SMT), 24-31 (CCD1 SMT).
 * So 2/4/8-thread rows are intra-CCD0, 16 threads span both CCDs on
 * physical cores, 32 threads add every SMT sibling.
 * Metric: wall time for ITERS increments per thread; aggregate
 * Mops/s and ns/op (wall / total increments), median of 5 reps.
 * Work is verified every rep: sum of counters == threads * ITERS.
 * Counters are only ever touched atomically (stores to zero before
 * thread creation, relaxed loads for the sum after pthread_join) -
 * no plain-pointer access to the _Atomic objects (resume fix, C11).
 * Timing: clock_gettime(CLOCK_MONOTONIC). Increments are relaxed
 * atomic fetch_add (a locked RMW on x86-64: forces line ownership
 * transfer without serialising on a shared total).
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

#define MAX_THREADS 32
#define ITERS 2000000u
#define REPS 5

static long long now_ns(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (long long)ts.tv_sec*1000000000LL+ts.tv_nsec;}
static int cmp_d(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);}
static void pin(int cpu){cpu_set_t s;CPU_ZERO(&s);CPU_SET(cpu,&s);if(sched_setaffinity(0,sizeof(s),&s)!=0){perror("sched_setaffinity");exit(1);}}

/* CPU fill order: physical cores of both CCDs, then SMT siblings. */
static const int cpu_order[MAX_THREADS] = {
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
};

static _Atomic int go_flag;
static char *slots_base;      /* 4096-aligned arena */
static size_t slot_stride;    /* bytes between counters */

struct targ { int idx; int cpu; };
static void *worker(void *p){
    struct targ *a = (struct targ*)p; pin(a->cpu);
    _Atomic uint64_t *ctr = (_Atomic uint64_t*)(slots_base + (size_t)a->idx * slot_stride);
    while(atomic_load_explicit(&go_flag, memory_order_acquire) == 0){ }
    for(uint32_t i = 0; i < ITERS; i++)
        atomic_fetch_add_explicit(ctr, 1, memory_order_relaxed);
    return NULL;
}

static double run_once(int nthreads, uint64_t *sum_out){
    for(int i = 0; i < MAX_THREADS; i++)
        atomic_store_explicit((_Atomic uint64_t*)(slots_base + (size_t)i * slot_stride), 0, memory_order_relaxed);
    atomic_store(&go_flag, 0);
    pthread_t th[MAX_THREADS]; struct targ args[MAX_THREADS];
    for(int i = 0; i < nthreads; i++){
        args[i].idx = i; args[i].cpu = cpu_order[i];
        if(pthread_create(&th[i], NULL, worker, &args[i]) != 0){fprintf(stderr,"pthread_create failed\n");exit(1);}
    }
    long long t0 = now_ns();
    atomic_store_explicit(&go_flag, 1, memory_order_release);
    for(int i = 0; i < nthreads; i++) pthread_join(th[i], NULL);
    long long t1 = now_ns();
    uint64_t sum = 0;
    for(int i = 0; i < nthreads; i++)
        sum += atomic_load_explicit((_Atomic uint64_t*)(slots_base + (size_t)i * slot_stride), memory_order_relaxed);
    *sum_out = sum;
    double total_ops = (double)nthreads * (double)ITERS;
    return (double)(t1 - t0) / total_ops; /* ns per increment */
}

int main(void){
    printf("PROBE Z-016 timing=CLOCK_MONOTONIC iters_per_thread=%u reps=%d\n", ITERS, REPS);
    const struct { const char *name; size_t stride; } layouts[] = {
        {"same-line", 8}, {"neighbour", 64}, {"padded128", 128}, {"padded256", 256}
    };
    const int counts[] = {2, 4, 8, 16, 32};
    if(posix_memalign((void**)&slots_base, 4096, (size_t)MAX_THREADS * 256) != 0){fprintf(stderr,"memalign failed\n");return 1;}
    for(size_t li = 0; li < sizeof(layouts)/sizeof(layouts[0]); li++){
        slot_stride = layouts[li].stride;
        for(size_t ci = 0; ci < sizeof(counts)/sizeof(counts[0]); ci++){
            int n = counts[ci];
            double vals[REPS]; uint64_t sum = 0;
            for(int r = 0; r < REPS; r++){
                uint64_t s = 0; vals[r] = run_once(n, &s);
                if(r == 0) sum = s;
                if(s != (uint64_t)n * ITERS){fprintf(stderr,"SUM MISMATCH layout=%s threads=%d got=%llu\n", layouts[li].name, n, (unsigned long long)s); return 1;}
            }
            double srt[REPS]; memcpy(srt, vals, sizeof(vals)); qsort(srt, REPS, sizeof(double), cmp_d);
            double med = srt[REPS/2];
            printf("LADDER layout=%s stride=%zu threads=%d iters=%u median_ns_per_op=%.3f min_ns=%.3f max_ns=%.3f mops=%.2f sum=%llu reps_ns=",
                   layouts[li].name, layouts[li].stride, n, ITERS, med, srt[0], srt[REPS-1],
                   1000.0/med, (unsigned long long)sum);
            for(int r = 0; r < REPS; r++) printf("%s%.3f", r?",":"", vals[r]);
            printf("\n"); fflush(stdout);
        }
    }
    printf("PROBE DONE Z-016\n");
    return 0;
}
