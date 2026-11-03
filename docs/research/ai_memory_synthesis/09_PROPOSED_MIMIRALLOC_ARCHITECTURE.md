# Proposed MimirAlloc architecture: integrating the research

## Design boundary

MimirAlloc is currently research and planning. This document is an original proposal based on the cited research; it is not an implemented design or claim of formal clean-room separation. The existing project goal of investigating the actual Mojo/ARM obstacle remains important and independent from the more ambitious AI-memory fabric.

## Layers

### A. Core allocator

A small namespaced allocator implements target ABI behavior, checked arithmetic, alignment, ownership, concurrency and error handling. First support explicitly named host targets and test a freestanding arena separately. It does not require a language model, network, vector database, GPU or remote service to boot or free memory.

### B. Managed-object layer

Optional objects have opaque identity, required access, scope, lifetime, mobility, durability and budget. A backend returns an actual compatible view or failure. Raw `malloc` pointers keep their normal contract. The manager can coordinate explicitly movable, quiesced objects; it cannot silently move arbitrary live objects.

### C. Domain adapters

- **Model runtime:** KV blocks and recurrent state with engine-owned compatibility and completion.
- **RAG store:** source/index/result resources with ACL, version and invalidation.
- **Accelerator:** supported CPU/GPU/NPU buffers, export/import and fences.
- **Neural-computer guest:** bounded offsets and snapshots through the actual host bridge.
- **Remote object service:** explicitly versioned objects, authorization, idempotency and failure.

Each adapter has a contract and may be unavailable. Integration is established per runtime and device, not by an adapter's name.

### D. Telemetry and evidence

Collect bounded aggregate resource counters and optional sampled allocation histories. Store test conditions, revisions, source provenance, negative results and policy outcomes. Avoid raw content by default, respect tenant boundaries and make observability non-recursive.

### E. Optional learned policy

A small model or simple predictor may propose classifications or settings from bounded telemetry and a permission-scoped retrieval corpus. It operates asynchronously. A deterministic guard checks typed actions, exact current epoch, supported operations, limits and authorization. A missing advisor is normal.

## Cross-domain flow: repeated document query

1. Application authenticates a user and supplies a request to a retrieval service.
2. Retrieval checks permissions, resolves current documents and returns evidence with version/provenance.
3. Inference runtime creates tokens and determines whether an existing KV prefix is exactly compatible for that tenant/model/configuration.
4. Managed-object layer accounts for supported index, buffer and cache resources. It cannot declare an engine cache hit itself.
5. Optional policy suggests a cache budget; validator applies it only to reclaimable or runtime-managed resources.
6. A device transfer waits for required completion and visibility. On failure, the runtime recomputes if its contract permits or returns an error.
7. Results and aggregate measurements are recorded with privacy and retention rules.

## Novel research hypotheses to test

| Hypothesis | Proposed mechanism | Counterexample / risk | Comparison |
|---|---|---|---|
| Joint budgets improve memory pressure | coordinate reclaimable RAG and KV caches | starving retrieval or destroying high-value KV | independent fixed quotas |
| Lifetime-aware heap classes reduce retention | classify allocation sites with a bounded predictor | stale predictions strand memory | system allocator and simple heuristic |
| Version-aware evidence improves advisor settings | retrieve scoped past experiments | similar metadata hides hardware differences | no RAG / deterministic rules |
| A common object interface simplifies placement | CPU/device adapters with explicit handles | abstraction overhead and capability mismatch | direct application-managed copies |
| State snapshots accelerate resume | runtime-specific checkpoint and restore | invalid state, storage IO and privacy | safe full recomputation |
| Neural-computer support advances portability | guest fixed arena plus host accounting | tiny guest RAM and slow trace execution | native CPU and conventional emulator |

These are proposed tests, not new results or performance promises.

## Minimal vertical slice

1. Demonstrate a deterministic fixed-budget CPU allocator or arena on an actual supported target.
2. Measure it with a small representative program; record live and retained bytes.
3. Add a separate static policy file and an explicit reclaim-safe cache limit.
4. Add optional shadow advisor logging for one proposal class; keep it unable to mutate.
5. Add exactly one real runtime adapter only after API and ownership are understood.
6. Test failure and rollback before considering accelerator/remote integration.

Every stage can produce useful evidence even if a later stage is infeasible. Avoid claiming a complete unified fabric from a successful local allocation.

## Sources

- [LLAMA allocation research](https://research.google/pubs/learning-based-memory-allocation-for-c-server-workloads/)
- [PagedAttention](https://arxiv.org/abs/2309.06180)
- [RAG](https://papers.neurips.cc/paper/2020/hash/6b493230205f780e1bc26945df7481e5-Abstract.html)
- [Linux CXL](https://cxl.docs.kernel.org/)
