/*
 * test_b007.c -- C-004 B-007 completeness-audit evidence program.
 *
 * Slice C-004. Exercises, through the public L1 boundary only
 * (include/mimir_alloc.h), the behaviours that
 * docs/PHASE_3_ARITHMETIC_ALIGNMENT_INIT_ERROR_DESIGN.md (B-007)
 * claims for the banked C-003 core, and reads back the
 * in-mapping descriptor (B-006 S7.3 layout, at the user pointer
 * minus one system page per B-006 S7.1) to prove the
 * descriptor/metadata clean-initialization duties of B-007
 * C1.4. The full item-by-item audit that this program is the
 * evidence for is tests/core/AUDIT_B007_C004.md.
 *
 * What is proved here (each check names its B-007 item):
 *   - Bootstrap / clean start (C2, F-08 posture): the first
 *     mimir_malloc call in this process succeeds with no init
 *     step, no constructor and no prior call of any kind.
 *   - Boundary sizes around page round-up (A5/A6): requested
 *     sizes page-1, page, page+1, 2*page-1, 2*page, 2*page+1
 *     (and 1) succeed, and the descriptor mapping_length read
 *     back equals reserve page + page_round_up(requested)
 *     exactly -- the round-up is observed, not assumed.
 *   - Alignment (B.1): every returned pointer is aligned to
 *     the runtime system page size (the by-construction claim)
 *     and to _Alignof(max_align_t) (the non-numeric suitably
 *     aligned claim, MA-6.1-MALLOC-01 by analogy). No numeric
 *     contract alignment is claimed (U-15 open).
 *   - Descriptor clean-init (C1.4): on every allocation
 *     inspected, magic, mapping_base (== the descriptor own
 *     address), mapping_length, usable_offset (== one system
 *     page), requested_size, usable_size (== mapping_length -
 *     usable_offset, and >= requested_size, the A8 publication
 *     gate), state == LIVE, flags == 0, reserved[24] all zero,
 *     and -- while it is the only live block -- list_next ==
 *     list_prev == NULL.
 *   - User contents (C1): patterns written over the full
 *     requested extent read back exactly, and live blocks hold
 *     distinct patterns undisturbed across partial frees and
 *     further allocations (C1.2: the allocator never touches
 *     user bytes). This program deliberately never asserts
 *     that fresh storage reads as zero: B-007 C1.1/C1.3 forbid
 *     that assertion for mimir_malloc.
 *   - Overflow refusals (A3/A5, D.0): SIZE_MAX, SIZE_MAX - 8,
 *     SIZE_MAX - page + 1 (fails the A3 addition check) and
 *     SIZE_MAX - page (passes A3 exactly, fails the A5
 *     round-up check in L6) all return NULL.
 *   - Unsatisfiable request (F-01 disposition): SIZE_MAX / 2
 *     is representable and passes the checked arithmetic, its
 *     mapping cannot be satisfied, and it returns NULL.
 *   - Health after failed acquire (A.3 rollback posture, the
 *     observable side of F-06 discipline): after every failed
 *     request above, a sentinel live block pattern is verified
 *     undisturbed and a fresh allocation succeeds, is usable
 *     and frees normally; mimir_free(NULL) after the failures
 *     returns normally; a fresh allocation after all frees
 *     succeeds.
 *   - Zero-size: mimir_malloc(0) returns NULL -- this
 *     implementation posture ONLY. Zero-size is
 *     [UNRESOLVED: U-09]; this check does not settle it.
 *
 * What is deliberately NOT checked, and why (see the audit):
 *   - allocation contents before writing (unspecified, never
 *     zero-promised -- B-007 C1.1, C1.3; INC-B007-3);
 *   - any errno value (not promised -- U-10 / B-007 D.1.1);
 *   - invalid frees, double free, use after free (undefined
 *     behaviour -- B-007 D.3; no defined outcome exists);
 *   - the failure branches of A8 / A9 / A10, the F-06 munmap
 *     rollback, F-07 lock failure and F-11 page-size discovery
 *     failure: none can be triggered through the public API
 *     on a healthy host and no fault-injection hook exists in
 *     Phase 3 scope, so those items rest on code inspection
 *     in the audit (INC-C004-02), not on this program.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if every
 * check passed. Prints only facts it observed.
 */

#include "mimir_alloc.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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

