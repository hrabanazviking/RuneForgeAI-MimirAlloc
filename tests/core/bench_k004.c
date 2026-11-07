/* bench_k004.c - K-004 build-flavour bench. Workloads 1-2
 * reuse bench_j008's shape exactly (20000x malloc(64)/
 * memset/free; 2000x malloc(4096)/memset/free). Workload 3
 * is new: 200 rounds of 256 live small64 then free-all.
 * The bench TU is compiled -O2 in BOTH legs; only the
 * allocator objects differ (-O0 shipped vs -O2). One run
 * per process; driver does 5 reps per leg, fresh process. */
#define _POSIX_C_SOURCE 200809L
#include "mimir_alloc.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static long long now_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (long long)t.tv_sec*1000000000LL+t.tv_nsec;}
int main(void){
  unsigned long long sink=0;
  long long t0=now_ns();
  for(int i=0;i<20000;i++){void *p=mimir_malloc(64);if(!p)return 2;memset(p,1,64);sink+=((unsigned char*)p)[0];mimir_free(p);}
  long long t1=now_ns();
  for(int i=0;i<2000;i++){void *p=mimir_malloc(4096);if(!p)return 2;memset(p,1,4096);sink+=((unsigned char*)p)[0];mimir_free(p);}
  long long t2=now_ns();
  void *live[256];
  for(int r=0;r<200;r++){for(int i=0;i<256;i++){live[i]=mimir_malloc(64);if(!live[i])return 2;memset(live[i],1,64);}for(int i=0;i<256;i++){sink+=((unsigned char*)live[i])[0];mimir_free(live[i]);}}
  long long t3=now_ns();
  printf("small64: %lld ns/op (%lld ns / 20000 ops)\n",(t1-t0)/20000,t1-t0);
  printf("large4096: %lld ns/op (%lld ns / 2000 ops)\n",(t2-t1)/2000,t2-t1);
  printf("burst256: %lld ns/op (%lld ns / 51200 ops)\n",(t3-t2)/51200,t3-t2);
  printf("sink: %llu\n",sink);
  return 0;
}
