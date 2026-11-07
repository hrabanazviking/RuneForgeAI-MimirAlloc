/* probe_m005.c - M-005 Phase 13 long soak + repeated cycles.
 * Main leg (mode "main"): object-linked against the library
 * sources (L-008 pattern) so the banked test-visible surfaces
 * are readable: stats via tests/core/mimir_stats_j002.h,
 * retained/trim via tests/core/mimir_trim_i003.h. All churn
 * goes through the public mimir_ API only (explicit adoption).
 * 10 cycles x 20000 mixed ops, 256-slot live set, small
 * classes + large path, full live-set pattern verification
 * per cycle, trim at each cycle boundary, 10 s light pacing
 * between cycles. Band (stated in advance, derived from
 * cycles 1-2, never widened): post-trim retained bytes == 0
 * every cycle; post-cycle bytes_mapped <= max(c1,c2)+1 page;
 * post-cycle VmRSS <= max(c1,c2)+512 kB. Beyond band prints
 * FINDING and stops with exit 1.
 * Dlopen leg (mode "dl <so>"): 6 load/unload cycles against
 * build/libmimiralloc.so, public symbols via dlsym only,
 * G-008 workload shape; after each dlclose the image must
 * have 0 mappings and anonymous rw bytes must equal the
 * cycle-0 close level (H-001 shape: 0 B/cycle growth). */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>
#include <unistd.h>
#include "mimir_alloc.h"
#include "../core/mimir_stats_j002.h"
#include "../core/mimir_trim_i003.h"

static uint64_t g_ops, g_checks, g_bytes_checked;
static int g_fail;
static uint32_t g_rng;
static uint32_t rnd(void){ g_rng = g_rng*1664525u + 1013904223u; return g_rng >> 8; }
static uint8_t patb(uint32_t seed, size_t o){ return (uint8_t)(seed + (uint32_t)o*31u + ((uint32_t)o >> 7)); }
static void fillp(uint8_t *p, size_t n, uint32_t seed){ for(size_t o=0;o<n;o++) p[o]=patb(seed,o); }
static int verifyp(const uint8_t *p, size_t n, uint32_t seed){
 g_checks++; g_bytes_checked += n;
 for(size_t o=0;o<n;o++) if(p[o]!=patb(seed,o)) return -1;
 return 0;
}
static long rss_kb(void){
 FILE *f=fopen("/proc/self/status","r"); char ln[256]; long kb=-1;
 if(!f) return -1;
 while(fgets(ln,sizeof ln,f)) if(sscanf(ln,"VmRSS: %ld kB",&kb)==1) break;
 fclose(f); return kb;
}

/* ---------------- main leg ---------------- */
#define NSLOT 256
#define CYCLES 10
#define OPS_PER_CYCLE 20000
struct slot { uint8_t *p; size_t n; uint32_t seed; };
static struct slot g_slots[NSLOT];
static uint64_t g_live, g_hw;
static const size_t g_small[] = {16,32,48,64,96,128,192,256,384,512,768,1024,1536,2048};

