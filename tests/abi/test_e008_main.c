/* test_e008_main.c - E-008 cross-module ownership consumer
 * ("module A", Phase 5, Wave 5).
 *
 * This executable links against TWO shared libraries:
 * build/libmimiralloc.so (the allocator) and
 * build/libe008peer.so (the peer test module,
 * tests/abi/test_e008_peer.c). Both this executable and
 * the peer carry their own DT_NEEDED entry for
 * libmimiralloc.so and their own undefined mimir_*
 * references; the dynamic loader binds both modules'
 * references to the ONE libmimiralloc.so instance mapped
 * into the process (DT_NEEDED / ldd / LD_DEBUG=bindings
 * evidence in tests/abi/recorded_run_e008.txt). That one
 * instance is what "same instance" means in this proof:
 * there is exactly one registry, one lock and one set of
 * mappings behind both modules' calls, so a block either
 * module allocates is a live block of the same allocator
 * the other module reallocs and frees.
 *
 * Lifecycles proven here, contents checked byte-for-byte
 * at every hand-off:
 * - Startup: the peer constructor's block, allocated
 *   before main inside the peer, is verified in main,
 *   main writes its sentinel into the head, the peer
 *   verifies the combined contents from its side, and
 *   the peer destructor frees the block after main
 *   (its printed line is the post-main evidence).
 * - Direction A (main -> peer): main allocates, the
 *   peer fills, main verifies, the peer reallocs (grow),
 *   main verifies the preserved prefix and fills the
 *   tail, the peer verifies both regions, the peer frees.
 * - Direction B (peer -> main): the peer allocates, main
 *   fills and verifies, main reallocs (grow), main
 *   verifies the preserved prefix byte-for-byte, the peer
 *   verifies a uniform tail main wrote, main frees.
 * - calloc in both directions: peer calloc verified
 *   zeroed in main, filled in main, verified in the peer,
 *   freed in the peer; main calloc verified zeroed by the
 *   peer, realloc'd (shrink) in the peer with the prefix
 *   preserved, freed in main.
 *
 * Defined cases only: zero-size is NOT exercised
 * ([UNRESOLVED: U-09]), realloc-to-zero is NOT exercised
 * (U-12), errno is NOT checked (U-10). NO foreign pointer
 * is ever passed to a mimir_* entry point in this
 * program: mixed-allocator ownership is undefined
 * behaviour with no defined rejection (header, by analogy
 * to MA-6.1-FREE-03/-04); the honest probes for it live
 * in the separate tests/abi/test_e008_mixed.c child
 * process. Behaviour is by analogy under
 * [UNRESOLVED: U-03], exactly as the header states.
 *
 * Standalone on purpose: built and run via
 * make test-e008; NOT part of make test. See
 * docs/PHASE_5_STARTUP_OWNERSHIP_E008.md.
 */
#include "mimir_alloc.h"
#include "test_e008_peer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int passed = 0;
static int failed = 0;

static void check(int condition, const char *name)
{
    if (condition) {
        passed++;
        printf("PASS %s\n", name);
    } else {
        failed++;
        printf("FAIL %s\n", name);
    }
}

