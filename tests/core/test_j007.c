/*
 * test_j007.c -- evidence test for slice J-007 (Phase 10
 * allocation-free diagnostics / error reporting;
 * docs/PHASE_10_DIAGNOSTICS.md; J-001 exit item X6).
 *
 * The channel under test (src/core/mimir_core.c,
 * mimir_core_diag_report) claims:
 *  (a) USEFUL: one fixed-format line per genuine failure
 *      naming op, cause class, requested bytes and the
 *      J-002 counter snapshot (failures / posture).
 *  (b) ALLOCATION-FREE: no malloc-family call, no L6 call
 *      on the report path -- proven here with the counting
 *      interposer of tests/core/mimir_diag_shim_j007.c
 *      (--wrap malloc/calloc/realloc/free/write) plus the
 *      I-004 fault shim's L6 counters, including an
 *      emission while every backing acquire is armed to
 *      fail.
 *  (c) RECURSION-PROOF: a report attempted from inside a
 *      report's own write(2) is suppressed (counted), the
 *      emitted line is exactly one, and max depth stays 1;
 *      a report emitted from inside an allocation failure
 *      adds ZERO further L6 acquires.
 *  (d) WIRED + QUIET WHERE IT MUST BE: real failure NULLs
 *      (calloc overflow, oversize malloc, injected backing
 *      / lock / page-size failures, realloc grow backing
 *      failure) each emit exactly one matching line;
 *      posture NULLs (malloc(0), calloc zero factor,
 *      realloc(ptr,0)), successful calls and invalid-
 *      pointer realloc emit NOTHING, and the J-002 counters
 *      keep their disjoint semantics throughout.
 *
 * Capture: fd 2 is redirected to a pipe in-process for the
 * duration of each leg (no child process is needed: no leg
 * aborts). All assertions are on captured bytes, counter
 * deltas read from the J-002 snapshot surface, the I-004
 * counters and the interposer counters -- never on timing.
 */
#define _POSIX_C_SOURCE 200809L /* setenv / unsetenv / dup2 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../include/mimir_alloc.h"
#include "mimir_stats_j002.h"
#include "mimir_diag_j007.h"
#include "mimir_fault_i004.h"
#include "mimir_diag_shim_j007.h"

static int checks;
static int failures;

