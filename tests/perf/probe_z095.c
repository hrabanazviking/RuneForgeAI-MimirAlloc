/* probe_z095.c - Z-095 trace replay probe (MimirAlloc Z13).
 *
 * Replays a converted allocation trace (convert_z095.py
 * output) against one allocator leg and times the replay.
 * ONE source, TWO binaries, bench_k002.c's pattern:
 *   -DLEG_GLIBC : stdlib malloc/free/realloc
 *   (default)   : mimir_malloc/mimir_free/mimir_realloc from
 *                 the core objects it is linked with (which
 *                 may be a Z-095 variant build: the runner
 *                 links scratch objects with retuned TLS
 *                 constants / class table and says so)
 *
 * Trace format (text, one op per line):
 *   A <size>            allocate; becomes the next live index
 *   F <idx>             free live block <idx>
 *   R <idx> <newsize>   realloc live block <idx>
 * Live indices are assigned in allocation order by the
 * converter; an index is never reused within a trace.
 *
 * Method (Z-004 protocol shape): a warm-up replay at 1/10
 * scale (the trace prefix, untimed), then ONE timed full
 * replay in this process; repetitions are separate processes
 * driven by run_z095.sh (taskset-pinned, legs interleaved,
 * median taken by summarize_z095.py).
 *
 * Every block is pattern-written on allocation (first, last
 * and one middle byte) and the bytes are verified before
 * free/realloc; the checksum folds sizes and verified bytes
 * so the two legs on the same trace MUST agree, and a silent
 * no-op or corruption shows as a mismatch or a hard FAIL.
 * Zero-size allocations never appear in a converted trace
 * (the converter drops them: MimirAlloc's D-013 contract
 * refuses them by design, so they are not replayable events;
 * the converter counts them).
 *
 * Prints one CSV line:
 *   leg,trace,ops,elapsed_ns,ns_per_op,checksum,peak_live_bytes
 * Exit 0 iff the replay completed with every pattern check
 * passing and no NULL from a non-zero allocation.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef LEG_GLIBC
static void *(*alloc_fn)(size_t) = malloc;
static void (*free_fn)(void *) = free;
static void *(*realloc_fn)(void *, size_t) = realloc;
static const char *leg_name = "glibc";
#else
#include "mimir_alloc.h"
static void *(*alloc_fn)(size_t) = mimir_malloc;
static void (*free_fn)(void *) = mimir_free;
static void *(*realloc_fn)(void *, size_t) = mimir_realloc;
static const char *leg_name = "mimiralloc";
#endif

struct op { char kind; uint32_t idx; uint64_t size; };

static uint64_t fnv(uint64_t h, uint64_t v)
{
    int i;
    for (i = 0; i < 8; i++) {
        h ^= (v >> (i * 8)) & 0xffu;
        h *= UINT64_C(1099511628211);
    }
    return h;
}

static void pat_write(unsigned char *p, size_t n, uint64_t tag)
{
    if (n == 0) return;
    p[0] = (unsigned char)tag;
    if (n > 1) p[n - 1] = (unsigned char)(tag >> 8);
    if (n > 2) p[n / 2] = (unsigned char)(tag >> 16);
}

static int pat_check(const unsigned char *p, size_t n, uint64_t tag)
{
    if (n == 0) return 1;
    if (p[0] != (unsigned char)tag) return 0;
    if (n > 1 && p[n - 1] != (unsigned char)(tag >> 8)) return 0;
    if (n > 2 && p[n / 2] != (unsigned char)(tag >> 16)) return 0;
    return 1;
}

/* Run nops ops from ops[]. Returns checksum; *bad set on any
 * failure. Live arrays are caller-owned scratch. */