static int all_uniform(const unsigned char *p, size_t n,
                       unsigned char pat)
{
    size_t i;
    if (p == NULL) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (p[i] != pat) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    unsigned char *ctor;
    unsigned char *a;
    unsigned char *g;
    unsigned char *p;
    unsigned char *r;
    unsigned char *z;
    unsigned char *w;
    unsigned char *w2;
    unsigned char *fresh;
    size_t i;
    int ok;

    printf("E-008 cross-module ownership consumer "
           "(main module + peer module, one libmimiralloc.so)\n");

    /* --- Startup ordering (b): the peer constructor's
     * block, allocated before main in the peer module. */
    ctor = (unsigned char *)e008_peer_ctor_block();
    check(ctor != NULL,
          "peer ctor block (allocated before main) is non-NULL in main");
    if (ctor != NULL) {
        ok = 1;
        for (i = 0; i < E008_CTOR_BLOCK_SIZE; i++) {
            if (ctor[i] != e008_peer_ctor_pattern(i)) { ok = 0; }
        }
        check(ok, "ctor block contents match peer pattern byte-for-byte in main");
        check(((uintptr_t)ctor % _Alignof(max_align_t)) == 0,
              "ctor block aligned to _Alignof(max_align_t)");
        memset(ctor, E008_MAIN_SENTINEL, E008_CTOR_SENTINEL_LEN);
        check(e008_peer_check_ctor_block(),
              "peer verifies ctor block (main sentinel + peer pattern) from peer module");
    }

    /* --- Direction A: allocate in main, fill / realloc /
     * free in the peer. */
    a = mimir_malloc(96);
    check(a != NULL, "A: mimir_malloc(96) in main returns non-NULL");
    if (a != NULL && ctor != NULL) {
        check(a != ctor, "A: main block distinct from live ctor block");
    }
    if (a != NULL) {
        e008_peer_fill(a, 96, 0x41);
        check(all_uniform(a, 96, 0x41),
              "A: peer-filled pattern verified byte-for-byte in main");
        g = (unsigned char *)e008_peer_realloc(a, 240);
        check(g != NULL, "A: mimir_realloc to 240 executed in peer returns non-NULL");
        if (g != NULL) {
            check(all_uniform(g, 96, 0x41),
                  "A: realloc in peer preserves first 96 bytes (checked in main)");
            memset(g + 96, 0x42, 240 - 96);
            check(e008_peer_check_uniform(g, 96, 0x41),
                  "A: peer verifies preserved prefix from peer module");
            check(e008_peer_check_uniform(g + 96, 240 - 96, 0x42),
                  "A: peer verifies main-filled tail from peer module");
            e008_peer_free(g);
            check(1, "A: mimir_free of the block executed in peer returns normally");
        } else {
            mimir_free(a);
        }
    }
    fresh = mimir_malloc(48);
    check(fresh != NULL, "A: allocator healthy in main after peer free");
    if (fresh != NULL) {
        memset(fresh, 0x2D, 48);
        check(fresh[0] == 0x2D && fresh[47] == 0x2D,
              "A: fresh block writable over full extent");
        mimir_free(fresh);
    }

    /* --- Direction B: allocate in the peer, fill /
     * realloc / free in main. */
    p = (unsigned char *)e008_peer_alloc(80);
    check(p != NULL, "B: mimir_malloc(80) executed in peer returns non-NULL");
    if (p != NULL && ctor != NULL) {
        check(p != ctor, "B: peer block distinct from live ctor block");
    }
    if (p != NULL) {
        for (i = 0; i < 80; i++) {
            p[i] = (unsigned char)((i * 3u + 1u) & 0xFFu);
        }
        r = mimir_realloc(p, 200);
        check(r != NULL, "B: mimir_realloc to 200 in main returns non-NULL");
        if (r != NULL) {
            ok = 1;
            for (i = 0; i < 80; i++) {
                if (r[i] != (unsigned char)((i * 3u + 1u) & 0xFFu)) { ok = 0; }
            }
            check(ok, "B: realloc in main preserves peer-allocated prefix byte-for-byte");
            memset(r + 80, 0x99, 200 - 80);
            check(e008_peer_check_uniform(r + 80, 200 - 80, 0x99),
                  "B: peer verifies main-filled tail of its own block");
            mimir_free(r);
            check(1, "B: mimir_free of peer-allocated block in main returns normally");
        } else {
            mimir_free(p);
        }
    }

    /* --- calloc across the boundary, both directions. */
    z = (unsigned char *)e008_peer_calloc(10, 16); /* 160 bytes */
    check(z != NULL, "C: mimir_calloc(10, 16) executed in peer returns non-NULL");
    if (z != NULL) {
        check(all_uniform(z, 160, 0),
              "C: peer calloc storage reads zero at return (checked in main)");
        memset(z, 0x77, 160);
        check(e008_peer_check_uniform(z, 160, 0x77),
              "C: peer verifies main-filled calloc block from peer module");
        e008_peer_free(z);
        check(1, "C: peer frees its calloc block after main use, returns normally");
    }
    w = mimir_calloc(8, 24); /* 192 bytes */
    check(w != NULL, "C: mimir_calloc(8, 24) in main returns non-NULL");
    if (w != NULL) {
        check(e008_peer_check_uniform(w, 192, 0),
              "C: peer verifies main calloc storage zeroed, from peer module");
        memset(w, 0x11, 192);
        w2 = (unsigned char *)e008_peer_realloc(w, 64);
        check(w2 != NULL, "C: mimir_realloc shrink to 64 executed in peer returns non-NULL");
        if (w2 != NULL) {
            check(all_uniform(w2, 64, 0x11),
                  "C: peer realloc shrink preserves first 64 bytes (checked in main)");
            mimir_free(w2);
        } else {
            mimir_free(w);
        }
    }

    /* --- Cross-module no-op + final health. */
    e008_peer_free(NULL);
    check(1, "mimir_free(NULL) executed in peer returns normally (defined no-op)");
    mimir_free(NULL);
    check(1, "mimir_free(NULL) in main returns normally (defined no-op)");
    fresh = (unsigned char *)e008_peer_alloc(64);
    check(fresh != NULL, "final: peer allocation after all cross-module frees is non-NULL");
    if (fresh != NULL) {
        e008_peer_fill(fresh, 64, 0xE8);
        check(all_uniform(fresh, 64, 0xE8),
              "final: peer-filled block verified in main");
        mimir_free(fresh);
    }
    if (ctor != NULL) {
        check(e008_peer_check_ctor_block(),
              "ctor block still valid in main at end of main (freed later by peer destructor)");
    }

    printf("E008-MAIN %d/%d checks passed\n", passed, passed + failed);
    printf("RESULT: %s\n", failed == 0 ? "PASS" : "FAIL");
    fflush(stdout);
    return failed == 0 ? 0 : 1;
}
