/* test_z076_dyn.c - Z-076 dynamic consumer (E-series style).
 *
 * Linked ONLY against build/libmimiralloc.so, never the
 * static objects: it proves the public statistics entry
 * point resolves and behaves through the shared library's
 * C ABI, including a cross-thread small free counted
 * through the .so alone. The export list, DT_NEEDED and
 * undefined-symbol proofs are in the runner
 * (tests/abi/run_z076.sh) and its recorded run.
 *
 * Standalone: built and run via `make test-z076`; NOT
 * part of `make test`.
 */
#include "mimir_alloc.h"

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

static void *g_ptr;
static void *free_in_worker(void *arg)
{
    (void)arg;
    mimir_free(g_ptr);
    return NULL;
}

int main(void)
{
    struct mimir_stats_v1 a;
    struct mimir_stats_v1 b;
    pthread_t th;
    void *p;

    printf("Z-076 dynamic consumer (libmimiralloc.so only)\n");
    CHECK(mimir_stats_snapshot(NULL) == -1);
    memset(&a, 0, sizeof a);
    CHECK(mimir_stats_snapshot(&a) == 0);
    CHECK(a.struct_version == 1);
    CHECK(a.struct_size == sizeof(struct mimir_stats_v1));
    CHECK(a.lock_contention_measured == 1);
    CHECK(a.live_requested_measured == 0);
    CHECK(a.live_requested_bytes == 0);
    CHECK(a.page_size_bytes == 4096);
    CHECK(a.span_class_size[0] == 16);
    CHECK(a.span_class_size[19] == 1024);

    p = mimir_malloc(100);
    CHECK(p != NULL);
    memset(&b, 0, sizeof b);
    CHECK(mimir_stats_snapshot(&b) == 0);
    CHECK(b.bytes_requested == a.bytes_requested + 100);
    CHECK(b.bytes_allocated == a.bytes_allocated + 112);
    mimir_free(p);

    /* Cross-thread small free through the .so: +1. */
    memset(&a, 0, sizeof a);
    CHECK(mimir_stats_snapshot(&a) == 0);
    g_ptr = mimir_malloc(48);
    CHECK(g_ptr != NULL);
    CHECK(pthread_create(&th, NULL, free_in_worker, NULL) == 0);
    CHECK(pthread_join(th, NULL) == 0);
    memset(&b, 0, sizeof b);
    CHECK(mimir_stats_snapshot(&b) == 0);
    CHECK(b.cross_thread_small_frees ==
          a.cross_thread_small_frees + 1);
    CHECK(b.cross_thread_frees == a.cross_thread_frees + 1);
    CHECK(b.cross_thread_small_frees <= b.cross_thread_frees);

    printf("Z076 DYN: %d checks, %d failed\n", checks_run,
           checks_failed);
    printf("RESULT: %s\n", checks_failed == 0 ? "PASS" : "FAIL");
    return checks_failed == 0 ? 0 : 1;
}
