/*
 * test_j004.c -- evidence test for slice J-004 (Phase 10
 * debug-only red zones + canaries; slice record:
 * docs/PHASE_10_DEBUG_REDZONES.md; plan:
 * docs/PHASE_10_FOUNDATION_PLAN.md S5).
 *
 * Run via `make DEBUG=1 test-j004` (objects in build-debug/)
 * or as part of `make DEBUG=1 test-debug`. NOT part of
 * `make test`, whose composition is unchanged.
 *
 * Debug legs: layout capacities at exact arithmetic;
 * J-002 counter semantics unmoved; on-demand verify-all
 * clean, then detecting, then clean again after restore;
 * disposable CHILD processes that overwrite a post-zone
 * (8 bytes), a pre-zone (4 bytes) and a post-zone checked
 * on the realloc path, each required to die by SIGABRT
 * with the fixed MIMIR_DEBUG record on fd 2; a clean child
 * that must exit 0; a no-false-positive workload across
 * all classes in-process; a bench leg whose timings are
 * labelled timing-never-an-oracle.
 *
 * Production flavour (no MIMIR_DEBUG): only the flavour
 * leg runs -- production capacities at the banked
 * arithmetic and the bench -- proving the test itself
 * distinguishes the flavours.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */
#define _POSIX_C_SOURCE 200809L

#include "mimir_alloc.h"
#include "mimir_debug_j004.h"
#include "mimir_stats_j002.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
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

#ifdef MIMIR_DEBUG
static void check_u64(uint64_t got, uint64_t want,
                      const char *label)
{
    checks++;
    printf("  [%s] %s (got %llu, want %llu)\n",
           got == want ? "ok" : "FAIL", label,
           (unsigned long long)got,
           (unsigned long long)want);
    if (got != want) {
        failures++;
    }
}
#endif /* MIMIR_DEBUG: check_u64 is used by debug legs only */

static const size_t class_sizes[20] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};

static uint64_t bench_ns_per_op(void)
{
    struct timespec a, b;
    int i;

    clock_gettime(CLOCK_MONOTONIC, &a);
    for (i = 0; i < 20000; i++) {
        void *p = mimir_malloc(64);
        if (p == NULL) {
            return 0;
        }
        memset(p, 0x5A, 64);
        mimir_free(p);
    }
    clock_gettime(CLOCK_MONOTONIC, &b);
    return (uint64_t)(b.tv_sec - a.tv_sec) * 1000000000ull +
           (uint64_t)(b.tv_nsec - a.tv_nsec);
}

#ifdef MIMIR_DEBUG
/* Child bodies: each returns only if detection FAILED to
 * fire; the parent requires death by SIGABRT instead. */
static void child_post_zone(void)
{
    unsigned char *p = mimir_malloc(48);

    if (p == NULL) {
        _exit(43);
    }
    memset(p, 0x11, 48);
    memset(p + 48, 0xFF, 8); /* 8 bytes into the post-zone */
    mimir_free(p);
    _exit(42); /* detection did not fire */
}

static void child_pre_zone(void)
{
    unsigned char *p = mimir_malloc(48);

    if (p == NULL) {
        _exit(43);
    }
    memset(p, 0x11, 48);
    memset(p - 4, 0xFF, 4); /* 4 bytes of the pre-zone */
    mimir_free(p);
    _exit(42);
}

static void child_realloc_zone(void)
{
    unsigned char *p = mimir_malloc(32);

    if (p == NULL) {
        _exit(43);
    }
    memset(p, 0x11, 32);
    p[32] = 0x00; /* first post-zone byte */
    (void)mimir_realloc(p, 5000);
    _exit(42);
}

static void child_clean(void)
{
    int i;

    for (i = 0; i < 200; i++) {
        size_t n = (size_t)(i % 1024) + 1;
        unsigned char *p = mimir_malloc(n);

        if (p == NULL) {
            _exit(43);
        }
        memset(p, 0x77, n);
        mimir_free(p);
    }
    _exit(mimir_core_debug_verify_all() == 0 ? 0 : 44);
}

/* Run fn in a child with fd 2 captured; require SIGABRT and
 * the fixed record naming `where`. */
