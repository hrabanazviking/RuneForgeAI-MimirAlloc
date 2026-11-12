/* test_z078.c - Z-078 zero-size and errno contract test
 * (Wave 9, decision D-013). Static link against the
 * production objects (LIBOBJS), so the E-005 audit
 * counters are readable through
 * tests/abi/mimir_audit_e005.h and the J-002 ledger
 * through tests/core/mimir_stats_j002.h.
 *
 * What is proven (decision D-013, stated as contract in
 * include/mimir_alloc.h and docs/Z13_Z078_ZERO_SIZE_
 * ERRNO.md; the gate is header text + tests matching
 * the decision byte for byte in behaviour):
 *  - U-09: every zero-size form is a defined,
 *    non-failure refusal -- exactly NULL (ENOMEM with
 *    *memptr unchanged for the posix-shaped entry),
 *    counted in alloc_posture_nulls, NEVER in
 *    alloc_failures, with errno left exactly as on
 *    entry (sentinel-pinned).
 *  - U-12: realloc-to-zero (directly and through
 *    reallocarray) returns NULL and leaves the original
 *    block live, byte-intact, same usable size, still
 *    realloc-able and freeable -- proven positively on
 *    both the class path and the mapping path.
 *  - U-10: errno is never a channel. On every checked
 *    refusal that never reaches the OS (arithmetic
 *    overflow, invalid alignment, arithmetic-only
 *    unsatisfiable sizes) the sentinel survives; the
 *    posix-shaped entry's EINVAL / ENOMEM are return
 *    values, with errno likewise untouched.
 *  - Precedence: alignment validity is judged before
 *    size -- an invalid alignment with size 0 is a
 *    genuine FAILURE, not a zero-size refusal, and the
 *    posix-shaped zero-size / invalid legs move NEITHER
 *    ledger counter (the entry returns before any note).
 *  - Audit counters: the six affected L1 counters move
 *    exactly once per call of their entry point (plus
 *    the free counter), reconciled at the end; every
 *    public call in this file goes through a counting
 *    wrapper except the worker thread's, which are
 *    accounted for explicitly (the Z-077 S5 pattern).
 *    mimir_usable_size and the core stats snapshot are
 *    called directly and are NOT reconciled.
 *
 * Standalone on purpose: built and run via
 * `make test-z078`; NOT part of `make test`, so the
 * standing gate count is unchanged. The dynamic and
 * C++ consumers live in test_z078_dyn.c /
 * test_z078_cpp.cpp; the ABI recorded run is
 * tests/abi/run_z078.sh, filed as
 * tests/abi/recorded_run_z13_z078.txt.
 */
#include "mimir_alloc.h"
#include "mimir_audit_e005.h"
#include "mimir_stats_j002.h"

#include <errno.h>
#include <pthread.h>
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

#define SENTINEL 77

/* Call with errno pre-set to the sentinel, then pin it. */
#define CHECK_ERRNO_UNTOUCHED() CHECK(errno == SENTINEL)

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
static unsigned long my_free_calls = 0;

static void *t_malloc(size_t n) { my_malloc_calls++; return mimir_malloc(n); }
static void *t_calloc(size_t a, size_t b) { my_calloc_calls++; return mimir_calloc(a, b); }
static void *t_realloc(void *p, size_t n) { my_realloc_calls++; return mimir_realloc(p, n); }
static void *t_ra(void *p, size_t a, size_t b) { my_ra_calls++; return mimir_reallocarray(p, a, b); }
static void *t_aligned(size_t a, size_t n) { my_aligned_calls++; return mimir_aligned_alloc(a, n); }
static int t_posix(void **m, size_t a, size_t n) { my_posix_calls++; return mimir_posix_memalign(m, a, n); }
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

struct stats_pair {
    uint64_t failures;
    uint64_t postures;
};

