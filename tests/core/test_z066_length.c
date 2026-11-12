/* test_z066_length.c -- Z-066 (D-12) INC-J006-02
 * consistent-length detection evidence
 * (docs/MIMIRALLOC_Z13_ROADMAP.md slice Z-066; the
 * 25-probe flip itself lives in tests/core/test_j006.c).
 *
 * Legs, each in a forked child so corruption and counter
 * baselines never leak between them:
 *   0 control: valid large/small churn with the diag
 *     gate open moves NEITHER the detection tallies NOR
 *     the diag emitted counter -- detection never fires
 *     on a valid tree.
 *   1 descriptor: consistent length growth on a live
 *     mapping block. The first free is refused (block
 *     stays LIVE, Q-RELEASED unmoved) and tallies ONE
 *     detection episode + one diag emission; a second
 *     free of a DIFFERENT valid block during the same
 *     episode is refused as COLLATERAL (fail-closed
 *     registry-wide posture) and tallies collateral,
 *     not a second detection, while still emitting;
 *     after restoration both blocks free cleanly and no
 *     further tally moves.
 *   2 span: consistent length doubling on a live small
 *     span. A cross-thread free is refused (a
 *     same-thread free would bin under Z-029 without
 *     consulting the central finder -- the banked
 *     posture for every corruption class), tallies one
 *     detection + one emission; across 400 further
 *     allocations the corrupt span is never carved or
 *     refilled from (serves from it are exactly its
 *     pre-corruption TLS bin stock, all in-page);
 *     restore + free is clean and the walk returns 0.
 *   3 compensating pair (the recorded boundary of the
 *     mechanism): two live descriptors corrupted by
 *     +1 page and -1 page (the shrunken one also
 *     lowering requested_size, without which the J-006
 *     relations catch it alone) keep the registry byte
 *     sum equal, so the audit passes and both frees succeed
 *     with NO tally movement. This leg proves the
 *     limitation the slice records -- it is expected
 *     behaviour of a byte-sum audit, not a defect
 *     discovered later. The child exits without further
 *     allocator use; the short release of the shrunken
 *     descriptor dies with the child.
 *
 * Standalone: `make test-z066`; NOT part of `make test`
 * (the Z-064 standalone precedent).
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "mimir_alloc.h"
#include "mimir_stats_j002.h"
#include "mimir_diag_j007.h"
#include "mimir_integrity_j006.h"

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
static void *desc_of(const void *user)
{ return (void *)((const char *)user - PAGE); }
static void *span_of(const void *p)
{ return (void *)((uintptr_t)p & ~(uintptr_t)(PAGE - 1)); }
static int snap(struct mimir_stats *s)
{ return mimir_core_stats_snapshot(s); }

static int leg_control(void)
{
    struct mimir_stats s;
    int i;
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    for (i = 0; i < 8; i++) {
        void *b = mimir_malloc(20000);
        void *a = mimir_malloc(32);
        REQ(b && a);
        memset(b, 0x5A, 20000);
        memset(a, 0xA5, 32);
        mimir_free(a);
        mimir_free(b);
    }
    REQ(snap(&s) == 0 && s.live_mappings == 0 &&
        s.live_small_objects == 0);
    REQ(mimir_core_metadata_detections() == 0);
    REQ(mimir_core_metadata_collateral_refusals() == 0);
    REQ(mimir_core_diag_emitted() == 0);
    REQ(mimir_core_metadata_check() == 0);
    printf("    control: valid churn, diag gate open: "
           "detections=0 collateral=0 emitted=0\n");
    return failures ? 1 : 0;
}

static int leg_descriptor(void)
{
    struct mimir_stats s0, s1;
    void *b = mimir_malloc(20000);
    void *c = mimir_malloc(20000);
    void *d;
    uint64_t L, U;
    REQ(b && c);
    d = desc_of(b);
    L = rd64(d, 16); U = rd64(d, 40);
    REQ(L >= 2 * (uint64_t)PAGE &&
        rd64(d, 24) == (uint64_t)PAGE && U == L - (uint64_t)PAGE);
    REQ(snap(&s0) == 0 && s0.live_mappings == 2);
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    wr64(d, 16, L + (uint64_t)PAGE);
    wr64(d, 40, U + (uint64_t)PAGE);
    REQ(mimir_core_metadata_check() > 0);
    mimir_free(b);              /* refused: episode opens */
    REQ(snap(&s1) == 0 && s1.live_mappings == 2);
    REQ(s1.bytes_released == s0.bytes_released);
    REQ(mimir_core_metadata_detections() == 1);
    REQ(mimir_core_metadata_collateral_refusals() == 0);
    REQ(mimir_core_diag_emitted() == 1);
    mimir_free(c);              /* valid block, same episode:
                                 * refused as collateral */
    REQ(snap(&s1) == 0 && s1.live_mappings == 2);
    REQ(mimir_core_metadata_detections() == 1);
    REQ(mimir_core_metadata_collateral_refusals() == 1);
    REQ(mimir_core_diag_emitted() == 2);
    wr64(d, 16, L); wr64(d, 40, U);
    mimir_free(b);
    mimir_free(c);
    REQ(snap(&s1) == 0 && s1.live_mappings == 0);
    REQ(mimir_core_metadata_detections() == 1);
    REQ(mimir_core_metadata_collateral_refusals() == 1);
    REQ(mimir_core_metadata_check() == 0);
    printf("    descriptor: episode tally=1, collateral=1, "
           "emitted=2, restore+free clean\n");
    return failures ? 1 : 0;
}

