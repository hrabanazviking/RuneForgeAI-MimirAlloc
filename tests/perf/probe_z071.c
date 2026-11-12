/* probe_z071.c -- Z-071 diagnostics default-on packet (L-07).
 *
 * One process = one cell. The probe never touches the
 * environment: the runner (run_z071.sh) sets the diag gate
 * posture for each process, so a cell is either one of the
 * K-002 oracle workloads (workload logic byte-identical to
 * bench_k002.c -- checksums must match bench_k002's for the
 * same workload) or one genuine-failure shape that reports
 * through mimir_core_diag_report when the gate is open.
 * Channel counters (emitted / suppressed) are read before
 * and after the cell and printed as exact deltas: emission
 * is an exact counted quantity, not a graded one.
 *
 * CSV line: leg,cell,ops_or_attempts,elapsed_ns,ns_per_op,
 *           checksum_or_nulls,emitted_delta,suppressed_delta
 * The probe prints nothing to stderr: stderr belongs to the
 * channel, and the runner counts its bytes as an exact
 * quantity. Errors exit with a code and a line on stdout.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mimir_alloc.h"
#include "../core/mimir_diag_j007.h"

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) +
           (uint64_t)ts.tv_nsec;
}

static unsigned char touch(void *p, size_t n, unsigned seed)
{
    unsigned char *b = (unsigned char *)p;
    b[0] = (unsigned char)(seed & 0xFFu);
    b[n - 1] = (unsigned char)((seed >> 3) & 0xFFu);
    return (unsigned char)(b[0] ^ b[n - 1]);
}

/* Size tables copied verbatim from bench_k002.c. */
static const size_t small_sizes[] = {
    16, 32, 48, 64, 96, 128, 192, 256
};
static const size_t mixed_sizes[] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};
static const size_t large_sizes[] = {
    2048, 8192, 16384, 65536, 262144, 1048576
};

typedef uint64_t (*workload_t)(long, uint64_t *);

static uint64_t w_small_churn(long iters, uint64_t *ops_out)
{
    uint64_t chk = 0;
    long i;
    for (i = 0; i < iters; i++) {
        size_t n = small_sizes[i % 8];
        void *p = mimir_malloc(n);
        if (!p) { printf("PROBE_ERROR small_churn null\n"); exit(1); }
        chk += touch(p, n, (unsigned)i);
        mimir_free(p);
    }
    *ops_out = (uint64_t)iters;
    return chk;
}

#define BATCH 256
static uint64_t w_mixed_batch(long batches, uint64_t *ops_out)
{
    static void *ptrs[BATCH];
    uint64_t chk = 0;
    long b;
    int i;
    for (b = 0; b < batches; b++) {
        for (i = 0; i < BATCH; i++) {
            size_t n = mixed_sizes[(i + (int)b) % 20];
            ptrs[i] = mimir_malloc(n);
            if (!ptrs[i]) { printf("PROBE_ERROR mixed_batch null\n"); exit(1); }
            chk += touch(ptrs[i], n, (unsigned)(b + i));
        }
        for (i = BATCH - 1; i >= 0; i--) mimir_free(ptrs[i]);
    }
    *ops_out = (uint64_t)batches * BATCH;
    return chk;
}

#define LBATCH 32
static uint64_t w_large_batch(long batches, uint64_t *ops_out)
{
    static void *ptrs[LBATCH];
    uint64_t chk = 0;
    long b;
    int i;
    for (b = 0; b < batches; b++) {
        for (i = 0; i < LBATCH; i++) {
            size_t n = large_sizes[(i + (int)b) % 6];
            ptrs[i] = mimir_malloc(n);
            if (!ptrs[i]) { printf("PROBE_ERROR large_batch null\n"); exit(1); }
            chk += touch(ptrs[i], n, (unsigned)(b + i));
        }
        for (i = 0; i < LBATCH; i++) mimir_free(ptrs[i]);
    }
    *ops_out = (uint64_t)batches * LBATCH;
    return chk;
}

#define MBATCH 512
static uint64_t w_mixed_lifecycle(long rounds, uint64_t *ops_out)
{
    static void *ptrs[MBATCH];
    static size_t sizes[MBATCH];
    uint64_t chk = 0;
    long r;
    int i;
    for (i = 0; i < MBATCH; i++) {
        sizes[i] = (i % 8 == 7) ? large_sizes[i % 3]
                                : mixed_sizes[i % 20];
    }
    for (r = 0; r < rounds; r++) {
        for (i = 0; i < MBATCH; i++) {
            ptrs[i] = mimir_malloc(sizes[i]);
            if (!ptrs[i]) { printf("PROBE_ERROR lifecycle null\n"); exit(1); }
            chk += touch(ptrs[i], sizes[i], (unsigned)(r + i));
        }
        for (i = 1; i < MBATCH; i += 2) { mimir_free(ptrs[i]); ptrs[i] = NULL; }
        for (i = 1; i < MBATCH; i += 2) {
            ptrs[i] = mimir_malloc(sizes[i]);
            if (!ptrs[i]) { printf("PROBE_ERROR lifecycle refill null\n"); exit(1); }
            chk += touch(ptrs[i], sizes[i], (unsigned)(r + i + 1));
        }
        for (i = 0; i < MBATCH; i++) mimir_free(ptrs[i]);
    }
    *ops_out = (uint64_t)rounds * 768;
    return chk;
}

