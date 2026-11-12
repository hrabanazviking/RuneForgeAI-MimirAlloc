// header_only_z074.cpp - Z-074 header self-containment
// proof from C++ (E-007 precedent): includes ONLY the
// public header and takes the two Wave 9 functions'
// addresses with their exact C signatures, proving the
// declarations sit inside the header's extern "C" guards
// with the POSIX-shaped prototype intact (a mangled or
// mis-typed declaration would fail this assignment).
// Compile-only; built by `make test-z074`, never run.
#include "mimir_alloc.h"

void *(*const z074_ref_aligned)(size_t, size_t) =
    &mimir_aligned_alloc;
int (*const z074_ref_posix)(void **, size_t, size_t) =
    &mimir_posix_memalign;
