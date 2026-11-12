/* fuzz_m004.c - M-004 Phase 13 fuzzing harness (roadmap S10).
 * Seeded pseudo-random sequences of alloc, write/verify,
 * realloc, calloc, cross-thread transfer/free, trim and
 * thread exit, checked against a simple independent
 * ownership model after every batch of 16 operations.
 * Observables compared (defined behaviour only): pattern
 * integrity, zero-fill, live-count via the test-visible
 * stats route (J-002 shape b), overlap of requested
 * extents, success/failure shape. Never pointer identity
 * or timing. Same seed -> same sequence. Bounded: fixed
 * op counts per seed; see docs/PHASE_13_FUZZING_M004.md. */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "mimir_alloc.h"
#include "../core/mimir_stats_j002.h"
#include "../core/mimir_trim_i003.h"

#define MAXLIVE 128
#define BATCH 16
static uint64_t g_checks=0, g_fails=0;
static uint64_t g_seed=0; static long g_op=-1;
static void fail(const char *what, const char *detail){
  g_fails++;
  printf("FINDING seed=%llu op=%ld %s %s\n",
    (unsigned long long)g_seed,(long)g_op,what,detail);
}
#define CHECK(cond,what,detail) do{ g_checks++; \
  if(!(cond)) fail(what,detail); }while(0)

typedef struct{ uint64_t s; } rng_t;
static uint64_t rng_next(rng_t *r){ uint64_t x=r->s;
  x^=x>>12; x^=x<<25; x^=x>>27; r->s=x;
  return x*0x2545F4914F6CDD1DULL; }
static size_t rng_below(rng_t *r,size_t n){
  return (size_t)(rng_next(r)%n); }

typedef struct{ void *p; size_t sz; uint8_t pat; int owner;
  int live; uint64_t serial; } blk_t;
static blk_t g_model[MAXLIVE];
static uint64_t g_serial=0;
static int model_live_count(void){ int n=0,i;
  for(i=0;i<MAXLIVE;i++) if(g_model[i].live) n++;
  return n; }
static int model_free_slot(void){ int i;
  for(i=0;i<MAXLIVE;i++) if(!g_model[i].live) return i;
  return -1; }
static int model_pick_live(rng_t *r){ int n=model_live_count(),k,i;
  if(!n) return -1;
  k=(int)rng_below(r,(size_t)n);
  for(i=0;i<MAXLIVE;i++){ if(g_model[i].live && k--==0) return i; }
  return -1; }
static void fill_blk(blk_t *b){ memset(b->p,b->pat,b->sz); }
static int verify_blk(blk_t *b){ size_t i; const unsigned char *c=b->p;
  for(i=0;i<b->sz;i++) if(c[i]!=b->pat) return 0;
  return 1; }
static int cmp_ptr(const void *a,const void *b){
  uintptr_t x=(uintptr_t)*(void *const *)a;
  uintptr_t y=(uintptr_t)*(void *const *)b;
  return (x>y)-(x<y); }
/* Full batch check: patterns, overlap, stats live count. */
static void batch_check(void){
  int i,n=model_live_count(); void *ptrs[MAXLIVE]; size_t szs[MAXLIVE];
  int m=0; struct mimir_stats st = { 0 }; uint64_t alloc_live;
  for(i=0;i<MAXLIVE;i++) if(g_model[i].live){
    CHECK(verify_blk(&g_model[i]),"pattern","live block corrupt");
    ptrs[m]=g_model[i].p; szs[m]=g_model[i].sz; m++; }
  qsort(ptrs,m,sizeof(void*),cmp_ptr);
  for(i=1;i<m;i++){ uintptr_t prev=(uintptr_t)ptrs[i-1];
    /* find size of prev */ size_t psz=0; int j;
    for(j=0;j<MAXLIVE;j++) if(g_model[j].live&&g_model[j].p==ptrs[i-1]) psz=g_model[j].sz;
    (void)szs; CHECK(prev+psz<=(uintptr_t)ptrs[i],"overlap","live extents overlap"); }
  CHECK(mimir_core_stats_snapshot(&st)==0,"stats","snapshot failed");
  alloc_live=st.live_small_objects+st.live_mappings;
  CHECK(alloc_live==(uint64_t)n,"live-count","model != allocator live count");
}
static const size_t SIZES[]={1,8,16,24,32,48,64,96,128,256,512,
  1024,2048,4096,8192,16384,32768,65536};