static void note_fail(const char *what, int cyc){
 g_fail++;
 printf("FAILURE cycle=%d %s\n", cyc, what);
}
static int slot_alloc(int s, int cyc){
 uint32_t r = rnd(); int kind = (int)(r % 10u);
 if(kind < 7){
  size_t n = g_small[rnd() % 14u];
  uint8_t *p = mimir_malloc(n); g_ops++;
  if(!p){ note_fail("malloc_null", cyc); return -1; }
  g_slots[s].seed = 0xA5u + (uint32_t)s*7u + (uint32_t)cyc*131u;
  fillp(p, n, g_slots[s].seed); g_slots[s].p=p; g_slots[s].n=n; g_live += n;
 } else if(kind < 9){
  size_t cnt = 1 + rnd()%16u, sz = 16u*(1 + rnd()%64u), n = cnt*sz;
  uint8_t *p = mimir_calloc(cnt, sz); g_ops++;
  if(!p){ note_fail("calloc_null", cyc); return -1; }
  g_checks++; g_bytes_checked += n;
  for(size_t o=0;o<n;o++) if(p[o]!=0){ note_fail("calloc_nonzero", cyc); mimir_free(p); g_ops++; return -1; }
  g_slots[s].seed = 0x5Au + (uint32_t)s*11u + (uint32_t)cyc*17u;
  fillp(p, n, g_slots[s].seed); g_slots[s].p=p; g_slots[s].n=n; g_live += n;
 } else {
  size_t n = 8192u + (rnd()%16u)*8192u;
  uint8_t *p = mimir_malloc(n); g_ops++;
  if(!p){ note_fail("large_malloc_null", cyc); return -1; }
  g_slots[s].seed = 0xC3u + (uint32_t)s*3u + (uint32_t)cyc*29u;
  fillp(p, n, g_slots[s].seed); g_slots[s].p=p; g_slots[s].n=n; g_live += n;
 }
 if(g_live > g_hw) g_hw = g_live;
 return 0;
}
static int run_cycle(int cyc){
 g_rng = 0x5005u ^ (uint32_t)(cyc+1)*2654435761u;
 uint64_t ops0=g_ops, chk0=g_checks; g_hw=0;
 for(int i=0;i<OPS_PER_CYCLE;i++){
  int s = (int)(rnd() % NSLOT);
  if(!g_slots[s].p){ if(slot_alloc(s,cyc)!=0) return -1; continue; }
  uint32_t act = rnd() % 10u;
  if(act < 4){
   if(verifyp(g_slots[s].p, g_slots[s].n, g_slots[s].seed)!=0){ note_fail("pattern_before_free", cyc); return -1; }
   g_live -= g_slots[s].n; mimir_free(g_slots[s].p); g_ops++; g_slots[s].p=NULL; g_slots[s].n=0;
  } else if(act < 7){
   size_t oldn = g_slots[s].n; uint32_t oldseed = g_slots[s].seed;
   if(verifyp(g_slots[s].p, oldn, oldseed)!=0){ note_fail("pattern_before_realloc", cyc); return -1; }
   size_t nn = (rnd()%3u==0) ? 8192u + (rnd()%8u)*8192u : g_small[rnd()%14u];
   uint8_t *np = mimir_realloc(g_slots[s].p, nn); g_ops++;
   if(!np){ note_fail("realloc_null_original_kept", cyc); return -1; }
   size_t keep = oldn<nn?oldn:nn;
   g_checks++; g_bytes_checked += keep;
   for(size_t o=0;o<keep;o++) if(np[o]!=patb(oldseed,o)){ note_fail("realloc_preserve", cyc); return -1; }
   g_live = g_live - oldn + nn;
   g_slots[s].seed = oldseed + 977u; fillp(np, nn, g_slots[s].seed);
   g_slots[s].p=np; g_slots[s].n=nn; if(g_live>g_hw) g_hw=g_live;
  } else {
   if(verifyp(g_slots[s].p, g_slots[s].n, g_slots[s].seed)!=0){ note_fail("pattern_touch", cyc); return -1; }
  }
 }
 /* cycle boundary: full live-set verification, free all, trim */
 for(int s=0;s<NSLOT;s++) if(g_slots[s].p){
  if(verifyp(g_slots[s].p, g_slots[s].n, g_slots[s].seed)!=0){ note_fail("pattern_cycle_end", cyc); return -1; }
  g_live -= g_slots[s].n; mimir_free(g_slots[s].p); g_ops++; g_slots[s].p=NULL; g_slots[s].n=0;
 }
 size_t pre_spans = mimir_core_retained_spans(), pre_bytes = mimir_core_retained_bytes();
 size_t released = mimir_core_trim_retained();
 struct mimir_stats st; memset(&st,0,sizeof st);
 if(mimir_core_stats_snapshot(&st)!=0){ note_fail("stats_snapshot", cyc); return -1; }
 size_t post_spans = mimir_core_retained_spans(), post_bytes = mimir_core_retained_bytes();
 long rss = rss_kb();
 printf("CYCLE %d ops=%llu checks=%llu live_high_water=%llu pre_trim_retained_spans=%zu pre_trim_retained_bytes=%zu trim_released=%zu post_retained_spans=%zu post_retained_bytes=%zu post_cached=%llu post_mapped=%llu live_small=%llu live_mappings=%llu live_spans=%llu peak_mapped=%llu alloc_failures=%llu rss_kb=%ld result=%s\n",
  cyc+1, (unsigned long long)(g_ops-ops0), (unsigned long long)(g_checks-chk0),
  (unsigned long long)g_hw, pre_spans, pre_bytes, released, post_spans, post_bytes,
  (unsigned long long)st.bytes_cached, (unsigned long long)st.bytes_mapped,
  (unsigned long long)st.live_small_objects, (unsigned long long)st.live_mappings,
  (unsigned long long)st.live_spans, (unsigned long long)st.peak_mapped,
  (unsigned long long)st.alloc_failures, rss,
  (post_bytes==0 && st.bytes_cached==0 && st.live_small_objects==0 && st.live_mappings==0 && g_live==0)?"ok":"FAIL");
 if(post_bytes!=0 || st.bytes_cached!=0) { note_fail("retained_after_trim_nonzero", cyc); return -2; }
 if(st.live_small_objects!=0 || st.live_mappings!=0 || g_live!=0){ note_fail("live_not_zero_at_boundary", cyc); return -2; }
 /* stash post values via globals for band logic */
 extern uint64_t g_post_mapped; extern long g_post_rss;
 g_post_mapped = st.bytes_mapped; g_post_rss = rss;
 (void)0;
 return 0;
}
uint64_t g_post_mapped; long g_post_rss;
static int main_leg(void){
 printf("probe_m005 main leg: cycles=%d ops_per_cycle=%d slots=%d pace_s=10 bound: wall<=1200s design, runner timeout 900s\n", CYCLES, OPS_PER_CYCLE, NSLOT);
 printf("observables: mimir_core_stats_snapshot{bytes_cached,bytes_mapped,live_small_objects,live_mappings,live_spans,peak_mapped,alloc_failures} via tests/core/mimir_stats_j002.h; mimir_core_retained_spans/bytes + mimir_core_trim_retained via tests/core/mimir_trim_i003.h; VmRSS via /proc/self/status\n");
 uint64_t base_mapped=0; long base_rss=0; uint64_t page=4096;
 int stopped=0;
 for(int c=0;c<CYCLES;c++){
  int rc = run_cycle(c);
  if(rc!=0){ printf("FINDING main leg stopped at cycle %d rc=%d (state preserved in output above)\n", c+1, rc); stopped=1; break; }
  if(c==0){ struct mimir_stats t; if(mimir_core_stats_snapshot(&t)==0) page=t.page_size_bytes; }
  if(c<2){
   if(g_post_mapped>base_mapped) base_mapped=g_post_mapped;
   if(g_post_rss>base_rss) base_rss=g_post_rss;
   if(c==1) printf("BAND derived from cycles 1-2: post_mapped<=%llu (base %llu + 1 page %llu), post_rss_kb<=%ld (base %ld + 512), post_retained_bytes==0; band fixed, never widened\n",
     (unsigned long long)(base_mapped+page), (unsigned long long)base_mapped, (unsigned long long)page, base_rss+512, base_rss);
  } else {
   if(g_post_mapped > base_mapped + page){ printf("FINDING cycle %d post_mapped=%llu beyond band %llu\n", c+1, (unsigned long long)g_post_mapped, (unsigned long long)(base_mapped+page)); g_fail++; stopped=1; break; }
   if(g_post_rss > base_rss + 512){ printf("FINDING cycle %d rss_kb=%ld beyond band %ld\n", c+1, g_post_rss, base_rss+512); g_fail++; stopped=1; break; }
  }
  printf("CYCLE %d band_verdict=%s\n", c+1, (c<2)?"baseline":"within_band");
  if(c<CYCLES-1){ printf("PACE sleep 10s before cycle %d\n", c+2); fflush(stdout); sleep(10); }
 }
 struct mimir_stats fin; memset(&fin,0,sizeof fin);
 int sok = mimir_core_stats_snapshot(&fin)==0;
 printf("MAIN ACCOUNTING total_ops=%llu total_checks=%llu total_bytes_checked=%llu failures=%d final_live_small=%llu final_live_mappings=%llu final_retained_bytes=%zu alloc_failures=%llu\n",
  (unsigned long long)g_ops, (unsigned long long)g_checks, (unsigned long long)g_bytes_checked, g_fail,
  (unsigned long long)(sok?fin.live_small_objects:0), (unsigned long long)(sok?fin.live_mappings:0),
  mimir_core_retained_bytes(), (unsigned long long)(sok?fin.alloc_failures:0));
 if(sok && fin.alloc_failures!=0) g_fail++;
 int clean = sok && fin.live_small_objects==0 && fin.live_mappings==0 && mimir_core_retained_bytes()==0;
 printf("M005 MAIN RESULT: %s (stopped_early=%d final_state_clean=%d)\n", (g_fail==0&&!stopped&&clean)?"PASS":"FAIL", stopped, clean);
 return (g_fail==0&&!stopped&&clean)?0:1;
}

