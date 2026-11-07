/*
 * test_i008.c -- evidence test for slice I-008 (Phase 9
 * repeated pressure cycles + responsiveness under
 * pressure; slice record: docs/PHASE_9_PRESSURE_CYCLES.md;
 * exit item X9 of docs/PHASE_9_FOUNDATION_PLAN.md, OWED at
 * base: "repeated pressure cycles are bounded: across the
 * stated cycle count, retained/live figures return to the
 * same bound each cycle, failure behaviour is stable, and
 * responsiveness under pressure is reported without a
 * bound claim").
 *
 * Run standalone via `make test-i008` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-i003..test-i007 pattern).
 *
 * DESIGN (stated before the run): CYCLES = 10 repeated
 * pressure cycles inside ONE forked child, so any ratchet
 * would accumulate in one process and be seen. The child
 * uses the I-005 form: it calibrates a per-process
 * RLIMIT_AS from its own measured VmSize baseline plus a
 * headroom of exactly 128 pages, and the parent enforces
 * a 30 s wall bound (poll waitpid at 2 ms granularity, SIGKILL on expiry).
 * Per-process limit ONLY -- no system-wide limit is ever
 * touched. The parent (unlimited) samples Samtal
 * (GET http://127.0.0.1:8787/, the G-008 house form)
 * before the child starts, on every poll while it runs,
 * and after it exits: every sample must be HTTP 200.
 *
 * Page arithmetic (measured on this host, asserted in
 * Leg 0; identical to I-005): page = 16384 B; a small
 * span costs 1 page of VA; class 64 holds 254 objects per
 * span (I-006 workload geometry), so 1200 x 64 B blocks
 * publish exactly 5 spans; a large 4096 B block costs
 * 2 pages of VA (1 reserve + 1 usable).
 *
 * ONE CYCLE (identical script every cycle):
 *  1. quiescent snapshot: Q-MAPPED == 0, Q-RETAINED == 0,
 *     span published == span retired, mapping published
 *     == mapping retired (all cumulative counters);
 *  2. small phase: allocate exactly 1200 x 64 B (patterns
 *     written), span-published delta exactly 5;
 *  3. free all small: retained spans exactly 5, retained
 *     bytes exactly 5 x page, Q-MAPPED exactly 5 x page
 *     (retention bound raised to 16 first, test-only tune
 *     -- the I-005 Leg D setup, so nothing is retired at
 *     free time);
 *  4. large fill: allocate 4096 B blocks until NULL --
 *     exactly 61 successes predicted: floor((128 - 5) / 2)
 *     (the I-005 Leg D figure at the same setup), then one
 *     further large request returns exactly NULL again;
 *  5. at full pressure: all 61 live patterns intact and
 *     pairwise non-overlapping;
 *  6. responsiveness probe DURING pressure: (a) a small
 *     64 B allocation (served from a retained span),
 *     pattern written and verified, then freed; (b) a
 *     Samtal-independent local health check (read and
 *     parse /proc/self/status, getpid self-match). Both
 *     timed with CLOCK_MONOTONIC and required to complete
 *     inside a stated 5 s process-liveness bound; the
 *     times themselves are recorded as
 *     timing-never-an-oracle -- printed, never asserted
 *     as a performance bound, and no responsiveness bound
 *     is claimed for the shared home host (X9 text);
 *  7. release: free all 61 large blocks; mapping-retired
 *     delta exactly 61;
 *  8. trim: mimir_core_trim_retained() releases exactly
 *     the 5 retained spans; retained bytes 0; the cycle
 *     ends at the same quiescent snapshot it began with.
 * After 10 cycles the child compares every cycle's
 * measured tuple against cycle 1's: any drift fails the
 * run here, in the open -- it is never smoothed over.
 *
 * EVIDENCE CLASS: the NULL in step 4 is a REAL OS failure
 * (kernel refuses mmap under RLIMIT_AS), the I-005 class,
 * kept separate from I-004's injected failures.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include "mimir_alloc.h"
#include "mimir_trim_i003.h"
#include "mimir_tune_g007.h"
#include "mimir_small_g003.h"
#include "../abi/mimir_audit_e005.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CYCLES 10
#define N_SMALL 1200
#define SMALL_REQ 64
#define LARGE_REQ 4096
#define HEADROOM_PAGES 128
#define EXPECT_SPANS 5
#define EXPECT_LARGE 61
#define CHILD_WALL_MS 30000L
#define PROBE_BOUND_NS 5000000000LL

static int failures = 0;
static int checks = 0;
static char outbuf[262144];
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
static long long now_ns(void){
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
  return (long long)ts.tv_sec*1000000000LL+(long long)ts.tv_nsec;
}
static int arm_limit(size_t headroom, size_t *base_out, size_t *lim_out){
  size_t base=vm_size_bytes(); if(base==0) return -1;
  struct rlimit rl; rl.rlim_cur=(rlim_t)(base+headroom);
  rl.rlim_max=(rlim_t)(base+headroom);
  if(setrlimit(RLIMIT_AS,&rl)!=0) return -1;
  *base_out=base; *lim_out=base+headroom; return 0;
}
/* Samtal sample via curl (parent side, G-008 house form).
 * Returns HTTP code, or -1 when curl itself failed;
 * latency in ms through ms_out when non-NULL. */
