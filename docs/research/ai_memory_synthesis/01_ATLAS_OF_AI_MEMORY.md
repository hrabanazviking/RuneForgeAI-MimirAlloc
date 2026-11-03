# Atlas of AI and computer memory

**Research snapshot:** 2026-11-02. **Status:** curated research synthesis and proposals; not an exhaustive catalog of every online paper. No MimirAlloc feature, benchmark result or hardware integration is implemented by this document.

## The central idea

An AI system can contain several kinds of “memory” at once. Some hold bytes for a running process; some hold the state of a neural computation; some retain documents, facts or experiences; and some describe where data physically resides. They can be coordinated, but they cannot be substituted for one another without changing guarantees.

MimirAlloc can study a shared vocabulary for **identity, lifetime, cost, placement, provenance and access** while delegating each kind of memory to its actual owner. A deterministic heap stays responsible for memory safety. An inference runtime owns its model states. A document store owns persistent records. A device backend owns device-specific access and synchronization. A research controller can suggest policy, not overrule those contracts.

## A map of memory families

| Family | Stored form | Typical lifetime | Primary question |
|---|---|---|---|
| Model parameters | learned weights | checkpoint/model version | What was encoded during training? |
| Context | current tokens and inputs | request/session window | What is in this immediate computation? |
| Attention KV | model-specific intermediate tensors | inference session/cache policy | Can prior attention work be reused? |
| Recurrent/SSM state | model-specific state vectors/matrices | stream/session | What state is required to continue exactly? |
| External knowledge | documents, chunks, indexes, graphs | user-defined, versioned | Which evidence should be retrieved? |
| Agent experience | events, episodic records, derived summaries | session to persistent | What happened, when, and with what evidence? |
| CPU allocation | heap/stack/OS mappings | allocation/process | Who may access or release these bytes? |
| Accelerator buffers | device-specific objects, tensors | task/queue/session | Which device can access them, and when? |
| Remote/persistent object | versioned data, handles and replicas | policy-dependent | What are the consistency and failure rules? |
| Neural-computer guest state | registers and memory encoded in model state | guest execution/snapshot | Which guest operations are implemented and verified? |

KV “keys and values” are attention tensors, not necessarily records in a key-value database. A semantic memory is not CPU RAM; a context window is not a durable database. The same word does not imply the same API, precision, ownership or recovery behavior.

## Foundational lines of work

- **Learned external memory:** [Neural Turing Machines](https://arxiv.org/abs/1410.5401) study differentiable read/write access to a memory-like structure. This is a trained architecture, not an OS page allocator.
- **Long sequence state:** [Transformer-XL](https://arxiv.org/abs/1901.02860) studies segment-level recurrence and [Mamba](https://arxiv.org/abs/2312.00752) studies selective state spaces. Neither makes arbitrary states interchangeable with a conventional KV cache.
- **External retrieval:** [RAG](https://papers.neurips.cc/paper/2020/hash/6b493230205f780e1bc26945df7481e5-Abstract.html) combines a language model and non-parametric retrieval.
- **Persistent agent memory:** [MemGPT](https://arxiv.org/abs/2310.08560) explores OS-inspired context management; [Generative Agents](https://research.google/pubs/generative-agents-interactive-simulacra-of-human-behavior/) explores stored experiences, reflection and retrieval.
- **Inference memory:** [PagedAttention](https://arxiv.org/abs/2309.06180) manages attention caches in blocks for the system and workloads studied.
- **Learned allocation:** [LLAMA](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/) uses a learned language model over allocation contexts to predict lifetime classes in selected C++ server workloads.

These studies solve related but distinct problems. Their reported results do not add up to a universal AI memory system without integration, compatibility work and independent validation.

## Proposed coordination architecture

1. **Resource registry:** discover authorized CPU, device, storage and network resources; report capability, capacity, physical-backing uncertainty and failure domain.
2. **Deterministic memory core:** implement heap and managed-object invariants, bounded operations, ownership and error handling.
3. **Domain adapters:** optional KV, recurrent-state, retrieval, neural-computer and accelerator interfaces. A domain remains authoritative for its own correctness and semantics.
4. **Knowledge and history:** versioned, permission-scoped descriptions of experiments, workload summaries, source records and policy outcomes.
5. **Optional policy advisor:** proposes decisions from bounded metadata; a guard validates them. The system works when the advisor is off.
6. **Evaluation and audit:** record versioned decisions, tests, resource costs, rollback and failure; avoid private payload collection by default.

## An important limit on “unified”

A single management interface can offer a coordinated view of compatible resources. It cannot make an opaque NPU scratchpad exportable, make remote memory behave like local DRAM, increase physical RAM, or guarantee coherence across unrelated devices. Unknown capacity must be reported as unknown. Live raw pointers cannot be moved merely because a model predicts a better location. Source: [Linux CXL](https://cxl.docs.kernel.org/), [Vulkan memory](https://docs.vulkan.org/spec/latest/chapters/memory.html), [CUDA unified memory](https://docs.nvidia.com/cuda/cuda-programming-guide/02-basics/understanding-memory.html).

## Evidence labels

**Source-reported:** authors or official documentation report a scoped result. **Proposed:** an original integration idea or hypothesis in this collection. **Prototype:** code exists. **Device-tested:** a named physical device passed named tests. **Integration-proven:** the real target uses the component and passes the intended workload. **Measured:** reproducible comparisons include correctness, total resource cost and uncertainty. This collection establishes only the first two categories.
