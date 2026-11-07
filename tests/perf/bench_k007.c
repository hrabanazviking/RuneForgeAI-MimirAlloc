/* bench_k007.c - K-007 batching experiments.
 * One harness built twice (bench_k002 precedent): glibc leg
 * (-DLEG_GLIBC, stdlib malloc/free) and MimirAlloc leg
 * (banked LIBOBJS, shipped -O2 flavour per K-004; harness
 * itself -O2). Patterns at matched totals: interleaved
 * alloc/touch/free (batch 1) vs burst alloc-all/touch-all/
 * free-all at batch 8/32/128/512 (LIFO free), plus a FIFO
 * free-order variant at batch 512. One leg per process
 * (argv key name:size) so ru_maxrss/minflt are per-leg
 * clean; reps are fresh processes driven by
 * run_bench_k007.sh; warm-up 1/10 untimed (K-001 M-6).
 * Whole-pass CLOCK_MONOTONIC. Checksum depends only on
 * the global op sequence, so all patterns at one size
 * must print identical ck. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#ifdef LEG_GLIBC
#define A_ALLOC malloc
#define A_FREE free
#define LEGNAME "glibc"
#else
#include "mimir_alloc.h"
#define A_ALLOC mimir_malloc
#define A_FREE mimir_free
#define LEGNAME "mimir"
#endif
static uint64_t now_ns(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (uint64_t)ts.tv_sec*UINT64_C(1000000000)+(uint64_t)ts.tv_nsec;}
static long minflt(void){struct rusage r;getrusage(RUSAGE_SELF,&r);return r.ru_minflt;}
static long maxrss(void){struct rusage r;getrusage(RUSAGE_SELF,&r);return r.ru_maxrss;}
static void *ptrs[512];
static int run_pass(size_t sz,size_t batch,size_t total,int fifo,uint64_t *ck){
 size_t done=0;uint64_t seq=0;
 while(done<total){
  size_t b=batch,i;if(b>total-done)b=total-done;
  for(i=0;i<b;i++){void *p=A_ALLOC(sz);if(!p)return 3;memset(p,(int)(seq&0x3F)+1,sz);*ck+=((unsigned char*)p)[0];seq++;ptrs[i]=p;}
  if(fifo){for(i=0;i<b;i++)A_FREE(ptrs[i]);}
  else{for(i=b;i>0;i--)A_FREE(ptrs[i-1]);}
  done+=b;}
 return 0;}
static int leg(const char *name,size_t sz,size_t batch,size_t total,int fifo){
 uint64_t ck=0,t0,t1;long f0,f1,r0,r1;int rc;
 f0=minflt();r0=maxrss();
 rc=run_pass(sz,batch,total/10,fifo,&ck);if(rc)return rc; /* warm-up */
 ck=0;t0=now_ns();
 rc=run_pass(sz,batch,total,fifo,&ck);if(rc)return rc;
 t1=now_ns();f1=minflt();r1=maxrss();
 printf("%s,%s,%zu,%zu,%zu,%llu,%llu,%llu,%ld,%ld\n",LEGNAME,name,batch,sz,total,(unsigned long long)(t1-t0),(unsigned long long)((t1-t0)/total),(unsigned long long)ck,f1-f0,r1-r0);
 return 0;}
struct spec{const char *name;size_t sz,batch,total;int fifo;};
static const struct spec SPECS[]={
 {"inter1",64,1,120000,0},{"burst8",64,8,120000,0},
 {"burst32",64,32,120000,0},{"burst128",64,128,120000,0},
 {"burst512",64,512,120000,0},{"burst512fifo",64,512,120000,1},
 {"inter1",1024,1,30000,0},{"burst8",1024,8,30000,0},
 {"burst32",1024,32,30000,0},{"burst128",1024,128,30000,0},
 {"burst512",1024,512,30000,0},{"burst512fifo",1024,512,30000,1}};
int main(int argc,char **argv){
 size_t i;int rc=0;
 for(i=0;i<sizeof(SPECS)/sizeof(SPECS[0]);i++){
  char key[64];snprintf(key,sizeof key,"%s:%zu",SPECS[i].name,SPECS[i].sz);
  if(argc>1&&strcmp(argv[1],key))continue;
  {int r=leg(SPECS[i].name,SPECS[i].sz,SPECS[i].batch,SPECS[i].total,SPECS[i].fifo);if(r)rc=r;}}
 return rc;}