static void run_child_expect_abort(void (*fn)(void),
                                   const char *where,
                                   const char *label)
{
    int fds[2];
    pid_t pid;
    char buf[512];
    ssize_t total = 0;
    ssize_t r;
    int status = 0;

    if (pipe(fds) != 0) {
        check(0, "pipe for child leg");
        return;
    }
    fflush(stdout);
    pid = fork();
    if (pid == 0) {
        (void)dup2(fds[1], 2);
        (void)close(fds[0]);
        (void)close(fds[1]);
        fn();
        _exit(45);
    }
    (void)close(fds[1]);
    while (total < (ssize_t)sizeof buf - 1 &&
           (r = read(fds[0], buf + total,
                     sizeof buf - 1 - (size_t)total)) > 0) {
        total += r;
    }
    (void)close(fds[0]);
    buf[total] = '\0';
    (void)waitpid(pid, &status, 0);
    printf("  child stderr: %s", total > 0 ? buf : "(none)\n");
    check(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT,
          label);
    check(strstr(buf, "MIMIR_DEBUG J-004 CORRUPTION") != NULL &&
              strstr(buf, where) != NULL,
          "child record names the check point");
}
#endif /* MIMIR_DEBUG */

int main(void)
{
    struct mimir_stats s0, s1;
    size_t page;
    int i;

    printf("test_j004: Phase 10 debug red zones + canaries (J-004)\n");
    if (mimir_core_stats_snapshot(&s0) != 0) {
        check(0, "baseline snapshot");
        printf("RESULT: FAIL\n");
        return 1;
    }
    page = (size_t)s0.page_size_bytes;
    check(page > 0, "page size discovered");
    printf("  page size = %zu bytes\n", page);

#ifdef MIMIR_DEBUG
    printf("  flavour: DEBUG (MIMIR_DEBUG defined)\n");
    {
        /* Leg 1: capacities at exact debug arithmetic. One
         * live object per class => exactly one span each. */
        void *keep[20];
        int all_ok = 1;

        for (i = 0; i < 20; i++) {
            keep[i] = mimir_malloc(class_sizes[i]);
            if (keep[i] == NULL) {
                all_ok = 0;
            }
        }
        check(all_ok, "leg1: one alloc per class non-NULL");
        if (mimir_core_stats_snapshot(&s1) == 0) {
            printf("  class  prod_cap  debug_cap(expected=observed)\n");
            for (i = 0; i < 20; i++) {
                uint64_t prod =
                    (uint64_t)((page - 96) / class_sizes[i]);
                uint64_t dbg = (uint64_t)((page - 96) /
                                          (class_sizes[i] + 32));
                printf("  %5zu  %8llu  %8llu\n", class_sizes[i],
                       (unsigned long long)prod,
                       (unsigned long long)s1.span_capacity_objects[i]);
                if (s1.span_capacity_objects[i] != dbg) {
                    all_ok = 0;
                }
            }
        } else {
            all_ok = 0;
        }
        check(all_ok, "leg1: debug capacities == (page-96)/(class+32)");
        for (i = 0; i < 20; i++) {
            mimir_free(keep[i]);
        }
    }
    {
        /* Leg 2: J-002 counter semantics unmoved by guards. */
        uint64_t ml = (uint64_t)((page + 5000 + page - 1) / page * page);
        struct mimir_stats sb;

        {
            struct mimir_stats pre;
            void *x, *y, *z;
            uint64_t usable = ml - page;

            (void)mimir_core_stats_snapshot(&pre);
            x = mimir_malloc(100);
            y = mimir_malloc(30);
            z = mimir_malloc(5000);
            (void)mimir_core_stats_snapshot(&sb);
            check_u64(sb.bytes_requested - pre.bytes_requested,
                      100 + 30 + 5000,
                      "leg2: bytes_requested delta == requested sum");
            check_u64(sb.bytes_allocated - pre.bytes_allocated,
                      112 + 32 + usable,
                      "leg2: bytes_allocated delta == class/usable sum (guards NOT counted)");
            mimir_free(x);
            mimir_free(y);
            mimir_free(z);
        }
    }
    check(mimir_core_debug_verify_all() == 0,
          "leg3: verify_all clean at quiescent point");
    {
        /* Leg 4: verify_all detects, restore heals the
         * evidence (test-side restore only; the allocator
         * itself never repairs). */
        unsigned char *p = mimir_malloc(64);

        check(p != NULL, "leg4: alloc non-NULL");
        p[64] = 0x00;
        check(mimir_core_debug_verify_all() >= 1,
              "leg4: verify_all detects post-zone overwrite");
        p[64] = 0xA5;
        check(mimir_core_debug_verify_all() == 0,
              "leg4: verify_all clean after test-side restore");
        mimir_free(p);
    }
    run_child_expect_abort(child_post_zone, "free",
                           "leg5: post-zone overwrite on free -> SIGABRT");
    run_child_expect_abort(child_pre_zone, "free",
                           "leg6: pre-zone overwrite on free -> SIGABRT");
    run_child_expect_abort(child_realloc_zone, "realloc",
                           "leg7: post-zone overwrite on realloc -> SIGABRT");
    {
        /* Leg 8: clean child exits 0. */
        pid_t pid;
        int status = 0;

        fflush(stdout);
        pid = fork();
        if (pid == 0) {
            child_clean();
        }
        (void)waitpid(pid, &status, 0);
        check(WIFEXITED(status) && WEXITSTATUS(status) == 0,
              "leg8: clean child exits 0 (no false positive)");
    }
    {
        /* Leg 9: no false positives, all classes, in-process. */
        void *ptrs[500];
        uint32_t rng = 0x12345678u;
        int ok = 1;

        for (i = 0; i < 500; i++) {
            size_t n;
            rng = rng * 1664525u + 1013904223u;
            n = (size_t)((rng >> 16) % 1024) + 1;
            ptrs[i] = mimir_malloc(n);
            if (ptrs[i] == NULL) {
                ok = 0;
                break;
            }
            memset(ptrs[i], 0x3C, n);
        }
        check(ok, "leg9: 500-block workload allocated");
        check(mimir_core_debug_verify_all() == 0,
              "leg9: verify_all clean mid-workload");
        for (i = 0; i < 500; i += 2) {
            mimir_free(ptrs[i]);
            ptrs[i] = NULL;
        }
        for (i = 1; i < 500; i += 2) {
            mimir_free(ptrs[i]);
        }
        check(mimir_core_debug_verify_all() == 0,
              "leg9: verify_all clean after frees");
        {
            void *z = mimir_calloc(10, 16);
            int zeroed = 1;
            int k;

            if (z == NULL) {
                zeroed = 0;
            } else {
                for (k = 0; k < 160; k++) {
                    if (((unsigned char *)z)[k] != 0) {
                        zeroed = 0;
                    }
                }
            }
            check(zeroed, "leg9: calloc extent zeroed under debug");
            mimir_free(z);
        }
    }
    {
        /* Leg 10: B-007 C1.4 clean-init, span form, in debug
         * arithmetic. Replicates every header-field clause
         * of the banked g003_span_clean (magic, base, length,
         * usable_offset 96, class field, usable_size, LIVE,
         * class index, live_count >= 1) and replaces ONLY
         * the production stride identity (off-96) % cs == 0
         * with the debug identity (off-112) % (cs+32) == 0.
         * The banked clause itself is INC-J004-02. */
        unsigned char *q = mimir_malloc(37); /* class 48 */
        const unsigned char *h;
        uint64_t magic; size_t base_v, mlen, uoff, csf, usz;
        uint32_t state, cidx; size_t live, off;
        int ok;

        check(q != NULL, "leg10: alloc(37) non-NULL");
        h = (const unsigned char *)((uintptr_t)q & ~(uintptr_t)(page - 1));
        memcpy(&magic, h + 0, 8); memcpy(&base_v, h + 8, 8);
        memcpy(&mlen, h + 16, 8); memcpy(&uoff, h + 24, 8);
        memcpy(&csf, h + 32, 8); memcpy(&usz, h + 40, 8);
        memcpy(&state, h + 48, 4); memcpy(&cidx, h + 52, 4);
        memcpy(&live, h + 80, 8);
        off = (size_t)(q - h);
        ok = magic == UINT64_C(0x4D494D495253504E) &&
             base_v == (size_t)(uintptr_t)h && mlen == page &&
             uoff == 96 && csf == 48 && usz == page - 96 &&
             state == 1 && cidx < 20 && live >= 1 &&
             off >= 112 && (off - 112) % (48 + 32) == 0;
        check(ok, "leg10: C1.4 header clauses hold, debug stride identity (off-112)%(cs+32)==0");
        mimir_free(q);
    }
#else
    printf("  flavour: PRODUCTION (MIMIR_DEBUG absent)\n");
    {
        void *keep[20];
        int all_ok = 1;

        for (i = 0; i < 20; i++) {
            keep[i] = mimir_malloc(class_sizes[i]);
            if (keep[i] == NULL) {
                all_ok = 0;
            }
        }
        if (mimir_core_stats_snapshot(&s1) == 0) {
            for (i = 0; i < 20; i++) {
                if (s1.span_capacity_objects[i] !=
                    (uint64_t)((page - 96) / class_sizes[i])) {
                    all_ok = 0;
                }
            }
        } else {
            all_ok = 0;
        }
        check(all_ok,
              "prod leg: capacities == banked (page-96)/class");
        for (i = 0; i < 20; i++) {
            mimir_free(keep[i]);
        }
    }
#endif
    {
        uint64_t total = bench_ns_per_op();

        check(total > 0, "bench: 20000 x malloc(64)/free ran");
        printf("  bench: %llu ns/op over 20000 ops "
               "(timing-never-an-oracle)\n",
               (unsigned long long)(total / 20000));
    }
    printf("checks: %d, failures: %d\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
