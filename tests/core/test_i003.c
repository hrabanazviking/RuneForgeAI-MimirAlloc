/*
 * test_i003.c -- evidence test for slice I-003 (Phase 9
 * trimming and cache-release implementation; slice record:
 * docs/PHASE_9_TRIM_IMPLEMENTATION.md; semantics fixed by
 * docs/PHASE_9_BUDGET_AND_TELEMETRY_DESIGN.md Section 5).
 *
 * Run standalone via `make test-i003` (NOT part of
 * `make test`, whose composition is unchanged -- the
 * test-g003..test-h006 pattern).
 *
 * The binary links the library OBJECTS (LIBOBJS), so it
 * can read the G-003 span/object counters
 * (tests/core/mimir_small_g003.h), the E-005 mapping
 * counters (tests/abi/mimir_audit_e005.h), the G-007
 * retention-bound getter (tests/core/mimir_tune_g007.h),
 * the H-001 drain (tests/core/mimir_drain_h001.h) and the
 * I-003 trim surface itself
 * (tests/core/mimir_trim_i003.h), all INC-E001-07
 * shape (b). It proves, at quiescent points only (single
 * thread, no allocation in flight, I-002 S5.3):
 *
 * - warming three small classes (256 / 512 / 1024) past
 *   one span each and freeing every block retains exactly
 *   the G-005/G-007 bound of 2 EMPTY spans per class
 *   (6 spans, 6 x page bytes of Q-RETAINED), reconciled
 *   exactly against Q-MAPPED, the span counters and the
 *   object/mapping counter pairs;
 * - a partial trim (trim_retained_some) releases exactly
 *   the asked number of spans, class-fair: reuse probes
 *   show no warmed class was stripped bare by a partial
 *   trim until the fair walk reaches it, and the walk
 *   exhausts the lowest class first on the next call;
 * - a full trim releases exactly the retained-empty
 *   total: the returned span count, the Q-RETAINED drop,
 *   the G-003 span_retired delta and the OS-side
 *   anonymous rw byte drop in /proc/self/maps all agree
 *   byte-exactly, while live small blocks and a live
 *   large mapping stay byte-identical throughout;
 * - trim again (and a partial trim) on an empty cache is
 *   a no-op returning 0; the allocator stays fully usable
 *   after trims; the retention bound re-verifies after
 *   trimming (no ratchet); the H-001 drain still releases
 *   exactly the retained spans (unregressed); and a final
 *   drain brings every counter pair to exact
 *   reconciliation (published == retired, alloc == free).
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */

#define _POSIX_C_SOURCE 200809L

#include "mimir_alloc.h"
#include "mimir_small_g003.h"
#include "mimir_trim_i003.h"
#include "mimir_tune_g007.h"
#include "mimir_drain_h001.h"
#include "../abi/mimir_audit_e005.h"

#include <fcntl.h>
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

static void check_eq_size(size_t got, size_t want,
                          const char *label)
{
    checks++;
    printf("  [%s] %s (got %zu, want %zu)\n",
           got == want ? "ok" : "FAIL", label, got, want);
    if (got != want) {
        failures++;
    }
}

static void fill_pattern(void *ptr, size_t len,
                         unsigned char seed)
{
    unsigned char *p = (unsigned char *)ptr;
    size_t i;
    for (i = 0; i < len; i++) {
        p[i] = (unsigned char)(seed + (unsigned char)i);
    }
}

static int pattern_intact(const void *ptr, size_t len,
                          unsigned char seed)
{
    const unsigned char *p = (const unsigned char *)ptr;
    size_t i;
    for (i = 0; i < len; i++) {
        if (p[i] != (unsigned char)(seed + (unsigned char)i)) {
            return 0;
        }
    }
    return 1;
}

/* Sum of anonymous rw-p bytes in /proc/self/maps, read
 * with raw open/read into a static buffer: no stdio, no
 * heap allocation between the two readings of a trim, so
 * the only anonymous mappings that can move across the
 * measurement are the spans the trim itself releases. */
static char maps_buf[1 << 19];

