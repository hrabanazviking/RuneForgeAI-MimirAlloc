# 10 — MimirAlloc requirements and clean-room boundary

MimirAlloc is a proposed independent allocator, not a renamed TCMalloc patch. The field investigation supplies requirements and test cases; it does not establish an implementation, a replacement strategy or compatibility with Mojo.

## Behavioral contract to specify

Before implementation, write and review the contracts for allocation, zero-size requests, alignment, deallocation, resize, arithmetic overflow, allocation failure, ownership, concurrent use and thread lifecycle. Distinguish the project-owned API from platform-specific libc interposition. If a C or C++ compatibility layer is proposed, document the ABI and behavioral requirements for the exact target; do not infer uniform behavior across all platforms.

The first implementation should favor auditable correctness, bounded metadata and explicit failure handling. Page-size assumptions must be verified or configured. Address-width assumptions, pointer tags and alignment masks need overflow-safe arithmetic and architecture-specific tests. A virtual-memory reserve is not the same as committed or resident physical pages. Resource-pressure behavior should avoid indefinite retry or unbounded queues.

## Allocator ownership and integration

Memory must be released by the compatible owner. A pointer returned by one allocator and freed by another can corrupt state. Shared libraries and compiler runtimes can allocate through hidden, static or internal symbols. Merely preloading a library or seeing exported `malloc` symbols does not prove that all calls use it. Map allocation paths and symbol resolution, including aligned and C++ allocation variants, before proposing substitution.

Start with a namespaced API and deterministic tests. Only consider process-wide replacement after proving the actual link and dispatch path on an isolated test binary. Test loaded libraries, runtime startup and teardown, threads, signal/fork behavior where applicable, and failure paths. A successful integration on one binary is scoped to its exact build and configuration.

## Clean-room and licensing

Record each source consulted, revision, license and purpose. Publicly visible TCMalloc behavior and the field notebook can inform independently written requirements and test vectors. Do not copy allocator implementation into a clean-room implementation. Where separation of investigation and implementation roles is practical, document the boundary; a single shared author who has read implementation sources should not claim a strict separation that did not occur. Follow the [engineering guide](../../ENGINEERING_GUIDE.md) and [MIT project license](../../../LICENSE).

## Resource and platform goals

A future design should account for small virtual-address spaces and low-memory devices without conflating them. It should expose instrumentation that distinguishes reserved, committed and resident memory where the operating system permits. It should respect Android app isolation, page-size variation, lifecycle and power/thermal constraints. Goals are not acceptance evidence; see [the test matrix](12_TEST_MATRIX_AND_EXPERIMENTS.md).
