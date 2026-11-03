# 13 — Decisions, open questions and next steps

## Decisions made during the field work

1. **Protect the tablet.** Do not load a roughly 1.19 GiB model when the observed available-memory estimate leaves unmeasured KV, scratch and foreground headroom. A model file's size is not an admission decision.
2. **Keep backends separate.** A GLES compute result, an NNAPI vendor-interface result and a CPU model run are independent evidence. None substitutes for another.
3. **Keep the candidate isolated.** The experimental TCMalloc source change stays a labeled experiment. It is not MimirAlloc's clean-room code, and no binary patch is presented as a working Mojo fix.
4. **Stop at the guard.** The partial single-job build was terminated at the sampled memory floor and cached for possible resumption. A timeout or memory stop is a result to preserve, not a completed build.
5. **Treat source and wheel separately.** A public source tree selecting jemalloc and a nightly wheel that still aborts in TCMalloc are observations about different artifacts until their provenance is established.

## Immediate work

1. On a development host with sufficient memory and storage, reproduce the pinned allocator build and ensure the width setting reaches every compilation unit. Preserve a second, standard-width baseline.
2. Run a bounded allocation, resize, alignment and multithread test on the tablet. Inspect actual mappings and memory use; confirm allocator identity and pointer ownership.
3. Build the Mojo source artifact with a documented allocator choice. Test startup, a minimal compilation, a generated executable and error handling on a 39-bit device. Investigate current source and release packaging rather than assuming either automatically solves it.
4. Independently evaluate whether Android/bionic cross-compilation and packaging are supported. A glibc experiment is not Android-native integration.
5. Write MimirAlloc's own behavioral contract and test plan in original language. Begin a namespaced implementation only under the repository's independent design and provenance rules.

## Open questions

- Is 39-bit user addressability the exact kernel configuration, and how do other target devices differ?
- Does a complete patched TCMalloc actually reserve, tag, allocate and free safely on this kernel? Are NUMA, sanitizers, profiling and aligned operations correct?
- Does a Mojo build using public source's allocator selection also link another incompatible allocator component?
- Can the build operate within measured responsiveness and thermal budgets? What does Android's memory-management API report under real use?
- Which Android toolchain, API level, standard library and runtime ABI support a loadable Mojo component?
- Can a physical NPU dispatch be traced and can relevant model operations be compiled and verified?
- What measured headroom, context and model format permit a correct Qwen session without disrupting foreground work?

An answer is not complete until a specific artifact, device, command, expected result and observed result are recorded. Do not convert a proposal into a capability label because it sounds plausible.
