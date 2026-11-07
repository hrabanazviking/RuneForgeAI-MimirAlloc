/*
 * test_i007.c -- evidence test for slice I-007 (Phase 9
 * telemetry separation proof; slice record:
 * docs/PHASE_9_TELEMETRY_SEPARATION.md; definitions:
 * docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md (I-002),
 * esp. S3.2 quantities, S4.1/S4.2 separation tables and
 * the S5.3 quiescent-point rule; foundation reconciliation
 * reading: docs/PHASE_9_FOUNDATION_PLAN.md (I-001)).
 *
 * Run standalone via `make test-i007` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-i003..test-i005 pattern).
 *
 * DEFINITIONS (written before the run, per I-002):
 * - Q-MAPPED: allocator-managed mapped bytes =
 *   mimir_core_mapped_bytes() (one locked snapshot:
 *   live mapping-path bytes + live span bytes).
 * - Q-RETAINED: mimir_core_retained_bytes() (EMPTY spans
 *   x page size, one locked snapshot).
 * - Audit counters: E-005 call/mapping counters +
 *   G-003 span/small counters, read at the same instant.
 * - Process figures (NOT allocator quantities): VmSize
 *   and VmRSS from /proc/self/status; Rss / Anonymous /
 *   Pss_File from /proc/self/smaps_rollup; mapping count
 *   and anonymous VM total (sum of Size over pathname-less
 *   or bracket-annotated entries) from /proc/self/maps.
 *
 * METHOD: single thread; every snapshot is taken at a
 * quiescent point (no allocation in flight). Snapshot
 * reads use raw open/read into static buffers -- no stdio,
 * no heap -- so the act of measuring moves neither the
 * allocator accounts nor (beyond the already-warm stdio
 * buffer) the process figures. Two sentinel blocks (one
 * large mapping-path, one small span-path) are held live
 * across every leg and verified byte-identical after each
 * leg: their constant contribution is 32768 + 16384 =
 * 49152 B of Q-MAPPED.
 *
 * PREDICTED ARITHMETIC (page = host page size, asserted):
 * - mimir_malloc(4096): mapping_length = page_round_up(
 *   page + 4096) = 2 pages = 32768 B at page 16384.
 * - mimir_malloc(1024): one new span = 1 page = 16384 B
 *   (class 1024, distinct from the sentinel 256 span).
 * - Free of a large block releases its mapping whole:
 *   Q-MAPPED and VmSize both drop by exactly its length.
 * - Free of the only object in a span RETAINS the span
 *   (bound 2): Q-MAPPED and VmSize do not move at all;
 *   only Q-RETAINED moves (+1 page). Trim then releases
 *   it: Q-MAPPED / VmSize / RSS drop by exactly 1 page.
 * - A raw anonymous mmap and a file-backed mmap move the
 *   process figures by exactly their lengths while every
 *   allocator quantity and counter stays exactly unchanged.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include "mimir_alloc.h"
#include "mimir_small_g003.h"
#include "mimir_trim_i003.h"
#include "../abi/mimir_audit_e005.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
static int failures=0, checks=0;
static void check(int c,const char*l){checks++;printf("  [%s] %s\n",c?"ok":"FAIL",l);if(!c)failures++;}
static void check_eq(size_t g,size_t w,const char*l){checks++;printf("  [%s] %s (got %zu, want %zu)\n",g==w?"ok":"FAIL",l,g,w);if(g!=w)failures++;}
static void fillp(unsigned char*p,size_t n,unsigned s){for(size_t i=0;i<n;i++)p[i]=(unsigned char)(s+(unsigned)i*7u);}
static int patok(const unsigned char*p,size_t n,unsigned s){for(size_t i=0;i<n;i++)if(p[i]!=(unsigned char)(s+(unsigned)i*7u))return 0;return 1;}
static void touch(unsigned char*p,size_t n,size_t pg){for(size_t o=0;o<n;o+=pg)p[o]=(unsigned char)(p[o]+1u);}
typedef struct{
  size_t q_mapped,q_retained,ret_spans;
  unsigned long c_malloc,c_free,c_pub,c_ret,c_spanpub,c_spanret,c_salloc,c_sfree;
  size_t vm_size,vm_rss,roll_rss,roll_anon,roll_pssfile,maps_count,maps_anon;
} snap_t;
static char buf_status[8192];
static char buf_roll[8192];
static char buf_maps[1<<19];
static size_t readfile(const char*path,char*buf,size_t cap){
  int fd=open(path,O_RDONLY); if(fd<0) return 0;
  size_t tot=0; ssize_t r;
  while(tot+1<cap && (r=read(fd,buf+tot,cap-1-tot))>0) tot+=(size_t)r;
  close(fd); buf[tot]='\0'; return tot;
}
static size_t kb_field(const char*buf,const char*key){
  const char*p=strstr(buf,key); if(!p) return 0;
  p+=strlen(key); size_t v=0; int any=0;
  while(*p==' '||*p=='\t') p++;
  while(*p>='0'&&*p<='9'){v=v*10+(size_t)(*p-'0');p++;any=1;}
  return any?v*1024u:0;
}
static void maps_parse(const char*buf,size_t*count,size_t*anon){
  size_t cnt=0,an=0; const char*p=buf;
  while(*p){
    const char*eol=strchr(p,'\n'); if(!eol) eol=p+strlen(p);
    unsigned long long a=0,b=0; int pos=0;
    /* parse two hex fields */
    size_t i=0; unsigned long long vals[2]={0,0}; int vi=0; int innum=0;
    for(i=0;p+i<eol && vi<2;i++){
      char c=p[i];
      int hex=(c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');
      if(hex){int d=(c<='9')?c-'0':(c>='a'?c-'a'+10:c-'A'+10); vals[vi]=vals[vi]*16+(unsigned long long)d; innum=1;}
      else if(c=='-'&&innum){vi++;innum=0;}
      else if(innum){break;}
    }
    a=vals[0]; b=vals[1]; (void)pos;
    /* pathname: after 5 whitespace-separated fields */
    int fields=0; const char*q=p; int inws=1; const char*pathst=NULL;
    while(q<eol){
      if(*q==' '||*q=='\t'){inws=1; q++;}
      else{ if(inws){fields++; if(fields==6){pathst=q; break;}} inws=0; q++; }
    }
    int isanon=1;
    if(pathst){ if(*pathst!='/') isanon=1; else isanon=0; }
    cnt++; if(isanon) an+=(size_t)(b-a);
    p=(*eol=='\n')?eol+1:eol;
  }
  *count=cnt; *anon=an;
}
static void snapshot(snap_t*s){
  s->q_mapped=mimir_core_mapped_bytes();
  s->q_retained=mimir_core_retained_bytes();
  s->ret_spans=mimir_core_retained_spans();
  s->c_malloc=mimir_audit_call_malloc; s->c_free=mimir_audit_call_free;
  s->c_pub=mimir_audit_alloc_published; s->c_ret=mimir_audit_mapping_retired;
  s->c_spanpub=mimir_audit_span_published; s->c_spanret=mimir_audit_span_retired;
  s->c_salloc=mimir_audit_small_alloc; s->c_sfree=mimir_audit_small_free;
  readfile("/proc/self/status",buf_status,sizeof buf_status);
  s->vm_size=kb_field(buf_status,"VmSize:");
  s->vm_rss=kb_field(buf_status,"VmRSS:");
  readfile("/proc/self/smaps_rollup",buf_roll,sizeof buf_roll);
  s->roll_rss=kb_field(buf_roll,"Rss:");
  s->roll_anon=kb_field(buf_roll,"Anonymous:");
  s->roll_pssfile=kb_field(buf_roll,"Pss_File:");
  readfile("/proc/self/maps",buf_maps,sizeof buf_maps);
  maps_parse(buf_maps,&s->maps_count,&s->maps_anon);
}
static void print_snap(const char*tag,const snap_t*s){
  printf("  SNAP %s: Q-MAPPED=%zu Q-RETAINED=%zu ret_spans=%zu VmSize=%zu VmRSS=%zu RollRss=%zu RollAnon=%zu PssFile=%zu maps=%zu maps_anon=%zu pub-ret=%lu spanpub-spanret=%lu salloc-sfree=%lu\n",tag,s->q_mapped,s->q_retained,s->ret_spans,s->vm_size,s->vm_rss,s->roll_rss,s->roll_anon,s->roll_pssfile,s->maps_count,s->maps_anon,s->c_pub-s->c_ret,s->c_spanpub-s->c_spanret,s->c_salloc-s->c_sfree);
}
static int alloc_same(const snap_t*a,const snap_t*b){
  return a->q_mapped==b->q_mapped && a->q_retained==b->q_retained && a->ret_spans==b->ret_spans && a->c_malloc==b->c_malloc && a->c_free==b->c_free && a->c_pub==b->c_pub && a->c_ret==b->c_ret && a->c_spanpub==b->c_spanpub && a->c_spanret==b->c_spanret && a->c_salloc==b->c_salloc && a->c_sfree==b->c_sfree;
}
int main(void){
  size_t pg=(size_t)sysconf(_SC_PAGESIZE);
  printf("test_i007: telemetry separation proof (I-002 definitions, quiescent points)\n");
  printf("page=%zu\n",pg);
  check(pg==16384,"host page size is 16384 (measured, Pi)");
  snap_t s0,s1,sa,sb,sc,sd,se,sf,sg,sh,sz;
  snapshot(&s0); print_snap("S0-baseline",&s0);
  check_eq(s0.q_mapped,0,"S0: Q-MAPPED is 0 before any mimir allocation");
  check_eq(s0.q_retained,0,"S0: Q-RETAINED is 0");
  /* Sentinels held across all legs. */
  unsigned char *big=mimir_malloc(4096);
  unsigned char *small=mimir_malloc(256);
  check(big!=NULL && small!=NULL,"sentinels allocated (large 4096, small 256)");
  if(!big||!small) return 1;
  fillp(big,4096,0xB1); fillp(small,256,0x51);
  snapshot(&s1); print_snap("S1-sentinels",&s1);
  check_eq(s1.q_mapped,49152,"S1: Q-MAPPED = 32768 (large) + 16384 (span) = 49152");
  check_eq(s1.vm_size-s0.vm_size,49152,"S1: VmSize delta equals Q-MAPPED delta exactly");
  check_eq(s1.maps_anon-s0.maps_anon,49152,"S1: anonymous VM delta equals Q-MAPPED delta exactly");
  check_eq(s1.maps_count-s0.maps_count,2,"S1: mapping count delta = 2 (1 mapping + 1 span)");
  /* LEG A1: two more large blocks move allocator + process together. */
  unsigned char *a1=mimir_malloc(4096);
  unsigned char *a2=mimir_malloc(4096);
  check(a1!=NULL && a2!=NULL,"A1: two large blocks allocated");
  if(!a1||!a2) return 1;
  fillp(a1,4096,0xA1); fillp(a2,4096,0xA2);
  snapshot(&sa); print_snap("A1-two-large",&sa);
  check_eq(sa.q_mapped-s1.q_mapped,65536,"A1: Q-MAPPED delta = 2 x 32768 exactly");
  check_eq(sa.vm_size-s1.vm_size,65536,"A1: VmSize delta = Q delta exactly");
  check_eq(sa.maps_anon-s1.maps_anon,65536,"A1: anonymous VM delta = Q delta exactly");
  printf("  A1 mapping count delta=%zd (kernel VMA merging: adjacent anon mappings may merge; count is NOT an allocator quantity -- bytes are, see INC-I007-01)\n",(ssize_t)sa.maps_count-(ssize_t)s1.maps_count);
  check(sa.maps_count<=s1.maps_count+2,"A1: mapping count delta <= 2 (merging can only reduce it)");
  check_eq((size_t)(sa.c_pub-sa.c_ret)-(size_t)(s1.c_pub-s1.c_ret),2,"A1: live mappings (published-retired) delta = 2");
  printf("  A1 residency: VmRSS delta=%zd RollAnon delta=%zd (touched allocator pages become resident; mapped != resident by definition)\n",(ssize_t)sa.vm_rss-(ssize_t)s1.vm_rss,(ssize_t)sa.roll_anon-(ssize_t)s1.roll_anon);
  check(sa.roll_anon>=s1.roll_anon+32768,"A1: rollup Anonymous rose by at least 32768 (residency is kernel-paced, reported not equated)");
  check(patok(big,4096,0xB1)&&patok(small,256,0x51),"A1: sentinels byte-identical");
  /* LEG A2: one small 1024 object acquires one new span. */
  unsigned char *sm=mimir_malloc(1024);
  check(sm!=NULL,"A2: small 1024 block allocated");
  if(!sm) return 1;
  fillp(sm,1024,0x24);
  snapshot(&sb); print_snap("A2-small-span",&sb);
  check_eq(sb.q_mapped-sa.q_mapped,16384,"A2: Q-MAPPED delta = 1 span page exactly");
  check_eq(sb.vm_size-sa.vm_size,16384,"A2: VmSize delta = 1 page exactly");
  check(sb.maps_count<=sa.maps_count+1,"A2: mapping count delta <= 1 (VMA merging, INC-I007-01)");
  check(patok(big,4096,0xB1)&&patok(small,256,0x51),"A2: sentinels byte-identical");
  /* LEG A3: free larges (release whole), then free small (retains span). */
  mimir_free(a1); mimir_free(a2);
  snapshot(&sc); print_snap("A3a-large-freed",&sc);
  check_eq(sb.q_mapped-sc.q_mapped,65536,"A3a: Q-MAPPED drops exactly 65536 on large frees");
  check_eq(sb.vm_size-sc.vm_size,65536,"A3a: VmSize drops exactly 65536 on large frees");
  mimir_free(sm);
  snapshot(&sd); print_snap("A3b-small-freed-retained",&sd);
  check_eq(sd.q_mapped,sc.q_mapped,"A3b: Q-MAPPED does NOT move when small free retains the span");
  check_eq(sd.vm_size,sc.vm_size,"A3b: VmSize does NOT move on retained free");
  check_eq(sd.vm_rss,sc.vm_rss,"A3b: VmRSS does NOT move on retained free (page stays mapped+resident)");
  check_eq(sd.q_retained-sc.q_retained,16384,"A3b: Q-RETAINED rises by exactly 1 page -- the ONLY figure that moves");
  check_eq(sd.ret_spans,1,"A3b: retained span count = 1");
  check(patok(big,4096,0xB1)&&patok(small,256,0x51),"A3: sentinels byte-identical");
  /* LEG A4: trim releases the retained span. */
  size_t trimmed=mimir_core_trim_retained();
  snapshot(&se); print_snap("A4-trimmed",&se);
  check_eq(trimmed,1,"A4: trim released exactly 1 span");
  check_eq(sd.q_mapped-se.q_mapped,16384,"A4: Q-MAPPED drops exactly 1 page on trim");
  check_eq(sd.vm_size-se.vm_size,16384,"A4: VmSize drops exactly 1 page on trim");
  check_eq(se.q_retained,0,"A4: Q-RETAINED returns to 0");
  check_eq(se.q_mapped,s1.q_mapped,"A4: Q-MAPPED back to sentinel-only 49152");
  check(patok(big,4096,0xB1)&&patok(small,256,0x51),"A4: sentinels byte-identical");
  /* LEG B1: raw anonymous mmap moves process, never allocator. */
  unsigned char *raw=mmap(NULL,65536,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  check(raw!=MAP_FAILED,"B1: raw anonymous mmap of 65536 B succeeded");
  if(raw==MAP_FAILED) return 1;
  touch(raw,65536,pg);
  snapshot(&sf); print_snap("B1-raw-anon-mmap",&sf);
  check(alloc_same(&se,&sf),"B1: EVERY allocator quantity + counter exactly unchanged by raw mmap");
  check_eq(sf.vm_size-se.vm_size,65536,"B1: VmSize rises exactly 65536");
  check_eq(sf.maps_anon-se.maps_anon,65536,"B1: anonymous VM rises exactly 65536");
  check(sf.maps_count<=se.maps_count+1,"B1: mapping count rises by at most 1 (VMA merging, INC-I007-01)");
  check_eq(sf.roll_anon-se.roll_anon,65536,"B1: rollup Anonymous rises exactly 65536 (all 4 pages touched)");
  check_eq(sf.vm_rss-se.vm_rss,65536,"B1: VmRSS rises exactly 65536");
  munmap(raw,65536);
  snapshot(&sg); print_snap("B1b-munmap",&sg);
  check_eq(sg.vm_size,se.vm_size,"B1b: VmSize returns exactly after munmap");
  check_eq(sg.maps_count,se.maps_count,"B1b: mapping count returns exactly");
  check(alloc_same(&se,&sg),"B1b: allocator accounts still exactly unchanged");
  check(patok(big,4096,0xB1)&&patok(small,256,0x51),"B1: sentinels byte-identical");
  /* LEG B2: file-backed mmap -- named non-allocator kind (I-001 X8). */
  char tmpl[]="/tmp/mimir_i007_XXXXXX";
  int fd=mkstemp(tmpl);
  check(fd>=0,"B2: temp file created");
  if(fd<0) return 1;
  check(ftruncate(fd,65536)==0,"B2: temp file sized 65536");
  unsigned char wbuf[4096]; memset(wbuf,0x46,sizeof wbuf);
  for(int i=0;i<16;i++){ ssize_t w=write(fd,wbuf,sizeof wbuf); if(w!=(ssize_t)sizeof wbuf){check(0,"B2: write full file"); break;} }
  unsigned char *fb=mmap(NULL,65536,PROT_READ,MAP_PRIVATE,fd,0);
  check(fb!=MAP_FAILED,"B2: file-backed mmap succeeded");
  if(fb==MAP_FAILED) return 1;
  volatile unsigned long sum=0; for(size_t o=0;o<65536;o+=pg) sum+=fb[o];
  printf("  B2 file bytes checksum (volatile read): %lu\n",sum);
  snapshot(&sh); print_snap("B2-file-backed",&sh);
  check(alloc_same(&sg,&sh),"B2: EVERY allocator quantity + counter exactly unchanged by file-backed mmap");
  check_eq(sh.vm_size-sg.vm_size,65536,"B2: VmSize rises exactly 65536");
  check_eq(sh.maps_count-sg.maps_count,1,"B2: mapping count rises by 1");
  check_eq(sh.maps_anon,sg.maps_anon,"B2: anonymous VM does NOT move (file-backed is not anonymous)");
  check_eq(sh.roll_anon,sg.roll_anon,"B2: rollup Anonymous does NOT move");
  check(sh.vm_rss>=sg.vm_rss+16384,"B2: VmRSS rises (file pages resident; exact delta reported in snapshot)");
  printf("  B2 residency: VmRSS delta=%zd PssFile delta=%zd RollRss delta=%zd\n",(ssize_t)sh.vm_rss-(ssize_t)sg.vm_rss,(ssize_t)sh.roll_pssfile-(ssize_t)sg.roll_pssfile,(ssize_t)sh.roll_rss-(ssize_t)sg.roll_rss);
  munmap(fb,65536); close(fd); unlink(tmpl);
  check(patok(big,4096,0xB1)&&patok(small,256,0x51),"B2: sentinels byte-identical");
  /* LEG C: free sentinels, trim, final quiescent reconciliation. */
  mimir_free(big); mimir_free(small);
  size_t tr2=mimir_core_trim_retained();
  snapshot(&sz); print_snap("C-final",&sz);
  printf("  C: final trim released %zu retained span(s)\n",tr2);
  check_eq(sz.q_mapped,0,"C: Q-MAPPED returns to exactly 0");
  check_eq(sz.q_retained,0,"C: Q-RETAINED returns to exactly 0");
  check(sz.c_pub==sz.c_ret,"C: mapping counters reconcile published == retired");
  check(sz.c_spanpub==sz.c_spanret,"C: span counters reconcile published == retired");
  check(sz.c_salloc==sz.c_sfree,"C: small counters reconcile alloc == free");
  check_eq(sz.vm_size,s0.vm_size,"C: VmSize returns EXACTLY to baseline -- allocator VM fully released");
  printf("  RECONCILIATION (quiescent, baseline S0): process VmSize=%zu = allocator Q-MAPPED %zu + non-allocator VM %zu\n",s0.vm_size,s0.q_mapped,s0.vm_size-s0.q_mapped);
  printf("  RECONCILIATION: baseline anonymous VM=%zu = allocator 0 + non-allocator anonymous (libc heap/stack/loader) %zu\n",s0.maps_anon,s0.maps_anon);
  printf("  RECONCILIATION: baseline rollup Anonymous (resident anon)=%zu, VmRSS=%zu, PssFile=%zu -- all non-allocator at baseline\n",s0.roll_anon,s0.vm_rss,s0.roll_pssfile);
  printf("  RECONCILIATION RESIDUAL: final VmRSS - baseline VmRSS = %zd B; final RollAnon - baseline = %zd B (residency is kernel-managed; VM accounts reconcile exactly, resident accounts within this reported residual)\n",(ssize_t)sz.vm_rss-(ssize_t)s0.vm_rss,(ssize_t)sz.roll_anon-(ssize_t)s0.roll_anon);
  check(sz.vm_rss<=s0.vm_rss+16384,"C: final VmRSS within 1 page of baseline (residual reported above)");
  printf("checks=%d failures=%d\n",checks,failures);
  printf("RESULT: %s\n",failures?"FAIL":"PASS");
  return failures?1:0;
}