/* ---------------- dlopen leg ---------------- */
typedef void *(*mf)(size_t); typedef void (*ff)(void*);
typedef void *(*cf)(size_t,size_t); typedef void *(*rf)(void*,size_t);
static int maps_stats(int *img_lines, long *anon_rw){
 FILE *f=fopen("/proc/self/maps","r"); char ln[1024];
 int img=0; long anon=0;
 if(!f) return -1;
 while(fgets(ln,sizeof ln,f)){
  unsigned long a,b; char perms[8]; char path[512]; int n=0;
  path[0]='\0';
  int got=sscanf(ln,"%lx-%lx %7s %*s %*s %*s %n",&a,&b,perms,&n);
  if(got<3) continue;
  if(n>0 && ln[n]) sscanf(ln+n,"%511s",path);
  if(strstr(ln,"libmimiralloc.so")) img++;
  else if(perms[0]=='r'&&perms[1]=='w'&&path[0]=='\0') anon += (long)(b-a);
 }
 fclose(f); *img_lines=img; *anon_rw=anon; return 0;
}
static int dl_leg(const char *so){
 printf("probe_m005 dl leg: cycles=6 so=%s (H-001 shape: post-close anon growth must be 0 B/cycle, image lines 0)\n", so);
 long base_anon=-1; int fails=0; uint64_t ops=0, checks=0;
 for(int c=0;c<6;c++){
  void *h=dlopen(so, RTLD_NOW|RTLD_LOCAL);
  if(!h){ printf("DL CYCLE %d FAIL dlopen: %s\n", c+1, dlerror()); return 1; }
  mf dm=(mf)dlsym(h,"mimir_malloc"); ff df=(ff)dlsym(h,"mimir_free");
  cf dc=(cf)dlsym(h,"mimir_calloc"); rf dr=(rf)dlsym(h,"mimir_realloc");
  if(!dm||!df||!dc||!dr){ printf("DL CYCLE %d FAIL dlsym\n", c+1); dlclose(h); return 1; }
  uint8_t *blk[41]; size_t bn[41]; int nb=0, bad=0;
  for(int i=0;i<32;i++){ blk[nb]=dm(256); if(!blk[nb]){bad=1;break;} bn[nb]=256; nb++; }
  for(int i=0;i<8 && !bad;i++){ blk[nb]=dm(1024); if(!blk[nb]){bad=1;break;} bn[nb]=1024; nb++; }
  if(!bad){ blk[nb]=dc(1,8192); if(!blk[nb]) bad=1; else { bn[nb]=8192; nb++; } }
  ops += (uint64_t)nb;
  for(int i=0;i<nb;i++){ for(size_t o=0;o<bn[i];o++) blk[i][o]=(uint8_t)(i+o); }
  if(!bad && nb>0){ uint8_t *np=dr(blk[0],512); ops++; if(!np) bad=1; else { blk[0]=np; bn[0]=512; for(size_t o=0;o<256;o++) if(blk[0][o]!=(uint8_t)o) bad=1; } }
  for(int i=0;i<nb;i++){ checks++; for(size_t o=0;o<bn[i];o++) if(blk[i][o]!=(uint8_t)(i+o) && !(i==0&&o>=256)) { if(i==0&&o<256) bad=1; else if(i!=0) bad=1; } }
  for(int i=0;i<nb;i++) df(blk[i]);
  ops += (uint64_t)nb;
  if(dlclose(h)!=0){ printf("DL CYCLE %d FAIL dlclose\n", c+1); return 1; }
  int img=-1; long anon=-1;
  if(maps_stats(&img,&anon)!=0){ printf("DL CYCLE %d FAIL maps\n", c+1); return 1; }
  if(c==0) base_anon=anon;
  long growth = anon - base_anon;
  int ok = !bad && img==0 && growth==0;
  if(!ok) fails++;
  printf("DL CYCLE %d ops_cycle=%d img_lines_after_close=%d anon_rw_bytes_after_close=%ld anon_growth_vs_cycle0=%ld rss_kb=%ld result=%s\n",
   c+1, nb*2+1, img, anon, growth, rss_kb(), ok?"ok":"FAIL");
 }
 printf("DL ACCOUNTING total_ops=%llu total_checks=%llu failures=%d library_unloaded_after_each_cycle=1\n",
  (unsigned long long)ops, (unsigned long long)checks, fails);
 printf("M005 DL RESULT: %s\n", fails==0?"PASS":"FAIL");
 return fails==0?0:1;
}
int main(int argc, char **argv){
 if(argc>=2 && strcmp(argv[1],"main")==0) return main_leg();
 if(argc>=3 && strcmp(argv[1],"dl")==0) return dl_leg(argv[2]);
 printf("usage: probe_m005 main | dl <libmimiralloc.so>\n");
 return 2;
}
