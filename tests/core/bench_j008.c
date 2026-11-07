/* bench_j008.c - J-008 overhead bench (timing is a
 * measurement, never an oracle). Shape follows the banked
 * J-004/J-005 micro-bench: 20000 x malloc(64)/memset/free,
 * plus 2000 x malloc(4096)/free for the large path.
 * argv[1] = "stats": also take one locked J-002 snapshot
 * per 1000 small ops (the diagnostics cost at this base).
 * One run per process; the recorded run drives 5 reps per
 * flavour and reports every rep plus the median. */
#define _POSIX_C_SOURCE 200809L
#include "mimir_alloc.h"
#include "mimir_stats_j002.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static long long now_ns(void){struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (long long)t.tv_sec*1000000000LL+t.tv_nsec;}
int main(int argc,char**argv){
  int stats=(argc>1 && strcmp(argv[1],"stats")==0);
  struct mimir_stats st;
  long long t0=now_ns();
  for(int i=0;i<20000;i++){void *p=mimir_malloc(64); if(!p) return 2; memset(p,1,64); if(stats && i%1000==0){ if(mimir_core_stats_snapshot(&st)!=0) return 3; } mimir_free(p);}
  long long t1=now_ns();
  for(int i=0;i<2000;i++){void *p=mimir_malloc(4096); if(!p) return 2; memset(p,1,4096); mimir_free(p);}
  long long t2=now_ns();
  printf("small64%s: %lld ns/op (%lld ns / 20000 ops)\n", stats?"+snapshot/1000":"", (t1-t0)/20000, t1-t0);
  printf("large4096: %lld ns/op (%lld ns / 2000 ops)\n", (t2-t1)/2000, t2-t1);
  return 0;
}
