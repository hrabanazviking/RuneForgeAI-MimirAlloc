/*
 * test_c008.c -- C-008 controlled-failure evidence program
 * (roadmap Section 9.3) for the B-008 unsupported-behaviour
 * record (docs/PHASE_3_CALLOC_REALLOC_AND_UNSUPPORTED_BEHAVIOR.md
 * Sections 6-7).
 *
 * Slice C-008. Exercises, through the public L1 boundary only
 * (include/mimir_alloc.h), the controlled failures of roadmap
 * Section 9.3 that the Pi can induce WITHOUT a fault-injection
 * shim (none exists in the banked base -- INC-C007-01, carried
 * here as INC-C008-01):
 *
 *   S93-1  Integer / alignment / page boundaries (9.3 bullet
 *          4): allocations straddling the runtime system page
 *          and the fundamental alignment, calloc products at
 *          page multiples, a realloc grow across a page
 *          boundary, and the Defined refusals of B-008 S6.4
 *          (malloc oversize in both L2 forms, calloc product
 *          overflow in both factor orders, realloc to an
 *          unrepresentable and to a representable-but-
 *          unsatisfiable size) -- after EVERY induced failure
 *          a live sentinel is verified byte-for-byte intact
 *          and a fresh valid allocation succeeds (the recovery
 *          probe this slice's tasking requires).
 *   S93-2  Low process limits (9.3 bullet 2, first half): a
 *          forked child lowers its own RLIMIT_AS and allocates
 *          through the public API until the OS refuses; the
 *          refusal must arrive as exactly a null return (no
 *          crash, no hang, no partial block), and after the
 *          child frees its blocks a fresh allocation under the
 *          same limit succeeds. Exit-code protocol only; the
 *          child prints nothing. The exact byte threshold is
 *          environment-shaped and is NOT asserted
 *          (INC-C008-05).
 *   S93-3  Fragmented address space (9.3 bullet 2, second
 *          half): mixed-size blocks, every second one freed,
 *          survivors verified, new blocks allocated into the
 *          holed space; every live requested extent is then
 *          verified pairwise disjoint (9.3 bullet 7: failures
 *          and churn never return overlapping valid blocks).
 *   S93-4  Kill / restart (9.3 bullet 3): a child allocates,
 *          fills and verifies a block, signals readiness over
 *          a pipe and is SIGKILLed mid-work; a fresh child then
 *          allocates normally and the parent's sentinel is
 *          intact. The allocator keeps all state per-process
 *          (static registry, in-mapping descriptors), so this
 *          proves process isolation and restart health ONLY;
 *          application-level state recovery is outside Phase 3
 *          scope and is not claimed (INC-C008-02).
 *   S93-5  Failure storm + non-overlap (9.3 bullet 7): 120
 *          rounds interleaving induced Defined failures with
 *          live allocations; no call hangs (the suite itself
 *          completing is the evidence), live blocks stay
 *          pairwise disjoint with contents intact, and a
 *          recovery probe succeeds at the end.
 *
 * Cases that need what does not exist are printed as SKIP
 * lines with their reason and are never faked: the
 * fail-the-Nth-mapping / metadata-allocation injection sweep
 * and the EMFILE/EAGAIN classes (INC-C007-01), the Android
 * and proot tracks of 9.3 (this host is Linux only), the
 * fork-child lock state (U-17..U-21 open), and the teardown
 * interval (B-008 S6.3 X-8: no contract row covers it).
 *
 * Deliberately NOT exercised, per the house discipline
 * (tests/core/test_c005.c, B-009 NO-TEST-UB): every B-008
 * S6.2 UB case (foreign / interior / already-released
 * pointers, double free, use after free, out-of-extent
 * access, metadata reliance, excess-extent use, signal-
 * handler calls). Those rows are audited by inspection in
 * tests/core/AUDIT_B008_C008.md (INC-C008-03); running them
 * could not produce contract evidence and might crash the
 * suite. Zero-size and realloc(p, 0) postures are likewise
 * not re-run here: they are [UNRESOLVED: U-09 / U-12],
 * posture-only, already recorded by C-005, and hook C-006
 * stays BLOCKED (AUDIT_B008_C008.md Section C). No errno
 * value is asserted anywhere (U-10). Thread churn and
 * cross-thread transfer (9.3 bullet 5) and process
 * initialisation (bullet 6, init half) are NOT redone: they
 * are run by the C-007 suite (hook C-009) and the C-006
 * suite in the same `make test` session.
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every run check passed. Prints only facts it observed.
 */

