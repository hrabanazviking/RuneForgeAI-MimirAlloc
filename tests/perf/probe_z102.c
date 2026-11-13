/* probe_z102.c - Z-102 memory-pressure chaos probe.
 *
 * Roadmap slice Z-102 (Wave 12): "Cgroup ceilings
 * (4 GiB, 16 GiB) over allocation storms: behaviour at
 * the wall, failure cleanliness, recovery. Gate: chaos
 * report; every failure mode is a clean NULL + diag
 * line or a filed defect."
 *
 * Two binaries from this one source (the bench_k002
 * house pattern): the MimirAlloc binary links the
 * production LIBOBJS and drives the namespaced API
 * (mimir_malloc / mimir_free + the public statistics
 * snapshot + the Z-079 core budget surface for the
 * budget mode); the glibc binary (-DLEG_GLIBC) drives
 * plain malloc/free as the control leg. Identical storm
 * code for both legs.
 *
 * Modes (argv[1]):
 *  dense <chunk_mib> <max_chunks>
 *      Pure-cgroup storm: allocate dense-touched large
 *      chunks until the ceiling answers. Every chunk is
 *      memset in full (the caller's touch is what the
 *      cgroup charges) and pattern-verified per page.
 *      Progress lines are flushed as they are printed,
 *      with a marker BEFORE each chunk's touch, so if
 *      the kernel OOM-kills the process at the wall the
 *      last line brackets where death occurred (inside
 *      the allocator call or inside the caller's
 *      touch). A clean NULL instead completes the
 *      survival path: integrity check, free-half,
 *      re-allocate, free-all, health cycle.
 *  small <block_bytes> <max_blocks>
 *      Pure-cgroup storm of small blocks (pointer
 *      tracker pre-mmap'd by the probe itself, its
 *      size printed and charged honestly). Same
 *      survival path on a clean NULL.
 *  poll <chunk_mib> <stop_pct>
 *      Approach-and-recover INSIDE the ceiling: allocate
 *      dense-touched chunks, polling the cgroup's
 *      memory.current after each, and stop at stop_pct
 *      of memory.max (or at a clean NULL). Then:
 *      integrity check, free every second chunk,
 *      re-allocate back above 80%, verify, free all,
 *      allocator quiescence figures, health cycle.
 *      This mode must PASS (exit 0) or the slice's
 *      recovery claim fails with it.
 *  budget <budget_bytes> <chunk_mib> <max_chunks>
 *      MimirAlloc only (glibc prints SKIP-BY-DESIGN):
 *      the Z-079 soft budget over Q-MAPPED is set to
 *      budget_bytes (below the cgroup ceiling) and a
 *      large-chunk storm runs against it. The budget
 *      judges MAPPED bytes at backing-growth time, so
 *      chunks are touched one page each (usability
 *      proof) rather than dense-filled. Expected: clean
 *      NULL refusals counted in Q-REFUSED-BUDGET
 *      (mimir_core_budget_failures), process alive,
 *      alloc_failures UNMOVED (D-014 disjointness),
 *      then budget unset and an over-old-budget
 *      allocation succeeds (recovery).
 *  heritage <headroom_mib> <chunk_mib> <max_chunks>
 *      The Z-060 ceiling form, run INSIDE the cgroup
 *      scope by the runner: the probe sets its own
 *      RLIMIT_AS from its measured VmSize baseline +
 *      headroom, so backing acquisition itself fails
 *      at the allocator (mmap ENOMEM) while the cgroup
 *      ceiling stands as the outer wall. Dense storm
 *      to the clean NULL; for the MimirAlloc leg the
 *      runner counts the J-007 diag lines on stderr
 *      (MIMIRALLOC_DIAG=1). Then the limit is restored
 *      and recovery is proven.
 *
 * Every mode prints key=value lines; the last line is
 * "PROBE_Z102_<MODE>: PASS" / "SURVIVED_CLEAN_NULL" /
 * "FAIL <reason>" as applicable. Exit 0 iff the mode
 * completed its contract; an OOM kill inside the
 * pure-cgroup modes is the kernel's answer (exit 137
 * via SIGKILL), classified by the runner from the
 * flushed progress lines, never by this probe.
 *
 * Safety: this probe only ever allocates inside its
 * own process; the cgroup scopes are created by the
 * runner (run_z102.sh) around this probe alone. No
 * other process is placed in a scope, limited, or
 * signalled. Standalone via make probe-z102 /
 * make test-z102; NOT part of make test.
 */
