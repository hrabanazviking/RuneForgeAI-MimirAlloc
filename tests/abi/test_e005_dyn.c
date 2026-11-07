/* test_e005_dyn.c - MimirAlloc slice E-005 call-path proof,
 * dynamic leg (shared library route R2 of E-003).
 *
 * This program links ONLY against build/libmimiralloc.so
 * (via `make test-e005-dyn`) and includes only the public
 * header. It cannot read the E-005 audit counters - they
 * are local in the .so, and the recorded run proves a
 * consumer that tries to link them against the .so fails
 * at link time. So this leg proves the same call path by
 * effects that only MimirAlloc's implementation produces,
 * observable from outside the library:
 *
 * (1) Serving-header validation. Every block this
 *     allocator publishes carries a 96-byte serving header
 *     (B-006 S7.3 layout discipline). From Phase 7 there
 *     are TWO serving forms, and this leg validates each
 *     pointer against the form that serves its size, at
 *     full strength for both:
 *     - Mapping form (requests above the 1024-byte
 *       cutoff, G-002 R-1): the banked descriptor in the
 *       reserve page immediately before the user pointer:
 *       magic "MIMIRALL", mapping_base == the descriptor's
 *       own address, usable_offset == one system page,
 *       requested_size == the requested size, state ==
 *       LIVE (1), mapping_length == the exact banked
 *       mapping length for the size. This is the original
 *       E-005 validation, unchanged.
 *     - Span form (requests of 1..1024 bytes): the span
 *       header at the start of the span's one system page
 *       (the page containing the user pointer): magic
 *       "MIMIRSPN", mapping_base == the header's own
 *       address, mapping_length == exactly one system
 *       page, usable_offset == 96, the offset-32 field ==
 *       the size class serving the request, usable_size
 *       == page - 96, state == LIVE (1), class index ==
 *       the class's index, live_count >= 1, and the user
 *       pointer at the class stride inside the object
 *       area (offset >= 96, (offset - 96) % class == 0).
 *     The test reads a serving header ONLY for pointers
 *     returned by mimir_* calls in this process, whose
 *     header page is allocator-owned by construction; it
 *     never dereferences memory derived from a foreign
 *     pointer (the core's own S11.2 order). This is a
 *     test-side observation of an internal layout fact,
 *     NOT a public facility: the layout is expressly
 *     "still not a stable ABI" (B-006 S7.3).
 *
 *     G-004 re-teach (INC-G003-02): before this re-teach
 *     the leg applied the mapping-form validation and the
 *     mapping-form mapped-bytes model to EVERY pointer,
 *     and segfaulted on the first span-served pointer:
 *     for a span object at span_base + 96, the read at
 *     (ptr - page) lands in an unmapped page. The re-teach
 *     is in the same spirit as G-003's INC-G003-01
 *     amendments to test_b007 / test_c006 / the E-005
 *     static leg: no check is weakened - span-served
 *     allocations get span-equivalent rigour (above), and
 *     mapping-served allocations keep every original
 *     descriptor check at full strength, now exercised in
 *     this leg by an explicit above-cutoff section the
 *     original script (all of whose sizes were <= 128,
 *     mapping-served only by the pre-Phase-7 accident)
 *     never needed.
 * (2) Mapped-bytes accounting. Observable from
 *     /proc/self/maps (byte sum; mapping LINE counts are
 *     not used: adjacent anonymous mappings merge, so a
 *     line count undercounts - INC-E005-02; the byte sum
 *     is exact):
 *     - Mapping form: each successful allocation adds
 *       exactly page_round_up(page + size) bytes and each
 *       free removes it, exactly as the original leg
 *       asserted.
 *     - Span form: allocations of one class share spans
 *       of exactly one system page each. The first live
 *       object of a class adds one page; further objects
 *       of that class add nothing while a live span of
 *       the class has room. Freeing span objects removes
 *       nothing while the span is retained empty under
 *       the G-003 retention bound (at most 2 empty spans
 *       per class), so after every block in this script
 *       is freed the mapped total stands at baseline +
 *       (retained empty spans x one page) - the exact
 *       span-aware expectation this leg asserts, derived
 *       from the classes the script touches, not read
 *       back from the allocator.
 *     Small libc allocations from the arena add nothing
 *     in either model.
 * (3) Test-side call counting (INC-E001-07 shape (a)):
 *     this program counts its own mimir_* call sites in
 *     local variables and requires the header-validated
 *     count to match them exactly, per serving form.
 *
 * Standalone on purpose: built and run via `make
 * test-e005-dyn`; NOT part of `make test`. See
 * docs/PHASE_5_AUDIT_HOOK_E005.md, the G-004 slice record
 * docs/PHASE_7_CENTRAL_FALLBACK_G004.md, and the recorded
 * run tests/core/recorded_run_g004.txt.
 */
