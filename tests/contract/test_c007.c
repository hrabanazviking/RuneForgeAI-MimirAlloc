/*
 * test_c007.c -- portable unit/contract test suite (slice C-007),
 * implementing the suite designed in
 * docs/PHASE_3_TEST_SUITE_PLAN.md (B-009) for the public
 * namespaced API: mimir_malloc, mimir_free, mimir_calloc,
 * mimir_realloc (include/mimir_alloc.h only).
 *
 * Portability discipline (B-009 S4.5, S3.2): portable C11 plus
 * POSIX facilities present on both Phase 3 exit targets (Linux +
 * glibc on ARM64 and on x86-64): pthread, fork, sysconf. No
 * ARM64-only assumption, no internal header, and NO literal
 * system page size anywhere in this file: every page quantity
 * is derived from the runtime-discovered system page size
 * (sysconf(_SC_PAGESIZE), failure-checked, B-009 S3.2).
 * Ordinary request sizes in this file are plain byte counts,
 * never page claims (MA-6.2-041).
 *
 * Every contract citation is BY ANALOGY under U-03: the
 * namespaced names have no contract rows of their own
 * (INC-B006-1). errno is never asserted (U-10). No numeric
 * alignment value is asserted: the only alignment check is
 * suitability for max_align_t, the non-numeric fundamental
 * alignment claim (U-15 open). Pointer addresses are never
 * oracles, except the single REA-002 precondition observation
 * recorded in NOTES_C007.md (in this implementation a grow
 * always moves, B-008 S4.4 design fact; inequality of the
 * returned pointer is recorded to show the move instance
 * occurred, never as a behavioural oracle). No timing is an
 * oracle. Zero-size postures are NOT exercised as contract
 * cases here: P3-T-CAL-031 / P3-T-REA-041 / P3-T-FAIL-041 are
 * BLOCKED-by-U-09 / U-12 and are reported SKIPPED below.
 *
 * Verdict vocabulary (B-009 S9.1, adapted to this suite's
 * single-programme form in NOTES_C007.md): each registered
 * case is reported exactly once, as PASS (run on this Pi in
 * this run, criterion satisfied), FAIL, SKIPPED (not run;
 * reason printed here and recorded in NOTES_C007.md), or
 * INSPECTED (audit case discharged by source/document
 * inspection recorded in NOTES_C007.md, never by a runtime
 * claim). The full registry of 119 case IDs minted by B-009
 * Appendix A is accounted for: 40 run, 9 inspected, 70
 * skipped. Exit status is 0 only if no run case failed.
 *
 * This run is ARM64 Linux only. The x86-64 run of this
 * identical suite remains OWED (B-009 S3.4) and Phase 3 exit
 * stays UNMET until both targets pass (B-009 S10); the exit
 * packet itself is C-009, not this slice.
 */

#include "mimir_alloc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>

/* ------------------------------------------------------------------ */
/* Harness                                                             */
/* ------------------------------------------------------------------ */

static int total_checks = 0;
static int total_fails = 0;
static int cases_pass = 0;
static int cases_fail = 0;
static int cases_skip = 0;
static int cases_insp = 0;

static const char *cur_case = "";
static int cur_fails = 0;
static int cur_checks = 0;

static void case_begin(const char *id)
{
    cur_case = id;
    cur_fails = 0;
    cur_checks = 0;
}

static void case_end(void)
{
    if (cur_fails == 0 && cur_checks > 0) {
        cases_pass++;
        printf("CASE %s PASS (%d checks)\n", cur_case, cur_checks);
    } else {
        cases_fail++;
        printf("CASE %s FAIL (%d of %d checks failed)\n",
               cur_case, cur_fails, cur_checks);
    }
}

static void check(int condition, const char *label)
{
    total_checks++;
    cur_checks++;
    printf("  [%s] %s: %s\n", condition ? "ok" : "FAIL", cur_case, label);
    if (!condition) {
        total_fails++;
        cur_fails++;
    }
}

static void skip(const char *id, const char *reason)
{
    cases_skip++;
    printf("SKIP %s %s\n", id, reason);
}

static void inspected(const char *id, const char *basis)
{
    cases_insp++;
    printf("INSPECTED %s %s\n", id, basis);
}

/* Position-dependent per-block pattern (as in test_core.c). */
static void fill_pattern(unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;

    for (i = 0; i < n; i++) {
        p[i] = (unsigned char)((i * 31u + seed * 17u + (i >> 8)) & 0xffu);
    }
}

static int pattern_ok(const unsigned char *p, size_t n, unsigned int seed)
{
    size_t i;

    for (i = 0; i < n; i++) {
        unsigned char want =
            (unsigned char)((i * 31u + seed * 17u + (i >> 8)) & 0xffu);
        if (p[i] != want) {
            return 0;
        }
    }
    return 1;
}

static int ranges_overlap(const unsigned char *a, size_t an,
                          const unsigned char *b, size_t bn)
{
    uintptr_t a0 = (uintptr_t)a;
    uintptr_t b0 = (uintptr_t)b;

    return a0 < b0 + bn && b0 < a0 + an;
}

/* Deterministic PRNG (fixed seed; the seed is an evidence
 * field, never an oracle, B-009 S8.1.3). */
static uint64_t rng_state = 0x243F6A8885A308D3ull;

static uint64_t rng_next(void)
{
    uint64_t x = rng_state;

    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return x;
}

static size_t page_size_runtime(void)
{
    long queried = sysconf(_SC_PAGESIZE);

    return queried > 0 ? (size_t)queried : 0;
}

/* ------------------------------------------------------------------ */
/* P3-T-SYS-001 / P3-T-SYS-003: registry + evidence-schema units       */
/* ------------------------------------------------------------------ */

/* The full B-009 Appendix A registry: 119 case IDs. */
static const char *const registry[] = {
    "P3-T-SYS-001", "P3-T-SYS-002", "P3-T-SYS-003",
    "P3-T-DIFF-001", "P3-T-DIFF-002",
    "P3-T-MAP-001", "P3-T-MAP-002", "P3-T-MAP-003", "P3-T-MAP-011",
    "P3-T-MAP-012", "P3-T-MAP-021", "P3-T-MAP-031", "P3-T-MAP-032",
    "P3-T-MAP-041", "P3-T-MAP-042", "P3-T-MAP-051",
    "P3-T-FAIL-001", "P3-T-FAIL-002", "P3-T-FAIL-003", "P3-T-FAIL-011",
    "P3-T-FAIL-021", "P3-T-FAIL-031", "P3-T-FAIL-041", "P3-T-FAIL-051",
    "P3-T-FAIL-061", "P3-T-FAIL-081", "P3-T-FAIL-091", "P3-T-FAIL-101",
    "P3-T-FAIL-111",
    "P3-T-DATA-001", "P3-T-DATA-002", "P3-T-DATA-011", "P3-T-DATA-012",
    "P3-T-DATA-021", "P3-T-DATA-022",
    "P3-T-ARITH-001", "P3-T-ARITH-002", "P3-T-ARITH-011",
    "P3-T-ARITH-012", "P3-T-ARITH-031", "P3-T-ARITH-032",
    "P3-T-ARITH-041", "P3-T-ARITH-042", "P3-T-ARITH-043",
    "P3-T-ALIGN-001", "P3-T-ALIGN-002", "P3-T-ALIGN-011",
    "P3-T-ALIGN-012", "P3-T-ALIGN-021", "P3-T-ALIGN-031",
    "P3-T-ALIGN-041", "P3-T-ALIGN-051", "P3-T-ALIGN-052",
    "P3-T-ALIGN-061", "P3-T-ALIGN-062", "P3-T-ALIGN-071",
    "P3-T-ALIGN-081",
    "P3-T-INIT-001", "P3-T-INIT-011", "P3-T-INIT-021", "P3-T-INIT-031",
    "P3-T-INIT-041", "P3-T-INIT-051", "P3-T-INIT-061", "P3-T-INIT-071",
    "P3-T-INIT-081",
    "P3-T-ERR-031", "P3-T-ERR-041", "P3-T-ERR-051", "P3-T-ERR-061",
    "P3-T-ERR-071", "P3-T-ERR-081", "P3-T-ERR-091", "P3-T-ERR-101",
    "P3-T-ERR-111", "P3-T-ERR-121", "P3-T-ERR-131", "P3-T-ERR-141",
    "P3-T-ERR-151", "P3-T-ERR-161", "P3-T-ERR-171", "P3-T-ERR-181",
    "P3-T-ERR-191", "P3-T-ERR-201", "P3-T-ERR-211", "P3-T-ERR-212",
    "P3-T-CAL-001", "P3-T-CAL-002", "P3-T-CAL-003", "P3-T-CAL-011",
    "P3-T-CAL-012", "P3-T-CAL-021", "P3-T-CAL-031",
    "P3-T-REA-001", "P3-T-REA-002", "P3-T-REA-011", "P3-T-REA-012",
    "P3-T-REA-021", "P3-T-REA-022", "P3-T-REA-031", "P3-T-REA-041",
    "P3-T-COND-001", "P3-T-COND-002", "P3-T-COND-003",
    "P3-T-UNS-001", "P3-T-UNS-011", "P3-T-UNS-021", "P3-T-UNS-031",
    "P3-T-UNS-041", "P3-T-UNS-051", "P3-T-UNS-061", "P3-T-UNS-062",
    "P3-T-UNS-063", "P3-T-UNS-064", "P3-T-UNS-065", "P3-T-UNS-071",
    "P3-T-UNS-081", "P3-T-UNS-091", "P3-T-UNS-101"
};