#define _GNU_SOURCE 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <malloc.h>

#ifndef LEG_GLIBC
#include "mimir_alloc.h"
#include "core/mimir_core.h"
#define P_MALLOC mimir_malloc
#define P_FREE mimir_free
#define LEG_NAME "mimir"
#else
#define P_MALLOC malloc
#define P_FREE free
#define LEG_NAME "glibc"
#endif

/* ---------- cgroup v2 readers (this process only) --- */

static int cg_relpath(char *buf, size_t cap)
{
    FILE *f = fopen("/proc/self/cgroup", "r");
    char line[1024];
    int found = 0;
    if (!f) return -1;
    while (fgets(line, sizeof line, f)) {
        char *c1 = strchr(line, ':');
        char *c2 = c1 ? strchr(c1 + 1, ':') : NULL;
        if (!c2) continue;
        snprintf(buf, cap, "%s", c2 + 1);
        char *nl = strchr(buf, '\n');
        if (nl) *nl = '\0';
        found = 1;
    }
    fclose(f);
    return found ? 0 : -1;
}

static int cg_path(const char *leaf, char *buf, size_t cap)
{
    char rel[1024];
    if (cg_relpath(rel, sizeof rel) != 0) return -1;
    snprintf(buf, cap, "/sys/fs/cgroup%s/%s", rel, leaf);
    return 0;
}

/* memory.current / memory.max: single value or "max". */
static long long cg_value(const char *leaf)
{
    char path[1200], buf[128];
    FILE *f;
    if (cg_path(leaf, path, sizeof path) != 0) return -1;
    f = fopen(path, "r");
    if (!f) return -1;
    if (!fgets(buf, sizeof buf, f)) { fclose(f); return -1; }
    fclose(f);
    if (strncmp(buf, "max", 3) == 0) return -2;
    return strtoll(buf, NULL, 10);
}

/* memory.events: "key value" lines. */
static long long cg_event(const char *key)
{
    char path[1200], line[256];
    FILE *f;
    size_t klen = strlen(key);
    long long out = -1;
    if (cg_path("memory.events", path, sizeof path) != 0) return -1;
    f = fopen(path, "r");
    if (!f) return -1;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, key, klen) == 0 && line[klen] == ' ') {
            out = strtoll(line + klen + 1, NULL, 10);
            break;
        }
    }
    fclose(f);
    return out;
}

static long long status_kib(const char *key)
{
    FILE *f = fopen("/proc/self/status", "r");
    char line[512];
    size_t klen = strlen(key);
    long long out = -1;
    if (!f) return -1;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, key, klen) == 0) {
            out = strtoll(line + klen, NULL, 10);
            break;
        }
    }
    fclose(f);
    return out;
}

static void print_cg(const char *tag)
{
    printf("%s mem_current=%lld mem_max=%lld ev_max=%lld "
           "ev_oom=%lld ev_oom_kill=%lld rss_kib=%lld\n",
           tag, cg_value("memory.current"), cg_value("memory.max"),
           cg_event("max"), cg_event("oom"), cg_event("oom_kill"),
           status_kib("VmRSS:"));
    fflush(stdout);
}

/* ---------- pattern helpers ------------------------- */

static unsigned char chunk_tag(size_t idx) { return (unsigned char)((idx % 250) + 1); }

