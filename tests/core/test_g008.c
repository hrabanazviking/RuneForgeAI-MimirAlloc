/*
 * test_g008.c -- evidence test for slice G-008 (Phase 7
 * repeated load/unload + multitasking responsiveness
 * record; slice record:
 * docs/PHASE_7_LOAD_UNLOAD_RESPONSIVENESS_G008.md).
 *
 * Run standalone via `make test-g008` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-g003..test-g007 pattern).
 *
 * This harness deliberately does NOT link the library
 * objects. It consumes libmimiralloc.so exactly as an
 * outside consumer does: dlopen / dlsym / dlclose of the
 * four public symbols only (nm -D of the .so is exactly
 * 4; the E-005/G-003 audit counters are hidden in the
 * .so and CANNOT be read through it -- that fact is
 * itself recorded as INC-G008-01, and counter-level
 * carry-over is therefore established by the mapping
 * evidence below plus the recorded src/ inspection,
 * never assumed).
 *
 * MODE 1 (default): load/unload record.
 *   50 bounded cycles of dlopen(RTLD_NOW|RTLD_LOCAL) ->
 *   dlsym x4 -> synthetic workload (32 x 256 B + 8 x
 *   1024 B small-class blocks + 1 x 8192 B large block,
 *   fill + verify + free all) -> dlclose. Per cycle the
 *   harness records: open/sym/close success, the
 *   mimir_malloc address (mapping-base stability), and,
 *   after dlclose, the number of libmimiralloc.so image
 *   mappings still present in /proc/self/maps, the
 *   total anonymous rw bytes mapped, and VmRSS.
 *   Questions answered by measurement, not assumption:
 *   (a) does the library image fully unmap on dlclose?
 *   (b) what allocator-owned state survives dlclose?
 *   The G-003 span path mmaps its spans separately from
 *   the library image, and src/ registers no destructor
 *   (recorded grep in the run file), so spans retained
 *   at dlclose time (G-005 RP-2 bound, INC-G005-03) are
 *   expected to stay mapped as orphans until process
 *   exit; the per-cycle anonymous-bytes series measures
 *   exactly how much, per cycle.
 *   ORPHAN PROBE (after the 50 cycles): one more load,
 *   allocate ONE 256 B block, fill it, do NOT free it,
 *   dlclose; then verify from /proc/self/maps that the
 *   block's page is still mapped and its pattern still
 *   reads back intact. The block is intentionally never
 *   freed: after a full unmap the registry that owned
 *   it is gone, so freeing it through a later load
 *   would be undefined behaviour per the public header.
 *   The probe quantifies the leak as exactly the spans
 *   the workload had warmed (reported, not hidden).
 *
 * MODE 2: churn (responsiveness workload generator).
 *   test_g008 <so> churn <seconds> <threads>
 *   Loads the library once, runs <threads> worker
 *   threads (bounded: 1..4, the G-004 thread bound)
 *   churning small allocate/fill/free loops through the
 *   dlopen'd symbols until the deadline, prints the op
 *   count, and exits 0 only if every verify passed.
 *   Driven by tests/core/run_g008_probe.sh, which
 *   samples Samtal HTTP + trivial-command latency with
 *   and without this churn running. All workloads in
 *   this file are SYNTHETIC.
 *
 * Exit status 0 and a final "RESULT: PASS" line only
 * if every check passed (mode 1) / no verify failed
 * (mode 2). Prints only facts it observed.
 */

#define _POSIX_C_SOURCE 200809L

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

typedef void *(*mimir_malloc_fn)(size_t);
typedef void (*mimir_free_fn)(void *);
typedef void *(*mimir_calloc_fn)(size_t, size_t);
typedef void *(*mimir_realloc_fn)(void *, size_t);

static mimir_malloc_fn g_malloc;
static mimir_free_fn g_free;
static mimir_calloc_fn g_calloc;
static mimir_realloc_fn g_realloc;

static int failures = 0;
static int checks = 0;

static void check(int condition, const char *label)
{
    checks++;
    printf("  [%s] %s\n", condition ? "ok" : "FAIL", label);
    if (!condition) {
        failures++;
    }
}

static long read_rss_kb(void)
{
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    long kb = -1;
    if (!f) {
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "VmRSS: %ld kB", &kb) == 1) {
            break;
        }
    }
    fclose(f);
    return kb;
}

/* Parse /proc/self/maps once. Outputs: number of image
 * mapping lines naming libmimiralloc.so and their total
 * bytes; number of anonymous rw regions (no pathname)
 * and their total bytes. Returns 0 on success. */