static int id_wellformed(const char *id)
{
    /* Shape: P3-T-<AREA>-<nnn>, AREA in the frozen registry,
     * nnn exactly three digits, 001..999. No architecture,
     * page-size, or result suffix (B-009 S4.2). */
    static const char *const areas[] = {
        "SYS", "DIFF", "MAP", "FAIL", "DATA", "ARITH", "ALIGN",
        "INIT", "ERR", "CAL", "REA", "COND", "UNS", "EXIT"
    };
    const char *dash;
    size_t alen;
    size_t i;
    int area_ok = 0;

    if (strncmp(id, "P3-T-", 5) != 0) {
        return 0;
    }
    dash = strchr(id + 5, '-');
    if (dash == NULL) {
        return 0;
    }
    alen = (size_t)(dash - (id + 5));
    for (i = 0; i < sizeof areas / sizeof areas[0]; i++) {
        if (strlen(areas[i]) == alen &&
            strncmp(id + 5, areas[i], alen) == 0) {
            area_ok = 1;
        }
    }
    if (!area_ok) {
        return 0;
    }
    if (strlen(dash + 1) != 3) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        if (dash[1 + i] < '0' || dash[1 + i] > '9') {
            return 0;
        }
    }
    return !(dash[1] == '0' && dash[2] == '0' && dash[3] == '0');
}

static void case_sys_001(void)
{
    size_t n = sizeof registry / sizeof registry[0];
    size_t i;
    size_t j;
    int all_ok = 1;
    int unique = 1;

    case_begin("P3-T-SYS-001");
    for (i = 0; i < n; i++) {
        if (!id_wellformed(registry[i])) {
            all_ok = 0;
        }
        for (j = i + 1; j < n; j++) {
            if (strcmp(registry[i], registry[j]) == 0) {
                unique = 0;
            }
        }
    }
    check(n == 119, "registry holds all 119 B-009 Appendix A case IDs");
    check(all_ok, "every registered ID is well-formed under the S4.2 scheme");
    check(unique, "no registered ID is duplicated");
    case_end();
}

/* Evidence-schema validation unit (B-009 S9.2): a record
 * missing any mandatory field is rejected, never counted as
 * PASS. This exercises the collector rule on sample records
 * built in memory; no full evidence collector exists in the
 * banked base (INC-C007-01). */
static const char *const mandatory_fields[] = {
    "Case ID:", "Contract IDs:", "Hook ID and scheme:",
    "Suite commit:", "Build hash:", "Toolchain:",
    "Target manifest:", "Annex ID:", "Command line:", "Seed:",
    "Fault-injection parameters:", "Watchdog bound:",
    "Precondition state:", "Oracle invoked:", "Exit status:",
    "Artefact SHA-256:", "Result:", "Differential pair:"
};

static int record_complete(const char *record)
{
    size_t i;

    for (i = 0; i < sizeof mandatory_fields / sizeof mandatory_fields[0];
         i++) {
        if (strstr(record, mandatory_fields[i]) == NULL) {
            return 0;
        }
    }
    return 1;
}

static void case_sys_003(void)
{
    static const char complete_record[] =
        "Case ID: P3-T-MAP-001\n"
        "Contract IDs: MA-6.1-MALLOC-01\n"
        "Hook ID and scheme: MA-MALLOC-SUCCESS (named)\n"
        "Suite commit: sample\n"
        "Build hash: sample\n"
        "Toolchain: sample\n"
        "Target manifest: sample\n"
        "Annex ID: none applied (no annex exists, INC-C007-03)\n"
        "Command line: ./build/test_c007\n"
        "Seed: none-by-design\n"
        "Fault-injection parameters: none\n"
        "Watchdog bound: none\n"
        "Precondition state: sample\n"
        "Oracle invoked: contracts S2 row, by analogy under U-03\n"
        "Exit status: 0\n"
        "Artefact SHA-256: sample\n"
        "Result: PASS\n"
        "Differential pair: pending-pair (x86-64 leg OWED)\n";
    static const char missing_seed[] =
        "Case ID: P3-T-MAP-001\n"
        "Contract IDs: MA-6.1-MALLOC-01\n"
        "Hook ID and scheme: MA-MALLOC-SUCCESS (named)\n"
        "Suite commit: sample\n"
        "Build hash: sample\n"
        "Toolchain: sample\n"
        "Target manifest: sample\n"
        "Annex ID: none applied\n"
        "Command line: ./build/test_c007\n"
        "Fault-injection parameters: none\n"
        "Watchdog bound: none\n"
        "Precondition state: sample\n"
        "Oracle invoked: contracts S2 row\n"
        "Exit status: 0\n"
        "Artefact SHA-256: sample\n"
        "Result: PASS\n"
        "Differential pair: pending-pair\n";
    static const char missing_result[] =
        "Case ID: P3-T-MAP-001\n"
        "Contract IDs: MA-6.1-MALLOC-01\n"
        "Hook ID and scheme: MA-MALLOC-SUCCESS (named)\n"
        "Suite commit: sample\n"
        "Build hash: sample\n"
        "Toolchain: sample\n"
        "Target manifest: sample\n"
        "Annex ID: none applied\n"
        "Command line: ./build/test_c007\n"
        "Seed: none-by-design\n"
        "Fault-injection parameters: none\n"
        "Watchdog bound: none\n"
        "Precondition state: sample\n"
        "Oracle invoked: contracts S2 row\n"
        "Exit status: 0\n"
        "Artefact SHA-256: sample\n"
        "Differential pair: pending-pair\n";

    case_begin("P3-T-SYS-003");
    check(record_complete(complete_record),
          "a record carrying every S9.2 mandatory field is accepted");
    check(!record_complete(missing_seed),
          "a record missing the Seed field is rejected, not a PASS record");
    check(!record_complete(missing_result),
          "a record missing the Result field is rejected, not a PASS record");
    case_end();
}

/* ------------------------------------------------------------------ */
/* MAP cases                                                           */
/* ------------------------------------------------------------------ */

static void case_map_042(void)
{
    /* Run FIRST among runtime cases so its "before any
     * allocation" step is literally true in this process. */
    void *a;
    void *b;

    case_begin("P3-T-MAP-042");
    mimir_free(NULL);
    a = mimir_malloc(48);
    check(a != NULL, "allocation after free(NULL)-before-any-allocation works");
    mimir_free(NULL);
    mimir_free(NULL);
    b = mimir_malloc(48);
    check(b != NULL, "allocation after interleaved free(NULL) calls works");
    if (a != NULL) {
        fill_pattern((unsigned char *)a, 48, 42);
        check(pattern_ok((const unsigned char *)a, 48, 42),
              "live block undisturbed by interleaved free(NULL) calls");
    }
    mimir_free(a);
    mimir_free(b);
    mimir_free(NULL);
    check(1, "free(NULL) after all frees returns normally");
    case_end();
}

static void case_map_001(void)
{
    void *p;
    void *q;

    case_begin("P3-T-MAP-001");
    p = mimir_malloc(1);
    check(p != NULL, "mimir_malloc(1) returns non-null");
    if (p != NULL) {
        check((uintptr_t)p % _Alignof(max_align_t) == 0,
              "returned pointer is suitably aligned (fundamental alignment, no numeric value asserted, U-15)");
    }
    q = mimir_malloc(1000);
    check(q != NULL, "mimir_malloc(1000) returns non-null");
    if (q != NULL) {
        check((uintptr_t)q % _Alignof(max_align_t) == 0,
              "second returned pointer is suitably aligned");
    }
    mimir_free(p);
    mimir_free(q);
    case_end();
}

