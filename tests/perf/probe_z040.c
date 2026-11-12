/* probe_z040.c - Z-040 False-sharing purge microbench (MimirAlloc Z13).
 *
 * Per-structure before/after for the Z-040 purge. The production
 * patch gives every multi-writer hot field its own 64 B line
 * (Z-016 rule of record: stride 64 B, 0 guard lines). This probe
 * replicates the EXACT access pattern of each purged structure
 * group in two layouts - packed (the pre-purge layout, proven by
 * the nm .bss offsets in the recorded run) and padded (stride
 * 64 B, the post-purge layout) - so the contention delta is
 * attributable per structure, which a whole-allocator run cannot
 * do (every public op touches several groups at once).
 *
 * Patterns:
 *   l1mix    4 counters, thread t increments ONLY counter t%4
 *            with a relaxed atomic fetch_add, no lock. Models the
 *            L1 audit call counters (mimir_alloc.c) and the core
 *            audit groups under concurrent DIFFERENT entry points:
 *            the counters are logically independent, so any cost
 *            of the packed layout is pure false sharing.
 *   l1same   CONTROL: every thread increments counter 0. True
 *            sharing: padding cannot help and the probe must show
 *            it (an honest control, not a verdict row).
 *   owner    N slots of a {key, owner, span} entry; thread t
 *            works ONLY slot t (neighbouring slots, the way
 *            unrelated blocks hash into neighbouring owner-table
 *            slots): per op = atomic load of key, atomic
 *            fetch_add on owner, atomic store of span. Entry
 *            stride 24 B (production tls_owner_entry today) vs
 *            64 B (purged). Lock-free, like production.
 *   locked7  7 plain counters under ONE pthread mutex; per op =
 *            lock, increment all 7, unlock. Models the J-002
 *            stats state + registry/span anchor groups, whose
 *            writes all happen under the one global lock.
 *   prelock2 2 counters, thread t increments ONLY counter t%2
 *            with a relaxed atomic fetch_add, no lock. Models
 *            the pre-lock atomic stats pair (alloc_failures /
 *            alloc_posture_nulls) and the audit pairs.
 *
 * Protocol (Z-004): threads pinned, physical cores first
 * (0-7 CCD0, 8-15 CCD1, 16-31 SMT), barrier-free go-flag start
 * like probe_z016, median of 5 reps, every rep's work verified
 * by exact counter sums (a wrong sum aborts the probe).
 * Metric: aggregate ns/op = wall / total ops across threads;
 * the false-sharing signal is the packed-vs-padded ratio at a
 * FIXED thread count (Z-016 discipline).
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
#define REPS 5

static long long now_ns(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (long long)ts.tv_sec*1000000000LL+ts.tv_nsec;}
static int cmp_d(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);}
static void pin(int cpu){cpu_set_t s;CPU_ZERO(&s);CPU_SET(cpu,&s);if(sched_setaffinity(0,sizeof(s),&s)!=0){perror("sched_setaffinity");exit(1);}}
static const int cpu_order[MAX_THREADS] = {
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
};

static _Atomic int go_flag;

/* ---------- generic counter patterns (l1mix / l1same / prelock2) -- */
static char *ctr_base;        /* 4096-aligned arena of counters */
static size_t ctr_stride;     /* 8 = packed, 64 = padded */
static int ctr_ncounters;
static int ctr_mode;          /* 0 = own counter t%nc, 1 = all on 0 */
static uint32_t ctr_iters;

static void *ctr_worker(void *p){
    long idx = (long)p; pin(cpu_order[idx]);
    int c = ctr_mode ? 0 : (int)(idx % ctr_ncounters);
    _Atomic uint64_t *ctr = (_Atomic uint64_t*)(ctr_base + (size_t)c * ctr_stride);
    while(atomic_load_explicit(&go_flag, memory_order_acquire) == 0){ }
    for(uint32_t i = 0; i < ctr_iters; i++)
        atomic_fetch_add_explicit(ctr, 1, memory_order_relaxed);
    return NULL;
}

