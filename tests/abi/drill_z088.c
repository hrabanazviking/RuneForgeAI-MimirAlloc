/* drill_z088.c - MimirAlloc Z-088: failure-drill victim.
 *
 * Built as an ordinary glibc consumer: it references only
 * standard malloc-family names and links against NO
 * MimirAlloc artifact (the only extra link is -ldl, used
 * to resolve the public statistics entry point at run
 * time). It is run by tests/abi/run_z088.sh two ways:
 *   plain:      ./build/drill_z088 <mode>
 *   preloaded:  LD_PRELOAD=build/libmimiralloc_interpose.so
 *               ./build/drill_z088 <mode>
 * Under preload the interposing variant (Z-082) serves
 * every malloc-family call; the victim proves that per
 * run with the D-013 probe (malloc(0) returns NULL only
 * in the MIMIR world) and prints PRELOAD-VERDICT: MIMIR
 * or GLIBC as its first verdict line. A drill whose
 * verdict is not MIMIR proves nothing and the runner
 * fails it.
 *
 * Each mode performs ONE drill inside this process and
 * prints only facts: the diag lines are emitted by the
 * allocator itself to stderr (fd 2, the J-007 channel,
 * gated by MIMIRALLOC_DIAG=1), the counters are read
 * back through the public statistics snapshot
 * (mimir_stats_snapshot, resolved with dlsym; the struct
 * below is a verbatim copy of the public v1 layout in
 * include/mimir_alloc.h and every run self-validates it:
 * the snapshot's struct_size must equal this copy's
 * sizeof), and the DRILL-RESULT line states only that
 * the victim survived its own script. Classification
 * (LOUD through the diag channel, or SILENT with the
 * consistency evidence) is the runner's job, from these
 * facts -- the victim asserts no allocator behaviour.
 *
 * SAFETY LAW for this slice (recorded in the team's
 * CAMPAIGN_STATE.md): drills run only inside processes
 * the drill runner itself spawns from this worktree;
 * no fault is injected into any running user or system
 * process; the kill drills signal only PIDs the runner
 * itself spawned for the drill. Every fault below is
 * committed by this victim against its OWN heap, in a
 * process that exists only to be drilled.
 *
 * Modes:
 *   control          no fault: 1000x malloc(64)/free
 *                    churn; the harness-validity leg.
 *   doublefree       free one small block twice (the
 *                    TLS-bin course of Z-070).
 *   doublefree-calloc  same, on a calloc block.
 *   poison           free a block, then write through
 *                    the dangling pointer into the
 *                    freed-block poison range [8,40),
 *                    then allocate the class again
 *                    (Z-070 poison course).
 *   overflow         locate an adjacent pair of live
 *                    blocks at run time, free the upper
 *                    one, write from the lower block
 *                    past its usable end into the freed
 *                    neighbour's poison range, then
 *                    allocate the class again. Exits 4
 *                    (INCONCLUSIVE) if no adjacent pair
 *                    exists in this process -- the
 *                    layout fact is printed either way.
 *   badfree-int      free an interior pointer (p+16),
 *                    then free p properly, then a
 *                    256-block pattern-verified churn:
 *                    the consistency proof for whatever
 *                    the interior free did.
 *   badfree-foreign  free a pointer this allocator
 *                    never issued (a static buffer),
 *                    then the same churn proof.
 *   badfree-large    free one large (mapping-path)
 *                    block twice, then the churn proof
 *                    (double-free detection is small-
 *                    path only by Z-070's design; this
 *                    drill measures the large course).
 *   huge             three unsatisfiable requests:
 *                    malloc(2^62), calloc(2^32, 2^32),
 *                    reallocarray(NULL, 2^33, 2^33) --
 *                    the failure-NULL course; sizes are
 *                    held in volatile variables so the
 *                    compiler cannot fold the requests
 *                    away or refuse them at build time.
 *   churn            unbounded allocation churn with
 *                    HEARTBEAT lines; exists only to be
 *                    killed by the runner (SIGKILL and
 *                    SIGTERM legs). Never returns.
 *
 * Compiled -fno-builtin (Makefile Z-088 block) so no
 * builtin malloc-family assumption can rewrite the
 * drill calls. The dangling-pointer writes in the
 * poison/overflow modes go through volatile pointers
 * so the optimiser cannot delete the fault itself.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

size_t malloc_usable_size(void *ptr);

/* Verbatim copy of struct mimir_stats_v1 (public v1
 * layout, include/mimir_alloc.h, Z-076). Self-validated
 * per run via the struct_size field. */