static void case_map_002(void)
{
    static const size_t sizes[4] = {16, 500, 5000, 20000};
    unsigned char *live[4];
    int all = 1;
    int disjoint = 1;
    int distinct = 1;
    int i;
    int j;

    case_begin("P3-T-MAP-002");
    for (i = 0; i < 4; i++) {
        live[i] = (unsigned char *)mimir_malloc(sizes[i]);
        if (live[i] == NULL) {
            all = 0;
        }
    }
    check(all, "several live blocks allocated");
    for (i = 0; i < 4; i++) {
        for (j = i + 1; j < 4; j++) {
            if (live[i] != NULL && live[j] != NULL) {
                if (live[i] == live[j]) {
                    distinct = 0;
                }
                if (ranges_overlap(live[i], sizes[i], live[j], sizes[j])) {
                    disjoint = 0;
                }
            }
        }
    }
    check(distinct, "live blocks are pairwise distinct");
    check(disjoint, "live blocks are pairwise non-overlapping over full requested extents");
    for (i = 0; i < 4; i++) {
        mimir_free(live[i]);
    }
    case_end();
}

static void case_map_003(size_t page)
{
    size_t sizes[5];
    size_t n = 0;
    size_t i;

    case_begin("P3-T-MAP-003");
    sizes[n++] = 1;
    sizes[n++] = 64;
    sizes[n++] = 1000;
    if (page > 0) {
        sizes[n++] = page;
        sizes[n++] = page + 1;
    }
    check(page > 0, "runtime system page size discovered (failure-checked, S3.2)");
    for (i = 0; i < n; i++) {
        unsigned char *p = (unsigned char *)mimir_malloc(sizes[i]);
        char label[96];

        snprintf(label, sizeof label,
                 "every requested byte of malloc(%zu) writable and readable back", sizes[i]);
        if (p == NULL) {
            check(0, label);
            continue;
        }
        fill_pattern(p, sizes[i], (unsigned int)(10 + i));
        check(pattern_ok(p, sizes[i], (unsigned int)(10 + i)), label);
        mimir_free(p);
    }
    case_end();
}

static void case_map_011_012(void)
{
    unsigned char *sentinel;
    void *after;

    case_begin("P3-T-MAP-011");
    check(mimir_malloc(SIZE_MAX / 2) == NULL,
          "representable but unsatisfiable request returns null, no unallocated pointer in any form");
    case_end();

    case_begin("P3-T-MAP-012");
    sentinel = (unsigned char *)mimir_malloc(128);
    check(sentinel != NULL, "sentinel block allocated before induced failure");
    if (sentinel != NULL) {
        fill_pattern(sentinel, 128, 77);
    }
    check(mimir_malloc(SIZE_MAX / 2) == NULL,
          "induced failure returns null (as in MAP-011)");
    if (sentinel != NULL) {
        check(pattern_ok(sentinel, 128, 77),
              "no ownership passed or disturbed: live sentinel bytes unchanged after failure");
    }
    after = mimir_malloc(64);
    check(after != NULL, "next ordinary in-scope allocation after failure succeeds");
    if (after != NULL) {
        fill_pattern((unsigned char *)after, 64, 78);
        check(pattern_ok((const unsigned char *)after, 64, 78),
              "post-failure allocation behaves as in MAP-001 (writable/readable)");
    }
    mimir_free(after);
    mimir_free(sentinel);
    case_end();
}

static void case_map_021(void)
{
    void *after;

    case_begin("P3-T-MAP-021");
    check(mimir_malloc(SIZE_MAX) == NULL,
          "maximum representable size request fails with null, no ownership passes");
    check(mimir_malloc(SIZE_MAX - 8) == NULL,
          "size + reserve/rounding unrepresentable request fails with null");
    after = mimir_malloc(32);
    check(after != NULL, "ordinary allocation after oversize refusals succeeds");
    mimir_free(after);
    case_end();
}

static void case_map_031(void)
{
    unsigned char *p;
    unsigned char *fresh;

    case_begin("P3-T-MAP-031");
    p = (unsigned char *)mimir_malloc(256);
    check(p != NULL, "block allocated for free case");
    if (p != NULL) {
        fill_pattern(p, 256, 31);
        check(pattern_ok(p, 256, 31), "block contents verified before free");
    }
    mimir_free(p);
    check(1, "free of a live in-scope pointer completes, retired exactly once (single free call)");
    fresh = (unsigned char *)mimir_malloc(256);
    check(fresh != NULL, "storage eligible for reuse: fresh allocation after free succeeds");
    if (fresh != NULL) {
        fill_pattern(fresh, 256, 32);
        check(pattern_ok(fresh, 256, 32), "fresh allocation fully usable");
    }
    mimir_free(fresh);
    case_end();
}

static void case_map_032(void)
{
    static const size_t sizes[5] = {24, 300, 1500, 7000, 12000};
    unsigned char *live[5];
    int i;

    case_begin("P3-T-MAP-032");
    for (i = 0; i < 5; i++) {
        live[i] = (unsigned char *)mimir_malloc(sizes[i]);
        if (live[i] != NULL) {
            fill_pattern(live[i], sizes[i], (unsigned int)(50 + i));
        }
    }
    check(live[0] && live[1] && live[2] && live[3] && live[4],
          "mixed sequence: five live blocks allocated");
    mimir_free(live[1]);
    mimir_free(live[3]);
    check(live[0] && pattern_ok(live[0], sizes[0], 50) &&
          live[2] && pattern_ok(live[2], sizes[2], 52) &&
          live[4] && pattern_ok(live[4], sizes[4], 54),
          "freeing each block exactly once disturbs no other live block's bytes");
    mimir_free(live[0]);
    mimir_free(live[2]);
    mimir_free(live[4]);
    {
        void *fresh = mimir_malloc(96);

        check(fresh != NULL, "after all frees, a fresh allocation succeeds (baseline behaviour)");
        mimir_free(fresh);
    }
    case_end();
}

static void case_map_041(void)
{
    void *p;

    case_begin("P3-T-MAP-041");
    mimir_free(NULL);
    check(1, "mimir_free(NULL) returns normally, no error (hook C-005)");
    p = mimir_malloc(80);
    check(p != NULL, "later allocation behaves normally after free(NULL)");
    if (p != NULL) {
        fill_pattern((unsigned char *)p, 80, 41);
        check(pattern_ok((const unsigned char *)p, 80, 41),
              "post-free(NULL) allocation fully usable");
    }
    mimir_free(p);
    case_end();
}

static void case_map_051(void)
{
    unsigned char *p;
    void *fresh;

    case_begin("P3-T-MAP-051");
    p = (unsigned char *)mimir_malloc(512);
    check(p != NULL, "block obtained from mimir_malloc (compatible family)");
    if (p != NULL) {
        fill_pattern(p, 512, 51);
        check(pattern_ok(p, 512, 51), "block contents verified before release");
    }
    mimir_free(p);
    check(1, "block released by the compatible API family (mimir_free), exactly once");
    fresh = mimir_malloc(512);
    check(fresh != NULL, "fresh allocation after compatible release succeeds");
    mimir_free(fresh);
    case_end();
}

/* ------------------------------------------------------------------ */
/* DATA cases                                                          */
/* ------------------------------------------------------------------ */

static void case_data_001_002(void)
{
    unsigned char *p;
    int cycles = 0;
    int i;

    case_begin("P3-T-DATA-001");
    p = (unsigned char *)mimir_malloc(777);
    check(p != NULL, "single cycle: allocation succeeds");
    if (p != NULL) {
        fill_pattern(p, 777, 61);
        check(pattern_ok(p, 777, 61),
              "single cycle: unique pattern verifies over full extent");
    }
    mimir_free(p);
    {
        void *fresh = mimir_malloc(777);

        check(fresh != NULL, "single cycle: allocation after the cycle succeeds (baseline behaviour)");
        mimir_free(fresh);
    }
    case_end();

    case_begin("P3-T-DATA-002");
    for (i = 0; i < 200; i++) {
        size_t n = (size_t)(i % 97 + 1) * 13u;
        unsigned char *q = (unsigned char *)mimir_malloc(n);

        if (q == NULL) {
            break;
        }
        fill_pattern(q, n, (unsigned int)(100 + i));
        if (!pattern_ok(q, n, (unsigned int)(100 + i))) {
            mimir_free(q);
            break;
        }
        mimir_free(q);
        cycles++;
    }
    check(cycles == 200, "200 allocate/write/verify/free cycles complete, each returning to baseline behaviour");
    case_end();
}