static int samtal_sample(double *ms_out){
  FILE *p=popen("curl -s -o /dev/null -w '%{http_code} %{time_total}' --max-time 5 http://127.0.0.1:8787/","r");
  if(!p) return -1;
  char buf[128]; buf[0]='\0';
  if(!fgets(buf,sizeof buf,p)){ pclose(p); return -1; }
  pclose(p);
  int code=-1; double sec=0.0;
  if(sscanf(buf,"%d %lf",&code,&sec)!=2) return -1;
  if(ms_out) *ms_out=sec*1000.0;
  return code;
}
/* Per-cycle measured tuple, compared across cycles. */
struct cyc {
  size_t n_small, span_delta, retained_spans, retained_bytes;
  size_t mapped_at_retained, n_large, trim_released;
  unsigned long spanret_delta, mapret_delta;
  long long probe_alloc_ns, probe_health_ns;
  int probe_ok, health_ok, patterns_ok, overlap_ok;
};
static struct cyc cycs[CYCLES];
static void *small_blocks[N_SMALL];
static void *large_blocks[256];

/* Quiescent-point reconciliation: nothing live, nothing
 * retained, every published counter retired. */
static int quiescent(const char *where){
  size_t qm=mimir_core_mapped_bytes();
  size_t qr=mimir_core_retained_bytes();
  size_t qs=mimir_core_retained_spans();
  unsigned long sp=mimir_audit_span_published, sr=mimir_audit_span_retired;
  unsigned long mp=mimir_audit_alloc_published, mr=mimir_audit_mapping_retired;
  printf("  QUIESCENT %s Q-MAPPED=%zu Q-RETAINED=%zu retained_spans=%zu span_pub=%lu span_ret=%lu map_pub=%lu map_ret=%lu\n",
         where,qm,qr,qs,sp,sr,mp,mr);
  int ok = (qm==0) && (qr==0) && (qs==0) && (sp==sr) && (mp==mr);
  check(ok,"quiescent: counters reconcile exactly (Q-MAPPED==0, Q-RETAINED==0, published==retired)");
  return ok;
}

