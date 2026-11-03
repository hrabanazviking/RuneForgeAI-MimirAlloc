# MimirAlloc Memory Systems Research Library

A practical map of memory allocation from a single process to heterogeneous edge devices and distributed systems. These documents are **research and proposals**; they do not claim that MimirAlloc already implements or supports these ideas. Begin with the original [research and development roadmap](../MimirAlloc_Research_and_Development_Roadmap.md) for project scope and the actual Mojo blocker.

| Guide | Read it for |
|---|---|
| [Memory allocation: a practical foundation](01_ALLOCATOR_FOUNDATIONS.md) | How allocators work and what they promise |
| [Modern allocator design and improvement](02_MODERN_ALLOCATOR_DESIGN.md) | Modern designs, tradeoffs and pressure policy |
| [ARM, RISC-V, Android and constrained CPU targets](03_ARM_RISCV_ANDROID.md) | ARM, RISC-V and Android platform constraints |
| [Edge devices, low memory and resource pressure](04_EDGE_MEMORY_PRESSURE.md) | Low-memory devices and pressure-aware decisions |
| [GPU, NPU and shared-memory allocation](05_GPU_NPU_SHARED_MEMORY.md) | GPU/NPU buffers, synchronization and shared memory |
| [Cloud, NUMA, memory tiers and CXL pools](06_CLOUD_NUMA_CXL.md) | NUMA, CXL, cloud and memory tiers |
| [Distributed and networked memory systems](07_DISTRIBUTED_NETWORKED_MEMORY.md) | Networked memory, remote objects and failures |
| [AI inference, memory lifetimes and Project A.E.S.I.R.](08_INFERENCE_MEMORY_SYSTEMS.md) | Inference, KV cache and A.E.S.I.R. |
| [Correctness, security, testing and evidence](09_SECURITY_CORRECTNESS_TESTING.md) | Correctness, security, integration and test gates |
| [Future architectures and MimirAlloc research roadmap](10_FUTURE_ARCHITECTURES_ROADMAP.md) | Architecture vision and staged milestones |
| [Allocator landscape: what to study and compare](11_ALLOCATOR_LANDSCAPE.md) | Established allocators and comparison methods |
| [Experimental lab and measurement recipes](12_EXPERIMENT_LAB.md) | Practical experiment and measurement protocols |
| [Primary-source research and reading map](13_SOURCE_READING_MAP.md) | Primary-source bibliography and reading map |
| [Neural computers and LLM-embedded virtual machines](llm_computers/README.md) | Guest allocators, model state, verification, and distributed worker research |
| [Intelligent allocation, RAG, KV cache and federated memory](intelligent_memory/README.md) | Tiny-model policies, retrieval, unified resource management and experiments |
| [AI and memory integrated research synthesis](ai_memory_synthesis/README.md) | Neural, agent, retrieval, allocator and device memory research |

## Suggested paths

- **New to allocators:** 01 → 02 → 11 → 12.
- **Building for the tablet or Pi:** 01 → 03 → 04 → 09 → 12.
- **Project A.E.S.I.R.:** 04 → 05 → 08 → 12.
- **Cloud and future systems:** 05 → 06 → 07 → 10.
- **Contributors:** 09 → 10 → 12 → 13; preserve provenance and document real results.

## Evidence labels

**Documented** = supported by a cited source for its stated scope. **Proposed** = a design or hypothesis to investigate. **Host-tested** and **device-tested** require named builds and logs. **Integration-proven** requires the original target and evidence it actually used MimirAlloc. **Performance-measured** requires reproducible workloads, baselines and results. None of this research alone establishes an implementation, performance win, Mojo compatibility, Android support or remote-memory service.