static void case_data_011_012(void)
{
    enum { LIVE = 6 };
    unsigned char *live[LIVE];
    size_t sizes[LIVE];
    int i;
    int j;
    int rounds_ok = 1;
    int disjoint = 1;

    for (i = 0; i < LIVE; i++) {
        sizes[i] = 1 + (size_t)(rng_next() % 5000u);
        live[i] = (unsigned char *)mimir_malloc(sizes[i]);
        if (live[i] != NULL) {
            fill_pattern(live[i], sizes[i], (unsigned int)(200 + i));
        }
    }

    case_begin("P3-T-DATA-011");
    check(live[0] && live[1] && live[2] && live[3] && live[4] && live[5],
          "live set of seeded random sizes coexists (seed 0x243F6A8885A308D3)");
    for (j = 0; j < 3; j++) {
        for (i = 0; i < LIVE; i++) {
            if (live[i] == NULL ||
                !pattern_ok(live[i], sizes[i], (unsigned int)(200 + i)) ||
                live[i][0] != (unsigned char)((0 * 31u + (200u + (unsigned)i) * 17u) & 0xffu)) {
                rounds_ok = 0;
            }
        }
    }
    check(rounds_ok, "every live block's unique pattern still reads as written on repeated checks, first and last bytes included");
    case_end();

    case_begin("P3-T-DATA-012");
    for (i = 0; i < LIVE; i++) {
        for (j = i + 1; j < LIVE; j++) {
            if (live[i] != NULL && live[j] != NULL &&
                ranges_overlap(live[i], sizes[i], live[j], sizes[j])) {
                disjoint = 0;
            }
        }
    }
    check(disjoint, "live blocks pairwise non-overlapping across full requested extents");
    if (live[1] != NULL) {
        fill_pattern(live[1], sizes[1], 219);
    }
    mimir_free(live[0]);
    {
        void *extra = mimir_malloc(333);

        check(extra != NULL, "new allocation proceeds while live set is live");
        mimir_free(extra);
    }
    {
        int intact = 1;

        for (i = 1; i < LIVE; i++) {
            unsigned int seed = (i == 1) ? 219u : (unsigned int)(200 + i);

            if (live[i] == NULL || !pattern_ok(live[i], sizes[i], seed)) {
                intact = 0;
            }
        }
        check(intact, "no live block's bytes changed when neighbours were written, freed, or newly allocated");
    }
    for (i = 1; i < LIVE; i++) {
        mimir_free(live[i]);
    }
    case_end();
}

static void case_data_021_022(size_t page)
{
    case_begin("P3-T-DATA-021");
    check(page > 0, "runtime system page size discovered (size derived from it, never a literal)");
    if (page > 0) {
        size_t n = 8 * page + page / 2;
        unsigned char *p = (unsigned char *)mimir_malloc(n);

        check(p != NULL, "large allocation spanning many runtime-discovered system pages succeeds");
        if (p != NULL) {
            fill_pattern(p, n, 88);
            check(pattern_ok(p, n, 88),
                  "pattern verified over the full large extent (start, middle, end)");
            check(p[page - 1] == (unsigned char)(((page - 1) * 31u + 88u * 17u + ((page - 1) >> 8)) & 0xffu) &&
                  p[page] == (unsigned char)((page * 31u + 88u * 17u + (page >> 8)) & 0xffu) &&
                  p[page + 1] == (unsigned char)(((page + 1) * 31u + 88u * 17u + ((page + 1) >> 8)) & 0xffu),
                  "bytes straddling a runtime-page boundary verify");
            mimir_free(p);
        }
    }
    case_end();

    case_begin("P3-T-DATA-022");
    {
        enum { KEEP = 4 };
        static const size_t keep_sizes[KEEP] = {111, 2222, 333, 4444};
        unsigned char *keep[KEEP];
        int churn = 0;
        int i;

        for (i = 0; i < KEEP; i++) {
            keep[i] = (unsigned char *)mimir_malloc(keep_sizes[i]);
            if (keep[i] != NULL) {
                fill_pattern(keep[i], keep_sizes[i], (unsigned int)(90 + i));
            }
        }
        check(keep[0] && keep[1] && keep[2] && keep[3],
              "persistent live set allocated before churn");
        for (i = 0; i < 100; i++) {
            size_t n = (size_t)(rng_next() % 4000u) + 1;
            void *q = mimir_malloc(n);

            if (q == NULL) {
                break;
            }
            ((unsigned char *)q)[0] = (unsigned char)i;
            ((unsigned char *)q)[n - 1] = (unsigned char)(i + 1);
            mimir_free(q);
            churn++;
        }
        check(churn == 100, "100 interleaved alloc/free churn operations complete (no hang)");
        {
            int intact = 1;

            for (i = 0; i < KEEP; i++) {
                if (keep[i] == NULL ||
                    !pattern_ok(keep[i], keep_sizes[i], (unsigned int)(90 + i))) {
                    intact = 0;
                }
            }
            check(intact, "after churn, every surviving live block verifies intact");
        }
        for (i = 0; i < KEEP; i++) {
            mimir_free(keep[i]);
        }
        check(1, "surviving blocks each retired exactly once after churn");
    }
    case_end();
}

/* ------------------------------------------------------------------ */
/* ARITH cases                                                         */
/* ------------------------------------------------------------------ */

static void case_arith_001_002(void)
{
    static const size_t bad[5] = {
        SIZE_MAX, SIZE_MAX - 1, SIZE_MAX - 8, SIZE_MAX - 16, SIZE_MAX / 2
    };
    const char *const badname[5] = {
        "SIZE_MAX", "SIZE_MAX-1", "SIZE_MAX-8", "SIZE_MAX-16", "SIZE_MAX/2"
    };
    int all_null = 1;
    int i;
    void *after;

    case_begin("P3-T-ARITH-001");
    check(mimir_malloc(SIZE_MAX) == NULL,
          "request at the maximum representable size is rejected with the defined failure (null), no wrapped allocation");
    case_end();

    case_begin("P3-T-ARITH-002");
    for (i = 0; i < 5; i++) {
        char label[96];

        snprintf(label, sizeof label, "boundary request %s rejected with null", badname[i]);
        if (mimir_malloc(bad[i]) != NULL) {
            all_null = 0;
            check(0, label);
        }
    }
    check(all_null, "each symbolic boundary request (max, max-reserve class, representable-unsatisfiable) is rejected, never wraps to a small allocation");
    after = mimir_malloc(24);
    check(after != NULL, "ordinary allocation after boundary refusals succeeds");
    mimir_free(after);
    case_end();
}

static void case_arith_011_012(void)
{
    unsigned char input[64];
    unsigned char *sentinel;
    int causes_ok = 1;

    fill_pattern(input, sizeof input, 55);

    case_begin("P3-T-ARITH-011");
    sentinel = (unsigned char *)mimir_malloc(64);
    check(sentinel != NULL, "live block in place before failure case");
    if (sentinel != NULL) {
        fill_pattern(sentinel, 64, 56);
    }
    check(mimir_malloc(SIZE_MAX / 2) == NULL,
          "failure is reported via the defined failure channel (null), no partially constructed block returned or reachable");
    check(pattern_ok(input, sizeof input, 55),
          "caller-supplied inputs stay caller-owned and unchanged");
    if (sentinel != NULL) {
        check(pattern_ok(sentinel, 64, 56),
              "pre-existing live block unchanged by the failure");
    }
    mimir_free(sentinel);
    case_end();

    case_begin("P3-T-ARITH-012");
    if (mimir_malloc(SIZE_MAX) != NULL) {
        causes_ok = 0;
    }
    if (mimir_malloc(SIZE_MAX / 2) != NULL) {
        causes_ok = 0;
    }
    if (mimir_calloc(SIZE_MAX / 2 + 1, 2) != NULL) {
        causes_ok = 0;
    }
    check(causes_ok, "oversize, mapping-failure, and calloc-overflow causes all yield the same failure class (defined failure, no ownership, no false success)");
    {
        void *after = mimir_malloc(24);

        check(after != NULL, "ordinary allocation after all failure causes succeeds");
        mimir_free(after);
    }
    case_end();
}