static void run_cycle(int idx, size_t pg){
  struct cyc *c=&cycs[idx]; memset(c,0,sizeof *c);
  char lbl[128];
  printf("CYCLE %d of %d\n",idx+1,CYCLES);
  quiescent("cycle-start");
  /* Small phase: exactly N_SMALL x 64 B. */
  unsigned long sp0=mimir_audit_span_published;
  size_t ns=0;
  for(size_t i=0;i<N_SMALL;i++){
    void *p=mimir_malloc(SMALL_REQ); if(!p) break;
    small_blocks[ns]=p; fillp(p,SMALL_REQ,(unsigned)(0x30+i)); ns++;
  }
  c->n_small=ns;
  snprintf(lbl,sizeof lbl,"cycle %d: small successes exactly %d",idx+1,N_SMALL);
  check(ns==N_SMALL,lbl);
  c->span_delta=(size_t)(mimir_audit_span_published-sp0);
  snprintf(lbl,sizeof lbl,"cycle %d: span-published delta exactly %d",idx+1,EXPECT_SPANS);
  check(c->span_delta==EXPECT_SPANS,lbl);
  int spat=1; for(size_t i=0;i<ns;i++) if(!patok(small_blocks[i],SMALL_REQ,(unsigned)(0x30+i))) spat=0;
  snprintf(lbl,sizeof lbl,"cycle %d: small patterns intact before free",idx+1);
  check(spat,lbl);
  /* Free all small: retention bound 16 holds all 5 spans. */
  for(size_t i=0;i<ns;i++) mimir_free(small_blocks[i]);
  c->retained_spans=mimir_core_retained_spans();
  c->retained_bytes=mimir_core_retained_bytes();
  c->mapped_at_retained=mimir_core_mapped_bytes();
  snprintf(lbl,sizeof lbl,"cycle %d: retained spans exactly %d after free-all",idx+1,EXPECT_SPANS);
  check(c->retained_spans==EXPECT_SPANS,lbl);
  snprintf(lbl,sizeof lbl,"cycle %d: retained bytes exactly %d x page",idx+1,EXPECT_SPANS);
  check(c->retained_bytes==EXPECT_SPANS*pg,lbl);
  snprintf(lbl,sizeof lbl,"cycle %d: Q-MAPPED at retained point exactly %d x page",idx+1,EXPECT_SPANS);
  check(c->mapped_at_retained==EXPECT_SPANS*pg,lbl);
  /* Large fill to the clean NULL. */
  unsigned long mp0=mimir_audit_alloc_published;
  size_t nl=0; void *p;
  while(nl<256 && (p=mimir_malloc(LARGE_REQ))!=NULL){
    large_blocks[nl]=p; fillp(p,LARGE_REQ,(unsigned)(0x90+nl)); nl++;
  }
  c->n_large=nl;
  snprintf(lbl,sizeof lbl,"cycle %d: large successes before NULL exactly %d (floor((128-5)/2))",idx+1,EXPECT_LARGE);
  check(nl==EXPECT_LARGE,lbl);
  snprintf(lbl,sizeof lbl,"cycle %d: mapping-published delta equals large successes",idx+1);
  check((unsigned long)(mimir_audit_alloc_published-mp0)==(unsigned long)nl,lbl);
  snprintf(lbl,sizeof lbl,"cycle %d: further large request at pressure exactly NULL (stable failure)",idx+1);
  check(mimir_malloc(LARGE_REQ)==NULL,lbl);
  /* Live patterns + non-overlap at full pressure. */
  int pat=1; for(size_t i=0;i<nl;i++) if(!patok(large_blocks[i],LARGE_REQ,(unsigned)(0x90+i))) pat=0;
  c->patterns_ok=pat;
  snprintf(lbl,sizeof lbl,"cycle %d: live large patterns intact at full pressure",idx+1);
  check(pat,lbl);
  int ov=1;
  for(size_t i=0;i<nl&&ov;i++) for(size_t j=i+1;j<nl;j++){
    unsigned char *a=large_blocks[i],*b=large_blocks[j];
    if(a<b+LARGE_REQ && b<a+LARGE_REQ) ov=0;
  }
  c->overlap_ok=ov;
  snprintf(lbl,sizeof lbl,"cycle %d: live large blocks pairwise non-overlapping",idx+1);
  check(ov,lbl);
  /* Responsiveness probe DURING pressure (still at the
   * NULL point, all 61 large blocks live): a small alloc
   * served from a retained span + a local health check.
   * Times recorded; only the stated 5 s process-liveness
   * bound is asserted (timing-never-an-oracle). */
  long long t0=now_ns();
  unsigned char *probe=mimir_malloc(SMALL_REQ);
  if(probe){ fillp(probe,SMALL_REQ,0xEE); }
  long long t1=now_ns();
  c->probe_alloc_ns=t1-t0;
  c->probe_ok = (probe!=NULL) && patok(probe,SMALL_REQ,0xEE);
  snprintf(lbl,sizeof lbl,"cycle %d: responsiveness probe small alloc served under pressure",idx+1);
  check(c->probe_ok,lbl);
  snprintf(lbl,sizeof lbl,"cycle %d: probe alloc completed inside stated 5 s liveness bound",idx+1);
  check(c->probe_alloc_ns>=0 && c->probe_alloc_ns<PROBE_BOUND_NS,lbl);
  if(probe) mimir_free(probe);
  snprintf(lbl,sizeof lbl,"cycle %d: retained spans back to exactly %d after probe free",idx+1,EXPECT_SPANS);
  check(mimir_core_retained_spans()==EXPECT_SPANS,lbl);
  t0=now_ns();
  size_t vsz=vm_size_bytes();
  int hok = (vsz>0) && (getpid()>0);
  t1=now_ns();
  c->probe_health_ns=t1-t0; c->health_ok=hok;
  snprintf(lbl,sizeof lbl,"cycle %d: local health check (/proc/self/status) ok under pressure",idx+1);
  check(hok,lbl);
  snprintf(lbl,sizeof lbl,"cycle %d: health check inside stated 5 s liveness bound",idx+1);
  check(c->probe_health_ns>=0 && c->probe_health_ns<PROBE_BOUND_NS,lbl);
  /* Release all large, then trim back to quiescence. */
  unsigned long mr0=mimir_audit_mapping_retired;
  unsigned long sr0=mimir_audit_span_retired;
  for(size_t i=0;i<nl;i++) mimir_free(large_blocks[i]);
  c->mapret_delta=mimir_audit_mapping_retired-mr0;
  snprintf(lbl,sizeof lbl,"cycle %d: mapping-retired delta equals large successes",idx+1);
  check(c->mapret_delta==(unsigned long)nl,lbl);
  c->trim_released=mimir_core_trim_retained();
  c->spanret_delta=mimir_audit_span_retired-sr0;
  snprintf(lbl,sizeof lbl,"cycle %d: trim released exactly the %d retained spans",idx+1,EXPECT_SPANS);
  check(c->trim_released==EXPECT_SPANS,lbl);
  snprintf(lbl,sizeof lbl,"cycle %d: span-retired delta equals trim count",idx+1);
  check(c->spanret_delta==(unsigned long)c->trim_released,lbl);
  quiescent("cycle-end");
  printf("  CYCLE-ROW cycle=%d n_small=%zu span_delta=%zu retained_spans=%zu retained_bytes=%zu n_large=%zu probe_ok=%d probe_alloc_ns=%lld health_ok=%d health_ns=%lld trim=%zu patterns_ok=%d overlap_ok=%d\n",
         idx+1,c->n_small,c->span_delta,c->retained_spans,c->retained_bytes,
         c->n_large,c->probe_ok,c->probe_alloc_ns,c->health_ok,c->probe_health_ns,
         c->trim_released,c->patterns_ok,c->overlap_ok);
}

