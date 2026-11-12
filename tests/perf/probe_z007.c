/* probe_z007.c - Z-007 energy probe (driver for the RAPL energy runner).
 *
 * Per-workload driver around the BANKED K-002 harness.
 * bench_k002.c is included UNMODIFIED (its main() renamed
 * away below): the workload functions, size tables, op
 * definition (one allocation plus its matching free) and
 * checksums are exactly the banked ones - this probe adds
 * no workload code of its own. It exists because bench_k002
 * runs all four workloads inside one process, so an external
 * `perf stat` around the bench binary can only produce
 * whole-process counters. This probe runs ONE workload per
 * process (selected by argv[1]) so perf stat measures that
 * workload alone:
 *   - one warm-up pass at 1/10 scale, exactly as bench_k002
 *   - then R timed passes back to back (argv[2], default 1);
 *     counters from perf cover warm-up + timed passes +
 *     process start-up, so R is chosen per workload by the
 *     runner (run_energy_z007.sh) to keep the warm-up share
 *     <= 2% of executed ops and start-up amortised; the
 *     recorded run states the exact shares.
 * Prints the bench_k002 CSV line format, aggregated over
 * the R timed passes:
 *   leg,workload,ops,elapsed_ns,ops_per_sec,ns_per_op,checksum
 * (ops and checksum are the R-pass totals; the per-pass
 * checksum must equal the Z-003 recorded checksum, which
 * the summariser verifies.)
 * Leg selection is bench_k002's own: -DLEG_GLIBC builds the
 * glibc leg, default builds the mimiralloc leg linked
 * against the banked LIBOBJS.
 */
#define main z007_renamed_k002_main
#include "bench_k002.c"
#undef main

int main(int argc, char **argv)
{
    size_t w;
    long reps = 1, r;
    uint64_t total_ops = 0, total_chk = 0, elapsed_total = 0;
    if (argc < 2) {
        fprintf(stderr, "usage: probe_z007 <workload> [reps]\n");
        return 2;
    }
    if (argc >= 3) {
        reps = strtol(argv[2], NULL, 10);
        if (reps < 1) { fprintf(stderr, "reps must be >= 1\n"); return 2; }
    }
    for (w = 0; w < sizeof(workloads) / sizeof(workloads[0]); w++) {
        uint64_t ops = 0;
        if (strcmp(argv[1], workloads[w].name) != 0) continue;
        (void)workloads[w].run(alloc_fn, free_fn,
                               workloads[w].timed_iters / 10, &ops);
        for (r = 0; r < reps; r++) {
            uint64_t t0, t1, chk;
            t0 = now_ns();
            chk = workloads[w].run(alloc_fn, free_fn,
                                   workloads[w].timed_iters, &ops);
            t1 = now_ns();
            elapsed_total += t1 - t0;
            total_ops += ops;
            total_chk += chk;
        }
        printf("%s,%s,%llu,%llu,%.1f,%.2f,%llu\n",
               leg_name, workloads[w].name,
               (unsigned long long)total_ops,
               (unsigned long long)elapsed_total,
               (double)total_ops * 1e9 / (double)elapsed_total,
               (double)elapsed_total / (double)total_ops,
               (unsigned long long)total_chk);
        return 0;
    }
    fprintf(stderr, "unknown workload: %s\n", argv[1]);
    return 2;
}
