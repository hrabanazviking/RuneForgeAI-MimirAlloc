/* mimir_alloc.c - Phase 3 foundation stub (slice C-001).
 *
 * This translation unit is a STUB. It implements no
 * allocator, acquires no memory, and publishes no block.
 * Its only jobs are to prove that include/mimir_alloc.h is
 * self-contained and linkable, and to give the smoke test a
 * documented not-implemented result to check.
 *
 * Stub behaviour, stated honestly and NOT as a contract:
 * - mimir_malloc returns NULL for every request. For a
 *   size > 0, representable request that is the shape of
 *   the contracts' defined failure channel (null, no
 *   partial block; MA-6.1-MALLOC-02 by analogy) and the
 *   B-008 S6.4 defined refusal for an unsatisfiable
 *   request. The stub satisfies no request, so NULL is
 *   also its not-implemented result. It NEVER returns a
 *   pointer to storage it did not allocate - it allocates
 *   nothing, so it can return nothing else honestly.
 * - For size == 0 the stub also returns NULL, but
 *   zero-size behaviour is [UNRESOLVED: U-09]: this return
 *   is a stub posture, not a contract claim, and no caller
 *   or test may cite it as settling U-09.
 * - mimir_free(NULL) performs the defined no-op
 *   (MA-6.1-FREE-02 by analogy). For any other pointer the
 *   stub does nothing and dereferences nothing: the stub
 *   owns no allocations, so no live in-scope pointer can
 *   exist, and passing a foreign or interior pointer is
 *   undefined behaviour under the contracts in any case
 *   (B-007 S D.3). The stub never forwards a pointer to
 *   any other allocator (no silent fallback, B-008 S7.1
 *   item 4).
 *
 * A real implementation replaces this file; this stub must
 * never be presented as an allocator or as Phase 3 exit
 * evidence.
 */
#include "mimir_alloc.h"

void *mimir_malloc(size_t size)
{
    (void)size;
    return NULL;
}

void mimir_free(void *ptr)
{
    (void)ptr;
}
