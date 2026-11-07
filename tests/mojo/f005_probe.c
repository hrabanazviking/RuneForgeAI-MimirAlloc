/* f005_probe.c - MimirAlloc slice F-005 test-only helpers,
 * dynamic leg (E-005 dynamic-leg machinery on the Mojo path).
 *
 * This file lives under tests/mojo/ on purpose. It is NOT
 * installed, NOT part of the public surface, and NOT a
 * contract document: the public header include/mimir_alloc.h
 * declares exactly the four public entry points and is
 * unchanged by F-005.
 *
 * The three functions here observe effects that only
 * MimirAlloc's implementation produces, from OUTSIDE the
 * shared library, in the process that hosts the generated
 * Mojo executable (they are compiled into a test-only
 * shared object, build/libf005probe.so, loaded into that
 * process; they hold no allocator state of their own):
 *
 * - f005_mapped_bytes: the process's total mapped extent,
 *   summed from /proc/self/maps (the E-005 dynamic leg's
 *   accounting; byte sums are exact - mapping LINE counts
 *   are not used, per INC-E005-02).
 * - f005_descriptor_valid: the E-005 dynamic leg's
 *   descriptor validation, carried to the Phase 7
 *   two-form serving headers by the G-004 re-teach
 *   (INC-G004-05, in the spirit of INC-G003-01): for a
 *   request above the 1024-byte cutoff the original
 *   mapping-form validation runs unchanged (96-byte
 *   descriptor, B-006 S7.3, in the reserve page
 *   immediately before the user pointer); for a request
 *   of 1..1024 bytes the span-form validation runs at
 *   equivalent rigour (96-byte span header at the base
 *   of the span's one system page: magic "MIMIRSPN",
 *   mapping_base == its own address, mapping_length ==
 *   one page, usable_offset == 96, class-size field ==
 *   the class serving the request, usable_size ==
 *   page - 96, state == LIVE, class index == the
 *   class's index, live_count >= 1, pointer at the class
 *   stride in the object area). Before the re-teach this
 *   function applied the mapping form to every pointer
 *   and segfaulted on the first span-served pointer
 *   (the read at user_ptr - page lands in an unmapped
 *   page for a span object at span_base + 96). This
 *   function is called ONLY
 *   for pointers returned by mimir_* calls in the same
 *   process, whose reserve page is allocator-owned by
 *   construction; it is never called on a foreign pointer
 *   (the core's own S11.2 order). Test-side observation of
 *   an internal layout fact, NOT a public facility: the
 *   layout is expressly "still not a stable ABI" (B-006
 *   S7.3).
 * - f005_page_size: the system page size the two
 *   observations above are computed against.
 *
 * The E-005 audit counters are deliberately NOT referenced
 * here: they are local in libmimiralloc.so, so a shared
 * object that referenced them could not load beside it.
 * The counter leg of F-005 is the separate static build
 * (tests/mojo/f005_audit.c + tests/mojo/test_f005_audit
 * .mojo), exactly as E-005's two legs are separate.
 */
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

struct f005_desc_probe {
    uint64_t magic;
    void *mapping_base;
    size_t mapping_length;
    size_t usable_offset;
    size_t requested_size;
    size_t usable_size;
    uint32_t state;
    uint32_t flags;
};

#define F005_DESC_MAGIC UINT64_C(0x4D494D4952414C4C) /* "MIMIRALL" */

long f005_page_size(void)
{
    return sysconf(_SC_PAGESIZE);
}

static size_t f005_mapping_len_for(long page_sz, size_t size)
{
    size_t total = (size_t)page_sz + size;
    size_t pg = (size_t)page_sz;
    return (total + pg - 1) / pg * pg;
}

/* Layout mirror of the internal span header (G-003,
 * the full 96 bytes in the S7.3 field discipline). */
struct f005_span_probe {
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

#define F005_SPAN_MAGIC UINT64_C(0x4D494D495253504E) /* "MIMIRSPN" */
#define F005_SPAN_HEADER_SIZE ((size_t)96)
#define F005_SMALL_CUTOFF ((size_t)1024)

static const size_t f005_class_sizes[20] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};

static int f005_span_valid(const void *user_ptr, size_t size,
                           long page_sz)
{
    const struct f005_span_probe *sp =
        (const struct f005_span_probe *)((uintptr_t)user_ptr &
                                         ~(uintptr_t)(page_sz - 1));
    size_t idx = 20, cs = 0, i, off;
    for (i = 0; i < 20; i++) {
        if (size <= f005_class_sizes[i]) {
            idx = i;
            cs = f005_class_sizes[i];
            break;
        }
    }
    if (cs == 0) {
        return 0;
    }
    off = (size_t)((const char *)user_ptr - (const char *)sp);
    return sp->magic == F005_SPAN_MAGIC &&
           sp->mapping_base == (const void *)sp &&
           sp->mapping_length == (size_t)page_sz &&
           sp->usable_offset == F005_SPAN_HEADER_SIZE &&
           sp->class_size == cs &&
           sp->usable_size ==
               (size_t)page_sz - F005_SPAN_HEADER_SIZE &&
           sp->state == 1 &&
           sp->class_index == (uint32_t)idx &&
           sp->live_count >= 1 &&
           off >= F005_SPAN_HEADER_SIZE &&
           (off - F005_SPAN_HEADER_SIZE) % cs == 0;
}

int f005_descriptor_valid(const void *user_ptr, size_t size)
{
    long page_sz = sysconf(_SC_PAGESIZE);
    if (page_sz <= 0 || user_ptr == NULL) {
        return 0;
    }
    if (size <= F005_SMALL_CUTOFF) {
        return f005_span_valid(user_ptr, size, page_sz);
    }
    const struct f005_desc_probe *d =
        (const struct f005_desc_probe *)((const char *)user_ptr -
                                          page_sz);
    return d->magic == F005_DESC_MAGIC &&
           d->mapping_base == (const void *)d &&
           d->usable_offset == (size_t)page_sz &&
           d->requested_size == size && d->state == 1 &&
           d->mapping_length == f005_mapping_len_for(page_sz, size);
}

unsigned long long f005_mapped_bytes(void)
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