static double run_ctr(int nthreads, uint64_t *sum_out){
    for(int i = 0; i < ctr_ncounters; i++)
        atomic_store_explicit((_Atomic uint64_t*)(ctr_base + (size_t)i * ctr_stride), 0, memory_order_relaxed);
    atomic_store(&go_flag, 0);
    pthread_t th[MAX_THREADS];
    for(int i = 0; i < nthreads; i++)
        if(pthread_create(&th[i], NULL, ctr_worker, (void*)(long)i) != 0){fprintf(stderr,"pthread_create failed\n");exit(1);}
    long long t0 = now_ns();
    atomic_store_explicit(&go_flag, 1, memory_order_release);
    for(int i = 0; i < nthreads; i++) pthread_join(th[i], NULL);
    long long t1 = now_ns();
    uint64_t sum = 0;
    for(int i = 0; i < ctr_ncounters; i++)
        sum += atomic_load_explicit((_Atomic uint64_t*)(ctr_base + (size_t)i * ctr_stride), memory_order_relaxed);
    *sum_out = sum;
    return (double)(t1 - t0) / ((double)nthreads * (double)ctr_iters);
}

/* ---------- owner-table pattern ---------------------------------- */
struct owner_ent { _Atomic uintptr_t key; _Atomic uint64_t owner; _Atomic uintptr_t span; };
static char *own_base;
static size_t own_stride;     /* 24 = packed, 64 = padded */
static uint32_t own_iters;

static void *own_worker(void *p){
    long idx = (long)p; pin(cpu_order[idx]);
    char *e = own_base + (size_t)idx * own_stride;
    _Atomic uintptr_t *key = (_Atomic uintptr_t*)e;
    _Atomic uint64_t *owner = (_Atomic uint64_t*)(e + 8);
    _Atomic uintptr_t *span = (_Atomic uintptr_t*)(e + 16);
    while(atomic_load_explicit(&go_flag, memory_order_acquire) == 0){ }
    for(uint32_t i = 0; i < own_iters; i++){
        (void)atomic_load_explicit(key, memory_order_acquire);
        atomic_fetch_add_explicit(owner, 1, memory_order_relaxed);
        atomic_store_explicit(span, (uintptr_t)i, memory_order_relaxed);
    }
    return NULL;
}

static double run_own(int nthreads, uint64_t *sum_out){
    for(int i = 0; i < MAX_THREADS; i++){
        char *e = own_base + (size_t)i * own_stride;
        atomic_store_explicit((_Atomic uintptr_t*)e, (uintptr_t)(i + 1), memory_order_relaxed);
        atomic_store_explicit((_Atomic uint64_t*)(e + 8), 0, memory_order_relaxed);
        atomic_store_explicit((_Atomic uintptr_t*)(e + 16), 0, memory_order_relaxed);
    }
    atomic_store(&go_flag, 0);
    pthread_t th[MAX_THREADS];
    for(int i = 0; i < nthreads; i++)
        if(pthread_create(&th[i], NULL, own_worker, (void*)(long)i) != 0){fprintf(stderr,"pthread_create failed\n");exit(1);}
    long long t0 = now_ns();
    atomic_store_explicit(&go_flag, 1, memory_order_release);
    for(int i = 0; i < nthreads; i++) pthread_join(th[i], NULL);
    long long t1 = now_ns();
    uint64_t sum = 0;
    for(int i = 0; i < nthreads; i++)
        sum += atomic_load_explicit((_Atomic uint64_t*)(own_base + (size_t)i * own_stride + 8), memory_order_relaxed);
    *sum_out = sum;
    return (double)(t1 - t0) / ((double)nthreads * (double)own_iters);
}

/* ---------- locked-group pattern (locked7) ------------------------ */
static pthread_mutex_t grp_lock = PTHREAD_MUTEX_INITIALIZER;
static char *grp_base;
static size_t grp_stride;     /* 8 = packed, 64 = padded */
static uint32_t grp_iters;
#define GRP_N 7

static void *grp_worker(void *p){
    long idx = (long)p; pin(cpu_order[idx]);
    while(atomic_load_explicit(&go_flag, memory_order_acquire) == 0){ }
    for(uint32_t i = 0; i < grp_iters; i++){
        pthread_mutex_lock(&grp_lock);
        for(int c = 0; c < GRP_N; c++)
            *(uint64_t*)(grp_base + (size_t)c * grp_stride) += 1;
        pthread_mutex_unlock(&grp_lock);
    }
    return NULL;
}

