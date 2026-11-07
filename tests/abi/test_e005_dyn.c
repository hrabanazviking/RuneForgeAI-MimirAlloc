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
 * (1) Descriptor validation. Every block this allocator
 *     publishes carries its 96-byte descriptor (B-006
 *     S7.3) in the reserve page immediately before the
 *     user pointer: magic "MIMIRALL", mapping_base == the
 *     descriptor's own address, usable_offset == one
 *     system page, requested_size == the requested size,
 *     state == LIVE (1). A pointer serviced by any other
 *     allocator carries no such descriptor at that fixed
 *     offset. The test reads the descriptor ONLY for
 *     pointers returned by mimir_* calls in this process,
 *     whose reserve page is allocator-owned by
 *     construction; it never dereferences memory derived
 *     from a foreign pointer (the core's own S11.2 order).
 *     This is a test-side observation of an internal
 *     layout fact, NOT a public facility: the layout is
 *     expressly "still not a stable ABI" (B-006 S7.3).
 * (2) Mapped-bytes accounting. The allocator is the
 *     mapping-per-allocation baseline: each successful
 *     allocation adds exactly page_round_up(page + size)
 *     bytes to the process's total mapped extent (summed
 *     from /proc/self/maps) and each free removes it.
 *     Small libc allocations from the arena add nothing.
 *     (Mapping LINE counts are not used: adjacent
 *     anonymous mappings merge in /proc/self/maps, so a
 *     line count undercounts - recorded as INC-E005-02;
 *     the byte sum is exact, as the prototype runs in the
 *     slice document showed.)
 * (3) Test-side call counting (INC-E001-07 shape (a)):
 *     this program counts its own mimir_* call sites in
 *     local variables and requires the descriptor-
 *     validated count to match them exactly.
 *
 * Standalone on purpose: built and run via `make
 * test-e005-dyn`; NOT part of `make test`. See
 * docs/PHASE_5_AUDIT_HOOK_E005.md and the recorded run
 * tests/abi/recorded_run_e005.txt.
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

static long page_sz;
static size_t mapping_len_for(size_t size)
{
    size_t total = (size_t)page_sz + size;
    size_t pg = (size_t)page_sz;
    return (total + pg - 1) / pg * pg;
}

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
     * this program, counted here in the consumer. */
    unsigned long site_malloc = 0, site_calloc = 0;
    unsigned long validated = 0;

    /* Four mimir_malloc(64) through the .so. */
    unsigned char *p[4];
    for (int i = 0; i < 4; i++) {
        p[i] = mimir_malloc(64);
        site_malloc++;
        CHECK(p[i] != NULL);
        if (p[i] != NULL && descriptor_valid(p[i], 64)) {
            validated++;
        }
    }
    CHECK(validated == site_malloc);
    unsigned long long after_mimir = mapped_bytes();
    printf("MAPPED after_mimir_malloc_x4=%llu delta=%llu\n",
           after_mimir, after_mimir - base);
    CHECK(after_mimir - base == 4 * mapping_len_for(64));

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
           after_libc, after_libc - after_mimir);
    CHECK(after_libc == after_mimir);
    for (int i = 0; i < 4; i++) {
        free(q[i]);
    }
    CHECK(mapped_bytes() == after_mimir);

    /* Two mimir_calloc(16, 8) through the .so: descriptor
     * records the 128-byte product; extent reads zero. */
    unsigned char *c[2];
    for (int i = 0; i < 2; i++) {
        c[i] = mimir_calloc(16, 8);
        site_calloc++;
        CHECK(c[i] != NULL);
        if (c[i] != NULL) {
            if (descriptor_valid(c[i], 128)) {
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
           after_calloc, after_calloc - after_mimir);
    CHECK(after_calloc - after_mimir == 2 * mapping_len_for(128));

    /* realloc grow through the .so: the new pointer's
     * descriptor records the new size, contents are
     * preserved, and the mapped total is unchanged (one
     * mapping published, the old one retired, both the
     * same length at these sizes). */
    if (p[0] != NULL) {
        for (size_t j = 0; j < 64; j++) {
            p[0][j] = (unsigned char)(j + 3);
        }
        unsigned char *g = mimir_realloc(p[0], 96);
        CHECK(g != NULL);
        if (g != NULL) {
            CHECK(descriptor_valid(g, 96));
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
    CHECK(mapped_bytes() == after_calloc);

    /* Free every MimirAlloc block: the mapped total
     * returns exactly to the baseline. */
    for (int i = 0; i < 4; i++) {
        mimir_free(p[i]);
    }
    for (int i = 0; i < 2; i++) {
        mimir_free(c[i]);
    }
    unsigned long long final_bytes = mapped_bytes();
    printf("MAPPED final=%llu base=%llu\n", final_bytes, base);
    CHECK(final_bytes == base);

    printf("CHECKS: %d run, %d failed\n", checks_run,
           checks_failed);
    if (checks_failed == 0) {
        printf("RESULT: PASS\n");
        return 0;
    }
    printf("RESULT: FAIL\n");
    return 1;
}
