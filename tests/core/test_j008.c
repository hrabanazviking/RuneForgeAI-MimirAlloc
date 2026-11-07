/* test_j008.c - J-008 evidence: hardened-flavour functional
 * sanity + integer-overflow refusal proofs (audit sites not
 * covered by earlier waves; re-verifications labelled).
 * Built ONLY against the hardened objects (build-hardened/)
 * via `make test-j008`; not part of `make test`. */
#define _POSIX_C_SOURCE 200809L
#include "mimir_alloc.h"
#include "mimir_stats_j002.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static int failures=0, checks=0;
static void check(int c,const char*l){checks++;if(c){printf("PASS %s\n",l);}else{failures++;printf("FAIL %s\n",l);}}
int main(void){
  long ps=sysconf(_SC_PAGESIZE); size_t page=(size_t)ps;
  printf("page_size=%zu\n",page);
  /* Leg A: functional sanity, hardened flavour */
  void *a=mimir_malloc(64); check(a!=NULL,"A1 malloc(64)");
  if(a){memset(a,0xAB,64); check(((unsigned char*)a)[63]==0xAB,"A2 small write/read");}
  void *b=mimir_malloc(8192); check(b!=NULL,"A3 malloc(8192) large");
  if(b){memset(b,0xCD,8192); check(((unsigned char*)b)[8191]==0xCD,"A4 large write/read");}
  void *c=mimir_calloc(16,32); check(c!=NULL,"A5 calloc(16,32)");
  if(c){int z=1; for(int i=0;i<512;i++) if(((unsigned char*)c)[i]) z=0; check(z,"A6 calloc zeroed");}
  void *d=mimir_malloc(128); if(d) memset(d,0x11,128);
  void *d2=mimir_realloc(d,4096); check(d2!=NULL,"A7 realloc grow");
  if(d2){int ok=1; for(int i=0;i<128;i++) if(((unsigned char*)d2)[i]!=0x11) ok=0; check(ok,"A8 grow preserves bytes");}
  mimir_free(a); mimir_free(b); mimir_free(c); mimir_free(d2);
  check(1,"A9 frees return");
  /* Leg B: overflow refusals. NULL proves no wrap: a wrapped
   * size would have been admitted and mapped small. */
  check(mimir_malloc(SIZE_MAX)==NULL,"B1 malloc(SIZE_MAX) NULL (re-verifies banked)");
  check(mimir_malloc(SIZE_MAX-page+1)==NULL,"B2 malloc(SIZE_MAX-page+1) NULL - A3 gate, NEW");
  check(mimir_malloc(SIZE_MAX-page)==NULL,"B3 malloc(SIZE_MAX-page) NULL - admissible arith, unmappable, NEW");
  check(mimir_calloc(SIZE_MAX/2+1,2)==NULL,"B4 calloc product overflow (re-verifies C-005)");
  check(mimir_calloc(2,SIZE_MAX/2+1)==NULL,"B5 calloc product overflow swapped, NEW");
  check(mimir_calloc(SIZE_MAX,1)==NULL,"B6 calloc(SIZE_MAX,1): product==SIZE_MAX passes product gate, A3 refuses, NEW");
  check(mimir_calloc(1,SIZE_MAX)==NULL,"B7 calloc(1,SIZE_MAX) NULL, NEW");
  void *p=mimir_malloc(2048); check(p!=NULL,"B8 malloc(2048) pre-realloc");
  if(p){memset(p,0x5A,2048);
    check(mimir_realloc(p,SIZE_MAX)==NULL,"B9 realloc grow to SIZE_MAX NULL, NEW");
    int ok=1; for(int i=0;i<2048;i++) if(((unsigned char*)p)[i]!=0x5A) ok=0;
    check(ok,"B10 original preserved after refused grow, NEW");
    mimir_free(p); check(1,"B11 original freeable after refused grow, NEW");}
  void *q=mimir_malloc(64); check(q!=NULL,"B12 allocator healthy after refusal storm");
  mimir_free(q);
  struct mimir_stats st; int rc=mimir_core_stats_snapshot(&st);
  check(rc==0,"B13 stats snapshot on hardened build");
  if(rc==0){check(st.live_mappings==0,"B14 no live mappings at quiescence");
    check(st.live_small_objects==0,"B15 no live small objects at quiescence");
    check(st.alloc_failures>0,"B16 genuine failures counted");}
  printf("RESULT: %s (%d checks, %d failures)\n", failures?"FAIL":"PASS",checks,failures);
  return failures?1:0;
}
