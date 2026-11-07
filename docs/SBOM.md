# Software Bill of Materials - MimirAlloc

Slice M-008 (Phase 13). Inventory generated at base commit
6b312e3197cdb2e491ab0ffa24a4ba8ab95a1b17 by manual inspection of
this tree: the Makefile in full, every `#include` across `src/` and
`include/` classified by hand, `readelf -d` and `ldd` on the library
built in this slice's baseline run, an inventory of `scripts/` and
`tests/`, and the root tree listing. No tool was installed to
produce this document. The machine-readable rendering is
`docs/sbom.json` (SPDX-lite JSON; see the slice document for the
location reasoning). Component lists in the two renderings were
cross-checked identical before commit.

## Headline

The allocator itself is dependency-free at runtime beyond the C
library. `readelf -d build/libmimiralloc.so` in this slice's
recorded run shows exactly one DT_NEEDED entry: `libc.so.6`.
Third-party runtime libraries: NONE. That is the whole runtime
story, and it is evidenced, not asserted: see
`tests/abi/recorded_run_m008.txt`.

## The project itself

- Name: MimirAlloc (RuneForgeAI-MimirAlloc).
- Version: unversioned at this base; versioning is M-007 work in
  flight. No version number is guessed here.
- Licence: MIT (LICENSE at repository root, Copyright (c) 2026
  Volmarr Wyrd).
- Supplier: the repository hrabanazviking/RuneForgeAI-MimirAlloc.
- Shipped library components (file inventory from `src/` and
  `include/` at the base):
  - `include/mimir_alloc.h` - the public header; declares exactly
    mimir_malloc, mimir_free, mimir_calloc, mimir_realloc.
  - `src/api/mimir_alloc.c` - L1 API layer.
  - `src/api/mimir_alloc.map` - version script (anonymous node).
  - `src/core/mimir_core.c`, `src/core/mimir_core.h` - core.
  - `src/platform/mimir_platform.c`,
    `src/platform/mimir_platform.h` - L6 platform layer.

## Build-time dependencies (build environment, not redistributed)

- C compiler `cc`: gcc (Debian 12.2.0-14+deb12u1) 12.2.0, as
  recorded on this Pi in this slice's run. The Makefile requires a
  C11 compiler and libc headers; C++ consumer proof uses `c++`
  with -std=c++17 (same toolchain).
- GNU Make 4.3, as recorded on this Pi.
- POSIX shell and core utilities used by Makefile recipes and
  `scripts/` (no version asserted beyond the recorded host).

## Runtime dependencies

- `libc.so.6` (glibc 2.36 on the recorded host: Debian GLIBC
  2.36-9+rpt2+deb12u14) - the sole DT_NEEDED entry.
- The dynamic loader and `linux-vdso` shown by `ldd` are platform
  services of the host, not linked components.
- Include classification (every include in `src/` and `include/`):
  C standard library - stddef.h, stdint.h, stdlib.h, string.h,
  threads.h; POSIX / system - unistd.h, sys/mman.h, pthread.h
  (pthread is used via `-pthread` and the C library on this host;
  no separate libpthread DT_NEEDED appears); project-internal -
  mimir_alloc.h, mimir_core.h, mimir_platform.h. No third-party
  header appears anywhere in the shipped sources.

## Test and development dependencies

- The project's own test harnesses under `tests/` - no external
  test framework, no third-party test library.
- Mojo / pixi environment: used ONLY as the Phase 6 consumer-test
  environment (the Makefile's Mojo-guarded standalone targets build
  `tests/mojo/*.mojo` consumer programs when a `mojo` is on PATH).
  It is not a build dependency of the allocator and no Mojo code is
  in the shipped library. Its licence is recorded in banked
  documents as proprietary: LicenceRef-Modular-Proprietary. A reader
  of this SBOM should know the test environment is not all open.

## Explicitly NOT components

- Model files, the Raspberry Pi itself, Samtal and other household
  software sharing the development machine: not part of this
  project or its package.
- Root-tree non-package content: the repository root contains image
  and media files (photographs, generated images, and a personal
  essay, `why_i_hate_docker.md` with its image) that are repository
  content, NOT part of the allocator package. The package boundary
  is the library, its public header, and its build. A packager must
  not ship the root media as part of MimirAlloc, and must not read
  their presence as allocator components.