#include "mimir_alloc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>

static int failures = 0;
static int checks = 0;
static int skips = 0;

static void check(int condition, const char *label)
{
    checks++;
    printf("  [%s] %s\n", condition ? "ok" : "FAIL", label);
    if (!condition) {
        failures++;
    }
}

static void skip(const char *id, const char *reason)
{
    skips++;
    printf("SKIP %s %s\n", id, reason);
}

static void fill_pattern(unsigned char *p, size_t n, unsigned seed)
{
    size_t i;

    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)(seed + (unsigned)(i * 31u));
    }
}

static int pattern_ok(const unsigned char *p, size_t n, unsigned seed)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (p[i] != (unsigned char)(seed + (unsigned)(i * 31u))) {
            return 0;
        }
    }
    return 1;
}

/* The recovery probe: after an induced failure, a subsequent
 * valid allocation must succeed, be usable, and free cleanly. */
static int recovery_ok(void)
{
    unsigned char *p = (unsigned char *)mimir_malloc(64);

    if (p == NULL) {
        return 0;
    }
    fill_pattern(p, 64, 0x5A);
    if (!pattern_ok(p, 64, 0x5A)) {
        mimir_free(p);
        return 0;
    }
    mimir_free(p);
    return 1;
}

struct live_block {
    unsigned char *ptr;
    size_t size;
    unsigned seed;
};

static int all_disjoint(const struct live_block *blocks, size_t count)
{
    size_t i;
    size_t j;

    for (i = 0; i < count; i++) {
        uintptr_t a0 = (uintptr_t)blocks[i].ptr;
        uintptr_t a1 = a0 + blocks[i].size;

        for (j = i + 1; j < count; j++) {
            uintptr_t b0 = (uintptr_t)blocks[j].ptr;
            uintptr_t b1 = b0 + blocks[j].size;

            if (a0 < b1 && b0 < a1) {
                return 0;
            }
        }
    }
    return 1;
}

