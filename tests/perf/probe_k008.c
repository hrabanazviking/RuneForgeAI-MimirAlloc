/* probe_k008.c - K-008 Phase 11 tail-latency + RSS/PSS probe.
 * One source, two binaries (bench_k002/k006/k007 precedent):
 * glibc leg (-DLEG_GLIBC) and MimirAlloc leg (banked LIBOBJS,
 * shipped -O2 flavour per K-004; harness itself -O2).
 * Modes:
 *   (no arg) "dist": per-op latency distributions + footprint.
 *     Per-call latencies are timed individually with
 *     CLOCK_MONOTONIC around ONE alloc or ONE free call,
 *     collected, sorted, and reported as p50/p95/p99/max
 *     (nearest-rank, idx=floor(p*n/100) clamped) plus mean.
 *     Patterns (K-003/K-007): inter_small = 64 B alloc/touch/
 *     free batch 1; burst512 = 64 B alloc-all/touch-all/
 *     free-all LIFO batch 512; large_inter = sizes cycling
 *     2048..1048576 B alloc/touch/free batch 1. A timer_floor
 *     pattern times back-to-back clock_gettime pairs so the
 *     timing overhead floor is printed, not hidden.
 *     Footprint: held live sets in the same process; VmRSS /
 *     VmHWM from /proc/self/status and Pss from
 *     /proc/self/smaps_rollup, baseline taken in-process
 *     immediately before the hold, after-free re-read for
 *     retained memory. Process figures only; the allocator-
 *     attributable figure is the stated delta minus the
 *     requested bytes (I-007 separation precedent).
 *   "energy": sustained inter_small loop for ~6 s; the
 *     runner samples the Pi 5 PMIC (vcgencmd pmic_read_adc)
 *     around it. This probe prints only ops + elapsed.
 * Warm-up: 1/10 scale untimed pass per pattern (K-001 M-6).
 * Repetitions are fresh processes (run_bench_k008.sh).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef LEG_GLIBC
#define A_ALLOC malloc
#define A_FREE free
#define LEGNAME "glibc"
#else
#include "mimir_alloc.h"
#define A_ALLOC mimir_malloc
#define A_FREE mimir_free
#define LEGNAME "mimiralloc"
#endif
static uint64_t now_ns(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (uint64_t)ts.tv_sec*UINT64_C(1000000000)+(uint64_t)ts.tv_nsec;}
static int cmp_u64(const void *a,const void *b){uint64_t x=*(const uint64_t*)a,y=*(const uint64_t*)b;return (x>y)-(x<y);}
static uint64_t pct(uint64_t *v,size_t n,double p){size_t i=(size_t)(p*(double)n/100.0);if(i>=n)i=n-1;return v[i];}
static void report(const char *pat,const char *kind,uint64_t *v,size_t n,uint64_t ck){
 size_t i;uint64_t sum=0;for(i=0;i<n;i++)sum+=v[i];qsort(v,n,sizeof(uint64_t),cmp_u64);
 printf("LAT,%s,%s,%s,%zu,%llu,%llu,%llu,%llu,%llu,%llu\n",LEGNAME,pat,kind,n,
  (unsigned long long)pct(v,n,50),(unsigned long long)pct(v,n,95),
  (unsigned long long)pct(v,n,99),(unsigned long long)v[n-1],
  (unsigned long long)(sum/n),(unsigned long long)ck);fflush(stdout);}
static unsigned char touch(void *p,size_t n,unsigned s){unsigned char *b=(unsigned char*)p;b[0]=(unsigned char)(s&0xFFu);b[n-1]=(unsigned char)((s>>3)&0xFFu);return (unsigned char)(b[0]^b[n-1]);}
#define MAXS 51200
static uint64_t abuf[MAXS],fbuf[MAXS];
static void *ptrs[512];
static const size_t large_sizes[]={2048,8192,16384,65536,262144,1048576};
static uint64_t pat_inter(size_t iters,int timed){
 size_t i;uint64_t ck=0;
 for(i=0;i<iters;i++){uint64_t t0,t1;void *p;
  if(timed){t0=now_ns();p=A_ALLOC(64);t1=now_ns();abuf[i]=t1-t0;}
  else p=A_ALLOC(64);
  if(!p){fprintf(stderr,"FAIL inter null\n");exit(1);}
  ck+=touch(p,64,(unsigned)i);
  if(timed){t0=now_ns();A_FREE(p);t1=now_ns();fbuf[i]=t1-t0;}
  else A_FREE(p);}
 if(timed){report("inter_small","alloc",abuf,iters,ck);report("inter_small","free",fbuf,iters,ck);}
 return ck;}
static uint64_t pat_burst(size_t cycles,int timed){
 size_t c,i,ai=0,fi=0;uint64_t ck=0;
 for(c=0;c<cycles;c++){
  for(i=0;i<512;i++){uint64_t t0,t1;void *p;
   if(timed){t0=now_ns();p=A_ALLOC(64);t1=now_ns();abuf[ai++]=t1-t0;}
   else p=A_ALLOC(64);
   if(!p){fprintf(stderr,"FAIL burst null\n");exit(1);}
   ptrs[i]=p;}
  for(i=0;i<512;i++)ck+=touch(ptrs[i],64,(unsigned)(c+i));
  for(i=512;i>0;i--){uint64_t t0,t1;
   if(timed){t0=now_ns();A_FREE(ptrs[i-1]);t1=now_ns();fbuf[fi++]=t1-t0;}
   else A_FREE(ptrs[i-1]);}}
 if(timed){report("burst512","alloc",abuf,ai,ck);report("burst512","free",fbuf,fi,ck);}
 return ck;}
static uint64_t pat_large(size_t iters,int timed){
 size_t i;uint64_t ck=0;
 for(i=0;i<iters;i++){size_t n=large_sizes[i%6];uint64_t t0,t1;void *p;
  if(timed){t0=now_ns();p=A_ALLOC(n);t1=now_ns();abuf[i]=t1-t0;}
  else p=A_ALLOC(n);
  if(!p){fprintf(stderr,"FAIL large null\n");exit(1);}
  ck+=touch(p,n,(unsigned)i);
  if(timed){t0=now_ns();A_FREE(p);t1=now_ns();fbuf[i]=t1-t0;}
  else A_FREE(p);}
 if(timed){report("large_inter","alloc",abuf,iters,ck);report("large_inter","free",fbuf,iters,ck);}
 return ck;}
static long read_kb(const char *path,const char *key){
 FILE *f=fopen(path,"r");char line[256];long v=-1;size_t kl=strlen(key);
 if(!f)return -1;
 while(fgets(line,sizeof line,f)){if(!strncmp(line,key,kl)){v=strtol(line+kl,NULL,10);break;}}
 fclose(f);return v;}
static void memline(const char *set,long live,long req_kb,long br,long bp,long hr,long hp,long ar,long ap,long hwm,uint64_t ck){
 printf("MEM,%s,%s,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%llu\n",LEGNAME,set,live,req_kb,br,bp,hr,hp,hr-br,hp-bp,ar-br,ap-bp,hwm,(unsigned long long)ck);fflush(stdout);}
static void footprint(void){
 long br,bp,hr,hp,ar,ap,hwm;uint64_t ck=0;size_t i;
 /* small hold: 20000 x 64 B = 1250 KiB requested */
 static void *hold[20000];
 br=read_kb("/proc/self/status","VmRSS:");bp=read_kb("/proc/self/smaps_rollup","Pss:");
 for(i=0;i<20000;i++){hold[i]=A_ALLOC(64);if(!hold[i]){fprintf(stderr,"FAIL hold null\n");exit(1);}ck+=touch(hold[i],64,(unsigned)i);}
 hr=read_kb("/proc/self/status","VmRSS:");hp=read_kb("/proc/self/smaps_rollup","Pss:");hwm=read_kb("/proc/self/status","VmHWM:");
 for(i=0;i<20000;i++)A_FREE(hold[i]);
 ar=read_kb("/proc/self/status","VmRSS:");ap=read_kb("/proc/self/smaps_rollup","Pss:");
 memline("small_hold",20000,1250,br,bp,hr,hp,ar,ap,hwm,ck);
 /* large hold: 24 blocks cycling large_sizes = 5480 KiB requested, touched per page */
 static void *lhold[24];long req=0;
 br=read_kb("/proc/self/status","VmRSS:");bp=read_kb("/proc/self/smaps_rollup","Pss:");
 for(i=0;i<24;i++){size_t n=large_sizes[i%6],off;req+=(long)(n/1024);lhold[i]=A_ALLOC(n);if(!lhold[i]){fprintf(stderr,"FAIL lhold null\n");exit(1);}for(off=0;off<n;off+=4096)ck+=touch((unsigned char*)lhold[i]+off,1,(unsigned)(i+off));}
 hr=read_kb("/proc/self/status","VmRSS:");hp=read_kb("/proc/self/smaps_rollup","Pss:");hwm=read_kb("/proc/self/status","VmHWM:");
 for(i=0;i<24;i++)A_FREE(lhold[i]);
 ar=read_kb("/proc/self/status","VmRSS:");ap=read_kb("/proc/self/smaps_rollup","Pss:");
 memline("large_hold",24,req,br,bp,hr,hp,ar,ap,hwm,ck);}
static int dist(void){
 size_t i;uint64_t ck=0;
 for(i=0;i<5000;i++){uint64_t t0=now_ns(),t1=now_ns();abuf[i]=t1-t0;}
 report("timer_floor","pair",abuf,5000,0);
 (void)pat_inter(5000,0);(void)pat_burst(10,0);(void)pat_large(600,0);
 ck+=pat_inter(50000,1);ck+=pat_burst(100,1);ck+=pat_large(6000,1);
 footprint();
 printf("DONE,%s,%llu\n",LEGNAME,(unsigned long long)ck);return 0;}
static int energy(void){
 uint64_t t0=now_ns(),ops=0,ck=0;const uint64_t span=UINT64_C(6000000000);
 while(now_ns()-t0<span){void *p=A_ALLOC(64);if(!p)return 3;ck+=touch(p,64,(unsigned)ops);A_FREE(p);ops++;}
 printf("ENERGYRUN,%s,%llu,%llu,%llu\n",LEGNAME,(unsigned long long)ops,(unsigned long long)(now_ns()-t0),(unsigned long long)ck);return 0;}
int main(int argc,char **argv){
 if(argc>1&&!strcmp(argv[1],"energy"))return energy();
 return dist();}
