/* test_z082_dyn.c - Z-082 dynamic consumer: links ONLY
 * against build/libmimiralloc_interpose.so and uses ONLY
 * standard malloc-family names (no mimir_* symbol is
 * referenced), proving the interposing variant serves a
 * plain C consumer at load time: lifecycle, calloc
 * zeroing, grow preservation, the D-013 zero-size and
 * realloc-to-zero answers through the standard names,
 * aligned allocation, usable size and trim. Exit 0 iff
 * every check passes. */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cfree(void *ptr);
size_t malloc_usable_size(void *ptr);
int malloc_trim(size_t pad);
void *reallocarray(void *ptr, size_t nmemb, size_t size);
int posix_memalign(void **memptr, size_t alignment,
                   size_t size);

static int fails = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            fails++;                                                    \
            printf("  [FAIL] %s (line %d)\n", #cond, __LINE__);         \
        }                                                               \
    } while (0)

#define SENTINEL 77

int main(void)
{
    unsigned char *p, *q;
    void *m = (void *)0x1;
    size_t i;
    int rc;

    p = malloc(96);
    CHECK(p != NULL);
    for (i = 0; i < 96; i++) {
        p[i] = (unsigned char)(i * 7u + 2u);
    }
    p = realloc(p, 300);
    CHECK(p != NULL);
    for (i = 0; i < 96; i++) {
        CHECK(p[i] == (unsigned char)(i * 7u + 2u));
    }
    CHECK(malloc_usable_size(p) >= 300);

    /* D-013 through the standard names, via the .so. */
    errno = SENTINEL; CHECK(malloc(0) == NULL); CHECK(errno == SENTINEL);
    errno = SENTINEL; CHECK(realloc(p, 0) == NULL); CHECK(errno == SENTINEL);
    for (i = 0; i < 96; i++) {
        CHECK(p[i] == (unsigned char)(i * 7u + 2u)); /* still live */
    }
    free(p);

    q = calloc(16, 16);
    CHECK(q != NULL);
    for (i = 0; i < 256; i++) {
        CHECK(q[i] == 0);
    }
    q = reallocarray(q, 8, 16);
    CHECK(q != NULL);
    cfree(q);

    p = aligned_alloc(64, 100);
    CHECK(p != NULL);
    CHECK(((uintptr_t)p % 64) == 0);
    free(p);

    errno = SENTINEL; rc = posix_memalign(&m, 8, 0);
    CHECK(rc == ENOMEM); CHECK(errno == SENTINEL);
    CHECK(m == (void *)0x1);
    rc = posix_memalign(&m, 64, 128);
    CHECK(rc == 0);
    CHECK(((uintptr_t)m % 64) == 0);
    free(m);

    rc = malloc_trim(0);
    CHECK(rc == 0 || rc == 1);
    p = malloc(32);
    CHECK(p != NULL);
    free(p);

    printf("test_z082_dyn: %s\n", fails == 0 ? "PASS" : "FAIL");
    return fails == 0 ? 0 : 1;
}