#define CHECK(cond) do { \
        checks++; \
        if (!(cond)) { \
            failures++; \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

/* ---- fd 2 capture ---------------------------------------- */
static int cap_pipe[2];
static int cap_saved = -1;

static void cap_start(void)
{
    fflush(NULL);
    if (pipe(cap_pipe) != 0) { perror("pipe"); exit(2); }
    cap_saved = dup(STDERR_FILENO);
    if (cap_saved < 0 || dup2(cap_pipe[1], STDERR_FILENO) < 0) {
        perror("dup"); exit(2);
    }
}

static size_t cap_stop(char *buf, size_t cap)
{
    size_t total = 0;
    ssize_t n;
    fflush(NULL);
    if (dup2(cap_saved, STDERR_FILENO) < 0) { perror("dup2"); exit(2); }
    close(cap_saved);
    close(cap_pipe[1]);
    while (total + 1 < cap &&
           (n = read(cap_pipe[0], buf + total, cap - 1 - total)) > 0) {
        total += (size_t)n;
    }
    close(cap_pipe[0]);
    buf[total] = '\0';
    return total;
}

/* ---- field parsing ---------------------------------------- */
static int field_u64(const char *line, const char *key,
                     uint64_t *out)
{
    const char *p = strstr(line, key);
    char *end = NULL;
    if (p == NULL) { return 0; }
    p += strlen(key);
    *out = (uint64_t)strtoull(p, &end, 10);
    return end != p;
}

static int line_is(const char *line, const char *op,
                   const char *cause, uint64_t req,
                   uint64_t failures_want, uint64_t posture_want)
{
    char want_prefix[128];
    uint64_t v;
    snprintf(want_prefix, sizeof want_prefix,
             "mimiralloc: error op=%s cause=%s req=", op, cause);
    if (strncmp(line, want_prefix, strlen(want_prefix)) != 0) {
        return 0;
    }
    if (!field_u64(line, "req=", &v) || v != req) { return 0; }
    if (!field_u64(line, "failures=", &v) || v != failures_want) {
        return 0;
    }
    if (!field_u64(line, "posture=", &v) || v != posture_want) {
        return 0;
    }
    /* exactly one line: one newline, at the very end */
    {
        size_t len = strlen(line);
        size_t i;
        int nl = 0;
        for (i = 0; i < len; i++) { if (line[i] == '\n') { nl++; } }
        if (nl != 1 || len == 0 || line[len - 1] != '\n') {
            return 0;
        }
    }
    return 1;
}

static void wrap_reset(void)
{
    mimir_diag_wrap_malloc_calls = 0;
    mimir_diag_wrap_calloc_calls = 0;
    mimir_diag_wrap_realloc_calls = 0;
    mimir_diag_wrap_free_calls = 0;
    mimir_diag_wrap_write_calls = 0;
}

static int wrap_alloc_zero(void)
{
    return mimir_diag_wrap_malloc_calls == 0 &&
           mimir_diag_wrap_calloc_calls == 0 &&
           mimir_diag_wrap_realloc_calls == 0 &&
           mimir_diag_wrap_free_calls == 0;
}

int main(void)
{
    char buf[1024];
    struct mimir_stats s0, s1;
    size_t n;
    void *p;
    uint64_t e0, sup0;
    unsigned long acq0;

    /* L0: INC-J007-01 gate -- with MIMIRALLOC_DIAG unset, a
     * genuine failure and a direct report call emit NOTHING
     * and move no channel counter (the C-006 silence
     * posture), while the J-002 failure counter still
     * counts the failure itself. */
    unsetenv("MIMIRALLOC_DIAG");
    e0 = mimir_core_diag_emitted();
    cap_start();
    mimir_core_diag_report(MIMIR_DIAG_OP_MALLOC,
                           MIMIR_DIAG_CAUSE_BACKING, 4096);
    p = mimir_malloc(SIZE_MAX);
    n = cap_stop(buf, sizeof buf);
    CHECK(p == NULL);
    CHECK(n == 0);
    CHECK(mimir_core_diag_emitted() == e0);
    setenv("MIMIRALLOC_DIAG", "1", 1);

    /* L1: direct emission -- format, fields, snapshot values. */
    CHECK(mimir_core_stats_snapshot(&s0) == 0);
    e0 = mimir_core_diag_emitted();
    cap_start();
    mimir_core_diag_report(MIMIR_DIAG_OP_MALLOC,
                           MIMIR_DIAG_CAUSE_BACKING, 4096);
    n = cap_stop(buf, sizeof buf);
    CHECK(n > 0 && n < 224);
    CHECK(line_is(buf, "malloc", "backing", 4096,
                  s0.alloc_failures, s0.alloc_posture_nulls));
    CHECK(mimir_core_diag_emitted() == e0 + 1);
    CHECK(mimir_core_diag_max_depth() == 1);
    printf("L1 line: %s", buf);

    /* L2: zero allocation on the report path, and the report
     * still emits while backing allocation is armed to fail
     * (I-004 acquire arm) -- the interposer counters and the
     * L6 counters must not move at all during emission. */
    mimir_fault_arm_acquire(1);
    acq0 = mimir_fault_acquire_attempts;
    cap_start();
    wrap_reset();
    mimir_core_diag_report(MIMIR_DIAG_OP_CALLOC,
                           MIMIR_DIAG_CAUSE_ARITHMETIC,
                           (uint64_t)SIZE_MAX);
    CHECK(wrap_alloc_zero());
    CHECK(mimir_diag_wrap_write_calls >= 1);
    n = cap_stop(buf, sizeof buf);
    mimir_fault_disarm();
    CHECK(mimir_fault_acquire_attempts == acq0);
    CHECK(mimir_core_stats_snapshot(&s1) == 0);
    CHECK(line_is(buf, "calloc", "arithmetic", (uint64_t)SIZE_MAX,
                  s1.alloc_failures, s1.alloc_posture_nulls));
    printf("L2 line: %s", buf);

    /* L3: wiring -- calloc checked-arithmetic refusal. */
    CHECK(mimir_core_stats_snapshot(&s0) == 0);
    cap_start();
    p = mimir_calloc(SIZE_MAX, 2);
    n = cap_stop(buf, sizeof buf);
    CHECK(p == NULL);
    CHECK(mimir_core_stats_snapshot(&s1) == 0);
    CHECK(s1.alloc_failures == s0.alloc_failures + 1);
    CHECK(s1.alloc_posture_nulls == s0.alloc_posture_nulls);
    CHECK(line_is(buf, "calloc", "arithmetic", (uint64_t)SIZE_MAX,
                  s0.alloc_failures + 1, s0.alloc_posture_nulls));
    printf("L3 line: %s", buf);

    /* L4: wiring -- oversize malloc arithmetic refusal, with
     * ZERO backing acquires attempted. */
    acq0 = mimir_fault_acquire_attempts;
    cap_start();
    p = mimir_malloc(SIZE_MAX);
    n = cap_stop(buf, sizeof buf);
    CHECK(p == NULL);
    CHECK(mimir_fault_acquire_attempts == acq0);
    CHECK(line_is(buf, "malloc", "arithmetic", (uint64_t)SIZE_MAX,
                  s1.alloc_failures + 1, s1.alloc_posture_nulls));
    printf("L4 line: %s", buf);

    /* L5: wiring + recursion proof P1 -- injected backing
     * failure on a large malloc. Exactly ONE acquire attempt
     * in total: the report emitted from inside the failure
     * re-entered nothing (no acquire, no malloc-family call). */
    CHECK(mimir_core_stats_snapshot(&s0) == 0);
    e0 = mimir_core_diag_emitted();
    mimir_fault_arm_acquire(1);
    acq0 = mimir_fault_acquire_attempts;
    cap_start();
    wrap_reset();
    p = mimir_malloc(200000);
    CHECK(wrap_alloc_zero());
    n = cap_stop(buf, sizeof buf);
    mimir_fault_disarm();
    CHECK(p == NULL);
    CHECK(mimir_fault_acquire_attempts == acq0 + 1);
    CHECK(mimir_core_diag_emitted() == e0 + 1);
    CHECK(line_is(buf, "malloc", "backing", 200000,
                  s0.alloc_failures + 1, s0.alloc_posture_nulls));
    printf("L5 line: %s", buf);

    /* L6: wiring -- injected lock failure on malloc. */
    CHECK(mimir_core_stats_snapshot(&s0) == 0);
    mimir_fault_arm_lock(1);
    cap_start();
    p = mimir_malloc(64);
    n = cap_stop(buf, sizeof buf);
    mimir_fault_disarm();
    CHECK(p == NULL);
    CHECK(line_is(buf, "malloc", "lock", 64,
                  s0.alloc_failures + 1, s0.alloc_posture_nulls));
    printf("L6 line: %s", buf);

    /* L7: wiring -- injected page-size failure on malloc. */
    CHECK(mimir_core_stats_snapshot(&s0) == 0);
    mimir_fault_arm_pagesize(1);
    cap_start();
    p = mimir_malloc(200000);
    n = cap_stop(buf, sizeof buf);
    mimir_fault_disarm();
    CHECK(p == NULL);
    CHECK(line_is(buf, "malloc", "page-size", 200000,
                  s0.alloc_failures + 1, s0.alloc_posture_nulls));
    printf("L7 line: %s", buf);

    /* L8: recursion proof P2 -- a report attempted from
     * inside the outer report's own write(2) (interposer
     * armed) is suppressed exactly once; exactly one line
     * is emitted; max depth stays 1. */
    sup0 = mimir_core_diag_suppressed();
    e0 = mimir_core_diag_emitted();
    mimir_diag_wrap_write_reenter = 1;
    cap_start();
    mimir_core_diag_report(MIMIR_DIAG_OP_MALLOC,
                           MIMIR_DIAG_CAUSE_INTERNAL, 42);
    n = cap_stop(buf, sizeof buf);
    CHECK(mimir_diag_wrap_write_reenter == 0);
    CHECK(mimir_core_diag_suppressed() == sup0 + 1);
    CHECK(mimir_core_diag_emitted() == e0 + 1);
    CHECK(mimir_core_diag_max_depth() == 1);
    CHECK(mimir_core_stats_snapshot(&s1) == 0);
    CHECK(line_is(buf, "malloc", "internal", 42,
                  s1.alloc_failures, s1.alloc_posture_nulls));
    printf("L8 line: %s", buf);

    /* L9: posture NULLs are NOT errors -- total silence,
     * posture counter +3, failures unmoved. */
    CHECK(mimir_core_stats_snapshot(&s0) == 0);
    cap_start();
    {
        void *a = mimir_malloc(0);
        void *b = mimir_calloc(0, 8);
        void *live = mimir_malloc(32);
        void *r = mimir_realloc(live, 0);
        CHECK(a == NULL && b == NULL && live != NULL && r == NULL);
        mimir_free(live);
    }
    n = cap_stop(buf, sizeof buf);
    CHECK(n == 0);
    CHECK(mimir_core_stats_snapshot(&s1) == 0);
    CHECK(s1.alloc_posture_nulls == s0.alloc_posture_nulls + 3);
    CHECK(s1.alloc_failures == s0.alloc_failures);

    /* L10: successful calls are silent. */
    cap_start();
    {
        void *a = mimir_malloc(128);
        void *b = mimir_calloc(4, 16);
        CHECK(a != NULL && b != NULL);
        mimir_free(a);
        mimir_free(b);
        mimir_free(NULL);
    }
    n = cap_stop(buf, sizeof buf);
    CHECK(n == 0);

    /* L11: wiring -- realloc grow with injected backing
     * failure: NULL, original block LIVE and intact, one
     * op=realloc cause=backing line. */
    {
        unsigned char *old = mimir_malloc(5000);
        size_t i;
        CHECK(old != NULL);
        for (i = 0; i < 5000; i++) { old[i] = (unsigned char)i; }
        CHECK(mimir_core_stats_snapshot(&s0) == 0);
        mimir_fault_arm_acquire(1);
        cap_start();
        p = mimir_realloc(old, 300000);
        n = cap_stop(buf, sizeof buf);
        mimir_fault_disarm();
        CHECK(p == NULL);
        CHECK(line_is(buf, "realloc", "backing", 300000,
                      s0.alloc_failures + 1,
                      s0.alloc_posture_nulls));
        printf("L11 line: %s", buf);
        {
            size_t bad = 0;
            for (i = 0; i < 5000; i++) {
                if (old[i] != (unsigned char)i) { bad++; }
            }
            CHECK(bad == 0);
        }
        mimir_free(old);
    }

    /* L12: invalid-pointer realloc -- the banked
     * non-destructive refusal: NULL, nothing counted,
     * nothing reported (not an allocation failure). */
    {
        static char foreign[64];
        CHECK(mimir_core_stats_snapshot(&s0) == 0);
        cap_start();
        p = mimir_realloc(foreign, 100);
        n = cap_stop(buf, sizeof buf);
        CHECK(p == NULL);
        CHECK(n == 0);
        CHECK(mimir_core_stats_snapshot(&s1) == 0);
        CHECK(s1.alloc_failures == s0.alloc_failures);
        CHECK(s1.alloc_posture_nulls == s0.alloc_posture_nulls);
    }

    printf("test_j007: %d checks / %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
