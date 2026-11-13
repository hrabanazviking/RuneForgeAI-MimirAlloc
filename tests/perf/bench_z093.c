/* bench_z093.c - Z-093 batch verdict run harness.
 * Roadmap slice Z-093 (Wave 11): prototype vs per-op loop
 * vs glibc on batch-shaped workloads.
 *
 * Original run (commit ab2aa5f): the Z-092 batch API
 * prototype was NOT present in that tree (verified by the
 * runner's prototype probe: no batch symbol in the public
 * header or in the built .so exports), so only the two
 * legs that existed were measured - mimir per-op loop and
 * glibc per-op loop - and the prototype leg was recorded
 * OWED (verdict doc S1/S5).
 *
 * Fold-in (this version): Z-092 is banked at seat main
 * 4188da8 and merged into this tree. The batch symbols
 * are internal (src/core/mimir_core.c, declared in
 * src/core/mimir_core.h and in the test-only contract
 * header tests/contract/mimir_batch_z092.h); they are
 * NOT public exports (nm -D on libmimiralloc.so still
 * shows zero batch symbols - the runner probe records
 * both facts). Three legs now exist:
 *   mimir per-op loop  (banked LIBOBJS, -O2, public
 *                       mimir_malloc / mimir_free)
 *   glibc per-op loop  (-DLEG_GLIBC, stdlib malloc/free)
 *   proto batch leg    (-DLEG_PROTO, Z-092 internal
 *                       mimir_batch_alloc / mimir_batch_free
 *                       over the same LIBOBJS)
 * driven over the IDENTICAL batch-shaped workloads:
 * every shape, total and checksum below is unchanged
 * from the original run, so the prototype numbers grade
 * directly against the filed per-op numbers.
 *
 * Prototype leg construction (honest to the Z-092
 * contract, docs/Z13_Z092_BATCH_API_PROTOTYPE.md S2):
 * - same-class shapes: one mimir_batch_alloc call per
 *   batch window, touch in sequence order, one
 *   mimir_batch_free call per window. batch_free frees
 *   in REVERSE (LIFO) order by contract, matching the
 *   LIFO shapes natively; the FIFO shape is driven by
 *   handing batch_free a reversed pointer array, so the
 *   callee's reverse walk frees in FIFO order - the
 *   batch API itself has no order parameter.
 * - mixbatch512: the batch API is same-class only, so
 *   each 512 window is grouped by class (six
 *   mimir_batch_alloc calls, one per class present),
 *   touched in the original sequence order (checksum
 *   identical), and freed grouped by class (LIFO within
 *   each class group). Free ORDER across classes differs
 *   from the per-op LIFO walk; Z-091 found free order is
 *   not a priced factor on this machine, and the
 *   grouping is the only contract-legal way to drive a
 *   mixed batch through this API - recorded here, not
 *   hidden.
 * - The Z-092 prototype loops the per-op core entries
 *   and removes no walk (its own doc S2 says so); this
 *   leg measures exactly that shipped prototype, not
 *   the span-grouped design Z-091 briefed as future work.
 *
 * Shapes (K-007 geometry, unmodified totals): interleaved
 * alloc/touch/free (batch 1) vs burst alloc-all/touch-all/
 * free-all at batch 8/32/128/512 (LIFO free), plus FIFO at
 * batch 512, at 64 B and 1024 B; plus one mixed-class batch
 * shape (mixbatch512: batch of 512 cycling sizes
 * 32/64/128/256/512/1024, LIFO free) - the batch-shaped
 * workload a real bulk caller actually holds. One leg per
 * process (argv key name:size; mix uses size key 0) so
 * ru_maxrss/minflt are per-leg clean; reps are fresh
 * processes driven by run_bench_z093.sh; warm-up 1/10
 * untimed (K-001 M-6). Whole-pass CLOCK_MONOTONIC.
 * Checksum depends only on the global op sequence, so all
 * patterns at one size must print identical ck on all
 * three legs. */
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
#elif defined(LEG_PROTO)
#include "../contract/mimir_batch_z092.h"
#define LEGNAME "proto"
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
static const size_t MIXSIZES[6]={32,64,128,256,512,1024};
#ifndef LEG_PROTO
static int run_pass(size_t sz,size_t batch,size_t total,int fifo,uint64_t *ck){
 size_t done=0;uint64_t seq=0;
 while(done<total){
  size_t b=batch,i;if(b>total-done)b=total-done;
  for(i=0;i<b;i++){void *p=A_ALLOC(sz);if(!p)return 3;memset(p,(int)(seq&0x3F)+1,sz);*ck+=((unsigned char*)p)[0];seq++;ptrs[i]=p;}
  if(fifo){for(i=0;i<b;i++)A_FREE(ptrs[i]);}
  else{for(i=b;i>0;i--)A_FREE(ptrs[i-1]);}
  done+=b;}
 return 0;}