#include "mimir_alloc.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks_run++;                                                   \
        if (cond) {                                                     \
            printf("  [ok] %s\n", #cond);                               \
        } else {                                                        \
            checks_failed++;                                            \
            printf("  [FAIL] %s (line %d)\n", #cond, __LINE__);         \
        }                                                               \
    } while (0)

/* Layout mirror of the internal descriptor (B-006 S7.3,
 * first 56 bytes); test-side only, see the file header. */
struct desc_probe {
    uint64_t magic;
    void *mapping_base;
    size_t mapping_length;
    size_t usable_offset;
    size_t requested_size;
    size_t usable_size;
    uint32_t state;
    uint32_t flags;
};

#define DESC_MAGIC UINT64_C(0x4D494D4952414C4C) /* "MIMIRALL" */

/* Layout mirror of the internal span header (G-003,
 * the full 96 bytes in the S7.3 field discipline);
 * test-side only, see the file header. */
struct span_probe {
    uint64_t magic;
    void *mapping_base;
    size_t mapping_length;
    size_t usable_offset;
    size_t class_size;
    size_t usable_size;
    uint32_t state;
    uint32_t class_index;
    void *list_next;
    void *list_prev;
    void *free_head;
    size_t live_count;
    size_t carve_count;
};

#define SPAN_MAGIC UINT64_C(0x4D494D495253504E) /* "MIMIRSPN" */
#define SPAN_HEADER_SIZE ((size_t)96)
#define SMALL_CUTOFF ((size_t)1024)

static const size_t class_sizes[20] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};

static size_t class_for(size_t size, size_t *idx_out)
{
    size_t i;
    for (i = 0; i < 20; i++) {
        if (size <= class_sizes[i]) {
            if (idx_out != NULL) {
                *idx_out = i;
            }
            return class_sizes[i];
        }
    }
    if (idx_out != NULL) {
        *idx_out = 20;
    }
    return 0;
}

static long page_sz;
static size_t mapping_len_for(size_t size)
{
    size_t total = (size_t)page_sz + size;
    size_t pg = (size_t)page_sz;
    return (total + pg - 1) / pg * pg;
}

/* Mapping-form validation: the original E-005 checks,
 * at full strength, for above-cutoff pointers only. */
static int descriptor_valid(const void *user_ptr, size_t size)
{
    const struct desc_probe *d =
        (const struct desc_probe *)((const char *)user_ptr -
                                    page_sz);
    return d->magic == DESC_MAGIC &&
           d->mapping_base == (const void *)d &&
           d->usable_offset == (size_t)page_sz &&
           d->requested_size == size && d->state == 1 &&
           d->mapping_length == mapping_len_for(size);
}

/* Span-form validation: the span-equivalent rigour for
 * pointers served from a size-class span. The header is
 * at the base of the system page containing the pointer. */
static int span_valid(const void *user_ptr, size_t size)
{
    const struct span_probe *s =
        (const struct span_probe *)((uintptr_t)user_ptr &
                                    ~(uintptr_t)(page_sz - 1));
    size_t idx = 20;
    size_t cs = class_for(size, &idx);
    size_t off;
    if (cs == 0) {
        return 0;
    }
    off = (size_t)((const char *)user_ptr - (const char *)s);
    return s->magic == SPAN_MAGIC &&
           s->mapping_base == (const void *)s &&
           s->mapping_length == (size_t)page_sz &&
           s->usable_offset == SPAN_HEADER_SIZE &&
           s->class_size == cs &&
           s->usable_size == (size_t)page_sz - SPAN_HEADER_SIZE &&
           s->state == 1 &&
           s->class_index == (uint32_t)idx &&
           s->live_count >= 1 &&
           off >= SPAN_HEADER_SIZE &&
           (off - SPAN_HEADER_SIZE) % cs == 0;
}

