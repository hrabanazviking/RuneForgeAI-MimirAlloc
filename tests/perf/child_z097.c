/* child_z097.c - Z-097 short-lived process payload.
 *
 * An ordinary glibc consumer: references only standard
 * malloc-family names, links against NO MimirAlloc
 * artifact. The storm driver (probe_z097.c) execs it
 * plain (glibc world) and under LD_PRELOAD of the Z-082
 * interposing build (build/libmimiralloc_interpose.so).
 * Modes (argv[1]):
 *   none    exit 0 immediately: loader + libc init +
 *           preload .so load only, no allocation call.
 *   first   one malloc(32), touch, free: load + the
 *           first-allocation cost of a short-lived proc.
 *   multi   eight first allocations across the class
 *           ladder (16..262144), touch both ends, free
 *           in reverse order.
 *   churn   1000 small alloc/touch/free cycles, sizes
 *           cycling 16..512: a short-lived process that
 *           actually works its allocator.
 *   verdict the Z-082 two-probe world check (malloc(0)
 *           and posix_memalign(8,0)): prints exactly
 *           PRELOAD-VERDICT: GLIBC or MIMIR; exit 0 iff
 *           the world is coherent. NOT a timed shape.
 * Timed modes exit 0 iff the checksum over the bytes
 * actually read back from the allocations matches the
 * same checksum computed arithmetically from the
 * deterministic fill pattern (no allocation involved),
 * 42 on mismatch. Compiled -fno-builtin so no builtin
 * malloc assumption can fold the calls.
 */
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int posix_memalign(void **memptr, size_t alignment, size_t size);

#define H_SEED 1469598103934665603UL

static unsigned long mix(unsigned long h, unsigned char b)
{
    return (h ^ (unsigned long)b) * 1099511628211UL;
}

static int write_all(const char *s, size_t n)
{
    while (n > 0) {
        ssize_t w = write(1, s, n);
        if (w <= 0) return -1;
        s += (size_t)w;
        n -= (size_t)w;
    }
    return 0;
}

static int mode_verdict(void)
{
    void *z = malloc(0);
    void *m = (void *)0x1;
    int rc = posix_memalign(&m, 8, 0);
    int mv = 0, gv = 0;
    if (z == NULL) { mv++; } else { gv++; free(z); }
    if (rc == ENOMEM && m == (void *)0x1) { mv++; }
    else if (rc == 0 && m != (void *)0x1 && m != NULL) {
        gv++; free(m);
    }
    if (mv == 2) {
        const char s[] = "PRELOAD-VERDICT: MIMIR\n";
        return write_all(s, sizeof(s) - 1) == 0 ? 0 : 1;
    }
    if (gv == 2) {
        const char s[] = "PRELOAD-VERDICT: GLIBC\n";
        return write_all(s, sizeof(s) - 1) == 0 ? 0 : 1;
    }
    {
        const char s[] = "PRELOAD-VERDICT: MIXED\n";
        (void)write_all(s, sizeof(s) - 1);
    }
    return 1;
}

static int mode_first(void)
{
    unsigned long h = H_SEED, want = H_SEED;
    unsigned char *p = (unsigned char *)malloc(32);
    int i;
    if (p == NULL) return 42;
    for (i = 0; i < 32; i++) p[i] = (unsigned char)(i * 7 + 1);
    for (i = 0; i < 32; i++) {
        h = mix(h, p[i]);
        want = mix(want, (unsigned char)(i * 7 + 1));
    }
    free(p);
    return h == want ? 0 : 42;
}

static int mode_multi(void)
{
    static const size_t sizes[8] =
        {16, 64, 256, 1024, 4096, 16384, 65536, 262144};
    unsigned char *ptrs[8];
    unsigned long h = H_SEED, want = H_SEED;
    int k;
    for (k = 0; k < 8; k++) {
        ptrs[k] = (unsigned char *)malloc(sizes[k]);
        if (ptrs[k] == NULL) return 42;
        ptrs[k][0] = (unsigned char)(k * 13 + 3);
        ptrs[k][sizes[k] - 1] = (unsigned char)(k * 29 + 5);
    }
    for (k = 0; k < 8; k++) {
        h = mix(h, ptrs[k][0]);
        h = mix(h, ptrs[k][sizes[k] - 1]);
        want = mix(want, (unsigned char)(k * 13 + 3));
        want = mix(want, (unsigned char)(k * 29 + 5));
    }
    for (k = 7; k >= 0; k--) free(ptrs[k]);
    return h == want ? 0 : 42;
}

static int mode_churn(void)
{
    unsigned long h = H_SEED, want = H_SEED;
    int i;
    for (i = 0; i < 1000; i++) {
        size_t n = 16 + (size_t)(i % 8) * 64;
        unsigned char *p = (unsigned char *)malloc(n);
        if (p == NULL) return 42;
        p[0] = (unsigned char)(i & 0xff);
        p[n - 1] = (unsigned char)((i * 3) & 0xff);
        h = mix(h, p[0]);
        h = mix(h, p[n - 1]);
        want = mix(want, (unsigned char)(i & 0xff));
        want = mix(want, (unsigned char)((i * 3) & 0xff));
        free(p);
    }
    return h == want ? 0 : 42;
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "none";
    if (strcmp(mode, "none") == 0) return 0;
    if (strcmp(mode, "verdict") == 0) return mode_verdict();
    if (strcmp(mode, "first") == 0) return mode_first();
    if (strcmp(mode, "multi") == 0) return mode_multi();
    if (strcmp(mode, "churn") == 0) return mode_churn();
    return 2;
}
