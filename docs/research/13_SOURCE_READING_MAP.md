# Primary-source research and reading map

This bibliography points to specifications, original research and official project documentation. Accessed for this research collection in November 2026; live documentation may change. A source supports the particular subject indicated, not every design proposal in this collection. Save revision or publication details in an experiment's source ledger before implementation.

## Allocator designs and evaluations

1. [mimalloc: Free List Sharding in Action](https://www.microsoft.com/en-us/research/publication/mimalloc-free-list-sharding-in-action/) — research on sharding, locality and benchmark performance. Use as an architectural comparison; hardware and workloads matter.
2. [snmalloc: A Message Passing Allocator](https://spiral.imperial.ac.uk/entities/publication/a3c77e4b-1db1-4e65-ba35-73b075c351a6) — cross-thread deallocation and message passing. Study ownership and batching tradeoffs.
3. [scalloc: multicore-scalable allocation](https://research.google/pubs/fast-multicore-scalable-low-fragmentation-memory-allocation-through-large-virtual-memory-and-global-data-structures/) — allocator scalability and virtual-memory-based design; evaluate address-space requirements for small devices.
4. [Characterizing a Memory Allocator at Warehouse Scale](https://research.google/pubs/characterizing-a-memory-allocator-at-warehouse-scale/) — production-scale TCMalloc workload measurements; not a tablet benchmark.
5. [Beyond malloc Efficiency to Fleet Efficiency](https://research.google/pubs/beyond-malloc-efficiency-to-fleet-efficiency-a-hugepage-aware-memory-allocator/) — hugepage-aware allocation and fleet implications.
6. [Adaptive Hugepage Subrelease](https://research.google/pubs/adaptive-hugepage-subrelease-for-non-moving-memory-allocators-in-warehouse-scale-computers/) — memory release versus hugepage preservation.
7. [StarMalloc](https://doi.org/10.1145/3689773) — verification of a hardened allocator; informs correctness and security work.
8. [LLVM Scudo documentation](https://llvm.org/docs/ScudoHardenedAllocator.html) — production-oriented hardening design and configuration.
9. [TCMalloc documentation](https://google.github.io/tcmalloc/overview.html) — allocator components and platform-specific concepts; check the exact version involved in the Mojo issue.

## CPU, OS and memory pressure

10. [Linux memory-management concepts](https://docs.kernel.org/admin-guide/mm/concepts.html) — paging, memory zones and placement.
11. [Linux PSI](https://docs.kernel.org/accounting/psi.html) — stall-based pressure signals; not an allocation-failure oracle.
12. [Linux cgroup v2](https://docs.kernel.org/admin-guide/cgroup-v2.html) — memory control, throttling, limits and event reporting.
13. [Linux NUMA memory policy](https://docs.kernel.org/admin-guide/mm/numa_memory_policy.html) — placement policies and scope.
14. [Android 16 KB page-size guidance](https://developer.android.com/guide/practices/page-sizes) — page-size compatibility, ELF alignment and tests; check current publication requirements before release.
15. [Android Arm MTE](https://developer.android.com/ndk/guides/arm-mte) and [Linux AArch64 MTE](https://docs.kernel.org/arch/arm64/memory-tagging-extension.html) — supported hardware tagging and OS interfaces.
16. [RISC-V RVWMO](https://docs.riscv.org/reference/isa/unpriv/rvwmo.html) and [A extension](https://docs.riscv.org/reference/isa/unpriv/a-st-ext.html) — memory ordering and atomic instructions; match the actual target.

## Accelerator and heterogeneous memory

17. [Vulkan memory specification](https://docs.vulkan.org/spec/latest/chapters/memory.html) — memory heaps, types and allocation behavior.
18. [Vulkan embedded programming](https://docs.vulkan.org/guide/latest/embedded_programming.html) — UMA-relevant properties, flushing and invalidation.
19. [CUDA stream-ordered allocator](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__MALLOC__ASYNC.html) — CUDA-specific allocation/free ordering and pool behavior.
20. [Linux DMA-BUF](https://docs.kernel.org/driver-api/dma-buf.html) — shared buffers and synchronization; actual support depends on drivers and APIs.
21. [ONNX Runtime execution providers](https://onnxruntime.ai/docs/execution-providers/) — heterogeneous backend selection; not a universal NPU memory ABI.
22. [PagedAttention / vLLM research](https://arxiv.org/abs/2309.06180) — paging ideas for KV cache and serving. Different runtimes may implement different cache designs.

## Fabric and distributed systems

23. [Linux CXL documentation](https://cxl.docs.kernel.org/) and [CXL device types](https://docs.kernel.org/driver-api/cxl/devices/device-types.html) — Linux's supported CXL memory device concepts.
24. [CXL 3.0 specification](https://computeexpresslink.org/wp-content/uploads/2024/02/CXL-3.0-Specification.pdf) — formal architecture and device protocol scope. Product capabilities require independent checking.
25. [Linux user verbs](https://docs.kernel.org/infiniband/user_verbs.html) — userspace RDMA interface and resource model; transport completion and persistence are distinct.

## Source hygiene

Prefer a pinned release, paper DOI or versioned specification for an implementation decision. Note a source's authors, scope, date, license, relevant sections, what is directly supported, and what remains an inference. Do not import third-party source code, illustrations or extensive prose merely because a URL is listed. This document's research is broad but not an exhaustive survey of all allocator literature or future architectures.