static int all_patterns_ok(const struct live_block *blocks, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (!pattern_ok(blocks[i].ptr, blocks[i].size, blocks[i].seed)) {
            return 0;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* S93-1: boundaries + the B-008 S6.4 Defined refusals, each one      */
/* followed by sentinel verification and a recovery probe.            */
/* ------------------------------------------------------------------ */
static void section_boundaries(size_t page)
{
    size_t boundary[5];
    unsigned char *sentinel;
    size_t i;
    int ok = 1;

    printf("S93-1 boundaries and defined refusals (B-008 S6.4)\n");

    sentinel = (unsigned char *)mimir_malloc(64);
    check(sentinel != NULL, "sentinel block allocated");
    if (sentinel == NULL) {
        return;
    }
    fill_pattern(sentinel, 64, 0x11);

    boundary[0] = page - 1;
    boundary[1] = page;
    boundary[2] = page + 1;
    boundary[3] = 2 * page - 1;
    boundary[4] = 2 * page + 1;
    for (i = 0; i < 5; i++) {
        unsigned char *p = (unsigned char *)mimir_malloc(boundary[i]);

        if (p == NULL ||
            (uintptr_t)p % (uintptr_t)sizeof(void *) != 0) {
            ok = 0;
        }
        if (p != NULL) {
            fill_pattern(p, boundary[i], (unsigned)(0x20 + i));
            if (!pattern_ok(p, boundary[i], (unsigned)(0x20 + i))) {
                ok = 0;
            }
            mimir_free(p);
        }
    }
    check(ok, "malloc at page-1 / page / page+1 / 2page-1 / 2page+1: "
              "success, aligned, full extent usable");

    {
        /* calloc product exactly one page, and one page + 16. */
        unsigned char *z = (unsigned char *)mimir_calloc(16, page / 16);
        int zeroed = 1;

        if (z == NULL) {
            zeroed = 0;
        } else {
            for (i = 0; i < page; i++) {
                if (z[i] != 0) {
                    zeroed = 0;
                    break;
                }
            }
            mimir_free(z);
        }
        check(zeroed, "calloc product == one page: every requested "
                      "byte zero at return");
    }

    {
        unsigned char *p = (unsigned char *)mimir_malloc(page - 1);
        unsigned char *q;

        check(p != NULL, "realloc boundary: block of page-1 allocated");
        if (p != NULL) {
            fill_pattern(p, page - 1, 0x33);
            q = (unsigned char *)mimir_realloc(p, page + 1);
            check(q != NULL, "realloc grow page-1 -> page+1 succeeds");
            if (q != NULL) {
                check(pattern_ok(q, page - 1, 0x33),
                      "realloc across page boundary preserves the "
                      "whole old extent byte-for-byte");
                mimir_free(q);
            } else {
                mimir_free(p);
            }
        }
    }

    /* The Defined refusals (B-008 S6.4). After each one: null,
     * sentinel intact, recovery probe succeeds. */
    check(mimir_malloc(SIZE_MAX) == NULL,
          "refusal: malloc(SIZE_MAX) returns null (total-size overflow)");
    check(pattern_ok(sentinel, 64, 0x11) && recovery_ok(),
          "after malloc(SIZE_MAX): sentinel intact, recovery succeeds");

    check(mimir_malloc(SIZE_MAX - page + 1) == NULL,
          "refusal: malloc(SIZE_MAX - page + 1) returns null "
          "(reserve-sum overflow, pre-admission)");
    check(pattern_ok(sentinel, 64, 0x11) && recovery_ok(),
          "after reserve-overflow refusal: sentinel intact, recovery succeeds");

    check(mimir_malloc(SIZE_MAX / 2) == NULL,
          "refusal: malloc(SIZE_MAX/2) returns null (representable, "
          "unsatisfiable)");
    check(pattern_ok(sentinel, 64, 0x11) && recovery_ok(),
          "after unsatisfiable malloc: sentinel intact, recovery succeeds");

    check(mimir_calloc(SIZE_MAX, 2) == NULL,
          "refusal: calloc(SIZE_MAX, 2) returns null (product overflow)");
    check(pattern_ok(sentinel, 64, 0x11) && recovery_ok(),
          "after calloc overflow: sentinel intact, recovery succeeds");

    check(mimir_calloc(2, SIZE_MAX) == NULL,
          "refusal: calloc(2, SIZE_MAX) returns null (product overflow, "
          "swapped factors)");
    check(pattern_ok(sentinel, 64, 0x11) && recovery_ok(),
          "after swapped calloc overflow: sentinel intact, recovery succeeds");

    check(mimir_calloc(SIZE_MAX / 2 + 1, 2) == NULL,
          "refusal: calloc(SIZE_MAX/2 + 1, 2) returns null (product "
          "overflow at the boundary)");
    check(pattern_ok(sentinel, 64, 0x11) && recovery_ok(),
          "after boundary calloc overflow: sentinel intact, recovery succeeds");

    {
        unsigned char *p = (unsigned char *)mimir_malloc(128);

        check(p != NULL, "realloc refusal: original block allocated");
        if (p != NULL) {
            fill_pattern(p, 128, 0x44);
            check(mimir_realloc(p, SIZE_MAX) == NULL,
                  "refusal: realloc(p, SIZE_MAX) returns null");
            check(pattern_ok(p, 128, 0x44),
                  "after realloc(SIZE_MAX): original preserved "
                  "byte-for-byte (B-008 S6.4)");
            check(mimir_realloc(p, SIZE_MAX / 2) == NULL,
                  "refusal: realloc(p, SIZE_MAX/2) returns null "
                  "(representable, unsatisfiable)");
            check(pattern_ok(p, 128, 0x44) && recovery_ok(),
                  "after unsatisfiable realloc: original preserved, "
                  "recovery succeeds");
            mimir_free(p);
            check(recovery_ok(),
                  "original freed exactly as before the failed reallocs; "
                  "allocator healthy");
        }
    }

    check(pattern_ok(sentinel, 64, 0x11),
          "sentinel intact at end of S93-1");
    mimir_free(sentinel);
}

/* ------------------------------------------------------------------ */
/* S93-2: low process limit in a forked child (RLIMIT_AS).            */
/* Child exit codes: 0 ok; 10 no allocation succeeded; 11 no failure  */
/* observed within the loop cap; 12 recovery under the limit failed;  */
/* 13 setrlimit failed (parent reports the case SKIPPED).             */
/* ------------------------------------------------------------------ */
static void *rlimit_blocks[8192];

static void section_rlimit(void)
{
    pid_t pid;
    int status = 0;

    printf("S93-2 low process limit (RLIMIT_AS child)\n");
    pid = fork();
    if (pid < 0) {
        check(0, "rlimit case: fork succeeded");
        return;
    }
    if (pid == 0) {
        struct rlimit rl;
        size_t count = 0;
        int failed = 0;
        size_t i;
        unsigned char *q;

        rl.rlim_cur = (rlim_t)(96u * 1024u * 1024u);
        rl.rlim_max = (rlim_t)(96u * 1024u * 1024u);
        if (setrlimit(RLIMIT_AS, &rl) != 0) {
            _exit(13);
        }
        for (i = 0; i < 8192; i++) {
            void *p = mimir_malloc(4096);

            if (p == NULL) {
                failed = 1;
                break;
            }
            rlimit_blocks[count++] = p;
        }
        if (count == 0) {
            _exit(10);
        }
        if (!failed) {
            _exit(11);
        }
        for (i = 0; i < count; i++) {
            mimir_free(rlimit_blocks[i]);
        }
        q = (unsigned char *)mimir_malloc(4096);
        if (q == NULL) {
            _exit(12);
        }
        fill_pattern(q, 4096, 0x66);
        if (!pattern_ok(q, 4096, 0x66)) {
            _exit(12);
        }
        mimir_free(q);
        _exit(0);
    }
    if (waitpid(pid, &status, 0) < 0) {
        check(0, "rlimit case: waitpid succeeded");
        return;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 13) {
        skip("S93-2", "SKIPPED/NOT-YET: setrlimit(RLIMIT_AS) failed in "
                      "the child on this host; the low-limit case could "
                      "not be instantiated");
        return;
    }
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "rlimit child: allocations succeed until the OS refuses, the "
          "refusal is exactly null (child reached it alive, no crash "
          "or hang), and after freeing, a fresh allocation under the "
          "same limit succeeds");
    check(recovery_ok(),
          "parent allocator healthy after the rlimit child");
}

/* ------------------------------------------------------------------ */
/* S93-3: fragmented address space + non-overlap.                     */
/* ------------------------------------------------------------------ */
static void section_fragmentation(void)
{
    static const size_t cycle[5] = {64, 1000, 5000, 20000, 333};
    struct live_block live[48];
    void *blocks[48];
    size_t sizes[48];
    size_t live_count = 0;
    size_t i;
    int ok = 1;

    printf("S93-3 fragmented address space\n");
    for (i = 0; i < 48; i++) {
        sizes[i] = cycle[i % 5];
        blocks[i] = mimir_malloc(sizes[i]);
        if (blocks[i] == NULL) {
            ok = 0;
        } else {
            fill_pattern((unsigned char *)blocks[i], sizes[i],
                         (unsigned)(0x70 + i));
        }
    }
    check(ok, "fragmentation setup: 48 mixed-size blocks allocated");

    for (i = 1; i < 48; i += 2) {
        mimir_free(blocks[i]);
        blocks[i] = NULL;
    }
    ok = 1;
    for (i = 0; i < 48; i += 2) {
        if (!pattern_ok((unsigned char *)blocks[i], sizes[i],
                        (unsigned)(0x70 + i))) {
            ok = 0;
        }
    }
    check(ok, "survivors byte-for-byte intact after every second "
              "block is freed (holes opened)");

    for (i = 0; i < 48; i += 2) {
        live[live_count].ptr = (unsigned char *)blocks[i];
        live[live_count].size = sizes[i];
        live[live_count].seed = (unsigned)(0x70 + i);
        live_count++;
    }
    ok = 1;
    for (i = 0; i < 24; i++) {
        size_t n = 777 + (i * 61) % 12000;
        unsigned char *p = (unsigned char *)mimir_malloc(n);

        if (p == NULL) {
            ok = 0;
        } else {
            fill_pattern(p, n, (unsigned)(0xA0 + i));
            live[live_count].ptr = p;
            live[live_count].size = n;
            live[live_count].seed = (unsigned)(0xA0 + i);
            live_count++;
        }
    }
    check(ok, "24 new blocks allocated into the holed space");
    check(all_disjoint(live, live_count),
          "every live requested extent pairwise disjoint after "
          "fragmentation churn (no overlapping valid blocks)");
    check(all_patterns_ok(live, live_count),
          "every live block's contents intact after fragmentation churn");
    for (i = 0; i < live_count; i++) {
        mimir_free(live[i].ptr);
    }
    check(recovery_ok(), "recovery succeeds after fragmentation case");
}

/* ------------------------------------------------------------------ */
/* S93-4: kill / restart.                                             */
/* ------------------------------------------------------------------ */
static void section_kill_restart(void)
{
    int pipefd[2];
    pid_t pid;
    int status = 0;
    char byte = 0;
    unsigned char *sentinel;

    printf("S93-4 kill / restart during work\n");
    sentinel = (unsigned char *)mimir_malloc(64);
    check(sentinel != NULL, "kill case: parent sentinel allocated");
    if (sentinel == NULL) {
        return;
    }
    fill_pattern(sentinel, 64, 0x22);

    if (pipe(pipefd) != 0) {
        check(0, "kill case: pipe created");
        mimir_free(sentinel);
        return;
    }
    pid = fork();
    if (pid < 0) {
        check(0, "kill case: fork succeeded");
        (void)close(pipefd[0]);
        (void)close(pipefd[1]);
        mimir_free(sentinel);
        return;
    }
    if (pid == 0) {
        unsigned char *p;

        (void)close(pipefd[0]);
        p = (unsigned char *)mimir_malloc(8192);
        if (p == NULL) {
            _exit(3);
        }
        fill_pattern(p, 8192, 0x77);
        if (!pattern_ok(p, 8192, 0x77)) {
            _exit(4);
        }
        if (write(pipefd[1], "R", 1) != 1) {
            _exit(5);
        }
        for (;;) {
            (void)pause();
        }
    }
    (void)close(pipefd[1]);
    if (read(pipefd[0], &byte, 1) != 1) {
        check(0, "kill case: child signalled readiness");
        (void)kill(pid, SIGKILL);
        (void)waitpid(pid, &status, 0);
        (void)close(pipefd[0]);
        mimir_free(sentinel);
        return;
    }
    (void)close(pipefd[0]);
    (void)kill(pid, SIGKILL);
    if (waitpid(pid, &status, 0) < 0) {
        check(0, "kill case: waitpid succeeded");
        mimir_free(sentinel);
        return;
    }
    check(WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL,
          "worker child killed with SIGKILL while holding a live, "
          "verified block");

    pid = fork();
    if (pid == 0) {
        unsigned char *p = (unsigned char *)mimir_malloc(8192);

        if (p == NULL) {
            _exit(6);
        }
        fill_pattern(p, 8192, 0x78);
        if (!pattern_ok(p, 8192, 0x78)) {
            _exit(7);
        }
        mimir_free(p);
        _exit(0);
    }
    status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0) {
        check(0, "kill case: restart child ran");
    } else {
        check(WIFEXITED(status) && WEXITSTATUS(status) == 0,
              "fresh process after the kill allocates, verifies and "
              "frees normally (restart health; per-process state only)");
    }
    check(pattern_ok(sentinel, 64, 0x22) && recovery_ok(),
          "parent sentinel intact and parent allocator healthy after "
          "the kill");
    mimir_free(sentinel);
}