struct free_arg { void *ptr; };
static void *free_thread(void *arg)
{
    mimir_free(((struct free_arg *)arg)->ptr);
    return NULL;
}

static int leg_span(void)
{
    void *arr[400];
    void *a = mimir_malloc(16);
    void *S;
    struct mimir_stats s;
    struct free_arg arg;
    pthread_t th;
    int i, from_s = 0;
    REQ(a);
    S = span_of(a);
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    wr64(S, 16, 2 * (uint64_t)PAGE);
    wr64(S, 40, 2 * (uint64_t)PAGE - 96);
    REQ(mimir_core_metadata_check() > 0);
    arg.ptr = a;
    REQ(pthread_create(&th, NULL, free_thread, &arg) == 0);
    REQ(pthread_join(th, NULL) == 0);
    REQ(snap(&s) == 0 && s.live_small_objects == 1);
    REQ(mimir_core_metadata_detections() == 1);
    REQ(mimir_core_metadata_collateral_refusals() == 0);
    REQ(mimir_core_diag_emitted() == 1);
    for (i = 0; i < 400; i++) {
        void *p = mimir_malloc(16);
        REQ(p);
        arr[i] = p;
        if (span_of(p) == S) {
            from_s++;
            REQ((uintptr_t)p < (uintptr_t)S + PAGE);
        }
    }
    /* Serves from S after corruption are exactly the
     * objects carved BEFORE it and already sitting in
     * this thread's TLS bin (capacity 16, Z-028 contract):
     * in-page, previously published, never re-validated
     * on the bin path -- the recorded Z-029 posture for
     * every corruption class. What must never happen is
     * a NEW carve from the corrupt span (the per-pointer
     * REQ above bounds every from-S serve to the real
     * page) or a refill from it: with 400 allocations
     * the bin drains many times over, so a from-S count
     * above the bin capacity would prove a refill or a
     * carve. */
    REQ(from_s >= 1 && from_s <= 16);
    wr64(S, 16, (uint64_t)PAGE);
    wr64(S, 40, (uint64_t)PAGE - 96);
    mimir_free(a);
    for (i = 0; i < 400; i++) {
        mimir_free(arr[i]);
    }
    REQ(snap(&s) == 0 && s.live_small_objects == 0);
    REQ(mimir_core_metadata_check() == 0);
    printf("    span: cross-thread free refused, tally=1, "
           "emitted=1, %d of 400 served from S (pre-corruption "
           "TLS bin stock only, all in-page)\n", from_s);
    return failures ? 1 : 0;
}

