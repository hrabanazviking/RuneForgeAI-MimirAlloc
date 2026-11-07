/*
 * test_i005.c -- evidence test for slice I-005 (Phase 9
 * constrained-memory runs; slice record:
 * docs/PHASE_9_CONSTRAINED_RUNS.md; exit item X5 of
 * docs/PHASE_9_FOUNDATION_PLAN.md, PARTIAL at base with
 * C-008 S93-2 as its prior small-scale form).
 *
 * Run standalone via `make test-i005` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-i003/test-i004 pattern).
 *
 * METHOD (stated before the run, per C-008 S9.3-2 child
 * form, strengthened): every leg runs in a forked child
 * that (1) measures its own current address-space size
 * (VmSize from /proc/self/status), (2) sets a per-process
 * RLIMIT_AS of exactly baseline + headroom, where the
 * headroom is stated in exact page arithmetic below, and
 * (3) exercises the allocator under that real OS limit.
 * The parent enforces a 30-second wall bound per child
 * (poll waitpid, SIGKILL on expiry): completion inside
 * the bound is the no-hang evidence. Per-process limit
 * ONLY -- no system-wide limit is ever touched.
 *
 * Page arithmetic (measured on this host, asserted in
 * Leg 0): page = 16384 B. A large allocation of 4096 B
 * costs 1 reserve page + 1 usable page = 2 pages =
 * 32768 B of VA (core descriptor layout, C-008). A small
 * span costs 1 page = 16384 B of VA (96 B header inside
 * the page). Small cutoff 1024 B; class sizes 16..1024.
 *
 * Headrooms: Leg A 64 pages, Leg B 64 pages, Leg C
 * 16 pages, Leg D 128 pages (largest level, trim leg),
 * Leg E 64 pages. Baselines differ per child because
 * each child inherits the parent image; calibrating from
 * the child's OWN measured baseline is what makes the
 * arithmetic exact rather than assumed.
 *
 * EVIDENCE CLASS: the failures here are REAL OS failures
 * (mmap refused by the kernel under RLIMIT_AS), a
 * different class from I-004's injected failures; the
 * two are never mixed (see the slice doc).
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include "mimir_alloc.h"
#include "mimir_trim_i003.h"
#include "mimir_tune_g007.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
static int failures = 0;
static int checks = 0;
static char outbuf[65536];
static void check(int c, const char *label){
  checks++; printf("  [%s] %s\n", c?"ok":"FAIL", label);
  if(!c) failures++;
}
static void fillp(unsigned char *p, size_t n, unsigned seed){
  for(size_t i=0;i<n;i++) p[i]=(unsigned char)(seed+(unsigned)i*7u);
}
static int patok(const unsigned char *p, size_t n, unsigned seed){
  for(size_t i=0;i<n;i++) if(p[i]!=(unsigned char)(seed+(unsigned)i*7u)) return 0;
  return 1;
}
static size_t vm_size_bytes(void){
  FILE *f=fopen("/proc/self/status","r"); if(!f) return 0;
  char line[256]; size_t kb=0;
  while(fgets(line,sizeof line,f)){
    if(sscanf(line,"VmSize: %zu kB",&kb)==1) break;
  }
  fclose(f); return kb*1024u;
}
static size_t page_sz(void){ long p=sysconf(_SC_PAGESIZE); return p>0?(size_t)p:0; }
/* Set RLIMIT_AS = own VmSize + headroom. Returns 0 ok, -1 fail. */
static int arm_limit(size_t headroom, size_t *base_out, size_t *lim_out){
  size_t base=vm_size_bytes(); if(base==0) return -1;
  struct rlimit rl; rl.rlim_cur=(rlim_t)(base+headroom);
  rl.rlim_max=(rlim_t)(base+headroom);
  if(setrlimit(RLIMIT_AS,&rl)!=0) return -1;
  *base_out=base; *lim_out=base+headroom; return 0;
}
/* Leg A: large-fill until clean NULL (headroom 64 pages). */
static void *legA_blocks[256];
static void leg_a(void){
  size_t pg=page_sz(), base=0, lim=0;
  if(arm_limit(64*pg,&base,&lim)!=0) _exit(2);
  printf("  baseline=%zu limit=%zu headroom_pages=64\n",base,lim);
  size_t n=0; void *p;
  while(n<256 && (p=mimir_malloc(4096))!=NULL){ legA_blocks[n]=p; fillp(p,4096,(unsigned)(0xA0+n)); n++; }
  printf("  large successes before NULL: %zu\n",n);
  check(n>=4,"A: at least 4 large blocks served under limit");
  check(n<=64,"A: successes bounded by headroom arithmetic (<=64)");
  check(n<256,"A: clean NULL reached (fill loop stopped on NULL)");
  int ov=1;
  for(size_t i=0;i<n&&ov;i++) for(size_t j=i+1;j<n;j++){
    unsigned char *a=legA_blocks[i],*b=legA_blocks[j];
    if(a<b+4096 && b<a+4096) ov=0;
  }
  check(ov,"A: live large blocks pairwise non-overlapping");
  int pat=1; for(size_t i=0;i<n;i++) if(!patok(legA_blocks[i],4096,(unsigned)(0xA0+i))) pat=0;
  check(pat,"A: all live block patterns intact at the limit");
  for(size_t i=0;i<n;i++) mimir_free(legA_blocks[i]);
  void *r=mimir_malloc(4096);
  check(r!=NULL,"A: allocator serves again after freeing all");
  if(r) mimir_free(r);
}
/* Leg B: smaller requests served where headroom remains. */
static void *legB_blocks[256];
static void leg_b(void){
  size_t pg=page_sz(), base=0, lim=0;
  if(arm_limit(64*pg,&base,&lim)!=0) _exit(2);
  printf("  baseline=%zu limit=%zu headroom_pages=64\n",base,lim);
  size_t n=0; void *p;
  while(n<256 && (p=mimir_malloc(4096))!=NULL){ legB_blocks[n]=p; n++; }
  check(n>=4 && n<256,"B: filled to clean NULL first");
  /* Free TWO large blocks = 4 pages of headroom: the large
   * re-request costs 2 pages and the small span 1 page, so
   * both fit with 1 page to spare (exact arithmetic; an
   * earlier draft freed one block and the small span then
   * starved the large request -- INC-I005-02). */
  mimir_free(legB_blocks[n-1]); legB_blocks[n-1]=NULL;
  mimir_free(legB_blocks[n-2]); legB_blocks[n-2]=NULL;
  void *l=mimir_malloc(4096);
  check(l!=NULL,"B: large request served again in freed headroom");
  void *s=mimir_malloc(64);
  check(s!=NULL,"B: small 64 B request served in remaining headroom");
  if(s) mimir_free(s);
  if(l) mimir_free(l);
  for(size_t i=0;i+2<n;i++) mimir_free(legB_blocks[i]);
}
/* Leg C: realloc failure leaves original byte-identical. */
static void leg_c(void){
  size_t pg=page_sz(), base=0, lim=0;
  if(arm_limit(16*pg,&base,&lim)!=0) _exit(2);
  printf("  baseline=%zu limit=%zu headroom_pages=16\n",base,lim);
  unsigned char *p=mimir_malloc(4096);
  check(p!=NULL,"C: original block allocated under limit");
  if(!p) return;
  fillp(p,4096,0xC0);
  void *q=mimir_realloc(p,8u*1024u*1024u); /* 8 MiB >> 16-page headroom */
  check(q==NULL,"C: oversize realloc under limit returns exactly NULL");
  check(patok(p,4096,0xC0),"C: original byte-identical after failed realloc");
  mimir_free(p);
  void *r=mimir_malloc(4096);
  check(r!=NULL,"C: allocator healthy after failed realloc");
  if(r) mimir_free(r);
}
/* Leg D: trim under pressure, largest level (128 pages). */
static void *legD_small[1200];
static void *legD_large[256];
static void leg_d(void){
  size_t pg=page_sz(), base=0, lim=0;
  if(arm_limit(128*pg,&base,&lim)!=0) _exit(2);
  printf("  baseline=%zu limit=%zu headroom_pages=128\n",base,lim);
  check(mimir_tune_set_retention_bound(16)==0,"D: retention bound raised to 16 (test-only tune)");
  size_t ns=0;
  for(size_t i=0;i<1200;i++){ void *p=mimir_malloc(64); if(!p) break; legD_small[ns++]=p; }
  printf("  small blocks allocated: %zu\n",ns);
  check(ns>=1000,"D: at least 1000 small blocks (4+ spans) allocated");
  for(size_t i=0;i<ns;i++) mimir_free(legD_small[i]);
  size_t rs=mimir_core_retained_spans(), rb=mimir_core_retained_bytes();
  printf("  retained after frees: spans=%zu bytes=%zu\n",rs,rb);
  check(rs>=2,"D: at least 2 retained empty spans held under pressure setup");
  check(rb==rs*pg,"D: retained bytes == spans x page exactly");
  size_t nl=0; void *p;
  while(nl<256 && (p=mimir_malloc(4096))!=NULL){ legD_large[nl]=p; nl++; }
  printf("  large successes before NULL: %zu\n",nl);
  check(nl<256,"D: clean NULL reached with retained spans held");
  size_t tr=mimir_core_trim_retained();
  printf("  trim released spans: %zu\n",tr);
  check(tr==rs,"D: trim released exactly the retained spans");
  check(mimir_core_retained_bytes()==0,"D: retained bytes 0 after trim");
  void *r=mimir_malloc(4096);
  check(r!=NULL,"D: large request served after trim freed VA under the same limit");
  if(r) mimir_free(r);
  for(size_t i=0;i<nl;i++) mimir_free(legD_large[i]);
}
/* Leg E: raw-mmap fragmentation leg (headroom 64 pages). */
static void *legE_maps[8192];
static void leg_e(void){
  size_t pg=page_sz(), base=0, lim=0;
  if(arm_limit(64*pg,&base,&lim)!=0) _exit(2);
  printf("  baseline=%zu limit=%zu headroom_pages=64\n",base,lim);
  size_t nm=0;
  while(nm<8192){
    void *m=mmap(NULL,pg,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(m==MAP_FAILED) break;
    legE_maps[nm++]=m;
  }
  printf("  raw 1-page mmaps before MAP_FAILED: %zu\n",nm);
  check(nm>=16,"E: raw mmap filled the headroom to MAP_FAILED");
  check(mimir_malloc(4096)==NULL,"E: large request exactly NULL while VA exhausted by raw mmap");
  check(mimir_malloc(64)==NULL,"E: small request exactly NULL while VA exhausted by raw mmap");
  for(size_t i=0;i<nm;i++) munmap(legE_maps[i],pg);
  void *r=mimir_malloc(4096);
  check(r!=NULL,"E: allocator healthy after raw mappings released");
  if(r) mimir_free(r);
}
/* Child runner: returns child exit status, or -1 on timeout. */
typedef void (*leg_fn)(void);
static int run_leg(const char *name, leg_fn fn){
  printf("LEG %s\n",name); fflush(stdout);
  pid_t pid=fork();
  if(pid==0){ checks=0; failures=0; fn();
    printf("  CHILD %s checks=%d failures=%d\n",name,checks,failures);
    fflush(stdout); _exit(failures?1:0); }
  if(pid<0){ check(0,"fork succeeded"); return -1; }
  int status=0; long waited_ms=0;
  for(;;){
    pid_t w=waitpid(pid,&status,WNOHANG);
    if(w==pid) break;
    if(waited_ms>=30000){ kill(pid,SIGKILL); (void)waitpid(pid,&status,0);
      printf("  [%s] %s: child exceeded 30 s wall bound (killed)\n","FAIL",name);
      failures++; checks++; return -1; }
    struct timespec ts; ts.tv_sec=0; ts.tv_nsec=50*1000*1000L;
    nanosleep(&ts,NULL); waited_ms+=50;
  }
  printf("  child wall time approx %ld ms (bound 30000 ms)\n",waited_ms);
  if(WIFEXITED(status) && WEXITSTATUS(status)==2){
    printf("  [skip] %s: setrlimit failed in child\n",name); return 2; }
  int ok=WIFEXITED(status) && WEXITSTATUS(status)==0;
  check(ok,name);
  return ok?0:1;
}
int main(void){
  setvbuf(stdout,outbuf,_IOFBF,sizeof outbuf);
  printf("test_i005: Phase 9 constrained-memory runs (X5)\n");
  size_t pg=page_sz();
  printf("PARENT page=%zu small_cutoff=1024 large4096_va_pages=2 span_va_pages=1\n",pg);
  check(pg==16384,"0: page size is 16384 B on this host (stated arithmetic)");
  run_leg("A large-fill until clean NULL",leg_a);
  run_leg("B smaller served where headroom remains",leg_b);
  run_leg("C realloc failure atomic under limit",leg_c);
  run_leg("D trim under pressure (largest level)",leg_d);
  run_leg("E raw-mmap fragmentation",leg_e);
  printf("LEG F fresh-process health probe (parent, unlimited)\n");
  unsigned char *h=mimir_malloc(4096);
  check(h!=NULL,"F: parent malloc works after constrained children");
  if(h){ fillp(h,4096,0xF0); check(patok(h,4096,0xF0),"F: parent pattern intact"); mimir_free(h); }
  unsigned char *z=mimir_calloc(128,16);
  check(z!=NULL,"F: parent calloc works");
  if(z){ int allz=1; for(size_t i=0;i<2048;i++) if(z[i]) allz=0; check(allz,"F: calloc memory zeroed"); mimir_free(z); }
  printf("RESULT: %s (%d checks, %d failures)\n", failures?"FAIL":"PASS",checks,failures);
  fflush(stdout);
  return failures?1:0;
}