/* ------------------------------------------------------------------ */
/* S93-5: failure storm + non-overlap + no hang.                      */
/* ------------------------------------------------------------------ */
static void section_storm(void)
{
    struct live_block live[16];
    size_t live_count = 0;
    unsigned round;
    int ok = 1;

    printf("S93-5 failure storm (120 rounds)\n");
    for (round = 0; round < 120; round++) {
        switch (round % 4) {
        case 0:
            if (mimir_malloc(SIZE_MAX) != NULL) {
                ok = 0;
            }
            break;
        case 1:
            if (mimir_calloc(SIZE_MAX, 2) != NULL) {
                ok = 0;
            }
            break;
        case 2: {
            unsigned char *t = (unsigned char *)mimir_malloc(128);

            if (t == NULL) {
                ok = 0;
                break;
            }
            fill_pattern(t, 128, 0x55);
            if (mimir_realloc(t, SIZE_MAX / 2) != NULL) {
                ok = 0;
            }
            if (!pattern_ok(t, 128, 0x55)) {
                ok = 0;
            }
            mimir_free(t);
            break;
        }
        default:
            if (mimir_malloc(SIZE_MAX / 2) != NULL) {
                ok = 0;
            }
            break;
        }
        if (live_count < 16) {
            size_t n = 128 + (round % 512);
            unsigned char *p = (unsigned char *)mimir_malloc(n);

            if (p == NULL) {
                ok = 0;
            } else {
                fill_pattern(p, n, (unsigned)(0x30 + round));
                live[live_count].ptr = p;
                live[live_count].size = n;
                live[live_count].seed = (unsigned)(0x30 + round);
                live_count++;
            }
        }
    }
    check(ok, "120 rounds: every induced failure is exactly null, "
              "every interleaved allocation succeeds, no hang (the "
              "suite completing is the no-hang evidence)");
    check(all_disjoint(live, live_count),
          "storm survivors pairwise disjoint (failures never return "
          "overlapping valid blocks)");
    check(all_patterns_ok(live, live_count),
          "storm survivors' contents intact");
    {
        size_t i;

        for (i = 0; i < live_count; i++) {
            mimir_free(live[i].ptr);
        }
    }
    check(recovery_ok(), "recovery succeeds after the failure storm");
}