/* Dense-touch a chunk and leave per-page evidence. */
static void touch_dense(void *p, size_t len, size_t idx)
{
    memset(p, chunk_tag(idx), len);
}

/* Verify first+last byte of every page of a chunk. */
static size_t verify_dense(void *const *ptrs, const size_t *lens,
                           size_t n, size_t page)
{
    size_t bad = 0;
    for (size_t i = 0; i < n; i++) {
        const unsigned char *p = (const unsigned char *)ptrs[i];
        unsigned char want = chunk_tag(i);
        if (!p) continue;
        for (size_t off = 0; off < lens[i]; off += page) {
            if (p[off] != want) bad++;
            if (p[lens[i] - 1] != want) bad++;
        }
    }
    return bad;
}

static int health_cycle(void)
{
    /* Small + large alloc/free with pattern checks: the
     * allocator (either leg) must be fully itself. */
    for (int i = 0; i < 1000; i++) {
        unsigned char *p = (unsigned char *)P_MALLOC(64);
        if (!p) return -1;
        memset(p, 0xC3, 64);
        if (p[0] != 0xC3 || p[63] != 0xC3) { P_FREE(p); return -2; }
        P_FREE(p);
    }
    {
        unsigned char *p = (unsigned char *)P_MALLOC(2u * 1024u * 1024u);
        if (!p) return -3;
        memset(p, 0x3C, 2u * 1024u * 1024u);
        if (p[0] != 0x3C || p[2u * 1024u * 1024u - 1] != 0x3C) {
            P_FREE(p);
            return -4;
        }
        P_FREE(p);
    }
    return 0;
}

static void print_allocator_state(const char *tag)
{
#ifndef LEG_GLIBC
    struct mimir_stats_v1 st;
    if (mimir_stats_snapshot(&st) == 0) {
        printf("%s alloc_failures=%llu budget_failures=%llu "
               "live_mappings=%llu live_spans=%llu "
               "live_small_objects=%llu bytes_mapped=%llu\n",
               tag,
               (unsigned long long)st.alloc_failures,
               (unsigned long long)mimir_core_budget_failures(),
               (unsigned long long)st.live_mappings,
               (unsigned long long)st.live_spans,
               (unsigned long long)st.live_small_objects,
               (unsigned long long)st.bytes_mapped);
    } else {
        printf("%s stats_snapshot=FAILED\n", tag);
    }
#else
    struct mallinfo2 mi = mallinfo2();
    printf("%s mallinfo_uordblks=%zu mallinfo_fordblks=%zu "
           "mallinfo_arena=%zu\n",
           tag, (size_t)mi.uordblks, (size_t)mi.fordblks,
           (size_t)mi.arena);
#endif
    fflush(stdout);
}

/* ---------- survival path (clean NULL) -------------- */

/* After a clean NULL at the wall: verify what is held,
 * free half, prove re-allocation, free all, health. */
static int survival_path(void **ptrs, size_t *lens, size_t n,
                         size_t chunk, const char *mode)
{
    size_t page = 4096;
    size_t bad = verify_dense((void *const *)ptrs,
                              lens, n, page);
    printf("%s_integrity_bad=%zu held=%zu\n", mode, bad, n);
    fflush(stdout);
    if (bad != 0) {
        printf("PROBE_Z102_%s: FAIL integrity after NULL\n",
               mode);
        return 2;
    }
    /* Free every second chunk, then re-allocate the
     * same count: recovery inside the same ceiling. */
    {
        size_t freed = 0, regrown = 0;
        for (size_t i = 1; i < n; i += 2) {
            P_FREE(ptrs[i]);
            ptrs[i] = NULL;
            freed++;
        }
        for (size_t i = 1; i < n; i += 2) {
            void *p = P_MALLOC(chunk);
            if (!p) break;
            touch_dense(p, chunk, i);
            ptrs[i] = p;
            regrown++;
        }
        printf("%s_recovery freed=%zu regrown=%zu\n",
               mode, freed, regrown);
        fflush(stdout);
        if (regrown != freed) {
            printf("PROBE_Z102_%s: FAIL regrow %zu of %zu\n",
                   mode, regrown, freed);
            return 2;
        }
    }
    for (size_t i = 0; i < n; i++) P_FREE(ptrs[i]);
    print_allocator_state("post_freeall");
    if (health_cycle() != 0) {
        printf("PROBE_Z102_%s: FAIL health cycle\n", mode);
        return 2;
    }
    printf("PROBE_Z102_%s: SURVIVED_CLEAN_NULL\n", mode);
    return 0;
}

