/* test_z067_livecount.c -- Z-067 (D-12) INC-J006-06
 * live_count release-path detection evidence
 * (docs/MIMIRALLOC_Z13_ROADMAP.md slice Z-067; the
 * 25-probe flip itself lives in tests/core/test_j006.c,
 * probe span-live-zero). INC-J006-04 (free_head naming a
 * live object) is covered by leg 5 as the recorded
 * undetectable-by-design class.
 *
 * Legs, each in a forked child so corruption and counter
 * baselines never leak between them:
 *   0 control: valid churn with drains and trims on a
 *     valid tree moves NEITHER the detection tallies NOR
 *     the diag emitted counter -- the audit never fires
 *     on a valid tree, and valid empty spans still
 *     release.
 *   1 drain: live_count corrupted to 0 under two live
 *     objects (after a flush settles the count, the
 *     host-true setup of the Z-067 probe). The drain
 *     refuses the span: 0 released, the span stays
 *     mapped, ONE detection tallies and ONE diag line
 *     emits (op=drain, cause=metadata). A second drain
 *     tallies COLLATERAL, not a second detection. After
 *     restoration the objects free cleanly and a later
 *     drain releases the span legitimately with no
 *     further tally.
 *   2 trim: the same refusal on the trim entry point
 *     (op=trim).
 *   3 free/retention: with the retention bound tuned to
 *     0, a cross-thread free that empties a span whose
 *     live_count was corrupted low is refused the
 *     retention release (span stays mapped, detection +
 *     emission on the free path, op=free); after the
 *     count is restored the last free releases the span
 *     whole (mincore proves the unmap).
 *   4 gate-off: the drain refusal and its tally happen
 *     with the diag gate closed and emitted stays 0 --
 *     tallies are a stats surface, emission is the gated
 *     channel (the Z-066 discipline).
 *   5 freehead-live (INC-J006-04, by design): free_head
 *     repointed at a live object on the span's CENTRAL
 *     free list (bin stock drained back first, the
 *     host-true setup). The next allocation returns the
 *     SAME live object -- the overlap handout happens
 *     with no tally, no emission and no refusal: the
 *     class preserves every count relation the allocator
 *     keeps, so no operational detection exists in the
 *     in-band free-list design; the verdict and its
 *     compensating controls are recorded in
 *     tests/perf/recorded_run_z13_z067.txt.
 *
 * Standalone: `make test-z067`; NOT part of `make test`
 * (the Z-064 standalone precedent).
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "mimir_alloc.h"
#include "mimir_stats_j002.h"
#include "mimir_diag_j007.h"
#include "mimir_integrity_j006.h"
#include "mimir_drain_h001.h"
#include "mimir_trim_i003.h"

static size_t PAGE;
static int checks;
static int failures;

#define REQ(cond) do { checks++; if (!(cond)) { \
    failures++; \
    printf("    REQ-FAIL %s:%d: %s\n", __func__, __LINE__, #cond); \
    fflush(stdout); } } while (0)

static uint64_t rd64(const void *b, size_t o)
{ uint64_t v; memcpy(&v, (const char *)b + o, 8); return v; }
static void wr64(void *b, size_t o, uint64_t v)
{ memcpy((char *)b + o, &v, 8); }
static void wrptr(void *b, size_t o, void *v)
{ memcpy((char *)b + o, &v, sizeof v); }
static void *span_of(const void *p)
{ return (void *)((uintptr_t)p & ~(uintptr_t)(PAGE - 1)); }
static int snap(struct mimir_stats *s)
{ return mimir_core_stats_snapshot(s); }
static int span_mapped(const void *S)
{ unsigned char vec = 0; return mincore((void *)S, PAGE, &vec) == 0; }

static int leg_control(void)
{
    struct mimir_stats s;
    int i;
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    for (i = 0; i < 8; i++) {
        void *b = mimir_malloc(20000);
        void *a = mimir_malloc(32);
        void *c = mimir_malloc(1024);
        REQ(b && a && c);
        memset(b, 0x5A, 20000);
        memset(a, 0xA5, 32);
        memset(c, 0x3C, 1024);
        mimir_free(a);
        mimir_free(c);
        mimir_free(b);
    }
    /* Valid drains/trims release valid empty spans and
     * move no detection counter. */
    (void)mimir_core_drain_retained();
    (void)mimir_core_trim_retained();
    REQ(snap(&s) == 0 && s.live_mappings == 0 &&
        s.live_small_objects == 0);
    REQ(mimir_core_live_release_detections() == 0);
    REQ(mimir_core_live_release_collateral_refusals() == 0);
    REQ(s.live_release_detections == 0 &&
        s.live_release_collateral_refusals == 0);
    REQ(mimir_core_diag_emitted() == 0);
    REQ(mimir_core_metadata_check() == 0);
    printf("    control: valid churn + drain + trim: "
           "detections=0 collateral=0 emitted=0\n");
    return failures ? 1 : 0;
}

