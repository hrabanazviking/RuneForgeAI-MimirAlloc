// header_only_z075.cpp - Z-075 header self-containment
// proof from C++ (E-007 precedent): includes ONLY the
// public header and takes the Z-075 function's address
// with its exact C signature, proving the declaration
// sits inside the header's extern "C" guards with the
// const void * prototype intact (a mangled or mis-typed
// declaration would fail this assignment). Compile-only;
// built by `make test-z075`, never run.
#include "mimir_alloc.h"

size_t (*const z075_ref_usable)(const void *) =
    &mimir_usable_size;
