/* test_e008_peer.h - E-008 peer module interface (Phase 5, Wave 5).
 *
 * The peer module is a SEPARATE shared library,
 * build/libe008peer.so, built from tests/abi/test_e008_peer.c.
 * It is the "module B" of the E-008 cross-module ownership
 * proof: it links against build/libmimiralloc.so with its
 * own DT_NEEDED entry, exactly as the main consumer
 * (tests/abi/test_e008_main.c, "module A") does, so both
 * modules' mimir_* references must bind to the one shared
 * library instance in the process (binding evidence in
 * tests/abi/recorded_run_e008.txt).
 *
 * The functions below are TEST-ONLY plumbing between the
 * two test modules. They are not part of MimirAlloc: they
 * live only in the test peer library, are declared in no
 * installed header, and libmimiralloc.so neither knows nor
 * exports any of them (nm -D on libmimiralloc.so stays
 * exactly the four public symbols). Each peer function
 * performs its named operation by calling the public
 * mimir_* entry points FROM INSIDE the peer module, which
 * is what makes the cross-module lifecycle a real proof:
 * allocation, fill, realloc and free genuinely execute in
 * different ELF modules while ownership passes between
 * them through the one allocator instance.
 *
 * The peer also owns the startup-ordering fixture: its
 * constructor allocates a block through mimir_malloc
 * BEFORE main runs and fills it with the pattern
 * e008_peer_ctor_pattern(); its destructor verifies the
 * block (including the sentinel the main module writes
 * into its first 32 bytes) and frees it AFTER main
 * returns. See docs/PHASE_5_STARTUP_OWNERSHIP_E008.md.
 */
#ifndef TEST_E008_PEER_H
#define TEST_E008_PEER_H

#include <stddef.h>

/* Size of the constructor-allocated block (bytes). */
#define E008_CTOR_BLOCK_SIZE 128u
/* Bytes at the head of the ctor block the main module
 * overwrites with E008_MAIN_SENTINEL. */
#define E008_CTOR_SENTINEL_LEN 32u
#define E008_MAIN_SENTINEL 0x5Cu

/* Expected ctor-block byte at index i (for i >= the
 * sentinel length, and for every i before main writes
 * its sentinel). Pure function of i, so either module
 * can verify the block byte-for-byte. */
unsigned char e008_peer_ctor_pattern(size_t i);

/* The constructor-allocated block, or NULL if the ctor
 * allocation failed. Owned by the peer: the main module
 * may read and write it but must NOT free it; the peer
 * destructor frees it after main. */
void *e008_peer_ctor_block(void);

/* Fill n bytes at p with pat, executed in the peer. */
void e008_peer_fill(void *p, size_t n, unsigned char pat);

/* Return 1 iff the n bytes at p all equal pat, checked
 * in the peer; 0 otherwise (including p == NULL). */
int e008_peer_check_uniform(const void *p, size_t n,
                            unsigned char pat);

/* Return 1 iff the ctor block currently reads: sentinel
 * bytes in [0, E008_CTOR_SENTINEL_LEN) and ctor pattern
 * in the rest; checked in the peer. */
int e008_peer_check_ctor_block(void);

/* Allocate through the public API from inside the peer. */
void *e008_peer_alloc(size_t n);
void *e008_peer_calloc(size_t nmemb, size_t size);

/* Reallocate / free through the public API from inside
 * the peer. e008_peer_free(NULL) is the defined no-op. */
void *e008_peer_realloc(void *p, size_t n);
void e008_peer_free(void *p);

#endif /* TEST_E008_PEER_H */
