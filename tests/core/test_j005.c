/*
 * test_j005.c -- evidence test for slice J-005 (Phase 10
 * debug-only poisoning + quarantine of freed blocks;
 * slice record: docs/PHASE_10_POISON_QUARANTINE.md;
 * plan: docs/PHASE_10_FOUNDATION_PLAN.md S5.2/S5.3).
 *
 * Run standalone via `make test-j005` (NOT part of
 * `make test`, whose composition is unchanged). The
 * binary links the DEBUG library objects (build-debug/,
 * -DMIMIR_DEBUG=1) only; the production flavour's
 * unchanged behaviour is proven by the production
 * `make test` + nm -D legs of the recorded run.
 *
 * Definitions (written before the run, slice doc S2/S3):
 * poison byte 0xDD over the whole class extent; quarantine
 * capacity exactly 32 blocks, one global FIFO; a block
 * returns to reuse only through verified eviction;
 * bytes_quarantined is the summed class extents of held
 * blocks and D-CACHED (J-002 bytes_cached) must not move
 * when quarantine fills or holds.
 *
 * The abort-shaped proof (eviction of a corrupted block)
 * runs ONLY in a disposable child process; the parent
 * never aborts and proves detection non-destructively
 * through mimir_debug_quarantine_verify in its own child
 * as well, so no inherited state is ever corrupted in
 * the parent. Timings are printed as measurements only,
 * never asserted (timing is never an oracle).
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */

#define _POSIX_C_SOURCE 200809L

#include "mimir_alloc.h"
#include "mimir_quarantine_j005.h"
#include "mimir_stats_j002.h"
#include "mimir_trim_i003.h"
#include "mimir_drain_h001.h"

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
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

