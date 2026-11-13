/* test_z094_trace_smoke.c - Z-094: scripted consumer for
 * the audit-hook capture pipeline's self-test. Runs under
 * build/libmimirtrace_z094.so (LD_PRELOAD); its allocation
 * pattern is fixed, so the summarised canonical trace and
 * both replay legs must reproduce it exactly.
 *
 * Pattern: malloc across classes, a calloc, an aligned
 * alloc, a growing realloc, an in-place shrinking
 * realloc, a deliberately failing huge realloc (E form),
 * frees in mixed order, and one foreign free (a pointer
 * from __libc_malloc, which bypasses the PLT boundary on
 * purpose: the shim must record F 0 and forward it).
 * No zero-size ops: those are D-013 territory, exercised
 * by the real traces, and this smoke's job is pipeline
 * truth, not posture law.
 * Exit 0 iff every serviced call behaved as scripted.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern void *__libc_malloc(size_t size);
extern void __libc_free(void *ptr);

int main(void)
{
    char *a = (char *)malloc(24);
    char *b = (char *)malloc(4096);
    int *c = (int *)calloc(100, sizeof(int));
    char *d = (char *)aligned_alloc(64, 640);
    void *foreign;
    int i;

    if (!a || !b || !c || !d)
        return 1;
    memset(a, 0x41, 24);
    memset(b, 0x42, 4096);
    for (i = 0; i < 100; i++)
        if (c[i] != 0)
            return 2; /* calloc not zero-filled */
    if ((uintptr_t)d % 64 != 0)
        return 3;
    memset(d, 0x44, 640);

    a = (char *)realloc(a, 100000); /* grow: may move */
    if (!a)
        return 4;
    a = (char *)realloc(a, 32); /* shrink */
    if (!a)
        return 5;
    {
        /* The E form: a failing realloc returns NULL and
         * leaves the old block live in the CALLER's hands,
         * so the result goes to a scratch variable - never
         * assigned over the only copy of the old pointer.
         * (The first draft did assign it over b; GCC's
         * -Wnonnull fortify check caught the resulting
         * memset(NULL, ..., 4096) and the lost block.) */
        char *nb = (char *)realloc(b, (size_t)1 << 62);
        if (nb)
            return 6; /* the E form requires NULL here */
    }
    /* b still owns its original 4096 bytes */
    memset(b, 0x43, 4096);

    foreign = __libc_malloc(128);
    if (!foreign)
        return 7;
    free(foreign); /* foreign to the shim: F 0, forwarded */
    (void)__libc_free; /* linked to prove the route exists */

    free(c);
    free(d);
    free(b);
    free(a);
    return 0;
}
