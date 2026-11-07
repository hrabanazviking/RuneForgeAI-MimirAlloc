/*
 * test_c006.c -- C-006 section 6.3 evidence program:
 * allocation-free startup/init and diagnostics-free error
 * paths (docs/contracts.md Section 4 [Roadmap 6.3] and
 * conformance Section 6.3), audited in
 * tests/core/AUDIT_6_3_C006.md.
 *
 * Slice C-006. Exercises, through the public L1 boundary
 * only (include/mimir_alloc.h), the section 6.3 behaviours
 * that are observable through the public API on this host:
 *
 *   - First entry with no init step (MA-6.3-001 / -005):
 *     this process's first allocator call is
 *     mimir_free(NULL), which returns normally; the first
 *     mimir_malloc then succeeds and its descriptor is
 *     fully formed at publication (B-007 C1.4 read-back, as
 *     in test_b007.c). Two forked children repeat first
 *     entry in a fresh image whose registry is empty: the
 *     child's first-ever call is a mimir_calloc in one and
 *     a mimir_realloc(NULL, n) in the other. The fork is a
 *     harness technique for obtaining a fresh process image
 *     ONLY: it asserts no fork behaviour, and U-17..U-21
 *     remain open exactly as the header states.
 *   - Concurrent first entry (MA-6.3-001 / -004 observable
 *     side): in a forked fresh image, eight threads race to
 *     be the allocator's first users, interleaving induced
 *     failures with successful alloc/free cycles. All
 *     succeed. There is no init step for them to race.
 *   - Diagnostics-free error paths (assignment item (b);
 *     MA-6.3-020..023 bound, at zero bytes): fds 1 and 2 are
 *     redirected to a tmpfile while a battery of induced
 *     failures and successes runs; the captured byte count
 *     must be exactly zero -- no error path (and no success
 *     path) writes anything to stdio.
 *   - No leaked lock after failure (assignment item (c)):
 *     after EVERY induced failure below, a fresh allocation
 *     must succeed, verify, and free -- the observable proof
 *     that the failure return released the global lock and
 *     left the registry healthy. A 60-round mixed storm and
 *     a 500-round failure cascade extend the same check
 *     (MA-6.3-023 observable side: repeated errors do not
 *     accumulate into unavailability).
 *
 * What is deliberately NOT checked, and why (audit):
 *   - that initialisation performs no allocation, that no
 *     error path allocates or logs internally, and that
 *     every failure branch unlocks: none of these can be
 *     triggered or observed through the public API on a
 *     healthy host (no interposition, fault-injection or
 *     counter hooks exist in Phase 3 scope). They rest on
 *     code inspection in AUDIT_6_3_C006.md, labelled
 *     INSPECTED there -- the INC-C004-02 evidence
 *     limitation, carried as INC-C006-03.
 *   - invalid frees / reallocs, double free, use after
 *     free (undefined behaviour; no defined outcome);
 *   - any errno value (not promised -- U-10 / V-02);
 *   - signal-handler and fork behaviour (no claim is made
 *     -- MA-6.3-030..032 / -040..042, U-05, U-17..U-21);
 *   - zero-size outcomes as contract (postures only --
 *     U-09 / U-12; the one zero-size call below is part of
 *     the silence battery, where only its silence and the
 *     subsequent allocator health are asserted).
 *
 * Exit status 0 and a final "RESULT: PASS" line only if
 * every check passed. Prints only facts it observed.
 */

#include "mimir_alloc.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>

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

/* Descriptor read-back helpers (B-006 S7.3 offsets, as in
 * test_b007.c): the descriptor sits one system page before
 * the user pointer. */
#define DESC_OFF_MAGIC           0u
#define DESC_OFF_MAPPING_BASE    8u
#define DESC_OFF_MAPPING_LENGTH 16u
#define DESC_OFF_USABLE_OFFSET  24u
#define DESC_OFF_REQUESTED_SIZE 32u
#define DESC_OFF_USABLE_SIZE    40u
#define DESC_OFF_STATE          48u
#define DESC_OFF_FLAGS          52u
#define DESC_OFF_RESERVED       72u
#define DESC_RESERVED_LEN       24u
#define DESC_MAGIC UINT64_C(0x4D494D4952414C4C)
#define DESC_STATE_LIVE 1u