static void child_body(void){
  size_t pg=page_sz(), base=0, lim=0;
  if(arm_limit(HEADROOM_PAGES*pg,&base,&lim)!=0) _exit(2);
  printf("  baseline=%zu limit=%zu headroom_pages=%d\n",base,lim,HEADROOM_PAGES);
  check(mimir_tune_set_retention_bound(16)==0,"child: retention bound raised to 16 (test-only tune)");
  check(mimir_tune_get_retention_bound()==16,"child: retention bound reads back 16");
  for(int i=0;i<CYCLES;i++) run_cycle(i,pg);
  /* Drift verdict: every cycle's tuple equals cycle 1's
   * (probe/health times excluded -- timing-never-an-oracle). */
  int drift=0;
  for(int i=1;i<CYCLES;i++){
    struct cyc *a=&cycs[0], *b=&cycs[i];
    if(a->n_small!=b->n_small || a->span_delta!=b->span_delta ||
       a->retained_spans!=b->retained_spans || a->retained_bytes!=b->retained_bytes ||
       a->mapped_at_retained!=b->mapped_at_retained || a->n_large!=b->n_large ||
       a->trim_released!=b->trim_released || a->spanret_delta!=b->spanret_delta ||
       a->mapret_delta!=b->mapret_delta || a->probe_ok!=b->probe_ok ||
       a->health_ok!=b->health_ok || a->patterns_ok!=b->patterns_ok ||
       a->overlap_ok!=b->overlap_ok) drift=1;
  }
  check(!drift,"drift: all 10 cycle tuples identical to cycle 1 (no ratchet, failure behaviour stable)");
}

