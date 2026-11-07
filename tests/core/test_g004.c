/*
 * test_g004.c -- evidence test for the G-004 Phase 7
 * central fallback integration audit
 * (docs/PHASE_7_CENTRAL_FALLBACK_G004.md; routing rules:
 * docs/PHASE_7_SIZE_CLASSES_G002.md R-1..R-6 as
 * implemented by G-003).
 *
 * Run standalone via `make test-g004` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-g001/test-g003 pattern). It audits, through the
 * four public symbols plus both audit-counter sets
 * (E-005 mapping counters via tests/abi/
 * mimir_audit_e005.h, G-003 span/object counters via
 * mimir_small_g003.h), that the span path and the
 * Phase 3 mapping-per-allocation core are integrated
 * through the ONE central dispatcher (alloc_any_locked)
 * with no route left unserved and no pointer left
 * ownerless:
 *
 *   F1 boundary routing (R-1): size 1 and size 1024
 *      move only the small counters; sizes 1025 and
 *      4096 move only the mapping counters.
 *   F2 span exhaustion (R-1 continued): allocating a
 *      full span capacity of one class plus 10 more
 *      live objects publishes at least one further span
 *      of that class (the class path falls back to a
 *      fresh central span, never to a spurious failure
 *      and never to a private mapping); every object is
 *      distinct and pattern-clean.
 *   F3 retention/retirement at the bound (G-003 bound,
 *      2 empty spans per class): forcing a third span
 *      of one class and freeing everything retires
 *      exactly the span beyond the bound.
 *   F4 two-registry free, interleaved (R-4): small and
 *      large blocks alternate; freeing in cross order
 *      moves exactly the owning registry's counters and
 *      leaves survivors' patterns intact.
 *   F5 free posture probes (R-4): interior span
 *      pointer, span-header pointer, span double free,
 *      mapping double free, foreign (libc) pointer and
 *      free(NULL) all move no counter and disturb no
 *      live block (posture probes in the C-008 idiom:
 *      invalid frees are UB at contract level; these
 *      record THIS implementation's non-destructive
 *      course only).
 *   F6 realloc matrix (R-3 / B-008): within-class stay,
 *      span shrink stay, cross-class span move,
 *      small-to-large move, large grow, large shrink
 *      across the cutoff staying on its mapping,
 *      SIZE_MAX failure atomicity on both registries,
 *      realloc(NULL) as malloc.
 *   F7 calloc (R-2): small calloc zeroes the whole
 *      class extent of a REUSED block; large calloc is
 *      mapping-served and zeroed; product overflow is
 *      exactly NULL and publishes nothing.
 *   F8 defined failures: malloc(0) posture NULL,
 *      malloc(SIZE_MAX) and malloc(SIZE_MAX - 100)
 *      exactly NULL with no counter movement in either
 *      registry. (A span backing failure cannot be
 *      forced through the public API on a healthy host
 *      -- no fault-injection hook exists in scope; the
 *      R-1 no-silent-reroute rule for that case rests on
 *      the code audit in the slice doc, INC-G004-02.)
 *
 * Final accounting: small allocs == small frees,
 * mapping published == retired, live spans within the
 * global retention bound (20 classes x 2).
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */

#define _POSIX_C_SOURCE 200809L

#include "mimir_alloc.h"
#include "mimir_small_g003.h"
#include "../abi/mimir_audit_e005.h"

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

static unsigned long load(const unsigned long *ctr)
{
    return __atomic_load_n(ctr, __ATOMIC_RELAXED);
}

struct snap {
    unsigned long pub, ret, span_pub, span_ret, sa, sf;
};

static struct snap take_snap(void)
{
    struct snap s;
    s.pub = load(&mimir_audit_alloc_published);
    s.ret = load(&mimir_audit_mapping_retired);
    s.span_pub = load(&mimir_audit_span_published);
    s.span_ret = load(&mimir_audit_span_retired);
    s.sa = load(&mimir_audit_small_alloc);
    s.sf = load(&mimir_audit_small_free);
    return s;
}