static uint64_t rd_u64(const unsigned char *d, size_t off)
{
    uint64_t v;
    memcpy(&v, d + off, sizeof v);
    return v;
}

static size_t rd_size(const unsigned char *d, size_t off)
{
    size_t v;
    memcpy(&v, d + off, sizeof v);
    return v;
}

static uint32_t rd_u32(const unsigned char *d, size_t off)
{
    uint32_t v;
    memcpy(&v, d + off, sizeof v);
    return v;
}

static const void *rd_ptr(const unsigned char *d, size_t off)
{
    const void *v;
    memcpy(&v, d + off, sizeof v);
    return v;
}

/* G-003 shape awareness (docs/PHASE_7_SMALL_OBJECT_PATH_G003.md,
 * INC-G003-01): from Phase 7, requests of 1..1024 bytes are
 * served from size-class spans (banked G-002 design), whose
 * serving header sits at the start of the span's one system
 * page -- NOT one page before the user pointer, and the
 * pointer is 16-aligned, not page-aligned (G-002 S9 expressly
 * disowns the mapping form's page alignment as an accident
 * no caller or test may rely on). The descriptor read-backs
 * in this file therefore branch on the serving form:
 * page-aligned user pointers (the mapping path) keep the
 * original B-006 S7.3 descriptor checks at full strength;
 * span-served pointers get the equivalent clean-init
 * verification of the span header (G-002 S7 / G-003 layout:
 * magic "MIMIRSPN", base == header address, mapping_length
 * == one system page, usable_offset == 96, class-size field
 * == the class serving the request, usable_size ==
 * mapping_length - 96, state LIVE, class index in range,
 * live_count >= 1, pointer at the class stride inside the
 * object area). No contract-level check in this file
 * changed: patterns, max_align_t alignment, overflow
 * refusals, health cycles and postures are untouched. */
#define SPAN_MAGIC UINT64_C(0x4D494D495253504E) /* "MIMIRSPN" */
#define SPAN_OFF_CLASS_INDEX    52u
#define SPAN_OFF_LIVE_COUNT     80u

static size_t g003_class_for(size_t n, size_t *idx_out)
{
    static const size_t cs[20] = {
        16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
        224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
    };
    size_t i;

    for (i = 0; i < 20; i++) {
        if (n <= cs[i]) {
            if (idx_out != NULL) {
                *idx_out = i;
            }
            return cs[i];
        }
    }
    if (idx_out != NULL) {
        *idx_out = 20;
    }
    return 0;
}

static const unsigned char *g003_span_header_of(const void *user,
                                                size_t page)
{
    return (const unsigned char *)((uintptr_t)user &
                                   ~(uintptr_t)(page - 1));
}

static int g003_span_clean(const unsigned char *h,
                           const void *user, size_t page,
                           size_t requested)
{
    size_t idx = 20;
    size_t cs = g003_class_for(requested, &idx);
    size_t off;

    if (cs == 0) {
        return 0;
    }
    off = (size_t)((const unsigned char *)user - h);
    return rd_u64(h, DESC_OFF_MAGIC) == SPAN_MAGIC &&
           rd_ptr(h, DESC_OFF_MAPPING_BASE) == (const void *)h &&
           rd_size(h, DESC_OFF_MAPPING_LENGTH) == page &&
           rd_size(h, DESC_OFF_USABLE_OFFSET) == 96 &&
           rd_size(h, DESC_OFF_REQUESTED_SIZE) == cs &&
           rd_size(h, DESC_OFF_USABLE_SIZE) == page - 96 &&
           rd_u32(h, DESC_OFF_STATE) == DESC_STATE_LIVE &&
           rd_u32(h, SPAN_OFF_CLASS_INDEX) == (uint32_t)idx &&
           rd_size(h, SPAN_OFF_LIVE_COUNT) >= 1 &&
           off >= 96 && (off - 96) % cs == 0;
}


static void fill_pattern(unsigned char *p, size_t n,
                         unsigned int seed)
{
    size_t i;

    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)((i * 31u + seed * 17u +
                                (i >> 8)) & 0xffu);
    }
}

static int pattern_ok(const unsigned char *p, size_t n,
                      unsigned int seed)
{
    size_t i;

    for (i = 0; i < n; i++) {
        unsigned char want =
            (unsigned char)((i * 31u + seed * 17u +
                             (i >> 8)) & 0xffu);
        if (p[i] != want) {
            return 0;
        }
    }
    return 1;
}