static int all_bytes(const unsigned char *p, size_t n,
                     unsigned char v)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (p[i] != v) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    struct mimir_stats snap0, snap1, snap2;

    printf("test_j005: J-005 debug poisoning + quarantine\n");

    /* Baseline: quarantine starts empty and clean. */
    check_u64(mimir_debug_quarantine_count(), 0,
              "quarantine empty at start");
    check(mimir_debug_quarantine_verify() == 0,
          "quarantine verifies clean at start");

    /* Leg A (S5.3): quarantine holds do not inflate
     * D-CACHED and are not released by the drain. Two
     * 500-byte requests land in class 512, one span. */
    printf("Leg A: D-CACHED unmoved by quarantine (S5.3)\n");
    check(mimir_core_stats_snapshot(&snap0) == 0, "snapshot 0");
    {
        void *a = mimir_malloc(500);
        void *b = mimir_malloc(500);

        check(a != NULL && b != NULL && a != b,
              "two class-512 blocks allocated");
        mimir_free(a);
        mimir_free(b);
    }
    check_u64(mimir_debug_quarantine_count(), 2,
              "two blocks held in quarantine");
    check_u64(mimir_debug_quarantined_bytes(), 1024,
              "bytes_quarantined = 2 x 512");
    check(mimir_core_stats_snapshot(&snap1) == 0, "snapshot 1");
    check_u64(snap1.bytes_cached, snap0.bytes_cached,
              "D-CACHED unmoved by quarantined span");
    check_u64(snap1.bytes_mapped,
              snap0.bytes_mapped + snap0.page_size_bytes,
              "quarantine span stays mapped (one page)");
    check_u64(mimir_core_retained_bytes(), 0,
              "Q-RETAINED counts no quarantine-held span");
    check_u64(mimir_core_drain_retained(), 0,
              "drain releases no quarantine-held span");
    check(mimir_core_stats_snapshot(&snap2) == 0, "snapshot 2");
    check_u64(snap2.bytes_mapped, snap1.bytes_mapped,
              "mapped bytes unchanged by drain");

    /* Leg B: poison pattern present after free. */
    printf("Leg B: poison pattern after free\n");
    {
        unsigned char *p = (unsigned char *)mimir_malloc(64);

        check(p != NULL, "class-64 block allocated");
        memset(p, 0x11, 64);
        mimir_free(p);
        check_u64(mimir_debug_quarantine_count(), 3,
                  "third block held in quarantine");
        check(all_bytes(p, 64, 0xDD),
              "all 64 user bytes are poison 0xDD after free");
        check(mimir_debug_quarantine_verify() == 0,
              "on-demand verify: all held blocks clean");
    }

    /* Leg C: the bound is exact. 40 class-224 blocks are
     * freed on top of the 3 held: exactly 32 stay held,
     * the oldest evicted (verified clean) return to reuse. */
    printf("Leg C: quarantine bound exact at 32\n");
    {
        void *blocks[40];
        void *r;
        int i, j, in_evicted_set = 0, over_bound = 0;

        for (i = 0; i < 40; i++) {
            blocks[i] = mimir_malloc(200);
            check(blocks[i] != NULL, "bound-leg allocation");
        }
        for (i = 0; i < 40; i++) {
            mimir_free(blocks[i]);
            if (mimir_debug_quarantine_count() > 32) {
                over_bound = 1;
            }
        }
        check(!over_bound, "quarantine never exceeds 32");
        check_u64(mimir_debug_quarantine_count(), 32,
                  "quarantine at exactly its bound");
        check_u64(mimir_debug_quarantined_bytes(), 32 * 224,
                  "bytes_quarantined = 32 x 224 at bound");
        check(mimir_debug_quarantine_verify() == 0,
              "held blocks verify clean at bound");
        r = mimir_malloc(200);
        check(r != NULL, "reuse allocation succeeds");
        for (j = 0; j < 8; j++) {
            if (r == blocks[j]) {
                in_evicted_set = 1;
            }
        }
        check(in_evicted_set,
              "reuse returns an evicted (verified) block");
        memset(r, 0x5A, 200);
        check(all_bytes((const unsigned char *)r, 200, 0x5A),
              "reused block fully writable and readable");
        mimir_free(r);
        check_u64(mimir_debug_quarantine_count(), 32,
                  "quarantine back at bound after re-free");
        /* Double free of a quarantined block: the banked
         * non-destructive posture -- nothing is retired
         * twice, nothing is counted on suspicion. */
        mimir_free(blocks[20]);
        check_u64(mimir_debug_quarantine_count(), 32,
                  "double free of quarantined block is inert");
        check(mimir_debug_quarantine_verify() == 0,
              "verify still clean after double free");
        check(mimir_core_stats_snapshot(&snap1) == 0,
              "snapshot at quiescence");
        check_u64(snap1.live_small_objects, 0,
                  "no live small objects at quiescence");
    }

    /* Leg D: debug outcomes match production for defined
     * calls (S5.2): calloc zeroes, realloc grow preserves,
     * the large path is untouched by quarantine, posture
     * NULLs stay posture NULLs. */
    printf("Leg D: defined-call outcomes unchanged\n");
    {
        unsigned char *c = (unsigned char *)mimir_calloc(10, 10);
        unsigned char *g;
        void *big;
        size_t before;
        int i, preserved = 1;

        check(c != NULL && all_bytes(c, 100, 0),
              "calloc extent zeroed in debug build");
        for (i = 0; i < 100; i++) {
            c[i] = (unsigned char)(i + 1);
        }
        g = (unsigned char *)mimir_realloc(c, 300);
        check(g != NULL, "realloc grow succeeds");
        for (i = 0; i < 100; i++) {
            if (g[i] != (unsigned char)(i + 1)) {
                preserved = 0;
            }
        }
        check(preserved, "realloc grow preserves min bytes");
        mimir_free(g);
        before = mimir_debug_quarantine_count();
        big = mimir_malloc(4096);
        check(big != NULL, "large allocation succeeds");
        mimir_free(big);
        check_u64(mimir_debug_quarantine_count(), before,
                  "large free never enters quarantine");
        check(mimir_malloc(0) == NULL,
              "zero-size posture NULL in debug build");
    }

    /* Leg E: overhead, measured and reported (never an
     * oracle). Static table: 32 entries x 16 bytes = 512
     * bytes once per process; per-block layout overhead is
     * 0 bytes (header and class extents unchanged); held
     * user bytes at bound for one class are capacity x
     * class extent, read back from bytes_quarantined. */
    printf("Leg E: overhead measurement\n");
    {
        struct timespec t0, t1;
        long long elapsed_ns;
        int i;
        const int iters = 20000;

        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (i = 0; i < iters; i++) {
            void *t = mimir_malloc(64);
            mimir_free(t);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        elapsed_ns = (long long)(t1.tv_sec - t0.tv_sec) *
                         1000000000LL +
                     (long long)(t1.tv_nsec - t0.tv_nsec);
        printf("  [info] debug alloc/free cycle x%d: %lld ns "
               "total, %lld ns/op (measurement only)\n",
               iters, elapsed_ns, elapsed_ns / iters);
        printf("  [info] quarantine capacity 32 blocks; "
               "static table 512 bytes; per-block layout "
               "overhead 0 bytes; held bytes now (32 x "
               "class 64 after the churn leg) %llu\n",
               (unsigned long long)
                   mimir_debug_quarantined_bytes());
        check(elapsed_ns > 0, "timing leg ran");
    }

    /* Leg F: detection proofs, disposable children only.
     * Child A: a use-after-free write into a quarantined
     * block is DETECTED by the on-demand verify (exit 0
     * iff exactly one corrupted block is reported).
     * Child B: forcing eviction of a corrupted block takes
     * the allocator's fail-clearly path -- fd 2 report +
     * abort -- so the child must die of SIGABRT and never
     * reach its clean exit. The parent only observes. */
    printf("Leg F: UAF detection in disposable children\n");
    {
        pid_t pid;
        int status = 0;

        pid = fork();
        if (pid == 0) {
            unsigned char *q =
                (unsigned char *)mimir_malloc(64);
            int bad;

            if (q == NULL) {
                _exit(3);
            }
            mimir_free(q);
            q[10] = 0x99; /* simulated use-after-free write */
            bad = mimir_debug_quarantine_verify();
            _exit(bad == 1 ? 0 : 1);
        }
        check(pid > 0 && waitpid(pid, &status, 0) == pid,
              "child A ran");
        check(WIFEXITED(status) && WEXITSTATUS(status) == 0,
              "child A: UAF write detected by verify (1 bad)");

        status = 0;
        pid = fork();
        if (pid == 0) {
            unsigned char *b =
                (unsigned char *)mimir_malloc(64);
            int i;

            if (b == NULL) {
                _exit(3);
            }
            mimir_free(b);
            b[0] = 0x00; /* simulated use-after-free write */
            for (i = 0; i < 48; i++) {
                void *t = mimir_malloc(64);
                mimir_free(t); /* churn evicts b: must abort */
            }
            _exit(2); /* reached only if detection failed */
        }
        check(pid > 0 && waitpid(pid, &status, 0) == pid,
              "child B ran");
        check(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT,
              "child B: eviction of corrupted block aborts");
        check(mimir_debug_quarantine_verify() == 0,
              "parent quarantine untouched by children");
    }

    printf("J005 SUMMARY: checks=%d failures=%d\n",
           checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
