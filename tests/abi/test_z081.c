/* test_z081.c - Z-081 contracts appendix and ABI audit
 * test (Wave 9, D-10). Static link against the production
 * objects (LIBOBJS), so the E-005 audit counters are
 * readable through tests/abi/mimir_audit_e005.h.
 *
 * What is proven (the audit's behavioural leg; the
 * documentation leg is tests/abi/run_z081.sh S9, which
 * mechanically checks that every exported symbol is
 * inventoried in docs/contracts.md Appendix A, declared
 * in include/mimir_alloc.h, and listed in the version
 * script):
 *  - Every one of the ten public entry points is called
 *    through a counting wrapper and behaves per its
 *    public-header contract on this host: the core four
 *    (malloc / calloc zeroing / realloc preserve / free
 *    incl. free(NULL)), the aligned pair (alignment
 *    honoured, invalid alignment refused exactly as the
 *    header states), usable size (exact class extent,
 *    NULL -> 0), the versioned statistics snapshot
 *    (version / size / page-size fields, NULL -> -1),
 *    trim (return domain {0,1}, idempotent second full
 *    trim returns 0), and reallocarray (product
 *    semantics, overflow refusal preserving the
 *    original).
 *  - Audit-counter reconciliation: every wrapper call
 *    moves exactly its own L1 E-005 counter by one --
 *    all ten counters, one per public symbol -- and no
 *    other counter moves. A public symbol with no
 *    counter, or a counter with no symbol, fails here
 *    or in run_z081.sh S3 (the counters are bound local
 *    in the .so and absent from .dynsym).
 *
 * Standalone on purpose: built and run via
 * `make test-z081`; NOT part of `make test`, so the
 * standing gate count is unchanged. The dynamic and
 * C++ consumers live in test_z081_dyn.c /
 * test_z081_cpp.cpp; the ABI recorded run is
 * tests/abi/run_z081.sh, filed as
 * tests/abi/recorded_run_z13_z081.txt; the audit
 * report is docs/Z13_Z081_ABI_AUDIT.md.
 */
#include "mimir_alloc.h"
#include "mimir_audit_e005.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        checks_run++;                                                   \
        if (!(cond)) {                                                  \
            checks_failed++;                                            \
            printf("  [FAIL] %s (line %d)\n", #cond, __LINE__);         \
        }                                                               \
    } while (0)

static unsigned long load(unsigned long *counter)
{
    return __atomic_load_n(counter, __ATOMIC_RELAXED);
}

static unsigned long my_malloc_calls = 0;
static unsigned long my_calloc_calls = 0;
static unsigned long my_realloc_calls = 0;
static unsigned long my_ra_calls = 0;
static unsigned long my_aligned_calls = 0;
static unsigned long my_posix_calls = 0;
static unsigned long my_usable_calls = 0;
static unsigned long my_stats_calls = 0;
static unsigned long my_trim_calls = 0;
static unsigned long my_free_calls = 0;

static void *t_malloc(size_t n) { my_malloc_calls++; return mimir_malloc(n); }
static void *t_calloc(size_t a, size_t b) { my_calloc_calls++; return mimir_calloc(a, b); }
static void *t_realloc(void *p, size_t n) { my_realloc_calls++; return mimir_realloc(p, n); }
static void *t_ra(void *p, size_t a, size_t b) { my_ra_calls++; return mimir_reallocarray(p, a, b); }
static void *t_aligned(size_t a, size_t n) { my_aligned_calls++; return mimir_aligned_alloc(a, n); }
static int t_posix(void **m, size_t a, size_t n) { my_posix_calls++; return mimir_posix_memalign(m, a, n); }
static size_t t_usable(const void *p) { my_usable_calls++; return mimir_usable_size(p); }
static int t_stats(struct mimir_stats_v1 *o) { my_stats_calls++; return mimir_stats_snapshot(o); }
static int t_trim(size_t pad) { my_trim_calls++; return mimir_trim(pad); }
static void t_free(void *p) { my_free_calls++; mimir_free(p); }

static void fill(unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)(seed + i * 31u);
    }
}

