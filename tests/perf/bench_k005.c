/* bench_k005.c - K-005 locality + metadata placement.
 * MimirAlloc only (banked -O0 LIBOBJS; harness -O2 like
 * bench_k002). One live set per workload; access legs over
 * the SAME set print identical checksums (equivalence).
 * Whole-pass CLOCK_MONOTONIC; reps are fresh processes
 * driven by run_bench_k005.sh. Warm-up: 1 untimed pass /
 * 1/10 churn per process (M-6). */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include "mimir_alloc.h"
static uint64_t now_ns(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (uint64_t)ts.tv_sec*UINT64_C(1000000000)+(uint64_t)ts.tv_nsec;}
static long minflt(void){struct rusage r;getrusage(RUSAGE_SELF,&r);return r.ru_minflt;}
static uint64_t rng_state;
static uint32_t rnd(void){rng_state^=rng_state<<13;rng_state^=rng_state>>7;rng_state^=rng_state<<17;return (uint32_t)(rng_state>>32);}
static void shuffle(size_t *p,size_t n){size_t i;for(i=n-1;i>0;i--){size_t j=rnd()%(i+1);size_t t=p[i];p[i]=p[j];p[j]=t;}}
static uint64_t access_pass(void **ptrs,const size_t *ord,size_t n){
 uint64_t s=0;size_t i;for(i=0;i<n;i++){unsigned char *b=ptrs[ord[i]];s+=b[0];s+=*(uint64_t*)b;}return s;}
/* Access legs over a uniform live set. mode 0=seq 1=rand */
static int leg_access(const char *name,size_t sz,size_t n,int mode,int passes){
 void **ptrs=malloc(n*sizeof(void*));size_t *ord=malloc(n*sizeof(size_t));
 size_t i;long f0,f1;uint64_t ck=0,t0,t1;int p;
 if(!ptrs||!ord)return 2;
 rng_state=UINT64_C(0x9E3779B97F4A7C15);
 for(i=0;i<n;i++)ord[i]=i;
 if(mode==1)shuffle(ord,n);
 f0=minflt();
 for(i=0;i<n;i++){ptrs[i]=mimir_malloc(sz);if(!ptrs[i])return 3;memset(ptrs[i],(int)(i&0x3F)+1,sz);}
 f1=minflt();
 ck=access_pass(ptrs,ord,n); /* warm-up */
 t0=now_ns();for(p=0;p<passes;p++)ck+=access_pass(ptrs,ord,n);t1=now_ns();
 printf("mimir,%s,%zu,%llu,%llu,%llu,%ld\n",name,n*(size_t)passes,(unsigned long long)(t1-t0),(unsigned long long)((t1-t0)/(n*(size_t)passes)),(unsigned long long)ck,f1-f0);
 for(i=0;i<n;i++){mimir_free(ptrs[i]);} free(ptrs); free(ord); return 0;}
/* Mixed interleaved set: access in alloc order vs grouped */
static int leg_mixed(int grouped){
 const size_t n=2048;void **ptrs=malloc(n*sizeof(void*));size_t *ord=malloc(n*sizeof(size_t));
 size_t i,k=0;long f0,f1;uint64_t ck=0,t0,t1;int p;const int passes=200;
 if(!ptrs||!ord)return 2;
 f0=minflt();
 for(i=0;i<n;i++){size_t sz=(i%2==0)?64:1024;ptrs[i]=mimir_malloc(sz);if(!ptrs[i])return 3;memset(ptrs[i],(int)(i&0x3F)+1,sz);}
 f1=minflt();
 if(!grouped){for(i=0;i<n;i++)ord[k++]=i;}
 else{for(i=0;i<n;i+=2)ord[k++]=i;for(i=1;i<n;i+=2)ord[k++]=i;}
 ck=access_pass(ptrs,ord,n);
 t0=now_ns();for(p=0;p<passes;p++)ck+=access_pass(ptrs,ord,n);t1=now_ns();
 printf("mimir,%s,%zu,%llu,%llu,%llu,%ld\n",grouped?"mixed_grouped":"mixed_allocorder",n*(size_t)passes,(unsigned long long)(t1-t0),(unsigned long long)((t1-t0)/(n*(size_t)passes)),(unsigned long long)ck,f1-f0);
 for(i=0;i<n;i++){mimir_free(ptrs[i]);} free(ptrs); free(ord); return 0;}
/* Churn legs: single alloc/touch/free. small stays in one
 * span page (header+user same page); large takes the
 * mapping path (descriptor on its own reserve page). */
static int leg_churn(const char *name,size_t sz,size_t ops){
 size_t i;long f0,f1;uint64_t ck=0,t0,t1;
 for(i=0;i<ops/10;i++){void *p=mimir_malloc(sz);if(!p)return 3;memset(p,1,sz);ck+=((unsigned char*)p)[0];mimir_free(p);}
 f0=minflt();t0=now_ns();
 for(i=0;i<ops;i++){void *p=mimir_malloc(sz);if(!p)return 3;memset(p,1,sz);ck+=((unsigned char*)p)[0];mimir_free(p);}
 t1=now_ns();f1=minflt();
 printf("mimir,%s,%zu,%llu,%llu,%llu,%ld\n",name,ops,(unsigned long long)(t1-t0),(unsigned long long)((t1-t0)/ops),(unsigned long long)ck,f1-f0);
 return 0;}
int main(int argc,char **argv){
 const char *only=(argc>1)?argv[1]:"all";int rc=0;
 #define RUN(cond,call) do{if(cond){int r=(call);if(r)rc=r;}}while(0)
 RUN(!strcmp(only,"all")||!strcmp(only,"seq64"),leg_access("seq64",64,4096,0,200));
 RUN(!strcmp(only,"all")||!strcmp(only,"rand64"),leg_access("rand64",64,4096,1,200));
 RUN(!strcmp(only,"all")||!strcmp(only,"seq1024"),leg_access("seq1024",1024,1024,0,200));
 RUN(!strcmp(only,"all")||!strcmp(only,"rand1024"),leg_access("rand1024",1024,1024,1,200));
 RUN(!strcmp(only,"all")||!strcmp(only,"mixed_allocorder"),leg_mixed(0));
 RUN(!strcmp(only,"all")||!strcmp(only,"mixed_grouped"),leg_mixed(1));
 RUN(!strcmp(only,"all")||!strcmp(only,"churn64"),leg_churn("churn64",64,100000));
 RUN(!strcmp(only,"all")||!strcmp(only,"churn8192"),leg_churn("churn8192",8192,5000));
 return rc;}