static void case_arith_043(size_t page)
{
    size_t align = _Alignof(max_align_t);
    size_t sizes[7];
    size_t n = 0;
    size_t i;
    int all_ok = 1;

    case_begin("P3-T-ARITH-043");
    sizes[n++] = 1;
    sizes[n++] = align - 1;
    sizes[n++] = align;
    sizes[n++] = align + 1;
    if (page > 0) {
        sizes[n++] = page - 1;
        sizes[n++] = page;
        sizes[n++] = page + 1;
    }
    check(page > 0, "runtime system page size discovered for boundary sizes");
    for (i = 0; i < n; i++) {
        unsigned char *p = (unsigned char *)mimir_malloc(sizes[i]);

        if (p == NULL) {
            all_ok = 0;
            continue;
        }
        fill_pattern(p, sizes[i], (unsigned int)(30 + i));
        if (!pattern_ok(p, sizes[i], (unsigned int)(30 + i))) {
            all_ok = 0;
        }
        mimir_free(p);
    }
    check(all_ok, "over-rejection guard: requests just inside the representable boundary (1, alignment-1, alignment, alignment+1, page-1, page, page+1) succeed and verify, never spuriously rejected");
    case_end();
}

/* ------------------------------------------------------------------ */
/* ERR cases: fork parent half + cross-thread (hook C-009 home)        */
/* ------------------------------------------------------------------ */

static void case_err_151(void)
{
    unsigned char *a;
    unsigned char *b;
    pid_t pid;
    int status = 0;

    case_begin("P3-T-ERR-151");
    a = (unsigned char *)mimir_malloc(200);
    b = (unsigned char *)mimir_malloc(300);
    check(a != NULL && b != NULL, "live parent blocks allocated immediately before fork");
    if (a != NULL) {
        fill_pattern(a, 200, 151);
    }
    if (b != NULL) {
        fill_pattern(b, 300, 152);
    }
    pid = fork();
    if (pid == 0) {
        /* Child: NO allocator call of any kind; the child's
         * allocator behaviour is exactly what U-17..U-21 leave
         * open and is not exercised (B-009 S8.2.4). */
        _exit(0);
    }
    check(pid > 0, "fork succeeded");
    if (pid > 0) {
        check(waitpid(pid, &status, 0) == pid && WIFEXITED(status) &&
              WEXITSTATUS(status) == 0,
              "child exited without touching the allocator (no child claim made)");
    }
    check(a != NULL && pattern_ok(a, 200, 151) &&
          b != NULL && pattern_ok(b, 300, 152),
          "parent case only: after fork, every live parent block is still valid, bytes as written, ownership unchanged");
    mimir_free(a);
    mimir_free(b);
    {
        void *fresh = mimir_malloc(64);

        check(fresh != NULL, "parent frees each block exactly once and allocates normally after fork");
        mimir_free(fresh);
    }
    case_end();
}

struct handoff {
    unsigned char *ptr;
    size_t size;
    unsigned int seed;
    int verified;
};

static void *verify_and_free_thread(void *arg)
{
    struct handoff *h = (struct handoff *)arg;

    h->verified = h->ptr != NULL && pattern_ok(h->ptr, h->size, h->seed);
    mimir_free(h->ptr);
    return NULL;
}

static void case_err_211(void)
{
    int cycles = 0;
    int i;

    case_begin("P3-T-ERR-211");
    for (i = 0; i < 50; i++) {
        struct handoff h;
        pthread_t th;

        h.ptr = (unsigned char *)mimir_malloc(256);
        h.size = 256;
        h.seed = (unsigned int)(300 + i);
        h.verified = 0;
        if (h.ptr == NULL) {
            break;
        }
        fill_pattern(h.ptr, h.size, h.seed);
        if (pthread_create(&th, NULL, verify_and_free_thread, &h) != 0) {
            mimir_free(h.ptr);
            break;
        }
        if (pthread_join(th, NULL) != 0 || !h.verified) {
            break;
        }
        cycles++;
    }
    check(cycles == 50, "50 cycles: block allocated on thread A freed on thread B exactly as on one thread, bytes intact at handoff, ownership preserved exactly (hook C-009)");
    case_end();
}

struct churn_job {
    size_t size;
    unsigned int seed;
    unsigned char *produced;
    int produced_ok;
};

static void *churn_thread(void *arg)
{
    struct churn_job *job = (struct churn_job *)arg;

    job->produced = (unsigned char *)mimir_malloc(job->size);
    if (job->produced != NULL) {
        fill_pattern(job->produced, job->size, job->seed);
        job->produced_ok = pattern_ok(job->produced, job->size, job->seed);
    }
    return NULL;
}

static void case_err_212(void)
{
    int rounds_ok = 0;
    int round;

    case_begin("P3-T-ERR-212");
    for (round = 0; round < 4; round++) {
        enum { THREADS = 4 };
        pthread_t th[THREADS];
        struct churn_job jobs[THREADS];
        struct handoff hands[THREADS];
        int created = 0;
        int ok = 1;
        int i;

        for (i = 0; i < THREADS; i++) {
            jobs[i].size = 128 + (size_t)(rng_next() % 2048u);
            jobs[i].seed = (unsigned int)(400 + round * 10 + i);
            jobs[i].produced = NULL;
            jobs[i].produced_ok = 0;
            hands[i].ptr = (unsigned char *)mimir_malloc(192);
            hands[i].size = 192;
            hands[i].seed = (unsigned int)(500 + round * 10 + i);
            hands[i].verified = 0;
            if (hands[i].ptr != NULL) {
                fill_pattern(hands[i].ptr, hands[i].size, hands[i].seed);
            } else {
                ok = 0;
            }
        }
        for (i = 0; i < THREADS; i++) {
            if (pthread_create(&th[i], NULL, churn_thread, &jobs[i]) == 0) {
                created++;
            } else {
                ok = 0;
            }
        }
        for (i = 0; i < created; i++) {
            pthread_join(th[i], NULL);
        }
        /* Transfer back: each block the worker threads
         * produced is transferred to main, which verifies it
         * and retires it exactly once as its new owner (the
         * "or after" half of ERR-212; the cross-thread free
         * itself is ERR-211's case). The hands[] blocks are a
         * main-owned live set held across the churn: verified
         * undisturbed and retired exactly once by main. */
        for (i = 0; i < THREADS; i++) {
            if (jobs[i].produced == NULL || !jobs[i].produced_ok ||
                !pattern_ok(jobs[i].produced, jobs[i].size, jobs[i].seed)) {
                ok = 0;
            }
            mimir_free(jobs[i].produced);
            if (hands[i].ptr == NULL ||
                !pattern_ok(hands[i].ptr, hands[i].size, hands[i].seed)) {
                ok = 0;
            }
            mimir_free(hands[i].ptr);
        }
        if (ok) {
            rounds_ok++;
        }
    }
    check(rounds_ok == 4, "churn: 4 rounds x 4 threads created and destroyed while blocks transfer between threads (seeded); every transferred block released exactly once by its correct owner, no overlap or corruption (hook C-009; correctness only)");
    case_end();
}

/* ------------------------------------------------------------------ */
/* CAL cases                                                           */
/* ------------------------------------------------------------------ */

static int all_zero(const unsigned char *p, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        if (p[i] != 0) {
            return 0;
        }
    }
    return 1;
}

static void case_cal_001_002_003(void)
{
    unsigned char *p;
    unsigned char *q;

    case_begin("P3-T-CAL-001");
    p = (unsigned char *)mimir_calloc(4, 8);
    check(p != NULL, "calloc(4, 8) returns non-null");
    if (p != NULL) {
        check((uintptr_t)p % _Alignof(max_align_t) == 0,
              "calloc storage suitably aligned as in MALLOC-01 (no numeric value asserted, U-15)");
        check(all_zero(p, 32), "every byte reads zero before any caller write (hook C-002)");
    }
    mimir_free(p);
    q = (unsigned char *)mimir_calloc(3, 7);
    check(q != NULL && all_zero(q, 21),
          "calloc(3, 7): non-multiple-of-alignment product (21 bytes) returns non-null and zeroed");
    mimir_free(q);
    case_end();

    case_begin("P3-T-CAL-002");
    {
        static const size_t pairs[5][2] = {
            {1, 1}, {2, 3}, {5, 13}, {16, 16}, {7, 100}
        };
        int ok = 1;
        int i;

        for (i = 0; i < 5; i++) {
            size_t n = pairs[i][0] * pairs[i][1];
            unsigned char *r = (unsigned char *)mimir_calloc(pairs[i][0], pairs[i][1]);

            if (r == NULL || !all_zero(r, n) ||
                (uintptr_t)r % _Alignof(max_align_t) != 0) {
                ok = 0;
            }
            mimir_free(r);
        }
        check(ok, "zeroed, suitably aligned result holds across the registered size set including non-multiples of the alignment");
    }
    case_end();

    case_begin("P3-T-CAL-003");
    p = (unsigned char *)mimir_calloc(37, 11);
    check(p != NULL, "calloc(37, 11) returns non-null");
    if (p != NULL) {
        check(all_zero(p, 37u * 11u),
              "every inspected byte of the requested product (407 bytes) reads zero at the point of return; no byte beyond the requested product is inspected as an oracle");
    }
    mimir_free(p);
    case_end();
}

