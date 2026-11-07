/*
 * test_h001.c -- evidence test for slice H-001 (Phase 7
 * unload drain; slice record:
 * docs/PHASE_7_UNLOAD_DRAIN_H001.md; commissioned fix for
 * INC-G008-02, the G-008 finding that dlclose orphaned
 * retained spans at exactly 49,152 B = 3 pages per cycle
 * because src/ registered no destructor).
 *
 * Run standalone via `make test-h001` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-g003..test-g008 pattern).
 *
 * Two legs in one binary:
 *
 * DLOPEN LEG (runs first, the G-008 harness shape):
 * consumes build/libmimiralloc.so exactly as an outside
 * consumer does -- dlopen / dlsym / dlclose of the four
 * public symbols only -- for 50 bounded cycles of the
 * G-008 workload (32 x 256 B + 8 x 1024 B small-class
 * blocks + 1 x 8192 B calloc on the large path + one
 * realloc grow 256 -> 512 per cycle; fill, verify every
 * pattern byte, free all). After every dlclose it records
 * the libmimiralloc.so image mappings still present in
 * /proc/self/maps (must be 0, as in G-008) and the total
 * anonymous rw bytes mapped. With the H-001 destructor
 * draining retained EMPTY spans at dlclose, the required
 * evidence is anonymous rw growth of exactly 0 B per
 * cycle. A LIVE-AT-UNLOAD probe then leaks one 256 B
 * block across a final dlclose on purpose: per the
 * INC-G008-03 posture its span must NOT be drained --
 * its page stays mapped, its pattern stays intact, and
 * the anonymous total grows by exactly that one page.
 * A reload leg verifies a fresh load works cleanly and
 * leaves the anonymous total at the probe's level.
 *
 * DIRECT LEG (runs second): links the library OBJECTS
 * (LIBOBJS), so it can read the G-003 span/object
 * counters (tests/core/mimir_small_g003.h), the E-005
 * mapping counters (tests/abi/mimir_audit_e005.h) and
 * call the drain entry point itself through the
 * test-only declaration tests/core/mimir_drain_h001.h
 * (INC-E001-07 shape (b)). It proves the drain semantics
 * exactly: an empty drain returns 0; warmed-then-freed
 * classes retain exactly one empty span each; a drain
 * releases exactly the EMPTY spans and never a span
 * holding a live block or a live large mapping (the live
 * patterns are verified byte-for-byte after the drain);
 * a second drain returns 0 (idempotent); the allocator
 * is fully usable after a drain; and a final drain
 * brings every counter pair to exact reconciliation
 * (published == retired, alloc == free). The static
 * instance used here is disjoint from the dlopen leg's
 * loads (separate .bss, fresh registry), and this leg
 * starts only after the dlopen leg has finished.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check in both legs passed. Prints only facts it
 * observed.
 */

#define _POSIX_C_SOURCE 200809L

#include "mimir_alloc.h"
#include "mimir_small_g003.h"
#include "mimir_drain_h001.h"
#include "../abi/mimir_audit_e005.h"

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

/* ================= dlopen leg (G-008 shape) ================= */

typedef void *(*malloc_fn)(size_t);
typedef void (*free_fn)(void *);
typedef void *(*calloc_fn)(size_t, size_t);
typedef void *(*realloc_fn)(void *, size_t);

static malloc_fn d_malloc;
static free_fn d_free;
static calloc_fn d_calloc;
static realloc_fn d_realloc;

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

/* Parse /proc/self/maps once (the G-008 reader, unchanged
 * in shape). Outputs: number of image mapping lines naming
 * libmimiralloc.so and their total bytes; number of
 * anonymous rw regions (no pathname) and total bytes. */
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

/* The G-008 per-cycle workload, unchanged: 32 x 256 B +
 * 8 x 1024 B (small-class path) + 1 x 8192 B calloc
 * (large mapping path), fill + verify + free all, plus
 * one realloc grow 256 -> 512 (only the preserved 256 B
 * carry the pattern; the grow tail is unspecified). */
