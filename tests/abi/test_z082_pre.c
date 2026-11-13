/* test_z082_pre.c - Z-082 LD_PRELOAD probe consumer.
 *
 * Built as an ordinary glibc consumer: it references
 * only standard malloc-family names and links against
 * NO MimirAlloc artifact. Run two ways by the recorded
 * run (tests/abi/run_z082.sh S6) and by `make test-z082`:
 *   plain:      ./test_z082_pre
 *   preloaded:  LD_PRELOAD=build/libmimiralloc_interpose.so
 *               ./test_z082_pre
 * The two probes are chosen so BOTH outcomes are fully
 * defined in BOTH allocator worlds (no use-after-free,
 * no reading of a maybe-freed block):
 *   probe 1: malloc(0) -- glibc returns a distinct
 *     freeable pointer; the interposing variant returns
 *     NULL (the D-013 contract null).
 *   probe 2: posix_memalign(&m, 8, 0) -- glibc returns
 *     0 and sets m; the variant returns ENOMEM and
 *     leaves m unchanged (D-013).
 * The verdict line is exactly one of:
 *   PRELOAD-VERDICT: GLIBC
 *   PRELOAD-VERDICT: MIMIR
 *   PRELOAD-VERDICT: MIXED
 * Exit 0 iff the verdict is GLIBC or MIMIR (a coherent
 * single-allocator world); the run script asserts which
 * one each leg must print. Compiled -fno-builtin so the
 * compiler cannot fold the probes on builtin malloc
 * assumptions (Makefile Z-082 block). The probe frees
 * only pointers it obtained from the malloc family in
 * this TU, in the same run that obtained them -- the
 * standing preload discipline (never free a foreign
 * allocator's pointer). */
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int posix_memalign(void **memptr, size_t alignment,
                   size_t size);

int main(void)
{
    void *z = malloc(0);
    void *m = (void *)0x1;
    int rc = posix_memalign(&m, 8, 0);
    int mimir_votes = 0;
    int glibc_votes = 0;

    if (z == NULL) {
        mimir_votes++;
    } else {
        glibc_votes++;
        free(z);
    }
    if (rc == ENOMEM && m == (void *)0x1) {
        mimir_votes++;
    } else if (rc == 0 && m != (void *)0x1 && m != NULL) {
        glibc_votes++;
        free(m);
    }

    if (mimir_votes == 2) {
        printf("PRELOAD-VERDICT: MIMIR\n");
        return 0;
    }
    if (glibc_votes == 2) {
        printf("PRELOAD-VERDICT: GLIBC\n");
        return 0;
    }
    printf("PRELOAD-VERDICT: MIXED (zero=%s posix_rc=%d)\n",
           z == NULL ? "NULL" : "ptr", rc);
    return 1;
}