int main(void)
{
    long queried;

    printf("C-008 controlled-failure test (roadmap S9.3, B-008 S6)\n");
    queried = sysconf(_SC_PAGESIZE);
    check(queried > 0, "runtime page size discovered");
    if (queried <= 0) {
        printf("CHECKS: %d run, %d failed\n", checks, failures);
        printf("RESULT: FAIL\n");
        return 1;
    }
    printf("runtime system page size: %ld bytes\n", queried);

    section_boundaries((size_t)queried);
    section_rlimit();
    section_fragmentation();
    section_kill_restart();
    section_storm();

    skip("S93-INJ-1", "SKIPPED/NOT-YET: fail-the-Nth-mapping sweep "
                      "needs the fault-injection shim on the L6 "
                      "mapping boundary; none exists in the banked "
                      "base (INC-C007-01, carried as INC-C008-01)");
    skip("S93-INJ-2", "SKIPPED/NOT-YET: metadata-allocation failure "
                      "injection needs the shim; none exists "
                      "(INC-C007-01)");
    skip("S93-INJ-3", "SKIPPED/NOT-YET: EMFILE / EAGAIN-class OS "
                      "failure injection needs the shim; none exists "
                      "(INC-C007-01)");
    skip("S93-ENV-1", "SKIPPED/NOT-YET: Android track of roadmap S9.3 "
                      "cannot run on this Linux Pi host; results are "
                      "tracked separately per target and none is "
                      "claimed (INC-C008-04)");
    skip("S93-ENV-2", "SKIPPED/NOT-YET: proot track of roadmap S9.3 "
                      "cannot run on this host (INC-C008-04)");
    skip("S93-FORK-1", "SKIPPED: BLOCKED-by-U-17..U-21 -- fork-child "
                       "lock state and first allocation in the child "
                       "are open (B-008 S6.3 X-4a/X-4b); no behaviour "
                       "is asserted");
    skip("S93-TEARDOWN-1", "SKIPPED: BLOCKED -- calls after allocator "
                           "teardown / from exit-path code have no "
                           "contract row (B-008 S6.3 X-8, "
                           "INC-B008-8); no claim is made or tested");

    printf("CHECKS: %d run, %d failed, %d skipped-not-yet\n",
           checks, failures, skips);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
