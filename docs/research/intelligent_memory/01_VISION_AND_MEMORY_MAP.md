# Intelligent and unified memory: vision and map

**Research date:** 2026-11-02. **Status:** proposals and primary-source research for MimirAlloc. None of the allocator, LLM policy, RAG, KV cache or distributed memory functionality described here is implemented or tested by these documents.

## Vision

Build a memory system that can make diverse resources easier to use together: CPU-accessible memory, device buffers, accelerator state, local storage, selected remote capacity, model KV caches and retrieval collections. A common control and naming layer could discover resources, enforce budgets and place explicitly managed objects where they are useful. Optional small models could suggest policies using summarized telemetry and retrieved prior experience. This is an ambitious systems research program rather than an ordinary `malloc` replacement.

The goal is a **unified experience and coordinated resource policy**, with truthful capabilities for each resource. A single coherent physical memory shared by every existing device is not implied: some memory is private, not exportable, unsupported by its driver, or costly to access. Network failures, permission boundaries, different consistency guarantees and device synchronization remain real. A user should see what can be shared, where copies exist and what happens if a resource disappears.

## Five kinds of memory, five contracts

| Kind | What it represents | Can it be discarded or reconstructed? | Manager |
|---|---|---|---|
| CPU heap allocation | Bytes currently owned by an application | Live raw-pointer objects usually cannot be discarded or moved transparently | Allocator + application |
| GPU/NPU buffer | Device-visible storage with API-specific access/lifetime | Only if application marks it recomputable or has another valid copy | Device API + application |
| LLM KV cache | Intermediate attention state for a particular model/prefix/configuration | Often recomputable from exact input when the runtime permits; may be in active use | Inference runtime |
| RAG corpus/index | Original documents, metadata, embeddings and search index | An index may be rebuildable only if source data and exact configuration remain | Storage/retrieval service |
| Remote/pooled object | Versioned data held by another process or host | Depends on durability, replicas and consistency guarantees | Object service + client |

“KV” means **key/value tensors in attention**, not necessarily a generic key-value database. RAG is retrieval of external content to support a model, not a replacement for CPU virtual memory. A trained language model's weights are not a reliable authoritative allocator metadata database.

## Proposed four-plane architecture

1. **Correctness/data plane:** small deterministic allocator and backend-specific storage code track bounds, ownership, synchronization and failure. It can operate without an LLM or RAG.
2. **Resource plane:** inventory CPU, GPU/NPU, storage and remote capabilities with actual capacity, budgets, locality, access methods and compatibility, using opaque handles for movable objects.
3. **Knowledge plane:** optional local retrieval of vetted operating guides, versioned workload measurements, incidents and reproducible results. Apply access controls, expiration and citations.
4. **Advisory plane:** optional small model or lightweight predictor proposes bounded settings, admission choices and placement hints. A validator checks freshness, safety and policy constraints before any change; a deterministic fallback always exists.

The planes share a common resource vocabulary while keeping authority explicit. A control-plane advisor can be paused or replaced without invalidating live allocations. Inference on every `malloc` would add latency, memory use and possible recursion; first test slower, batched policy decisions.

## Evidence and inspiration

Google's [LLAMA allocation research](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/) studied a neural language model that predicts allocation lifetimes from calling contexts to reduce fragmentation in specific C++ server workloads. It is not proof that a general-purpose chat model improves every allocator, ARM tablet or workload.

[PagedAttention](https://arxiv.org/abs/2309.06180) demonstrates a design for noncontiguous KV blocks and sharing in its evaluated serving context. [LMCache](https://github.com/LMCache/LMCache) describes a tiered KV cache service. [RAG](https://papers.neurips.cc/paper/2020/hash/6b493230205f780e1bc26945df7481e5-Abstract.html) combines retrieval from an external index with generation. These are starting points, not blanket compatibility guarantees.

## Two paths through the research

**Allocator builder:** [small-model control](02_TINY_LLM_POLICY.md) → [learned allocation](03_LEARNED_ALLOCATION.md) → [safety and control](08_CONTROL_SAFETY.md) → [experiments](10_EXPERIMENTS.md).

**Memory-system architect:** [resource federation](06_FEDERATED_MEMORY.md) → [KV systems](04_KV_CACHE_DESIGN.md) → [RAG systems](05_RAG_MEMORY.md) → [distributed protocols](07_DISTRIBUTED_OBJECTS.md) → [roadmap](11_ROADMAP.md).

## Shared design principles

- Never allow a prediction to override memory safety, isolation, bounds, ownership or a committed consistency contract.
- A missing, slow or invalid advisor result uses the known safe policy without blocking critical allocation.
- Make object mobility, copies, durability and accelerator synchronization explicit.
- Measure total overhead, including the advisor's own memory, compute, energy, index, retrieval and inference-runtime costs.
- Present unsupported or unavailable hardware as unavailable, not as imaginary pool capacity.
- Any effect on generated output quality, recovery, privacy or access control belongs in the evaluation.