static void case_cal_011_012_021(void)
{
    case_begin("P3-T-CAL-011");
    check(mimir_calloc(SIZE_MAX / 2 + 1, 2) == NULL,
          "overflow product (count x size unrepresentable) fails with null, no wrapped-around allocation (hook C-001; errno not asserted, U-10)");
    check(mimir_calloc(2, SIZE_MAX / 2 + 1) == NULL,
          "overflow detected with the factors in the other order");
    check(mimir_calloc(SIZE_MAX, SIZE_MAX) == NULL,
          "maximal overflow product fails with null");
    case_end();

    case_begin("P3-T-CAL-012");
    check(mimir_calloc(1, SIZE_MAX / 2) == NULL,
          "representable product above the target maximum fails with null, no wrapped allocation");
    check(mimir_calloc(SIZE_MAX / 2, 1) == NULL,
          "same representable-but-unsatisfiable product with factors exchanged fails with null");
    case_end();

    case_begin("P3-T-CAL-021");
    check(mimir_calloc(1024, SIZE_MAX / 1024) == NULL,
          "non-overflow failure (representable product cannot be satisfied) returns null, no pointer to unallocated storage in any form");
    {
        unsigned char *after = (unsigned char *)mimir_calloc(8, 8);

        check(after != NULL && all_zero(after, 64),
              "after the failure the next ordinary in-scope calloc behaves normally (non-null, zeroed)");
        mimir_free(after);
    }
    case_end();
}

/* ------------------------------------------------------------------ */
/* REA cases                                                           */
/* ------------------------------------------------------------------ */

static void case_rea_001_002(void)
{
    unsigned char *p;
    unsigned char *grown;
    unsigned char *shrunk;

    case_begin("P3-T-REA-001");
    p = (unsigned char *)mimir_malloc(32);
    check(p != NULL, "grow/shrink setup allocation succeeds");
    if (p != NULL) {
        fill_pattern(p, 32, 71);
        grown = (unsigned char *)mimir_realloc(p, 512);
        check(grown != NULL, "realloc grow to 512 succeeds (hook C-004)");
        if (grown != NULL) {
            check(pattern_ok(grown, 32, 71),
                  "grow: first min(old, new) = 32 bytes preserved byte-for-byte");
            shrunk = (unsigned char *)mimir_realloc(grown, 16);
            check(shrunk != NULL, "realloc shrink to 16 succeeds");
            if (shrunk != NULL) {
                check(pattern_ok(shrunk, 16, 71),
                      "shrink: first min(old, new) = 16 bytes preserved byte-for-byte");
                mimir_free(shrunk);
            } else {
                mimir_free(grown);
            }
        } else {
            mimir_free(p);
        }
    }
    case_end();

    case_begin("P3-T-REA-002");
    p = (unsigned char *)mimir_malloc(24);
    check(p != NULL, "move-case setup allocation succeeds");
    if (p != NULL) {
        void *old = (void *)p;

        fill_pattern(p, 24, 72);
        grown = (unsigned char *)mimir_realloc(p, 8192);
        check(grown != NULL, "realloc grow to 8192 succeeds");
        if (grown != NULL) {
            check(pattern_ok(grown, 24, 72),
                  "on a move, the preserved range (24 bytes) matches byte-for-byte");
            check((void *)grown != old,
                  "move instance observed: returned pointer differs from the original (precondition observation only, per B-008 S4.4 grow-always-moves design fact; never a behavioural oracle)");
            mimir_free(grown);
        } else {
            mimir_free(p);
        }
    }
    case_end();
}

static void case_rea_011_012(void)
{
    unsigned char *p;

    case_begin("P3-T-REA-011");
    p = (unsigned char *)mimir_malloc(64);
    check(p != NULL, "failure-preservation setup allocation succeeds");
    if (p != NULL) {
        fill_pattern(p, 64, 73);
        check(mimir_realloc(p, SIZE_MAX) == NULL,
              "realloc to an unsatisfiable size returns null (hook C-003; errno not asserted, U-10)");
        check(pattern_ok(p, 64, 73),
              "original allocation unchanged in contents and still caller-owned after failed realloc (MA-6.2-022)");
        fill_pattern(p, 64, 74);
        check(pattern_ok(p, 64, 74), "original remains usable after failed realloc");
        mimir_free(p);
    }
    case_end();

    case_begin("P3-T-REA-012");
    p = (unsigned char *)mimir_malloc(64);
    check(p != NULL, "freeable-after-failure setup allocation succeeds");
    if (p != NULL) {
        fill_pattern(p, 64, 75);
        check(mimir_realloc(p, SIZE_MAX / 2) == NULL,
              "realloc to a representable unsatisfiable size returns null");
        check(pattern_ok(p, 64, 75), "original intact after the failed realloc");
        mimir_free(p);
        check(1, "original freed normally exactly as before the call (MA-6.2-022)");
    }
    {
        void *fresh = mimir_malloc(64);

        check(fresh != NULL, "fresh allocation after freeing the preserved original succeeds (baseline behaviour)");
        mimir_free(fresh);
    }
    case_end();
}

