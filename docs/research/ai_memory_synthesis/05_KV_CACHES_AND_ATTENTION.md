# Attention caches, IO and model inference

## Cache purpose and scope

For many autoregressive transformers, previous keys and values are stored to avoid recomputing prior projections at each decoding step. Their size grows with cached tokens and concurrency in typical full-attention designs. KV tensors are computational intermediates for a specific model and prefix; they are not general documents, durable facts or universally compatible between models.

Illustrative standard full-attention estimate: `2 × layers × tokens × KV_heads × head_dim × bytes_per_element × sequences`. This omits padding, block metadata, workspace and other architecture-specific costs. Shared prefixes, sliding windows, hybrid layers, compressed/quantized caches and recurrent architectures require an adjusted calculation. Verify with runtime measurements.

## Important research directions

- [PagedAttention](https://arxiv.org/abs/2309.06180) describes block-based noncontiguous KV allocation and sharing in an inference-serving system. Block tables, model kernels and scheduling must agree.
- [FlashAttention](https://arxiv.org/abs/2205.14135) studies an IO-aware tiled attention algorithm that reduces data movement between memory levels for supported computations. It is not a general-purpose malloc and does not automatically eliminate the autoregressive KV cache.
- [H2O](https://arxiv.org/abs/2306.14048) studies retaining certain high-attention and recent entries when evicting cache. Eviction can change output; test model/task quality and correctness contract.
- [CacheGen](https://doi.org/10.1145/3651890.3672274) studies compression and streaming of KV state. Savings and quality in its paper are workload-specific.
- [LMCache](https://github.com/LMCache/LMCache) describes integration of KV cache storage across multiple tiers. Verify the exact engine, layout, model and backend supported.

## Correct reuse

A valid prefix-cache key depends on the inference engine's full contract, potentially including exact token IDs and order, model/checkpoint and adapter, tokenizer/preprocessing, positions and RoPE, attention masks, multimodal inputs, tensor dtype/layout, runtime version and tenant policy. Similar meaning or the same document text is not enough. Cache entries should be scoped by authorization and immutable or safely copied on write. [vLLM's prefix documentation](https://docs.vllm.ai/en/latest/features/automatic_prefix_caching/) explains a particular engine's behavior: reuse primarily saves prefill, not generation of new tokens.

## Managed-cache research interface

A cooperating runtime—not the generic allocator—marks cache blocks as active, shared, reclaimable or reconstructible and supplies readiness/completion semantics. The memory manager can account, reserve, transfer or free only as authorized by this runtime. Device or network copies require compatible format, synchronization and safe teardown. If the engine has no integration interface, MimirAlloc can observe public metrics but cannot truthfully control its cache.

## Placement decisions

For an eligible block compare expected reuse, recompute time, transfer latency, bandwidth, serialization/compression, available device/host memory, interference and energy. CPU, SSD and remote tiers have different latency/failure behavior. Ensure that offloaded data is compatible with the restoring runtime and access scope. If a remote cache fails, fall back to valid recomputation where permitted; otherwise return an explicit error.

## Proposed combined experiment

1. Fixed model, exact tokenizer, adapter, prompt set and runtime.
2. Compare no prefix cache, native runtime cache and a supported multi-tier adapter. Sweep session count and block size.
3. Record time to first token, token latency, throughput, cache hit/miss, transfer bytes, CPU/GPU memory and energy where measurable.
4. For compression or eviction, evaluate output/logit differences or task quality on held-out contexts; describe precision and stochastic settings.
5. Change one compatibility attribute at a time and require a miss or verified conversion. Test cancellation, device reset, stale handles and cross-tenant isolation.

## Sources

- [PagedAttention](https://arxiv.org/abs/2309.06180)
- [FlashAttention](https://arxiv.org/abs/2205.14135)
- [H2O](https://arxiv.org/abs/2306.14048)
- [CacheGen](https://doi.org/10.1145/3651890.3672274)
- [vLLM prefix caching](https://docs.vllm.ai/en/latest/features/automatic_prefix_caching/)
