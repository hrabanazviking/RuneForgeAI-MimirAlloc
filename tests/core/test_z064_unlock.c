/*
 * test_z064_unlock.c -- Z-064 lock-failure observability
 * (D-01) evidence.
 *
 * Slice: Z-064 (MIMIRALLOC_Z13_ROADMAP.md, Wave 8 opener).
 * Standalone via `make test-z064`; NOT part of `make test`
 * (the Z-052 pattern), so the standing gate's counts are
 * untouched. Links the library OBJECTS directly plus the
 * test-only unlock shim (I-004 --wrap heritage, extended
 * to mimir_platform_global_unlock).
 *
 * Gate legs (roadmap: "injection run shows the tally
 * moving; standing gate green"):
 *  1. CONTROL: a mixed small/large workload with no
 *     injection. The tally (stats snapshot field
 *     unlock_failures AND the lock-free getter) stays 0
 *     while the shim sees real unlock traffic.
 *  2. REPORT: the shim performs the real unlock but
 *     reports ERR_LOCK to the core for one armed unlock.
 *     The stats snapshot shows the tally at exactly 1,
 *     the getter agrees, and further allocator work
 *     succeeds (the allocator's own handling of a reported
 *     unlock failure adds no wedge). Two more armed report
 *     failures move the tally to exactly 3.
 *  3. TRUE: in a forked child, the shim does NOT perform
 *     the armed unlock (a genuine failure's exact state:
 *     the mutex stays locked). The child's one large
 *     allocation still returns its block (unlock failure
 *     is post-publication), the tally moves by exactly 1,
 *     read back through the lock-free getter (a locked
 *     snapshot is impossible in that state -- that is
 *     D-01's wedge, and why the getter exists), and the
 *     child exits promptly without taking the lock again:
 *     the failure is counted and reported, never silent.
 *     The parent's tally is unaffected (process copy) and
 *     the parent keeps allocating.
 * The test prints TALLY evidence lines for the recorded
 * run in addition to its CHECKS summary.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "mimir_alloc.h"
#include "mimir_stats_j002.h"
#include "mimir_unlock_shim_z064.h"

static int checks = 0;
static int failures = 0;

#define CHECK(cond, msg) do { \
        checks++; \
        if (!(cond)) { failures++; \
            printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } \
    } while (0)

static uint64_t snap_tally(void)
{
    struct mimir_stats s;

    if (mimir_core_stats_snapshot(&s) != 0) {
        return UINT64_MAX;
    }
    return s.unlock_failures;
}

/* Mixed workload guaranteed to exercise central lock /
 * unlock traffic on both the small (span) and large
 * (mapping) paths. Contents are pattern-touched so the
 * work cannot be optimised away. */
static void workload(void)
{
    void *small[64];
    void *large[8];
    size_t i;

    for (i = 0; i < 64; i++) {
        small[i] = mimir_malloc(32 + i);
        if (small[i] != NULL) {
            memset(small[i], (int)(0x40 + i), 32 + i);
        }
    }
    for (i = 0; i < 8; i++) {
        large[i] = mimir_malloc(200000 + i * 4096);
        if (large[i] != NULL) {
            memset(large[i], (int)(0x60 + i), 4096);
        }
    }
    for (i = 0; i < 64; i++) {
        mimir_free(small[i]);
    }
    for (i = 0; i < 8; i++) {
        mimir_free(large[i]);
    }
}

