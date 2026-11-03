# Intelligent Allocation, RAG, KV Cache and Federated Memory

A research collection for MimirAlloc, exploring optional tiny-model policies, retrieval-augmented operational knowledge, AI inference caches, and a common interface for heterogeneous resources. **Status:** research and proposed designs only. No working allocator, learned controller, cache manager or federated pool is claimed.

| Guide | Focus |
|---|---|
| [Intelligent and unified memory: vision and map](01_VISION_AND_MEMORY_MAP.md) | Vision and memory categories |
| [Tiny language models as optional memory-policy advisors](02_TINY_LLM_POLICY.md) | Tiny LLMs as bounded optional advisors |
| [Learned allocation: lifetimes, placement and practical baselines](03_LEARNED_ALLOCATION.md) | Lifetime prediction and learned allocation |
| [KV cache as a managed memory domain](04_KV_CACHE_DESIGN.md) | KV cache blocks, reuse and tiering |
| [RAG and retrieval-aware memory management](05_RAG_MEMORY.md) | RAG for and within memory systems |
| [Federated heterogeneous memory: one system, explicit capabilities](06_FEDERATED_MEMORY.md) | Unified control across diverse device memory |
| [Distributed memory objects and cross-device contracts](07_DISTRIBUTED_OBJECTS.md) | Distributed object and failure contracts |
| [Control, correctness and safe autonomy](08_CONTROL_SAFETY.md) | Safety, control loops and fallback |
| [Edge deployment, model overhead and device budgets](09_EDGE_PLANS_AND_BUDGETS.md) | Edge deployment and accounting |
| [Experimental program and evaluation framework](10_EXPERIMENTS.md) | Experiment and evaluation plans |
| [Roadmap: from deterministic allocation to intelligent memory federation](11_ROADMAP.md) | Implementation roadmap and gates |
| [Source catalog and evidence boundaries](12_REFERENCES.md) | Primary sources and evidence |
| [Proposed interfaces and policy schemas](13_INTERFACE_AND_POLICY_SCHEMAS.md) | Typed object, advisor, KV and retrieval contracts |
| [Case studies and integration](14_CASE_STUDIES_AND_INTEGRATION.md) | Practical edge, RAG, device and distributed scenarios |

## Read by interest

- **Understand the architecture:** [vision](01_VISION_AND_MEMORY_MAP.md), [federation](06_FEDERATED_MEMORY.md), [roadmap](11_ROADMAP.md).
- **Build optional intelligence:** [tiny-model policy](02_TINY_LLM_POLICY.md), [learned allocation](03_LEARNED_ALLOCATION.md), [control safety](08_CONTROL_SAFETY.md).
- **AI inference and knowledge:** [KV cache](04_KV_CACHE_DESIGN.md), [RAG](05_RAG_MEMORY.md), [experiments](10_EXPERIMENTS.md).
- **Connect devices:** [federated memory](06_FEDERATED_MEMORY.md), [distributed objects](07_DISTRIBUTED_OBJECTS.md), [edge budgets](09_EDGE_PLANS_AND_BUDGETS.md).

A single interface can coordinate compatible resources; it does not manufacture direct access, physical capacity or coherence where hardware and drivers do not provide them. Optional language-model suggestions never override deterministic ownership, bounds, synchronization, permissions or failure handling.

Return to the [overall research library](../README.md), [neural-computer research](../llm_computers/README.md), or [original MimirAlloc roadmap](../../MimirAlloc_Research_and_Development_Roadmap.md).
