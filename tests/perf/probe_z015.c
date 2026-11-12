/* probe_z015.c - Z-015 Syscall and VMA economics (MimirAlloc Z13).
 * mmap/munmap/mremap cost ladder + VMA split/merge counts under
 * churn, sampled from /proc/self/maps. Method per
 * docs/Z13_MEASUREMENT_PROTOCOL.md: pinned by runner (taskset CPU2),
 * clock_gettime(CLOCK_MONOTONIC) per op, per-op samples sorted,
 * median + min reported. Untouched vs touched legs separate fault
 * cost from syscall cost. VMA experiments use one large mapping
 * (partial unmap = split, edge unmap = shrink) plus a gapped churn
 * pattern that cannot merge, so counts are deterministic.
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static long long now_ns(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return (long long)ts.tv_sec*1000000000LL+ts.tv_nsec;}
static int cmp_ll(const void *a,const void *b){long long x=*(const long long*)a,y=*(const long long*)b;return (x>y)-(x<y);}
static long long med(long long *v,int n){qsort(v,(size_t)n,sizeof(long long),cmp_ll);return v[n/2];}
static long long mn(long long *v,int n){long long m=v[0];for(int i=1;i<n;i++)if(v[i]<m)m=v[i];return m;}
static void *xmap(size_t len){void *p=mmap(NULL,len,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(p==MAP_FAILED){perror("mmap");exit(1);}return p;}
static int count_vmas(void){FILE *f=fopen("/proc/self/maps","r");if(!f)return -1;int n=0,c,bol=1;while((c=fgetc(f))!=EOF){if(bol&&c!='\n')n++;bol=(c=='\n');}fclose(f);return n;}

static long long s_mm[256],s_mu[256],s_tot[256],s_mu2[256];
int main(void){
 long ps=sysconf(_SC_PAGESIZE);
 printf("pagesize=%ld max_map_count via runner header\n",ps);
 static const size_t SZ[]={4096,16384,65536,262144,1048576,4194304,16777216,67108864};
 printf("=== LADDER untouched: size mmap_med mmap_min munmap_med munmap_min ns ===\n");
 for(int s=0;s<8;s++){size_t len=SZ[s];int reps=len>=67108864?15:(len>=4194304?31:101);
  for(int i=0;i<reps;i++){long long t0=now_ns();void *p=xmap(len);long long t1=now_ns();if(munmap(p,len)!=0){perror("munmap");return 1;}long long t2=now_ns();s_mm[i]=t1-t0;s_mu[i]=t2-t1;}
  long long a[256],b[256];memcpy(a,s_mm,sizeof a);memcpy(b,s_mu,sizeof b);
  printf("LAD %zu %lld %lld %lld %lld reps=%d\n",len,med(a,reps),mn(a,reps),med(b,reps),mn(b,reps),reps);}
 printf("=== LADDER touched (fault every page): size total_med total_min munmap_med ns ===\n");
 for(int s=0;s<7;s++){size_t len=SZ[s];int reps=len>=16777216?15:31;volatile unsigned char sink=0;
  for(int i=0;i<reps;i++){long long t0=now_ns();void *p=xmap(len);unsigned char *c=(unsigned char*)p;for(size_t o=0;o<len;o+=(size_t)ps)c[o]=(unsigned char)(o>>12);sink+=c[len-(size_t)ps];long long t1=now_ns();if(munmap(p,len)!=0)return 1;long long t2=now_ns();s_tot[i]=t1-t0;s_mu2[i]=t2-t1;}
  long long a[256],b[256];memcpy(a,s_tot,sizeof a);memcpy(b,s_mu2,sizeof b);
  printf("TOU %zu %lld %lld %lld reps=%d sink=%u\n",len,med(a,reps),mn(a,reps),med(b,reps),reps,(unsigned)sink);}
 printf("=== MREMAP grow/shrink: old new med min ns reps ===\n");
 static const size_t G[][2]={{1048576,4194304},{4194304,16777216},{16777216,67108864},{67108864,16777216},{16777216,4194304}};
 for(int g=0;g<5;g++){size_t ol=G[g][0],nl=G[g][1];int reps=nl>=67108864||ol>=67108864?15:31;
  for(int i=0;i<reps;i++){void *p=xmap(ol);long long t0=now_ns();void *q=mremap(p,ol,nl,MREMAP_MAYMOVE);long long t1=now_ns();if(q==MAP_FAILED){perror("mremap");return 1;}s_mm[i]=t1-t0;if(munmap(q,nl)!=0)return 1;}
  long long a[256];memcpy(a,s_mm,sizeof a);printf("REM %zu %zu %lld %lld reps=%d\n",ol,nl,med(a,reps),mn(a,reps),reps);}
 printf("=== VMA experiments (counts from /proc/self/maps) ===\n");
 int base=count_vmas();printf("VMA base %d 0\n",base);
 size_t big=16777216;unsigned char *p=(unsigned char*)xmap(big);
 int after_alloc=count_vmas();printf("VMA alloc16M %d %+d\n",after_alloc,after_alloc-base);
 if(munmap(p+4194304,4194304)!=0){return 1;} int after_hole=count_vmas();
 printf("VMA hole_punch_4M_at_4M %d %+d (split: one VMA becomes two)\n",after_hole,after_hole-after_alloc);
 if(munmap(p,4194304)!=0){return 1;} int after_head=count_vmas();
 printf("VMA head_unmap_4M %d %+d (edge shrink, no split)\n",after_head,after_head-after_hole);
 if(munmap(p+8388608,8388608)!=0){return 1;} int after_all=count_vmas();
 printf("VMA unmap_rest %d %+d (back to base expected)\n",after_all,after_all-base);
 void *a1=xmap(4194304);int n1=count_vmas();
 void *hint=(void*)((unsigned char*)a1+4194304);
 void *a2=mmap(hint,4194304,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 int n2=count_vmas();
 if(a2==hint)printf("VMA adjacent_hint %d %+d (0 = merged with previous, +1 = separate)\n",n2,n2-n1);
 else printf("VMA adjacent_hint NOT_PLACED  %d %+d (kernel placed elsewhere)\n",n2,n2-n1);
 if(a2!=MAP_FAILED)munmap(a2,4194304);
 munmap(a1,4194304);
 int after_adj=count_vmas();printf("VMA after_adj_cleanup %d %+d (back to base expected)\n",after_adj,after_adj-base);
 printf("=== VMA churn: gapped allocations cannot merge ===\n");
 enum {CH=48};
 static void *chp[CH];
 for(int i=0;i<CH;i++){unsigned char *q=(unsigned char*)xmap(131072);if(munmap(q+65536,65536)!=0)return 1;chp[i]=q;}
 int nch=count_vmas();printf("VMA gapped_churn_48x64K %d %+d (expect +48: each survivor isolated by a 64K hole)\n",nch,nch-base);
 for(int i=0;i<CH;i++)munmap(chp[i],65536);
 int nend=count_vmas();printf("VMA after_churn_free %d %+d (back to base expected)\n",nend,nend-base);
 printf("=== VMA churn: hole punches split one VMA repeatedly ===\n");
 unsigned char *big2=(unsigned char*)xmap(8388608);
 int nb0=count_vmas();
 for(int k=0;k<16;k++){if(munmap(big2+65536+k*131072,65536)!=0)return 1;}
 int nb1=count_vmas();printf("VMA punch16_in_8M before=%d after=%d delta=%+d (each interior hole splits a survivor: 1 VMA becomes 17)\n",nb0,nb1,nb1-nb0);
 if(munmap(big2,8388608)!=0)return 1;
 int nb2=count_vmas();printf("VMA after_big2_full_unmap %d %+d (full-range unmap removes all pieces; holes are a no-op)\n",nb2,nb2-base);
 printf("=== done ===\n");
 return 0;
}
