/* header_only_e007.cpp - E-007 header C++-clean proof TU.
 *
 * This translation unit does exactly one thing: it includes
 * the public header include/mimir_alloc.h as C++ and takes
 * the addresses of the four declared functions into
 * function pointers of the exact declared types. Compiling
 * it cleanly under the slice's strict C++ flags proves the
 * header is C++-clean standalone: the extern "C" guards are
 * present and well-formed, the declarations carry C
 * linkage, and the declared signatures are usable from C++
 * with no header change. It is compiled to an object by
 * `make test-e007`; it is never linked or run - the
 * lifecycle consumer is test_e007.cpp. See
 * tests/abi/recorded_run_e007.txt S3 and
 * docs/PHASE_5_CPP_CONSUMPTION_E007.md.
 */
#include "mimir_alloc.h"

#include <cstddef>

namespace mimir_e007_header_probe {

using MallocFn = void *(*)(std::size_t);
using FreeFn = void (*)(void *);
using CallocFn = void *(*)(std::size_t, std::size_t);
using ReallocFn = void *(*)(void *, std::size_t);

/* Taking each address into a pointer of the declared type
 * only compiles if the declaration is visible from C++ with
 * exactly that signature and C linkage. */
MallocFn const malloc_fn = &mimir_malloc;
FreeFn const free_fn = &mimir_free;
CallocFn const calloc_fn = &mimir_calloc;
ReallocFn const realloc_fn = &mimir_realloc;

} // namespace mimir_e007_header_probe
