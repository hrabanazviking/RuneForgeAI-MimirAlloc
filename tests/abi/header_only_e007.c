/* header_only_e007.c - E-007 header C11-clean proof TU.
 *
 * The C11 twin of header_only_e007.cpp: it includes the
 * public header include/mimir_alloc.h as C11 and takes the
 * addresses of the four declared functions into function
 * pointers of the exact declared types. Compiling it
 * cleanly under the banked C11 flags proves the header
 * still compiles as C11 in the same slice that proves it
 * C++-clean - the C++ proof costs the C surface nothing.
 * It is compiled to an object by `make test-e007`; it is
 * never linked or run. See tests/abi/recorded_run_e007.txt
 * S3 and docs/PHASE_5_CPP_CONSUMPTION_E007.md.
 */
#include "mimir_alloc.h"

#include <stddef.h>

void *(*const mimir_e007_probe_malloc)(size_t) = mimir_malloc;
void (*const mimir_e007_probe_free)(void *) = mimir_free;
void *(*const mimir_e007_probe_calloc)(size_t, size_t) = mimir_calloc;
void *(*const mimir_e007_probe_realloc)(void *, size_t) = mimir_realloc;
