/* test_z075.c - Z-075 usable size exactness test (Wave 9).
 * Static link against the production objects (LIBOBJS), so
 * the E-005 audit counter appended for the new entry point
 * is readable through tests/abi/mimir_audit_e005.h.
 *
 * What is proven (decision D-012 in docs/DECISION_LEDGER.md;
 * behaviour by analogy under [UNRESOLVED: U-03] to
 * contracts.md MA-COND-USABLE-01/-02, with the V-05
 * per-target semantics D-012 defines):
 *  - Per-class exactness: every request 1..1024 answers
 *    exactly its size class's extent (the test's own copy
 *    of the class table is the expectation), and the FULL
 *    class extent is writable and readable back -- the
 *    answer is the usable extent, not the request and
 *    not a rounding guess.
 *  - Mapping exactness: every request above the cutoff
 *    answers exactly page_round_up(request) on this host
 *    (mapping_length - one reserve page), the full extent
 *    again writable.
 *  - calloc and the Z-074 aligned entries compose: their
 *    blocks answer by the same two rules (aligned at or
 *    below the family floor routes small; above it the
 *    mapping rule; for above-page alignment the extent is
 *    per-block record state -- the offset depends on the
 *    mapping base's alignment residue -- so the test pins
 *    its invariants and its full-extent usability instead
 *    of a closed form).
 *  - NULL answers 0. Interior, foreign (stack, static,
 *    libc-malloc) and already-freed pointers answer 0 --
 *    freed includes a block resident in this thread's own
 *    TLS bin and a block retired by a realloc move.
 *  - realloc postures: a shrink NEVER reduces the answer
 *    (small: the class is retained; large: the mapping
 *    extent is retained, only the requested-size record
 *    drops); a move answers the new block's own extent.
 *  - Cross-thread: a live block answers exactly from any
 *    thread; a block freed on a worker thread answers 0
 *    back on the owner thread.
 *  - The call modifies nothing: contents survive repeated
 *    queries byte-for-byte, and the L1 audit counter moves
 *    exactly once per call, NULL and refusal calls
 *    included.
 *
 * Standalone on purpose: built and run via
 * `make test-z075`; NOT part of `make test`, so the
 * standing gate count is unchanged. The dynamic and C++
 * consumers live in test_z075_dyn.c / test_z075_cpp.cpp;
 * the ABI recorded run is tests/abi/run_z075.sh, filed as
 * tests/abi/recorded_run_z13_z075.txt.
 */
#include "mimir_alloc.h"
#include "mimir_audit_e005.h"

#include <pthread.h>
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
        if (!(cond)) {                                                  \
            checks_failed++;                                            \
            printf("  [FAIL] %s (line %d)\n", #cond, __LINE__);         \
        }                                                               \
    } while (0)

static unsigned long load(unsigned long *counter)
{
    return __atomic_load_n(counter, __ATOMIC_RELAXED);
}

static void fill(unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)(seed + (i * 31u));
    }
}

static int verify(const unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (p[i] != (unsigned char)(seed + (i * 31u))) {
            return 0;
        }
    }
    return 1;
}

/* The expectation table: this test's own copy of the
 * banked G-002 class table. The implementation reads its
 * answer from the span record; agreement across every
 * request is the exactness proof, so the table is stated
 * here independently, not shared with the core. */
static const size_t class_sizes[20] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};

static size_t class_for(size_t req)
{
    size_t i;
    for (i = 0; i < 20; i++) {
        if (req <= class_sizes[i]) {
            return class_sizes[i];
        }
    }
    return 0;
}

static size_t page_round_up(size_t n, size_t ps)
{
    return (n + ps - 1) & ~(ps - 1);
}

/* ---- cross-thread fixtures ---- */
static void *xt_small;
static void *xt_large;
static size_t xt_small_usable;
static size_t xt_large_usable;

static void *worker_query_main(void *unused)
{
    (void)unused;
    xt_small_usable = mimir_usable_size(xt_small);
    xt_large_usable = mimir_usable_size(xt_large);
    return NULL;
}

static void *worker_free_large_main(void *unused)
{
    (void)unused;
    mimir_free(xt_large);
    return NULL;
}

static void *worker_alloc_main(void *unused)
{
    (void)unused;
    xt_small = mimir_malloc(300);
    xt_large = mimir_malloc(5000);
    return NULL;
}

static int static_foreign_obj;

