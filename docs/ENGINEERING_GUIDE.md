# MimirAlloc Engineering Guide

This guide is for anyone exploring or contributing to the project. **Status: documentation and planning only.** There is no allocator implementation, release, build system, or runnable test suite in this repository yet.

## Start here

1. Read the [project README](../README.md) for the vision and current status.
2. Read the full [research and development roadmap](MimirAlloc_Research_and_Development_Roadmap.md) for design options, evidence gates, a testing matrix, and primary references.
3. Check existing issues and discuss concrete findings with the project. If creating an issue, include the target and observable behavior.

## Reproduce the motivating issue

Record the exact Mojo and dependency revisions, build command, OS, CPU architecture, libc, compiler, linker, build tool, and the first relevant error. Note whether the failing environment is native Android/bionic, Termux, Android `proot`, or ARM64 Linux; these are distinct environments.

Identify **which process** is failing: build tool, compiler, linked runtime, generated binary, or application. Capture a minimal reproduction and inspect the actual linkage before choosing a replacement strategy. Avoid publishing private paths, credentials, access tokens, device serial numbers, or private prompts.

A report might include:

```text
Target and environment:
Project and revision:
Build command:
Expected result:
Observed result and first relevant error:
Allocator revision and link mode, if known:
Reproduction steps:
```

## Important technical distinctions

- TCMalloc documents Linux AArch64 support. A device-specific or build-specific failure still needs its own evidence.
- AArch64 Linux and Android ARM64 use different operating-system ABIs and libc environments.
- A successful `proot` test does not prove a native Android application works.
- Dynamic symbol interposition is not guaranteed to replace static, hidden, or explicitly linked allocator calls.
- C allocation behavior can vary by platform, especially around zero-size allocations, alignment, `errno`, and `realloc(p, 0)`.
- Users of separate allocators must respect pointer ownership; allocating with one and freeing with another can corrupt memory.

## Design and clean-room process

Write requirements and a behavioral contract in original words before implementing. Maintain a source and license ledger; do not copy implementation code into an independent implementation. Separate investigation and implementation roles when practical, and document the limits of any clean-room process. The [roadmap](MimirAlloc_Research_and_Development_Roadmap.md) has a more complete protocol.

Begin with checked arithmetic, explicit alignment, clear ownership, a correct simple concurrency design, and predictable failure paths. Optimize only after profiling and correctness tests exist. Preserve evidence about what was tested on physical devices.

## Proposed first work

1. Capture and classify the Mojo/ARM failure.
2. Map the actual allocator symbols and link paths.
3. Independently compare available configuration or allocator options to establish a baseline.
4. Specify the needed API and its target-specific behavior.
5. Implement an explicitly experimental, namespaced allocator and contract tests.
6. Demonstrate actual integration and measure it on real ARM hardware.

These are planned tasks, not current capabilities. Do not add hypothetical install, benchmark, or test commands to public documentation until they work in the repository.

## Verification and contributions

For each proposed change, explain the problem, environments tested, commands run, results and limitations. Include targeted regression tests once a test infrastructure exists. Preserve the MIT license and credit external material according to its own terms. Open an issue or pull request through the repository to share a reproducible result.
