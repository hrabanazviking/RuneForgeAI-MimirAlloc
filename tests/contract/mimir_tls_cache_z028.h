/* mimir_tls_cache_z028.h - TEST-ONLY declarations for Z-028.
 *
 * Slice Z-028 (docs/Z13_TLS_CACHE_CONTRACT.md): the contract
 * surface a per-thread freelist cache (TLS cache) must
 * provide when Z-029 implements it over the banked size
 * classes. This header lives under tests/contract/ on
 * purpose. It is NOT installed, NOT part of the public
 * surface (include/mimir_alloc.h still declares exactly the
 * four public entry points), and NOT an ABI: it is the
 * INC-E001-07 shape (b) test-only redeclaration pattern used
 * by every internal surface in this tree (drain, trim, tune,
 * stats, integrity). Z-029 must define these symbols in the
 * core object so a test-only static build linking the
 * library OBJECTS directly can read them; the shared
 * library binds them local, so nm -D on libmimiralloc.so
 * stays exactly 4.
 *
 * Contract constants (fixed by Z-028; Z-029 may not retune
 * them without amending the contract doc and this header in
 * the same commit):
 * - Classes cached: all 20 banked small classes, requests
 *   1..1024 bytes (src/core/mimir_core.c class table).
 *   Requests above the 1024-byte cutoff are never cached.
 * - Bin capacity: 16 blocks per class per thread. Ground:
 *   the banked mixed_batch workload holds 256 live blocks
 *   cycling all 20 classes (~13 per class); 16 is the
 *   smallest power of two that holds one such burst per
 *   class. A bound, not a performance claim.
 * - Refill / drain batch: 8 blocks (half a bin) per central
 *   core transaction on miss / on bin-full local free.
 * - Worst-case cached free bytes per thread:
 *   16 * (16+32+48+64+80+96+112+128+160+192+224+256+320+
 *   384+448+512+640+768+896+1024) = 16 * 6400 = 102400 bytes,
 *   in blocks that stay inside their spans; span retention
 *   stays governed by the G-007 bound, unchanged.
 */
#ifndef MIMIR_TLS_CACHE_Z028_H
#define MIMIR_TLS_CACHE_Z028_H

#include <stddef.h>
#include <stdint.h>

#define MIMIR_TLS_CLASS_COUNT 20
#define MIMIR_TLS_BIN_CAPACITY ((size_t)16)
#define MIMIR_TLS_BATCH ((size_t)8)

/* Per-calling-thread snapshot. All counters are monotone
 * since thread start except the cached_now gauges. A
 * snapshot describes ONLY the calling thread's cache:
 * no field may be written by any other thread (Z-016
 * single-writer discipline; see contract S5). */
struct mimir_tls_cache_snapshot {
    uint64_t cached_blocks;        /* sum of per_class_cached */
    uint64_t cached_bytes;         /* sum per_class*class_size */
    uint64_t per_class_cached[MIMIR_TLS_CLASS_COUNT];
    uint64_t hits;                 /* alloc served from bin */
    uint64_t misses;               /* alloc not served from bin */
    uint64_t refills;              /* batched refills from core */
    uint64_t refill_blocks;        /* blocks moved by refills */
    uint64_t drains;               /* batched drains to core */
    uint64_t drain_blocks;         /* blocks moved by drains */
    uint64_t local_frees;          /* own frees entering bin */
    uint64_t local_frees_to_core;  /* own frees bypassing a full bin */
    uint64_t remote_frees_out;     /* frees of blocks owned by another thread, routed to the central core */
    uint64_t calloc_served;        /* calloc allocations routed through the cache path */
};

/* 0 on success, -1 on NULL out. Callable from any thread;
 * the result describes the calling thread only. Taking a
 * snapshot must not allocate, must not take the central
 * lock, and must not move any counter except by the
 * caller's own prior actions. */
int mimir_tls_cache_snapshot(struct mimir_tls_cache_snapshot *out);

/* Bin capacity for class_index (0..19): exactly
 * MIMIR_TLS_BIN_CAPACITY. 0 for any out-of-range index
 * (including the above-cutoff space: there is no bin). */
size_t mimir_tls_cache_bin_capacity(size_t class_index);

/* Refill/drain batch for class_index (0..19): exactly
 * MIMIR_TLS_BATCH. 0 for any out-of-range index. */
size_t mimir_tls_cache_batch(size_t class_index);

/* Drain the calling thread's bins to the central core now:
 * every cached-but-free block is returned exactly once;
 * after the call the calling thread's cached_blocks is 0.
 * Live blocks are never touched. Idempotent. */
void mimir_tls_cache_drain_thread(void);

/* Process-global monotone counters of thread-exit drains
 * (TLS teardown drains, contract S6): how many thread
 * caches have been drained at exit, and how many blocks
 * those drains returned. Readable from any thread at a
 * quiescent point; written under the central lock. */
uint64_t mimir_tls_cache_exit_drains(void);
uint64_t mimir_tls_cache_exit_drain_blocks(void);

#endif /* MIMIR_TLS_CACHE_Z028_H */
