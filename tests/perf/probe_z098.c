/* probe_z098.c - Z-098 fair-fight leg: glibc tunable
 * engagement probe (MimirAlloc Z13).
 *
 * The Z-098 matrix (tests/perf/run_z098.sh) drives the
 * Wave-1 oracle harness tests/perf/bench_k002.c under a
 * matrix of GLIBC_TUNABLES settings. Tunable strings that
 * glibc rejects are ignored SILENTLY, so a matrix built on
 * them alone could be measuring default glibc fifteen
 * times and calling it tuning. This probe is the hard
 * evidence that the tunable channel is live on this host:
 *
 *   1. It echoes the GLIBC_TUNABLES value it was started
 *      under (read back from its own environment).
 *   2. mmap_threshold evidence (mallinfo2): a single
 *      64 KiB allocation is taken in a fresh process.
 *      Under the default dynamic threshold (initial
 *      128 KiB) and under mmap_threshold=1048576 the
 *      block must land on the heap (hblkhd unchanged,
 *      uordblks grows by ~64 KiB). Under
 *      mmap_threshold=4096 it must be served by mmap
 *      (hblkhd grows by ~64 KiB, uordblks unmoved).
 *      The runner asserts exactly
 *      this pattern and fails the run if it is absent.
 *   3. tcache engagement timing: 200k alloc/free pairs
 *      of one 64 B block, ns/op printed. With
 *      tcache_count=0 the tcache fast path is disabled
 *      and the same loop takes the slower bin path;
 *      the runner records the two numbers side by side
 *      as engagement evidence (a signature, not a
 *      verdict - the matrix itself grades the tunables).
 *
 * Exit 0 unless an allocation fails or mallinfo2 moves
 * in neither field (a broken probe, not a tunable
 * result). Native C11 only; no MimirAlloc code; the
 * probe measures the host glibc it is linked against.
 */
#define _POSIX_C_SOURCE 200809L
#include <malloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* The measured pointer must escape observably: with a
 * plain malloc + memset + free whose bytes nobody reads,
 * the compiler lawfully eliminates the allocation
 * entirely and mallinfo2 never moves (found in this
 * slice's first probe draft, 2026-11-12: every delta
 * read 0 under every tunable; the volatile sink plus
 * the printed byte sum below are the fix). */
static void *volatile sink;

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) +
           (uint64_t)ts.tv_nsec;
}

int main(void)
{
    /* Snapshot BEFORE the first printf: stdio's own
     * buffer allocation initialises the arena, after
     * which a 64 KiB block can hide inside the existing
     * heap top (arena size unmoved). The heap witness
     * below is therefore uordblks (bytes in use), not
     * arena size. */
    struct mallinfo2 before = mallinfo2();
    const char *tun = getenv("GLIBC_TUNABLES");
    printf("tunables_env: %s\n", tun ? tun : "(unset)");

    /* --- mmap_threshold evidence: one 64 KiB block --- */
    void *p = malloc(65536);
    if (!p) { fprintf(stderr, "FAIL malloc 65536\n"); return 1; }
    memset(p, 0xA5, 65536);
    sink = p;
    struct mallinfo2 after = mallinfo2();
    unsigned byte_sum = 0;
    for (size_t i = 0; i < 65536; i += 4096)
        byte_sum += ((unsigned char *)p)[i];
    long long hblk_delta = (long long)after.hblkhd - (long long)before.hblkhd;
    long long arena_delta = (long long)after.arena - (long long)before.arena;
    long long uord_delta = (long long)after.uordblks - (long long)before.uordblks;
    printf("mmap_probe: size=65536 byte_sum=%u hblkhd_delta=%lld arena_delta=%lld uordblks_delta=%lld\n",
           byte_sum, hblk_delta, arena_delta, uord_delta);
    free(p);
    if (hblk_delta == 0 && uord_delta == 0) {
        fprintf(stderr, "FAIL mallinfo2 unmoved\n");
        return 1;
    }

    /* --- tcache engagement signature: 64 B churn --- */
    enum { ITERS = 200000 };
    uint64_t chk = 0;
    uint64_t t0 = now_ns();
    for (int i = 0; i < ITERS; i++) {
        void *q = malloc(64);
        if (!q) { fprintf(stderr, "FAIL malloc 64\n"); return 1; }
        /* Escape every allocation through the volatile
         * sink and read it back through a volatile
         * pointer: without this the compiler folds the
         * whole malloc/memset/free loop into arithmetic
         * (first draft read 0.20 ns/op under every
         * tunable, tcache or none - the same elimination
         * class as the mmap witness above). */
        sink = q;
        memset(q, i & 0xFF, 64);
        chk += *(volatile unsigned char *)q;
        free(q);
    }
    uint64_t t1 = now_ns();
    printf("tcache_probe: size=64 iters=%d ns_per_op=%.2f checksum=%llu\n",
           ITERS, (double)(t1 - t0) / (double)ITERS,
           (unsigned long long)chk);
    return 0;
}