#define NSIZES (sizeof(SIZES)/sizeof(SIZES[0]))
static uint64_t op_counts[10];
/* op classes: 0 alloc 1 write 2 realloc 3 calloc 4 free
 * 5 trim 6 transfer-free 7 handoff-exit 8 verify 9 free-null */
static void do_alloc(rng_t *r,int owner){
  int s=model_free_slot(); size_t sz; blk_t *b; void *p;
  op_counts[0]++; if(s<0) return;
  sz=SIZES[rng_below(r,NSIZES)]; p=mimir_malloc(sz);
  CHECK(p!=NULL,"alloc","unexpected NULL for lawful size");
  if(!p) return;
  b=&g_model[s]; b->p=p; b->sz=sz;
  b->pat=(uint8_t)(0x3Cu^(uint8_t)(++g_serial)); b->owner=owner;
  b->live=1; b->serial=g_serial; fill_blk(b); }
static void do_write(rng_t *r){ int s=model_pick_live(r); blk_t *b;
  op_counts[1]++; if(s<0){ do_alloc(r,0); return; } b=&g_model[s];
  CHECK(verify_blk(b),"write","pattern corrupt before rewrite");
  b->pat=(uint8_t)(b->pat+0x11u); fill_blk(b); }
static void do_free_idx(int s){ blk_t *b=&g_model[s];
  CHECK(verify_blk(b),"free","pattern corrupt before free");
  mimir_free(b->p); b->live=0; b->p=NULL; }
static void do_realloc(rng_t *r){ int s=model_pick_live(r); blk_t *b;
  size_t oldsz,newsz,minsz; void *np; unsigned kind;
  op_counts[2]++; if(s<0){ do_alloc(r,0); return; } b=&g_model[s];
  oldsz=b->sz; kind=(unsigned)rng_below(r,10);
  if(kind==0){ /* failure-shaped: huge size, expect NULL */
    np=mimir_realloc(b->p,(size_t)-1);
    CHECK(np==NULL,"realloc-fail","huge realloc not NULL");
    CHECK(verify_blk(b),"realloc-fail","original damaged"); return; }
  newsz=SIZES[rng_below(r,NSIZES)];
  np=mimir_realloc(b->p,newsz);
  CHECK(np!=NULL,"realloc","unexpected NULL for lawful size");
  if(!np) return;
  minsz=oldsz<newsz?oldsz:newsz;
  { size_t i; const unsigned char *c=np; int ok=1;
    for(i=0;i<minsz;i++) if(c[i]!=b->pat) ok=0;
    CHECK(ok,"realloc","preserved prefix corrupt"); }
  b->p=np; b->sz=newsz; fill_blk(b); }
static void do_calloc(rng_t *r){ int s=model_free_slot();
  size_t nm,sz,i; blk_t *b; unsigned char *p; int ok=1;
  op_counts[3]++; if(rng_below(r,10)==0){
    p=mimir_calloc(((size_t)-1)/2+1,2);
    CHECK(p==NULL,"calloc-overflow","overflow not NULL"); return; }
  if(s<0) return;
  nm=1+rng_below(r,16); sz=1+rng_below(r,64);
  p=mimir_calloc(nm,sz);
  CHECK(p!=NULL,"calloc","unexpected NULL"); if(!p) return;
  for(i=0;i<nm*sz;i++) if(p[i]!=0) ok=0;
  CHECK(ok,"calloc","not zero-filled");
  b=&g_model[s]; b->p=p; b->sz=nm*sz;
  b->pat=(uint8_t)(0xC3u^(uint8_t)(++g_serial)); b->owner=0;
  b->live=1; b->serial=g_serial; fill_blk(b); }
typedef struct{ void *p; uint8_t pat; size_t sz; int ok; } xfer_t;
static void *xfer_free_thread(void *arg){ xfer_t *x=arg;
  const unsigned char *c=x->p; size_t i; x->ok=1;
  for(i=0;i<x->sz;i++) if(c[i]!=x->pat) x->ok=0;
  mimir_free(x->p); return NULL; }
