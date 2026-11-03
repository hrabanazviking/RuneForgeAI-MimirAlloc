# KV cache as a managed memory domain

## What the cache stores

In many autoregressive attention models, keys and values from earlier tokens are retained so later tokens do not have to recompute all previous projections. Cache layout, precision, token positions and reuse rules depend on model architecture and inference engine. Some models also use recurrent or hybrid state; that state is not interchangeable with a conventional attention KV cache. KV is derived computational state, not a general-purpose RAM allocator and not an authoritative knowledge store.

## Estimating capacity

For a conventional full-attention layout with uniform layers and separate keys/values, a starting estimate is:

`bytes ≈ 2 × layers × cached_tokens × kv_heads × head_dimension × bytes_per_element × sequences`

For 32 layers, 4096 tokens, 8 KV heads, 128 dimensions, 2 bytes per element and one sequence, that is 536,870,912 bytes = 512 MiB before metadata, padding, page fragmentation, batching and workspace. Actual implementations may use different layer types, sliding windows, compressed or quantized state, prefix sharing, heterogeneous head counts or backend-specific layouts. Measure the real model and actual engine. Multiplying a shared prefix by every sequence can overcount; adding GPU and CPU views of shared UMA pages can also double-count.

## A MimirAlloc-facing design — proposed

MimirAlloc could define an **optional managed-buffer interface** for a cooperating inference runtime. The runtime remains authoritative over attention semantics. The adapter reports model/engine identity, buffer handles, layer/group, token positions, block size, dtype, location, bytes, reference count or equivalent ownership, readiness/completion token and recomputability. The backend provides storage and transfer only when the runtime's API exposes it.

### Paged blocks

A paged design stores logical sequence positions in fixed-size blocks mapped to physical locations. [PagedAttention](https://arxiv.org/abs/2309.06180) presents one such approach and evaluates sharing and copy-on-write in vLLM. Smaller blocks reduce per-request tail waste but increase block-table, scheduling and transfer overhead. Larger blocks do the reverse. Block identity and safe lifetime must be engine-specific; MimirAlloc cannot page arbitrary models by changing their malloc implementation alone.

### Sharing and prefix reuse

A reusable prefix must match all relevant state: exact token IDs and order, model weights/revision, tokenizer/input processing, position/RoPE configuration, adapters/LoRA, multimodal inputs, architecture, attention masking, numerical/layout compatibility and applicable tenant policy. The engine must decide the full compatibility key. A matching text string or semantic similarity is not enough. Blocks should be immutable while shared, with copy-on-write when the engine requires divergence. A cache hit can speed prefill; it does not automatically speed generation of new tokens. See [vLLM prefix caching](https://docs.vllm.ai/en/latest/features/automatic_prefix_caching/).

### Tiers and eviction

Potential locations: device-local memory, host memory, supported local SSD storage or a remote service. A tier can store a valid cache only if the engine supports serialization, transfer, layout and restore. Compare transfer and restore time with recomputation and storage/energy cost. Reference counts and completion events protect live or in-flight blocks. Eviction must not free active state unless the runtime can recompute or resume correctly; choose eviction from explicitly reclaimable objects and enforce tenant isolation.

### Compression and precision

Quantization, pruning, compression and selective retention can reduce footprint but may affect output and accuracy, need conversion kernels, and complicate sharing. [CacheGen](https://doi.org/10.1145/3651890.3672274) reports results for its studied serving workloads; those results do not guarantee equivalent quality or savings for A.E.S.I.R. or arbitrary model architectures. Test reproducibility, quality across long contexts, restore correctness and total CPU/GPU time. Treat lossy and lossless options separately.

## Budget contract

A runtime may request `min_required`, `preferred`, `max_allowed`, latency target, privacy scope and recompute policy for each session. The system admits, queues, reduces service level only with caller consent, or rejects before exceeding hard limits. During pressure it can purge unreferenced compatible blocks first and request the runtime to recompute/offload others. It must not silently discard live session context and pretend behavior is unchanged.

## Example experiments

1. Same model/prompt/session set: compare no reuse, exact-prefix reuse, and tiered reuse. Record first-token latency, decode latency, GPU/host memory, hit rate, disk/network traffic and errors.
2. Sweep block sizes under identical sessions; include many short prompts and a long context. Record block fragmentation and scheduling overhead.
3. Compare recompute vs host/SSD/network load with measured bandwidth and concurrent demand; include eviction and read/write amplification.
4. Test cache-key mismatches for model revision, tokenizer, position, adapters, multimodal input and security scope. A false hit is a correctness failure.
5. With quantization, compare output/logits or task-quality metrics against a baseline over multiple seeds and workload types; report regressions and nondeterminism.

## Sources

- [PagedAttention paper](https://arxiv.org/abs/2309.06180)
- [vLLM automatic prefix caching](https://docs.vllm.ai/en/latest/features/automatic_prefix_caching/)
- [vLLM prefix caching design](https://docs.vllm.ai/en/latest/design/prefix_caching/)
- [CacheGen paper](https://doi.org/10.1145/3651890.3672274)
- [LMCache architecture](https://github.com/LMCache/LMCache/blob/dev/docs/source/developer_guide/architecture.rst)
- [Transformers cache documentation](https://huggingface.co/docs/transformers/en/kv_cache)