/* Genuine-failure shapes. Each attempt must return NULL;
 * a non-NULL return is counted separately (field nulls
 * would then disagree with attempts, which the summariser
 * treats as a broken cell, never as data). Whether a shape
 * emits is MEASURED (emitted_delta), never assumed -- the
 * header's posture list (docs/PHASE_10_DIAGNOSTICS.md S2)
 * says posture NULLs emit nothing, and this packet records
 * each shape's behaviour as found. */
typedef void *(*fail_t)(void);

static void *f_malloc_huge(void) { return mimir_malloc(SIZE_MAX); }
static void *f_calloc_overflow(void)
{
    return mimir_calloc(SIZE_MAX / 2 + 1, 4);
}
static void *f_malloc_backing(void)
{
    return mimir_malloc((size_t)1 << 58);
}

struct workload { const char *name; workload_t run; long iters; };
struct failshape { const char *name; fail_t run; };

static const struct workload workloads[] = {
    { "small_churn",     w_small_churn,     100000 },
    { "mixed_batch",     w_mixed_batch,        1000 },
    { "large_batch",     w_large_batch,         300 },
    { "mixed_lifecycle", w_mixed_lifecycle,     400 },
};
static const struct failshape failshapes[] = {
    { "fail_malloc_huge",     f_malloc_huge },
    { "fail_calloc_overflow", f_calloc_overflow },
    { "fail_malloc_backing",  f_malloc_backing },
};

int main(int argc, char **argv)
{
    const char *leg;
    const char *cell;
    size_t i;
    if (argc < 3) {
        printf("PROBE_ERROR usage: probe_z071 <leg> <cell> [count]\n");
        return 2;
    }
    leg = argv[1];
    cell = argv[2];
    for (i = 0; i < sizeof(workloads) / sizeof(workloads[0]); i++) {
        if (strcmp(cell, workloads[i].name) == 0) {
            long iters = workloads[i].iters;
            uint64_t ops = 0, chk, t0, t1;
            uint64_t e0, e1, s0, s1, dummy = 0;
            if (argc >= 4) iters = atol(argv[3]);
            e0 = mimir_core_diag_emitted();
            s0 = mimir_core_diag_suppressed();
            (void)workloads[i].run(iters / 10, &dummy);
            t0 = now_ns();
            chk = workloads[i].run(iters, &ops);
            t1 = now_ns();
            e1 = mimir_core_diag_emitted();
            s1 = mimir_core_diag_suppressed();
            printf("%s,%s,%llu,%llu,%.2f,%llu,%llu,%llu\n", leg, cell,
                   (unsigned long long)ops,
                   (unsigned long long)(t1 - t0),
                   (double)(t1 - t0) / (double)ops,
                   (unsigned long long)chk,
                   (unsigned long long)(e1 - e0),
                   (unsigned long long)(s1 - s0));
            return 0;
        }
    }
    for (i = 0; i < sizeof(failshapes) / sizeof(failshapes[0]); i++) {
        if (strcmp(cell, failshapes[i].name) == 0) {
            long attempts = 2000, a;
            uint64_t nulls = 0, nonnull = 0, t0, t1;
            uint64_t e0, e1, s0, s1;
            if (argc >= 4) attempts = atol(argv[3]);
            for (a = 0; a < attempts / 10; a++) {
                void *p = failshapes[i].run();
                if (p) { mimir_free(p); }
            }
            e0 = mimir_core_diag_emitted();
            s0 = mimir_core_diag_suppressed();
            t0 = now_ns();
            for (a = 0; a < attempts; a++) {
                void *p = failshapes[i].run();
                if (p) { nonnull++; mimir_free(p); } else { nulls++; }
            }
            t1 = now_ns();
            e1 = mimir_core_diag_emitted();
            s1 = mimir_core_diag_suppressed();
            printf("%s,%s,%llu,%llu,%.2f,%llu,%llu,%llu\n", leg, cell,
                   (unsigned long long)attempts,
                   (unsigned long long)(t1 - t0),
                   (double)(t1 - t0) / (double)attempts,
                   (unsigned long long)nulls,
                   (unsigned long long)(e1 - e0),
                   (unsigned long long)(s1 - s0));
            if (nonnull != 0) {
                printf("PROBE_ERROR %s nonnull=%llu\n", cell,
                       (unsigned long long)nonnull);
                return 3;
            }
            return 0;
        }
    }
    printf("PROBE_ERROR unknown cell %s\n", cell);
    return 2;
}
