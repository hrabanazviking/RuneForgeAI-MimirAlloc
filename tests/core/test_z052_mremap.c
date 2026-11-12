/*
 * test_z052_mremap.c -- Z-052 mremap growth contract evidence.
 *
 * Slice: Z-052 (MIMIRALLOC_Z13_ROADMAP.md, Wave 6). Standalone
 * via `make test-z052`; NOT part of `make test` (the Z-048
 * pattern), so the standing gate's counts are untouched.
 *
 * What is pinned here (docs/Z13_MREMAP_GROWTH.md):
 *  1. Freed-neighbour extension: when the address room above
 *     a huge block is freed, an uncacheable growth EXTENDS
 *     in place -- the pointer is unchanged.
 *  2. Uncacheable growth ladder (34 -> 48 -> 64 MiB): every
 *     step succeeds with contents preserved, the Q-MAPPED
 *     delta is exactly the mapping-length delta, and the
 *     metadata check stays clean. (Whether each step stays
 *     or is kernel-relocated is reported, not asserted: it
 *     depends on the process's address layout, and both are
 *     mremap growths -- neither copies a byte.)
 *  3. A cacheable growth (inside the Z-048 pool bound)
 *     keeps the banked move: the pointer changes, contents
 *     preserved.
 *  4. A by-record growth the retained mapping already
 *     serves (after a shrink) keeps the banked move even at
 *     huge scale (the test_g004 F6 rule).
 *  5. Failure preservation at huge scale: SIZE_MAX and
 *     SIZE_MAX/2 grows return NULL, original byte-identical.
 *  6. A small mapping grown in one jump past the bound
 *     succeeds with its prefix preserved.
 * The test links the library OBJECTS directly (internal
 * queries via the banked test-only headers), never the .so.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mimir_alloc.h"
#include "mimir_pool_z048.h"
#include "mimir_trim_i003.h"
#include "mimir_integrity_j006.h"

static int checks = 0;
static int failures = 0;
static size_t page_sz = 0;

#define CHECK(cond, msg) do { \
        checks++; \
        if (!(cond)) { failures++; \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } \
    } while (0)

#define MIB ((size_t)1024 * 1024)

static size_t map_len(size_t request)
{
    size_t total = page_sz + request;
    return ((total + page_sz - 1) / page_sz) * page_sz;
}

/* One pattern byte per page, from the page index alone, so
 * any extent can be verified after any sequence of grows. */
static unsigned char pat_byte(size_t page_idx, unsigned int seed)
{
    return (unsigned char)((page_idx * 131u + seed * 17u + 7u) & 0xffu);
}

static void fill_pages(unsigned char *p, size_t bytes, unsigned int seed)
{
    size_t pg;
    for (pg = 0; pg * page_sz < bytes; pg++) {
        size_t last = pg * page_sz + page_sz - 1 < bytes ?
            pg * page_sz + page_sz - 1 : bytes - 1;
        p[pg * page_sz] = pat_byte(pg, seed);
        p[last] = pat_byte(pg, seed ^ 0x5au);
    }
}