static long maps_anon_rw_bytes(void)
{
    int fd = open("/proc/self/maps", O_RDONLY);
    ssize_t total = 0;
    char *line;
    long sum = 0;

    if (fd < 0) {
        return -1;
    }
    for (;;) {
        ssize_t n = read(fd, maps_buf + total,
                         sizeof(maps_buf) - 1 - (size_t)total);
        if (n <= 0) {
            break;
        }
        total += n;
        if ((size_t)total >= sizeof(maps_buf) - 1) {
            break;
        }
    }
    close(fd);
    maps_buf[total] = '\0';
    line = maps_buf;
    while (*line != '\0') {
        char *nl = strchr(line, '\n');
        unsigned long lo = 0, hi = 0;
        char perms[8] = {0};
        char tail[512];
        int fields;

        if (nl != NULL) {
            *nl = '\0';
        }
        tail[0] = '\0';
        fields = sscanf(line, "%lx-%lx %7s %*s %*s %*s %511s",
                        &lo, &hi, perms, tail);
        /* Anonymous: the pathname field (7th) is absent,
         * so sscanf stops at 3 fields. */
        if (fields == 3 && strcmp(perms, "rw-p") == 0) {
            sum += (long)(hi - lo);
        }
        if (nl == NULL) {
            break;
        }
        line = nl + 1;
    }
    return sum;
}

/* Allocate n blocks of req bytes, pattern-fill and verify
 * each, returning the block array (static storage: the
 * test itself must not allocate through the allocator it
 * is measuring beyond the scripted calls). */
#define MAX_WARM 128
static void *warm_blocks[MAX_WARM];

static int warm_alloc(size_t req, size_t n,
                      unsigned char seed)
{
    size_t i;
    if (n > MAX_WARM) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        warm_blocks[i] = mimir_malloc(req);
        if (warm_blocks[i] == NULL) {
            return 0;
        }
        fill_pattern(warm_blocks[i], req,
                     (unsigned char)(seed + (unsigned char)i));
    }
    for (i = 0; i < n; i++) {
        if (!pattern_intact(warm_blocks[i], req,
                            (unsigned char)(seed +
                                            (unsigned char)i))) {
            return 0;
        }
    }
    return 1;
}

static void warm_free(size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        mimir_free(warm_blocks[i]);
        warm_blocks[i] = NULL;
    }
}

static size_t span_capacity_for(size_t page, size_t cls)
{
    return (page - 96) / cls;
}

static size_t live_spans(void)
{
    return (size_t)(mimir_audit_span_published -
                    mimir_audit_span_retired);
}