static int leg_compensating(void)
{
    struct mimir_stats s;
    void *b = mimir_malloc(20000);
    void *c = mimir_malloc(20000);
    void *db, *dc;
    uint64_t Lb, Ub, Lc, Uc;
    REQ(b && c);
    db = desc_of(b); dc = desc_of(c);
    Lb = rd64(db, 16); Ub = rd64(db, 40);
    Lc = rd64(dc, 16); Uc = rd64(dc, 40);
    REQ(Lb >= 2 * (uint64_t)PAGE && Lc >= 2 * (uint64_t)PAGE);
    (void)setenv("MIMIRALLOC_DIAG", "1", 1);
    wr64(db, 16, Lb + (uint64_t)PAGE);
    wr64(db, 40, Ub + (uint64_t)PAGE);
    wr64(dc, 16, Lc - (uint64_t)PAGE);
    wr64(dc, 40, Uc - (uint64_t)PAGE);
    /* The shrink side must ALSO lower requested_size:
     * usable is always the minimal page count covering
     * the request, so a bare -1 page shrink breaks the
     * J-006 relation requested <= usable and is caught
     * by descriptor_is_live itself. With requested
     * lowered too, the pair is fully relation-consistent
     * and only the aggregate sum could catch it. */
    wr64(dc, 32, 1);
    /* The byte sum is unchanged: the audit passes, both
     * frees succeed, no tally moves. Recorded boundary
     * of the mechanism (see the recorded run). */
    mimir_free(b);
    mimir_free(c);
    REQ(snap(&s) == 0 && s.live_mappings == 0);
    REQ(mimir_core_metadata_detections() == 0);
    REQ(mimir_core_metadata_collateral_refusals() == 0);
    printf("    compensating pair (+1P/-1P): NOT detected "
           "by design of a byte-sum audit -- frees succeed, "
           "tally=0 (recorded boundary)\n");
    return failures ? 1 : 0;
}


static int leg_gate_off(void)
{
    struct mimir_stats s;
    void *b = mimir_malloc(20000);
    void *d;
    uint64_t L, U;
    REQ(b);
    d = desc_of(b);
    L = rd64(d, 16); U = rd64(d, 40);
    (void)unsetenv("MIMIRALLOC_DIAG");
    wr64(d, 16, L + (uint64_t)PAGE);
    wr64(d, 40, U + (uint64_t)PAGE);
    mimir_free(b);              /* refused; tally moves,
                                 * emission stays gated off */
    REQ(snap(&s) == 0 && s.live_mappings == 1);
    REQ(mimir_core_metadata_detections() == 1);
    REQ(mimir_core_diag_emitted() == 0);
    wr64(d, 16, L); wr64(d, 40, U);
    mimir_free(b);
    REQ(snap(&s) == 0 && s.live_mappings == 0);
    printf("    gate-off: refusal + tally=1 with emitted=0 "
           "(the tally is a stats surface; emission is the "
           "gated channel)\n");
    return failures ? 1 : 0;
}

struct leg { const char *name; int (*fn)(void); };
static const struct leg LEGS[] = {
    { "control",      leg_control },
    { "descriptor",   leg_descriptor },
    { "span",         leg_span },
    { "compensating", leg_compensating },
    { "gate-off",     leg_gate_off },
};

int main(void)
{
    size_t i;
    int bad = 0;
    PAGE = (size_t)sysconf(_SC_PAGESIZE);
    printf("test_z066_length: Z-066 INC-J006-02 detection, "
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
    printf("test_z066_length: legs=%zu failed=%d "
           "(parent checks=%d failures=%d)\n",
           sizeof LEGS / sizeof LEGS[0], bad, checks, failures);
    return (bad || failures) ? 1 : 0;
}
