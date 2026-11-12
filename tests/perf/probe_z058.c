/* probe_z058.c - Z-058: split and coalesce audit.
 *
 * Adversarial patterns constructed from Z-055's worst cases
 * (tests/perf/recorded_run_z13_z055.txt): F-3 free-but-
 * unusable = 32064 B at R-EXT 0.260937 with class-locked
 * holes; fragmented bearing spans in classes 64/256/1024;
 * class-1024 tail 928 B; large-mapping overhead 4576/5056 B
 * and the Z-048 size-exact mapping pool.
 *
 * Question per pattern: when a block is freed, can its
 * space be SPLIT to serve a smaller request, or COALESCED
 * with neighbours to serve a larger one? Measured on this
 * host only, exact counters at quiesced points (Z-055
 * method: mimir_core_trim_retained_some(0) flushes TLS
 * bins before every snapshot). No production code changed.
 * Standalone (run_z058.sh), NOT part of make test.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include "mimir_alloc.h"
#include "mimir_small_g003.h"
#include "mimir_trim_i003.h"
#include "mimir_pool_z048.h"
#include "mimir_stats_j002.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int checks = 0;
static void check(int c, const char *label) {
    checks++;
    printf("  [%s] %s\n", c ? "ok" : "FAIL", label);
    if (!c) failures++;
}
static void check_eq(size_t got, size_t want, const char *label) {
    checks++;
    printf("  [%s] %s (got %zu, want %zu)\n", got == want ? "ok" : "FAIL", label, got, want);
    if (got != want) failures++;
}
static void fill(void *p, size_t n, unsigned char s) {
    unsigned char *b = (unsigned char *)p; size_t i;
    for (i = 0; i < n; i++) b[i] = (unsigned char)(s + (unsigned char)i);
}
static int intact(const void *p, size_t n, unsigned char s) {
    const unsigned char *b = (const unsigned char *)p; size_t i;
    for (i = 0; i < n; i++) if (b[i] != (unsigned char)(s + (unsigned char)i)) return 0;
    return 1;
}
static void quiesce(void) { (void)mimir_core_trim_retained_some(0); }
static struct mimir_stats snap(void) {
    struct mimir_stats s; memset(&s, 0, sizeof s);
    quiesce();
    if (mimir_core_stats_snapshot(&s) != 0) { printf("  [FAIL] snapshot\n"); failures++; }
    return s;
}
static int in_set(void *p, void **set, size_t n) {
    size_t i; for (i = 0; i < n; i++) if (set[i] == p) return 1; return 0;
}
/* class indices: 16:0 32:1 48:2 64:3 128:7 256:11 1024:19 */
#define CI64 3
#define CI1024 19