int main(void)
{
    uint64_t t0, t1, t3;
    void *p;
    int pipefd[2];
    pid_t pid;
    int status = 0;
    int waited_ms;
    char buf[256];
    ssize_t nread;
    size_t total;
    unsigned long long child_before = 0, child_after = 0;
    int child_parsed;

    setvbuf(stdout, NULL, _IOLBF, 0);

    /* ---- Leg 1: CONTROL, no injection ------------------ */
    mimir_unlock_disarm();
    workload();
    workload();
    t0 = snap_tally();
    printf("TALLY control snapshot=%llu getter=%llu "
           "shim_attempts=%lu shim_injected=%lu "
           "shim_real_failed=%lu\n",
           (unsigned long long)t0,
           (unsigned long long)mimir_core_unlock_failures(),
           mimir_unlock_attempts,
           mimir_unlock_injected_report +
               mimir_unlock_injected_true,
           mimir_unlock_real_failed);
    CHECK(t0 == 0, "control: snapshot tally is 0");
    CHECK(mimir_core_unlock_failures() == 0,
          "control: getter tally is 0");
    CHECK(mimir_unlock_attempts > 0,
          "control: shim saw real unlock traffic");
    CHECK(mimir_unlock_injected_report == 0 &&
          mimir_unlock_injected_true == 0,
          "control: nothing injected");
    CHECK(mimir_unlock_real_failed == 0,
          "control: no genuine unlock failure on this host");

    /* ---- Leg 2: REPORT injection ----------------------- */
    mimir_unlock_arm_report(1);
    p = mimir_malloc(300000);
    CHECK(p != NULL, "report: large allocation succeeds");
    if (p != NULL) {
        memset(p, 0x5a, 4096);
    }
    t1 = snap_tally();
    printf("TALLY report1 snapshot=%llu getter=%llu "
           "shim_injected_report=%lu\n",
           (unsigned long long)t1,
           (unsigned long long)mimir_core_unlock_failures(),
           mimir_unlock_injected_report);
    CHECK(t1 == 1, "report: snapshot tally moved to exactly 1");
    CHECK(mimir_core_unlock_failures() == 1,
          "report: getter agrees at 1");
    CHECK(mimir_unlock_injected_report == 1,
          "report: shim injected exactly 1");
    mimir_free(p);

    /* The process is unwedged: more work, tally unmoved. */
    workload();
    CHECK(snap_tally() == 1,
          "report: tally stays 1 across later clean work");

    mimir_unlock_arm_report(1);
    p = mimir_malloc(300000);
    CHECK(p != NULL, "report2: large allocation succeeds");
    mimir_unlock_arm_report(1);
    mimir_free(p);
    t3 = snap_tally();
    printf("TALLY report3 snapshot=%llu getter=%llu "
           "shim_injected_report=%lu\n",
           (unsigned long long)t3,
           (unsigned long long)mimir_core_unlock_failures(),
           mimir_unlock_injected_report);
    CHECK(t3 == 3, "report: tally at exactly 3 after two more");
    CHECK(mimir_core_unlock_failures() == 3,
          "report: getter agrees at 3");

    /* ---- Leg 3: TRUE injection in a forked child ------- */
    if (pipe(pipefd) != 0) {
        CHECK(0, "true: pipe() failed");
        printf("CHECKS: %d run, %d failed\n", checks, failures);
        printf("RESULT: FAIL\n");
        return 1;
    }
    pid = fork();
    if (pid == 0) {
        uint64_t before, after;
        void *q;

        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            _exit(4);
        }
        close(pipefd[1]);
        before = mimir_core_unlock_failures();
        mimir_unlock_arm_true(1);
        q = mimir_malloc(300000);
        after = mimir_core_unlock_failures();
        printf("CHILD_TALLY before=%llu after=%llu alloc_ok=%d\n",
               (unsigned long long)before,
               (unsigned long long)after,
               q != NULL ? 1 : 0);
        fflush(stdout);
        /* No further lock-taking call: the mutex is truly
         * still locked (the genuine failure state). Exit
         * promptly -- counted and reported, never silent,
         * and this harness process is not wedged. */
        _exit(q != NULL && after == before + 1 ? 0 : 3);
    }
    CHECK(pid > 0, "true: fork succeeded");
    close(pipefd[1]);
    waited_ms = 0;
    if (pid > 0) {
        for (;;) {
            pid_t w = waitpid(pid, &status, WNOHANG);

            if (w == pid) {
                break;
            }
            if (waited_ms >= 10000) {
                kill(pid, 9);
                (void)waitpid(pid, &status, 0);
                CHECK(0, "true: child wedged past 10 s bound");
                break;
            }
            usleep(10000);
            waited_ms += 10;
        }
    }
    total = 0;
    while (total + 1 < sizeof buf &&
           (nread = read(pipefd[0], buf + total,
                         sizeof buf - 1 - total)) > 0) {
        total += (size_t)nread;
    }
    buf[total] = '\0';
    close(pipefd[0]);
    printf("TALLY true-child waited_ms=%d exit=%s raw: %s",
           waited_ms,
           (pid > 0 && WIFEXITED(status) &&
            WEXITSTATUS(status) == 0) ? "0" : "nonzero",
           buf);
    if (buf[0] != '\0' && buf[strlen(buf) - 1] != '\n') {
        printf("\n");
    }
    child_parsed = sscanf(buf, "CHILD_TALLY before=%llu after=%llu",
                          &child_before, &child_after);
    CHECK(child_parsed == 2, "true: child tally line parsed");
    CHECK(child_after == child_before + 1,
          "true: child tally moved by exactly 1");
    CHECK(pid > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "true: child exited 0 promptly (no wedge in harness)");

    /* Parent unaffected (the child's tally was its copy)
     * and still fully in service. */
    CHECK(snap_tally() == 3,
          "true: parent tally still exactly 3 after child");
    workload();
    CHECK(snap_tally() == 3,
          "true: parent keeps working, tally still 3");

    printf("CHECKS: %d run, %d failed\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