static int run_pass_mix(size_t batch,size_t total,uint64_t *ck){
 size_t done=0;uint64_t seq=0;
 while(done<total){
  size_t b=batch,i;if(b>total-done)b=total-done;
  for(i=0;i<b;i++){size_t sz=MIXSIZES[seq%6];void *p=A_ALLOC(sz);if(!p)return 3;memset(p,(int)(seq&0x3F)+1,sz);*ck+=((unsigned char*)p)[0];seq++;ptrs[i]=p;}
  for(i=b;i>0;i--)A_FREE(ptrs[i-1]);
  done+=b;}
 return 0;}
#else /* LEG_PROTO: Z-092 batch API leg */
static void *rev[512];
static int run_pass(size_t sz,size_t batch,size_t total,int fifo,uint64_t *ck){
 size_t done=0;uint64_t seq=0;
 while(done<total){
  size_t b=batch,i;if(b>total-done)b=total-done;
  if(mimir_batch_alloc(sz,ptrs,b)!=b)return 3;
  for(i=0;i<b;i++){memset(ptrs[i],(int)(seq&0x3F)+1,sz);*ck+=((unsigned char*)ptrs[i])[0];seq++;}
  if(fifo){for(i=0;i<b;i++)rev[i]=ptrs[b-1-i];if(mimir_batch_free(rev,b)!=b)return 4;}
  else{if(mimir_batch_free(ptrs,b)!=b)return 4;}
  done+=b;}
 return 0;}
static int run_pass_mix(size_t batch,size_t total,uint64_t *ck){
 size_t done=0;uint64_t seq=0;
 void *tmp[512];size_t idx[512];
 while(done<total){
  size_t b=batch,i,c;uint64_t base=seq;if(b>total-done)b=total-done;
  for(c=0;c<6;c++){
   size_t n=0,k;
   for(i=0;i<b;i++)if(MIXSIZES[(base+i)%6]==MIXSIZES[c])idx[n++]=i;
   if(n==0)continue;
   if(mimir_batch_alloc(MIXSIZES[c],tmp,n)!=n)return 3;
   for(k=0;k<n;k++)ptrs[idx[k]]=tmp[k];}
  for(i=0;i<b;i++){size_t sz=MIXSIZES[(base+i)%6];memset(ptrs[i],(int)((base+i)&0x3F)+1,sz);*ck+=((unsigned char*)ptrs[i])[0];}
  for(c=0;c<6;c++){
   size_t n=0;
   for(i=0;i<b;i++)if(MIXSIZES[(base+i)%6]==MIXSIZES[c])tmp[n++]=ptrs[i];
   if(n==0)continue;
   if(mimir_batch_free(tmp,n)!=n)return 4;}
  seq+=b;done+=b;}
 return 0;}
#endif /* LEG_PROTO */
static int leg(const char *name,size_t sz,size_t batch,size_t total,int fifo){
 uint64_t ck=0,t0,t1;long f0,f1,r0,r1;int rc;
 f0=minflt();r0=maxrss();
 rc=run_pass(sz,batch,total/10,fifo,&ck);if(rc)return rc; /* warm-up */
 ck=0;t0=now_ns();
 rc=run_pass(sz,batch,total,fifo,&ck);if(rc)return rc;
 t1=now_ns();f1=minflt();r1=maxrss();
 printf("%s,%s,%zu,%zu,%zu,%llu,%llu,%llu,%ld,%ld\n",LEGNAME,name,batch,sz,total,(unsigned long long)(t1-t0),(unsigned long long)((t1-t0)/total),(unsigned long long)ck,f1-f0,r1-r0);
 return 0;}
static int leg_mix(const char *name,size_t batch,size_t total){
 uint64_t ck=0,t0,t1;long f0,f1,r0,r1;int rc;
 f0=minflt();r0=maxrss();
 rc=run_pass_mix(batch,total/10,&ck);if(rc)return rc; /* warm-up */
 ck=0;t0=now_ns();
 rc=run_pass_mix(batch,total,&ck);if(rc)return rc;
 t1=now_ns();f1=minflt();r1=maxrss();
 printf("%s,%s,%zu,%zu,%zu,%llu,%llu,%llu,%ld,%ld\n",LEGNAME,name,batch,(size_t)0,total,(unsigned long long)(t1-t0),(unsigned long long)((t1-t0)/total),(unsigned long long)ck,f1-f0,r1-r0);
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
 if(argc<=1||!strcmp(argv[1],"mixbatch512:0")){
  int r=leg_mix("mixbatch512",512,30720);if(r)rc=r;}
 return rc;}