int main(void){
  setvbuf(stdout,outbuf,_IOFBF,sizeof outbuf);
  printf("test_i008: Phase 9 repeated pressure cycles + responsiveness (X9)\n");
  printf("DESIGN cycles=%d headroom_pages=%d n_small=%d expect_spans=%d expect_large=%d child_wall_bound_ms=%ld probe_liveness_bound_s=5\n",
         CYCLES,HEADROOM_PAGES,N_SMALL,EXPECT_SPANS,EXPECT_LARGE,CHILD_WALL_MS);
  size_t pg=page_sz();
  printf("PARENT page=%zu small_cutoff=1024 large4096_va_pages=2 span_va_pages=1 class64_per_span=254\n",pg);
  check(pg==16384,"0: page size is 16384 B on this host (stated arithmetic)");
  double ms=0.0; int code=samtal_sample(&ms);
  printf("SAMTAL pre-run http=%d ms=%.3f\n",code,ms);
  check(code==200,"0: Samtal HTTP 200 before the cycles");
  /* Sampler process: forked BEFORE the cycles child and
   * killed right after it is reaped, it samples Samtal
   * back-to-back (curl's own latency is the pacing) into
   * a scratch file for the whole cycles window. The
   * parent's own in-loop sampling under-sampled the
   * window (the cycles child finishes in tens of ms,
   * faster than one parent-side curl plus poll sleep:
   * INC-I008-02). */
  char samp_path[]="/tmp/i008_samtal_XXXXXX";
  int sfd=mkstemp(samp_path); if(sfd>=0) close(sfd);
  pid_t sampler=-1;
  if(sfd>=0){
    sampler=fork();
    if(sampler==0){
      /* Direct-socket sampler: one persistent loopback
       * connection to Samtal (HTTP/1.1, Content-Length
       * framed), one timed GET / per sample, each result
       * line flushed to the scratch file at once. Two
       * earlier forms under-sampled this ~50 ms window:
       * parent-side curl polling (1 sample) and a
       * persistent curl whose pipe buffering held its
       * output back (INC-I008-02). */
      FILE *sf=fopen(samp_path,"a"); if(!sf) _exit(3);
      int sock=-1;
      for(;;){
        if(sock<0){
          sock=socket(AF_INET,SOCK_STREAM,0);
          struct sockaddr_in a; memset(&a,0,sizeof a);
          a.sin_family=AF_INET; a.sin_port=htons(8787);
          a.sin_addr.s_addr=htonl(0x7f000001u);
          if(sock<0 || connect(sock,(struct sockaddr*)&a,sizeof a)!=0){
            fprintf(sf,"-1 0.000\n"); fflush(sf);
            if(sock>=0) close(sock);
            sock=-1;
            struct timespec ts; ts.tv_sec=0; ts.tv_nsec=100*1000*1000L;
            nanosleep(&ts,NULL); continue;
          }
        }
        static const char req[]="GET / HTTP/1.1\r\nHost: 127.0.0.1:8787\r\nConnection: keep-alive\r\n\r\n";
        long long t0=now_ns();
        int bad=0, code=-1; long clen=-1;
        if(send(sock,req,sizeof req-1,0)!=(ssize_t)(sizeof req-1)) bad=1;
        char hb[8192]; size_t hn=0;
        while(!bad && hn<sizeof hb-1){
          char ch; ssize_t r=recv(sock,&ch,1,0);
          if(r<=0){ bad=1; break; }
          hb[hn++]=ch;
          if(hn>=4 && memcmp(hb+hn-4,"\r\n\r\n",4)==0) break;
        }
        hb[hn]='\0';
        if(!bad){
          if(sscanf(hb,"HTTP/%*s %d",&code)!=1) code=-1;
          const char *cl=strstr(hb,"Content-Length:");
          if(cl) clen=strtol(cl+15,NULL,10);
          if(clen<0) bad=1;
        }
        size_t got=0;
        while(!bad && got<(size_t)clen){
          char db[16384];
          size_t want=(size_t)clen-got; if(want>sizeof db) want=sizeof db;
          ssize_t r=recv(sock,db,want,0);
          if(r<=0){ bad=1; break; }
          got+=(size_t)r;
        }
        long long t1=now_ns();
        if(bad){ fprintf(sf,"-1 %.3f\n",(double)(t1-t0)/1000000.0); fflush(sf);
          close(sock); sock=-1; continue; }
        fprintf(sf,"%d %.3f\n",code,(double)(t1-t0)/1000000.0); fflush(sf);
      }
    }
  }
  printf("LEG cycles x%d (one child, RLIMIT_AS calibrated)\n",CYCLES); fflush(stdout);
  pid_t pid=fork();
  if(pid==0){ checks=0; failures=0; child_body();
    printf("  CHILD checks=%d failures=%d\n",checks,failures);
    fflush(stdout); _exit(failures?1:0); }
  if(pid<0){ check(0,"fork succeeded"); }
  else {
    int status=0; long waited_ms=0; int killed=0;
    for(;;){
      pid_t w=waitpid(pid,&status,WNOHANG);
      if(w==pid) break;
      if(waited_ms>=CHILD_WALL_MS){ kill(pid,SIGKILL); (void)waitpid(pid,&status,0); killed=1;
        printf("  [FAIL] child exceeded %ld ms wall bound (killed)\n",CHILD_WALL_MS);
        failures++; checks++; break; }
      struct timespec ts; ts.tv_sec=0; ts.tv_nsec=2*1000*1000L;
      nanosleep(&ts,NULL); waited_ms+=2;
    }
    printf("  child wall time approx %ld ms (bound %ld ms; poll granularity 2 ms, sampler time excluded)\n",waited_ms,CHILD_WALL_MS);
    (void)killed;
    if(WIFEXITED(status) && WEXITSTATUS(status)==2){
      printf("  [skip] cycles child: setrlimit failed in child\n"); checks++; failures++;
    } else if(!killed) {
      int ok=WIFEXITED(status) && WEXITSTATUS(status)==0;
      check(ok,"cycles child: all cycles passed");
    }
  }
  if(sampler>0){ kill(sampler,SIGKILL); (void)waitpid(sampler,NULL,0); }
  int samples=0, all200=1; double mn=1e18, mx=0.0;
  if(sfd>=0){
    FILE *sf=fopen(samp_path,"r");
    if(sf){ int sc; double sm;
      while(fscanf(sf,"%d %lf",&sc,&sm)==2){ samples++;
        if(sc!=200) all200=0;
        if(sm<mn) mn=sm;
        if(sm>mx) mx=sm; }
      fclose(sf); }
    unlink(samp_path);
  }
  printf("SAMTAL during-run samples=%d all_http_200=%d ms_min=%.3f ms_max=%.3f (sampler process spanning the cycles window; timing-never-an-oracle, no bound claimed)\n",
         samples,all200,samples?mn:0.0,mx);
  check(samples>=1,"parent: Samtal sampled at least once across the cycles window");
  check(all200 && samples>=1,"parent: Samtal HTTP 200 on every during-run sample");
  ms=0.0; code=samtal_sample(&ms);
  printf("SAMTAL post-run http=%d ms=%.3f\n",code,ms);
  check(code==200,"parent: Samtal HTTP 200 after the cycles");
  printf("LEG F fresh-process health probe (parent, unlimited)\n");
  unsigned char *h=mimir_malloc(4096);
  check(h!=NULL,"F: parent malloc works after pressure cycles");
  if(h){ fillp(h,4096,0xF0); check(patok(h,4096,0xF0),"F: parent pattern intact"); mimir_free(h); }
  unsigned char *z=mimir_calloc(128,16);
  check(z!=NULL,"F: parent calloc works");
  if(z){ int allz=1; for(size_t i=0;i<2048;i++) if(z[i]) allz=0; check(allz,"F: calloc memory zeroed"); mimir_free(z); }
  printf("RESULT: %s (%d checks, %d failures)\n", failures?"FAIL":"PASS",checks,failures);
  fflush(stdout);
  return failures?1:0;
}