int main(void)
{
    long page_l = sysconf(_SC_PAGESIZE);
    size_t page;
    size_t n256, n512, n1024;
    void *live_small[5];
    void *live_large;
    size_t map_len;
    size_t i;
    unsigned long retired0;
    unsigned long published0;
    void *probe;
    long anon_before, anon_after;
    size_t released;

    printf("test_i003: Phase 9 trim / cache-release evidence\n");
    check(page_l >= 4096, "page size sane");
    if (page_l < 4096) {
        printf("RESULT: FAIL (no usable page size)\n");
        return 1;
    }
    page = (size_t)page_l;
    printf("  page_size=%zu\n", page);
    n256 = span_capacity_for(page, 256) + 1;
    n512 = span_capacity_for(page, 512) + 1;
    n1024 = span_capacity_for(page, 1024) + 1;
    printf("  warm counts: 256x%zu 512x%zu 1024x%zu\n",
           n256, n512, n1024);

    check_eq_size(mimir_tune_get_retention_bound(), 2,
                  "retention bound is the default 2");
    check_eq_size(mimir_core_drain_retained(), 0,
                  "fresh process: drain releases 0");
    check_eq_size(mimir_core_retained_spans(), 0,
                  "fresh process: Q-RETAINED spans 0");
    check_eq_size(mimir_core_retained_bytes(), 0,
                  "fresh process: Q-RETAINED bytes 0");

    /* Live blocks that no trim may ever touch: five small
     * blocks (request 100, class 112) sharing one span,
     * and one large mapping (8192). */
    for (i = 0; i < 5; i++) {
        live_small[i] = mimir_malloc(100);
        check(live_small[i] != NULL, "live small allocated");
        if (live_small[i] != NULL) {
            fill_pattern(live_small[i], 100,
                         (unsigned char)(0x30 + (unsigned char)i));
        }
    }
    live_large = mimir_malloc(8192);
    check(live_large != NULL, "live large allocated");
    if (live_large != NULL) {
        fill_pattern(live_large, 8192, (unsigned char)0xC0);
    }

    /* Warm three classes past one span each, then free
     * everything: the bound retains 2 EMPTY spans each. */
    check(warm_alloc(256, n256, 0x40), "warm 256 filled");
    warm_free(n256);
    check(warm_alloc(512, n512, 0x50), "warm 512 filled");
    warm_free(n512);
    check(warm_alloc(1024, n1024, 0x60), "warm 1024 filled");
    warm_free(n1024);

    check_eq_size(mimir_core_retained_spans(), 6,
                  "after warm+free: 6 retained spans");
    check_eq_size(mimir_core_retained_bytes(), 6 * page,
                  "after warm+free: Q-RETAINED = 6 pages");
    map_len = ((page + 8192 + page - 1) / page) * page;
    check_eq_size(mimir_core_mapped_bytes(),
                  7 * page + map_len,
                  "Q-MAPPED = live span + retained + mapping");
    check_eq_size(live_spans(), 7,
                  "span counters: 7 live+retained spans");
    check_eq_size((size_t)(mimir_audit_small_alloc -
                           mimir_audit_small_free),
                  5, "object counters: 5 live small objects");
    check_eq_size((size_t)(mimir_audit_alloc_published -
                           mimir_audit_mapping_retired),
                  1, "mapping counters: 1 live mapping");

    /* Partial trims: exact counts, class-fair order. */
    retired0 = mimir_audit_span_retired;
    check_eq_size(mimir_core_trim_retained_some(0), 0,
                  "trim_some(0) releases 0");
    check_eq_size(mimir_core_retained_spans(), 6,
                  "trim_some(0) changed nothing");
    check_eq_size(mimir_core_trim_retained_some(2), 2,
                  "trim_some(2) releases exactly 2");
    check_eq_size(mimir_core_retained_spans(), 4,
                  "after trim_some(2): 4 retained");
    check_eq_size(mimir_core_retained_bytes(), 4 * page,
                  "after trim_some(2): Q-RETAINED 4 pages");
    check_eq_size((size_t)(mimir_audit_span_retired -
                           retired0),
                  2, "span_retired delta = 2");
    for (i = 0; i < 5; i++) {
        check(live_small[i] != NULL &&
              pattern_intact(live_small[i], 100,
                             (unsigned char)(0x30 +
                                             (unsigned char)i)),
              "live small intact after partial trim");
    }
    check(live_large != NULL &&
          pattern_intact(live_large, 8192, (unsigned char)0xC0),
          "live large intact after partial trim");

    /* Class-fairness probes: after trim_some(2) each of
     * classes 256 and 512 must still hold a retained span
     * (a greedy walk would have stripped one bare), so a
     * fresh allocation of each reuses a span and publishes
     * nothing new. */
    published0 = mimir_audit_span_published;
    probe = mimir_malloc(256);
    check(probe != NULL, "probe 256 allocated");
    check_eq_size((size_t)(mimir_audit_span_published -
                           published0),
                  0, "probe 256 reused a retained span");
    mimir_free(probe);
    published0 = mimir_audit_span_published;
    probe = mimir_malloc(512);
    check(probe != NULL, "probe 512 allocated");
    check_eq_size((size_t)(mimir_audit_span_published -
                           published0),
                  0, "probe 512 reused a retained span");
    mimir_free(probe);
    check_eq_size(mimir_core_retained_spans(), 4,
                  "probes freed: retained back to 4");

    check_eq_size(mimir_core_trim_retained_some(1), 1,
                  "trim_some(1) releases exactly 1");
    check_eq_size(mimir_core_retained_spans(), 3,
                  "after trim_some(1): 3 retained");
    /* The fresh walk starts at the lowest class, so the
     * span just released was class 256's last retained
     * one: a 256 probe must now publish a fresh span. */
    published0 = mimir_audit_span_published;
    probe = mimir_malloc(256);
    check(probe != NULL, "probe 256 (exhausted) allocated");
    check_eq_size((size_t)(mimir_audit_span_published -
                           published0),
                  1, "probe 256 published a fresh span");
    mimir_free(probe);
    check_eq_size(mimir_core_retained_spans(), 4,
                  "probe freed: retained back to 4");

    /* Full trim: returned count, Q-RETAINED drop, counter
     * delta and the OS-side anonymous figure agree. */
    retired0 = mimir_audit_span_retired;
    anon_before = maps_anon_rw_bytes();
    check(anon_before > 0, "anon figure readable before trim");
    released = mimir_core_trim_retained();
    anon_after = maps_anon_rw_bytes();
    check(anon_after >= 0, "anon figure readable after trim");
    check_eq_size(released, 4, "full trim releases 4 spans");
    check_eq_size(mimir_core_retained_spans(), 0,
                  "after full trim: 0 retained spans");
    check_eq_size(mimir_core_retained_bytes(), 0,
                  "after full trim: Q-RETAINED 0");
    check_eq_size((size_t)(mimir_audit_span_retired -
                           retired0),
                  4, "span_retired delta = 4");
    if (anon_before > 0 && anon_after >= 0) {
        check_eq_size((size_t)(anon_before - anon_after),
                      4 * page,
                      "anon rw drop = released bytes");
        printf("  anon_rw_before=%ld anon_rw_after=%ld "
               "released_bytes=%zu\n",
               anon_before, anon_after, released * page);
    }
    check_eq_size(live_spans(), 1,
                  "only the live small span remains");
    for (i = 0; i < 5; i++) {
        check(live_small[i] != NULL &&
              pattern_intact(live_small[i], 100,
                             (unsigned char)(0x30 +
                                             (unsigned char)i)),
              "live small intact after full trim");
    }
    check(live_large != NULL &&
          pattern_intact(live_large, 8192, (unsigned char)0xC0),
          "live large intact after full trim");

    check_eq_size(mimir_core_trim_retained(), 0,
                  "trim again is a no-op (0)");
    check_eq_size(mimir_core_trim_retained_some(3), 0,
                  "partial trim on empty cache is 0");

    /* Allocator usable after trim; bound re-verified. */
    check(warm_alloc(256, n256, 0x70), "re-warm 256 filled");
    warm_free(n256);
    check_eq_size(mimir_core_retained_spans(), 2,
                  "bound re-verified: 2 retained, no ratchet");
    check_eq_size(mimir_core_trim_retained(), 2,
                  "trim releases the re-warmed 2");

    /* H-001 drain unregressed on the shared walk. */
    check(warm_alloc(512, n512, 0x78), "re-warm 512 filled");
    warm_free(n512);
    check_eq_size(mimir_core_retained_spans(), 2,
                  "drain leg: 2 retained");
    check_eq_size(mimir_core_drain_retained(), 2,
                  "drain releases exactly the 2");
    check_eq_size(mimir_core_drain_retained(), 0,
                  "drain again is a no-op (0)");

    /* Free the live blocks; their span becomes the one
     * retained span, the final drain releases it, and
     * every counter pair reconciles exactly. */
    mimir_free(live_large);
    for (i = 0; i < 5; i++) {
        mimir_free(live_small[i]);
    }
    check_eq_size(mimir_core_retained_spans(), 1,
                  "live span emptied: 1 retained");
    check_eq_size(mimir_core_drain_retained(), 1,
                  "final drain releases the last span");
    check_eq_size(mimir_core_retained_bytes(), 0,
                  "final: Q-RETAINED 0");
    check_eq_size(mimir_core_mapped_bytes(), 0,
                  "final: Q-MAPPED 0");
    check(mimir_audit_span_published ==
          mimir_audit_span_retired,
          "span counters reconcile (published == retired)");
    check(mimir_audit_small_alloc == mimir_audit_small_free,
          "object counters reconcile (alloc == free)");
    check(mimir_audit_alloc_published ==
          mimir_audit_mapping_retired,
          "mapping counters reconcile (published == retired)");
    printf("  totals: span_published=%lu span_retired=%lu "
           "small_alloc=%lu small_free=%lu "
           "map_published=%lu map_retired=%lu\n",
           mimir_audit_span_published,
           mimir_audit_span_retired,
           mimir_audit_small_alloc, mimir_audit_small_free,
           mimir_audit_alloc_published,
           mimir_audit_mapping_retired);
    printf("checks=%d failures=%d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
