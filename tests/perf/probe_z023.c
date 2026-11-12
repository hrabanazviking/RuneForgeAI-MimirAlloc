/* probe_z023.c - Z-023 large-path anatomy probe.
 *
 * Decomposes the large_batch cost (KL-C: a mapping per
 * allocation past the 1024-byte cutoff) into the three shares
 * the roadmap names - the mmap/munmap pair, the bookkeeping,
 * and the page faults - per request size and for the batch
 * shape as a whole, on this host only.
 *
 * The bench workload code is bench_k002.c included
 * UNMODIFIED below (its main renamed away), so the mimir and
 * glibc batch legs run the banked w_large_batch itself: same
 * size table {2048, 8192, 16384, 65536, 262144, 1048576},
 * same batch of 32, same endpoint touch(), same forward
 * free order. The single-size legs reuse the bench's own
 * touch() so the fault pattern is byte-identical.
 *
 * Phases (argv[1]); one phase per process, the runner
 * (run_z023.sh) drives repetitions as separate processes
 * under the Z-004 protocol (pin CPU 2, warm-ups discarded,
 * legs interleaved, headline = median of processes):
 *   op <mimir|glibc> <size> <iters>
 *       Single-size alloc + endpoint-touch + free, one
 *       block live at a time. The full allocator path.
 *   raw <notouch|touch|fulltouch> <size> <iters>
 *       The bare mapping pair with EXACTLY MimirAlloc's
 *       geometry and flags: length = round_up(4096 + size,
 *       4096) (one reserve page + user extent, alloc_locked
 *       / mimir_platform.c), mmap PROT_READ|PROT_WRITE,
 *       MAP_PRIVATE|MAP_ANONYMOUS, user area at base+4096.
 *       notouch: mmap + munmap only (the pure syscall pair).
 *       touch: + the bench endpoint touch (faults only the
 *       first and last user pages - 1 page when the request
 *       fits one page, else 2). fulltouch: + one byte per
 *       user page (what full use of the block would fault).
 *       Decomposition per size:
 *         pair        = raw/notouch
 *         faults      = raw/touch - raw/notouch
 *         bookkeeping = op/mimir - raw/touch  (residual: the
 *           one global lock, registry insert/remove and the
 *           free-side registry walk, descriptor write and
 *           validation, J-002/E-005 counters, dispatch -
 *           plus any non-additivity between legs measured
 *           in different processes; labelled residual, never
 *           a directly timed quantity)
 *   batch <mimir|glibc>
 *       w_large_batch itself, 30 warm-up batches + 300 timed
 *       (the bench's own 1/10 warm-up discipline at its own
 *       scale). Checksum must equal the K-002 value.
 *   batchraw <notouch|touch>
 *       The batch shape (32 live, forward free) rebuilt on
 *       raw mappings with identical size cycling and seeds,
 *       so its touch checksum must also equal the K-002
 *       value: what the batch would cost with zero allocator
 *       bookkeeping. Separates batch-shape effects (32 live
 *       VMAs, page-table warmth) from bookkeeping.
 *   walk <mimir|raw> <live> <reps>
 *       <live> blocks of 65536 live at once (endpoint
 *       touched), then the free-all pass timed, forward
 *       (oldest-first) order - for MimirAlloc the worst
 *       registry-walk geometry (insert at head, oldest is
 *       deepest). mimir - raw at the same <live> isolates
 *       walk + per-free bookkeeping from the munmap itself.
 *
 * Timing: CLOCK_MONOTONIC_RAW around the timed section
 * only. Minor-fault delta from /proc/self/stat (field 10)
 * around the timed section, printed per phase so fault
 * counts - not just fault time - are on record.
 * CSV (one line per process):
 *   phase,leg,size,live,iters,total_ns,ns_per_op,
 *   minflt_delta,checksum
 * Exit 0 iff every allocation/mapping succeeded; checksum
 * discipline is enforced by the summariser (K-002 rule:
 * exactly one checksum per comparable phase family).
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20 /* Linux: exposed only under _DEFAULT_SOURCE; value is ABI */
#endif
#include <time.h>
#include <unistd.h>