/* Route by size exactly as the allocator does (G-002
 * R-1): at or below the cutoff the span form must hold;
 * above it the mapping form must hold. */
static int served_valid(const void *user_ptr, size_t size)
{
    if (size <= SMALL_CUTOFF) {
        return span_valid(user_ptr, size);
    }
    return descriptor_valid(user_ptr, size);
}

static unsigned long long mapped_bytes(void)
{
    FILE *f = fopen("/proc/self/maps", "r");
    if (f == NULL) {
        return 0;
    }
    char line[512];
    unsigned long long total = 0, s, e;
    while (fgets(line, sizeof line, f) != NULL) {
        if (sscanf(line, "%llx-%llx", &s, &e) == 2) {
            total += e - s;
        }
    }
    fclose(f);
    return total;
}

int main(void)
{
    printf("E-005 call-path test (dynamic leg, linked "
           "against libmimiralloc.so only)\n");
    page_sz = sysconf(_SC_PAGESIZE);
    CHECK(page_sz > 0);
    printf("PAGE: %ld\n", page_sz);

    unsigned long long base = mapped_bytes();
    printf("MAPPED base=%llu\n", base);
    CHECK(base > 0);

    /* Test-side counters (shape (a)): call sites made by
     * this program, counted here in the consumer, split
     * by serving form as the Phase 7 allocator splits
     * them. */
    unsigned long site_malloc = 0, site_calloc = 0;
    unsigned long site_large = 0;
    unsigned long validated = 0, large_validated = 0;

    /* Four mimir_malloc(64) through the .so: span-served
     * (class 64). All four objects live in ONE span, so
     * the mapped total grows by exactly one system page,
     * not by four mapping lengths (the span-aware model,
     * header item (2)). The four pointers are distinct
     * objects at the class stride. */
    unsigned char *p[4];
    for (int i = 0; i < 4; i++) {
        p[i] = mimir_malloc(64);
        site_malloc++;
        CHECK(p[i] != NULL);
        if (p[i] != NULL && served_valid(p[i], 64)) {
            validated++;
        }
    }
    CHECK(p[0] != p[1] && p[0] != p[2] && p[0] != p[3] &&
          p[1] != p[2] && p[1] != p[3] && p[2] != p[3]);
    CHECK(validated == site_malloc);
    unsigned long long after_mimir = mapped_bytes();
    printf("MAPPED after_mimir_malloc_x4=%llu delta=%llu\n",
           after_mimir, after_mimir - base);
    CHECK(after_mimir - base == (unsigned long long)page_sz);

    /* Two mimir_malloc(4096) through the .so: above the
     * cutoff, mapping-served - the original E-005 form at
     * full strength. Each adds exactly its banked mapping
     * length; the span total does not move. */
    unsigned char *a[2];
    for (int i = 0; i < 2; i++) {
        a[i] = mimir_malloc(4096);
        site_large++;
        CHECK(a[i] != NULL);
        if (a[i] != NULL && descriptor_valid(a[i], 4096)) {
            large_validated++;
        }
    }
    CHECK(large_validated == site_large);
    unsigned long long after_large = mapped_bytes();
    printf("MAPPED after_mimir_malloc4096_x2=%llu delta=%llu\n",
           after_large, after_large - after_mimir);
    CHECK(after_large - after_mimir ==
          2 * mapping_len_for(4096));

    /* NEGATIVE: small libc allocations in the same process
     * add no mapped bytes and are not MimirAlloc blocks;
     * the mimir total must not move. */
    void *q[4];
    for (int i = 0; i < 4; i++) {
        q[i] = malloc(64);
        CHECK(q[i] != NULL);
    }
    unsigned long long after_libc = mapped_bytes();
    printf("MAPPED after_libc_malloc_x4=%llu delta_vs_mimir=%llu\n",
           after_libc, after_libc - after_large);
    CHECK(after_libc == after_large);
    for (int i = 0; i < 4; i++) {
        free(q[i]);
    }
    CHECK(mapped_bytes() == after_large);

    /* Two mimir_calloc(16, 8) through the .so: product
     * 128, span-served (class 128); the span header
     * records the class, the extent reads zero. Both
     * objects share ONE new class-128 span: exactly one
     * more page mapped. */
    unsigned char *c[2];
    for (int i = 0; i < 2; i++) {
        c[i] = mimir_calloc(16, 8);
        site_calloc++;
        CHECK(c[i] != NULL);
        if (c[i] != NULL) {
            if (served_valid(c[i], 128)) {
                validated++;
            }
            int zeroed = 1;
            for (size_t j = 0; j < 128; j++) {
                if (c[i][j] != 0) {
                    zeroed = 0;
                }
            }
            CHECK(zeroed);
        }
    }
    CHECK(validated == site_malloc + site_calloc);
    unsigned long long after_calloc = mapped_bytes();
    printf("MAPPED after_mimir_calloc_x2=%llu delta=%llu\n",
           after_calloc, after_calloc - after_large);
    CHECK(after_calloc - after_large ==
          (unsigned long long)page_sz);

    /* realloc grow within the small regime through the
     * .so: 64 -> 96 crosses a class boundary, so the
     * block moves to a fresh class-96 span (construct,
     * copy, retire the old object onto its span's free
     * list; the old span is retained, still mapped).
     * Contents are preserved and the mapped total grows
     * by exactly the one new span page. */
    if (p[0] != NULL) {
        for (size_t j = 0; j < 64; j++) {
            p[0][j] = (unsigned char)(j + 3);
        }
        unsigned char *g = mimir_realloc(p[0], 96);
        CHECK(g != NULL);
        if (g != NULL) {
            CHECK(served_valid(g, 96));
            int preserved = 1;
            for (size_t j = 0; j < 64; j++) {
                if (g[j] != (unsigned char)(j + 3)) {
                    preserved = 0;
                }
            }
            CHECK(preserved);
            p[0] = g;
        }
    }
    unsigned long long after_grow_small = mapped_bytes();
    printf("MAPPED after_realloc_64_to_96=%llu delta=%llu\n",
           after_grow_small, after_grow_small - after_calloc);
    CHECK(after_grow_small - after_calloc ==
          (unsigned long long)page_sz);

    /* realloc grow across the cutoff through the .so:
     * 64 -> 2048 moves a span object onto the mapping
     * path (the central fallback serving the new size).
     * The new pointer carries a full-strength mapping
     * descriptor for 2048, contents are preserved, and
     * the mapped total grows by exactly the new mapping
     * length (the vacated span object frees into its
     * retained span; nothing unmaps). */
    if (p[1] != NULL) {
        for (size_t j = 0; j < 64; j++) {
            p[1][j] = (unsigned char)(j + 11);
        }
        unsigned char *g = mimir_realloc(p[1], 2048);
        CHECK(g != NULL);
        if (g != NULL) {
            CHECK(descriptor_valid(g, 2048));
            int preserved = 1;
            for (size_t j = 0; j < 64; j++) {
                if (g[j] != (unsigned char)(j + 11)) {
                    preserved = 0;
                }
            }
            CHECK(preserved);
            p[1] = g;
        }
    }
    unsigned long long after_grow_large = mapped_bytes();
    printf("MAPPED after_realloc_64_to_2048=%llu delta=%llu\n",
           after_grow_large, after_grow_large - after_grow_small);
    CHECK(after_grow_large - after_grow_small ==
          mapping_len_for(2048));

    /* Free every MimirAlloc block. The mapping-served
     * bytes all leave; the three spans this script
     * touched (classes 64, 128, 96 - one span each) are
     * retained empty under the G-003 bound of 2 per
     * class, so the mapped total returns to baseline +
     * exactly three span pages: the retained-after-free
     * expectation of the span-aware model, stated in the
     * header before it is asserted here. */
    for (int i = 0; i < 4; i++) {
        mimir_free(p[i]);
    }
    for (int i = 0; i < 2; i++) {
        mimir_free(c[i]);
    }
    for (int i = 0; i < 2; i++) {
        mimir_free(a[i]);
    }
    unsigned long long final_bytes = mapped_bytes();
    printf("MAPPED final=%llu base=%llu retained=%llu\n",
           final_bytes, base, final_bytes - base);
    CHECK(final_bytes - base ==
          3 * (unsigned long long)page_sz);

    printf("CHECKS: %d run, %d failed\n", checks_run,
           checks_failed);
    if (checks_failed == 0) {
        printf("RESULT: PASS\n");
        return 0;
    }
    printf("RESULT: FAIL\n");
    return 1;
}