static int d_workload(int cycle)
{
    void *small[32];
    void *mid[8];
    void *large;
    int fails = 0;
    int i;
    size_t j;
    unsigned char seed = (unsigned char)(0x40 + cycle);

    for (i = 0; i < 32; i++) {
        small[i] = d_malloc(256);
        if (!small[i]) {
            fails++;
        } else {
            memset(small[i], seed + i, 256);
        }
    }
    for (i = 0; i < 8; i++) {
        mid[i] = d_malloc(1024);
        if (!mid[i]) {
            fails++;
        } else {
            memset(mid[i], seed + 0x20 + i, 1024);
        }
    }
    large = d_calloc(1, 8192);
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
    if (small[0]) {
        void *grown = d_realloc(small[0], 512);
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
            for (j = 0; j < 256; j++) {
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
        d_free(small[i]);
    }
    for (i = 0; i < 8; i++) {
        d_free(mid[i]);
    }
    d_free(large);
    return fails;
}

static void *d_load_lib(const char *path)
{
    void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        return NULL;
    }
    dlerror();
    d_malloc = (malloc_fn)dlsym(h, "mimir_malloc");
    d_free = (free_fn)dlsym(h, "mimir_free");
    d_calloc = (calloc_fn)dlsym(h, "mimir_calloc");
    d_realloc = (realloc_fn)dlsym(h, "mimir_realloc");
    if (!d_malloc || !d_free || !d_calloc || !d_realloc) {
        dlclose(h);
        return NULL;
    }
    return h;
}

#define CYCLES 50

