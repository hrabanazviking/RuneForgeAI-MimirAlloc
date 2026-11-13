/* harness_z089.c - Z-089 rollback drill subject.
 *
 * A small deterministic allocation workload, linked
 * against NO MimirAlloc artifact, run by
 * tests/abi/run_z089.sh in three allocator worlds:
 *   glibc control   : exits 0, prints HARNESS-RESULT: OK
 *                     with a fixed checksum.
 *   healthy preload : the Z-082 interposing variant;
 *                     same OK line, same checksum.
 *   poison preload  : tests/abi/poison_z089.c, whose
 *                     malloc family always returns NULL;
 *                     the harness detects the first NULL,
 *                     prints HARNESS-RESULT: BROKEN and
 *                     exits 3. That is the drill's broken
 *                     state, contained in this spawned
 *                     process and nowhere else.
 * Rollback is then proven by running the identical
 * command with LD_PRELOAD unset: OK returns, byte-
 * identical to the glibc control.
 * Compiled -fno-builtin by the runner so the compiler
 * cannot fold the allocations on builtin assumptions. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    unsigned long checksum = 0;
    size_t i;
    unsigned char *a = (unsigned char *)malloc(64);
    unsigned char *b;
    char *s;

    if (a == NULL) {
        printf("HARNESS-RESULT: BROKEN allocation returned NULL\n");
        return 3;
    }
    for (i = 0; i < 64; i++)
        a[i] = (unsigned char)(i * 7u + 1u);
    for (i = 0; i < 64; i++)
        checksum += a[i];

    b = (unsigned char *)calloc(256, 4);
    if (b == NULL) {
        printf("HARNESS-RESULT: BROKEN allocation returned NULL\n");
        free(a);
        return 3;
    }
    for (i = 0; i < 1024; i++)
        b[i] = (unsigned char)(i & 0xffu);
    for (i = 0; i < 1024; i++)
        checksum += b[i];

    s = (char *)malloc(32);
    if (s == NULL) {
        printf("HARNESS-RESULT: BROKEN allocation returned NULL\n");
        free(a);
        free(b);
        return 3;
    }
    memcpy(s, "mimiralloc-z089", 16);
    for (i = 0; s[i] != '\0'; i++)
        checksum += (unsigned char)s[i];

    printf("HARNESS-RESULT: OK checksum=%lu blocks=3 bytes=1120\n",
           checksum);
    free(a);
    free(b);
    free(s);
    return 0;
}
