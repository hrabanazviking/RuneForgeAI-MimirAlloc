/* poison_z089.c - Z-089 deliberately broken allocator.
 *
 * This is a DRILL POISON, not a product artifact: a tiny
 * shared object whose malloc family always fails. It is
 * built by tests/abi/run_z089.sh into a private temp
 * directory at drill time and is LD_PRELOADed ONLY into
 * harness processes the drill itself spawns
 * (tests/abi/harness_z089.c), to create an honest broken
 * state from which the one-variable rollback is proven.
 * It must NEVER be preloaded into any real program,
 * service, display manager, systemd, or banking tool:
 * every allocation it is offered returns NULL, so any
 * real subject would fail exactly as the harness does.
 * free() is a no-op: the poison never hands out a real
 * pointer, so there is nothing it may legitimately free,
 * and pointers from other allocators must never be
 * freed through it. */
#include <stddef.h>

void *malloc(size_t size)
{
    (void)size;
    return NULL;
}

void *calloc(size_t nmemb, size_t size)
{
    (void)nmemb;
    (void)size;
    return NULL;
}

void *realloc(void *ptr, size_t size)
{
    (void)ptr;
    (void)size;
    return NULL;
}

void free(void *ptr)
{
    (void)ptr;
}
