# Software Bill of Materials - MimirAlloc

REFRESHED at slice Z-105 (Z13 line) on base c3424b2 (banked
Z-001..Z-101), purpleraider. Original: slice M-008 (Phase 13)
at Pi base 6b312e3, preserved in git history. Refresh method
is M-008's, repeated on this tree: the Makefile in full,
every `#include` across `src/` and `include/` classified by
hand, `readelf -d` and `ldd` on the libraries built in this
slice's recorded run (tests/abi/recorded_run_z13_z105.txt),
an inventory of `scripts/` and `tests/`, and the root tree
listing. No tool was installed to produce this document. The
machine-readable rendering is `docs/sbom.json` (SPDX-lite
JSON; see the M-008 slice document for the location
reasoning). Component lists in the two renderings were
cross-checked identical before commit.

## Headline

The allocator itself is dependency-free at runtime beyond
the C library. `readelf -d build/libmimiralloc.so.0.1.0`
in this slice's recorded run shows DT_NEEDED entries
`libc.so.6` and (this host toolchain's direct loader
dependency) `ld-linux-x86-64.so.2`; `ldd` resolves only
libc plus the platform vdso. Third-party runtime
libraries: NONE. That is the whole runtime story, and it
is evidenced, not asserted: see
`tests/abi/recorded_run_z13_z105.txt` S3.

## The project itself

- Name: MimirAlloc (RuneForgeAI-MimirAlloc).
- Version: 0.1.0 (VERSION file at repository root, M-007
  decision D1). Decision D-017 (Z-105) assigns 0.2.0 to the
  next release cut; the in-tree version stays 0.1.0 until
  that cut executes the packet's checklist.
- Licence: MIT (LICENSE at repository root, Copyright (c)
  2026 Volmarr Wyrd).
- Supplier: the repository hrabanazviking/RuneForgeAI-MimirAlloc.
- Shipped library components (file inventory from `src/`
  and `include/` at this base):
  - `include/mimir_alloc.h` - the public header; declares
    exactly the 13 namespaced entry points (mimir_malloc,
    mimir_free, mimir_calloc, mimir_realloc,
    mimir_reallocarray, mimir_trim, mimir_aligned_alloc,
    mimir_posix_memalign, mimir_memalign, mimir_valloc,
    mimir_pvalloc, mimir_usable_size,
    mimir_stats_snapshot).
  - `src/api/mimir_alloc.c` - L1 API layer.
  - `src/api/mimir_alloc.map` - version script (anonymous
    node; exactly the 13 names above).
  - `src/api/mimir_interpose.c`,
    `src/api/mimir_interpose.map` - the OPT-IN interposing
    variant (Z-082, D-015): a separate library
    (libmimiralloc_interpose.so, exactly 20 exports: 10
    namespaced + 10 standard names). Its namespaced set is
    the Z-082-era 10 - see F-Z105-1 in
    docs/Z13_Z105_RELEASE_ENGINEERING.md.
  - `src/core/mimir_core.c`, `src/core/mimir_core.h` - core.
  - `src/platform/mimir_platform.c`,
    `src/platform/mimir_platform.h` - L6 platform layer.

## Build-time dependencies (build environment, not redistributed)

- C compiler `cc`: gcc (Ubuntu 15.3.0-4ubuntu1) 15.3.0, as
  recorded on purpleraider in this slice's run. The
  Makefile requires a C11 compiler and libc headers; C++
  consumer proofs use `c++` with -std=c++17 (same
  toolchain). Build flags: the banked Makefile set,
  -O3 per Z-096.
- GNU Make 4.4.1, as recorded on this host.
- POSIX shell and core utilities used by Makefile recipes
  and `scripts/` (no version asserted beyond the recorded
  host).

## Runtime dependencies

- `libc.so.6` (glibc 2.44 on the recorded host: Ubuntu
  GLIBC 2.44-1ubuntu2) - the sole library DT_NEEDED entry
  besides the loader entry named in the Headline.
- The dynamic loader and `linux-vdso` shown by `ldd` are
  platform services of the host, not linked components.
- Include classification (every include in `src/` and
  `include/`): C standard library - stddef.h, stdint.h,
  stdlib.h, string.h, threads.h, errno.h, time.h,
  stdio.h; POSIX / system - unistd.h, sys/mman.h,
  pthread.h (via `-pthread` and the C library; no separate
  libpthread DT_NEEDED), sched.h, x86intrin.h (this
  arch); project-internal - mimir_alloc.h, mimir_core.h,
  mimir_platform.h, plus the compile-time contract header
  tests/contract/mimir_tls_cache_z028.h included by
  src/core/mimir_core.c for its _Static_assert class-count
  contract (Z-029; assertion only, no test code ships).
  No third-party header appears anywhere in the shipped
  sources.

## Test and development dependencies

- The project's own test harnesses under `tests/` - no
  external test framework, no third-party test library.
- Mojo / pixi environment: used ONLY as the Phase 6
  consumer-test environment (the Makefile's Mojo-guarded
  standalone targets build `tests/mojo/*.mojo` consumer
  programs when a `mojo` is on PATH). It is not a build
  dependency of the allocator and no Mojo code is in the
  shipped library. Its licence is recorded in banked
  documents as proprietary: LicenceRef-Modular-Proprietary.
  A reader of this SBOM should know the test environment
  is not all open.

## Explicitly NOT components

- Model files, the host machines themselves, and other
  household software sharing the development machines:
  not part of this project or its package.
- Root-tree non-package content: the repository root
  contains image and media files (photographs, generated
  images, and a personal essay, `why_i_hate_docker.md`
  with its image) that are repository content, NOT part of
  the allocator package. The package boundary is the
  library, its public header, and its build. A packager
  must not ship the root media as part of MimirAlloc, and
  must not read their presence as allocator components.