/* ---------- dense mode ------------------------------ */

static int run_dense(size_t chunk, size_t max_chunks)
{
    void **ptrs = NULL;
    size_t *lens = NULL;
    size_t n = 0;
    long long null_at = -1;

    ptrs = (void **)mmap(NULL, max_chunks * sizeof(void *),
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    lens = (size_t *)mmap(NULL, max_chunks * sizeof(size_t),
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptrs == MAP_FAILED || lens == MAP_FAILED) {
        printf("PROBE_Z102_DENSE: FAIL tracker mmap\n");
        return 2;
    }
    printf("dense leg=%s chunk_bytes=%zu max_chunks=%zu "
           "tracker_bytes=%zu\n",
           LEG_NAME, chunk, max_chunks,
           max_chunks * (sizeof(void *) + sizeof(size_t)));
    print_cg("dense_start");
    for (size_t i = 0; i < max_chunks; i++) {
        void *p;
        printf("dense_alloc i=%zu live=%zu\n", i, n);
        fflush(stdout);
        p = P_MALLOC(chunk);
        if (!p) {
            null_at = (long long)i;
            printf("dense_clean_null i=%zu live=%zu\n", i, n);
            fflush(stdout);
            break;
        }
        printf("dense_touch i=%zu live=%zu\n", i, n);
        fflush(stdout);
        touch_dense(p, chunk, i);
        ptrs[n] = p;
        lens[n] = chunk;
        n++;
        if (n % 8 == 0) print_cg("dense_progress");
    }
    printf("dense_storm_end live=%zu live_bytes=%zu null_at=%lld\n",
           n, n * chunk, null_at);
    print_cg("dense_end");
    print_allocator_state("dense_end");
    if (null_at < 0) {
        printf("PROBE_Z102_DENSE: FAIL cap reached, no wall "
               "answer (neither NULL nor kill)\n");
        return 2;
    }
    return survival_path(ptrs, lens, n, chunk, "DENSE");
}

/* ---------- small mode ------------------------------ */

static int run_small(size_t block, size_t max_blocks)
{
    void **tracker = NULL;
    size_t n = 0;
    long long null_at = -1;

    tracker = (void **)mmap(NULL, max_blocks * sizeof(void *),
                            PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (tracker == MAP_FAILED) {
        printf("PROBE_Z102_SMALL: FAIL tracker mmap\n");
        return 2;
    }
    printf("small leg=%s block_bytes=%zu max_blocks=%zu "
           "tracker_bytes=%zu\n",
           LEG_NAME, block, max_blocks,
           max_blocks * sizeof(void *));
    print_cg("small_start");
    for (size_t i = 0; i < max_blocks; i++) {
        void *p = P_MALLOC(block);
        if (!p) {
            null_at = (long long)i;
            printf("small_clean_null i=%zu live=%zu\n", i, n);
            fflush(stdout);
            break;
        }
        ((uint64_t *)p)[0] = (uint64_t)(i + 1);
        ((unsigned char *)p)[block - 1] = 0x5A;
        tracker[n++] = p;
        if (n % 250000 == 0) {
            printf("small_progress live=%zu\n", n);
            print_cg("small_progress");
        }
    }
    printf("small_storm_end live=%zu null_at=%lld\n", n, null_at);
    print_cg("small_end");
    print_allocator_state("small_end");
    if (null_at < 0) {
        printf("PROBE_Z102_SMALL: FAIL cap reached, no wall "
               "answer (neither NULL nor kill)\n");
        return 2;
    }
    /* Integrity: first word + last byte of every block. */
    {
        size_t bad = 0;
        for (size_t i = 0; i < n; i++) {
            const unsigned char *p =
                (const unsigned char *)tracker[i];
            if (((const uint64_t *)p)[0] != (uint64_t)(i + 1)) bad++;
            if (p[block - 1] != 0x5A) bad++;
        }
        printf("small_integrity_bad=%zu held=%zu\n", bad, n);
        if (bad != 0) {
            printf("PROBE_Z102_SMALL: FAIL integrity\n");
            return 2;
        }
    }
    /* Free half, regrow, free all, health. */
    {
        size_t freed = 0, regrown = 0;
        for (size_t i = 1; i < n; i += 2) {
            P_FREE(tracker[i]);
            tracker[i] = NULL;
            freed++;
        }
        for (size_t i = 1; i < n; i += 2) {
            void *p = P_MALLOC(block);
            if (!p) break;
            ((uint64_t *)p)[0] = (uint64_t)(i + 1);
            ((unsigned char *)p)[block - 1] = 0x5A;
            tracker[i] = p;
            regrown++;
        }
        printf("small_recovery freed=%zu regrown=%zu\n",
               freed, regrown);
        if (regrown != freed) {
            printf("PROBE_Z102_SMALL: FAIL regrow\n");
            return 2;
        }
    }
    for (size_t i = 0; i < n; i++) P_FREE(tracker[i]);
    print_allocator_state("post_freeall");
    if (health_cycle() != 0) {
        printf("PROBE_Z102_SMALL: FAIL health cycle\n");
        return 2;
    }
    printf("PROBE_Z102_SMALL: SURVIVED_CLEAN_NULL\n");
    return 0;
}

/* ---------- poll mode ------------------------------- */

static int run_poll(size_t chunk, long stop_pct)
{
    void **ptrs = NULL;
    size_t *lens = NULL;
    size_t cap = 4096, n = 0;
    long long max = cg_value("memory.max");
    long long stop_at;
    int hit_null = 0;

    if (max <= 0) {
        printf("PROBE_Z102_POLL: FAIL no cgroup ceiling "
               "(memory.max=%lld)\n", max);
        return 2;
    }
    stop_at = max * stop_pct / 100;
    ptrs = (void **)mmap(NULL, cap * sizeof(void *),
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    lens = (size_t *)mmap(NULL, cap * sizeof(size_t),
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptrs == MAP_FAILED || lens == MAP_FAILED) {
        printf("PROBE_Z102_POLL: FAIL tracker mmap\n");
        return 2;
    }
    printf("poll leg=%s chunk_bytes=%zu mem_max=%lld "
           "stop_pct=%ld stop_at=%lld\n",
           LEG_NAME, chunk, max, stop_pct, stop_at);
    print_cg("poll_start");
    while (n < cap) {
        void *p = P_MALLOC(chunk);
        long long cur;
        if (!p) { hit_null = 1; break; }
        touch_dense(p, chunk, n);
        ptrs[n] = p;
        lens[n] = chunk;
        n++;
        cur = cg_value("memory.current");
        if (n % 16 == 0) {
            printf("poll_progress live=%zu mem_current=%lld\n",
                   n, cur);
            fflush(stdout);
        }
        if (cur >= stop_at) break;
    }
    printf("poll_approach live=%zu live_bytes=%zu hit_null=%d\n",
           n, n * chunk, hit_null);
    print_cg("poll_approach");
    {
        size_t bad = verify_dense((void *const *)ptrs, lens,
                                  n, 4096);
        printf("poll_integrity_bad=%zu\n", bad);
        if (bad != 0) {
            printf("PROBE_Z102_POLL: FAIL integrity at pressure\n");
            return 2;
        }
    }
    print_allocator_state("poll_at_pressure");
    /* Free every second chunk: pressure release. */
    {
        size_t held = n;
        for (size_t i = 1; i < n; i += 2) {
            P_FREE(ptrs[i]);
            ptrs[i] = NULL;
        }
        print_cg("poll_after_free_half");
        /* Re-approach: re-allocate the freed slots. */
        {
            size_t regrown = 0;
            for (size_t i = 1; i < held; i += 2) {
                void *p = P_MALLOC(chunk);
                if (!p) break;
                touch_dense(p, chunk, i);
                ptrs[i] = p;
                regrown++;
            }
            printf("poll_reapproach regrown=%zu of %zu\n",
                   regrown, held / 2);
            if (regrown != held / 2) {
                printf("PROBE_Z102_POLL: FAIL re-approach\n");
                return 2;
            }
        }
        {
            size_t bad = verify_dense((void *const *)ptrs,
                                      lens, held, 4096);
            printf("poll_reverify_bad=%zu\n", bad);
            if (bad != 0) {
                printf("PROBE_Z102_POLL: FAIL re-verify\n");
                return 2;
            }
        }
        for (size_t i = 0; i < held; i++) P_FREE(ptrs[i]);
    }
    print_cg("poll_after_free_all");
    print_allocator_state("poll_quiescent");
    if (health_cycle() != 0) {
        printf("PROBE_Z102_POLL: FAIL health cycle\n");
        return 2;
    }
    printf("PROBE_Z102_POLL: PASS\n");
    return 0;
}

/* ---------- budget mode ----------------------------- */

static int run_budget(unsigned long long budget, size_t chunk,
                      size_t max_chunks)
{
#ifdef LEG_GLIBC
    (void)budget; (void)chunk; (void)max_chunks;
    printf("PROBE_Z102_BUDGET: SKIP-BY-DESIGN glibc has no "
           "budget surface\n");
    return 0;
#else
    void **ptrs = NULL;
    size_t n = 0;
    long long null_at = -1;
    uint64_t bf_before, bf_after;
    struct mimir_stats_v1 st0, st1;

    ptrs = (void **)mmap(NULL, max_chunks * sizeof(void *),
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptrs == MAP_FAILED) {
        printf("PROBE_Z102_BUDGET: FAIL tracker mmap\n");
        return 2;
    }
    if (mimir_stats_snapshot(&st0) != 0) {
        printf("PROBE_Z102_BUDGET: FAIL snapshot before\n");
        return 2;
    }
    bf_before = mimir_core_budget_failures();
    if (mimir_budget_set_mapped_bytes((size_t)budget) != 0) {
        printf("PROBE_Z102_BUDGET: FAIL budget set\n");
        return 2;
    }
    printf("budget leg=mimir budget_bytes=%llu chunk_bytes=%zu "
           "budget_failures_before=%llu alloc_failures_before=%llu\n",
           budget, chunk,
           (unsigned long long)bf_before,
           (unsigned long long)st0.alloc_failures);
    print_cg("budget_start");
    for (size_t i = 0; i < max_chunks; i++) {
        void *p = mimir_malloc(chunk);
        if (!p) {
            null_at = (long long)i;
            printf("budget_clean_null i=%zu live=%zu\n", i, n);
            fflush(stdout);
            break;
        }
        /* Budget judges mapped bytes; one touched page
         * per chunk proves usability. */
        ((unsigned char *)p)[0] = chunk_tag(i);
        ptrs[n++] = p;
        if (n % 32 == 0) print_cg("budget_progress");
    }
    bf_after = mimir_core_budget_failures();
    if (mimir_stats_snapshot(&st1) != 0) {
        printf("PROBE_Z102_BUDGET: FAIL snapshot after\n");
        return 2;
    }
    printf("budget_storm_end live=%zu live_bytes=%zu "
           "null_at=%lld budget_failures=%llu->%llu "
           "alloc_failures=%llu->%llu bytes_mapped=%llu\n",
           n, n * chunk, null_at,
           (unsigned long long)bf_before,
           (unsigned long long)bf_after,
           (unsigned long long)st0.alloc_failures,
           (unsigned long long)st1.alloc_failures,
           (unsigned long long)st1.bytes_mapped);
    print_cg("budget_end");
    if (null_at < 0) {
        printf("PROBE_Z102_BUDGET: FAIL budget never refused\n");
        return 2;
    }
    if (bf_after <= bf_before) {
        printf("PROBE_Z102_BUDGET: FAIL refusals not counted "
               "in Q-REFUSED-BUDGET\n");
        return 2;
    }
    if (st1.alloc_failures != st0.alloc_failures) {
        printf("PROBE_Z102_BUDGET: FAIL D-014 disjointness: "
               "budget refusals leaked into alloc_failures\n");
        return 2;
    }
    /* Integrity of what is held (touched first pages). */
    {
        size_t bad = 0;
        for (size_t i = 0; i < n; i++) {
            if (((unsigned char *)ptrs[i])[0] != chunk_tag(i))
                bad++;
        }
        printf("budget_integrity_bad=%zu\n", bad);
        if (bad != 0) {
            printf("PROBE_Z102_BUDGET: FAIL integrity\n");
            return 2;
        }
    }
    for (size_t i = 0; i < n; i++) mimir_free(ptrs[i]);
    /* Recovery: unset the budget; an allocation larger
     * than the whole old budget must now succeed (it
     * stays far below the cgroup ceiling: budget was
     * half the ceiling, this is budget + one chunk). */
    if (mimir_budget_set_mapped_bytes(0) != 0) {
        printf("PROBE_Z102_BUDGET: FAIL budget unset\n");
        return 2;
    }
    {
        size_t big = (size_t)budget + chunk;
        void *p = mimir_malloc(big);
        printf("budget_recovery big_alloc=%s bytes=%zu\n",
               p ? "OK" : "NULL", big);
        if (!p) {
            printf("PROBE_Z102_BUDGET: FAIL recovery alloc\n");
            return 2;
        }
        ((unsigned char *)p)[0] = 0x7E;
        mimir_free(p);
    }
    print_allocator_state("budget_quiescent");
    if (health_cycle() != 0) {
        printf("PROBE_Z102_BUDGET: FAIL health cycle\n");
        return 2;
    }
    printf("PROBE_Z102_BUDGET: PASS\n");
    return 0;
#endif
}

/* ---------- heritage mode -------------------------- */

static int run_heritage(size_t headroom_mib, size_t chunk,
                        size_t max_chunks)
{
    void **ptrs = NULL;
    size_t *lens = NULL;
    size_t n = 0;
    long long null_at = -1;
    long long vm_base;
    struct rlimit rl;
#ifndef LEG_GLIBC
    struct mimir_stats_v1 st0;
    int have_st0 = (mimir_stats_snapshot(&st0) == 0);
#endif

    ptrs = (void **)mmap(NULL, max_chunks * sizeof(void *),
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    lens = (size_t *)mmap(NULL, max_chunks * sizeof(size_t),
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptrs == MAP_FAILED || lens == MAP_FAILED) {
        printf("PROBE_Z102_HERITAGE: FAIL tracker mmap\n");
        return 2;
    }
    /* Warmup: settle the allocator before measuring the
     * VmSize baseline (the Z-060 form). */
    {
        void *w = P_MALLOC(64u * 1024u * 1024u);
        if (w) {
            memset(w, 0x11, 64u * 1024u * 1024u);
            P_FREE(w);
        }
    }
    vm_base = status_kib("VmSize:");
    printf("heritage leg=%s vm_base_kib=%lld "
           "headroom_mib=%zu chunk_bytes=%zu\n",
           LEG_NAME, vm_base, headroom_mib, chunk);
    print_cg("heritage_start");
    if (getrlimit(RLIMIT_AS, &rl) != 0) {
        printf("PROBE_Z102_HERITAGE: FAIL getrlimit\n");
        return 2;
    }
    /* Lower ONLY the soft limit; the hard limit stays
     * where it was so the release below can restore the
     * soft limit (a lowered hard limit is one-way for a
     * non-privileged process). */
    rl.rlim_cur = (rlim_t)((vm_base * 1024) +
                           (headroom_mib * 1024 * 1024));
    if (setrlimit(RLIMIT_AS, &rl) != 0) {
        printf("PROBE_Z102_HERITAGE: FAIL setrlimit\n");
        return 2;
    }
    printf("heritage_rlimit_as=%llu\n",
           (unsigned long long)rl.rlim_cur);
    for (size_t i = 0; i < max_chunks; i++) {
        void *p = P_MALLOC(chunk);
        if (!p) {
            null_at = (long long)i;
            printf("heritage_clean_null i=%zu live=%zu\n",
                   i, n);
            fflush(stdout);
            break;
        }
        touch_dense(p, chunk, i);
        ptrs[n] = p;
        lens[n] = chunk;
        n++;
        if (n % 16 == 0) print_cg("heritage_progress");
    }
    printf("heritage_storm_end live=%zu live_bytes=%zu "
           "null_at=%lld\n", n, n * chunk, null_at);
#ifndef LEG_GLIBC
    if (have_st0) {
        struct mimir_stats_v1 st1;
        if (mimir_stats_snapshot(&st1) == 0) {
            printf("heritage_alloc_failures=%llu->%llu\n",
                   (unsigned long long)st0.alloc_failures,
                   (unsigned long long)st1.alloc_failures);
            if (null_at >= 0 &&
                st1.alloc_failures <= st0.alloc_failures) {
                printf("PROBE_Z102_HERITAGE: FAIL clean NULL "
                       "not counted in alloc_failures\n");
                return 2;
            }
        }
    }
#endif
    if (null_at < 0) {
        printf("PROBE_Z102_HERITAGE: FAIL cap reached, no "
               "clean NULL\n");
        return 2;
    }
    /* Restore the soft limit BEFORE the survival
     * path's re-allocations (pressure release, the
     * Z-060 form). */
    if (getrlimit(RLIMIT_AS, &rl) != 0) {
        printf("PROBE_Z102_HERITAGE: FAIL re-getrlimit\n");
        return 2;
    }
    rl.rlim_cur = rl.rlim_max;
    if (setrlimit(RLIMIT_AS, &rl) != 0) {
        printf("PROBE_Z102_HERITAGE: FAIL restore limit\n");
        return 2;
    }
    return survival_path(ptrs, lens, n, chunk, "HERITAGE");
}

/* ---------- main ------------------------------------ */

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "";
    size_t a1 = (argc > 2) ? strtoull(argv[2], NULL, 10) : 0;
    size_t a2 = (argc > 3) ? strtoull(argv[3], NULL, 10) : 0;
    size_t a3 = (argc > 4) ? strtoull(argv[4], NULL, 10) : 0;

    if (strcmp(mode, "dense") == 0) {
        return run_dense(a1 * 1024 * 1024, a2);
    }
    if (strcmp(mode, "small") == 0) {
        return run_small(a1, a2);
    }
    if (strcmp(mode, "poll") == 0) {
        return run_poll(a1 * 1024 * 1024, (long)a2);
    }
    if (strcmp(mode, "budget") == 0) {
        return run_budget(strtoull(argv[2], NULL, 10),
                          a2 * 1024 * 1024, a3);
    }
    if (strcmp(mode, "heritage") == 0) {
        return run_heritage(a1, a2 * 1024 * 1024, a3);
    }
    printf("usage: probe_z102 <dense|small|poll|budget|"
           "heritage> ...\n");
    return 2;
}