static int maps_stats(int *img_lines, long *img_bytes,
                      int *anon_regions, long *anon_bytes)
{
    FILE *f = fopen("/proc/self/maps", "r");
    char line[1024];
    int il = 0, ar = 0;
    long ib = 0, ab = 0;
    if (!f) {
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        unsigned long lo = 0, hi = 0;
        char perms[8] = {0};
        int npath = 0;
        int n = sscanf(line, "%lx-%lx %7s %*s %*s %*s %n",
                       &lo, &hi, perms, &npath);
        const char *path;
        if (n < 3) {
            continue;
        }
        path = line + npath;
        while (*path == ' ') {
            path++;
        }
        if (strstr(line, "libmimiralloc.so") != NULL) {
            il++;
            ib += (long)(hi - lo);
        } else if (perms[0] == 'r' && perms[1] == 'w' &&
                   (*path == '\0' || *path == '\n')) {
            ar++;
            ab += (long)(hi - lo);
        }
    }
    fclose(f);
    *img_lines = il;
    *img_bytes = ib;
    *anon_regions = ar;
    *anon_bytes = ab;
    return 0;
}

/* Is the page containing addr currently mapped? */
static int addr_is_mapped(const void *addr)
{
    FILE *f = fopen("/proc/self/maps", "r");
    char line[1024];
    unsigned long a = (unsigned long)(uintptr_t)addr;
    int found = 0;
    if (!f) {
        return 0;
    }
    while (fgets(line, sizeof(line), f)) {
        unsigned long lo = 0, hi = 0;
        if (sscanf(line, "%lx-%lx", &lo, &hi) == 2 &&
            a >= lo && a < hi) {
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

/* Synthetic per-cycle workload: 32 x 256 B + 8 x 1024 B
 * (small-class path) + 1 x 8192 B (large mapping path),
 * fill every block with a cycle-seeded pattern, verify
 * every byte, free everything. Returns verify failures. */
static int workload(int cycle)
{
    void *small[32];
    void *mid[8];
    void *large;
    int fails = 0;
    int i;
    size_t j;
    unsigned char seed = (unsigned char)(0x40 + cycle);

    for (i = 0; i < 32; i++) {
        small[i] = g_malloc(256);
        if (!small[i]) {
            fails++;
        } else {
            memset(small[i], seed + i, 256);
        }
    }
    for (i = 0; i < 8; i++) {
        mid[i] = g_malloc(1024);
        if (!mid[i]) {
            fails++;
        } else {
            memset(mid[i], seed + 0x20 + i, 1024);
        }
    }
    large = g_calloc(1, 8192);
    if (!large) {
        fails++;
    } else {
        unsigned char *p = (unsigned char *)large;
        for (j = 0; j < 8192; j++) {
            if (p[j] != 0) {
                fails++;
                break;
            }
        }
        memset(large, seed, 8192);
    }
    /* One realloc leg per cycle (grow 256 -> 512). */
    if (small[0]) {
        void *grown = g_realloc(small[0], 512);
        if (!grown) {
            fails++;
        } else {
            unsigned char *p = (unsigned char *)grown;
            for (j = 0; j < 256; j++) {
                if (p[j] != (unsigned char)(seed + 0)) {
                    fails++;
                    break;
                }
            }
            small[0] = grown;
        }
    }
    for (i = 0; i < 32; i++) {
        if (small[i]) {
            unsigned char *p = (unsigned char *)small[i];
            size_t n = 256; /* grown block: only the first 256 B carry the pattern; the grow tail is unspecified (B-008) */
            for (j = 0; j < n; j++) {
                if (p[j] != (unsigned char)(seed + i)) {
                    fails++;
                    break;
                }
            }
        }
    }
    for (i = 0; i < 8; i++) {
        if (mid[i]) {
            unsigned char *p = (unsigned char *)mid[i];
            for (j = 0; j < 1024; j++) {
                if (p[j] != (unsigned char)(seed + 0x20 + i)) {
                    fails++;
                    break;
                }
            }
        }
    }
    for (i = 0; i < 32; i++) {
        g_free(small[i]);
    }
    for (i = 0; i < 8; i++) {
        g_free(mid[i]);
    }
    g_free(large);
    return fails;
}

static void *load_lib(const char *path)
{
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        return NULL;
    }
    dlerror();
    g_malloc = (mimir_malloc_fn)dlsym(h, "mimir_malloc");
    g_free = (mimir_free_fn)dlsym(h, "mimir_free");
    g_calloc = (mimir_calloc_fn)dlsym(h, "mimir_calloc");
    g_realloc = (mimir_realloc_fn)dlsym(h, "mimir_realloc");
    if (!g_malloc || !g_free || !g_calloc || !g_realloc) {
        dlclose(h);
        return NULL;
    }
    return h;
}

#define CYCLES 50

static int mode_load_unload(const char *path)
{
    long page = sysconf(_SC_PAGESIZE);
    long rss_start, rss_end;
    int img0 = -1, img_loaded = -1, img_final = -1;
    long img_bytes_loaded = 0;
    int anon0 = 0;
    long anon_bytes0 = 0, anon_bytes_c1 = 0, anon_bytes_end = 0;
    void *first_malloc_addr = NULL;
    int same_addr = 0, diff_addr = 0;
    int total_workload_fails = 0;
    int i;

    printf("MODE load-unload path=%s cycles=%d pagesize=%ld\n",
           path, CYCLES, page);
    rss_start = read_rss_kb();
    if (maps_stats(&img0, &(long){0}, &anon0, &anon_bytes0) != 0) {
        check(0, "maps readable at start");
        printf("RESULT: FAIL checks=%d failures=%d\n", checks, failures);
        return 1;
    }
    printf("START rss_kb=%ld img_lines=%d anon_rw_regions=%d "
           "anon_rw_bytes=%ld\n",
           rss_start, img0, anon0, anon_bytes0);
    check(img0 == 0, "no libmimiralloc.so image mapped before first dlopen");

    for (i = 0; i < CYCLES; i++) {
        void *h = load_lib(path);
        int img_a = -1, anon_r = 0;
        long anon_b = 0, ib = 0;
        int wf;
        check(h != NULL, "dlopen + dlsym x4 succeed");
        if (!h) {
            break;
        }
        if (i == 0) {
            first_malloc_addr = (void *)g_malloc;
            if (maps_stats(&img_loaded, &img_bytes_loaded,
                           &anon_r, &anon_b) == 0) {
                printf("LOADED cycle0 img_lines=%d img_bytes=%ld\n",
                       img_loaded, img_bytes_loaded);
            }
            check(img_loaded > 0,
                  "library image mapped while loaded (cycle 0)");
        } else if ((void *)g_malloc == first_malloc_addr) {
            same_addr++;
        } else {
            diff_addr++;
        }
        wf = workload(i);
        total_workload_fails += wf;
        check(wf == 0, "per-cycle workload fill/verify/free clean");
        check(dlclose(h) == 0, "dlclose returns 0");
        if (maps_stats(&img_a, &ib, &anon_r, &anon_b) == 0) {
            check(img_a == 0,
                  "library image fully unmapped after dlclose");
            if (i == 0) {
                anon_bytes_c1 = anon_b;
            }
            anon_bytes_end = anon_b;
            printf("CYCLE %d ok img_after_close=%d "
                   "anon_rw_bytes_after_close=%ld rss_kb=%ld\n",
                   i, img_a, anon_b, read_rss_kb());
        } else {
            check(0, "maps readable after dlclose");
        }
    }
    printf("ADDR mimir_malloc first=%p same_as_first=%d "
           "different=%d (of %d later cycles)\n",
           first_malloc_addr, same_addr, diff_addr, CYCLES - 1);
    check(total_workload_fails == 0,
          "workload verify failures total exactly 0 over 50 cycles");
    printf("ANON anon_rw_bytes start=%ld after_cycle0_close=%ld "
           "after_cycle49_close=%ld growth_c1_to_c50=%ld bytes "
           "= %ld pages\n",
           anon_bytes0, anon_bytes_c1, anon_bytes_end,
           anon_bytes_end - anon_bytes_c1,
           (anon_bytes_end - anon_bytes_c1) / page);
    check(anon_bytes_end > anon_bytes_c1,
          "anonymous rw bytes grow across cycles: retained spans "
          "orphaned by dlclose (no destructor runs)");

    /* ORPHAN PROBE: leak one block across a final close. */
    {
        void *h = load_lib(path);
        check(h != NULL, "orphan probe: dlopen succeeds");
        if (h) {
            unsigned char *blk = (unsigned char *)g_malloc(256);
            int mapped_before, mapped_after, pattern_ok = 0;
            size_t j;
            check(blk != NULL, "orphan probe: 256 B block allocated");
            if (blk) {
                memset(blk, 0xA5, 256);
                mapped_before = addr_is_mapped(blk);
                printf("ORPHAN block=%p mapped_while_loaded=%d\n",
                       (void *)blk, mapped_before);
                check(mapped_before == 1,
                      "orphan probe: block page mapped while loaded");
            }
            check(dlclose(h) == 0, "orphan probe: dlclose returns 0");
            if (blk) {
                mapped_after = addr_is_mapped(blk);
                if (mapped_after) {
                    pattern_ok = 1;
                    for (j = 0; j < 256; j++) {
                        if (blk[j] != 0xA5) {
                            pattern_ok = 0;
                            break;
                        }
                    }
                }
                printf("ORPHAN mapped_after_close=%d "
                       "pattern_intact_after_close=%d "
                       "(block intentionally never freed; its span "
                       "stays mapped until process exit)\n",
                       mapped_after, pattern_ok);
                check(mapped_after == 1,
                      "orphan probe: allocator span survives dlclose "
                      "(image unmaps, span mmaps do not)");
                check(pattern_ok == 1,
                      "orphan probe: leaked block contents intact "
                      "after dlclose");
            }
            /* Reload-after-orphan sanity: a fresh load works
             * and its registry is new; the orphaned block is
             * NOT freed through it (would be UB). */
            h = load_lib(path);
            check(h != NULL, "reload after orphan: dlopen succeeds");
            if (h) {
                int wf = workload(99);
                check(wf == 0,
                      "reload after orphan: workload clean on fresh load");
                check(dlclose(h) == 0,
                      "reload after orphan: dlclose returns 0");
            }
        }
    }

    if (maps_stats(&img_final, &(long){0}, &anon0, &anon_bytes0) == 0) {
        printf("FINAL img_lines=%d\n", img_final);
        check(img_final == 0,
              "no library image mapped at end of record");
    }
    rss_end = read_rss_kb();
    printf("RSS rss_start_kb=%ld rss_end_kb=%ld delta_kb=%ld\n",
           rss_start, rss_end, rss_end - rss_start);
    printf("G008 load-unload: %d checks, %d failures\n",
           checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}

/* ---- churn mode ---- */

static volatile int g_churn_stop = 0;
static volatile long g_churn_ops = 0;
static volatile long g_churn_fails = 0;

static void *churn_worker(void *arg)
{
    long idx = (long)(uintptr_t)arg;
    unsigned int iter = 0;
    while (!g_churn_stop) {
        void *blk[16];
        int i;
        size_t sz = (iter % 2 == 0) ? 64 : 256;
        for (i = 0; i < 16; i++) {
            blk[i] = g_malloc(sz);
            if (blk[i]) {
                memset(blk[i], (int)(idx + iter + i), sz);
            } else {
                __sync_fetch_and_add(&g_churn_fails, 1);
            }
        }
        for (i = 0; i < 16; i++) {
            if (blk[i]) {
                unsigned char *p = (unsigned char *)blk[i];
                if (p[0] != (unsigned char)(idx + iter + i) ||
                    p[sz - 1] != (unsigned char)(idx + iter + i)) {
                    __sync_fetch_and_add(&g_churn_fails, 1);
                }
            }
        }
        for (i = 0; i < 16; i++) {
            g_free(blk[i]);
        }
        __sync_fetch_and_add(&g_churn_ops, 16);
        iter++;
    }
    return NULL;
}

static int mode_churn(const char *path, int seconds, int nthreads)
{
    void *h = load_lib(path);
    pthread_t th[4];
    int i;
    if (!h) {
        printf("CHURN dlopen FAILED\n");
        return 1;
    }
    if (nthreads < 1) {
        nthreads = 1;
    }
    if (nthreads > 4) {
        nthreads = 4;
    }
    printf("CHURN start seconds=%d threads=%d (synthetic workload)\n",
           seconds, nthreads);
    for (i = 0; i < nthreads; i++) {
        if (pthread_create(&th[i], NULL, churn_worker,
                           (void *)(uintptr_t)i) != 0) {
            printf("CHURN pthread_create FAILED\n");
            dlclose(h);
            return 1;
        }
    }
    sleep((unsigned int)seconds);
    g_churn_stop = 1;
    for (i = 0; i < nthreads; i++) {
        pthread_join(th[i], NULL);
    }
    printf("CHURN done ops=%ld fails=%ld rss_kb=%ld\n",
           g_churn_ops, g_churn_fails, read_rss_kb());
    dlclose(h);
    printf("RESULT: %s\n", g_churn_fails == 0 ? "PASS" : "FAIL");
    return g_churn_fails == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *path = "build/libmimiralloc.so";
    if (argc >= 2) {
        path = argv[1];
    }
    if (argc >= 3 && strcmp(argv[2], "churn") == 0) {
        int seconds = (argc >= 4) ? atoi(argv[3]) : 10;
        int nthreads = (argc >= 5) ? atoi(argv[4]) : 2;
        return mode_churn(path, seconds, nthreads);
    }
    return mode_load_unload(path);
}