#define main z023_renamed_k002_main
#include "bench_k002.c"
#undef main

/* bench_k002.c binds alloc_fn/free_fn to mimir under the
 * default build; the legs below name both pairs explicitly
 * so one binary serves every leg. */
static void *(*const MIMIR_AF)(size_t) = mimir_malloc;
static void (*const MIMIR_FF)(void *) = mimir_free;
static void *(*const GLIBC_AF)(size_t) = malloc;
static void (*const GLIBC_FF)(void *) = free;

static long long z23_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (long long)ts.tv_sec * 1000000000LL +
           (long long)ts.tv_nsec;
}

/* /proc/self/stat field 10 (minflt), parser as probe_z014. */
static long read_minflt(void)
{
    FILE *f = fopen("/proc/self/stat", "r");
    if (!f) return -1;
    char buf[4096];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    if (n == 0) return -1;
    buf[n] = 0;
    char *rp = strrchr(buf, ')');
    if (!rp) return -1;
    char *p = rp + 1;
    long val = -1;
    for (int tok = 1; tok <= 8; tok++) {
        while (*p == ' ') p++;
        if (!*p) return -1;
        if (tok == 8) val = strtol(p, NULL, 10);
        while (*p && *p != ' ') p++;
    }
    return val;
}

static size_t map_len_for(size_t size)
{
    size_t total = 4096u + size;
    return (total + 4095u) & ~(size_t)4095u;
}

static void emit(const char *phase, const char *leg, size_t size,
                 long live, long iters, long long total_ns,
                 long minflt_delta, uint64_t chk)
{
    printf("%s,%s,%zu,%ld,%ld,%lld,%.2f,%ld,%llu\n",
           phase, leg, size, live, iters, total_ns,
           iters > 0 ? (double)total_ns / (double)iters : 0.0,
           minflt_delta, (unsigned long long)chk);
}

/* op: single-size full allocator path, batch depth 1. */
static int phase_op(void *(*af)(size_t), void (*ff)(void *),
                    const char *leg, size_t size, long iters)
{
    long warm = iters / 10;
    long i;
    uint64_t chk = 0;
    for (i = 0; i < warm; i++) {
        void *p = af(size);
        if (!p) return 1;
        (void)touch(p, size, (unsigned)i);
        ff(p);
    }
    long f0 = read_minflt();
    long long t0 = z23_now_ns();
    for (i = 0; i < iters; i++) {
        void *p = af(size);
        if (!p) return 1;
        chk += touch(p, size, (unsigned)i);
        ff(p);
    }
    long long t1 = z23_now_ns();
    long f1 = read_minflt();
    if (f0 < 0 || f1 < 0) return 1;
    emit("op", leg, size, 1, iters, t1 - t0, f1 - f0, chk);
    return 0;
}

/* raw, timed implementation (kept separate so the fault
 * delta brackets exactly the timed iterations). */
