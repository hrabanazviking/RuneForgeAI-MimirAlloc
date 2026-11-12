/*
 * mimir_aligned_z073.h -- Z-073 proposed aligned-allocation
 * surface (contract first, Wave 9 opener).
 *
 * This header is the CONTRACT SURFACE ONLY. It is test-only:
 * it is NOT include/mimir_alloc.h, it claims no ABI, and no
 * symbol declared here exists in the core yet (Z-074 is the
 * implementation slice; it promotes these declarations to
 * the public header when it lands the code). Contract text:
 * docs/Z13_ALIGNED_ALLOCATION_CONTRACT.md, written in
 * docs/contracts.md style (rows AL-073-*).
 *
 * The surface (two entries, namespaced, C ABI):
 *
 *   void *mimir_aligned_alloc(size_t alignment, size_t size);
 *     aligned_alloc-shaped. Valid alignment: any power of
 *     two (alignment >= 1); alignments below the 16-byte
 *     floor are promoted to the floor, never refused.
 *     Size: any size >= 1; size need NOT be a multiple of
 *     alignment (decision D-Z073-2, settling U-11's second
 *     half for this surface; glibc 2.44 on this host does
 *     not enforce the multiple either - measured). Success:
 *     non-null pointer, ptr % alignment == 0, at least size
 *     writable bytes, freeable by mimir_free exactly once.
 *     Failure (invalid alignment, unsatisfiable or
 *     overflowing size, interim zero size): exactly NULL,
 *     no allocation, no other live block disturbed; errno
 *     is NOT promised (U-10, owned by Z-078).
 *
 *   int mimir_posix_memalign(void **memptr, size_t alignment,
 *                            size_t size);
 *     posix_memalign-shaped. Valid alignment: a power of
 *     two AND a multiple of sizeof(void *) (POSIX form;
 *     glibc 2.44 on this host measured identical). Success:
 *     returns 0 and stores the aligned pointer in *memptr.
 *     Invalid alignment or memptr == NULL: returns EINVAL
 *     and leaves *memptr unchanged. Unsatisfiable or
 *     overflowing size (and interim zero size): returns
 *     ENOMEM and leaves *memptr unchanged. The return value
 *     is the only error channel; errno is not the channel
 *     (settles U-14 for this surface on this target).
 */
#ifndef MIMIR_ALIGNED_Z073_H
#define MIMIR_ALIGNED_Z073_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *mimir_aligned_alloc(size_t alignment, size_t size);
int mimir_posix_memalign(void **memptr, size_t alignment, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* MIMIR_ALIGNED_Z073_H */
