// header_only_z077.cpp - Z-077 header self-containment
// proof from C++ (E-007 precedent): includes ONLY the
// public header and takes the two Wave 9 functions'
// addresses with their exact C signatures, proving the
// declarations sit inside the header's extern "C" guards
// (a mangled or mis-typed declaration would fail these
// assignments). Compile-only; built by `make test-z077`,
// never run.
#include "mimir_alloc.h"

int (*const z077_ref_trim)(size_t) = &mimir_trim;
void *(*const z077_ref_reallocarray)(void *, size_t,
                                     size_t) =
    &mimir_reallocarray;
