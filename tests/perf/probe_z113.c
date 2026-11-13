/* probe_z113.c - Z-113 GPU-shared UMA pool behaviour probe.
 * Per-workload driver around the BANKED K-002 harness,
 * exactly the probe_z005.c pattern: bench_k002.c is included
 * UNMODIFIED (main renamed away); one workload per process;
 * one warm-up pass at 1/10 scale, then R timed passes.
 * Prints the bench_k002 CSV format aggregated over R passes:
 *   leg,workload,ops,elapsed_ns,ops_per_sec,ns_per_op,checksum
 * Leg: -DLEG_GLIBC builds glibc, default builds mimiralloc.
 * The Z-113 runner (run_z113.sh) runs this probe in two
 * phases on this host: without and with a GPU co-tenant
 * load sharing the UMA memory (ollama 7B generation on the
 * integrated Radeon of the Strix Halo). The probe itself
 * knows nothing of phases; the runner labels them.
 */
#define main z113_renamed_k002_main
#include "bench_k002.c"
#undef main

int main(int argc, char **argv)
{
    size_t w;
    long reps = 1, r;
    uint64_t total_ops = 0, total_chk = 0, elapsed_total = 0;
    if (argc < 2) { fprintf(stderr, "usage: probe_z113 <workload> [reps]\n"); return 2; }
    if (argc >= 3) { reps = strtol(argv[2], NULL, 10); if (reps < 1) return 2; }
    for (w = 0; w < sizeof(workloads) / sizeof(workloads[0]); w++) {
        uint64_t ops = 0;
        if (strcmp(argv[1], workloads[w].name) != 0) continue;
        (void)workloads[w].run(alloc_fn, free_fn, workloads[w].timed_iters / 10, &ops);
        for (r = 0; r < reps; r++) {
            uint64_t t0, t1, chk;
            t0 = now_ns();
            chk = workloads[w].run(alloc_fn, free_fn, workloads[w].timed_iters, &ops);
            t1 = now_ns();
            elapsed_total += t1 - t0; total_ops += ops; total_chk += chk;
        }
        printf("%s,%s,%llu,%llu,%.1f,%.2f,%llu\n", leg_name, workloads[w].name,
               (unsigned long long)total_ops, (unsigned long long)elapsed_total,
               elapsed_total ? (double)total_ops * 1e9 / (double)elapsed_total : 0.0,
               total_ops ? (double)elapsed_total / (double)total_ops : 0.0,
               (unsigned long long)total_chk);
        return 0;
    }
    fprintf(stderr, "unknown workload %s\n", argv[1]);
    return 2;
}