static void do_transfer_free(rng_t *r){ int s=model_pick_live(r);
  blk_t *b; xfer_t x; pthread_t th;
  op_counts[6]++; if(s<0){ do_alloc(r,0); return; } b=&g_model[s];
  x.p=b->p; x.pat=b->pat; x.sz=b->sz; x.ok=0;
  if(pthread_create(&th,NULL,xfer_free_thread,&x)!=0){
    CHECK(0,"transfer","pthread_create failed"); return; }
  pthread_join(th,NULL);
  CHECK(x.ok,"transfer","pattern corrupt in other thread");
  b->live=0; b->p=NULL; }
typedef struct{ void *p[2]; size_t sz[2]; uint8_t pat[2]; } hand_t;
static void *handoff_thread(void *arg){ hand_t *h=arg; int i;
  for(i=0;i<2;i++){ h->sz[i]=64+(size_t)i*32;
    h->p[i]=mimir_malloc(h->sz[i]);
    h->pat[i]=(uint8_t)(0x5Au+i);
    if(h->p[i]) memset(h->p[i],h->pat[i],h->sz[i]); }
  return NULL; /* voluntary exit with live blocks: H-005 posture */ }
static void do_handoff(void){ hand_t h; pthread_t th; int i;
  op_counts[7]++; memset(&h,0,sizeof h);
  if(pthread_create(&th,NULL,handoff_thread,&h)!=0){
    CHECK(0,"handoff","pthread_create failed"); return; }
  pthread_join(th,NULL);
  for(i=0;i<2;i++){ int s=model_free_slot(); blk_t *b;
    CHECK(h.p[i]!=NULL,"handoff","helper alloc NULL");
    if(!h.p[i]||s<0){ if(h.p[i]) mimir_free(h.p[i]); continue; }
    b=&g_model[s]; b->p=h.p[i]; b->sz=h.sz[i]; b->pat=h.pat[i];
    b->owner=99; b->live=1; b->serial=++g_serial;
    CHECK(verify_blk(b),"handoff","handed-off block corrupt"); } }
static long run_seed(uint64_t seed,long ops){
  rng_t r; long i; uint64_t fails0=g_fails;
  memset(g_model,0,sizeof g_model); memset(op_counts,0,sizeof op_counts);
  r.s=seed?seed:1; g_seed=seed;
  for(i=0;i<ops;i++){ unsigned cls; g_op=i;
    cls=(unsigned)rng_below(&r,100);
    if(cls<28) do_alloc(&r,0);
    else if(cls<42) do_write(&r);
    else if(cls<58) do_realloc(&r);
    else if(cls<68) do_calloc(&r);
    else if(cls<83){ int s=model_pick_live(&r); op_counts[4]++;
      if(s<0) do_alloc(&r,0); else do_free_idx(s); }
    else if(cls<87){ op_counts[5]++; (void)mimir_core_trim_retained();
      batch_check(); }
    else if(cls<93) do_transfer_free(&r);
    else if(cls<96) do_handoff();
    else if(cls<98){ op_counts[8]++; batch_check(); }
    else { op_counts[9]++; mimir_free(NULL); g_checks++; }
    if(i%BATCH==BATCH-1) batch_check();
    if(g_fails>fails0) break; }
  /* final drain: free everything the model holds */
  { int s; for(s=0;s<MAXLIVE;s++) if(g_model[s].live) do_free_idx(s); }
  (void)mimir_core_trim_retained(); batch_check();
  CHECK(model_live_count()==0,"final","model not empty");
  printf("SEED %llu ops=%ld alloc=%llu write=%llu realloc=%llu calloc=%llu free=%llu trim=%llu xfer=%llu handoff=%llu verify=%llu freenull=%llu checks=%llu fails=%llu live_end=%d\n",
    (unsigned long long)seed,(long)(g_op+1),
    (unsigned long long)op_counts[0],(unsigned long long)op_counts[1],
    (unsigned long long)op_counts[2],(unsigned long long)op_counts[3],
    (unsigned long long)op_counts[4],(unsigned long long)op_counts[5],
    (unsigned long long)op_counts[6],(unsigned long long)op_counts[7],
    (unsigned long long)op_counts[8],(unsigned long long)op_counts[9],
    (unsigned long long)g_checks,(unsigned long long)g_fails,
    model_live_count());
  return g_fails>fails0?1:0; }
