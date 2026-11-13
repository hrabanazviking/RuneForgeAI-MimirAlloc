// header_only_z083.cpp - Z-083 header self-containment
// proof from C++ (E-007 precedent): includes ONLY the
// public header and takes the three memalign-family
// functions' addresses with their exact C signatures,
// proving the declarations sit inside the header's
// extern "C" guards. Compile-only; built by
// `make test-z083`, never run.
#include "mimir_alloc.h"

void *(*const z083_ref_memalign)(size_t, size_t) =
    &mimir_memalign;
void *(*const z083_ref_valloc)(size_t) = &mimir_valloc;
void *(*const z083_ref_pvalloc)(size_t) = &mimir_pvalloc;