int main(void)
{
    size_t ps = (size_t)sysconf(_SC_PAGESIZE);
    size_t req;
    size_t i;

    printf("Z-075 usable size exactness test (static link)\n");
    CHECK(ps == 4096); /* this host, Z-002 host truth */

    /* NULL: the defined 0 (MA-COND-USABLE-02 by analogy). */
    CHECK(mimir_usable_size(NULL) == 0);

    /* Per-class exactness over EVERY small request: the
     * answer is the class extent, and the whole extent is
     * the caller's to write and read back. */
    for (req = 1; req <= 1024; req++) {
        unsigned char *p = mimir_malloc(req);
        size_t want = class_for(req);
        CHECK(p != NULL);
        if (p == NULL) {
            continue;
        }
        CHECK(mimir_usable_size(p) == want);
        fill(p, want, (unsigned int)req);
        CHECK(verify(p, want, (unsigned int)req));
        /* A query modifies nothing and is idempotent. */
        CHECK(mimir_usable_size(p) == want);
        CHECK(verify(p, want, (unsigned int)req));
        mimir_free(p);
    }

    /* Mapping exactness above the cutoff: the answer is
     * the descriptor's usable extent, page_round_up(req)
     * on this host's layout (one reserve page). */
    {
        static const size_t big[] = { 1025, 1026, 2000, 4095,
            4096, 4097, 5000, 8192, 65536, 100000, 1048576,
            1048577 };
        for (i = 0; i < sizeof big / sizeof big[0]; i++) {
            unsigned char *p = mimir_malloc(big[i]);
            size_t want = page_round_up(big[i], ps);
            CHECK(p != NULL);
            if (p == NULL) {
                continue;
            }
            CHECK(mimir_usable_size(p) == want);
            fill(p, want, (unsigned int)big[i]);
            CHECK(verify(p, want, (unsigned int)big[i]));
            mimir_free(p);
        }
    }

    /* calloc composes by the same two rules. */
    {
        unsigned char *p = mimir_calloc(3, 10); /* 30 -> 32 */
        unsigned char *q = mimir_calloc(1, 2000);
        CHECK(p != NULL && mimir_usable_size(p) == 32);
        CHECK(q != NULL &&
              mimir_usable_size(q) == page_round_up(2000, ps));
        if (p != NULL) {
            CHECK(p[0] == 0 && p[29] == 0);
        }
        mimir_free(p);
        mimir_free(q);
    }

    /* The Z-074 aligned entries compose by the same two
     * rules: at/below the floor the class machinery
     * answers; above it the mapping rule, including the
     * >page construction's exact offset arithmetic. */
    {
        unsigned char *p;
        void *out = NULL;

        p = mimir_aligned_alloc(16, 100);
        CHECK(p != NULL && mimir_usable_size(p) == 112);
        mimir_free(p);
        p = mimir_aligned_alloc(8, 24);
        CHECK(p != NULL && mimir_usable_size(p) == 32);
        mimir_free(p);
        p = mimir_aligned_alloc(64, 100);
        CHECK(p != NULL && mimir_usable_size(p) == ps);
        mimir_free(p);
        p = mimir_aligned_alloc(4096, 100);
        CHECK(p != NULL && mimir_usable_size(p) == ps);
        mimir_free(p);
        p = mimir_aligned_alloc(8192, 100);
        {
            /* Above-page alignment: the extent is the descriptor's
             * mapping_length - usable_offset, and the offset
             * depends on the mapping base's own alignment residue
             * (the base is page-aligned, not alignment-aligned),
             * so no closed form in (alignment, size) exists:
             * probe evidence on this host showed 8192/100 ->
             * 12288 and 16384/100 -> 8192. What is exact here:
             * the answer is a whole number of pages, covers the
             * request, never exceeds the mapping length, and the
             * full extent is the caller's to use. */
            size_t u = (p != NULL) ? mimir_usable_size(p) : 0;
            CHECK(p != NULL && (uintptr_t)p % 8192 == 0);
            CHECK(u >= 100 && u % ps == 0 &&
                  u <= page_round_up(8192 + ps + 100, ps));
            if (p != NULL) {
                fill(p, u, 99u);
                CHECK(verify(p, u, 99u));
            }
        }
        mimir_free(p);
        CHECK(mimir_posix_memalign(&out, 256, 50) == 0);
        CHECK(out != NULL && mimir_usable_size(out) == ps);
        mimir_free(out);
    }

    /* Interior and foreign pointers answer 0. */
    {
        unsigned char *p = mimir_malloc(256);
        unsigned char *q = mimir_malloc(5000);
        int stack_obj = 0;
        void *libc_p = malloc(64);
        CHECK(p != NULL && q != NULL && libc_p != NULL);
        CHECK(mimir_usable_size(p + 1) == 0);
        CHECK(mimir_usable_size(p + 16) == 0);
        CHECK(mimir_usable_size(q + 1) == 0);
        CHECK(mimir_usable_size(q + 4096) == 0);
        CHECK(mimir_usable_size(&stack_obj) == 0);
        CHECK(mimir_usable_size(&static_foreign_obj) == 0);
        CHECK(mimir_usable_size(libc_p) == 0);
        free(libc_p);
        mimir_free(p);
        mimir_free(q);
    }

    /* Already-freed pointers answer 0: central free list,
     * this thread's own TLS bin, a retired-or-pooled
     * mapping, and the old pointer of a realloc move. */
    {
        unsigned char *p = mimir_malloc(100);
        unsigned char *batch[20];
        unsigned char *big = mimir_malloc(5000);
        unsigned char *old;
        unsigned char *moved;
        int k;

        CHECK(p != NULL && big != NULL);
        mimir_free(p);
        CHECK(mimir_usable_size(p) == 0);
        CHECK(mimir_usable_size(p) == 0); /* refusal is stable */
        for (k = 0; k < 20; k++) {
            batch[k] = mimir_malloc(64);
            CHECK(batch[k] != NULL);
        }
        for (k = 0; k < 20; k++) {
            mimir_free(batch[k]);
        }
        for (k = 0; k < 20; k++) {
            CHECK(mimir_usable_size(batch[k]) == 0);
        }
        mimir_free(big);
        CHECK(mimir_usable_size(big) == 0);

        old = mimir_malloc(90);
        CHECK(old != NULL && mimir_usable_size(old) == 96);
        moved = mimir_realloc(old, 200);
        CHECK(moved != NULL && moved != old);
        CHECK(mimir_usable_size(moved) == 224);
        CHECK(mimir_usable_size(old) == 0);
        mimir_free(moved);
    }

    /* realloc postures: shrink never reduces the answer;
     * a move answers the new block's own extent. */
    {
        unsigned char *p = mimir_malloc(100); /* class 112 */
        unsigned char *q;
        CHECK(p != NULL && mimir_usable_size(p) == 112);
        q = mimir_realloc(p, 50);
        CHECK(q == p && mimir_usable_size(q) == 112);
        q = mimir_realloc(p, 112);
        CHECK(q == p && mimir_usable_size(q) == 112);
        q = mimir_realloc(p, 5000);
        CHECK(q != NULL &&
              mimir_usable_size(q) == page_round_up(5000, ps));
        mimir_free(q);

        p = mimir_malloc(5000); /* usable 8192 on this host */
        CHECK(p != NULL &&
              mimir_usable_size(p) == page_round_up(5000, ps));
        fill(p, 5000, 7u);
        q = mimir_realloc(p, 2000); /* shrink: extent kept */
        CHECK(q == p &&
              mimir_usable_size(q) == page_round_up(5000, ps));
        CHECK(verify(p, 2000, 7u));
        /* After a shrink the recorded request is 2000, so asking
         * for 9000 is a grow and moves; the new block answers its
         * own extent and carries the surviving 2000 bytes. */
        q = mimir_realloc(q, 9000);
        CHECK(q != NULL &&
              mimir_usable_size(q) == page_round_up(9000, ps));
        if (q != NULL) {
            CHECK(verify(q, 2000, 7u));
            mimir_free(q);
        } else {
            mimir_free(p);
        }
        /* A true same-size realloc (request == recorded request)
         * stays in place with the extent unchanged. */
        p = mimir_malloc(5000);
        CHECK(p != NULL &&
              mimir_usable_size(p) == page_round_up(5000, ps));
        q = mimir_realloc(p, 5000);
        CHECK(q == p &&
              mimir_usable_size(q) == page_round_up(5000, ps));
        if (q != NULL) {
            mimir_free(q);
        } else {
            mimir_free(p);
        }
    }

    /* Cross-thread: exact answers from any thread for a
     * live block; 0 back home after a worker-thread free. */
    {
        pthread_t th;
        xt_small = mimir_malloc(300); /* class 320 */
        xt_large = mimir_malloc(5000);
        CHECK(xt_small != NULL && xt_large != NULL);
        CHECK(mimir_usable_size(xt_small) == 320);
        CHECK(pthread_create(&th, NULL, worker_query_main,
                             NULL) == 0);
        CHECK(pthread_join(th, NULL) == 0);
        CHECK(xt_small_usable == 320);
        CHECK(xt_large_usable == page_round_up(5000, ps));
        CHECK(pthread_create(&th, NULL, worker_free_large_main,
                             NULL) == 0);
        CHECK(pthread_join(th, NULL) == 0);
        CHECK(mimir_usable_size(xt_large) == 0);
        mimir_free(xt_small);

        /* Worker-allocated blocks stay exactly answerable
         * (and freeable) on the main thread. */
        xt_small = NULL;
        xt_large = NULL;
        CHECK(pthread_create(&th, NULL, worker_alloc_main,
                             NULL) == 0);
        CHECK(pthread_join(th, NULL) == 0);
        CHECK(xt_small != NULL && xt_large != NULL);
        CHECK(mimir_usable_size(xt_small) == 320);
        CHECK(mimir_usable_size(xt_large) ==
              page_round_up(5000, ps));
        mimir_free(xt_small);
        mimir_free(xt_large);
    }

    /* Audit counter: exactly one increment per call,
     * whatever the outcome. */
    {
        unsigned long u0 = load(&mimir_audit_call_usable_size);
        unsigned char *p = mimir_malloc(48);
        (void)mimir_usable_size(p);
        (void)mimir_usable_size(NULL);
        (void)mimir_usable_size(&static_foreign_obj);
        mimir_free(p);
        (void)mimir_usable_size(p);
        CHECK(load(&mimir_audit_call_usable_size) - u0 == 4);
    }

    printf("Z075 STATIC: %d checks, %d failed\n", checks_run,
           checks_failed);
    printf("RESULT: %s\n", checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