/* Descriptor field readers (B-006 S7.3 offsets, design layout,
 * not a stable ABI). The descriptor sits at the start of the
 * reserve page, one system page before the user pointer
 * (B-006 S7.1). Fields are read with memcpy so no alignment
 * or aliasing assumption is made by the test itself. */
#define DESC_OFF_MAGIC           0u
#define DESC_OFF_MAPPING_BASE    8u
#define DESC_OFF_MAPPING_LENGTH 16u
#define DESC_OFF_USABLE_OFFSET  24u
#define DESC_OFF_REQUESTED_SIZE 32u
#define DESC_OFF_USABLE_SIZE    40u
#define DESC_OFF_STATE          48u
#define DESC_OFF_FLAGS          52u
#define DESC_OFF_LIST_NEXT      56u
#define DESC_OFF_LIST_PREV      64u
#define DESC_OFF_RESERVED       72u
#define DESC_RESERVED_LEN       24u

#define DESC_MAGIC UINT64_C(0x4D494D4952414C4C) /* "MIMIRALL" */
#define DESC_STATE_LIVE 1u

static uint64_t rd_u64(const unsigned char *d, size_t off)
{
    uint64_t v;
    memcpy(&v, d + off, sizeof v);
    return v;
}

static size_t rd_size(const unsigned char *d, size_t off)
{
    size_t v;
    memcpy(&v, d + off, sizeof v);
    return v;
}

static uint32_t rd_u32(const unsigned char *d, size_t off)
{
    uint32_t v;
    memcpy(&v, d + off, sizeof v);
    return v;
}

static const void *rd_ptr(const unsigned char *d, size_t off)
{
    const void *v;
    memcpy(&v, d + off, sizeof v);
    return v;
}

static const unsigned char *descriptor_of(const void *user,
                                          size_t page)
{
    return (const unsigned char *)user - page;
}

/* G-003 shape awareness (docs/PHASE_7_SMALL_OBJECT_PATH_G003.md,
 * INC-G003-01): from Phase 7, requests of 1..1024 bytes are
 * served from size-class spans (banked G-002 design), whose
 * serving header sits at the start of the span's one system
 * page -- NOT one page before the user pointer, and the
 * pointer is 16-aligned, not page-aligned (G-002 S9 expressly
 * disowns the mapping form's page alignment as an accident
 * no caller or test may rely on). The descriptor read-backs
 * in this file therefore branch on the serving form:
 * page-aligned user pointers (the mapping path) keep the
 * original B-006 S7.3 descriptor checks at full strength;
 * span-served pointers get the equivalent clean-init
 * verification of the span header (G-002 S7 / G-003 layout:
 * magic "MIMIRSPN", base == header address, mapping_length
 * == one system page, usable_offset == 96, class-size field
 * == the class serving the request, usable_size ==
 * mapping_length - 96, state LIVE, class index in range,
 * live_count >= 1, pointer at the class stride inside the
 * object area). No contract-level check in this file
 * changed: patterns, max_align_t alignment, overflow
 * refusals, health cycles and postures are untouched. */
#define SPAN_MAGIC UINT64_C(0x4D494D495253504E) /* "MIMIRSPN" */
#define SPAN_OFF_CLASS_INDEX    52u
#define SPAN_OFF_LIVE_COUNT     80u

static size_t g003_class_for(size_t n, size_t *idx_out)
{
    static const size_t cs[20] = {
        16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
        224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
    };
    size_t i;

    for (i = 0; i < 20; i++) {
        if (n <= cs[i]) {
            if (idx_out != NULL) {
                *idx_out = i;
            }
            return cs[i];
        }
    }
    if (idx_out != NULL) {
        *idx_out = 20;
    }
    return 0;
}

static const unsigned char *g003_span_header_of(const void *user,
                                                size_t page)
{
    return (const unsigned char *)((uintptr_t)user &
                                   ~(uintptr_t)(page - 1));
}