static double run_grp(int nthreads, uint64_t *sum_out){
    for(int c = 0; c < GRP_N; c++)
        *(uint64_t*)(grp_base + (size_t)c * grp_stride) = 0;
    atomic_store(&go_flag, 0);
    pthread_t th[MAX_THREADS];
    for(int i = 0; i < nthreads; i++)
        if(pthread_create(&th[i], NULL, grp_worker, (void*)(long)i) != 0){fprintf(stderr,"pthread_create failed\n");exit(1);}
    long long t0 = now_ns();
    atomic_store_explicit(&go_flag, 1, memory_order_release);
    for(int i = 0; i < nthreads; i++) pthread_join(th[i], NULL);
    long long t1 = now_ns();
    uint64_t sum = 0;
    for(int c = 0; c < GRP_N; c++)
        sum += *(uint64_t*)(grp_base + (size_t)c * grp_stride);
    *sum_out = sum;
    return (double)(t1 - t0) / ((double)nthreads * (double)grp_iters);
}

/* ---------- driver ------------------------------------------------ */
static void report(const char *pattern, const char *layout, int nthreads,
                   double (*run)(int, uint64_t*), uint64_t expect_per_thread){
    double vals[REPS]; uint64_t sum = 0;
    for(int r = 0; r < REPS; r++){
        uint64_t s = 0; vals[r] = run(nthreads, &s);
        if(r == 0) sum = s;
        if(s != expect_per_thread * (uint64_t)nthreads){
            fprintf(stderr,"SUM MISMATCH pattern=%s layout=%s threads=%d got=%llu want=%llu\n",
                    pattern, layout, nthreads, (unsigned long long)s,
                    (unsigned long long)(expect_per_thread * (uint64_t)nthreads));
            exit(1);
        }
    }
    double srt[REPS]; memcpy(srt, vals, sizeof(vals)); qsort(srt, REPS, sizeof(double), cmp_d);
    printf("Z040 pattern=%s layout=%s threads=%d median_ns_per_op=%.3f min_ns=%.3f max_ns=%.3f sum=%llu reps_ns=",
           pattern, layout, nthreads, srt[REPS/2], srt[0], srt[REPS-1], (unsigned long long)sum);
    for(int r = 0; r < REPS; r++) printf("%s%.3f", r?",":"", vals[r]);
    printf("\n"); fflush(stdout);
}

int main(void){
    printf("PROBE Z-040 reps=%d pinning=physical-first(0-7,8-15,16-31)\n", REPS);
    const int counts[] = {2, 8, 16, 32};
    if(posix_memalign((void**)&ctr_base, 4096, 4096) != 0) return 1;
    if(posix_memalign((void**)&own_base, 4096, (size_t)MAX_THREADS * 64) != 0) return 1;
    if(posix_memalign((void**)&grp_base, 4096, 4096) != 0) return 1;

    /* l1mix: 4 independent counters, iters 2M */
    ctr_ncounters = 4; ctr_mode = 0; ctr_iters = 2000000u;
    for(int li = 0; li < 2; li++){
        ctr_stride = li ? 64 : 8;
        for(size_t ci = 0; ci < sizeof(counts)/sizeof(counts[0]); ci++)
            report("l1mix", li ? "padded" : "packed", counts[ci], run_ctr, ctr_iters);
    }
    /* l1same: true-sharing control, iters 2M */
    ctr_mode = 1;
    for(int li = 0; li < 2; li++){
        ctr_stride = li ? 64 : 8;
        for(size_t ci = 0; ci < sizeof(counts)/sizeof(counts[0]); ci++)
            report("l1same", li ? "padded" : "packed", counts[ci], run_ctr, ctr_iters);
    }
    /* prelock2: 2 independent atomic counters, iters 2M */
    ctr_ncounters = 2; ctr_mode = 0;
    for(int li = 0; li < 2; li++){
        ctr_stride = li ? 64 : 8;
        for(size_t ci = 0; ci < sizeof(counts)/sizeof(counts[0]); ci++)
            report("prelock2", li ? "padded" : "packed", counts[ci], run_ctr, ctr_iters);
    }
    /* owner: entry stride 24 vs 64, iters 500k */
    own_iters = 500000u;
    for(int li = 0; li < 2; li++){
        own_stride = li ? 64 : 24;
        for(size_t ci = 0; ci < sizeof(counts)/sizeof(counts[0]); ci++)
            report("owner", li ? "padded" : "packed", counts[ci], run_own, own_iters);
    }
    /* locked7: 7 counters under one mutex, iters 200k */
    grp_iters = 200000u;
    for(int li = 0; li < 2; li++){
        grp_stride = li ? 64 : 8;
        for(size_t ci = 0; ci < sizeof(counts)/sizeof(counts[0]); ci++)
            report("locked7", li ? "padded" : "packed", counts[ci], run_grp, (uint64_t)GRP_N * grp_iters);
    }
    printf("PROBE DONE Z-040\n");
    return 0;
}