static int leg_drain(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(32);
    void *b = mimir_malloc(32);
    void *S;
    REQ(a && b);
    S = span_of(a);
    REQ(span_of(b) == S);
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    /* Settle the count: the flush inside the checker
     * converts any TLS tally units into live_count. */
    REQ(mimir_core_metadata_check() == 0);
    REQ(rd64(S, 80) == 2);
    wr64(S, 80, 0); /* live_count = 0 while 2 objects live */
    REQ(mimir_core_metadata_check() > 0);
    REQ(mimir_core_drain_retained() == 0);
    REQ(span_mapped(S));
    REQ(mimir_core_live_release_detections() == 1);
    REQ(mimir_core_live_release_collateral_refusals() == 0);
    REQ(mimir_core_diag_emitted() == 1);
    /* Same episode again: collateral, not a detection. */
    REQ(mimir_core_drain_retained() == 0);
    REQ(span_mapped(S));
    REQ(mimir_core_live_release_detections() == 1);
    REQ(mimir_core_live_release_collateral_refusals() == 1);
    REQ(mimir_core_diag_emitted() == 2);
    wr64(S, 80, 2); /* restore the true count */
    REQ(mimir_core_metadata_check() == 0);
    mimir_free(a);
    mimir_free(b);
    /* The now-legitimately-empty span releases, and the
     * passing audit closes the episode without tallying. */
    REQ(mimir_core_drain_retained() >= 1);
    REQ(mimir_core_live_release_detections() == 1);
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    printf("    drain: refusal x2 (1 detection + 1 collateral, "
           "2 emissions), restore+free+drain releases clean\n");
    return failures ? 1 : 0;
}

static int leg_trim(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(32);
    void *b = mimir_malloc(32);
    void *S;
    REQ(a && b);
    S = span_of(a);
    REQ(span_of(b) == S);
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    REQ(mimir_core_metadata_check() == 0);
    REQ(rd64(S, 80) == 2);
    wr64(S, 80, 0);
    REQ(mimir_core_metadata_check() > 0);
    REQ(mimir_core_trim_retained() == 0);
    REQ(span_mapped(S));
    REQ(mimir_core_live_release_detections() == 1);
    REQ(mimir_core_diag_emitted() == 1);
    wr64(S, 80, 2);
    REQ(mimir_core_metadata_check() == 0);
    mimir_free(a);
    mimir_free(b);
    REQ(mimir_core_trim_retained() >= 1);
    REQ(mimir_core_live_release_detections() == 1);
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    printf("    trim: refusal (1 detection, 1 emission), "
           "restore+free+trim releases clean\n");
    return failures ? 1 : 0;
}

struct free_job { void *p; };
static void *free_worker(void *arg)
{
    mimir_free(((struct free_job *)arg)->p);
    return NULL;
}
static void free_from_worker(void *p)
{
    struct free_job job;
    pthread_t th;
    job.p = p;
    REQ(pthread_create(&th, NULL, free_worker, &job) == 0);
    REQ(pthread_join(th, NULL) == 0);
}

static int leg_free_retention(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(32);
    void *b = mimir_malloc(32);
    void *S;
    REQ(a && b);
    S = span_of(a);
    REQ(span_of(b) == S);
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    REQ(mimir_tune_set_retention_bound(0) == 0);
    REQ(mimir_tune_get_retention_bound() == 0);
    /* Central shape: bin stock back on the free list,
     * tally 0, live_count == 2 (the drain cannot release
     * the span: its objects are live). */
    REQ(mimir_core_drain_retained() == 0);
    REQ(mimir_core_metadata_check() == 0);
    REQ(rd64(S, 80) == 2);
    wr64(S, 80, 1); /* live_count lies: claims 1, truth 2 */
    free_from_worker(a); /* live 1 -> 0 on the corrupt count */
    REQ(span_mapped(S)); /* retention release REFUSED */
    REQ(mimir_core_live_release_detections() == 1);
    REQ(mimir_core_diag_emitted() == 1);
    wr64(S, 80, 1); /* restore truth: only b is live */
    REQ(mimir_core_metadata_check() == 0);
    free_from_worker(b); /* live 1 -> 0, audit passes */
    REQ(!span_mapped(S)); /* released whole this time */
    REQ(mimir_core_live_release_detections() == 1);
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    printf("    free/retention: cross-thread emptying free "
           "refused on the corrupt count (span mapped, "
           "detection + emission), restored free releases\n");
    return failures ? 1 : 0;
}

static int leg_gate_off(void)
{
    struct mimir_stats s;
    void *a = mimir_malloc(32);
    void *b = mimir_malloc(32);
    void *S;
    REQ(a && b);
    S = span_of(a);
    REQ(span_of(b) == S);
    (void)unsetenv("MIMIRALLOC_DIAG");
    REQ(mimir_core_metadata_check() == 0);
    wr64(S, 80, 0);
    REQ(mimir_core_drain_retained() == 0);
    REQ(span_mapped(S));
    REQ(mimir_core_live_release_detections() == 1);
    REQ(mimir_core_diag_emitted() == 0);
    wr64(S, 80, 2);
    mimir_free(a);
    mimir_free(b);
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    printf("    gate-off: refusal + tally=1 with emitted=0 "
           "(tally is a stats surface; emission is the "
           "gated channel)\n");
    return failures ? 1 : 0;
}

