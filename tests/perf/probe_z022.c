/* probe_z022.c - Z-022 small-path anatomy probe.
 *
 * Drives ONLY the banked K-002 small_churn workload
 * (bench_k002.c included UNMODIFIED below, its main()
 * renamed away) against the production core objects, in
 * one long timed pass, so that `perf record` (driven by
 * tests/perf/run_anatomy_z022.sh) sees a steady-state
 * small-path sample stream and `perf report` can rank the
 * hot spots by cycles for the roadmap's top-10 gate.
 *
 * Usage: probe_z022 [total_ops]
 *   total_ops: alloc+free pairs in the timed pass
 *   (default 20000000). The bench's own w_small_churn is
 *   called in chunks of its timed scale (100000 ops) so
 *   the size cycle and checksum are exactly the K-002
 *   ones; the printed checksum must therefore equal the
 *   K-002 small_churn checksum scaled by the chunk count
 *   (the runner/summariser check cross-process equality,
 *   the K-002 discipline).
 *
 * Warm-up: one untimed chunk pass (100000 ops) plus the
 * runner's discarded warm-up processes (Z-004 protocol).
 *
 * Output (stdout), one line:
 *   Z022,small_churn,ops,elapsed_ns,ns_per_op,checksum
 * Timing: CLOCK_MONOTONIC around the timed pass only
 * (bench_k002.c's now_ns).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define main z022_renamed_k002_main
#include "bench_k002.c"
#undef main

int main(int argc, char **argv)
{
    long total = 20000000L;
    long chunk = 100000L; /* bench_k002 small_churn scale */
    long done = 0;
    uint64_t ops = 0, chk = 0, t0, t1, elapsed;

    if (argc > 1) {
        total = strtol(argv[1], NULL, 10);
        if (total <= 0) {
            fprintf(stderr, "usage: probe_z022 [total_ops]\n");
            return 1;
        }
    }

    /* Untimed warm-up chunk (in-process, in addition to
     * the runner's discarded warm-up processes). */
    chk += w_small_churn(alloc_fn, free_fn, chunk, &ops);

    t0 = now_ns();
    while (done < total) {
        long this_chunk = chunk;
        if (total - done < this_chunk) this_chunk = total - done;
        chk += w_small_churn(alloc_fn, free_fn, this_chunk, &ops);
        done += this_chunk;
    }
    t1 = now_ns();
    elapsed = t1 - t0;

    printf("Z022,small_churn,%llu,%llu,%.2f,%llu\n",
           (unsigned long long)total,
           (unsigned long long)elapsed,
           (double)elapsed / (double)total,
           (unsigned long long)chk);
    fflush(stdout);
    return 0;
}