struct stats_v1 {
    uint32_t struct_version;
    uint32_t struct_size;
    uint64_t bytes_requested;
    uint64_t bytes_allocated;
    uint64_t bytes_cached;
    uint64_t bytes_mapped;
    uint64_t bytes_released;
    uint64_t peak_mapped;
    uint64_t alloc_failures;
    uint64_t alloc_posture_nulls;
    uint64_t cross_thread_frees;
    uint64_t lock_contention;
    uint64_t lock_contention_measured;
    uint64_t live_mappings;
    uint64_t live_spans;
    uint64_t live_small_objects;
    uint64_t live_mapping_bytes;
    uint64_t live_span_bytes;
    uint64_t page_size_bytes;
    uint64_t span_class_size[20];
    uint64_t span_live_objects[20];
    uint64_t span_total_spans[20];
    uint64_t span_empty_spans[20];
    uint64_t span_capacity_objects[20];
    uint64_t unlock_failures;
    uint64_t drain_lock_failures;
    uint64_t trim_lock_failures;
    uint64_t free_lock_failures;
    uint64_t metadata_detections;
    uint64_t metadata_collateral_refusals;
    uint64_t live_release_detections;
    uint64_t live_release_collateral_refusals;
    uint64_t double_free_detections;
    uint64_t poison_detections;
    uint64_t cross_thread_small_frees;
    uint64_t small_frees_unrecorded;
    uint64_t live_requested_bytes;
    uint64_t live_requested_measured;
};

typedef int (*snap_fn)(struct stats_v1 *out);
static snap_fn g_snap;

static void print_stats(const char *tag)
{
    struct stats_v1 s;
    memset(&s, 0, sizeof s);
    if (g_snap == NULL) {
        printf("STATS %s unavailable\n", tag);
        fflush(stdout);
        return;
    }
    int rc = g_snap(&s);
    printf("STATS %s rc=%d size=%u sizeof=%zu df=%llu poison=%llu "
           "meta=%llu live_small=%llu live_map=%llu fail=%llu "
           "posture=%llu\n",
           tag, rc, s.struct_size, sizeof(struct stats_v1),
           (unsigned long long)s.double_free_detections,
           (unsigned long long)s.poison_detections,
           (unsigned long long)s.metadata_detections,
           (unsigned long long)s.live_small_objects,
           (unsigned long long)s.live_mappings,
           (unsigned long long)s.alloc_failures,
           (unsigned long long)s.alloc_posture_nulls);
    fflush(stdout);
}

static void verdict(void)
{
    void *z = malloc(0);
    if (z == NULL) {
        printf("PRELOAD-VERDICT: MIMIR\n");
    } else {
        printf("PRELOAD-VERDICT: GLIBC\n");
        free(z);
    }
    fflush(stdout);
}

/* 256-block pattern-verified churn: allocate mixed
 * sizes, stamp each with a per-block byte, verify every
 * byte, free all. Returns 0 iff every pattern is intact
 * -- the post-drill consistency proof for the silent
 * drills. */