/* --- Bounded multithread seed: 3 threads, barrier phases.
 * Phase1 own seeded ops; Phase2 transfer half the live
 * blocks to the next thread inbox (single writer per
 * inbox); Phase3 adopt, verify, free every 3rd adopted
 * (free by non-owner); Phase4 voluntary exit with the
 * rest handed to main. No cancellation, no fork. --- */
#define MT_THREADS 3
#define MT_SLOTS 32
typedef struct{ void *p; size_t sz; uint8_t pat; int live; } mt_blk_t;
static mt_blk_t mt_own[MT_THREADS][MT_SLOTS];
static mt_blk_t mt_inbox[MT_THREADS][MT_SLOTS];
static int mt_inbox_n[MT_THREADS];
static mt_blk_t mt_hand[MT_THREADS][MT_SLOTS];
static int mt_hand_n[MT_THREADS];
static uint64_t mt_counts[MT_THREADS][5]; /* a w r f xfer */
static pthread_barrier_t mt_bar;
static int mt_verify(mt_blk_t *b){ size_t i; const unsigned char *c=b->p;
  for(i=0;i<b->sz;i++) if(c[i]!=b->pat) return 0;
  return 1; }
typedef struct{ int id; uint64_t seed; } mt_arg_t;
static void *mt_thread(void *arg){ mt_arg_t *a=arg; int id=a->id;
  rng_t r; int i; r.s=a->seed+(uint64_t)id*7919u;
  for(i=0;i<300;i++){ unsigned cls=(unsigned)rng_below(&r,100);
    int live_n=0,j,pick=-1; 
    for(j=0;j<MT_SLOTS;j++) if(mt_own[id][j].live) live_n++;
    if(cls<45||live_n==0){ int s=-1; size_t sz; void *p;
      for(j=0;j<MT_SLOTS;j++) if(!mt_own[id][j].live){ s=j; break; }
      mt_counts[id][0]++; if(s<0) continue;
      sz=SIZES[rng_below(&r,10)]; p=mimir_malloc(sz);
      if(!p){ g_fails++; continue; }
      mt_own[id][s].p=p; mt_own[id][s].sz=sz;
      mt_own[id][s].pat=(uint8_t)(0x30u+id*16u+(i&7));
      mt_own[id][s].live=1; memset(p,mt_own[id][s].pat,sz); }
    else if(cls<70){ int k=(int)rng_below(&r,(size_t)live_n);
      for(j=0;j<MT_SLOTS;j++) if(mt_own[id][j].live&&k--==0) pick=j;
      mt_counts[id][1]++;
      if(pick>=0){ if(!mt_verify(&mt_own[id][pick])) g_fails++;
        mt_own[id][pick].pat=(uint8_t)(mt_own[id][pick].pat+1);
        memset(mt_own[id][pick].p,mt_own[id][pick].pat,mt_own[id][pick].sz); } }
    else { int k=(int)rng_below(&r,(size_t)live_n);
      for(j=0;j<MT_SLOTS;j++) if(mt_own[id][j].live&&k--==0) pick=j;
      mt_counts[id][3]++;
      if(pick>=0){ if(!mt_verify(&mt_own[id][pick])) g_fails++;
        mimir_free(mt_own[id][pick].p); mt_own[id][pick].live=0; } } }
  pthread_barrier_wait(&mt_bar);
  { int nxt=(id+1)%MT_THREADS,n=0,j;
    for(j=0;j<MT_SLOTS;j++) if(mt_own[id][j].live&&(j%2==0)){
      mt_inbox[nxt][n++]=mt_own[id][j]; mt_own[id][j].live=0;
      mt_counts[id][4]++; }
    mt_inbox_n[nxt]=n; }
  pthread_barrier_wait(&mt_bar);
  { int n=0,j; for(j=0;j<mt_inbox_n[id];j++){
      if(!mt_verify(&mt_inbox[id][j])) g_fails++;
      if(j%3==0){ mimir_free(mt_inbox[id][j].p); mt_counts[id][3]++; }
      else { int s=-1,k;
        for(k=0;k<MT_SLOTS;k++) if(!mt_own[id][k].live){ s=k; break; }
        if(s>=0) mt_own[id][s]=mt_inbox[id][j];
        else { mimir_free(mt_inbox[id][j].p); mt_counts[id][3]++; } } }
    (void)n; }
  pthread_barrier_wait(&mt_bar);
  { int n=0,j;
    for(j=0;j<MT_SLOTS;j++) if(mt_own[id][j].live) mt_hand[id][n++]=mt_own[id][j];
    mt_hand_n[id]=n; }
  return NULL; }
