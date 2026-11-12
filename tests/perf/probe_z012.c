/* probe_z012.c - Z-012 CCD and SMT behaviour (MimirAlloc Z13, Wave 2).
 *
 * Part A: cache-line ping-pong latency. Two threads pinned to a
 * CPU pair bounce ownership of ONE 64-byte cache line via an
 * atomic counter (turn-taking stores). ns/transfer is the cost of
 * moving a shared line between the two CPUs: SMT sibling, intra-
 * CCD (same 32 MiB L3), and cross-CCD pairs are compared.
 * Part B: SMT vs physical-core throughput for memory work. Each
 * thread streams over its OWN private 64 MiB buffer (write pass +
 * read pass, no sharing), pinned: 1 thread alone, 2 threads on
 * SMT siblings of one core, 2 threads on distinct cores of one
 * CCD, 2 threads on distinct CCDs. Aggregate GB/s shows how much
 * a sibling adds over a lone thread vs what a real core adds.
 *
 * Topology (sysfs, verified by the runner at run time):
 *   CCD0: CPUs 0-7 (+ SMT siblings 16-23), L3 32 MiB shared
 *   CCD1: CPUs 8-15 (+ SMT siblings 24-31), L3 32 MiB shared
 *   SMT sibling of CPU n (n<16) is CPU n+16.
 * Timing: clock_gettime(CLOCK_MONOTONIC). Medians of reps.
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

static long long now_ns(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (long long)ts.tv_sec*1000000000LL+ts.tv_nsec;}
static int cmp_d(const void*a,const void*b){double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);}
static void pin(int cpu){cpu_set_t s;CPU_ZERO(&s);CPU_SET(cpu,&s);if(sched_setaffinity(0,sizeof(s),&s)!=0){perror("sched_setaffinity");exit(1);}}

/* ---- Part A: ping-pong ---- */
static _Atomic uint64_t pp_counter; /* sole occupant of its line */
static _Atomic int pp_go;
struct pp_arg{int cpu;int parity;uint64_t transfers;};
static void *pp_thread(void *p){
    struct pp_arg *a=(struct pp_arg*)p; pin(a->cpu);
    while(atomic_load_explicit(&pp_go,memory_order_acquire)==0){ }
    for(uint64_t want=(uint64_t)a->parity; want<a->transfers; want+=2){
        while(atomic_load_explicit(&pp_counter,memory_order_acquire)!=want){ }
        atomic_store_explicit(&pp_counter,want+1,memory_order_release);
    }
    return NULL;
}
static double pingpong(int ca,int cb,uint64_t transfers){
    atomic_store(&pp_counter,0); atomic_store(&pp_go,0);
    struct pp_arg a={ca,0,transfers}, b={cb,1,transfers};
    pthread_t ta,tb;
    if(pthread_create(&ta,NULL,pp_thread,&a)!=0||pthread_create(&tb,NULL,pp_thread,&b)!=0){fprintf(stderr,"pthread_create failed\n");exit(1);}
    long long t0=now_ns();
    atomic_store_explicit(&pp_go,1,memory_order_release);
    pthread_join(ta,NULL); pthread_join(tb,NULL);
    long long t1=now_ns();
    return (double)(t1-t0)/(double)transfers;
}

/* ---- Part B: streaming throughput ---- */
#define BW_BYTES (64ULL<<20)
#define BW_PASSES 5
struct bw_arg{int cpu;uint64_t *buf;double gbs;uint64_t sink;};
static void *bw_thread(void *p){
    struct bw_arg *a=(struct bw_arg*)p; pin(a->cpu);
    size_t n=BW_BYTES/sizeof(uint64_t);
    for(size_t i=0;i<n;i++) a->buf[i]=(uint64_t)i; /* warm-up + first touch */
    uint64_t sink=0; long long t0=now_ns();
    for(int pass=0;pass<BW_PASSES;pass++){
        for(size_t i=0;i<n;i++) a->buf[i]=(uint64_t)(i^0x9E3779B9ULL);
        for(size_t i=0;i<n;i++) sink+=a->buf[i];
    }
    long long t1=now_ns();
    double secs=(double)(t1-t0)/1e9;
    a->gbs=(double)BW_BYTES*BW_PASSES*2.0/secs/1e9; /* write+read bytes */
    a->sink=sink; return NULL;
}
static double bandwidth(const int *cpus,int nthreads){
    struct bw_arg args[2]; pthread_t th[2];
    for(int i=0;i<nthreads;i++){
        args[i].cpu=cpus[i]; args[i].gbs=0; args[i].sink=0;
        if(posix_memalign((void**)&args[i].buf,4096,BW_BYTES)!=0){fprintf(stderr,"alloc fail\n");exit(1);}
    }
    for(int i=0;i<nthreads;i++) if(pthread_create(&th[i],NULL,bw_thread,&args[i])!=0){fprintf(stderr,"pthread_create failed\n");exit(1);}
    double agg=0; uint64_t sink=0;
    for(int i=0;i<nthreads;i++){pthread_join(th[i],NULL); agg+=args[i].gbs; sink+=args[i].sink; free(args[i].buf);}
    printf("  (bw sink=%llu)\n",(unsigned long long)sink);
    return agg;
}

int main(void){
    printf("PROBE Z-012 timing=CLOCK_MONOTONIC\n");
    const struct{int a,b;const char *kind;} pairs[]={
        {0,16,"smt-sibling"},{0,1,"intra-ccd"},{0,7,"intra-ccd-far"},
        {8,9,"intra-ccd-ccd1"},{0,8,"cross-ccd"},{7,8,"cross-ccd-edge"}};
    for(size_t i=0;i<sizeof(pairs)/sizeof(pairs[0]);i++){
        double vals[5];
        for(int r=0;r<5;r++) vals[r]=pingpong(pairs[i].a,pairs[i].b,200000);
        double s[5]; memcpy(s,vals,sizeof(vals)); qsort(s,5,sizeof(double),cmp_d);
        printf("PINGPONG kind=%s cpu_a=%d cpu_b=%d transfers=200000 median_ns=%.2f min_ns=%.2f max_ns=%.2f reps_ns=%.2f,%.2f,%.2f,%.2f,%.2f\n",
            pairs[i].kind,pairs[i].a,pairs[i].b,s[2],s[0],s[4],vals[0],vals[1],vals[2],vals[3],vals[4]);
        fflush(stdout);
    }
    const struct{const char *name;int cpus[2];int n;} cfgs[]={
        {"single-cpu0",{0,-1},1},{"smt-siblings-0-16",{0,16},2},
        {"intra-ccd-0-1",{0,1},2},{"cross-ccd-0-8",{0,8},2}};
    for(size_t i=0;i<sizeof(cfgs)/sizeof(cfgs[0]);i++){
        double vals[3];
        for(int r=0;r<3;r++) vals[r]=bandwidth(cfgs[i].cpus,cfgs[i].n);
        double s[3]; memcpy(s,vals,sizeof(vals)); qsort(s,3,sizeof(double),cmp_d);
        printf("BANDWIDTH cfg=%s threads=%d buf_per_thread=%llu passes=%d median_gbs=%.2f min_gbs=%.2f max_gbs=%.2f reps_gbs=%.2f,%.2f,%.2f\n",
            cfgs[i].name,cfgs[i].n,(unsigned long long)BW_BYTES,BW_PASSES,s[1],s[0],s[2],vals[0],vals[1],vals[2]);
        fflush(stdout);
    }
    printf("PROBE DONE Z-012\n");
    return 0;
}