int main(void) {
    struct mimir_stats s0, s;
    printf("Z-058 split and coalesce audit (this host)\n");
    s0 = snap();
    printf("page=%llu baseline mapped=%llu live_small=%llu pool=%zu/%zu\n",
        (unsigned long long)s0.page_size_bytes,
        (unsigned long long)s0.bytes_mapped,
        (unsigned long long)s0.live_small_objects,
        mimir_core_pool_count(), mimir_core_pool_bytes());
    check_eq((size_t)s0.page_size_bytes, 4096, "host page 4096");

    /* P1 CARVE-SPLIT: a fresh span page is split into fixed
     * class slots by carving; slots are class-exact. */
    printf("== P1 carve-split: page -> class-64 slots ==\n");
    {
        void *p[20]; size_t i, distinct = 0; int strides_ok = 1;
        for (i = 0; i < 20; i++) { p[i] = mimir_malloc(64); if (!p[i]) { check(0, "P1 alloc"); break; } fill(p[i], 64, (unsigned char)i); }
        for (i = 0; i < 20; i++) { size_t j; int seen = 0; for (j = 0; j < i; j++) if (p[j] == p[i]) seen = 1; if (!seen) distinct++; if (i && ((char *)p[i] - (char *)p[0]) % 64 != 0) strides_ok = 0; }
        check_eq(distinct, 20, "P1 20 distinct slots carved");
        check(strides_ok, "P1 every slot offset is a multiple of class size 64 from slot 0 (carve stride)");
        s = snap();
        printf("  P1 measured: spans class64=%llu capacity=%llu live=%llu\n",
            (unsigned long long)s.span_total_spans[CI64],
            (unsigned long long)s.span_capacity_objects[CI64],
            (unsigned long long)s.span_live_objects[CI64]);
        check_eq((size_t)s.span_live_objects[CI64], 20, "P1 live class-64 objects");
        for (i = 0; i < 20; i++) check(intact(p[i], 64, (unsigned char)i), "P1 pattern");
        for (i = 0; i < 20; i++) mimir_free(p[i]);
    }
    /* P2 CHECKERBOARD: free every other class-64 slot, then
     * ask for class 128 (coalesce test) and class 64
     * (exact reuse test). */
    printf("== P2 checkerboard holes: coalesce to 128? ==\n");
    {
        void *p[32]; void *freed[16]; size_t i, nf = 0; void *big, *reuse;
        for (i = 0; i < 32; i++) { p[i] = mimir_malloc(64); fill(p[i], 64, (unsigned char)(i + 40)); }
        for (i = 1; i < 32; i += 2) { freed[nf++] = p[i]; mimir_free(p[i]); p[i] = NULL; }
        quiesce();
        big = mimir_malloc(128);
        check(big != NULL, "P2 class-128 request served");
        check(!in_set(big, freed, nf), "P2 class-128 NOT served from coalesced 64-holes (no coalescing)");
        reuse = mimir_malloc(64);
        check(in_set(reuse, freed, nf), "P2 class-64 request reuses an exact freed hole");
        for (i = 0; i < 32; i++) if (p[i]) { check(intact(p[i], 64, (unsigned char)(i + 40)), "P2 survivor pattern"); mimir_free(p[i]); }
        mimir_free(big); mimir_free(reuse);
    }
    /* P3 CROSS-CLASS SPLIT: free a 1024 slot while its span
     * is PINNED by a second live 1024 block (so the span and
     * the freed slot provably stay alive as class 1024), then
     * ask for 16s. A pilot without the pin showed a 16 block
     * landing on the freed address - that was whole-PAGE
     * reuse after the empty span was released and a class-16
     * span acquired at the same page, not a slot split; the
     * pin separates the two mechanisms. */
    printf("== P3 cross-class split: freed 1024 -> 16s? ==\n");
    {
        void *a = mimir_malloc(1024); void *pin = mimir_malloc(1024); void *small[16]; size_t i; int any = 0;
        check(a != NULL && pin != NULL, "P3 class-1024 allocs (slot + pin)");
        fill(pin, 1024, 0x44);
        mimir_free(a); quiesce();
        for (i = 0; i < 16; i++) { small[i] = mimir_malloc(16); if (small[i] == a) any = 1; }
        check(!any, "P3 no 16-byte block is carved out of the live pinned span's freed 1024 slot (no cross-class split)");
        check(intact(pin, 1024, 0x44), "P3 pin pattern intact");
        for (i = 0; i < 16; i++) mimir_free(small[i]);
        mimir_free(pin);
    }
    /* P4 PIN TRAP (Z-055 worst case, F-3 shape): fill class-64
     * spans, free all but ONE block; the page stays mapped
     * and its free slots are unusable by any other class. */
    printf("== P4 pin trap: one live block pins a span ==\n");
    {
        void *p[124]; size_t i; struct mimir_stats sf, sp;
        for (i = 0; i < 124; i++) { p[i] = mimir_malloc(64); fill(p[i], 64, 0x5A); }
        sf = snap();
        for (i = 1; i < 124; i++) mimir_free(p[i]);
        sp = snap();
        printf("  P4 measured: full spans=%llu live=%llu mapped=%llu ; pinned spans=%llu live=%llu mapped=%llu retained=%llu\n",
            (unsigned long long)sf.span_total_spans[CI64],
            (unsigned long long)sf.span_live_objects[CI64],
            (unsigned long long)sf.bytes_mapped,
            (unsigned long long)sp.span_total_spans[CI64],
            (unsigned long long)sp.span_live_objects[CI64],
            (unsigned long long)sp.bytes_mapped,
            (unsigned long long)sp.bytes_cached);
        check_eq((size_t)sp.span_live_objects[CI64], 1, "P4 exactly one live class-64 block remains");
        check(sp.span_total_spans[CI64] >= 1, "P4 at least one whole span stays mapped for the pin");
        check(intact(p[0], 64, 0x5A), "P4 pinned block pattern intact");
        printf("  P4 WASTE: pinned span bytes=%llu for 64 live bytes (span granularity is the only release unit)\n",
            (unsigned long long)(sp.span_total_spans[CI64] * sp.page_size_bytes));
        mimir_free(p[0]);
    }
    /* P5 FREE-ORDER: free A then B; under TLS batching the
     * pair is not necessarily the next two served (pilot:
     * neither was), so reuse is audited over a batch: both
     * freed slots must reappear among the next 256 class-64
     * allocations, as exact slots. */
    printf("== P5 free-order reuse over a batch ==\n");
    {
        void *a = mimir_malloc(64), *b = mimir_malloc(64); void *batch[256]; size_t i; long ia = -1, ib = -1;
        mimir_free(a); mimir_free(b); quiesce();
        for (i = 0; i < 256; i++) { batch[i] = mimir_malloc(64); if (batch[i] == a) ia = (long)i; if (batch[i] == b) ib = (long)i; }
        printf("  P5 measured: freed A reappears at batch index %ld, B at %ld (TLS batch order, not strict LIFO)\n", ia, ib);
        check(ia >= 0 && ib >= 0, "P5 both freed slots are reused exactly within the next batch (no merging, no loss)");
        for (i = 0; i < 256; i++) mimir_free(batch[i]);
    }
    /* P6 LARGE SPLIT: pool holds a freed 45056 mapping;
     * a 20000 request (24576 mapping) must NOT split it. */
    printf("== P6 large split: pooled 45056 vs request 24576 ==\n");
    {
        void *a, *b; struct mimir_stats s1, s2;
        (void)mimir_core_trim_retained(); quiesce();
        check_eq(mimir_core_pool_count(), 0, "P6 pool empty at start (trim drained it)");
        a = mimir_malloc(40000); fill(a, 40000, 0x11);
        check(a != NULL && intact(a, 40000, 0x11), "P6 large 40000 alloc + pattern");
        mimir_free(a); quiesce();
        check_eq(mimir_core_pool_count(), 1, "P6 freed large mapping pooled");
        check_eq(mimir_core_pool_bytes(), 45056, "P6 pooled bytes = 45056 (Z-055 F-2 arithmetic)");
        s1 = snap();
        b = mimir_malloc(20000);
        check(b != NULL && b != a, "P6 smaller large request gets a FRESH mapping, not a split of the pooled one");
        check_eq(mimir_core_pool_count(), 1, "P6 pool untouched by the smaller request (no split)");
        s2 = snap();
        printf("  P6 measured: mapped %llu -> %llu (delta %lld = fresh 24576 mapping)\n",
            (unsigned long long)s1.bytes_mapped, (unsigned long long)s2.bytes_mapped,
            (long long)s2.bytes_mapped - (long long)s1.bytes_mapped);
        check_eq((size_t)(s2.bytes_mapped - s1.bytes_mapped), 24576, "P6 mapped delta is the full new mapping");
        mimir_free(b); quiesce();
        /* P7 LARGE COALESCE: pool now holds 45056 + 24576;
         * a 70000 request must NOT coalesce them. */
        printf("== P7 large coalesce: pooled 45056+24576 vs request 77824 ==\n");
        check_eq(mimir_core_pool_count(), 2, "P7 pool holds both freed mappings");
        {
            void *d = mimir_malloc(70000);
            check(d != NULL && d != a && d != b, "P7 larger request gets a fresh mapping (no coalescing of pooled mappings)");
            check_eq(mimir_core_pool_count(), 2, "P7 pool untouched by the larger request (no coalesce)");
            mimir_free(d);
        }
        /* P8 EXACT REUSE control: the one reuse the pool does. */
        printf("== P8 large exact reuse (control) ==\n");
        {
            void *e = mimir_malloc(40000);
            check(e == a, "P8 exact-length request IS served from the pool (same address)");
            check_eq(mimir_core_pool_count(), 2, "P8 pool drops by the reused entry (3 pooled incl. 77824, minus 1 reused)");
            mimir_free(e);
        }
        (void)mimir_core_trim_retained(); quiesce();
        check_eq(mimir_core_pool_count(), 0, "P6-P8 cleanup: trim releases the whole pool");
    }
    /* P9 REALLOC: grow moves; shrink keeps the whole block
     * (no split of the tail, mapping/class unchanged). */
    printf("== P9 realloc split behaviour ==\n");
    {
        void *x = mimir_malloc(32); void *y; struct mimir_stats g1, g2;
        fill(x, 32, 0x22);
        y = mimir_realloc(x, 256);
        check(y != NULL && intact(y, 32, 0x22), "P9 small grow 32->256 preserves contents (move)");
        mimir_free(y);
        {
            void *g = mimir_malloc(40000); void *h;
            fill(g, 40000, 0x33);
            g1 = snap();
            h = mimir_realloc(g, 20000);
            g2 = snap();
            check(h == g, "P9 large shrink 40000->20000 stays in place (no split)");
            check(intact(h, 20000, 0x33), "P9 shrink preserves contents");
            printf("  P9 measured: live_mapping_bytes %llu -> %llu (whole 45056 mapping retained for a 20000 request)\n",
                (unsigned long long)g1.live_mapping_bytes, (unsigned long long)g2.live_mapping_bytes);
            check_eq((size_t)g2.live_mapping_bytes, (size_t)g1.live_mapping_bytes, "P9 shrink releases NO mapping bytes (tail never split off)");
            mimir_free(h);
        }
    }
    /* P10 WHOLE-SPAN RELEASE: the only coalescing-like act -
     * when a span empties past the retention bound, the WHOLE
     * page is released; free-all returns to baseline. */
    printf("== P10 whole-span release on empty ==\n");
    {
        struct mimir_stats fin;
        (void)mimir_core_trim_retained(); quiesce();
        fin = snap();
        printf("  P10 measured: mapped=%llu (baseline %llu) live_small=%llu retained=%llu pool=%zu\n",
            (unsigned long long)fin.bytes_mapped, (unsigned long long)s0.bytes_mapped,
            (unsigned long long)fin.live_small_objects, (unsigned long long)fin.bytes_cached,
            mimir_core_pool_count());
        check_eq((size_t)fin.live_small_objects, 0, "P10 no live small objects at end");
        check_eq((size_t)fin.bytes_mapped, (size_t)s0.bytes_mapped, "P10 mapped returns to baseline after free-all + trim");
    }
    printf("checks=%d failures=%d\n", checks, failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
