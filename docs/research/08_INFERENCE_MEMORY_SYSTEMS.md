# AI inference, memory lifetimes and Project A.E.S.I.R.

## The opportunity

Local inference combines long-lived weights, per-session KV, temporary activations, graph workspaces, runtime objects and backend driver allocations. A general-purpose CPU allocator affects only some of these. Improvements must be attributed to the part actually controlled and demonstrated in the actual runtime.

## Categorize before optimizing

| Category | Typical lifetime | Potential strategy | Essential caveat |
|---|---|---|---|
| Model weights | model load to unload | read-only mapping, shared file-backed pages where supported | mapping is not guaranteed resident or shared in every environment |
| KV cache | session or sequence | paged blocks or bounded arenas | model-specific layouts and cache policies |
| Activations | layer or graph step | planned reuse from a workspace | asynchronous execution changes lifetime |
| Scratch / graph workspace | operation or session | backend-aware arena | alignment and device dependencies |
| Tokenizer and host metadata | application/session | ordinary CPU heap or pool | allocator may not dominate total memory |
| Transfers | queue/event lifetime | bounded staging pool | pinned memory and completion constraints |

## KV cache budgeting

An illustrative standard attention formula is `2 * L * T * Hkv * D * B * S` bytes, for layers, tokens, KV heads, head dimension, bytes per stored value, and concurrent sequences. Validate model architecture and actual runtime implementation. This calculation excludes allocator rounding, block tables, alignment, padding and cache metadata. Sliding-window attention, latent or compressed KV, shared prefixes and per-layer differences require a different calculation. Context limits in a model config are not proof that a device can fit that context.

As a hypothetical only: 32 layers, 4096 cached tokens, 8 KV heads, head dimension 128 and 2 bytes per value for one sequence gives 536,870,912 bytes = 512 MiB before overhead. Double token count approximately doubles this term under these assumptions; model weights and workspace are separate.

## Reuse and graph planning

For a known graph, collect tensor sizes and their first/last uses. Non-overlapping lifetimes can share a buffer if alignment, access mode and execution order are compatible. Represent asynchronous streams and fences in the lifetime graph: enqueue completion, not host scheduling, is the relevant endpoint. Test dynamic shapes, graph branches and multi-session overlap. Optimizing for peak live data can reduce allocated capacity but may add synchronization or copies.

## Measurement program

1. Fix model revision, checksum, quantization, prompt, output length, thread count and batch. Record runtime, drivers and hardware.
2. Record cold and warm model-load times, peak host and device memory, prefill and decode throughput, time to first token and tail per-token latency.
3. Attribute allocations using supported runtime/backend instrumentation. Compare baseline and candidate with the same model outputs and deterministic settings where possible.
4. Test context growth, multiple concurrent sessions, cancellation and repeated load/unload. Record output validity, OOM failures and retention after quiescence.
5. Repeat with small ARM Linux, Android environment if supported, and intended integrated GPU; report each separately.

Synthetic allocator speed alone cannot establish end-to-end inference gain. A change may reduce host heap use but leave GPU workspace or physical-memory pressure unchanged.

## Proposed A.E.S.I.R. interfaces

Consider explicit lifetime-scoped allocator contexts and budget reporting for model, session, graph and device. An API may let a caller request buffer properties and query actual placement. Use opaque handles for movable or asynchronously used objects. Keep core inference functional with platform allocators; optional MimirAlloc integration should be independently switchable, avoiding a dependency cycle. No planned interface is implemented here.

## Sources

- [ONNX Runtime execution providers](https://onnxruntime.ai/docs/execution-providers/)
- [Vulkan memory specification](https://docs.vulkan.org/spec/latest/chapters/memory.html)
- [CUDA stream-ordered memory](https://docs.nvidia.com/cuda/cuda-driver-api/group__CUDA__MALLOC__ASYNC.html)
- [PagedAttention/vLLM paper](https://arxiv.org/abs/2309.06180)
