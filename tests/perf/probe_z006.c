/* probe_z006.c - Z-006 RSS/PSS probe.
 *
 * Per-workload driver around the BANKED K-002 harness.
 * bench_k002.c is included UNMODIFIED (its main() renamed
 * away below): the workload functions, size tables, op
 * definition (one allocation plus its matching free) and
 * checksums are exactly the banked ones - this probe adds
 * no workload code of its own.
 *
 * Purpose (Z-006): bench_k002 runs each workload flat-out
 * and frees as it goes, so an external sampler cannot hit
 * defined memory states. This probe runs ONE workload per
 * process (argv[1]) and stops at DEFINED PHASES, printing
 * a marker line "PHASE <name>" to stdout (flushed) and
 * blocking on one line from stdin, while the runner
 * (run_rss_z006.py) reads /proc/<pid>/smaps_rollup.
 * Phases, in order:
 *   START         process up, before any workload call
 *   WARM          after the 1/10-scale warm-up pass
 *                 (exactly bench_k002's warm-up)
 *   STEADY        after half of the R timed passes
 *   PEAK          characteristic live set allocated and
 *                 fully memset (every page touched, so
 *                 RSS is real, not just reserved); the
 *                 probe prints "LIVE <bytes>" first
 *   AFTER_FREE    odd slots of the live set freed
 *   AFTER_FREE_ALL every held block freed (retention)
 * Timed passes: R (argv[2], default 1) full passes run
 * between WARM and STEADY/END exactly as bench_k002 runs
 * them; the probe then prints the bench_k002 CSV line
 * aggregated over the R timed passes:
 *   leg,workload,ops,elapsed_ns,ops_per_sec,ns_per_op,checksum
 * (ops/checksum are R-pass totals; per-pass checksum must
 * equal the Z-003 recorded checksum - the summariser and
 * the recorded run verify this, proving both legs ran
 * identical work.)
 * PEAK live sets (the workload's own characteristic set):
 *   small_churn:     8192 blocks cycling small_sizes - the
 *                    bench live set is a single block, so a
 *                    defined accumulation is used and this
 *                    is stated in the recorded run
 *   mixed_batch:     256 blocks, one full bench batch
 *   large_batch:     32 blocks, one full bench batch
 *   mixed_lifecycle: 512 blocks, one full bench round set
 * Leg selection is bench_k002's own: -DLEG_GLIBC builds
 * the glibc leg, default builds the mimiralloc leg linked
 * against the banked LIBOBJS.
 */
#define main z006_renamed_k002_main
#include "bench_k002.c"
#undef main

static void phase(const char *name)
{
    char buf[64];
    printf("PHASE %s\n", name);
    fflush(stdout);
    if (!fgets(buf, sizeof buf, stdin)) exit(3);
}

/* Allocate and fully touch the characteristic live set
 * for workload w into ptrs[]; returns live bytes. */
static uint64_t hold_alloc(const char *wname, void **ptrs,
                           size_t *sizes, long *n_out)
{
    long n = 0, i;
    uint64_t live = 0;
    if (strcmp(wname, "small_churn") == 0) {
        n = 8192;
        for (i = 0; i < n; i++) sizes[i] = small_sizes[i % 8];
    } else if (strcmp(wname, "mixed_batch") == 0) {
        n = BATCH;
        for (i = 0; i < n; i++) sizes[i] = mixed_sizes[i % 20];
    } else if (strcmp(wname, "large_batch") == 0) {
        n = LBATCH;
        for (i = 0; i < n; i++) sizes[i] = large_sizes[i % 6];
    } else if (strcmp(wname, "mixed_lifecycle") == 0) {
        n = MBATCH;
        for (i = 0; i < n; i++)
            sizes[i] = (i % 8 == 7) ? large_sizes[i % 3]
                                    : mixed_sizes[i % 20];
    } else {
        fprintf(stderr, "unknown workload %s\n", wname);
        exit(2);
    }
    for (i = 0; i < n; i++) {
        ptrs[i] = alloc_fn(sizes[i]);
        if (!ptrs[i]) { fprintf(stderr, "FAIL hold null\n"); exit(1); }
        memset(ptrs[i], 0xA5, sizes[i]); /* touch every page */
        live += sizes[i];
    }
    *n_out = n;
    return live;
}

int main(int argc, char **argv)
{
    static void *hold_ptrs[8192];
    static size_t hold_sizes[8192];
    size_t w;
    long reps = 1, r, hn = 0, i;
    uint64_t total_ops = 0, total_chk = 0, elapsed_total = 0;
    uint64_t live;
    if (argc < 2) {
        fprintf(stderr, "usage: probe_z006 <workload> [reps]\n");
        return 2;
    }
    if (argc >= 3) {
        reps = strtol(argv[2], NULL, 10);
        if (reps < 1) { fprintf(stderr, "reps must be >= 1\n"); return 2; }
    }
    for (w = 0; w < sizeof(workloads) / sizeof(workloads[0]); w++) {
        uint64_t ops = 0, chk;
        uint64_t t0, t1;
        if (strcmp(argv[1], workloads[w].name) != 0) continue;
        phase("START");
        /* Warm-up at 1/10 scale, untimed, as bench_k002. */
        (void)workloads[w].run(alloc_fn, free_fn,
                               workloads[w].timed_iters / 10, &ops);
        phase("WARM");
        for (r = 0; r < reps; r++) {
            t0 = now_ns();
            chk = workloads[w].run(alloc_fn, free_fn,
                                   workloads[w].timed_iters, &ops);
            t1 = now_ns();
            elapsed_total += t1 - t0;
            total_ops += ops;
            total_chk += chk;
            if (r + 1 == (reps + 1) / 2) phase("STEADY");
        }
        live = hold_alloc(workloads[w].name, hold_ptrs,
                          hold_sizes, &hn);
        printf("LIVE %llu\n", (unsigned long long)live);
        fflush(stdout);
        phase("PEAK");
        for (i = 1; i < hn; i += 2) { free_fn(hold_ptrs[i]); hold_ptrs[i] = NULL; }
        phase("AFTER_FREE");
        for (i = 0; i < hn; i++) if (hold_ptrs[i]) free_fn(hold_ptrs[i]);
        phase("AFTER_FREE_ALL");
        printf("%s,%s,%llu,%llu,%.1f,%.2f,%llu\n",
               leg_name, workloads[w].name,
               (unsigned long long)total_ops,
               (unsigned long long)elapsed_total,
               (double)total_ops * 1e9 / (double)elapsed_total,
               (double)elapsed_total / (double)total_ops,
               (unsigned long long)total_chk);
        fflush(stdout);
        return 0;
    }
    fprintf(stderr, "unknown workload %s\n", argv[1]);
    return 2;
}