static long run_mt_seed(uint64_t seed){
  pthread_t th[MT_THREADS]; mt_arg_t args[MT_THREADS]; int i,j;
  uint64_t fails0=g_fails,ta=0,tw=0,tr=0,tf=0,tx=0; int handed=0;
  memset(mt_own,0,sizeof mt_own); memset(mt_inbox,0,sizeof mt_inbox);
  memset(mt_hand,0,sizeof mt_hand); memset(mt_counts,0,sizeof mt_counts);
  memset(mt_inbox_n,0,sizeof mt_inbox_n); memset(mt_hand_n,0,sizeof mt_hand_n);
  memset(g_model,0,sizeof g_model); g_seed=seed; g_op=-1;
  pthread_barrier_init(&mt_bar,NULL,MT_THREADS);
  for(i=0;i<MT_THREADS;i++){ args[i].id=i; args[i].seed=seed;
    if(pthread_create(&th[i],NULL,mt_thread,&args[i])!=0){
      CHECK(0,"mt","pthread_create failed"); return 1; } }
  for(i=0;i<MT_THREADS;i++) pthread_join(th[i],NULL);
  pthread_barrier_destroy(&mt_bar);
  for(i=0;i<MT_THREADS;i++){ ta+=mt_counts[i][0]; tw+=mt_counts[i][1];
    tr+=mt_counts[i][2]; tf+=mt_counts[i][3]; tx+=mt_counts[i][4];
    for(j=0;j<mt_hand_n[i];j++){ int s=model_free_slot(); blk_t *b;
      handed++; if(s<0){ mimir_free(mt_hand[i][j].p); continue; }
      b=&g_model[s]; b->p=mt_hand[i][j].p; b->sz=mt_hand[i][j].sz;
      b->pat=mt_hand[i][j].pat; b->owner=i; b->live=1; b->serial=++g_serial; } }
  g_checks++; if(g_fails>fails0) fail("mt","thread phase failures");
  batch_check(); /* patterns + overlap + stats == handed model */
  for(i=0;i<MAXLIVE;i++) if(g_model[i].live) do_free_idx(i);
  (void)mimir_core_trim_retained(); batch_check();
  printf("SEED %llu MT threads=%d alloc=%llu write=%llu realloc=0 free=%llu xfer=%llu handed=%d checks=%llu fails=%llu live_end=%d\n",
    (unsigned long long)seed,MT_THREADS,(unsigned long long)ta,
    (unsigned long long)tw,(unsigned long long)tf,
    (unsigned long long)tx,handed,(unsigned long long)g_checks,
    (unsigned long long)g_fails,model_live_count());
  return g_fails>fails0?1:0; }
int main(int argc,char **argv){
  long rc=0;
  printf("FUZZ-M004 harness v1 (trace version 1) bounded\n");
  if(argc>=3){ g_checks=0; g_fails=0;
    if(strcmp(argv[1],"mt")==0) rc=run_mt_seed(strtoull(argv[2],NULL,0));
    else rc=run_seed(strtoull(argv[1],NULL,0),atol(argv[2]));
    printf("RESULT: %s checks=%llu fails=%llu\n",rc?"FAIL":"PASS",
      (unsigned long long)g_checks,(unsigned long long)g_fails);
    return (int)rc; }
  { uint64_t seeds[3]={4001,4002,4003}; long ops[3]={2000,2000,2000}; int i;
    for(i=0;i<3;i++){ g_checks=0; g_fails=0;
      rc|=run_seed(seeds[i],ops[i]);
      printf("SEED-RESULT %llu %s\n",(unsigned long long)seeds[i],
        g_fails?"FAIL":"PASS"); }
    g_checks=0; g_fails=0; rc|=run_mt_seed(4004);
    printf("SEED-RESULT 4004 %s\n",g_fails?"FAIL":"PASS"); }
  printf("RESULT: %s\n",rc?"FAIL":"PASS");
  return (int)rc; }