static uint64_t replay(const struct op *ops, size_t nops,
                       void **live, uint64_t *lsz, int *bad)
{
    uint64_t h = UINT64_C(1469598103934665603);
    size_t i;
    *bad = 0;
    for (i = 0; i < nops; i++) {
        char k = ops[i].kind;
        if (k == 'A') {
            void *p = alloc_fn(ops[i].size);
            if (!p && ops[i].size) { *bad = 1; return h; }
            live[ops[i].idx] = p;
            lsz[ops[i].idx] = ops[i].size;
            pat_write(p, ops[i].size, ops[i].idx + 1);
            h = fnv(h, ops[i].size);
        } else if (k == 'F') {
            void *p = live[ops[i].idx];
            if (p) {
                if (!pat_check(p, lsz[ops[i].idx], ops[i].idx + 1)) { *bad = 1; return h; }
                h = fnv(h, lsz[ops[i].idx] ^ UINT64_C(0xf17e));
                free_fn(p);
                live[ops[i].idx] = NULL;
                lsz[ops[i].idx] = 0;
            }
        } else if (k == 'R') {
            void *p = live[ops[i].idx];
            void *q;
            if (p && !pat_check(p, lsz[ops[i].idx], ops[i].idx + 1)) { *bad = 1; return h; }
            q = realloc_fn(p, ops[i].size);
            if (!q && ops[i].size) { *bad = 1; return h; }
            live[ops[i].idx] = q;
            lsz[ops[i].idx] = ops[i].size;
            pat_write(q, ops[i].size, ops[i].idx + 1);
            h = fnv(h, ops[i].size ^ UINT64_C(0xee11));
        }
    }
    return h;
}

int main(int argc, char **argv)
{
    FILE *f;
    struct op *ops = NULL;
    size_t nops = 0, cap = 0, maxidx = 0, i;
    void **live;
    uint64_t *lsz;
    char line[128];
    const char *tname;
    struct timespec t0, t1;
    uint64_t chk, peak = 0, cur = 0;
    int bad;
    long el;

    if (argc != 2) { fprintf(stderr, "usage: probe_z095 <trace>\n"); return 2; }
    tname = argv[1];
    f = fopen(tname, "r");
    if (!f) { fprintf(stderr, "cannot open trace\n"); return 2; }
    while (fgets(line, sizeof line, f)) {
        struct op o;
        if (line[0] == 'A') {
            o.kind = 'A'; o.idx = 0;
            o.size = strtoull(line + 2, NULL, 10);
            o.idx = (uint32_t)maxidx++;
        } else if (line[0] == 'F') {
            o.kind = 'F'; o.size = 0;
            o.idx = (uint32_t)strtoul(line + 2, NULL, 10);
        } else if (line[0] == 'R') {
            char *sp;
            o.kind = 'R';
            o.idx = (uint32_t)strtoul(line + 2, &sp, 10);
            o.size = strtoull(sp, NULL, 10);
        } else {
            continue;
        }
        if (nops == cap) {
            cap = cap ? cap * 2 : 65536;
            ops = realloc(ops, cap * sizeof *ops);
            if (!ops) return 2;
        }
        ops[nops++] = o;
    }
    fclose(f);

    live = calloc(maxidx ? maxidx : 1, sizeof(void *));
    lsz = calloc(maxidx ? maxidx : 1, sizeof(uint64_t));
    if (!live || !lsz) return 2;

    /* Warm-up: prefix replay at 1/10 scale, untimed, then a
     * full free sweep of whatever it left live. */
    replay(ops, nops / 10, live, lsz, &bad);
    if (bad) { fprintf(stderr, "FAIL warmup\n"); return 1; }
    for (i = 0; i < maxidx; i++) {
        if (live[i]) { free_fn(live[i]); live[i] = NULL; lsz[i] = 0; }
    }

    /* Peak live bytes from the op stream (leg-independent). */
    {
        uint64_t *sim = calloc(maxidx ? maxidx : 1, sizeof(uint64_t));
        if (!sim) return 2;
        for (i = 0; i < nops; i++) {
            if (ops[i].kind == 'A') { sim[ops[i].idx] = ops[i].size; cur += ops[i].size; }
            else if (ops[i].kind == 'F') { cur -= sim[ops[i].idx]; sim[ops[i].idx] = 0; }
            else { cur = cur - sim[ops[i].idx] + ops[i].size; sim[ops[i].idx] = ops[i].size; }
            if (cur > peak) peak = cur;
        }
        free(sim);
    }

    clock_gettime(CLOCK_MONOTONIC, &t0);
    chk = replay(ops, nops, live, lsz, &bad);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    if (bad) { fprintf(stderr, "FAIL replay pattern/null\n"); return 1; }
    el = (long)(t1.tv_sec - t0.tv_sec) * 1000000000L + (t1.tv_nsec - t0.tv_nsec);
    printf("%s,%s,%zu,%ld,%.2f,%llu,%llu\n", leg_name, tname, nops, el,
           nops ? (double)el / (double)nops : 0.0,
           (unsigned long long)chk, (unsigned long long)peak);
    for (i = 0; i < maxidx; i++) if (live[i]) free_fn(live[i]);
    free(live); free(lsz); free(ops);
    return 0;
}