static int pages_ok(const unsigned char *p, size_t bytes, unsigned int seed)
{
    size_t pg;
    for (pg = 0; pg * page_sz < bytes; pg++) {
        size_t last = pg * page_sz + page_sz - 1 < bytes ?
            pg * page_sz + page_sz - 1 : bytes - 1;
        if (p[pg * page_sz] != pat_byte(pg, seed) ||
            p[last] != pat_byte(pg, seed ^ 0x5au)) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    long ps = sysconf(_SC_PAGESIZE);
    size_t mapped0;

    printf("Z-052 mremap growth contract test\n");
    CHECK(ps > 0, "runtime page size discovered");
    if (ps <= 0) { printf("RESULT: FAIL\n"); return 1; }
    page_sz = (size_t)ps;
    mapped0 = mimir_core_mapped_bytes();

    /* ---- 1: freed-neighbour extension stays in place ---- */
    printf("Case 1: freed-neighbour extension 34 -> 48 MiB\n");
    {
        unsigned char *b = mimir_malloc(40 * MIB);
        unsigned char *a = mimir_malloc(34 * MIB);
        unsigned char *q;
        CHECK(a != NULL && b != NULL, "neighbour pair allocated");
        if (a != NULL && b != NULL) {
            fill_pages(a, 34 * MIB, 6);
            mimir_free(b); /* opens the room directly above a */
            q = mimir_realloc(a, 48 * MIB);
            CHECK(q == a, "growth into freed room stays in place");
            CHECK(q != NULL && pages_ok(q, 34 * MIB, 6),
                  "extension preserves the old extent");
            CHECK(mimir_core_metadata_check() == 0,
                  "metadata check 0 after in-place extension");
            mimir_free(q);
        } else {
            mimir_free(a);
            mimir_free(b);
        }
    }

    /* ---- 2: uncacheable growth ladder ---- */
    printf("Case 2: mremap ladder 34 -> 48 -> 64 MiB\n");
    {
        unsigned char *p = mimir_malloc(34 * MIB);
        unsigned char *q;
        size_t m0, m1, m2;
        CHECK(p != NULL, "malloc 34 MiB non-null");
        if (p != NULL) {
            fill_pages(p, 34 * MIB, 1);
            m0 = mimir_core_mapped_bytes();
            CHECK(m0 - mapped0 == map_len(34 * MIB),
                  "Q-MAPPED holds exactly the 34 MiB mapping");
            q = mimir_realloc(p, 48 * MIB);
            CHECK(q != NULL, "grow 34->48 MiB succeeds (mremap)");
            printf("  info: 34->48 MiB %s\n",
                   q == p ? "STAYED in place" : "RELOCATED by kernel");
            CHECK(q != NULL && pages_ok(q, 34 * MIB, 1),
                  "first 34 MiB preserved byte-for-byte (page-sampled)");
            m1 = mimir_core_mapped_bytes();
            CHECK(m1 - m0 == map_len(48 * MIB) - map_len(34 * MIB),
                  "Q-MAPPED delta is exactly the mapping delta");
            CHECK(mimir_core_metadata_check() == 0,
                  "metadata check 0 after mremap grow");
            if (q != NULL) {
                fill_pages(q, 48 * MIB, 1);
                p = q;
                q = mimir_realloc(q, 64 * MIB);
                CHECK(q != NULL, "grow 48->64 MiB succeeds");
                printf("  info: 48->64 MiB %s\n",
                       q == p ? "STAYED in place" : "RELOCATED by kernel");
                CHECK(q != NULL && pages_ok(q, 48 * MIB, 1),
                      "first 48 MiB preserved after second grow");
                m2 = mimir_core_mapped_bytes();
                CHECK(m2 - m1 == map_len(64 * MIB) - map_len(48 * MIB),
                      "second Q-MAPPED delta exact");
                CHECK(mimir_core_metadata_check() == 0,
                      "metadata check 0 after second grow");
                mimir_free(q);
            } else {
                mimir_free(p);
            }
            CHECK(mimir_core_pool_count() == 0,
                  "grown mapping not pooled at free (beyond the bound)");
            CHECK(mimir_core_mapped_bytes() == mapped0,
                  "Q-MAPPED back to baseline after free");
        }
    }

    /* ---- 3: cacheable growth keeps the banked move ---- */
    printf("Case 3: cacheable growth 4096 -> 8192 moves\n");
    {
        unsigned char *p = mimir_malloc(4096);
        unsigned char *q;
        CHECK(p != NULL, "malloc 4096 non-null");
        if (p != NULL) {
            fill_pages(p, 4096, 2);
            q = mimir_realloc(p, 8192);
            CHECK(q != NULL && q != p,
                  "grow within the pool bound moves (banked road)");
            CHECK(q != NULL && pages_ok(q, 4096, 2),
                  "moved grow preserves the old extent");
            mimir_free(q);
        }
        (void)mimir_core_trim_retained();
    }

    /* ---- 4: by-record growth inside usable keeps the move ---- */
    printf("Case 4: by-record grow after shrink (40 MiB, ->100, ->39 MiB)\n");
    {
        unsigned char *p = mimir_malloc(40 * MIB);
        unsigned char *r, *q;
        CHECK(p != NULL, "malloc 40 MiB non-null");
        if (p != NULL) {
            fill_pages(p, 40 * MIB, 3);
            r = mimir_realloc(p, 100);
            CHECK(r == p, "shrink returns the input pointer");
            if (r != NULL) {
                size_t i;
                int ok = 1;
                for (i = 0; i < 100; i++) {
                    r[i] = (unsigned char)(i ^ 0x33u);
                }
                q = mimir_realloc(r, 39 * MIB);
                CHECK(q != NULL && q != r,
                      "by-record grow inside usable moves (F6 rule)");
                if (q != NULL) {
                    for (i = 0; i < 100; i++) {
                        if (q[i] != (unsigned char)(i ^ 0x33u)) {
                            ok = 0;
                            break;
                        }
                    }
                }
                CHECK(ok, "by-record move preserves the recorded 100 bytes");
                mimir_free(q);
            } else {
                mimir_free(p);
            }
        }
        (void)mimir_core_trim_retained();
    }

    /* ---- 5: failure preservation at huge scale ---- */
    printf("Case 5: unsatisfiable grows preserve the original\n");
    {
        unsigned char *p = mimir_malloc(34 * MIB);
        CHECK(p != NULL, "malloc 34 MiB non-null (failure setup)");
        if (p != NULL) {
            fill_pages(p, 34 * MIB, 4);
            CHECK(mimir_realloc(p, SIZE_MAX) == NULL,
                  "realloc SIZE_MAX returns NULL");
            CHECK(pages_ok(p, 34 * MIB, 4),
                  "original intact after SIZE_MAX failure");
            CHECK(mimir_realloc(p, SIZE_MAX / 2) == NULL,
                  "realloc SIZE_MAX/2 returns NULL");
            CHECK(pages_ok(p, 34 * MIB, 4),
                  "original intact after SIZE_MAX/2 failure");
            CHECK(mimir_core_metadata_check() == 0,
                  "metadata check 0 after failed grows");
            mimir_free(p);
        }
    }

    /* ---- 6: one jump from a small mapping past the bound ---- */
    printf("Case 6: single jump 2048 -> 40 MiB\n");
    {
        unsigned char *p = mimir_malloc(2048);
        unsigned char *q;
        CHECK(p != NULL, "malloc 2048 non-null");
        if (p != NULL) {
            memset(p, 0x6d, 2048);
            q = mimir_realloc(p, 40 * MIB);
            CHECK(q != NULL, "jump grow succeeds");
            printf("  info: jump grow %s\n",
                   q == p ? "STAYED in place" : "RELOCATED by kernel");
            {
                int ok = 1;
                size_t i;
                if (q != NULL) {
                    for (i = 0; i < 2048; i++) {
                        if (q[i] != 0x6d) { ok = 0; break; }
                    }
                }
                CHECK(ok, "jump grow preserves the 2048-byte prefix");
            }
            CHECK(mimir_core_metadata_check() == 0,
                  "metadata check 0 after jump grow");
            mimir_free(q);
        }
    }

    CHECK(mimir_core_mapped_bytes() == mapped0,
          "Q-MAPPED back to baseline at end");
    printf("checks=%d failures=%d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
