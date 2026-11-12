/* probe_z034.c - Z-034 metadata diet probe.
 * Built twice by tests/perf/run_z034.sh (the Z-019 pattern):
 * against the production core and against the trial core
 * (-DMIMIR_OOL_META). One source, three modes:
 *
 *   table           Per-class header-cost audit: capacity of
 *                   one span verified EMPIRICALLY through the
 *                   J-002 stats surface (span count steps at
 *                   capacity+1 live blocks), span-header
 *                   amortisation printed per class.
 *   churn <bytes>   Small-class alloc/free timing: LIFO
 *                   single-block churn + 256-block batch
 *                   churn, clock_gettime(CLOCK_MONOTONIC),
 *                   checksum over touched bytes printed so
 *                   the two legs' work is provably equal.
 *   pattern <bytes> Out-of-line proof: fill a block with a
 *                   pattern, free it, re-allocate the class;
 *                   report whether the first 8 bytes of the
 *                   re-allocated block still hold the pattern
 *                   (prod: overwritten by the free-list link;
 *                   trial: untouched for classes 16..64).
 * Exit 0 on success in every mode. */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mimir_alloc.h"
#include "../../tests/core/mimir_stats_j002.h"

#define NCLASS 20
static const size_t classes[NCLASS] = {
    16, 32, 48, 64, 80, 96, 112, 128, 160, 192,
    224, 256, 320, 384, 448, 512, 640, 768, 896, 1024
};
#define PAGE_BYTES 4096u
#define HDR_BYTES 96u

static void *ptrs[4200];
static volatile unsigned long sink;

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static int mode_table(void)
{
    int bad = 0;
    size_t ci;

    printf("class cap_emp cap_formula hdr_per_blk_B tail_waste_B "
           "span_eff_pct\n");
    for (ci = 0; ci < NCLASS; ci++) {
        size_t cs = classes[ci];
        size_t formula = (PAGE_BYTES - HDR_BYTES) / cs;
        struct mimir_stats before, mid, after;
        size_t i, cap_emp = 0;

        if (mimir_core_stats_snapshot(&before) != 0) return 1;
        /* Allocate until the class gains a second span. */
        for (i = 0; i < formula + 2 && i < 4200; i++) {
            ptrs[i] = mimir_malloc(cs);
            if (ptrs[i] == NULL) { bad = 1; break; }
            memset(ptrs[i], 0x5A, cs);
            if (mimir_core_stats_snapshot(&mid) != 0) return 1;
            if (mid.span_total_spans[ci] >
                before.span_total_spans[ci] + 1) {
                cap_emp = i; /* i blocks live, span #2 at i+1 */
                break;
            }
        }
        if (cap_emp == 0) cap_emp = formula + 1; /* never stepped */
        {
            size_t live = (cap_emp == formula + 1) ? formula + 1
                                                   : cap_emp + 1;
            size_t k;
            for (k = 0; k < live; k++) mimir_free(ptrs[k]);
        }
        if (mimir_core_stats_snapshot(&after) != 0) return 1;
        if (after.live_small_objects != 0) {
            printf("LEAK class %zu: %llu small objects live after "
                   "free-all\n", cs,
                   (unsigned long long)after.live_small_objects);
            bad = 1;
        }
        {
            double hdr = (double)HDR_BYTES / (double)formula;
            size_t tail = PAGE_BYTES - HDR_BYTES - formula * cs;
            double eff = 100.0 * (double)(formula * cs) /
                         (double)PAGE_BYTES;
            printf("%zu %zu %zu %.3f %zu %.2f\n", cs, cap_emp,
                   formula, hdr, tail, eff);
            if (cap_emp != formula) {
                printf("MISMATCH class %zu: empirical %zu != "
                       "formula %zu\n", cs, cap_emp, formula);
                bad = 1;
            }
        }
    }
    printf("table_result: %s\n", bad ? "FAIL" : "OK");
    return bad;
}

static int mode_churn(size_t cs)
{
    /* LIFO single-block churn. */
    const long iters = 200000;
    long i;
    unsigned long chk = 0;
    double t0, t1;
    void *warm = mimir_malloc(cs);
    if (warm == NULL) return 1;
    mimir_free(warm);

    t0 = now_ns();
    for (i = 0; i < iters; i++) {
        unsigned char *p = (unsigned char *)mimir_malloc(cs);
        if (p == NULL) return 1;
        p[0] = (unsigned char)i;
        p[cs - 1] = (unsigned char)(i >> 8);
        chk += p[0] + p[cs - 1];
        mimir_free(p);
    }
    t1 = now_ns();
    printf("churn1 class=%zu ns_per_op=%.2f checksum=%lu\n", cs,
           (t1 - t0) / (double)iters, chk);

    /* Batch churn: 256 live, freed in reverse order. */
    {
        const long rounds = 2000;
        const int batch = 256;
        long r;
        int k;
        chk = 0;
        t0 = now_ns();
        for (r = 0; r < rounds; r++) {
            for (k = 0; k < batch; k++) {
                unsigned char *p =
                    (unsigned char *)mimir_malloc(cs);
                if (p == NULL) return 1;
                p[0] = (unsigned char)(k + r);
                ptrs[k] = p;
            }
            for (k = batch - 1; k >= 0; k--) {
                unsigned char *p = (unsigned char *)ptrs[k];
                chk += p[0];
                mimir_free(p);
            }
        }
        t1 = now_ns();
        printf("churn256 class=%zu ns_per_op=%.2f checksum=%lu\n",
               cs, (t1 - t0) / (double)(rounds * batch), chk);
    }
    sink = chk;
    return 0;
}

static int mode_pattern(size_t cs)
{
    unsigned char pat[8];
    unsigned char *p;
    int k;

    for (k = 0; k < 8; k++) pat[k] = (unsigned char)(0xA0 + k);
    p = (unsigned char *)mimir_malloc(cs);
    if (p == NULL) return 1;
    memcpy(p, pat, 8);
    memset(p + 8, 0x3C, cs - 8);
    mimir_free(p);
    {
        unsigned char *q = (unsigned char *)mimir_malloc(cs);
        int same;
        if (q == NULL) return 1;
        same = memcmp(q, pat, 8) == 0;
        printf("pattern class=%zu first8_%s\n", cs,
               same ? "PRESERVED" : "OVERWRITTEN");
        mimir_free(q);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "table") == 0) {
        return mode_table();
    }
    if (argc >= 3 && strcmp(argv[1], "churn") == 0) {
        return mode_churn((size_t)atol(argv[2]));
    }
    if (argc >= 3 && strcmp(argv[1], "pattern") == 0) {
        return mode_pattern((size_t)atol(argv[2]));
    }
    fprintf(stderr, "usage: probe_z034 table|churn <B>|pattern <B>\n");
    return 2;
}