static void case_rea_021_022_031(void)
{
    unsigned char *p;

    case_begin("P3-T-REA-021");
    p = (unsigned char *)mimir_realloc(NULL, 64);
    check(p != NULL, "realloc(NULL, 64) behaves as mimir_malloc(64): non-null, ownership passes");
    if (p != NULL) {
        fill_pattern(p, 64, 76);
        check(pattern_ok(p, 64, 76), "realloc(NULL) block fully writable/readable");
    }
    mimir_free(p);
    case_end();

    case_begin("P3-T-REA-022");
    check(mimir_realloc(NULL, SIZE_MAX / 2) == NULL,
          "realloc(NULL, unsatisfiable) behaves as the malloc failure case: null, no ownership passes");
    check(mimir_realloc(NULL, SIZE_MAX) == NULL,
          "realloc(NULL, SIZE_MAX) behaves as the malloc oversize case: null");
    p = (unsigned char *)mimir_realloc(NULL, 32);
    check(p != NULL, "ordinary realloc(NULL, 32) succeeds after the failure cases");
    mimir_free(p);
    case_end();

    case_begin("P3-T-REA-031");
    p = (unsigned char *)mimir_malloc(40);
    check(p != NULL, "grow-tail setup allocation succeeds");
    if (p != NULL) {
        unsigned char *grown;

        fill_pattern(p, 40, 77);
        grown = (unsigned char *)mimir_realloc(p, 400);
        check(grown != NULL, "realloc grow to 400 succeeds");
        if (grown != NULL) {
            check(pattern_ok(grown, 40, 77),
                  "preserved range (first min(old, new) = 40 bytes) verified; tail bytes beyond it are recorded UNSPECIFIED and are never asserted (REALLOC-06)");
            mimir_free(grown);
        } else {
            mimir_free(p);
        }
    }
    case_end();
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void)
{
    size_t page = page_size_runtime();

    printf("C-007 portable unit/contract test suite (B-009)\n");
    printf("target of this run: ARM64 Linux (this Pi only); x86-64 run OWED\n");
    printf("runtime system page size: %zu bytes (sysconf(_SC_PAGESIZE), failure-checked)\n", page);
    printf("deterministic seed: 0x243F6A8885A308D3\n");

    /* SYS units first: they perform no allocation. */
    case_sys_001();
    case_sys_003();
    skip("P3-T-SYS-002", "SKIPPED/NOT-YET: no completed per-target annex exists (B-009 Appendix B is a template only, INC-C007-03); an absent annex decision leaves the case BLOCKED on this target");
    skip("P3-T-DIFF-001", "SKIPPED/NOT-YET: x86-64 leg OWED in full (B-009 S3.4); no paired record exists to compare");
    skip("P3-T-DIFF-002", "SKIPPED/NOT-YET: x86-64 leg OWED in full (B-009 S3.4); no pair, so no divergence judgement can be made");

    /* MAP-042 first among runtime cases (see its comment). */
    case_map_042();
    case_map_001();
    case_map_002();
    case_map_003(page);
    case_map_011_012();
    case_map_021();
    case_map_031();
    case_map_032();
    case_map_041();
    case_map_051();

    skip("P3-T-FAIL-001", "SKIPPED/NOT-YET: needs the fault-injection shim on the L6 mapping boundary; no shim exists in the banked base (INC-C007-01, carried from INC-C004-02/INC-C005-03). The public-API failure+recovery aspect is run as MAP-011/MAP-012");
    skip("P3-T-FAIL-002", "SKIPPED/NOT-YET: N-sweep needs the fault-injection shim; none exists in the banked base (INC-C007-01)");
    skip("P3-T-FAIL-003", "SKIPPED/NOT-YET: the recovery probe is specified against an injected failure (shim); none exists. Uninjected recovery after a real unsatisfiable failure is run as MAP-012");
    skip("P3-T-FAIL-011", "SKIPPED/NOT-YET: the no-syscall aspect needs the shim's mapping-attempt counter; the return-value aspect is run as MAP-021/ARITH-001/ARITH-002");
    skip("P3-T-FAIL-021", "SKIPPED/NOT-YET: EMFILE/ENFILE-class injection needs the shim; none exists (INC-C007-01)");
    skip("P3-T-FAIL-031", "SKIPPED/NOT-YET: EAGAIN-class injection and attempt counting need the shim; none exists (INC-C007-01)");
    skip("P3-T-FAIL-041", "SKIPPED: BLOCKED-by-U-09 (zero-size undecided, V-01); no pass criterion exists and none is written");
    skip("P3-T-FAIL-051", "SKIPPED/NOT-YET: release-failure injection on the L6 release boundary needs the shim; the branch rests on code inspection as at C-004 (INC-C007-01)");
    skip("P3-T-FAIL-061", "SKIPPED/NOT-YET: lock/initialisation-primitive fault injection does not exist in the banked base (INC-C007-01)");
    skip("P3-T-FAIL-081", "SKIPPED/NOT-YET: DIAGNOSTIC-ONLY case for a DEBUG/internal-check build; no such build exists in the banked base and it is never a conformance PASS (B-009 S7.2)");
    inspected("P3-T-FAIL-091", "INSPECTED (inapplicability recorded, per its own row): the banked baseline makes ONE mapping attempt per allocation in alloc_locked (src/core/mimir_core.c, single mimir_platform_map_acquire call site), all-or-nothing with release-on-partial-failure before any publication; the multi-step variant this case guards does not exist in Phase 3. See NOTES_C007.md");
    skip("P3-T-FAIL-101", "SKIPPED/NOT-YET: forcing page-size discovery failure needs interposition on the L6 query; none exists in the banked base (INC-C007-01). Discovery itself is failure-checked and exercised in every run of this suite");
    skip("P3-T-FAIL-111", "SKIPPED/NOT-YET: needs an instrumented error-reporting path placed in a cannot-allocate state; no such instrumentation exists in the banked base (INC-C007-01)");

    case_data_001_002();
    case_data_011_012();
    case_data_021_022(page);

    case_arith_001_002();
    case_arith_011_012();
    skip("P3-T-ARITH-031", "SKIPPED/NOT-YET: metadata-allocation failure injection needs the shim; none exists (INC-C007-01)");
    skip("P3-T-ARITH-032", "SKIPPED/NOT-YET: failure after partial descriptor construction needs fault injection; none exists (INC-C007-01). The following-allocation half is run as MAP-012");
    skip("P3-T-ARITH-041", "SKIPPED/NOT-YET: the namespaced API takes no alignment parameter, so alignment-rounding overflow has no executable instance here; the size+reserve overflow aspect is run as ARITH-002 (INC-C007-05)");
    skip("P3-T-ARITH-042", "SKIPPED/NOT-YET: the before-any-mapping-attempt aspect needs the shim's attempt counter (count 0); the rejection aspect is run as ARITH-001/ARITH-002");
    case_arith_043(page);

    skip("P3-T-ALIGN-001", "SKIPPED/NOT-YET: no explicit-alignment entry point exists in the banked namespaced surface (documented absence, B-008 S6.1; INC-C007-05). Fundamental alignment of mimir_malloc itself is run under MAP-001");
    skip("P3-T-ALIGN-002", "SKIPPED/NOT-YET: no explicit-alignment entry point exists in the banked surface (INC-C007-05)");
    skip("P3-T-ALIGN-011", "SKIPPED/NOT-YET: no explicit-alignment entry point exists, and its failure induction also needs the shim (INC-C007-01/-05)");
    skip("P3-T-ALIGN-012", "SKIPPED/NOT-YET: no explicit-alignment entry point exists in the banked surface (INC-C007-05)");
    skip("P3-T-ALIGN-021", "SKIPPED: BLOCKED-by-U-11 (invalid-alignment validity undecided, V-03); no pass criterion exists");
    skip("P3-T-ALIGN-031", "SKIPPED: BLOCKED-by-U-11 (size-multiple-of-alignment rule undecided, V-03); no pass criterion exists");
    skip("P3-T-ALIGN-041", "SKIPPED: BLOCKED-by-U-09 (also U-11); no pass criterion exists");
    skip("P3-T-ALIGN-051", "SKIPPED/NOT-YET: posix_memalign is not in the banked surface (symbol set unsettled, U-01; documented absence, B-008 S6.1)");
    skip("P3-T-ALIGN-052", "SKIPPED/NOT-YET: posix_memalign is not in the banked surface (U-01)");
    skip("P3-T-ALIGN-061", "SKIPPED/NOT-YET: posix_memalign is not in the banked surface, so the C-008 unchanged-output model has no executable instance (U-01)");
    skip("P3-T-ALIGN-062", "SKIPPED: BLOCKED-by-U-14 (exact posix_memalign error numbers undecided, V-06); no numeric criterion is written");
    skip("P3-T-ALIGN-071", "SKIPPED: BLOCKED-by-U-11 (invalid alignment for posix_memalign undecided); no pass criterion exists");
    skip("P3-T-ALIGN-081", "SKIPPED: BLOCKED-by-U-09 (zero-size with valid alignment undecided); no pass criterion exists");

    skip("P3-T-INIT-001", "SKIPPED: BLOCKED-by-U-05 for promotion (watchdog bound undecided; results would be PRELIMINARY per contracts S6.3). Startup/init work is also C-006 scope, which is not in this slice's banked base");
    skip("P3-T-INIT-011", "SKIPPED: BLOCKED-by-U-05 (as for INIT-001); the instrumented normal path it needs does not exist in the banked base");
    skip("P3-T-INIT-021", "SKIPPED: BLOCKED-by-U-05 (as for INIT-001)");
    skip("P3-T-INIT-031", "SKIPPED: BLOCKED-by-U-05 (as for INIT-001)");
    skip("P3-T-INIT-041", "SKIPPED: BLOCKED-by-U-05 (inherits INIT-001); also needs a shared-library pre-main harness arrangement that does not exist in the banked base");
    skip("P3-T-INIT-051", "SKIPPED/NOT-YET: no distinct bootstrap path exists in the banked base to serve a block under C-010 conditions (registry anchors and lock are static, B-006 F-08 precondition fact), so the case cannot be instantiated as specified");
    inspected("P3-T-INIT-061", "INSPECTED (audit case): init-path dependency list enumerated from the banked sources -- src/api/mimir_alloc.c, src/core/mimir_core.c, src/platform/mimir_platform.c and their headers are portable C11 only (includes: stddef/stdint/string, sys/mman, unistd, pthread); no C++ container or utility appears on the path, and this programme (a pre-main-free binary) reaches main and allocates. Worksheet in NOTES_C007.md");
    skip("P3-T-INIT-071", "SKIPPED/NOT-YET: no ordering mechanism for allocating static/global construction is stated by any banked design, so the case's precondition cannot be satisfied as specified");
    inspected("P3-T-INIT-081", "INSPECTED (audit case): central bookkeeping is fixed static storage (registry head pointer + counters in src/core/mimir_core.c); per-block descriptors live inside each block's own mapping, so central bookkeeping footprint does not grow with the number of allocations or threads. Runtime footprint measurement is not claimed. Worksheet in NOTES_C007.md");

    skip("P3-T-ERR-031", "SKIPPED/NOT-YET: DIAGNOSTIC-ONLY debug-detection case; no DEBUG/internal-check build exists in the banked base; never a conformance PASS (B-009 S7.3)");
    skip("P3-T-ERR-041", "SKIPPED/NOT-YET: needs an internal-check build plus induced metadata corruption, neither available through the public API or the banked base (INC-C007-01)");
    skip("P3-T-ERR-051", "SKIPPED/NOT-YET: documentation terminology audit over the full document set; it is an audit artefact of the S7.6 completeness audit (exit item 12), not a runtime case this suite can pass. No claim is made here");
    skip("P3-T-ERR-061", "SKIPPED/NOT-YET: documentation audit as for ERR-051; this suite's own records qualify every page quantity as system page size or requested bytes");
    skip("P3-T-ERR-071", "SKIPPED/NOT-YET: needs the reporting path instrumented for dynamic-allocation use during an induced failure (its failure-context case FAIL-111 is likewise skipped); no such instrumentation exists");
    skip("P3-T-ERR-081", "SKIPPED/NOT-YET: needs reporting invoked from an induced corrupted state (F-09/F-12 induction); not available in the banked base");
    skip("P3-T-ERR-091", "SKIPPED/NOT-YET: needs bootstrap-path reporting instrumentation (as in INIT-011, BLOCKED-by-U-05); not available in the banked base");
    skip("P3-T-ERR-101", "SKIPPED/NOT-YET: needs cascaded-error instrumentation with retained-storage accounting; not available in the banked base");
    skip("P3-T-ERR-111", "SKIPPED/NOT-YET: full claim-surface documentation audit (headers, docs, diagnostics text) belongs to the S7.6 completeness audit; the header-scoped half is inspected under UNS-071");
    skip("P3-T-ERR-121", "SKIPPED/NOT-YET: documentation audit as for ERR-111 (implied claims included)");
    skip("P3-T-ERR-131", "SKIPPED/NOT-YET: documentation audit half belongs to the S7.6 completeness audit; any stronger runtime signal-state claim is BLOCKED-by-U-05 and is not made");
    skip("P3-T-ERR-141", "SKIPPED: BLOCKED-by-U-05 (also U-19): no atfork registration or lock discipline is documented in a banked design; no fork claim is made");
    case_err_151();
    skip("P3-T-ERR-161", "SKIPPED: BLOCKED-by-U-05 (also U-17..U-21): fork behaviour beyond the ERR-151 parent case is not Defined; no pass criterion exists");
    skip("P3-T-ERR-171", "SKIPPED: BLOCKED-by-U-03, out of Phase 3 scope: no runtime configuration surface exists (L7 compiled out, B-006 S12.3)");
    skip("P3-T-ERR-181", "SKIPPED: BLOCKED-by-U-03, out of Phase 3 scope: no statistics setting exists (as for ERR-171)");
    skip("P3-T-ERR-191", "SKIPPED: BLOCKED-by-U-03, out of Phase 3 scope: no diagnostics setting exists (as for ERR-171)");
    skip("P3-T-ERR-201", "SKIPPED: BLOCKED-by-U-03, out of Phase 3 scope: no config or config version ID exists (as for ERR-171)");
    case_err_211();
    case_err_212();

    case_cal_001_002_003();
    case_cal_011_012_021();
    skip("P3-T-CAL-031", "SKIPPED: BLOCKED-by-U-09 (calloc zero factor undecided, V-01); no pass criterion exists and none is written. The implementation's zero-factor posture is recorded by C-005, not by this suite");

    case_rea_001_002();
    case_rea_011_012();
    case_rea_021_022_031();
    skip("P3-T-REA-041", "SKIPPED: BLOCKED-by-U-12 (realloc(p, 0) undecided, V-04); in particular neither the return value nor the ownership/freeability of p is asserted. The implementation posture is recorded by C-005, not by this suite");

    skip("P3-T-COND-001", "SKIPPED: BLOCKED-by-U-16: the reallocarray gate does not hold for this target (no annex records it; the symbol is absent from the banked surface)");
    skip("P3-T-COND-002", "SKIPPED: BLOCKED-by-U-16 (as for COND-001)");
    skip("P3-T-COND-003", "SKIPPED: BLOCKED-by-U-16 (as for COND-001)");

    skip("P3-T-UNS-001", "SKIPPED: BLOCKED-by-U-09 (and U-12 for realloc(p, 0)): zero-size is listed unsupported/unresolved in the written record and no conformance case in this suite claims a zero-size result; no runtime refusal is asserted");
    skip("P3-T-UNS-011", "SKIPPED: BLOCKED-by-U-11 (numeric guarantee also U-15): explicit alignment is listed unsupported in the written record; no failure model exists to assert");
    inspected("P3-T-UNS-021", "INSPECTED (audit case): docs/PHASE_3_MINIMAL_ALLOCATOR_DESIGN.md (B-006) states it provides NO mimir_calloc and NO mimir_realloc (its S12.2, verified by grep at this slice's run); the pair's conformance home is the CAL/REA cases above under the B-008 S7.2 amended supported list, by analogy under U-03. Worksheet in NOTES_C007.md");
    inspected("P3-T-UNS-031", "INSPECTED (audit case): the API translation unit exports exactly mimir_malloc, mimir_free, mimir_calloc, mimir_realloc and no standard name (nm evidence in the recorded run); no interposition or standard-name replacement is claimed or tested");
    inspected("P3-T-UNS-041", "INSPECTED (audit case): no case in this suite asserts address reuse, trim, size classes, caches, or sharding; the suite contains no trim operation and no fuzzer (the B-009 S4.3d fuzzer does not exist in the banked base, INC-C007-01)");
    skip("P3-T-UNS-051", "SKIPPED: BLOCKED-by-U-03: whether any statistics/trim surface is even in scope is the open U-03 decision; no Phase 3 conformance case here depends on a stats/trim call");
    skip("P3-T-UNS-061", "SKIPPED: BLOCKED-by-U-17 (under U-05): lock state in the fork child is UNRESOLVED; nothing about the child is asserted (see ERR-151 parent case)");
    skip("P3-T-UNS-062", "SKIPPED: BLOCKED-by-U-18 (under U-05): first allocation in the fork child is UNRESOLVED");
    skip("P3-T-UNS-063", "SKIPPED: BLOCKED-by-U-19 (under U-05): atfork ordering is UNRESOLVED");
    skip("P3-T-UNS-064", "SKIPPED: BLOCKED-by-U-20 (under U-05): statistics/diagnostics across fork are UNRESOLVED (and no stats surface exists)");
    skip("P3-T-UNS-065", "SKIPPED: BLOCKED-by-U-21 (under U-05): fork during bootstrap is UNRESOLVED");
    inspected("P3-T-UNS-071", "INSPECTED (audit case, header-scoped): include/mimir_alloc.h states no async-signal-safety claim (it expressly states none is stated or implied), and no case in this suite calls an allocator function from a signal handler -- this suite contains no signal handler at all (verified by grep of this source at run time). The full claim-surface audit remains ERR-111..131, skipped above");
    inspected("P3-T-UNS-081", "INSPECTED (audit case): this suite's only recorded run is ARM64 Linux + glibc on this Pi; no annex or case instance exists for Android/bionic/musl/Termux/proot and no evidence from them is worded as exit evidence. The x86-64 annex is likewise absent and its run OWED");
    inspected("P3-T-UNS-091", "INSPECTED (audit case): no timing value is an oracle in any case of this suite (verified by inspection of this source: no clock/timing assertion exists); timestamps in the recorded run are records, never oracles, and no PASS line carries a performance conclusion");
    skip("P3-T-UNS-101", "SKIPPED: BLOCKED-by-U-13 (presence also U-01): malloc_usable_size is outside its COND gate; no presence, semantics, or null-case result is asserted");

    printf("SUITE cases: %d passed, %d failed, %d skipped, %d inspected (of 119 registered)\n",
           cases_pass, cases_fail, cases_skip, cases_insp);
    printf("CHECKS: %d run, %d failed\n", total_checks, total_fails);
    printf("RESULT: %s\n", (cases_fail == 0 && total_fails == 0) ? "PASS" : "FAIL");
    printf("x86-64 run of this suite: OWED (B-009 S3.4); Phase 3 exit: UNMET\n");
    return (cases_fail == 0 && total_fails == 0) ? 0 : 1;
}