static int leg_freehead(void)
{
    void *a = mimir_malloc(32);
    void *b = mimir_malloc(32);
    void *S;
    void *c;
    int walk;
    REQ(a && b);
    S = span_of(a);
    REQ(span_of(b) == S);
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    mimir_free(b);
    /* Central shape first (the Z-067 probe setup): the
     * drain returns the bin stock to the central list. */
    REQ(mimir_core_drain_retained() == 0);
    REQ(mimir_core_metadata_check() == 0);
    wrptr(S, 72, a); /* free_head now names LIVE a */
    walk = mimir_core_metadata_check();
    printf("    checker on the live-named head: %d "
           "violation(s) (truncated node count only; no "
           "operational path consults the walk)\n", walk);
    ((unsigned char *)a)[0] = 0x11;
    c = mimir_malloc(32);
    REQ(c == a); /* the overlap handout itself */
    ((unsigned char *)c)[0] = 0x22;
    REQ(((unsigned char *)a)[0] == 0x22);
    REQ(mimir_core_live_release_detections() == 0);
    REQ(mimir_core_live_release_collateral_refusals() == 0);
    REQ(mimir_core_diag_emitted() == 0);
    printf("    freehead-live: overlap handout with zero "
           "operational detection -- INC-J006-04 "
           "undetectable by design (Z-067 record)\n");
    return failures ? 1 : 0;
}

struct leg { const char *name; int (*fn)(void); };
struct sweep_job { void *c; void *span; };
static void *sweep_worker(void *arg)
{
    struct sweep_job *j = arg;
    void *a = mimir_malloc(32);
    void *b = mimir_malloc(32);
    j->c = mimir_malloc(32);
    j->span = a ? span_of(a) : NULL;
    if (!a || !b || !j->c) {
        return NULL;
    }
    mimir_free(a);
    mimir_free(b);
    memset(j->c, 0x5A, 32);
    /* live_count lies: claims 0, truth 1 (j->c).  The
     * thread-exit drain's retention sweep is the release
     * path that would trust it. */
    wr64(j->span, 80, 0);
    return NULL;
}

static int leg_sweep(void)
{
    struct mimir_stats s;
    struct sweep_job job;
    pthread_t th;
    job.c = NULL;
    job.span = NULL;
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    REQ(mimir_tune_set_retention_bound(0) == 0);
    REQ(pthread_create(&th, NULL, sweep_worker, &job) == 0);
    REQ(pthread_join(th, NULL) == 0);
    REQ(job.c && job.span);
    /* Pre-wire, this exact shape released the span under
     * the live block at thread exit (scratch experiment
     * /tmp/exp67d.c on this host, span mapped=0).  Wired:
     * the sweep's audit refuses, tallies one detection,
     * and the drain-thread entry reports it (trim). */
    REQ(mimir_core_live_release_detections() == 1);
    REQ(mimir_core_diag_emitted() == 1);
    REQ(span_mapped(job.span));
    wr64(job.span, 80, 1); /* restore truth: only c live */
    REQ(mimir_core_metadata_check() == 0);
    mimir_free(job.c);
    REQ(!span_mapped(job.span)); /* released whole now */
    REQ(mimir_core_live_release_detections() == 1);
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    printf("    sweep: thread-exit retention sweep refused "
           "on the corrupt count (span mapped, detection + "
           "trim emission), restored free releases\n");
    return failures ? 1 : 0;
}

static const struct leg LEGS[] = {
    { "control",         leg_control },
    { "drain",           leg_drain },
    { "trim",            leg_trim },
    { "free-retention",  leg_free_retention },
    { "gate-off",        leg_gate_off },
    { "freehead-live",   leg_freehead },
    { "sweep",           leg_sweep },
};

int main(void)
{
    size_t i;
    int bad = 0;
    PAGE = (size_t)sysconf(_SC_PAGESIZE);
    printf("test_z067_livecount: Z-067 INC-J006-06 release-"
           "path detection (+ INC-J006-04 by-design leg), "
           "page=%zu, %zu legs, each in a disposable child\n",
           PAGE, sizeof LEGS / sizeof LEGS[0]);
    fflush(stdout);
    for (i = 0; i < sizeof LEGS / sizeof LEGS[0]; i++) {
        pid_t pid = fork();
        int status = 0;
        if (pid == 0) {
            int r = LEGS[i].fn();
            fflush(stdout);
            _exit(r ? 1 : 0);
        }
        REQ(pid > 0);
        REQ(waitpid(pid, &status, 0) == pid);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            bad++;
            printf("  [FAIL] leg %s (status=%d)\n",
                   LEGS[i].name, status);
        } else {
            printf("  [ok] leg %s\n", LEGS[i].name);
        }
        fflush(stdout);
    }
    printf("test_z067_livecount: legs=%zu failed=%d "
           "(parent checks=%d failures=%d)\n",
           sizeof LEGS / sizeof LEGS[0], bad, checks, failures);
    return (bad || failures) ? 1 : 0;
}