static int g003_span_clean(const unsigned char *h,
                           const void *user, size_t page,
                           size_t requested)
{
    size_t idx = 20;
    size_t cs = g003_class_for(requested, &idx);
    size_t off;

    if (cs == 0) {
        return 0;
    }
    off = (size_t)((const unsigned char *)user - h);
    return rd_u64(h, DESC_OFF_MAGIC) == SPAN_MAGIC &&
           rd_ptr(h, DESC_OFF_MAPPING_BASE) == (const void *)h &&
           rd_size(h, DESC_OFF_MAPPING_LENGTH) == page &&
           rd_size(h, DESC_OFF_USABLE_OFFSET) == 96 &&
           rd_size(h, DESC_OFF_REQUESTED_SIZE) == cs &&
           rd_size(h, DESC_OFF_USABLE_SIZE) == page - 96 &&
           rd_u32(h, DESC_OFF_STATE) == DESC_STATE_LIVE &&
           rd_u32(h, SPAN_OFF_CLASS_INDEX) == (uint32_t)idx &&
           rd_size(h, SPAN_OFF_LIVE_COUNT) >= 1 &&
           off >= 96 && (off - 96) % cs == 0;
}


static int reserved_is_zero(const unsigned char *d)
{
    size_t i;

    for (i = 0; i < DESC_RESERVED_LEN; i++) {
        if (d[DESC_OFF_RESERVED + i] != 0) {
            return 0;
        }
    }
    return 1;
}

/* Full C1.4 clean-init verdict for one live descriptor whose
 * block is currently the ONLY live block (so its registry
 * links must both be NULL). Returns 1 if every promised field
 * is exactly as B-006 S7.3 / B-007 C1.4 require. */