static int phase_raw_timed(const char *mode, size_t size,
                           long iters)
{
    size_t len = map_len_for(size);
    size_t user_pages = (len - 4096u) / 4096u;
    long warm = iters / 10;
    long i;
    uint64_t chk = 0;
    int do_touch = strcmp(mode, "notouch") != 0;
    int full = strcmp(mode, "fulltouch") == 0;
    for (i = 0; i < warm; i++) {
        char *base = mmap(NULL, len, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) return 1;
        if (do_touch && !full)
            (void)touch(base + 4096u, size, (unsigned)i);
        else if (full) {
            size_t pg;
            for (pg = 0; pg < user_pages; pg++)
                base[4096u + pg * 4096u] = (char)(pg * 7u + 1u);
        }
        if (munmap(base, len) != 0) return 1;
    }
    long f0 = read_minflt();
    long long t0 = z23_now_ns();
    for (i = 0; i < iters; i++) {
        char *base = mmap(NULL, len, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (base == MAP_FAILED) return 1;
        if (do_touch && !full) {
            chk += touch(base + 4096u, size, (unsigned)i);
        } else if (full) {
            size_t pg;
            uint64_t s = 0;
            for (pg = 0; pg < user_pages; pg++)
                base[4096u + pg * 4096u] =
                    (char)(pg * 7u + 1u + (size_t)i);
            for (pg = 0; pg < user_pages; pg++)
                s += (unsigned char)base[4096u + pg * 4096u];
            chk += s;
        }
        if (munmap(base, len) != 0) return 1;
    }
    long long t1 = z23_now_ns();
    long f1 = read_minflt();
    if (f0 < 0 || f1 < 0) return 1;
    emit("raw", mode, size, 1, iters, t1 - t0, f1 - f0, chk);
    return 0;
}

/* batch: the banked w_large_batch, both allocator legs. */
static int phase_batch(void *(*af)(size_t), void (*ff)(void *),
                       const char *leg)
{
    uint64_t ops = 0;
    uint64_t chk;
    (void)w_large_batch(af, ff, 30, &ops); /* warm-up 1/10 */
    long f0 = read_minflt();
    long long t0 = z23_now_ns();
    chk = w_large_batch(af, ff, 300, &ops);
    long long t1 = z23_now_ns();
    long f1 = read_minflt();
    if (f0 < 0 || f1 < 0) return 1;
    emit("batch", leg, 0, LBATCH, (long)ops, t1 - t0,
         f1 - f0, chk);
    return 0;
}

/* batchraw, timed implementation: warm-up batches run
 * untimed first, then the timed batches. */
static int phase_batchraw_timed(const char *mode)
{
    static void *ptrs[LBATCH];
    static size_t lens[LBATCH];
    int do_touch = strcmp(mode, "notouch") != 0;
    uint64_t chk = 0;
    long b;
    int i;
    for (b = 0; b < 30; b++) {
        for (i = 0; i < LBATCH; i++) {
            size_t n = large_sizes[(i + (int)b) % 6];
            size_t len = map_len_for(n);
            char *base = mmap(NULL, len,
                              PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (base == MAP_FAILED) return 1;
            ptrs[i] = base;
            lens[i] = len;
            if (do_touch)
                (void)touch(base + 4096u, n, (unsigned)(b + i));
        }
        for (i = 0; i < LBATCH; i++)
            if (munmap(ptrs[i], lens[i]) != 0) return 1;
    }
    long f0 = read_minflt();
    long long t0 = z23_now_ns();
    for (b = 0; b < 300; b++) {
        for (i = 0; i < LBATCH; i++) {
            size_t n = large_sizes[(i + (int)b) % 6];
            size_t len = map_len_for(n);
            char *base = mmap(NULL, len,
                              PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (base == MAP_FAILED) return 1;
            ptrs[i] = base;
            lens[i] = len;
            if (do_touch)
                chk += touch(base + 4096u, n, (unsigned)(b + i));
        }
        for (i = 0; i < LBATCH; i++)
            if (munmap(ptrs[i], lens[i]) != 0) return 1;
    }
    long long t1 = z23_now_ns();
    long f1 = read_minflt();
    if (f0 < 0 || f1 < 0) return 1;
    emit("batchraw", mode, 0, LBATCH, 300L * LBATCH, t1 - t0,
         f1 - f0, chk);
    return 0;
}

/* walk: <live> blocks of 65536 live, timed forward free. */
static int phase_walk(const char *leg, long live, long reps)
{
    static void *ptrs[4096];
    static size_t lens[4096];
    const size_t size = 65536;
    long r;
    int i;
    uint64_t chk = 0;
    if (live < 1 || live > 4096) return 2;
    int is_mimir = strcmp(leg, "mimir") == 0;
    /* one untimed build+free to warm allocator and kernel */
    for (i = 0; i < live; i++) {
        if (is_mimir) {
            ptrs[i] = MIMIR_AF(size);
            if (!ptrs[i]) return 1;
        } else {
            size_t len = map_len_for(size);
            char *base = mmap(NULL, len,
                              PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (base == MAP_FAILED) return 1;
            ptrs[i] = base;
            lens[i] = len;
        }
        void *user = is_mimir ? ptrs[i]
                              : (void *)((char *)ptrs[i] + 4096u);
        (void)touch(user, size, (unsigned)i);
    }
    for (i = 0; i < live; i++) {
        if (is_mimir) MIMIR_FF(ptrs[i]);
        else if (munmap(ptrs[i], lens[i]) != 0) return 1;
    }
    long long total = 0;
    for (r = 0; r < reps; r++) {
        for (i = 0; i < live; i++) {
            if (is_mimir) {
                ptrs[i] = MIMIR_AF(size);
                if (!ptrs[i]) return 1;
            } else {
                size_t len = map_len_for(size);
                char *base = mmap(NULL, len,
                                  PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS,
                                  -1, 0);
                if (base == MAP_FAILED) return 1;
                ptrs[i] = base;
                lens[i] = len;
            }
            void *user = is_mimir ? ptrs[i]
                                  : (void *)((char *)ptrs[i] + 4096u);
            chk += touch(user, size, (unsigned)i);
        }
        long long t0 = z23_now_ns();
        for (i = 0; i < live; i++) {
            if (is_mimir) MIMIR_FF(ptrs[i]);
            else if (munmap(ptrs[i], lens[i]) != 0) return 1;
        }
        total += z23_now_ns() - t0;
    }
    emit("walk", leg, size, live, live * reps, total, -1, chk);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && strcmp(argv[1], "op") == 0 && argc == 5) {
        size_t size = (size_t)strtoull(argv[3], NULL, 10);
        long iters = strtol(argv[4], NULL, 10);
        if (strcmp(argv[2], "mimir") == 0)
            return phase_op(MIMIR_AF, MIMIR_FF, "mimir",
                            size, iters);
        if (strcmp(argv[2], "glibc") == 0)
            return phase_op(GLIBC_AF, GLIBC_FF, "glibc",
                            size, iters);
    } else if (argc >= 2 && strcmp(argv[1], "raw") == 0 &&
               argc == 5) {
        size_t size = (size_t)strtoull(argv[3], NULL, 10);
        long iters = strtol(argv[4], NULL, 10);
        return phase_raw_timed(argv[2], size, iters);
    } else if (argc >= 2 && strcmp(argv[1], "batch") == 0 &&
               argc == 3) {
        if (strcmp(argv[2], "mimir") == 0)
            return phase_batch(MIMIR_AF, MIMIR_FF, "mimir");
        if (strcmp(argv[2], "glibc") == 0)
            return phase_batch(GLIBC_AF, GLIBC_FF, "glibc");
    } else if (argc >= 2 && strcmp(argv[1], "batchraw") == 0 &&
               argc == 3) {
        return phase_batchraw_timed(argv[2]);
    } else if (argc >= 2 && strcmp(argv[1], "walk") == 0 &&
               argc == 5) {
        long live = strtol(argv[3], NULL, 10);
        long reps = strtol(argv[4], NULL, 10);
        return phase_walk(argv[2], live, reps);
    }
    fprintf(stderr,
            "usage: probe_z023 op <mimir|glibc> <size> <iters>\n"
            "       probe_z023 raw <notouch|touch|fulltouch>"
            " <size> <iters>\n"
            "       probe_z023 batch <mimir|glibc>\n"
            "       probe_z023 batchraw <notouch|touch>\n"
            "       probe_z023 walk <mimir|raw> <live> <reps>\n");
    return 2;
}