static void leg_dlopen(const char *path)
{
    long page = sysconf(_SC_PAGESIZE);
    long rss_start;
    int img0 = -1, img_loaded = -1, img_final = -1;
    long img_bytes_loaded = 0;
    int anon0 = 0;
    long anon_bytes0 = 0, anon_c0 = -1, anon_end = -1;
    long anon_after_orphan = -1, anon_after_reload = -1;
    void *first_malloc_addr = NULL;
    int same_addr = 0, diff_addr = 0;
    int total_workload_fails = 0;
    int checks_before = checks;
    int i;

    printf("LEG dlopen path=%s cycles=%d pagesize=%ld\n",
           path, CYCLES, page);
    rss_start = read_rss_kb();
    if (maps_stats(&img0, &(long){0}, &anon0, &anon_bytes0) != 0) {
        check(0, "maps readable at start");
        return;
    }
    printf("START rss_kb=%ld img_lines=%d anon_rw_regions=%d "
           "anon_rw_bytes=%ld\n",
           rss_start, img0, anon0, anon_bytes0);
    check(img0 == 0, "no libmimiralloc.so image mapped before first dlopen");

    for (i = 0; i < CYCLES; i++) {
        void *h = d_load_lib(path);
        int img_a = -1, anon_r = 0;
        long anon_b = 0, ib = 0;
        int wf;
        check(h != NULL, "dlopen + dlsym x4 succeed");
        if (!h) {
            break;
        }
        if (i == 0) {
            first_malloc_addr = (void *)d_malloc;
            if (maps_stats(&img_loaded, &img_bytes_loaded,
                           &anon_r, &anon_b) == 0) {
                printf("LOADED cycle0 img_lines=%d img_bytes=%ld\n",
                       img_loaded, img_bytes_loaded);
            }
            check(img_loaded > 0,
                  "library image mapped while loaded (cycle 0)");
        } else if ((void *)d_malloc == first_malloc_addr) {
            same_addr++;
        } else {
            diff_addr++;
        }
        wf = d_workload(i);
        total_workload_fails += wf;
        check(wf == 0, "per-cycle workload fill/verify/free clean");
        check(dlclose(h) == 0, "dlclose returns 0");
        if (maps_stats(&img_a, &ib, &anon_r, &anon_b) == 0) {
            check(img_a == 0,
                  "library image fully unmapped after dlclose");
            if (i == 0) {
                anon_c0 = anon_b;
            } else {
                check(anon_b == anon_c0,
                      "anon rw bytes after close equal the cycle-0 "
                      "close level (0 B growth this cycle)");
            }
            anon_end = anon_b;
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
           "after_cycle49_close=%ld growth_start_to_c0=%ld bytes "
           "growth_c0_to_c49=%ld bytes = %ld pages\n",
           anon_bytes0, anon_c0, anon_end,
           anon_c0 - anon_bytes0, anon_end - anon_c0,
           page > 0 ? (anon_end - anon_c0) / page : -1);
    check(anon_c0 == anon_bytes0,
          "anon rw bytes after cycle-0 close equal the pre-load "
          "level (cycle 0 left 0 B behind)");
    check(anon_end == anon_c0,
          "anon rw growth cycle 0 -> cycle 49 exactly 0 B "
          "(destructor drains retained spans at every dlclose)");

    /* LIVE-AT-UNLOAD PROBE (INC-G008-03 posture): leak one
     * block across a final close on purpose. Its span holds
     * a live block, so the drain must NOT release it. */
    {
        void *h = d_load_lib(path);
        check(h != NULL, "live probe: dlopen succeeds");
        if (h) {
            unsigned char *blk = (unsigned char *)d_malloc(256);
            int mapped_before = 0, mapped_after = 0, pattern_ok = 0;
            size_t j;
            check(blk != NULL, "live probe: 256 B block allocated");
            if (blk) {
                memset(blk, 0xA5, 256);
                mapped_before = addr_is_mapped(blk);
                printf("LIVE block=%p mapped_while_loaded=%d\n",
                       (void *)blk, mapped_before);
                check(mapped_before == 1,
                      "live probe: block page mapped while loaded");
            }
            check(dlclose(h) == 0, "live probe: dlclose returns 0");
            if (blk) {
                int il = -1, ar = 0;
                long ab = 0;
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
                if (maps_stats(&il, &(long){0}, &ar, &ab) == 0) {
                    anon_after_orphan = ab;
                }
                printf("LIVE mapped_after_close=%d "
                       "pattern_intact_after_close=%d "
                       "anon_rw_bytes_after_close=%ld "
                       "(block intentionally never freed; its span "
                       "is live, so the drain leaves it mapped)\n",
                       mapped_after, pattern_ok, anon_after_orphan);
                check(mapped_after == 1,
                      "live probe: live block's span survives dlclose "
                      "(drain releases only EMPTY spans)");
                check(pattern_ok == 1,
                      "live probe: live block contents intact after dlclose");
                check(anon_after_orphan == anon_end + page,
                      "live probe: anon growth across the probe close "
                      "is exactly the one live span (1 page)");
            }
            /* Reload sanity: a fresh load works and its own
             * retained spans drain at its close, returning the
             * anonymous total to the probe level. The leaked
             * block is NOT freed through it (would be UB). */
            h = d_load_lib(path);
            check(h != NULL, "reload after live probe: dlopen succeeds");
            if (h) {
                int wf = d_workload(99);
                int il = -1, ar = 0;
                long ab = 0;
                check(wf == 0,
                      "reload after live probe: workload clean on fresh load");
                check(dlclose(h) == 0,
                      "reload after live probe: dlclose returns 0");
                if (maps_stats(&il, &(long){0}, &ar, &ab) == 0) {
                    anon_after_reload = ab;
                    check(il == 0,
                          "reload after live probe: image unmapped after close");
                    check(ab == anon_after_orphan,
                          "reload after live probe: anon level back to the "
                          "probe level (reload's spans drained)");
                } else {
                    check(0, "maps readable after reload close");
                }
                printf("RELOAD anon_rw_bytes_after_close=%ld\n",
                       anon_after_reload);
            }
        }
    }

    if (maps_stats(&img_final, &(long){0}, &anon0, &anon_bytes0) == 0) {
        printf("FINAL img_lines=%d\n", img_final);
        check(img_final == 0,
              "no library image mapped at end of dlopen leg");
    }
    printf("RSS rss_start_kb=%ld rss_end_kb=%ld\n",
           rss_start, read_rss_kb());
    printf("LEG dlopen: %d checks, %d failures so far\n",
           checks - checks_before, failures);
}

/* ================= direct leg (static link) ================= */

static unsigned long live_spans(void)
{
    return mimir_audit_span_published - mimir_audit_span_retired;
}

static unsigned long live_small(void)
{
    return mimir_audit_small_alloc - mimir_audit_small_free;
}

static unsigned long live_mappings(void)
{
    return mimir_audit_alloc_published - mimir_audit_mapping_retired;
}

static int pattern_ok(const void *ptr, size_t n, unsigned char v)
{
    const unsigned char *p = (const unsigned char *)ptr;
    size_t j;
    for (j = 0; j < n; j++) {
        if (p[j] != v) {
            return 0;
        }
    }
    return 1;
}

static void leg_direct(void)
{
    int checks_before = checks;
    void *a256[4], *a512[4], *a1024[4];
    void *live256, *live_large, *new512;
    size_t drained;
    int i, ok;

    printf("LEG direct (static link, drain semantics)\n");

    /* D0: fresh instance, empty drain. */
    check(live_spans() == 0, "D0 no live spans at start");
    check(live_small() == 0, "D0 no live small objects at start");
    check(live_mappings() == 0, "D0 no live mappings at start");
    check(mimir_core_drain_retained() == 0,
          "D0 drain with nothing retained returns 0");
    check(mimir_core_drain_retained() == 0,
          "D0 second empty drain returns 0 (idempotent)");

    /* D1: warm three classes, free everything: exactly one
     * retained EMPTY span per warmed class. */
    ok = 1;
    for (i = 0; i < 4; i++) {
        a256[i] = mimir_malloc(256);
        a512[i] = mimir_malloc(512);
        a1024[i] = mimir_malloc(1024);
        if (!a256[i] || !a512[i] || !a1024[i]) {
            ok = 0;
        } else {
            memset(a256[i], 0x11 + i, 256);
            memset(a512[i], 0x21 + i, 512);
            memset(a1024[i], 0x31 + i, 1024);
        }
    }
    check(ok, "D1 12 warm blocks (256/512/1024) allocated");
    ok = 1;
    for (i = 0; i < 4; i++) {
        if (!pattern_ok(a256[i], 256, (unsigned char)(0x11 + i)) ||
            !pattern_ok(a512[i], 512, (unsigned char)(0x21 + i)) ||
            !pattern_ok(a1024[i], 1024, (unsigned char)(0x31 + i))) {
            ok = 0;
        }
    }
    check(ok, "D1 warm block patterns verify");
    for (i = 0; i < 4; i++) {
        mimir_free(a256[i]);
        mimir_free(a512[i]);
        mimir_free(a1024[i]);
    }
    check(live_small() == 0, "D1 live small objects 0 after freeing all");
    check(live_spans() == 3,
          "D1 exactly 3 retained empty spans (1 per warmed class)");
    printf("D1 live_spans=%lu live_small=%lu\n",
           live_spans(), live_small());

    /* D2: live preservation. A live 256 block reuses the
     * retained 256 span; a live 8192 block takes the mapping
     * path. The drain must release exactly the two EMPTY
     * spans (512, 1024) and touch nothing live. */
    live256 = mimir_malloc(256);
    live_large = mimir_malloc(8192);
    check(live256 != NULL && live_large != NULL,
          "D2 live 256 B and 8192 B blocks allocated");
    if (live256) {
        memset(live256, 0x5A, 256);
    }
    if (live_large) {
        memset(live_large, 0x3C, 8192);
    }
    check(live_spans() == 3, "D2 live spans still 3 (256 span reused)");
    check(live_mappings() == 1, "D2 exactly 1 live large mapping");
    drained = mimir_core_drain_retained();
    printf("D2 drained=%zu live_spans=%lu live_small=%lu "
           "live_mappings=%lu\n",
           drained, live_spans(), live_small(), live_mappings());
    check(drained == 2,
          "D2 drain released exactly the 2 EMPTY spans (512, 1024)");
    check(live_spans() == 1, "D2 the live 256 span remains");
    check(live_small() == 1, "D2 the live small object remains counted");
    check(live256 && pattern_ok(live256, 256, 0x5A),
          "D2 live 256 pattern byte-intact after drain");
    check(live_large && pattern_ok(live_large, 8192, 0x3C),
          "D2 live 8192 pattern byte-intact after drain");
    check(live_mappings() == 1,
          "D2 live large mapping untouched by drain");

    /* D3: idempotence + post-drain usability. */
    check(mimir_core_drain_retained() == 0,
          "D3 drain again returns 0 (idempotent)");
    check(live_spans() == 1, "D3 live spans unchanged by empty drain");
    new512 = mimir_malloc(512);
    check(new512 != NULL,
          "D3 allocation after drain succeeds (fresh span acquired)");
    if (new512) {
        memset(new512, 0x77, 512);
    }
    check(new512 && pattern_ok(new512, 512, 0x77),
          "D3 post-drain block pattern verifies");
    check(live_spans() == 2, "D3 live spans 2 with the fresh 512 span");
    mimir_free(new512);
    check(live_spans() == 2,
          "D3 freed 512 span retained empty (within RP-2 bound)");

    /* D4: free the live blocks, drain the rest, reconcile. */
    mimir_free(live256);
    mimir_free(live_large);
    check(live_small() == 0, "D4 live small objects 0");
    check(live_mappings() == 0, "D4 live mappings 0");
    check(live_spans() == 2,
          "D4 exactly 2 retained empty spans remain (256, 512)");
    drained = mimir_core_drain_retained();
    printf("D4 drained=%zu live_spans=%lu\n", drained, live_spans());
    check(drained == 2, "D4 final drain released exactly 2 spans");
    check(live_spans() == 0, "D4 no live spans after final drain");
    check(mimir_audit_span_published == mimir_audit_span_retired,
          "D4 span counters reconcile (published == retired)");
    check(mimir_audit_small_alloc == mimir_audit_small_free,
          "D4 small counters reconcile (alloc == free)");
    check(mimir_audit_alloc_published == mimir_audit_mapping_retired,
          "D4 mapping counters reconcile (published == retired)");
    printf("COUNTERS span_published=%lu span_retired=%lu "
           "small_alloc=%lu small_free=%lu map_published=%lu "
           "map_retired=%lu\n",
           mimir_audit_span_published, mimir_audit_span_retired,
           mimir_audit_small_alloc, mimir_audit_small_free,
           mimir_audit_alloc_published, mimir_audit_mapping_retired);
    printf("LEG direct: %d checks, %d failures so far\n",
           checks - checks_before, failures);
}

int main(int argc, char **argv)
{
    const char *path = "build/libmimiralloc.so";
    if (argc >= 2) {
        path = argv[1];
    }
    printf("H-001 unload drain evidence test\n");
    leg_dlopen(path);
    leg_direct();
    printf("H001: %d checks, %d failures\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