static int descriptor_clean(const unsigned char *d,
                            const void *user, size_t page,
                            size_t requested,
                            size_t expected_mapping_length)
{
    size_t mlen = rd_size(d, DESC_OFF_MAPPING_LENGTH);
    size_t uoff = rd_size(d, DESC_OFF_USABLE_OFFSET);
    size_t usable = rd_size(d, DESC_OFF_USABLE_SIZE);

    return rd_u64(d, DESC_OFF_MAGIC) == DESC_MAGIC &&
           rd_ptr(d, DESC_OFF_MAPPING_BASE) == (const void *)d &&
           mlen == expected_mapping_length &&
           uoff == page &&
           rd_size(d, DESC_OFF_REQUESTED_SIZE) == requested &&
           usable == mlen - uoff &&
           usable >= requested &&
           rd_u32(d, DESC_OFF_STATE) == DESC_STATE_LIVE &&
           rd_u32(d, DESC_OFF_FLAGS) == 0 &&
           reserved_is_zero(d) &&
           rd_ptr(d, DESC_OFF_LIST_NEXT) == NULL &&
           rd_ptr(d, DESC_OFF_LIST_PREV) == NULL &&
           (const unsigned char *)user == d + page;
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

static size_t page_round_up(size_t x, size_t page)
{
    return ((x + (page - 1)) / page) * page;
}

int main(void)
{
    long queried = sysconf(_SC_PAGESIZE);
    size_t page = queried > 0 ? (size_t)queried : 0;
    char label[192];
    size_t i;

    printf("C-004 B-007 audit test\n");
    printf("runtime system page size: %zu bytes\n", page);
    check(page > 0, "runtime page size discovered (A2 input)");

    /* --- Bootstrap / first call (C2, F-08 posture) --------
     * This is the first mimir_malloc call in the process. No
     * init function exists or was called; the registry and
     * counters are static storage in their defined starting
     * state and the L6 lock is statically initialised. */
    {
        unsigned char *p = (unsigned char *)mimir_malloc(37);

        check(p != NULL,
              "C2/F-08: first call in process succeeds, no init step");
        if (p != NULL) {
            check((uintptr_t)p % _Alignof(max_align_t) == 0,
                  "B.1: first pointer aligned to max_align_t "
                  "(G-002 S9: page alignment is the mapping "
                  "form's accident, not the class path's claim)");
            check(g003_span_clean(g003_span_header_of(p, page),
                                  p, page, 37),
                  "C1.4: first serving header (span form) fully "
                  "clean-initialised (G-003; the descriptor form "
                  "of this check runs on the large sizes below)");
            fill_pattern(p, 37, 5);
            check(pattern_ok(p, 37, 5),
                  "C1.2: first block pattern verifies over full extent");
            mimir_free(p);
        }
    }

    /* --- Boundary sizes around page round-up (A5/A6, B.1,
     * C1.4) ------------------------------------------------ */
    {
        size_t sizes[7];
        size_t nsizes = 0;

        sizes[nsizes++] = 1;
        if (page > 0) {
            sizes[nsizes++] = page - 1;
            sizes[nsizes++] = page;
            sizes[nsizes++] = page + 1;
            sizes[nsizes++] = 2 * page - 1;
            sizes[nsizes++] = 2 * page;
            sizes[nsizes++] = 2 * page + 1;
        }
        for (i = 0; i < nsizes; i++) {
            size_t n = sizes[i];
            size_t want_mlen = page + page_round_up(n, page);
            unsigned char *p = (unsigned char *)mimir_malloc(n);

            snprintf(label, sizeof label,
                     "A5/A6: malloc(%zu) non-null", n);
            check(p != NULL, label);
            if (p == NULL) {
                continue;
            }
            if (n > 1024) {
                snprintf(label, sizeof label,
                         "B.1: malloc(%zu) system-page aligned", n);
                check((uintptr_t)p % page == 0, label);
                snprintf(label, sizeof label,
                         "C1.4/A5/A6/A8: malloc(%zu) descriptor "
                         "clean, mapping_length == %zu",
                         n, want_mlen);
                check(descriptor_clean(descriptor_of(p, page), p,
                                       page, n, want_mlen), label);
            } else {
                snprintf(label, sizeof label,
                         "C1.4 (span form): malloc(%zu) span "
                         "header clean, one system page", n);
                check(g003_span_clean(
                          g003_span_header_of(p, page), p, page, n),
                      label);
            }
            snprintf(label, sizeof label,
                     "B.1: malloc(%zu) aligned to max_align_t", n);
            check((uintptr_t)p % _Alignof(max_align_t) == 0,
                  label);
            fill_pattern(p, n, (unsigned int)(11 + i));
            snprintf(label, sizeof label,
                     "C1.2: malloc(%zu) pattern verifies over "
                     "full extent", n);
            check(pattern_ok(p, n, (unsigned int)(11 + i)),
                  label);
            mimir_free(p);
        }
    }

    /* --- Repeated clean-init (C1.4, C2) --------------------
     * Every fresh descriptor, across many cycles, carries
     * zeroed flags and reserved bytes and LIVE state; no
     * residue from a previous mapping is ever published. */
    {
        int all_clean = 1;

        for (i = 0; i < 24; i++) {
            size_t n = (i * 97u) % 2048u + 1u;
            unsigned char *p = (unsigned char *)mimir_malloc(n);

            if (p == NULL) {
                all_clean = 0;
                break;
            }
            if (n > 1024) {
                const unsigned char *d = descriptor_of(p, page);

                if (rd_u64(d, DESC_OFF_MAGIC) != DESC_MAGIC ||
                    rd_u32(d, DESC_OFF_STATE) != DESC_STATE_LIVE ||
                    rd_u32(d, DESC_OFF_FLAGS) != 0 ||
                    !reserved_is_zero(d)) {
                    all_clean = 0;
                }
            } else {
                const unsigned char *h =
                    g003_span_header_of(p, page);

                if (rd_u64(h, DESC_OFF_MAGIC) != SPAN_MAGIC ||
                    rd_u32(h, DESC_OFF_STATE) != DESC_STATE_LIVE ||
                    rd_size(h, DESC_OFF_REQUESTED_SIZE) !=
                        g003_class_for(n, NULL)) {
                    all_clean = 0;
                }
            }
            mimir_free(p);
            if (!all_clean) {
                break;
            }
        }
        check(all_clean,
              "C1.4: 24 alloc/free cycles, every serving header "
              "clean-init (descriptor or span form, G-003)");
    }

    /* --- Interleaved live blocks (C1.2, Invariant 1) ------ */
    {
        enum { NBLK = 8, NNEW = 4 };
        unsigned char *blk[NBLK];
        unsigned char *extra[NNEW];
        size_t bsz[NBLK];
        int ok = 1;

        for (i = 0; i < (size_t)NBLK; i++) {
            bsz[i] = 100u + i * 333u;
            blk[i] = (unsigned char *)mimir_malloc(bsz[i]);
            if (blk[i] == NULL) {
                ok = 0;
            } else {
                fill_pattern(blk[i], bsz[i],
                             (unsigned int)(60 + i));
            }
        }
        check(ok, "interleave: eight simultaneous allocations succeed");
        mimir_free(blk[1]);
        mimir_free(blk[3]);
        mimir_free(blk[5]);
        blk[1] = blk[3] = blk[5] = NULL;
        for (i = 0; i < (size_t)NNEW; i++) {
            extra[i] = (unsigned char *)mimir_malloc(777u + i);
            if (extra[i] == NULL) {
                ok = 0;
            } else {
                fill_pattern(extra[i], 777u + i,
                             (unsigned int)(90 + i));
            }
        }
        check(ok, "interleave: four allocations after partial frees succeed");
        ok = 1;
        for (i = 0; i < (size_t)NBLK; i++) {
            if (blk[i] != NULL &&
                !pattern_ok(blk[i], bsz[i],
                            (unsigned int)(60 + i))) {
                ok = 0;
            }
        }
        for (i = 0; i < (size_t)NNEW; i++) {
            if (extra[i] != NULL &&
                !pattern_ok(extra[i], 777u + i,
                            (unsigned int)(90 + i))) {
                ok = 0;
            }
        }
        check(ok, "C1.2: surviving blocks undisturbed by frees and new allocations");
        for (i = 0; i < (size_t)NBLK; i++) {
            mimir_free(blk[i]);
        }
        for (i = 0; i < (size_t)NNEW; i++) {
            mimir_free(extra[i]);
        }
        {
            unsigned char *p = (unsigned char *)mimir_malloc(256);

            check(p != NULL,
                  "fresh allocation after interleaved frees succeeds");
            if (p != NULL) {
                mimir_free(p);
            }
        }
    }

    /* --- Overflow refusals and unsatisfiable requests, with
     * health after every failed acquire (A3/A5, F-01, A.3) - */
    {
        unsigned char *sentinel =
            (unsigned char *)mimir_malloc(2048);
        size_t bad[5];
        const char *badname[5];
        size_t nbad = 0;

        check(sentinel != NULL, "sentinel allocation succeeds");
        if (sentinel != NULL) {
            fill_pattern(sentinel, 2048, 77);
        }

        bad[nbad] = SIZE_MAX;
        badname[nbad++] = "SIZE_MAX (oversize, A.3)";
        bad[nbad] = SIZE_MAX - 8;
        badname[nbad++] = "SIZE_MAX - 8 (oversize, A.3)";
        bad[nbad] = SIZE_MAX - page + 1;
        badname[nbad++] = "SIZE_MAX - page + 1 (fails A3 check)";
        bad[nbad] = SIZE_MAX - page;
        badname[nbad++] = "SIZE_MAX - page (passes A3, fails A5 check)";
        bad[nbad] = SIZE_MAX / 2;
        badname[nbad++] = "SIZE_MAX / 2 (representable, unsatisfiable, F-01)";

        for (i = 0; i < nbad; i++) {
            void *r = mimir_malloc(bad[i]);
            unsigned char *t;

            snprintf(label, sizeof label,
                     "D.0: malloc(%s) returns NULL", badname[i]);
            check(r == NULL, label);
            if (sentinel != NULL) {
                snprintf(label, sizeof label,
                         "A.3: sentinel undisturbed after %s",
                         badname[i]);
                check(pattern_ok(sentinel, 2048, 77), label);
            }
            t = (unsigned char *)mimir_malloc(128);
            snprintf(label, sizeof label,
                     "A.3: fresh allocation works after %s",
                     badname[i]);
            check(t != NULL, label);
            if (t != NULL) {
                fill_pattern(t, 128, 3);
                check(pattern_ok(t, 128, 3),
                      "A.3: post-failure allocation pattern verifies");
                mimir_free(t);
            }
        }

        mimir_free(sentinel);
        mimir_free(NULL);
        check(1, "D.0: mimir_free(NULL) returns normally after failures");
        {
            unsigned char *p = (unsigned char *)mimir_malloc(4096);

            check(p != NULL,
                  "allocation after all failures and frees succeeds");
            if (p != NULL) {
                fill_pattern(p, 4096, 99);
                check(pattern_ok(p, 4096, 99),
                      "post-failure allocation pattern verifies");
                mimir_free(p);
            }
        }
    }

    /* --- Zero-size: implementation posture ONLY (U-09) ---- */
    check(mimir_malloc(0) == NULL,
          "malloc(0) returns NULL (posture only, U-09 unresolved)");

    printf("CHECKS: %d run, %d failed\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