static void fill_pattern(unsigned char *p, size_t n,
                         unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)((i * 31u + seed * 17u +
                                (i >> 8)) & 0xffu);
    }
}

static int pattern_ok(const unsigned char *p, size_t n,
                      unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        unsigned char want =
            (unsigned char)((i * 31u + seed * 17u +
                             (i >> 8)) & 0xffu);
        if (p[i] != want) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    long page_l = sysconf(_SC_PAGESIZE);
    size_t page = (size_t)page_l;

    printf("test_g004: Phase 7 central fallback integration "
           "audit (page=%zu)\n", page);
    check(page_l > 0, "F0: page size discovered at runtime");

    /* ---------------- F1: boundary routing (R-1) ------- */
    printf("F1 boundary routing\n");
    {
        struct snap a = take_snap();
        void *s1 = mimir_malloc(1);
        void *s2 = mimir_malloc(1024);
        struct snap b = take_snap();
        check(s1 != NULL && s2 != NULL,
              "F1: sizes 1 and 1024 both served");
        check(b.sa - a.sa == 2 && b.sf == a.sf &&
              b.pub == a.pub && b.ret == a.ret,
              "F1: 1 and 1024 move only small counters");
        void *l1 = mimir_malloc(1025);
        void *l2 = mimir_malloc(4096);
        struct snap c = take_snap();
        check(l1 != NULL && l2 != NULL,
              "F1: sizes 1025 and 4096 both served");
        check(c.pub - b.pub == 2 && c.sa == b.sa &&
              c.sf == b.sf && c.ret == b.ret,
              "F1: 1025 and 4096 move only mapping counters");
        mimir_free(s1);
        mimir_free(s2);
        mimir_free(l1);
        mimir_free(l2);
        struct snap d = take_snap();
        check(d.sf - c.sf == 2 && d.ret - c.ret == 2,
              "F1: frees route back to the owning registry");
    }

    /* ---------------- F2: span exhaustion -------------- */
    printf("F2 span exhaustion (class 32)\n");
    {
        size_t cap = (page - 96) / 32;
        size_t n = cap + 10;
        size_t i, j;
        int distinct = 1, clean = 1;
        unsigned char **objs = malloc(n * sizeof *objs);
        struct snap a = take_snap();
        check(objs != NULL, "F2: test-side array allocated");
        for (i = 0; i < n; i++) {
            objs[i] = mimir_malloc(32);
            if (objs[i] == NULL) {
                clean = 0;
            } else {
                fill_pattern(objs[i], 32, (unsigned)i + 1);
            }
        }
        struct snap b = take_snap();
        check(b.sa - a.sa == n,
              "F2: every exhaustion object small-served");
        check(b.pub == a.pub && b.ret == a.ret,
              "F2: exhaustion publishes NO private mapping");
        check(b.span_pub - a.span_pub >= 1,
              "F2: exhaustion publishes a further span");
        for (i = 0; i < n && distinct; i++) {
            for (j = i + 1; j < n; j++) {
                if (objs[i] == objs[j]) {
                    distinct = 0;
                }
            }
        }
        check(distinct, "F2: all exhaustion objects distinct");
        for (i = 0; i < n; i++) {
            if (objs[i] == NULL ||
                !pattern_ok(objs[i], 32, (unsigned)i + 1)) {
                clean = 0;
            }
        }
        check(clean, "F2: all exhaustion patterns intact");
        for (i = 0; i < n; i++) {
            mimir_free(objs[i]);
        }
        free(objs);
        struct snap c = take_snap();
        check(c.sf - b.sf == n,
              "F2: every exhaustion object freed via spans");
    }

    /* ---------------- F3: retirement beyond the bound -- */
    printf("F3 retirement beyond the bound (class 1024)\n");
    {
        /* F1 left at most one empty class-1024 span
         * retained. Force the class to three live spans:
         * 2 * capacity + 1 live objects need 3 spans
         * whatever was retained. On free-all, exactly the
         * spans beyond 2 empty-per-class are retired. */
        size_t cap = (page - 96) / 1024;
        size_t n = 2 * cap + 1;
        size_t i;
        int clean = 1;
        unsigned char **objs = malloc(n * sizeof *objs);
        struct snap a = take_snap();
        unsigned long live_before =
            a.span_pub - a.span_ret;
        for (i = 0; i < n; i++) {
            objs[i] = mimir_malloc(1024);
            if (objs[i] == NULL) {
                clean = 0;
            } else {
                fill_pattern(objs[i], 1024, (unsigned)i + 41);
            }
        }
        struct snap b = take_snap();
        check(b.sa - a.sa == n && b.pub == a.pub,
              "F3: forcing batch all span-served");
        check((b.span_pub - b.span_ret) - live_before >= 2,
              "F3: class forced to at least 3 live spans");
        for (i = 0; i < n; i++) {
            if (objs[i] == NULL ||
                !pattern_ok(objs[i], 1024, (unsigned)i + 41)) {
                clean = 0;
            }
        }
        check(clean, "F3: forcing batch patterns intact");
        for (i = 0; i < n; i++) {
            mimir_free(objs[i]);
        }
        free(objs);
        struct snap c = take_snap();
        check(c.span_ret > b.span_ret,
              "F3: freeing past the bound retires a span");
        check((c.span_pub - c.span_ret) - live_before <= 2,
              "F3: retained empty spans of the class <= 2 "
              "beyond the pre-batch live count");
    }

    /* ---------------- F4: interleaved two-registry free  */
    printf("F4 interleaved two-registry free\n");
    {
        unsigned char *s1 = mimir_malloc(64);
        unsigned char *l1 = mimir_malloc(2048);
        unsigned char *s2 = mimir_malloc(128);
        unsigned char *l2 = mimir_malloc(8192);
        check(s1 != NULL && l1 != NULL && s2 != NULL &&
              l2 != NULL, "F4: both registries populated");
        if (s1 != NULL) fill_pattern(s1, 64, 61);
        if (l1 != NULL) fill_pattern(l1, 2048, 62);
        if (s2 != NULL) fill_pattern(s2, 128, 63);
        if (l2 != NULL) fill_pattern(l2, 8192, 64);
        struct snap a = take_snap();
        mimir_free(l1);
        mimir_free(s1);
        struct snap b = take_snap();
        check(b.ret - a.ret == 1 && b.sf - a.sf == 1 &&
              b.pub == a.pub && b.sa == a.sa,
              "F4: cross-order frees hit owning registries");
        check(s2 != NULL && pattern_ok(s2, 128, 63) &&
              l2 != NULL && pattern_ok(l2, 8192, 64),
              "F4: survivors intact after cross frees");
        mimir_free(s2);
        mimir_free(l2);
    }

    /* ---------------- F5: free posture probes ---------- */
    printf("F5 free posture probes\n");
    {
        unsigned char *s = mimir_malloc(64);
        unsigned char *l = mimir_malloc(2048);
        unsigned char *hdr;
        void *foreign;
        check(s != NULL && l != NULL,
              "F5: probe blocks allocated");
        if (s != NULL) fill_pattern(s, 64, 71);
        if (l != NULL) fill_pattern(l, 2048, 72);
        hdr = (unsigned char *)((uintptr_t)s &
                                ~(uintptr_t)(page - 1));
        struct snap a = take_snap();
        mimir_free(NULL);
        mimir_free(s + 8);       /* interior span pointer */
        mimir_free(hdr);         /* span header pointer */
        foreign = malloc(64);
        if (foreign != NULL) {
            mimir_free(foreign); /* foreign pointer */
        }
        struct snap b = take_snap();
        check(b.sf == a.sf && b.ret == a.ret &&
              b.sa == a.sa && b.pub == a.pub,
              "F5: NULL/interior/header/foreign frees move "
              "no counter");
        check(s != NULL && pattern_ok(s, 64, 71) &&
              l != NULL && pattern_ok(l, 2048, 72),
              "F5: live blocks undisturbed by probes");
        if (foreign != NULL) {
            memset(foreign, 0x5A, 64); /* still libc-owned */
            free(foreign);
        }
        mimir_free(s);
        struct snap c = take_snap();
        check(c.sf - b.sf == 1, "F5: real span free counts");
        mimir_free(s);           /* span double free */
        struct snap d = take_snap();
        check(d.sf == c.sf && d.ret == c.ret,
              "F5: span double free moves no counter");
        {
            unsigned char *s2 = mimir_malloc(64);
            check(s2 != NULL,
                  "F5: span class reusable after double free");
            if (s2 != NULL) {
                fill_pattern(s2, 64, 73);
                check(pattern_ok(s2, 64, 73),
                      "F5: reused object pattern-clean");
            }
            mimir_free(s2);
        }
        mimir_free(l);
        struct snap e = take_snap();
        check(e.ret - d.ret == 1, "F5: real mapping free counts");
        mimir_free(l);           /* mapping double free */
        struct snap f = take_snap();
        check(f.ret == e.ret && f.sf == e.sf,
              "F5: mapping double free moves no counter");
    }

    /* ---------------- F6: realloc matrix --------------- */
    printf("F6 realloc matrix\n");
    {
        unsigned char *s = mimir_malloc(50);
        unsigned char *g;
        check(s != NULL, "F6: span block allocated (50)");
        if (s != NULL) fill_pattern(s, 50, 81);
        g = mimir_realloc(s, 64);
        check(g == s, "F6: within-class realloc stays (50->64)");
        check(g != NULL && pattern_ok(g, 50, 81),
              "F6: within-class contents preserved");
        s = g;
        g = mimir_realloc(s, 32);
        check(g == s, "F6: span shrink stays (64->32)");
        s = g;
        {
            struct snap a = take_snap();
            g = mimir_realloc(s, 96);
            struct snap b = take_snap();
            check(g != NULL && g != s,
                  "F6: cross-class realloc moves (32->96)");
            check(g != NULL && pattern_ok(g, 32, 81),
                  "F6: cross-class contents preserved");
            check(b.sa - a.sa == 1 && b.sf - a.sf == 1 &&
                  b.pub == a.pub && b.ret == a.ret,
                  "F6: cross-class move is span-to-span only");
            s = g;
        }
        {
            struct snap a = take_snap();
            if (s != NULL) fill_pattern(s, 96, 82);
            g = mimir_realloc(s, 5000);
            struct snap b = take_snap();
            check(g != NULL && g != s,
                  "F6: small-to-large realloc moves (96->5000)");
            check(g != NULL && pattern_ok(g, 96, 82),
                  "F6: small-to-large contents preserved");
            check(b.pub - a.pub == 1 && b.sf - a.sf == 1 &&
                  b.sa == a.sa && b.ret == a.ret,
                  "F6: small-to-large publishes one mapping, "
                  "frees one span object");
            s = g;
        }
        {
            struct snap a = take_snap();
            g = mimir_realloc(s, 64);
            struct snap b = take_snap();
            check(g == s,
                  "F6: large shrink across cutoff stays on "
                  "its mapping (5000->64)");
            check(b.sa == a.sa && b.sf == a.sf &&
                  b.pub == a.pub && b.ret == a.ret,
                  "F6: large shrink moves no counter");
            g = mimir_realloc(s, 8192);
            check(g != NULL && g != s,
                  "F6: large grow moves (64->8192 by record)");
            if (g != NULL) {
                s = g;
            }
        }
        {
            unsigned char *t = mimir_malloc(64);
            unsigned char *r;
            check(t != NULL, "F6: failure-atomic span block");
            if (t != NULL) fill_pattern(t, 64, 83);
            r = mimir_realloc(t, SIZE_MAX);
            check(r == NULL,
                  "F6: span realloc SIZE_MAX fails NULL");
            check(t != NULL && pattern_ok(t, 64, 83),
                  "F6: span block untouched after failed grow");
            mimir_free(t);
            if (s != NULL) fill_pattern(s, 128, 84);
            r = mimir_realloc(s, SIZE_MAX);
            check(r == NULL,
                  "F6: mapping realloc SIZE_MAX fails NULL");
            check(s != NULL && pattern_ok(s, 128, 84),
                  "F6: mapping block untouched after failed grow");
        }
        {
            unsigned char *n = mimir_realloc(NULL, 64);
            check(n != NULL, "F6: realloc(NULL, 64) as malloc");
            mimir_free(n);
        }
        mimir_free(s);
    }

    /* ---------------- F7: calloc (R-2) ----------------- */
    printf("F7 calloc\n");
    {
        unsigned char *d = mimir_malloc(64);
        unsigned char *z;
        int zeroed = 1;
        size_t i;
        check(d != NULL, "F7: dirty block allocated");
        if (d != NULL) {
            memset(d, 0xA5, 64);
        }
        mimir_free(d);
        z = mimir_calloc(1, 64);
        check(z != NULL, "F7: small calloc served");
        if (z != NULL) {
            for (i = 0; i < 64; i++) {
                if (z[i] != 0) zeroed = 0;
            }
        }
        check(zeroed,
              "F7: reused small calloc zero over class extent");
        mimir_free(z);
        {
            struct snap a = take_snap();
            unsigned char *big = mimir_calloc(2, 2048);
            struct snap b = take_snap();
            int bz = 1;
            check(big != NULL, "F7: large calloc served");
            check(b.pub - a.pub == 1 && b.sa == a.sa,
                  "F7: large calloc is mapping-served only");
            if (big != NULL) {
                for (i = 0; i < 4096; i++) {
                    if (big[i] != 0) bz = 0;
                }
            }
            check(bz, "F7: large calloc extent zeroed");
            mimir_free(big);
        }
        {
            struct snap a = take_snap();
            void *ovf = mimir_calloc(SIZE_MAX, 2);
            struct snap b = take_snap();
            check(ovf == NULL, "F7: calloc overflow exactly NULL");
            check(b.pub == a.pub && b.sa == a.sa &&
                  b.span_pub == a.span_pub,
                  "F7: calloc overflow publishes nothing");
        }
    }

    /* ---------------- F8: defined failures ------------- */
    printf("F8 defined failures\n");
    {
        struct snap a = take_snap();
        check(mimir_malloc(0) == NULL,
              "F8: malloc(0) posture NULL (U-09 unresolved)");
        check(mimir_malloc(SIZE_MAX) == NULL,
              "F8: malloc(SIZE_MAX) exactly NULL");
        check(mimir_malloc(SIZE_MAX - 100) == NULL,
              "F8: malloc(SIZE_MAX-100) exactly NULL");
        struct snap b = take_snap();
        check(b.pub == a.pub && b.sa == a.sa &&
              b.span_pub == a.span_pub && b.ret == a.ret &&
              b.sf == a.sf && b.span_ret == a.span_ret,
              "F8: failures publish and retire nothing");
    }

    /* ---------------- Final accounting ----------------- */
    printf("Final accounting\n");
    {
        struct snap z = take_snap();
        check(z.sa == z.sf,
              "FINAL: small allocs == small frees (no live "
              "span object)");
        check(z.pub == z.ret,
              "FINAL: mappings published == retired (no live "
              "mapping)");
        check(z.span_pub - z.span_ret <= 40,
              "FINAL: live spans within 20 classes x 2 bound");
        printf("AUDIT span_published=%lu span_retired=%lu "
               "small_alloc=%lu small_free=%lu published=%lu "
               "retired=%lu live_spans=%lu\n",
               z.span_pub, z.span_ret, z.sa, z.sf, z.pub,
               z.ret, z.span_pub - z.span_ret);
    }

    printf("CHECKS: %d run, %d failed\n", checks, failures);
    if (failures == 0) {
        printf("RESULT: PASS\n");
        return 0;
    }
    printf("RESULT: FAIL\n");
    return 1;
}