static int intact(const unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (p[i] != (unsigned char)(seed + i * 31u)) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    unsigned long c_malloc0 = load(&mimir_audit_call_malloc);
    unsigned long c_calloc0 = load(&mimir_audit_call_calloc);
    unsigned long c_realloc0 = load(&mimir_audit_call_realloc);
    unsigned long c_ra0 = load(&mimir_audit_call_reallocarray);
    unsigned long c_aligned0 = load(&mimir_audit_call_aligned_alloc);
    unsigned long c_posix0 = load(&mimir_audit_call_posix_memalign);
    unsigned long c_usable0 = load(&mimir_audit_call_usable_size);
    unsigned long c_stats0 = load(&mimir_audit_call_stats_snapshot);
    unsigned long c_trim0 = load(&mimir_audit_call_trim);
    unsigned long c_free0 = load(&mimir_audit_call_free);
    unsigned char *p, *q;
    void *m;
    int rc, t1, t2;
    size_t i;
    struct mimir_stats_v1 st;

    printf("=== test_z081: contracts appendix + ABI audit (D-10) ===\n");

    printf("-- S1 core four --\n");
    p = t_malloc(96);
    CHECK(p != NULL);
    fill(p, 96, 7u);
    CHECK(intact(p, 96, 7u));
    q = t_calloc(8, 16);
    CHECK(q != NULL);
    for (i = 0; i < 128; i++) {
        CHECK(q[i] == 0); /* calloc extent zeroed */
    }
    q = t_realloc(q, 200);
    CHECK(q != NULL);
    for (i = 0; i < 128; i++) {
        CHECK(q[i] == 0); /* realloc preserves the prefix */
    }
    t_free(q);
    t_free(NULL); /* defined no-op */
    CHECK(1);     /* reached: free(NULL) returned normally */

    printf("-- S2 aligned pair --\n");
    p = t_aligned(64, 100);
    CHECK(p != NULL);
    CHECK(((uintptr_t)p % 64) == 0);
    fill(p, 100, 3u);
    CHECK(intact(p, 100, 3u));
    t_free(p);
    CHECK(t_aligned(3, 32) == NULL); /* not a power of two */
    CHECK(t_aligned(0, 32) == NULL); /* zero alignment */
    m = (void *)0x1;
    rc = t_posix(&m, 64, 100);
    CHECK(rc == 0);
    CHECK(m != NULL && m != (void *)0x1);
    CHECK(((uintptr_t)m % 64) == 0);
    t_free(m);
    m = (void *)0x1;
    rc = t_posix(&m, 3, 32);
    CHECK(rc == EINVAL);
    CHECK(m == (void *)0x1); /* *memptr unchanged on EINVAL */

    printf("-- S3 usable size --\n");
    p = t_malloc(90);
    CHECK(p != NULL);
    CHECK(t_usable(p) == 96); /* exact class extent (D-012) */
    t_free(p);
    p = t_malloc(8192);
    CHECK(p != NULL);
    CHECK(t_usable(p) >= 8192); /* mapping extent covers it */
    t_free(p);
    CHECK(t_usable(NULL) == 0);

    printf("-- S4 statistics snapshot --\n");
    memset(&st, 0, sizeof(st));
    CHECK(t_stats(&st) == 0);
    CHECK(st.struct_version == MIMIR_STATS_VERSION);
    CHECK(st.struct_size == sizeof(struct mimir_stats_v1));
    CHECK(st.page_size_bytes == 4096); /* this host, 4 KiB pages */
    CHECK(t_stats(NULL) == -1);

    printf("-- S5 trim --\n");
    p = t_malloc(64);
    CHECK(p != NULL);
    t_free(p); /* leaves retained state for trim to judge */
    t1 = t_trim(0);
    CHECK(t1 == 0 || t1 == 1); /* exact return domain */
    t2 = t_trim(0);
    CHECK(t2 == 0); /* second identical full trim: nothing left */

    printf("-- S6 reallocarray --\n");
    p = t_ra(NULL, 4, 32);
    CHECK(p != NULL);
    CHECK(t_usable(p) >= 128);
    fill(p, 128, 9u);
    CHECK(t_ra(p, SIZE_MAX, 2) == NULL); /* checked overflow */
    CHECK(intact(p, 128, 9u));           /* original preserved */
    p = t_ra(p, 8, 32);
    CHECK(p != NULL);
    CHECK(intact(p, 128, 9u)); /* grow preserves min(old,new) */
    t_free(p);

    printf("-- S7 audit-counter reconciliation (ten counters) --\n");
    CHECK(load(&mimir_audit_call_malloc) - c_malloc0 == my_malloc_calls);
    CHECK(load(&mimir_audit_call_calloc) - c_calloc0 == my_calloc_calls);
    CHECK(load(&mimir_audit_call_realloc) - c_realloc0 == my_realloc_calls);
    CHECK(load(&mimir_audit_call_reallocarray) - c_ra0 == my_ra_calls);
    CHECK(load(&mimir_audit_call_aligned_alloc) - c_aligned0 == my_aligned_calls);
    CHECK(load(&mimir_audit_call_posix_memalign) - c_posix0 == my_posix_calls);
    CHECK(load(&mimir_audit_call_usable_size) - c_usable0 == my_usable_calls);
    CHECK(load(&mimir_audit_call_stats_snapshot) - c_stats0 == my_stats_calls);
    CHECK(load(&mimir_audit_call_trim) - c_trim0 == my_trim_calls);
    CHECK(load(&mimir_audit_call_free) - c_free0 == my_free_calls);
    printf("malloc calls: %lu, calloc calls: %lu, realloc calls: %lu, "
           "reallocarray calls: %lu, aligned calls: %lu, posix calls: %lu, "
           "usable calls: %lu, stats calls: %lu, trim calls: %lu, "
           "free calls: %lu\n",
           my_malloc_calls, my_calloc_calls, my_realloc_calls, my_ra_calls,
           my_aligned_calls, my_posix_calls, my_usable_calls, my_stats_calls,
           my_trim_calls, my_free_calls);

    printf("test_z081: %d checks, %d failed\n", checks_run, checks_failed);
    printf("RESULT: %s\n", checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