static int churn_verify(void)
{
    void *ptrs[256];
    for (int i = 0; i < 256; i++) {
        size_t sz = 16 + (size_t)((i * 53) % 1024);
        ptrs[i] = malloc(sz);
        if (ptrs[i] == NULL) {
            printf("churn alloc fail %d\n", i);
            fflush(stdout);
            return 1;
        }
        memset(ptrs[i], (i & 0x3F) + 1, sz < 64 ? sz : 64);
    }
    for (int i = 0; i < 256; i++) {
        size_t sz = 16 + (size_t)((i * 53) % 1024);
        unsigned char want = (unsigned char)((i & 0x3F) + 1);
        unsigned char *b = (unsigned char *)ptrs[i];
        size_t n = sz < 64 ? sz : 64;
        for (size_t j = 0; j < n; j++) {
            if (b[j] != want) {
                printf("churn pattern mismatch blk %d byte %zu\n", i, j);
                fflush(stdout);
                return 2;
            }
        }
    }
    for (int i = 0; i < 256; i++) {
        free(ptrs[i]);
    }
    printf("churn verify OK (256 blocks, patterns intact, all freed)\n");
    fflush(stdout);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: drill_z088 <mode>\n");
        return 2;
    }
    const char *mode = argv[1];
    g_snap = (snap_fn)dlsym(RTLD_DEFAULT, "mimir_stats_snapshot");
    printf("PID %ld mode=%s snap=%s\n", (long)getpid(), mode,
           g_snap ? "found" : "absent");
    fflush(stdout);
    verdict();
    print_stats("before");

    if (strcmp(mode, "control") == 0) {
        for (int i = 0; i < 1000; i++) {
            void *p = malloc(64);
            if (p == NULL) {
                printf("CONTROL alloc fail at %d\n", i);
                fflush(stdout);
                return 3;
            }
            memset(p, 0xAB, 64);
            free(p);
        }
        print_stats("after");
        printf("DRILL-RESULT control SURVIVED\n");
        return 0;
    }

    if (strcmp(mode, "doublefree") == 0) {
        void *p = malloc(64);
        memset(p, 0x11, 64);
        free(p);
        printf("first free done\n");
        fflush(stdout);
        free(p);
        printf("second free returned\n");
        fflush(stdout);
        print_stats("after");
        void *q = malloc(64);
        printf("post malloc %s\n", q ? "ok" : "NULL");
        fflush(stdout);
        if (q) {
            memset(q, 0x22, 64);
            free(q);
        }
        printf("DRILL-RESULT doublefree SURVIVED\n");
        return 0;
    }

    if (strcmp(mode, "doublefree-calloc") == 0) {
        void *p = calloc(1, 96);
        free(p);
        printf("first free done\n");
        fflush(stdout);
        free(p);
        printf("second free returned\n");
        fflush(stdout);
        print_stats("after");
        printf("DRILL-RESULT doublefree-calloc SURVIVED\n");
        return 0;
    }

    if (strcmp(mode, "poison") == 0) {
        void *p = malloc(64);
        memset(p, 0x33, 64);
        free(p);
        printf("freed p=%p\n", p);
        fflush(stdout);
        volatile unsigned char *v = (volatile unsigned char *)p;
        for (int i = 8; i < 40; i++) {
            v[i] = 0xEE;
        }
        printf("uaf write done (bytes [8,40) of the freed block)\n");
        fflush(stdout);
        void *q = malloc(64);
        printf("reuse q=%p same=%d\n", q, q == p);
        fflush(stdout);
        print_stats("after");
        if (q) {
            memset(q, 0x44, 64);
            free(q);
        }
        printf("DRILL-RESULT poison SURVIVED\n");
        return 0;
    }

    if (strcmp(mode, "overflow") == 0) {
        void *arr[8];
        for (int i = 0; i < 8; i++) {
            arr[i] = malloc(64);
            memset(arr[i], 0x50 + i, 64);
        }
        size_t usable = malloc_usable_size(arr[0]);
        int lo = -1;
        for (int i = 0; i < 7; i++) {
            printf("pair %d: p=%p next=%p delta=%td usable=%zu\n",
                   i, arr[i], arr[i + 1],
                   (ptrdiff_t)((char *)arr[i + 1] - (char *)arr[i]),
                   usable);
            if (lo < 0 &&
                (char *)arr[i + 1] == (char *)arr[i] + (ptrdiff_t)usable) {
                lo = i;
            }
        }
        printf("adjacent_lo=%d\n", lo);
        fflush(stdout);
        if (lo < 0) {
            printf("DRILL-RESULT overflow INCONCLUSIVE "
                   "(no adjacent pair in this process)\n");
            fflush(stdout);
            return 4;
        }
        free(arr[lo + 1]);
        printf("freed neighbour arr[%d]=%p\n", lo + 1, arr[lo + 1]);
        fflush(stdout);
        volatile unsigned char *v = (volatile unsigned char *)arr[lo];
        for (size_t i = usable + 8; i < usable + 24; i++) {
            v[i] = 0xDD;
        }
        printf("overflow write done (neighbour bytes [8,24), "
               "written from the live block past its usable end)\n");
        fflush(stdout);
        void *q = malloc(64);
        printf("reuse q=%p neighbour=%d\n", q, q == arr[lo + 1]);
        fflush(stdout);
        print_stats("after");
        for (int i = 0; i < 8; i++) {
            if (i != lo + 1) {
                free(arr[i]);
            }
        }
        if (q && q != arr[lo + 1]) {
            free(q);
        }
        printf("DRILL-RESULT overflow SURVIVED\n");
        return 0;
    }

    if (strcmp(mode, "badfree-int") == 0) {
        void *p = malloc(128);
        memset(p, 0x55, 128);
        print_stats("after-alloc");
        printf("freeing interior p+16 of p=%p\n", p);
        fflush(stdout);
        free((char *)p + 16);
        printf("interior free returned\n");
        fflush(stdout);
        print_stats("after-interior");
        free(p);
        printf("proper free of original returned\n");
        fflush(stdout);
        print_stats("after-proper");
        int rc = churn_verify();
        printf("DRILL-RESULT badfree-int churn_rc=%d SURVIVED\n", rc);
        return 0;
    }

    if (strcmp(mode, "badfree-foreign") == 0) {
        static char buf[256];
        memset(buf, 0x77, sizeof buf);
        printf("freeing static buffer %p (never issued by any "
               "allocator in this process)\n", (void *)buf);
        fflush(stdout);
        free((void *)buf);
        printf("foreign free returned\n");
        fflush(stdout);
        print_stats("after-foreign");
        int rc = churn_verify();
        printf("DRILL-RESULT badfree-foreign churn_rc=%d SURVIVED\n", rc);
        return 0;
    }

    if (strcmp(mode, "badfree-large") == 0) {
        void *p = malloc(20000);
        memset(p, 0x66, 20000);
        print_stats("after-alloc");
        free(p);
        printf("first large free done\n");
        fflush(stdout);
        print_stats("after-free1");
        free(p);
        printf("second large free returned\n");
        fflush(stdout);
        print_stats("after-free2");
        int rc = churn_verify();
        printf("DRILL-RESULT badfree-large churn_rc=%d SURVIVED\n", rc);
        return 0;
    }

    if (strcmp(mode, "huge") == 0) {
        volatile size_t huge_bytes = (size_t)1 << 62;
        volatile size_t f32 = (size_t)1 << 32;
        volatile size_t f33 = (size_t)1 << 33;
        void *a = malloc(huge_bytes);
        printf("malloc(2^62)=%p\n", a);
        fflush(stdout);
        if (a) {
            free(a);
        }
        void *b = calloc(f32, f32);
        printf("calloc(2^32,2^32)=%p\n", b);
        fflush(stdout);
        if (b) {
            free(b);
        }
        void *c = reallocarray(NULL, f33, f33);
        printf("reallocarray(2^33,2^33)=%p\n", c);
        fflush(stdout);
        if (c) {
            free(c);
        }
        print_stats("after");
        void *q = malloc(64);
        printf("post malloc %s\n", q ? "ok" : "NULL");
        fflush(stdout);
        if (q) {
            free(q);
        }
        printf("DRILL-RESULT huge SURVIVED\n");
        return 0;
    }

    if (strcmp(mode, "churn") == 0) {
        printf("CHURN-READY pid=%ld\n", (long)getpid());
        fflush(stdout);
        unsigned long n = 0;
        void *ring[64];
        for (int i = 0; i < 64; i++) {
            ring[i] = NULL;
        }
        for (;;) {
            for (int i = 0; i < 64; i++) {
                free(ring[i]);
                ring[i] = malloc(32 + (size_t)((i * 37) % 512));
                if (ring[i]) {
                    memset(ring[i], (int)(n & 0xFF), 32);
                }
            }
            n++;
            if (n % 2000 == 0) {
                printf("HEARTBEAT %lu\n", n);
                fflush(stdout);
            }
        }
        return 0;
    }

    fprintf(stderr, "unknown mode %s\n", mode);
    return 2;
}