/* One alloc/write/free cycle: the observable health proof
 * used after every induced failure. Returns 1 on success. */
static int health_cycle(unsigned int seed)
{
    unsigned char *p = (unsigned char *)mimir_malloc(128);

    if (p == NULL) {
        return 0;
    }
    fill_pattern(p, 128, seed);
    if (!pattern_ok(p, 128, seed)) {
        mimir_free(p);
        return 0;
    }
    mimir_free(p);
    return 1;
}

/* Thread worker for the concurrent-first-entry race: mixes
 * successful cycles with induced failures; any failure to
 * allocate a satisfiable request, any unexpected success of
 * an unsatisfiable one, or any pattern mismatch is counted. */
static int thread_errors = 0;

static void *race_worker(void *arg)
{
    long id = (long)arg;
    int i;

    for (i = 0; i < 150; i++) {
        unsigned char *p;
        size_t n = 32u + (size_t)((i * 37 + (int)id * 11) % 512);

        if (i % 10 == 9) {
            if (mimir_malloc(SIZE_MAX) != NULL) {
                __sync_fetch_and_add(&thread_errors, 1);
            }
            continue;
        }
        p = (unsigned char *)mimir_malloc(n);
        if (p == NULL) {
            __sync_fetch_and_add(&thread_errors, 1);
            continue;
        }
        fill_pattern(p, n, (unsigned int)(id + 1));
        if (!pattern_ok(p, n, (unsigned int)(id + 1))) {
            __sync_fetch_and_add(&thread_errors, 1);
        }
        mimir_free(p);
    }
    return NULL;
}