static struct stats_pair snap(void)
{
    struct mimir_stats st;
    struct stats_pair out;
    if (mimir_core_stats_snapshot(&st) != 0) {
        out.failures = (uint64_t)-1;
        out.postures = (uint64_t)-1;
        return out;
    }
    out.failures = st.alloc_failures;
    out.postures = st.alloc_posture_nulls;
    return out;
}

static void *worker_zero(void *arg)
{
    unsigned char *p;
    void *m = (void *)0x1;
    (void)arg;
    if (mimir_malloc(0) != NULL) {
        return (void *)1;
    }
    p = mimir_malloc(96);
    if (p == NULL) {
        return (void *)2;
    }
    fill(p, 96, 55u);
    if (mimir_realloc(p, 0) != NULL) {
        return (void *)3;
    }
    if (!intact(p, 96, 55u)) {
        return (void *)4;
    }
    if (mimir_posix_memalign(&m, 8, 0) != ENOMEM) {
        return (void *)5;
    }
    if (m != (void *)0x1) {
        return (void *)6;
    }
    mimir_free(p);
    return NULL;
}

int main(void)
{
    unsigned long malloc0 = load(&mimir_audit_call_malloc);
    unsigned long calloc0 = load(&mimir_audit_call_calloc);
    unsigned long realloc0 = load(&mimir_audit_call_realloc);
    unsigned long ra0 = load(&mimir_audit_call_reallocarray);
    unsigned long aligned0 = load(&mimir_audit_call_aligned_alloc);
    unsigned long posix0 = load(&mimir_audit_call_posix_memalign);
    unsigned long free0 = load(&mimir_audit_call_free);
    struct stats_pair s0, s1;
    unsigned char *p, *q;
    void *m;
    size_t usable_before;
    int rc;

    printf("=== test_z078: zero-size + errno contract (D-013) ===\n");

    /* ---- S1 zero-size malloc / calloc (U-09) ---- */
    printf("-- S1 zero-size malloc/calloc --\n");
    s0 = snap();
    errno = SENTINEL; CHECK(t_malloc(0) == NULL); CHECK_ERRNO_UNTOUCHED();
    errno = SENTINEL; CHECK(t_calloc(0, 16) == NULL); CHECK_ERRNO_UNTOUCHED();
    errno = SENTINEL; CHECK(t_calloc(16, 0) == NULL); CHECK_ERRNO_UNTOUCHED();
    errno = SENTINEL; CHECK(t_calloc(0, 0) == NULL); CHECK_ERRNO_UNTOUCHED();
    s1 = snap();
    CHECK(s1.postures - s0.postures == 4);
    CHECK(s1.failures - s0.failures == 0);
    /* The allocator is unharmed and the refusal allocated
     * nothing: a normal request still succeeds. */
    p = t_malloc(64);
    CHECK(p != NULL);
    t_free(p);

    /* ---- S2 realloc-to-zero (U-09 inheritance + U-12) ---- */
    printf("-- S2 realloc-to-zero --\n");
    s0 = snap();
    errno = SENTINEL; CHECK(t_realloc(NULL, 0) == NULL); CHECK_ERRNO_UNTOUCHED();
    /* Class path: 200-byte request. */
    p = t_malloc(200);
    CHECK(p != NULL);
    fill(p, 200, 11u);
    usable_before = mimir_usable_size(p);
    CHECK(usable_before >= 200);
    errno = SENTINEL; CHECK(t_realloc(p, 0) == NULL); CHECK_ERRNO_UNTOUCHED();
    CHECK(intact(p, 200, 11u));                    /* contents live */
    CHECK(mimir_usable_size(p) == usable_before);  /* extent live */
    q = t_realloc(p, 200);                         /* still owned: same-size course */
    CHECK(q == p);
    CHECK(intact(q, 200, 11u));
    t_free(q);
    /* Mapping path: 5000-byte request. */
    p = t_malloc(5000);
    CHECK(p != NULL);
    fill(p, 5000, 23u);
    usable_before = mimir_usable_size(p);
    CHECK(usable_before >= 5000);
    errno = SENTINEL; CHECK(t_realloc(p, 0) == NULL); CHECK_ERRNO_UNTOUCHED();
    CHECK(intact(p, 5000, 23u));
    CHECK(mimir_usable_size(p) == usable_before);
    t_free(p);                                     /* freeable exactly as before */
    s1 = snap();
    CHECK(s1.postures - s0.postures == 3);
    CHECK(s1.failures - s0.failures == 0);

    /* ---- S3 reallocarray zero factors (U-09 / U-12) ---- */
    printf("-- S3 reallocarray zero factors --\n");
    s0 = snap();
    errno = SENTINEL; CHECK(t_ra(NULL, 0, 8) == NULL); CHECK_ERRNO_UNTOUCHED();
    errno = SENTINEL; CHECK(t_ra(NULL, 8, 0) == NULL); CHECK_ERRNO_UNTOUCHED();
    p = t_malloc(128);
    CHECK(p != NULL);
    fill(p, 128, 37u);
    errno = SENTINEL; CHECK(t_ra(p, 0, 8) == NULL); CHECK_ERRNO_UNTOUCHED();
    CHECK(intact(p, 128, 37u));
    errno = SENTINEL; CHECK(t_ra(p, 8, 0) == NULL); CHECK_ERRNO_UNTOUCHED();
    CHECK(intact(p, 128, 37u));
    t_free(p);
    s1 = snap();
    CHECK(s1.postures - s0.postures == 4);
    CHECK(s1.failures - s0.failures == 0);

    /* ---- S4 aligned zero-size + precedence ---- */
    printf("-- S4 aligned zero-size / precedence --\n");
    s0 = snap();
    errno = SENTINEL; CHECK(t_aligned(64, 0) == NULL); CHECK_ERRNO_UNTOUCHED();
    errno = SENTINEL; CHECK(t_aligned(4096, 0) == NULL); CHECK_ERRNO_UNTOUCHED();
    errno = SENTINEL; CHECK(t_aligned(1, 0) == NULL); CHECK_ERRNO_UNTOUCHED();
    s1 = snap();
    CHECK(s1.postures - s0.postures == 3);
    CHECK(s1.failures - s0.failures == 0);
    /* Invalid alignment is judged BEFORE size: with a
     * zero size it is still a genuine failure, never a
     * zero-size refusal. */
    s0 = snap();
    errno = SENTINEL; CHECK(t_aligned(0, 0) == NULL); CHECK_ERRNO_UNTOUCHED();
    errno = SENTINEL; CHECK(t_aligned(3, 100) == NULL); CHECK_ERRNO_UNTOUCHED();
    errno = SENTINEL; CHECK(t_aligned(24, 0) == NULL); CHECK_ERRNO_UNTOUCHED();
    s1 = snap();
    CHECK(s1.failures - s0.failures == 3);
    CHECK(s1.postures - s0.postures == 0);

    /* ---- S5 posix-shaped entry (U-09 / U-10) ---- */
    printf("-- S5 posix_memalign rows --\n");
    m = (void *)0x1; /* unchanged-marker */
    s0 = snap();
    errno = SENTINEL; CHECK(t_posix(NULL, 8, 100) == EINVAL); CHECK_ERRNO_UNTOUCHED();
    errno = SENTINEL; CHECK(t_posix(&m, 3, 100) == EINVAL); CHECK_ERRNO_UNTOUCHED();
    CHECK(m == (void *)0x1);
    errno = SENTINEL; CHECK(t_posix(&m, 8, 0) == ENOMEM); CHECK_ERRNO_UNTOUCHED();
    CHECK(m == (void *)0x1);
    errno = SENTINEL; CHECK(t_posix(&m, 16, 0) == ENOMEM); CHECK_ERRNO_UNTOUCHED();
    CHECK(m == (void *)0x1);
    s1 = snap();
    /* The posix entry returns before any ledger note on
     * every refusal above: neither counter moves. */
    CHECK(s1.postures - s0.postures == 0);
    CHECK(s1.failures - s0.failures == 0);
    /* A valid but arithmetic-unsatisfiable request is
     * ENOMEM with *memptr unchanged; the refusal is
     * reached through the family failure ledger (the
     * aligned construction notes it), and errno is
     * still not the channel. */
    s0 = snap();
    errno = SENTINEL; rc = t_posix(&m, 8, SIZE_MAX); CHECK(rc == ENOMEM); CHECK_ERRNO_UNTOUCHED();
    CHECK(m == (void *)0x1);
    s1 = snap();
    CHECK(s1.failures - s0.failures == 1);
    CHECK(s1.postures - s0.postures == 0);
    /* Success still works exactly as before. */
    errno = SENTINEL; rc = t_posix(&m, 64, 100); CHECK(rc == 0);
    CHECK(m != NULL && m != (void *)0x1);
    CHECK(((uintptr_t)m % 64) == 0);
    t_free(m);

    /* ---- S6 checked arithmetic failures (U-10) ---- */
    printf("-- S6 arithmetic failures, errno untouched --\n");
    s0 = snap();
    errno = SENTINEL; CHECK(t_malloc(SIZE_MAX) == NULL); CHECK_ERRNO_UNTOUCHED();
    errno = SENTINEL; CHECK(t_calloc(SIZE_MAX, 2) == NULL); CHECK_ERRNO_UNTOUCHED();
    errno = SENTINEL; CHECK(t_calloc(2, SIZE_MAX) == NULL); CHECK_ERRNO_UNTOUCHED();
    errno = SENTINEL; CHECK(t_ra(NULL, SIZE_MAX, 2) == NULL); CHECK_ERRNO_UNTOUCHED();
    p = t_malloc(64);
    CHECK(p != NULL);
    fill(p, 64, 41u);
    errno = SENTINEL; CHECK(t_realloc(p, SIZE_MAX) == NULL); CHECK_ERRNO_UNTOUCHED();
    CHECK(intact(p, 64, 41u)); /* failure preservation */
    t_free(p);
    s1 = snap();
    CHECK(s1.failures - s0.failures == 5);
    CHECK(s1.postures - s0.postures == 0);

    /* ---- S7 worker thread ---- */
    printf("-- S7 worker thread --\n");
    {
        pthread_t th;
        void *res = NULL;
        CHECK(pthread_create(&th, NULL, worker_zero, NULL) == 0);
        CHECK(pthread_join(th, &res) == 0);
        CHECK(res == NULL);
    }
    /* NOTE: the worker called the entry points directly,
     * outside the counting wrappers: account for its
     * calls (malloc x2, realloc x1, posix x1, free x1)
     * so the S8 reconciliation stays exact. */
    my_malloc_calls += 2;
    my_realloc_calls += 1;
    my_posix_calls += 1;
    my_free_calls += 1;

    /* ---- S8 audit counter reconciliation ---- */
    printf("-- S8 audit counters --\n");
    CHECK(load(&mimir_audit_call_malloc) - malloc0 == my_malloc_calls);
    CHECK(load(&mimir_audit_call_calloc) - calloc0 == my_calloc_calls);
    CHECK(load(&mimir_audit_call_realloc) - realloc0 == my_realloc_calls);
    CHECK(load(&mimir_audit_call_reallocarray) - ra0 == my_ra_calls);
    CHECK(load(&mimir_audit_call_aligned_alloc) - aligned0 == my_aligned_calls);
    CHECK(load(&mimir_audit_call_posix_memalign) - posix0 == my_posix_calls);
    CHECK(load(&mimir_audit_call_free) - free0 == my_free_calls);
    printf("malloc calls: %lu, calloc calls: %lu, realloc calls: %lu, reallocarray calls: %lu, aligned calls: %lu, posix calls: %lu, free calls: %lu\n",
           my_malloc_calls, my_calloc_calls, my_realloc_calls,
           my_ra_calls, my_aligned_calls, my_posix_calls,
           my_free_calls);

    printf("test_z078: %d checks, %d failed\n", checks_run,
           checks_failed);
    printf("RESULT: %s\n", checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
