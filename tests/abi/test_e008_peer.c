/* test_e008_peer.c - E-008 peer module ("module B").
 *
 * Built as build/libe008peer.so, a shared library of its
 * own, linked against build/libmimiralloc.so. Every
 * allocator operation below calls the public mimir_*
 * entry points from inside THIS module. The main consumer
 * (test_e008_main.c) performs the complementary half of
 * each lifecycle from inside the executable module, so a
 * block's allocation, fill, realloc and free execute in
 * different ELF modules against the one library instance.
 *
 * Startup fixture: the constructor below runs before
 * main (the dynamic loader initialises DT_NEEDED
 * libraries before transferring control to the
 * executable's entry point). It allocates through
 * mimir_malloc with NO prior allocator call of any kind
 * in this module and no setup call anywhere - the banked
 * design fact E-008 tests is that the allocator needs
 * none (statically initialised lock and registry,
 * docs/PHASE_5_FOUNDATION_PLAN.md Section 2). The
 * destructor runs after main returns: it verifies the
 * block, including the sentinel main wrote, and frees
 * it through mimir_free from inside this module.
 *
 * The ctor/dtor printf lines are ordering evidence: in
 * the recorded run the CTOR line precedes every line of
 * main's output and the DTOR line follows main's RESULT
 * line. The destructor cannot return a status to anyone,
 * so its verdict exists only as that printed line
 * (INC-E008-03).
 *
 * Standalone on purpose: built and run via
 * make test-e008; NOT part of make test. See
 * docs/PHASE_5_STARTUP_OWNERSHIP_E008.md.
 */
#include "test_e008_peer.h"
#include "mimir_alloc.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static unsigned char *g_ctor_block;

unsigned char e008_peer_ctor_pattern(size_t i)
{
    return (unsigned char)((i * 7u + 0x31u) & 0xFFu);
}

__attribute__((constructor))
static void e008_peer_ctor(void)
{
    size_t i;
    g_ctor_block = mimir_malloc(E008_CTOR_BLOCK_SIZE);
    if (g_ctor_block != NULL) {
        for (i = 0; i < E008_CTOR_BLOCK_SIZE; i++) {
            g_ctor_block[i] = e008_peer_ctor_pattern(i);
        }
    }
    printf("E008 PEER CTOR: mimir_malloc(%u) before main -> %s\n",
           (unsigned)E008_CTOR_BLOCK_SIZE,
           g_ctor_block != NULL ? "non-NULL, pattern filled" : "NULL (FAIL)");
    fflush(stdout);
}

__attribute__((destructor))
static void e008_peer_dtor(void)
{
    int ok = e008_peer_check_ctor_block();
    if (g_ctor_block != NULL) {
        mimir_free(g_ctor_block);
        g_ctor_block = NULL;
    }
    printf("E008 PEER DTOR: after main, ctor block contents %s, "
           "freed via mimir_free in peer module\n",
           ok ? "verified byte-for-byte" : "MISMATCH (FAIL)");
    fflush(stdout);
}

void *e008_peer_ctor_block(void)
{
    return g_ctor_block;
}

void e008_peer_fill(void *p, size_t n, unsigned char pat)
{
    if (p != NULL && n > 0) {
        memset(p, pat, n);
    }
}

int e008_peer_check_uniform(const void *p, size_t n,
                            unsigned char pat)
{
    const unsigned char *b = (const unsigned char *)p;
    size_t i;
    if (b == NULL) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (b[i] != pat) {
            return 0;
        }
    }
    return 1;
}

int e008_peer_check_ctor_block(void)
{
    size_t i;
    if (g_ctor_block == NULL) {
        return 0;
    }
    for (i = 0; i < E008_CTOR_SENTINEL_LEN; i++) {
        if (g_ctor_block[i] != (unsigned char)E008_MAIN_SENTINEL) {
            return 0;
        }
    }
    for (i = E008_CTOR_SENTINEL_LEN; i < E008_CTOR_BLOCK_SIZE; i++) {
        if (g_ctor_block[i] != e008_peer_ctor_pattern(i)) {
            return 0;
        }
    }
    return 1;
}

void *e008_peer_alloc(size_t n)
{
    return mimir_malloc(n);
}

void *e008_peer_calloc(size_t nmemb, size_t size)
{
    return mimir_calloc(nmemb, size);
}

void *e008_peer_realloc(void *p, size_t n)
{
    return mimir_realloc(p, n);
}

void e008_peer_free(void *p)
{
    mimir_free(p);
}