int main(void)
{
    long queried = sysconf(_SC_PAGESIZE);
    size_t page = queried > 0 ? (size_t)queried : 0;

    printf("C-006 section 6.3 startup / error-path test\n");
    printf("runtime system page size: %zu bytes\n", page);
    check(page > 0, "runtime page size discovered");

    /* --- First entry, no init step (MA-6.3-001 / -005) ---
     * The next call is the first allocator call in this
     * process. No init function exists in the header and
     * none was called: static zero state plus the
     * statically initialised L6 lock are the whole of
     * "initialisation" in this design (audit Section A). */
    mimir_free(NULL);
    check(1, "MA-6.3-001: first call in process is "
             "mimir_free(NULL), returns normally, no init step");
    {
        unsigned char *p = (unsigned char *)mimir_malloc(48);

        check(p != NULL,
              "MA-6.3-001/-005: first mimir_malloc succeeds "
              "with no prior initialisation");
        if (p != NULL) {
            int clean;

            if ((uintptr_t)p % page == 0) {
                const unsigned char *d =
                    (const unsigned char *)p - page;
                size_t i;

                clean = 1;
                if (rd_u64(d, DESC_OFF_MAGIC) != DESC_MAGIC ||
                    rd_ptr(d, DESC_OFF_MAPPING_BASE) !=
                        (const void *)d ||
                    rd_size(d, DESC_OFF_USABLE_OFFSET) != page ||
                    rd_size(d, DESC_OFF_REQUESTED_SIZE) != 48 ||
                    rd_size(d, DESC_OFF_USABLE_SIZE) !=
                        rd_size(d, DESC_OFF_MAPPING_LENGTH) -
                            page ||
                    rd_u32(d, DESC_OFF_STATE) != DESC_STATE_LIVE ||
                    rd_u32(d, DESC_OFF_FLAGS) != 0) {
                    clean = 0;
                }
                for (i = 0; i < DESC_RESERVED_LEN; i++) {
                    if (d[DESC_OFF_RESERVED + i] != 0) {
                        clean = 0;
                    }
                }
            } else {
                /* G-003 (INC-G003-01): 48 bytes is span-served
                 * from Phase 7; the fully-formed-before-
                 * publication duty is verified on the span
                 * header that actually serves the call. */
                clean = g003_span_clean(
                    g003_span_header_of(p, page), p, page, 48);
            }
            check(clean,
                  "B-007 C1.4: first serving header fully formed "
                  "before publication (descriptor or span form: "
                  "magic, base, sizes, LIVE)");
            mimir_free(p);
        }
    }

    /* --- Fresh-image first entry via fork (harness only;
     * no fork behaviour is asserted -- U-17..U-21 open) -- */
    {
        pid_t pid = fork();

        if (pid == 0) {
            /* Child: registry empty (parent holds no live
             * blocks here); first-ever call is calloc. */
            unsigned char *p =
                (unsigned char *)mimir_calloc(6, 7);
            int ok = p != NULL;
            size_t i;

            if (ok) {
                for (i = 0; i < 42; i++) {
                    if (p[i] != 0) {
                        ok = 0;
                    }
                }
                mimir_free(p);
                ok = ok && health_cycle(4);
            }
            _exit(ok ? 0 : 1);
        }
        if (pid > 0) {
            int status = 0;

            check(waitpid(pid, &status, 0) == pid &&
                  WIFEXITED(status) && WEXITSTATUS(status) == 0,
                  "fresh image: first call mimir_calloc "
                  "succeeds, zeroed, frees, health cycle ok");
        } else {
            check(0, "fresh image: fork for calloc child");
        }
    }
    {
        pid_t pid = fork();

        if (pid == 0) {
            unsigned char *p = (unsigned char *)
                mimir_realloc(NULL, 64);
            int ok = p != NULL;

            if (ok) {
                fill_pattern(p, 64, 8);
                ok = pattern_ok(p, 64, 8);
                mimir_free(p);
            }
            _exit(ok ? 0 : 1);
        }
        if (pid > 0) {
            int status = 0;

            check(waitpid(pid, &status, 0) == pid &&
                  WIFEXITED(status) && WEXITSTATUS(status) == 0,
                  "fresh image: first call mimir_realloc(NULL, "
                  "64) behaves as malloc and frees");
        } else {
            check(0, "fresh image: fork for realloc child");
        }
    }

    /* --- Concurrent first entry in a fresh image -------- */
    {
        pid_t pid = fork();

        if (pid == 0) {
            pthread_t th[8];
            long i;
            int created = 0;

            thread_errors = 0;
            for (i = 0; i < 8; i++) {
                if (pthread_create(&th[i], NULL, race_worker,
                                   (void *)i) == 0) {
                    created++;
                }
            }
            for (i = 0; i < created; i++) {
                pthread_join(th[i], NULL);
            }
            _exit(created == 8 && thread_errors == 0 &&
                  health_cycle(9) ? 0 : 1);
        }
        if (pid > 0) {
            int status = 0;

            check(waitpid(pid, &status, 0) == pid &&
                  WIFEXITED(status) && WEXITSTATUS(status) == 0,
                  "MA-6.3-001/-004: 8 threads race first entry "
                  "in a fresh image, 150 cycles each with "
                  "induced failures, no deadlock, no error");
        } else {
            check(0, "concurrent first entry: fork");
        }
    }

    /* --- Diagnostics-free paths: silence on fds 1/2 ----- */
    {
        unsigned char *live = (unsigned char *)mimir_malloc(96);
        FILE *cap;
        int saved_out = -1, saved_err = -1;
        long captured = -1;
        int battery_ok = 1;

        check(live != NULL, "silence battery: live block ready");
        if (live != NULL) {
            fill_pattern(live, 96, 21);
        }
        cap = tmpfile();
        check(cap != NULL, "silence battery: tmpfile ready");
        if (live != NULL && cap != NULL) {
            fflush(NULL);
            saved_out = dup(STDOUT_FILENO);
            saved_err = dup(STDERR_FILENO);
            if (saved_out >= 0 && saved_err >= 0 &&
                dup2(fileno(cap), STDOUT_FILENO) >= 0 &&
                dup2(fileno(cap), STDERR_FILENO) >= 0) {
                /* No printf (or any stdio) by this program
                 * inside the redirected window. */
                void *r;

                r = mimir_malloc(SIZE_MAX);
                if (r != NULL) battery_ok = 0;
                r = mimir_malloc(SIZE_MAX / 2);
                if (r != NULL) battery_ok = 0;
                r = mimir_calloc(SIZE_MAX, 2);
                if (r != NULL) battery_ok = 0;
                r = mimir_calloc(1, SIZE_MAX);
                if (r != NULL) battery_ok = 0;
                r = mimir_realloc(live, SIZE_MAX);
                if (r != NULL) battery_ok = 0;
                r = mimir_realloc(live, SIZE_MAX / 2);
                if (r != NULL) battery_ok = 0;
                r = mimir_malloc(0); /* posture call: only its
                                      * silence and the health
                                      * after it are asserted */
                (void)r;
                {
                    void *t = mimir_malloc(64);

                    if (t == NULL) battery_ok = 0;
                    else mimir_free(t);
                }
                mimir_free(NULL);
                fflush(NULL);
                if (dup2(saved_out, STDOUT_FILENO) < 0 ||
                    dup2(saved_err, STDERR_FILENO) < 0) {
                    battery_ok = 0;
                }
                if (fseek(cap, 0L, SEEK_END) == 0) {
                    captured = ftell(cap);
                }
            } else {
                battery_ok = 0;
            }
            if (saved_out >= 0) close(saved_out);
            if (saved_err >= 0) close(saved_err);
            check(battery_ok,
                  "silence battery: failure and success calls "
                  "behave as designed inside the window");
            check(captured == 0,
                  "MA-6.3-020..023: zero bytes written to "
                  "stdout/stderr across all failure and "
                  "success paths in the battery");
            check(pattern_ok(live, 96, 21),
                  "silence battery: live block contents intact "
                  "after failed reallocs inside the window");
            mimir_free(live);
        } else if (live != NULL) {
            mimir_free(live);
        }
        if (cap != NULL) {
            fclose(cap);
        }
    }

    /* --- Lock-leak battery: health after every failure -- */
    {
        unsigned char *orig = (unsigned char *)mimir_malloc(200);
        int ok = orig != NULL;

        check(ok, "lock battery: original block allocated");
        if (orig != NULL) {
            fill_pattern(orig, 200, 33);
        }
        check(mimir_malloc(SIZE_MAX) == NULL && health_cycle(1),
              "after malloc(SIZE_MAX) NULL: allocation succeeds "
              "(no leaked lock)");
        check(mimir_malloc(SIZE_MAX - 8) == NULL &&
              health_cycle(2),
              "after malloc(SIZE_MAX-8) NULL: allocation "
              "succeeds");
        check(mimir_malloc(SIZE_MAX / 2) == NULL &&
              health_cycle(3),
              "after malloc(SIZE_MAX/2) NULL (unsatisfiable): "
              "allocation succeeds");
        check(mimir_calloc(SIZE_MAX, 2) == NULL &&
              health_cycle(4),
              "after calloc overflow NULL: allocation succeeds");
        check(mimir_calloc(2, SIZE_MAX) == NULL &&
              health_cycle(5),
              "after calloc overflow (swapped) NULL: allocation "
              "succeeds");
        check(mimir_calloc(1, SIZE_MAX) == NULL &&
              health_cycle(6),
              "after calloc unsatisfiable NULL: allocation "
              "succeeds");
        if (orig != NULL) {
            check(mimir_realloc(orig, SIZE_MAX) == NULL &&
                  pattern_ok(orig, 200, 33) && health_cycle(7),
                  "after realloc grow SIZE_MAX NULL: original "
                  "intact, allocation succeeds");
            check(mimir_realloc(orig, SIZE_MAX / 2) == NULL &&
                  pattern_ok(orig, 200, 33) && health_cycle(8),
                  "after realloc grow SIZE_MAX/2 NULL: original "
                  "intact, allocation succeeds");
            mimir_free(orig);
        }
        {
            int storm_ok = 1;
            int i;

            for (i = 0; i < 60; i++) {
                void *r = (i % 3 == 0)
                    ? mimir_malloc(SIZE_MAX)
                    : (i % 3 == 1)
                        ? mimir_calloc(SIZE_MAX, 2)
                        : mimir_malloc(SIZE_MAX / 2);

                if (r != NULL || !health_cycle(10)) {
                    storm_ok = 0;
                    break;
                }
            }
            check(storm_ok,
                  "60-round mixed failure/success storm: every "
                  "failure NULL, every health cycle succeeds");
        }
        {
            int cascade_ok = 1;
            int i;

            for (i = 0; i < 500; i++) {
                if (mimir_malloc(SIZE_MAX) != NULL) {
                    cascade_ok = 0;
                    break;
                }
            }
            check(cascade_ok && health_cycle(12),
                  "MA-6.3-023: 500 consecutive failures stay "
                  "NULL and the allocator is healthy after "
                  "(no accumulation, no leaked lock)");
        }
    }

    printf("CHECKS: %d run, %d failed\n", checks, failures);
    printf("RESULT: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
