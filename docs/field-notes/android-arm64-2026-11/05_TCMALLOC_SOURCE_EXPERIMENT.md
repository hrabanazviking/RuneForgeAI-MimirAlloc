# 05 — TCMalloc source experiment

## What was changed

An independent experiment modified a local checkout of Google TCMalloc at `12f255231938d30493186b0a037feedd70f5a1c1`. For the ARM64 Linux configuration only, the candidate allowed a build-time address-width value, retained 48 as the default, and rejected experimental values outside 39–48. The experiment was committed locally as `3390b10` on `experiment/aarch64-address-bits`.

This is an investigation of TCMalloc configuration, **not MimirAlloc implementation code**. The external code is governed by its own license. MimirAlloc's proposed independent implementation must follow its own [clean-room guide](../../ENGINEERING_GUIDE.md), source-provenance process and MIT licensing; do not silently copy this experiment into the clean-room allocator.

## Header-level test

On the tablet, a C++ smoke test included TCMalloc's configuration and memory-tag headers. It checked that:

- Without the override, the compile-time address width remained 48.
- With the override set to 39, the width was 39 and tag shift followed the checked expression.
- Overrides 38 and 49 were rejected with the intended static assertion.

The first two compiled and their tiny executables ran. These checks validate a limited header property. They do not instantiate the allocator, prove usable address reservations, exercise malloc/free or verify concurrency.

## Why the candidate is narrow

TCMalloc uses configuration constants in multiple structures. Every relevant translation unit and consumer of its internal headers must agree. A mixture of widths in one binary may make pointers impossible to classify correctly. A safe release would need a reproducible build graph showing consistent flags, ABI boundaries, stress tests, a supported target range and a clear identity for the artifact.

The candidate only covers the bounded experimental range; it is not a runtime detector, does not support every smaller address space, and has not been reviewed as an upstream patch. It was never linked into the installed Mojo compiler. A different allocator such as the one selected by current public Mojo source must be tested separately, including any remaining TCMalloc-linked dependencies.

## Acceptance gates

1. Compile the complete allocator and link a real program, with all configuration consistent.
2. Test zero-size, alignment, realloc, large/small allocations, arithmetic failure and invalid-use behavior according to the actual public contract.
3. Exercise simultaneous allocations and frees across threads, thread exit, stress, failures and teardown.
4. Verify actual addresses, page sizes and behavior on both 39-bit and standard larger-address systems.
5. Build and start the target compiler; compile and run programs; test memory pressure and device responsiveness.
6. Separately verify native Android/bionic output.

Header tests have passed; gates 1–6 remain open. [TCMalloc's source repository](https://github.com/google/tcmalloc) is the authoritative place to inspect its current implementation and license.
