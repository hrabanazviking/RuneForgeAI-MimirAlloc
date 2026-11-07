/* probe_l008.c - L-008 Phase 12 tuning evaluation harness.
 * Candidate: retention_bound, the ONLY runtime tuning knob
 * (G-007: default 2, range 0..16, internal setter). Legs:
 * bound 2 (banked default) vs 0 vs 16, identical workloads.
 * W1 = the L-006 phase shape, exact sizes: KV 32x45056 held,
 * prefill ids 128 (the one small object) + embed 131072 +
 * ws 4x262144, decode 32 steps (KV slice held + scratch
 * 8192 + logits 128000 churned per step), teardown frees all.
 * W2 = small churn sensitivity leg shaped on L-004/L-005
 * small objects (64B records, 128B ids): batches alloc/free.
 * W2 is NOT a Phase 12 profile shape; it exists only to give
 * the retention knob its best case, labelled as such.
 * Timing: clock_gettime MONOTONIC per iteration, 1 warm-up
 * + 5 timed iterations per setting, medians printed.
 * Gates: deterministic only (setter/getter, alloc success,
 * teardown live counters return to 0). Times are evidence. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "mimir_alloc.h"
#include "../core/mimir_stats_j002.h"
#include "../core/mimir_tune_g007.h"
static int g_fail;
static void gate(int ok, const char *n){ printf("GATE %s: %s\n", n, ok?"ok":"FAIL"); if(!ok) g_fail++; }
static uint64_t now_ns(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return (uint64_t)ts.tv_sec*1000000000ull+(uint64_t)ts.tv_nsec; }
static void touch(uint8_t *p, uint64_t n){ for(uint64_t o=0;o<n;o+=4096) p[o]=(uint8_t)(o>>12); if(n) p[n-1]=1; }
static int w1(void){
 void *kv[64]; int nk=0;
 for(int i=0;i<32;i++){ kv[nk]=mimir_malloc(45056); if(!kv[nk]) return -1; touch(kv[nk],45056); nk++; }
 uint32_t *ids=mimir_malloc(128); if(!ids) return -1; for(int i=0;i<32;i++) ids[i]=(uint32_t)i;
 uint8_t *emb=mimir_malloc(131072); if(!emb) return -1; touch(emb,131072);
 uint8_t *ws[4]; for(int i=0;i<4;i++){ ws[i]=mimir_malloc(262144); if(!ws[i]) return -1; touch(ws[i],262144); }
 for(int s=0;s<32;s++){
  kv[nk]=mimir_malloc(45056); if(!kv[nk]) return -1; touch(kv[nk],45056); nk++;
  uint8_t *sc=mimir_malloc(8192); if(!sc) return -1; touch(sc,8192);
  uint8_t *lg=mimir_malloc(128000); if(!lg) return -1; touch(lg,4096);
  mimir_free(lg); mimir_free(sc);
 }
 for(int i=0;i<4;i++) mimir_free(ws[i]);
 mimir_free(emb); mimir_free(ids);
 for(int i=0;i<nk;i++) mimir_free(kv[i]);
 return 0;
}
static int w2(void){
 void *b[96];
 for(int r=0;r<200;r++){
  int n=0;
  for(int i=0;i<64;i++){ b[n]=mimir_malloc(64); if(!b[n]) return -1; ((uint8_t*)b[n])[0]=1; n++; }
  for(int i=0;i<32;i++){ b[n]=mimir_malloc(128); if(!b[n]) return -1; ((uint8_t*)b[n])[0]=1; n++; }
  for(int i=n-1;i>=0;i--) mimir_free(b[i]);
 }
 return 0;
}
static int cmp_u64(const void *a, const void *b){ uint64_t x=*(const uint64_t*)a, y=*(const uint64_t*)b; return (x>y)-(x<y); }
int main(int argc, char **argv){
 if(argc!=2){ printf("usage: probe_l008 <retention_bound>\n"); return 2; }
 size_t bd=(size_t)strtoul(argv[1],NULL,10);
 printf("probe_l008 bound=%zu (one fresh process per leg)\n", bd);
 gate(mimir_tune_set_retention_bound(bd)==0, "set_retention");
 gate(mimir_tune_get_retention_bound()==bd, "get_retention_matches");
 gate(w1()==0, "w1_warmup_allocs");
 gate(w2()==0, "w2_warmup_allocs");
 uint64_t t1[7], t2[7];
 for(int i=0;i<7;i++){ uint64_t a=now_ns(); if(w1()!=0){ gate(0,"w1_iter"); break; } t1[i]=now_ns()-a; }
 for(int i=0;i<7;i++){ uint64_t a=now_ns(); if(w2()!=0){ gate(0,"w2_iter"); break; } t2[i]=now_ns()-a; }
 uint64_t s1[7], s2[7]; memcpy(s1,t1,sizeof s1); memcpy(s2,t2,sizeof s2); qsort(s1,7,sizeof s1[0],cmp_u64); qsort(s2,7,sizeof s2[0],cmp_u64);
 printf("BOUND %zu W1 median_ns=%llu iters", bd,(unsigned long long)s1[3]); for(int i=0;i<7;i++) printf(" %llu",(unsigned long long)s1[i]); printf("\n");
 printf("BOUND %zu W2 median_ns=%llu iters", bd,(unsigned long long)s2[3]); for(int i=0;i<7;i++) printf(" %llu",(unsigned long long)s2[i]); printf("\n");
 struct mimir_stats st; memset(&st,0,sizeof st);
 gate(mimir_core_stats_snapshot(&st)==0, "stats_snapshot");
 uint64_t empty=0; for(int c=0;c<20;c++) empty+=st.span_empty_spans[c];
 printf("BOUND %zu after-teardown: live_small=%llu live_mappings=%llu live_span_bytes=%llu empty_spans=%llu bytes_mapped=%llu bytes_cached=%llu\n", bd,(unsigned long long)st.live_small_objects,(unsigned long long)st.live_mappings,(unsigned long long)st.live_span_bytes,(unsigned long long)empty,(unsigned long long)st.bytes_mapped,(unsigned long long)st.bytes_cached);
 gate(st.live_small_objects==0 && st.live_mappings==0, "teardown_live_zero");
 printf("L008 RESULT: %s (failures=%d)\n", g_fail==0?"PASS":"FAIL", g_fail);
 return g_fail==0?0:1;
}
